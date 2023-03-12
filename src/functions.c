
/*******************************************************************************************************************************************************************************
****************************************************************						   *************************************************************************************
*************************************************************** NFC Statemachine FUNCTIONS	************************************************************************************
****************************************************************						   *************************************************************************************
*******************************************************************************************************************************************************************************/

#include "def.h"

//initialization functions
void init_cp(XIOModule *gpo)
{
	// init IO module
	XIOModule_Initialize(gpo, XPAR_IOMODULE_0_DEVICE_ID);

	// interrupt setup
	//microblaze_register_handler(XIOModule_DeviceInterruptHandler, XPAR_IOMODULE_0_DEVICE_ID );

	XIOModule_Start(gpo);
	//XIOModule_Connect(gpo, XPAR_IOMODULE_0_SYSTEM_INTC_INTERRUPT_0_INTR, ext_int_ISR, XPAR_IOMODULE_0_DEVICE_ID);
	//XIOModule_Enable(gpo, XPAR_IOMODULE_0_SYSTEM_INTC_INTERRUPT_0_INTR);
	//microblaze_enable_interrupts();
}

//transmits single pulse (~272ns) to TC engine when current operation is finished
void op_done(XIOModule *gpo)
{
	XIOModule_DiscreteWrite(gpo, 1, 0x1);
	XIOModule_DiscreteWrite(gpo, 1, 0x0);
}

//sets rx bit of gpo port to high or low
void rx_set(XIOModule *gpo, unsigned int val)
{
	if (val == 1)
	XIOModule_DiscreteWrite(gpo, 1, 0x02);
	else
	XIOModule_DiscreteWrite(gpo, 1, 0x00);
}

//sets tx bit of gpo port to high or low
void tx_set(XIOModule *gpo, unsigned int val)
{
	if (val == 1)
	XIOModule_DiscreteWrite(gpo, 1, 0x04);
	else
	XIOModule_DiscreteWrite(gpo, 1, 0x00);
}

void ext_int_ISR()
{
	print("Interrupt Service Routine\n\r");
}

//function reads opcode data from axi stream id=1 - first element of data buffer contains op_code, 2nd element starts with payload
unsigned int read_op_code_stream(unsigned char *data_buf)
{
	unsigned int axi_data;
	unsigned int ptr;
	char error;
	unsigned int msr;
	ptr = 0;

	do{
		// blocking get
		cgetfsl(axi_data, S_AXIS_OP_CODE);
		if(ptr==0)
			data_buf[ptr] = axi_data & 0xFF;
		else
			data_buf[ptr] = (axi_data >> 8) & 0xFF;

		ptr = ptr + 1;

		//if tlast = 0, error = 1
		fsl_iserror(error);

		// clear error flag if occured
		if(error){
			msr = mfmsr();
			msr &= 0xFFFFFFEF; // clear Bit 4 (inverse order --> Bit 27 in Xilinx document)
			mtmsr(msr);
		}
	}while(error);

	return ptr;	//length
}

//function reads NFC data from axi stream id=0
unsigned int read_nfc_rx_stream(unsigned char *data_buf, XIOModule *io_module)
{
	unsigned int axi_data;
	unsigned int ptr;
	char error;
	unsigned int msr;
	ptr = 0;

	rx_set(io_module,1); //set rx active signal high
	rx_set(io_module,0); //set rx active signal low

	do{
		// blocking get
		cgetfsl(axi_data, S_AXIS_NFC_RX);
		data_buf[ptr] = axi_data & 0xFF;
		ptr = ptr + 1;

		//if tlast = 0, error = 1
		fsl_iserror(error);

		// clear error flag if occured
		if(error){
			msr = mfmsr();
			msr &= 0xFFFFFFEF; // clear Bit 4 (inverse order --> Bit 27 in Xilinx document)
			mtmsr(msr);
		}
	}while(error);
	return ptr;	//length
}

//function transmits data to NFC axi stream interface
void write_nfc_tx_stream(unsigned char *data, unsigned int len, unsigned char cfg, XIOModule *io_module)
{
	unsigned int x;
	unsigned int data_out;

	tx_set(io_module,1); //set tx active signal high
	tx_set(io_module,0); //set tx active signal low

	for(x=0; x < len-1 ;x++)
	{
		//cfg byte fixed to 0x01 (hex only)
		data_out = 0x0100;
		data_out |= data[x];
		putfsl(data_out, M_AXIS_NFC_TX);
		//xil_printf("\r\nRF TX data:%X",data_out);
	}

	//cfg byte = transmitted variable
	data_out = cfg;
	data_out = data_out << 8;
	data_out &= 0xFF00;
	data_out |= data[x];

	cputfsl(data_out, M_AXIS_NFC_TX);	//Last TX for tlast
	//xil_printf("\r\nRF TX data:%X",data_out);
}

//send all buffer data to uart
void write_uart_buf(unsigned char *data, unsigned int len)
{
	unsigned int x;

	for(x=0; x < len ;x++)
	{
		//outbyte(data[x]);		//send single byte to uart
		outbyte(data[x] + 48);	//send single byte to uart (ASCII)
	}
}


