//  diskid32.cpp

//  for displaying the details of hard drives in 

//  06/11/2000  Lynn McGuire  written with many contributions from others,
//                            IDE drives only under Windows NT/2K and 9X,
//                            maybe SCSI drives later

#define PRINTING_TO_CONSOLE_ALLOWED

#include <windows.h>
#include <atlbase.h>
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <winioctl.h>
#include <Nb30.h>
#include "CRCUtility.h"

#include "IDGen.h"

#pragma comment( lib, "Netapi32.lib" )
////////////////////////////////////
//
//	硬盘区
//
////////////////////////////////////


//  Required to ensure correct PhysicalDrive IOCTL structure setup
#pragma pack(1)


//  Max number of drives assuming primary/secondary, master/slave topology
#define  MAX_IDE_DRIVES  4
#define  IDENTIFY_BUFFER_SIZE  512


//  IOCTL commands
#define  DFP_GET_VERSION          0x00074080
#define  DFP_SEND_DRIVE_COMMAND   0x0007c084
#define  DFP_RECEIVE_DRIVE_DATA   0x0007c088

#define  FILE_DEVICE_SCSI              0x0000001b
#define  IOCTL_SCSI_MINIPORT_IDENTIFY  ((FILE_DEVICE_SCSI << 16) + 0x0501)
#define  IOCTL_SCSI_MINIPORT 0x0004D008  //  see NTDDSCSI.H for definition



//  GETVERSIONOUTPARAMS contains the data returned from the 
//  Get Driver Version function.
typedef struct _GETVERSIONOUTPARAMS
{
	BYTE bVersion;      // Binary driver version.
	BYTE bRevision;     // Binary driver revision.
	BYTE bReserved;     // Not used.
	BYTE bIDEDeviceMap; // Bit map of IDE devices.
	DWORD fCapabilities; // Bit mask of driver capabilities.
	DWORD dwReserved[4]; // For future use.
} GETVERSIONOUTPARAMS, *PGETVERSIONOUTPARAMS, *LPGETVERSIONOUTPARAMS;


//  Bits returned in the fCapabilities member of GETVERSIONOUTPARAMS 
#define  CAP_IDE_ID_FUNCTION             1  // ATA ID command supported
#define  CAP_IDE_ATAPI_ID                2  // ATAPI ID command supported
#define  CAP_IDE_EXECUTE_SMART_FUNCTION  4  // SMART commannds supported


//  IDE registers
/*typedef struct _IDEREGS
{
	BYTE bFeaturesReg;       // Used for specifying SMART "commands".
	BYTE bSectorCountReg;    // IDE sector count register
	BYTE bSectorNumberReg;   // IDE sector number register
	BYTE bCylLowReg;         // IDE low order cylinder value
	BYTE bCylHighReg;        // IDE high order cylinder value
	BYTE bDriveHeadReg;      // IDE drive/head register
	BYTE bCommandReg;        // Actual IDE command.
	BYTE bReserved;          // reserved for future use.  Must be zero.
} IDEREGS, *PIDEREGS, *LPIDEREGS;*/


//  SENDCMDINPARAMS contains the input parameters for the 
//  Send Command to Drive function.
/*typedef struct _SENDCMDINPARAMS
{
	DWORD     cBufferSize;   //  Buffer size in bytes
	IDEREGS   irDriveRegs;   //  Structure with drive register values.
	BYTE bDriveNumber;       //  Physical drive number to send 
	//  command to (0,1,2,3).
	BYTE bReserved[3];       //  Reserved for future expansion.
	DWORD     dwReserved[4]; //  For future use.
	BYTE      bBuffer[1];    //  Input buffer.
} SENDCMDINPARAMS, *PSENDCMDINPARAMS, *LPSENDCMDINPARAMS;*/


//  Valid values for the bCommandReg member of IDEREGS.
#define  IDE_ATAPI_IDENTIFY  0xA1  //  Returns ID sector for ATAPI.
#define  IDE_ATA_IDENTIFY    0xEC  //  Returns ID sector for ATA.


// Status returned from driver
/*typedef struct _DRIVERSTATUS
{
	BYTE  bDriverError;  //  Error code from driver, or 0 if no error.
	BYTE  bIDEStatus;    //  Contents of IDE Error register.
	//  Only valid when bDriverError is SMART_IDE_ERROR.
	BYTE  bReserved[2];  //  Reserved for future expansion.
	DWORD  dwReserved[2];  //  Reserved for future expansion.
} DRIVERSTATUS, *PDRIVERSTATUS, *LPDRIVERSTATUS;*/


// Structure returned by PhysicalDrive IOCTL for several commands
/*typedef struct _SENDCMDOUTPARAMS
{
	DWORD         cBufferSize;   //  Size of bBuffer in bytes
	DRIVERSTATUS  DriverStatus;  //  Driver status structure.
	BYTE          bBuffer[1];    //  Buffer of arbitrary length in which to store the data read from the                                                       // drive.
} SENDCMDOUTPARAMS, *PSENDCMDOUTPARAMS, *LPSENDCMDOUTPARAMS;*/


// The following struct defines the interesting part of the IDENTIFY
// buffer:
typedef struct _IDSECTOR
{
	USHORT  wGenConfig;
	USHORT  wNumCyls;
	USHORT  wReserved;
	USHORT  wNumHeads;
	USHORT  wBytesPerTrack;
	USHORT  wBytesPerSector;
	USHORT  wSectorsPerTrack;
	USHORT  wVendorUnique[3];
	CHAR    sSerialNumber[20];
	USHORT  wBufferType;
	USHORT  wBufferSize;
	USHORT  wECCSize;
	CHAR    sFirmwareRev[8];
	CHAR    sModelNumber[40];
	USHORT  wMoreVendorUnique;
	USHORT  wDoubleWordIO;
	USHORT  wCapabilities;
	USHORT  wReserved1;
	USHORT  wPIOTiming;
	USHORT  wDMATiming;
	USHORT  wBS;
	USHORT  wNumCurrentCyls;
	USHORT  wNumCurrentHeads;
	USHORT  wNumCurrentSectorsPerTrack;
	ULONG   ulCurrentSectorCapacity;
	USHORT  wMultSectorStuff;
	ULONG   ulTotalAddressableSectors;
	USHORT  wSingleWordDMA;
	USHORT  wMultiWordDMA;
	BYTE    bReserved[128];
} IDSECTOR, *PIDSECTOR;


typedef struct _SRB_IO_CONTROL
{
	ULONG HeaderLength;
	UCHAR Signature[8];
	ULONG Timeout;
	ULONG ControlCode;
	ULONG ReturnCode;
	ULONG Length;
} SRB_IO_CONTROL, *PSRB_IO_CONTROL;

// Define global buffers.
BYTE IdOutCmd [sizeof (SENDCMDOUTPARAMS) + IDENTIFY_BUFFER_SIZE - 1];

char *ConvertToString (DWORD diskdata [256], int firstIndex, int lastIndex);
//void PrintIdeInfo (int drive, DWORD diskdata [256]);
BOOL DoIDENTIFY (HANDLE, PSENDCMDINPARAMS, PSENDCMDOUTPARAMS, BYTE, BYTE,
                 PDWORD);

char g_strTempBuf [1024];


int ReadPhysicalDriveInNT (void)
{
	USES_CONVERSION;
	int done = FALSE;
	int drive = 0;
	
//	for (drive = 0; drive < MAX_IDE_DRIVES; drive++)
	{
		HANDLE hPhysicalDriveIOCTL = 0;
		
		//  Try to get a handle to PhysicalDrive IOCTL, report failure
		//  and exit if can't.
		char driveName [256];
		
		sprintf_s (driveName, sizeof(driveName), "\\\\.\\PhysicalDrive%d", drive);
		
		//  Windows NT, Windows 2000, must have admin rights
		hPhysicalDriveIOCTL = CreateFile ( A2T( driveName ),
			GENERIC_READ | GENERIC_WRITE, 
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, 0, NULL);
		// if (hPhysicalDriveIOCTL == INVALID_HANDLE_VALUE)
		//    printf ("Unable to open physical drive %d, error code: 0x%lX\n",
		//            drive, GetLastError ());
		
		if (hPhysicalDriveIOCTL != INVALID_HANDLE_VALUE)
		{
			GETVERSIONOUTPARAMS VersionParams;
			DWORD               cbBytesReturned = 0;
			
            // Get the version, etc of PhysicalDrive IOCTL
			memset ((void*) &VersionParams, 0, sizeof(VersionParams));
			
			if ( ! DeviceIoControl (hPhysicalDriveIOCTL, DFP_GET_VERSION,
				NULL, 
				0,
				&VersionParams,
				sizeof(VersionParams),
				&cbBytesReturned, NULL) )
			{         
				// printf ("DFP_GET_VERSION failed for drive %d\n", i);
				// continue;
			}
			
            // If there is a IDE device at number "i" issue commands
            // to the device
			if (VersionParams.bIDEDeviceMap > 0)
			{
				BYTE             bIDCmd = 0;   // IDE or ATAPI IDENTIFY cmd
				SENDCMDINPARAMS  scip;
				//SENDCMDOUTPARAMS OutCmd;
				
				// Now, get the ID sector for all IDE devices in the system.
				// If the device is ATAPI use the IDE_ATAPI_IDENTIFY command,
				// otherwise use the IDE_ATA_IDENTIFY command
				bIDCmd = (VersionParams.bIDEDeviceMap >> drive & 0x10) ? \
							IDE_ATAPI_IDENTIFY : IDE_ATA_IDENTIFY;
				
				memset (&scip, 0, sizeof(scip));
				memset (IdOutCmd, 0, sizeof(IdOutCmd));
				
				if ( DoIDENTIFY (hPhysicalDriveIOCTL, 
					&scip, 
					(PSENDCMDOUTPARAMS)&IdOutCmd, 
					(BYTE) bIDCmd,
					(BYTE) drive,
					&cbBytesReturned))
				{
					DWORD diskdata [256];
					int ijk = 0;
					USHORT *pIdSector = (USHORT *)
						((PSENDCMDOUTPARAMS) IdOutCmd) -> bBuffer;
					
					for (ijk = 0; ijk < 256; ijk++)
						diskdata [ijk] = pIdSector [ijk];
					
					//               PrintIdeInfo (drive, diskdata);
					strcat_s (g_strTempBuf, sizeof(g_strTempBuf), ConvertToString (diskdata, 10, 19));
					
					done = TRUE;
				}
			}
			
			CloseHandle (hPhysicalDriveIOCTL);
		}
	}
	
	return done;
}


// DoIDENTIFY
// FUNCTION: Send an IDENTIFY command to the drive
// bDriveNum = 0-3
// bIDCmd = IDE_ATA_IDENTIFY or IDE_ATAPI_IDENTIFY
BOOL DoIDENTIFY (HANDLE hPhysicalDriveIOCTL, PSENDCMDINPARAMS pSCIP,
                 PSENDCMDOUTPARAMS pSCOP, BYTE bIDCmd, BYTE bDriveNum,
                 PDWORD lpcbBytesReturned)
{
	// Set up data structures for IDENTIFY command.
	pSCIP -> cBufferSize = IDENTIFY_BUFFER_SIZE;
	pSCIP -> irDriveRegs.bFeaturesReg = 0;
	pSCIP -> irDriveRegs.bSectorCountReg = 1;
	pSCIP -> irDriveRegs.bSectorNumberReg = 1;
	pSCIP -> irDriveRegs.bCylLowReg = 0;
	pSCIP -> irDriveRegs.bCylHighReg = 0;
	
	// Compute the drive number.
	pSCIP -> irDriveRegs.bDriveHeadReg = 0xA0 | ((bDriveNum & 1) << 4);
	
	// The command can either be IDE identify or ATAPI identify.
	pSCIP -> irDriveRegs.bCommandReg = bIDCmd;
	pSCIP -> bDriveNumber = bDriveNum;
	pSCIP -> cBufferSize = IDENTIFY_BUFFER_SIZE;
	
	return ( DeviceIoControl (hPhysicalDriveIOCTL, DFP_RECEIVE_DRIVE_DATA,
		(LPVOID) pSCIP,
		sizeof(SENDCMDINPARAMS) - 1,
		(LPVOID) pSCOP,
		sizeof(SENDCMDOUTPARAMS) + IDENTIFY_BUFFER_SIZE - 1,
		lpcbBytesReturned, NULL) );
}



// ------------------------------------------------- //
//                   WinIo v1.2                      //
//  Direct Hardware Access Under Windows 9x/NT/2000  //
//        Copyright 1998-2000 Yariv Kaplan           //
//           http://www.internals.com                //
// ------------------------------------------------- //

//#include <windows.h>
//#include "instdrv.h"

BOOL LoadDeviceDriver( const TCHAR * Name, const TCHAR * Path, HANDLE * lphDevice );
BOOL UnloadDeviceDriver( const TCHAR * Name );

HANDLE hDriver;
bool IsNT;
bool IsWinIoInitialized = false;


bool IsWinNT()
{
	OSVERSIONINFO OSVersionInfo;
	
	OSVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	
	GetVersionEx(&OSVersionInfo);
	
	return OSVersionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT;
}


// ------------------------------------------------ //
//                  Port32 v3.0                     //
//    Direct Port Access Under Windows 9x/NT/2000   //
//        Copyright 1998-2000 Yariv Kaplan          //
//            http://www.internals.com              //
// ------------------------------------------------ //

// These are our ring 0 functions responsible for tinkering with the hardware ports.
// They have a similar privilege to a Windows VxD and are therefore free to access
// protected system resources (such as the page tables) and even place calls to
// exported VxD services.
#define FILE_DEVICE_WINIO 0x00008010

// Macro definition for defining IOCTL and FSCTL function control codes.
// Note that function codes 0-2047 are reserved for Microsoft Corporation,
// and 2048-4095 are reserved for customers.

#define WINIO_IOCTL_INDEX 0x810

#define IOCTL_WINIO_WRITEPORT     CTL_CODE(FILE_DEVICE_WINIO,  \
	WINIO_IOCTL_INDEX + 2,   \
	METHOD_BUFFERED,         \
FILE_ANY_ACCESS)

#define IOCTL_WINIO_READPORT      CTL_CODE(FILE_DEVICE_WINIO,  \
	WINIO_IOCTL_INDEX + 3,   \
	METHOD_BUFFERED,         \
FILE_ANY_ACCESS)

#pragma pack(1)

struct GDT_DESCRIPTOR
{
	WORD Limit_0_15;
	WORD Base_0_15;
	BYTE Base_16_23;
	BYTE Type         : 4;
	BYTE System       : 1;
	BYTE DPL          : 2;
	BYTE Present      : 1;
	BYTE Limit_16_19  : 4;
	BYTE Available    : 1;
	BYTE Reserved     : 1;
	BYTE D_B          : 1;
	BYTE Granularity  : 1;
	BYTE Base_24_31;
};

struct CALLGATE_DESCRIPTOR
{
	WORD Offset_0_15;
	WORD Selector;
	WORD ParamCount   : 5;
	WORD Unused       : 3;
	WORD Type         : 4;
	WORD System       : 1;
	WORD DPL          : 2;
	WORD Present      : 1;
	WORD Offset_16_31;
};

struct GDTR
{
	WORD wGDTLimit;
	DWORD dwGDTBase;
};

#pragma pack()

struct tagPhys32Struct
{
	HANDLE PhysicalMemoryHandle;
	ULONG dwPhysMemSizeInBytes;
	PVOID pvPhysAddress;
	PVOID pvPhysMemLin;
};

extern struct tagPhys32Struct Phys32Struct;

struct tagPort32Struct
{
	USHORT wPortAddr;
	ULONG dwPortVal;
	UCHAR bSize;
};


//extern struct tagPort32Struct Port32Struct;
//-----------------------------------------------------
__declspec(naked) void Ring0GetPortVal()
{
	_asm
	{
		Cmp CL, 1
			Je ByteVal
			Cmp CL, 2
			Je WordVal
			Cmp CL, 4
			Je DWordVal
			
ByteVal:
		
		In AL, DX
			Mov [EBX], AL
			Retf
			
WordVal:
		
		In AX, DX
			Mov [EBX], AX
			Retf
			
DWordVal:
		
		In EAX, DX
			Mov [EBX], EAX
			Retf
	}
}


__declspec(naked) void Ring0SetPortVal()
{
	_asm
	{
		Cmp CL, 1
			Je ByteVal
			Cmp CL, 2
			Je WordVal
			Cmp CL, 4
			Je DWordVal
			
ByteVal:
		
		Mov AL, [EBX]
			Out DX, AL
			Retf
			
WordVal:
		
		Mov AX, [EBX]
			Out DX, AX
			Retf
			
DWordVal:
		
		Mov EAX, [EBX]
			Out DX, EAX
			Retf
	}
}


// This function makes it possible to call ring 0 code from a ring 3
// application.

bool CallRing0(PVOID pvRing0FuncAddr, WORD wPortAddr, PDWORD pdwPortVal, BYTE bSize)
{
	
	struct GDT_DESCRIPTOR *pGDTDescriptor;
	struct GDTR gdtr;
	WORD CallgateAddr[3];
	WORD wGDTIndex = 1;
	
	_asm Sgdt [gdtr]
		
		// Skip the null descriptor
		
		pGDTDescriptor = (struct GDT_DESCRIPTOR *)(gdtr.dwGDTBase + 8);
	
	// Search for a free GDT descriptor
	
	for (wGDTIndex = 1; wGDTIndex < (gdtr.wGDTLimit / 8); wGDTIndex++)
	{
		if (pGDTDescriptor->Type == 0     &&
			pGDTDescriptor->System == 0   &&
			pGDTDescriptor->DPL == 0      &&
			pGDTDescriptor->Present == 0)
		{
			// Found one !
			// Now we need to transform this descriptor into a callgate.
			// Note that we're using selector 0x28 since it corresponds
			// to a ring 0 segment which spans the entire linear address
			// space of the processor (0-4GB).
			
			struct CALLGATE_DESCRIPTOR *pCallgate;
			
			pCallgate =	(struct CALLGATE_DESCRIPTOR *) pGDTDescriptor;
			pCallgate->Offset_0_15 = LOWORD(pvRing0FuncAddr);
			pCallgate->Selector = 0x28;
			pCallgate->ParamCount =	0;
			pCallgate->Unused = 0;
			pCallgate->Type = 0xc;
			pCallgate->System = 0;
			pCallgate->DPL = 3;
			pCallgate->Present = 1;
			pCallgate->Offset_16_31 = HIWORD(pvRing0FuncAddr);
			
			// Prepare the far call parameters
			
			CallgateAddr[0] = 0x0;
			CallgateAddr[1] = 0x0;
			CallgateAddr[2] = (wGDTIndex << 3) | 3;
			
			// Please fasten your seat belts!
			// We're about to make a hyperspace jump into RING 0.
			
			_asm Mov DX, [wPortAddr]
			_asm Mov EBX, [pdwPortVal]
			_asm Mov CL, [bSize]
			_asm Call FWORD PTR [CallgateAddr]
				
				// We have made it !
				// Now free the GDT descriptor
				
				memset(pGDTDescriptor, 0, 8);
			
			// Our journey was successful. Seeya.
			
			return true;
		}
		
		// Advance to the next GDT descriptor
		
		pGDTDescriptor++; 
	}
	
	// Whoops, the GDT is full
	
	return false;
}


bool GetPortVal(WORD wPortAddr, PDWORD pdwPortVal, BYTE bSize)
{
	bool Result;
	DWORD dwBytesReturned;
	struct tagPort32Struct Port32Struct;
	
	if (IsNT)
	{
		if (!IsWinIoInitialized)
			return false;
		
		Port32Struct.wPortAddr = wPortAddr;
		Port32Struct.bSize = bSize;
		
		if (!DeviceIoControl(hDriver, IOCTL_WINIO_READPORT, &Port32Struct,
			sizeof(struct tagPort32Struct), &Port32Struct, 
			sizeof(struct tagPort32Struct),
			&dwBytesReturned, NULL))
			return false;
		else
			*pdwPortVal = Port32Struct.dwPortVal;
	}
	else
	{
		Result = CallRing0((PVOID)Ring0GetPortVal, wPortAddr, pdwPortVal, bSize);
		
		if (Result == false)
			return false;
	}
	
	return true;
}


bool SetPortVal(WORD wPortAddr, DWORD dwPortVal, BYTE bSize)
{
	DWORD dwBytesReturned;
	struct tagPort32Struct Port32Struct;
	
	if (IsNT)
	{
		if (!IsWinIoInitialized)
			return false;
		
		Port32Struct.wPortAddr = wPortAddr;
		Port32Struct.dwPortVal = dwPortVal;
		Port32Struct.bSize = bSize;
		
		if (!DeviceIoControl(hDriver, IOCTL_WINIO_WRITEPORT, &Port32Struct,
			sizeof(struct tagPort32Struct), NULL, 0, &dwBytesReturned, NULL))
			return false;
	}
	else
		return CallRing0((PVOID)Ring0SetPortVal, wPortAddr, &dwPortVal, bSize);
	
	return true;
}


int ReadDrivePortsInWin9X (void)
{
	int done = FALSE;
	int drive = 0;
//	InitializeWinIo ();
	
	//  Get IDE Drive info from the hardware ports
	//  loop thru all possible drives
	for (drive = 0; drive < 8; drive++)
	{
		DWORD diskdata [256];
		WORD  baseAddress = 0;   //  Base address of drive controller 
		DWORD portValue = 0;
		int waitLoop = 0;
		int index = 0;
		
		switch (drive / 2)
		{
		case 0: 
			baseAddress = 0x1f0; 
			break;
		case 1: 
			baseAddress = 0x170; 
			break;
		case 2: 
			baseAddress = 0x1e8; 
			break;
		case 3: 
			baseAddress = 0x168; 
			break;
		}
		
		//  Wait for controller not busy 
		waitLoop = 100000;
		while (--waitLoop > 0)
		{
			GetPortVal ((WORD) (baseAddress + 7), &portValue, (BYTE) 1);
            //  drive is ready
			if ((portValue & 0x40) == 0x40) 
				break;
            //  previous drive command ended in error
			if ((portValue & 0x01) == 0x01) 
				break;
		}

		if (waitLoop < 1) 
			continue;

		//  Set Master or Slave drive
		if ((drive % 2) == 0)
			SetPortVal ((WORD) (baseAddress + 6), 0xA0, 1);
		else
			SetPortVal ((WORD) (baseAddress + 6), 0xB0, 1);
		
		//  Get drive info data
		SetPortVal ((WORD) (baseAddress + 7), 0xEC, 1);
		
		// Wait for data ready 
		waitLoop = 100000;
		while (--waitLoop > 0)
		{
			GetPortVal ((WORD) (baseAddress + 7), &portValue, 1);
            //  see if the drive is ready and has it's info ready for us
			if ((portValue & 0x48) == 0x48) 
				break;
            //  see if there is a drive error
			if ((portValue & 0x01) == 0x01) 
				break;
		}
		
		//  check for time out or other error                                                    
		if (waitLoop < 1 || portValue & 0x01) 
			continue;
		
		//  read drive id information
		for (index = 0; index < 256; index++)
		{
			diskdata [index] = 0;   //  init the space
			GetPortVal (baseAddress, &(diskdata [index]), 2);
		}

		//      PrintIdeInfo (drive, diskdata);
		strcat_s (g_strTempBuf, sizeof(g_strTempBuf), ConvertToString (diskdata, 10, 19));
		
		done = TRUE;
		break;
	}
	
//	ShutdownWinIo ();
	
	return done;
}


#define  SENDIDLENGTH  sizeof (SENDCMDOUTPARAMS) + IDENTIFY_BUFFER_SIZE


int ReadIdeDriveAsScsiDriveInNT (void)
{
	USES_CONVERSION;
	int done = FALSE;
	int controller = 0;
	
//	for (controller = 0; controller < 2; controller++)
	{
		HANDLE hScsiDriveIOCTL = 0;
		char   driveName [256];
		
		//  Try to get a handle to PhysicalDrive IOCTL, report failure
		//  and exit if can't.
		sprintf_s (driveName, sizeof(driveName), "\\\\.\\Scsi%d:", controller);
		
		//  Windows NT, Windows 2000, any rights should do
		hScsiDriveIOCTL = CreateFile ( A2T( driveName ),
			GENERIC_READ | GENERIC_WRITE, 
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, 0, NULL);
		// if (hScsiDriveIOCTL == INVALID_HANDLE_VALUE)
		//    printf ("Unable to open SCSI controller %d, error code: 0x%lX\n",
		//            controller, GetLastError ());
		
		if (hScsiDriveIOCTL != INVALID_HANDLE_VALUE)
		{
			int drive = 0;
			
			for (drive = 0; drive < 2; drive++)
			{
				char buffer [sizeof (SRB_IO_CONTROL) + SENDIDLENGTH];
				SRB_IO_CONTROL *p = (SRB_IO_CONTROL *) buffer;
				SENDCMDINPARAMS *pin =
					(SENDCMDINPARAMS *) (buffer + sizeof (SRB_IO_CONTROL));
				DWORD dummy;
				
				memset (buffer, 0, sizeof (buffer));
				p -> HeaderLength = sizeof (SRB_IO_CONTROL);
				p -> Timeout = 10000;
				p -> Length = SENDIDLENGTH;
				p -> ControlCode = IOCTL_SCSI_MINIPORT_IDENTIFY;
				memcpy(p->Signature, "SCSIDISK", sizeof(p->Signature));
				
				pin -> irDriveRegs.bCommandReg = IDE_ATA_IDENTIFY;
				pin -> bDriveNumber = drive;
				
				if (DeviceIoControl (hScsiDriveIOCTL, IOCTL_SCSI_MINIPORT, 
					buffer,
					sizeof (SRB_IO_CONTROL) +
					sizeof (SENDCMDINPARAMS) - 1,
					buffer,
					sizeof (SRB_IO_CONTROL) + SENDIDLENGTH,
					&dummy, NULL))
				{
					SENDCMDOUTPARAMS *pOut =
						(SENDCMDOUTPARAMS *) (buffer + sizeof (SRB_IO_CONTROL));
					IDSECTOR *pId = (IDSECTOR *) (pOut -> bBuffer);
					if (pId -> sModelNumber [0])
					{
						DWORD diskdata [256];
						int ijk = 0;
						USHORT *pIdSector = (USHORT *) pId;
						
						for (ijk = 0; ijk < 256; ijk++)
							diskdata [ijk] = pIdSector [ijk];
						
						//                  PrintIdeInfo (controller * 2 + drive, diskdata);
						strcat_s (g_strTempBuf, sizeof(g_strTempBuf), ConvertToString (diskdata, 10, 19));
						
						done = TRUE;
					}
				}
			}
			CloseHandle (hScsiDriveIOCTL);
		}
	}
	
	return done;
}


char *ConvertToString (DWORD diskdata [256], int firstIndex, int lastIndex)
{
	static char string [1024];
	int index = 0;
	int position = 0;
	
	//  each integer has two characters stored in it backwards
	for (index = firstIndex; index <= lastIndex; index++)
	{
		//  get high byte for 1st character
		string [position] = (char) (diskdata [index] / 256);
					position++;
								
		//  get low byte for 2nd character
		string [position] = (char) (diskdata [index] % 256);
					position++;
	}
	
	//  end the string 
	string [position] = '\0';
	
	//  cut off the trailing blanks
	for (index = position - 1; index > 0 && ' ' == string [index]; index--)
								string [index] = '\0';
	
	return string;
}

const char*  GetHardDriveComputerID (void)
{
	int done = FALSE;
	memset( g_strTempBuf, 0, sizeof(g_strTempBuf) );
	if( IsWinNT() )
	{
		//  this works under WinNT4 or Win2K if you have admin rights
		done = ReadPhysicalDriveInNT ();
		
		//  this should work in WinNT or Win2K if previous did not work
		//  this is kind of a backdoor via the SCSI mini port driver into
		//     the IDE drives
		if ( ! done)
			done = ReadIdeDriveAsScsiDriveInNT ();
	}
	else
	{
		//  this works under Win9X and calls WINIO.DLL
		if ( !done ) 
			done = ReadDrivePortsInWin9X ();
	}
	return (const char*) g_strTempBuf;
}

const char*  GetHardDriveComputerID_Win7(void)
{
	int done = FALSE;
	memset( g_strTempBuf, 0, sizeof(g_strTempBuf) );
	if( IsWinNT() )
	{
		// win7 下, 没有uac权限的时候, 用这个方法取
		//  this works under WinNT4 or Win2K if you have admin rights
//		done = ReadPhysicalDriveInNT ();
		
		//  this should work in WinNT or Win2K if previous did not work
		//  this is kind of a backdoor via the SCSI mini port driver into
		//     the IDE drives
//		if ( ! done)
			done = ReadIdeDriveAsScsiDriveInNT ();
	}
	else
	{
		//  this works under Win9X and calls WINIO.DLL
		if ( !done ) 
			done = ReadDrivePortsInWin9X ();
	}
	return (const char*) g_strTempBuf;
}



////////////////////////////////////
//
//	网卡区
//
////////////////////////////////////

typedef struct _ASTAT_
{
	ADAPTER_STATUS adapt;
	NAME_BUFFER    NameBuff [30];
}ASTAT, *PASTAT;

ASTAT Adapter;

const char* GetTheFirstMac(void)
{
	NCB ncb;
	UCHAR uRetCode;
	LANA_ENUM lana_enum_buf;
	
	memset( g_strTempBuf, 0, sizeof(g_strTempBuf) );

	memset( &ncb, 0, sizeof(ncb) );
	ncb.ncb_command = NCBENUM;
	
	ncb.ncb_buffer = (unsigned char *) &lana_enum_buf;
	ncb.ncb_length = sizeof(lana_enum_buf);
	
	// 向网卡发送NCBENUM命令，以获取当前机器的网卡信息，如有多少个网卡、每张网卡的编号等 
	uRetCode = Netbios( &ncb );
	//	printf( "The NCBENUM return		code is: 0x%x \n", uRetCode );
	if ( uRetCode != 0 )
	{
		return g_strTempBuf;
	}

	int nIndex = 0;
//	for(int nIndex = 0; nIndex < lana_enum_buf.length; nIndex++)
	{
		int lana_num = lana_enum_buf.lana[nIndex];
		
		memset( &ncb, 0, sizeof(ncb) );
		ncb.ncb_command = NCBRESET;
		ncb.ncb_lana_num = lana_num;   
		// 指定网卡号
		
		// 首先对选定的网卡发送一个NCBRESET命令，以便进行初始化 
		uRetCode = Netbios( &ncb );
		//    printf( "The NCBRESET return code is:   0x%x \n", uRetCode );
		
		memset( &ncb, 0, sizeof(ncb) );
		ncb.ncb_command = NCBASTAT;
		ncb.ncb_lana_num = lana_num;     // 指定网卡号
		
		memcpy(ncb.ncb_callname, "*               ", sizeof(ncb.ncb_callname));
		ncb.ncb_buffer = (unsigned char *) &Adapter;
		
		// 指定返回的信息存放的变量 
		ncb.ncb_length = sizeof(Adapter);
		
		// 接着，可以发送NCBASTAT命令以获取网卡的信息 
		uRetCode = Netbios( &ncb );
		//    printf( "The NCBASTAT   return code is: 0x%x \n", uRetCode );
		if ( uRetCode == 0 )
		{
			// 把网卡MAC地址格式化成常用的16进制形式，如0010A4E45802 
			char szTemp[33] = {0};
			_snprintf_s (szTemp, sizeof(szTemp), 32, "%02X%02X%02X%02X%02X%02X", 
						Adapter.adapt.adapter_address[0],
						Adapter.adapt.adapter_address[1],
						Adapter.adapt.adapter_address[2],
						Adapter.adapt.adapter_address[3],
						Adapter.adapt.adapter_address[4],
						Adapter.adapt.adapter_address[5] );

			strcat_s(g_strTempBuf, sizeof(g_strTempBuf), szTemp);
		}
	}

	return g_strTempBuf;
	
}

////////////////////////////////////
//
//	主板区
//
////////////////////////////////////

typedef LONG    NTSTATUS;
 
typedef struct _UNICODE_STRING
{
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING;
 
typedef enum _SECTION_INHERIT
{
    ViewShare = 1,
    ViewUnmap = 2
} SECTION_INHERIT, *PSECTION_INHERIT;
 
typedef struct _OBJECT_ATTRIBUTES
{
    ULONG Length;
    HANDLE RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG Attributes;
    PVOID SecurityDescriptor;
    PVOID SecurityQualityOfService;
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;
 
#define InitializeObjectAttributes( p, n, a, r, s ) { \
    (p)->Length = sizeof( OBJECT_ATTRIBUTES ); \
    (p)->RootDirectory = r; \
    (p)->Attributes = a; \
    (p)->ObjectName = n; \
    (p)->SecurityDescriptor = s; \
    (p)->SecurityQualityOfService = NULL; \
}

 
// Interesting functions in NTDLL
typedef NTSTATUS (WINAPI *ZwOpenSectionProc)
(
    PHANDLE SectionHandle,
    DWORD DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes
);
typedef NTSTATUS (WINAPI *ZwMapViewOfSectionProc)
(
    HANDLE SectionHandle,
    HANDLE ProcessHandle,
    PVOID *BaseAddress,
    ULONG ZeroBits,
    ULONG CommitSize,
    PLARGE_INTEGER SectionOffset,
    PULONG ViewSize,
    SECTION_INHERIT InheritDisposition,
    ULONG AllocationType,
    ULONG Protect
);
typedef NTSTATUS (WINAPI *ZwUnmapViewOfSectionProc)
(
    HANDLE ProcessHandle,
    PVOID BaseAddress
);
typedef VOID (WINAPI *RtlInitUnicodeStringProc)
(
    IN OUT PUNICODE_STRING DestinationString,
    IN PCWSTR SourceString
);
 
// Global variables
static HMODULE hModule = NULL;
static HANDLE hPhysicalMemory = NULL;
static ZwOpenSectionProc ZwOpenSection;
static ZwMapViewOfSectionProc ZwMapViewOfSection;
static ZwUnmapViewOfSectionProc ZwUnmapViewOfSection;
static RtlInitUnicodeStringProc RtlInitUnicodeString;
 
// initialize
BOOL InitPhysicalMemory()
{
    if ( !( hModule = LoadLibrary( _T( "ntdll.dll" ) ) ) )
    {
        return FALSE;
    }
 
    // 以下从NTDLL获取我们需要的几个函数指针
    if (!(ZwOpenSection = (ZwOpenSectionProc)GetProcAddress(hModule, "ZwOpenSection")))
    {
        return FALSE;
    }
 
    if (!(ZwMapViewOfSection = (ZwMapViewOfSectionProc)GetProcAddress(hModule, "ZwMapViewOfSection")))
    {
        return FALSE;
    }
 
    if (!(ZwUnmapViewOfSection = (ZwUnmapViewOfSectionProc)GetProcAddress(hModule, "ZwUnmapViewOfSection")))
    {
        return FALSE;
    }
  
    if (!(RtlInitUnicodeString = (RtlInitUnicodeStringProc)GetProcAddress(hModule, "RtlInitUnicodeString")))
    {
        return FALSE;
    }
 
    // 以下打开内核对象
    WCHAR PhysicalMemoryName[] = L"\\Device\\PhysicalMemory";
    UNICODE_STRING PhysicalMemoryString;
    OBJECT_ATTRIBUTES attributes;
    RtlInitUnicodeString(&PhysicalMemoryString, PhysicalMemoryName);
    InitializeObjectAttributes(&attributes, &PhysicalMemoryString, 0, NULL, NULL);
    NTSTATUS status = ZwOpenSection(&hPhysicalMemory, SECTION_MAP_READ, &attributes );
 
    return (status >= 0);
}
 
// terminate -- free handles
void ExitPhysicalMemory()
{
    if (hPhysicalMemory != NULL)
    {
        CloseHandle(hPhysicalMemory);
    }
 
    if (hModule != NULL)
    {
        FreeLibrary(hModule);
    }
}
 
BOOL ReadPhysicalMemory(PVOID buffer, DWORD address, DWORD length)
{
    DWORD outlen;            // 输出长度，根据内存分页大小可能大于要求的长度
    PVOID vaddress;          // 映射的虚地址
    NTSTATUS status;         // NTDLL函数返回的状态
    LARGE_INTEGER base;      // 物理内存地址
 
    vaddress = 0;
    outlen = length;
    base.QuadPart = (ULONGLONG)(address);
 
    // 映射物理内存地址到当前进程的虚地址空间
    status = ZwMapViewOfSection(hPhysicalMemory,
        (HANDLE) -1,
        (PVOID *)&vaddress,
        0,
        length,
        &base,
        &outlen,
        ViewShare,
        0,
        PAGE_READONLY);
 
    if (status < 0)
    {
        return FALSE;
    }
 
    // 当前进程的虚地址空间中，复制数据到输出缓冲区
    memmove(buffer, vaddress, length);
 
    // 完成访问，取消地址映射
    status = ZwUnmapViewOfSection((HANDLE)-1, (PVOID)vaddress);
 
    return (status >= 0);
}
///////////////////////////////
//CRC

DWORD g_pdwCrc32Table[256];
void InitCRCTable(void)
{
	DWORD dwPolynomial = 0xEDB88320;
	int i, j;

	DWORD dwCrc;
	for(i = 0; i < 256; i++)
	{
		dwCrc = i;
		for(j = 8; j > 0; j--)
		{
			if(dwCrc & 1)
				dwCrc = (dwCrc >> 1) ^ dwPolynomial;
			else
				dwCrc >>= 1;
		}
		g_pdwCrc32Table[i] = dwCrc;
	}
}

inline void CalcCrc32(const BYTE byte, DWORD &dwCrc32)
{
	dwCrc32 = ((dwCrc32) >> 8) ^ g_pdwCrc32Table[(byte) ^ ((dwCrc32) & 0x000000FF)];
}
 
// 一个测试函数，从物理地址0xfe000开始，读取4096个字节
// 对于Award BIOS，可以从这段数据找到序列号等信息
const char* GetBiosCRC(void)
{
    UCHAR buf[4096];
	ZeroMemory(buf, 4096);
	memset( g_strTempBuf, 0, sizeof(g_strTempBuf) );

	if(IsWinNT())
	{
		if (!InitPhysicalMemory())
		{
			return g_strTempBuf;
		}
 
		if (!ReadPhysicalMemory(buf, 0xfe000, 4096))
		{
			ExitPhysicalMemory();
			return g_strTempBuf;
		}
	}
	else
	{
		memcpy( buf, (void*)(0xfe000), 4096 );
	}
    // ... 成功读取了指定数据
 
	DWORD dwTemp;
	InitCRCTable();
	const BYTE* pBuf = buf;

	for( int i=0; i<2; i++ )
	{
		char strTemp[32] = {0};
		dwTemp = BufCrc32( pBuf, 2048 );

		sprintf_s( strTemp, sizeof(strTemp), "%04X", dwTemp );
		
		strcat_s( g_strTempBuf, sizeof(g_strTempBuf), strTemp );

		pBuf += 1024;
	}

    ExitPhysicalMemory();
    return g_strTempBuf;
}

