// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_UTIL_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_UTIL_H_

#include <cstddef>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "chrome/browser/importer/profile_writer.h"
#include "url/gurl.h"

#include "chrome/common/importer/imported_favicon_usage.h"

using namespace base;
class Importer;
#define INTERNET_MAX_URL_LENGTH_LARGE 8192

namespace importer {
// From HTML to bookmark entry.
bool ParseCharsetFromLine(const std::string& line,
  std::string* charset);

void HTMLUnescape(string16* text);

bool GetAttribute(const std::string& attribute_list,
  const std::string& attribute,
  std::string* value);

bool ParseFolderNameFromLine(const std::string& line,
  const std::string& charset,
  string16* folder_name,
  bool* last_folder_name_valid,
  bool* is_toolbar_folder,
  base::Time* add_date,
  int *cnt);

bool ParseBookmarkFromLine(const std::string& line,
  const std::string& charset,
  string16* title,
  GURL* url,
  GURL* favicon,
  string16* shortcut,
  base::Time* add_date,
  string16* post_data,
  int *cnt);

bool ParseMinimumBookmarkFromLine(const std::string& line,
  const std::string& charset,
  string16* title,
  GURL* url);

void DataURLToFaviconUsage(
  const GURL& link_url,
  const GURL& favicon_data,
  std::vector<ImportedFaviconUsage>* favicons);

bool CanImport(const GURL& url);

void ImportBookmarksFromFile(
  const base::FilePath& file_path,
  const std::set<GURL>& default_urls,
  Importer* importer,
  std::vector<ImportedBookmarkEntry>* bookmarks,
  std::vector<TemplateURL*>* template_urls,
  favicon_base::FaviconUsageDataList* favicons);

}  // namespace importer

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_UTIL_H_
