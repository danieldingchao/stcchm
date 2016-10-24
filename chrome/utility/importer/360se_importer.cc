#include "chrome/utility/importer/360se_importer.h"

#include <shlobj.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/file_version_info.h"
#include "base/md5.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/360se_importer/database/FavDBManager.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/importer_data_types.h"
//#include "chrome/common/importer/imported_favicon_usage.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/utility/importer/favicon_reencode.h"
#include "chrome/utility/importer/local_bookmarks_misc.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "sql/connection.h"
#include "sql/transaction.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

#include "components/search_engines/search_engines_pref_names.h"

using namespace base;
using namespace favicon_base;

namespace {
#define FILE_ICON					L"*.ico"
} // anonymous namespace

void SafeBrowserImporter::StartImport(
    const importer::SourceProfile& profile_info,
    uint16_t items,
    ImporterBridge* bridge) {
  bridge_ = bridge;
  FileVersionInfo* file_version_info =
      FileVersionInfo::CreateFileVersionInfo(profile_info.source_path);
  base::FilePath  src_profile_dir = profile_info.app_path.Append(L"User Data\\Default");
  bridge_->NotifyStarted();

  if ((items & importer::HOME_PAGE) && !cancelled())
    ImportHomepageAndStartupPage(); 

  if ((items & importer::SEARCH_ENGINES) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::SEARCH_ENGINES);
    ImportSearchEngines();
    bridge_->NotifyItemEnded(importer::SEARCH_ENGINES);
  }

  if ((items & importer::FAVORITES) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::FAVORITES);
    ImportFavorites();
    bridge_->NotifyItemEnded(importer::FAVORITES);
  }
  if ((items & importer::MOST_VISITED) && !cancelled()) {
	  bridge_->NotifyItemStarted(importer::MOST_VISITED);
    if(SE_VER_6 == browser_version_)
      ImportMostVisitedSE6(src_profile_dir);
	bridge_->NotifyItemEnded(importer::MOST_VISITED);
  }
  bridge_->NotifyEnded();
}

void SafeBrowserImporter::ImportSearchEngines() {
  if (SE_VER_6 != browser_version_) 
    return;

  base::FilePath app_data_path, pref_file_path; 
  if (!PathService::Get(base::DIR_APP_DATA, &app_data_path))
    return;
 
  pref_file_path = app_data_path.Append(L"360se6\\User Data\\Default\\Preferences");
  
  JSONFileValueDeserializer deserializer(pref_file_path);
  std::unique_ptr<base::Value> root(deserializer.Deserialize(NULL, NULL));
  //root.reset(deserializer.Deserialize(NULL, NULL));
  if (!root.get())
    return;

  Value* root_node = root.get();
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
        bridge_->SetDefaultSearchEngine(ASCIIToUTF16(url), L"");
      }
    }
  }
}

void SafeBrowserImporter::ImportHomepageAndStartupPage() {
  base::FilePath app_data_path, pref_file_path;

  if (SE_VER_6 != browser_version_) {
    GURL home_page("http://hao.360.cn");
    bridge_->AddHomePage(home_page);
    bridge_->AddStartupPage(home_page);
    return;
  }

  if (PathService::Get(base::DIR_APP_DATA, &app_data_path)) {
    pref_file_path = app_data_path.Append(L"360se6\\User Data\\Default\\Preferences");

    std::unique_ptr<base::Value> root;
    JSONFileValueDeserializer deserializer(pref_file_path);
	root.reset(deserializer.Deserialize(NULL, NULL).get());
    if (!root.get())
      return;

    Value* root_node = root.get();
    if (root_node->GetType() != Value::TYPE_DICTIONARY)
      return;

    DictionaryValue& d_value = static_cast<DictionaryValue&>(*root_node);
    Value* startup_page_value;
    if (d_value.Get(prefs::kURLsToRestoreOnStartup, &startup_page_value)) {
      if (startup_page_value->GetType() == Value::TYPE_LIST) {
        ListValue* list = static_cast<ListValue*>(startup_page_value);
        if (!list->empty()) {
          Value* home_page_value;
          if (list->Get(0, &home_page_value) && home_page_value->GetType() == Value::TYPE_STRING) {
            StringValue* home_page_str = static_cast<StringValue*>(home_page_value);
            std::string homepage;
            home_page_str->GetAsString(&homepage);
            if (!homepage.empty()) {
              bridge_->AddHomePage(GURL(homepage));
              bridge_->AddStartupPage(GURL(homepage));
            }
          }
        }
      }
    }
  }
}


void SafeBrowserImporter::ImportMostVisitedSE6(base::FilePath& profile_path){
  base::FilePath se6_history_path, history_file_path, se6_topsites_path,
      topsites_file_path;

  if (!profile_path.empty()) {
    se6_history_path = profile_path.Append(L"History");
    se6_topsites_path = profile_path.Append(L"Top Sites");
  }
  
  base::FilePath temp_path;
  wchar_t path[MAX_PATH];
  GetTempPath(MAX_PATH,path);
  temp_path = base::FilePath(path);

  if (!temp_path.empty()) {
    history_file_path = temp_path.Append(L"importHistory");
    topsites_file_path = temp_path.Append(L"importTop Sites");
  }

  base::CopyFile(se6_history_path,history_file_path);
  base::CopyFile(se6_topsites_path,topsites_file_path);

  bool bRet = false;
  sql::Connection history_db;
  history_db.set_page_size(4096);
  history_db.set_cache_size(6000);

  if (history_db.Open(history_file_path)) {
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
      history_db.Execute("INSERT INTO segments_tmp SELECT "
      "id, name,url,title, url_id FROM segments") &&
      history_db.Execute("DROP TABLE segments") &&
      history_db.Execute("ALTER TABLE segments_tmp RENAME TO segments") &&
      history_db.Execute("UPDATE meta SET value = '25' WHERE key = 'version'") &&
      transaction.Commit();
    if(!bRet)
      return ;
  }

  bridge_->SetTopSites();
}

void SafeBrowserImporter::ImportFavicons(
	favicon_base::FaviconUsageDataList & favicons,
    std::vector<FavItem> & arrFavItem) {
  wchar_t buffer[MAX_PATH] = {0};
  if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL,
    SHGFP_TYPE_CURRENT, buffer))) {
      std::wstring path(buffer);

    // se5的图标，位置存在具体帐号下
    if(SE_VER_5 == browser_version_) {
      ImpoterResult importPath(Gen360seFilePath());
      importPath.second.append(L"data\\ico");
      path = importPath.second;
    }
    // se6的图标，统一存放在一处
    else
      path.append(L"\\360se6\\apps\\data\\users\\default\\data\\ico");

    std::vector<FileNameAndTime> FileList;
    SystemCommon::FilePathHelper::SearchFile( path.c_str() , FILE_ICON , FileList );
    if(!FileList.empty()) {
      // 遍历图标文件和收藏url，找到对应关系
      for(unsigned int i = 0; i < FileList.size(); ++i) {
        for(unsigned int j = 0; j < arrFavItem.size(); ++j) {
          std::wstring strIconName = FileList[i].strFullPath;
          int nPos = strIconName.rfind(L'\\')+1;
          strIconName = strIconName.substr(nPos, strIconName.size() - nPos);
          nPos = strIconName.rfind(L'.');
          strIconName = strIconName.substr(0, nPos);
          if(arrFavItem[j].strURL.find(strIconName) != std::wstring::npos) {
            std::string data;
            base::ReadFileToString(base::FilePath(FileList[i].strFullPath),
              &data);
            const unsigned char* ptr =
              reinterpret_cast<const unsigned char*>(data.c_str());

			FaviconUsageData usage;
            usage.favicon_url = GURL(arrFavItem[j].strURL);
            if(usage.favicon_url.is_valid()) {
              if(importer::ReencodeFavicon(ptr, data.size(), &usage.png_data)) {
                usage.urls.insert(usage.favicon_url);
                favicons.push_back(usage);
              }
            }
          }
        }
      }
    }
  }
}

void SafeBrowserImporter::ImportFavorites() {
  std::wstring path(Gen360seDBFileName());
  
  std::vector<FavItem> arrFavItem;
  SafeBrowserImporter::BookmarkVector bookmarks;
  favicon_base::FaviconUsageDataList favicons;
  if(!path.empty()){
    sql::Connection * sqlite = SafeBrowserDBImporter::OpenAndCheck(path);
    if(sqlite != NULL){
      std::unique_ptr<sql::Connection> db(sqlite);
      SafeBrowserDBImporter::ImportData(db.get(), arrFavItem);
      SafeBrowserDBImporter::ArrangeAllItems(arrFavItem, bookmarks);
      ImportFavicons(favicons, arrFavItem);
    }
  }

  if ((!bookmarks.empty())&& !cancelled() ) {
    std::wstring first_folder_name = L"从 360SE 导入";
    if(SE_VER_6 == browser_version_)
      first_folder_name =  L"从 360SE 导入";//l10n_util::GetStringUTF16( IDS_IMPORT_FIRSTRUN_DLG2_360_FAVORITES )/*L"360se收藏夹"*/;
    else if(TW_VER_5 == browser_version_)
      first_folder_name =  L"从 世界之窗5 导入";
    bridge_->AddBookmarks(bookmarks, first_folder_name);
  }

  if(!favicons.empty())
    bridge_->SetFavicons(favicons);
}

SafeBrowserImporter::ImpoterResult SafeBrowserImporter::Gen360seFilePath() {

  wchar_t buffer[MAX_PATH] = {0};
  if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL,
    SHGFP_TYPE_CURRENT, buffer))) {

      if(TW_VER_5 == browser_version_) {
        std::wstring path(buffer);
        path.append(L"\\TheWorld\\data\\");
        return std::make_pair(LOCAL_IMPORTER_TYPE, path);
      }
      
      std::wstring login_ini(buffer);
      std::wstring path(buffer);
      if(SE_VER_5 == browser_version_)
        login_ini.append(L"\\360se\\login.ini");
      else if(SE_VER_6 == browser_version_)
        login_ini.append(L"\\360se6\\apps\\data\\users\\login.ini");
        
      int is_auto = GetPrivateProfileInt(
        L"NowLogin",
        L"IsAuto",
        0,
        (login_ini).c_str()
        );
      wchar_t buffer_name[MAX_PATH] = {0};
      if(is_auto==1){
        memset(buffer,0,sizeof(buffer));
        GetPrivateProfileString(
          L"NowLogin",
          L"UserMd5",
          L"",
          buffer_name,
          MAX_PATH,
          (login_ini).c_str()
          );
        std::wstring user_md5 = buffer_name;
        GetPrivateProfileString(
          L"NowLogin",
          L"NowUser",
          L"",
          buffer_name,
          MAX_PATH,
          (login_ini).c_str()
          );
        std::wstring user_name(buffer_name);

        if(user_md5.size() > 0) {
          std::wstring final_path(path);
          if(SE_VER_5 == browser_version_)
            final_path += L"\\360se\\";
          else if(SE_VER_6 == browser_version_)
            final_path += L"\\360se6\\apps\\data\\users\\";
          final_path+=user_md5;
          final_path+=L"\\";
          return std::make_pair(NET_IMPORTER_TYPE, final_path);
        }
        else if(user_name.size()){
          user_name=base::ToLowerASCII(user_name);
          std::string md5_filename = base::MD5String(base::SysWideToNativeMB(user_name));
          std::wstring md5_wfilename(base::SysNativeMBToWide(md5_filename));
          std::wstring final_path(path);
          if(SE_VER_5 == browser_version_)
            final_path += L"\\360se\\";
          else if(SE_VER_6 == browser_version_)
            final_path += L"\\360se6\\apps\\data\\users\\";
          final_path+=md5_wfilename;
          final_path+=L"\\";
          return std::make_pair(NET_IMPORTER_TYPE, final_path);
        }else
          goto local;

      }else{
        goto local;
      }

local:
      if(SE_VER_5 == browser_version_)
        path += L"\\360se\\";
      else if(SE_VER_6 == browser_version_)
        path += L"\\360se6\\apps\\data\\users\\";
      return std::make_pair(LOCAL_IMPORTER_TYPE, path);

  }
  return std::make_pair(NONE_IMPORTER_TYPE,std::wstring());

}


std::wstring SafeBrowserImporter::Gen360seDBFileName()
{
  //using namespace file_util;
  ImpoterResult path(Gen360seFilePath());

  if(path.first == NET_IMPORTER_TYPE ) {
    path.second += L"360sefav.db";
  } else if(path.first==LOCAL_IMPORTER_TYPE) {
      if(SE_VER_5 == browser_version_)
        path.second += L"data\\360sefav.db";
      else if(SE_VER_6 == browser_version_)
        path.second += L"default\\360sefav.db";
      else if(TW_VER_5 == browser_version_)
        path.second += L"theworldfav.db";
  }

  return path.second;

}

