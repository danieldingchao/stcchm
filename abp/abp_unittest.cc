#include "abp/abp.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace abp{

TEST(abpUtil_Test,IsThirdParty)
{
  EXPECT_TRUE(!AbpIsThirdParty("http://a.b.bar.jp/file.html",
    "http://a.b.bar.jp/file.html"));  // b.bar.jp
  EXPECT_TRUE(!AbpIsThirdParty("http://a.b.bar.jp/file.html",
    "http://b.b.bar.jp/file.html"));  // b.bar.jp
  EXPECT_FALSE(!AbpIsThirdParty("http://a.foo.jp/file.html",     // foo.jp
    "http://a.not.jp/file.html"));   // not.jp
  EXPECT_FALSE(!AbpIsThirdParty("http://a.foo.jp/file.html",     // foo.jp
    "http://a.foo.jp./file.html"));  // foo.jp.
  EXPECT_FALSE(!AbpIsThirdParty("http://a.com/file.html",        // a.com
    "http://b.com/file.html"));      // b.com
  EXPECT_TRUE(!AbpIsThirdParty("http://a.x.com/file.html",
    "http://b.x.com/file.html"));     // x.com
  EXPECT_TRUE(!AbpIsThirdParty("http://a.x.com/file.html",
    "http://.x.com/file.html"));      // x.com
  EXPECT_TRUE(!AbpIsThirdParty("http://a.x.com/file.html",
    "http://..b.x.com/file.html"));   // x.com
  EXPECT_TRUE(!AbpIsThirdParty("http://intranet/file.html",
    "http://intranet/file.html"));    // intranet
  EXPECT_TRUE(!AbpIsThirdParty("http://127.0.0.1/file.html",
    "http://127.0.0.1/file.html"));   // 127.0.0.1
  EXPECT_FALSE(!AbpIsThirdParty("http://192.168.0.1/file.html",  // 192.168.0.1
    "http://127.0.0.1/file.html"));  // 127.0.0.1
  EXPECT_FALSE(!AbpIsThirdParty("file:///C:/file.html",
    "file:///C:/file.html"));        // no host
}

TEST(abpUtil_Test,Getcandidates)
{
  std::vector<std::string> candidates;
  AbpGetCandidates("",candidates);
  EXPECT_TRUE(candidates.size()==0);

  candidates.clear();
  AbpGetCandidates("||so.com",candidates);
  EXPECT_TRUE(candidates.size()==1);
  EXPECT_EQ(candidates[0],"com");


  /*
    0: "http"
    1: "irs01"
    2: "com"
    3: "irt"
    4: "iwt"
    5: "360"
    6: "000001"
    7: "jsonp"
    8: "emuow"
  */
  candidates.clear();
  AbpGetCandidates("http://irs01.com/irt?_iwt_UA=UA-360-000001&jsonp=_EMUOW",candidates);
  EXPECT_TRUE(candidates.size()==9);
  EXPECT_EQ(candidates[0],"http");
  EXPECT_EQ(candidates[1],"irs01");
  EXPECT_EQ(candidates[2],"com");
  EXPECT_EQ(candidates[3],"irt");
  EXPECT_EQ(candidates[4],"iwt");
  EXPECT_EQ(candidates[5],"360");
  EXPECT_EQ(candidates[6],"000001");
  EXPECT_EQ(candidates[7],"jsonp");
  EXPECT_EQ(candidates[8],"emuow");
}

TEST(abpUtil_Test,GetcandidatesWithDelimiter)
{
  std::vector<base::StringPiece> candidates;
  AbpGetCandidatesWithDelimiter("",candidates);
  EXPECT_TRUE(candidates.size()==0);

  candidates.clear();
  AbpGetCandidatesWithDelimiter("||so.com",candidates);
  EXPECT_TRUE(candidates.size()==0);

  candidates.clear();
  AbpGetCandidatesWithDelimiter("||so.com^",candidates);
  EXPECT_TRUE(candidates.size()==1);
  EXPECT_EQ(candidates[0],".com");

  //||google.com/uds/
  //0: "|google"
  //1: ".com"
  //2: "/uds"
  candidates.clear();
  AbpGetCandidatesWithDelimiter("||google.com/uds/",candidates);
  EXPECT_TRUE(candidates.size()==3);
  EXPECT_EQ(candidates[0],"|google");
  EXPECT_EQ(candidates[1],".com");
  EXPECT_EQ(candidates[2],"/uds");

  //||search.1und1.de/s.gif?jsession=c3VjaGUuYWJwMiZ*
  //0: "|search"
  //1: ".1und1"
  //2: ".gif"
  //3: "?jsession"
  candidates.clear();
  AbpGetCandidatesWithDelimiter("||search.1und1.de/s.gif?jsession=c3VjaGUuYWJwMiZ*",candidates);
  EXPECT_TRUE(candidates.size()==4);
  EXPECT_EQ(candidates[0],"|search");
  EXPECT_EQ(candidates[1],".1und1");
  EXPECT_EQ(candidates[2],".gif");
  EXPECT_EQ(candidates[3],"?jsession");
}

TEST(abpUtil_Test,WildMatches)
{
  EXPECT_FALSE(AbpWildcardMatches("*baidu.com^*/egg*.swf*","http://www.baidu.com/egg.swf",true));
  EXPECT_TRUE(AbpWildcardMatches("*baidu.com^*/egg*.swf*","http://www.baidu.com//egg.swf",true));
  EXPECT_TRUE(AbpWildcardMatches("*baidu.com^*/egg*.Swf*","http://www.baidu.com/test/egg.swf",false));

  EXPECT_FALSE(AbpWildcardMatches("*baidu.com^*/egg*.swf*","http://www.baidu.com/test/eg.swf",true));
  EXPECT_TRUE(AbpWildcardMatches("*baidu.com^*/egg*.swf*","http://www.baidu.com/test/eggegg.swf",true));


  EXPECT_FALSE(AbpWildcardMatches("******************sina.com.cn","www.baidu.com",false));

}

TEST(abpUtil_Test,IsStartWithScheme)
{
  EXPECT_TRUE(AbpIsStartWithScheme("|http:"));
  EXPECT_FALSE(AbpIsStartWithScheme("||http:"));
  EXPECT_TRUE(AbpIsStartWithScheme("http:"));
  EXPECT_FALSE(AbpIsStartWithScheme("http"));
  EXPECT_FALSE(AbpIsStartWithScheme("|:"));
  EXPECT_TRUE(AbpIsStartWithScheme("-:"));
  EXPECT_FALSE(AbpIsStartWithScheme("--"));
  EXPECT_FALSE(AbpIsStartWithScheme("||googleads.g.doubleclick.net/pagead/*^"));
}

}
