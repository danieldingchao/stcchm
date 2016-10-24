// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/theworld_importer_utils.h"

#include <shlobj.h>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/win/registry.h"

static const wchar_t* kTheworld3Path = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\TheWorld.exe";
static const wchar_t* kTheworld5Path = L"SOFTWARE\\TheWorld";
static const wchar_t* kTheworldChromePath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\TheWorld Chrome";
static const wchar_t* kPath = L"Path";

bool IsTheWorld3Exist() {
  base::string16 registry_path = kTheworld3Path;
  base::win::RegKey reg_key(HKEY_LOCAL_MACHINE, registry_path.c_str(),
    KEY_READ);
  // 需要读取46AA30DF-ED7B-438a-9462-60AB9A6D57E4判断世界之窗3是否存在
  std::wstring key_path2(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{46AA30DF-ED7B-438a-9462-60AB9A6D57E4}");
  base::win::RegKey key2(HKEY_LOCAL_MACHINE, key_path2.c_str(), KEY_READ);
  if(reg_key.Valid() && key2.Valid())
    return true;

  return false;
}

// 世界之窗5是解压缩使用的，没有安装过程。控制面板中也不显示卸载
bool IsTheWorld5Exist() {
  base::string16 registry_path = kTheworld5Path;
  base::win::RegKey reg_key(HKEY_CURRENT_USER, registry_path.c_str(),
    KEY_READ);
  if(reg_key.Valid())
    return true;
  else
    return false;
}

bool IsTheWorldChromeExist() {
  base::string16 registry_path = kTheworldChromePath;
  base::win::RegKey reg_key(HKEY_CURRENT_USER, registry_path.c_str(),
    KEY_READ);
  if(reg_key.Valid())
    return true;
  else
    return false;
}
