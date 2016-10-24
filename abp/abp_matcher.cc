#include "abp/abp_matcher.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/string_util.h"

namespace abp{
//////////////////////////////////////////////////////////////////////////

// ÓÃmultimapÄ£Äâhashtable
void Matcher::Add(Filter* filter){
  if(keyword_by_filter_.find(filter->Text())!=keyword_by_filter_.end())
    return;

  DCHECK(filter->Type()==FT_WHITELIST || filter->Type()==FT_BLACKLIST);
  base::StringPiece keyword = FindKeyword(static_cast<WildcardFilter*>(filter));
  //DCHECK(keyword.length());
  //ÕÒ²»µ½·Ö´ÊµÄ£¬¼ÓÈëµ½""ÀïÃæ×ö¸ö´óÁ´±í£¬±ÈÈç@@||so.com$document,ÆäÊµ
  //@@||so.com^$document¾Í¿ÉÒÔ¹æ±Ü=
  filter_by_keyword_.insert(std::make_pair(keyword,filter));

  keyword_by_filter_[filter->Text()]=keyword;
}

Filter* Matcher::MatchByKeyword(const std::string& keyword,const std::string& req_url,
    ContentType content_type,const std::string& doc_host,bool third_party){
  typedef std::multimap<base::StringPiece,Filter*>::const_iterator CIT;
  typedef std::pair<CIT,CIT> Range;
  Range range= filter_by_keyword_.equal_range(keyword);
  for(CIT i=range.first; i!=range.second; ++i){
    WildcardFilter* filter = static_cast<WildcardFilter*>(i->second);
    if(filter->Matches(req_url,content_type,doc_host,third_party)){
      return filter;
    }
  }
    
  return NULL;
}

void Matcher::Reset(){
  filter_by_keyword_.clear();
  keyword_by_filter_.clear();
}
  
bool Matcher::HitKeyword(const std::string& keyword){
  return filter_by_keyword_.find(keyword)!=filter_by_keyword_.end();
}

// ÌáÈ¡¹æÔòµÄ¹Ø¼ü´Ê=
// Ã¿¸ö¹æÔòÖ»ÌáÈ¡Ò»¸ö¹Ø¼ü´Ê
base::StringPiece Matcher::FindKeyword(WildcardFilter* filter){
  // ÏÈ·Ö´Ê
  std::vector<base::StringPiece> candidates;
  AbpGetCandidatesWithDelimiter(filter->Rule(),candidates);

  // Æ½ºâËã·¨,ÓÅÏÈÑ¡Ôñ¶ÔÓ¦Í°³¤¶È¶ÌµÄ
  // Èç¹ûÍ°³¤¶ÈÏàÍ¬£¬Ñ¡Ôñ³¤¶È³¤µÄ¹Ø¼ü´Ê
  base::StringPiece result;
  int result_count = 16777215;
  size_t result_len = 0;
  for(size_t i=0;i<candidates.size();i++){
    base::StringPiece candidate = candidates[i].substr(1);
    int count = filter_by_keyword_.count(candidate);
    if (count < result_count 
        ||(count == result_count && candidate.length() > result_len)) {
      result = candidate;
      result_count = count;
      result_len = candidate.length();
    }
  }

  return result;
}

//////////////////////////////////////////////////////////////////////////

void CombinedMatcher::Add(Filter* filter){
  if(filter->Type() == FT_BLACKLIST){
    blacklist_matcher_.Add(filter);
  }
  else{
    WildcardFilter* wfilter = static_cast<WildcardFilter*>(filter);
    base::StringPiece sitekeys = wfilter->GetSiteKeys();
    if(sitekeys.length()){
      std::vector<base::StringPiece> keymap;
      base::Tokenize(sitekeys,"|",&keymap);
      for(size_t i=0;i<keymap.size();i++){
        sitekeys_[keymap[i]]=filter;
      }
    }else{
      whitelist_matcher_.Add(filter);
    }
  }

  // hack!!!
  if(result_cache_.size())
    result_cache_.clear();
}

Filter* CombinedMatcher::MatchAny(const std::string& req_url,ContentType content_type,
  const std::string& doc_host,bool third_party){
  // first search in cache
  std::string key = base::StringPrintf("%s+%d+%s+%d",req_url.c_str(),content_type,
    doc_host.c_str(),third_party);
  if(result_cache_.find(key)!=result_cache_.end())
    return result_cache_[key];

  // match internal
  Filter* ret = MatchAnyInternal(req_url,content_type,
    doc_host,third_party);

  // cache result
  result_cache_[key] = ret;
  return ret;
}

Filter* CombinedMatcher::MatchByKey(const std::string& req_url,const std::string& k, 
    const std::string& doc_host){
  std::string key = base::ToUpperASCII(k);
  if(sitekeys_.find(key)!=sitekeys_.end()){
    WildcardFilter* filter = static_cast<WildcardFilter*>(sitekeys_[key]);
    if(filter && filter->Matches(req_url,CT_DOCUMENT,doc_host,false)){
      return filter;
    }
  }

  return NULL;
}

void CombinedMatcher::Reset(){
  whitelist_matcher_.Reset();
  blacklist_matcher_.Reset();
  result_cache_.clear();
  sitekeys_.clear();
}

// ÏÈ¶Ôurl×ö·Ö´Ê£¬ÕÒµ½¿ÉÄÜµÄ¹æÔò£¬Æ¥Åä
// ÏÈÆ¥Åä°×Ãûµ¥£¬È»ºóºÚÃûµ¥
// °×Ãûµ¥¶¼Ã»ÓÐÆ¥Åäµ½£¬²ÅÈ¥Æ¥ÅäºÚÃûµ¥=
Filter* CombinedMatcher::MatchAnyInternal(const std::string& req_url,ContentType content_type,
    const std::string& doc_host,bool third_party){
  //get candidates
  std::vector<std::string> candidates;
  AbpGetCandidates(req_url,candidates);
  candidates.push_back(""); //hack,Ìí¼ÓÒ»¸ö"",¶ÔÓÚ¹æÔò·Ö²»³ö´ÊµÄ¶¼ÓÃ""À´×ö=

  Filter* blacklist_ret = NULL;
  for(size_t i=0;i<candidates.size();i++){
    std::string keyword = candidates[i];

    // first whitelist
    if(whitelist_matcher_.HitKeyword(keyword)){
      Filter* ret = whitelist_matcher_.MatchByKeyword(keyword,req_url,content_type,
        doc_host,third_party);
      if(ret){
        return ret;
      }
    }

    // then blacklist
    if(!blacklist_ret && blacklist_matcher_.HitKeyword(keyword))
      blacklist_ret = blacklist_matcher_.MatchByKeyword(keyword,req_url,content_type,
      doc_host,third_party);
  }

  return blacklist_ret;
}

//////////////////////////////////////////////////////////////////////////
void ElemHider::Add(Filter* filter){
  if(filter->Type()==FT_HIDEEXCEPTION){
    ElemHideFilter* hfilter = static_cast<ElemHideFilter*>(filter);
    base::StringPiece selector = hfilter->GetSelector();
    exception_by_selector_.insert(std::make_pair(selector,filter));
  }else{
    elemhide_filter_.push_back(filter);
  }
}
void ElemHider::GetSelectorsForHost(const std::string& host, DomainMatchType match_type,
    std::vector<std::string>& css){
  for(size_t idx = 0;idx < elemhide_filter_.size();idx++){
    Filter* filter = elemhide_filter_[idx];
    ElemHideFilter* hfilter = static_cast<ElemHideFilter*>(filter);

    //Ã»ÓÐÖ¸¶¨domainsºÍÖ»Ö¸¶¨ÁË·´×ª¹æÔòµÄÂÔµÄ¹æÔò
    if(match_type == DMT_JUST_INCLUDE && hfilter->IsActiveOnHost("")){
      continue;
    }

    //Ã»ÓÐÖ¸¶¨domainsµÄ¹æÔòÂÔ
    if(match_type == DMT_INCLUDE_EXCLUDE && hfilter->GetDomains().length()==0){
      continue;
    }

    if(hfilter->IsActiveOnHost(host) && !HitException(hfilter,host)){
      std::string s;
      hfilter->GetSelector().CopyToString(&s);
      css.push_back(s);
    }
  }
}
void ElemHider::Reset(){
  exception_by_selector_.clear();
  elemhide_filter_.clear();
}

ElemHideFilter* ElemHider::HitException(ElemHideFilter* filter,const std::string& host){
  base::StringPiece selector = filter->GetSelector();
  if(exception_by_selector_.find(selector)==exception_by_selector_.end()){
    return NULL;
  }
  typedef std::multimap<base::StringPiece,Filter*>::const_iterator CIT;
  typedef std::pair<CIT,CIT> Range;
  Range range= exception_by_selector_.equal_range(selector);
  for(CIT i=range.first; i!=range.second; ++i){
    ElemHideFilter* hfilter = static_cast<ElemHideFilter*>(i->second);
    if(hfilter->IsActiveOnHost(host)){
      return hfilter;
    }
  }
  return NULL;
}


//////////////////////////////////////////////////////////////////////////
}