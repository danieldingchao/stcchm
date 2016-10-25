#ifndef CHROME_BROWSER_UI_VIEWS_MOUSE_GESTURE_H_
#define CHROME_BROWSER_UI_VIEWS_MOUSE_GESTURE_H_

#include <Windows.h>
#include <ui/gfx/font.h>

#include <atlbase.h>
#include <atlapp.h>
#include <atlctrls.h>
#include <atlgdi.h>
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_member.h"
#include "chrome/common/pref_names.h"
#include "content/common/mouse_gesture.h"
#include "content/public/browser/notification_observer.h"
//#include "chrome/browser/ui/views/options/thirdparty_browser_option.h"

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp)                        ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)                        ((int)(short)HIWORD(lp))
#endif

class MouseGestureArrowLabel;
class MouseGestureTextLabel;

class MouseGestureData : public content::NotificationObserver
{
 public:
  static void RegisterUserPrefs(user_prefs::PrefRegistrySyncable* registry);
 public:
  void Init(Profile *pref);

  const IntegerPrefMember& Pref(int index) const ;

  virtual void Observe(int type, 
    const content::NotificationSource& source, const content::NotificationDetails& details){};

  MouseGestureData() {
    inited_ = false;
  }
  ~MouseGestureData(){};

 private:
  IntegerPrefMember mouse_gesture_pref[20];
  bool inited_;
};

#define MOUSE_GESTURE_MAX	4
#define GESTURE_COMMAND_LPARAM	0xFF1974FF 

typedef struct tagGestureAction
{
	const char * gesture;
	int action;
}GESTURE_ACTION, *PGESTURE_ACTION;

class SkBitmap;
class Browser;

class CDIBSection : public WTL::CBitmap
{
public:
  CDIBSection()
    : m_pbBuffer(NULL)
  {
    m_size.cx = m_size.cy = 0;
  }

  CDIBSection(int cx, int cy)
  {
    CreateDIBSection(cx, cy);
  }
  void Attach(HBITMAP hBitmap)
  {
    __super::Attach(hBitmap);

    BITMAP bm = { 0 };
    if (GetBitmap(bm))
    {
      ATLASSERT(bm.bmBits != NULL && bm.bmBitsPixel == 32 && bm.bmWidthBytes == bm.bmWidth * 4);

      m_size.cx = bm.bmWidth;
      m_size.cy = bm.bmHeight;
      m_pbBuffer = (RGBQUAD*)bm.bmBits;
    }
    else
      ATLASSERT(FALSE);
  }
  HBITMAP Detach()
  {
    m_size.cx = m_size.cy = 0;
    m_pbBuffer = NULL;

    return __super::Detach();
  }
  void CreateDIBSection(int cx, int cy)
  {
    ATLASSERT(m_hBitmap == NULL);

    m_size.cx = cx;
    m_size.cy = cy;
    BITMAPINFOHEADER bih = { sizeof(BITMAPINFOHEADER), cx, -cy, 1, 32, 0 };
    WTL::CBitmap::CreateDIBSection(NULL,(BITMAPINFO*)&bih, 0, (void**)&m_pbBuffer, 0, 0);
    ATLASSERT(m_hBitmap != NULL);
  }
  void FillRect(DWORD color)
  {
    FillRect(0, 0, m_size.cx, m_size.cy, color);
  }
  void FillRect(int x, int y, int width, int height, DWORD color)
  {
    ATLASSERT(m_pbBuffer != NULL && x >= 0 && y >= 0 && (x + width) <= m_size.cx && (y + height) <= m_size.cy);
    if (m_pbBuffer == NULL || x < 0 || y < 0 || (x + width) > m_size.cx || (y + height) > m_size.cy)
      return;

    int w = width + x;
    int h = height + y;
    for (int _y = y; _y < h; _y++)
    {
      DWORD* ptr = (DWORD*)((LPBYTE)m_pbBuffer + (_y * m_size.cx + x) * 4);
      for (int _x = x; _x < w; _x++)
      {
        *ptr++ = color;
      }
    }
  }

  RGBQUAD* m_pbBuffer;
  SIZE m_size;

protected:
  CDIBSection(const CDIBSection&);
};
class CTemporaryDC : public WTL::CDC
{
public:
  CTemporaryDC(HBITMAP hBitmap)
  {
    CreateCompatibleDC(NULL);
    ATLASSERT(m_hDC != NULL);

    m_hBmpOld = SelectBitmap(hBitmap);
  }
  ~CTemporaryDC()
  {
    SelectBitmap(m_hBmpOld);
  }
protected:
  HBITMAP m_hBmpOld;
};
class MouseGestureCore: public MouseGesture
{
public:
	void Init(Browser* b, HINSTANCE hInstance);
	void UnInit();
	void ShowGestureWnd( HWND hRelateWnd, UINT message = 0);

	MouseGestureCore(Profile *pref);
	~MouseGestureCore();

	bool enable_;

	HWND m_hWndRButtonDown;
	virtual int  Run(HWND hWnd, UINT &message, WPARAM wParam, LPARAM lParam) override;	

private:
	static LRESULT __stdcall s_WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
	LRESULT __stdcall WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
	
	void ClearObject();
	bool IsMouseGestureMove( LPARAM lParam );
	void DrawTraceLine(HWND hWnd, POINT point);
	void DrawTraceLineGDIPlus(HWND hWnd, POINT point);
	void DrawTraceLineGDI(POINT point);


  bool GetCurGesture(GESTURE_ACTION &ga, int *pnIndex = NULL);
	bool MoveDirection(POINT &point, char *Direction);
	bool PushMouseGesture(char gesture);
	void UpdateGesture();
	void ActionGesture();

	POINT ptGestureLast_;
	int  MGLen_;
	char MouseGestures_[MOUSE_GESTURE_MAX+1];
	//
	
	HWND hRelateWnd_;
	HWND hWnd_;
	HINSTANCE hInstance_;
	gfx::Font font_;

	POINT ptLast_;
	int move_count_;

	Browser *browser_;
  Profile* profile_;
  MouseGestureData* setting_;
  MouseGestureArrowLabel* arrow_label_;

	bool inited_;

  bool enable_gdiplus_;
  CDIBSection back_buffer_;

  //gdiplus
  ULONG_PTR m_gdiplusToken;
};

extern const GESTURE_ACTION _Gestures[];

#endif //CHROME_BROWSER_UI_VIEWS_MOUSE_GESTURE_H_