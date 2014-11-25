/**
 * \addtogroup EtherCAT middleware layer (DRAFT)
 * \{
 */

#ifndef ECAT_H
#define ECAT_H

#include "ethercattype.h"
#include "nicdrv.h"
#include "ethercatbase.h"
#include "ethercatmain.h"
#include "ethercatconfig.h"
#include "ethercatcoe.h"
#include "ethercatdc.h"
#include "ethercatprint.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
  ecx_contextt ctx;
  char * ifname;
  uint8_t * io_map;
  unsigned int expected_wc;
} ecat_net_t;


typedef struct {
  uint32_t a;
  /* TBD */
} ecat_slave_t;

typedef struct {
  uint32_t a;
  /* TBD */
} ecat_statistics_t;

typedef enum {
  ECAT_STATE_INIT    = 0x01,
  ECAT_STATE_PRE_OP  = 0x02,
  ECAT_STATE_BOOT    = 0x03,
  ECAT_STATE_SAFE_OP = 0x04,
  ECAT_STATE_OP      = 0x08,

  ECAT_STATE_ACK     = 0x10,
  ECAT_STATE_ERROR   = 0x10,
} ecat_state_t;

typedef enum {
    ECAT_SLAVE_NO_LINK,
    ECAT_SLAVE_LINK_DETECTED
} ecat_slave_link_detected_t;

typedef enum {
    ECAT_SLAVE_PORT_OPEN,
    ECAT_SLAVE_PORT_CLOSED
} ecat_slave_loop_port_t;

typedef enum {
    ECAT_SLAVE_NO_STABLE_COM,
    ECAT_SLAVE_COM_ESTABLISHED
} ecat_slave_stable_com_t;

typedef struct {
    ecat_slave_link_detected_t link_detected;
    ecat_slave_loop_port_t loop_port;
    ecat_slave_stable_com_t stable_com;
    uint8_t lost_link_counter;
    uint8_t invalid_frame_counter;
    uint8_t rx_error_counter;
    uint8_t forwarded_rx_error_counter;
} ecat_slave_port_status_t;

typedef struct {
    uint8_t watchdog_counter_process_data;
    uint8_t watchdog_counter_pdi;
} ecat_slave_wd_cnt_t;

typedef struct {
    ecat_slave_port_status_t port[4];
    ecat_slave_wd_cnt_t wd_cnt;
    uint8_t ecat_processing_unit_error_counter;
    uint8_t pdi_error_counter;
    uint8_t pdi_error_code;
} ecat_slave_status_t;

typedef struct {
    uint64_t time;
    uint64_t offset;
    uint32_t delay;
    int32_t difference;
} ecat_slave_dc_status_t;

/***********************************************************************
  Example:

  ecat_net_t * net;
  ...

  // This part is done during "boot"
  net = ecat_net_init ("ie1g1", settings);
  num_slaves = ecat_net_scan (net);
  
  if (ecat_net_verify (net, ...) < 0)
  {
    // handle net mismatch
  }
  
  if (ecat_net_get_state (net) != ECAT_STATE_SAFE_OP)
  {
    // handle that not all slaves are in SAFE-OP
  }

  expected_wc = ecat_net_get_expected_wc (net);
  ecat_net_print_slave_states (net);



  // This is done when starting the EtherCAT communication
  if (ecat_net_set_state (net, ECAT_STATE_OP) < 0)
  {
    // handle that not all slaves are in OPERATIONAL
  }



  // When the EtherCAT network is up and running
  // In the start of the cycle process incomming message
  wc = ecat_net_receive (net, 0);
  if (wc != expected_wc)
  {
    // handle invalid working counter
  }

  // In the end of the cycle, send out the message
  ecat_net_send (net);



  // This is done when shutting down the system.
  ecat_net_set_state (net, ECAT_STATE_SAFE_OP);
  // go to other states if needed
  ...
  ecat_net_deinit (net);

 ***********************************************************************/




/***********************************************************************
 *** EtherCAT network                                                ***
 ***********************************************************************/

/**
 * Initiate a master instance, allocates memory.
 * 
 * \return The network handle to be used in all subsequent operations
 * on the EtherCAT network and slaves
 */
ecat_net_t * ecat_net_init(char * ifname); //, ecat_net_settings_t * settings);

/**
 * Sets up network interface, scan the EtherCAT network, set up the IOMAP,
 * configure slaves and bring the slaves up to SAFE OPERATIONAL.
 *
 * \param net          Network handle
 * \return number of slaves found on success, negative number on failure
 */
int ecat_net_scan(ecat_net_t * net);

/**
 * Verifies the scanned network against an expected newtork.
 *
 * \param net       Network handle
 * \param TBD       Expected network
 * \return 0 on success, negative number on failure
 */
int ecat_net_verify(ecat_net_t * net); //, /* TBD */);

/**
 * Tries to change the state on all slaves on the network, for example
 * SAFE OPERATIONAL to OPERATIONAL.
 *
 * \param net       Network handle
 * \param state     State to change to
 * \return 0 on success, negative number on failure
 */
int ecat_net_set_state(ecat_net_t * net, ecat_state_t state);

/**
 * Get the lowest common state for all slaves on the network. If all
 * slaves are in ECAT_STATE_OP and one in ECAT_STATE_SAFE_OP this
 * function will return ECAT_STATE_SAFE_OP.
 *
 * \param net       Network handle
 * \param state     State to change to
 * \return current network state
 */
ecat_state_t ecat_net_get_state(ecat_net_t * net);

/**
 * Sends process data from the IOMAP on the network.
 *
 * \param net       Network handle
 * \return 0 on success, negative number on failure
 */
int ecat_net_send(ecat_net_t * net);

/**
 * Process incomming process data and store it in the IOMAP.
 *
 * \param net          Network handle
 * \param timeout_us   Timeout (us)
 * \return wc on success, negative number on failure
 */
int ecat_net_receive(ecat_net_t * net, unsigned int timeout_us);

/**
 * Closes the network and cleans up the allocated memory.
 *
 * \param net          Network handle
 */
void ecat_net_deinit(ecat_net_t * net);

/**
 * Gets the expected working counter of the network.
 *
 * \param net          Network handle
 * \return the expected network handle of the network
 */
int ecat_net_get_expected_wc(ecat_net_t * net);

/**
 * Query the slaves for slave status and error codes and prints the
 * result to the console.
 *
 * \param net          Network handle
 */
void ecat_net_print_slave_states (ecat_net_t * net);

/**
 * Returns statistics from this master such as number of sent and
 * received frames.
 *
 * \param net          Network handle
 * \param stats        Statistics stuct
 */
void ecat_net_statistics(ecat_net_t * net, ecat_statistics_t * stats);


/***********************************************************************
 *** EtherCAT slave                                                  ***
 ***********************************************************************/

/**
 * Set state on a specific slave.
 *
 * \param net          Network handle
 * \param slave        Slave index
 * \param state        State to go to
 * \return 0 on success, negative number on failure
 */
int ecat_slave_set_state(ecat_net_t * net, uint32_t slave,
                         ecat_state_t state);

/**
 * Get state on a specific slave.
 *
 * \param net          Network handle
 * \param slave        Slave index
 * \return 0 on success, negative number on failure
 */
ecat_state_t ecat_slave_get_state(ecat_net_t * net);

/**
 * Read SDO data from a slave. The `data` area to be large enough to
 * hold the SDO data.
 *
 * \param net          Network handle
 * \param slave        Slave index
 * \param index        CoE index
 * \param subindex     CoE subindex
 * \param data         Pointer to where the data should be stored
 * \return 0 on success, negative number on failure
 */
int ecat_slave_sdo_read(ecat_net_t * net, uint32_t slave, uint16_t index,
                        uint16_t subindex, void * data);
/**
 * Write SDO data to a slave.
 *
 * \param net          Network handle
 * \param slave        Slave index
 * \param index        CoE index
 * \param subindex     CoE subindex
 * \param data         Pointer to the SDO data
 * \return 0 on success, negative number on failure
 */
int ecat_slave_sdo_write(ecat_net_t * net, uint32_t slave, uint16_t index,
                         uint16_t subindex, const void * data);

/**
 * Read PDO data from the IOMAP. The `data` area to be large enough to
 * hold the PDO data.
 *
 * \param net          Network handle
 * \param slave        Slave index
 * \param index        CoE index
 * \param subindex     CoE subindex
 * \param data         Pointer to where the data should be stored
 * \return 0 on success, negative number on failure
 */
int ecat_slave_pdo_read(ecat_net_t * net, uint32_t slave, uint16_t index,
                        uint16_t subindex, void * data);

/**
 * Write PDO data to the IOMAP.
 *
 * \param net          Network handle
 * \param slave        Slave index
 * \param index        CoE index
 * \param subindex     CoE subindex
 * \param data         Pointer to the SDO data
 * \return 0 on success, negative number on failure
 */
int ecat_slave_pdo_write(ecat_net_t * net, uint32_t slave, uint16_t index,
                         uint16_t subindex, const void * data);

/**
 * Read EEPROM data from the slave.
 *
 * \param net          Network handle
 * \param slave        Slave index
 * \param address      EEPROM start address
 * \param size         EEPROM data size
 * \param data         Pointer to where the data should be stored
 * \return 0 on success, negative number on failure
 */
int ecat_slave_eeprom_read(ecat_net_t * net, uint32_t slave, uint32_t address,
                           size_t size, void * data);

/**
 * Write EEPROM data to the slave.
 *
 * \param net          Network handle
 * \param slave        Slave index
 * \param address      EEPROM start address
 * \param size         EEPROM data size
 * \param data         Pointer to EEPROM data
 * \return 0 on success, negative number on failure
 */
int ecat_slave_eeprom_write(ecat_net_t * net, uint32_t slave, uint32_t address,
                            size_t size, const void * data);

/**
 * Read register data from the slave.
 *
 * \param net          Network handle
 * \param slave        Slave index
 * \param address      Register start address
 * \param length       Register data length
 * \param data         Pointer to where the data should be stored
 * \return 0 on success, negative number on failure
 */
int ecat_slave_reg_read(ecat_net_t * net, uint32_t slave, uint32_t address,
                        uint32_t length, void * data);

/**
 * Write register data to the slave.
 *
 * \param net          Network handle
 * \param slave        Slave index
 * \param address      Register start address
 * \param size         Register data size
 * \param data         Pointer to register data
 * \return 0 on success, negative number on failure
 */
int ecat_slave_reg_write(ecat_net_t * net, uint32_t slave, uint32_t address,
                         size_t size, void * data);

/**
 * Read file data from the slave.
 *
 * \param net          Network handle
 * \param slave        Slave index
 * \param filename     File to read
 * \param size         Maximum file size
 * \param data         Pointer to where the data should be stored
 * \return 0 on success, negative number on failure
 */
int ecat_slave_file_read(ecat_net_t * net, uint32_t slave, char * filename,
                         size_t size, void * data);

/**
 * Write file data to the slave.
 *
 * \param net          Network handle
 * \param slave        Slave index
 * \param filename     File to write
 * \param size         File size
 * \param data         Pointer to file data
 * \return 0 on success, negative number on failure
 */
int ecat_slave_file_write(ecat_net_t * net, uint32_t slave, char * filename,
                          size_t size, const void * data);

/**
 * Gets the AL status from a slave.
 *
 * \param net          Network handle
 * \param slave        Slave index
 * \return AL status
 */
uint16_t ecat_slave_get_al_status(ecat_net_t * net, uint32_t slave);

/**
 * Gets the distributed clock status from a slave.
 *
 * \param net          Network handle
 * \param slave        Slave index
 * \param status       Pointer to slave dc status structure
 * \return 0 on success, negative number on failure
 */
int ecat_slave_get_dc_status(ecat_net_t * net, uint32_t slave, 
        ecat_slave_dc_status_t * status);

/**
 * Gets the status from a slave.
 *
 * \param net          Network handle
 * \param slave        Slave index
 * \param status       Pointer to slave status structure
 * \return   0 on success, negative number on failure
 *          -1 DL status read failed
 *          -2 Error counters read failed
 *          -3 DL status and Error counters read failed
 *          -4 Watchdog read failed
 *          -5 DL status and Watchdog read failed
 *          -6 Error counters and Watchdog read failed
 *          -7 DL status and Error counters and Watchdog read failed
 */
int ecat_slave_get_status(ecat_net_t * net, uint32_t slave, 
        ecat_slave_status_t * status);


/***********************************************************************
 *** EtherCAT utils                                                  ***
 ***********************************************************************/

/**
 * Returns the corresponding string for a EtherCAT state.
 *
 * \param state          The state
 * \returns a string
 */
char * ecat_util_state_to_str(ecat_state_t state);

/**
 * Returns the corresponding string for an AL status code.
 *
 * \param alstatus          AL status
 * \returns a string
 */
char * ecat_util_al_status_to_str(ecat_net_t * net, uint16_t alstatus);



#ifdef __cplusplus
}
#endif
#endif /* ECAT_H */

/**
 * \}
 */
