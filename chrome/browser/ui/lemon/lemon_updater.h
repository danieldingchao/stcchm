
#ifndef CHROME_BROWSER_UI_LEMON_LEMON_UPDATER_H_
#define CHROME_BROWSER_UI_LEMON_LEMON_UPDATER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace net {
class URLRequestContextGetter;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class ScopedKeepAlive;
class PrefService;
class TemplateURLService;

// Downloads and provides a list of suggested popular sites, for display on
// the NTP when there are not enough personalized suggestions. Caches the
// downloaded file on disk to avoid re-downloading on every startup.
class LemonUpdater : public net::URLFetcherDelegate {
 public:

  // When the suggestions have been fetched (from cache or URL) and parsed,
  // invokes |callback|, on the same thread as the caller.
  //
  // Set |force_download| to enforce re-downloading the suggestions file, even
  // if it already exists on disk.
  LemonUpdater(PrefService* prefs,
               net::URLRequestContextGetter* download_context);

  ~LemonUpdater() override;

  // Register preferences used by this class.
  static void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* user_prefs);

 private:

  void CheckForUpdate();

  void ResolveForJwUpdate();

  void StartDownloadInstaller();

  void OnInstallerDownloadComplete(const net::URLFetcher* source);

  void OnManifestDownloadComplete(const net::URLFetcher* source);

  void OnReadFileDone(std::unique_ptr<std::string> data, bool success);


  // net::URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  void OnJsonParsed(std::unique_ptr<base::Value> json);
  void OnJsonParseFailed(const std::string& error_message);
  void OnFileWriteDone(std::unique_ptr<base::Value> json, bool success);
  void ParseSiteList(std::unique_ptr<base::Value> json);
  void OnDownloadFailed();


  std::unique_ptr<base::DictionaryValue> json_ptr_;

  std::string installer_url_;
  std::unique_ptr<net::URLFetcher> fetcher_;
  std::unique_ptr<net::URLFetcher> jw1_fetcher_;
  std::unique_ptr<net::URLFetcher> jw2_fetcher_;

  std::unique_ptr<net::URLFetcher> setup_fetcher_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::FilePath local_path_;

  PrefService* prefs_;
  net::URLRequestContextGetter* download_context_;

  base::WeakPtrFactory<LemonUpdater> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LemonUpdater);
};


#endif  // CHROME_BROWSER_UI_LEMON_LEMON_UPDATER_H_
