// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/utility/importer/google_chrome_importer.h"

#if defined(OS_WIN)
#include <Psapi.h>
#endif

#include "base/files/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/base_paths.h"
#include "base/environment.h"
#include "base/json/json_file_value_serializer.h"
#include "base/strings/utf_string_conversions.h"
#if defined(OS_WIN)
#include <strsafe.h>
#include <ShlObj.h>
#endif
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/importer/imported_favicon_usage.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/utility/importer/google_chrome_bookmark_util.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "ui/base/l10n/l10n_util.h"
#include "grit/components_strings.h"
#include "components/search_engines/search_engines_pref_names.h"

#if defined(OS_LINUX)
#include "base/nix/xdg_util.h"
#endif

using content::BrowserThread;
using importer::BookmarkNode;
using importer::BookmarkCodec;

namespace {
#if defined(OS_WIN)
  bool IsGoogleChromeRunning(const std::wstring &chrome_exe)
  {
    DWORD processid[1024] = {0};
    DWORD needed = 0;
    DWORD processcount = 0;
    HANDLE hProcess = NULL;

    EnumProcesses(processid, sizeof(processid), &needed);
    processcount = needed / sizeof(DWORD);

    hProcess = NULL;
    for (DWORD i = 0; i<processcount; i++) {
      if(NULL != hProcess)
      {
        CloseHandle(hProcess);
        hProcess = NULL;
      }

      hProcess = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, FALSE, processid[i]);
      if(NULL == hProcess)
      {
        continue;
      }

      TCHAR path[MAX_PATH] = {0};
      GetModuleFileNameEx(hProcess, NULL, path, _countof(path));
      if (0 == chrome_exe.compare(path))
        return true;
    }
    return false;
  }
#endif

  class MobileNode : public BookmarkNode {
  public:
    explicit MobileNode(int64_t id);
    virtual ~MobileNode();

    void set_visible(bool value) { visible_ = value; }

    // BookmarkNode overrides:
    virtual bool IsVisible() const OVERRIDE;

  private:
    bool visible_;

    DISALLOW_COPY_AND_ASSIGN(MobileNode);
  };

  MobileNode::MobileNode(int64_t id)
    : BookmarkNode(id, GURL()),
    visible_(false) {
  }

  MobileNode::~MobileNode() {
  }

  bool MobileNode::IsVisible() const {
    return visible_ || !empty();
  }

  BookmarkNode* CreatePermanentNode(BookmarkNode::Type type, int64_t id) {
    DCHECK(type == BookmarkNode::BOOKMARK_BAR ||
      type == BookmarkNode::OTHER_NODE ||
      type == BookmarkNode::MOBILE);
    BookmarkNode* node;
    if (type == BookmarkNode::MOBILE) {
      node = new MobileNode(id);
      node->SetTitle(
        l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_MOBILE_FOLDER_NAME));
    } else {
      node = new BookmarkNode(id, GURL());
      if (type == BookmarkNode::BOOKMARK_BAR) {
        node->SetTitle(l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_FOLDER_NAME));
      } else if (type == BookmarkNode::OTHER_NODE) {
        node->SetTitle(
          l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_OTHER_FOLDER_NAME));
      } else {
        NOTREACHED();
      }
    }
    node->set_type(type);
    return node;
  }

  void AddBookmarksToVector(std::vector<ImportedBookmarkEntry>& bookmarks,
    BookmarkNode* node,std::vector<base::string16> path, bool in_toolbar, bool first = true) {
      if (node->is_url()) {
        if (node->url().is_valid()) {
          ImportedBookmarkEntry entry;
          entry.in_toolbar = in_toolbar;
          entry.title = node->GetTitle();
          entry.url = node->url();
          entry.creation_time = node->date_added();
          entry.path = path;
          entry.is_folder = node->is_folder();
          bookmarks.push_back(entry);
        }
      }
      else {
        if (first) {
          #if defined(OS_WIN)
          if ((node->GetTitle() != L"收藏栏") && (node->GetTitle() != L"Bookmarks bar")
            && (node->GetTitle() != L"其他收藏") && (node->GetTitle() != L"Other bookmarks"))
            path.push_back(node->GetTitle());
          #endif
        }
        else
          path.push_back(node->GetTitle());

        first = false;

        for (int i = 0; i < node->child_count(); ++i) {
          AddBookmarksToVector(bookmarks, node->GetChild(i), path, in_toolbar, first);
        }
      }
  }

  void LoadBookmarksFile(const base::FilePath& path,
      importer::BookmarkNode* bb_node,
      importer::BookmarkNode* other_node,
      importer::BookmarkNode* mobile_node) {
    JSONFileValueDeserializer deserializer(path);
    std::unique_ptr<Value> root(deserializer.Deserialize(NULL,NULL));
    if (root.get()) {
		int64_t max_node_id = 0;
      importer::BookmarkCodec codec;

      const Value* root_node = root.get();
      codec.Decode(bb_node, other_node, mobile_node, &max_node_id,*root_node);
    }
  }
}

GoogleChromeImporter::GoogleChromeImporter(Chrome_Type chrome_type) 
    : chrome_type_(chrome_type) {
}

void GoogleChromeImporter::StartImport(const importer::SourceProfile& profile_info, uint16_t items, ImporterBridge* bridge) {
  bridge_ = bridge;
#if defined(OS_WIN)
  chrome_path_ = profile_info.source_path;
#endif
  InitPaths();

  bridge_->NotifyStarted();

  if ((items & importer::HOME_PAGE) && !cancelled())
    ImportHomepageAndStartupPage();

  if ((items & importer::FAVORITES) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::FAVORITES);
    ImportFavorites();
    bridge_->NotifyItemEnded(importer::FAVORITES);
  }

  if ((items & importer::SEARCH_ENGINES) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::SEARCH_ENGINES);
    ImportSearchEngines();
    bridge_->NotifyItemEnded(importer::SEARCH_ENGINES);
  }

  if ((items & importer::PASSWORDS) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::PASSWORDS);
    ImportPasswords();
    bridge_->NotifyItemEnded(importer::PASSWORDS);
  }

  if ((items & importer::MOST_VISITED) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::MOST_VISITED);
    if ( CHROME_360 == chrome_type_)
      ImportMostVisitedEE();
    else if ( CHROME_TW == chrome_type_ || GOOGLE_CHROME == chrome_type_)
      ImportMostVisitedWorldChromeAndGoogleChrome();
    bridge_->NotifyItemEnded(importer::MOST_VISITED);
  }

  bridge_->NotifyEnded();
}

//////////////////////////////////////////////////////////////////////////
// Private Methods.
bool GoogleChromeImporter::CanImport() {
#if defined(OS_WIN)
  if (IsGoogleChromeRunning(chrome_path_.value())) {
    return false;
  }
  else {
    return true;
  }
#else
  return true;
#endif
}

void GoogleChromeImporter::ImportHomepageAndStartupPage() {
  base::FilePath path = profile_path_.Append(chrome::kPreferencesFilename);
  if (!base::PathExists(path))
    return;

  JSONFileValueDeserializer deserializer(path);
  root_.reset(deserializer.Deserialize(NULL, NULL).release());
  if (!root_.get())
    return;

  Value* root_node = root_.get();
  if (root_node->GetType() != Value::TYPE_DICTIONARY)
    return;

  DictionaryValue& d_value = static_cast<DictionaryValue&>(*root_node);

  std::string homepage;

  // Load Homepage.
  Value* home_page_value;
  if (d_value.Get(prefs::kHomePage, &home_page_value)) {
    if (home_page_value->GetType() == Value::TYPE_STRING) {
      StringValue* home_page_str = static_cast<StringValue*>(home_page_value);
      home_page_str->GetAsString(&homepage);
      if (!homepage.empty())
        bridge_->AddHomePage(GURL(homepage));
    }
  }

  // Load Homepage.
  Value* startup_page_value;
  if (d_value.Get(prefs::kURLsToRestoreOnStartup, &startup_page_value)) {
    if (startup_page_value->GetType() == Value::TYPE_LIST) {
      ListValue* list = static_cast<ListValue*>(startup_page_value);
      if (!list->empty()) {
        Value* home_page_value;
        if (list->Get(0, &home_page_value) && home_page_value->GetType() == Value::TYPE_STRING) {
          StringValue* home_page_str = static_cast<StringValue*>(home_page_value);
          home_page_str->GetAsString(&homepage);
          if (!homepage.empty()) 
            bridge_->AddStartupPage(GURL(homepage));
        }
      }
    }
  }

}

void GoogleChromeImporter::ImportSearchEngines() {
  base::FilePath path = profile_path_.Append(chrome::kPreferencesFilename);
  if (!base::PathExists(path))
    return;

  JSONFileValueDeserializer deserializer(path);
  root_.reset(deserializer.Deserialize(NULL, NULL).release());
  if (!root_.get())
    return;

  Value* root_node = root_.get();
  if (root_node->GetType() != Value::TYPE_DICTIONARY)
    return;

  DictionaryValue& d_value = static_cast<DictionaryValue&>(*root_node);

  Value* default_search_url_value;
  std::string url;

  if (d_value.Get(prefs::kDefaultSearchProviderSearchURL, &default_search_url_value)) {
    if (default_search_url_value->GetType() == Value::TYPE_STRING) {
      StringValue* default_search_url = static_cast<StringValue*>(default_search_url_value);
      default_search_url->GetAsString(&url);
      if (!url.empty()) {
        bridge_->SetDefaultSearchEngine(ASCIIToUTF16(url), ASCIIToUTF16(""));
      }
    }
  }
}

void GoogleChromeImporter::ImportFavicons() {
  base::FilePath favicon_path;

  favicon_path = profile_path_.Append(chrome::kFaviconsFilename);

  base::FilePath tmp_path;
  PathService::Get(base::DIR_TEMP, &tmp_path);
  tmp_path = tmp_path.Append(FILE_PATH_LITERAL("lemon_import_icon_tmp.db"));
  base::CopyFile(favicon_path, tmp_path);
  
  sql::Connection favicon_db;
  favicon_db.set_page_size(4096);
  favicon_db.set_cache_size(6000);

  bool open_did_succeed = favicon_db.Open(tmp_path);
  if (!open_did_succeed)
    return;

  sql::Statement s;

  if(favicon_db.DoesTableExist("icon_mapping") && favicon_db.DoesTableExist("favicon_bitmaps")) {
  s.Assign(favicon_db.GetUniqueStatement("SELECT page_url, image_data "
    "FROM icon_mapping join favicon_bitmaps on icon_mapping.icon_id = favicon_bitmaps.icon_id"));
  }
  // 旧版的icon存储方式，猎豹，twchrome采用此种方式
  else {
    // 表不存在，返回
    if(!favicon_db.DoesTableExist("favicons") || !favicon_db.DoesTableExist("icon_mapping"))
      return;
    s.Assign(favicon_db.GetUniqueStatement("SELECT page_url, image_data "
      "FROM icon_mapping join favicons on icon_mapping.icon_id = favicons.id"));
  }

  favicon_base::FaviconUsageDataList favicons;
  while (s.Step()) {
	favicon_base::FaviconUsageData usage;
    usage.urls.insert(GURL(s.ColumnString(0)));
    usage.favicon_url = GURL(s.ColumnString(0));
    s.ColumnBlobAsVector(1, &usage.png_data);
    favicons.push_back(usage);
  }
  base::DeleteFile(tmp_path, false);
  if(!favicons.empty())
    bridge_->SetFavicons(favicons);
}
#if defined(OS_WIN)
base::FilePath get360ChromeBookmarkPath(const base::FilePath& profile) {
  base::FilePath bookmark_path = profile.Append(chrome::kBookmarksFileName);
  base::FileEnumerator temp_traversal(profile,
    false,  // recursive
    base::FileEnumerator::DIRECTORIES);
  for (base::FilePath current = temp_traversal.Next(); !current.empty();
    current = temp_traversal.Next()) {
    if (current.BaseName().value().find(L"360UID") == 0){
      base::FilePath path = current.Append(chrome::kBookmarksFileName);
      if (base::PathExists(path)) {
        bookmark_path = path;
      }
      break;
    }
  }
  return  bookmark_path;
}

base::FilePath getTW6BookmarkPath(const base::FilePath& profile) {
  base::FilePath bookmark_path = profile.Append(chrome::kBookmarksFileName);
  base::FileEnumerator temp_traversal(profile,
    false,  // recursive
    base::FileEnumerator::DIRECTORIES);
  for (base::FilePath current = temp_traversal.Next(); !current.empty();
    current = temp_traversal.Next()) {
    if (current.BaseName().value().length() == 32) {
      base::FilePath path = current.Append(chrome::kBookmarksFileName);
      if (base::PathExists(path)) {
        bookmark_path = path;
        break;
      }
    }
  }
  return  bookmark_path;
}
#endif

void GoogleChromeImporter::ImportFavorites() {
  base::FilePath bookmark_path;
#if defined(OS_WIN)
  if (chrome_type_ == TW_LOCAL)
    bookmark_path = bookmark_path.FromUTF16Unsafe(chrome_path_.value());
  else if (chrome_type_ == CHROME_360) {
    bookmark_path = get360ChromeBookmarkPath(profile_path_);
  } else if (chrome_type_ == THEWORLD_6){
    bookmark_path = getTW6BookmarkPath(profile_path_);
  } else {
    bookmark_path = profile_path_.Append(chrome::kBookmarksFileName);
  }
#else
  bookmark_path = profile_path_.Append(chrome::kBookmarksFileName);
#endif

  if (!base::PathExists(bookmark_path))
    return;

  base::FilePath tmp_path;
#if defined(OS_WIN)
  wchar_t tmp[MAX_PATH];
  GetTempPath(MAX_PATH, tmp);
  tmp_path = base::FilePath(tmp);
#else
  PathService::Get(base::DIR_TEMP, &tmp_path);
#endif
  tmp_path = tmp_path.Append(FILE_PATH_LITERAL("lemon_bookmarks_tmp"));
  base::CopyFile(bookmark_path, tmp_path);

  int id = 0;
  std::auto_ptr<BookmarkNode> bb_node(CreatePermanentNode(BookmarkNode::BOOKMARK_BAR, id++));
  std::auto_ptr<BookmarkNode> other_node(CreatePermanentNode(BookmarkNode::OTHER_NODE, id++));
  std::auto_ptr<BookmarkNode> mobile_node(CreatePermanentNode(BookmarkNode::MOBILE, id++));


  LoadBookmarksFile(tmp_path, bb_node.get(), other_node.get(), mobile_node.get());
  base::DeleteFile(tmp_path, false);

  std::vector<base::string16> path;

  BookmarkVector other_bookmarks;
  AddBookmarksToVector(other_bookmarks, other_node.get(), path, false);

  BookmarkVector bookmarks;
  AddBookmarksToVector(bookmarks, bb_node.get(), path, true);


  if ((!bookmarks.empty()) && !cancelled()) {
#if defined(OS_WIN)
    string16 first_folder_name = L"从 Chrome 导入";
    if(CHROME_360 == chrome_type_)
      first_folder_name = L"从 360极速 导入";
    else if(CHROME_TW == chrome_type_)
      first_folder_name = L"从 世界之窗极速版 导入";
	else if (THEWORLD_6 == chrome_type_)
		first_folder_name = L"从 世界之窗6 导入";
    else if(CHROME_LIEBAO == chrome_type_)
      first_folder_name = L"从 猎豹 导入";
	else if (CENT_BROWSER == chrome_type_)
		first_folder_name = L"从 Cent浏览器 导入";
	else if (STAR7_BROWSER == chrome_type_)
		first_folder_name = L"从 七星浏览器 导入";
#else
   string16 first_folder_name = ASCIIToUTF16("Import from Google Chrome");
#endif
      // l10n_util::GetStringUTF16(IDS_BOOKMARK_GROUP_FROM_GOOGLE_CHROME);
    bridge_->AddBookmarks(bookmarks, first_folder_name);
    bridge_->AddBookmarks(other_bookmarks, first_folder_name, 
        importer::BOOKMARK_OTHER_NODE);
  }
  ImportFavicons();
}

void GoogleChromeImporter::ImportPasswords() {
  base::FilePath login_file_path = profile_path_.Append(chrome::kLoginDataFileName);
  if (!base::PathExists(login_file_path))
    return;

  // copy the original login data
  base::FilePath tmp_path;
  PathService::Get(base::DIR_TEMP, &tmp_path);
  tmp_path = tmp_path.Append(FILE_PATH_LITERAL("lemon_import_login_tmp.db"));
  base::CopyFile(login_file_path, tmp_path);

  importer::LoginDatabase login_db;
  if (!login_db.Init(tmp_path))
    return;

  std::vector<autofill::PasswordForm*> forms;
  login_db.GetAutofillableLogins(&forms);
  login_db.GetBlacklistLogins(&forms);
  base::DeleteFile(tmp_path, false);

  if (!cancelled()) {
    for (size_t i = 0; i < forms.size(); ++i) {
      bridge_->SetPasswordForm(*forms[i]);
    }
  }

  for (size_t i = 0; i < forms.size(); ++i) {
    delete forms[i];
    forms[i] = NULL;
  }
}

#if defined(OS_LINUX)
using base::nix::GetXDGDirectory;
using base::nix::GetXDGUserDirectory;
using base::nix::kDotConfigDir;
using base::nix::kXdgConfigHomeEnvVar;
#endif

void GoogleChromeImporter::InitPaths() {
#if defined(OS_WIN)
  wchar_t buffer[MAX_PATH] = {0};
  if(CHROME_360 == chrome_type_ || CHROME_TW == chrome_type_ || THEWORLD_6 == chrome_type_) {
    user_data_ = base::FilePath(chrome_path_).DirName().DirName().
        Append(chrome::kUserDataDirname);
  }

  else if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL,
    SHGFP_TYPE_CURRENT, buffer))) {
      std::wstring path(buffer);
      if(GOOGLE_CHROME == chrome_type_)
        path.append(L"\\Google\\Chrome\\");
      else if(CHROME_LIEBAO == chrome_type_)
        path.append(L"\\liebao\\");
	  else if (CENT_BROWSER == chrome_type_)
		  path.append(L"\\CentBrowser\\");
	  else if (STAR7_BROWSER == chrome_type_)
		  path.append(L"\\7Star\\7Star\\");
      user_data_ = base::FilePath(path).Append(chrome::kUserDataDirname);
  }
#elif defined(OS_LINUX)
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::FilePath config_dir(GetXDGDirectory(env.get(),
                                            kXdgConfigHomeEnvVar,
                                            kDotConfigDir));
  user_data_ = config_dir.Append("google-chrome");
#endif
  base::FilePath local_state = user_data_.Append(chrome::kLocalStateFilename);

  std::string profile_name;
  PrefsMap prefs_map;
  LoadProfileName(local_state, profile_name, prefs_map);

  profile_path_ = user_data_.AppendASCII(profile_name.c_str());
  local_prefs_.swap(prefs_map);

}

void GoogleChromeImporter::LoadProfileName(const base::FilePath& local_state, std::string& profile_name, PrefsMap& prefs_map){
  JSONFileValueDeserializer deserializer(local_state);
  local_state_.reset(deserializer.Deserialize(NULL, NULL).release());
  if (local_state_.get()) {
    Value* root_node = local_state_.get();
    if (root_node->GetType() != Value::TYPE_DICTIONARY)
      return;

    DictionaryValue& d_value = static_cast<DictionaryValue&>(*root_node);
    std::string last_used_profile_name;
      d_value.GetString(prefs::kProfileLastUsed,&last_used_profile_name);
  // Make sure the system profile can't be the one marked as the last one used
  // since it shouldn't get a browser.
  if (!last_used_profile_name.empty() &&
      last_used_profile_name !=
          base::FilePath(chrome::kSystemProfileDir).AsUTF8Unsafe()) {
    profile_name = last_used_profile_name;
  } else {
    profile_name = chrome::kInitialProfile;
    }
  }
}

void GoogleChromeImporter::ImportMostVisitedEE()
{
  base::FilePath app_data_path,user_data_path;
  base::FilePath ee_history_path,history_file_path,ee_topsites_path,topsites_file_path;
  if (!profile_path_.empty()) {
    ee_history_path = profile_path_.Append(FILE_PATH_LITERAL("History"));
    ee_topsites_path = profile_path_.Append(FILE_PATH_LITERAL("Top Sites"));
  }

  base::FilePath temp_path;

  PathService::Get(base::DIR_TEMP, &temp_path);

  if (!temp_path.empty()) {
    history_file_path = temp_path.Append(FILE_PATH_LITERAL("importHistory"));
    topsites_file_path = temp_path.Append(FILE_PATH_LITERAL("importTop Sites"));
  }

  base::CopyFile(ee_history_path,history_file_path);
  base::CopyFile(ee_topsites_path,topsites_file_path);

  bool bRet = false;
  sql::Connection history_db;
  sql::Connection topsites_db;
  history_db.set_page_size(4096);
  history_db.set_cache_size(6000);

  topsites_db.set_page_size(4096);
  topsites_db.set_cache_size(6000);

  if( history_db.Open(history_file_path))
  {
    bRet = history_db.DoesTableExist("segments");
    if (!bRet)
      return;
    sql::Transaction transaction(&history_db);
    bRet = transaction.Begin() &&
      history_db.Execute("CREATE TABLE segments_tmp ("
      "id INTEGER PRIMARY KEY,"
      "name VARCHAR,"
      "url VARCHAR,"
      "title VACHAR,"
      "url_id INTEGER NON NULL)") &&
      history_db.Execute("INSERT INTO segments_tmp(id,name,url,title,url_id) SELECT "
      "id, name,qh_url,qh_title, url_id FROM segments") &&
      history_db.Execute("DROP TABLE segments") &&
      history_db.Execute("ALTER TABLE segments_tmp RENAME TO segments") &&
      history_db.Execute("UPDATE meta SET value = '25' WHERE key = 'version'") &&
      transaction.Commit();
    if(!bRet)
      return ;
  }

  bridge_->SetTopSites();
}


void GoogleChromeImporter::ImportMostVisitedWorldChromeAndGoogleChrome()
{

  base::FilePath app_data_path,user_data_path,src_db_path;
  base::FilePath chrome_history_path,history_file_path,chrome_topsites_path,topsites_file_path;
  if (!profile_path_.empty()) {
    chrome_history_path = profile_path_.Append(FILE_PATH_LITERAL("History"));
    chrome_topsites_path = profile_path_.Append(FILE_PATH_LITERAL("Top Sites"));
  }

  base::FilePath temp_path;

  PathService::Get(base::DIR_TEMP, &temp_path);

  if (!temp_path.empty()) {
    history_file_path = temp_path.Append(FILE_PATH_LITERAL("importHistory"));
    topsites_file_path = temp_path.Append(FILE_PATH_LITERAL("importTop Sites"));
  }

  base::CopyFile(chrome_history_path,history_file_path);
  base::CopyFile(chrome_topsites_path,topsites_file_path);

  bool bRet = false;
  sql::Connection history_db;
  sql::Connection topsites_db;
  history_db.set_page_size(4096);
  history_db.set_cache_size(6000);

  topsites_db.set_page_size(4096);
  topsites_db.set_cache_size(6000);

  if( history_db.Open(history_file_path))
  {
    bRet = history_db.DoesTableExist("segments");
    if (!bRet)
      return;
    sql::Transaction transaction(&history_db);
    bRet = transaction.Begin() &&
      history_db.Execute("CREATE TABLE segments_tmp ("
      "id INTEGER PRIMARY KEY,"
      "name VARCHAR,"
      "url LONGVARCHAR,"
      "title LONGVARCHAR,"
      "url_id INTEGER NON NULL)") &&
      history_db.Execute("UPDATE meta SET value = '25' WHERE key = 'version'") &&
      transaction.Commit();
    if(!bRet)
      return ;

    sql::Statement statement(history_db.GetCachedStatement(SQL_FROM_HERE,
      "SELECT name, url_id, id "
      "FROM segments "
      "ORDER BY url_id"));

    sql::Statement statement2(history_db.GetCachedStatement(SQL_FROM_HERE,
      "SELECT urls.url, urls.title FROM urls "
      "JOIN segments ON segments.url_id = urls.id "
      "WHERE segments.url_id = ?"));

    sql::Statement insert(history_db.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO segments_tmp (id, name, url, title, url_id) "
      "VALUES (?,?,?,?,?)"));

    if (!statement.is_valid() || !statement2.is_valid() || !insert.is_valid())
      return;

    while (statement.Step()) {

      std::string name = statement.ColumnString(0);
      int64_t url_id = statement.ColumnInt64(1);
      int64_t segment_id = statement.ColumnInt64(2);
      statement2.BindInt64(0,url_id);

      if (statement2.Step()) {

        std::string url = statement2.ColumnString(0);
        std::string title = statement2.ColumnString(1);
        
        insert.BindInt64(0,segment_id);
        insert.BindString(1,name);
        insert.BindString(2,url);
        insert.BindString(3,title);
        insert.BindInt64(4,url_id);
        bRet = insert.Run();
        insert.Reset(true);
      }
      statement2.Reset(true);
    }

    sql::Transaction transaction2(&history_db);
    bRet = transaction2.Begin() &&
      history_db.Execute("DROP TABLE segments") &&
      history_db.Execute("ALTER TABLE segments_tmp RENAME TO segments") &&
      transaction2.Commit();
    if (!bRet)
      return ;
  }

  bridge_->SetTopSites();
}
