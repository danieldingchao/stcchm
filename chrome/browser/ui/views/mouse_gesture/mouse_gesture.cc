#include "chrome/browser/ui/views/mouse_gesture/mouse_gesture.h"
#include <cassert>
#include <math.h>
#include <stdio.h>

#include "components/prefs/pref_service.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/chrome_dll_resource.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/mouse_gesture/mouse_gesture_label.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"

#include "ui/display/win/screen_win.h"

using content::NotificationSource;
using content::NotificationDetails;
using content::Details;

//++++++++gdiplus+++++++
#include <algorithm>   // for min and max
using std::min;
using std::max;
#include <gdiplus.h>
//using namespace Gdiplus;
#pragma comment(lib, "gdiplus.lib")


//--------gdiplus-------
namespace {
  struct StringElement { const char* key; int v; };
  struct SettingElement { const char* key;  int v; const char* pref; const StringElement* options; };
  static const StringElement mouse_gesture[] = {
    { "IDS_MORE_SETTING_DLG_U", IDS_MORE_SETTING_DLG_U },
    { "IDS_MORE_SETTING_DLG_D", IDS_MORE_SETTING_DLG_D },
    { "IDS_MORE_SETTING_DLG_L", IDS_MORE_SETTING_DLG_L },
    { "IDS_MORE_SETTING_DLG_R", IDS_MORE_SETTING_DLG_R },
    { "IDS_MORE_SETTING_DLG_UL", IDS_MORE_SETTING_DLG_UL },
    { "IDS_MORE_SETTING_DLG_UR", IDS_MORE_SETTING_DLG_UR },
    { "IDS_MORE_SETTING_DLG_DL", IDS_MORE_SETTING_DLG_DL },
    { "IDS_MORE_SETTING_DLG_DR", IDS_MORE_SETTING_DLG_DR },
    { "IDS_MORE_SETTING_DLG_RL", IDS_MORE_SETTING_DLG_RL },
    { "IDS_MORE_SETTING_DLG_LR", IDS_MORE_SETTING_DLG_LR },
    { "IDS_MORE_SETTING_DLG_UD", IDS_MORE_SETTING_DLG_UD },
    { "IDS_MORE_SETTING_DLG_DU", IDS_MORE_SETTING_DLG_DU },
    { "IDS_MORE_SETTING_DLG_RU", IDS_MORE_SETTING_DLG_RU },
    { "IDS_MORE_SETTING_DLG_RD", IDS_MORE_SETTING_DLG_RD },
    { "IDS_MORE_SETTING_DLG_LU", IDS_MORE_SETTING_DLG_LU },
    { "IDS_MORE_SETTING_DLG_LD", IDS_MORE_SETTING_DLG_LD },
    { "IDS_MORE_SETTING_DLG_BA", IDS_MORE_SETTING_DLG_BA },
    { "IDS_MORE_SETTING_DLG_BC", IDS_MORE_SETTING_DLG_BC },
    { "IDS_MORE_SETTING_DLG_BCU", IDS_MORE_SETTING_DLG_BCU },
    { "IDS_MORE_SETTING_DLG_BCD", IDS_MORE_SETTING_DLG_BCD }
  };

  // Name map to _Actions in MouseGesture.cc
  // 必须保证与_Actions数组的个数、次序的一一对应
  static const StringElement mouse_gesture_action[] = {
    { "ID_MGA_NONE", ID_MGA_NONE },
    { "ID_MGA_SCROLLPAGEUP", ID_MGA_SCROLLPAGEUP },
    { "ID_MGA_SCROLLPAGEDOWN", ID_MGA_SCROLLPAGEDOWN },
    { "ID_MGA_SCROLLLEFT", ID_MGA_SCROLLLEFT },
    { "ID_MGA_SCROLLRIGHT", ID_MGA_SCROLLRIGHT },

    { "ID_MGA_SCROLLBEGIN", ID_MGA_SCROLLBEGIN },
    { "ID_MGA_SCROLLEND", ID_MGA_SCROLLEND },
    { "ID_MGA_BACK", ID_MGA_BACK },
    { "ID_MGA_FORWARD", ID_MGA_FORWARD },
    { "ID_MGA_RELOAD", ID_MGA_RELOAD },

    { "ID_MGA_NEWTAB",  ID_MGA_NEWTAB },
    { "ID_MGA_HOME_KEY", ID_MGA_HOME_KEY },
    { "ID_MGA_CLOSETAB", ID_MGA_CLOSETAB },
    { "ID_MGA_CLOSEOTHERTABS",  ID_MGA_CLOSEOTHERTABS },
    { "ID_MGA_RESTORE_TAB", ID_MGA_RESTORE_TAB },

    { "ID_MGA_KEYLEFT", ID_MGA_KEYLEFT },
    { "ID_MGA_KEYRIGHT", ID_MGA_KEYRIGHT },

    { "ID_MGA_PREV_TAB",  ID_MGA_PREV_TAB },
    { "ID_MGA_NEXT_TAB", ID_MGA_NEXT_TAB },

    { "ID_MGA_FULLSCREEN", ID_MGA_FULLSCREEN },
    { "ID_MGA_STOP", ID_MGA_STOP },

    { "ID_MGA_MINI",  ID_MGA_MINI },
    //{"ID_MGA_CLOSE_ALL_TAB", ID_MGA_CLOSE_ALL_TAB}

  };

  const int _action_count = arraysize(mouse_gesture);

#ifndef MAX_LOADSTRING
#define MAX_LOADSTRING 100
#endif

static const wchar_t* kMouseGestureClass = L"WORLD_GESTURE";
static const wchar_t* kMouseGestureProc = L"WORLD_GESTURE_PROC";

#define RBUT_LEFT	'L'
#define RBUT_RIGHT	'R'
#define RBUT_UP		'U'
#define RBUT_DOWN	'D'

const UINT _ActionNameStringID[] = 
{
	IDS_MORE_SETTING_DLG_U,
	IDS_MORE_SETTING_DLG_D,
	IDS_MORE_SETTING_DLG_L,
	IDS_MORE_SETTING_DLG_R,

	IDS_MORE_SETTING_DLG_UL,
	IDS_MORE_SETTING_DLG_UR,
	IDS_MORE_SETTING_DLG_DL,
	IDS_MORE_SETTING_DLG_DR,

	IDS_MORE_SETTING_DLG_RL,
	IDS_MORE_SETTING_DLG_LR,
	IDS_MORE_SETTING_DLG_UD,
	IDS_MORE_SETTING_DLG_DU,

	IDS_MORE_SETTING_DLG_RU,
	IDS_MORE_SETTING_DLG_RD,
	IDS_MORE_SETTING_DLG_LU,
	IDS_MORE_SETTING_DLG_LD,

	IDS_MORE_SETTING_DLG_BA,
	IDS_MORE_SETTING_DLG_BC,
	IDS_MORE_SETTING_DLG_BCU,
	IDS_MORE_SETTING_DLG_BCD
};

};

const GESTURE_ACTION _Gestures[] = 
{
	//gesture,  action index
	{ "U",		1,  },
	{ "D",		2,  },
	{ "L",		7,  },
	{ "R",		8,	},

	{ "UL",		17,  },
	{ "UR",		18,  },
	{ "DL",		0,  },
	{ "DR",		12,	},

	{ "RL",		14, },
	{ "LR",		12,	},
	{ "UD",		6,	},
	{ "DU",		5,	},

	{ "RU",		10,	},
	{ "RD",		9,	},
	{ "LU",		22,	},
	{ "LD",		20,	},

	{ "BA",		21,	},
	{ "BC",		0,	},
	{ "BCU",	17,	},
	{ "BCD",	18,	},
};
const int _GestureActionCount = sizeof(_Gestures)/sizeof(GESTURE_ACTION);
// 修改了这个数组，记得改_action_count!!
const UINT _Actions[] = 
{
	// String ID
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// Note: Never remove or insert gesture, 
	//       you can only add gesture at the end of list
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	ID_MGA_NONE,
	ID_MGA_SCROLLPAGEUP,
	ID_MGA_SCROLLPAGEDOWN,
	ID_MGA_SCROLLLEFT,
	ID_MGA_SCROLLRIGHT,

	ID_MGA_SCROLLBEGIN,
	ID_MGA_SCROLLEND,
	ID_MGA_BACK,
	ID_MGA_FORWARD,
	ID_MGA_RELOAD,

//10
	ID_MGA_NEWTAB,
	ID_MGA_HOME_KEY,
	ID_MGA_CLOSETAB,
	ID_MGA_CLOSEOTHERTABS,
	ID_MGA_RESTORE_TAB,

	ID_MGA_KEYLEFT,
	ID_MGA_KEYRIGHT,
//17
	ID_MGA_PREV_TAB,
	ID_MGA_NEXT_TAB,

	ID_MGA_FULLSCREEN,
	ID_MGA_STOP,
//21
	ID_MGA_MINI,

  //ID_MGA_CLOSE_ALL_TAB,
  //ID_MGA_BOSSKEY,
  //ID_MGA_CLOSE_CURRENT_WINDOW,
  //ID_MGA_RELOAD_ALL_TAB,
  //ID_MGA_ZOOMIN_PAGE,
  //ID_MGA_ZOOMOUT_PAGE,
  //ID_MGA_ADD_FAVORITE

//	ID_MGA_EXIT_APP


//	ID_MGA_CLOSE_WINDOW_MAC,
//	ID_MGA_STOP_MENU_MAC,
//	ID_MGA_TEXT_BIGGER_MAC,
//20
//	ID_MGA_TEXT_SMALLER_MAC,
//	ID_MGA_FULLSCREEN_MAC,
//	ID_MGA_EXIT_MAC,
};
const int _uActionsCount = sizeof(_Actions)/(sizeof(UINT));


MouseGestureCore::MouseGestureCore(Profile *pref):
	hRelateWnd_(NULL),
	hWnd_(NULL),
	move_count_(0),
	browser_(NULL),
  arrow_label_(NULL),
  profile_(pref),
  enable_(false),
  enable_gdiplus_(false),
	inited_(false)
{
  setting_ = profile_->GetMouseGestureData();
}

MouseGestureCore::~MouseGestureCore()
{
}

void MouseGestureCore::ClearObject()
{
  if (back_buffer_)
    back_buffer_.DeleteObject();
}

bool MouseGestureCore::IsMouseGestureMove(LPARAM lParam)
{
	POINT point;
	point.x = GET_X_LPARAM(lParam);
	point.y = GET_Y_LPARAM(lParam);

	if( abs(ptLast_.x - point.x) > 1 || abs(ptLast_.y - point.y) > 1 )
		return true;
	return false;
}
static int reset_rbottomup = 0;

int MouseGestureCore::Run(HWND hWnd, UINT &message, WPARAM wParam, LPARAM lParam )
{
  if(!enable_gdiplus_)
    return 0;
  bool enable_mg = profile_->GetPrefs()->
    GetBoolean(prefs::kMGPUserMouseGesture);
  if(!enable_mg)
  {
    return 0;
  }

  if(1==reset_rbottomup && message==WM_RBUTTONUP)
  {
    std::wstring null_text;
    browser_->ShowMouseGestureActionString(null_text);
    return 1;
  }

  if( (message == WM_LBUTTONDOWN) && (wParam & MK_RBUTTON))
  {
    strcpy( MouseGestures_, "BA" );
    MGLen_ = 2;
    UpdateGesture();
    ActionGesture();
    // ShowGestureWnd(hWnd);
    reset_rbottomup = 1;
    return 1;
  }
  else if( (message == WM_MBUTTONDOWN) && (wParam & MK_RBUTTON))
  {
    strcpy( MouseGestures_, "BC" );
    MGLen_ = 2;
    UpdateGesture();
    ActionGesture();
    // ShowGestureWnd(hWnd);
    reset_rbottomup = 1;
    return 1;
  }
  else if( (message == WM_MOUSEWHEEL) && (LOWORD(wParam) & MK_RBUTTON) )
  {
    short d = HIWORD(wParam);
    int nLine = d/WHEEL_DELTA;
    if( nLine<0 ) nLine = -nLine;
    if( d > 0 )
    {
      strcpy( MouseGestures_, "BCU" );
    }
    else
    {
      strcpy( MouseGestures_, "BCD" );
    }
    MGLen_ = 3;
    UpdateGesture();
    ActionGesture();
    // ShowGestureWnd(hWnd);
    reset_rbottomup = 1;
    return 1;
  }

  if(message == WM_RBUTTONDOWN)
  {
    reset_rbottomup = 0;

    // eat rbuttondown
    ptLast_.x = GET_X_LPARAM(lParam);
    ptLast_.y = GET_Y_LPARAM(lParam);
    move_count_++;
    hRelateWnd_ = hWnd;
    return 2;
  }

  if(0 < move_count_)
  {
    if(message == WM_MOUSEMOVE && (wParam & MK_RBUTTON) && IsMouseGestureMove(lParam) )
    {
      ShowGestureWnd(hWnd);

      // reset mark
      move_count_ = 0;
      reset_rbottomup = 0;
      return 1;
    }
    else if(message==WM_RBUTTONUP)
    {
      // reset mark
      move_count_ = 0;
      return 2;
    }
  }

  return 0;
}

LRESULT MouseGestureCore::s_WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	MouseGestureCore * pThis = (MouseGestureCore*)GetProp( hWnd, kMouseGestureProc );
	if( !pThis )
		return DefWindowProc(hWnd, message, wParam, lParam);

	return pThis->WndProc( hWnd, message, wParam, lParam );
}

void MouseGestureCore::Init(Browser* b, HINSTANCE hInstance)
{
	if(!inited_)
	{
		inited_ = true;
    browser_ = b;
		hInstance_ = hInstance;
		WNDCLASSEX wcex;
		memset( &wcex, 0, sizeof(WNDCLASSEX) );

		wcex.cbSize = sizeof(WNDCLASSEX); 
		wcex.style			= CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc	= (WNDPROC)MouseGestureCore::s_WndProc;
		wcex.hInstance		= hInstance;
		wcex.lpszClassName	= kMouseGestureClass;

		RegisterClassEx(&wcex);

		ResourceBundle &rb = ResourceBundle::GetSharedInstance();
		font_ = rb.GetFont(ResourceBundle::BaseFont);

    //gdiplus
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    __try {
      if(0 == Gdiplus::GdiplusStartup(&m_gdiplusToken,
          &gdiplusStartupInput, NULL))
        enable_gdiplus_ = true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
      enable_gdiplus_ = false;
    }
	}
}

void MouseGestureCore::UnInit()
{
	//no need unregisterclass

    //gdiplus
    if(enable_gdiplus_) {
      Gdiplus::GdiplusShutdown(m_gdiplusToken);
    }
}

void MouseGestureCore::DrawTraceLine(HWND hWnd, POINT point)
{
  __try
  {// @liangzhihui, GDIPlus version possible crash with mmx operation.
    DrawTraceLineGDIPlus(hWnd,point);
  }__except(EXCEPTION_EXECUTE_HANDLER) {
    // @liangzhihui, much safer version with GDI.
    DrawTraceLineGDI(point);
  }
}

void MouseGestureCore::DrawTraceLineGDIPlus(HWND hWnd, POINT point)
{
  if (back_buffer_)
  {
    CTemporaryDC temp_dc(back_buffer_);
    if (temp_dc)
    {
      Gdiplus::Graphics g(temp_dc);
      Gdiplus::Pen p(Gdiplus::Color(255,0,114,243),3.0f);
      p.SetStartCap(Gdiplus::LineCapRound);
      p.SetEndCap(Gdiplus::LineCapRound);
      g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
      g.DrawLine(&p, ptLast_.x, ptLast_.y, point.x, point.y);
    }
  }
  ptLast_ = point;
};
void MouseGestureCore::DrawTraceLineGDI(POINT point)
{
  if (back_buffer_)
  {
    CTemporaryDC temp_dc(back_buffer_);
    if (temp_dc)
    {
      HPEN hPen = ::CreatePen(PS_SOLID, 3, RGB(0, 114, 243));
      HPEN hOldPen = (HPEN)SelectObject(temp_dc, hPen);
      MoveToEx(temp_dc, ptLast_.x, ptLast_.y, NULL);
      LineTo(temp_dc, point.x, point.y);
      SelectObject(temp_dc , hOldPen);
      DeleteObject(hPen);
    }
  }
  ptLast_ = point;
};
bool MouseGestureCore::PushMouseGesture( CHAR gesture )
{
	// edge
	if( MGLen_ >= MOUSE_GESTURE_MAX-1 )
		return false;

	// Ignore same pattern
	if( MGLen_ != -1 && MouseGestures_[MGLen_] == gesture )
		return false;

	MouseGestures_[++MGLen_] = gesture;
	return true;
}

bool MouseGestureCore::MoveDirection(POINT &point, char *Direction)
{
	int x = point.x - ptGestureLast_.x;
	int y = point.y - ptGestureLast_.y;
	int dist = x*x+y*y;
	if(dist>64)
	{
		if(x>abs(y) && x>0)
			*Direction = RBUT_RIGHT;
		else if(abs(x)>abs(y) && x<0)
			*Direction = RBUT_LEFT;
		else if(y>abs(x) && y>0)
			*Direction = RBUT_DOWN;
		else if(abs(y)>abs(x) && y<0)
			*Direction = RBUT_UP;
		else
			return FALSE;

		return TRUE;
	}
	else
		return FALSE;
}


bool MouseGestureCore::GetCurGesture(GESTURE_ACTION &ga, int *pnIndex)
{
	if( strlen(MouseGestures_) > 0 )
	{
		for( int i=0;i<_GestureActionCount;i++ )
		{
			if( strcmp( MouseGestures_, _Gestures[i].gesture ) == 0 )
			{
        if(setting_) 
        {
          if( pnIndex )
            *pnIndex = i;
          ga.gesture = _Gestures[i].gesture;
          ga.action = setting_->Pref(i).GetValue();
          return true;
        }
			}
		}
	}
	if( pnIndex )
		*pnIndex = -1;

	return false;
}

void MouseGestureCore::UpdateGesture()
{
  GESTURE_ACTION ga;
  memset(&ga, 0, sizeof(ga));

  int nIndex = -1;
  bool get_gesture = GetCurGesture(ga, &nIndex);

  if(get_gesture && ga.action>=_uActionsCount)
    return;

  UINT uStringID = 0;
  if( get_gesture )
    uStringID = _Actions[ga.action];
  else
    uStringID = _Actions[0];

  std::wstring text = l10n_util::GetStringUTF16(uStringID);

  browser_->ShowMouseGestureActionString(text);
  if(arrow_label_)
    arrow_label_->SetText(get_gesture ? std::string(ga.gesture) : "", text);
  return ;
}

void MouseGestureCore::ActionGesture()
{
  GESTURE_ACTION ga = {};
  bool get_gesture = GetCurGesture(ga);

//  DCHECK(NULL != ga)<<L"Invalid Mouse Gesture"<<L"\n"<<MouseGestures_;

  if (get_gesture)
  {
    if (ga.action >= _uActionsCount)
      return;

    wchar_t text[MAX_LOADSTRING];
    LoadString(hInstance_, _Actions[ga.action], text, MAX_LOADSTRING);

    if (NULL == browser_)
    {
      return;
    }

    switch (_Actions[ga.action])
    {
    case ID_MGA_BACK:
      chrome::GoBack(browser_, WindowOpenDisposition::CURRENT_TAB);
      break;
    case ID_MGA_FORWARD:
      chrome::GoForward(browser_, WindowOpenDisposition::CURRENT_TAB);
      break;
    case ID_MGA_RESTORE_TAB:
      chrome::RestoreTab(browser_);
      break;
    case ID_MGA_HOME_KEY:
      chrome::Home(browser_, WindowOpenDisposition::CURRENT_TAB);
      break;
    case ID_MGA_SCROLLPAGEUP:
      browser_->MouseGesturePageOperation(ID_MGA_SCROLLPAGEUP);
      // browser_->Home(CURRENT_TAB);
      break;
    case ID_MGA_SCROLLPAGEDOWN:
      browser_->MouseGesturePageOperation(ID_MGA_SCROLLPAGEDOWN);
      // browser_->Home(CURRENT_TAB);
      break;
    case ID_MGA_SCROLLLEFT:
      browser_->MouseGesturePageOperation(ID_MGA_SCROLLLEFT);
      // browser_->Home(CURRENT_TAB);
      break;
    case ID_MGA_SCROLLRIGHT:
      browser_->MouseGesturePageOperation(ID_MGA_SCROLLRIGHT);
      // browser_->Home(CURRENT_TAB);
      break;

    case ID_MGA_CLOSETAB:
      chrome::CloseTab(browser_);
      break;
    case ID_MGA_CLOSEOTHERTABS:
      browser_->MouseGesturePageOperation(ID_MGA_CLOSEOTHERTABS);
      // browser_->Home(CURRENT_TAB);
      break;
    case ID_MGA_RELOAD:
      chrome::Reload(browser_, WindowOpenDisposition::CURRENT_TAB);
      break;

    case ID_MGA_NEWTAB:
      chrome::NewTab(browser_);
      break;
    case ID_MGA_SCROLLBEGIN:
      browser_->MouseGesturePageOperation(ID_MGA_SCROLLBEGIN);
      break;
    case ID_MGA_SCROLLEND:
      browser_->MouseGesturePageOperation(ID_MGA_SCROLLEND);
      //browser_->Home(CURRENT_TAB);
      break;
    case ID_MGA_KEYLEFT:
      browser_->MouseGesturePageOperation(ID_MGA_KEYLEFT);
      break;
    case ID_MGA_KEYRIGHT:
      browser_->MouseGesturePageOperation(ID_MGA_KEYRIGHT);
      break;
    case ID_MGA_PREV_TAB:
      chrome::SelectPreviousTab(browser_);
      break;
    case ID_MGA_NEXT_TAB:
      chrome::SelectNextTab(browser_);
      break;
    case ID_MGA_STOP:
      chrome::Stop(browser_);
      break;
    case ID_MGA_FULLSCREEN:
      browser_->MouseGesturePageOperation(ID_MGA_FULLSCREEN);
      break;

    case ID_MGA_MINI:
      browser_->MouseGesturePageOperation(ID_MGA_MINI);
      break;
      //  case IDS_EXIT_MAC:
      //    browser_->Exit();
      //    break;

    /*case ID_MGA_CLOSE_ALL_TAB:
      browser_->tab_strip_model()->CloseAllTabs_ButNewTab();
      break;
    case ID_MGA_BOSSKEY:
      if(profile_->GetPrefs()->GetBoolean(prefs::kBossKeyEnable))
        BrowserView::HideBrowsers();
      break;
    case ID_MGA_CLOSE_CURRENT_WINDOW:
      chrome::CloseWindow(browser_);
      break;
    case ID_MGA_RELOAD_ALL_TAB:
      chrome::ReloadAll(browser_);
      break;
    case ID_MGA_ZOOMIN_PAGE:
      chrome::Zoom2(browser_, content::PAGE_ZOOM_IN);
      break;
    case ID_MGA_ZOOMOUT_PAGE:
      chrome::Zoom2(browser_, content::PAGE_ZOOM_OUT);
      break;
    case ID_MGA_ADD_FAVORITE:
      chrome::BookmarkCurrentPageIgnoringExtensionOverrides(browser_);
      break;*/
    }
  }
}

LRESULT MouseGestureCore::WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch( message )
	{
	case WM_CAPTURECHANGED:
		ShowGestureWnd(NULL);
		break;

	case WM_DESTROY:
		ClearObject();
		break;

	case WM_MOUSEMOVE:
		if(MK_RBUTTON & wParam)
		{
			POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
			
			char dir;
			if(MoveDirection(point, &dir))
			{
				PushMouseGesture(dir);
				ptGestureLast_ = point;
			}
		
      if (back_buffer_)
      {
        DrawTraceLine(hWnd, point);

        CTemporaryDC temp_dc(back_buffer_);
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, 0xff, AC_SRC_ALPHA };
        POINT ptSrc = { 0, 0 };
        UpdateLayeredWindow(hWnd, NULL, NULL, &back_buffer_.m_size, temp_dc, &ptSrc, 0, &bf, ULW_ALPHA);
      }
			UpdateGesture();
			return 0;
		}
	    break;

	case WM_RBUTTONUP:
    // reset_rbottomup为1，表示已完成手势操作，此时在绘图窗口上就不处理WM_RBUTTONUP了，否则会再次切下一标签 [4/20/2012 guoxiaolong-browser]
    ShowGestureWnd(NULL);
    browser_->window()->Activate();   

    if(reset_rbottomup == 0)
		  ActionGesture();
		return 0;

	case WM_CANCELMODE:
		ShowGestureWnd(NULL);
		return 0;

	case WM_LBUTTONDOWN:
		strcpy( MouseGestures_, "BA" );
		MGLen_ = 2;
		UpdateGesture();
		return 0;

	case WM_LBUTTONUP:
		ActionGesture();
		memset( MouseGestures_, 0, sizeof(MouseGestures_) );
		MGLen_ = -1;
		UpdateGesture();
		ShowGestureWnd(NULL);
		return 0;

	case WM_MBUTTONUP:
		ActionGesture();
		memset( MouseGestures_, 0, sizeof(MouseGestures_) );
		MGLen_ = -1;
		UpdateGesture();
		ShowGestureWnd(NULL);
		return 0;

	case WM_MBUTTONDOWN:
		strcpy( MouseGestures_, "BC" );
		MGLen_ = 2;
		UpdateGesture();
		return 0;
	
	case WM_MOUSEWHEEL:
		if( HIWORD( GetKeyState( VK_RBUTTON ) ) && !(BOOL)HIWORD( GetKeyState( VK_MBUTTON ) ) )
		{
			short d = HIWORD(wParam);
			int nLine = d/WHEEL_DELTA;
			if( nLine<0 ) nLine = -nLine;

			if( d > 0 )
				strcpy( MouseGestures_, "BCU" );
			else
				strcpy( MouseGestures_, "BCD" );
			MGLen_ = 3;

			UpdateGesture();
			ActionGesture();
			ShowGestureWnd(NULL);
			return 0;
		}
		break;
	}
	
	return DefWindowProc(hWnd, message, wParam, lParam);
}

void MouseGestureCore::ShowGestureWnd(HWND hRelateWnd, UINT message)
{

	if( NULL == hRelateWnd )
	{
    move_count_ = 0;

    // 清除提示窗口，清除状态栏手势提示文字=
    if(arrow_label_) {
      arrow_label_->Close();
      arrow_label_ = NULL;
    }

    //Hide gesture window
    if (IsWindow(hWnd_))
    {
      ShowWindow( hWnd_, SW_HIDE );
      DestroyWindow(hWnd_);
      
      //add by zongxiaobin，当前鼠标手势窗口关闭后，必须把焦点归回给顶层窗口。
      ::ReleaseCapture();
      //
    }
    hWnd_ = NULL;
    std::wstring null_text;
    browser_->ShowMouseGestureActionString(null_text);
		return;
	}

	hRelateWnd_ = hRelateWnd;
	
	//Show gesture window
	if( hWnd_ == NULL )
	{
		RECT rc = {0,0,10,10};
		hWnd_ = CreateWindowEx( WS_EX_TOOLWINDOW|WS_EX_NOPARENTNOTIFY | WS_EX_LAYERED | WS_EX_NOACTIVATE, 
			kMouseGestureClass, L"", WS_POPUP, 0,0,0,0, NULL, 
			NULL, hInstance_, 0 );
		SetProp( hWnd_, kMouseGestureProc, (HANDLE)this );
	}
	
	if( hWnd_ == NULL )
		return;

	assert( ::IsWindow(hWnd_) );

	ClearObject();

	RECT rcWnd;
	GetWindowRect( hRelateWnd_, &rcWnd );

  back_buffer_.CreateDIBSection(rcWnd.right - rcWnd.left, rcWnd.bottom - rcWnd.top);

  SetWindowPos(hWnd_, HWND_TOPMOST, rcWnd.left, rcWnd.top, rcWnd.right - rcWnd.left, rcWnd.bottom - rcWnd.top, SWP_SHOWWINDOW | SWP_NOACTIVATE);
	UpdateWindow(hWnd_);
	SetCapture(hWnd_);

  // show label text window
  bool enable_tips = profile_->GetPrefs()->
    GetBoolean(prefs::kMGPUserMouseGestureTips);
  if (enable_tips) {
    arrow_label_ = ShowGestureArrowLabel(hWnd_);
  }

	MGLen_ = -1;
	memset( MouseGestures_, 0, sizeof(MouseGestures_) );

	ptGestureLast_ = ptLast_;

	if( message != 0 )
		PostMessage( hWnd_, message, 0, 0 );
}


void MouseGestureData::RegisterUserPrefs(user_prefs::PrefRegistrySyncable* registry) {

  for(int iLoop=0; iLoop<_action_count; iLoop++) {
    std::string t = prefs::kMouseGestureProfile;
    t += ".";
    t += _Gestures[iLoop].gesture;

    // Fix Bug:
    // https://192.168.0.82/bugz-browser/show_bug.cgi?id=11426
    // Disable the actions:
    // Click left button, Click middle button , wheel up and wheel down.
    int action = _Gestures[iLoop].action;
    bool reset = (0 == strcmp(_Gestures[iLoop].gesture, "BA")) || 
      (0 == strcmp(_Gestures[iLoop].gesture, "BC")) || 
      (0 == strcmp(_Gestures[iLoop].gesture, "BCU")) || 
      (0 == strcmp(_Gestures[iLoop].gesture, "BCD"));
    if(reset) {
      action = 0;
    }

    registry->RegisterIntegerPref(t.c_str(), action, 
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  }

  registry->RegisterBooleanPref(prefs::kMGPOpenLabelInBookmark, 
    true, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kMGPRightButtonCloseTab, 
    false, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kMGPUserTheWorldOpenTabOrder, 
    true, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kMGPUserMouseGesture, 
    true, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kMGPUserMouseGestureTips, 
	  true, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(prefs::kMGPUserOpenNewTabOrder, 
    0, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(prefs::kMGPUserCloseTabOrder, 
    0, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kMGPUserWheelChangeTabOrder, 
    true, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kMGPUserHoverActiveTabOrder, 
    false, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(prefs::kMGPUserHoverActiveTimeval, 
    400, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}


void MouseGestureData::Init(Profile *pref)
{
  if(inited_)
    return;

  inited_ = true;
  PrefService* pref_service = pref->GetPrefs();

  for(int iLoop=0; iLoop<_action_count; iLoop++) {
    std::string t = prefs::kMouseGestureProfile;
    t += ".";
    t += _Gestures[iLoop].gesture;
    mouse_gesture_pref[iLoop].Init(t.c_str(), pref_service);
  }
}

const IntegerPrefMember& MouseGestureData::Pref(int index) const {
  DCHECK(index>=0 && index<_action_count);
  return mouse_gesture_pref[index];
}