// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a class that contains various method related to branding.
// It provides only default implementations of these methods. Usually to add
// specific branding, we will need to extend this class with a custom
// implementation.

#include "chrome/installer/util/browser_distribution.h"

#include <utility>

#include "base/atomicops.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_icon_resources_win.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/util/app_registration_data.h"
#include "chrome/installer/util/chrome_frame_distribution.h"
#include "chrome/installer/util/chromium_binaries_distribution.h"
#include "chrome/installer/util/google_chrome_binaries_distribution.h"
#include "chrome/installer/util/google_chrome_distribution.h"
#include "chrome/installer/util/google_chrome_sxs_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/non_updating_app_registration_data.h"

#include "base/win/i18n.h"

using installer::MasterPreferences;

namespace {

const wchar_t kChromiumActiveSetupGuid[] =
    L"{7D2B3E1D-D096-4594-9D8F-A6667F12E0AC}";

const wchar_t kCommandExecuteImplUuid[] =
    L"{A2DF06F9-A21A-44A8-8A99-8B9C84F29160}";

// The BrowserDistribution objects are never freed.
BrowserDistribution* g_browser_distribution = NULL;
BrowserDistribution* g_chrome_frame_distribution = NULL;
BrowserDistribution* g_binaries_distribution = NULL;

BrowserDistribution::Type GetCurrentDistributionType() {
  return BrowserDistribution::CHROME_BROWSER;
}

bool GetUserDefaultUILanguage(std::wstring* language, std::wstring* region) {
  DCHECK(language);

  LANGID lang_id = ::GetUserDefaultUILanguage();
  if (LOCALE_CUSTOM_UI_DEFAULT != lang_id) {
    const LCID locale_id = MAKELCID(lang_id, SORT_DEFAULT);
    // max size for LOCALE_SISO639LANGNAME and LOCALE_SISO3166CTRYNAME is 9
    wchar_t result_buffer[9];
    int result_length =
      GetLocaleInfo(locale_id, LOCALE_SISO639LANGNAME, &result_buffer[0],
        arraysize(result_buffer));
    DPCHECK(0 != result_length) << "Failed getting language id";
    if (1 < result_length) {
      language->assign(&result_buffer[0], result_length - 1);
      region->clear();
      if (SUBLANG_NEUTRAL != SUBLANGID(lang_id)) {
        result_length =
          GetLocaleInfo(locale_id, LOCALE_SISO3166CTRYNAME, &result_buffer[0],
            arraysize(result_buffer));
        DPCHECK(0 != result_length) << "Failed getting region id";
        if (1 < result_length)
          region->assign(&result_buffer[0], result_length - 1);
      }
      return true;
    }
  }
  else {
    // This is entirely unexpected on pre-Vista, which is the only time we
    // should try GetUserDefaultUILanguage anyway.
    NOTREACHED() << "Cannot determine language for a supplemental locale.";
  }
  return false;
}

}  // namespace

BrowserDistribution::BrowserDistribution()
    : type_(CHROME_BROWSER),
      app_reg_data_(base::MakeUnique<NonUpdatingAppRegistrationData>(
          L"Software\\Chromium")) {
  std::wstring lan_region;
  if (!GetUserDefaultUILanguage(&m_language, &lan_region))
    m_language = L"en";
}

BrowserDistribution::BrowserDistribution(
    Type type,
    std::unique_ptr<AppRegistrationData> app_reg_data)
    : type_(type), app_reg_data_(std::move(app_reg_data)) {
  std::wstring lan_region;
  if (!GetUserDefaultUILanguage(&m_language, &lan_region))
    m_language = L"en";
}

BrowserDistribution::~BrowserDistribution() {}

template<class DistributionClass>
BrowserDistribution* BrowserDistribution::GetOrCreateBrowserDistribution(
    BrowserDistribution** dist) {
  if (!*dist) {
    DistributionClass* temp = new DistributionClass();
    if (base::subtle::NoBarrier_CompareAndSwap(
            reinterpret_cast<base::subtle::AtomicWord*>(dist), NULL,
            reinterpret_cast<base::subtle::AtomicWord>(temp)) != NULL)
      delete temp;
  }

  return *dist;
}

BrowserDistribution* BrowserDistribution::GetDistribution() {
  return GetSpecificDistribution(GetCurrentDistributionType());
}

// static
BrowserDistribution* BrowserDistribution::GetSpecificDistribution(
    BrowserDistribution::Type type) {
  BrowserDistribution* dist = NULL;

  switch (type) {
    case CHROME_BROWSER:
#if defined(GOOGLE_CHROME_BUILD)
      if (InstallUtil::IsChromeSxSProcess()) {
        dist = GetOrCreateBrowserDistribution<GoogleChromeSxSDistribution>(
            &g_browser_distribution);
      } else {
        dist = GetOrCreateBrowserDistribution<GoogleChromeDistribution>(
            &g_browser_distribution);
      }
#else
      dist = GetOrCreateBrowserDistribution<BrowserDistribution>(
          &g_browser_distribution);
#endif
      break;

    case CHROME_FRAME:
      dist = GetOrCreateBrowserDistribution<ChromeFrameDistribution>(
          &g_chrome_frame_distribution);
      break;

    default:
      DCHECK_EQ(CHROME_BINARIES, type);
#if defined(GOOGLE_CHROME_BUILD)
      dist = GetOrCreateBrowserDistribution<GoogleChromeBinariesDistribution>(
          &g_binaries_distribution);
#else
      dist = GetOrCreateBrowserDistribution<ChromiumBinariesDistribution>(
          &g_binaries_distribution);
#endif
  }

  return dist;
}

const AppRegistrationData& BrowserDistribution::GetAppRegistrationData() const {
  return *app_reg_data_;
}

base::string16 BrowserDistribution::GetAppGuid() const {
  return app_reg_data_->GetAppGuid();
}

base::string16 BrowserDistribution::GetStateKey() const {
  return app_reg_data_->GetStateKey();
}

base::string16 BrowserDistribution::GetStateMediumKey() const {
  return app_reg_data_->GetStateMediumKey();
}

base::string16 BrowserDistribution::GetVersionKey() const {
  return app_reg_data_->GetVersionKey();
}

void BrowserDistribution::DoPostUninstallOperations(
    const base::Version& version, const base::FilePath& local_data_path,
    const base::string16& distribution_data) {
}

base::string16 BrowserDistribution::GetActiveSetupGuid() {
  return kChromiumActiveSetupGuid;
}

base::string16 BrowserDistribution::GetBaseAppName() {
  if (m_language == L"zh")
    return L"ÄûÃÊä¯ÀÀÆ÷";
  return L"LemonBrowser";
}

base::string16 BrowserDistribution::GetDisplayName() {
  return GetShortcutName();
}

base::string16 BrowserDistribution::GetShortcutName() {
  return GetBaseAppName();
}

int BrowserDistribution::GetIconIndex() {
  return icon_resources::kApplicationIndex;
}

base::string16 BrowserDistribution::GetIconFilename() {
  return installer::kChromeExe;
}

base::string16 BrowserDistribution::GetStartMenuShortcutSubfolder(
    Subfolder subfolder_type) {
  switch (subfolder_type) {
    case SUBFOLDER_APPS:
      return installer::GetLocalizedString(IDS_APP_SHORTCUTS_SUBDIR_NAME_BASE);
    default:
      DCHECK_EQ(SUBFOLDER_CHROME, subfolder_type);
      return GetShortcutName();
  }
}

base::string16 BrowserDistribution::GetBaseAppId() {
  return L"LemonBrowser";
}

base::string16 BrowserDistribution::GetBrowserProgIdPrefix() {
  // This used to be "ChromiumHTML", but was forced to become "ChromiumHTM"
  // because of http://crbug.com/153349.  See the declaration of this function
  // in the header file for more details.
  return L"LemonHTM";
}

base::string16 BrowserDistribution::GetBrowserProgIdDesc() {
  return L"Lemon HTML Document";
}


base::string16 BrowserDistribution::GetInstallSubDir() {
  return L"LemonBrowser";
}

base::string16 BrowserDistribution::GetPublisherName() {
  return L"LemonBrowser";
}

base::string16 BrowserDistribution::GetAppDescription() {
  return L"Browse the web";
}

base::string16 BrowserDistribution::GetLongAppDescription() {
  const base::string16& app_description =
      installer::GetLocalizedString(IDS_PRODUCT_DESCRIPTION_BASE);
  return app_description;
}

std::string BrowserDistribution::GetSafeBrowsingName() {
  return "LemonBrowser";
}

base::string16 BrowserDistribution::GetDistributionData(HKEY root_key) {
  return L"";
}

base::string16 BrowserDistribution::GetRegistryPath() {
  return base::string16(L"Software\\").append(GetInstallSubDir());
}

base::string16 BrowserDistribution::GetUninstallRegPath() {
  return L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\LemonBrowser";
}

BrowserDistribution::DefaultBrowserControlPolicy
    BrowserDistribution::GetDefaultBrowserControlPolicy() {
  return DEFAULT_BROWSER_FULL_CONTROL;
}

bool BrowserDistribution::CanCreateDesktopShortcuts() {
  return true;
}

bool BrowserDistribution::GetChromeChannel(base::string16* channel) {
  return false;
}

base::string16 BrowserDistribution::GetCommandExecuteImplClsid() {
  return kCommandExecuteImplUuid;
}

void BrowserDistribution::UpdateInstallStatus(bool system_install,
    installer::ArchiveType archive_type,
    installer::InstallStatus install_status) {
}

bool BrowserDistribution::ShouldSetExperimentLabels() {
  return false;
}

bool BrowserDistribution::HasUserExperiments() {
  return false;
}
