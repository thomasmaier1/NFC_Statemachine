
/*******************************************************************************************************************************************************************************
*******************************************************************						   *************************************************************************************
******************************************************************	NFC Statemachine MAIN	************************************************************************************
*******************************************************************						   *************************************************************************************
*******************************************************************************************************************************************************************************/

/*
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include "xiomodule.h"
#include "platform.h"
#include "xil_printf.h"
#include "def.h"
#include "unistd.h"

//#define UART	1			// if defined, UART messages are sent
#undef	UART				//	undefined = no UART messages

#define STANDBY 0
#define RETRANSMISSION	1		//	[FEATURE]
#define WTX	2			//	[FEATURE]
#define RUID	3			//	[FEATURE]
#define PUPI_SPLIT	4		//	[FEATURE]
#define READ_RF 5			//	[DEBUG]
#define	READ_SEND_RF 6			//	[DEBUG]
#define SEND_RF	7			//	[DEBUG]


int main()
{
	//Declaration
	XIOModule io_module;
	unsigned char rx_buf[100];					//	RX Buffer array, used for all* read operations
	unsigned char tx_buf[1] = {0xAA};				//	TX Buffer array, used for [DEBUG] TX operations
	unsigned char deselect[3] = {0xC2, 0xE0, 0xB4};			//	Fixed deselect message
	unsigned char ruid[100];					//	Buffer for RUID handling
	unsigned char rpupi[100];					//	Buffer for RUID handling
	unsigned int len;						//	Length variable, commonly re-used
	unsigned int len_op;						//	Opcode payload length variable, only written at the beginning
	unsigned int opcode;						// 	Opcode variable, used for statemachine selection
	unsigned int loop_limit = 1;					//	Limit for consecutive execution
	unsigned int loop_count = 0;					//	Counter for consecutive execution
	unsigned int loop_done = 0;					//  Indicator for premature loop break-out
	unsigned char tx_config = 0x00;					//	Config byte for TX operations

	//Initialization
    init_platform();
    init_cp(&io_module);

    while(1)
    {
    	//Reset loop variables
    	loop_done = 0;
		loop_count = 0;

		//Read opcode + payload
		len = read_op_code_stream(rx_buf);
		opcode = rx_buf[0]; //opcode is always first byte of payload
		loop_limit = rx_buf[1]; //loop_limit is always second byte of payload

		//Save opcode payload to extra array (for future use)
		len_op = len-2;
		unsigned char tx_buf_opcode[len_op];
		for (int i = 0; i < len_op; i++)
		{
			tx_buf_opcode[i] = rx_buf[i+2];
		}

			switch(opcode)
			{
/*******************************************************************************************************************************************************************************/
				case RETRANSMISSION: 		//When opcode == 1 -> PCD retransmission handling [PCD TC FEATURE]
					tx_config = 0x01;	//Set TX config to: HEX:1 | CRC:0 | BCC:0
					//while loop for consecutive Retransmission requests
					while ((loop_count < loop_limit) && (loop_done == 0))
					{
						//Read RF
						len = read_nfc_rx_stream(rx_buf, &io_module);

						//Check if first byte is 0xB2 OR 0xB3 (R(NAK) identifier)
						if(rx_buf[0] == 0xB2 || rx_buf[0] == 0xB3)
						{
							//Send opcode payload data
							write_nfc_tx_stream(tx_buf_opcode,len_op,tx_config,&io_module);

							//Optional UART messages
							#ifdef UART
							print("\r\nRetransmission requested!\r\n");
							print("\r\nTX sent:");
							write_uart_buf(tx_buf_opcode, len_op);
							print("\r\nDone!\r\n");
							#endif
						}
						else
						{
							//Do nothing, proceed with TC

							//Optional UART messages
							#ifdef UART
							print("\r\nRetransmission NOT requested!\r\n");
							#endif

							//If no Retransmission is requested, finish Retransmission handling
							loop_done = 1;
						}
						loop_count++;
					}
					break;
/*******************************************************************************************************************************************************************************/
				case WTX: 					//When opcode == 2 -> PICC WTX handling [PICC TC FEATURE]
					tx_config = 0x03;	//Set TX config to: HEX:1 | CRC:0 | BCC:0
					//while loop for consecutive WTX requests
					while ((loop_count <= loop_limit) && (loop_done == 0))
					{
						//Read RF
						len = read_nfc_rx_stream(rx_buf, &io_module);

						//Check if first byte is 0xF2 (S(WTX) identifier)
						if(rx_buf[0] == 0xF2)
						{
							//Check if second byte is correct (WTXM)
							if(rx_buf[1] == 0x00 || rx_buf[1] == 0xC0 || (rx_buf[1] >= 0x3C && rx_buf[1] <= 0x3F) || rx_buf[1] >= 0xFC)
							{
								//Send S(DESELECT) message
								len = sizeof(deselect);
								write_nfc_tx_stream(deselect,len,tx_config,&io_module);

								//Optional UART messages
								#ifdef UART
								print("\r\nWTXM protocol error!\r\n");
								print("\r\nTX sent:");
								write_uart_buf(deselect,len);
								print("\r\nDone!\r\n");
								#endif

								//If no WTX is requested, finish WTX handling
								loop_done = 1;
							}
							else
							{
								//No TX on last run
								if (loop_count < loop_limit)
								{
									//Send S(WTX) response
									write_nfc_tx_stream(rx_buf,2,tx_config,&io_module);
								}

								//Optional UART messages
								#ifdef UART
								print("\r\nWTX requested!\r\n");
								print("\r\nTX sent:");
								write_uart_buf(rx_buf,len);
								print("\r\nDone!\r\n");
								#endif
							}
						}
						else
						{
							//Do nothing, proceed with TC
							#ifdef UART
							print("\r\nWTX not requested!\r\n");
							#endif

							//If no WTX is requested, finish WTX handling
							loop_done = 1;
						}
						loop_count++;
					}
					break;
/*******************************************************************************************************************************************************************************/
				case RUID:					//When opcode == 3 -> PICC RUID/PUPI handling
					tx_config = 0x03;	//Set TX config to: HEX:1 | CRC:1 | BCC:0
					if(loop_limit == 1)		// 'loop_limit' is reused for TypeA/B separation, loop_limit = 1: Type A
					{
						//Read RF -> save UID to rx_buf
						len = read_nfc_rx_stream(rx_buf, &io_module);

						//Add fixed data 0x9370 to beginning of new array, followed by the previously recorded UID
						ruid[0] = 0x93;
						ruid[1] = 0x70;
						for (int i = 0; i < len; i++)
						{
							ruid[i+2] = rx_buf[i];
						}

						//Send generated TX data + CRC
						write_nfc_tx_stream(ruid,len+2,tx_config,&io_module);

						//Optional UART messages
						#ifdef UART
						print("\r\nRUID requested!\r\n");
						print("\r\nTX sent:");
						write_uart_buf(rx_buf,len);
						print("\r\nDone!\r\n");
						#endif
					}
					else
					{
						if(loop_limit == 2)	// 'loop_limit' is reused for TypeA/B separation, loop_limit = 2: Type B
						{
							//Read RF -> save UID to rx_buf
							len = read_nfc_rx_stream(rx_buf, &io_module);

							//Add fixed data 0x1D to beginning of new array, followed by the previously recorded PUPI and fixed bytes 0x00080100
							ruid[0] = 0x1D;
							for (int i = 1; i < 5; i++)
							{
								ruid[i] = rx_buf[i];
							}
							ruid[5] = 0x00;
							ruid[6] = 0x08;
							ruid[7] = 0x01;
							ruid[8] = 0x00;
							len = 9;

							//Send generated TX data + CRC
							write_nfc_tx_stream(ruid,len,tx_config,&io_module);

							//Optional UART messages
							#ifdef UART
							print("\r\nPUPI requested!\r\n");
							print("\r\nTX sent:");
							write_uart_buf(rx_buf,len);
							print("\r\nDone!\r\n");
							#endif
						}
						else
						{
							#ifdef UART
							print("\r\nUnknown Type selected! (opcode payload incorrect)\r\n");
							#endif
						}
					}
					break;
/*******************************************************************************************************************************************************************************/
				case PUPI_SPLIT:				//When opcode == 4 -> PICC RUID/PUPI handling
					//print("\r\npupi_split\r\n");
					tx_config = 0x03;	//Set TX config to: HEX:1 | CRC:1 | BCC:0
					if(loop_limit == 1)		// 'loop_limit' is reused for RX/TX separation
					{
						//loop_limit == 1: Only RX
						//Read RF -> save UID to rx_buf
						len = read_nfc_rx_stream(rx_buf, &io_module);

						//Add fixed data 0x1D to beginning of new array, followed by the previously recorded PUPI and fixed bytes 0x00080100
						rpupi[0] = 0x1D;
						for (int i = 1; i < 5; i++)
						{
							rpupi[i] = rx_buf[i];
						}
						rpupi[5] = 0x00;
						rpupi[6] = 0x08;
						rpupi[7] = 0x01;
						rpupi[8] = 0x00;

						//Optional UART messages
						#ifdef UART
							xil_printf("\r\nRF_RX length:%i",len);
							print("\r\nRPUPI data:\r\n");
							write_uart_buf(rpupi, 9);
							#endif
					}
					else
					{
						if(loop_limit == 2)	// 'loop_limit' is reused for RX/TX separation
						{
							//loop_limit == 2: Only TX
							len = 9;

							//Send generated TX data + CRC
							write_nfc_tx_stream(rpupi,len,tx_config,&io_module);

							//Optional UART messages
							#ifdef UART
								print("\r\nPUPI requested!\r\n");
								print("\r\nTX sent:");
								write_uart_buf(rpupi,len);
								print("\r\nDone!\r\n");
								#endif
						}
						else
						{
							#ifdef UART
							print("\r\nUnknown Type selected! (opcode payload incorrect)\r\n");
							#endif
						}
					}
					break;
/*******************************************************************************************************************************************************************************/
				case READ_RF: 				//When opcode == 5 -> read RF and send over UART [DEBUG]
					//Read RF
					len = read_nfc_rx_stream(rx_buf, &io_module);

					#ifdef UART
					xil_printf("\r\nRF_RX length:%i",len);
					print("\r\nRF data:\r\n");
					write_uart_buf(rx_buf, len);
					#endif
					break;
/*******************************************************************************************************************************************************************************/
				case READ_SEND_RF: 			//When opcode == 6 -> read RF and send RF [DEBUG]
					tx_config = 0x01;	//Set TX config to: HEX:1 | CRC:0 | BCC:0
					//Read RF
					len = read_nfc_rx_stream(rx_buf,&io_module);

					#ifdef UART
					xil_printf("\r\nRF RX length:%i",len);
					print("\r\nRF data:\r\n");
					write_uart_buf(rx_buf, len);
					#endif

					//Send RF
					len = sizeof(tx_buf);
					write_nfc_tx_stream(tx_buf,len,tx_config,&io_module);

					#ifdef UART
					xil_printf("\r\nRF TX length:%i",len);
					print("\r\nSent tx data:%d\r\n");
					write_uart_buf(tx_buf, len);
					#endif
					break;
/*******************************************************************************************************************************************************************************/
				case SEND_RF:				//When opcode == 7 -> send RF [DEBUG]
					tx_config = 0x01;	//Set TX config to: HEX:1 | CRC:0 | BCC:0

					//Send RF
					len = sizeof(tx_buf);
					write_nfc_tx_stream(tx_buf,len,tx_config,&io_module);

					#ifdef UART
					xil_printf("\r\nRF TX length:%i",len);
					print("\r\nSent tx data:%d\r\n");
					write_uart_buf(tx_buf, len);
					#endif
					break;
/*******************************************************************************************************************************************************************************/
				default: 					//	Default -> Do nothing
					break;
/*******************************************************************************************************************************************************************************/
			}
		//Send op_done pulse to finish opcode execution (no actions possible after op_done!)
		op_done(&io_module);
    }
    cleanup_platform();
    return 0;
}
