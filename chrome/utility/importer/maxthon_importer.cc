// Copyright (c) 2011 qihoo.com
// Author liuqingping@qihoo.net

#include "chrome/utility/importer/maxthon_importer.h"

#include <shlobj.h>

#include "base/file_version_info.h"
#include "base/files/file_util.h"
#include "base/md5.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/imported_favicon_usage.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/utility/importer/importer_util.h"
#include "chrome/common/url_constants.h"
#include "net/base/load_flags.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/url_request/url_request_status.h"
#include "sql/init_status.h"
#include "sql/transaction.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "url/gurl.h"

MaxthonBrowserImporter::MaxthonBrowserImporter() {
  wchar_t path[MAX_PATH];
  GetTempPath(MAX_PATH, path);
  m_strDesHtmlPath = path;
  m_strDesHtmlPath.append(L"tw6_import_tmp.html");
}

void MaxthonBrowserImporter::StartImport(
    const importer::SourceProfile& profile_info,
    uint16_t items,
  ImporterBridge* bridge) {
  bridge_ = bridge;

  bridge_->NotifyStarted();

  if ((items & importer::FAVORITES) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::FAVORITES);
    ImportFavorites();
    bridge_->NotifyItemEnded(importer::FAVORITES);
  }
  bridge_->NotifyEnded();
}

void MaxthonBrowserImporter::ImportFavorites() {
  if(ImportMaxthon() == 1) {
    std::set<GURL> default_urls;
    base::FilePath file(m_strDesHtmlPath);
    std::vector<ImportedBookmarkEntry> bookmarks, toolbar_bookmarks;
    std::vector<TemplateURL*> template_urls;
    favicon_base::FaviconUsageDataList favicons;
    base::Time start_time = base::Time::Now();
    importer::ImportBookmarksFromFile(file, default_urls, this, &bookmarks, &template_urls,
      &favicons);

    if ((!bookmarks.empty())&& !cancelled() ) {
      bridge_->AddBookmarks(bookmarks, L"从 傲游浏览器 导入");
    }
  }
  const base::FilePath tmp_file(m_strDesHtmlPath);
  base::DeleteFile(tmp_file, true);
}

int MaxthonBrowserImporter::ImportMaxthon()
{
  std::wstring strMaxthonFavPath = SystemCommon::FilePathHelper::GetParentPath(m_MXDBMgr.GetMaxthonFavFilePath());
  // 收藏文件不存在
  if(!PathFileExists(strMaxthonFavPath.c_str()))
  {
    return -1;
  }
  std::wstring strFlag = SystemCommon::StringHelper::Format( L"chk_%d", GetTickCount());
  // 加解密失败了
  if(!m_MXDBMgr.Init(strMaxthonFavPath.c_str(), m_MXDBMgr.GetMaxthonFavFilePath().c_str(), strFlag.c_str()))
  {
    return 0;
  }

  // 开始将傲游收藏->html文件
  FILE * fp = _tfopen(m_strDesHtmlPath, L"w+");
  if(fp)
  {
    unsigned char szHead[4] = {0xef,0xbb,0xbf,0x00};
    fwrite(szHead,3,1,fp);
    //fprintf(fp, "%s<!DOCTYPE NETSCAPE-Bookmark-file-1>\n", szHead);
    fprintf(fp, "%s","<!DOCTYPE NETSCAPE-Bookmark-file-1>\n");
    fprintf(fp, "<!-- This is an automatically generated file.\n");
    fprintf(fp, "It will be read and overwritten.\n");
    fprintf(fp, "Do Not Edit! -->\n");

    // 设置文件编码格式 == 2则为utf - 8， == 1则为ansi
    fprintf(fp,"<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">\n");
    fprintf(fp, "<TITLE>Bookmarks</TITLE>\n");
    fprintf(fp, "<H1>Bookmarks</H1>\n");

    // 把收藏夹下的项写入到html文件
    fprintf(fp, "<DL><p>\n");
    unsigned char pstrID[16] = {0};
    WriteFolderToHtmlMaxthon(pstrID, fp, 0);
    fprintf(fp,"</DL><p>\n");

    fclose(fp);
  }
  // UnitDB等等操作
  m_MXDBMgr.UnInit();
  return 1;
}

void MaxthonBrowserImporter::WriteFolderToHtmlMaxthon(unsigned char * pszPID, FILE * fp, int nIndex)
{
  std::vector<FavItemEx> vFav;
  GetAllItemsMaxthon(pszPID, vFav);

  // 没有子项全部返回
  int nSubSize=vFav.size();
  if (nSubSize < 1)
    return ;
  char szStep[MAX_PATH * 2] = {0};
  for (int e = 0; e < nIndex;  e++)
  {
    strncat(szStep, "\t", (MAX_PATH * 2) - 1);
  }
  //先遍历文件夹
  for (int i = 0; i < nSubSize; i++)
  {
    FavItemEx & item = vFav[i];
    if (item.bFolder)
    {
      char szTmpBuf[MAX_PATH*3] = {0};
      sprintf_s(szTmpBuf,"%s<DT><H3 FOLDED ADD_DATE=\"0\" >", szStep);
      std::string strTmpinfo(szTmpBuf);
      strTmpinfo += item.strTitleA.c_str();
      strTmpinfo += "</H3>\n";
      //fprintf_s(fp,strTmpinfo.c_str());
      fwrite(strTmpinfo.c_str(),1,strTmpinfo.length(),fp);
      fprintf(fp, "%s<DL><p>\n", szStep);
      WriteFolderToHtmlMaxthon((unsigned char*)item.strID.data(), fp, nIndex + 1);
      fprintf(fp,"%s</DL><p>\n", szStep);
    }
  }

  //再遍历URL文件
  for (int i = 0; i < nSubSize; i++)
  {
    FavItemEx & item = vFav[i];
    if (!item.bFolder)
    {
      // 换一种输出方式
      char szTmpBufA[INTERNET_MAX_URL_LENGTH_LARGE*2] = {0};
      char szTmpBufB[INTERNET_MAX_URL_LENGTH_LARGE] = {0};

      sprintf_s (szTmpBufA,INTERNET_MAX_URL_LENGTH_LARGE*2,"%s<DT><A HREF=\"%s\" ADD_DATE=\"0\"",szStep, CT2A(item.strURL.c_str()).m_psz);
      sprintf_s (szTmpBufB,INTERNET_MAX_URL_LENGTH_LARGE," >%s</A>\n", item.strTitleA.c_str());
      fwrite(szTmpBufA,1,strlen(szTmpBufA),fp);
      fwrite(szTmpBufB,1,strlen(szTmpBufB),fp);
    }
  }
}

int MaxthonBrowserImporter::GetAllItemsMaxthon(unsigned char * pszPID, std::vector<FavItemEx> & vecItems)
{
  CppSQLite3Buffer sql;
  CppSQLite3Query oQuery;
  CppSQLite3DB* pMXDB = m_MXDBMgr.GetDB();

  if ( !pMXDB )
  {
    return 0;
  }
  char szSqlBuff[1000] = {0};
  _snprintf(szSqlBuff, ARRAYSIZE(szSqlBuff) - 1, "SELECT id, parent_id, type, title, url, norder FROM MyFavNodes where parent_id=?");
  CppSQLite3Statement stat = pMXDB->compileStatement(szSqlBuff);
  stat.bind(1, pszPID, 16);
  oQuery = stat.execQuery();

  while (!oQuery.eof())
  {
    int nLen = INTERNET_MAX_URL_LENGTH_LARGE;

    unsigned char * pszID = (unsigned char *)oQuery.getBlobField("id", nLen);
    std::string strID = Util::CodeMisc::UTF8EncodeW((LPCTSTR)pszID);

    unsigned char * pszPID = (unsigned char *)oQuery.getBlobField("parent_id", nLen);
    std::string strPID = Util::CodeMisc::UTF8EncodeW((LPCTSTR)pszPID);

    unsigned char * pszTitle = (unsigned char *)oQuery.getBlobField("title", nLen);
    std::string strTitle = Util::CodeMisc::UTF8EncodeW((LPCTSTR)pszTitle);

    unsigned char * pszUrl = (unsigned char *)oQuery.getBlobField("url", nLen);
    std::string strUrl = Util::CodeMisc::UTF8EncodeW((LPCTSTR)pszUrl);

    int nType = oQuery.getIntField("type");
    int nPos = oQuery.getIntField("norder");

    FavItemEx item;
    item.strPID.assign( (const char*)pszPID , sizeof(wchar_t)*( wcslen((const wchar_t*)pszPID) + 1 ) );
    item.strID.assign( (const char*)pszID , sizeof(wchar_t)*( wcslen((const wchar_t*)pszID) + 1 ) );
    if(nType == 2)
      item.bFolder = true;
    else
      item.bFolder = false;
    item.strTitleA = strTitle;
    item.strTitle = (LPCTSTR)pszTitle; // A2T(strTitle.c_str());
    bool toadd = true;  
    if(!item.bFolder)
      item.strURL = (LPCTSTR)pszUrl; // A2T(strUrl.c_str());
    else
    {
      if( item.strID.size() < 2 ) toadd = false;
      else if( item.strID[0] == '\0' && item.strID[1] == '\0' )
        toadd = false;
    }
    if( toadd )
      vecItems.push_back(item);
    oQuery.nextRow();
  }
  return 0;
}