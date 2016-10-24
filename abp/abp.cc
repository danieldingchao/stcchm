#include "abp/abp.h"

#include "abp/abp_matcher.h"
#include "abp/abp_parser.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/debug/alias.h"

#include "url/gurl.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "base/lazy_instance.h"
#include <map>
#include <utility>

namespace {

  bool IsStringANSI(const base::StringPiece& str) {
    if (str.length() < 3)
      return false;
    const unsigned char * data = (const unsigned char *)str.data();
    if ((data[0] == 0xFF && data[1] == 0xFE) ||
        (data[0] == 0xFE && data[1] == 0xFF) ||
        (data[0] == 0xEF && data[1] == 0xBB) ||
        base::IsStringUTF8(str.as_string()))
      return false;
    return true;
  }
} // namespace

namespace abp{
//////////////////////////////////////////////////////////////////////////

bool AbpIsThirdParty(const std::string& request_url,
  const std::string& document_url){
  return !net::registry_controlled_domains::SameDomainOrHost(GURL(request_url),
      GURL(document_url),
      net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
}

std::string CheckSum(const base::StringPiece& text){
    return base::MD5String(text);
}

void Encrypt(base::StringPiece text){
    if (!text.length()){
        return;
    }

    char* buf = (char*)text.data();
    for (size_t i = 0; i<text.length(); i++){
        buf[i] ^= ((i + 9) % 13 + 'B');
    }
}

base::StringPiece Decrypt(const base::StringPiece& text){
    if (text.length() && text[0] == '#'){
        return text;
    }
    if (text.length() < 32){
        return "";
    }

    base::StringPiece check_sum = text.substr(text.length() - 32);
    base::StringPiece plain_text = text.substr(0, text.length() - 32);
    Encrypt(plain_text);
    std::string check_sum_computed = CheckSum(plain_text);

    if (check_sum != check_sum_computed){
        return "";
    }

    return plain_text;
}

bool AbpHostIsIpAdress(const std::string& url_str){
  GURL gurl(url_str);
  const url::Component host =
    gurl.parsed_for_possibly_invalid_spec().host;
  if (host.len <= 0)
    return false;
  return gurl.HostIsIPAddress();
}

std::string AbpGetBaseDomain(const std::string& url){
  return net::registry_controlled_domains::GetDomainAndRegistry(url,
      net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
}

std::string AbpExtractHostFromUrl(const std::string& url_str){
  GURL gurl(url_str);
  const url::Component host =
    gurl.parsed_for_possibly_invalid_spec().host;
  if (host.len <= 0)
    return "";
  return gurl.host();
}

bool AbpIsLowAlphaOrDigitOrSpeical(char c,char* speical){
  if( c >= 'a' && c <= 'z' 
    || c >= '0' && c <= '9'){
      return true;
  }

  if(!speical){
    return true;
  }
  while(*speical){
    if(c == *speical) 
      return true;
    speical++;
  }
  return false;
}

//var candidates = location.toLowerCase().match(/[a-z0-9%]{3,}/g);
void AbpGetCandidates(const std::string& url,std::vector<std::string>& candidates){
  std::string low_url = base::ToLowerASCII(url);

  int start = 0;
  int len = 0;
  char cur_char = 0;
  for(size_t i=0;i<low_url.length();i++){
    cur_char = low_url[i];
    if( AbpIsLowAlphaOrDigitOrSpeical(cur_char,"%")){
        len++;
    }else{
      if(len>=3){
        candidates.push_back(low_url.substr(start,len));
      }
      start = start+len+1;
      len = 0;
    }
  }
  if(len>=3){
    candidates.push_back(low_url.substr(start,len));
  }
}

//var candidates = text.toLowerCase().match(/[^a-z0-9%*][a-z0-9%]{3,}(?=[^a-z0-9%*])/g);
void AbpGetCandidatesWithDelimiter(const base::StringPiece& low_text,
                                   std::vector<base::StringPiece>& candidates){
  int start = 0;
  int len = 0;
  char cur_char = 0;
  for(size_t i=0;i<low_text.length();i++){
    cur_char = low_text[i];
    if( !AbpIsLowAlphaOrDigitOrSpeical(cur_char,"%*")){
      if(len>=4){
        candidates.push_back(low_text.substr(start,len));
      }
      start=i;
      len=1;
    }else{
      if(len && cur_char!='*'){
        len++;
      }else{
        start=i;
        len=0;
      }
    }
  }
}

// ^: [^a-z0-9_-.%]
// *: anything
bool AbpWildcardMatches(const char *wildcard, const char *str,bool match_case) {
  while (1) {
    if (*wildcard == '\0'){
      return *str == '\0';
    }
    if (*wildcard == '^') {
      if(!*str || AbpIsLowAlphaOrDigitOrSpeical(base::ToLowerASCII(*str),"_-.%")){
        return false;
      }
      ++wildcard; ++str;
    } else if (*wildcard == '*') {
      while(*wildcard == '*') {
        wildcard++;
      }
      for (; *str; ++str){
        if (AbpWildcardMatches(wildcard, str,match_case)){
          return true;
        }
      }
      return *wildcard == '\0';
    } else {
      if(match_case){
        if (*wildcard != *str){
          return false;
        }
      }else{
        if(base::ToLowerASCII(*wildcard) != base::ToLowerASCII(*str)){
          return false;
        }
      }

      ++wildcard; ++str;
    }
  }
}

// /^\|?[\w\-]+:/.test(text)) 
bool AbpIsStartWithScheme(const base::StringPiece& text){
  int len = 0;
  char cur_char = 0;
  for(size_t i=0;i<text.length();++i){
    cur_char = text[i];
    if(len==0){
      if(cur_char=='|' || base::IsAsciiAlpha(cur_char) || cur_char=='-'){
        len++;
        continue;
      }else{
        return false;
      }
    }else{
      if(base::IsAsciiAlpha(cur_char) || cur_char == '-'){
        len++;
        continue;
      }else if(cur_char==':'){
        if(len==1 && text[0]=='|'){
          return false;
        }
        return true;
      }else{
        return false;
      }
    }
  }
  return false;
}

//////////////////////////////////////////////////////////////////////////
class FilterMgrImpl : public FilterMgr{
public:
  FilterMgrImpl(char const*const name)
    :is_valid_(true),
     name_(name),
     id_(-1),
     is_enable_(true) {
  }
  FilterMgrImpl()
    :is_valid_(true),
     name_("whitelist"),
     id_(-1),
     is_enable_(true) {
  }
  virtual ~FilterMgrImpl() {
    char name[50];
    base::debug::Alias(&name);
    if (name_) {
      strncpy(name, name_, sizeof(name));
    }
    Reset();
  }

  virtual void InitForDebug() override{
  }

  virtual void InitFromFile(const base::FilePath& file_path){
    int64_t file_size = 0;
    if(!base::GetFileSize(file_path,&file_size) || file_size>10*1024*1024)
      return;

    char* file_buf = new char[file_size];
    if(base::ReadFile(file_path,file_buf,file_size)!=file_size)
      return;
    
    file_buf_.set(file_buf,file_size);
    base::StringPiece rule_buf = Decrypt(file_buf_);
    if(rule_buf.length()==0){
      is_valid_ = false;
      return;
    }

    std::vector<base::StringPiece> lines;
    base::Tokenize(rule_buf,"\r\n",&lines);
    known_filters_.resize(lines.size()); //pre-alloc
    
    ProcessLines(lines);
  }

  virtual void InitFromFileWithoutDescript(const base::FilePath& file_path) {
    int64_t file_size = 0;
    if(!base::GetFileSize(file_path,&file_size) || file_size>10*1024*1024)
      return;
    LOG(WARNING) << "InitFromFileWithoutDescript" ;
    char* file_buf = new char[file_size];
    if(base::ReadFile(file_path,file_buf,file_size)!=file_size)
      return;
    
    file_buf_.set(file_buf,file_size);
    base::StringPiece rule_buf = file_buf_;
    if( rule_buf.length()==0 ) {
      is_valid_ = false;
      return;
    }

    std::vector<base::StringPiece> lines;
    base::Tokenize(rule_buf,"\r\n",&lines);
	known_filters_.resize(lines.size()); //pre-alloc
    ProcessLines(lines);
  }

  virtual void Reset() override{
    matcher_.Reset();
    elem_hide_.Reset();

    std::vector<Filter*>::iterator it = known_filters_.begin();
    for(;it!=known_filters_.end();it++)
      delete *it;
    known_filters_.clear();

    char* file_buf = (char*)file_buf_.data();
    if(file_buf_.length() && file_buf)
      delete[] file_buf;
  }

  virtual Filter* MatchAny(const std::string& req_url,ContentType content_type,
    const std::string& doc_host,bool third_party) override{


    if(content_type==CT_DOCUMENT || content_type==CT_ELEMHIDE){
      std::string really_req_url = req_url;
      std::string really_doc_host = doc_host;

      if(really_doc_host.length()==0){
        really_doc_host = AbpExtractHostFromUrl(req_url);
      }

      std::string::size_type idx = req_url.find("#");
      if(idx!=std::string::npos){
        really_req_url = req_url.substr(0,idx);
      }

      third_party = false;

      return matcher_.MatchAny(req_url,content_type,really_doc_host,third_party);
    }

    return matcher_.MatchAny(req_url,content_type,doc_host,third_party);
  }

  virtual Filter* MatchByKey(const std::string& req_url, 
      const std::string& key, 
      const std::string& doc_host) override{
    return matcher_.MatchByKey(req_url,key,doc_host);
  }

  virtual void GetSelectorsForHost(const std::string& host, DomainMatchType match_type,
    std::vector<std::string>& css) override{
    elem_hide_.GetSelectorsForHost(host,match_type,css);
  }

  virtual Filter* ProcessLine(const base::StringPiece& raw_text) override{
    base::StringPiece text = base::TrimWhitespaceASCII(raw_text, base::TRIM_TRAILING);
    if(text.empty())
      return NULL;

    Filter* ret = Parser::FromText(text);
    if(!ret)
      return ret;
    known_filters_.push_back(ret);

    FilterType type = ret->Type();
    switch (type)
    {
    case FT_BLACKLIST:
    case FT_WHITELIST:
      matcher_.Add(ret);
      if (id_ != -1) {
        size_t len = text.length();
        if (len < 3)
          is_valid_ = false;
      }
      break;
    case FT_ELEMHIDE:
    case FT_HIDEEXCEPTION:
      elem_hide_.Add(ret);
      if (id_ != -1) {
        size_t len = text.length();
        if (len < 3)
          is_valid_ = false;
      }
      break;
    case FT_INVALID:
      if (id_ != -1)
        is_valid_ = false;
      break;
    default:
      ;
    }
    return ret;
  }

  virtual std::string GetTitle() override{
    return title_;
  }

  virtual std::string GetUpdatedTime() override{
    return updated_time_;
  }

  virtual std::string GetVersion() override{
    return version_;
  }

  virtual std::string GetAuthor() override{
    return author_;
  }

  virtual std::string GetUrl() override{
    return url_;
  }

  virtual bool IsValid() override{
    return is_valid_;
  }

  virtual void SetId(int id) override {
    id_ = id;
  }

  virtual int GetId() override {
    return id_;
  }

  virtual void Enable() override {
    is_enable_ = true;
  }

  virtual void Disable() override {
    is_enable_ = false;
  }

  virtual bool IsEnable() override {
    return is_enable_;
  }

  virtual const base::FilePath& GetFilePath() override {
    return file_path_;
  }

  virtual void SetFilePath(const base::FilePath& file_path) override {
    file_path_ = file_path;
  }

private:
  bool IsValidLine(const base::StringPiece& text){
      return text.length() && text[0] != '[';
  }

  void ProcessTitleLine(const base::StringPiece& text){
    std::vector<base::StringPiece> kv;
    base::Tokenize(text,"=",&kv);

      if (kv[0]=="title")  title_ = kv[1].as_string();
      if (kv[0] == "url")    url_ = kv[1].as_string();
      if (kv[0] == "version")      version_ = kv[1].as_string();
      if (kv[0] == "updated_time") updated_time_ = kv[1].as_string();
      if (kv[0] == "author")       author_ = kv[1].as_string();
  }

  void ProcessFilterSection(const std::vector<base::StringPiece>& lines,
      int& cur_line){
    int total_line = lines.size();
    cur_line++;
    while(cur_line<total_line && IsValidLine(lines[cur_line])) {
      ProcessLine(lines[cur_line]);
      cur_line++;
    }
  }

  void ProcessTitleSection(const std::vector<base::StringPiece>& lines,
    int &cur_line){
    int total_line = lines.size();
    cur_line++;
    while(cur_line<total_line && IsValidLine(lines[cur_line])) {
      ProcessTitleLine(lines[cur_line]);
      cur_line++;
    }
  }

  void ProcessLines(const std::vector<base::StringPiece>& lines){

		LOG(WARNING) << "ProcessLines" ;
    int cur_line = 0;
    int total_line = lines.size();
    while(cur_line<total_line){
      if(lines[cur_line][0]=='['){
        base::StringPiece section = TrimWhitespaceASCII(lines[cur_line], base::TRIM_ALL);
        if (IsValidLine(lines[cur_line])) {
            ProcessLine(lines[cur_line]);
            cur_line++;
        } else{
          cur_line++;
        }
      }else{
          if (IsValidLine(lines[cur_line])) {
              ProcessLine(lines[cur_line]);
          }
          cur_line++;
      }
    }
  }

private:
  base::StringPiece file_buf_;       //free atexit
  std::vector<Filter*> known_filters_; //free atexit,
  CombinedMatcher matcher_;
  ElemHider elem_hide_;

  // AdBlock=
  std::string title_;
  std::string updated_time_;
  std::string url_;
  std::string version_;
  std::string author_;
  int id_;
  bool is_enable_;
  base::FilePath file_path_;
  bool is_valid_;
  char const*const name_;
};

// static
FilterMgr* FilterMgr::NewInstance(char const*const name) {
  return new FilterMgrImpl(name);
}

static base::LazyInstance<FilterMgrImpl>g_LazePopWhiteList = LAZY_INSTANCE_INITIALIZER;
static FilterMgr * g_PopWhite = NULL;
FilterMgr* FilterMgr::InitPopWhiteList(base::FilePath& filename){
	if( !g_PopWhite ){
		FilterMgr* pMgr = g_LazePopWhiteList.Pointer();
		pMgr->InitFromFile(filename);
		g_PopWhite = pMgr;
	}
	return g_PopWhite;
}
FilterMgr* FilterMgr::GetPopWhiteList(){
	return g_PopWhite;
}
//////////////////////////////////////////////////////////////////////////
} //namesapce abp
