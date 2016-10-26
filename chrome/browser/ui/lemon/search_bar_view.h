// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LEMON_SEARCH_BAR_VIEW_H_
#define CHROME_BROWSER_UI_LEMON_SEARCH_BAR_VIEW_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_edit_controller.h"
#include "chrome/browser/ui/views/dropdown_bar_host.h"
#include "chrome/browser/ui/views/dropdown_bar_host_delegate.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "components/prefs/pref_member.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/security_state/security_state_model.h"
#include "components/zoom/zoom_event_manager_observer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/drag_controller.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/controls/image_view.h"

class ActionBoxButtonView;
class CommandUpdater;
class ContentSettingBubbleModelDelegate;
class ContentSettingImageView;
class ExtensionAction;
class GURL;
class InstantController;
class KeywordHintView;
class LocationIconView;
class OpenPDFInReaderView;
class ManagePasswordsIconViews;
class PageActionWithBadgeView;
class PageActionImageView;
class Profile;
class SelectedKeywordView;
class StarView;
class TemplateURLService;
class TranslateIconView;
class ZoomView;

namespace autofill {
class SaveCardIconView;
}

namespace views {
class Label;
class Widget;
}

/////////////////////////////////////////////////////////////////////////////
//
// SearchBarView class
//
//   The SearchBarView class is a View subclass that paints the background
//   of the URL bar strip and contains its content.
//
/////////////////////////////////////////////////////////////////////////////
class SearchBarView : public views::TextfieldController,
  public views::View {
 public:
  
  // Width (and height) of icons in location bar.
  static constexpr int kIconWidth = 16;

  // The location bar view's class name.
  static const char kViewClassName[];

  SearchBarView();

  ~SearchBarView() override;

  void Init(Browser* browser);
  void Layout() override;

  bool HandleKeyEvent(views::Textfield* sender,
    const ui::KeyEvent& key_event) override;


private:
  views::Textfield* text_field_;

  // The star.
  views::ImageView* icon_view_;
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(SearchBarView);
};

#endif  // CHROME_BROWSER_UI_LEMON_SEARCH_BAR_VIEW_H_
