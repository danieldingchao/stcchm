// Win32Project1.cpp : Defines the entry point for the application.
//

#include "InstallerWindow.h"

#pragma warning(disable: 4302)

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"

#include "setupdef.h"
#include "setup_util.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atldlgs.h>


INT_PTR CALLBACK    InstallProc(HWND, UINT, WPARAM, LPARAM);

void ShowInstallWindow() {
  g_hWnd = (HWND)DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_INSTALLER), NULL, InstallProc, (LPARAM)g_hInstance);
}

void PostMessage(UINT msg) {
  ::PostMessage(g_hWnd, msg, 0, 0);
}

void OnBtnInstall() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  BOOL m_ChromeRunning = installer::IsBrowserAlreadyRunning();
  if (m_ChromeRunning && cmd_line->HasSwitch(installer::switches::kSameVersionExist))
  {
    if (g_language != L"zh")
      MessageBox(g_hWnd, L"The same version of The World is running, please close it to proceed with the installation.", L"Lemon Browser", MB_OK | MB_ICONERROR);
    else
      MessageBox(g_hWnd, L"您正在运行相同版本。如需重复安装，请先关闭浏览器后重试安装。", L"柠檬浏览器", MB_OK | MB_ICONERROR);
    return;
  }
  g_Installing = true;
  ::ShowWindow(GetDlgItem(g_hWnd, IDC_LABEL_INSTALL), SW_HIDE);
  SendDlgItemMessageW(g_hWnd, IDC_LABEL_INSTALL, WM_SETTEXT, 0, (LPARAM)L"正在安装...");
  ::ShowWindow(GetDlgItem(g_hWnd, IDC_LABEL_INSTALL), SW_SHOW);

  SetEvent(g_hStartInstallEvent);

}

void OnBtnPath(HWND hDlg)
{
	base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
	if (cmd_line->HasSwitch(installer::switches::kSameVersionExist) ||
		cmd_line->HasSwitch(installer::switches::kHigherVersionExist) ||
		cmd_line->HasSwitch(installer::switches::kLowerVersionExist)) {
		if (g_language != L"zh")
			MessageBox(g_hWnd, L"You have installed lemon,please unstall current before choose other directory to install.", L"Lemon Browser", MB_OK | MB_ICONERROR);
		else
			MessageBox(g_hWnd, L"检测到您已安装柠檬浏览器，在选择安装到其他目录前，请先卸载当前安装。", L"柠檬浏览器", MB_OK | MB_ICONERROR);
		return;
	}

  wchar_t* str  = L"请选择文件夹：";
  CFolderDialog dlg(NULL, str,
    BIF_BROWSEFORCOMPUTER | BIF_NEWDIALOGSTYLE | BIF_RETURNONLYFSDIRS);

  if (dlg.DoModal(hDlg) == IDOK)
  {
    base::FilePath install_path(dlg.GetFolderPath());
    if (install_path.empty())
      return;
    base::FilePath g_installPath_copy(g_installPath);
    g_installPath = dlg.GetFolderPath();
    int check_caninstall_error = 0;
    if (!installer::CheckCanInstall(base::FilePath(g_installPath), &check_caninstall_error))
    {
      g_installPath = g_installPath_copy.value();
      MessageBox(g_hWnd, L"路径无效", L"柠檬浏览器", MB_OK);
      return;
    }

    base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    if (cmd_line->HasSwitch(installer::switches::kMigrateUserDataDir))
    {
      base::FilePath old_install_path = cmd_line->GetSwitchValuePath(installer::switches::kInstallPath);
    }

	::ShowWindow(GetDlgItem(g_hWnd, IDC_LABEL_INSTALLER_DIR), SW_HIDE);
    SendDlgItemMessageW(hDlg, IDC_LABEL_INSTALLER_DIR, WM_SETTEXT, 0, (LPARAM)g_installPath.c_str());
	::ShowWindow(GetDlgItem(g_hWnd, IDC_LABEL_INSTALLER_DIR), SW_SHOW);
  }
}

INT_PTR CALLBACK InstallProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  UNREFERENCED_PARAMETER(lParam);
  HFONT LabelFontHandle = NULL;
  switch (message)
  {
  case WM_INITDIALOG: {
	  g_hWnd = hDlg;
    LabelFontHandle = CreateFontW(28, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"微软雅黑");
    if (LabelFontHandle == NULL) {
      LabelFontHandle = CreateFontW(28, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"宋体");
    }
    SendDlgItemMessageW(hDlg, IDC_LABEL_INSTALL, WM_SETFONT, (WPARAM)LabelFontHandle, 0);
    SendDlgItemMessageW(hDlg, IDC_LABEL_PRODUCT, WM_SETFONT, (WPARAM)LabelFontHandle, 0);
    SendDlgItemMessageW(hDlg, IDC_LABEL_INSTALLER_DIR, WM_SETTEXT, 0, (LPARAM)g_installPath.c_str());
    return (INT_PTR)TRUE;
  }
  case WM_CTLCOLORSTATIC: {
    int id = GetDlgCtrlID((HWND)lParam);
    HDC hStaticDC = (HDC)wParam;
    SetBkMode(hStaticDC, TRANSPARENT);
    switch (id) {
    case  IDC_LABEL_INSTALL:
      SetTextColor(hStaticDC, RGB(78, 78, 78));
      break;
    case IDC_LABEL_PRODUCT:
      SetTextColor(hStaticDC, RGB(0, 0, 0));
      break;
    default:
      break;
    }
    return (INT_PTR)::GetStockObject(NULL_BRUSH);
  }      
  case WM_DESTROY:
    if (!g_Installing) {
      EndDialog(hDlg, 0);
      return TRUE;
    }
  case WM_CLOSE:
    if (!g_Installing) {
      g_isClosed = TRUE;
	  SetEvent(g_hStartInstallEvent);
      EndDialog(hDlg, 0);
      return TRUE;
    }
    return FALSE;
  case WM_INSTALL_FINISHED:
	  EndDialog(hDlg, 0);
	  return TRUE;
  case WM_COMMAND:{
		switch (LOWORD(wParam)) {
			case IDC_CHOOSE_DIR:
        OnBtnPath(hDlg);
        return TRUE;
      case IDC_LABEL_INSTALL:
      case IDB_INSTALL_BTN:
        OnBtnInstall();
        return TRUE;

			default:
				return FALSE;
			}
		default:
			FALSE;;
		}

  }
  return FALSE;
}

