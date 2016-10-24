/* --
#
# Sogou Explorer Favorite xxoo
#
# Copyright (C) 2011-2012 Liao
#
# This program is not free software; you cannot redistribute it and/or modify it;
#

@author:        Liao
@version:       3.0 && 3.1
@organization:  www.acfun.tv

-- */

#ifndef __SOGOU_EXP_H
#define __SOGOU_EXP_H

#define DECRYPT_CREATEFILE       0x0000
#define DECRYPT_FILESIZE      0x0001
#define DECRYPT_SMALL        0x0002
#define DECRYPT_MALLOC      0x0004
#define DECRYPT_CREATE      0x0008
#define DECRYPT_READ      0x0010
#define DECRYPT_WRITE      0x0011
#define DECRYPT_ALIGN      0x0012

#define DECRYPT_FAILED      -1
#define DECRYPT_OK        0x10000

#define SWAP(x) (_lrotl(x,8)&0x00ff00ff|_lrotr(x,8)&0xff00ff00)
#define GETU32(p) SWAP(*((unsigned int *)(p)))
#define PUTU32(ct, st) {*((unsigned int *)(ct))=SWAP((st));}

typedef unsigned char  u8;
typedef unsigned short  u16;
typedef unsigned int  u32;

BOOL
SEFavoriteDecrypt(
  char *favorite_file,  // 搜狗浏览器加密后的收藏夹(本地或者网络)完整路径
  char *decrypt_file,   // 解密后的收藏夹数据库输出位置
  BOOL bForce        // 是否暴力计算Key，不管True还是False, 目前都打算先尝试万能Key否则暴力
  );

#endif // __SOGOU_EXP_H
