
/*******************************************************************************************************************************************************************************
*****************************************************************						  *************************************************************************************
****************************************************************  NFC Statemachine HEADER  ************************************************************************************
*****************************************************************						  *************************************************************************************
*******************************************************************************************************************************************************************************/

#include "fsl.h"
#include "xiomodule.h"
#include "mb_interface.h"

//definitions for axi stream interfaces
#define M_AXIS_NFC_TX	0
#define S_AXIS_NFC_RX	0
#define S_AXIS_OP_CODE	1

void init_cp(XIOModule *gpo);
void op_done(XIOModule *gpo); //cp_done signal pulse
void rx_set(XIOModule *gpo, unsigned int val); //rx active signal
void tx_set(XIOModule *gpo, unsigned int val); //tx active signal

unsigned int read_op_code_stream(unsigned char *data_buf);
unsigned int read_nfc_rx_stream(unsigned char *data_buf, XIOModule *io_module);
void write_nfc_tx_stream(unsigned char *data, unsigned int len, unsigned char cfg, XIOModule *io_module);

void write_uart_buf(unsigned char *data, unsigned int len);

void ext_int_ISR();
