/** \file
 * \brief Example code for Simple Open EtherCAT master
 *
 * Usage: firm_update ifname1 slave fname
 * ifname is NIC interface, f.e. eth0
 * slave = slave number in EtherCAT order 1..n
 * fname = binary file to store in slave
 * CAUTION! Using the wrong file can result in a bricked slave!
 *
 * This is a slave firmware update test.
 *
 * (c)Arthur Ketels 2011
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "ethercat.h"

#define FWBUFSIZE (20 * 1024 * 1024)

uint8 ob;
uint16 ow;
uint32 data;
char filename[256];
char filebuffer[FWBUFSIZE]; // 20MB buffer
int filesize;
int j;
uint16 argslave;

int input_bin(char *fname, int *length)
{
	FILE *fp;

	int cc = 0, c;

	fp = fopen(fname, "rb");
	if(fp == NULL)
		return 0;
	while (((c = fgetc(fp)) != EOF) && (cc < FWBUFSIZE))
		filebuffer[cc++] = (uint8)c;
	*length = cc;
	fclose(fp);
	return 1;
}


int boottest(char *ifname, uint16 slave, char *filename)
{
	int retval = 0;
	printf("Starting firmware update example\n");

	/* initialise SOEM, bind socket to ifname */
	if (ec_init(ifname))
	{
		printf("ec_init on %s succeeded.\n",ifname);
		/* find and auto-config slaves */


	    if ( ec_config_init(FALSE) > 0 )
		{
			int counter = 3;
			char sm_zero[sizeof(ec_smt)*2];
			char fmmu_zero[sizeof(ec_fmmut)];
			unsigned char rd_stat = 0xff;

			memset(sm_zero, 0, sizeof(sm_zero));
			memset(fmmu_zero, 0, sizeof(fmmu_zero));

			printf("%d slaves found and configured.\n",ec_slavecount);

			counter = 3;
			do {
				/* wait for slave to reach PREOP state */
				if (ec_statecheck(slave, EC_STATE_PRE_OP,  EC_TIMEOUTSTATE * 4) == EC_STATE_PRE_OP) {
					printf("Slave is in PREOP state\n");
					counter = 0;
				}
			}
			while (counter-- > 0);

			counter = 3;
			do {
				printf("Request init state for slave %d\n", slave);
				ec_slave[slave].state = EC_STATE_INIT;
				ec_writestate(slave);

				/* wait for slave to reach INIT state */
				if (ec_statecheck(slave, EC_STATE_INIT,  EC_TIMEOUTSTATE * 4) == EC_STATE_INIT) {
					counter = 0;
				}
			}
			while (counter-- > 0);
			printf("Slave %d state to INIT. counter: %d\n", slave, counter);

			/* clean FMMUs */
			ec_FPWR(ec_slave[slave].configadr, ECT_REG_FMMU0, sizeof(fmmu_zero), fmmu_zero, EC_TIMEOUTRET3);
			ec_FPWR(ec_slave[slave].configadr, ECT_REG_FMMU1, sizeof(fmmu_zero), fmmu_zero, EC_TIMEOUTRET3);

			/* clean all SM0 / SM1 to set new bootstrap values later */
			ec_FPWR(ec_slave[slave].configadr, ECT_REG_SM0, sizeof(ec_smt) * 2, sm_zero, EC_TIMEOUTRET3);
			ec_FPRD(ec_slave[slave].configadr, ECT_REG_SM1STAT, 1, &rd_stat, EC_TIMEOUTRET3);
			printf("Read back Status of SM1: %02x\n", rd_stat);

			/* read BOOT mailbox data, master -> slave */
			data = ec_readeeprom(slave, ECT_SII_BOOTRXMBX, EC_TIMEOUTEEP);
			ec_slave[slave].SM[0].StartAddr = (uint16)LO_WORD(data);
            		ec_slave[slave].SM[0].SMlength = (uint16)HI_WORD(data);
			/* store boot write mailbox address */
			ec_slave[slave].mbx_wo = (uint16)LO_WORD(data);
			/* store boot write mailbox size */
			ec_slave[slave].mbx_l = (uint16)HI_WORD(data);

			/* read BOOT mailbox data, slave -> master */
			data = ec_readeeprom(slave, ECT_SII_BOOTTXMBX, EC_TIMEOUTEEP);
			ec_slave[slave].SM[1].StartAddr = (uint16)LO_WORD(data);
                        ec_slave[slave].SM[1].SMlength = (uint16)HI_WORD(data);
			/* store boot read mailbox address */
			ec_slave[slave].mbx_ro = (uint16)LO_WORD(data);
			/* store boot read mailbox size */
			ec_slave[slave].mbx_rl = (uint16)HI_WORD(data);

			printf(" SM0 A:%4.4x L:%4d F:%8.8x\n", ec_slave[slave].SM[0].StartAddr, ec_slave[slave].SM[0].SMlength,
			    (int)ec_slave[slave].SM[0].SMflags);
			printf(" SM1 A:%4.4x L:%4d F:%8.8x\n", ec_slave[slave].SM[1].StartAddr, ec_slave[slave].SM[1].SMlength,
			    (int)ec_slave[slave].SM[1].SMflags);
			/* program SM0 mailbox in for slave */
			ec_FPWR (ec_slave[slave].configadr, ECT_REG_SM0, sizeof(ec_smt), &ec_slave[slave].SM[0], EC_TIMEOUTRET);
			/* program SM1 mailbox out for slave */
			ec_FPWR (ec_slave[slave].configadr, ECT_REG_SM1, sizeof(ec_smt), &ec_slave[slave].SM[1], EC_TIMEOUTRET);

			/* give EEPROM control to PDI */
			rd_stat = 1;
			ec_FPWR (ec_slave[slave].configadr, ECT_REG_EEPCFG, 1, &rd_stat, EC_TIMEOUTRET);

			counter = 3;
			do {
				uint16 ret;
				printf("Request BOOT state for slave %d\n", slave);
				ec_slave[slave].state = EC_STATE_BOOT;
				printf("ec_writestate returned: %d\n", ec_writestate(slave));
				ret = ec_statecheck(slave, EC_STATE_BOOT,  EC_TIMEOUTSTATE );
				printf("ec_statecheck returned: %d\n", ret);
				if (ret	== EC_STATE_BOOT)
					counter = 0;
			} while(counter-- > 0);

			printf("check BOOT state again\n");

			/* wait for slave to reach BOOT state */
			if (ec_statecheck(slave, EC_STATE_BOOT,  EC_TIMEOUTSTATE * 10) == EC_STATE_BOOT)
			{
				printf("Slave %d state to BOOT.\n", slave);

				if (input_bin(filename, &filesize))
				{
					char *short_filename = strrchr(filename, '/'); /* search for last / */
					if (short_filename)
						short_filename++; /* jump over the \ */
					else
						short_filename = filename; /* no \ found -> must be a short filename */
					printf("File read OK, %d bytes.\n",filesize);
					printf("FoE write %s....\n", short_filename);
					j = ec_FOEwrite(slave, short_filename, 0, filesize , &filebuffer, EC_TIMEOUTSTATE);
					printf("result %d.\n",j);
					if (j == 0) retval = 5;
					else if (j != 1) retval = j;

					if (retval == 0) {
						/* wait max 600s => 10 minutes for firmware update to complete */
						counter = 60;
						retval = 6; /* only success switch to init state afterwards completes firmware update */
						do {
							uint16 ret;
							printf("Request init state for slave %d (counter: %d)\n", slave, counter);
							ec_slave[slave].state = EC_STATE_INIT;
							ec_writestate(slave);
							ret = ec_statecheck(slave, EC_STATE_INIT, 10000000);
							if (ret == 0) {
								int count;
								if ((count = ec_config_init(FALSE)) >= slave) {
									ec_slave[slave].state = EC_STATE_INIT;
									ec_writestate(slave);
									ret = ec_statecheck(slave, EC_STATE_INIT, EC_TIMEOUTSTATE * 10);
								}
								else {
									printf("Slaves found: %d Needed: %d .. will try it again\n", count, slave);
#ifndef WIN32
									sleep(10);
#endif
									ret = 25555;
								}
							}
							if (ret == EC_STATE_INIT) {
								counter = 0;
								printf("State change to init success\n");
								retval = 0;
							}
						} while (counter-- > 0);
					}
				}
				else
				{
					printf("File not read OK.\n");
					retval = 4;
				}
			}
			else {
				printf("Failed to enter boot mode\n");
				retval = 3;
			}

		}
		else
		{
			printf("No slaves found!\n");
			retval = 2;
		}
		printf("End firmware update example, close socket\n");
		/* stop SOEM, close socket */
		ec_close();
	}
	else
	{
		printf("No socket connection on %s\nExcecute as root\n",ifname);
		retval = 1;
	}
	return retval;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	printf("SOEM (Simple Open EtherCAT Master)\nFirmware update example\n");

	if (argc > 3)
	{
		argslave = atoi(argv[2]);
		ret = boottest(argv[1], argslave, argv[3]);
	}
	else
	{
		printf("Usage: firm_update ifname1 slave fname\n");
		printf("ifname = eth0 for example\n");
		printf("slave = slave number in EtherCAT order 1..n\n");
		printf("fname = binary file to store in slave\n");
		printf("CAUTION! Using the wrong file can result in a bricked slave!\n");
	}

	printf("End program\n");
	return ret;
}
