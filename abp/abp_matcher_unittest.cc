#include "abp/abp.h"

#include "abp/abp_matcher.h"
#include "abp/abp_parser.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <Windows.h>

namespace abp{
//////////////////////////////////////////////////////////////////////////

TEST(abpMatcher_Test,MatcherABC){
  scoped_ptr<FilterMgr> filtermgr;
  filtermgr.reset(FilterMgr::NewInstance());
  //Sleep(10*1000);
  filtermgr->InitForDebug(); 
  //Sleep(10*1000);
  filtermgr.reset();
  //Sleep(10*1000);
}

TEST(abpMatcher_Test,MatcherBase){
  scoped_ptr<Filter> filter;
  WildcardFilter* wildcard;
  Filter* match_filter;
  Filter* match_wildcard;
  scoped_ptr<ElemHider> elemhide;
  std::vector<std::string> css;
  scoped_ptr<CombinedMatcher> matcher;

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

  //////////////////////////////////////////////////////////////////////////
  matcher.reset(new CombinedMatcher());
  matcher->Add(wildcard);

  match_filter = matcher->MatchAny("http://www.google.com/uds/ad.js",CT_SCRIPT,"netzwelt.de",true);
  EXPECT_TRUE(match_filter);
  EXPECT_EQ(match_filter->Type(),FT_WHITELIST);
  match_wildcard = static_cast<WildcardFilter*>(match_filter);
  EXPECT_EQ(match_wildcard,wildcard);

  match_filter = matcher->MatchAny("http://www.google.com/uds/ad.html",CT_SUBDOCUMENT,"netzwelt.de",true);
  EXPECT_TRUE(match_filter);
  EXPECT_EQ(match_filter->Type(),FT_WHITELIST);
  match_wildcard = static_cast<WildcardFilter*>(match_filter);
  EXPECT_EQ(match_wildcard,wildcard);


  //////////////////////////////////////////////////////////////////////////
  std::string t2="@@||so.com$document";
  filter.reset(Parser::FromText(t2));
  EXPECT_TRUE(filter.get());
  EXPECT_EQ(filter->Type(),FT_WHITELIST);
  wildcard = static_cast<WildcardFilter*>(filter.get());
  matcher.reset(new CombinedMatcher());
  matcher->Add(wildcard);

  match_filter = matcher->MatchAny("http://www.so.com",CT_DOCUMENT,"",false);
  EXPECT_TRUE(match_filter);


  //////////////////////////////////////////////////////////////////////////
  css.clear();
  elemhide.reset(new ElemHider());
  std::string t3="~timeoutbengaluru.net,~timeoutdelhi.net,~timeoutmumbai.net##.promoAd";
  filter.reset(Parser::FromText(t3));
  EXPECT_TRUE(filter.get());
  EXPECT_EQ(filter->Type(),FT_ELEMHIDE);
  elemhide->Add(filter.get());
  elemhide->GetSelectorsForHost("www.sina.com.cn",DMT_JUST_INCLUDE,css);
  EXPECT_EQ(css.size(),0);
  css.clear();
  elemhide->GetSelectorsForHost("www.sina.com.cn",DMT_INCLUDE_EXCLUDE,css);
  EXPECT_EQ(css.size(),1);

  //////////////////////////////////////////////////////////////////////////
  css.clear();
  elemhide.reset(new ElemHider());
  std::string t4 = "timeoutbengaluru.net,~timeoutdelhi.net,~timeoutmumbai.net##.promoAd";
  filter.reset(Parser::FromText(t4));
  EXPECT_TRUE(filter.get());
  EXPECT_EQ(filter->Type(),FT_ELEMHIDE);
  elemhide->Add(filter.get());
  elemhide->GetSelectorsForHost("timeoutbengaluru.net",DMT_JUST_INCLUDE,css);
  EXPECT_EQ(css.size(),1);
  EXPECT_EQ(css[0],".promoAd");
/*
  //////////////////////////////////////////////////////////////////////////
  css.clear();
  elemhide.reset(new ElemHider());
  std::string t5="yocoy.com#@##AdArea";
  elemhide->Add(Parser::FromText(t5));  //leak
  std::string t6="###AdArea";
  elemhide->Add(Parser::FromText(t6)); //leak
  elemhide->GetSelectorsForHost("www.sina.com.cn",DMT_ALL,css);
  EXPECT_EQ(css.size(),1);
  EXPECT_EQ(css[0],"#AdArea");
*/
  //////////////////////////////////////////////////////////////////////////
  css.clear();
  std::string t7="||wiseie.com^$third-party";
  filter.reset(Parser::FromText(t7));
  EXPECT_TRUE(filter.get());
  EXPECT_EQ(filter->Type(),FT_BLACKLIST);
  wildcard = static_cast<WildcardFilter*>(filter.get());
  EXPECT_EQ(wildcard->Rule(),"||wiseie.com^");
  EXPECT_EQ(wildcard->GetContentType(),CT_POPUP-1);
  EXPECT_EQ(wildcard->GetThirdParty(),2);
  matcher.reset(new CombinedMatcher());
  matcher->Add(wildcard);
  match_filter = matcher->MatchAny("http://www.wiseie.com/css/index.css",CT_STYLESHEET,"www.wiseie.com",false);
  EXPECT_FALSE(match_filter);
  match_filter = matcher->MatchAny("http://www.wiseie.com/css/index.css",CT_STYLESHEET,"www.wiseiexxx.com",true);
  EXPECT_TRUE(match_filter);
}

TEST(abpMatcher_Test,FilterMgrBase){
  scoped_ptr<FilterMgr> filtermgr;
  Filter* match_filter;
  Filter* match_wildcard;
  std::vector<std::string> css;
 
  //||google.com/jsapi?autoload=*%22ads%22$script,domain=youtube.com
  filtermgr.reset(FilterMgr::NewInstance());
  filtermgr->InitForDebug(); 

  match_filter = filtermgr->MatchAny("http://www.google.com/jsapi?autoload=xxx%22ads%22",CT_SCRIPT,"www.youtube.com",true);
  EXPECT_TRUE(match_filter);
  EXPECT_EQ(match_filter->Type(),FT_BLACKLIST);
  match_wildcard = static_cast<WildcardFilter*>(match_filter);

  match_filter = filtermgr->MatchAny("https://xxx.google.com/jsapi?autoload=%22ads%22xxx",CT_SCRIPT,"xxx.youtube.com",true);
  EXPECT_TRUE(match_filter);
  EXPECT_EQ(match_filter->Type(),FT_BLACKLIST);
  match_wildcard = static_cast<WildcardFilter*>(match_filter);

  match_filter = filtermgr->MatchAny("http://d2.sina.com.cn/201208/14/449118_38590.JPG",CT_IMAGE,"www.sina.com.cn",true);
  EXPECT_TRUE(match_filter);
  EXPECT_EQ(match_filter->Type(),FT_BLACKLIST);
  match_wildcard = static_cast<WildcardFilter*>(match_filter);

  match_filter = filtermgr->MatchAny("http://d2.sina.com.cn/201208/14/449118_38590.JPG",CT_IMAGE,"*",true);
  EXPECT_TRUE(match_filter);
  EXPECT_EQ(match_filter->Type(),FT_BLACKLIST);
  match_wildcard = static_cast<WildcardFilter*>(match_filter);

  match_filter = filtermgr->MatchAny("http://www.startpage.com/faint.html",CT_ELEMHIDE,"",false);
  EXPECT_TRUE(match_filter);
  EXPECT_EQ(match_filter->Type(),FT_WHITELIST);
  match_wildcard = static_cast<WildcardFilter*>(match_filter);
/*
  css.clear();
  filtermgr->GetSelectorsForHost("www.sina.com.cn",DMT_ALL,css);
  EXPECT_EQ(css.size(),6153); //±£³ÖºÍabpµÄ²éÑ¯½á¹ûÒ»ÖÂ=
*/
  css.clear();
  filtermgr->GetSelectorsForHost("www.sina.com.cn",DMT_JUST_INCLUDE,css);
  EXPECT_EQ(css.size(),13); //±£³ÖºÍabpµÄ²éÑ¯½á¹ûÒ»ÖÂ=

  css.clear();
  filtermgr->GetSelectorsForHost("www.sina.com.cn",DMT_INCLUDE_EXCLUDE,css);
  EXPECT_EQ(css.size(),289); //±£³ÖºÍabpµÄ²éÑ¯½á¹ûÒ»ÖÂ=

  EXPECT_EQ(filtermgr->GetVersion(),"1.0.0.1001");
  EXPECT_EQ(filtermgr->GetUrl(),"http://se.360.cn/adblock/patterns.ini");
  EXPECT_EQ(filtermgr->GetUpdatedTime(),"2013.3.6");
  EXPECT_EQ(filtermgr->GetAuthor(),"360");
  EXPECT_EQ(filtermgr->GetTitle(),"chinalist+easylist");
  EXPECT_TRUE(filtermgr->IsValid());

}

//////////////////////////////////////////////////////////////////////////
}
