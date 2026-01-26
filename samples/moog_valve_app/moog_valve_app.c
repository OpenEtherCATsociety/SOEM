/*
  moog_valve_app.c  (single-file SOEM app, fixed slave order)

  Physical order (fixed):
    1 EK1100
    2 EL1008  (DI)
    3 EL2008  (DO)
    4 EL3062  (AI)
    5 EL4032  (AO +/-10V)

  Function:
    - 3-position switch wired to two DIs:
        DI0 = Forward, DI1 = Reverse
      If both ON -> treated as OFF (fault)
    - Potentiometer on EL3062 CH1 (assumed 0..10V)
    - Command output on EL4032 CH1: -10..+10V
      Uses pot as magnitude:
        FWD -> +pot_volts
        REV -> -pot_volts
        OFF -> 0V
    - Solenoids on DO:
        DO0 = Solenoid FWD, DO1 = Solenoid REV
        FWD -> DO0=1 DO1=0
        REV -> DO0=0 DO1=1
        OFF/fault -> DO0=0 DO1=0
    - Failsafe:
        If WKC wrong or not OP -> outputs forced safe (0V, solenoids off) and recovery attempted.

  Headless:
    - No logging by default (stdout/stderr closed).
    - Enable DEBUG_PRINT if you want diagnostics.

  IMPORTANT NOTE about EL3062 PDO layout:
    Many Beckhoff analog input terminals map each channel as:
      status (2 bytes) + value (2 bytes)
    This code assumes that layout:
      CH1 value = Istart + 2
      CH2 value = Istart + 6
    If your EL3062 is mapped "value-only", change EL3062_CH1_VALUE_OFFSET to 0.
*/

#include "soem/soem.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ===================== User config ===================== */
#define DEBUG_PRINT 0

/* Cycle time */
#define CYCLE_USEC 100000   /* 100ms. Change to 10000 for 10ms, 50000 for 50ms, etc. */

/* DI assignments (EL1008) */
#define DI_FWD_BIT 0
#define DI_REV_BIT 1

/* DO assignments (EL2008) */
#define DO_SOL_FWD_BIT 0
#define DO_SOL_REV_BIT 1

/* EL3062 value offsets (within its input area) */
#define EL3062_CH_STRIDE_BYTES      4
#define EL3062_CH1_VALUE_OFFSET     2   /* status(2)+value(2) => value at +2 */
#define EL3062_CH2_VALUE_OFFSET     6   /* CH2 value: base + 4 + 2 */

/* Analog scaling assumptions (common Beckhoff convention) */
static inline float clampf(float v, float lo, float hi) { return (v < lo) ? lo : (v > hi) ? hi : v; }
static inline float raw_to_v_0_10(int16_t raw)           { return ((float)raw) * 10.0f / 32767.0f; }
static inline int16_t v_to_raw_pm10(float v)             { v = clampf(v, -10.0f, 10.0f); return (int16_t)(v * 32767.0f / 10.0f); }

#if DEBUG_PRINT
  #define DPRINTF(...) do { printf(__VA_ARGS__); fflush(stdout); } while(0)
#else
  #define DPRINTF(...) do { } while(0)
#endif

/* ===================== Fieldbus wrapper ===================== */
typedef struct {
  ecx_contextt ctx;
  char *iface;
  uint8_t group;
  uint8_t iomap[4096];
  int roundtrip_us;
} Fieldbus;

static void fieldbus_initialize(Fieldbus *fb, char *iface)
{
  memset(fb, 0, sizeof(*fb));
  fb->iface = iface;
  fb->group = 0;
}

static int fieldbus_roundtrip(Fieldbus *fb)
{
  ec_timet start, end, diff;
  int wkc;

  start = osal_current_time();
  ecx_send_processdata(&fb->ctx);
  wkc = ecx_receive_processdata(&fb->ctx, EC_TIMEOUTRET);
  end = osal_current_time();

  osal_time_diff(&start, &end, &diff);
  fb->roundtrip_us = (int)(diff.tv_sec * 1000000 + diff.tv_nsec / 1000);
  return wkc;
}

static int fieldbus_expected_wkc(Fieldbus *fb)
{
  ec_groupt *grp = fb->ctx.grouplist + fb->group;
  return grp->outputsWKC * 2 + grp->inputsWKC;
}

static boolean fieldbus_start(Fieldbus *fb)
{
  ec_groupt *grp = fb->ctx.grouplist + fb->group;
  ec_slavet *slave0 = fb->ctx.slavelist;

  DPRINTF("ecx_init(%s)\n", fb->iface);
  if (!ecx_init(&fb->ctx, fb->iface)) return FALSE;

  if (ecx_config_init(&fb->ctx) <= 0) return FALSE;

  ecx_config_map_group(&fb->ctx, fb->iomap, fb->group);
  ecx_configdc(&fb->ctx);

  ecx_statecheck(&fb->ctx, 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 4);

  /* one roundtrip to settle outputs */
  fieldbus_roundtrip(fb);

  /* request OP */
  slave0->state = EC_STATE_OPERATIONAL;
  ecx_writestate(&fb->ctx, 0);

  for (int i = 0; i < 10; ++i) {
    fieldbus_roundtrip(fb);
    ecx_statecheck(&fb->ctx, 0, EC_STATE_OPERATIONAL, EC_TIMEOUTSTATE / 10);
    if (slave0->state == EC_STATE_OPERATIONAL) return TRUE;
  }
  return FALSE;
}

static void fieldbus_stop(Fieldbus *fb)
{
  ec_slavet *slave0 = fb->ctx.slavelist;
  slave0->state = EC_STATE_INIT;
  ecx_writestate(&fb->ctx, 0);
  ecx_close(&fb->ctx);
}

/* Recovery logic (from simple_ng) */
static void fieldbus_check_state(Fieldbus *fb)
{
  ec_groupt *grp = fb->ctx.grouplist + fb->group;
  grp->docheckstate = FALSE;
  ecx_readstate(&fb->ctx);

  for (int i = 1; i <= fb->ctx.slavecount; ++i) {
    ec_slavet *s = fb->ctx.slavelist + i;
    if (s->group != fb->group) continue;

    if (s->state != EC_STATE_OPERATIONAL) {
      grp->docheckstate = TRUE;

      if (s->state == (EC_STATE_SAFE_OP + EC_STATE_ERROR)) {
        s->state = EC_STATE_SAFE_OP + EC_STATE_ACK;
        ecx_writestate(&fb->ctx, i);
      } else if (s->state == EC_STATE_SAFE_OP) {
        s->state = EC_STATE_OPERATIONAL;
        ecx_writestate(&fb->ctx, i);
      } else if (s->state > EC_STATE_NONE) {
        if (ecx_reconfig_slave(&fb->ctx, i, EC_TIMEOUTRET)) {
          s->islost = FALSE;
        }
      } else if (!s->islost) {
        ecx_statecheck(&fb->ctx, i, EC_STATE_OPERATIONAL, EC_TIMEOUTRET);
        if (s->state == EC_STATE_NONE) s->islost = TRUE;
      }
    } else if (s->islost) {
      if (s->state != EC_STATE_NONE) {
        s->islost = FALSE;
      } else if (ecx_recover_slave(&fb->ctx, i, EC_TIMEOUTRET)) {
        s->islost = FALSE;
      }
    }
  }
}

/* ===================== Fixed IO mapping ===================== */
typedef struct {
  /* fixed slave indices (because physical order is fixed) */
  int s_ek1100, s_el1008, s_el2008, s_el3062, s_el4032;

  /* offsets into grp->inputs / grp->outputs */
  int di_base; /* EL1008 Istart */
  int do_base; /* EL2008 Ostart */
  int ai_base; /* EL3062 Istart */
  int ao_base; /* EL4032 Ostart */
} IOMap;

static int iomap_bind_fixed(IOMap *io, Fieldbus *fb)
{
  memset(io, 0, sizeof(*io));

  io->s_ek1100 = 1;
  io->s_el1008 = 2;
  io->s_el2008 = 3;
  io->s_el3062 = 4;
  io->s_el4032 = 5;

  if (fb->ctx.slavecount < 5) return 0;

  io->di_base = (fb->ctx.slavelist + io->s_el1008)->Istart;
  io->do_base = (fb->ctx.slavelist + io->s_el2008)->Ostart;
  io->ai_base = (fb->ctx.slavelist + io->s_el3062)->Istart;
  io->ao_base = (fb->ctx.slavelist + io->s_el4032)->Ostart;

  return 1;
}

static inline void outputs_safe(Fieldbus *fb, IOMap *io)
{
  ec_groupt *grp = fb->ctx.grouplist + fb->group;

  /* Solenoids OFF */
  uint8_t out = grp->outputs[io->do_base];
  out &= (uint8_t)~((1u << DO_SOL_FWD_BIT) | (1u << DO_SOL_REV_BIT));
  grp->outputs[io->do_base] = out;

  /* Analog command 0V */
  *(int16_t *)(grp->outputs + io->ao_base + 0) = v_to_raw_pm10(0.0f);
}

/* ===================== Main ===================== */
int main(int argc, char *argv[])
{
  /* Headless: close stdout/stderr (prevents any accidental logging) */
  if (!DEBUG_PRINT) {
    fclose(stdout);
    fclose(stderr);
  }

  if (argc != 2) return 1;

  Fieldbus fb;
  IOMap io;

  fieldbus_initialize(&fb, argv[1]);

  if (!fieldbus_start(&fb)) {
    fieldbus_stop(&fb);
    return 1;
  }

  if (!iomap_bind_fixed(&io, &fb)) {
    fieldbus_stop(&fb);
    return 1;
  }

  ec_groupt *grp = fb.ctx.grouplist + fb.group;
  const int expected = fieldbus_expected_wkc(&fb);

  for (;;)
  {
    int wkc = fieldbus_roundtrip(&fb);

    /* Check comms + OP state */
    int ok = 1;
    if (wkc < expected) ok = 0;
    if (fb.ctx.slavelist[0].state != EC_STATE_OPERATIONAL) ok = 0;

    if (!ok) {
      outputs_safe(&fb, &io);
      fieldbus_check_state(&fb);
      osal_usleep(CYCLE_USEC);
      continue;
    }

    /* ---- READ DI: switch ---- */
    uint8_t di = grp->inputs[io.di_base];
    int fwd = (di >> DI_FWD_BIT) & 0x01;
    int rev = (di >> DI_REV_BIT) & 0x01;

    /* Invalid (both on) => OFF */
    if (fwd && rev) { fwd = 0; rev = 0; }

    /* ---- READ AI: pot on EL3062 CH1 ---- */
    int16_t raw_pot = *(int16_t *)(grp->inputs + io.ai_base + EL3062_CH1_VALUE_OFFSET);
    float pot_v = clampf(raw_to_v_0_10(raw_pot), 0.0f, 10.0f);

    /* ---- LOGIC ----
       Pot controls magnitude, switch controls direction.
       OFF => 0V command.
    */
    float cmd_v = 0.0f;
    int sol_fwd = 0, sol_rev = 0;

    if (fwd) {
      sol_fwd = 1; sol_rev = 0;
      cmd_v = +pot_v;          /* 0..+10V */
    } else if (rev) {
      sol_fwd = 0; sol_rev = 1;
      cmd_v = -pot_v;          /* 0..-10V */
    } else {
      sol_fwd = 0; sol_rev = 0;
      cmd_v = 0.0f;
    }

    /* ---- WRITE DO: solenoids ---- */
    uint8_t out = grp->outputs[io.do_base];
    out &= (uint8_t)~((1u << DO_SOL_FWD_BIT) | (1u << DO_SOL_REV_BIT));
    if (sol_fwd) out |= (1u << DO_SOL_FWD_BIT);
    if (sol_rev) out |= (1u << DO_SOL_REV_BIT);
    grp->outputs[io.do_base] = out;

    /* ---- WRITE AO: EL4032 CH1 ---- */
    *(int16_t *)(grp->outputs + io.ao_base + 0) = v_to_raw_pm10(cmd_v);

    osal_usleep(CYCLE_USEC);
  }

  /* never reached */
  /* fieldbus_stop(&fb); */
  return 0;
}
