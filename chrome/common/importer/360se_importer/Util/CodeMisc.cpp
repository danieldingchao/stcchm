#include <windows.h>
#include <tchar.h>
#include <string>
#include <strsafe.h>
#include <Shlwapi.h>

namespace Util {
namespace CodeMisc {
/*	
void UnicodeToUtf8(char * utf8) 
{
	int len = 0;
	int size_d = 8;
	DWORD dwNum = MultiByteToWideChar (CP_ACP, 0, utf8, -1, NULL, 0);
	wchar_t *wchar;
	wchar = new wchar_t[dwNum];
	if(!wchar)
	{
		delete []wchar;
	}
	MultiByteToWideChar (CP_ACP, 0,utf8, -1, wchar, dwNum);
	for(int i = 0; i <dwNum; i++)
	{
		if ((wchar[i]) < 0x80)
		{  //
			utf8[len++] = (char)(wchar[i]);
		}
		else if((wchar[i]) < 0x800)
		{
			utf8[len++] = 0xc0 | ( (*wchar) >> 6 );
			utf8[len++] = 0x80 | ( (*wchar) & 0x3f );
		}
		else if((wchar[i]) < 0x10000 )
		{
			utf8[len++] = 0xE0 | ((wchar[i]) >> 12);
			utf8[len++] = 0x80 | (((wchar[i])>>6) & 0x3F);
			utf8[len++] = 0x80 | ((wchar[i]) & 0x3F);  
		}
		else if((wchar[i]) < 0x200000 ) 
		{
			utf8[len++] = 0xf0 | ( (int)(wchar[i]) >> 18 );
			utf8[len++] = 0x80 | ( ((wchar[i]) >> 12) & 0x3f );
			utf8[len++] = 0x80 | ( ((wchar[i]) >> 6) & 0x3f );
			utf8[len++] = 0x80 | ( (wchar[i]) & 0x3f );
		}
	}
}

CString UTF8toUnicode(char *s)
{
	int len = 0; 
	WCHAR* r = new WCHAR[strlen(s) * 2]; 
	while(s[0]) 
	{ 
		int bytes = 1; 
		if(s[0] & 0x80) 
			while(s[0] & (0x80 >> bytes)) bytes++; 
			if(bytes == 1) 
				r[len] = s[0]; 
			else 
			{ 
				r[len] = 0; 
				for(char*p = s + (bytes - 1); p > s; p--) 
					r[len] |= ((*p) & 0x3F) << ((bytes - (p - s) - 1) * 6); 
				r[len] |= (s[0] & ((1 << (7 - bytes)) - 1)) << ((bytes - 1) * 6); 
			} 
			len++; 
			s += bytes; 
	} 
	r[len] = 0; 
	char*buffer = new char[len * 2 + 1]; 
	ZeroMemory(buffer, len * 2 + 1); 
	::WideCharToMultiByte(CP_ACP, NULL, r, len, buffer, 1+ 2 * len, NULL, NULL); 
	CString str = buffer; 
	delete[] r; 
	delete[] buffer; 
	return str; 
}
*/

std::string UTF8EncodeW(LPCWSTR lpszUnicodeSrc)
{
	std::string utf8("");
	if(!lpszUnicodeSrc)
		return utf8;
	
	int u8Len = ::WideCharToMultiByte(CP_UTF8, NULL, lpszUnicodeSrc, wcslen(lpszUnicodeSrc), NULL, 0, NULL, NULL);
	
	char* szU8 = new char[u8Len + 1];
	if(szU8)
	{
		::WideCharToMultiByte(CP_UTF8, NULL, lpszUnicodeSrc, wcslen(lpszUnicodeSrc), szU8, u8Len, NULL, NULL);
		szU8[u8Len] = '\0';
		utf8 = szU8;
		delete []szU8;
	}
	return utf8;
}

std::string UTF8EncodeA(LPCSTR lpszAnsiSrc)
{
	std::string utf8("");
	if(!lpszAnsiSrc)
		return lpszAnsiSrc;

	int  wcsLen  =  ::MultiByteToWideChar(CP_ACP, NULL, lpszAnsiSrc, strlen(lpszAnsiSrc), NULL,  0 );
	wchar_t *  wszString  =   new  wchar_t[wcsLen  +   1 ];
	if(wszString)
	{
		::MultiByteToWideChar(CP_ACP, NULL, lpszAnsiSrc, strlen(lpszAnsiSrc), wszString, wcsLen);
		wszString[wcsLen] = L'\0';
		utf8 = UTF8EncodeW(wszString);
		delete []wszString;
	}

	return utf8;
}

std::wstring UTF8DecodeW(LPCSTR lpszUtf8Src)
{
	std::wstring strUnicode(L"");
	if(!lpszUtf8Src)
		return strUnicode;
	
	int wcsLen = ::MultiByteToWideChar(CP_UTF8, NULL, lpszUtf8Src, strlen(lpszUtf8Src), NULL, 0);
	
	wchar_t* wszString = new wchar_t[wcsLen + 1];
	if(wszString)
	{
		::MultiByteToWideChar(CP_UTF8, NULL, lpszUtf8Src, strlen(lpszUtf8Src), wszString, wcsLen);
		wszString[wcsLen] = L'\0';
		strUnicode = wszString;
		delete []wszString;
	}	
	return strUnicode;
}

std::string UTF8DecodeA(LPCSTR lpszUtf8Src)
{
	std::string strAnsi("");
	if(!lpszUtf8Src)
		return strAnsi;

	int wcsLen = ::MultiByteToWideChar(CP_UTF8, NULL, lpszUtf8Src, strlen(lpszUtf8Src), NULL, 0);
	
	wchar_t* wszString = new wchar_t[wcsLen + 1];
	if(wszString)
	{
		::MultiByteToWideChar(CP_UTF8, NULL, lpszUtf8Src, strlen(lpszUtf8Src), wszString, wcsLen);
		wszString[wcsLen] = L'\0';

		int ansiLen = ::WideCharToMultiByte(CP_ACP, NULL, wszString, wcslen(wszString), NULL, 0, NULL, NULL);
		char* szAnsi = new char[ansiLen + 1];
		if(szAnsi)
		{
			::WideCharToMultiByte(CP_ACP, NULL, wszString, wcslen(wszString), szAnsi, ansiLen, NULL, NULL);
			szAnsi[ansiLen] = '\0';
			strAnsi = szAnsi;
			delete []szAnsi;
		}
		delete []wszString;
	}

	return strAnsi;
}

BOOL GetPEFileVersion( LPCTSTR lpszFilePath, LPTSTR pszVersion, DWORD dwSize)
{
    if(!lpszFilePath || !pszVersion || 0 == dwSize)
        return FALSE;
    
    TCHAR  szVersionBuffer[8192] = {0};
    DWORD dwVerSize = 0;
    DWORD dwHandle = 0;
    
    dwVerSize = ::GetFileVersionInfoSize(lpszFilePath, &dwHandle );
    if( dwVerSize == 0 || dwVerSize > (sizeof(szVersionBuffer) - 1))
        return FALSE;
    
    if(::GetFileVersionInfo( lpszFilePath, 0, dwVerSize, szVersionBuffer) )
    {
        VS_FIXEDFILEINFO * pInfo = NULL;
        unsigned int nInfoLen = 0;
        
        if(::VerQueryValue( szVersionBuffer, _T("\\"), (void**)&pInfo, &nInfoLen ) )
        {
            StringCchPrintf( pszVersion, dwSize, _T("%d.%d.%d.%d"), 
                HIWORD( pInfo->dwFileVersionMS ), LOWORD( pInfo->dwFileVersionMS ), 
                HIWORD( pInfo->dwFileVersionLS ), LOWORD( pInfo->dwFileVersionLS ) );
            return TRUE;
        }
    }
    return FALSE;
}

//  扩展点击统计
void ClickStat()
{
#define WM_EXTENSION_CLICK (WM_USER + 0x1012)
    HWND hWnd = FindWindowEx(HWND_MESSAGE, NULL, _T("XWnd"), _T("360se_ExtAddonsWnd"));
    if (hWnd)
    {
        DWORD dwRet = 0;
        ::SendMessageTimeout(hWnd, WM_EXTENSION_CLICK, (WPARAM)_T("Favorites"), 0, SMTO_ABORTIFHUNG, 500, &dwRet);
    }
}

LPCTSTR GetSEMidString()
{
    static TCHAR szMid[33] = {0};
    if (!szMid[0])
    {
        TCHAR szValue[MAX_PATH] = {0};
        DWORD dwSize = MAX_PATH-1;
        DWORD dwType = REG_SZ;
        ::SHGetValue( HKEY_LOCAL_MACHINE, _T("SOFTWARE\\360Safe\\Liveup"), _T("mid"), &dwType, szValue, &dwSize );
        _sntprintf_s( szMid, 32, _T("%s"), szValue );
    }
    return szMid;
}

} // end namespace Base
} // end namespace Util