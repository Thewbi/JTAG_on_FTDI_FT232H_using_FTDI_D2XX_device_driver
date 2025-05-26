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

int _tmain(int argc, _TCHAR* argv[])
{
	FT_HANDLE ftHandle; // Handle of the FTDI device
	FT_STATUS ftStatus; // Result of each D2XX call
	DWORD dwNumDevs; // The number of devices
	unsigned int uiDevIndex = 0xF; // The device in the list that is used
	BYTE byOutputBuffer[1024]; // Buffer to hold MPSSE commands and data to be sent to the FT2232H
	BYTE byInputBuffer[1024]; // Buffer to hold data read from the FT2232H
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
	Sleep(5000); 


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
	
	// Pin name                  | Signal Direction Config | Initial State Config
	// ---------------------------------------------------------------------------
	// ADBUS0 TCK/SK clock       | output 1                | low  0
	// ADBUS1 TDI/D0             | output 1                | low  0
	// ADBUS2 TDO/DI             | input  0                | low  0
	// ADBUS3 TMS/CS chip select | output 1                | high 1
	// ADBUS4 GPIOL0             | input  0                | low  0
	// ADBUS5 GPIOL1             | input  0                | low  0
	// ADBUS6 GPIOL2             | input  0                | low  0
	// ADBUS7 GPIOL3             | input  0                | low  0
	// ---------------------------------------------------------------------------
	// 0x80                      | 0x0B (lsb first)        | 0x08 (lsb first)

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

	// Navigate TMS through:
	// Test-Logic-Reset -> Run-Test-Idle -> Select-DR-Scan -> Select-IR-Scan
	// TMS=1               TMS=0            TMS=1             TMS=1

	// The command 0x4B is defined in this document:
	// https://ftdichip.com/Documents/AppNotes/AN_108_Command_Processor_for_MPSSE_and_MCU_Host_Bus_Emulation_Modes.pdf
	// 
	// Don't read data in Test-Logic-Reset, Run-Test-Idle, Select-DR-Scan, Select-IR-Scan
	byOutputBuffer[dwNumBytesToSend++] = 0x4B; // 0100 1011
	
	// Number of clock pulses = Length + 1 (6 clocks here)
	byOutputBuffer[dwNumBytesToSend++] = 0x05;
	
	// Data is shifted LSB first, so the TMS pattern is 101100
	byOutputBuffer[dwNumBytesToSend++] = 0x0D;
	
	// Send off the TMS command
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
	
	// Reset output buffer pointer
	dwNumBytesToSend = 0;



	
	
	
	// Source: https://ftdichip.com/Documents/AppNotes/AN_129_FTDI_Hi_Speed_USB_To_JTAG_Example.pdf
	// TMS is currently low.
	// State machine is in Shift-IR, so now use the TDI/TDO command to shift 1's out TDI/DO 
	// while reading TDO/DI. Although 8 bits need shifted in, only 7 are clocked here. 
	// The 8th will be in conjunction with a TMS command, coming next
	
	// Clock data out through states Capture-IR, Shift-IR and Exit-IR, read back result
	byOutputBuffer[dwNumBytesToSend++] = 0x3B; // 0x3B == 0011 1011
	
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
	// Navigate TMS through Update-DR -> Select-DR-Scan -> Select-IR-Scan -> Test Logic Reset
	// TMS=1 TMS=1 TMS=1 TMS=1
	byOutputBuffer[dwNumBytesToSend++] = 0x4B;
	// Don't read data in Update-DR -> Select-DR-Scan -> Select-IR-Scan -> Test Logic Reset
	byOutputBuffer[dwNumBytesToSend++] = 0x03;
	// Number of clock pulses = Length + 1 (4 clocks here)
	byOutputBuffer[dwNumBytesToSend++] = 0xFF;
	// Data is shifted LSB first, so the TMS pattern is 101100
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
	
	
	
	// Send off the TMS command
	// Reset output buffer pointer
	dwNumBytesToSend = 0; 
	do
	{
		ftStatus = FT_GetQueueStatus(ftHandle, &dwNumBytesToRead);
		// Get the number of bytes in the device input buffer
	} while ((dwNumBytesToRead == 0) && (ftStatus == FT_OK));
	
	// or Timeout
	ftStatus = FT_Read(ftHandle, &byInputBuffer, dwNumBytesToRead, &dwNumBytesRead);
	
	// Read out the data from input buffer
	printf("\n");
	printf("TI SN74BCT8244A IR default value is 0x81\n");
	printf("The value scanned by the FT2232H is 0x%x\n", byInputBuffer[dwNumBytesRead - 3]);
	printf("\n");
	printf("TI SN74BCT8244A DR bypass expected data is 00000010 = 0x2\n");
	printf(" The value scanned by the FT2232H = 0x%x\n", (byInputBuffer[dwNumBytesRead - 1] >> 5));
	
	// Generate a clock while in Test-Logic-Reset
	// This will not do anything with the TAP in the Test-Logic-Reset state,
	// but will demonstrate generation of clocks without any data transfer
	byOutputBuffer[dwNumBytesToSend++] = 0x8F;
	// Generate clock pulses
	byOutputBuffer[dwNumBytesToSend++] = 0x02;
	// (0x0002 + 1) * 8 = 24 clocks
	byOutputBuffer[dwNumBytesToSend++] = 0x00;
	//
	ftStatus = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
	// Send off the clock commands
	dwNumBytesToSend = 0; // Reset output buffer pointer

	
	// -----------------------------------------------------------
	// Start closing everything down
	// -----------------------------------------------------------
	
	printf("\nJTAG program executed successfully.\n");
	printf("Press <Enter> to continue\n");

	getchar(); // wait for a carriage return

	FT_Close(ftHandle); // Close the port

	return 0; // Exit with success
}