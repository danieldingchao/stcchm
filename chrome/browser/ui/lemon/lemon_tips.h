// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LEMON_LEMON_TIPS_H_
#define CHROME_BROWSER_UI_LEMON_LEMON_TIPS_H_

#include <memory>

#include "base/macros.h"
#include "ui/events/event.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/button/image_button.h"

namespace views {
class EventMonitor;
}

class Browser;

class LemonTipsBubble : public views::BubbleDialogDelegateView,
                        public views::LinkListener,
                        public views::ButtonListener {
 public:
  // |browser| is the opening browser and is NULL in unittests.
  static LemonTipsBubble* ShowBubble(Browser* browser, views::View* anchor_view);
  //void Layout() override;
  //gfx::Size GetPreferredSize() const override;

 protected:
  // views::BubbleDialogDelegateView overrides:
  void Init() override;
  int GetDialogButtons() const override;

 private:
  LemonTipsBubble(Browser* browser, views::View* anchor_view);
  ~LemonTipsBubble() override;

  bool ShouldShowCloseButton() const override;

  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::LinkListener overrides:
  void LinkClicked(views::Link* source, int event_flags) override;

  Browser* browser_;

  views::Label* tips1_;
  views::Label* tips2_;
  views::Link* link_;


  DISALLOW_COPY_AND_ASSIGN(LemonTipsBubble);
};

#endif  // CHROME_BROWSER_UI_LEMON_LEMON_TIPS_H_
