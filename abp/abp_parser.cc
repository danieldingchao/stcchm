#include "abp/abp_parser.h"

#include "abp/abp_filter.h"
#include "base/lazy_instance.h"
#include "base/strings/string_util.h"

typedef std::map<base::StringPiece,int> ContentTypeMap;
static base::LazyInstance<ContentTypeMap> g_type_map =
  LAZY_INSTANCE_INITIALIZER;

namespace{
  void StringPieceToUpperASCII(base::StringPiece& str) {
    char* str_buf = (char*)str.data();
    for(size_t idx = 0;idx<str.length();idx++){
        str_buf[idx]=base::ToUpperASCII(str_buf[idx]);
    }
  }
  void StringPieceToLowerASCII(base::StringPiece& str) {
    char* str_buf = (char*)str.data();
    for(size_t idx = 0;idx<str.length();idx++){
        str_buf[idx]=base::ToLowerASCII(str_buf[idx]);
    }
  }
  void StringPieceReplaceWith(base::StringPiece& str,char s,char t) {
    char* str_buf = (char*)str.data();
    for(size_t idx = 0;idx<str.length();idx++){
        if(str_buf[idx]==s)
          str_buf[idx]=t;
    }
  }
}

namespace abp{
//////////////////////////////////////////////////////////////////////////

class WildcardParser{
public:
  static Filter* FromText(const base::StringPiece& text){
    base::StringPiece rule = text;

    bool blocking = true;
    if(rule.find("@@") == 0){
      blocking = false;
      rule = rule.substr(2);
    }
    
    int content_type = -1; //Ä¬ÈÏÊÇCT_POPUP-1
    bool match_case =false; //Ä¬ÈÏ²»¹ØÐÄ´óÐ¡Ð´=
    base::StringPiece domains;
    base::StringPiece sitekeys;
    int thirdparty = 0;   //Ä¬ÈÏ²»¹ØÐÄÊÇ·ñÊÇÍâÁ´=
    bool collapse = true; //Ä¬ÈÏÒþ²ØÕ¼Î»·ûºÅ=
    std::vector<base::StringPiece> options;

    //parser options
    bool has_document_option = false;
    std::string::size_type option_start = rule.find("$");
    if(option_start!=std::string::npos){
      base::StringPiece match = rule.substr(option_start+1);
      rule = rule.substr(0,option_start);
      StringPieceToUpperASCII(match);//upper
      Tokenize(match,",",&options);
      for(size_t i=0;i<options.size();i++){
        base::StringPiece value;
        base::StringPiece option;

        //replace - with _
        option = options[i];
        StringPieceReplaceWith(option,'-','_');

        // split
        std::string::size_type separator_index = option.find("=");
        if(separator_index!=std::string::npos){
          value = option.substr(separator_index+1);
          option = option.substr(0,separator_index);
        }
        
        // init map
        InitTypeMap();
        ContentTypeMap& typeMap = g_type_map.Get();
        if(typeMap.find(option)!=typeMap.end()){
          int ctype = typeMap[option];
          if(content_type == -1){
            content_type = 0;
          }
          content_type|=ctype;
          if(ctype==CT_DOCUMENT){
            has_document_option=true;
          }
        }else if(option[0]=='~' 
            && typeMap.find(option.substr(1))!=typeMap.end()){
          if(content_type == -1){
            content_type = CT_POPUP-1; //all above FT_POPUP
          }
          content_type &= ~typeMap[option.substr(1)];
        }else if(option == "MATCH_CASE"){
          match_case = true;
        }
        else if (option == "DOMAIN" && value.length()){
          domains = value; //"|"
        }
        else if (option == "THIRD_PARTY"){
          thirdparty = 2;
        }
        else if (option == "~THIRD_PARTY"){
          thirdparty = 1;
        }
        else if (option == "COLLAPSE"){
          collapse = true;
        }
        else if (option == "~COLLAPSE"){
          collapse = false;
        }
        else if (option == "SITEKEY" && value.length()){
          sitekeys = value; //"|"
        }
      }
    }

    // È¥µôÄ¬ÈÏµÄ CT_DOCUMENT Èç¹û²»ÊÇ |scheme: »òÕß scheme:£¨±íÊ¾¿ªÍ·,chrome-extension://£©
    // origText: "@@||googleads.g.doubleclick.net/pagead/*^$domain=hon30.org"
    // |http:ÊÇÆ¥Åähttp https
    // ||xxxÊÇÆ¥Åä http://xxx https://xxx http://www.xxx https://www.xxx
    // ÕâÀïÒªÅÅ³ý|http: http:
    if(!blocking
        && (content_type==-1 || content_type&CT_DOCUMENT) 
        && (options.size()==0 ||!has_document_option) 
        && (!AbpIsStartWithScheme(rule)) 
        ){
      if(content_type==-1){
        content_type = CT_POPUP - 1;
      }
      content_type &= ~CT_DOCUMENT;
    }

    if(!blocking && sitekeys.length()){
      content_type = CT_DOCUMENT;
    }

    // Ä¬ÈÏÊÇCT_POPUP-1
    if(content_type==-1){
      content_type = CT_POPUP - 1;
    }

    // skip regex;¹æÔòÀïÃæÎªÁË±ÜÃâ³öÏÖÍ·Î²¶¼ÊÇ/¶¼ÔÚ/ºóÃæ¼ÓÁË*
    if(rule.length()>=2 && rule[0]=='/' && rule[rule.length()-1]=='/'){
      return new InvalidFilter(text,"not support regexp");
    }
    StringPieceToLowerASCII(rule);//lower

    if(blocking){
      return new WildcardFilter(text,rule,content_type,match_case,
        domains,thirdparty,collapse);
    }else{
      return new WildcardFilter(text,rule,content_type,match_case,
        domains,thirdparty,sitekeys,0);
    }
  }

private:
  static void InitTypeMap(){
    ContentTypeMap& typeMap = g_type_map.Get();
    if(!typeMap.size()){
      typeMap["OTHER"] = CT_OTHER;
      typeMap["SCRIPT"] = CT_SCRIPT;
      typeMap["IMAGE"] = CT_IMAGE;
      typeMap["STYLESHEET"] = CT_STYLESHEET;
      typeMap["OBJECT"] = CT_OBJECT;
      typeMap["SUBDOCUMENT"] = CT_SUBDOCUMENT;
      typeMap["DOCUMENT"] = CT_DOCUMENT;
      typeMap["XBL"] = CT_XBL;
      typeMap["PING"] = CT_PING;
      typeMap["XMLHTTPREQUEST"] = CT_XMLHTTPREQUEST;
      typeMap["OBJECT_SUBREQUEST"] = CT_OBJECT_SUBREQUEST;
      typeMap["DTD"] = CT_DTD;
      typeMap["MEDIA"] = CT_MEDIA;
      typeMap["FONT"] = CT_FONT;
      typeMap["BACKGROUND"] = CT_BACKGROUND;
      typeMap["POPUP"] = CT_POPUP;
      typeMap["DONOTTRACK"] = CT_DONOTTRACK;
      typeMap["ELEMHIDE"] = CT_ELEMHIDE;
    }
  }
};

//////////////////////////////////////////////////////////////////////////
class ElemHideParser{
public:
  static Filter* FromText(const base::StringPiece& text){
    base::StringPiece domain;
    base::StringPiece selector;
    bool exception = false;

    size_t idx = text.find("#@#");
    if(idx!=std::string::npos){
      exception = true;
      domain = text.substr(0,idx);
      StringPieceToUpperASCII(domain); //upper
      selector = text.substr(idx+3);
    }else{
      idx = text.find("##");
      domain = text.substr(0,idx);
      StringPieceToUpperASCII(domain); //upper
      selector = text.substr(idx+2);
    }

    //remove \, replace with space
    StringPieceReplaceWith(selector,'\\',' ');

    //Èç¹ûÃ»ÓÐÖ¸¶¨domain£¬ÓÅ»¯µô£¬½ÚÊ¡ÄÚ´æ=DMT_ALL=²»Ö§³ÖÁË=
    //¾ÍÊÇ##¿ªÍ·µÄ£¬²»Ö§³ÖÁË=
    if(!exception && !domain.size())
      return NULL;

    return new ElemHideFilter(text,domain,selector,exception);
  }
};

//////////////////////////////////////////////////////////////////////////
Filter* Parser::FromText(const base::StringPiece& text){
  Filter* ret = NULL;

  if(text[0]=='!'){
    ret = new CommentFilter(text);
  }
  else if(text.find("##")!=std::string::npos ||
      text.find("#@#")!=std::string::npos){
    ret = ElemHideParser::FromText(text);
  }
  else {
    ret = WildcardParser::FromText(text);
  }

  return ret;
};

//////////////////////////////////////////////////////////////////////////
} // end of namespace abp
