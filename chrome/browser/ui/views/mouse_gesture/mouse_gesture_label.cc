//////////////////////////////////////////////////////////////////////////
// add by guoxiaolong 增强鼠标手势的文字和箭头提示

#include "chrome/browser/ui/views/mouse_gesture/mouse_gesture_label.h"

#include <algorithm>   // for min and max
using std::min;
using std::max;

#include "base/strings/utf_string_conversions.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/win/shell.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/win/hwnd_util.h"

#include "ui/display/win/screen_win.h"

#define LABEL_ARROW_MAX_HEIGHT  90
#define LABEL_MAX_WIDTH         180
#define CORNER_RADIUS            3


MouseGestureArrowLabel::MouseGestureArrowLabel(views::Widget* widget)
  : widget_(widget) {
}

MouseGestureArrowLabel::~MouseGestureArrowLabel() {
}

void MouseGestureArrowLabel::Close() {
  widget_->Close();
}

gfx::Size MouseGestureArrowLabel::GetPreferredSize() const {
  return gfx::Size(LABEL_MAX_WIDTH, LABEL_ARROW_MAX_HEIGHT);
}

void MouseGestureArrowLabel::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  gfx::Size label_size = GetPreferredSize();
  gfx::Rect round_rc(label_size);
  SkPaint rect_paint;
  rect_paint.setStyle(SkPaint::kFill_Style);
  rect_paint.setAntiAlias(true);
  rect_paint.setARGB(255, 0, 0, 0);
  canvas->SaveLayerAlpha(191);
  canvas->DrawRoundRect(round_rc, CORNER_RADIUS, rect_paint);
  if (!str_text_.empty()) {
    gfx::Font font;
    //font = font.Derive(16 - font.GetFontSize(),0, 0);
    gfx::Rect title_rc(0, 0, gfx::GetStringWidth(str_text_, gfx::FontList(font)), font.GetHeight());
    gfx::Rect string_rc((round_rc.width() - title_rc.width()) / 2, round_rc.height() - title_rc.height() - 8,
      title_rc.width(), title_rc.height());

    canvas->DrawStringRectWithFlags(str_text_, gfx::FontList(font),
      SK_ColorWHITE, string_rc, gfx::Canvas::NO_SUBPIXEL_RENDERING | gfx::Canvas::TEXT_ALIGN_LEFT);
  }
  canvas->Restore();
  int arrow_num = str_direction_text_.length();
  if (arrow_num) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    int arrow_img_width = rb.GetImageSkiaNamed(IDR_MOUSEGESTURE_ARROW_LEFT)->width();
  // 有效性检测=
  for(int i = 0; i < arrow_num; i++) {
    if(str_direction_text_[i] != 'U' &&
       str_direction_text_[i] != 'D' &&
       str_direction_text_[i] != 'L' &&
       str_direction_text_[i] != 'R')
      return;
  }


  // 先画半透明的圆角矩形=
    int x = (round_rc.width() - arrow_num * arrow_img_width) / 2;
    int y = 13;
  // 画箭头图片=
  for(int i = 0; i < arrow_num; i++) {
    int img_id = 0;
    
    if(str_direction_text_[i] == L'U')
      img_id = IDR_MOUSEGESTURE_ARROW_UP;
    else if(str_direction_text_[i] == L'D')
      img_id = IDR_MOUSEGESTURE_ARROW_DOWN;
    else if(str_direction_text_[i] == L'L')
      img_id = IDR_MOUSEGESTURE_ARROW_LEFT;
    else if(str_direction_text_[i] == L'R')
      img_id = IDR_MOUSEGESTURE_ARROW_RIGHT;

    canvas->DrawImageInt(*rb.GetImageSkiaNamed(img_id), x, y);
    x += arrow_img_width;
  }
}
}
void MouseGestureArrowLabel::SetText(std::string str_direction, base::string16& str_text) {
  str_direction_text_ = str_direction;
  str_text_ = str_text;
  SchedulePaint();
}
//////////////////////////////////////////////////////////////////////////
MouseGestureArrowLabel* ShowGestureArrowLabel(HWND parent) {
  views::Widget* gesture_widget = new views::Widget();
  MouseGestureArrowLabel* gesture_arrow_label = new MouseGestureArrowLabel(gesture_widget);
  
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.keep_on_top = true;
  params.delegate = gesture_arrow_label;

  gfx::Size label_size = gesture_arrow_label->GetPreferredSize();
  RECT rc;
  GetWindowRect(parent, &rc);
  rc.left += (rc.right - rc.left - label_size.width())/2;
  rc.top += ((rc.bottom - rc.top)/2 - label_size.height() - 8);
  rc.right = rc.left + label_size.width();
  rc.bottom = rc.top + label_size.height();

  params.bounds = gfx::Rect(rc);// display::ScreenToDIPRect(parent, gfx::Rect(rc));

  gesture_widget->Init(params);
  if (!ui::win::IsAeroGlassEnabled())
    gesture_widget->SetVisibilityChangedAnimationsEnabled(false);
  gesture_widget->ShowInactive();

  return gesture_arrow_label;
}
