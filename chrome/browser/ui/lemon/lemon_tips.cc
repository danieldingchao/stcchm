// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lemon/lemon_tips.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/search_engines/util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/resources/grit/ui_resources.h"

namespace {
const int kTopInset = 1;
const int kLeftInset = 2;
const int kBottomInset = 7;
const int kRightInset = 2;
}  // namespace

extern std::string sBubbleTips1;
extern std::string sBubbleTips2;
extern std::string sLinkText;
extern std::string sLinkUrl;
// static
LemonTipsBubble* LemonTipsBubble::ShowBubble(Browser* browser,
                                           views::View* anchor_view) {
  first_run::LogFirstRunMetric(first_run::FIRST_RUN_BUBBLE_SHOWN);
  LemonTipsBubble* delegate = new LemonTipsBubble(browser, anchor_view);
  views::BubbleDialogDelegateView::CreateBubble(delegate)->ShowInactive();
  return delegate;
}

void LemonTipsBubble::Init() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::FontList& original_font_list =
      rb.GetFontList(ui::ResourceBundle::MediumFont);

  base::string16 strtips1 = base::UTF8ToUTF16(sBubbleTips1);
  base::string16 strtips2 = base::UTF8ToUTF16(sBubbleTips2);
  base::string16 linktext = base::UTF8ToUTF16(sLinkText);
  tips1_ = new views::Label(strtips1,
        original_font_list.Derive(2, gfx::Font::NORMAL, gfx::Font::Weight::BOLD));
      //original_font_list);

  tips2_ = new views::Label(strtips2,
      original_font_list);

  link_ =
      new views::Link(linktext);
  link_->SetFontList(original_font_list);
  link_->set_listener(this);

  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);
  layout->SetInsets(-5, kLeftInset, kBottomInset, kRightInset);

  views::ColumnSet* columns = layout->AddColumnSet(0);

  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(1, 0);


  layout->StartRow(0, 0);
  //layout->AddView(close_button_);

  layout->StartRowWithPadding(0, 0, 0,
    views::kRelatedControlSmallVerticalSpacing);
  layout->AddView(tips1_);
  //layout->AddView(close_button_);
  layout->StartRowWithPadding(0, 0, 0,10);

  layout->AddView(tips2_);
  layout->AddView(link_);
}


LemonTipsBubble::LemonTipsBubble(Browser* browser, views::View* anchor_view)
    : views::BubbleDialogDelegateView(anchor_view,
                                      views::BubbleBorder::TOP_LEFT),
      browser_(browser) {
  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(
      GetLayoutConstant(LOCATION_BAR_BUBBLE_ANCHOR_VERTICAL_INSET), 0));
}

int LemonTipsBubble::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

bool LemonTipsBubble::ShouldShowCloseButton() const {
  return true;
}

LemonTipsBubble::~LemonTipsBubble() {
}

void LemonTipsBubble::LinkClicked(views::Link* source, int event_flags) {

  GetWidget()->Close();
  GURL url(sLinkUrl);
  if (url.is_valid()) {
    chrome::NavigateParams params(browser_, url, ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    chrome::Navigate(&params);
  }
}

void LemonTipsBubble::ButtonPressed(views::Button* sender, const ui::Event& event) {
  GetWidget()->Close();
}
