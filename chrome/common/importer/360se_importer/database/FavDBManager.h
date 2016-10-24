#pragma once
#include <vector>
#include <string>
#include <xstring>

#include "chrome/common/importer/360se_importer/Common.h"
#include "chrome/common/importer/360se_importer/MiscFunc.h"
#include "chrome/utility/importer/360se_importer.h"
#include "third_party/sqlite/sqlite3.h"
#include "sql/connection.h"
using namespace std;
// wanyong 20100331 收藏数据库管理器

struct FavItem;
struct DBInfo;
class CppSQLite3DB;
//class CFavDBThreadWnd;

struct BookmarkEntryWrapper {
	bool in_toolbar;
	std::wstring url;
	std::vector<std::wstring> path;
	std::wstring title;
	__int64 creation_time;

	BookmarkEntryWrapper() : in_toolbar(false) {}
};
//typedef std::vector<BookmarkEntryWrapper> BookmarkVectorWrapper;



//
// 数据库操作类，所在线程：CFavDBThreadWnd
//
class CFavDBManager
{
public:
	//  [7/23/2010 by ken] 

	bool ImportAllItems(vector<FavItem> & vecItems);

	bool ArrangeAllItems(vector<FavItem> const & vecItems,SafeBrowserImporter::BookmarkVector & result);

	std::vector<std::wstring> GenPath(FavItem,vector<FavItem> const &);

	// 传入了管理器线程指针，在适当的时候可能需要调用一些工具函数，他们在同一线程，因此不会出问题。
	CFavDBManager(/*CFavDBThreadWnd * pFavDBThreadWnd*/);
	~CFavDBManager();

	// 校验数据库文件属性，返回TRUE说明数据库文件可用
	static bool CheckDBFileAttribute(LPCTSTR lpszFile);

	// 检查数据库格式是否完整
	static bool CheckDBIntegrity(CppSQLite3DB * pDB);
	
	// 检查数据的版本号
	// bAutoUpgrade: 当数据库的版本比较低的时候，自动升级到现在的新版本
	static int CheckDBVersion(CppSQLite3DB * pDB);
	static bool CheckDBVersion(CppSQLite3DB * pDB, bool bAutoUpgrade);

	// 确保数据库和表都存在
	// bForceCreate: 当表不存在的时候是否强制创建
	static bool IsTablesExist(CppSQLite3DB * pDB, bool bForceCreate); 


	//////////////////////////////////////////////////////////////////////////

	// 切换数据库
	int ChangeDBPath(LPCTSTR lpszDBPath);

	// 合并数据库，将服务器的数据合并本地。
	//		lpszLocalDBPath:	本地收藏数据库路径
	//		lpszServerDBPath;	服务器收藏数据库路径
	//bool MergeDB(LPCTSTR lpszLocalDBPath, LPCTSTR lpszServerDBPath);

	// 判断文件夹/链接是否存在，如果存在则返回ID
	int ExistItem(int nParentID, bool bFolder, LPCTSTR lpszTitle);

	// 根据ID查找元素
	bool IsExistID(int nId);

	// 添加文件夹/链接
	int AddFolder(int nParentID, LPCTSTR lpszTitle, int nPos = -1);
	int AddItem(int nParentID, LPCTSTR lpszTitle, LPCTSTR lpszURL, int nPos = -1, bool bIsBest = false);

	// 获取指定文件夹下面的项
	int GetAllItems(int nParentID, vector<FavItem> & vecItems);

	// 获得指定项
	int GetItem(int nID, FavItem * pItem);

	// 设置某项为“我的最爱”
	int SetItemBest(int nId,  bool bBest);

	// 删除某一项
	int DeleteItem(int nId, bool bFolder);
	int BatchDelete(std::vector<int>* batchItems);

	// 修改一项,lpszUrl=NULL表示修改的为目录
	// *****需要去除title和url中的非法字符（'）
	int ModifyItem(int nId, LPCTSTR lpszTitle, LPCTSTR lpszUrl = NULL);

	// 移动某一项，在这之前需要调用CheckItemCollision确定有没有冲突
	int MoveItem(int nId, int nTargetId, int nMoveStyle, bool bBatch);

	// 判断nTargetId目录下，是否有与nId相同的项或目录
	int CheckItemCollision(int nId, int nTargetId);

	// 判断nDestId是否为nSrcId的子项，nSrcId必须为一个目录
	bool IsChild(int nSrcId, int nDestId);

	// 按照title排序，不递归，只对指定文件夹排序
	// 文件夹在前面，link在后面，两项分别按照title排序
	int SortFolderByTitle(int nParentId);

	// 按照添加的时间排序
	int SortFolderByCreateTime(int nParentId);

	// 由于事务不支持嵌套，所以做一个全局的事务控制
	bool BeginTransaction(bool bExistCommit = FALSE);
	bool RollbackTransaction();
	bool CommitTransaction();
	void EndTransaction();

	// 备份数据库
	bool BackupDB(LPCTSTR lpszDBFilePath);
	
	// 恢复数据库
	bool RestoreDB(LPCTSTR lpszDBFilePath);

	// 获得Favorite总条数
	int GetFavoriteTotal();

	// 获得数据库的信息
	int GetLocalDBInfo(DBInfo* pInfo);
	int GetServerDBInfo(LPCTSTR lpszDBFilePath, DBInfo* pInfo);

	// 覆盖数据库，将lpszDBFilePath数据库覆盖到本地
	// 再者之前，请务必调用GetLocalDBInfo和GetServerDBInfo以获得数据的信息，在确定可以覆盖后，在调用本方法
	bool OverwriteDB(LPCTSTR lpszDBFilePath);

	// 获得数据库的路径
	bool GetDBPath(LPTSTR lpszDBPath, DWORD dwSize);

	// 设置DB最后同步时间为当前时间
	void SetDBLastSynTime();

	inline int GetDBStatus(){return m_nDBStatus;}

	bool GetAllFolders(vector<FavItem> & vecItems);

	bool GetPathFromId(TCHAR* szPaht, int nSize, int nId);
	
	int GetFolderIdFromPath(LPCTSTR lpszFolderPath);

	// 获得收藏栏显示的目录的ID号
	int GetFavShowFolderId();

	// 设置收藏栏显示的目录的ID号
	bool SetFavShowFolderId(int nFolderId);

	int SearchItem(vector<FavItem>& vecItems,LPCTSTR lpszKeyWords);

	// 重新创建数据库（清空数据库）
	bool ReNewDB();

	// 获取/设置数据库的自动导入的状态
	int GetAutoLoad(int& nAutoLoad);
	int SetAutoLoad(int nAutoLoad);
protected:

	// 初始化数据库：检查数据文件是否存在，数据库格式是否完整，表是否存在。。。
	bool InitDB();
	
	void UninitDB();

	// 自动回复数据库
	bool RestoreDBAuto();

	// 日常备份，只限于备份本地数据库，网络数据已经备份到服务器上，无需备份
	bool DailyBackup();


	
	//////////////////////////////////////////////////////////////////////////

	// 添加文件夹/链接
	// *****需要去除title和url中的非法字符（'）
	int AddItem(int nParentID, bool bFolder, LPCTSTR lpszTitle, LPCTSTR lpszURL, int nPos = -1, bool bIsBest = false);

	// 删除项/目录
	int DeleteItem(int nId, int nParentId, int nPos, bool bMoveNexts = TRUE);
	int DeleteFolder(int nId, int nParentId, int nPos, bool bMoveNexts = TRUE);

	int DeleteItemNotify(int nId, int nParentId, int nPos, bool bMoveNexts = TRUE);
	int DeleteFolderNotify(int nId, int nParentId, int nPos, bool bMoveNexts = TRUE);

	// 按照添某字段排序文件夹，不递归
	int SortFolderByField(int nParentId, LPCSTR lpFildeName, bool bAsc = TRUE);

	// Move：没有冲突时，移动项/目录到新的目录下
	// 需要MoveOperStruct中字段：nSrcID,nSrcParentID,nSrcPos,  nDestParentID,nDestPos
	// 步骤1：将nSrcParentID目录中位置大于nSrcPos的项向上移
	// 步骤2：将nDestParentID目录中位置小于nDestPos的项向下移
	// 步骤3：更新nSrcID的父节点(变为nDestParentID)，位置(变为nDestPos)，时间
	int MoveItem2Folder(MoveOperStruct* pMS, bool bMovePrevs, bool bMoveNexts);

	// Merge：有冲突时，合并项/目录到新的目录下
	// 需要MoveOperStruct中字段：nSrcID,nSrcParentID,nSrcPos,  nDestParentID,nDestPos,  nCollisionID,nCollisionParentID,nCollisionPos
	// 步骤1：调用MoveItem2Folder，移动nSrcID到nDestParentID中的nDestPos位置
	// 步骤2：删除冲突的项（nCollisionID）
	int MergeItem2Folder(MoveOperStruct* pMS, bool bMovePrevs);
	// 需要MoveOperStruct中字段：nSrcID,nSrcParentID,nSrcPos,  nDestParentID,nDestPos,  nCollisionID,nCollisionParentID,nCollisionPos
	// 步骤1：查询nSrcId目录中与nCollisionId冲突的项/目录
	// 步骤2：调用MergeItem2Folder和递归调用MergeFolder2Folder，合并这些项和目录
	// 步骤3：查询nSrcId目录中与nCollisionId不冲突的项/目录
	// 步骤4：调用MoveItem2Folder，移动这些项/目录
	// 步骤5：删除自己
	// 步骤6：调用MoveItem2Folder，将nCollisionID项移动到nDestPos位置
	int MergeFolder2Folder(MoveOperStruct* pMS, bool bMovePrevs);

	int GetDBInfo(CppSQLite3DB * pDB, DBInfo* pInfo);

	// 算出需要备份的目标文件名
	CString GetDailyBackupFilePath();

	// 删除多于的备份文件，只保留7份数据
	void DeleteOldBackupFile();

	// 获得所有备份文件，并且以文件名从小到大排序
	void GetBackupFiles(std::vector<CString>& listFiles);

	// 获得某个目录下所有的目录
	void GetAllChildrenFolder(std::vector<int>& vecChildren, int nParnet);

	// 格式化为 utf8 字符串
	string GetUTF8String(LPCTSTR lpszString);

	bool MoveErrorDBFile(LPCTSTR lpszDBFile);

	

private:

	// 数据库线程的窗口指针，这里用于转发数据库通知
	//CFavDBThreadWnd * m_pFavDBThreadWnd;

	// SQLite3数据库操作对象
	CppSQLite3DB * m_pDB;

	// 当前数据库的文件全路径
	CString m_strDBPath;

	// 标识是否有没有提交是事务
	bool   m_bOpenTransaction;

	// 标识数据的打开状态，TRUE为成功打开，并且经过校验了
	bool  m_bDBOpen;

	// 标识数据库是否有改变
	bool m_bDBChanged;

	// 标志当前数据库的状态
	int	m_nDBStatus;
};


class SafeBrowserDBImporter{

public :
	SafeBrowserDBImporter(){}
	~SafeBrowserDBImporter(){}

	static sql::Connection* OpenAndCheck(std::wstring const & filename);
	static bool ImportData(sql::Connection* sqlite,vector<FavItem> & vecItems);
	static bool ArrangeAllItems(vector<FavItem> const & vecItems,SafeBrowserImporter::BookmarkVector & result);
	static std::vector<std::wstring> GenPath(FavItem item,vector<FavItem> const & vecItems);
};