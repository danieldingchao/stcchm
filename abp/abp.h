#pragma once

#include "abp/abp_export.h"
#include "base/files/file_path.h"
#include "base/strings/string_piece.h"

#include <vector>

namespace abp{

//////////////////////////////////////////////////////////////////////////
ABP_EXPORT  base::StringPiece Decrypt(const base::StringPiece& text);

ABP_EXPORT bool AbpIsThirdParty(const std::string& request_url,
  const std::string& document_url);

ABP_EXPORT bool AbpHostIsIpAdress(const std::string& url);

ABP_EXPORT std::string AbpGetBaseDomain(const std::string& url);

ABP_EXPORT std::string AbpExtractHostFromUrl(const std::string& url);

ABP_EXPORT bool AbpIsLowAlphaOrDigitOrSpeical(char c,char* speical);

//var candidates = location.toLowerCase().match(/[a-z0-9%]{3,}/g);
ABP_EXPORT void AbpGetCandidates(const std::string& url,
  std::vector<std::string>& candidates);

//var candidates = text.toLowerCase().match(/[^a-z0-9%*][a-z0-9%]{3,}(?=[^a-z0-9%*])/g);
ABP_EXPORT void AbpGetCandidatesWithDelimiter(const base::StringPiece& low_text,
  std::vector<base::StringPiece>& candidates);

// ^: [^a-z0-9_-.%]
// *: anything
ABP_EXPORT bool AbpWildcardMatches(const char *wildcard,
                                   const char *str,bool match_case);

// /^\|?[\w\-]+:/.test(text)) 
ABP_EXPORT bool AbpIsStartWithScheme(const base::StringPiece& text);

//////////////////////////////////////////////////////////////////////////

enum FilterType{
  FT_INVALID=1,
  FT_COMMENT,
  FT_BLACKLIST,
  FT_WHITELIST,
  FT_ELEMHIDE,
  FT_HIDEEXCEPTION
};

enum ContentType{
  CT_OTHER=1,
  CT_SCRIPT=2,  //ok,
  CT_IMAGE=4,   //ok,
  CT_STYLESHEET=8, //ok,
  CT_OBJECT=16, //ok,
  CT_SUBDOCUMENT=32, //sub_frame,ok
  CT_DOCUMENT=64, //main_frame
  CT_XBL=1,
  CT_PING=1,
  CT_XMLHTTPREQUEST=2048,//ok
  CT_OBJECT_SUBREQUEST=4096,
  CT_DTD=1,
  CT_MEDIA=16384,
  CT_FONT=32768,
  CT_BACKGROUND=4,
  CT_POPUP=268435456,
  CT_DONOTTRACK=536870912,
  CT_ELEMHIDE=1073741824
};

//
enum DomainMatchType{
  DMT_ALL_UNUSED, //
  DMT_INCLUDE_EXCLUDE,//Ö¸
  DMT_JUST_INCLUDE,//
};

class ABP_EXPORT Filter{
public:
  explicit Filter(const base::StringPiece& text,FilterType type)
    :text_(text),type_(type){
  }

  base::StringPiece Text(){
    return text_;
  }
  FilterType Type(){
    return type_;
  }

private:
  base::StringPiece text_; //
  FilterType type_;
};

class ABP_EXPORT FilterMgr{
public:
  FilterMgr(){}
  virtual ~FilterMgr(){}

  virtual void Reset() = 0;
  virtual void InitForDebug() = 0;
  virtual void InitFromFile(const base::FilePath& file_path) = 0;
  virtual void InitFromFileWithoutDescript(const base::FilePath& file_path) = 0;
  virtual Filter* ProcessLine(const base::StringPiece& text) = 0;
  virtual std::string GetTitle() = 0; //utf8
  virtual std::string GetUpdatedTime() = 0;//utf8
  virtual std::string GetVersion() = 0;//utf8
  virtual std::string GetAuthor() = 0;//utf8
  virtual std::string GetUrl() = 0;//utf8
  virtual bool IsValid() = 0;
  virtual void SetId(int id) = 0;
  virtual int GetId() = 0;
  virtual void Enable() = 0;
  virtual void Disable() = 0;
  virtual bool IsEnable() = 0;
  virtual void SetFilePath(const base::FilePath& file_path) = 0;
  virtual const base::FilePath& GetFilePath() = 0;
  //
  //
  virtual Filter* MatchAny(const std::string& req_url,
    ContentType content_type, 
    const std::string& doc_host, 
    bool third_party) = 0; 
 
  virtual Filter* MatchByKey(const std::string& req_url,
    const std::string& key,
    const std::string& doc_host) = 0;

  virtual void GetSelectorsForHost(const std::string& host, 
    DomainMatchType match_type, 
    std::vector<std::string>& css) = 0;

  static FilterMgr* NewInstance(char const*const name);
  static FilterMgr* InitPopWhiteList(base::FilePath& filename);
  static FilterMgr* GetPopWhiteList();
};

//////////////////////////////////////////////////////////////////////////
} // end of namespace abp
