// MIDGen.h: interface for the CMIDGen class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MIDGEN_H__FDE2694A_7D49_4683_A30A_5FBAEB251CB0__INCLUDED_)
#define AFX_MIDGEN_H__FDE2694A_7D49_4683_A30A_5FBAEB251CB0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "base/base_export.h"

BOOL ReadFirstPhysicalDriveInNTWithAdminRights(LPSTR lpSerialNumber, DWORD cbSize);
BOOL ReadFirstPhysicalDriveInNTWithScsiBackDoor(LPSTR lpSerialNumber, DWORD cbSize);
BOOL ReadFirstPhysicalDriveInNTWithZeroRights (LPSTR lpSerialNumber, DWORD cbSize);
BOOL ReadFirstPhysicalDriveInNTWithSmart(LPSTR lpSerialNumber, DWORD cbSize);
BOOL ReadFirstPhysicalDriveInNTWithWMI(LPSTR lpSerialNumber, DWORD cbSize);

BOOL GetFirstAdapterMAC_NetworkCards(LPSTR lpszMAC, DWORD cbSize);
BOOL GetFirstAdapterMAC_Netbios(LPSTR lpszMAC, DWORD cbSize);

BOOL GetCPUID(LPSTR lpBuffer, DWORD cbSize);

BASE_EXPORT void GenClientId3(LPSTR lpszMid, DWORD cbSize);
#if defined(SE6)
BASE_EXPORT void GenHardDiskID(LPSTR lpszMid, DWORD cbSize);
BASE_EXPORT void GenNetCardID(LPSTR lpszMid, DWORD cbSize);
#endif
#endif // !defined(AFX_MIDGEN_H__FDE2694A_7D49_4683_A30A_5FBAEB251CB0__INCLUDED_)
