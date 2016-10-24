// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Safe Browsing service is responsible for downloading anti-phishing and
// anti-malware tables and checking urls against them.

#ifndef CHROME_BROWSER_ADFILTER_ADFILTER_SERVICE_H_
#define CHROME_BROWSER_ADFILTER_ADFILTER_SERVICE_H_
#pragma once

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "components/prefs/pref_change_registrar.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/common/resource_messages.h"

#include "url/gurl.h"
#include "net/url_request/url_request.h"
#include "abp/abp.h"

class Profile;
class AdfilterServiceFactory;
class PrefService;

namespace base {
class Thread;
}

namespace abp{
  class FilterMgr;
  class Filter;
  class CssJsInjecorManager;
}

namespace content{
  class WebContents;
  class ResourceMessageFilter;
  class ResourceRequestInfo;
}
namespace user_prefs{
  class PrefRegistrySyncable;
}
struct URLHitResult {
  GURL url;
  content::ResourceType resource_type;
  std::string filterTitle;
  std::string filterRule;
};

struct PopBlockResult {
  GURL url;
  PopBlockResult();
};

struct AdFilterRuleFileItem {
  std::string title_;
  std::string author_;
  std::string last_updatetime_;
  std::string file_version_;
  std::string original_url_;
  int id_;
  bool is_enable_;
};

// Construction needs to happen on the main thread.
class AdfilterService
    : public base::RefCountedThreadSafe<AdfilterService, content::BrowserThread::DeleteOnUIThread>,
      public content::NotificationObserver
{
public:
  AdfilterService();
  virtual ~AdfilterService();

  // Create an instance of the safe browsing service.
  static AdfilterService* CreateAdfilterService();
  class Observer
  {
  public:
    virtual void OnURLHited(const URLHitResult & AdUrlResult) = 0;
    virtual void OnUpdateSelectorsForHostImpl(std::string&host,std::vector<std::string>&css) = 0;
    virtual void OnInjectCssJs(std::vector<int>& types, std::vector<int>& times, std::vector<std::string>& contents) = 0;

    virtual void OnGetFilterRulesComplete(const std::vector< AdFilterRuleFileItem >& rules) = 0;
  protected:
    Observer() {}
    virtual ~Observer() {}
  private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  struct siteblocksettings
  {
     bool bDisableAll;
     bool bDisableUrlBlock;
     bool bDisableEleHide;
     GURL url;
     siteblocksettings() {
       bDisableAll = false;
       bDisableUrlBlock = false;
       bDisableEleHide = false;
     }
  };

  enum ADFilterLevle{
     ADFilter_Uninitialize = -1,
     ADFilter_Disable = 0,
     ADFilter_Normal = 1,
     ADFilter_Advanced = 2
  };

  struct OpenerInfo{
    GURL opener_url;
    int64_t render_process_host_id;
    int64_t render_view_id;
  };
  // NotificationObserver overrides
  virtual void Observe(int type, const content::NotificationSource& source, const content::NotificationDetails& details) override;

  void OnPreferenceChanged(PrefService* pref_service, const std::string& pref);

  void Initialize();
  bool CheckBrowseUrl(net::URLRequest* request, bool* bcancel);
  // Add and remove observers.  These methods must be invoked on the UI thread.
  void AddObserver(content::WebContents*,Observer* observer);
  void RemoveObserver(Observer* remove);
  void OnUpdateSelectorsForHost(GURL,int child_id,int route_id);
  void OnCheckInjectCssJs(GURL url, int child_id, int route_id);
  void ShutDown(bool process_exit);

  void GetFilterRules(content::WebContents*);
  void AddUserAdfilterRulse(const base::FilePath& file_path,
                            content::WebContents* web);
  void ReplaceUserAdfilterRulse(const base::FilePath& from_file, int id,
                                content::WebContents* web);
  void DeleteUserAdfilterRulse(int id, content::WebContents* web);
  void EnableUserAdfilterRulse(int id, bool is_enable);
  abp::FilterMgr* GetUserAdfilterMgrById(size_t id);
  

  void Reload();
  void Start();
  static void RegisterUserPrefs(user_prefs::PrefRegistrySyncable* registry);
protected:
 
  void RefreshState();
  void OnIOInitialize();
  void LoadAllFilterMgrs();
  void OnIOShutdown(bool process_exit);
  void ProcessCancelRequest(net::URLRequest* request);
  void NotifyURLHited(const URLHitResult& result,int child_id,int route_id,bool bWhitle);
  void OnURLHitedResultImpl(URLHitResult result, int child_id, int route_id, bool bWhite = false, bool bFake =false);
  void OnUpdateSelectorsForHostImpl(int child_id,int route_id,std::string host,std::vector<std::string> css);
  void OnInjectCssJs(int child_id, int route_id, std::vector<int> types, std::vector<int> times, std::vector<std::string> contents);
  abp::Filter* FilterMgrMatchAny(const std::string& req_url, abp::ContentType content_type, 
    const std::string& doc_host, bool third_party, abp::FilterMgr** ppMgr);
  abp::Filter* FilterAllMgrsMatchAny(const std::string& req_url, abp::ContentType content_type, 
                                    const std::string& doc_host, bool third_party,abp::FilterMgr** ppMgr);
  abp::ContentType ConventResouceTypeToAbp(content::ResourceType type);
  Observer* GetObserveItem(int child_id,int route_id);
  bool IsEnable();
  static int64_t GetRenderViewKey(int64_t render_process_host_id,
    int64_t render_view_id) {
      return (render_process_host_id << 32) + render_view_id;
  }

  // Starts following the safe browsing preference on |pref_service|.
  void AddPrefService(PrefService* pref_service);

  // Stop following the safe browsing preference on |pref_service|.
  void RemovePrefService(PrefService* pref_service);

  void GetFilterRulesOnIOThread(int child_id, int route_id);
  void OnGetFilterRulesComplete(int child_id, int route_id, const std::vector< AdFilterRuleFileItem >& rules);

  //user adfilter rulse
  void InitUserAdfilterRulses();
  void UpdateUserAdfilterRulses();
  bool CheckUserAdfilterRulse(abp::FilterMgr* user_adfilter_rulses);
  void DeleteUserAdfilterRulseDone(
    int id, content::WebContents* web);
  void ReplaceUserAdfilterRulseDone(
    const base::FilePath& from_file, int id,
    content::WebContents* web);
  void UserAdfilterRulseChanged(content::WebContents* web);
  void EnableUserAdfilterRulseDone(int id, bool is_enable);
  void AddUserAdfilterRulseDone(const base::FilePath& file_path,
    content::WebContents* web);
  void SaveUserAdfilterRulseOnUI(base::FilePath::StringType file_name, bool is_enable);


  void LoadRules(ADFilterLevle adfilterlevel);
private:
  static AdfilterServiceFactory* factory_;
  std::auto_ptr<abp::FilterMgr> normal_filterMgr_, advanced_filterMgr_,test_filterMgr_;
  std::vector<abp::FilterMgr*> filterMgrs_;
  std::vector<abp::FilterMgr*> allFilterMgrs_;
  abp::FilterMgr* guidMgr_;
  scoped_refptr<content::ResourceMessageFilter> filter_;
  std::map<content::WebContents*,Observer*>  WebContents2observer_map_;
  std::map<int64_t,siteblocksettings> viewkey2adsetings_map_;
  
  // Tracks existing PrefServices, and the safe browsing preference on each.
  // This is used to determine if any profile is currently using the safe
  // browsing service, and to start it up or shut it down accordingly.
  std::map<PrefService*, PrefChangeRegistrar*> prefs_map_;

   // Used to track creation and destruction of profiles on the UI thread.
  content::NotificationRegistrar prefs_registrar_;

  ADFilterLevle   adfilterlevel_;
  bool cssjs_enabled_;
  base::FilePath adfilter_rules_dir_;

  //user adfilter rulses
  std::vector<abp::FilterMgr*> user_filterMgrs_;

  std::vector<Profile*> profiles_;

  std::auto_ptr<base::ListValue> user_adfilter_disable_list_;

  bool rulesinited_;

  std::map<int64_t, OpenerInfo> tab_loading_map_;
  std::auto_ptr<abp::CssJsInjecorManager> css_injector_manager_;
  DISALLOW_COPY_AND_ASSIGN(AdfilterService);
};
// Factory for creating SafeBrowsingService.  Useful for tests.
class AdfilterServiceFactory {
 public:
  AdfilterServiceFactory() { }
  virtual ~AdfilterServiceFactory() { }
  virtual AdfilterService* CreateAdfilterService() = 0;
 private:
  DISALLOW_COPY_AND_ASSIGN(AdfilterServiceFactory);
};

#endif// CHROME_BROWSER_ADFILTER_ADFILTER_SERVICE_H_
