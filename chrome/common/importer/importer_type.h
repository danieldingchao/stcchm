// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IMPORTER_IMPORTER_TYPE_H_
#define CHROME_COMMON_IMPORTER_IMPORTER_TYPE_H_

#include "build/build_config.h"

namespace importer {

// An enumeration of the type of importers that we support to import
// settings and data from (browsers, google toolbar and a bookmarks html file).
// NOTE: Numbers added so that data can be reliably cast to ints and passed
// across IPC.
enum ImporterType {
  TYPE_UNKNOWN         = -1,
#if defined(OS_WIN)
  TYPE_IE              = 0,
#endif
  // Value 1 was the (now deleted) Firefox 2 profile importer.
  TYPE_FIREFOX         = 2,
#if defined(OS_MACOSX)
  TYPE_SAFARI          = 3,
#endif
  // Value 4 was the (now deleted) Google Toolbar importer.
  TYPE_BOOKMARKS_FILE  = 5, // Identifies a 'bookmarks.html' file.
#if defined(OS_WIN)
  TYPE_EDGE            = 6,
  TYPE_THEWORLD5 = 7,
  TYPE_THEWORLDCHROME = 8,
  TYPE_GOOGLE_CHROME = 9,
  TYPE_360SE5 = 10,
  TYPE_360SE6 = 11,
  TYPE_360CHROME = 12,
  TYPE_LIEBAO = 13,
  TYPE_SOGOU = 14,
  TYPE_MAXTHON = 15,
  TYPE_THEWORLD3 = 16,
  TYPE_THEWORLD6 = 17,
  TYPE_CENT_BROWSER = 18,
  TYPE_7STAR_BROWSER = 19,
  TYPE_LOCAL = 20,
  TYPE_HTML = 21,
#endif
};

}  // namespace importer


#endif  // CHROME_COMMON_IMPORTER_IMPORTER_TYPE_H_
