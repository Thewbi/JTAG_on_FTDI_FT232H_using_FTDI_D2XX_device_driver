//#include <windows.h>
//#include "tchar.h"
//#include <stdio.h>
//
//int _tmain(int argc, _TCHAR* argv[])
//{
//	printf("test\n");
//
//	return 0;
//}

// https://ftdichip.com/Documents/AppNotes/AN_129_FTDI_Hi_Speed_USB_To_JTAG_Example.pdf

// AN_129_Hi-Speed_JTAG_with_MPSSE.cpp : Defines the entry point for the console application.
//
//#include "stdafx.h"
#include <windows.h>
#include "tchar.h"
#include <stdio.h>
#include "ftd2xx.h"

constexpr int input_buffer_length = 1024;

int _tmain(int argc, _TCHAR* argv[])
{
	FT_HANDLE ftHandle; // Handle of the FTDI device
	FT_STATUS ftStatus; // Result of each D2XX call
	DWORD dwNumDevs; // The number of devices
	unsigned int uiDevIndex = 0xF; // The device in the list that is used
	BYTE byOutputBuffer[1024]; // Buffer to hold MPSSE commands and data to be sent to the FT2232H
	BYTE byInputBuffer[input_buffer_length]; // Buffer to hold data read from the FT2232H
	DWORD dwCount = 0; // General loop index
	DWORD dwNumBytesToSend = 0; // Index to the output buffer
	DWORD dwNumBytesSent = 0; // Count of actual bytes sent - used with FT_Write
	DWORD dwNumBytesToRead = 0; // Number of bytes available to read in the driver's input buffer
	DWORD dwNumBytesRead = 0; // Count of actual bytes read - used with FT_Read
	DWORD dwClockDivisor = 0x05DB; // Value of clock divisor, SCL Frequency = 60/((1+0x05DB)*2) (MHz) = 20khz
	
	//
	// 1. FT_CreateDeviceInfoList()
	// 
	
	// Does an FTDI device exist?
	printf("Checking for FTDI devices...\n");
	ftStatus = FT_CreateDeviceInfoList(&dwNumDevs);
	
	// Get the number of FTDI devices
	if (ftStatus != FT_OK) // Did the command execute OK?
	{
		printf("Error in getting the number of devices\n");
		return 1; // Exit with error
	}
	
	if (dwNumDevs < 1) // Exit if we don't see any
	{
		printf("There are no FTDI devices installed\n");
		return 1; // Exist with error
	}
	printf("%d FTDI devices found - the count includes individual ports on a single chip\n", dwNumDevs);

	// Open the port - For this application note, assume the first device is a FT2232H or FT4232H
	// Further checks can be made against the device descriptions, locations, serial numbers, etc.
	// before opening the port.



	//
	// 1. FT_Open()
	// 

	printf("\nAssume first device has the MPSSE and open it...\n");
	ftStatus = FT_Open(0, &ftHandle);
	if (ftStatus != FT_OK)
	{
		printf("Open Failed with error %d\n", ftStatus);
		return 1; // Exit with error
	}

	// Configure port parameters
	printf("\nConfiguring port for MPSSE use...\n");
	ftStatus |= FT_ResetDevice(ftHandle);

	// Reset USB device
	// Purge USB receive buffer first by reading out all old data from FT2232H receive buffer
	ftStatus |= FT_GetQueueStatus(ftHandle, &dwNumBytesToRead);
	
	// Get the number of bytes in the FT2232H receive buffer
	if ((ftStatus == FT_OK) && (dwNumBytesToRead > 0))
	{
		FT_Read(ftHandle, &byInputBuffer, dwNumBytesToRead, &dwNumBytesRead);
	}

	//
	// 1. FT_SetUSBParameters()
	//
	
	// Read out the data from FT2232H receive buffer
	// Set USB request transfer size
	// Set USB request transfer sizes to 64K
	ftStatus |= FT_SetUSBParameters(ftHandle, 65536, 65535);
	
	// Disable event and error characters
	ftStatus |= FT_SetChars(ftHandle, false, 0, false, 0);
	
	// Sets the read and write timeouts in 5 sec for the FT2232H
	// Sets the read and write timeouts in milliseconds
	ftStatus |= FT_SetTimeouts(ftHandle, 0, 5000);	
	
	// Set the latency timer
	// Set the latency timer (default is 16mS)
	ftStatus |= FT_SetLatencyTimer(ftHandle, 16);
	
	// Reset controller
	ftStatus |= FT_SetBitMode(ftHandle, 0x0, 0x00);

	// Enable MPSSE mode
	ftStatus |= FT_SetBitMode(ftHandle, 0x0, 0x02);

	// check for any errors
	if (ftStatus != FT_OK)
	{
		printf("Error in initializing the MPSSE %d\n", ftStatus);
		FT_Close(ftHandle);

		// Exit with error
		return 1;
	}

	// Wait for all the USB stuff to complete and work
	Sleep(3000);


	// https://atadiat.com/en/e-ftdi-mpsse-engine-programming-basics-a-gui-example/


	
	// -----------------------------------------------------------
	// At this point, the MPSSE is ready for commands
	// -----------------------------------------------------------
	// -----------------------------------------------------------
	// Synchronize the MPSSE by sending a bogus opcode (0xAA),
	// The MPSSE will respond with "Bad Command" (0xFA) followed by
	// the bogus opcode itself.
	// -----------------------------------------------------------

	// Make sure that your application and MPSSE are in a right sync. 
	// For this end, MPSSE has a special command called ’bad command’ 
	// and when it is detected, the MPSSE returns the value of 0xFA, 
	// followed by the byte that caused the bad command.

	// To send a command between your application and MPSSE via USB, 
	// you need to send the data using ‘FT_Write’  api.



	byOutputBuffer[dwNumBytesToSend++] = 0xAA;//'\xAA';
	
	// Add bogus command ‘xAA’ to the queue
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
	
	// Send off the BAD commands
	dwNumBytesToSend = 0;

	// Reset output buffer pointer
	do
	{
		ftStatus = FT_GetQueueStatus(ftHandle, &dwNumBytesToRead);
		// Get the number of bytes in the device input buffer
	} while ((dwNumBytesToRead == 0) && (ftStatus == FT_OK));
	
	// or Timeout
	bool bCommandEchod = false;
	ftStatus = FT_Read(ftHandle, &byInputBuffer, dwNumBytesToRead, &dwNumBytesRead);
	
	// Read out the data from input buffer
	for (dwCount = 0; dwCount < dwNumBytesRead - 1; dwCount++)
	{
		// check if Bad command and echo command received
		if ((byInputBuffer[dwCount] == 0xFA) && (byInputBuffer[dwCount + 1] == 0xAA))
		{
			bCommandEchod = true;
			break;
		}
	}

	if (bCommandEchod == false)
	{
		printf("Error in synchronizing the MPSSE\n");
		FT_Close(ftHandle);

		// Exit with error
		return 1; 
	}

	// -----------------------------------------------------------
	// Configure the MPSSE settings for JTAG
	// Multiple commands can be sent to the MPSSE with one FT_Write
	// -----------------------------------------------------------



	// Start with a fresh index
	dwNumBytesToSend = 0;


	
	
	//
	// Set up the Hi-Speed specific commands for the FTx232H
	//

	// Use 60MHz master clock (disable divide by 5)
	byOutputBuffer[dwNumBytesToSend++] = 0x8A;

	// Turn off adaptive clocking (may be needed for ARM)
	byOutputBuffer[dwNumBytesToSend++] = 0x97;

	// Disable three-phase clocking
	byOutputBuffer[dwNumBytesToSend++] = 0x8D;

	// Send off the HS-specific commands
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);

	// Reset output buffer pointer
	dwNumBytesToSend = 0;
	



	
	
	
	 
	//
	// Set initial states of the MPSSE interface - low byte, both pin directions and output values
	//
	
	// Pin name                    | Signal Direction Config | Initial State Config
	// ---------------------------------------------------------------------------
	// AD_BUS_0 TCK/SK clock       | output 1                | low  0
	// AD_BUS_1 TDI/D0             | output 1                | low  0
	// AD_BUS_2 TDO/DI             | input  0                | low  0
	// AD_BUS_3 TMS/CS chip select | output 1                | high 1
	// AD_BUS_4 GPIOL0             | input  0                | low  0
	// AD_BUS_5 GPIOL1             | input  0                | low  0
	// AD_BUS_6 GPIOL2             | input  0                | low  0
	// AD_BUS_7 GPIOL3             | input  0                | low  0
	// ---------------------------------------------------------------------------
	// 0x80                        | 0x0B (lsb first)        | 0x08 (lsb first)

	// column 1 in the table above. 0x80 targets the first eight lines
	// which happen to be the pins from column 1
	byOutputBuffer[dwNumBytesToSend++] = 0x80; 

	// Set data bits low-byte of MPSSE port (column 3 in the table above)
	byOutputBuffer[dwNumBytesToSend++] = 0x08;

	// Initial state config above (column 2 in the table above)
	byOutputBuffer[dwNumBytesToSend++] = 0x0B;

	// Direction config above
	// https://ftdichip.com/wp-content/uploads/2023/09/D2XX_Programmers_Guide.pdf
	
	// Send off the low GPIO config commands
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);

	// Reset output buffer pointer
	dwNumBytesToSend = 0;

	

	
	//
	// Set initial states of the MPSSE interface - high byte, 
	//
	
	// both pin directions and output values
	// Pin name | Signal        |  Direction Config       | Initial State Config
	// --------------------------------------------------------------
	// ACBUS0   | GPIOH0        | input 0                 | low  0
	// ACBUS1   | GPIOH1        | input 0                 | low  0
	// ACBUS2   | GPIOH2        | input 0                 | low  0
	// ACBUS3   | GPIOH3        | input 0                 | low  0
	// ACBUS4   | GPIOH4        | input 0                 | low  0
	// ACBUS5   | GPIOH5        | input 0                 | low  0
	// ACBUS6   | GPIOH6        | input 0                 | low  0
	// ACBUS7   | GPIOH7        | input 0                 | low  0
	// ---------------------------------------------------------------------------
	// 0x82                     | 0x00 (lsb first)        | 0x00 (lsb first)

	// see: https://atadiat.com/en/e-ftdi-mpsse-engine-programming-basics-a-gui-example/
	// 
	// 0x82 selects the third eight lines, which happen to be the pins in
	// the first column above
	byOutputBuffer[dwNumBytesToSend++] = 0x82;

	// Set data bits low-byte of MPSSE port
	byOutputBuffer[dwNumBytesToSend++] = 0x00;

	// Initial state config above
	byOutputBuffer[dwNumBytesToSend++] = 0x00;

	// Direction config above
	// Send off the high GPIO config commands
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);

	// Reset output buffer pointer
	dwNumBytesToSend = 0;







	//
	// Clock settings
	// Set TCK frequency
	// 
	
	

	// Set TCK frequency
	// TCK = 60MHz /((1 + [(1 +0xValueH*256) OR 0xValueL])*2)
	byOutputBuffer[dwNumBytesToSend++] = '\x86';

	// Command to set clock divisor
	byOutputBuffer[dwNumBytesToSend++] = dwClockDivisor & 0xFF;

	// Set 0xValueL of clock divisor
	byOutputBuffer[dwNumBytesToSend++] = (dwClockDivisor >> 8) & 0xFF;

	// Set 0xValueH of clock divisor
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);

	// Reset output buffer pointer
	dwNumBytesToSend = 0;






	//
	// Send off the clock divisor commands
	// https://www.ftdichip.com/Support/Documents/AppNotes/AN_135_MPSSE_Basics.pdf
	// Section 3.2 Clocks
	// 
	// https://ftdichip.com/Documents/AppNotes/AN_129_FTDI_Hi_Speed_USB_To_JTAG_Example.pdf
	// 
	
	

	// Disable internal loop-back
	byOutputBuffer[dwNumBytesToSend++] = 0x85;

	// Send off the loopback command
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);

	// Reset output buffer pointer
	dwNumBytesToSend = 0;






	// https://ftdichip.com/Documents/AppNotes/AN_129_FTDI_Hi_Speed_USB_To_JTAG_Example.pdf
	//



	/**/

	//
	// Reset all state machines.
	// Navigate to Debug-Logic-Reset (send five ones)
	// because this is the well-known initial start state
	//

	dwNumBytesToSend = 0;
	byOutputBuffer[dwNumBytesToSend++] = 0x4B; // 0x4B : TMS with LSB first on -ve clk edge - use if clk is set to '0'
	byOutputBuffer[dwNumBytesToSend++] = 0x04; // length = 5 bit
	byOutputBuffer[dwNumBytesToSend++] = 0x1F; // 00011111
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
	dwNumBytesToSend = 0;

	

	//
	// Enter Shift-DR state by sending 0, 1, 0, 0
	// 
	// Debug-Logic-Reset -0-> 
	// Run-Test/Idle -1-> 
	// Select-DR-Scan -0-> 
	// Caputre-DR -0->
	// SHIFT-DR
	// 

	dwNumBytesToSend = 0;
	//byOutputBuffer[dwNumBytesToSend++] = 0x4A; // write TMS without read (TMS with LSB first on +ve clk edge - use if clk is set to '1')
	byOutputBuffer[dwNumBytesToSend++] = 0x4B; // write TMS without read (TMS with LSB first on -ve clk edge - use if clk is set to '0')
	byOutputBuffer[dwNumBytesToSend++] = 0x03; // length = 4 bit
	byOutputBuffer[dwNumBytesToSend++] = 0x02; // 00000010
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
	dwNumBytesToSend = 0;

	//// shift out 32 bits (IDCODE register is 32 bits wide)
	//dwNumBytesToSend = 0;
	//byOutputBuffer[dwNumBytesToSend++] = 0x3B; // write bits to TDI (out on -ve edge, in on +ve edge)
	////byOutputBuffer[dwNumBytesToSend++] = 0x3E; // write bits to TDI (out on +ve edge, in on -ve edge)
	////byOutputBuffer[dwNumBytesToSend++] = 0x6B; // TMS command

	//byOutputBuffer[dwNumBytesToSend++] = 0x07; // eight bit

	//byOutputBuffer[dwNumBytesToSend++] = 0x01; // 11111111
	//ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
	//dwNumBytesToSend = 0;


	dwNumBytesToSend = 0;
	byOutputBuffer[dwNumBytesToSend++] = 0x39; // 3.4.9 Clock Data Bytes In and Out LSB first 

	byOutputBuffer[dwNumBytesToSend++] = 0x03; // length low (4 byte)
	byOutputBuffer[dwNumBytesToSend++] = 0x00; // length high

	byOutputBuffer[dwNumBytesToSend++] = 0x00; // byte 0
	byOutputBuffer[dwNumBytesToSend++] = 0x00; // byte 1
	byOutputBuffer[dwNumBytesToSend++] = 0x00; // byte 2
	byOutputBuffer[dwNumBytesToSend++] = 0x00; // byte 3

	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);

	dwNumBytesToSend = 0;


	Sleep(100);

	memset(byInputBuffer, 0, input_buffer_length);

	dwNumBytesToRead = 0;
	do
	{
		// Get the number of bytes in the device input buffer or Timeout
		ftStatus = FT_GetQueueStatus(ftHandle, &dwNumBytesToRead);
	} while ((dwNumBytesToRead == 0) && (ftStatus == FT_OK));
	ftStatus = FT_Read(ftHandle, &byInputBuffer, dwNumBytesToRead, &dwNumBytesRead);
	
	printf("done");
	
	/*
	//
	// Enter Shift-IR state by sending 0, 1, 1, 0, 0
	// 
	// Debug-Logic-Reset -0-> 
	// Run-Test/Idle -1-> 
	// Select-DR-Scan -1-> 
	// Select-IR-Scan -0-> 
	// Caputer-IR -0-> 
	// Shift-IR
	// 
	// Shift-IR state is important because in the next step it is possible to shift
	// data into the IR register
	//

	dwNumBytesToSend = 0;
	//byOutputBuffer[dwNumBytesToSend++] = 0x4A; // write TMS without read (TMS with LSB first on +ve clk edge - use if clk is set to '1')
	byOutputBuffer[dwNumBytesToSend++] = 0x4B; // write TMS without read (TMS with LSB first on -ve clk edge - use if clk is set to '0')
	byOutputBuffer[dwNumBytesToSend++] = 0x04; // length = 5 bit
	byOutputBuffer[dwNumBytesToSend++] = 0x06; // 00000110
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
	dwNumBytesToSend = 0;
	*/



	//
	// Write the BYPASS instruction into all IR registers of the daisy chained devices
	//

/*
	for (int i = 0; i < 100; i++) {

		dwNumBytesToSend = 0;
		byOutputBuffer[dwNumBytesToSend++] = 0x3B; // write bits to TDI (out on -ve edge, in on +ve edge)
		//byOutputBuffer[dwNumBytesToSend++] = 0x3E; // write bits to TDI (out on +ve edge, in on -ve edge)
		byOutputBuffer[dwNumBytesToSend++] = 0x07; // eight bits
		byOutputBuffer[dwNumBytesToSend++] = 0xFF; // 11111111
		ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
		dwNumBytesToSend = 0;

		printf("dwNumBytesSent: %d\n", dwNumBytesSent);

	}

	printf("IR write done.\n");
*/

	/*
	//
	// Transition from Shift-IR to Exit1-IR
	// Use a TMS command to send TMS=1 and a data bit 1 at the same time
	//

	dwNumBytesToSend = 0;
	byOutputBuffer[dwNumBytesToSend++] = 0x4B;
	byOutputBuffer[dwNumBytesToSend++] = 0x00; // length = 1 bit
	byOutputBuffer[dwNumBytesToSend++] = 0x81; // 10000001
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
	dwNumBytesToSend = 0;
	*/

	
	/*
	//
	// Transition from Exit1-IR to Shift-DR by sending 1, 1, 0, 0 
	// Use a TMS command to send TMS=1100. The value of the data bit does not matter.
	// 
	// We need to enter Shift-DR because in the next state data is shifted into
	// the currently selected DR register of all devices of the daisy chain
	//

	dwNumBytesToSend = 0;
	byOutputBuffer[dwNumBytesToSend++] = 0x4B;
	byOutputBuffer[dwNumBytesToSend++] = 0x03; // length = 4 bit
	byOutputBuffer[dwNumBytesToSend++] = 0x03; // 00000011
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
	dwNumBytesToSend = 0;
	*/

	/*
	//
	// Make absolutely sure that all BYPASS registers of all devices in the daisy chain
	// are filled with zeroes
	//

	for (int i = 0; i < 100; i++) {

		dwNumBytesToSend = 0;
		byOutputBuffer[dwNumBytesToSend++] = 0x3B; // write bits to TDI (out on -ve edge, in on +ve edge)
		//byOutputBuffer[dwNumBytesToSend++] = 0x3E; // write bits to TDI (out on +ve edge, in on -ve edge)
		byOutputBuffer[dwNumBytesToSend++] = 0x07; // eight bits
		byOutputBuffer[dwNumBytesToSend++] = 0x00; // 00000000
		ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
		dwNumBytesToSend = 0;

	}

	printf("Fill with zero done\n");
	*/

	//
	// Start counting by shifting 1s into all BYPASS registers of the daisy chain, until
	// the first one exits the chain on TDO and count the amount of steps that it takes
	// for the first 1 to appear. That amount of steps is the amount of devices in the 
	// daisy chain.
	//

	bool done = false;
	while (!done) {
	//while (true) {

		printf("reading BYPASS ...\n");

		dwNumBytesToSend = 0;
		byOutputBuffer[dwNumBytesToSend++] = 0x3B; // write bits to TDI (out on -ve edge, in on +ve edge)
		//byOutputBuffer[dwNumBytesToSend++] = 0x3E; // write bits to TDI (out on +ve edge, in on -ve edge)
		//byOutputBuffer[dwNumBytesToSend++] = 0x6B; // TMS command
		
		byOutputBuffer[dwNumBytesToSend++] = 0x00; // a single bit
		//byOutputBuffer[dwNumBytesToSend++] = 0x01;
		
		byOutputBuffer[dwNumBytesToSend++] = 0x01; // 00000001
		ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
		dwNumBytesToSend = 0;

		Sleep(500);

		memset(byInputBuffer, 0, input_buffer_length);

		dwNumBytesToRead = 0;
		do
		{
			// Get the number of bytes in the device input buffer or Timeout
			ftStatus = FT_GetQueueStatus(ftHandle, &dwNumBytesToRead);
		} while ((dwNumBytesToRead == 0) && (ftStatus == FT_OK));
		ftStatus = FT_Read(ftHandle, &byInputBuffer, dwNumBytesToRead, &dwNumBytesRead);

		if (byInputBuffer[0] != 0) {
			done = true;
		}

		Sleep(100);
	}

	// 128, 0x80, 10000000
	// 192, 0xC0, 11000000
	// 224, 0xE0, 11100000
	// 240, 0xF0, 11110000
	// 248, 0xF8, 11111000
	// 252, 0xFC, 11111100
	// 254, 0xFE, 11111110
	// 255, 0xFE, 11111111

	
	printf("0: %d \n", byInputBuffer[0]);
	printf("200: %d \n", byInputBuffer[200]);

	printf("done ...\n");
	


	/*
	//
	// Navigate TMS through:
	// Navigate from Test-Logic-Reset to Shift-IR
	// 
	
	// Assumption: state machine is in Test-Logic-Reset
	// Test-Logic-Reset -> Run-Test-Idle -> Select-DR-Scan -> Select-IR-Scan -> Capture-IR -> Shift-IR
	// TMS=1               TMS=0            TMS=1             TMS=1             TMS=0         TMS=0

	// The command 0x4B is defined in this document:
	// https://ftdichip.com/Documents/AppNotes/AN_108_Command_Processor_for_MPSSE_and_MCU_Host_Bus_Emulation_Modes.pdf
	// 
	// Don't read data in 
	// Test-Logic-Reset, Run-Test-Idle, Select-DR-Scan, Select-IR-Scan, Capture-IR, Shift-IR
	//
	// Byte 0 - JTAG TMS Command Opcode - 0x4B (page 16) - No Data In, output clock edge is -VE
	byOutputBuffer[dwNumBytesToSend++] = 0x4B;
	
	// Byte 1 - Length
	// Number of clock pulses = Length + 1 (6 clocks here)
	// this value incremented by 1 yields the actual amount of
	// clock pulses that are executed. On each clock pulse, one
	// TMS bit is transmitted.
	byOutputBuffer[dwNumBytesToSend++] = 0x05; // length = 6
	
	// - send data bits 6 down to 0 to TMS
	// - data bit 7 is passed on to TDI/DO before the first clk 
	// of TMS and is held static for the duration of TMS clocking
	// 
	// Byte 2 - Data is set to 0x0D which is 00001101
	// Data is shifted LSB first towards MSB.
	// 
	// Data is shifted LSB first, so the TMS pattern is 101100
	byOutputBuffer[dwNumBytesToSend++] = 0x0D; // 00001101
	
	// Send off the TMS command
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
	
	// Reset output buffer pointer
	dwNumBytesToSend = 0;



	
	
	
	// Source: https://ftdichip.com/Documents/AppNotes/AN_129_FTDI_Hi_Speed_USB_To_JTAG_Example.pdf
	// 
	// TMS is currently low.
	// 
	// State machine is in Shift-IR state, so now use the TDI/TDO command to shift 1's out TDI/DO 
	// while reading TDO/DI. Although 8 bits need to be shifted in, only 7 are clocked here. 
	// The 8th will be in conjunction with a TMS command, coming next
	
	// Clock data out through states Capture-IR, Shift-IR and Exit-IR, read back result
	byOutputBuffer[dwNumBytesToSend++] = 0x3B;
	
	// Number of clock pulses = Length + 1 (7 clocks here)
	byOutputBuffer[dwNumBytesToSend++] = 0x06;
	
	// Shift out 1111111 (ignore last bit)
	byOutputBuffer[dwNumBytesToSend++] = 0xFF;
	
	// Send off the TMS command
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
	
	// Reset output buffer pointer
	dwNumBytesToSend = 0;
	
	
	
	
	
	
	// Here is the TMS command for one clock. Data is also shifted in.

	// Clock out TMS, Read one bit.
	byOutputBuffer[dwNumBytesToSend++] = 0x6B;
	
	// Number of clock pulses = Length + 0 (1 clock here)
	byOutputBuffer[dwNumBytesToSend++] = 0x00;
	
	// Data is shifted LSB first, so TMS becomes 1. Also, bit 7 is shifted into TDI/DO, also a 1
	// The 1 in bit 1 will leave TMS high for the next commands.	
	byOutputBuffer[dwNumBytesToSend++] = 0x83;
	
	// Send off the TMS command
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
	
	// Reset output buffer pointer
	dwNumBytesToSend = 0;



	
	
	
	
	// Navigate TMS from Exit-IR through 
	// Update-IR -> Select-DR-Scan -> Capture-DR
	// TMS=1        TMS=1             TMS=0

	// Don't read data in Update-IR -> Select-DR-Scan -> Capture-DR
	byOutputBuffer[dwNumBytesToSend++] = 0x4B;
	
	// Number of clock pulses = Length + 1 (4 clocks here)
	byOutputBuffer[dwNumBytesToSend++] = 0x03;
	
	// Data is shifted LSB first, so the TMS pattern is 110
	byOutputBuffer[dwNumBytesToSend++] = 0x83;
	
	// Send off the TMS command
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
		
	// Reset output buffer pointer
	dwNumBytesToSend = 0;



	// TMS is currently low.
	// 
	// State machine is in Shift-DR, so now use the TDI/TDO command to shift 101 out TDI/DO while reading TDO/DI
	// Although 3 bits need shifted in, only 2 are clocked here. The 3rd will be in conjunciton with a TMS command, coming next
	
	// Clock data out throuth states Shift-DR and Exit-DR.
	byOutputBuffer[dwNumBytesToSend++] = 0x3B;
	
	// Number of clock pulses = Length + 1 (2 clocks here)
	byOutputBuffer[dwNumBytesToSend++] = 0x01;
	
	// Shift out 101 (ignore last bit)
	byOutputBuffer[dwNumBytesToSend++] = 0x01;
	
	// Send off the TMS command
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
	
	// Reset output buffer pointer
	dwNumBytesToSend = 0;




	// Here is the TMS command for one clock. Data is also shifted in.
	byOutputBuffer[dwNumBytesToSend++] = 0x6B;
	// Clock out TMS, Read one bit.
	byOutputBuffer[dwNumBytesToSend++] = 0x00;
	// Number of clock pulses = Length + 0 (1 clock here)
	byOutputBuffer[dwNumBytesToSend++] = 0x83;
	// Data is shifted LSB first, so TMS becomes 1. Also, bit 7 is shifted into TDI/DO, also a 1
	// The 1 in bit 1 will leave TMS high for the next commands.
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
	
	
	
	// Send off the TMS command
	dwNumBytesToSend = 0; // Reset output buffer pointer
	// Navigate TMS through 
	// Update-DR -> Select-DR-Scan -> Select-IR-Scan -> Test Logic Reset
	// TMS=1        TMS=1             TMS=1             TMS=1
	// Don't read data in Update-DR -> Select-DR-Scan -> Select-IR-Scan -> Test Logic Reset
	byOutputBuffer[dwNumBytesToSend++] = 0x4B;
	// Number of clock pulses = Length + 1 (4 clocks here)
	byOutputBuffer[dwNumBytesToSend++] = 0x03;
	// Data is shifted LSB first, so the TMS pattern is 1111
	byOutputBuffer[dwNumBytesToSend++] = 0xFF;
	// Send off the TMS command
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
	// Reset output buffer pointer
	dwNumBytesToSend = 0;



	do
	{
		// Get the number of bytes in the device input buffer or Timeout
		ftStatus = FT_GetQueueStatus(ftHandle, &dwNumBytesToRead);
		
	} while ((dwNumBytesToRead == 0) && (ftStatus == FT_OK));
	ftStatus = FT_Read(ftHandle, &byInputBuffer, dwNumBytesToRead, &dwNumBytesRead);
	
	// Read out the data from input buffer
	printf("\n");
	printf("TI SN74BCT8244A IR default value is 0x81\n");
	printf("The value scanned by the FT2232H is 0x%x\n", byInputBuffer[dwNumBytesRead - 3]);
	printf("\n");
	printf("TI SN74BCT8244A DR bypass expected data is 00000010 = 0x2\n");
	printf(" The value scanned by the FT2232H = 0x%x\n", (byInputBuffer[dwNumBytesRead - 1] >> 5));
	
	// Generate a clock while in Test-Logic-Reset
	// 
	// This will not do anything with the TAP in the Test-Logic-Reset state,
	// but will demonstrate generation of clocks without any data transfer
	byOutputBuffer[dwNumBytesToSend++] = 0x8F;
	// Generate clock pulses
	byOutputBuffer[dwNumBytesToSend++] = 0x02;
	// (0x0002 + 1) * 8 = 24 clocks
	byOutputBuffer[dwNumBytesToSend++] = 0x00;
	// Send off the clock commands
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
	// Reset output buffer pointer
	dwNumBytesToSend = 0;

	*/
	
	// -----------------------------------------------------------
	// Start closing everything down
	// -----------------------------------------------------------
	
	printf("\nJTAG program executed successfully.\n");
	
	//printf("Press <Enter> to continue\n");
	//getchar(); // wait for a carriage return

	FT_Close(ftHandle); // Close the port

	return 0; // Exit with success
}