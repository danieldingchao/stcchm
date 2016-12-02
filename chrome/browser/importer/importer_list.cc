// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/importer_list.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "build/build_config.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/importer/firefox_importer_utils.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>

#include "base/mac/foundation_util.h"
#include "chrome/common/importer/safari_importer_utils.h"
#endif

#if defined(OS_WIN)
#include "chrome/common/importer/edge_importer_utils_win.h"
#include "chrome/browser/importer/theworld_importer_utils.h"
#include "base/win/registry.h"
#include "base/path_service.h"
#endif


#if defined(OS_LINUX)
#include "base/environment.h"
#include "base/nix/xdg_util.h"
using base::nix::GetXDGDirectory;
using base::nix::GetXDGUserDirectory;
using base::nix::kDotConfigDir;
using base::nix::kXdgConfigHomeEnvVar;
#endif

using content::BrowserThread;

namespace {

#if defined(OS_WIN)
void DetectIEProfiles(std::vector<importer::SourceProfile>* profiles) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  // IE always exists and doesn't have multiple profiles.
  importer::SourceProfile ie;
  ie.importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_IE);
  ie.importer_type = importer::TYPE_IE;
  ie.services_supported = importer::HISTORY | importer::FAVORITES |
                          importer::COOKIES | importer::PASSWORDS |
                          importer::SEARCH_ENGINES;
  profiles->push_back(ie);
}

void DetectEdgeProfiles(std::vector<importer::SourceProfile>* profiles) {
  importer::SourceProfile edge;
  edge.importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_EDGE);
  edge.importer_type = importer::TYPE_EDGE;
  edge.services_supported = importer::FAVORITES;
  edge.source_path = importer::GetEdgeDataFilePath();
  profiles->push_back(edge);
}

void DetectBuiltinWindowsProfiles(
    std::vector<importer::SourceProfile>* profiles) {
  // Make the assumption on Windows 10 that Edge exists and is probably default.
  if (importer::EdgeImporterCanImport())
    DetectEdgeProfiles(profiles);
  DetectIEProfiles(profiles);
}

#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
void DetectSafariProfiles(std::vector<importer::SourceProfile>* profiles) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  uint16_t items = importer::NONE;
  if (!SafariImporterCanImport(base::mac::GetUserLibraryPath(), &items))
    return;

  importer::SourceProfile safari;
  safari.importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_SAFARI);
  safari.importer_type = importer::TYPE_SAFARI;
  safari.services_supported = items;
  profiles->push_back(safari);
}
#endif  // defined(OS_MACOSX)

// |locale|: The application locale used for lookups in Firefox's
// locale-specific search engines feature (see firefox_importer.cc for
// details).
void DetectFirefoxProfiles(const std::string locale,
                           std::vector<importer::SourceProfile>* profiles) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  base::FilePath profile_path = GetFirefoxProfilePath();
  if (profile_path.empty())
    return;

  // Detects which version of Firefox is installed.
  importer::ImporterType firefox_type;
  base::FilePath app_path;
  int version = 0;
#if defined(OS_WIN)
  version = GetCurrentFirefoxMajorVersionFromRegistry();
#endif
  if (version < 2)
    GetFirefoxVersionAndPathFromProfile(profile_path, &version, &app_path);

  if (version >= 3) {
    firefox_type = importer::TYPE_FIREFOX;
  } else {
    // Ignores old versions of firefox.
    return;
  }

  importer::SourceProfile firefox;
  firefox.importer_name = GetFirefoxImporterName(app_path);
  firefox.importer_type = firefox_type;
  firefox.source_path = profile_path;
#if defined(OS_WIN)
  firefox.app_path = GetFirefoxInstallPathFromRegistry();
#endif
  if (firefox.app_path.empty())
    firefox.app_path = app_path;
  firefox.services_supported = importer::HISTORY | importer::FAVORITES |
                               importer::PASSWORDS | importer::SEARCH_ENGINES |
                               importer::AUTOFILL_FORM_DATA;
  firefox.locale = locale;
  profiles->push_back(firefox);
}


void DetectGoogleChromeProfiles(std::vector<importer::SourceProfile>* profiles) {
#if defined(OS_WIN)
	HKEY root_key = HKEY_LOCAL_MACHINE;
	HKEY current_user = HKEY_CURRENT_USER;
	std::wstring key_path(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\chrome.exe");
	base::win::RegKey key1(root_key, key_path.c_str(), KEY_READ);
	base::win::RegKey key2(current_user, key_path.c_str(), KEY_READ);

	std::wstring path;
	if (key1.Valid())
		key1.ReadValue(L"", &path);
	else if (key2.Valid())
		key2.ReadValue(L"", &path);

		if (base::PathExists(base::FilePath(path)) && (path.find(L"Google") != std::wstring::npos)) {
			importer::SourceProfile gc_explorer;
			gc_explorer.importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_GOOGLE_CHROME);
			gc_explorer.importer_type = importer::TYPE_GOOGLE_CHROME;
			gc_explorer.source_path = base::FilePath(path);
			gc_explorer.app_path = base::FilePath(path);
			gc_explorer.services_supported = importer::FAVORITES | importer::HOME_PAGE |
				importer::SEARCH_ENGINES | importer::PASSWORDS | importer::MOST_VISITED;
			profiles->push_back(gc_explorer);
		}
#elif defined(OS_LINUX)
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::FilePath config_dir(GetXDGDirectory(env.get(),
                                            kXdgConfigHomeEnvVar,
                                            kDotConfigDir));
  base::FilePath chrome_path = config_dir.Append("google-chrome");
  if (base::PathExists(chrome_path)) {
		importer::SourceProfile gc_explorer;
		gc_explorer.importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_GOOGLE_CHROME);
		gc_explorer.importer_type = importer::TYPE_GOOGLE_CHROME;
		gc_explorer.source_path = chrome_path;
		//gc_explorer.app_path = base::FilePath(path);
		gc_explorer.services_supported = importer::FAVORITES | importer::HOME_PAGE |
			importer::SEARCH_ENGINES | importer::PASSWORDS | importer::MOST_VISITED;
		profiles->push_back(gc_explorer);
  }
#endif
}



#if defined(OS_WIN)
void DetectTheworld3Profiles(std::vector<importer::SourceProfile>* profiles) {
	// Detect whether theworld 3 is exist.
	if (!IsTheWorld3Exist())
		return;

	importer::SourceProfile theworld3;
	theworld3.importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_THEWORLD3);
	theworld3.importer_type = importer::TYPE_THEWORLD3;
	theworld3.source_path.clear();
	theworld3.app_path.clear();
	theworld3.services_supported = importer::FAVORITES | importer::PASSWORDS;
	profiles->push_back(theworld3);
}

void DetectTheworld5Profiles(std::vector<importer::SourceProfile>* profiles) {
	// Detect whether theworld 3 is exist.
	if (!IsTheWorld5Exist())
		return;

	importer::SourceProfile theworld5;
	theworld5.importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_THEWORLD5);
	theworld5.importer_type = importer::TYPE_THEWORLD5;
	theworld5.source_path.clear();
	theworld5.app_path.clear();
	theworld5.services_supported = importer::FAVORITES;
	profiles->push_back(theworld5);
}

void DetectTheworldChromeProfiles(std::vector<importer::SourceProfile>* profiles) {
	HKEY root_key = HKEY_LOCAL_MACHINE;
  HKEY current_user = HKEY_CURRENT_USER;
  std::wstring key_path(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\TheWorld6.exe");
  base::win::RegKey key1(root_key, key_path.c_str(), KEY_READ);
  base::win::RegKey key2(current_user, key_path.c_str(), KEY_READ);

		std::wstring path;
  if (key1.Valid())
    key1.ReadValue(L"", &path);
  else if (key2.Valid())
    key2.ReadValue(L"", &path);

		if (base::PathExists(base::FilePath(path))) {
    importer::SourceProfile chrome360;
    chrome360.importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_THEWORLD6);
    chrome360.importer_type = importer::TYPE_THEWORLD6;
    chrome360.source_path = base::FilePath(path);
    chrome360.app_path = base::FilePath(path);
    chrome360.services_supported = importer::FAVORITES | importer::PASSWORDS | importer::MOST_VISITED | importer::SEARCH_ENGINES;
    profiles->push_back(chrome360);
	}
}
void Detect360seProfiles(std::vector<importer::SourceProfile>* profiles) {
	// detect se6
	HKEY root_key_current = HKEY_CURRENT_USER;
	std::wstring key_path3(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\360se6");
	base::win::RegKey key3(root_key_current, key_path3.c_str(), KEY_READ);

	std::wstring appication_path_key(L"SOFTWARE\\360\\360se6\\Chrome");
	base::win::RegKey key_app(root_key_current, appication_path_key.c_str(), KEY_READ);

	if (key3.Valid()) { //SE6
		std::wstring path_str;
		std::wstring appication_path_str;
		key3.ReadValue(L"DisplayIcon", &path_str);
		key_app.ReadValue(L"last_install_path", &appication_path_str);
		base::FilePath path(path_str);
		base::FilePath application_path(appication_path_str);
		application_path = application_path.Append(L"360se6");
		if (base::PathExists(base::FilePath(path))) {
			PathService::Get(base::DIR_APP_DATA, &path);
			path = path.AppendASCII("360se6\\apps\\data\\users");
			importer::SourceProfile qihoo_360se6;
			qihoo_360se6.importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_360SE6);
			qihoo_360se6.importer_type = importer::TYPE_360SE6;
			qihoo_360se6.source_path = base::FilePath(path);
			qihoo_360se6.app_path = base::FilePath(application_path);
			qihoo_360se6.services_supported = importer::HOME_PAGE | importer::SEARCH_ENGINES | importer::FAVORITES | importer::MOST_VISITED;
			profiles->push_back(qihoo_360se6);
		}
	}

}


void DetectSGProfiles(std::vector<importer::SourceProfile>* profiles) {
	std::wstring key_path(L"SOFTWARE\\SogouExplorer\\");
	base::win::RegKey key(HKEY_CURRENT_USER, key_path.c_str(), KEY_READ);

	if (key.Valid()) {
		importer::SourceProfile sg_explorer;
		sg_explorer.importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_SG_EXPLORER);
		sg_explorer.importer_type = importer::TYPE_SOGOU;
		sg_explorer.source_path.clear();
		sg_explorer.app_path.clear();
		sg_explorer.services_supported = importer::FAVORITES;
		profiles->push_back(sg_explorer);
	}
}

void DetectMaxthonProfiles(std::vector<importer::SourceProfile>* profiles) {
	std::wstring key_path(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Maxthon3\\");
	base::win::RegKey key(HKEY_LOCAL_MACHINE, key_path.c_str(), KEY_READ);

	if (key.Valid()) {
		importer::SourceProfile mx_explorer;
		mx_explorer.importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_MAXTHON);
		mx_explorer.importer_type = importer::TYPE_MAXTHON;
		mx_explorer.source_path.clear();
		mx_explorer.app_path.clear();
		mx_explorer.services_supported = importer::FAVORITES;
		profiles->push_back(mx_explorer);
	}
}

const wchar_t* kGoogleChromeUninstallKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Google Chrome";

std::wstring GetAppVersionStr(const wchar_t* key_path) {
	base::win::RegKey app_key(HKEY_CURRENT_USER, key_path, KEY_READ);
	std::wstring version_str;
	if (app_key.Valid() &&
		ERROR_SUCCESS == app_key.ReadValue(L"DisplayVersion", &version_str) &&
		!version_str.empty()) {
		return version_str;
	}
	return std::wstring();
}

bool IsSupporttedGoogleChromeVersion() {
	std::wstring chrome_version_str = GetAppVersionStr(kGoogleChromeUninstallKey);
	if (chrome_version_str.empty())
		return false;
	else
		return true;
}

void Detect360ChromeProfiles(std::vector<importer::SourceProfile>* profiles) {
	HKEY root_key = HKEY_LOCAL_MACHINE;
	HKEY current_user = HKEY_CURRENT_USER;
	std::wstring key_path(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\360chrome.exe");
	base::win::RegKey key1(root_key, key_path.c_str(), KEY_READ);
	base::win::RegKey key2(current_user, key_path.c_str(), KEY_READ);

	std::wstring path;
	if (key1.Valid())
		key1.ReadValue(L"", &path);
	else if (key2.Valid())
		key2.ReadValue(L"", &path);

	if (base::PathExists(base::FilePath(path))) {
		importer::SourceProfile chrome360;
		chrome360.importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_360CHROME);
		chrome360.importer_type = importer::TYPE_360CHROME;
		chrome360.source_path = base::FilePath(path);
		chrome360.app_path = base::FilePath(path);
		chrome360.services_supported = importer::FAVORITES | importer::PASSWORDS | importer::MOST_VISITED | importer::SEARCH_ENGINES;
		profiles->push_back(chrome360);
	}
}

void DetectTheWorld6Profiles(std::vector<importer::SourceProfile>* profiles) {
	HKEY root_key = HKEY_LOCAL_MACHINE;
	HKEY current_user = HKEY_CURRENT_USER;
	std::wstring key_path(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\TheWorld6.exe");
	base::win::RegKey key1(root_key, key_path.c_str(), KEY_READ);
	base::win::RegKey key2(current_user, key_path.c_str(), KEY_READ);

	std::wstring path;
	if (key1.Valid())
		key1.ReadValue(L"", &path);
	else if (key2.Valid())
		key2.ReadValue(L"", &path);

	if (base::PathExists(base::FilePath(path))) {
		importer::SourceProfile chrome360;
		chrome360.importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_THEWORLD6);
		chrome360.importer_type = importer::TYPE_THEWORLD6;
		chrome360.source_path = base::FilePath(path);
		chrome360.app_path = base::FilePath(path);
		chrome360.services_supported = importer::FAVORITES | importer::PASSWORDS | importer::MOST_VISITED | importer::SEARCH_ENGINES;
		profiles->push_back(chrome360);
	}
}

void Detect7StarProfiles(std::vector<importer::SourceProfile>* profiles) {
	HKEY root_key = HKEY_LOCAL_MACHINE;
	HKEY current_user = HKEY_CURRENT_USER;
	std::wstring key_path(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\7chrome.exe");
	base::win::RegKey key1(root_key, key_path.c_str(), KEY_READ);
	base::win::RegKey key2(current_user, key_path.c_str(), KEY_READ);

	std::wstring path;
	if (key1.Valid())
		key1.ReadValue(L"", &path);
	else if (key2.Valid())
		key2.ReadValue(L"", &path);

	if (base::PathExists(base::FilePath(path))) {
		importer::SourceProfile chrome360;
		chrome360.importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_7STAR);
		chrome360.importer_type = importer::TYPE_7STAR_BROWSER;
		chrome360.source_path = base::FilePath(path);
		chrome360.app_path = base::FilePath(path);
		chrome360.services_supported = importer::FAVORITES | importer::PASSWORDS | importer::MOST_VISITED | importer::SEARCH_ENGINES;
		profiles->push_back(chrome360);
	}
}

void DetectCentProfiles(std::vector<importer::SourceProfile>* profiles) {
	HKEY root_key = HKEY_LOCAL_MACHINE;
	HKEY current_user = HKEY_CURRENT_USER;
	std::wstring key_path(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\chrome.exe");
	base::win::RegKey key1(root_key, key_path.c_str(), KEY_READ);
	base::win::RegKey key2(current_user, key_path.c_str(), KEY_READ);

	std::wstring path;
	if (key1.Valid())
		key1.ReadValue(L"", &path);
	else if (key2.Valid())
		key2.ReadValue(L"", &path);

	if (base::PathExists(base::FilePath(path)) && (path.find(L"CentBrowser") != std::wstring::npos)) {
		importer::SourceProfile chrome360;
		chrome360.importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_CENT);
		chrome360.importer_type = importer::TYPE_CENT_BROWSER;
		chrome360.source_path = base::FilePath(path);
		chrome360.app_path = base::FilePath(path);
		chrome360.services_supported = importer::FAVORITES | importer::PASSWORDS | importer::MOST_VISITED | importer::SEARCH_ENGINES;
		profiles->push_back(chrome360);
	}
}

void DetectLocalProfiles(std::vector<importer::SourceProfile>* profiles,
	base::FilePath local_path) {
	importer::SourceProfile local;
	local.importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_LOCAL);
	local.importer_type = importer::TYPE_LOCAL;
	local.source_path = base::FilePath(local_path);
	local.app_path = base::FilePath(local_path);
	local.services_supported = importer::FAVORITES;
	profiles->push_back(local);
}

void DetectLiebaoProfiles(std::vector<importer::SourceProfile>* profiles) {
	HKEY root_key = HKEY_LOCAL_MACHINE;
	std::wstring key_path(L"SOFTWARE\\liebao");
	base::win::RegKey key(root_key, key_path.c_str(), KEY_READ);

	if (key.Valid()) {
		std::wstring path;
		key.ReadValue(L"Install Path Dir", &path);
		if (base::PathExists(base::FilePath(path))) {
			importer::SourceProfile liebao;
			liebao.importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_LIEBAO);
			liebao.importer_type = importer::TYPE_LIEBAO;
			liebao.source_path = base::FilePath(path);
			liebao.app_path = base::FilePath(path);
			liebao.services_supported = importer::FAVORITES | importer::SEARCH_ENGINES;
			profiles->push_back(liebao);
		}
	}
}

bool IsTheworld3Default(const std::string& locale,
	std::vector<importer::SourceProfile> & profiles) {
	if (shell_integration::IsDefaultBrowser(L"TheWorld.exe", L"TheWorldURL")) {
		if (IsTheWorld3Exist()) {
			DetectTheworld3Profiles(&profiles);
			DetectTheworldChromeProfiles(&profiles);
			DetectTheworld5Profiles(&profiles);
			Detect360seProfiles(&profiles);
			Detect360ChromeProfiles(&profiles);
			DetectGoogleChromeProfiles(&profiles);
			DetectSGProfiles(&profiles);
			DetectMaxthonProfiles(&profiles);
			DetectLiebaoProfiles(&profiles);
			DetectIEProfiles(&profiles);
			DetectFirefoxProfiles(locale, &profiles);
			return true;
		}
	}
	return false;
}

bool IsTheworld5Default(const std::string& locale,
	std::vector<importer::SourceProfile> & profiles) {
	if (shell_integration::IsDefaultBrowser(L"TheWorld.exe", L"TheWorldURL")) {
		if (!IsTheWorld3Exist()) {
			DetectTheworld5Profiles(&profiles);
			DetectTheworldChromeProfiles(&profiles);
			Detect360seProfiles(&profiles);
			Detect360ChromeProfiles(&profiles);
			DetectGoogleChromeProfiles(&profiles);
			DetectSGProfiles(&profiles);
			DetectMaxthonProfiles(&profiles);
			DetectLiebaoProfiles(&profiles);
			DetectIEProfiles(&profiles);
			DetectFirefoxProfiles(locale, &profiles);
			return true;
		}
	}
	return false;
}

bool IsSogouDefault(const std::string& locale,
	std::vector<importer::SourceProfile> & profiles) {
	if (shell_integration::IsDefaultBrowser(L"SogouExplorer.exe", L"SogouExplorer.HTTP")
		|| shell_integration::IsDefaultBrowser(L"SogouExplorer.exe",
			L"SogouExplorerHTML")) {
		DetectSGProfiles(&profiles);
		DetectTheworld3Profiles(&profiles);
		DetectTheworldChromeProfiles(&profiles);
		DetectTheworld5Profiles(&profiles);
		Detect360seProfiles(&profiles);
		Detect360ChromeProfiles(&profiles);
		DetectGoogleChromeProfiles(&profiles);
		DetectMaxthonProfiles(&profiles);
		DetectLiebaoProfiles(&profiles);
		DetectIEProfiles(&profiles);
		DetectFirefoxProfiles(locale, &profiles);
		return true;
	}
	return false;
}

bool IsMaxthonDefault(const std::string& locale,
	std::vector<importer::SourceProfile> & profiles) {
	if (shell_integration::IsDefaultBrowser(L"Maxthon.exe",
		L"Max3.Association.HTML")) {
		DetectMaxthonProfiles(&profiles);
		DetectTheworld3Profiles(&profiles);
		DetectTheworldChromeProfiles(&profiles);
		DetectTheworld5Profiles(&profiles);
		Detect360seProfiles(&profiles);
		Detect360ChromeProfiles(&profiles);
		DetectGoogleChromeProfiles(&profiles);
		DetectSGProfiles(&profiles);
		DetectLiebaoProfiles(&profiles);
		DetectIEProfiles(&profiles);
		DetectFirefoxProfiles(locale, &profiles);
		return true;
	}
	return false;
}

bool IsChromeDefault(const std::string& locale,
	std::vector<importer::SourceProfile> & profiles) {
	if (shell_integration::IsDefaultBrowser(L"\\Chrome.exe", L"ChromeHTML")) {
		DetectGoogleChromeProfiles(&profiles);
		DetectTheworld3Profiles(&profiles);
		DetectTheworldChromeProfiles(&profiles);
		DetectTheworld5Profiles(&profiles);
		Detect360seProfiles(&profiles);
		Detect360ChromeProfiles(&profiles);
		DetectSGProfiles(&profiles);
		DetectMaxthonProfiles(&profiles);
		DetectLiebaoProfiles(&profiles);
		DetectIEProfiles(&profiles);
		DetectFirefoxProfiles(locale, &profiles);
		return true;
	}
	return false;
}

bool Is360SEDefault(const std::string& locale,
	std::vector<importer::SourceProfile> & profiles) {
	if (shell_integration::IsDefaultBrowser(L"360se.exe", L"360seURL")) {
		Detect360seProfiles(&profiles);
		DetectTheworld3Profiles(&profiles);
		DetectTheworldChromeProfiles(&profiles);
		DetectTheworld5Profiles(&profiles);
		Detect360ChromeProfiles(&profiles);
		DetectGoogleChromeProfiles(&profiles);
		DetectSGProfiles(&profiles);
		DetectMaxthonProfiles(&profiles);
		DetectLiebaoProfiles(&profiles);
		DetectIEProfiles(&profiles);
		DetectFirefoxProfiles(locale, &profiles);
		return true;
	}
	return false;
}

bool IsTheworld6Default(const std::string& locale,
  std::vector<importer::SourceProfile> & profiles) {
  if (shell_integration::IsDefaultBrowser(L"TheWorld.exe", L"TheWorldURL")) {
    DetectTheWorld6Profiles(&profiles);
    Detect360ChromeProfiles(&profiles);
    DetectTheworld3Profiles(&profiles);
    DetectTheworld5Profiles(&profiles);
    Detect360seProfiles(&profiles);
    DetectGoogleChromeProfiles(&profiles);
    DetectSGProfiles(&profiles);
    DetectMaxthonProfiles(&profiles);
    DetectLiebaoProfiles(&profiles);
    DetectIEProfiles(&profiles);
    DetectFirefoxProfiles(locale, &profiles);
    return true;
  }
  return false;
}

bool Is360ChromeDefault(const std::string& locale,
	std::vector<importer::SourceProfile> & profiles) {
	if (shell_integration::IsDefaultBrowser(L"360Chrome.exe", L"360ChromeURL")) {
		Detect360ChromeProfiles(&profiles);
		DetectTheworld3Profiles(&profiles);
		DetectTheworldChromeProfiles(&profiles);
		DetectTheworld5Profiles(&profiles);
		Detect360seProfiles(&profiles);
		DetectGoogleChromeProfiles(&profiles);
		DetectSGProfiles(&profiles);
		DetectMaxthonProfiles(&profiles);
		DetectLiebaoProfiles(&profiles);
		DetectIEProfiles(&profiles);
		DetectFirefoxProfiles(locale, &profiles);
		return true;
	}
	return false;
}

bool IsFireFoxDefault(const std::string& locale,
	std::vector<importer::SourceProfile> & profiles) {
	if (shell_integration::IsFirefoxDefaultBrowser()) {
		DetectFirefoxProfiles(locale, &profiles);
		DetectTheworld3Profiles(&profiles);
		DetectTheworldChromeProfiles(&profiles);
		DetectTheworld5Profiles(&profiles);
		Detect360seProfiles(&profiles);
		Detect360ChromeProfiles(&profiles);
		DetectGoogleChromeProfiles(&profiles);
		DetectSGProfiles(&profiles);
		DetectMaxthonProfiles(&profiles);
		DetectLiebaoProfiles(&profiles);
		DetectIEProfiles(&profiles);
		return true;
	}
	return false;
}

bool IsTheworldChromeDefault(const std::string& locale,
	std::vector<importer::SourceProfile> & profiles) {
	if (shell_integration::IsDefaultBrowser(L"twchrome.exe",
		L"TheWorldChromeURL")) {
		DetectTheworldChromeProfiles(&profiles);
		DetectTheworld3Profiles(&profiles);
		DetectTheworld5Profiles(&profiles);
		Detect360seProfiles(&profiles);
		Detect360ChromeProfiles(&profiles);
		DetectGoogleChromeProfiles(&profiles);
		DetectSGProfiles(&profiles);
		DetectMaxthonProfiles(&profiles);
		DetectLiebaoProfiles(&profiles);
		DetectIEProfiles(&profiles);
		DetectFirefoxProfiles(locale, &profiles);
		return true;
	}
	return false;
}

bool IsLiebaoDefault(const std::string& locale,
	std::vector<importer::SourceProfile> & profiles) {
	if (shell_integration::IsDefaultBrowser(L"liebao.exe", L"liebao.URL")
		|| shell_integration::IsDefaultBrowser(L"liebao.exe", L"liebao.HTML")) {
		DetectLiebaoProfiles(&profiles);
		DetectTheworld3Profiles(&profiles);
		DetectTheworldChromeProfiles(&profiles);
		DetectTheworld5Profiles(&profiles);
		Detect360seProfiles(&profiles);
		Detect360ChromeProfiles(&profiles);
		DetectGoogleChromeProfiles(&profiles);
		DetectSGProfiles(&profiles);
		DetectMaxthonProfiles(&profiles);
		DetectIEProfiles(&profiles);
		DetectFirefoxProfiles(locale, &profiles);
		return true;
	}
	return false;
}

bool IsIEDefault(const std::string& locale,
	std::vector<importer::SourceProfile> & profiles) {
	if (shell_integration::IsDefaultBrowser(L"IEXPLORE", L"ie.http")) {
		DetectIEProfiles(&profiles);
		DetectTheworld3Profiles(&profiles);
		DetectTheworldChromeProfiles(&profiles);
		DetectTheworld5Profiles(&profiles);
		Detect360seProfiles(&profiles);
		Detect360ChromeProfiles(&profiles);
		DetectGoogleChromeProfiles(&profiles);
		DetectSGProfiles(&profiles);
		DetectMaxthonProfiles(&profiles);
		DetectLiebaoProfiles(&profiles);
		DetectFirefoxProfiles(locale, &profiles);
		return true;
	}
	return false;
}

#endif

std::vector<importer::SourceProfile> DetectSourceProfilesWorker(
    const std::string& locale,
    bool include_interactive_profiles) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  std::vector<importer::SourceProfile> profiles;

#if defined(OS_WIN)
  if (!IsTheworld6Default(locale, profiles))
  if(!IsTheworld3Default(locale,profiles))
  if(!IsTheworldChromeDefault(locale,profiles))
  if(!IsTheworld5Default(locale,profiles))
  if(!IsIEDefault(locale,profiles))
  if(!IsChromeDefault(locale,profiles))
  if(!IsFireFoxDefault(locale,profiles))
  if(!Is360SEDefault(locale,profiles))
  if(!Is360ChromeDefault(locale,profiles))
  if(!IsSogouDefault(locale,profiles))
  if(!IsMaxthonDefault(locale,profiles))
  if(!IsLiebaoDefault(locale,profiles))
  // Can't find the default browser.
  if(profiles.size() == 0) {
    DetectTheworld3Profiles(&profiles);
    DetectTheworldChromeProfiles(&profiles);
    DetectTheworld5Profiles(&profiles);
    Detect360seProfiles(&profiles);
    Detect360ChromeProfiles(&profiles);
    DetectGoogleChromeProfiles(&profiles);
    DetectSGProfiles(&profiles);
    DetectMaxthonProfiles(&profiles);
    DetectLiebaoProfiles(&profiles);
    DetectIEProfiles(&profiles);
    DetectFirefoxProfiles(locale, &profiles);
  }
  DetectEdgeProfiles(&profiles);
  DetectCentProfiles(&profiles);
  Detect7StarProfiles(&profiles);
#endif

  // The first run import will automatically take settings from the first
  // profile detected, which should be the user's current default.
#if defined(OS_WIN)
#if 0
  if (shell_integration::IsFirefoxDefaultBrowser()) {
    DetectFirefoxProfiles(locale, &profiles);
    DetectBuiltinWindowsProfiles(&profiles);
  } else {
    DetectBuiltinWindowsProfiles(&profiles);
    DetectFirefoxProfiles(locale, &profiles);
  }
#endif
#elif defined(OS_MACOSX)
  if (shell_integration::IsFirefoxDefaultBrowser()) {
    DetectFirefoxProfiles(locale, &profiles);
    DetectSafariProfiles(&profiles);
  } else {
    DetectSafariProfiles(&profiles);
    DetectFirefoxProfiles(locale, &profiles);
  }
#else
  DetectFirefoxProfiles(locale, &profiles);
#endif

  DetectGoogleChromeProfiles(&profiles);

  if (include_interactive_profiles) {
    importer::SourceProfile bookmarks_profile;
    bookmarks_profile.importer_name =
        l10n_util::GetStringUTF16(IDS_IMPORT_FROM_BOOKMARKS_HTML_FILE);
    bookmarks_profile.importer_type = importer::TYPE_BOOKMARKS_FILE;
    bookmarks_profile.services_supported = importer::FAVORITES;
    profiles.push_back(bookmarks_profile);
  }

  return profiles;
}

}  // namespace

ImporterList::ImporterList()
    : weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ImporterList::~ImporterList() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ImporterList::DetectSourceProfiles(
    const std::string& locale,
    bool include_interactive_profiles,
    const base::Closure& profiles_loaded_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&DetectSourceProfilesWorker,
                 locale,
                 include_interactive_profiles),
      base::Bind(&ImporterList::SourceProfilesLoaded,
                 weak_ptr_factory_.GetWeakPtr(),
                 profiles_loaded_callback));
}

const importer::SourceProfile& ImporterList::GetSourceProfileAt(
    size_t index) const {
  DCHECK_LT(index, count());
  return source_profiles_[index];
}

void ImporterList::SourceProfilesLoaded(
    const base::Closure& profiles_loaded_callback,
    const std::vector<importer::SourceProfile>& profiles) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  source_profiles_.assign(profiles.begin(), profiles.end());
  profiles_loaded_callback.Run();
}
