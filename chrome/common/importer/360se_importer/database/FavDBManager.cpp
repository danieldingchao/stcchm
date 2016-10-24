#ifndef _CRT_NON_CONFORMING_SWPRINTFS
#define _CRT_NON_CONFORMING_SWPRINTFS
#endif
#include <windows.h>
#include "FavDBManager.h"
//#include "FavDBThreadWnd.h"
//#include "FavDBMergeHelper.h"
#include "CppSQLite3.h"
#include "chrome/common/importer/360se_importer/Util/CodeMisc.h"
//#include "..\..\Utility\XLog.h"
#include <list>
#include <map>
#include <string>
#include <algorithm>
#include <Shlwapi.h>
//#include "../../utility/statistics/Statistics.h"

#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
//#include "chrome/browser/importer/importer_host.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "third_party/sqlite/sqlite3.h"

#define XLOGINFO(...)  
#define XLOGINFOA(...)  


#pragma   warning(   push   )  
#pragma  warning(disable:4390)

typedef vector<MoveOperStruct> MoveOperVector;

typedef std::multimap<int, int> FolderMap;
typedef std::pair<FolderMap::iterator, FolderMap::iterator> FolderMapItPair;
typedef std::pair<int, int> FolderPair;

typedef vector<DBTitleStor>	VecDBTitleStor;

// 定义常用的错误检查返回
#define CHECK_DB_RETURN(RES)		if(!m_pDB || !m_bDBOpen) return RES;
#define CHECK_DB					if(!m_pDB || !m_bDBOpen) return;

#if 0
CFavDBManager::CFavDBManager(/*CFavDBThreadWnd * pFavDBThreadWnd*/) : /*m_pFavDBThreadWnd(pFavDBThreadWnd), */m_pDB(NULL)
{
	m_bOpenTransaction = FALSE;
	m_bDBOpen = FALSE;
	m_bDBChanged = FALSE;
	m_nDBStatus = DB_STATUS_NEW;
}

CFavDBManager::~CFavDBManager()
{
	// 在DB线程退出时，先保存是否要备份
	bool bNeedDailyBackup = m_bDBOpen && m_bDBChanged;
	//CStatistics::GetInstance()->SetCount(COUNT_FAV_COUNT, GetFavoriteTotal());
	UninitDB();

	// 备份操作
// 	if (bNeedDailyBackup)
// 		DailyBackup();

	if (m_pDB)
	{
		delete m_pDB;
		m_pDB = NULL;
	}
}

// static

bool CFavDBManager::CheckDBFileAttribute(LPCTSTR lpszFile)
{
	if(!lpszFile)
		return false;

	// 如果文件不存在，返回可用
	if(!::PathFileExists(lpszFile))
		return true;

	return SetFileAttributes(lpszFile, FILE_ATTRIBUTE_NORMAL) == TRUE;
}

bool CFavDBManager::CheckDBIntegrity(CppSQLite3DB * pDB)
{
	return true;
	bool bRet = false;
	CppSQLite3Query query;
	if (pDB->execQuery(query, "pragma integrity_check(1);") && 0 == query.eof())
	{
		const char* pok = query.fieldValue(0);
		bRet = (0 == _strnicmp(pok, "ok", 2) ? true : false);
	}
	query.finalize();
	
/*
	if(!bRet)
		XLOGINFO(_T("CFavDBManager::CheckDBIntegrity, integrity_check:false"));*/

	return bRet;
}

int CFavDBManager::CheckDBVersion(CppSQLite3DB * pDB)
{
	if(!pDB)
		return FAV_DB_SQLITE_ERROR;

	int nRes = FAV_DB_SQLITE_ERROR;
	CppSQLite3Buffer sql;
	CppSQLite3Query oQuery;
	sql.format("select value from tb_misc where key='%s'", FAVDB_VERSION_KEY);
	if (pDB->execQuery(oQuery, sql) && !oQuery.eof())
	{
		const char* dbVersion = oQuery.getStringField(0);
		if(dbVersion)
		{
			int nVerComp = strcmp(dbVersion, FAVDB_VERSION_VALUE);
			if(nVerComp < 0)
			{
				// todo: 低版本升级到高版本
				nRes = FAV_DB_VERSION_LOWER;
				XLOGINFO(_T("Version of the database is relatively low"));
			}
			else if (nVerComp > 0)
			{
				// 数据的版本比较高
				// todo:升级收藏夹扩展
				nRes = FAV_DB_VERSION_HIGHER;
				XLOGINFO(_T("Version of the database is relatively higher"));
			}
			else
				nRes = FAV_DB_SUCCEEDED;
		}
		oQuery.finalize();
	}

	if(nRes != FAV_DB_SUCCEEDED)
		XLOGINFO(_T("CFavDBManager::CheckDBVersion:false"));
	return nRes;
}
// static 
bool CFavDBManager::CheckDBVersion(CppSQLite3DB * pDB, bool bAutoUpgrade)
{
	if(!pDB)
		return false;
	bool bRes = false;
	CppSQLite3Buffer sql;
	CppSQLite3Query oQuery;
	sql.format("select value from tb_misc where key='%s'", FAVDB_VERSION_KEY);
	if (pDB->execQuery(oQuery, sql) && !oQuery.eof())
	{
		const char* dbVersion = oQuery.getStringField(0);
		if(dbVersion)
		{
			int nVerComp = strcmp(dbVersion, FAVDB_VERSION_VALUE);
			if(nVerComp < 0)
			{
				// 数据库的版本比较低
				if(bAutoUpgrade)
				{
					// todo: 低版本升级到高版本
					XLOGINFO(_T("Version of the database is relatively low"));
				}
			}
			else if (nVerComp > 0)
			{
				// 数据的版本比较高
				// todo:升级收藏夹扩展
				XLOGINFO(_T("Version of the database is relatively higher"));
			}
			else
				bRes = true;
		}
		oQuery.finalize();
	}

	if(!bRes)
		XLOGINFO(_T("CFavDBManager::CheckDBVersion:false"));
	return bRes;
}

// static
bool CFavDBManager::IsTablesExist(CppSQLite3DB * pDB, bool bForceCreate)
{
	bool bRet = false;
	
	// 配置表是否存在
	if (!pDB->tableExists("tb_misc"))
	{
		if (bForceCreate)
		{
			pDB->execDML("CREATE TABLE [tb_misc] ("
				"[key] varchar(64) PRIMARY KEY,"						// 关键字
				"[value] varchar(128) default 0,"						// 描述
				"[reserved] int DEFAULT 0)"								// 保留字段
				);	
			// 插入当前数据库的版本，这样可以做一些兼容性的处理
			// todo: 定期回顾一下。
			CppSQLite3Buffer sql;
			sql.format("insert into tb_misc('key', 'value') values('%s', '%s');"
							"insert into tb_misc('key', 'value') values('%s', '%lld');"
							, FAVDB_VERSION_KEY, FAVDB_VERSION_VALUE
							, FAVDB_SYN_TIME_KEY, Util::Base::GetCurrentFileTime());
			pDB->execDML(sql);	
			if (!pDB->tableExists("tb_misc"))
				return false;
		}
	}
	
	// 收藏表
	if (!pDB->tableExists("tb_fav"))
	{
		if (bForceCreate)
		{
			pDB->execDML("CREATE TABLE [tb_fav] ("
				"[id] INTEGER PRIMARY KEY AUTOINCREMENT,"				// 自增id
				"[parent_id] int default 0,"							// 父节点id
				"[is_folder] tinyint check(is_folder in (0,1)),"		// 是否是文件夹
				"[title] varchar(256),"									// 标题
				"[url] varchar(1024),"									// 链接，如果是文件夹，此项为空
				"[pos] int default 0,"									// 位置，用于排序
				"[create_time] bigint,"									// 创建时间
				"[last_modify_time] bigint default 0,"					// 上次修改时间
				"[is_best] tinyint default 0,"							// 是否是最爱
//				"[is_deleted] tinyint default 0,"						// 是否被删除
				"[reserved] int default 0);"							// 保留字段
				"CREATE INDEX parent_id_index ON tb_fav(parent_id ASC);");	//创建索引
			if (!pDB->tableExists("tb_fav"))
				return false;

		}
	}
	
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
//public

int CFavDBManager::ChangeDBPath(LPCTSTR lpszDBPath)
{
	// 默认认为数据库是错了，这样，至少可以从网上down一份下来。
	int nRes = FAV_DB_SQLITE_ERROR;

    bool bNotify = false;

	if (lpszDBPath)
	{
		CString strChangeDBPath;
		strChangeDBPath.Format(_T("%s\\%s"), lpszDBPath, FAV_DB_NAME);
		if (strChangeDBPath !=  m_strDBPath)
		{
			bNotify = true;
            m_strDBPath = strChangeDBPath;
			UninitDB();
			CheckDBFileAttribute(m_strDBPath);
			InitDB();
			
			// 如果打开失败，说明数据库文件损坏，尝试恢复数据库
			if(!m_bDBOpen)
			{
// 				if(MoveErrorDBFile(m_strDBPath))
// 				{
// 					if(RestoreDBAuto())
// 					{
// 						nRes = FAV_DB_AUTO_RESTORE_SUCCEEDED;
// 						XLOGINFO(_T("Favorites database file is corrupted, but successed to automatically reply the database!"));
// 					}
// 					else
// 					{
// 						nRes = FAV_DB_AUTO_RESTORE_FAILD;
// 						XLOGINFO(_T("Favorites database file is corrupted, and failed to automatically reply the database!"));
// 					}
// 				}
// 				else
// 				{
// 					// 文件被占用。
// 					nRes = FAV_DB_SQLITE_ERROR;
// 					XLOGINFO(_T("CFavDBManager::MoveErrorDBFile Favorites file is access denied."));
// 				}
			}
			else
				nRes = FAV_DB_SUCCEEDED;
		}
		else
			nRes = FAV_DB_SUCCEEDED;
	}
	else
		nRes = FAV_DB_PARAM_ERROR;

/*
	if (bNotify)
        m_pFavDBThreadWnd->NotifyFavoriteControlReLoadDB();*/

	return nRes;
}

// int CFavDBManager::ExistItem(int nParentID, bool bFolder, LPCTSTR lpszTitle)
// {
// 	CHECK_DB_RETURN(FAV_DB_SQLITE_ERROR);
// 
// 	if (!lpszTitle)
//		return FAV_DB_PARAM_ERROR;
// 
// 	CppSQLite3Buffer sql;
// 	CppSQLite3Query oQuery;
// 
// 	std::string szutf8Tile = GetUTF8String(lpszTitle);
// 
// 	sql.format("SELECT [id] FROM tb_fav WHERE parent_id=%d AND is_folder=%d and title='%s'", nParentID, bFolder ? 1 : 0, szutf8Tile.c_str());
// 	if (m_pDB->execQuery(oQuery, sql))
// 	{
// 		if (oQuery.eof())
// 			return FAV_DB_ITEM_NOT_EXISTED;
// 		else
// 			return oQuery.getIntField(0);
// 	}
// 	return FAV_DB_SQLITE_ERROR;
// }
	
// bool CFavDBManager::IsExistID(int nId)
// {
// 	if (!m_pDB || !m_bDBOpen)
// 		return false;
// 
// 	CppSQLite3Buffer sql;
// 	CppSQLite3Query oQuery;
// 	sql.format("select id from tb_fav where id=%d", nId);
// 	if (!m_pDB->execQuery(oQuery, sql) || oQuery.eof())
// 		return false;
// 
// 	return true;
// }
// 
// int CFavDBManager::AddFolder(int nParentID, LPCTSTR lpszTitle, int nPos/* = -1*/)
// {
// 	return AddItem(nParentID, true, lpszTitle, NULL, nPos);
// }
// 
// int CFavDBManager::AddItem(int nParentID, LPCTSTR lpszTitle, LPCTSTR lpszURL, int nPos/* = -1*/, bool bIsBest/* = false*/)
// {
// 	return AddItem(nParentID, false, lpszTitle, lpszURL, nPos, bIsBest);
// }
// 
// int CFavDBManager::GetAllItems(int nParentID, vector<FavItem> & vecItems)
// {
// 	CHECK_DB_RETURN(FAV_DB_SQLITE_ERROR);
// 
// 	CppSQLite3Buffer sql;
// 	CppSQLite3Query oQuery;
// 
// 	sql.format("SELECT id, is_folder, title, url, create_time, is_best, pos FROM tb_fav WHERE parent_id=%d order by pos", nParentID);
// 	if (!m_pDB->execQuery(oQuery, sql))
// 		return FAV_DB_SQLITE_ERROR;
// 
// 	while (!oQuery.eof())
// 	{
// 		FavItem item;
// 		item.nID = oQuery.getIntField(0);
// 		item.nParentID = nParentID;
// 		item.bFolder = oQuery.getIntField(1) ? true : false;
// 
// 		// 转化标题(utf8)为unicode
// 		item.strTitle = UTF8Decode(oQuery.getStringField(2));
// 		if(!item.bFolder)
// 			item.strURL = UTF8Decode(oQuery.getStringField(3));
// 
// 		item.lCreateTime = _atoi64(oQuery.fieldValue(4));
// 		item.bBest = oQuery.getIntField(5) ? true : false;
// 		item.nPos = oQuery.getIntField(6);
// 		vecItems.push_back(item);
// 
// 		oQuery.nextRow();
// 	}
// 
// 	return FAV_DB_SUCCEEDED;
// }
// 
// int CFavDBManager::GetItem(int nID, FavItem * pItem)
// {
// 	CHECK_DB_RETURN(FAV_DB_SQLITE_ERROR);
// 
// 	if (!pItem)
// 		return FAV_DB_PARAM_ERROR;
// 
// 	CppSQLite3Buffer sql;
// 	CppSQLite3Query oQuery;
// 
// 	sql.format("SELECT is_folder, title, url, create_time, is_best, pos, parent_id FROM tb_fav WHERE id=%d", nID);
// 
// 	if (!m_pDB->execQuery(oQuery, sql))
// 		return FAV_DB_SQLITE_ERROR;
// 
// 	// 必须有一条记录
// 	if (oQuery.eof())
// 		return FAV_DB_SQLITE_ERROR;
// 
// 	pItem->nID = nID;
// 	pItem->bFolder = oQuery.getIntField(0) ? true : false;
// 
// 	pItem->strTitle = UTF8Decode(oQuery.getStringField(1));
// 	if(!pItem->bFolder)
// 		pItem->strURL = UTF8Decode(oQuery.getStringField(2));
// 
// 	pItem->lCreateTime = _atoi64(oQuery.fieldValue(3));
// 	pItem->bBest = oQuery.getIntField(4) ? true : false;
// 	pItem->nPos = oQuery.getIntField(5);
// 	pItem->nParentID = oQuery.getIntField(6);
// 
// 	return FAV_DB_SUCCEEDED;
// }
// 
// int CFavDBManager::SetItemBest(int nId,  bool bBest)
// {
// 	CHECK_DB_RETURN(FAV_DB_SQLITE_ERROR);
// 
// 	CppSQLite3Buffer sql;
// 	sql.format("update tb_fav set is_best=%d, last_modify_time=%lld WHERE id=%d", bBest ? 1 : 0, Util::Base::GetCurrentFileTime(), nId);
// 
// 	if(m_pDB->execDML(sql) <=0 )
// 	{
// 		XLOGINFO(_T("CFavDBManager::SetItemBest update faild."));
// 		return FAV_DB_SQLITE_ERROR;
// 	}
// 
// 	//m_pFavDBThreadWnd->NotifyFavoriteControlBestChangeItem(nId);
// 	m_bDBChanged = TRUE;
// 	return FAV_DB_SUCCEEDED;
// }
// 
// int CFavDBManager::DeleteItem(int nId, bool bFolder)
// {
// 	CHECK_DB_RETURN(FAV_DB_SQLITE_ERROR);
// 
// 	CppSQLite3Buffer sql;
// 	CppSQLite3Query oQuery;
// 
// 	sql.format("select pos, parent_id from tb_fav where id=%d", nId);
// 	if (!m_pDB->execQuery(oQuery, sql) || oQuery.eof())
// 		return FAV_DB_SQLITE_ERROR;
// 
// 	int nDelePos = oQuery.getIntField(0);
// 	int nParentId = oQuery.getIntField(1);
// 	oQuery.finalize();
// 
// 	BeginTransaction();
// 	int nRes = FAV_DB_SQLITE_ERROR;
// 	if (bFolder)
// 		nRes = DeleteFolderNotify(nId, nParentId, nDelePos, TRUE);
// 	else
// 		nRes = DeleteItemNotify(nId, nParentId, nDelePos, TRUE);
// 
// 	if ( nRes == FAV_DB_SUCCEEDED)
// 	{
// 		CommitTransaction();
// 		return FAV_DB_SUCCEEDED;
// 	}
// 	RollbackTransaction();
// 	return FAV_DB_SQLITE_ERROR;
// }
// 
// int CFavDBManager::BatchDelete(std::vector<int>* batchItems)
// {
// 	CHECK_DB_RETURN(FAV_DB_SQLITE_ERROR);
// 	if(!batchItems || batchItems->size() == 0)
// 		return FAV_DB_PARAM_ERROR;
// 
// 	CppSQLite3Buffer sql;
// 	CppSQLite3Query oQuery;
// 
// 	BeginTransaction();
// 	int nRes = FAV_DB_SUCCEEDED;
// 	int nDelePos = 0;
// 	int nParentId = 0;
// 	int nIsFolder = 0;
// 	std::vector<int>::iterator it = batchItems->begin();
// 	while(it != batchItems->end() && FAV_DB_SUCCEEDED == nRes)
// 	{
// 		int nId = *it;
// 		sql.format("select pos, parent_id, is_folder from tb_fav where id=%d", nId);
// 		if (!m_pDB->execQuery(oQuery, sql) || oQuery.eof())
// 			return FAV_DB_SQLITE_ERROR;
// 
// 		nDelePos = oQuery.getIntField(0);
// 		nParentId = oQuery.getIntField(1);
// 		nIsFolder = oQuery.getIntField(2);
// 		oQuery.finalize();
// 
// 		if (nIsFolder == 1)
// 			nRes = DeleteFolderNotify(nId, nParentId, nDelePos, TRUE);
// 		else
// 			nRes = DeleteItemNotify(nId, nParentId, nDelePos, TRUE);
// 
// 		it++;
// 	}
// 	if ( nRes == FAV_DB_SUCCEEDED)
// 	{
// 		CommitTransaction();
// 		//m_pFavDBThreadWnd->NotifyFavoriteControlUpdateFolder(nParentId);
// 		return FAV_DB_SUCCEEDED;
// 	}
// 	RollbackTransaction();
// 	return FAV_DB_SQLITE_ERROR;
// }
// 
// 
// int CFavDBManager::ModifyItem(int nId, LPCTSTR lpszTitle, LPCTSTR lpszUrl)
// {
// 	CHECK_DB_RETURN(FAV_DB_SQLITE_ERROR);
// 
// 	//	if (!lpszTitle || !lpszTitle[0])
// 	//		return FAV_DB_PARAM_ERROR;
// 
// 	CppSQLite3Buffer sql;
// 
// 	std::string szutf8Tile = GetUTF8String(lpszTitle);
// 	std::string szutf8Url = GetUTF8String(lpszUrl);
// 
// 	if(szutf8Tile == "" && szutf8Url == "")
// 		return FAV_DB_PARAM_ERROR;
// 	else if(szutf8Tile != "" && szutf8Url == "")
// 		sql.format("update tb_fav set title='%s', last_modify_time=%lld where id=%d", szutf8Tile.c_str(), Util::Base::GetCurrentFileTime(), nId);
// 	else if(szutf8Tile == "" && szutf8Url != "")
// 		sql.format("update tb_fav set url='%s', last_modify_time=%lld where id=%d", szutf8Url.c_str(), Util::Base::GetCurrentFileTime(), nId);
// 	else
// 		sql.format("update tb_fav set title='%s', url='%s', last_modify_time=%lld where id=%d", szutf8Tile.c_str(), szutf8Url.c_str(), Util::Base::GetCurrentFileTime(), nId);
// 
// 	if(m_pDB->execDML(sql) <= 0 )
// 	{
// 		XLOGINFO(_T("CFavDBManager::ModifyItem update faild."));
// 		return FAV_DB_SQLITE_ERROR;
// 	}
// 
// 	//m_pFavDBThreadWnd->NotifyFavoriteControlRenameItem(nId);
// 	m_bDBChanged = TRUE;
// 	return FAV_DB_SUCCEEDED;
// }
// 
// 
// int CFavDBManager::MoveItem(int nSrcId, int nTargetId, int nMoveStyle, bool bBatch)
// {
// 	CHECK_DB_RETURN(FAV_DB_SQLITE_ERROR);
// 
// 	bool bCollision = nMoveStyle & MOVEITEM_COLLISION;
// 	MoveOperStruct mo = {0};
// 
// 	CppSQLite3Buffer sql;
// 	CppSQLite3Query oQuery;
// 	sql.format("select pos, parent_id, is_folder from tb_fav where id=%d", nSrcId);
// 	if (!m_pDB->execQuery(oQuery, sql) || oQuery.eof())
// 		return FAV_DB_SQLITE_ERROR;
// 
// 	mo.nSrcID = nSrcId;
// 	mo.nSrcPos = oQuery.getIntField(0);
// 	mo.nSrcParentID = oQuery.getIntField(1);
// 	mo.bSrcFolder = oQuery.getIntField(2) > 0 ? TRUE : FALSE;
// 	oQuery.finalize();
// 
// 	// 如果nSrcId是以个目录，需要判断目标nTargetId是否为nSrcId是子目录
// 	if(mo.bSrcFolder && IsChild(nSrcId, nTargetId))
// 		return FAV_DB_PARAM_ERROR;
// 
// 	if(nTargetId > 0)
// 	{
// 		sql.format("select pos, parent_id, is_folder from tb_fav where id=%d", nTargetId);
// 		if (!m_pDB->execQuery(oQuery, sql) || oQuery.eof())
// 			return FAV_DB_SQLITE_ERROR;
// 
// 		mo.nDestID = nTargetId;
// 		mo.nDestPos = oQuery.getIntField(0);
// 		mo.nDestParentID = oQuery.getIntField(1);
// 		mo.bDestFolder = oQuery.getIntField(2) > 0 ? TRUE : FALSE;
// 		oQuery.finalize();
// 	}
// 	else
// 	{
// 		mo.nDestID = 0;
// 		mo.nDestPos = 0;
// 		mo.nDestParentID = 0;
// 		mo.bDestFolder = 0;
// 	}
// 
// 	if( nMoveStyle & MOVEITEM_DOWN)
// 		mo.nDestPos ++;
// 	// 移进去时，目标目录就是nTargetId（此时nTargetId为目录）
// 	if(nMoveStyle & MOVEITEM_IN)
// 		mo.nDestParentID = nTargetId;
// 
// 	int nRes = FAV_DB_SQLITE_ERROR;
// 	if (!bBatch)
// 		BeginTransaction();
// 	if(!bCollision)
// 	{
// 		if(nMoveStyle & MOVEITEM_IN)
// 		{
// 			sql.format("select max(pos) from tb_fav where parent_id=%d", mo.nDestID);
// 			if (m_pDB->execQuery(oQuery, sql))
// 			{
// 				mo.nDestPos = oQuery.getIntField(0) + 1;
// 				oQuery.finalize();
// 				nRes = MoveItem2Folder(&mo, TRUE, FALSE);
// 			}
// 		}
// 		else
// 			nRes = MoveItem2Folder(&mo, TRUE, TRUE);
// 	}
// 	else
// 	{
// 		// 获得冲突的id
// 		sql.format("select dest_tb_fav.id, dest_tb_fav.pos "
// 			"from (select id, pos, is_folder, title from tb_fav where parent_id=%d) as dest_tb_fav, tb_fav "
// 			"where (tb_fav.id=%d) and (tb_fav.is_folder=dest_tb_fav.is_folder) and (tb_fav.title=dest_tb_fav.title)"
// 			, mo.nDestParentID, mo.nSrcID);
// 
// 		if (!m_pDB->execQuery(oQuery, sql) || oQuery.eof())
// 			return FAV_DB_SQLITE_ERROR;
// 
// 		mo.nCollisionID = oQuery.getIntField(0);
// 		// 冲突项和目标向一定在同一个目录
// 		mo.nCollisionParentID = mo.nDestParentID;
// 		mo.nCollisionPos = oQuery.getIntField(1);
// 		oQuery.finalize();
// 
// 		if(mo.bSrcFolder)
// 		{
// 			nRes = MergeFolder2Folder(&mo, TRUE);
// 			//m_pFavDBThreadWnd->NotifyFavoriteControlUpdateFolder(mo.nCollisionID);
// 		}
// 		else
// 			nRes = MergeItem2Folder(&mo, TRUE);
// 	}
// 	if(FAV_DB_SUCCEEDED != nRes && !bBatch)
// 		RollbackTransaction();
// 	else
// 	{
// 		//m_pFavDBThreadWnd->NotifyFavoriteControlMoveItem(mo.nSrcID, mo.nSrcParentID, mo.nDestParentID);
// 		if (!bBatch)
// 			CommitTransaction();
// 	}
// 
// 	m_bDBChanged = m_bDBChanged || (nRes >= 0);
// 	// 移动item，需要合并
// 	return nRes;
// }
// 
// int CFavDBManager::CheckItemCollision(int nId, int nTargetId)
// {
// 	CHECK_DB_RETURN(FAV_DB_SQLITE_ERROR);
// 
// 	CppSQLite3Query oQuery;
// 	CppSQLite3Buffer sql;
// 	// SQL语句的含义：
// 	// 1. 查询nTargetId文件夹下所有没有删除的项，作为临时表dest_tb_fav
// 	// 2. 联合查询dest_tb_fav和tb_fav，链接条件是  is_folder相等， title相等
// 	// 3. tb_fav表的限制：id=nId， parent_id!=nTargetId
// 	sql.format("select tb_fav.id "
// 		"from (select is_folder, title from tb_fav where parent_id=%d) as dest_tb_fav, tb_fav "
// 		"where  (tb_fav.is_folder=dest_tb_fav.is_folder) and (tb_fav.title=dest_tb_fav.title) and  "
// 		"(tb_fav.id=%d) and (tb_fav.parent_id<>%d)"
// 		, nTargetId, nId, nTargetId);
// 	if (!m_pDB->execQuery(oQuery, sql))
// 		return FAV_DB_SQLITE_ERROR;
// 	int nRes = oQuery.eof() ? 0 : 1;
// 	oQuery.finalize();
// 	return nRes;
// }
// 
// bool CFavDBManager::IsChild(int nSrcId, int nDestId)
// {
// 	if (!m_pDB || !m_bDBOpen || nSrcId == nDestId)
// 		return false;
// 
// 	int nDestParentId = nDestId;
// 	CppSQLite3Query oQuery;
// 	CppSQLite3Buffer sql;
// 
// 	// 遍历出nDestId所有的祖先，判断是否有与nSrcId相等的
// 	while(nDestParentId)
// 	{
// 		sql.format("select parent_id from tb_fav where id=%d", nDestParentId);
// 		if (!m_pDB->execQuery(oQuery, sql) || oQuery.eof())
// 			return false;
// 		nDestParentId = oQuery.getIntField(0);
// 		oQuery.finalize();
// 		if(nDestParentId == nSrcId)
// 			return true;
// 	}
// 	return false;
// }
// 
// int CFavDBManager::SortFolderByTitle(int nParentId)
// {
// 	// 按title排序，不能用sql语句来排序，需要重数据库中读出来，再用字符串排序
// 
// 	// 修改中文排序错误的BUG hangcheng review by wanyong
// 	// BUG原因：在sql语句中用order by 排序，数据库处理的不对
// 	// 修改方法：把需要排序的目录下的项读出来后，title转成ANCI格式（必须）,在用字符串比较来排序，再将结果写入数据库
// 	CHECK_DB_RETURN(FAV_DB_SQLITE_ERROR);
// 
// 	CppSQLite3Query oQuery;
// 	CppSQLite3Buffer sql;
// 	sql.format("select id, is_folder, title from tb_fav where parent_id=%d;", nParentId);
// 	if (!m_pDB->execQuery(oQuery, sql))
// 		return FAV_DB_SQLITE_ERROR;
// 
// 	VecDBTitleStor items;
// 	int nRes = FAV_DB_SUCCEEDED;
// 	while (!oQuery.eof())
// 	{
// 		DBTitleStor item;
// 		item.nId= oQuery.getIntField(0);
// 		item.bFolder = oQuery.getIntField(1) == 0 ? false : true;
// 		item.strTile = UTF8Decode(oQuery.getStringField(2));
// 		items.push_back(item);
// 		oQuery.nextRow();
// 	}
// 	oQuery.finalize();
// 	if(items.size() == 0)
// 		return FAV_DB_SUCCEEDED;
// 
// 	std::sort(items.begin(), items.end());
// 
// 	BeginTransaction(TRUE);
// 	VecDBTitleStor::iterator it;
// 	int nPos = 1;
// 	// 移动位置，需要更新时间，不然在合并时会有问题
// 	__int64 tNow = Util::Base::GetCurrentFileTime();
// 	for(it = items.begin(); it != items.end(); it ++, nPos ++)
// 	{
// 		DBTitleStor& itm = *it;
// 		CppSQLite3Buffer sql;
// 		sql.format("update tb_fav set pos=%d, last_modify_time=%lld where id=%d", nPos, tNow, itm.nId);
// 		if(m_pDB->execDML(sql) <=0)
// 		{
// 			XLOGINFO(_T("CFavDBManager::SortFolderByTitle update faild."));
// 			RollbackTransaction();
// 			nRes = FAV_DB_SQLITE_ERROR;
// 			break;
// 		}
// 	}
// 	items.clear();
// 	CommitTransaction();
// 	if(FAV_DB_SUCCEEDED == nRes)
// 	{
// 		m_bDBChanged = TRUE;
// 		//m_pFavDBThreadWnd->NotifyFavoriteControlUpdateFolder(nParentId);
// 	}
// 	return nRes;
// }
// 
// int CFavDBManager::SortFolderByCreateTime(int nParentId)
// {
// 	return SortFolderByField(nParentId, "create_time");
// }

bool CFavDBManager::BeginTransaction(bool bExistCommit/* = FALSE*/)
{
	CHECK_DB_RETURN(FALSE);
	
	if(m_bOpenTransaction)
	{
		//如果事务已经开启过，需要提交之前的事务
		if(bExistCommit)
			CommitTransaction();
		else
			return TRUE;
	}
	m_bOpenTransaction = m_pDB->execDML("begin transaction") < 0 ? FALSE : TRUE;
	return m_bOpenTransaction;
}
bool CFavDBManager::RollbackTransaction()
{
	CHECK_DB_RETURN(FALSE);
	
	if(!m_bOpenTransaction)
		return FALSE;
	
	m_bOpenTransaction = m_pDB->execDML("rollback transaction") < 0 ? TRUE : FALSE;
	
	return !m_bOpenTransaction;
}

bool CFavDBManager::CommitTransaction()
{
	CHECK_DB_RETURN(FALSE);
	
	if(!m_bOpenTransaction)
		return FALSE;
	
	m_bOpenTransaction = m_pDB->execDML("commit transaction") < 0 ? TRUE : FALSE;
	return !m_bOpenTransaction;
}
void CFavDBManager::EndTransaction()
{
	CHECK_DB;
	
	if(!m_bOpenTransaction)
		return;
	
	m_pDB->execDML("end transaction");
}

// bool CFavDBManager::BackupDB(LPCTSTR lpszDBFilePath)
// {
// 	if (m_bDBOpen && lpszDBFilePath && m_strDBPath.GetLength() > 0)
// 	{
// 		CString strDBFile;
// 		strDBFile.Format(_T("%s"), lpszDBFilePath);
// 
// 		// 如果有事务没有提交，则提交事务
// 		CommitTransaction();
// 
// 		if(!CopyFile(m_strDBPath, strDBFile, FALSE))
// 			XLOGINFO(_T("CFavDBManager::BackupDB CopyFile %s to %s faild."), m_strDBPath, strDBFile);
// 		else
// 			return true;
// 	}
// 	return false;
// }
// 
// bool CFavDBManager::RestoreDB(LPCTSTR lpszDBFilePath)
// {
// 	bool bRes = false;
// 	if (lpszDBFilePath && PathFileExists(lpszDBFilePath))
// 	{
// 		CppSQLite3DB db;
// 
// 		// 校验还原的数据库
// 		std::string strDBFilePath = UTF8Encode(lpszDBFilePath);
// 		if(db.open(strDBFilePath.c_str()))
// 		{
// 			if(CheckDBIntegrity(&db) && CheckDBVersion(&db, false))
// 				bRes = true;
// 			db.close();
// 		}
// 		// else 源文件被占用
// 	}
// 
// 	if(bRes)
// 	{
// 		UninitDB();
// 		if(!CopyFile(lpszDBFilePath, m_strDBPath, FALSE))
// 			XLOGINFO(_T("CFavDBManager::RestoreDB CopyFile %s to %s faild."), lpszDBFilePath, m_strDBPath);
// 		InitDB();
// 		bRes = (m_bDBOpen == TRUE);
// 
// 	}
// 	return bRes;
// }
// 
	
// int CFavDBManager::GetFavoriteTotal()
// {
// 	CHECK_DB_RETURN(FAV_DB_SQLITE_ERROR);
// 
// 	CppSQLite3Buffer sql;
// 	CppSQLite3Query oQuery;
// 
// 	sql.format("Select count(*) From tb_fav where is_folder=0");
// 	if (!m_pDB->execQuery(oQuery, sql) || oQuery.eof())
// 		return FAV_DB_SQLITE_ERROR;
// 
// 	return oQuery.getIntField(0);
// }
// 
// int CFavDBManager::GetLocalDBInfo(DBInfo* pInfo)
// {
// 	return GetDBInfo(m_pDB, pInfo);
// }
// 
// int CFavDBManager::GetServerDBInfo(LPCTSTR lpszDBFilePath, DBInfo* pInfo)
// {
// 	int nRes = FAV_DB_PARAM_ERROR;
// 	if(lpszDBFilePath && lpszDBFilePath[0])
// 	{
// 		CppSQLite3DB db;
// 
// 		// 路径中可能含有中文，需要进行utf8的编码
// 		std::string strDBFilePath = UTF8Encode(lpszDBFilePath); 
// 		if (db.open(strDBFilePath.c_str()))
// 		{
// 			nRes = GetDBInfo(&db, pInfo);
// 			db.close();
// 		}
// 		else
// 			XLOGINFOA("CFavDBManager::GetServerDBInfo open db failed: %s", strDBFilePath.c_str());
// 	}
// 	return nRes;
// }
// 
// bool CFavDBManager::OverwriteDB(LPCTSTR lpszDBFilePath)
// {
// 	bool bRes = false;
// 	if(lpszDBFilePath && lpszDBFilePath[0] && PathFileExists(lpszDBFilePath))
// 	{
// 		UninitDB();
// 		if (!CopyFile(lpszDBFilePath, m_strDBPath, FALSE))
// 			XLOGINFO(_T("CFavDBManager::OverwriteDB copy file failed."));
// 		InitDB();
// 		if (m_bDBOpen)
// 			m_bDBChanged = true;
// 		//m_pFavDBThreadWnd->NotifyFavoriteControlReLoadDB();
// 	}
// 	return (m_bDBOpen == TRUE);
// }
// 
// 
// bool CFavDBManager::GetDBPath(LPTSTR lpszDBPath, DWORD dwSize)
// {
// 	if(lpszDBPath)
// 	{
// 		_sntprintf(lpszDBPath, dwSize, _T("%s"), m_strDBPath);
// 		return true;
// 	}
// 	return false;
// }
// 
// void CFavDBManager::SetDBLastSynTime()
// {
// 	if(m_pDB && m_bDBOpen)
// 	{
// 		CppSQLite3Buffer sql;
// 		sql.format("update tb_misc set value='%lld' where key='%s'",Util::Base::GetCurrentFileTime(), FAVDB_SYN_TIME_KEY);
// 		m_pDB->execDML(sql);
// 	}
// }
// 
// bool CFavDBManager::GetAllFolders(vector<FavItem> & vecItems)
// {
// 	CHECK_DB_RETURN(false);
// 	CppSQLite3Query oQuery;
// 
// 	if (!m_pDB->execQuery(oQuery, "SELECT id, parent_id, title, pos FROM tb_fav WHERE is_folder=1 order by pos, parent_id"))
// 		return false;
// 
// 	while (!oQuery.eof())
// 	{
// 		FavItem item;
// 		item.nID = oQuery.getIntField(0);
// 		item.nParentID = oQuery.getIntField(1);	
// 		// 转化标题(utf8)为unicode
// 		item.strTitle = UTF8Decode(oQuery.getStringField(2));
// 		item.nPos = oQuery.getIntField(3);
// 		vecItems.push_back(item);
// 		oQuery.nextRow();
// 	}
// 	return true;
// }
// 
// bool CFavDBManager::GetPathFromId(TCHAR* szPath, int nSize, int nId)
// {
// 	CHECK_DB_RETURN(false);
// 	if(!szPath)
// 		return false;
// 	CString strParent;
// 	int nDestParentId = nId;
// 
// 	CppSQLite3Query oQuery;
// 	CppSQLite3Buffer sql;
// 
// 	while(nDestParentId)
// 	{
// 		sql.format("select parent_id, title from tb_fav where id=%d", nDestParentId);
// 		if (!m_pDB->execQuery(oQuery, sql) || oQuery.eof())
// 			return false;
// 		std::wstring wstrParnet = UTF8Decode(oQuery.getStringField(1));
// 		if(nDestParentId != nId)
// 			strParent = _T("\\") + strParent;
// 		strParent = wstrParnet.c_str() + strParent;
// 		nDestParentId = oQuery.getIntField(0);
// 		oQuery.finalize();
// 	}
// 	_sntprintf(szPath, nSize, _T("%s"), strParent);
// 	return true;
// }
// 
// int CFavDBManager::GetFavShowFolderId()
// {
// 	CHECK_DB_RETURN(0);
// 
// 	CppSQLite3Query oQuery;
// 	CppSQLite3Buffer sql;
// 	int nFolderId = 0;
// 
// 	sql.format("select value from tb_misc where key='show_folder_id'");
// 	if (!m_pDB->execQuery(oQuery, sql) || oQuery.eof())
// 		return nFolderId;
// 
// 	nFolderId = oQuery.getIntField(0);
// 	oQuery.finalize();
// 
// 	sql.format("select is_folder from tb_fav where id=%d", nFolderId);
// 	if (!m_pDB->execQuery(oQuery, sql) || oQuery.eof())
// 		return nFolderId;
// 
// 	if(oQuery.getIntField(0) == 0)
// 		nFolderId = 0;
// 	oQuery.finalize();
// 
// 
// 	return nFolderId;
// }
// 
// bool CFavDBManager::SetFavShowFolderId(int nFolderId)
// {
// 	CHECK_DB_RETURN(false);
// 
// 	CppSQLite3Query oQuery;
// 	CppSQLite3Buffer sql;
// 
// 	bool bHave = false;
// 	sql.format("select value from tb_misc where key='show_folder_id'");
// 	if (m_pDB->execQuery(oQuery, sql) && !oQuery.eof())
// 		bHave = true;
// 	oQuery.finalize();
// 
// 	if(bHave)
// 		sql.format("update tb_misc set value='%d' where key='show_folder_id';", nFolderId);
// 	else 
// 		sql.format("insert into tb_misc(key,value) values('show_folder_id','%d');", nFolderId);
// 	if (m_pDB->execDML(sql) < 0)
// 	{
// 		XLOGINFO(_T("CFavDBManager::SetFavShowFolderId  faild."));
// 		return false;
// 	}
// 
// 	m_bDBChanged = true;
// 	//m_pFavDBThreadWnd->NotifyFavoriteShowFolderChange();
// 	return true;
// }
// 
// int CFavDBManager::SearchItem(vector<FavItem>& vecItems, LPCTSTR lpszKeyWords)
// {
// 	CHECK_DB_RETURN(FAV_DB_SQLITE_ERROR);
// 
// 	if(!lpszKeyWords || !lpszKeyWords[0])
// 		return FAV_DB_PARAM_ERROR;
// 
// 	std::string szutf8Kwywords = GetUTF8String(lpszKeyWords);
// 
// 	char szWords[MAX_PATH] = {0};
// 	_snprintf(szWords, SIZEOF(szWords) - 1, "%s", szutf8Kwywords.c_str());
// 	char* pItem = strtok(szWords, " ");
// 	std::string strWhere;
// 	while(pItem)
// 	{
// 		char szItem[MAX_PATH] = {0};
// 		_snprintf(szItem, SIZEOF(szItem), "title like '%%%s%%' or url like '%%%s%%'", pItem, pItem);
// 		if(strWhere.size() != 0)
// 			strWhere += " or ";
// 		strWhere += szItem;
// 		pItem = strtok(NULL, " ");
// 	}
// 
// 	//	std::string szutf8Where = UTF8Encode(strWhere);
// 
// 	if(strWhere.size() == 0)
// 		return FAV_DB_PARAM_ERROR;
// 
// 
// 	CppSQLite3Query oQuery;
// 	CppSQLite3Buffer sql;
// 
// 	sql.format("select id, parent_id, title, url FROM tb_fav WHERE is_folder=0 AND (%s);", strWhere.c_str());
// 	if (!m_pDB->execQuery(oQuery, sql))
// 		return FAV_DB_SQLITE_ERROR;
// 
// 	while (!oQuery.eof())
// 	{
// 		FavItem item;
// 		memset(&item, 0, sizeof(FavItem));
// 		item.nID = oQuery.getIntField(0);
// 		item.nParentID = oQuery.getIntField(1);
// 
// 		// 转化标题(utf8)为unicode
// 		item.strTitle = UTF8Decode(oQuery.getStringField(2));
// 		item.strURL = UTF8Decode(oQuery.getStringField(3));
// 
// 		vecItems.push_back(item);		
// 		oQuery.nextRow();
// 	}
// 
// 	oQuery.finalize();
// 	return FAV_DB_SUCCEEDED;
// }
// 
// int CFavDBManager::GetFolderIdFromPath(LPCTSTR lpszFolderPath)
// {
// 	if(!lpszFolderPath || !lpszFolderPath[0])
// 		return 0;
// 
// 	TCHAR szFolderPath[MAX_PATH] = {0};
// 	_sntprintf(szFolderPath, SIZEOF(szFolderPath) - 1, _T("%s"), lpszFolderPath);
// 
// 	int nParnetId = 0;
// 	TCHAR* pParentTitle = NULL;
// 
// 	CppSQLite3Query oQuery;
// 	CppSQLite3Buffer sql;
// 	pParentTitle = _tcstok(szFolderPath, _T("\\"));
// 	while(pParentTitle && pParentTitle[0])
// 	{
// 		std::string szutf8Tile = GetUTF8String(pParentTitle);
// 		sql.format("select id from tb_fav where title='%s' and parent_id=%d", szutf8Tile.c_str(), nParnetId);
// 		if (!m_pDB->execQuery(oQuery, sql) || oQuery.eof())
// 			return 0;
// 
// 		nParnetId = oQuery.getIntField(0);
// 		oQuery.finalize();
// 
// 		pParentTitle = _tcstok(NULL, _T("\\"));
// 	}
// 
// 	return nParnetId;
// }
// 
// bool CFavDBManager::ReNewDB()
// {
// 	UninitDB();
// 	if(!DeleteFile(m_strDBPath))
// 		XLOGINFO(_T("CFavDBManager::ReNewDB  DeleteFile %s faild."), m_strDBPath);
// 	InitDB();
// 	return m_bDBOpen;
// }
// 
// // 获取/设置数据库的自动导入的状态
// int CFavDBManager::GetAutoLoad(int& nAutoLoad)
// {
// 	CHECK_DB_RETURN(FAV_DB_SQLITE_ERROR);
// 	CppSQLite3Query oQuery;
// 
// 	if (!m_pDB->execQuery(oQuery, "select value from tb_misc where key='db_auto_load'"))
// 		return FAV_DB_SQLITE_ERROR;
// 
// 	// 如果不存在，返回默认值0
// 	if(oQuery.eof())
// 		return 0;
// 
// 	nAutoLoad = oQuery.getIntField(0);
// 	oQuery.finalize();
// 
// 	return FAV_DB_SUCCEEDED;
// }
// 
// int CFavDBManager::SetAutoLoad(int nAutoLoad)
// {
// 	CHECK_DB_RETURN(FAV_DB_SQLITE_ERROR);
// 
// 	CppSQLite3Query oQuery;
// 	CppSQLite3Buffer sql;
// 
// 	// 判断字段是否存在
// 	bool bHave = false;
// 	sql.format("select value from tb_misc where key='db_auto_load'");
// 	if (m_pDB->execQuery(oQuery, sql) && !oQuery.eof())
// 		bHave = true;
// 	oQuery.finalize();
// 
// 	if(bHave)
// 		sql.format("update tb_misc set value='%d' where key='db_auto_load';", nAutoLoad);
// 	else 
// 		sql.format("insert into tb_misc(key,value) values('db_auto_load','%d');", nAutoLoad);
// 	if (m_pDB->execDML(sql) < 0)
// 	{
// 		XLOGINFO(_T("CFavDBManager::SetAutoLoad  faild."));
// 		return FAV_DB_SQLITE_ERROR;
// 	}
// 	return FAV_DB_SUCCEEDED;
// }

////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
//protected

bool CFavDBManager::InitDB()
{
	if(m_bDBOpen)
		return TRUE;
	
	if (m_pDB == NULL)
		m_pDB = new CppSQLite3DB();
	
	// 如果是本地用户，数据库路径应该在 %appdata%\360se\data\360sefav.db
	// 如果是网络用户，数据库路径应该在 %appdata%\360se\b46c611628f96d50417b70ec6798ab27\360sefav.db，
	// b46c611628f96d50417b70ec6798ab27目录中的data目录是拷贝进去备份用的。
	
	// 考虑异常情况，数据库被损坏，即checkdb不通过，或者表被删除等
	// todo: ......
    m_nDBStatus = DB_STATUS_NEW;
	if(PathFileExists(m_strDBPath))
		m_nDBStatus = DB_STATUS_CACHE;
	
	std::string strDBFilePath = UTF8Encode(m_strDBPath); 
	
	m_bDBOpen = FALSE;
	if (m_pDB->open(strDBFilePath.c_str()))
	{
        if (CFavDBManager::CheckDBIntegrity(m_pDB))
        {
            if (CFavDBManager::IsTablesExist(m_pDB, true) && CFavDBManager::CheckDBVersion(m_pDB, true))
            {
                m_bDBOpen = TRUE;
            }
        }
        else
        {
            XLOGINFO(_T("CFavDBManager::InitDB check db intergrity failed."));
            m_nDBStatus = DB_STATUS_NEW;
            
            // 数据库损坏的人数打点
            //CStatistics::GetInstance()->SetCount(COUNT_FAV_DESTROY, 1);
        }
	}

	if(!m_bDBOpen)
    {
        XLOGINFOA("CFavDBManager::InitDB,open:%s", strDBFilePath.c_str());
        m_nDBStatus = DB_STATUS_NEW;
        m_pDB->close();
    }
	

	return (m_bDBOpen == TRUE);
}

void CFavDBManager::UninitDB()
{
	if (m_pDB && m_bDBOpen)
	{
		// 如果有事务没有提交，则提交事务
		// CommitTransaction();
		// 如果有事务，应该回滚，而不应该提交
		RollbackTransaction();
		m_pDB->close();
	}
	m_bDBOpen = FALSE;
}

// bool CFavDBManager::RestoreDBAuto()
// {
// 	std::vector<CString> listFileName;
// 	GetBackupFiles(listFileName);
// 
// 	// 尝试恢复所以的备份文件，知道恢复成功为止
// 	bool bRestoreDBSucc = false;
// 	while(listFileName.size() > 0 && !bRestoreDBSucc)
// 	{
// 		CString &strBackFileName = listFileName.back();
// 		bRestoreDBSucc = RestoreDB(strBackFileName);
// 		if(!bRestoreDBSucc)
// 			MoveErrorDBFile(strBackFileName);
// 
// 		listFileName.pop_back();
// 	}
// 
// 	// 如果恢复成功，则返回使用备份的状态，否则尝试创建一个空白数据库
// 	if(bRestoreDBSucc)
// 		m_nDBStatus = DB_STATUS_BACKUP;
// 	else
// 		InitDB();
// 
// 	return bRestoreDBSucc;
// }

// bool CFavDBManager::DailyBackup()
// {
// 	// 算出需要备份的目标文件名
// 	CString strFileName = GetDailyBackupFilePath();
// 	if (_T("") != strFileName && m_strDBPath.GetLength() > 0)
// 	{
// 		if(!CopyFile(m_strDBPath, strFileName, FALSE))
// 			XLOGINFO(_T("CFavDBManager::DailyBackup  CopyFile %s to %s  faild."), m_strDBPath, strFileName);
// 		DeleteOldBackupFile();
// 		return TRUE;
// 	}
// 	return FALSE;
// }
// 

// 必须放在这个线程操作，因为如果合并的时候，用户又在操作本地数据库，则可能会将这些操作丢失
// 1、将服务器数据合并到本地
// 2、将合并后的数据库覆盖到当前使用的数据库
//
// bool CFavDBManager::MergeDB(LPCTSTR lpszLocalDBPath, LPCTSTR lpszServerDBPath)
// {
// 	CFavDBMergeHelper merge;
// 	if (!merge.Merge(lpszLocalDBPath, lpszServerDBPath))
// 		return false;
// 	bool bReloadSucc = false;
// 
// 	// 需要关闭数据库，然后重新载入，无论拷贝是否成功，都需要重新加载数据库，但返回是否merge完成
// 	UninitDB();
// 	bReloadSucc = ::CopyFile(lpszLocalDBPath, m_strDBPath, FALSE) == TRUE;
// 	InitDB();
// 
//     m_pFavDBThreadWnd->NotifyFavoriteControlReLoadDB();
// 	return bReloadSucc;
// }


//////////////////////////////////////////////////////////////////////////

// int CFavDBManager::AddItem(int nParentID, bool bFolder, LPCTSTR lpszTitle, LPCTSTR lpszURL, int nPos/* = -1*/, bool bIsBest/* = false*/)
// {
// 	CHECK_DB_RETURN(FAV_DB_SQLITE_ERROR);
// 
// 	if (!lpszTitle || !lpszTitle[0])
		//return FAV_DB_PARAM_ERROR;
// 
// 	// 如果是链接，那么 url必须不为空
// 	if (!bFolder && (!lpszURL || !lpszURL[0]))
// 		return FAV_DB_PARAM_ERROR;
// 
// 	// 检查item是否存在
// 	/*	int nRet = ExistItem(nParentID, bFolder, lpszTitle);
// 	if (nRet != FAV_DB_ITEM_NOT_EXISTED)
// 	return nRet;*/
// 
// 	// 插入数据到数据库
// 	CppSQLite3Buffer sql;
// 	CppSQLite3Query oQuery;
// 
// 	std::string szutf8Tile = GetUTF8String(lpszTitle);
// 	std::string szutf8Url;
// 	if (!bFolder)
// 		szutf8Url = GetUTF8String(lpszURL);
// 
// 	// 获得插入的pos
// 	/*	sql.format("select max(pos) from tb_fav where parent_id=%d", nParentID);
// 	if (!m_pDB->execQuery(oQuery, sql))
// 	return FAV_DB_SQLITE_ERROR;
// 
// 	int	nNextPos = oQuery.eof() ? 1 : (oQuery.getIntField(0) + 1);
// 	oQuery.finalize();*/
// 	int	nNextPos = nPos;
// 	if(nNextPos == -1)
// 	{
// 		sql.format("select max(pos) from tb_fav where parent_id=%d", nParentID);
// 		if (!m_pDB->execQuery(oQuery, sql))
// 			return FAV_DB_SQLITE_ERROR;	
// 		nNextPos = oQuery.eof() ? 1 : (oQuery.getIntField(0) + 1);
// 		oQuery.finalize();
// 	}
// 
// 	__int64 tNow = Util::Base::GetCurrentFileTime();
// 
// 	sql.format("INSERT INTO tb_fav(parent_id, is_folder, title, url, pos, is_best,create_time, last_modify_time) VALUES(%d, %d, '%s', '%s', %d, %d,%lld, %lld);", nParentID, bFolder ? 1 : 0, szutf8Tile.c_str(), bFolder ? "" : szutf8Url.c_str(), nNextPos, bIsBest ? 1 : 0, tNow, tNow);
// 	if (m_pDB->execDML(sql) <= 0)
// 		return FAV_DB_SQLITE_ERROR;
// 
// 	sql.format("select last_insert_rowid();");
// 	if (!m_pDB->execQuery(oQuery, sql))
// 		return FAV_DB_SQLITE_ERROR;
// 
// 	// 必须有一条数据
// 	if (oQuery.eof())
// 		return FAV_DB_SQLITE_ERROR;
// 
// 	int nInsertId = atoi(oQuery.fieldValue(0));
// 	//m_pFavDBThreadWnd->NotifyFavoriteControlAddItem(nInsertId);
// 	m_bDBChanged = TRUE;
// 	// 返回刚插入的那条数据
// 	return nInsertId;
// }
// 
// int CFavDBManager::DeleteItem(int nId, int nParentId, int nPos, bool bMoveNexts/* = TRUE*/)
// {
// 	CHECK_DB_RETURN(FAV_DB_SQLITE_ERROR);
// 
// 	__int64 tNow = Util::Base::GetCurrentFileTime();
// 	CppSQLite3Buffer sql;
// 	sql.format("delete from tb_fav where id=%d", nId);
// 	if( m_pDB->execDML(sql) <= 0)
// 		return FAV_DB_SQLITE_ERROR;
// 
// 	if(bMoveNexts)
// 	{	
// 		sql.format("update tb_fav set pos=pos-1 where  parent_id=%d and pos>%d", nParentId, nPos);
// 		if( m_pDB->execDML(sql) < 0)
// 			return FAV_DB_SQLITE_ERROR;
// 	}
// 
// 	m_bDBChanged = TRUE;
// 	return FAV_DB_SUCCEEDED;
// }
// int CFavDBManager::DeleteFolder(int nId, int nParentId, int nPos, bool bMoveNexts/* = TRUE*/)
// {
// 	CHECK_DB_RETURN(FAV_DB_SQLITE_ERROR);
// 
// 	std::vector<int> vecChildren;
// 	vecChildren.push_back(nId);
// 	GetAllChildrenFolder(vecChildren, nId);
// 	int nSize = vecChildren.size();
// 	if( nSize == 0)
// 		return FAV_DB_PARAM_ERROR;
// 
// 	CppSQLite3Buffer sql;
// 	CppSQLite3Query oQuery;
// 
// 	for(int i = 0; i < nSize; i++)
// 	{
// 		sql.format("delete from tb_fav where parent_id=%d", vecChildren[i]);
// 		if (!m_pDB->execQuery(oQuery, sql))
// 			return FAV_DB_SQLITE_ERROR;
// 	}
// 
// 	return DeleteItem(nId, nParentId, nPos,bMoveNexts);
// }
// 
// 
// int CFavDBManager::DeleteItemNotify(int nId, int nParentId, int nPos, bool bMoveNexts)
// {
// 	int nReslut = DeleteItem(nId, nParentId, nPos, bMoveNexts);
// 	/*
// 	if(FAV_DB_SUCCEEDED == nReslut)
// 	m_pFavDBThreadWnd->NotifyFavoriteControlDeleteItem(nId, nParentId);*/
// 
// 	return nReslut;
// }
// int CFavDBManager::DeleteFolderNotify(int nId, int nParentId, int nPos, bool bMoveNexts)
// {
// 	int nReslut = DeleteFolder(nId, nParentId, nPos, bMoveNexts);
// 	/*
// 	if(FAV_DB_SUCCEEDED == nReslut)
// 	m_pFavDBThreadWnd->NotifyFavoriteControlDeleteItem(nId, nParentId);*/
// 
// 	return nReslut;
// }
// 
// int CFavDBManager::SortFolderByField(int nParentId, LPCSTR lpFildeName, bool bAsc/* = TRUE*/)
// {
// 	CHECK_DB_RETURN(FAV_DB_SQLITE_ERROR);
// 
// 	if(!lpFildeName || lpFildeName[0] == _T('\0'))
// 		return FAV_DB_PARAM_ERROR;
// 
// 	CppSQLite3Query oQuery;
// 	CppSQLite3Buffer sql;
// 	sql.format("select id from tb_fav where parent_id=%d order by is_folder desc,%s %s", nParentId, lpFildeName, bAsc ? "asc" : "desc");
// 	if (!m_pDB->execQuery(oQuery, sql))
// 		return FAV_DB_SQLITE_ERROR;
// 
// 	int nRes = FAV_DB_SUCCEEDED;
// 	vector<int> items;
// 	while (!oQuery.eof())
// 	{
// 		int nId= oQuery.getIntField(0);
// 		items.push_back(nId);
// 		oQuery.nextRow();
// 	}
// 	oQuery.finalize();
// 
// 	if(items.size() == 0)
// 		return FAV_DB_SUCCEEDED;
// 
// 	BeginTransaction(TRUE);
// 	vector<int>::iterator it;
// 	int nPos = 1;
// 	// 移动位置，需要更新时间，不然在合并时会有问题
// 	__int64 tNow = Util::Base::GetCurrentFileTime();
// 	for(it = items.begin(); it != items.end(); it ++, nPos ++)
// 	{
// 		CppSQLite3Buffer sql;
// 		sql.format("update tb_fav set pos=%d, last_modify_time=%lld where id=%d", nPos, tNow, *it);
// 		if(m_pDB->execDML(sql) <=0)
// 		{
// 			XLOGINFO(_T("CFavDBManager::SortFolderByField  update faild."));
// 			RollbackTransaction();
// 			nRes = FAV_DB_SQLITE_ERROR;
// 			break;
// 		}
// 		m_bDBChanged = TRUE;
// 	}
// 	items.clear();
// 	CommitTransaction();
// 
// 	//m_pFavDBThreadWnd->NotifyFavoriteControlUpdateFolder(nParentId);
// 	return nRes;
// }
// 
// int CFavDBManager::MoveItem2Folder(MoveOperStruct* pMS, bool bMovePrevs, bool bMoveNexts)
// {
// 	CHECK_DB_RETURN(FAV_DB_SQLITE_ERROR);
// 
// 	// 不需要移动时，直接返回正确
// 	if(pMS->nSrcParentID == pMS->nDestParentID && (pMS->nSrcPos == pMS->nDestPos || pMS->nSrcPos == pMS->nDestPos - 1))
// 		return FAV_DB_SUCCEEDED;
// 	//return FAV_DB_PARAM_ERROR;
// 
// 	CppSQLite3Buffer sql;
// 	CppSQLite3Query oQuery;
// 
// 	__int64 tNow = Util::Base::GetCurrentFileTime();
// 
// 	if(pMS->nSrcParentID != pMS->nDestParentID)
// 	{
// 		// 在不同目录中移动
// 		if(bMovePrevs)
// 		{
// 			// 移动Src目录中原来的项
// 			sql.format("update tb_fav set pos=pos-1 where  parent_id=%d and pos>%d", pMS->nSrcParentID, pMS->nSrcPos);
// 			if( m_pDB->execDML(sql) < 0)
// 				return FAV_DB_SQLITE_ERROR;
// 		}
// 		if(bMoveNexts)
// 		{
// 			// 移动Dest目录中原来的项
// 			sql.format("update tb_fav set pos=pos+1 where  parent_id=%d and pos>=%d", pMS->nDestParentID, pMS->nDestPos);
// 			if( m_pDB->execDML(sql) < 0)
// 				return FAV_DB_SQLITE_ERROR;
// 		}
// 		sql.format("update tb_fav set last_modify_time=%lld, pos=%d, parent_id=%d where id=%d", tNow, pMS->nDestPos, pMS->nDestParentID, pMS->nSrcID);
// 		if( m_pDB->execDML(sql) < 0)
// 			return FAV_DB_SQLITE_ERROR;
// 
// 		return FAV_DB_SUCCEEDED;
// 	}
// 	else
// 	{
// 		// 在相同目录中移动
// 		if(pMS->nSrcPos < pMS->nDestPos - 1)
// 		{
// 			sql.format("update tb_fav set pos=pos-1 where  parent_id=%d and pos>%d and pos<%d", pMS->nSrcParentID, pMS->nSrcPos, pMS->nDestPos);
// 			if( m_pDB->execDML(sql) < 0)
// 				return FAV_DB_SQLITE_ERROR;
// 
// 			sql.format("update tb_fav set pos=%d, last_modify_time=%lld, parent_id=%d where  id=%d", pMS->nDestPos - 1, tNow, pMS->nDestParentID, pMS->nSrcID);
// 			if( m_pDB->execDML(sql) <= 0)
// 				return FAV_DB_SQLITE_ERROR;
// 		}
// 		else if(pMS->nSrcPos > pMS->nDestPos)
// 		{
// 			sql.format("update tb_fav set pos=pos+1 where  parent_id=%d and pos>=%d and pos<%d", pMS->nSrcParentID, pMS->nDestPos, pMS->nSrcPos);
// 			if( m_pDB->execDML(sql) < 0)
// 				return FAV_DB_SQLITE_ERROR;
// 
// 			sql.format("update tb_fav set pos=%d, last_modify_time=%lld, parent_id=%d where  id=%d", pMS->nDestPos, tNow, pMS->nDestParentID, pMS->nSrcID);
// 			if( m_pDB->execDML(sql) <= 0)
// 				return FAV_DB_SQLITE_ERROR;
// 		}
// 		else
// 			return FAV_DB_PARAM_ERROR;
// 		return FAV_DB_SUCCEEDED;
// 	}
// 	return FAV_DB_SQLITE_ERROR;
// }
// int CFavDBManager::MergeItem2Folder(MoveOperStruct* pMS, bool bMovePrevs)
// {
// 	if( FAV_DB_SUCCEEDED == MoveItem2Folder(pMS, bMovePrevs, TRUE))
// 	{
// 		// 移动成功后删除冲突的项
// 		return DeleteItemNotify(pMS->nCollisionID, pMS->nCollisionParentID, pMS->nCollisionPos, TRUE);
// 	}
// 	return FAV_DB_SQLITE_ERROR;
// }
// 
// int CFavDBManager::MergeFolder2Folder(MoveOperStruct* pMS, bool bMovePrevs)
// {
// 	CHECK_DB_RETURN(FAV_DB_SQLITE_ERROR);
// 
// 	CppSQLite3Buffer sql;
// 	CppSQLite3Query oQuery;
// 
// 	// 1.相同的项，合并
// 	// 1.1查询nSrcId目录中与nCollisionId冲突的项/目录
// 	sql.format("select tb_fav.id, tb_fav.pos, dest_tb_fav.id, dest_tb_fav.pos, tb_fav.is_folder "
// 		"from (select id, is_folder, pos, title from tb_fav where parent_id=%d) as dest_tb_fav, tb_fav "
// 		"where (parent_id=%d) and (tb_fav.is_folder=dest_tb_fav.is_folder) and (tb_fav.title=dest_tb_fav.title)"
// 		, pMS->nCollisionID, pMS->nSrcID);
// 	if (!m_pDB->execQuery(oQuery, sql))
// 		return FAV_DB_SQLITE_ERROR;
// 
// 	MoveOperVector items;
// 	while (!oQuery.eof())
// 	{
// 		MoveOperStruct mop = {0};
// 		mop.nSrcID = oQuery.getIntField(0);
// 		mop.nSrcParentID = pMS->nSrcID;
// 		mop.nSrcPos = oQuery.getIntField(1);
// 
// 		mop.nDestID= oQuery.getIntField(2);
// 		mop.nDestParentID= pMS->nCollisionID;
// 		mop.nDestPos = oQuery.getIntField(3);
// 
// 		// 此时，目标项就是冲突项
// 		mop.nCollisionID= mop.nDestID;
// 		mop.nCollisionParentID= pMS->nCollisionID;
// 		mop.nCollisionPos = mop.nDestPos;
// 
// 		mop.bSrcFolder = mop.bDestFolder = oQuery.getIntField(4) > 0 ? true : false;
// 
// 		items.push_back(mop);
// 		oQuery.nextRow();
// 	}
// 	oQuery.finalize();
// 
// 	// 1.2.并目录下的所以冲突项合
// 	MoveOperVector::iterator it;
// 	for(it = items.begin(); it != items.end(); it ++)
// 	{
// 		MoveOperStruct& mop = *it;
// 		if(mop.bSrcFolder)
// 		{
// 			if( FAV_DB_SUCCEEDED != MergeFolder2Folder(&mop, FALSE))
// 				return FAV_DB_SQLITE_ERROR;
// 		}
// 		else
// 		{
// 			if( FAV_DB_SUCCEEDED != MergeItem2Folder(&mop, FALSE))
// 				return FAV_DB_SQLITE_ERROR;
// 		}
// 	}
// 
// 	// 2.不相同的项，移动
// 	// 2.1找到Src中与Dest不冲突项和目录，由于冲突项已经被合并，剩下的就是不冲突项了
// 	sql.format("select id, pos from tb_fav where  parent_id=%d", pMS->nSrcID);
// 	if (!m_pDB->execQuery(oQuery, sql))
// 		return FAV_DB_SQLITE_ERROR;
// 
// 	MoveOperVector noColItems;
// 	while (!oQuery.eof())
// 	{
// 		MoveOperStruct mo = {0};
// 		mo.nSrcID= oQuery.getIntField(0);
// 		mo.nSrcParentID = pMS->nSrcID;
// 		mo.nSrcPos = oQuery.getIntField(1);
// 		mo.nDestParentID = pMS->nCollisionID;
// 		noColItems.push_back(mo);
// 		oQuery.nextRow();
// 	}
// 	oQuery.finalize();
// 
// 	int nMaxPos = 0;
// 	if(noColItems.size() > 0)
// 	{
// 		sql.format("select max(pos) from  tb_fav where  parent_id=%d", pMS->nCollisionID);
// 		if (!m_pDB->execQuery(oQuery, sql))
// 			return FAV_DB_SQLITE_ERROR;
// 
// 		nMaxPos = oQuery.getIntField(0) + 1;
// 		oQuery.finalize();
// 	}
// 
// 	MoveOperVector::iterator vit;
// 	for(vit = noColItems.begin(); vit != noColItems.end(); vit++)
// 	{
// 		MoveOperStruct& mo = *vit;
// 		mo.nDestPos = nMaxPos++;
// 		// 到目前为止，MoveItem2Folder需要的5个参数都以配齐
// 		if( FAV_DB_SUCCEEDED != MoveItem2Folder(&mo, TRUE, FALSE))
// 			return FAV_DB_SQLITE_ERROR;
// 	}
// 
// 	items.clear();
// 	noColItems.clear();
// 
// 	// 3.删除自己
// 	if(FAV_DB_SUCCEEDED != DeleteItemNotify(pMS->nSrcID, pMS->nSrcParentID, pMS->nSrcPos, bMovePrevs))
// 		return FAV_DB_SQLITE_ERROR;
// 
// 
// 	// 移动冲突项到目标位置（递归操作不需要）
// 	if(!bMovePrevs)
// 		return FAV_DB_SUCCEEDED;
// 	return MoveItem2Folder(pMS, TRUE, TRUE);
// }
// 
// int CFavDBManager::GetDBInfo(CppSQLite3DB * pDB, DBInfo* pInfo)
// {
// 	CHECK_DB_RETURN(FAV_DB_SQLITE_ERROR);
// 
// 	if(!pInfo)
// 		return FAV_DB_PARAM_ERROR;
// 
// 	memset(pInfo, 0, sizeof(DBInfo));
// 
// 	pInfo->bComplete = CheckDBIntegrity(pDB);
// 	if(!pInfo->bComplete)
// 		return FAV_DB_SQLITE_ERROR;
// 
// 	CppSQLite3Buffer sql;
// 	CppSQLite3Query oQuery;
// 
// 	// 获得记录条数
// 	//sql.format("select seq From sqlite_sequence where name='tb_fav';");
// 	sql.format("select count(*) from tb_fav where is_folder=0");
// 	if (!pDB->execQuery(oQuery, sql) || oQuery.eof())
// 		return FAV_DB_SQLITE_ERROR;
// 	pInfo->nFavCount = oQuery.getIntField(0);
// 	oQuery.finalize();
// 
// 	// 获得版本号
// 	sql.format("select value from tb_misc where key='%s';", FAVDB_VERSION_KEY);
// 	if (!pDB->execQuery(oQuery, sql) || oQuery.eof())
// 		return FAV_DB_SQLITE_ERROR;
// 	_snprintf(pInfo->chVersion, 127, "%s", oQuery.getStringField(0));
// 	oQuery.finalize();
// 
// 	pInfo->nVersion = strcmp(pInfo->chVersion, FAVDB_VERSION_VALUE);
// 
// 	// 获得最后同步时间
// 	sql.format("select value from tb_misc where key='%s';", FAVDB_SYN_TIME_KEY);
// 	if (!pDB->execQuery(oQuery, sql) || oQuery.eof())
// 		return FAV_DB_SQLITE_ERROR;
// 	pInfo->lnSynTime = _atoi64(oQuery.getStringField(0));
// 	oQuery.finalize();
// 
// 	return FAV_DB_SUCCEEDED;
// }
// 
// CString CFavDBManager::GetDailyBackupFilePath()
// {
// 	TCHAR szBackupDir[MAX_PATH] = {0};
// 	PathCombine(szBackupDir,m_strDBPath,_T("..\\DailyBackup\\"));
// 	CreateDirectory(szBackupDir, NULL);
// 	CString strFilePath;
// 
// 	//CTime ti(time(NULL));	
// 	SYSTEMTIME st = {0};
// 	GetLocalTime(&st);
// 	strFilePath.Format(_T("%s360sefav_%04d_%02d_%02d.favdb"), szBackupDir, st.wYear, st.wMonth, st.wDay);
// 	if(PathFileExists(strFilePath))
// 		return _T("");
// 
// 	return strFilePath;
// }
// 
// void CFavDBManager::DeleteOldBackupFile()
// {
// 	std::vector<CString> listFileName;
// 	GetBackupFiles(listFileName);
// 
// 	while(listFileName.size() > 7)
// 	{
// 		// 如果文件数超过7个，需要删除，从最早的开始删除
// 		CString &strFilePath = listFileName.front();
// 		if(!DeleteFile(strFilePath))
// 			XLOGINFO(_T("CFavDBManager::DeleteOldBackupFile  DeleteFile %s faild."), strFilePath);
// 		listFileName.erase(listFileName.begin());
// 	}
// 
// 	listFileName.clear();
// }
// 
// void CFavDBManager::GetBackupFiles(std::vector<CString>& listFiles)
// {
// 	// 不需要排序，FindFirstFile与FindNextFile找出文件的循序就是以文件夹名字排序好的
// 	listFiles.clear();
// 
// 	static TCHAR szBackupDir[MAX_PATH] = {0};
// 	if(!szBackupDir[0])
// 		PathCombine(szBackupDir, m_strDBPath, _T("..\\DailyBackup\\"));
// 
// 	static TCHAR szFindFilePath[MAX_PATH] = {0};
// 	if(!szFindFilePath[0])
// 		_sntprintf(szFindFilePath, MAX_PATH - 1, _T( "%s360sefav_*_*_*.favdb" ), szBackupDir);
// 
// 	HANDLE hFindFile = INVALID_HANDLE_VALUE;
// 	WIN32_FIND_DATA struFindFileData={0};
// 	hFindFile = FindFirstFile(szFindFilePath, &struFindFileData);
// 	if (hFindFile == INVALID_HANDLE_VALUE)
// 		return;
// 	do
// 	{
// 		if (struFindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
// 			continue;
// 
// 		CString strFilePath;
// 		strFilePath.Format(_T( "%s%s" ), szBackupDir, struFindFileData.cFileName);
// 		listFiles.push_back(strFilePath);
// 	}
// 	while (FindNextFile(hFindFile, &struFindFileData));
// 	FindClose(hFindFile);
// 
// 	// 修复BUG：38969 hangcheng review by wanyong
// 	// 描述：删除的并不是最早的
// 	// 修复方法，需要对遍历出来的文件进行排序
// 	std::sort(listFiles.begin(), listFiles.end());
// }
// 
// void FindChild(FolderMap& mmapParent, std::vector<int>& vecDeleteParnet, int nId)
// {
// 	for (FolderMapItPair its = mmapParent.equal_range(nId); its.first != its.second; its.first++) 
// 	{
// 		FolderMap::iterator curit = its.first;
// 		vecDeleteParnet.push_back(curit->second);
// 		FindChild(mmapParent, vecDeleteParnet, curit->second);
// 	}
// }
// void CFavDBManager::GetAllChildrenFolder(std::vector<int>& vecChildren, int nParnet)
// {
// 	CHECK_DB;
// 
// 	CppSQLite3Buffer sql;
// 	CppSQLite3Query oQuery;
// 
// 	sql.format("select P.id, C.id from tb_fav as P, tb_fav as C where P.is_folder=1 and C.is_folder=1 and P.id=C.parent_id");
// 	if (!m_pDB->execQuery(oQuery, sql) || oQuery.eof())
// 		return;
// 
// 	FolderMap mmapParent;
// 	while (!oQuery.eof())
// 	{
// 		int nPid= oQuery.getIntField(0);
// 		int nCid= oQuery.getIntField(1);
// 		mmapParent.insert(FolderPair(nPid, nCid));
// 		oQuery.nextRow();
// 	}
// 	oQuery.finalize();
// 
// 	FindChild(mmapParent, vecChildren, nParnet);
// }
// 
// //（注：本函数只能用于拼接sql语句）
// string CFavDBManager::GetUTF8String(LPCTSTR lpszString)
// {
// 	//	static const char chErrChars[] = "\'\"*<>|\t\r\n";
// 	// 修改字符串过滤方法
// 	// 1。过滤掉空白字符\r\n\t
// 	// 2。将单引号替换成2个单引号
// 	std::string str = UTF8Encode(lpszString);
// 	LPCSTR lpUnicodeStr = str.c_str();
// 
// 	std::string strRes = "";
// 	for(unsigned int i = 0; i < str.size(); i++)
// 	{
// 		// 去掉空白字符
// 		if(lpUnicodeStr[i] == _T('\t') || lpUnicodeStr[i] == _T('\r') || lpUnicodeStr[i] == _T('\n'))
// 			continue;
// 
// 		strRes += lpUnicodeStr[i];
// 
// 		// 如果是单引号，需要再加以个单引号
// 		if(lpUnicodeStr[i] == _T('\''))
// 			strRes += _T('\'');
// 	}
// 	return strRes;
// }
// // 修复bug：38967 hangcheng review by wanyong
// // 错误DB文件不应该删除，应该保存到指定目录，待日后分析
// bool CFavDBManager::MoveErrorDBFile(LPCTSTR lpszDBFile)
// {
// 	if(!lpszDBFile || !lpszDBFile[0])
// 		return false;
// 
// 	// 根据路径获得文件名
// 	LPCTSTR lpszDBFileName = NULL;
// 	for(int i = _tcslen(lpszDBFile); i>0; i--)
// 	{
// 		if(lpszDBFile[i] == _T('\\'))
// 		{
// 			lpszDBFileName = lpszDBFile + i + 1;
// 			break;
// 		}
// 	}
// 
// 	if(!lpszDBFileName || !lpszDBFileName[0])
// 		return false;
// 
// 
// 	// 获得指定的目录，不存在则创建
// 	TCHAR szErrDir[MAX_PATH] = {0};
// 	PathCombine(szErrDir, m_strDBPath, _T("..\\DailyBackup\\"));
// 	CreateDirectory(szErrDir, NULL);
// 
// 	PathAppend(szErrDir, _T("err"));
// 	CreateDirectory(szErrDir, NULL);
// 
// 
// 	TCHAR szErrFile[MAX_PATH] = {0};
// 	//CTime ti(time(NULL));	
// 	SYSTEMTIME st = {0};
// 	GetLocalTime(&st);
// 
// 	_sntprintf(szErrFile, MAX_PATH - 1, _T("%s\\%s.%04d.%02d.%02d.%02d.%02d.%02d"),szErrDir, lpszDBFileName, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
// 
// 	XLOGINFO(_T("CFavDBManager::MoveErrorDBFile, dest:%s"), szErrFile);
// 
// 	if(!MoveFile(lpszDBFile, szErrFile))
// 	{
// 		XLOGINFO(_T("CFavDBManager::MoveErrorDBFile  %s to %s faild."), lpszDBFile, szErrFile);
// 		return false;
// 	}
// 	return true;
// }
// 
// 

//  [7/23/2010 by ken] 
	
bool CFavDBManager::ImportAllItems(vector<FavItem> & vecItems)
{
	CHECK_DB_RETURN(false);
	CppSQLite3Query oQuery;

	if (!m_pDB->execQuery(oQuery, "SELECT id, parent_id, is_folder,title,url, pos FROM tb_fav order by pos, parent_id"))
		return false;

	while (!oQuery.eof())
	{
		FavItem item;
		item.nID = oQuery.getIntField(0);
		item.nParentID = oQuery.getIntField(1);	
		item.bFolder=oQuery.getIntField(2)==1?true:false;	
		// 转化标题(utf8)为unicode
		item.strTitle = UTF8Decode(oQuery.getStringField(3));
		item.strURL = UTF8Decode(oQuery.getStringField(4));
		item.nPos = oQuery.getIntField(5);
		vecItems.push_back(item);
		oQuery.nextRow();
	}
	return true;
}
#endif

bool SafeBrowserDBImporter::ArrangeAllItems(vector<FavItem> const & vecItems,
    SafeBrowserImporter::BookmarkVector & result) {
	result.clear();
	for(unsigned int i = 0;i < vecItems.size(); i++) {
		ImportedBookmarkEntry _BookmarkEntry;
		_BookmarkEntry.in_toolbar = true;
		_BookmarkEntry.title = vecItems[i].strTitle;
		_BookmarkEntry.url = GURL(vecItems[i].strURL);
    // 必须标示是否是文件夹，否则导入的收藏夹顺序会有问题。
    _BookmarkEntry.is_folder = vecItems[i].bFolder;
		//_BookmarkEntry.creation_time=base::Time::FromTimeT(vecItems[i].lCreateTime);
		_BookmarkEntry.path = GenPath(vecItems[i], vecItems);
		result.push_back(_BookmarkEntry);
	}
	return true;
}

std::vector<std::wstring> SafeBrowserDBImporter::GenPath(FavItem item,
    vector<FavItem> const & vecItems) {
	std::vector<std::wstring> result;

	if(item.nParentID == ROOT_ITEM_ID) {
		return result;
	}
  // 极速的导入360se递归算法有问题，现在使用按层遍历解决书签顺序问题。（也可以使用深度优先遍历，这样可以不记录文件夹）
	else  {
    while(item.nParentID != ROOT_ITEM_ID) {
      for(unsigned int i = 0; i < vecItems.size(); ++i) {
        if(vecItems[i].nID == item.nParentID) {
          // 越顶层的结点，应该在越前方，所以用insert而不是push_back
          result.insert(result.begin(), vecItems[i].strTitle);
          item = vecItems[i];
          break;
        }
      }
    }
    return result;
	}
}

sql::Connection* SafeBrowserDBImporter::OpenAndCheck(std::wstring const & filename) {
	std::unique_ptr<sql::Connection> sqlite(new sql::Connection);
	if (!sqlite.get())
		return NULL;

	sqlite->set_page_size(4096);
	sqlite->set_cache_size(2000);

	bool open_did_succeed = sqlite->Open(base::FilePath(filename));
	if (!open_did_succeed)
		return NULL;

	const char* query = "select value from tb_misc where key = ?";
	sql::Statement s(sqlite->GetUniqueStatement(query));

  if (!s.BindString(0, FAVDB_VERSION_KEY))
    return NULL;

	if (s.Step()) {
		std::string ver = s.ColumnString(0);
		int nVerComp = strcmp(ver.c_str(), FAVDB_VERSION_VALUE);
		if (nVerComp < 0)
			return NULL;
	}

	return sqlite.release();
}

bool SafeBrowserDBImporter::ImportData(sql::Connection* sqlite,
                                       vector<FavItem> & vecItems)
{
  // 一定要按原来收藏夹的顺序导入，order by parent_id, pos
	sql::Statement s(sqlite->GetUniqueStatement("SELECT id, parent_id, is_folder, title,url, pos "
		  "FROM tb_fav order by parent_id, pos"));

	while (s.Step()) {
		FavItem item;
		item.nID = s.ColumnInt(0);
		item.nParentID = s.ColumnInt(1);	
		item.bFolder = s.ColumnInt(2) == 1 ? true : false;
		// 转化标题(utf8)为unicode
		item.strTitle = UTF8Decode(s.ColumnString(3).c_str());
		item.strURL = UTF8Decode(s.ColumnString(4).c_str());
		item.nPos = s.ColumnInt(5);
		vecItems.push_back(item);
	}
	return true;
}

#pragma  warning(pop)