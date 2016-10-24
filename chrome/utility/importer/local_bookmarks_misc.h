#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <windows.h>
#include <atlbase.h>
#include <strsafe.h>
#include <shlobj.h>
#pragma comment(lib,"strsafe.lib")
#pragma comment(lib,"shell32.lib")

#ifndef tstring
#ifdef UNICODE
#define tstring std::wstring
#else
#define tstring std::string
#endif // #ifdef UNICODE
#endif // #ifndef tstring

#define	LPCTSTR2(lpszData) ((lpszData == NULL) ? _T("") : lpszData)
#define	LPCSTR2(lpszData) ((lpszData == NULL) ? ("") : lpszData)


/** 
* @class   CTString
* @brief   
* 
* @ingroup  
**/
class CTString : public tstring
{
public:
    CTString() : m_pchData(NULL) {}
    CTString(LPCTSTR lpszContent) : m_pchData(NULL)  { *((tstring*)this) = LPCTSTR2(lpszContent);}
    CTString(const tstring& strContent) : m_pchData(NULL)  { *((tstring*)this) = strContent;}
//    CTString(const CString& lpszContent) : m_pchData(NULL)  { *((tstring*)this) = lpszContent;}

	virtual ~CTString() {ReleaseBuffer();}

protected:
	LPTSTR m_pchData;   // pointer to ref counted string data

public:
    void operator= (LPCTSTR lpszContent)
    {
        if (NULL == lpszContent)
            *((tstring*)this) = _T("");
        else
        {
            ATLASSERT((_tcslen(lpszContent) < (1024 * 1024)) && L"String max than 1Mb!");
            *((tstring*)this) = lpszContent;
        }
    }
    
    void operator= (const tstring& strContent)
    {
        *((tstring*)this) = strContent;
    }

    operator LPCTSTR() const   // as a C string
    {
			return c_str();
	}

	operator LPTSTR()
	{
		return (TCHAR*)c_str();
	}
	
	int GetLength() const
	{
		return size();
	}

	BOOL IsEmpty() const
	{
		return empty();
	}

    #define MAX_FORMATE_BUF 16*1024
    BOOL Format(const TCHAR *fmt, ...) 
    {
        BOOL bRet = FALSE;
        if (!fmt || !fmt[0])
            return bRet;
        
        try
        {
            TCHAR szBuf[MAX_FORMATE_BUF]={0};
            va_list arglist;
            va_start(arglist, fmt);
#if _MSC_VER < 1500
            _vstprintf( szBuf,MAX_FORMATE_BUF, fmt, arglist );
#else
			_vstprintf_s( szBuf, MAX_FORMATE_BUF,fmt, arglist );
#endif
            va_end(arglist);
            *this = szBuf;
            bRet = TRUE;
        }
        catch (...)
        {
            ATLASSERT(FALSE);
        }
        return bRet;
    }

    LPCTSTR MakeUpper()
    {
        int nLen = size();
        TCHAR* pBuffer = new TCHAR[nLen + 1];
        if (pBuffer)
        {
            memset(pBuffer, 0, (nLen + 1) * sizeof(TCHAR));
#if _MSC_VER < 1500
            _tcsncpy(pBuffer, c_str(), nLen);
#else
            _tcsncpy_s(pBuffer, nLen + 1, c_str(), nLen);
#endif
            CharUpper(pBuffer);
            *((tstring*)this) = pBuffer;
            delete [] pBuffer;
            pBuffer = NULL;
        }
        else
        {
            ATLASSERT(FALSE);
        }
        return *this;
    }

    LPCTSTR MakeLower()
    {
        int nLen = size();
        TCHAR* pBuffer = new TCHAR[nLen + 1];
        if (pBuffer)
        {
            memset(pBuffer, 0, (nLen + 1) * sizeof(TCHAR));
#if _MSC_VER < 1500
            _tcsncpy(pBuffer, c_str(), nLen);
#else
            _tcsncpy_s(pBuffer, nLen + 1, c_str(), nLen);
#endif
            CharLower(pBuffer);
            *((tstring*)this) = pBuffer;
            delete [] pBuffer;
            pBuffer = NULL;
        }
        else
        {
            ATLASSERT(FALSE);
        }
        return *this;
    }

    CTString Left(int n) const
    {
        tstring strRet = substr(0, n);
        return CTString(strRet);
    }

    CTString Right(int n) const
    {
        tstring strRet = substr(size() - n, n);
        return CTString(strRet);
    }

	CTString Mid(int nLen) const
	{
		return Mid(0, nLen);
	}

    CTString Mid(int nStartPos, int nLen) const
    {
        tstring strRet = substr(nStartPos, nLen);
        return CTString(strRet);
    }
        
    int Compare(LPCTSTR lpszStr) const
    {
        return StrCmp(c_str(), lpszStr);
    }

    int CompareNoCase(LPCTSTR lpszStr) const
    {
        return StrCmpI(c_str(), lpszStr);
    }

    LPCTSTR TrimLeft()
    {
        for (tstring::iterator it = this->begin(); it != this->end();)
        {
            if ((*it) == _T(' ') || (*it) == _T('\r') || (*it) == _T('\n') || (*it) == _T('\t'))
                this->erase(it);            
            else
                it++;
        }
        return *this;
    }

    LPCTSTR TrimRight()
    {
        for (tstring::reverse_iterator it = this->rbegin(); it != this->rend();)
        {
            if ((*it) == _T(' ') || (*it) == _T('\r') || (*it) == _T('\n') || (*it) == _T('\t'))
                this->erase((it + 1).base());            
            else
                it++;
        }
        return *this;
    }

    LPCTSTR Replace(LPCTSTR lpszOld, LPCTSTR lpszNew)
    {
        if (lpszOld && lpszOld[0] && lpszNew && lpszNew[0])
        {
            for (tstring::size_type pos(0); pos != tstring::npos; pos += _tcslen(lpszNew))
            {  
                if ((pos = find(lpszOld, pos)) != tstring::npos)  
                    replace(pos, _tcslen(lpszOld), lpszNew);  
                else break;  
            }  
	        return *this;         
        }
        else
        {
            ATLASSERT(lpszOld && lpszOld[0] && lpszNew && lpszNew[0]);
        }
        return NULL;
    }
    
    int Find(LPCTSTR lpszFind, int nStart = 0)
    {
        if (lpszFind && lpszFind[0])
            return (int)find(lpszFind, nStart);
        else
        {
            ATLASSERT(lpszFind && lpszFind[0]);
        }
        return -1;
    }
         
	LPTSTR GetBuffer(int nMinBufLength)
	{
		ReleaseBuffer();
	
		int nLen = size();
		if (nMinBufLength < nLen)
			nMinBufLength = nLen + 1;
		//else ok

		ATLASSERT(nMinBufLength > 0);
		m_pchData = new TCHAR[nMinBufLength];

#if _MSC_VER < 1500
		_tcsncpy(m_pchData, c_str(), nLen);
#else
		_tcsncpy_s(m_pchData, nMinBufLength, c_str(), _TRUNCATE);
#endif

		return m_pchData;
	}

	void ReleaseBuffer(int nNewLength = -1)
	{
		if (m_pchData != NULL)
		{
			delete [] m_pchData;
			m_pchData = NULL;
		}
	}

    BSTR AllocSysString() const
    {
#if defined(_UNICODE) || defined(OLE2ANSI)
        BSTR bstr = ::SysAllocStringLen(c_str(), size());
#else
        int nLen = MultiByteToWideChar(CP_ACP, 0, c_str(),
            size(), NULL, NULL);
        BSTR bstr = ::SysAllocStringLen(NULL, nLen);
        if (bstr != NULL)
            MultiByteToWideChar(CP_ACP, 0, c_str(), size(), bstr, nLen);
#endif
        return bstr;
	}

	void PathAddBackslash()
	{
		if (this->size() > 0)
		{
			if (this->Right(1) != _T("/") || this->Right(1) != _T("\\"))
				(*this) += _T("\\");
			//else ok
		}
		//else ok
	}
};

struct FileNameAndTime
{
	std::wstring	strFullPath;		//文件完整路径
	FILETIME		ftLastWriteTime;	//文件最后修改时间
};

struct FavItemEx
{
	int nID;					// id
	int nParentID;				// 所在文件夹的id
	bool bFolder;				// 是否是文件夹
	bool bBest;					// 是否我的最爱
	int  nPos;					// 收藏夹的位置
	std::wstring strTitle;		// 标题
	std::wstring strURL;		// 链接
	__int64 lCreateTime;		// 创建时间
	__int64 lLastModifyTime;	// 上次修改时间
	//	bool bDeleted;			// 是否被删除
	int nReserved;				// 保留字段
	std::string strID;		// 字符串类型的ID
	std::string strPID;		// 字符串类型的PID
	std::string strTitleA;


	FavItemEx & operator=(const FavItemEx & t1)
	{
    if( this != &t1 )
    {
		  nID = t1.nID;
		  nParentID = t1.nParentID;
		  bFolder = t1.bFolder;
		  bBest = t1.bBest;
		  nPos = t1.nPos;
		  strTitle = t1.strTitle;
      strTitleA = t1.strTitleA;
		  strURL = t1.strURL;
		  lCreateTime = t1.lCreateTime;
		  lLastModifyTime = t1.lLastModifyTime;
      strID = t1.strID;
      strPID = t1.strPID;
    }
		return *this;
	}
};

class SystemCommon
{
public:
class FilePathHelper
{
public:
  static BOOL DeepCopyFile( const std::wstring& strSrc, const std::wstring& strDst );
  static std::wstring SystemCommon::FilePathHelper::GetPath(std::wstring pathname);
  static BOOL ForceCreateDir( LPCTSTR pszFullPathFileName );
  static BOOL CreateMyDir( LPTSTR pszDir );
  static std::wstring GetParentPath(std::wstring pathname);
  static void SearchFile( LPCWSTR szFindDir, LPCWSTR szFindFileName, std::vector<FileNameAndTime>& FileList );

};
class StringHelper
{
public:
  static std::wstring& MakeLower(std::wstring &s);
  static BOOL CompareNoCase(std::wstring sA, std::wstring sB);
  static std::wstring Format(const TCHAR *fmt, ...) ;
  static std::string TString2String( std::wstring str );
  static std::string Wstring2String( const wchar_t* pwc );
  static std::wstring String2WString( const char* pc );
  static std::wstring String2TString(const char* pc);
};

};
//namespace Util
//{
//namespace CodeMisc
//{
//  std::string UTF8EncodeA(LPCSTR lpszAnsiSrc);
//  std::string UTF8EncodeW(LPCWSTR lpszUnicodeSrc);
//  std::wstring UTF8DecodeW(LPCSTR lpszUtf8Src);
//  std::string UTF8DecodeA(LPCSTR lpszUtf8Src);
//
//}
//}
//
//#ifdef _UNICODE
//#define UTF8Encode Util::CodeMisc::UTF8EncodeW
//#define UTF8Decode Util::CodeMisc::UTF8DecodeW
//#else
//#define UTF8Encode Util::CodeMisc::UTF8EncodeA
//#define UTF8Decode Util::CodeMisc::UTF8DecodeA
//#endif