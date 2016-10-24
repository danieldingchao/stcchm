#ifndef ADBLOCK_INFO_H_
#define ADBLOCK_INFO_H_
//¶¨ÒåÒ»¸öÍ¨ÓÃ½á¹¹Ìå
struct AdblockInfo{
  AdblockInfo():
    child_id(0)
    ,routing_id(0)
    ,page_blocks(0)
    ,popup_blocks(0)
    ,adblocks_on(true) {
  }
  int child_id;     //webcontentsµÄprocess id(ÓÃÀ´Çø·Ö²»Í¬µÄWebContent)
  int routing_id;   //webcontentsµÄview id
  int page_blocks;  //À¹½ØµÄÒ³Ãæ¹ã¸æ
  int popup_blocks; //À¹½ØµÄµ¯³ö¹ã¸æ
  bool adblocks_on; //ÊÇ·ñÒÑ¾­¿ªÆôÀ¹½Ø¿ª¹Ø,Èç¹ûÔÚÀýÍâÁÐ±í·µ»Øfalse
};
#endif//ADBLOCK_INFO_H_
