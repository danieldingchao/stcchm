//MaxthonDBManager.h        2012.7 by 段培堃
#ifndef __MX_DB_MANAGER__
#define __MX_DB_MANAGER__

#include "chrome/utility/importer/SgDBManager.h"

class CMaxthonDBManager
{
public:
	CMaxthonDBManager();
	~CMaxthonDBManager();
	
	// lpszFile:父目录，lpszFile:文件
	BOOL Init( LPCWSTR lpszFilePath, LPCWSTR lpszFile, LPCWSTR lpszFileNameFlag = L"" );
	void UnInit();

	CppSQLite3DB* GetDB(){return m_pDB;};

	//取得Maxthon的收藏文件所在目录，有多个账户时以最近修改为准
	std::wstring GetMaxthonFavFilePath();

protected:
  // 检查数据库格式是否完整
  static bool CheckDBIntegrity(CppSQLite3DB * pDB);

  // 打开数据库
	BOOL OpenDB( LPCWSTR lpszFile);

	// 关闭数据库
	void CloseDB();

	//查找文件
	void SearchFile( LPCWSTR szFindDir, LPCWSTR szFindFileName, std::vector<FileNameAndTime>& FileList );


private:
	// SQLite3数据库操作对象
	CppSQLite3DB* m_pDB;

	// 当前数据库的文件全路径
	std::wstring m_strMaxthonDB;

	// 标识数据的打开状态，TRUE为成功打开，并且经过校验了
	bool  m_bDBOpen;

	std::wstring m_strIniFile;
};

#endif//__MX_DB_MANAGER__