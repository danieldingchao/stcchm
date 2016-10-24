// Copyright (c) 2010 qihoo.com
// Author zhangyong@qihoo.net

#ifndef CHROME_BROWSER_IMPORTER_SG_IMPORTER_H_
#define CHROME_BROWSER_IMPORTER_SG_IMPORTER_H_

#include "chrome/utility/importer/importer.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include <wininet.h>

#include <string>
#include <set>

#include "sql/connection.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"
#include "url/gurl.h"
#include "base/memory/ref_counted.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

#include "SgDBManager.h"
#include "sogou_explorer_hacker_new.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
//#include "360se_importer/bookmarks_utils/sogou_explorer_hacker_new.h"


namespace sql {
class Connection;
class Statement;
}

class SGBrowserImporter : public Importer {
public:
  SGBrowserImporter();
  explicit SGBrowserImporter(bool first_run);
  virtual ~SGBrowserImporter();

  typedef std::vector<ImportedBookmarkEntry> BookmarkVector;

  // Importer
  virtual void StartImport(const importer::SourceProfile& source_profile,
                           uint16_t items,
                           ImporterBridge* bridge) override;

  void ImportFavorites(const importer::SourceProfile& profile_info,
    uint16_t items, std::string &import_pref_type,
    ImporterBridge* bridge, BookmarkVector* bookmarks);

protected:
  bool LoadSgDB();
  void UnLoadSgDB();
  void GetEntrys(int parent_id, BookmarkVector& bookmarks, int index, std::vector<base::string16> path);
  void GetEntrysInner(int parent_id, BookmarkVector& bookmarks, std::vector<base::string16> path);
  PCHAR GetSGHomePage(wchar_t const * pszFile);
  std::wstring  GenSGConfigFilePath();

  void ImportFavorites();
  void ImportHomepage();

private:
  // ËÑ¹·Êý¾Ý¿â
  CSgDBManager m_sgDBMgr;
  bool first_run_;
  bool loaded_;

  DISALLOW_COPY_AND_ASSIGN(SGBrowserImporter);
};
#endif  // CHROME_BROWSER_IMPORTER_SG_IMPORTER_H_
