// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/importer_creator.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/utility/importer/bookmarks_file_importer.h"
#include "chrome/utility/importer/firefox_importer.h"

#if defined(OS_WIN)
#include "chrome/common/importer/edge_importer_utils_win.h"
#include "chrome/utility/importer/edge_importer_win.h"
#include "chrome/utility/importer/ie_importer_win.h"

#include "chrome/utility/importer/360se_importer.h"
#include "chrome/utility/importer/google_chrome_importer.h"
#include "chrome/utility/importer/html_importer.h"
#include "chrome/utility/importer/maxthon_importer.h"
#include "chrome/utility/importer/sg_importer.h"
#endif

#if defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>

#include "base/mac/foundation_util.h"
#include "chrome/utility/importer/safari_importer.h"
#endif

namespace importer {

scoped_refptr<Importer> CreateImporterByType(ImporterType type) {
  switch (type) {
#if defined(OS_WIN)
    case TYPE_IE:
      return new IEImporter();
    case TYPE_EDGE:
      // If legacy mode we pass back an IE importer.
      if (IsEdgeFavoritesLegacyMode())
        return new IEImporter();
      return new EdgeImporter();
	case TYPE_GOOGLE_CHROME:
		return new GoogleChromeImporter(GoogleChromeImporter::GOOGLE_CHROME);
		//case TYPE_360SE5:
		//	return new SafeBrowserImporter(SafeBrowserImporter::SE_VER_5);
	case TYPE_360SE6:
		return new SafeBrowserImporter(SafeBrowserImporter::SE_VER_6);
	case TYPE_THEWORLD5:
		return new SafeBrowserImporter(SafeBrowserImporter::TW_VER_5);
	case TYPE_360CHROME:
		return new GoogleChromeImporter(GoogleChromeImporter::CHROME_360);
	case TYPE_THEWORLDCHROME:
		return new GoogleChromeImporter(GoogleChromeImporter::CHROME_TW);
	case TYPE_LIEBAO:
		return new GoogleChromeImporter(GoogleChromeImporter::CHROME_LIEBAO);
	case TYPE_THEWORLD6:
		return new GoogleChromeImporter(GoogleChromeImporter::THEWORLD_6);
	case TYPE_CENT_BROWSER:
		return new GoogleChromeImporter(GoogleChromeImporter::CENT_BROWSER);
	case TYPE_7STAR_BROWSER:
		return new GoogleChromeImporter(GoogleChromeImporter::STAR7_BROWSER);
	case TYPE_SOGOU:
		return new SGBrowserImporter();
	case TYPE_MAXTHON:
		return new MaxthonBrowserImporter();
	case TYPE_LOCAL:
		return new GoogleChromeImporter(GoogleChromeImporter::TW_LOCAL);
#endif
	case TYPE_HTML:
		return new HtmlImporter();
    case TYPE_BOOKMARKS_FILE:
      return new BookmarksFileImporter();
    case TYPE_FIREFOX:
      return new FirefoxImporter();
#if defined(OS_MACOSX)
    case TYPE_SAFARI:
      return new SafariImporter(base::mac::GetUserLibraryPath());
#endif
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace importer
