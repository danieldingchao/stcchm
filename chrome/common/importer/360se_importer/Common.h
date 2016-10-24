#pragma once
//
// 收藏数据结构
//
struct FavItem
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
//	bool bDeleted;				// 是否被删除
	int nReserved;				// 保留字段
};

struct FavItemA
{
	int nID;					// id
	int nParentID;				// 所在文件夹的id
	bool bFolder;				// 是否是文件夹
	bool bBest;					// 是否我的最爱
	int  nPos;					// 收藏夹的位置
	std::string strTitle;		// 标题
	std::string strURL;		// 链接
	__int64 lCreateTime;		// 创建时间
	__int64 lLastModifyTime;	// 上次修改时间
//	bool bDeleted;				// 是否被删除
	int nReserved;				// 保留字段
};

// 移动通知
struct MoveItemNotifyParam
{
	int nID;
	int nPreParent;
	int nNowParent;
};

// 用于移动项和目录,使用结构体记录用到的说有数据，免得多次查询操作
struct MoveOperStruct
{
	// 移动的原数据
	int nSrcID;	
	int nSrcParentID;
	int nSrcPos;
	bool bSrcFolder;

	//移动的目标数据
	int nDestID;	
	int nDestParentID;
	int nDestPos;
	bool bDestFolder;

	// 移动时有冲突的项的数据
	int nCollisionID;	
	int nCollisionParentID;
	int nCollisionPos;
};

// 根据title排序用
struct DBTitleStor
{
	int nId;
	bool bFolder;
	std::wstring strTile;
	bool operator < (DBTitleStor s)
    {
		if(bFolder && !s.bFolder)
			return true;
		else if(!bFolder && s.bFolder)
			return false;

		// 修改排序中的字符串比较算法
		return ::CompareString(MAKELCID(MAKELANGID(LANG_CHINESE,SUBLANG_CHINESE_SIMPLIFIED),SORT_CHINESE_PRCP), 
			0,//NORM_IGNORECASE | NORM_IGNOREKANATYPE | NORM_IGNORENONSPACE | NORM_IGNORESYMBOLS | NORM_IGNOREWIDTH | SORT_STRINGSORT | NORM_LINGUISTIC_CASING, 
			strTile.c_str(), -1, s.strTile.c_str(), -1)==CSTR_LESS_THAN ? true : false;
    }
};

// 数据库的基本信息
struct DBInfo
{
	bool bComplete;			// 数据库是否完整
	char chVersion[128];	// 数据库的版本号
	int nVersion;			// 数据库的版本号（<0表示低，0表示合适，>0表示高）
	int nFavCount;			  // 收藏的数据条数
	__int64 lnSynTime;		 // 上传同步时间
};

//////////////////////////////////////////////////////////////////////////

// tb_misc表中的key
#define FAVDB_SYN_TIME_KEY			"db_last_syn_time"
#define FAVDB_VERSION_KEY			"db_version"

// 定义数据库的版本号
#define FAVDB_VERSION_VALUE			"1"

///////////////////////////////////////////////////////////////////////////

#define ROOT_ITEM_ID            0
#define ROOT_ITEM_NET			0xFFFFFFEF
#define FAVBAR_SIDEBARID		0xFFFFFFEE

// 工具栏上菜单的ID
#define ROOT_TOOLBAR_MENUID		2147483647

// 增加按钮的ID 给整数的最大值
#define TOOLBAR_ADDBUTTON_ID	2147483646

// 收藏夹的移动方式
#define MOVEITEM_COLLISION		1					// 是否冲突
#define MOVEITEM_UP				2					// 移动到目标ID的上面
#define	MOVEITEM_DOWN			4					// 移动到目标ID的下面
#define MOVEITEM_IN				8					// 移动到目标ID的里面

// 数据库操作返回值
#define FAV_DB_SUCCEEDED			0				// succ
#define FAV_DB_SQLITE_ERROR			-1				// database connet error,文件被占用
#define FAV_DB_PARAM_ERROR			-2				// param error
#define FAV_DB_ITEM_EXISTED			-3				// folder/link existed
#define FAV_DB_ITEM_NOT_EXISTED		-4				// folder/link not existed
#define FAV_DB_VERSION_HIGHER		-5				// folder/link not existed
#define FAV_DB_VERSION_LOWER		-6				// folder/link not existed
#define FAV_DB_DESTROYED			-7				// folder/link not existed

// 数据库相应界面的操作错误吗
#define FAV_DB_ADD_ERROR			0				// 增加一项操作错误
#define FAV_DB_DELETE_ERROR			-1				// 删除一项错误
#define FAV_DB_MODIFY_ERROR			-2				// 修改一项操作错误
#define FAV_DB_BEST_CHANGE_ERROR	-3				// 我的最爱状态变更错误
#define FAV_DB_MOVE_ERROR			-4				// 移动操作错误
#define FAV_DB_RESTORE_ERROR		-5				// 恢复数据出错
#define FAV_DB_BACKUP_ERROR			-6				// 备份数据出错
#define FAV_DB_RESTORE_SUCCESS		1				// 恢复数据成功
#define FAV_DB_BACKUP_SUCCESS		2				// 备份数据成功	
#define FAV_DB_AUTO_RESTORE_FAILD		3			// 自动恢复失败
#define FAV_DB_AUTO_RESTORE_SUCCEEDED	4			// 自动恢复成功

// 标识当前数据库的状态
#define	 DB_STATUS_CACHE		0		// 本地缓存
#define  DB_STATUS_NEW			1		// 新创建
#define  DB_STATUS_BACKUP		2		// 自动恢复
//////////////////////////////////////////////////////////////////////////

//
// FavDBThreadWnd --> FaveriteControlWnd
// 数据库操作线程 --> 主线程
//

// 通知主线程增加了一项
// wparam 增加项的ID
#define WM_FAVCONTROL_ADD_ITEM			(WM_USER + 0x1001)				

// 通知主线程删除了一项
// wparam 删除项的ID
#define WM_FAVCONTROL_DELETE_ITEM		(WM_USER + 0x1002)

// 通知主线程一个项已经重命名
// wparam 重命名项的ID
#define WM_FAVCONTROL_RENAME_ITEM		(WM_USER + 0x1003)

// 通知主线程的某项我的最爱状态被更改
// wparam best状态发生更改的ID
#define WM_FAVCONTROL_BESTCHANGE_ITEM	(WM_USER + 0x1004)

// 通知主线程某项已经移动
// wparam 移动的id值， lparam = makelparam（prevParentID， ParentID）
#define WM_FAVCONTROL_MOVE_ITEM			(WM_USER + 0x1005)

// 通知主线程界面重新加载收藏夹
// wparam,lparam 没有使用
#define WM_FAVCONTROL_RELOAD_DB			(WM_USER + 0x1006)

// 通知主线程界面重新加载一个文件夹
// wparam Folder的ID， lparam未使用
#define WM_FAVCONTROL_UPDATE_FOLDER		(WM_USER + 0x1007)

// 通知主线程界面相应的操作错误
// wparam 错误码 lparam 未使用
#define WM_FAVCONTROL_OPERATE_ERROR		(WM_USER + 0x1008)

//
// FavDBThreadWnd --> FavSyncWnd
// 数据库操作线程 --> 同步线程
//

//  通知同步线程，收藏改变
//  wParam，lParam，未使用
#define WM_FAVSYNC_FAVCHANGE			(WM_USER + 0x1009)

//
// 窗口 --> 窗口
//

//  通知收藏夹面板手动下载完毕
//  WPARAM：title
//  LPARAM：text
#define WM_FAVPANE_DOWN           (WM_USER + 0x1010)

//  通知收藏夹面板手动上传完毕
//  WPARAM：title
//  LPARAM：text
#define WM_FAVPANE_UPLOAD         (WM_USER + 0x1011)
//////////////////////////////////////////////////////////////////////////

//
// 一些常量的定义
//

#define FAV_MENU_TITLE_ADDITEM				_T("添加到收藏夹...\tCtrl+D")
#define FAV_MENU_TITLE_ADDITEM_ALL			_T("添加所有页面到收藏夹...")
#define FAV_MENU_TITLE_CLEAN				_T("整理收藏夹...")
#define FAV_MENU_TITLE_MORE					_T("更多功能")
#define FAV_MENU_TITLE_SETTING				_T("收藏栏设置")
#define FAV_MENU_TITLE_BACKUP				_T("备份收藏夹...")
#define FAV_MENU_TITLE_RESTORE				_T("还原收藏夹...")
#define FAV_MENU_TITLE_IEIMPORT				_T("从IE收藏夹导入...")
#define FAV_MENU_TITLE_FILEIMPORT			_T("从文件导入...")
#define FAV_MENU_TITLE_IMPORT				_T("导入收藏夹")
#define FAV_MENU_TITLE_EXPORT				_T("导出收藏夹...")
#define FAV_MENU_TITLE_ADD_TO_FOLDER		_T("添加到本文件夹...")
#define FAV_MENU_TITLE_SYNA_FAV				_T("同步收藏")
#define FAV_MENU_TITLE_UPFAV				_T("手动上传网络收藏夹")
#define FAV_MENU_TITLE_DOWNFAV				_T("手动下载网络收藏夹")

#define FAV_SYNC_ERR_NET				    _T("连接超时，网络收藏夹更新失败。请检查：\r\n1. 网络连接; 2. 代理设置; 3. 防火墙设置")
//#define FAV_SYNC_SVR_ERR                    _T("服务器正在维护，您所作的修改暂时无法上\r\n传。问题解决后，网络收藏将自动恢复。")
#define FAV_SYNC_DBVER_ERR					_T("浏览器版本过低，请下载最新版本浏览器。")
#define FAV_SYNC_WORKING                    _T("正在更新网络收藏夹...")
#define FAV_SYNC_ERR_NETBUSY                _T("网络繁忙，暂时无法更新您的网络收藏夹，\r\n稍候将自动重试。")
#define FAV_SYNC_ERR_MD5                    _T("连接中断，网络收藏夹更新失败。\r\n稍候将自动重试。")

#define FAV_PANE_TITLE_USER					_T("的网络收藏夹")
#define FAV_PANE_TITLE_NO_USER				_T("网络收藏夹")
#define FAV_PANE_USERNAME					_T("帐号:")
#define FAV_PANE_FAVCOUNT					_T("收藏:")
#define FAV_PANE_LAST_SYNC					_T("上次更新完成于")

#define DEF_ONLINEFAV_UNLOGIN_TIP			_T("网络帐户未登录\r\n登录后使用网络收藏夹")
#define DEF_ONLINEFAV_LOGIN_TIP				_T("网络帐户已登录\r\n正在使用网络收藏夹")

// 产品需求，增大收藏栏的空间
// #define FAV_BAR_ADDBTN_TITLE				_T("添加收藏|")
// #define FAV_BAR_NETBTN_TITLE				_T("网络收藏夹|")
#define FAV_BAR_ADDBTN_TITLE				_T("添加|")
#define FAV_BAR_NETBTN_TITLE				_T("登录|")

#define FAV_UP_DOWN_NET_WAIT				_T("请稍候")
#define	FAV_UP_DOWN_NET_OK					_T("确定")

#define FAV_IMPORT_FOLDERNAME				_T("%d-%02d-%02d 导入")
#define FAV_IMPORT_FOLDERNAME_EX			_T("%d-%02d-%02d 导入(%d)")

#define FAV_IMPORT_FROM_IE					_T("从IE导入")

#define FAV_NEW_FOLDER						_T("新建文件夹")

//
//  统计字段
//
enum {
    COUNT_LOGINDLG = 0,     //  登录框展现
    COUNT_LOGINDLG_CANCEL,  //  登录框取消
    COUNT_LOGOUT,           //  退出登录
    COUNT_MULT_ACCOUNT,     //  有多个账户
    COUNT_FAV_SHOWTYPE,     //  收藏展现	0: 显示收藏栏，隐藏侧边栏  1:显示收藏栏，隐藏侧边栏
                            //  2: 隐藏收藏栏，显示侧边栏  3:显示收藏栏，显示侧边栏
    COUNT_FAV_COUNT,        //  收藏条数
    COUNT_FAV_FAVMGR,       //  整理收藏
    COUNT_FAV_DESTROY,      //  收藏损坏
    COUNT_FAV_GUIDE_REG,       
};

enum {
    SUM_FAV_OPEN_MENU = 0,  //  从菜单访问收藏的次数
    SUM_FAV_OPEN_BAR,       //  从收藏栏访问收藏的次数
    SUM_FAV_OPEN_TREE,      //  从侧边栏收藏的次数
    SUM_FAV_ADD_MENU,       //  从菜单栏添加
    SUM_FAV_ADD_BAR,        //  从收藏栏添加
    SUM_FAV_ADD_TREE,       //  从侧边栏添加
    SUM_FAV_ADD_SHORTCUT,   //  用快捷键添加
    SUM_FAV_ADD_FOLDER,     //  添加到文件夹
    SUM_FAV_PANE_SHOW,      //  收藏面版展示
    SUM_FAV_FAVMGR_SHOW,    //  整理收藏展现
};

// 收藏栏按钮的宽度，和菜单最大宽度
#define FAVBAR_BUTTON_DEFAULT_WIDTH 120
#define FAVMENU_DEFALT_WIDTH		120

// ID的有效性
#define FAVID_VALID_NOT				0	// ID不存在
#define FAVID_VALID_ROOT			1	// ID是根目录
#define FAVID_VALID_EXIST			2	// 存在该ID得收藏夹项




#define FAV_DB_NAME		_T("360sefav.db")		// 数据库名字