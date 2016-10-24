// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/importer_util.h"

#include "base/files/file_util.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/utility/importer/importer.h"
#include "chrome/common/importer/imported_favicon_usage.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "content/child/image_decoder.h"
#include "content/public/common/url_constants.h"
#include "net/base/data_url.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"

#if defined(USE_360HACK)
using namespace base;
#include "url/url_constants.h"
#endif

namespace importer {

const char kItemOpen[] = "<DT><A";
const char kItemClose[] = "</A>";
const char kFeedURLAttribute[] = "FEEDURL";
const char kHrefAttribute[] = "HREF";
const char kIconAttribute[] = "ICON";
const char kShortcutURLAttribute[] = "SHORTCUTURL";
const char kAddDateAttribute[] = "ADD_DATE";
const char kPostDataAttribute[] = "POST_DATA";
const char kCNTAttribute[] = "CNT";


bool StartsWithASCII(const std::string& str,
	const std::string& search,
	bool case_sensitive) {
	if (case_sensitive)
		return str.compare(0, search.length(), search) == 0;
	else
		return _strnicmp(str.c_str(), search.c_str(), search.length()) == 0;
	
}


//import/////////////////////////////////////////////////////////////////////
bool ParseCharsetFromLine(const std::string& line,
  std::string* charset){
    const char kCharset[] = "charset=";
	if (StartsWithASCII(line, "<META", false) &&
      (line.find("CONTENT=\"") != std::string::npos ||
      line.find("content=\"") != std::string::npos)) {
        size_t begin = line.find(kCharset);
        if (begin == std::string::npos)
          return false;
        begin += std::string(kCharset).size();
        size_t end = line.find_first_of('\"', begin);
        *charset = line.substr(begin, end - begin);
        return true;
    }
    return false;
}

void HTMLUnescape(string16* text){
  string16 text16 = *text;
  ReplaceSubstringsAfterOffset(
    &text16, 0, ASCIIToUTF16("&lt;"), ASCIIToUTF16("<"));
  ReplaceSubstringsAfterOffset(
    &text16, 0, ASCIIToUTF16("&gt;"), ASCIIToUTF16(">"));
  ReplaceSubstringsAfterOffset(
    &text16, 0, ASCIIToUTF16("&amp;"), ASCIIToUTF16("&"));
  ReplaceSubstringsAfterOffset(
    &text16, 0, ASCIIToUTF16("&quot;"), ASCIIToUTF16("\""));
  ReplaceSubstringsAfterOffset(
    &text16, 0, ASCIIToUTF16("&#39;"), ASCIIToUTF16("\'"));
  text->assign(text16);
}

bool GetAttribute(const std::string& attribute_list,
  const std::string& attribute,
  std::string* value){
    const char kQuote[] = "\"";

    size_t begin = attribute_list.find(attribute + "=" + kQuote);
    if (begin == std::string::npos)
      return false;  // Can't find the attribute.

    begin = attribute_list.find(kQuote, begin) + 1;

    size_t end = begin + 1;
    while (end < attribute_list.size()) {
      if (attribute_list[end] == '"' &&
        attribute_list[end - 1] != '\\') {
          break;
      }
      end++;
    }

    if (end == attribute_list.size())
      return false;  // The value is not quoted.

    *value = attribute_list.substr(begin, end - begin);
    return true;
}

void DataURLToFaviconUsage(
  const GURL& link_url,
  const GURL& favicon_data,
  favicon_base::FaviconUsageDataList* favicons){
    if (!link_url.is_valid() || !favicon_data.is_valid() ||
      !favicon_data.SchemeIs(url::kDataScheme))
      return;

    // Parse the data URL.
    std::string mime_type, char_set, data;
    if (!net::DataURL::Parse(favicon_data, &mime_type, &char_set, &data) ||
      data.empty())
      return;

	favicon_base::FaviconUsageData usage;
    // 搬到browser/importer去实现，先注释掉==
    //if (!importer::ReencodeFavicon(
    //  reinterpret_cast<const unsigned char*>(&data[0]),
    //  data.size(), &usage.png_data))
    //  return;  // Unable to decode.

    // We need to make up a URL for the favicon. We use a version of the page's
    // URL so that we can be sure it will not collide.
    usage.favicon_url = GURL(std::string("made-up-favicon:") + link_url.spec());

    // We only have one URL per favicon for Firefox 2 bookmarks.
    usage.urls.insert(link_url);

    favicons->push_back(usage);
}

bool ParseFolderNameFromLine(const std::string& line,
  const std::string& charset,
  string16* folder_name,
  bool* last_folder_name_valid,
  bool* is_toolbar_folder,
  base::Time* add_date,
  int *cnt){
    const char kFolderOpen[] = "<DT><H3";
    const char kFolderClose[] = "</H3>";
    const char kToolbarFolderAttribute[] = "PERSONAL_TOOLBAR_FOLDER";
    const char kAddDateAttribute[] = "ADD_DATE";

    if (!StartsWithASCII(line, kFolderOpen, true))
      return false;

    *last_folder_name_valid = false;
    size_t end = line.find(kFolderClose);
    size_t tag_end = line.rfind('>', end) + 1;
    // If no end tag or start tag is broken, we skip to find the folder name.
    if (end == std::string::npos || tag_end < arraysize(kFolderOpen))
      return false;

    *cnt = 0;

    base::CodepageToUTF16(line.substr(tag_end, end - tag_end), charset.c_str(),
      base::OnStringConversionError::SKIP, folder_name);
    HTMLUnescape(folder_name);

    std::string attribute_list = line.substr(arraysize(kFolderOpen),
      tag_end - arraysize(kFolderOpen) - 1);
    std::string value;

    // Add date
    if (GetAttribute(attribute_list, kAddDateAttribute, &value)) {
      int64_t time;
      base::StringToInt64(value, &time);
      // Upper bound it at 32 bits.
      if (0 < time && time < (1LL << 32))
        *add_date = base::Time::FromTimeT(time);
    }

    if (GetAttribute(attribute_list, kToolbarFolderAttribute, &value) &&
      LowerCaseEqualsASCII(value, "true"))
      *is_toolbar_folder = true;//lwg temp modify
    else
      *is_toolbar_folder = false;

    *last_folder_name_valid = true;
    return true;
}

bool ParseBookmarkFromLine(const std::string& line,
  const std::string& charset,
  string16* title,
  GURL* url,
  GURL* favicon,
  string16* shortcut,
  base::Time* add_date,
  string16* post_data,
  int *cnt){
    title->clear();
    *url = GURL();
    *favicon = GURL();
    shortcut->clear();
    post_data->clear();
    *add_date = base::Time();

    if (!StartsWithASCII(line, kItemOpen, true))
      return false;

    size_t end = line.find(kItemClose);
    size_t tag_end = line.rfind('>', end) + 1;
    if (end == std::string::npos || tag_end < arraysize(kItemOpen))
      return false;  // No end tag or start tag is broken.

    *cnt = 0;
    std::string attribute_list = line.substr(arraysize(kItemOpen),
      tag_end - arraysize(kItemOpen) - 1);

    // We don't import Live Bookmark folders, which is Firefox's RSS reading
    // feature, since the user never necessarily bookmarked them and we don't
    // have this feature to update their contents.
    std::string value;
    if (GetAttribute(attribute_list, kFeedURLAttribute, &value))
      return false;

    // Title
    base::CodepageToUTF16(line.substr(tag_end, end - tag_end), charset.c_str(),
      base::OnStringConversionError::SKIP, title);
    HTMLUnescape(title);

    // URL
    if (GetAttribute(attribute_list, kHrefAttribute, &value)) {
      string16 url16;
      base::CodepageToUTF16(value, charset.c_str(),
        base::OnStringConversionError::SKIP, &url16);
      HTMLUnescape(&url16);

      *url = GURL(url16);
    }

    // Favicon
    if (GetAttribute(attribute_list, kIconAttribute, &value))
      *favicon = GURL(value);

    // Keyword
    if (GetAttribute(attribute_list, kShortcutURLAttribute, &value)) {
      base::CodepageToUTF16(value, charset.c_str(),
        base::OnStringConversionError::SKIP, shortcut);
      HTMLUnescape(shortcut);
    }

    // Add date
    if (GetAttribute(attribute_list, kAddDateAttribute, &value)) {
      int64_t time;
      base::StringToInt64(value, &time);
      // Upper bound it at 32 bits.
      if (0 < time && time < (1LL << 32))
        *add_date = base::Time::FromTimeT(time);
    }

    // Post data.
    if (GetAttribute(attribute_list, kPostDataAttribute, &value)) {
      base::CodepageToUTF16(value, charset.c_str(),
        base::OnStringConversionError::SKIP, post_data);
      HTMLUnescape(post_data);
    }

    //CNT
    if (GetAttribute(attribute_list, kCNTAttribute, &value)) {
      if(value == "1")
        *cnt = 1;
      else if(value == "2")
        *cnt = 2;
    }
    return true;
}

bool ParseMinimumBookmarkFromLine(const std::string& line,
  const std::string& charset,
  string16* title,
  GURL* url){
    const char kItemOpen[] = "<DT><A";
    const char kItemClose[] = "</";
    const char kHrefAttributeUpper[] = "HREF";
    const char kHrefAttributeLower[] = "href";

    title->clear();
    *url = GURL();

    // Case-insensitive check of open tag.
    if (!StartsWithASCII(line, kItemOpen, false))
      return false;

    // Find any close tag.
    size_t end = line.find(kItemClose);
    size_t tag_end = line.rfind('>', end) + 1;
    if (end == std::string::npos || tag_end < arraysize(kItemOpen))
      return false;  // No end tag or start tag is broken.

    std::string attribute_list = line.substr(arraysize(kItemOpen),
      tag_end - arraysize(kItemOpen) - 1);

    // Title
    base::CodepageToUTF16(line.substr(tag_end, end - tag_end), charset.c_str(),
      base::OnStringConversionError::SKIP, title);
    HTMLUnescape(title);

    // URL
    std::string value;
    if (GetAttribute(attribute_list, kHrefAttributeUpper, &value) ||
      GetAttribute(attribute_list, kHrefAttributeLower, &value)) {
        if (charset.length() != 0) {
          string16 url16;
          base::CodepageToUTF16(value, charset.c_str(),
            base::OnStringConversionError::SKIP, &url16);
          HTMLUnescape(&url16);

          *url = GURL(url16);
        } else {
          *url = GURL(value);
        }
    }

    return true;
}

bool CanImport(const GURL& url){
  const char* kInvalidSchemes[] = {"wyciwyg", "place", "about", "chrome"};

  // The URL is not valid.
  if (!url.is_valid())
    return false;

  // Filter out the URLs with unsupported schemes.
  for (size_t i = 0; i < arraysize(kInvalidSchemes); ++i) {
    if (url.SchemeIs(kInvalidSchemes[i]))
      return false;
  }

  return true;
}

void ImportBookmarksFromFile(
  const base::FilePath& file_path,
  const std::set<GURL>& default_urls,
  Importer* importer,
  std::vector<ImportedBookmarkEntry>* bookmarks,
  std::vector<TemplateURL*>* template_urls,
  favicon_base::FaviconUsageDataList* favicons){
    std::string content;
    base::ReadFileToString(file_path, &content);

    // [6/9/2011 ken]
    std::string default_charset = "gb2312";
    if(content.size() > 3) {
      if( ( (uint8_t)content[0] == 0xef || (uint8_t)content[0] == 0xEF) &&
        ((uint8_t)content[1] == 0xbb || (uint8_t)content[1] == 0xBB) &&
        ((uint8_t)content[2] == 0xbf || (uint8_t)content[2] == 0xBF) ) {
          default_charset = "UTF-8";
      }
    }

    std::vector<std::string> lines;
	std::string separators("\n");
	lines = base::SplitString(content, separators, base::KEEP_WHITESPACE, SplitResult::SPLIT_WANT_ALL);

    string16 last_folder_name;
    bool last_folder_name_valid = false;
    bool last_folder_on_toolbar = false;
    bool last_folder_is_empty = true;
    bool has_subfolder = false;
    base::Time last_folder_add_date;
    std::vector<string16> path;
    size_t toolbar_folder = 0;
    std::string charset;
    int cnt = 0;
    // [6/9/2011 ken]
    for (size_t i = 0; i < lines.size() && (!importer || !importer->cancelled()) && i < 10;
      ++i) {
        std::string line;
        TrimString(lines[i], " ", &line);
        if (ParseCharsetFromLine(line, &charset))
          break;
    }

    // [6/1/2011 ken]
    if(charset.size() == 0)
      charset = default_charset;


    for(size_t i = 0; i < lines.size() && (!importer || !importer->cancelled());
      ++i) {
        std::string line;
        TrimString(lines[i], " ", &line);

        // Get the encoding of the bookmark file.
        std::string charset_notuse;
        if (ParseCharsetFromLine(line, &charset_notuse))
          continue;

        // Get the folder name.
        if (ParseFolderNameFromLine(line, charset, &last_folder_name,
          &last_folder_name_valid,
          &last_folder_on_toolbar,
          &last_folder_add_date,
          &cnt))
          continue;

        // Get the bookmark entry.
        string16 title;
        string16 shortcut;
        GURL url, favicon;
        base::Time add_date;
        string16 post_data;
        bool is_bookmark;
        // TODO(jcampan): http://b/issue?id=1196285 we do not support POST based
        //                keywords yet.
        is_bookmark = ParseBookmarkFromLine(line, charset, &title,
          &url, &favicon, &shortcut, &add_date,
          &post_data, &cnt) ||
          ParseMinimumBookmarkFromLine(line, charset, &title, &url);

        //if (is_bookmark)
        //  last_folder_is_empty = false;

        if (is_bookmark &&
          post_data.empty() &&
          CanImport(GURL(url)) &&
          default_urls.find(url) == default_urls.end()) {
            last_folder_is_empty = false;
            if (toolbar_folder > path.size() && !path.empty()) {
              NOTREACHED();  // error in parsing.
              break;
            }

            ImportedBookmarkEntry entry;
            entry.creation_time = add_date;
            entry.url = url;
            entry.title = title;
            // [6/2/2011 ken]
            entry.in_toolbar = true;

            if (toolbar_folder) {
              // The toolbar folder should be at the top level.
              entry.in_toolbar = true;
              entry.path.assign(path.begin() + toolbar_folder /*- 1*/, path.end());
            } else {
              // Add this bookmark to the list of |bookmarks|.
              if (!has_subfolder && last_folder_name_valid) {
                path.push_back(last_folder_name);
                last_folder_name_valid = false;
              }
              entry.path.assign(path.begin(), path.end());
            }
            bookmarks->push_back(entry);
            if(favicons)
              DataURLToFaviconUsage(url, favicon, favicons);
            continue;
        }

        // Bookmarks in sub-folder are encapsulated with <DL> tag.
        if (StartsWithASCII(line, "<DL>", false)) {
          has_subfolder = true;
          if (last_folder_name_valid) {
            path.push_back(last_folder_name);
            last_folder_name_valid = false;
          }
          if (last_folder_on_toolbar && !toolbar_folder)
            toolbar_folder = path.size();

          // Mark next folder empty as initial state.
          last_folder_is_empty = true;
        } else if (StartsWithASCII(line, "</DL>", false)) {
          if (path.empty())
            break;  // Mismatch <DL>.

          string16 folder_title = path.back();
          path.pop_back();

          if (last_folder_is_empty) {
            // Empty folder should be added explicitly.
            ImportedBookmarkEntry entry;
            entry.is_folder = true;
            entry.creation_time = last_folder_add_date;
            entry.title = folder_title;
            entry.in_toolbar = true;
            if (toolbar_folder) {
              // The toolbar folder should be at the top level.
              // Make sure we don't add the toolbar folder itself if it is empty.
              if (toolbar_folder <= path.size()) {
                entry.in_toolbar = true;
                entry.path.assign(path.begin() + toolbar_folder /*- 1*/, path.end());
                bookmarks->push_back(entry);
              }
            } else {
              // Add this folder to the list of |bookmarks|.
              entry.path.assign(path.begin(), path.end());
              bookmarks->push_back(entry);
            }

            // Parent folder include current one, so it's not empty.
            last_folder_is_empty = false;
          }

          if (toolbar_folder > path.size())
            toolbar_folder = 0;
        }
    }
}

}  // namespace importer
