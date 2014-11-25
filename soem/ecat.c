
#include "ecat.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

ecat_net_t * ecat_net_init(char * ifname)
{
  /* allocate memory */
  ecat_net_t * net = calloc (1, sizeof(ecat_net_t));
  if (!net)
  {
    printf ("[%s] Could not allocate memory for ecat_net_t\n", ifname);
    return NULL;
  }

  /* TODO: fix */
  net->ifname = strdup (ifname);
  net->ctx.port = calloc (1, sizeof(ecx_portt));
  net->ctx.slavelist = calloc (1, sizeof(ec_slavet) * EC_MAXSLAVE);
  net->ctx.slavecount = calloc (1, sizeof(int));
  net->ctx.grouplist = calloc (1, sizeof(ec_groupt) * EC_MAXGROUP);
  net->ctx.esibuf = calloc (1, sizeof(uint8) * EC_MAXEEPBUF);
  net->ctx.esimap = calloc (1, sizeof(uint32) * EC_MAXEEPBITMAP);
  net->ctx.elist = calloc (1, sizeof(ec_eringt));
  net->ctx.idxstack = calloc (1, sizeof(ec_idxstackT));
  net->ctx.ecaterror = calloc (1, sizeof(boolean));
  net->ctx.DCtime = calloc (1, sizeof(int64));
  net->ctx.SMcommtype = calloc (1, sizeof(ec_SMcommtypet));
  net->ctx.PDOassign = calloc (1, sizeof(ec_PDOassignt));
  net->ctx.PDOdesc = calloc (1, sizeof(ec_PDOdesct));
  net->ctx.eepSM = calloc (1, sizeof(ec_eepromSMt));
  net->ctx.eepFMMU = calloc (1, sizeof(ec_eepromFMMUt));

  net->ctx.maxslave = EC_MAXSLAVE;
  net->ctx.maxgroup = EC_MAXGROUP;
  net->ctx.esislave = 0;
  net->ctx.DCtO = 0;
  net->ctx.DCl = 0;
  net->ctx.FOEhook = 0;

  net->io_map = calloc (1, 4096);
  if (!net->io_map)
  {
    printf ("[%s] Could not allocate memory for IO map\n", ifname);
    goto cleanup;
  }

  return net;

cleanup:
  // TODO: cleanup
  if (net->io_map)
  {
    free (net->io_map);
  }
  
  if (net)
  {
    free (net);
  }
  return NULL;
}

int ecat_net_scan(ecat_net_t * net)
{
  int status, i;
  //int i, chk;

  status = ecx_init (&net->ctx, net->ifname);
  if (status <= 0)
  {
    printf ("[%s] ecx_init failed\n", net->ifname);
    return -1;
  }

  status = ecx_config_init (&net->ctx, FALSE);
  if (status <= 0)
  {
    printf ("[%s] no slaves found\n", net->ifname);
    return -1;
  }

  ecx_config_map_group (&net->ctx, net->io_map, 0);
  ecx_configdc (&net->ctx);

  while (*(net->ctx.ecaterror))
  {
    printf ("[%s] error: %s\n", net->ifname, ecx_elist2string (&net->ctx));
  }

  printf ("[%s] %d slaves found and configured\n", net->ifname, *(net->ctx.slavecount));
  net->expected_wc = (net->ctx.grouplist[0].outputsWKC * 2) +
    net->ctx.grouplist[0].inputsWKC;
  printf ("[%s] calculated workcounter %d\n", net->ifname, net->expected_wc);

  /* wait for all slaves to reach SAFE_OP state */
  ecx_statecheck (&net->ctx, 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 3);
  if (net->ctx.slavelist[0].state != EC_STATE_SAFE_OP )
  {
    // TODO: fix
    printf ("[%s] not all slaves reached safe operational state.\n", net->ifname);
    ecx_readstate (&net->ctx);
    for (i = 1; i <= *(net->ctx.slavecount) ; i++)
    {

      if (net->ctx.slavelist[i].state != EC_STATE_SAFE_OP)
      {
        printf ("[%s] slave:%d, state:%2x, statuscode:%4x : %s\n", net->ifname,
                i, net->ctx.slavelist[i].state, net->ctx.slavelist[i].ALstatuscode,
                ec_ALstatuscode2string (net->ctx.slavelist[i].ALstatuscode));
      }
    }
  }

  /* ecx_readstate (&net->ctx); */
  /* for(i = 1; i <= *(net->ctx.slavecount); i++) */
  /* { */
  /*   printf ("[%s] slave:%d, name:%s, output:%d, input:%d\n", ifname, i, */
  /*           net->ctx.slavelist[i].name, net->ctx.slavelist[i].Obits, */
  /*           net->ctx.slavelist[i].Ibits); */
  /* } */

#if 0
  printf ("[%s] Request operational state for all slaves\n", ifname);
  net->expected_wc = (net->ctx.grouplist[0].outputsWKC * 2) +
    net->ctx.grouplist[0].inputsWKC;
  printf ("[%s] calculated workcounter %d\n", ifname, net->expected_wc);
  net->ctx.slavelist[0].state = EC_STATE_OPERATIONAL;

  /* send one valid process data to make outputs in slaves happy*/
  ecx_send_processdata (&net->ctx);
  ecx_receive_processdata (&net->ctx, EC_TIMEOUTRET);
  /* request OP state for all slaves */
  ecx_writestate (&net->ctx, 0);

  chk = 40;
  /* wait for all slaves to reach OP state */
  do
  {
    ecx_send_processdata (&net->ctx);
    ecx_receive_processdata (&net->ctx, EC_TIMEOUTRET);
    ecx_statecheck (&net->ctx, 0, EC_STATE_OPERATIONAL, 50000);
  } while (chk-- && (net->ctx.slavelist[0].state != EC_STATE_OPERATIONAL));

  if (net->ctx.slavelist[0].state != EC_STATE_OPERATIONAL)
  {
    printf ("[%s] not all slaves reached safe operational state.\n", ifname);
    ecx_readstate (&net->ctx);
    for(i = 1; i <= *(net->ctx.slavecount); i++)
    {
      if(net->ctx.slavelist[i].state != EC_STATE_SAFE_OP)
      {
        printf ("[%s] slave:%d, state:%2x, statuscode:%4x : %s\n", ifname,
                i, net->ctx.slavelist[i].state, net->ctx.slavelist[i].ALstatuscode,
                ec_ALstatuscode2string(net->ctx.slavelist[i].ALstatuscode));
      }
    }
    goto cleanup;
  }
  
  printf("[%s] operational state reached for all slaves\n", ifname);

#endif
  return 0;
}


/* //if, size, callbacks, link_cb, etc.); */
/* int ecat_net_scan(ecat_net_t * net); */
/* //int ecat_net_verify(...); */
int ecat_net_set_state(ecat_net_t * net, ecat_state_t state)
{
  int ret = -1;
  ecx_readstate (&net->ctx);

  printf ("ecat_net_set_state: %s -> %s\n",
          ecat_util_state_to_str (net->ctx.slavelist[0].state),
          ecat_util_state_to_str (state));

  // TODO: clear errors?

  /* check if it's a valid transision */
  switch (net->ctx.slavelist[0].state) {
    /*
  case EC_STATE_INIT:
    if (state == ECAT_STATE_PRE_OP)
    {
      net->ctx.slavelist[0].state = EC_STATE_PRE_OP;
      ecx_writestate (&net->ctx, 0);
      ret = 0;
    }
    break;

  case EC_STATE_PRE_OP:
    if (state == ECAT_STATE_SAFE_OP)
    {
      net->ctx.slavelist[0].state = EC_STATE_SAFE_OP;
      ecx_writestate (&net->ctx, 0);
      ret = 0;
    }
    else if (state == ECAT_STATE_INIT)
    {
      net->ctx.slavelist[0].state = EC_STATE_INIT;
      ecx_writestate (&net->ctx, 0);
      ret = 0;
    }
    break;
    */
  case EC_STATE_SAFE_OP:
    if (state == ECAT_STATE_OP)
    {
      int chk = 40;
      net->ctx.slavelist[0].state = EC_STATE_OPERATIONAL;

      /* send one valid process data to make outputs in slaves happy*/
      ecx_send_processdata (&net->ctx);
      ecx_receive_processdata (&net->ctx, EC_TIMEOUTRET);
      ecx_writestate (&net->ctx, 0);
      do
      {
        ecx_send_processdata (&net->ctx);
        ecx_receive_processdata (&net->ctx, EC_TIMEOUTRET);
        ecx_statecheck (&net->ctx, 0, EC_STATE_OPERATIONAL, 50000);
      } while (chk-- && (net->ctx.slavelist[0].state != EC_STATE_OPERATIONAL));

      /* if (net->ctx.slavelist[0].state != EC_STATE_OPERATIONAL) */
      /* { */
      /*   printf ("[%s] not all slaves reached safe operational state.\n", net->ifname); */
      /*   ecx_readstate (&net->ctx); */
      /*   for(i = 1; i <= *(net->ctx.slavecount); i++) */
      /*   { */
      /*     if(net->ctx.slavelist[i].state != EC_STATE_SAFE_OP) */
      /*     { */
      /*       printf ("[%s] slave:%d, state:%2x, statuscode:%4x : %s\n", net->ifname, */
      /*               i, net->ctx.slavelist[i].state, net->ctx.slavelist[i].ALstatuscode, */
      /*               ec_ALstatuscode2string(net->ctx.slavelist[i].ALstatuscode)); */
      /*     } */
      /*   } */
      /*   ret = -1; */
      /* } */
      /* else { */
      /*   ret = 0; */
      /* } */
      ret = 0;
    }
    break;

  case EC_STATE_OPERATIONAL:
    if (state == ECAT_STATE_SAFE_OP)
    {
      int chk = 40;
      net->ctx.slavelist[0].state = EC_STATE_SAFE_OP;
      ecx_writestate (&net->ctx, 0);
      do
      {
        ecx_send_processdata (&net->ctx);
        ecx_receive_processdata (&net->ctx, EC_TIMEOUTRET);
        ecx_statecheck (&net->ctx, 0, EC_STATE_SAFE_OP, 50000);
      } while (chk-- && (net->ctx.slavelist[0].state != EC_STATE_SAFE_OP));

      ret = 0;
    }
    break;

  default:
    break;
  }
  return ret;
}

ecat_state_t ecat_net_get_state(ecat_net_t * net) //, ecat_net_state_t * state)
{
  ecx_readstate (&net->ctx);
  switch (net->ctx.slavelist[0].state)
  {
  case EC_STATE_INIT:
    return ECAT_STATE_INIT;

  case EC_STATE_PRE_OP:
    return ECAT_STATE_PRE_OP;

  case EC_STATE_SAFE_OP:
    return ECAT_STATE_SAFE_OP;

  case EC_STATE_OPERATIONAL:
    return ECAT_STATE_OP;

  case EC_STATE_BOOT:
    return ECAT_STATE_BOOT;

  default:
    return -1;
  }
}
/* int ecat_net_send(ecat_net_t * net); */
/* int ecat_net_receive(ecat_net_t * net); */

void ecat_net_print_slave_states (ecat_net_t * net)
{
  int i;
  ecx_readstate (&net->ctx);

  printf ("[%s] ix. status  name            outp inpb error\n", net->ifname);
  printf ("[%s]  -  %-7s system             -    - %s\n", net->ifname, 
          ecat_util_state_to_str (net->ctx.slavelist[0].state),
          net->ctx.slavelist[0].ALstatuscode ? "yes" : "no");

  for (i = 1; i <= *(net->ctx.slavecount) ; i++)
  {
    if (net->ctx.slavelist[i].ALstatuscode)
    {
      printf ("[%s] %2u. %-7s %-15s %4u %4d %s (%x)\n", net->ifname,
              i, ecat_util_state_to_str (net->ctx.slavelist[i].state),
              net->ctx.slavelist[i].name,
              net->ctx.slavelist[i].Obits,
              net->ctx.slavelist[i].Ibits,
              ec_ALstatuscode2string (net->ctx.slavelist[i].ALstatuscode),
              net->ctx.slavelist[i].ALstatuscode);
    }
    else
    {
      printf ("[%s] %2u. %-7s %-15s %4u %4u\n", net->ifname,
              i, ecat_util_state_to_str (net->ctx.slavelist[i].state),
              net->ctx.slavelist[i].name,
              net->ctx.slavelist[i].Obits,
              net->ctx.slavelist[i].Ibits);
    }
  }
}


typedef struct {
  uint8_t state;
  char name[8];
} _ecat_state_name_t;

#define ECAT_END_MARKER 0xffff
#define ECAT_ERR        0x10

const _ecat_state_name_t _ecat_state_name[] = {
  { ECAT_STATE_INIT                       & 0xff, "INIT" },
  { ECAT_STATE_PRE_OP                     & 0xff, "PRE" },
  { ECAT_STATE_SAFE_OP                    & 0xff, "SAFE" },
  { ECAT_STATE_OP                         & 0xff, "OPER" },
  { ECAT_STATE_BOOT                       & 0xff, "BOOT" },
  { ECAT_STATE_INIT    | ECAT_STATE_ERROR & 0xff, "INIT(E)" },
  { ECAT_STATE_PRE_OP  | ECAT_STATE_ERROR & 0xff, "PRE (E)" },
  { ECAT_STATE_SAFE_OP | ECAT_STATE_ERROR & 0xff, "SAFE(E)" },
  { ECAT_STATE_OP      | ECAT_STATE_ERROR & 0xff, "OPER(E)" },
  { ECAT_STATE_BOOT    | ECAT_STATE_ERROR & 0xff, "BOOT(E)" },
  { ECAT_END_MARKER                       & 0xff, "UNKN" }
};


char * ecat_util_state_to_str(ecat_state_t state)
{
   int i = 0;

   while ((_ecat_state_name[i].state != (uint8_t)ECAT_END_MARKER) && 
          (_ecat_state_name[i].state != (uint8_t)state) ) 
   {
      i++;
   }
   
   return (char *) _ecat_state_name[i].name;
}


int ecat_net_send(ecat_net_t * net)
{
  int status;

  status = ecx_send_processdata(&net->ctx);
  if (status >= 0) {
    // TODO: increase send error counter
    // failed to send frame
    return -1;
  }
  else
  {
    // TODO: increase number of sent messages
  }
  return 0;
}

int ecat_net_receive(ecat_net_t * net, unsigned int timeout_us)
{
  int wc;
  wc = ecx_receive_processdata(&net->ctx, timeout_us);

  if (wc <= EC_NOFRAME)
  {
    // TODO: increase missing frame counter
  }
  else if (wc != net->expected_wc)
  {
    // TODO: increase invalid message counter
  }
  else 
  {
    // TODO: increase received messages counter
  }
  return wc;
}



int ecat_slave_set_state(ecat_net_t * net, unsigned int slave, ecat_state_t state)
{
  int ret = -1;
  ecx_readstate (&net->ctx);

  printf ("ecat_net_set_state: %s -> %s\n",
          ecat_util_state_to_str (net->ctx.slavelist[0].state),
          ecat_util_state_to_str (state));

  // TODO: clear errors?

  /* check if it's a valid transision */
  switch (net->ctx.slavelist[0].state) {
    /*
  case EC_STATE_INIT:
    if (state == ECAT_STATE_PRE_OP)
    {
      net->ctx.slavelist[0].state = EC_STATE_PRE_OP;
      ecx_writestate (&net->ctx, 0);
      ret = 0;
    }
    break;

  case EC_STATE_PRE_OP:
    if (state == ECAT_STATE_SAFE_OP)
    {
      net->ctx.slavelist[0].state = EC_STATE_SAFE_OP;
      ecx_writestate (&net->ctx, 0);
      ret = 0;
    }
    else if (state == ECAT_STATE_INIT)
    {
      net->ctx.slavelist[0].state = EC_STATE_INIT;
      ecx_writestate (&net->ctx, 0);
      ret = 0;
    }
    break;
    */
  case EC_STATE_SAFE_OP:
    if (state == ECAT_STATE_OP)
    {
      int chk = 40;
      net->ctx.slavelist[0].state = EC_STATE_OPERATIONAL;

      /* send one valid process data to make outputs in slaves happy*/
      ecx_send_processdata (&net->ctx);
      ecx_receive_processdata (&net->ctx, EC_TIMEOUTRET);
      ecx_writestate (&net->ctx, 0);
      do
      {
        ecx_send_processdata (&net->ctx);
        ecx_receive_processdata (&net->ctx, EC_TIMEOUTRET);
        ecx_statecheck (&net->ctx, 0, EC_STATE_OPERATIONAL, 50000);
      } while (chk-- && (net->ctx.slavelist[0].state != EC_STATE_OPERATIONAL));

      /* if (net->ctx.slavelist[0].state != EC_STATE_OPERATIONAL) */
      /* { */
      /*   printf ("[%s] not all slaves reached safe operational state.\n", net->ifname); */
      /*   ecx_readstate (&net->ctx); */
      /*   for(i = 1; i <= *(net->ctx.slavecount); i++) */
      /*   { */
      /*     if(net->ctx.slavelist[i].state != EC_STATE_SAFE_OP) */
      /*     { */
      /*       printf ("[%s] slave:%d, state:%2x, statuscode:%4x : %s\n", net->ifname, */
      /*               i, net->ctx.slavelist[i].state, net->ctx.slavelist[i].ALstatuscode, */
      /*               ec_ALstatuscode2string(net->ctx.slavelist[i].ALstatuscode)); */
      /*     } */
      /*   } */
      /*   ret = -1; */
      /* } */
      /* else { */
      /*   ret = 0; */
      /* } */
      ret = 0;
    }
    break;

  case EC_STATE_OPERATIONAL:
    if (state == ECAT_STATE_SAFE_OP)
    {
      int chk = 40;
      net->ctx.slavelist[0].state = EC_STATE_SAFE_OP;
      ecx_writestate (&net->ctx, 0);
      do
      {
        ecx_send_processdata (&net->ctx);
        ecx_receive_processdata (&net->ctx, EC_TIMEOUTRET);
        ecx_statecheck (&net->ctx, 0, EC_STATE_SAFE_OP, 50000);
      } while (chk-- && (net->ctx.slavelist[0].state != EC_STATE_SAFE_OP));

      ret = 0;
    }
    break;

  default:
    break;
  }
  return ret;
}


int ecat_slave_get_dc_status(ecat_net_t * net, uint32_t slave, 
        ecat_slave_dc_status_t * status)
{
    uint8_t sys_time[32];

    /* Read System Time registers */
    if (ecx_FPRD(net->ctx.port, net->ctx.slavelist[slave].configadr, 
                ECT_REG_DCSYSTIME, sizeof(sys_time), sys_time, EC_TIMEOUTRET3))
    {
        /* System Time */
        memcpy(&status->time, 
                &sys_time[ECT_REG_DCSYSTIME - ECT_REG_DCSYSTIME], 
                sizeof(status->time)); 

        /* System Time Offset */
        memcpy(&status->offset, 
                &sys_time[ECT_REG_DCSYSOFFSET - ECT_REG_DCSYSTIME], 
                sizeof(status->offset)); 

        /* System Time Delay */
        memcpy(&status->delay, 
                &sys_time[ECT_REG_DCSYSDELAY - ECT_REG_DCSYSTIME], 
                sizeof(status->delay));

        /* System Time Difference */
        memcpy(&status->difference, 
                &sys_time[ECT_REG_DCSYSDIFF - ECT_REG_DCSYSTIME], 
                sizeof(status->difference));

        if (status->difference & 0x80000000) {
            status->difference = 
                0 - (status->difference & 0x7FFFFFFF);
        }
        return 0;
    }

    return -1;
}

int ecat_slave_get_status(ecat_net_t * net, uint32_t slave, 
        ecat_slave_status_t * status)
{
    int ret = 0x0;
    uint16_t dlstat;
    uint16_t wdcntr;
    uint8_t err_cntrs[20];
    uint8_t p;

    /* Read ESC DL status register */
    if (ecx_FPRD(net->ctx.port, net->ctx.slavelist[slave].configadr, 
                ECT_REG_DLSTAT, sizeof(dlstat), &dlstat, EC_TIMEOUTRET3)) 
    {

        /* Link detected */
        for (p=0; p <= 3; p++)
        {
            status->port[p].link_detected = 
                dlstat & (0x0010 << p)
                ? ECAT_SLAVE_LINK_DETECTED:ECAT_SLAVE_NO_LINK;
        }

        /* Loop port */
        for (p=0; p <= 3; p++)
        {
            status->port[p].loop_port = 
                dlstat & (0x0100 << (p * 2)) 
                ? ECAT_SLAVE_PORT_CLOSED:ECAT_SLAVE_PORT_OPEN;
        }

        /* Stable communication */
        for (p=0; p <= 3; p++)
        {
            status->port[p].stable_com = 
                dlstat & (0x0200 << (p * 2)) 
                ? ECAT_SLAVE_COM_ESTABLISHED:ECAT_SLAVE_NO_STABLE_COM;
        }
    } 
    else 
    {
        ret |= 0x1;
    }

    /* Read Error Counters registers */
    if (ecx_FPRD(net->ctx.port, net->ctx.slavelist[slave].configadr,
                ECT_REG_RXERR, sizeof(err_cntrs), err_cntrs, EC_TIMEOUTRET3))
    {

        /* Lost Link Counter */
        for (p=0; p <= 3; p++)
        {
            status->port[p].lost_link_counter = 
                err_cntrs[ECT_REG_LLCNT - ECT_REG_RXERR + p];
        }

        /* RX Error Counter */
        for (p=0; p <= 3; p++) 
        {
            status->port[p].invalid_frame_counter = 
                err_cntrs[ECT_REG_RXERR + (p * 2)];
            status->port[p].rx_error_counter =
                err_cntrs[ECT_REG_RXERR + (p * 2) + 1];
        }

        /* Forwarded RX Error Counter */
        for (p=0; p <= 3; p++)
        {
            status->port[p].forwarded_rx_error_counter = 
                err_cntrs[ECT_REG_FRXERR - ECT_REG_RXERR + p];
        }

        /* ECAT Processing Unit Error Counter */
        status->ecat_processing_unit_error_counter = 
            err_cntrs[ECT_REG_EPUECNT - ECT_REG_RXERR];

        /* PDI Error Counter */
        status->pdi_error_counter = 
            err_cntrs[ECT_REG_PECNT - ECT_REG_RXERR]; 

        /* PDI Error Code */
        status->pdi_error_code = 
            err_cntrs[ECT_REG_PECODE - ECT_REG_RXERR]; 

    } 
    else 
    {
        ret |= 0x2;
    }

    /* Read Watchdog register */
    if (ecx_FPRD(net->ctx.port, net->ctx.slavelist[slave].configadr, 
                ECT_REG_WDCNT, sizeof(wdcntr), &wdcntr, EC_TIMEOUTRET3))
    {

        /* Watchdog counter process data and pdi */
        status->wd_cnt.watchdog_counter_process_data = wdcntr & 0x00FF;
        status->wd_cnt.watchdog_counter_pdi = wdcntr & 0xFF00;
    } 
    else 
    {
        ret |= 0x4;
    }

    return ret > 0 ? -ret:ret;
}

