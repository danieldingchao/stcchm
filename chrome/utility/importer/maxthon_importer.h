// Copyright (c) 2010 qihoo.com
// Author zhangyong@qihoo.net

#ifndef CHROME_BROWSER_IMPORTER_MAXTHON_IMPORTER_H_
#define CHROME_BROWSER_IMPORTER_MAXTHON_IMPORTER_H_

//#include "chrome/browser/importer/importer.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

#include <string>
#include <set>

#include "sql/connection.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"
#include "base/memory/ref_counted.h"
#include "chrome/utility/importer/importer.h"
#include "chrome/utility/importer/MaxthonDBManager.h"
#include "url/gurl.h"

namespace sql {
  class Connection;
  class Statement;
}

class MaxthonBrowserImporter : public Importer {

public:

  MaxthonBrowserImporter();
  virtual ~MaxthonBrowserImporter() {}
  // Importer methods.
  virtual void StartImport(const importer::SourceProfile& browser_info,
    uint16_t items,
    ImporterBridge* bridge);


private:
  void ImportFavorites();

  enum ImporterType{
    NONE_IMPORTER_TYPE,
    NET_IMPORTER_TYPE,
    LOCAL_IMPORTER_TYPE,
  };
  int ImportMaxthon();
  void WriteFolderToHtmlMaxthon(unsigned char * pszPID, FILE * fp, int nIndex);
  int GetAllItemsMaxthon(unsigned char * pszPID, std::vector<FavItemEx> & vecItems);

  // 傲游数据库
  CMaxthonDBManager m_MXDBMgr;
  // 要导出的HTML文件路径
  CTString m_strDesHtmlPath;

  typedef std::pair<ImporterType,std::wstring> ImpoterResult;

  DISALLOW_COPY_AND_ASSIGN(MaxthonBrowserImporter);
};
#endif  // CHROME_BROWSER_IMPORTER_MAXTHON_IMPORTER_H_
