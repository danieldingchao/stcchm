#pragma once 

#include "abp/abp_export.h"
#include "abp/abp.h"
#include "base/logging.h"
#include <map>

namespace abp{
//////////////////////////////////////////////////////////////////////////

class ABP_EXPORT InvalidFilter : public Filter{
public:
  explicit InvalidFilter(const base::StringPiece& text,const base::StringPiece& reason)
    :Filter(text,FT_INVALID),reason_(reason){
  }
  base::StringPiece Reason(){
    return reason_;
  };
private:
  base::StringPiece reason_;
};

//////////////////////////////////////////////////////////////////////////
class ABP_EXPORT CommentFilter : public Filter{
public:
  explicit CommentFilter(const base::StringPiece& text)
    :Filter(text,FT_COMMENT){
  }
};

//////////////////////////////////////////////////////////////////////////

class ABP_EXPORT ActiveDomainHelper{
public:
  explicit ActiveDomainHelper(const base::StringPiece& domain){
      InitActiveDomains(domain);
  }
  bool IsActiveOnHost(const std::string& doc_host);

private:
  void InitActiveDomains(const base::StringPiece& domain);

  std::map<base::StringPiece,bool> active_domains_; //upper
};

//////////////////////////////////////////////////////////////////////////

class ABP_EXPORT WildcardFilter : public Filter{
public:
  //blacklist
  explicit WildcardFilter(const base::StringPiece& text,const base::StringPiece& rule,
      int content_type,bool match_case,const base::StringPiece& domains,
      int thirdparty,bool collapse)
    :Filter(text,FT_BLACKLIST),domains_(domains),
    rule_(rule),
    content_type_(content_type),
    match_case_(match_case),
    thirdparty_(thirdparty),
    collpase_(collapse){
  }

  //whitelist
  explicit WildcardFilter(const base::StringPiece& text,const base::StringPiece& rule,
      int content_type,bool match_case,const base::StringPiece& domains,
      int thirdparty,const base::StringPiece& sitekeys,int /*placeholder*/)
    :Filter(text,FT_WHITELIST),domains_(domains),
    rule_(rule),
    content_type_(content_type),
    match_case_(match_case),
    thirdparty_(thirdparty),
    sitekeys_(sitekeys){
  }

  bool Matches(const std::string& req_url,ContentType content_type,
      const std::string& doc_host,bool third_party);

  base::StringPiece Rule(){
    return rule_;
  }
  std::string RuleReally(){
    std::string rule_really,rule_domain,rule_domain_withdot;
    InitRules(rule_really,rule_domain,rule_domain_withdot);
    return rule_really;
  }
  std::string RuleDomain(){
    std::string rule_really,rule_domain,rule_domain_withdot;
    InitRules(rule_really,rule_domain,rule_domain_withdot);
    return rule_domain;
  }
  std::string RuleDomainWithDot(){
    std::string rule_really,rule_domain,rule_domain_withdot;
    InitRules(rule_really,rule_domain,rule_domain_withdot);
    return rule_domain_withdot;
  }
  int GetContentType(){
    return content_type_;
  }
  bool GetMatchCase(){
    return !!match_case_;
  }
  int GetThirdParty(){
    return thirdparty_;
  }
  bool GetCollapse(){
    DCHECK(Type()==FT_BLACKLIST);
    return !!collpase_;
  }
  base::StringPiece GetSiteKeys(){
    DCHECK(Type()==FT_WHITELIST);
    return sitekeys_;
  }
  base::StringPiece GetDomains(){
    return domains_;
  }

  bool IsActiveOnHost(const std::string& doc_host){
    if(!domains_.size())
      return true;

    if(!domains_helper_.get())
      domains_helper_.reset(new ActiveDomainHelper(domains_));

    return domains_helper_->IsActiveOnHost(doc_host);
  }

private:
  void InitRules(std::string& rule_really,std::string& rule_domain,std::string& rule_domain_withdot);
  size_t GetDomainFromRule(const std::string& r);

private:
  base::StringPiece rule_;
  base::StringPiece sitekeys_;  //whitelist,upper
  base::StringPiece domains_;//upper
  std::auto_ptr<ActiveDomainHelper> domains_helper_;

  int content_type_;
  unsigned int thirdparty_:2;       //0:ignore,1:no,2,yes
  unsigned int collpase_:1;         //blacklist,unused
  unsigned int match_case_:1;       //unused,always lower,2013.8.14
};

//////////////////////////////////////////////////////////////////////////

class ElemHideFilter : public Filter{
public:
  explicit ElemHideFilter(const base::StringPiece& text,const base::StringPiece& domain,
      const base::StringPiece& selector,bool exception)
      : Filter(text,exception?FT_HIDEEXCEPTION:FT_ELEMHIDE),
        selector_(selector),
        domains_(domain){
  }

  base::StringPiece GetSelector(){
    return selector_;
  }

  base::StringPiece GetDomains(){
    return domains_;
  }

  bool IsActiveOnHost(const std::string& doc_host){
    if(!domains_.size())
      return true;

    if(!domains_helper_.get())
      domains_helper_.reset(new ActiveDomainHelper(domains_));

    return domains_helper_->IsActiveOnHost(doc_host);
  }

private:
  base::StringPiece selector_;
  base::StringPiece domains_;//upper
  std::auto_ptr<ActiveDomainHelper> domains_helper_;
};


//////////////////////////////////////////////////////////////////////////
}
