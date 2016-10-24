#include "abp/abp.h"
#include "abp/abp_parser.h"
#include "abp/abp_filter.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace abp{
//////////////////////////////////////////////////////////////////////////

TEST(abpParser_Test,ParserBase){
  scoped_ptr<Filter> filter;
  WildcardFilter* wildcard;
  ElemHideFilter* elemhide;

  //////////////////////////////////////////////////////////////////////////

  std::string t1="@@||google.com/uds/$script,subdocument,document,domain=netzwelt.de";
  filter.reset(Parser::FromText(t1));
  EXPECT_TRUE(filter.get());
  EXPECT_EQ(filter->Type(),FT_WHITELIST);
  wildcard = static_cast<WildcardFilter*>(filter.get());
  EXPECT_EQ(wildcard->Rule(),"||google.com/uds/");
  EXPECT_EQ(wildcard->RuleReally(),"/uds/*");
  EXPECT_EQ(wildcard->RuleDomain(),"google.com");
  EXPECT_EQ(wildcard->RuleDomainWithDot(),".google.com");
  EXPECT_EQ(wildcard->GetContentType(),CT_SCRIPT|CT_SUBDOCUMENT|CT_DOCUMENT);
  EXPECT_EQ(wildcard->GetMatchCase(),false);
  EXPECT_EQ(wildcard->GetSiteKeys(),"");
  EXPECT_EQ(wildcard->GetThirdParty(),0);
  EXPECT_EQ(wildcard->GetDomains(),"NETZWELT.DE");

  std::string t2="@@||googleadservices.com/pagead/aclk^$subdocument,domain=netzwelt.de";
  filter.reset(Parser::FromText(t2));
  EXPECT_TRUE(filter.get());
  EXPECT_EQ(filter->Type(),FT_WHITELIST);
  wildcard = static_cast<WildcardFilter*>(filter.get());
  EXPECT_EQ(wildcard->Rule(),"||googleadservices.com/pagead/aclk^");
  EXPECT_EQ(wildcard->RuleReally(),"/pagead/aclk^*");
  EXPECT_EQ(wildcard->RuleDomain(),"googleadservices.com");
  EXPECT_EQ(wildcard->RuleDomainWithDot(),".googleadservices.com");
  EXPECT_EQ(wildcard->GetContentType(),CT_SUBDOCUMENT);
  EXPECT_EQ(wildcard->GetMatchCase(),false);
  EXPECT_EQ(wildcard->GetSiteKeys(),"");
  EXPECT_EQ(wildcard->GetThirdParty(),0);
  EXPECT_EQ(wildcard->GetDomains(),"NETZWELT.DE");

  std::string t3="@@$sitekey=MFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBANnylWw2vLY4hUn9w06zQKbhKBfvjFUCsdFlb6TdQhxb9RXWXuI4t31c+o8fYOv/s8q1LGPga3DE1L/tHU4LENMCAwEAAQ,image,~image";
  filter.reset(Parser::FromText(t3));
  EXPECT_TRUE(filter.get());
  EXPECT_EQ(filter->Type(),FT_WHITELIST);
  wildcard = static_cast<WildcardFilter*>(filter.get());
  EXPECT_EQ(wildcard->Rule(),"");
  EXPECT_EQ(wildcard->RuleReally(),"*");
  EXPECT_EQ(wildcard->RuleDomain(),"");
  EXPECT_EQ(wildcard->RuleDomainWithDot(),"");
  EXPECT_EQ(wildcard->GetContentType(),CT_DOCUMENT);
  EXPECT_EQ(wildcard->GetMatchCase(),false);
  EXPECT_EQ(wildcard->GetSiteKeys(),StringToUpperASCII(std::string("MFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBANnylWw2vLY4hUn9w06zQKbhKBfvjFUCsdFlb6TdQhxb9RXWXuI4t31c+o8fYOv/s8q1LGPga3DE1L/tHU4LENMCAwEAAQ")));
  EXPECT_EQ(wildcard->GetThirdParty(),0);
  EXPECT_EQ(wildcard->GetDomains(),"");
  
  std::string t4="@@||googleads.g.doubleclick.net/pagead/*^$domain=hon30.org";
  filter.reset(Parser::FromText(t4));
  EXPECT_TRUE(filter.get());
  EXPECT_EQ(filter->Type(),FT_WHITELIST);
  wildcard = static_cast<WildcardFilter*>(filter.get());
  EXPECT_EQ(wildcard->Rule(),"||googleads.g.doubleclick.net/pagead/*^");
  EXPECT_EQ(wildcard->RuleReally(),"/pagead/*^*");
  EXPECT_EQ(wildcard->RuleDomain(),"googleads.g.doubleclick.net");
  EXPECT_EQ(wildcard->RuleDomainWithDot(),".googleads.g.doubleclick.net");
  EXPECT_EQ(wildcard->GetContentType(),(CT_POPUP-1)&~CT_DOCUMENT);
  EXPECT_EQ(wildcard->GetMatchCase(),false);
  EXPECT_EQ(wildcard->GetSiteKeys(),"");
  EXPECT_EQ(wildcard->GetThirdParty(),0);
  EXPECT_EQ(wildcard->GetDomains(),"HON30.ORG");
 
  std::string t5="@@||amazon.com^$elemhide";
  filter.reset(Parser::FromText(t5));
  EXPECT_TRUE(filter.get());
  EXPECT_EQ(filter->Type(),FT_WHITELIST);
  wildcard = static_cast<WildcardFilter*>(filter.get());
  EXPECT_EQ(wildcard->Rule(),"||amazon.com^");
  EXPECT_EQ(wildcard->RuleReally(),"^*");
  EXPECT_EQ(wildcard->RuleDomain(),"amazon.com");
  EXPECT_EQ(wildcard->RuleDomainWithDot(),".amazon.com");
  EXPECT_EQ(wildcard->GetContentType(),CT_ELEMHIDE);
  EXPECT_EQ(wildcard->GetMatchCase(),false);
  EXPECT_EQ(wildcard->GetSiteKeys(),"");
  EXPECT_EQ(wildcard->GetThirdParty(),0);
  EXPECT_EQ(wildcard->GetDomains(),"");
  
  //////////////////////////////////////////////////////////////////////////

  std::string t6="||suche.web.de/s.gif?jsession=c3VjaGUuYWJwMiZ*";
  filter.reset(Parser::FromText(t6));
  EXPECT_TRUE(filter.get());
  EXPECT_EQ(filter->Type(),FT_BLACKLIST);
  wildcard = static_cast<WildcardFilter*>(filter.get());
  EXPECT_EQ(wildcard->Rule(),StringToLowerASCII(std::string("||suche.web.de/s.gif?jsession=c3VjaGUuYWJwMiZ*")));
  EXPECT_EQ(wildcard->RuleReally(),StringToLowerASCII(std::string("/s.gif?jsession=c3VjaGUuYWJwMiZ*")));
  EXPECT_EQ(wildcard->RuleDomain(),"suche.web.de");
  EXPECT_EQ(wildcard->RuleDomainWithDot(),".suche.web.de");
  EXPECT_EQ(wildcard->GetContentType(),CT_POPUP-1);
  EXPECT_EQ(wildcard->GetMatchCase(),false);
  EXPECT_EQ(wildcard->GetCollapse(),true);
  EXPECT_EQ(wildcard->GetThirdParty(),0);
  EXPECT_EQ(wildcard->GetDomains(),"");

  std::string t7="||acg.178.com^*/t_$subdocument";
  filter.reset(Parser::FromText(t7));
  EXPECT_TRUE(filter.get());
  EXPECT_EQ(filter->Type(),FT_BLACKLIST);
  wildcard = static_cast<WildcardFilter*>(filter.get());
  EXPECT_EQ(wildcard->Rule(),"||acg.178.com^*/t_");
  EXPECT_EQ(wildcard->RuleReally(),"^*/t_*");
  EXPECT_EQ(wildcard->RuleDomain(),"acg.178.com");
  EXPECT_EQ(wildcard->RuleDomainWithDot(),".acg.178.com");
  EXPECT_EQ(wildcard->GetContentType(),CT_SUBDOCUMENT);
  EXPECT_EQ(wildcard->GetMatchCase(),false);
  EXPECT_EQ(wildcard->GetCollapse(),true);
  EXPECT_EQ(wildcard->GetThirdParty(),0);
  EXPECT_EQ(wildcard->GetDomains(),"");
  
  std::string t8=".com/*/p$script,domain=99770.cc|99comic.com|99manga.com";
  filter.reset(Parser::FromText(t8));
  EXPECT_TRUE(filter.get());
  EXPECT_EQ(filter->Type(),FT_BLACKLIST);
  wildcard = static_cast<WildcardFilter*>(filter.get());
  EXPECT_EQ(wildcard->Rule(),".com/*/p");
  EXPECT_EQ(wildcard->RuleReally(),"*.com/*/p*");
  EXPECT_EQ(wildcard->RuleDomain(),"");
  EXPECT_EQ(wildcard->RuleDomainWithDot(),"");
  EXPECT_EQ(wildcard->GetContentType(),CT_SCRIPT);
  EXPECT_EQ(wildcard->GetMatchCase(),false);
  EXPECT_EQ(wildcard->GetCollapse(),true);
  EXPECT_EQ(wildcard->GetThirdParty(),0);
  EXPECT_EQ(wildcard->GetDomains(),"99770.CC|99COMIC.COM|99MANGA.COM");

  std::string t9="|.com/*/p|$script,domain=99770.cc|99comic.com|99manga.com";
  filter.reset(Parser::FromText(t9));
  EXPECT_TRUE(filter.get());
  EXPECT_EQ(filter->Type(),FT_BLACKLIST);
  wildcard = static_cast<WildcardFilter*>(filter.get());
  EXPECT_EQ(wildcard->Rule(),"|.com/*/p|");
  EXPECT_EQ(wildcard->RuleReally(),".com/*/p");
  EXPECT_EQ(wildcard->RuleDomain(),"");
  EXPECT_EQ(wildcard->RuleDomainWithDot(),"");
  EXPECT_EQ(wildcard->GetContentType(),CT_SCRIPT);
  EXPECT_EQ(wildcard->GetMatchCase(),false);
  EXPECT_EQ(wildcard->GetCollapse(),true);
  EXPECT_EQ(wildcard->GetThirdParty(),0);
  EXPECT_EQ(wildcard->GetDomains(),"99770.CC|99COMIC.COM|99MANGA.COM");

  std::string t10=".com/*/p|$script,domain=99770.cc|99comic.com|99manga.com";
  filter.reset(Parser::FromText(t10));
  EXPECT_TRUE(filter.get());
  EXPECT_EQ(filter->Type(),FT_BLACKLIST);
  wildcard = static_cast<WildcardFilter*>(filter.get());
  EXPECT_EQ(wildcard->Rule(),".com/*/p|");
  EXPECT_EQ(wildcard->RuleReally(),"*.com/*/p");
  EXPECT_EQ(wildcard->RuleDomain(),"");
  EXPECT_EQ(wildcard->RuleDomainWithDot(),"");
  EXPECT_EQ(wildcard->GetContentType(),CT_SCRIPT);
  EXPECT_EQ(wildcard->GetMatchCase(),false);
  EXPECT_EQ(wildcard->GetCollapse(),true);
  EXPECT_EQ(wildcard->GetThirdParty(),0);
  EXPECT_EQ(wildcard->GetDomains(),"99770.CC|99COMIC.COM|99MANGA.COM");

  std::string t11="|.com/*/p$script,domain=99770.cc|99comic.com|99manga.com";
  filter.reset(Parser::FromText(t11));
  EXPECT_TRUE(filter.get());
  EXPECT_EQ(filter->Type(),FT_BLACKLIST);
  wildcard = static_cast<WildcardFilter*>(filter.get());
  EXPECT_EQ(wildcard->Rule(),"|.com/*/p");
  EXPECT_EQ(wildcard->RuleReally(),".com/*/p*");
  EXPECT_EQ(wildcard->RuleDomain(),"");
  EXPECT_EQ(wildcard->RuleDomainWithDot(),"");
  EXPECT_EQ(wildcard->GetContentType(),CT_SCRIPT);
  EXPECT_EQ(wildcard->GetMatchCase(),false);
  EXPECT_EQ(wildcard->GetCollapse(),true);
  EXPECT_EQ(wildcard->GetThirdParty(),0);
  EXPECT_EQ(wildcard->GetDomains(),"99770.CC|99COMIC.COM|99MANGA.COM");

//////////////////////////////////////////////////////////////////////////

  std::string t12="~timeoutbengaluru.net,~timeoutdelhi.net,~timeoutmumbai.net##.promoAd";
  filter.reset(Parser::FromText(t12));
  EXPECT_TRUE(filter.get());
  EXPECT_EQ(filter->Type(),FT_ELEMHIDE);
  elemhide = static_cast<ElemHideFilter*>(filter.get());
  EXPECT_EQ(elemhide->GetSelector(),".promoAd");
  EXPECT_EQ(elemhide->GetDomains(),StringToUpperASCII(std::string("~timeoutbengaluru.net,~timeoutdelhi.net,~timeoutmumbai.net")));
  EXPECT_FALSE(elemhide->IsActiveOnHost("timeoutbengaluru.net"));
  EXPECT_FALSE(elemhide->IsActiveOnHost("www.timeoutbengaluru.net"));
  EXPECT_TRUE(elemhide->IsActiveOnHost("timeoutbengaluru.net.cn"));
/* Â²Â»ÃÂ§Â³Ã##Â¿ÂªÃÂ·ÂµÃÃÃ=
  std::string t13="###AdArea";
  filter.reset(Parser::FromText(t13));
  EXPECT_TRUE(filter.get());
  EXPECT_EQ(filter->Type(),FT_ELEMHIDE);
  elemhide = static_cast<ElemHideFilter*>(filter.get());
  EXPECT_EQ(elemhide->GetSelector(),"#AdArea");
  EXPECT_EQ(elemhide->GetDomains(),"");
  EXPECT_TRUE(elemhide->IsActiveOnHost("timeoutbengaluru.net"));
  EXPECT_TRUE(elemhide->IsActiveOnHost("www.timeoutbengaluru.net"));
  EXPECT_TRUE(elemhide->IsActiveOnHost("timeoutbengaluru.net.cn"));
*/
  
}

//////////////////////////////////////////////////////////////////////////
}
