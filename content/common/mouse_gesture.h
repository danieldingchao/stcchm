#ifndef CONTENT_COMMON_MOUSE_GESTURE_H_
#define CONTENT_COMMON_MOUSE_GESTURE_H_

// 鼠标手内部回放使用=
#define WM_THEWORLD_RBUTTOM_D WM_USER+1501
#define WM_THEWORLD_RBUTTOM_U WM_USER+1502
#define WM_THEWORLD_REPLAY      WM_USER+1503
#define WM_THEWORLD_CLEARFOCUSNODE   WM_USER+1504

#define WM_MOUSEGESTURE_CANCEL       WM_USER+1505
// webkit的flash手势使用，判断GetKeyState(RBUTTON)
#define WM_FLASH_MOUSEMOVE   WM_USER+1101
#define WM_FLASH_LBUTTOM_D   WM_USER+1102
#define WM_FLASH_MBUTTOM_D   WM_USER+1103
#define WM_FLASH_MOUSEWHEEL  WM_USER+1104
#define WM_FLASH_RBUTTOM_D   WM_USER+1105
#define WM_FLASH_RBUTTOM_U   WM_USER+1106

#define WM_CLOSE_ALONE_BROWSER WM_USER+1107

// 不判断KeyState，是MButtonDown就Post
#define WM_RENDER_MBUTTON_D WM_USER+1201

// 插件窗口获取了焦点  
#define WM_PLUGIN_FOCUSED WM_USER+1211
class MouseGesture
{
public:
  MouseGesture(){}
  virtual ~MouseGesture(){}

  //return:
  // 0 : 继续处理，手势没处理
  // 1 : 不用继续处理，被手势处理了
  // 2 : 手势开始和手势结束，分别对应rdown和rup，外部处理重放=
  virtual int  Run(HWND hWnd, UINT &message, WPARAM wParam, LPARAM lParam) = 0;	
};

#endif //CONTENT_COMMON_MOUSE_GESTURE_H_