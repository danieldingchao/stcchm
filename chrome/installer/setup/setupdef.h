// setupdef.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once


//#define WINVER			0x0500
//#define _WIN32_WINNT	0x0500
//#define _WIN32_IE		0x0700
#define _WIN32_DCOM

//#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define STRSAFE_NO_DEPRECATE
#define _CRT_SECURE_NO_WARNINGS
#define _CONVERSION_DONT_USE_THREAD_LOCALE
#define  ENABLE_WEB_DEBUGGER

#define ASSERT assert

#define DUIASSERT(x)

#include <windows.h>

#include <string>
#include "setup_resource.h"


#include "chrome/installer/util/installer_state.h"

extern HINSTANCE g_hInstance;
extern BOOL _bIsWindowsVista;
extern HANDLE g_hStartInstallEvent;
extern BOOL g_isClosed;
extern HWND g_hWnd;
extern std::wstring g_language;
extern std::wstring g_installPath;
extern installer::InstallStatus g_uninstall_status;
extern BOOL g_Installing;