#pragma once 

#include "abp/abp_export.h"
#include "abp/abp.h"
#include "abp/abp_filter.h"

#include <map>

namespace abp{
//////////////////////////////////////////////////////////////////////////

class ABP_EXPORT Matcher{
public:
  void Add(Filter* filter);

  Filter* MatchByKeyword(const std::string& keyword,const std::string& req_url,
      ContentType content_type,const std::string& doc_host,bool third_party);

  void Reset();
  
  bool HitKeyword(const std::string& keyword);

private:
  base::StringPiece FindKeyword(WildcardFilter* filter);

private:
  std::multimap<base::StringPiece,Filter*> filter_by_keyword_;
  std::map<base::StringPiece,base::StringPiece> keyword_by_filter_;
};

//////////////////////////////////////////////////////////////////////////
class ABP_EXPORT CombinedMatcher{
public:
  void Add(Filter* filter);

  Filter* MatchAny(const std::string& req_url,ContentType content_type,
    const std::string& doc_host,bool third_party);

  Filter* MatchByKey(const std::string& req_url,const std::string& key,
    const std::string& doc_host);

  void Reset();

private:
  Filter* MatchAnyInternal(const std::string& req_url,ContentType content_type,
      const std::string& doc_host,bool third_party);

private:
  Matcher whitelist_matcher_;
  Matcher blacklist_matcher_;
  std::map<std::string,Filter*> result_cache_; //ÕâÀï×öcacheÊÇ²»ÖªµÀÉÏÃæÂß¼­»á¸ã³öÊ²Ã´¶ñÐÄµÄÊÂÇé=
  std::map<base::StringPiece,Filter*> sitekeys_;
};

//////////////////////////////////////////////////////////////////////////
class ABP_EXPORT ElemHider{
public:
  void Add(Filter* filter);

  void GetSelectorsForHost(const std::string& host, DomainMatchType match_type,
      std::vector<std::string>& css);

  void Reset();

private:
  ElemHideFilter* HitException(ElemHideFilter* filter,const std::string& host);

private:
  std::multimap<base::StringPiece,Filter*> exception_by_selector_;
  std::vector<Filter*> elemhide_filter_;
};

//////////////////////////////////////////////////////////////////////////
}

