// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_THEWORLD_IMPORTER_UTILS_H_
#define CHROME_BROWSER_IMPORTER_THEWORLD_IMPORTER_UTILS_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "build/build_config.h"

class GURL;
class TemplateURL;

namespace base {
  class DictionaryValue;
  class FilePath;
}

// Detect whether the world 3 is exist.
bool IsTheWorld3Exist();

bool IsTheWorld5Exist();

bool IsTheWorldChromeExist();

bool IsTheWorld6Exist();



#endif  // CHROME_BROWSER_IMPORTER_THEWORLD_IMPORTER_UTILS_H_
