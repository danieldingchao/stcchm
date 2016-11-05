// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/ui/views/lemon/restore_menu.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/text_elider.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/favicon_size.h"

#include "base/strings/utf_string_conversions.h"
#include "components/grit/components_scaled_resources.h"

using content::NavigationController;
using content::NavigationEntry;
using base::UserMetricsAction;
using content::WebContents;

const int RestoreMenuModel::kMaxRestoreItems = 12;
static const int kMaxWidth = 700;

RestoreMenuModel::RestoreMenuModel(Browser* browser)
  : browser_(browser),
  menu_model_delegate_(NULL) {
}

RestoreMenuModel::~RestoreMenuModel() {
}

bool RestoreMenuModel::HasIcons() const {
  return true;
}

int RestoreMenuModel::GetItemCount() const {
  int items = GetRestoreItemCount();

  if (items > 0) {
    // If the menu is not empty, add two positions in the end
    // for a separator and a "Show Full History" item.
    items += 3;
  }

  return items;
}

ui::MenuModel::ItemType RestoreMenuModel::GetTypeAt(int index) const {
  return IsSeparator(index) ? TYPE_SEPARATOR : TYPE_COMMAND;
}

ui::MenuSeparatorType RestoreMenuModel::GetSeparatorTypeAt(
  int index) const {
    return ui::NORMAL_SEPARATOR;
}

int RestoreMenuModel::GetCommandIdAt(int index) const {
  return index;
}

base::string16 RestoreMenuModel::GetLabelAt(int index) const {
  // Return label "Show Full History" for the last item of the menu.
  if (index == GetItemCount() - 1)
    return l10n_util::GetStringUTF16(IDS_SHOWFULLHISTORY_LINK);
  else if (index == GetItemCount() - 2)
    return l10n_util::GetStringUTF16(IDS_CLEANRESTORE_LIST);

  // Return an empty string for a separator.
  if (IsSeparator(index))
    return base::string16();

  sessions::TabRestoreService::Entry* entry = GetTabRestoreEntry(index);
  if (entry == NULL)
    return base::string16();
  if (entry->type == sessions::TabRestoreService::TAB) {
    sessions::TabRestoreService::Tab* tab = static_cast<sessions::TabRestoreService::Tab*>(entry);
    const sessions::SerializedNavigationEntry& current_navigation =
      tab->navigations.at(tab->current_navigation_index);
    return current_navigation.title().empty() ? 
      base::ASCIIToUTF16(current_navigation.virtual_url().spec()) : current_navigation.title();
  } else  {
    DCHECK_EQ(entry->type, sessions::TabRestoreService::WINDOW);
    sessions::TabRestoreService::Window* window = static_cast<sessions::TabRestoreService::Window*>(entry);
    return l10n_util::GetStringFUTF16(
      IDS_NEW_TAB_RECENTLY_CLOSED_WINDOW_MULTIPLE,
      base::IntToString16(window->tabs.size()));
  }
}

bool RestoreMenuModel::IsItemDynamicAt(int index) const {
  // This object is only used for a single showing of a menu.
  return false;
}

bool RestoreMenuModel::GetAcceleratorAt(
  int index,
  ui::Accelerator* accelerator) const {
    return false;
}

bool RestoreMenuModel::IsItemCheckedAt(int index) const {
  return false;
}

int RestoreMenuModel::GetGroupIdAt(int index) const {
  return false;
}

bool RestoreMenuModel::GetIconAt(int index, gfx::Image* icon) {
  if (!ItemHasIcon(index))
    return false;

  if (index == GetItemCount() - 1) {
    *icon = ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_HISTORY_FAVICON);
  } else if (index == GetItemCount() - 2) {
    return false;
  } else {
    sessions::TabRestoreService::Entry* entry = GetTabRestoreEntry(index);
    if (entry == NULL)
      return false;
    
    if (entry->type == sessions::TabRestoreService::TAB) {
      sessions::TabRestoreService::Tab* tab = static_cast<sessions::TabRestoreService::Tab*>(entry);
      const sessions::SerializedNavigationEntry& current_navigation =
        tab->navigations.at(tab->current_navigation_index);
      //if (current_navigation.GetFavicon().valid) {
      //  *icon = current_navigation.GetFavicon().image;
      //  return true;
      //}

      //       if (menu_model_delegate()) {
      //         FetchFavicon(&current_navigation);
    }
    return true;
  }

  //if (service) {
  //  bitmap = service->GetEntryIconByIndex(index);
  //  if (bitmap.isNull() || 0 == bitmap.width() || 0 == bitmap.height())
  //    GetFaviconForURL(
  //    GURL(service->GetEntryVirtualUrlByIndex(index)), index);
  //}



  //NavigationEntry* entry = GetNavigationEntry(index);
  //*icon = entry->GetFavicon().image;
  //if (!entry->GetFavicon().valid && menu_model_delegate()) {
  //  FetchFavicon(entry);
  //}
  return true;
}

ui::ButtonMenuItemModel* RestoreMenuModel::GetButtonMenuItemAt(
  int index) const {
    return NULL;
}

bool RestoreMenuModel::IsEnabledAt(int index) const {
  return index < GetItemCount() && !IsSeparator(index);
}

ui::MenuModel* RestoreMenuModel::GetSubmenuModelAt(int index) const {
  return NULL;
}

void RestoreMenuModel::HighlightChangedTo(int index) {
}

void RestoreMenuModel::ActivatedAt(int index) {
  ActivatedAt(index, 0);
}

void RestoreMenuModel::ActivatedAt(int index, int event_flags) {
  DCHECK(!IsSeparator(index));

  // Execute the command for the last item: "Show Full History".
  if (index == GetItemCount() - 1) {
    content::RecordComputedAction(BuildActionName("ShowFullHistory", -1));
    chrome::ShowSingletonTabOverwritingNTP(browser_,
      chrome::GetSingletonTabNavigateParams(
      browser_, GURL(chrome::kChromeUIHistoryURL)));
    return;
  }
  sessions::TabRestoreService* service = 
    TabRestoreServiceFactory::GetForProfile(browser_->profile());
  if (service == NULL)    
    return;

  if (index == GetItemCount() - 2) {
    service->ClearEntries();
    return;
  }

  sessions::TabRestoreService::Entry* entry = GetTabRestoreEntry(index);
  if (entry == NULL)
    return;

  sessions::LiveTabContext* context =
    BrowserLiveTabContext::FindContextForWebContents(
    browser_->tab_strip_model()->GetActiveWebContents());
  service->RestoreEntryById(
    context,
    entry->id, WindowOpenDisposition::UNKNOWN);
}

void RestoreMenuModel::MenuWillShow() {
  content::RecordComputedAction(BuildActionName("Popup", -1));
  requested_favicons_.clear();
  cancelable_task_tracker_.TryCancelAll();
}

bool RestoreMenuModel::IsSeparator(int index) const {
  int restore_items = GetRestoreItemCount();

  // Look to see if we have reached the separator for the history items.
  return index == restore_items;
}

void RestoreMenuModel::SetMenuModelDelegate(
  ui::MenuModelDelegate* menu_model_delegate) {
    menu_model_delegate_ = menu_model_delegate;
}

ui::MenuModelDelegate* RestoreMenuModel::GetMenuModelDelegate() const {
  return menu_model_delegate_;
}

void RestoreMenuModel::FetchFavicon(const sessions::SerializedNavigationEntry* entry) {
  // If the favicon has already been requested for this menu, don't do
  // anything.
  if (requested_favicons_.find(entry->unique_id()) !=
    requested_favicons_.end()) {
      return;
  }
  requested_favicons_.insert(entry->unique_id());
  favicon::FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
    browser_->profile(), ServiceAccessType::EXPLICIT_ACCESS);
  if (!favicon_service)
    return;

  favicon_service->GetFaviconImage(   
    entry->favicon_url(),    
    base::Bind(&RestoreMenuModel::OnFavIconDataAvailable,
    base::Unretained(this),
    entry->unique_id()),
    &cancelable_task_tracker_);
}

void RestoreMenuModel::OnFavIconDataAvailable(
  int navigation_entry_unique_id,
  const favicon_base::FaviconImageResult& image_result) {
//     if (!image_result.image.IsEmpty()) {
//       // Find the current model_index for the unique id.
//       NavigationEntry* entry = NULL;
//       int model_index = -1;
//       for (int i = 0; i < GetItemCount() - 1; i++) {
//         if (IsSeparator(i))
//           continue;
//         if (GetNavigationEntry(i)->GetUniqueID() == navigation_entry_unique_id) {
//           model_index = i;
//           entry = GetNavigationEntry(i);
//           break;
//         }
//       }
// 
//       if (!entry)
//         // The NavigationEntry wasn't found, this can happen if the user
//         // navigates to another page and a NavigatationEntry falls out of the
//         // range of kMaxRestoreItems.
//         return;
// 
//       // Now that we have a valid NavigationEntry, decode the favicon and assign
//       // it to the NavigationEntry.
//       entry->GetFavicon().valid = true;
//       entry->GetFavicon().url = image_result.icon_url;
//       entry->GetFavicon().image = image_result.image;
//       if (menu_model_delegate()) {
//         menu_model_delegate()->OnIconChanged(model_index);
//       }
//     }
}

int RestoreMenuModel::GetRestoreItemCount() const {
  int items = 0;

  sessions::TabRestoreService* service = 
    TabRestoreServiceFactory::GetForProfile(browser_->profile());
  if (service)
    items = service->entries().size();

  if (items > kMaxRestoreItems)
    items = kMaxRestoreItems;
  else if (items < 0)
    items = 0;

  return items;
}

bool RestoreMenuModel::ItemHasCommand(int index) const {
  return index < GetItemCount() && !IsSeparator(index);
}

bool RestoreMenuModel::ItemHasIcon(int index) const {
  return index < GetItemCount() && !IsSeparator(index);
}

sessions::TabRestoreService::Entry* RestoreMenuModel::GetTabRestoreEntry(int index) const
{
  sessions::TabRestoreService* service = 
    TabRestoreServiceFactory::GetForProfile(browser_->profile());
  if (service) {
    const sessions::TabRestoreService::Entries& entries =
      service->entries();
    for (sessions::TabRestoreService::Entries::const_iterator it = entries.begin();
      it != entries.end(); ++it, index--)
    {
      if (index == 0) {
        sessions::TabRestoreService::Entry* entry = it->get();
        return entry;
      }
    }
  }

  NOTREACHED();
  return NULL;
}

std::string RestoreMenuModel::BuildActionName(
  const std::string& action, int index) const {
    DCHECK(!action.empty());
    DCHECK(index >= -1);
    std::string metric_string;
    metric_string += "RestoreMenu_";
    metric_string += action;
    if (index != -1) {
      // +1 is for historical reasons (indices used to start at 1).
      metric_string += base::IntToString(index + 1);
    }
    return metric_string;
}
