/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

#include "slaveinfo/slaveinfo.h"
#include "simple_test/simple_test.h"
#include "eepromtool/eepromtool.h"

#include "soem_cmds.h"

static int _cmd_slaveinfo (int argc, char * argv[])
{
   return slaveinfo (argc, argv);
}

static int _cmd_simple_test (int argc, char * argv[])
{
   return simple_test (argc, argv);
}

static int _cmd_eepromtool (int argc, char * argv[])
{
   return eepromtool (argc, argv);
}

const shell_cmd_t cmd_slaveinfo =
{
   .cmd = _cmd_slaveinfo,
   .name = "slaveinfo",
   .help_short = "show EtherCAT slave info",
   .help_long =
   "Usage: slaveinfo ifname [options]\n"
   "\n"
   "Find all EtherCAT slaves and show slave configuration.\n"
   "ifname will be ignored but must be given.\n"
   "Options :\n"
   " -sdo : print SDO info\n"
   " -map : print mapping\n"
};

const shell_cmd_t cmd_simple_test =
{
   .cmd = _cmd_simple_test,
   .name = "simple_test",
   .help_short = "EtherCAT process data test",
   .help_long =
   "Usage: simple_test ifname\n"
   "\n"
   "Send and receive (dummy) processdata on EtherCAT bus.\n"
   "ifname will be ignored but must be given.\n"
};

const shell_cmd_t cmd_eepromtool =
{
   .cmd = _cmd_eepromtool,
   .name = "eepromtool",
   .help_short = "program eeprom of EtherCAT slave",
   .help_long =
   "Usage: eepromtool ifname slave [options] fname|alias\n"
   "\n"
   "Program the EEPROM of an EtherCAT slave.\n"
   "ifname will be ignored but must be given.\n"
   "slave = slave number in EtherCAT order 1..n\n"
   "    -i      display EEPROM information\n"
   "    -walias write slave alias\n"
   "    -r      read EEPROM, output binary format\n"
   "    -ri     read EEPROM, output Intel Hex format\n"
   "    -w      write EEPROM, input binary format\n"
   "    -wi     write EEPROM, input Intel Hex format\n"
};
