#ifndef _CRC_UTILITY_H_
#define _CRC_UTILITY_H_

DWORD BufCrc32(const BYTE* pBinBuf, int nBufLen);
DWORD FileCrc32Assembly(LPCTSTR szFilename, DWORD &dwCrc32);

#endif
