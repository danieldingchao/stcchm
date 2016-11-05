// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_360SE_RESTORE_MENU_H_
#define CHROME_BROWSER_UI_360SE_RESTORE_MENU_H_

#include <set>
#include <string>

//#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "components/favicon/core/favicon_service.h"
#include "base/task/cancelable_task_tracker.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/window_open_disposition.h"
#include "components/sessions/core/tab_restore_service.h"

class Browser;

namespace content {
  class NavigationEntry;
  class WebContents;
}

namespace gfx {
  class Image;
}

///////////////////////////////////////////////////////////////////////////////
//
// RestoreMenuModel
//
// Interface for the showing of the dropdown menu for the Back/Forward buttons.
// Actual implementations are platform-specific.
///////////////////////////////////////////////////////////////////////////////
class RestoreMenuModel : public ui::MenuModel {
public:
  RestoreMenuModel(Browser* browser);
  virtual ~RestoreMenuModel();

  // MenuModel implementation.
  virtual bool HasIcons() const override;
  virtual int GetItemCount() const override;
  virtual ItemType GetTypeAt(int index) const override;
  virtual ui::MenuSeparatorType GetSeparatorTypeAt(int index) const override;
  virtual int GetCommandIdAt(int index) const override;
  virtual base::string16 GetLabelAt(int index) const override;
  virtual bool IsItemDynamicAt(int index) const override;
  virtual bool GetAcceleratorAt(int index,
    ui::Accelerator* accelerator) const override;
  virtual bool IsItemCheckedAt(int index) const override;
  virtual int GetGroupIdAt(int index) const override;
  virtual bool GetIconAt(int index, gfx::Image* icon) override;
  virtual ui::ButtonMenuItemModel* GetButtonMenuItemAt(
    int index) const override;
  virtual bool IsEnabledAt(int index) const override;
  virtual MenuModel* GetSubmenuModelAt(int index) const override;
  virtual void HighlightChangedTo(int index) override;
  virtual void ActivatedAt(int index) override;
  virtual void ActivatedAt(int index, int event_flags) override;
  virtual void MenuWillShow() override;

  // Is the item at |index| a separator?
  bool IsSeparator(int index) const;

  // Set the delegate for triggering OnIconChanged.
  virtual void SetMenuModelDelegate(
    ui::MenuModelDelegate* menu_model_delegate) override;
  virtual ui::MenuModelDelegate* GetMenuModelDelegate() const override;

protected:
  ui::MenuModelDelegate* menu_model_delegate() { return menu_model_delegate_; }

private:
  // Requests a favicon from the FaviconService. Called by GetIconAt if the
  // NavigationEntry has an invalid favicon.
  void FetchFavicon(const sessions::SerializedNavigationEntry* entry);

  // Callback from the favicon service.
  void OnFavIconDataAvailable(
    int navigation_entry_unique_id,
    const favicon_base::FaviconImageResult& image_result);

  int GetRestoreItemCount() const;

  // How many items (max) to show in the back/forward history menu dropdown.
  static const int kMaxRestoreItems;

  // Does the item have a command associated with it?
  bool ItemHasCommand(int index) const;

  // Returns true if there is an icon for this menu item.
  bool ItemHasIcon(int index) const;

  // Looks up a TabRestoreEntry by menu id.
  sessions::TabRestoreService::Entry* GetTabRestoreEntry(int index) const;

  // Build a string version of a user action on this menu, used as an
  // identifier for logging user behavior.
  // E.g. BuildActionName("Click", 2) returns "BackMenu_Click2".
  // An index of -1 means no index.
  std::string BuildActionName(const std::string& name, int index) const;

  Browser* browser_;

  // Keeps track of which favicons have already been requested from the history
  // to prevent duplicate requests, identified by
  // NavigationEntry->GetUniqueID().
  std::set<int> requested_favicons_;

  // Used for loading favicons.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // Used for receiving notifications when an icon is changed.
  ui::MenuModelDelegate* menu_model_delegate_;

  DISALLOW_COPY_AND_ASSIGN(RestoreMenuModel);
};

#endif  // CHROME_BROWSER_UI_360SE_RESTORE_MENU_H_
