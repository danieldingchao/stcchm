#pragma once

#include "windows.h"

#include "setup_resource.h"

#define WM_INSTALL_FINISHED (WM_USER + 1)


  void ShowInstallWindow();
  void PostMessage(UINT msg);
