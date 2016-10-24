#include "abp/abp_filter.h"

#include "base/strings/string_util.h"
#include "url/gurl.h"

namespace abp{
//////////////////////////////////////////////////////////////////////////

bool ActiveDomainHelper::IsActiveOnHost(const std::string& doc_host){
  // check
  if(!doc_host.length()){
    return active_domains_[""];
  }

  if(doc_host.length()==1 && doc_host[0]=='*'){
    return true;
  }

  std::string domain = doc_host;
  if(domain[domain.length()-1]=='.'){
    domain = domain.substr(0,domain.length()-1);
  }
  domain = base::ToUpperASCII(domain);
  while (true){
    if (active_domains_.find(domain)!=active_domains_.end()){
      return active_domains_[domain];
    }
    std::string::size_type nextDot = domain.find(".");
    if (nextDot == std::string::npos){
      break;
    }
    domain = domain.substr(nextDot + 1);
  }
  return active_domains_[""];
}

void ActiveDomainHelper::InitActiveDomains(const base::StringPiece& domain){
  DCHECK(domain.size());
  std::vector<base::StringPiece> domain_vector;
  base::Tokenize(domain,"|,",&domain_vector);

  if(domain_vector.size()==1 && domain_vector[0][0]!='~'){
    active_domains_[""] = false;

    base::StringPiece domain = domain_vector[0];
    if(domain[domain.length()-1]=='.'){
      domain = domain.substr(0,domain.length()-1);
    }
    active_domains_[domain] = true;
  }else{
    bool hasIncludes = false;
    for (size_t i = 0; i < domain_vector.size(); i++){
      base::StringPiece  domain = domain_vector[i];
      if(domain[domain.length()-1]=='.'){
        domain = domain.substr(0,domain.length()-1);
      }
      if (domain == ""){
        continue;
      }

      bool include;
      if (domain[0] == '~'){
        include = false;
        domain = domain.substr(1);
      }
      else{
        include = true;
        hasIncludes = true;
      }
      active_domains_[domain] = include;
    }
    active_domains_[""] = !hasIncludes;
  }

  if(!active_domains_.size()){
    active_domains_[""] = true;
  }
}

//////////////////////////////////////////////////////////////////////////

bool WildcardFilter::Matches(const std::string& req_url,ContentType content_type,
    const std::string& doc_host,bool third_party) {
  // third party
  if(thirdparty_>0 && ((unsigned int)third_party+1)!=thirdparty_){
    if(doc_host.length()==1 && doc_host[0]=='*'){
    }else{
      return false;
    }
  }

  // content type
  if(!(content_type_&content_type)){
    return false;
  }

  // host
  if(!IsActiveOnHost(doc_host)){
    return false;
  }

  // invalid rule
  std::string rule_really,rule_domain,rule_domain_withdot;
  InitRules(rule_really,rule_domain,rule_domain_withdot);
  if(rule_really.length()==0){
    NOTREACHED();
    return false;
  }

  //
  //  source = source.replace(/\^\|$/, "^")
  //    .replace(/\W/g, "\\$&")
  //    .replace(/\\\*/g, ".*")
  //    .replace(/\\\^/g, "(?:[\\x00-\\x24\\x26-\\x2C\\x2F\\x3A-\\x40\\x5B-\\x5E\\x60\\x7B-\\x80]|$)")
  //    .replace(/^\\\|\\\|/, "^[\\w\\-]+:\\/+(?!\\/)(?:[^.\\/]+\\.)*?")
  //    .replace(/^\\\|/, "^")
  //    .replace(/\\\|$/, "$");
  //

  std::string url_really = req_url;
  if(rule_.length()>=2 && rule_[0]=='|' && rule_[1]=='|'){
    std::string host = AbpExtractHostFromUrl(req_url);
    if(host.length() < rule_domain.length()){
      return false;
    }
    if(host.length()>rule_domain.length()){
      if(!base::EndsWith(host,rule_domain_withdot,GetMatchCase() ? base::CompareCase::SENSITIVE : base::CompareCase::INSENSITIVE_ASCII)){
        return false;
      }
    }
    else if(!base::EndsWith(host,rule_domain,GetMatchCase() ? base::CompareCase::SENSITIVE : base::CompareCase::INSENSITIVE_ASCII)){
      return false;
    }
    std::string::size_type idx = req_url.find(":");
    if(idx==std::string::npos){
      NOTREACHED();
      return false;
    }
    for(;idx<req_url.length();idx++){
      if(base::IsAsciiAlpha(req_url[idx]) || base::IsAsciiDigit(req_url[idx])){
        break;
      }
    }
    idx = idx + host.length();
    if(idx > req_url.length()){
      NOTREACHED();
      return false;
    }
    url_really = req_url.substr(idx);
  }

  return AbpWildcardMatches(rule_really.c_str(),url_really.c_str(),GetMatchCase());
}

size_t WildcardFilter::GetDomainFromRule(const std::string& r){
  std::string rule = base::ToLowerASCII(r);
  for(size_t i=2;i<rule.length();i++){
    if(!AbpIsLowAlphaOrDigitOrSpeical(r[i],"-."))
      return i;
  }
  return rule.length();
}

void WildcardFilter::InitRules(std::string& rule_really,std::string& rule_domain,std::string& rule_domain_withdot){
  if(rule_.length()==0){
    rule_really="*";
    return;
  }

  //start
  std::string rule_str;
  rule_.CopyToString(&rule_str);
  if(rule_str.length()>=2 && rule_str[0]=='|' && rule_str[1]=='|'){
    size_t domain_end = GetDomainFromRule(rule_str);
    rule_domain = rule_str.substr(2,domain_end-2);
    rule_domain_withdot = std::string(".")+rule_domain;

    rule_really = rule_str.substr(domain_end);
  }else if(rule_[0]=='|'){
    rule_really = rule_str.substr(1);
  }else{
    if(rule_[0]!='*'){
      rule_really = std::string("*")+rule_str;
    }else{
      rule_really = rule_str;
    }
  }

  //end
  if(rule_[rule_.length()-1]=='|'){
    rule_really = rule_really.substr(0,rule_really.length()-1);
  }
  else{
    if(rule_[rule_.length()-1]!='*'){
      rule_really = rule_really.append("*");
    }
  }
}

//////////////////////////////////////////////////////////////////////////
}
