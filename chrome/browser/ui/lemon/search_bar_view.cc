// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lemon/search_bar_view.h"

#include <algorithm>
#include <map>

#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/stl_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/location_bar/location_bar_layout.h"
#include "chrome/browser/ui/views/location_bar/background_with_1_px_border.h"
#include "chrome/grit/theme_resources.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "ui/base/resource/resource_bundle.h"


using content::WebContents;
using views::View;

namespace {

// The border color for MD windows, as well as non-MD popup windows.
const SkColor kBorderColor = SkColorSetA(SK_ColorBLACK, 0x4D);

}  // namespace


// SearchBarView -----------------------------------------------------------

// static
const char SearchBarView::kViewClassName[] = "SearchBarView";

SearchBarView::SearchBarView() {
  text_field_ = new views::Textfield();
}

SearchBarView::~SearchBarView() {
}

void SearchBarView::Init(Browser* browser) {
  browser_ = browser;
  SetPaintToLayer(true);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);
  const ui::NativeTheme* native_theme = GetNativeTheme();

  SkColor color = native_theme->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldDefaultBackground);
  set_background(
    new BackgroundWith1PxBorder(color, kBorderColor));
  icon_view_ = new views::ImageView();
  ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::ImageSkia& search_icon_image =
    *rb.GetImageSkiaNamed(IDR_BAIDU);
  icon_view_->SetImageSize(gfx::Size(16, 16));
  icon_view_->SetImage(search_icon_image);
  text_field_ = new views::Textfield();
  text_field_->set_controller(this);
  AddChildView(text_field_);
  AddChildView(icon_view_);

}

void SearchBarView::Layout() {
  LocationBarLayout leading_decorations(
    LocationBarLayout::LEFT_EDGE, 20,
    20);
  leading_decorations.AddDecoration(0, height(), true,
    0, 0,
    0, icon_view_);
  int x = 4;
  int y = (height() - icon_view_->GetPreferredSize().height()) / 2;
  icon_view_->SetBounds(x, y, icon_view_->GetPreferredSize().width(),
    icon_view_->GetPreferredSize().height());
  text_field_->SetBounds(0, 0, width(), height());

  gfx::Insets insets = text_field_->GetInsets();
  insets.Set(insets.left() + icon_view_->GetPreferredSize().width() + 6,
    insets.top(), insets.right(), insets.bottom());
  text_field_->SetBoundsRect(gfx::Rect(20, 2, width() - 24, height()-4));
  text_field_->SetBorder(
    views::Border::CreateEmptyBorder(0, 0, 0, 0));
}

bool SearchBarView::HandleKeyEvent(views::Textfield* sender,
  const ui::KeyEvent& key_event) {
  switch (key_event.key_code()) {
  case ui::VKEY_RETURN: {
    base::string16 text = text_field_->text();
    if (!text.empty()) {
      browser_->OnSearchText(text);
      return true;
    }
    break;
  }
  default: break;
  }
  return false;
}

