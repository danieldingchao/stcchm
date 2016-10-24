#pragma once 

#include "abp/abp_export.h"
#include "abp/abp.h"

namespace abp{
//////////////////////////////////////////////////////////////////////////

class ABP_EXPORT Parser{
public:
  //textËùÖ¸µÄ×Ö·û´®±ØÐë´ÓheapÉÏ·ÖÅä²¢ÔÚmgrÀïÃæÍ³Ò»¹ÜÀí=
  static Filter* FromText(const base::StringPiece& text);
};

//////////////////////////////////////////////////////////////////////////
}

