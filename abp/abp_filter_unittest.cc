#include "abp/abp.h"
#include "abp/abp_filter.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace abp{
//////////////////////////////////////////////////////////////////////////

TEST(abpFilter_Test,FilterBase){
  Filter filter("!filter",FT_COMMENT);
  EXPECT_EQ(filter.Text(),"!filter");
  EXPECT_EQ(filter.Type(),FT_COMMENT);

  InvalidFilter invalid_filter("/xxxxx/","not support regexp");
  EXPECT_EQ(invalid_filter.Text(),"/xxxxx/");
  EXPECT_EQ(invalid_filter.Type(),FT_INVALID);
  EXPECT_EQ(invalid_filter.Reason(),"not support regexp");

  CommentFilter comment_filter("!filter");
  EXPECT_EQ(comment_filter.Text(),"!filter");
  EXPECT_EQ(comment_filter.Type(),FT_COMMENT);

  /*
  WildcardFilter(const std::string& text,const std::string& rule,
      int content_type,bool match_case,const std::string& domains,
      int thirdparty,bool collapse)*/
  WildcardFilter blacklist_filter ("||baidu.com$image,domain=qiyi.com|~360.cn",
    "||baidu.com",CT_IMAGE,false,"QIYI.COM|~360.CN",-1,false);
  EXPECT_EQ(blacklist_filter.Text(),"||baidu.com$image,domain=qiyi.com|~360.cn");
  EXPECT_EQ(blacklist_filter.Type(),FT_BLACKLIST);
  EXPECT_EQ(blacklist_filter.Rule(),"||baidu.com");

  /*
  WildcardFilter(const std::string& text,const std::string& rule,
      int content_type,bool match_case,const std::string& domains,
      int thirdparty,const std::string& sitekeys,int placeholder)*/
  WildcardFilter whitelist_filter ("||baidu.com$image,domain=qiyi.com|~360.cn",
    "||baidu.com",CT_IMAGE,false,"QIYI.COM|~360.CN",-1,"aaaaaaaa|bbbbbbb",0);
  EXPECT_EQ(whitelist_filter.Text(),"||baidu.com$image,domain=qiyi.com|~360.cn");
  EXPECT_EQ(whitelist_filter.Type(),FT_WHITELIST);
  EXPECT_EQ(whitelist_filter.Rule(),"||baidu.com");

  //agelioforos.gr#@#.side-ad
  ElemHideFilter elemhide_filter ("agelioforos.gr#@#.side-ad","agelioforos.gr",
    ".side-ad",true);
  EXPECT_EQ(elemhide_filter.Text(),"agelioforos.gr#@#.side-ad");
  EXPECT_EQ(elemhide_filter.Type(),FT_HIDEEXCEPTION);
  EXPECT_EQ(elemhide_filter.GetSelector(),".side-ad");
  EXPECT_EQ(elemhide_filter.GetDomains(),"agelioforos.gr");

}

TEST(abpFilter_Test,FilterMatches){

}

//////////////////////////////////////////////////////////////////////////
}
