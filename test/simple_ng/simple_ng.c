/** \file
 * \brief Example code for Simple Open EtherCAT master
 *
 * Usage: simple_ng IFNAME1
 * IFNAME1 is the NIC interface name, e.g. 'eth0'
 *
 * This is a minimal test.
 */

#include "ethercat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct {
    ecx_contextt    context;
    char *          iface;
    uint8           group;
    int             roundtrip_time;

    /* Used by the context */
    uint8           map[4096];
    ecx_portt       port;
    ec_slavet       slavelist[EC_MAXSLAVE];
    int             slavecount;
    ec_groupt       grouplist[EC_MAXGROUP];
    uint8           esibuf[EC_MAXEEPBUF];
    uint32          esimap[EC_MAXEEPBITMAP];
    ec_eringt       elist;
    ec_idxstackT    idxstack;
    boolean         ecaterror;
    int64           DCtime;
    ec_SMcommtypet  SMcommtype[EC_MAX_MAPT];
    ec_PDOassignt   PDOassign[EC_MAX_MAPT];
    ec_PDOdesct     PDOdesc[EC_MAX_MAPT];
    ec_eepromSMt    eepSM;
    ec_eepromFMMUt  eepFMMU;
} Fieldbus;


static void
fieldbus_initialize(Fieldbus *fieldbus, char *iface)
{
    ecx_contextt *context;

    /* Let's start by 0-filling `fieldbus` to avoid surprises */
    memset(fieldbus, 0, sizeof(*fieldbus));

    fieldbus->iface = iface;
    fieldbus->group = 0;
    fieldbus->roundtrip_time = 0;
    fieldbus->ecaterror = FALSE;

    /* Initialize the ecx_contextt data structure */
    context = &fieldbus->context;
    context->port = &fieldbus->port;
    context->slavelist = fieldbus->slavelist;
    context->slavecount = &fieldbus->slavecount;
    context->maxslave = EC_MAXSLAVE;
    context->grouplist = fieldbus->grouplist;
    context->maxgroup = EC_MAXGROUP;
    context->esibuf = fieldbus->esibuf;
    context->esimap = fieldbus->esimap;
    context->esislave = 0;
    context->elist = &fieldbus->elist;
    context->idxstack = &fieldbus->idxstack;
    context->ecaterror = &fieldbus->ecaterror;
    context->DCtime = &fieldbus->DCtime;
    context->SMcommtype = fieldbus->SMcommtype;
    context->PDOassign = fieldbus->PDOassign;
    context->PDOdesc = fieldbus->PDOdesc;
    context->eepSM = &fieldbus->eepSM;
    context->eepFMMU = &fieldbus->eepFMMU;
    context->FOEhook = NULL;
    context->EOEhook = NULL;
    context->manualstatechange = 0;
}

static int
fieldbus_roundtrip(Fieldbus *fieldbus)
{
    ecx_contextt *context;
    ec_timet start, end, diff;
    int wkc;

    context = &fieldbus->context;

    start = osal_current_time();
    ecx_send_processdata(context);
    wkc = ecx_receive_processdata(context, EC_TIMEOUTRET);
    end = osal_current_time();
    osal_time_diff(&start, &end, &diff);
    fieldbus->roundtrip_time = diff.sec * 1000000 + diff.usec;

    return wkc;
}

static boolean
fieldbus_start(Fieldbus *fieldbus)
{
    ecx_contextt *context;
    ec_groupt *grp;
    ec_slavet *slave;
    int i;

    context = &fieldbus->context;
    grp = fieldbus->grouplist + fieldbus->group;

    printf("Initializing SOEM on '%s'... ", fieldbus->iface);
    if (! ecx_init(context, fieldbus->iface)) {
        printf("no socket connection\n");
        return FALSE;
    }
    printf("done\n");

    printf("Finding autoconfig slaves... ");
    if (ecx_config_init(context, FALSE) <= 0) {
        printf("no slaves found\n");
        return FALSE;
    }
    printf("%d slaves found\n", fieldbus->slavecount);

    printf("Sequential mapping of I/O... ");
    ecx_config_map_group(context, fieldbus->map, fieldbus->group);
    printf("mapped %dO+%dI bytes from %d segments",
           grp->Obytes, grp->Ibytes, grp->nsegments);
    if (grp->nsegments > 1) {
        /* Show how slaves are distrubuted */
        for (i = 0; i < grp->nsegments; ++i) {
            printf("%s%d", i == 0 ? " (" : "+", grp->IOsegment[i]);
        }
        printf(" slaves)");
    }
    printf("\n");

    printf("Configuring distributed clock... ");
    ecx_configdc(context);
    printf("done\n");

    printf("Waiting for all slaves in safe operational... ");
    ecx_statecheck(context, 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 4);
    printf("done\n");

    printf("Send a roundtrip to make outputs in slaves happy... ");
    fieldbus_roundtrip(fieldbus);
    printf("done\n");

    printf("Setting operational state..");
    /* Act on slave 0 (a virtual slave used for broadcasting) */
    slave = fieldbus->slavelist;
    slave->state = EC_STATE_OPERATIONAL;
    ecx_writestate(context, 0);
    /* Poll the result ten times before giving up */
    for (i = 0; i < 10; ++i) {
        printf(".");
        fieldbus_roundtrip(fieldbus);
        ecx_statecheck(context, 0, EC_STATE_OPERATIONAL, EC_TIMEOUTSTATE / 10);
        if (slave->state == EC_STATE_OPERATIONAL) {
            printf(" all slaves are now operational\n");
            return TRUE;
        }
    }

    printf(" failed,");
    ecx_readstate(context);
    for (i = 1; i <= fieldbus->slavecount; ++i) {
        slave = fieldbus->slavelist + i;
        if (slave->state != EC_STATE_OPERATIONAL) {
            printf(" slave %d is 0x%04X (AL-status=0x%04X %s)",
                 i, slave->state, slave->ALstatuscode,
                 ec_ALstatuscode2string(slave->ALstatuscode));
        }
    }
    printf("\n");

    return FALSE;
}

static void
fieldbus_stop(Fieldbus *fieldbus)
{
    ecx_contextt *context;
    ec_slavet *slave;

    context = &fieldbus->context;
    /* Act on slave 0 (a virtual slave used for broadcasting) */
    slave = fieldbus->slavelist;

    printf("Requesting init state on all slaves... ");
    slave->state = EC_STATE_INIT;
    ecx_writestate(context, 0);
    printf("done\n");

    printf("Close socket... ");
    ecx_close(context);
    printf("done\n");
}

static boolean
fieldbus_dump(Fieldbus *fieldbus)
{
    ec_groupt *grp;
    uint32 n;
    int wkc, expected_wkc;

    grp = fieldbus->grouplist + fieldbus->group;

    wkc = fieldbus_roundtrip(fieldbus);
    expected_wkc = grp->outputsWKC * 2 + grp->inputsWKC;
    printf("%6d usec  WKC %d", fieldbus->roundtrip_time, wkc);
    if (wkc < expected_wkc) {
        printf(" wrong (expected %d)\n", expected_wkc);
        return FALSE;
    }

    printf("  O:");
    for (n = 0; n < grp->Obytes; ++n) {
        printf(" %02X", grp->outputs[n]);
    }
    printf("  I:");
    for (n = 0; n < grp->Ibytes; ++n) {
        printf(" %02X", grp->inputs[n]);
    }
    printf("  T: %lld\r", (long long) fieldbus->DCtime);
    return TRUE;
}

static void
fieldbus_check_state(Fieldbus *fieldbus)
{
    ecx_contextt *context;
    ec_groupt *grp;
    ec_slavet *slave;
    int i;

    context = &fieldbus->context;
    grp = context->grouplist + fieldbus->group;
    grp->docheckstate = FALSE;
    ecx_readstate(context);
    for (i = 1; i <= fieldbus->slavecount; ++i) {
        slave = context->slavelist + i;
        if (slave->group != fieldbus->group) {
            /* This slave is part of another group: do nothing */
        } else if (slave->state != EC_STATE_OPERATIONAL) {
            grp->docheckstate = TRUE;
            if (slave->state == EC_STATE_SAFE_OP + EC_STATE_ERROR) {
                printf("* Slave %d is in SAFE_OP+ERROR, attempting ACK\n", i);
                slave->state = EC_STATE_SAFE_OP + EC_STATE_ACK;
                ecx_writestate(context, i);
            } else if(slave->state == EC_STATE_SAFE_OP) {
                printf("* Slave %d is in SAFE_OP, change to OPERATIONAL\n", i);
                slave->state = EC_STATE_OPERATIONAL;
                ecx_writestate(context, i);
            } else if(slave->state > EC_STATE_NONE) {
                if (ecx_reconfig_slave(context, i, EC_TIMEOUTRET)) {
                    slave->islost = FALSE;
                    printf("* Slave %d reconfigured\n", i);
                }
            } else if(! slave->islost) {
                ecx_statecheck(context, i, EC_STATE_OPERATIONAL, EC_TIMEOUTRET);
                if (slave->state == EC_STATE_NONE) {
                    slave->islost = TRUE;
                    printf("* Slave %d lost\n", i);
                }
            }
        } else if (slave->islost) {
            if(slave->state != EC_STATE_NONE) {
                slave->islost = FALSE;
                printf("* Slave %d found\n", i);
            } else if (ecx_recover_slave(context, i, EC_TIMEOUTRET)) {
                slave->islost = FALSE;
                printf("* Slave %d recovered\n", i);
            }
        }
    }

    if (! grp->docheckstate) {
        printf("All slaves resumed OPERATIONAL\n");
    }
}

int
main(int argc, char *argv[])
{
    Fieldbus fieldbus;

    if (argc != 2) {
        ec_adaptert * adapter = NULL;
        printf("Usage: simple_ng IFNAME1\n"
               "IFNAME1 is the NIC interface name, e.g. 'eth0'\n");

        printf("\nAvailable adapters:\n");
        adapter = ec_find_adapters();
        while (adapter != NULL)
        {
            printf("    - %s  (%s)\n", adapter->name, adapter->desc);
            adapter = adapter->next;
        }
        ec_free_adapters(adapter);
        return 1;
    }

    fieldbus_initialize(&fieldbus, argv[1]);
    if (fieldbus_start(&fieldbus)) {
        int i, min_time, max_time;
        min_time = max_time = 0;
        for (i = 1; i <= 10000; ++i) {
            printf("Iteration %4d:", i);
            if (! fieldbus_dump(&fieldbus)) {
                fieldbus_check_state(&fieldbus);
            } else if (i == 1) {
                min_time = max_time = fieldbus.roundtrip_time;
            } else if (fieldbus.roundtrip_time < min_time) {
                min_time = fieldbus.roundtrip_time;
            } else if (fieldbus.roundtrip_time > max_time) {
                max_time = fieldbus.roundtrip_time;
            }
            osal_usleep(5000);
        }
        printf("\nRoundtrip time (usec): min %d max %d\n", min_time, max_time);
        fieldbus_stop(&fieldbus);
    }

    return 0;
}
