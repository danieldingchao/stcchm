// Copyright (c) 2011 qihoo.com
// Author liuqingping@qihoo.net

#include "sg_importer.h"
//#include "base/crypto/des.h"
#include "base/bind.h"
#include "base/strings/string_util.h"
#include "sql/transaction.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/md5.h"
#if defined(OS_MACOSX)
#include "base/mac_util.h"
#endif
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "base/file_version_info.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"
//#include "chrome/browser/diagnostics/sqlite_diagnostics.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_status.h"
//#include "base/Report/MIDGen.h"
#include <string>
#include <Shlwapi.h>
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/importer/importer_bridge.h"
#include <shlobj.h>
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/url_constants.h"
#include <stdio.h>
#include <tchar.h>
#include "base/files/file_path.h"
#include "sql/init_status.h"
#include "base/strings/stringprintf.h"
#include "sogou_explorer_hacker_new.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/utility/importer/local_bookmarks_misc.h"
#include "chrome/common/importer/360se_importer/database/CppSQLite3.h"

// extern "C" {
//   int __stdcall GenSGFile(char const*  src,char const*  des);
// }

using base::string16;

SGBrowserImporter::SGBrowserImporter()
  : loaded_(FALSE)
  , first_run_(FALSE){}

SGBrowserImporter::SGBrowserImporter(bool first_run)
  : loaded_(FALSE)
  , first_run_(first_run){}

SGBrowserImporter::~SGBrowserImporter( void ) {}

bool SGBrowserImporter::LoadSgDB() {
  if (loaded_) {
    return true;
  }
  LOG(ERROR) << "Bookmark: Begin LoadSgDB";
  loaded_ = true;
  std::wstring strSgFavPath =
    SystemCommon::FilePathHelper::GetParentPath(m_sgDBMgr.GetSgFavFilePath());
  std::wstring strFlag = SystemCommon::StringHelper::Format( L"chk_%d", GetTickCount());
  if(!m_sgDBMgr.Init(strSgFavPath.c_str(), strFlag.c_str())) {
    loaded_ = false;       // load failed.
  }
  LOG(ERROR) << (loaded_ ?"Bookmark: Begin LoadSgDB success" : "Bookmark: Begin LoadSgDB failed");
  return loaded_;
}

void SGBrowserImporter::UnLoadSgDB() {
  LOG(ERROR) << "Bookmark: Begin UnLoadSgDB";
  if (loaded_) {
    m_sgDBMgr.UnInit();
  }
  loaded_ = false;
}

void SGBrowserImporter::GetEntrys(int parent_id, BookmarkVector& bookmarks, int index, std::vector<string16> path) {
  BookmarkVector inner_bookmarks;
  GetEntrysInner(parent_id, inner_bookmarks, path);

  size_t inner_size = inner_bookmarks.size();
  if (inner_size < 1)
    return;

  char szStep[MAX_PATH * 2] = {0};
  for (int e = 0; e < index; e++) {
    strncat(szStep, "\t", (MAX_PATH * 2) - 1);
  }

  //先遍历文件夹
  for (size_t i = 0; i < inner_size; i++) {
    ImportedBookmarkEntry& entry = inner_bookmarks[i];
    entry.in_toolbar = true;
    //std::string title = base::UTF16ToASCII(entry.title);
    //std::string msg = base::StringPrintf("Bookmark: GetEntrys %d, %d, %s, %s", entry.id, entry.parent_id, title.c_str(), entry.url.spec().c_str());
    //LOG(ERROR) << msg.c_str();
    bookmarks.push_back(entry);
    if (entry.is_folder) {
      std::vector<string16> path_inner(path);
      path_inner.push_back(entry.title);
      GetEntrys(entry.id, bookmarks, index + 1, path_inner);
    }
  }
}

void SGBrowserImporter::GetEntrysInner(int parent_id, BookmarkVector& bookmarks, std::vector<base::string16> path) {
  DCHECK(loaded_);

  CppSQLite3Buffer sql;
  CppSQLite3Query oQuery;
  CppSQLite3DB* pSGDB = m_sgDBMgr.GetDB();

  if (!pSGDB)
    return;

  // 两种表格
  sql.format( "SELECT id, pid, folder, title, url, sequenceNo, addtime, lastmodify FROM favorite WHERE pid=%d order by pid", parent_id);  // 4.2
  if (!pSGDB->execQuery(oQuery, sql)) {
    sql.format( "SELECT id, pid, folder, title, url, sequenceNo, addtime, lastmodify FROM favorTable WHERE pid=%d order by pid", parent_id);  // 3.x ???
    if (!pSGDB->execQuery(oQuery, sql)) {
      return;
    }
  }

  while (!oQuery.eof()) {
    int nSgID = oQuery.getIntField("id");
    int nSgPid = oQuery.getIntField("pid");
    bool bFolder = oQuery.getIntField("folder") ? true : false;
    std::wstring strTitle = UTF8Decode(oQuery.getStringField("title"));
    std::wstring strUrl = UTF8Decode(oQuery.getStringField("url"));
    int nPos = oQuery.getIntField("sequenceNo");
    __int64 iCreateTime = _atoi64(oQuery.fieldValue("addtime"));

    ImportedBookmarkEntry entry;
    entry.id = nSgID;
    entry.parent_id = nSgPid;
    entry.is_folder = bFolder;
    if (!entry.is_folder)
      entry.url = GURL(strUrl);
    entry.title = strTitle;
    entry.creation_time = base::Time::FromInternalValue(iCreateTime);
    entry.path = path;
    bookmarks.push_back(entry);

    oQuery.nextRow();
  }
  return;
}

void SGBrowserImporter::StartImport(const importer::SourceProfile& source_profile,
                           uint16_t items,
                           ImporterBridge* bridge) {
  bridge_ = bridge;
  bridge_->NotifyStarted();
  if ((items & importer::HOME_PAGE) && !cancelled())
     ImportHomepage();
  if ((items & importer::FAVORITES) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::FAVORITES);
    ImportFavorites();
    bridge_->NotifyItemEnded(importer::FAVORITES);
  }
  bridge_->NotifyEnded();
}

void SGBrowserImporter::ImportFavorites(const importer::SourceProfile& profile_info,
    uint16_t items, std::string &import_pref_type,
    ImporterBridge* bridge, BookmarkVector* bookmarks) {
  if (LoadSgDB()) {
    std::vector<string16> path;
    GetEntrys(0, *bookmarks, 0, path);
    UnLoadSgDB();
  }
}

void SGBrowserImporter::ImportFavorites() {
  BookmarkVector bookmarks;
  if (LoadSgDB()) {
    std::vector<string16> path;
    GetEntrys(0, bookmarks, 0, path);
    UnLoadSgDB();
  }
  if ((!bookmarks.empty())&& !cancelled()) {
    const string16& first_folder_name = L"搜狗收藏夹";
    bridge_->AddBookmarks(bookmarks, first_folder_name);
  }
}

PCHAR SGBrowserImporter::GetSGHomePage(wchar_t const * pszFile) {
  static CHAR szHomePage[1024] = {0};
  if( strlen(szHomePage) > 0 )
    return szHomePage;

//   LPCTSTR pszFile = L"c:\\config.xml";
  FILE * fp = _tfopen( pszFile, _T("r") );
  if ( fp ) {
    fseek( fp, 0, SEEK_END );
    long lSize = ftell(fp);
    if ( lSize > 0 && lSize < 1000000) {
      lSize ++;
      fseek( fp, 0, SEEK_SET );

      PCHAR pszContent = new CHAR[lSize];
      if ( pszContent ) {
        memset( pszContent, 0, lSize );
        fread( pszContent, 1, lSize-1, fp );

        PCHAR pszType = strstr( pszContent, "homepagetype=\"3\"" );
        if ( pszType ) {
          const char * pszKeyWord = "homepage=\"";
          PCHAR pszHome = strstr( pszContent, pszKeyWord );
          if ( pszHome ) {
            pszHome += strlen(pszKeyWord);
            PCHAR pszEnd = strstr( pszHome, "\" ");
            if ( pszEnd ) {
              memcpy( szHomePage, pszHome, pszEnd-pszHome );
            }
          }
        }

        delete []pszContent;
      }
    }
    fclose( fp );
  }

  if( strlen(szHomePage) > 0 )
    return szHomePage;
  return NULL;
}

std::wstring SGBrowserImporter::GenSGConfigFilePath() {
  std::wstring strSgFavPath = m_sgDBMgr.GetSgFilePath();
  if (strSgFavPath.size()) {
    std::wstring re(strSgFavPath);
    re.append(L"\\config.xml");
    return re;
  }
  std::wstring sg_db;
  wchar_t buffer[MAX_PATH] = {0};
  if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL,
    SHGFP_TYPE_CURRENT, buffer))) {
       sg_db=buffer;
      sg_db.append(L"\\SogouExplorer\\config.xml");
  }
  return sg_db;
}

void SGBrowserImporter::ImportHomepage() {

}
