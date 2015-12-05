/* Tyler Niles
 * April 2010
 * MR Project
 *
 * Asynchronous Comms via FTDI FT232RL
 *
 */

#include "main.h"
#include <windows.h>		// REQUIRED by ftd2xx.h
#include <iostream>
#include <stdio.h>
#include <ftd2xx.h>

using namespace std;

typedef unsigned long long KEYS;  // key list typedef
#define NUMKEYS	61		  // # of keys

#define RXBUFFERLENGTH	  256	  // MUST match var for buffer tracking in update_ftdi
#define BAUDSETTING    750000
#define SYNCHBYTE	 0xDB	  /* FTDI stuff */
FT_HANDLE ftHandle;	
FT_STATUS ftStatus;
DWORD dwModemStatus;
DWORD RxBytes = 18; 		// MUST be >=2x total # of incoming bytes (allow for synching)
DWORD BytesReceived;
unsigned char RxBuffer[RXBUFFERLENGTH];	// data to be read
DWORD InTransferSize = 512;	// 512 byte USB transfer2pc size (bytes, must be multiple of 64bytes, default=4096)

unsigned int numpacket;		// packet #
unsigned char k56, k48, k40, k32, k24, k16, k8, k0;	// temps for incoming bytes
KEYS keylist;			// received keylist
KEYS shifter;			// shift masker


int setup_ftdi()
{
	// USB OPEN PORT
	ftStatus = FT_Open(0, &ftHandle);
	if(ftStatus != FT_OK){ cout << "Error opening FTDI" << endl;  return 1; }

	// USB PARAMS
	FT_SetTimeouts(ftHandle,5000,1000); 	// handle, read timeout (ms), write timeout (ms)
	ftStatus = FT_SetUSBParameters(ftHandle, InTransferSize, 0);
	if(ftStatus != FT_OK){ cout << "SetUSBParams Failed!" << endl;  return 6; }

	// PURGE
	ftStatus = FT_Purge(ftHandle, FT_PURGE_RX | FT_PURGE_RX);	// Purge both Rx and Tx buffers
	if(ftStatus != FT_OK){ cout << "Device Purge Failed!" << endl;  return 5; }
	if(!FT_W32_PurgeComm(ftHandle, PURGE_TXABORT|PURGE_RXABORT))
	{  cout << "Windows Purge1 Failed!" << endl;  return 5;  }
	if(!FT_W32_PurgeComm(ftHandle, PURGE_TXCLEAR|PURGE_RXCLEAR))
	{  cout << "Windows Purge2 Failed!" << endl;  return 5;  }

	// FLOW CONTROL
	ftStatus = FT_SetFlowControl(ftHandle, FT_FLOW_RTS_CTS, 0x11, 0x13);
	if(ftStatus != FT_OK){ cout << "Flow Control could not be set" << endl;  return 2; }

	// SET BAUD
	ftStatus = FT_SetBaudRate(ftHandle, BAUDSETTING);
	if(ftStatus != FT_OK){ cout << "Baud Rate could not be set" << endl;  return 3; }

	// SET DATA
	ftStatus = FT_SetDataCharacteristics(ftHandle, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_NONE);
	if(ftStatus != FT_OK){ cout << "Data Params could not be set" << endl;  return 4; }

	cout << "FTDI Port Opened Successfully" << endl;
	return 0;
}


void update_ftdi()
{
  unsigned char i=0;	// DO NOT MAKE STATIC!!!!
  			// u-char: for rollover in buffer

  /* Call FT_Read() to get data (read from USB packet sent) */
  ftStatus = FT_Read(ftHandle,RxBuffer,RxBytes,&BytesReceived);

  #if 0
  unsigned int j;
  for(j=0; j<BytesReceived; j++)
  {
    cout<<hex<<(int)RxBuffer[j]<<" ";
  }
  cout<<dec<<endl;
  #endif

  if(ftStatus == FT_OK)
  { 
    if(BytesReceived == RxBytes)
    {
	while(RxBuffer[i++] != SYNCHBYTE);
	k0  = RxBuffer[i++];		// MSB
	k8  = RxBuffer[i++];
	k16 = RxBuffer[i++];
	k24 = RxBuffer[i++];
	k32 = RxBuffer[i++];
	k40 = RxBuffer[i++];
	k48 = RxBuffer[i++];
	k56 = RxBuffer[i++];		// LSB

	/* concatenate into keylist global */
	keylist  = 0x0000000000000000;		// init
	keylist =  k56;  keylist <<= 8; 	// assign, then shift
	keylist |= k48;  keylist <<= 8; 	//
	keylist |= k40;  keylist <<= 8; 	//
	keylist |= k32;  keylist <<= 8; 	//

	keylist |= k24;  keylist <<= 8; 	//
	keylist |= k16;  keylist <<= 8; 	//
	keylist |= k8;   keylist <<= 8; 	//
	keylist |= k0;  		 	//
			 
    } 
    else
    {
	cout << "Read wrong # of bytes" << endl;
    }

  }
  else
  {
    cout << "Could not Read Packets (FT_OK expected)" << endl;
    cout << "Verify device is connected" << endl;
    cout << " " << endl;
  }

}	/* End Update FTDI */


void play_keys()
{
	unsigned int i;
	KEYS databuf = 0;	// buffer
	databuf = keylist;

	/* PRINT OUT KEYLIST STATUS */
	cout<<hex<<databuf<<endl;
	for(i=0; i<NUMKEYS; i++)
	{
		if(databuf & shifter)
		{
			cout << "Key: " << i << endl;
		}
		databuf >>= 1;  //barrel through
	}
}


void clean_up()
{
	FT_Close(ftHandle);
}


/************************** * MAIN * **************************/
int main(int argc, char ** argv)
{
	int i=0;
	keylist   = 0x0000000000000000;
	shifter   = 0x0000000000000001;
	k56 = 0x00;  k48 = 0x00;  k40 = 0x00;  k32 = 0x00;
	k24 = 0x00;  k16 = 0x00;  k8  = 0x00;  k0  = 0x00;

	// Init buffers
	for(i=0;i<RXBUFFERLENGTH;i++)
	{
		RxBuffer[i] = 0;
	}
	
	// Open FTDI comms
	i=0;
	while(setup_ftdi()){
	  cout << "Retrying to Open Port" << endl;
	  if(i++>10){ Sleep(3000);  exit(0); }
	}		// make sure port can open

	// Main loop
	cout << "Begin Playing!" << endl;
	while(1){
	  update_ftdi();	// read-in keylist
	  play_keys();		// print keylist
	}

	clean_up();			// close port, etc.
	cout << "Program completed successfully" << endl;
	Sleep(3000);
	exit(0);
}

