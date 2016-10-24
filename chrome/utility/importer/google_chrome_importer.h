// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_GOOGLE_CHROME_IMPORTER_H_
#define CHROME_BROWSER_IMPORTER_GOOGLE_CHROME_IMPORTER_H_

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/utility/importer/importer.h"

class PersonalDataManager;
namespace base {
  class Value;
}

class GoogleChromeImporter : public Importer {
public:
  enum Chrome_Type {
    GOOGLE_CHROME,
    CHROME_360,
    CHROME_LIEBAO,
    CHROME_TW,
	THEWORLD_6,
	CENT_BROWSER,
	STAR7_BROWSER,
    TW_LOCAL,
  };
  enum ChromeVersion {
    SameMajorVersion = 0,  // Major version is the same.
    NewerMajorVersion,     // Major version is one version newer.
    NotSupportedVersion,
  };
  typedef std::vector<ImportedBookmarkEntry> BookmarkVector;
  typedef std::map<std::string, base::Value*> PrefsMap;


  GoogleChromeImporter(Chrome_Type chrome_type);

  // Importer methods.
  virtual void StartImport(const importer::SourceProfile& profile_info,
    uint16_t items,
    ImporterBridge* bridge);


private:
  //FRIEND_TEST(ImporterTest, GoogleChromeImporter);
  Chrome_Type chrome_type_;
  void InitPaths();
  virtual ~GoogleChromeImporter() {
  }

  void ImportFavorites();
  void ImportPasswords();
  void ImportSearchEngines();
  void ImportFavicons();
  void ImportHomepageAndStartupPage();
  void LoadProfileName(const base::FilePath& local_state, std::wstring& profile_name, PrefsMap& prefs_map);
  bool CanImport();

  void ImportMostVisitedEE();
  void ImportMostVisitedWorldChromeAndGoogleChrome();

  base::FilePath user_data_;
  base::FilePath profile_path_;
  std::wstring chrome_path_;
  std::wstring homepage_str_;
  std::auto_ptr<base::Value> root_;
  std::auto_ptr<base::Value> local_state_;

  PrefsMap prefs_map_;
  PrefsMap exts_map_;
  PrefsMap local_prefs_;

  DISALLOW_COPY_AND_ASSIGN(GoogleChromeImporter);
};

#endif  // CHROME_BROWSER_IMPORTER_GOOGLE_CHROME_IMPORTER_H_
