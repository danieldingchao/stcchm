
#include "chrome/browser/ui/lemon/lemon_updater.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/safe_json/safe_json_parser.h"
#include "content/public/browser/browser_thread.h"
#include "content/browser/browser_thread_impl.h"
#include "crypto/des.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/lifetime/keep_alive_registry.h"
#include "chrome/browser/lifetime/keep_alive_types.h"
#include "chrome/browser/lifetime/scoped_keep_alive.h"
#include "chrome/browser/browser_process.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"

#if defined(OS_WIN)
#include "base/Report/MIDGen.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/product.h"
#else
#include <fcntl.h>
#include <linux/hdreg.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#endif

using net::URLFetcher;

const char kJW1[] = "jw1.dat";
const char kJW2[] = "jw2.dat";

const char kAlterJW2Url[] = "http://www.koodroid.com/files/jw2.dat";


const char kUpdateCheckTestUrl[] =
    "http://www.koodroid.com/download/check";
    //"http://www.lemonbrowser.com/update.php?mid=%s&ver=%s&os=%s&pagetype=%d&locale=%s";

const char kUpdateCheckUrl[] =
    "http://www.lemonbrowser.com/update.php?mid=%s&ver=%s&os=%s&pagetype=%d&locale=%s";

const bool testUpdate = false;

const int kUpdateCheckIntervalHours = 12;

const int kUpdateDelayTimsMs = 10 * 1000;

const char kLastUpdateCheckTimePref[] = "last_update_check_time";

namespace{
  #if !defined(OS_WIN)
  int getDiskID(char *hardid) {
    int fd;
    struct hd_driveid hid;
    fd = open ("/dev/sda", O_RDONLY);
    if (fd < 0)
    {
        return -1;
    }
    if (ioctl (fd, HDIO_GET_IDENTITY, &hid) < 0)
    {
        return -1;
    }
    close (fd);
    sprintf(hardid,"%s", hid.serial_no);
    return 0;
  }
  int get_mac(char* mac)
  {
    int sockfd;
    struct ifreq tmp;
    char mac_addr[60];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if( sockfd < 0)
    {
        return -1;
    }

    memset(&tmp,0,sizeof(struct ifreq));
    strncpy(tmp.ifr_name,"eth0",sizeof(tmp.ifr_name)-1);
    if( (ioctl(sockfd,SIOCGIFHWADDR,&tmp)) < 0 )
    {
        return -1;
    }

    sprintf(mac_addr, "%02x%02x%02x%02x%02x%02x",
            (unsigned char)tmp.ifr_hwaddr.sa_data[0],
            (unsigned char)tmp.ifr_hwaddr.sa_data[1],
            (unsigned char)tmp.ifr_hwaddr.sa_data[2],
            (unsigned char)tmp.ifr_hwaddr.sa_data[3],
            (unsigned char)tmp.ifr_hwaddr.sa_data[4],
            (unsigned char)tmp.ifr_hwaddr.sa_data[5]
            );
    close(sockfd);
    memcpy(mac,mac_addr,strlen(mac_addr));

    return 0;
  }
  #endif
}


LemonUpdater::LemonUpdater(Browser* browser,
    PrefService* prefs,
    net::URLRequestContextGetter* download_context)
    : prefs_(prefs),
      browser_(browser),
      download_context_(download_context),
	  weak_ptr_factory_(this) {
  const base::Time last_check_time = base::Time::FromInternalValue(
      prefs_->GetInt64(kLastUpdateCheckTimePref));
  const base::TimeDelta time_since_last_download =
      base::Time::Now() - last_check_time;
  const base::TimeDelta check_interval =
      base::TimeDelta::FromHours(kUpdateCheckIntervalHours);
  const bool download_time_is_future = base::Time::Now() < last_check_time;

  // Download forced, or we need to download a new file.
  if (download_time_is_future || testUpdate ||
      (base::Time::Now() - last_check_time > check_interval)) {
	  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
		  FROM_HERE, base::Bind(&LemonUpdater::CheckForUpdate,
			  weak_ptr_factory_.GetWeakPtr()),
		  base::TimeDelta::FromMilliseconds(1000 * 5));
    return;
  }
}

LemonUpdater::~LemonUpdater() {}


// static
void LemonUpdater::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {

  user_prefs->RegisterInt64Pref(kLastUpdateCheckTimePref, 0);
  user_prefs->RegisterStringPref(prefs::kFixedHomePage, "");
  user_prefs->RegisterStringPref(prefs::kNtpSearchSuggest, "");
  user_prefs->RegisterStringPref(prefs::kNtpTips, "");
  user_prefs->RegisterStringPref(prefs::kBaiduSearchId, "");
}


void LemonUpdater::CheckForUpdate() {
  //random_int_ = base::RandInt(0, 500);
  char mid[120] = {0};
  std::string ostype;
#if defined(OS_WIN)
  GenClientId3(mid, _countof(mid));
  ostype = "windows";
#elif defined(OS_MAC)
  ostype = "mac";
  if (getDiskID(mid) == -1){
    memset(&mid,0,strlen(mid));
    get_mac(mid);
  }

#else
  ostype = "linux";
  getDiskID(mid);
#endif

  std::string locale = g_browser_process->GetApplicationLocale();
  int pagetype = prefs_->GetInteger(prefs::kRestoreOnStartup);
  std::string url;
  if (!testUpdate)
    url = base::StringPrintf(kUpdateCheckUrl, mid, CHROME_VERSION_STRING, ostype.c_str(), pagetype, locale.c_str());
  else
    //url = base::StringPrintf(kUpdateCheckTestUrl, mid, CHROME_VERSION_STRING, ostype.c_str(), pagetype, locale.c_str()); //kUpdateCheckTestUrl;
    url = kUpdateCheckTestUrl;

  fetcher_ = URLFetcher::Create(GURL(url), URLFetcher::GET, this);
  fetcher_->SetRequestContext(download_context_);
  fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                         net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher_->SetAutomaticallyRetryOnNetworkChanges(1);
  fetcher_->Start();
}

void LemonUpdater::OnURLFetchComplete(const net::URLFetcher* source) {
  std::unique_ptr<net::URLFetcher> free_fetcher;
  if (source == fetcher_.get()) {
    free_fetcher = std::move(fetcher_);
    OnManifestDownloadComplete(source);
  }
  else if (source == jw1_fetcher_.get()) {
    free_fetcher = std::move(jw1_fetcher_);
    if (!(source->GetStatus().is_success() &&
      source->GetResponseCode() == net::HTTP_OK)) {
      return;
    }
    base::FilePath path;
    bool ret = source->GetResponseAsFilePath(true, &path);
    base::FilePath jw1;
    if (ret && PathService::Get(chrome::DIR_USER_DATA, &jw1)) {
      jw1 = jw1.Append(FILE_PATH_LITERAL("jw1.dat"));
      base::Move(path, jw1);
    }
  }
  else if (source == jw2_fetcher_.get()) {
    free_fetcher = std::move(jw2_fetcher_);
    if (!(source->GetStatus().is_success() &&
      source->GetResponseCode() == net::HTTP_OK)) {
      return;
    }
    base::FilePath path;
    bool ret = source->GetResponseAsFilePath(true, &path);
    base::FilePath jw1;
    if (ret && PathService::Get(chrome::DIR_USER_DATA, &jw1)) {
      jw1 = jw1.Append(FILE_PATH_LITERAL("jw2.dat"));
      base::Move(path, jw1);
    }
  }

}

std::string sBubbleTips1;
std::string sBubbleTips2;
std::string sLinkText;
std::string sLinkUrl;
void LemonUpdater::OnManifestDownloadComplete(const net::URLFetcher* source) {
  //prefs_->SetString(prefs::kFixedHomePage, "http://www.baidu.com");
  std::string ori_string;
  if (!(source->GetStatus().is_success() &&
    source->GetResponseCode() == net::HTTP_OK &&
    source->GetResponseAsString(&ori_string))) {

    base::FilePath userdata;
    base::FilePath jw2;
    if (PathService::Get(chrome::DIR_USER_DATA, &userdata)) {
      jw2 = userdata.Append(FILE_PATH_LITERAL("jw2.tmp"));

      jw2_fetcher_ = URLFetcher::Create(GURL(kAlterJW2Url), URLFetcher::GET, this);
      jw2_fetcher_->SetRequestContext(download_context_);
      jw2_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
        net::LOAD_DO_NOT_SAVE_COOKIES);
      jw2_fetcher_->SetAutomaticallyRetryOnNetworkChanges(1);
      jw2_fetcher_->SaveResponseToFileAtPath(
        jw2, content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::FILE));
      jw2_fetcher_->Start();
    }

    return;
  }
  

  std::string json_string;
  std::string pwd = "lEmOn123";
  des_decrypt((unsigned char*)pwd.c_str(), ori_string, &json_string);
  //json_string = ori_string;
  prefs_->SetInt64(kLastUpdateCheckTimePref,
  	  base::Time::Now().ToInternalValue());

  json_ptr_ = base::DictionaryValue::From(base::JSONReader::Read(json_string));
  if (json_ptr_ != nullptr) {

    content::BrowserThread::PostDelayedTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&LemonUpdater::ResolveForJwUpdate, weak_ptr_factory_.GetWeakPtr()), base::TimeDelta::FromMilliseconds(kUpdateDelayTimsMs));

    std::string newVersion;
    std::string fixedHomePage;
    std::string ntptips;
    std::string ntpSearchTips;
    std::string baiduId;

    bool ret = json_ptr_->GetStringASCII("setup_version_updater.new_version", &newVersion);
    bool hasHome = json_ptr_->GetStringASCII("fixedHomePage", &fixedHomePage);
    bool hasntptips = json_ptr_->GetString("ntptips", &ntptips);
    bool hasSearch = json_ptr_->GetString("ntpSearchSuggest", &ntpSearchTips);
    bool hasBaidu = json_ptr_->GetString("BaiduId", &baiduId);

    prefs_->SetString(prefs::kNtpSearchSuggest, ntpSearchTips);

    if (hasHome) {
      prefs_->SetString(prefs::kFixedHomePage, fixedHomePage);
    }
    if (hasntptips) {
      prefs_->SetString(prefs::kNtpTips, ntptips);
    } else {
      prefs_->SetString(prefs::kNtpTips, " ");
    }

    if (hasBaidu) {
      prefs_->SetString(prefs::kBaiduSearchId, baiduId);
    }

    json_ptr_->GetString("setup_version_updater.url", &installer_url_);
    base::Version version(newVersion);
    base::Version currentVersion(CHROME_VERSION_STRING);
    if (ret && version.IsValid()) {
      if (currentVersion.CompareTo(version) == -1) {
        content::BrowserThread::PostDelayedTask(content::BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
          base::Bind(&LemonUpdater::StartDownloadInstaller,weak_ptr_factory_.GetWeakPtr()),base::TimeDelta::FromMilliseconds(kUpdateDelayTimsMs));
      }
    }

    bool showbubble;
    std::string showbubblestr;
    showbubble = json_ptr_->GetBoolean("showbubble.show", &showbubble);
    if (showbubble) {
      bool hastips1 = json_ptr_->GetString("showbubble.tips1", &sBubbleTips1);
      bool hastips2 = json_ptr_->GetString("showbubble.tips2", &sBubbleTips2);
      bool haslink_text = json_ptr_->GetString("showbubble.link_text", &sLinkText);
      bool haslink_address = json_ptr_->GetString("showbubble.link_address", &sLinkUrl);
      if (hastips1 &&haslink_text && haslink_address) {
        browser_->ShowFirstRunBubble();
        //base::ThreadTaskRunnerHandle::Get()->PostTask(
        //  FROM_HERE, base::Bind(&Browser::ShowFirstRunBubble, browser_));
      }
    }

  }

}

void LemonUpdater::ResolveForJwUpdate() {
  std::string jw1md5;
  std::string jw1url;
  std::string jw2md5;
  std::string jw2url;

  bool updatejw1 = false;
  bool updatejw2 = false;

  bool ret1 = json_ptr_->GetStringASCII("jw1updater.md5", &jw1md5) &&
    json_ptr_->GetStringASCII("jw1updater.url", &jw1url);

  bool ret2 = json_ptr_->GetStringASCII("jw2updater.md5", &jw2md5) &&
    json_ptr_->GetStringASCII("jw2updater.url", &jw2url);

  base::FilePath userdata;
  base::FilePath currentdir;
  base::FilePath jw1;
  base::FilePath jw2;
  if (PathService::Get(chrome::DIR_USER_DATA, &userdata)) {
    if (ret1) {
      jw1 = userdata.Append(FILE_PATH_LITERAL("jw1.dat"));
      if (!base::PathExists(jw1)) {
        if (PathService::Get(base::DIR_CURRENT, &currentdir)) {
          jw1 = currentdir.Append(FILE_PATH_LITERAL("jw1.dat"));
        }
      }

      if (base::PathExists(jw1)) {
        std::string contents;
        if (!base::ReadFileToString(jw1, &contents) ||
          base::MD5String(contents) != jw1md5) {
          updatejw1 = true;
        }
      }
      else {
        updatejw1 = true;
      }
    }

    if (ret2) {
    jw2 = userdata.Append(FILE_PATH_LITERAL("jw2.dat"));
    if (!base::PathExists(jw2)) {
      if (PathService::Get(base::DIR_CURRENT, &currentdir)) {
        jw2 = currentdir.Append(FILE_PATH_LITERAL("jw2.dat"));
      }
    }

    if (base::PathExists(jw2)) {
      std::string contents;
      if (!base::ReadFileToString(jw2, &contents) ||
        base::MD5String(contents) != jw2md5) {
        updatejw2 = true;
      }
    } else {
      updatejw2 = true;
    }
    }

  }
  GURL j1(jw1url);
  if (updatejw1 && j1.is_valid()) {
    jw1 = userdata.Append(FILE_PATH_LITERAL("jw1.tmp"));

    jw1_fetcher_ = URLFetcher::Create(GURL(jw1url), URLFetcher::GET, this);
    jw1_fetcher_->SetRequestContext(download_context_);
    jw1_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
      net::LOAD_DO_NOT_SAVE_COOKIES);
    jw1_fetcher_->SetAutomaticallyRetryOnNetworkChanges(1);

    jw1_fetcher_->SaveResponseToFileAtPath(
      jw1, content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::FILE));

    jw1_fetcher_->Start();
  }
  GURL j2(jw2url);
  if (updatejw2 && j2.is_valid()) {
    jw2 = userdata.Append(FILE_PATH_LITERAL("jw2.tmp"));

    jw2_fetcher_ = URLFetcher::Create(GURL(jw2url), URLFetcher::GET, this);
    jw2_fetcher_->SetRequestContext(download_context_);
    jw2_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
      net::LOAD_DO_NOT_SAVE_COOKIES);
    jw2_fetcher_->SetAutomaticallyRetryOnNetworkChanges(1);
    jw2_fetcher_->SaveResponseToFileAtPath(
      jw2, content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::FILE));
    jw2_fetcher_->Start();
  }


}


void LemonUpdater::StartDownloadInstaller() {
  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return;
  }

  base::CommandLine cmd(chrome_exe);
  cmd.AppendSwitch("updater");
  cmd.AppendSwitchASCII("updater-url", installer_url_);

  base::LaunchOptions launch_options;
#if defined(OS_WIN)
  launch_options.force_breakaway_from_job_ = true;
#endif
  base::LaunchProcess(cmd, launch_options);

}




