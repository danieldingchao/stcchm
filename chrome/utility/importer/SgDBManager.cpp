#include "SgDBManager.h"
//#include "favorite-import-simulator.h"
#include "sogou_explorer_hacker.h"
#include "sogou_explorer_hacker_new.h"
// TODO:搜狗收藏开关
#define SOGOU_31
// 搜狗收藏文件名
#define FILE_SOGOU_FAVORITE_DAT          L"Favorite2.dat"
#define FILE_SOGOU_FAVORITE_DAT_V3          L"Favorite3.dat"

CSgDBManager::CSgDBManager() : m_pDB(NULL)
{
  TCHAR szAppDataPath[MAX_PATH] = {0};
  SHGetSpecialFolderPath(NULL, szAppDataPath, CSIDL_APPDATA, FALSE);
  m_strIniFile = SystemCommon::StringHelper::Format( L"%s\\360SE\\extensions\\Favorites\\Favorites2.ini", szAppDataPath);
  m_bDBOpen = FALSE;
}

CSgDBManager::~CSgDBManager()
{
  CloseDB();
}

//初始化操作，lpszFile为SOGOU收藏文件目录全路径
BOOL CSgDBManager::Init( LPCWSTR lpszFile, LPCWSTR lpszFileNameFlag )
{
  if ( !lpszFile || !lpszFile[0] || !lpszFileNameFlag )
  {
    return FALSE;
  }

  //获取TMP路径
  wchar_t szTmpPath[MAX_PATH+1] = {0};
    DWORD dwRetVal = GetTempPath( MAX_PATH, szTmpPath);
    if (dwRetVal > MAX_PATH || (dwRetVal == 0))
    {
        return FALSE;
    }

  int nVersion = 1;
  // 兼容新旧版本

  std::wstring strSGFavFile = SystemCommon::StringHelper::Format( L"%s\\%s", lpszFile, FILE_SOGOU_FAVORITE_DAT_V3 );
  if( PathFileExists(strSGFavFile.c_str()))
  {
    nVersion = 3;
  }
  if( nVersion <3 )
  {
    strSGFavFile = SystemCommon::StringHelper::Format( L"%s\\%s", lpszFile, FILE_SOGOU_FAVORITE_DAT );
    if( PathFileExists(strSGFavFile.c_str()))
    {
      nVersion = 2;
    }
  }

  std::wstring strSGDat = SystemCommon::StringHelper::Format( L"%s%stmp.dat", szTmpPath, lpszFileNameFlag );//搜狗数据库文件的拷贝
  DeleteFile(strSGDat.c_str());

  if( nVersion == 2 )
  {
    SystemCommon::FilePathHelper::DeepCopyFile( strSGFavFile.c_str(), strSGDat );
  }

  //strSGFavFile 搜狗原始文件   strSGDat 源文件的副本      m_strSGDB生成的解密文件
  m_strSGDB = SystemCommon::StringHelper::Format( L"%ssg_%d_tmp.db", szTmpPath, GetTickCount());//解密后的搜狗数据库文件
  DeleteFile(m_strSGDB.c_str());
  BOOL bSuc = TRUE;
  if( nVersion == 3 )
  {
    mainV3( (WCHAR *)strSGDat.c_str(),(WCHAR *)m_strSGDB.c_str());
  }
  else
  {
#ifdef FOR_360CHROME    
    bSuc = SEFavoriteDecrypt((char*)SystemCommon::StringHelper::Wstring2String(strSGDat.c_str()).c_str(),
      (char*)SystemCommon::StringHelper::Wstring2String(m_strSGDB.c_str()).c_str(),TRUE);
#endif
  }

  /*
#ifdef SOGOU_31
  if (!SEFavoriteDecrypt((char*)StringHelper::Wstring2String(strSGDat.c_str()).c_str(),
                       (char*)StringHelper::Wstring2String(m_strSGDB.c_str()).c_str(),
               TRUE))
  {
    return FALSE;
  }
#else
  if ( SG_OK != GenSGFile( StringHelper::Wstring2String(strSGDat.c_str()).c_str(),
    StringHelper::Wstring2String(m_strSGDB.c_str()).c_str()) )
  {
    return FALSE;
  }
#endif //SOGOU_31
  */

  DeleteFile( strSGDat.c_str() );
  if( bSuc )
  {
    bSuc = OpenDB( m_strSGDB.c_str() );
  }
  return bSuc;
}

void CSgDBManager::UnInit()
{
  CloseDB();
  DeleteFile( m_strSGDB.c_str() );
}

BOOL CSgDBManager::OpenDB( LPCWSTR lpszFile)
{
  if (NULL == lpszFile || 0 == lpszFile[0])
    return FALSE;

  BOOL bOpend = FALSE;
  if (NULL == m_pDB)
    m_pDB = new CppSQLite3DB();

  std::string strFile = UTF8Encode(lpszFile);
  if (m_pDB && m_pDB->open(strFile.c_str()))
  {
        bOpend = TRUE;
  }
  return bOpend;
}

void CSgDBManager::CloseDB()
{
  if (m_pDB)
  {
    m_pDB->close();
    delete m_pDB;
    m_pDB = NULL;
  }
}

//取得sogou的收藏文件所在目录，有多个账户时以最近修改为准
std::wstring CSgDBManager::GetSgFavFilePath()
{
  //取得sogou的用户数据目录
  wchar_t szFindPath[MAX_PATH] = {0} ;
  SHGetFolderPath( NULL, CSIDL_APPDATA, NULL, 0, szFindPath );
  wcscat( szFindPath, L"\\SogouExplorer" );

  wcscat( szFindPath, L"\\");
  wcscat( szFindPath, FILE_SOGOU_FAVORITE_DAT);
  return szFindPath;
}

//取得sogou的收藏文件所在目录，有多个账户时以最近修改为准
std::wstring CSgDBManager::GetSgFilePath()
{
  std::wstring sg_file_path = GetSogouFilePathV3();
  if (!sg_file_path.empty())
    return sg_file_path;
  //取得sogou的用户数据目录
  wchar_t szFindPath[MAX_PATH] = {0} ;
  SHGetFolderPath( NULL, CSIDL_APPDATA, NULL, 0, szFindPath );
  wcscat( szFindPath, L"\\SogouExplorer\\" );
  return std::wstring(szFindPath);
}

//查找文件
void CSgDBManager::SearchFile( LPCWSTR szFindDir, LPCWSTR szFindFileName, std::vector<FileData>& FileList )
{
  if ( !szFindDir || !szFindDir[0] || !szFindFileName || !szFindFileName[0] )
  {
    return;
  }
  WIN32_FIND_DATA    FindData;
    HANDLE        hSearch;
    wchar_t        szFileName[MAX_PATH] = {0};

  swprintf( szFileName, MAX_PATH, L"%s\\%s", szFindDir, szFindFileName );

  hSearch = FindFirstFileEx( szFileName, FindExInfoStandard, &FindData, FindExSearchNameMatch, NULL, 0 );
  if ( hSearch != INVALID_HANDLE_VALUE )
  {
    do
    {
      FileData Data;
      Data.strFullPath = std::wstring( szFindDir ) + L"\\" + FindData.cFileName;
      Data.ftLastWriteTime = FindData.ftLastAccessTime;
      FileList.push_back( Data );
    } while( FindNextFile( hSearch, &FindData) );
    FindClose(hSearch);
  }

  //查找子文件夹
  swprintf( szFileName, MAX_PATH, L"%s\\%s", szFindDir, L"*.*" );
  hSearch = FindFirstFileEx( szFileName, FindExInfoStandard, &FindData, FindExSearchNameMatch, NULL, 0 );
  if ( hSearch != INVALID_HANDLE_VALUE )
  {
    do
    {
      if (  !(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        || ( FindData.cFileName[0] == _T('.') && FindData.cFileName[1] ==0)
        || ( FindData.cFileName[1]==_T('.') && FindData.cFileName[2]==0 ))
      {
        continue;
      }

      swprintf( szFileName, MAX_PATH, L"%s\\%s", szFindDir, FindData.cFileName );
      SearchFile( szFileName, szFindFileName, FileList );//递归查找
    } while( FindNextFile( hSearch, &FindData ) );
    FindClose(hSearch);
  }
}

void CSgDBManager::CheckFirstRun()
{

  BOOL bFirst = GetPrivateProfileInt(_T("main"), _T("FirsProcessSgFav"), 1, m_strIniFile.c_str());
  if (bFirst)
  {
    if(IsValidityFav())
    {
      //Global::ShowFavTipWnd( L"导入收藏夹功能升级！\n请点击此处下拉菜单查看。" );
    }
  }
}

//判断搜狗收藏文件是否正确
BOOL CSgDBManager::IsValidityFav()
{
  BOOL bRet = FALSE;
  std::wstring strFlag = SystemCommon::StringHelper::Format( L"chk_%d", GetTickCount() );
  if( Init( SystemCommon::FilePathHelper::GetParentPath(GetSgFavFilePath()).c_str(), strFlag.c_str()) )
  {
    CppSQLite3Buffer sql;
    CppSQLite3Query oQuery;

    sql.format( "SELECT id FROM favorTable" );
    if ( m_pDB->execQuery(oQuery, sql))
    {
      if (!oQuery.eof())
      {
        bRet = TRUE;
      }
    }
    oQuery.finalize();
  }

  UnInit();//不管初始化成功与否都要调用
  return bRet;
}
