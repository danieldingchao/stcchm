#ifndef CHROME_BROWSER_UI_VIEWS_MOUSE_GESTURE_LABEL_H_
#define CHROME_BROWSER_UI_VIEWS_MOUSE_GESTURE_LABEL_H_

#include "ui/views/widget/widget_delegate.h"

namespace views {
class Widget;
};

class MouseGestureArrowLabel : public views::WidgetDelegateView {
public:
  explicit MouseGestureArrowLabel(views::Widget* widget);
  ~MouseGestureArrowLabel();

  virtual bool ShouldShowWindowIcon() const override { return false; };
  virtual views::View* GetContentsView() override { return this; };
  virtual void OnPaint(gfx::Canvas* canvas) override;
  virtual gfx::Size GetPreferredSize() const override;

  void SetText(std::string str_direction, base::string16& str_text);

  void Close();

private:
  views::Widget* widget_;
  std::string str_direction_text_;
  base::string16 str_text_;

  DISALLOW_COPY_AND_ASSIGN(MouseGestureArrowLabel);
};

//////////////////////////////////////////////////////////////////////////
MouseGestureArrowLabel* ShowGestureArrowLabel(HWND parent);

#endif  //CHROME_BROWSER_UI_VIEWS_MOUSE_GESTURE_LABEL_H_
