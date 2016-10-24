//SgDBManager.h
#pragma once
#ifndef __SG_DB_MANAGER__
#define __SG_DB_MANAGER__
#include "local_bookmarks_misc.h"
#include "chrome/common/importer/360se_importer/database/CppSQLite3.h"
#include "chrome/common/importer/360se_importer/util/CodeMisc.h"

class CppSQLite3DB;

struct FileData
{
  std::wstring  strFullPath;    //文件完整路径
  FILETIME    ftLastWriteTime;  //文件最后修改时间
};

//
//wrf:搜狗数据文件操作类
//
class CSgDBManager
{
public:
  CSgDBManager();
  ~CSgDBManager();

  BOOL Init( LPCWSTR lpszFile, LPCWSTR lpszFileNameFlag = L"" );
  void UnInit();

  CppSQLite3DB* GetDB(){return m_pDB;};

  //取得sogou的收藏文件所在目录，有多个账户时以最近修改为准
  static std::wstring GetSgFavFilePath();
  static std::wstring GetSgFilePath();

  //新版收藏夹首次加载时（非浏览器全新安装首次运行），检测是否存在搜狗浏览器。
  //如果存在，尝试解密搜狗浏览器收藏夹。如果解密成功，弹出气泡
  void CheckFirstRun();

  //将标记设置为非第一次运行
  void SetNoFirstRun();

  //判断搜狗收藏文件是否正确
  BOOL IsValidityFav();

protected:
  // 打开数据库
  BOOL OpenDB( LPCWSTR lpszFile);

  // 关闭数据库
  void CloseDB();

  //查找文件
  static void SearchFile( LPCWSTR szFindDir, LPCWSTR szFindFileName, std::vector<FileData>& FileList );


private:
  // SQLite3数据库操作对象
  CppSQLite3DB* m_pDB;

  // 当前数据库的文件全路径
  std::wstring m_strSGDB;

  // 标识数据的打开状态，TRUE为成功打开，并且经过校验了
  bool  m_bDBOpen;

  std::wstring m_strIniFile;
};

#endif//__SG_DB_MANAGER__