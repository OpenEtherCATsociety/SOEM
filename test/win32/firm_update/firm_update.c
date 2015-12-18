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

#define FWBUFSIZE (8 * 1024 * 1024)

uint8 ob;
uint16 ow;
uint32 data;
char filename[256];
char filebuffer[FWBUFSIZE]; // 8MB buffer
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


void boottest(char *ifname, uint16 slave, char *filename)
{
	printf("Starting firmware update example\n");

	/* initialise SOEM, bind socket to ifname */
	if (ec_init(ifname))
	{
		printf("ec_init on %s succeeded.\n",ifname);
		/* find and auto-config slaves */


	    if ( ec_config_init(FALSE) > 0 )
		{
			printf("%d slaves found and configured.\n",ec_slavecount);

			printf("Request init state for slave %d\n", slave);
			ec_slave[slave].state = EC_STATE_INIT;
			ec_writestate(slave);

			/* wait for slave to reach INIT state */
			ec_statecheck(slave, EC_STATE_INIT,  EC_TIMEOUTSTATE * 4);
			printf("Slave %d state to INIT.\n", slave);

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

			printf("Request BOOT state for slave %d\n", slave);
			ec_slave[slave].state = EC_STATE_BOOT;
			ec_writestate(slave);

			/* wait for slave to reach BOOT state */
			if (ec_statecheck(slave, EC_STATE_BOOT,  EC_TIMEOUTSTATE * 10) == EC_STATE_BOOT)
			{
				printf("Slave %d state to BOOT.\n", slave);

				if (input_bin(filename, &filesize))
				{
					printf("File read OK, %d bytes.\n",filesize);
					printf("FoE write....");
					j = ec_FOEwrite(slave, filename, 0, filesize , &filebuffer, EC_TIMEOUTSTATE);
					printf("result %d.\n",j);
					printf("Request init state for slave %d\n", slave);
					ec_slave[slave].state = EC_STATE_INIT;
					ec_writestate(slave);
				}
				else
				    printf("File not read OK.\n");
			}

		}
		else
		{
			printf("No slaves found!\n");
		}
		printf("End firmware update example, close socket\n");
		/* stop SOEM, close socket */
		ec_close();
	}
	else
	{
		printf("No socket connection on %s\nExcecute as root\n",ifname);
	}
}

int main(int argc, char *argv[])
{
	printf("SOEM (Simple Open EtherCAT Master)\nFirmware update example\n");

	if (argc > 3)
	{
		argslave = atoi(argv[2]);
		boottest(argv[1], argslave, argv[3]);
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
	return (0);
}
