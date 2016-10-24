#ifndef CHROME_BROWSER_IMPORTER_360SE_IMPORTER_H_
#define CHROME_BROWSER_IMPORTER_360SE_IMPORTER_H_

#include "base/files/file_path.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
//#include "chrome/common/importer/imported_favicon_usage.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "chrome/utility/importer/importer.h"

struct FavItem;

class SafeBrowserImporter : public Importer {
public:
  typedef std::vector<ImportedBookmarkEntry> BookmarkVector;

  enum Browser_Version {
    SE_VER_5,
    SE_VER_6,
    TW_VER_5,
  };

  explicit SafeBrowserImporter(Browser_Version version) 
      : browser_version_(version) {
  } 

  // Importer methods.
  virtual void StartImport(const importer::SourceProfile& profile_info,
                           uint16_t items,
                           ImporterBridge* bridge);

//   void ImportFavorites(const importer::SourceProfile& profile_info,
//                            uint16 items,const std::string& import_pref_type,
//                            ImporterBridge* bridge, BookmarkVector* bookmarks);


 private:
  virtual ~SafeBrowserImporter() {}

  void ImportFavorites();

  void ImportFavicons(
	  favicon_base::FaviconUsageDataList & favicons,
    std::vector<FavItem>& arrFavItem);

  void InitializeImportConditions();

  void ImportMostVisitedSE6(base::FilePath& profile_path);
  void ImportSearchEngines();
  void ImportHomepageAndStartupPage();
  
  // IE does not have source path. It's used in unit tests only for
  // providing a fake source.
  std::wstring source_path_;

	enum ImporterType{
		NONE_IMPORTER_TYPE,
		NET_IMPORTER_TYPE,
		LOCAL_IMPORTER_TYPE,
	};

	typedef std::pair<ImporterType,std::wstring> ImpoterResult;

	ImpoterResult Gen360seFilePath();
  std::wstring Gen360seDBFileName();
  // std::wstring Gen360seIniFileName();

  Browser_Version browser_version_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowserImporter);
};

#endif  // CHROME_BROWSER_IMPORTER_IE_IMPORTER_H_
