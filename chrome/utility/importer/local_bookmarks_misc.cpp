#include "local_bookmarks_misc.h"

std::wstring& SystemCommon::StringHelper::MakeLower(std::wstring &s)   
{  
	if(s.empty())   
	{  
		return s;  
	}  
	
	TCHAR* pBuf = new TCHAR[s.length()+1];
	_tcsncpy_s(pBuf, s.length()+1, s.c_str(), _TRUNCATE);
#if _MSC_VER < 1500
	_tcslwr(pBuf);
#else
	_tcslwr_s(pBuf, s.length() + 1);
#endif
	s = pBuf;
	delete [] pBuf;
	
	return s;  
} 

BOOL SystemCommon::StringHelper::CompareNoCase(std::wstring sA, std::wstring sB)   
{  
	MakeLower( sA );
	MakeLower( sB );
	if( 0 == sA.compare(sB))
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
} 

std::wstring SystemCommon::FilePathHelper::GetPath(std::wstring pathname) 
{
	int i = 0;
	for(i = pathname.size()-1; i>=0; i-- ) 
	{ 
		if( pathname[i]=='\\' || pathname[i]=='//') 
			break; 
	} 
	return pathname.substr( 0, i+1 ); 
}

BOOL SystemCommon::FilePathHelper::DeepCopyFile( const std::wstring& strSrc, const std::wstring& strDst )
{
	BOOL bRet = FALSE;
	if (PathFileExists(strSrc.c_str()))
	{
		if(!StringHelper::CompareNoCase(strSrc, strDst))
		{
			if (PathFileExists(strDst.c_str()))
			{
				SetFileAttributes(strDst.c_str(), FILE_ATTRIBUTE_NORMAL);//去掉只读属性
			}
			else
			{
				std::wstring strPath = GetPath( strDst );
				ForceCreateDir( strPath.c_str() );
			}

			if (!DeleteFile(strDst.c_str()))
			{
				std::wstring strDstTmp = strDst + L".tmp";
				MoveFileEx( strDst.c_str(), strDstTmp.c_str(), MOVEFILE_REPLACE_EXISTING );
				if (!DeleteFile(strDstTmp.c_str()))
				{
					MoveFileEx( strDstTmp.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT );
				}
			}
			::CopyFile( strSrc.c_str(), strDst.c_str(), FALSE );
			bRet = TRUE;
		}		
	}
	return bRet;
}

BOOL SystemCommon::FilePathHelper::ForceCreateDir( LPCTSTR pszFullPathFileName )
{
	if (!pszFullPathFileName || !pszFullPathFileName[0])
		return FALSE;

	if ( PathIsRoot( pszFullPathFileName ) )
		return TRUE;
	
	TCHAR Dir[MAX_PATH+1]={};
	int	 nNeed;
	LPCTSTR p, pLast;
	BOOL  Result;
	
	Result=FALSE;
	ATLASSERT(pszFullPathFileName);
	pLast=pszFullPathFileName;
	if(_tcslen(pLast)>_MAX_PATH) return FALSE;
	while(NULL!=*pLast)
	{
		p=_tcsstr(pLast,_T("\\"));
		if(NULL==p)
		{
			p=_tcsstr(pLast,_T("/"));
			
			if(NULL==p)
				return Result;
		}
		nNeed=p-pszFullPathFileName;
		if(nNeed>0)
		{
			memset(Dir,0,sizeof(Dir));
			_tcsncpy_s(Dir, pszFullPathFileName, p - pszFullPathFileName);
			Result=CreateMyDir(Dir);
		}
		p++;
		pLast=p;
	}
	return Result;
}

BOOL SystemCommon::FilePathHelper::CreateMyDir( LPTSTR pszDir )
{
	ATLASSERT(pszDir);

	if (!pszDir || !pszDir[0])
		return FALSE;

    BOOL bRet;
	bRet = CreateDirectory( pszDir, NULL );
	if( FALSE == bRet )
	{
		if( ERROR_ALREADY_EXISTS == GetLastError() )
			return TRUE;
	}
	return bRet;
}

#define MAX_FORMATE_BUF 16*1024
std::wstring SystemCommon::StringHelper::Format(const TCHAR *fmt, ...) 
{ 
	if (!fmt || !fmt[0])
		return std::wstring(fmt);

	std::wstring sReturn = fmt;
	try
	{
		TCHAR szBuf[MAX_FORMATE_BUF]={};
		
		va_list arglist;
		va_start(arglist, fmt);
#if _MSC_VER < 1500
		_vstprintf( szBuf, fmt, arglist );
#else
		_vstprintf_s( szBuf, fmt, arglist );
#endif
		va_end(arglist);
		
		sReturn = szBuf;
	}
	catch (...)
	{
		ATLASSERT(FALSE);
	}
	return sReturn;
}

std::string SystemCommon::StringHelper::TString2String( std::wstring str )
{
	std::string result;
#ifdef _UNICODE
	result = SystemCommon::StringHelper::Wstring2String(str.c_str());
#else
	result = str;
#endif
    return result;
}

std::string SystemCommon::StringHelper::Wstring2String( const wchar_t* pwc )
{
	if (!pwc || !pwc[0])
		return "";

	std::string result;
	
	int nLen=WideCharToMultiByte( CP_ACP, 0, (LPCWSTR)pwc, -1, NULL, 0, NULL, NULL );
	if(nLen<=0)
	{
		return std::string( "" );
	}
	char *presult=new char[nLen];
	if ( NULL == presult ) 
	{
		return std::string("");
	}
	WideCharToMultiByte( CP_ACP, 0, (LPCWSTR)pwc, -1, presult, nLen, NULL, NULL );
	presult[nLen-1]=0;
	result = presult;
	delete [] presult;

    return result;
}

std::wstring SystemCommon::StringHelper::String2WString( const char* pc )
{
	if (!pc || !pc[0])
		return L"";

	int nLen = strlen( pc );
	int nSize = MultiByteToWideChar( CP_ACP, 0, (LPCSTR)pc, nLen, 0, 0 );
	if ( nSize<=0 ) 
	{
		return std::wstring( L"" );
	}
	WCHAR *pDst=new WCHAR[nSize+1];
	if ( NULL == pDst )
	{
		return NULL;
	}
	MultiByteToWideChar( CP_ACP, 0, (LPCSTR)pc, nLen, pDst, nSize );
	pDst[nSize]=0;
	if ( 0xFEFF == pDst[0] )
	{
		for (int i=0;i<nSize;i++)
		{
			pDst[i]=pDst[i+1];
		}
	}
	std::wstring wcstr(pDst);
	delete [] pDst;
	return wcstr;
}

std::wstring SystemCommon::StringHelper::String2TString(const char* pc)
{
	std::wstring result;
#ifdef _UNICODE
	result = SystemCommon::StringHelper::String2WString(pc);
#else
	result = pc;
#endif
    return result;
}

std::wstring SystemCommon::FilePathHelper::GetParentPath(std::wstring pathname) 
{ 
  if(pathname.empty()) return pathname;
  TCHAR wszPath[1024]={_T('\0')};
	_tcsncpy_s(wszPath, pathname.c_str(), _TRUNCATE);
	if ( ('\\' == pathname[pathname.size()-1]) || ('/' == pathname[pathname.size()-1]) )
	{
		PathRemoveFileSpec( wszPath );
	}
	PathRemoveFileSpec( wszPath );

	return std::wstring( wszPath )+L"\\"; 
}

//查找文件
void SystemCommon::FilePathHelper::SearchFile( LPCWSTR szFindDir, LPCWSTR szFindFileName, std::vector<FileNameAndTime>& FileList )
{
	if ( !szFindDir || !szFindDir[0] || !szFindFileName || !szFindFileName[0] )
	{
		return;
	}
  WIN32_FIND_DATA	FindData = {0};
	HANDLE hSearch = INVALID_HANDLE_VALUE;
  std::vector<std::wstring> forders;
  forders.push_back(szFindDir);
  do
  {
    std::wstring search_files = forders[0] + L"\\" + szFindFileName;
    WIN32_FIND_DATA	FindData = {0};
    HANDLE hSearch = FindFirstFileEx( search_files.c_str(), FindExInfoStandard, &FindData, FindExSearchNameMatch, NULL, 0 );
    if( hSearch != INVALID_HANDLE_VALUE )
    {
 		  do
		  {
			  FileNameAndTime Data;
			  Data.strFullPath = forders[0] + L"\\" + FindData.cFileName;
			  Data.ftLastWriteTime = FindData.ftLastAccessTime;
			  FileList.push_back( Data );
 		  } while( FindNextFile( hSearch, &FindData) );
      FindClose(hSearch);
    }
    std::wstring search_forders = forders[0] + L"\\*.*";
    hSearch = FindFirstFileEx( search_forders.c_str(), FindExInfoStandard, &FindData, FindExSearchNameMatch, NULL, 0 );
    if( hSearch != INVALID_HANDLE_VALUE )
    {
      do
      {
 			  if (  !(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) 
				  || ( FindData.cFileName[0] == L'.' && FindData.cFileName[1] ==0) 
				  || ( FindData.cFileName[1] == L'.' && FindData.cFileName[2]==0 ))
			  {
				  continue;
			  }
        forders.push_back( forders[0] + L"\\" + FindData.cFileName );
      }while( FindNextFile( hSearch, &FindData) );
      FindClose(hSearch);
    }
    forders.erase( forders.begin() );
  }while( !forders.empty() );
}

//namespace Util
//{
//namespace CodeMisc
//{
//std::string UTF8EncodeW(LPCWSTR lpszUnicodeSrc)
//{
//	std::string utf8("");
//	if(!lpszUnicodeSrc)
//		return utf8;
//	
//	int u8Len = ::WideCharToMultiByte(CP_UTF8, NULL, lpszUnicodeSrc, wcslen(lpszUnicodeSrc), NULL, 0, NULL, NULL);
//	
//	char* szU8 = new char[u8Len + 1];
//	if(szU8)
//	{
//		::WideCharToMultiByte(CP_UTF8, NULL, lpszUnicodeSrc, wcslen(lpszUnicodeSrc), szU8, u8Len, NULL, NULL);
//		szU8[u8Len] = '\0';
//		utf8 = szU8;
//		delete []szU8;
//	}
//	return utf8;
//}
//
//std::string UTF8EncodeA(LPCSTR lpszAnsiSrc)
//{
//	std::string utf8("");
//	if(!lpszAnsiSrc)
//		return lpszAnsiSrc;
//
//	int  wcsLen  =  ::MultiByteToWideChar(CP_ACP, NULL, lpszAnsiSrc, strlen(lpszAnsiSrc), NULL,  0 );
//	wchar_t *  wszString  =   new  wchar_t[wcsLen  +   1 ];
//	if(wszString)
//	{
//		::MultiByteToWideChar(CP_ACP, NULL, lpszAnsiSrc, strlen(lpszAnsiSrc), wszString, wcsLen);
//		wszString[wcsLen] = L'\0';
//		utf8 = UTF8EncodeW(wszString);
//		delete []wszString;
//	}
//	return utf8;
//}
//
//std::wstring UTF8DecodeW(LPCSTR lpszUtf8Src)
//{
//	std::wstring strUnicode(L"");
//	if(!lpszUtf8Src)
//		return strUnicode;
//	
//	int wcsLen = ::MultiByteToWideChar(CP_UTF8, NULL, lpszUtf8Src, strlen(lpszUtf8Src), NULL, 0);
//	
//	wchar_t* wszString = new wchar_t[wcsLen + 1];
//	if(wszString)
//	{
//		::MultiByteToWideChar(CP_UTF8, NULL, lpszUtf8Src, strlen(lpszUtf8Src), wszString, wcsLen);
//		wszString[wcsLen] = L'\0';
//		strUnicode = wszString;
//		delete []wszString;
//	}	
//	return strUnicode;
//}
//
//std::string UTF8DecodeA(LPCSTR lpszUtf8Src)
//{
//	std::string strAnsi("");
//	if(!lpszUtf8Src)
//		return strAnsi;
//
//	int wcsLen = ::MultiByteToWideChar(CP_UTF8, NULL, lpszUtf8Src, strlen(lpszUtf8Src), NULL, 0);
//	
//	wchar_t* wszString = new wchar_t[wcsLen + 1];
//	if(wszString)
//	{
//		::MultiByteToWideChar(CP_UTF8, NULL, lpszUtf8Src, strlen(lpszUtf8Src), wszString, wcsLen);
//		wszString[wcsLen] = L'\0';
//
//		int ansiLen = ::WideCharToMultiByte(CP_ACP, NULL, wszString, wcslen(wszString), NULL, 0, NULL, NULL);
//		char* szAnsi = new char[ansiLen + 1];
//		if(szAnsi)
//		{
//			::WideCharToMultiByte(CP_ACP, NULL, wszString, wcslen(wszString), szAnsi, ansiLen, NULL, NULL);
//			szAnsi[ansiLen] = '\0';
//			strAnsi = szAnsi;
//			delete []szAnsi;
//		}
//		delete []wszString;
//	}
//
//	return strAnsi;
//}
//
//
//}
//}
//
