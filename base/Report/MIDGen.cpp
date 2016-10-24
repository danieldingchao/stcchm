// MIDGen.cpp: implementation of the CMIDGen class.
//
//////////////////////////////////////////////////////////////////////
#include <windows.h>
#include <atlbase.h>
#include "MIDGen.h"
#include "360md5.h"
#include "def.h"
#include "IDGen.h"
#include <Nb30.h>
#include <WbemCli.h>
#pragma comment(lib, "Wbemuuid.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment( lib, "Netapi32.lib" )

#include <vector>
#include <cstring>
#include <algorithm>
using namespace std;

#include <strsafe.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

// Define global buffers.
BYTE IdOutCmd1 [sizeof (SENDCMDOUTPARAMS) + IDENTIFY_BUFFER_SIZE - 1];

static void MD5Encode( LPSTR lpszIn, LPSTR lpszOut, int nLen )
{
	#define MD5_CIPHER	"%s"

	CHAR szBuffer[MAX_PATH] = "";
	//	MD5_CTX	m_md5;
	MD5_CTX context;
	unsigned char szEncrypt[16]	= {0};
	
	MD5Init(&context);
	StringCbPrintfA( szBuffer, sizeof(szBuffer), MD5_CIPHER, lpszIn );
	MD5Update( &context , (unsigned char *)szBuffer, strlen(szBuffer) );
	MD5Final( &context );
	MD5Final( &context);
	memcpy( szEncrypt, context.digest, 16);
	
	CHAR szHexTmp[8] = {0};
	CHAR szOutput[64] = {0};
	
	memset( szOutput, 0, sizeof(szOutput) );
	
	for ( int i=0; i<16; i++ ) 
	{
		memset( szHexTmp, 0, sizeof(szHexTmp) );
		StringCbPrintfA( szHexTmp, sizeof(szHexTmp), "%02x", szEncrypt[i] );
		int nOutLen = sizeof(szOutput)- strlen(szOutput);
		strncat_s ( szOutput, sizeof(szOutput), szHexTmp, min( nOutLen, (int)strlen(szHexTmp) ) );
	}
	
	if( lpszOut )
	{
		memset( lpszOut, 0, nLen );
		StringCbPrintfA( lpszOut, nLen-1, "%s", szOutput );
		_strlwr_s( lpszOut, nLen);
	}
}

// 去掉最字符串最右边的空格:写的很简单，其他人不要用
static void _StrTrimRightA(LPSTR lpText)
{
	if(lpText == NULL)
		return;
	
	int nLen = strlen(lpText);
	while(nLen--)
	{
		if(lpText[nLen] == ' ')
			lpText[nLen] = '\0';
		else
			break;
	}
}

// 去掉最字符串最右边的空格:写的很简单，其他人不要用
static void _StrTrimRightW(LPWSTR lpText)
{
	if(lpText == NULL)
		return;
	
	int nLen = wcslen(lpText);
	while(nLen--)
	{
		if(lpText[nLen] == ' ')
			lpText[nLen] = '\0';
		else
			break;
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BOOL ReadFirstPhysicalDriveInNTWithAdminRights(LPSTR lpSerialNumber, DWORD cbSize)
{
	if(lpSerialNumber == NULL || cbSize == 0)
		return FALSE;

	BOOL done = FALSE;
	int drive = 0;

	for (drive = 0; drive < MAX_IDE_DRIVES; drive++)
	{
		HANDLE hPhysicalDriveIOCTL = INVALID_HANDLE_VALUE;
		
		//  Try to get a handle to PhysicalDrive IOCTL, report failure
		//  and exit if can't.
		TCHAR driveName [256];

		StringCbPrintf(driveName, sizeof(driveName), _T("\\\\.\\PhysicalDrive%d"), drive);
		
		//  Windows NT, Windows 2000, must have admin rights
		hPhysicalDriveIOCTL = CreateFile (driveName,
			GENERIC_READ | GENERIC_WRITE, 
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING, 0, NULL);
		
		if (hPhysicalDriveIOCTL != INVALID_HANDLE_VALUE)
		{
			GETVERSIONOUTPARAMS VersionParams;
			DWORD               cbBytesReturned = 0;
			
            // Get the version, etc of PhysicalDrive IOCTL
			ZeroMemory(&VersionParams, sizeof(VersionParams));
			if ( ! DeviceIoControl (hPhysicalDriveIOCTL, DFP_GET_VERSION,
				NULL, 
				0,
				&VersionParams,
				sizeof(VersionParams),
				&cbBytesReturned, NULL) )
			{         
				// printf ("DFP_GET_VERSION failed for drive %d\n", i);
				CloseHandle(hPhysicalDriveIOCTL);
				continue;
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
				bIDCmd = (VersionParams.bIDEDeviceMap >> drive & 0x10) ? ATAPI_ID_CMD : ID_CMD;
				
				ZeroMemory(&scip, sizeof(scip));
				ZeroMemory (IdOutCmd1, sizeof(IdOutCmd1));
				
				if ( DoIDENTIFY1 (hPhysicalDriveIOCTL, 
					&scip, 
					(PSENDCMDOUTPARAMS)&IdOutCmd1, 
					(BYTE) bIDCmd,
					(BYTE) drive,
					&cbBytesReturned))
				{
					DWORD diskdata [256];
					int ijk = 0;
					USHORT *pIdSector = (USHORT *)
						((PSENDCMDOUTPARAMS) IdOutCmd1) -> bBuffer;
					
					for (ijk = 0; ijk < 256; ijk++)
						diskdata [ijk] = pIdSector [ijk];
					
					// PrintIdeInfo (drive, diskdata);
					if(S_OK == StringCbCopyA(lpSerialNumber, cbSize, ConvertToString1 (diskdata, 10, 19)) )
						done = TRUE;
				}
			}

			CloseHandle (hPhysicalDriveIOCTL);

			if(done)
				break;
		}
	}
	
	return done;
}

#define  SENDIDLENGTH  sizeof (SENDCMDOUTPARAMS) + IDENTIFY_BUFFER_SIZE

BOOL ReadFirstPhysicalDriveInNTWithScsiBackDoor(LPSTR lpSerialNumber, DWORD cbSize)
{
	int done = FALSE;
	int controller = 0;
	
	for (controller = 0; controller < 16; controller++)
	{
		HANDLE hScsiDriveIOCTL = 0;
		TCHAR   driveName [256];
		
		//  Try to get a handle to PhysicalDrive IOCTL, report failure
		//  and exit if can't.
		StringCbPrintf(driveName, sizeof(driveName), _T("\\\\.\\Scsi%d:"), controller);
		
		//  Windows NT, Windows 2000, any rights should do
		hScsiDriveIOCTL = CreateFile (driveName,
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
				
				pin -> irDriveRegs.bCommandReg = ID_CMD; // IDE_ATA_IDENTIFY;
				pin -> bDriveNumber = drive;
				
				if (DeviceIoControl (hScsiDriveIOCTL, IOCTL_SCSI_MINIPORT, 
					buffer,
					sizeof (SRB_IO_CONTROL) +
					sizeof (SENDCMDINPARAMS) - 1,
					buffer,
					sizeof (SRB_IO_CONTROL) + SENDIDLENGTH,
					&dummy, NULL))
				{
					SENDCMDOUTPARAMS *pOut = (SENDCMDOUTPARAMS *) (buffer + sizeof (SRB_IO_CONTROL));
					IDSECTOR *pId = (IDSECTOR *) (pOut -> bBuffer);
					if (pId -> sModelNumber [0])
					{
						DWORD diskdata [256];
						int ijk = 0;
						USHORT *pIdSector = (USHORT *) pId;
						
						for (ijk = 0; ijk < 256; ijk++)
							diskdata [ijk] = pIdSector [ijk];
						
						HRESULT hr = StringCbCopyA(lpSerialNumber, cbSize, ConvertToString1(diskdata, 10, 19));
						// PrintIdeInfo (controller * 2 + drive, diskdata);

						if( SUCCEEDED(hr) )
							done = TRUE;
					}
				}

				if(done)
					break;
			}

			CloseHandle (hScsiDriveIOCTL);
		}
	}
	
	return done;
}

//////////////////////////////////////////////////////////////////////////
BOOL ReadFirstPhysicalDriveInNTWithZeroRights (LPSTR lpSerialNumber, DWORD cbSize)
{
	if(lpSerialNumber == NULL || cbSize == 0)
		return FALSE;

   int done = FALSE;
   int drive = 0;

   for (drive = 0; drive < MAX_IDE_DRIVES; drive++)
   {
      HANDLE hPhysicalDriveIOCTL = 0;

         //  Try to get a handle to PhysicalDrive IOCTL, report failure
         //  and exit if can't.
      TCHAR driveName [256];

      StringCbPrintf(driveName, sizeof(driveName), _T("\\\\.\\PhysicalDrive%d"), drive);

         //  Windows NT, Windows 2000, Windows XP - admin rights not required
      hPhysicalDriveIOCTL = CreateFile (driveName, 0,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                               OPEN_EXISTING, 0, NULL);

      if (hPhysicalDriveIOCTL != INVALID_HANDLE_VALUE)
      {
		 STORAGE_PROPERTY_QUERY query;
         DWORD cbBytesReturned = 0;
		 char buffer [10000];

         memset ((void *) & query, 0, sizeof (query));
		 query.PropertyId = StorageDeviceProperty;
		 query.QueryType = PropertyStandardQuery;

		 memset (buffer, 0, sizeof (buffer));

         if ( DeviceIoControl (hPhysicalDriveIOCTL, IOCTL_STORAGE_QUERY_PROPERTY,
                   & query,
                   sizeof (query),
				   & buffer,
				   sizeof (buffer),
                   & cbBytesReturned, NULL) )
         {         
			 STORAGE_DEVICE_DESCRIPTOR * descrip = (STORAGE_DEVICE_DESCRIPTOR *) & buffer;
			 char serialNumber [1000];
			 ZeroMemory(serialNumber, sizeof(serialNumber));

			 flipAndCodeBytes (buffer,
			                   descrip -> SerialNumberOffset,
			                   1, serialNumber );

			 if(S_OK == StringCbCopyA(lpSerialNumber, cbSize, serialNumber) )
				 done = TRUE;
         }

		CloseHandle (hPhysicalDriveIOCTL);

		if(done)
			break;
      }
   }

   return done;
}

//////////////////////////////////////////////////////////////////////////

// 内部函数，不要引用:
// 长度总是为40
static BOOL _ConvSerialNumber(LPCWSTR lpEncode, LPWSTR lpText)
{
	ATLASSERT( wcslen(lpEncode) == 40);

	BOOL bRet = TRUE;
	WCHAR szTemp[10] = L"0x";

	int n1, n2;

	for(int i=0; i<40; i+=4)
	{
		n1 = n2 = 0;

		szTemp[2] = lpEncode[i];
		szTemp[3] = lpEncode[i+1];
		szTemp[4] = 0;

		StrToIntExW(szTemp, STIF_SUPPORT_HEX, &n1);

		szTemp[2] = lpEncode[i+2];
		szTemp[3] = lpEncode[i+3];
		szTemp[4] = 0;
		
		StrToIntExW(szTemp, STIF_SUPPORT_HEX, &n2);

		if( n1 == 0 ||
			n2 == 0 )
		{
			bRet = FALSE;
			break;
		}

		lpText[0] = (TCHAR)n2;
		lpText[1] = (TCHAR)n1;

		lpText += 2;
	}

	lpText[0] = '\0';

	return bRet;
}

BOOL ReadFirstPhysicalDriveInNTWithSmart(LPSTR lpSerialNumber, DWORD cbSize)
{
	int done = FALSE;
	int drive = 0;
	
	for (drive = 0; drive < MAX_IDE_DRIVES; drive++)
	{
		HANDLE hPhysicalDriveIOCTL = 0;
		
		//  Try to get a handle to PhysicalDrive IOCTL, report failure
		//  and exit if can't.
		TCHAR driveName [256];
		
		StringCchPrintf(driveName, ARRSIZEOF(driveName), _T("\\\\.\\PhysicalDrive%d"), drive);
		
		//  Windows NT, Windows 2000, Windows Server 2003, Vista
		hPhysicalDriveIOCTL = CreateFile (driveName,
			GENERIC_READ | GENERIC_WRITE, 
			FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, 
			NULL, OPEN_EXISTING, 0, NULL);
		
		if (hPhysicalDriveIOCTL == INVALID_HANDLE_VALUE)
		{
#ifdef PRINTING_TO_CONSOLE_ALLOWED
			if (PRINT_DEBUG) 
				printf ("\n%d ReadPhysicalDriveInNTUsingSmart ERROR"
				"\nCreateFile(%s) returned INVALID_HANDLE_VALUE\n"
				"Error Code %d\n",
				__LINE__, driveName, GetLastError ());
#endif
		}
		else
		{
			GETVERSIONINPARAMS GetVersionParams;
			DWORD cbBytesReturned = 0;
			
            // Get the version, etc of PhysicalDrive IOCTL
			memset ((void*) & GetVersionParams, 0, sizeof(GetVersionParams));

			if ( ! DeviceIoControl (hPhysicalDriveIOCTL, SMART_GET_VERSION,
				NULL, 
				0,
				&GetVersionParams, sizeof (GETVERSIONINPARAMS),
				&cbBytesReturned, NULL) )
			{         
#ifdef PRINTING_TO_CONSOLE_ALLOWED
				if (PRINT_DEBUG)
				{
					DWORD err = GetLastError ();
					printf ("\n%d ReadPhysicalDriveInNTUsingSmart ERROR"
						"\nDeviceIoControl(%d, SMART_GET_VERSION) returned 0, error is %d\n",
						__LINE__, (int) hPhysicalDriveIOCTL, (int) err);
				}
#endif
			}
			else
			{
				// Print the SMART version
				// PrintVersion (& GetVersionParams);
				// Allocate the command buffer
				ULONG CommandSize = sizeof(SENDCMDINPARAMS) + IDENTIFY_BUFFER_SIZE;
				PSENDCMDINPARAMS Command = (PSENDCMDINPARAMS) malloc (CommandSize);
				// Retrieve the IDENTIFY data
				// Prepare the command
				// #define ID_CMD          0xEC            // Returns ID sector for ATA
				Command -> irDriveRegs.bCommandReg = ID_CMD;
				DWORD BytesReturned = 0;
				if ( ! DeviceIoControl (hPhysicalDriveIOCTL, 
					SMART_RCV_DRIVE_DATA, Command, sizeof(SENDCMDINPARAMS),
					Command, CommandSize,
					&BytesReturned, NULL) )
				{
					// Print the error
					//PrintError ("SMART_RCV_DRIVE_DATA IOCTL", GetLastError());
				} 
				else
				{
					// Print the IDENTIFY data
					DWORD diskdata [256];
					USHORT *pIdSector = (USHORT *)
						(PIDENTIFY_DATA) ((PSENDCMDOUTPARAMS) Command) -> bBuffer;
					
					for (int ijk = 0; ijk < 256; ijk++)
						diskdata [ijk] = pIdSector [ijk];
					
//					PrintIdeInfo (drive, diskdata);
					if(S_OK == StringCbCopyA(lpSerialNumber, cbSize, ConvertToString1 (diskdata, 10, 19)) )
						done = TRUE;
				}
				// Done
				CloseHandle (hPhysicalDriveIOCTL);
				free (Command);

				if(done)
					break;
			}
		}
	}
	
   return done;
}

// 读取磁盘序列号改为使用WMI来读取
// 原方式在VISTA下普通用户权限有问题
BOOL ReadFirstPhysicalDriveInNTWithWMI(LPSTR lpSerialNumber, DWORD cbSize)
{
	BOOL bRet = FALSE;
	HRESULT hr;
	
	if(lpSerialNumber == NULL || cbSize == 0)
		return FALSE;
	
	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	if(SUCCEEDED(hr))
	{
		hr = CoInitializeSecurity(NULL, -1, // COM authentication
			NULL,   					 // Authentication services
			NULL,   					 // Reserved
			RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
			RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
			NULL,   					 // Authentication info
			EOAC_NONE,  				 // Additional capabilities 
			NULL						 // Reserved
			);

		// RPC_E_TOO_LATE : CoInitializeSecurity has already been called.
		if(SUCCEEDED(hr) || hr == RPC_E_TOO_LATE)
		{
			CComQIPtr<IWbemLocator> spLocator;
			hr = spLocator.CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER);
			if(SUCCEEDED(hr) && spLocator)
			{
				CComQIPtr<IWbemServices> spServices;
				CComBSTR bsNameSpace(L"ROOT\\CIMV2");

				hr = spLocator->ConnectServer(bsNameSpace, NULL, NULL, NULL, 0, NULL, NULL, &spServices);
				if(SUCCEEDED(hr) && spServices)
				{
					hr = CoSetProxyBlanket(spServices,
						RPC_C_AUTHN_WINNT,
						RPC_C_AUTHZ_NONE,
						NULL,
						RPC_C_AUTHN_LEVEL_CALL,
						RPC_C_IMP_LEVEL_IMPERSONATE,
						NULL,					
						EOAC_NONE);

					if(SUCCEEDED(hr))
					{
						CComQIPtr<IEnumWbemClassObject> spEnumObject;
						CComBSTR bsWQL(L"SELECT SerialNumber FROM Win32_PhysicalMedia");

						hr = spServices->ExecQuery(CComBSTR(L"WQL"), bsWQL,
							WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
							NULL, &spEnumObject);

						if( SUCCEEDED(hr) && spEnumObject)
						{
							while(1)
							{
								CComQIPtr<IWbemClassObject> spClassObject;
								ULONG uReturn = 0;

								hr = spEnumObject->Next(WBEM_INFINITE, 1, &spClassObject, &uReturn);
								if(SUCCEEDED(hr) && uReturn != 0 && spClassObject)
								{
									CComVariant vSerialNumber;
									hr = spClassObject->Get(L"SerialNumber", 0, &vSerialNumber, NULL, NULL);
									if(SUCCEEDED(hr) && vSerialNumber.vt == VT_BSTR)
									{
										int nLen = wcslen(vSerialNumber.bstrVal);
										if( nLen > 0)
										{
											USES_CONVERSION;

											// bsSerialNumber = L"38303730313050443430303754444d4755394334";
											// 万恶的Vista! 在高权限下返回的是明文，在低权限下返回的是编码后的结果 !!!
											// 只能在这里粗暴的认为SerialNumber总是20个字符了
											WCHAR szTemp[100];

											if( nLen == 40 &&
												_ConvSerialNumber(vSerialNumber.bstrVal, szTemp) )
											{
											}
											else
											{
												StringCbCopyW(szTemp, sizeof(szTemp), vSerialNumber.bstrVal);
											}

											hr = StringCbCopyA(lpSerialNumber, cbSize, OLE2CA(szTemp));

											if(SUCCEEDED(hr))
												bRet = TRUE;

											break;
										}
									}
								}
								else
								{
									break;
								}
							}
						}
					}
				}
			}
		}
	}

	CoUninitialize();
	return bRet;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
static BOOL GetMACFromServiceName(LPCTSTR lpServiceName, LPSTR lpszMAC, DWORD cbSize)
{
	ATLASSERT(lpszMAC && cbSize);
	
	TCHAR szDeviceName[MAX_PATH];
	StringCbPrintf(szDeviceName, sizeof(szDeviceName), _T("\\\\.\\%s"), lpServiceName);
	HANDLE hDevice = CreateFile(szDeviceName,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ|FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);
	
	if(hDevice == INVALID_HANDLE_VALUE)
		return FALSE;
	
	DWORD dwInBuffer = 0x1010101;
	BYTE szOutBuffer[MAX_PATH]; 
	DWORD dwBytesReturn = 0;
	BOOL bRet = FALSE;
	
	if( DeviceIoControl(hDevice, 0x170002, &dwInBuffer, sizeof(dwInBuffer),
		szOutBuffer, sizeof(szOutBuffer), &dwBytesReturn, NULL) &&
		dwBytesReturn > 0)
	{
		//			StringCbPrintf(lpszMAC, cbSize, _T("%02X-%02X-%02X-%02X-%02X-%02X"), 
		StringCbPrintfA(lpszMAC, cbSize, "%02X%02X%02X%02X%02X%02X",
			szOutBuffer[0], szOutBuffer[1], szOutBuffer[2],
			szOutBuffer[3], szOutBuffer[4], szOutBuffer[5]);

		bRet = TRUE;
	}
	
	CloseHandle(hDevice);
	return bRet;
}

BOOL GetFirstAdapterMAC_NetworkCards(LPSTR lpszMAC, DWORD cbSize)
{
	if(lpszMAC == NULL || cbSize == 0)
		return FALSE;

	HKEY hKey;
	LPCTSTR lpCardsKey = _T("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\NetworkCards");
	CHAR szFirstMAC[100];

	ZeroMemory(szFirstMAC, sizeof(szFirstMAC));

	if( ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, lpCardsKey, 0, KEY_ENUMERATE_SUB_KEYS, &hKey) )
	{
		TCHAR szSubkeyName[MAX_PATH];
		int nIndex = 0;
		
		while(1)
		{
			DWORD cch = ARRSIZEOF(szSubkeyName);
			if(ERROR_SUCCESS == RegEnumKeyEx(hKey, nIndex, szSubkeyName, &cch, NULL, NULL, NULL, NULL))
			{
				HKEY hCardKey;
				if(ERROR_SUCCESS == RegOpenKeyEx(hKey, szSubkeyName, 0, KEY_QUERY_VALUE, &hCardKey))
				{
					TCHAR szServiceName[MAX_PATH];
					DWORD dwType = REG_SZ;
					DWORD cbSize = sizeof(szServiceName);
					
					if(ERROR_SUCCESS == RegQueryValueEx(hCardKey, _T("ServiceName"), NULL, &dwType, (LPBYTE)szServiceName, &cbSize) )
					{
						CHAR szMAC[100];
						ZeroMemory(szMAC, sizeof(szMAC));
						if( GetMACFromServiceName(szServiceName, szMAC, sizeof(szMAC)) )
						{
							if( szFirstMAC[0] == '\0' || StrCmpA(szMAC, szFirstMAC) < 0)
								StringCbCopyA(szFirstMAC, sizeof(szFirstMAC), szMAC);
						}
					}
					
					RegCloseKey(hCardKey);
				}
			}
			else
			{
				break;
			}
			
			nIndex ++;
		}
		
		RegCloseKey(hKey);
	}

	if(szFirstMAC[0])
	{
		HRESULT hr = StringCbCopyA(lpszMAC, cbSize, szFirstMAC);
		return (SUCCEEDED(hr));
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////

typedef struct _ASTAT1_
{
	ADAPTER_STATUS adapt;
	NAME_BUFFER    NameBuff [30];
}ASTAT1, *PASTAT1;

ASTAT1 Adapter;

BOOL GetFirstAdapterMAC_Netbios(LPSTR lpszMAC, DWORD cbSize)
{
	NCB ncb;
	UCHAR uRetCode;
	LANA_ENUM lana_enum_buf;
	
	memset( &ncb, 0, sizeof(ncb) );

	ncb.ncb_command = NCBENUM;	
	ncb.ncb_buffer = (unsigned char *) &lana_enum_buf;
	ncb.ncb_length = sizeof(lana_enum_buf);
	
	// 向网卡发送NCBENUM命令，以获取当前机器的网卡信息，如有多少个网卡、每张网卡的编号等 
	uRetCode = Netbios( &ncb );
	if ( uRetCode != 0 )
		return FALSE;
	
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
		
		memset( &ncb, 0, sizeof(ncb) );
		ncb.ncb_command = NCBASTAT;
		ncb.ncb_lana_num = lana_num;     // 指定网卡号
		
		StringCbCopyA( (char *)ncb.ncb_callname, NCBNAMSZ, "*               " );
		ncb.ncb_buffer = (unsigned char *) &Adapter;
		
		// 指定返回的信息存放的变量 
		ncb.ncb_length = sizeof(Adapter);
		
		// 接着，可以发送NCBASTAT命令以获取网卡的信息 
		uRetCode = Netbios( &ncb );
		if ( uRetCode == 0 )
		{
			// 把网卡MAC地址格式化成常用的16进制形式，如0010A4E45802 
			HRESULT hr = StringCbPrintfA(lpszMAC, cbSize, "%02X%02X%02X%02X%02X%02X", 
				Adapter.adapt.adapter_address[0],
				Adapter.adapt.adapter_address[1],
				Adapter.adapt.adapter_address[2],
				Adapter.adapt.adapter_address[3],
				Adapter.adapt.adapter_address[4],
				Adapter.adapt.adapter_address[5] );

			return SUCCEEDED(hr);
		}
	}
	
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
// CPU ID
BOOL GetCPUID(LPSTR lpBuffer, DWORD cbSize)
{
	BOOL bException = FALSE;
	BYTE szCpu[16]  = { 0 };
	UINT uCpuID     = 0U;
	
	__try 
	{
		_asm 
		{
			mov eax, 0
			cpuid
			mov dword ptr szCpu[0], ebx
			mov dword ptr szCpu[4], edx
			mov dword ptr szCpu[8], ecx
			mov eax, 1
			cpuid
			mov uCpuID, edx
		}
	}
	__except( EXCEPTION_EXECUTE_HANDLER )
	{
		bException = TRUE;
	}
	
	if(bException == FALSE)
	{
		StringCbPrintfA(lpBuffer, cbSize, "%s:%08x", szCpu, uCpuID);
	}
	
	return !bException;
}

static BOOL GetMidFromRegister(LPTSTR lpBuffer, DWORD cbSize)
{
	ATLASSERT(lpBuffer && cbSize);
	if(lpBuffer == NULL || cbSize < 32)
		return FALSE;

	DWORD dwType = REG_SZ;
	CHAR szTemp[1024];
	DWORD cbTempSize = sizeof(szTemp);

	ZeroMemory(szTemp, sizeof(szTemp));
	if( ERROR_SUCCESS == SHGetValueA(HKEY_CURRENT_USER, "Software\\TheWorld6\\Liveup", "mid", &dwType, szTemp, &cbTempSize) )
	{
		// 两个MD5
		if(strlen(szTemp) == 64)
		{
			CHAR szMid[128];
			CHAR szVerifyKey[128];

			StringCbCopyNA(szMid, sizeof(szMid), szTemp, 32);
			StringCbCopyA(szVerifyKey, sizeof(szVerifyKey), szTemp+32);

			ZeroMemory(szTemp, sizeof(szTemp));
			MD5Encode(szMid, szTemp, sizeof(szTemp));

			if(StrCmpIA(szTemp, szVerifyKey) == 0)
			{
				return (S_OK == StringCbCopyA((LPSTR)lpBuffer, cbSize, szMid) );
			}
		}
	}

	return FALSE;
}

static void SaveMidToRegister(LPTSTR lpMID)
{
	ATLASSERT(lpMID && lstrlenA((LPSTR)lpMID) == 32);

	CHAR szTemp[MAX_PATH];
	CHAR szVerifyKey[128];

	ZeroMemory(szVerifyKey, sizeof(szVerifyKey));
	MD5Encode((LPSTR)lpMID, szVerifyKey, sizeof(szVerifyKey));

	StringCbCopyA(szTemp ,sizeof(szTemp), (LPSTR)lpMID);
	StringCbCatA(szTemp, sizeof(szTemp), szVerifyKey);

	// not including the terminating null character. 
	SHSetValueA(HKEY_CURRENT_USER, "Software\\TheWorld6\\Liveup", "mid", REG_SZ, szTemp, strlen(szTemp)*sizeof(CHAR));
}

void GenClientId3( LPSTR lpszMid, DWORD cbSize )
{
	USES_CONVERSION;

	if(lpszMid == NULL || cbSize < 32)
		return;

	// 先从注册表中取回MID
	if(GetMidFromRegister((LPTSTR)lpszMid, cbSize))
		return;

	CHAR szMid[4096];
	CHAR szTemp[100];
	
	ZeroMemory(szMid, sizeof(szMid));

	GetCPUID(szMid, sizeof(szMid));
	
	// 先老算法，再新算法
	if( ReadFirstPhysicalDriveInNTWithAdminRights(szTemp, sizeof(szTemp)) ||
		ReadFirstPhysicalDriveInNTWithScsiBackDoor(szTemp, sizeof(szTemp)) ||
		ReadFirstPhysicalDriveInNTWithZeroRights(szTemp, sizeof(szTemp)) ||
		ReadFirstPhysicalDriveInNTWithSmart(szTemp, sizeof(szTemp)) )
//		ReadFirstPhysicalDriveInNTWithWMI(szTemp, sizeof(szTemp)) )
	{
		// 干掉右边的空格
		_StrTrimRightA(szTemp);
		StringCbCatA(szMid, sizeof(szMid), szTemp);
	}
	
	// 先新后老
	if( GetFirstAdapterMAC_NetworkCards(szTemp, sizeof(szTemp)) ||
		GetFirstAdapterMAC_Netbios(szTemp, sizeof(szTemp)) )
	{
		StringCbCatA(szMid, sizeof(szMid), szTemp);
	}

	BOOL bFailed = FALSE;
	if(szMid[0] == 0)
	{
		StringCbCopyA(szMid, sizeof(szMid), "Mid2Failed");
		bFailed = TRUE;
	}

	MD5Encode(szMid, lpszMid, cbSize);

	if(! bFailed)
		SaveMidToRegister((LPTSTR)lpszMid);
}

#if defined(SE6)
void GenHardDiskID( LPSTR lpszMid, DWORD cbSize )
{
  USES_CONVERSION;

  if(lpszMid == NULL || cbSize < 32)
    return;

  CHAR szMid[MAX_PATH];
  CHAR szTemp[MAX_PATH];

  ZeroMemory(szMid, sizeof(szMid));

  if( ReadFirstPhysicalDriveInNTWithAdminRights(szTemp, sizeof(szTemp)) ||
    ReadFirstPhysicalDriveInNTWithScsiBackDoor(szTemp, sizeof(szTemp)) ||
    ReadFirstPhysicalDriveInNTWithZeroRights(szTemp, sizeof(szTemp)) ||
    ReadFirstPhysicalDriveInNTWithSmart(szTemp, sizeof(szTemp)) )
  {
    _StrTrimRightA(szTemp);
    StringCbCatA(szMid, sizeof(szMid), szTemp);
  }


  BOOL bFailed = FALSE;
  if(szMid[0] == 0)
  {
    StringCbCopyA(szMid, sizeof(szMid), "Mid2Failed");
    bFailed = TRUE;
  }

  MD5Encode(szMid, lpszMid, cbSize);
}

void GenNetCardID( LPSTR lpszMid, DWORD cbSize )
{
  USES_CONVERSION;

  if(lpszMid == NULL || cbSize < 32)
    return;

  CHAR szMid[MAX_PATH];
  CHAR szTemp[MAX_PATH];

  ZeroMemory(szMid, sizeof(szMid));

  if( GetFirstAdapterMAC_NetworkCards(szTemp, sizeof(szTemp)) ||
    GetFirstAdapterMAC_Netbios(szTemp, sizeof(szTemp)) )
  {
    StringCbCatA(szMid, sizeof(szMid), szTemp);
  }

  BOOL bFailed = FALSE;
  if(szMid[0] == 0)
  {
    StringCbCopyA(szMid, sizeof(szMid), "Mid2Failed");
    bFailed = TRUE;
  }

  MD5Encode(szMid, lpszMid, cbSize);
}
#endif
