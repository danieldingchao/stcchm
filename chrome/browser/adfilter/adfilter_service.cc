// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/adfilter/adfilter_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/leak_tracker.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "components/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_service.h"
//#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/child_process_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/content_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/net_errors.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "abp/abp.h"
#include "abp/css_js_injector_manager.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"

#if defined(OS_WIN)
#include "chrome/installer/util/browser_distribution.h"
#endif

using content::BrowserThread;
using content::NavigationEntry;
using content::WebContents;

namespace {

  const char kAdfilterRulesDir[] = "adblock";
  const char kAdfilterDefaultRules[] = "normal.dat";
  const char kAdfilterAdvancedRules[] = "advance.dat";
  // for Test
  const char kSdcardRules[] = "//sdcard//chaozhuo//abprule.txt";
  const char* kAdFreeHosts[] = {
    "phoenixstudio.org",
    NULL,
  };
  bool IsAdFreeHost(const GURL& url) {
    for (int i = 0 ; kAdFreeHosts[i]; i++) {
      if (url.host().compare(kAdFreeHosts[i]) == 0) {
        return true;
      }
    }
    return false;
  }
}  // namespace
// static

AdfilterServiceFactory* AdfilterService::factory_ = NULL;
// The default SafeBrowsingServiceFactory.  Global, made a singleton so we
// don't leak it.
class AdfilterServiceFactoryImpl : public AdfilterServiceFactory {
 public:
  virtual AdfilterService* CreateAdfilterService() {
    return new AdfilterService();
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<AdfilterServiceFactoryImpl>;

  AdfilterServiceFactoryImpl() { }

  DISALLOW_COPY_AND_ASSIGN(AdfilterServiceFactoryImpl);
};

static base::LazyInstance<AdfilterServiceFactoryImpl>
    g_adfilter_service_factory_impl = LAZY_INSTANCE_INITIALIZER;

/* static */
AdfilterService* AdfilterService::CreateAdfilterService() {
  if (!factory_)
    factory_ = g_adfilter_service_factory_impl.Pointer();
  return factory_->CreateAdfilterService();
}

AdfilterService::AdfilterService() : rulesinited_(false), guidMgr_(NULL) ,cssjs_enabled_(false){
}

AdfilterService::~AdfilterService() {
  std::vector<abp::FilterMgr*>::iterator it;
  for (it = user_filterMgrs_.begin(); it != user_filterMgrs_.end(); ++it)
    delete *it;
}

// static
void AdfilterService::RegisterUserPrefs(user_prefs::PrefRegistrySyncable* registry){
  registry->RegisterIntegerPref(prefs::kAdfilterLevel, 2);
  registry->RegisterIntegerPref(prefs::kAdfilterCssJs, 1);
  registry->RegisterIntegerPref(prefs::kAdfilterPopup, 0);
  registry->RegisterFilePathPref(prefs::kUserAdfilterRulseLastDir, base::FilePath());
  registry->RegisterListPref(prefs::KUserAdfilterRulseDisableList);


}
void AdfilterService::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::FilePath dir_user_data;
  if (!PathService::Get(chrome::DIR_USER_DATA, &dir_user_data))
    return;
  adfilter_rules_dir_ = dir_user_data.AppendASCII(kAdfilterRulesDir);

  prefs_registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CREATED,
                       content::NotificationService::AllSources());
  prefs_registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                       content::NotificationService::AllSources());


  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
    base::Bind(&AdfilterService::OnIOInitialize, this));
}

void AdfilterService::AddPrefService(PrefService* pref_service) {
  DCHECK(prefs_map_.find(pref_service) == prefs_map_.end());
  PrefChangeRegistrar* registrar = new PrefChangeRegistrar();
  registrar->Init(pref_service);
  registrar->Add(prefs::kAdfilterLevel, base::Bind(&AdfilterService::OnPreferenceChanged, this, pref_service));
  registrar->Add(prefs::kAdfilterCssJs, base::Bind(&AdfilterService::OnPreferenceChanged, this, pref_service));
  prefs_map_[pref_service] = registrar;

  if (pref_service->GetList(prefs::KUserAdfilterRulseDisableList)) {
    user_adfilter_disable_list_.reset(
      pref_service->GetList(prefs::KUserAdfilterRulseDisableList)->DeepCopy());
  } else {
    user_adfilter_disable_list_.reset();
  }
  RefreshState();
}

void AdfilterService::RemovePrefService(PrefService* pref_service) {
  if (prefs_map_.find(pref_service) != prefs_map_.end()) {
    prefs_map_[pref_service]->RemoveAll();
    delete prefs_map_[pref_service];
    prefs_map_.erase(pref_service);
    RefreshState();
  } else {
    NOTREACHED();
  }
}

// NotificationObserver overrides
void AdfilterService::Observe(int type, const content::NotificationSource& source,
                     const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_CREATED: {
      DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
      Profile* profile = content::Source<Profile>(source).ptr();
      if (!profile->IsOffTheRecord()) {
        AddPrefService(profile->GetPrefs());
        profiles_.push_back(profile);
      }
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
      Profile* profile = content::Source<Profile>(source).ptr();
      if (!profile->IsOffTheRecord()) {
        RemovePrefService(profile->GetPrefs());
        std::vector<Profile*>::iterator it = profiles_.begin();
        for (; it != profiles_.end(); ++it) {
          if (*it == profile)
            profiles_.erase(it);
        }
      }
      break;
    }
   default:
      NOTREACHED();
  }
}

void AdfilterService::OnPreferenceChanged(PrefService* pref_service, const std::string& pref) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if( pref == prefs::kAdfilterLevel ||pref == prefs::kAdfilterCssJs) {

    RefreshState();
    if ( IsEnable() )
      Start();
    else
      ShutDown(false);
  }
}
void AdfilterService::LoadRules(ADFilterLevle adfilterlevel){
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  filterMgrs_.clear();
  allFilterMgrs_.clear();
  guidMgr_ = NULL;
  normal_filterMgr_.reset();
  advanced_filterMgr_.reset();

  if (!IsEnable())
    return;

  rulesinited_ = true;
  base::ThreadRestrictions::ScopedAllowIO allowio;
  //if( !base::PathExists(adfilter_rules_dir_) ) {
  //  base::FilePath adfilter_rules_dir;
  //  if( PathService::Get(chrome::DIR_APP, &adfilter_rules_dir) ) {
  //      adfilter_rules_dir = adfilter_rules_dir.AppendASCII(kAdfilterRulesDir);
  //    base::CopyDirectory(adfilter_rules_dir, adfilter_rules_dir_, false);
  //  }
  //}

  switch( adfilterlevel ) {
    case ADFilter_Uninitialize:
    case ADFilter_Disable:
      break;
    case ADFilter_Normal:
      if( !normal_filterMgr_.get() ) {
        normal_filterMgr_.reset(abp::FilterMgr::NewInstance("normal_filterMgr_"));
        base::FilePath defaultRules = adfilter_rules_dir_.AppendASCII(kAdfilterDefaultRules);
        if( base::PathExists(defaultRules) )
			normal_filterMgr_->InitFromFile(defaultRules);
      }
      filterMgrs_.push_back(normal_filterMgr_.get());
      break;
    case ADFilter_Advanced:


      if( !advanced_filterMgr_.get() ){
        advanced_filterMgr_.reset(abp::FilterMgr::NewInstance("advanced_filterMgr_"));
        base::FilePath AdvancedRules = adfilter_rules_dir_.AppendASCII(kAdfilterAdvancedRules);
        if( base::PathExists(AdvancedRules) )
			advanced_filterMgr_->InitFromFile(AdvancedRules);
      }

      if (!normal_filterMgr_.get()) {
          normal_filterMgr_.reset(abp::FilterMgr::NewInstance("normal_filterMgr_"));
          base::FilePath defaultRules = adfilter_rules_dir_.AppendASCII(kAdfilterDefaultRules);
          if (base::PathExists(defaultRules))
              normal_filterMgr_->InitFromFile(defaultRules);
      }

#if defined(OS_ANDROID)
      if (!test_filterMgr_.get()) {
    	  test_filterMgr_.reset(abp::FilterMgr::NewInstance("test_filterMgr_"));
                base::FilePath defaultRules = base::FilePath(kSdcardRules);
                if (base::PathExists(defaultRules)) {
                	test_filterMgr_->InitFromFileWithoutDescript(defaultRules);
                } else {
                }
      }
#endif


      InitUserAdfilterRulses();
      filterMgrs_.push_back(advanced_filterMgr_.get());
      filterMgrs_.push_back(normal_filterMgr_.get());
#if defined(OS_ANDROID)
      filterMgrs_.push_back(test_filterMgr_.get());
#endif
      break;
    default:
      DCHECK(false);
      break;
  }
}
void AdfilterService::OnIOInitialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::ThreadRestrictions::ScopedAllowIO allowio;
  filterMgrs_.clear();
  allFilterMgrs_.clear();
  guidMgr_ = NULL;

  if (!IsEnable())
    return;

}
void AdfilterService::LoadAllFilterMgrs(){
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  allFilterMgrs_.clear();

  base::ThreadRestrictions::ScopedAllowIO allowio;

  if( !advanced_filterMgr_.get() ){
    advanced_filterMgr_.reset(abp::FilterMgr::NewInstance("advanced_filterMgr_"));
    base::FilePath AdvancedRules = adfilter_rules_dir_.AppendASCII(kAdfilterAdvancedRules);
    if( base::PathExists(AdvancedRules) )
        advanced_filterMgr_->InitFromFileWithoutDescript(AdvancedRules);
  }
  allFilterMgrs_.push_back(advanced_filterMgr_.get());

  if (user_filterMgrs_.empty())
    UpdateUserAdfilterRulses();

  std::vector<abp::FilterMgr*>::reverse_iterator it;
  for (it = user_filterMgrs_.rbegin(); it != user_filterMgrs_.rend(); ++it) {
    allFilterMgrs_.push_back(*it);
  }
}
void AdfilterService::OnIOShutdown(bool process_exit){
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if( process_exit ) {
    guidMgr_ = NULL;
    filterMgrs_.clear();
    allFilterMgrs_.clear();
    tab_loading_map_.clear();
  }
}

void AdfilterService::ShutDown(bool process_exit) {
  prefs_registrar_.RemoveAll();
  BrowserThread::PostTask(
    BrowserThread::IO, FROM_HERE, base::Bind(&AdfilterService::OnIOShutdown, this, process_exit));
}
void AdfilterService::RefreshState() {
   DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
   // Check if any profile requires the service to be active.
  std::map<PrefService*, PrefChangeRegistrar*>::iterator iter;
  int level = -1;
  int cssjs = -1;
  for (iter = prefs_map_.begin(); iter != prefs_map_.end(); ++iter) {
    int value = iter->first->GetInteger(prefs::kAdfilterLevel);
    int cssvalue = iter->first->GetInteger(prefs::kAdfilterCssJs);
    if( value>level )
      level = value;
    if (cssvalue > cssjs)
    	cssjs = cssvalue;
  }
  if (cssjs > 0)
	  cssjs_enabled_ = true;
  else
	  cssjs_enabled_ = false;

  if (level == -1)
    level = 1;
  // Check if any profile requires the service to be active.
  switch( level ) {
    case -1:
      adfilterlevel_ = ADFilter_Uninitialize;
      break;
    case 0:
      adfilterlevel_ = ADFilter_Disable;
      break;
    case 1:
      adfilterlevel_ = ADFilter_Normal;
      break;
    case 2:
      adfilterlevel_ = ADFilter_Advanced;
      break;
    default:
      break;
  }
}


bool AdfilterService::IsEnable() {
   if( adfilterlevel_ == ADFilter_Disable)
     return false;
   return true;
}

void AdfilterService::Start() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
    BrowserThread::IO, FROM_HERE, base::Bind(&AdfilterService::LoadRules, this, adfilterlevel_));
}

bool AdfilterService::CheckBrowseUrl(net::URLRequest* request, bool* block) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK( request && block );
  *block = false;
  if (!IsEnable() )
    return false;

  GURL gurl = request->url();

  if( ! (gurl.SchemeIs("http") || gurl.SchemeIs("https") ) )
    return false;

  if (IsAdFreeHost(request->first_party_for_cookies()))
    return false;
  const content::ResourceRequestInfo* pResInfo = content::ResourceRequestInfo::ForRequest(request);
  if( pResInfo ) {
    int child_id = pResInfo->GetChildID();
    int route_id = pResInfo->GetRouteID();

    if (content::RESOURCE_TYPE_MAIN_FRAME == pResInfo->GetResourceType()){
      return true;
    }
    std::string  url = request->url().spec();
    std::string  host = abp::AbpExtractHostFromUrl(request->referrer());
    abp::ContentType type = ConventResouceTypeToAbp(pResInfo->GetResourceType());
    bool bThirdParty = abp::AbpIsThirdParty(url,request->referrer());

    if( route_id != MSG_ROUTING_NONE ){
      if( host == "" )
        return true;
    }

    if( !rulesinited_ ){
      LoadRules(adfilterlevel_);
    }

    URLHitResult result;
    result.url = request->url();
    result.resource_type = pResInfo->GetResourceType();

    abp::FilterMgr * pMgr = NULL;
    if( adfilterlevel_ != ADFilter_Uninitialize ) {
      abp::Filter * pFliter = FilterMgrMatchAny(url, type, host, bThirdParty, &pMgr);
      if( pFliter ){
        result.filterRule = pFliter->Text().as_string();
        result.filterTitle = pMgr->GetTitle();
        if( pFliter->Type() == abp::FT_BLACKLIST ){
          *block = true;
          request->OnCallToDelegate();
          LOG(WARNING) << "resource blocked by:" + result.filterRule;
            base::MessageLoop::current()->task_runner()->PostTask(FROM_HERE,
              base::Bind(&AdfilterService::ProcessCancelRequest,
              base::Unretained(this),request));
        }
        else if( pFliter->Type() == abp::FT_WHITELIST ){
        }
        if( route_id != MSG_ROUTING_NONE )
          NotifyURLHited(result, child_id, route_id, !(*block));
      }
    }

    if (guidMgr_ ){
      abp::Filter * pFliter = guidMgr_->MatchAny(url, type, host, bThirdParty);
      if (pFliter&& pFliter->Type() == abp::FT_BLACKLIST){
        BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&AdfilterService::OnURLHitedResultImpl,
          this, result, child_id, route_id, true, true));
      }
    }
  }
  return true;
}

abp::ContentType AdfilterService::ConventResouceTypeToAbp(content::ResourceType type){
  switch(type ){
  case content::RESOURCE_TYPE_SUB_FRAME:
    return abp::CT_SUBDOCUMENT;
    break;
  case content::RESOURCE_TYPE_SCRIPT:
    return abp::CT_SCRIPT;
    break;
  case content::RESOURCE_TYPE_IMAGE:
    return abp::CT_IMAGE;
    break;
  case content::RESOURCE_TYPE_MAIN_FRAME:
    return abp::CT_DOCUMENT;
    break;
  case content::RESOURCE_TYPE_STYLESHEET:
    return abp::CT_STYLESHEET;
    break;
    // case content::RESOURCE_TYPE_FONT_RESOURCE:
    //  return abp::CT_FONT;
    //  break;
    //  case content::RESOURCE_TYPE_SUB_RESOURCE:
    //   return abp::CT_OTHER;
    //   break;
  case content::RESOURCE_TYPE_XHR:
    return abp::CT_XMLHTTPREQUEST;
    break;
  case content::RESOURCE_TYPE_OBJECT:
    return abp::CT_OBJECT;
    break;
  default:
    break;
  }
  return  abp::CT_IMAGE;
}


abp::Filter* AdfilterService::FilterMgrMatchAny(const std::string& req_url,
                                                abp::ContentType content_type,
                                     const std::string& doc_host,
                                     bool third_party,abp::FilterMgr** ppMgr){
  abp::Filter * pFliter = NULL;
  std::vector<abp::FilterMgr*>::iterator itor;
  for( itor = filterMgrs_.begin(); itor != filterMgrs_.end(); ++itor) {
    if (!(*itor)->IsEnable()) continue;
    abp::Filter * pt = (*itor)->MatchAny(req_url,content_type,doc_host,third_party);
    if( pt ) {
      if( pt->Type() == abp::FT_WHITELIST ){
        *ppMgr = *itor;
        return pt;
      }
      else {
        if( !pFliter ){
          *ppMgr = *itor;
          pFliter = pt;
        }
      }
    }
  }
  return pFliter;
}

abp::Filter* AdfilterService::FilterAllMgrsMatchAny(const std::string& req_url,
                                                abp::ContentType content_type,
                                     const std::string& doc_host,
                                     bool third_party,abp::FilterMgr** ppMgr){
  abp::Filter * pFliter = NULL;
  std::vector<abp::FilterMgr*>::iterator itor;
  for( itor = allFilterMgrs_.begin(); itor != allFilterMgrs_.end(); ++itor) {
    if (!(*itor)->IsEnable()) continue;
    abp::Filter * pt = (*itor)->MatchAny(req_url,content_type,doc_host,third_party);
    if( pt ) {
      if( pt->Type() == abp::FT_WHITELIST ){
        *ppMgr = *itor;
        return pt;
      }
      else {
        if( !pFliter ){
          *ppMgr = *itor;
          pFliter = pt;
        }
      }
    }
  }
  return pFliter;
}
void AdfilterService::OnUpdateSelectorsForHost(GURL url,int child_id,int route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if( !rulesinited_ ){
        LoadRules(adfilterlevel_);
  }

  std::vector<std::string> css;
  std::string  host = abp::AbpExtractHostFromUrl(url.spec());
  std::vector<abp::FilterMgr*>::iterator itor;
  for( itor = filterMgrs_.begin();itor != filterMgrs_.end();itor++){
    if (!(*itor)->IsEnable()) continue;
    std::vector<std::string> tcss;
    (*itor)->GetSelectorsForHost(host,abp::DMT_INCLUDE_EXCLUDE,tcss);
    std::vector<std::string>::iterator itor2;
    for( itor2 = tcss.begin();itor2!= tcss.end();itor2++ ){
      css.push_back(*itor2);
    }
  }
  if( css.size() > 0 ){
    BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&AdfilterService::OnUpdateSelectorsForHostImpl,
      this,child_id,route_id,host,css));
  }
}

void AdfilterService::OnCheckInjectCssJs(GURL url, int child_id, int route_id) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (!cssjs_enabled_)
    	return;


    if (!css_injector_manager_.get()) {
		base::ThreadRestrictions::ScopedAllowIO allowio;
    	css_injector_manager_.reset(new abp::CssJsInjecorManager());
    	css_injector_manager_->InitFromDir(adfilter_rules_dir_);
    }

    std::string host = abp::AbpExtractHostFromUrl(url.spec());

    std::vector<int> types;
    std::vector<int> times;
    std::vector<std::string> injected_js_;

    std::vector<abp::InjectRule> rules;
    css_injector_manager_->GetInjectRulesForUrl(url.spec(), rules);
    int count = rules.size();
    for (int i = 0; i < count; i++) {
        types.push_back(rules[i].type);
        times.push_back(rules[i].time);
        injected_js_.push_back(rules[i].content);
    }
    if (injected_js_.size() > 0){
    	LOG(WARNING) << "onCheckInjectCssJs size > 0";
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&AdfilterService::OnInjectCssJs,
            this, child_id, route_id, types, times,injected_js_));
    }
}

void AdfilterService::NotifyURLHited(const URLHitResult& result,int child_id,int route_id,bool bWhitle){
   DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
   if( !bWhitle ){
     BrowserThread::PostTask(
       BrowserThread::UI, FROM_HERE,
       base::Bind( &AdfilterService::OnURLHitedResultImpl,
       this,result,child_id,route_id,false,false));
   }
}
void AdfilterService::OnURLHitedResultImpl(URLHitResult result, int child_id, int route_id, bool bWhite, bool bFake){
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Observer* pObser = GetObserveItem(child_id, route_id);
  if (pObser){
    if (bFake){
      result.filterTitle = "guid";
    }
    else if (bWhite){
      //result.filterTitle = "white";
      //pObser->OnWhiteURLHited(result);
    }
    else{
//      AdblockCountIncrement(false);
      pObser->OnURLHited(result);
    }
  }
}


void AdfilterService::OnUpdateSelectorsForHostImpl(int child_id,int route_id,std::string host,std::vector<std::string> css){
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Observer* pObser = GetObserveItem(child_id,route_id);
  if( pObser ){
    pObser->OnUpdateSelectorsForHostImpl(host,css);
  }
}

void AdfilterService::OnInjectCssJs(int child_id, int route_id, std::vector<int> types, std::vector<int> times, std::vector<std::string> contents){
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    Observer* pObser = GetObserveItem(child_id, route_id);
    if (pObser){
        pObser->OnInjectCssJs(types,times,contents);
    }
}


void AdfilterService::ProcessCancelRequest(net::URLRequest* request){
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    DCHECK(request);
  if( request->url_chain().size()==0 )
    const_cast<std::vector<GURL>&>(request->url_chain()).push_back(GURL());
  //add by conyzhou begin:fixed dmp f89bf3ae96006519142962318ecb6d6a,
  // invalid delegate call when task run.
  if(request){
    if( net::URLRequestStatus::CANCELED ==
        request->status().status() ){
      request->set_delegate(NULL);
    }
  }
  //add by conyzhou end:
  request->BeforeRequestComplete(net::ERR_BLOCKED_BY_CLIENT);
}

void AdfilterService::AddObserver(content::WebContents* pWeb,Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  WebContents2observer_map_[pWeb] = observer;
}

void AdfilterService::RemoveObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::map<content::WebContents*,Observer*>::iterator iter = WebContents2observer_map_.begin();
  for (; iter != WebContents2observer_map_.end(); ++iter){
    if( iter->second == observer ){
       WebContents2observer_map_.erase(iter);
       return;
    }
  }
  DCHECK(false);
}

AdfilterService::Observer* AdfilterService::GetObserveItem(int child_id,int route_id){
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  content::WebContents* web_contents2 = tab_util::GetWebContentsByID(child_id, route_id);
  if( web_contents2 ){
    std::map<content::WebContents*,Observer*>::iterator iter =  WebContents2observer_map_.find(web_contents2);
    if( iter != WebContents2observer_map_.end() )
      return iter->second;
  }
 return NULL;
}

void AdfilterService::GetFilterRulesOnIOThread(int child_id, int route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if( filterMgrs_.empty() ) {
    LoadRules(adfilterlevel_);
  }
  if(allFilterMgrs_.empty()){
    LoadAllFilterMgrs();
  }

  std::vector<abp::FilterMgr*>::iterator itor;
  std::vector< AdFilterRuleFileItem > rules;
  for( itor = allFilterMgrs_.begin(); itor != allFilterMgrs_.end(); ++itor) {
    AdFilterRuleFileItem item;
    item.title_ = (*itor)->GetTitle();
    item.author_ = (*itor)->GetAuthor();
    item.file_version_ = (*itor)->GetVersion();
    item.last_updatetime_ = (*itor)->GetUpdatedTime();
    item.original_url_ = (*itor)->GetUrl();
    item.id_ = (*itor)->GetId();
    item.is_enable_ = (*itor)->IsEnable();
    rules.push_back(item);
  }
  BrowserThread::PostTask( BrowserThread::UI, FROM_HERE,
    base::Bind( &AdfilterService::OnGetFilterRulesComplete, this, child_id, route_id, rules));
}

void AdfilterService::OnGetFilterRulesComplete(int child_id, int route_id, const std::vector< AdFilterRuleFileItem >& rules) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Observer* pObser = GetObserveItem(child_id, route_id);
  if( pObser ) {
    pObser->OnGetFilterRulesComplete(rules);
  }
}

void AdfilterService::GetFilterRules(content::WebContents* web) {
  // DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // std::vector<Profile*>::iterator it = profiles_.begin();
  // for (; it != profiles_.end(); ++it) {
  //   Browser* last_active =
  //     chrome::FindLastActiveWithProfile(*it, chrome::GetActiveDesktop());
  //   int index = TabStripModel::kNoTab;
  //   if (last_active)
  //     index = last_active->tab_strip_model()->GetIndexOfWebContents(web);
  //   if (index != TabStripModel::kNoTab) {
  // int route_id = web->GetRenderViewHost()->GetRoutingID();
  // int child_id = web->GetRenderProcessHost()->GetID();
  // BrowserThread::PostTask(
  //      BrowserThread::IO, FROM_HERE,
  //      base::Bind( &AdfilterService::GetFilterRulesOnIOThread, this, child_id, route_id));
  //     break;
  //   }
  // }
}

void AdfilterService::InitUserAdfilterRulses() {
  if (user_filterMgrs_.empty())
    UpdateUserAdfilterRulses();

  std::vector<abp::FilterMgr*>::reverse_iterator it;
  for (it = user_filterMgrs_.rbegin(); it != user_filterMgrs_.rend(); ++it) {
    filterMgrs_.push_back(*it);
  }
}

void AdfilterService::UpdateUserAdfilterRulses() {
  /*
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  std::vector<abp::FilterMgr*>::iterator it;
  for (it = user_filterMgrs_.begin(); it != user_filterMgrs_.end(); ++it)
    delete *it;
  user_filterMgrs_.clear();

  std::set<base::FilePath> file_paths;

  base::FileEnumerator all_adfilter_files(adfilter_rules_dir_, false,
    base::FileEnumerator::FILES);
  while (true) {
    base::FilePath adfilter_rules_file = all_adfilter_files.Next();
    if (adfilter_rules_file.empty())
      break;
    file_paths.insert(adfilter_rules_file);
  }
  int id = 0;
  std::set<base::FilePath>::iterator set_it;
  for (set_it = file_paths.begin(); set_it != file_paths.end(); ++set_it) {
    if (set_it->BaseName().value().find(L"user_adfilter_") != std::string::npos
        && set_it->Extension() == L".dat") {
      scoped_ptr<abp::FilterMgr> user_filterMgr(abp::FilterMgr::NewInstance("user_FilterMgr"));
      user_filterMgr->SetId(0);
      if (base::PathExists(*set_it)) {
        user_filterMgr->InitFromFileWithoutDescript(*set_it);
        user_filterMgr->SetFilePath(*set_it);
        if (user_adfilter_disable_list_.get()) {
          std::string file_name = base::WideToASCII(set_it->BaseName().value());
          scoped_ptr<base::Value> string_value(new base::StringValue(file_name));
          if (user_adfilter_disable_list_->Find(*string_value)
              != user_adfilter_disable_list_->end())
            user_filterMgr->Disable();
        }
        if (CheckUserAdfilterRulse(user_filterMgr.get())) {
          user_filterMgr->SetId(++id);
          user_filterMgrs_.push_back(user_filterMgr.release());
        }
      }
    }
  }*/
}

bool AdfilterService::CheckUserAdfilterRulse(
  abp::FilterMgr* user_adfilter_rulses) {
    bool result = !user_adfilter_rulses->GetTitle().empty() &&
      !user_adfilter_rulses->GetAuthor().empty() &&
      !user_adfilter_rulses->GetUpdatedTime().empty() &&
      !user_adfilter_rulses->GetVersion().empty() &&
      user_adfilter_rulses->IsValid();
  return result;
}

void AdfilterService::AddUserAdfilterRulse(const base::FilePath& file_path,
    content::WebContents* web) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
    base::Bind(&AdfilterService::AddUserAdfilterRulseDone,
               this, file_path, web));
}

void AdfilterService::AddUserAdfilterRulseDone(const base::FilePath& file_path,
    content::WebContents* web) {
    /*
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::ThreadRestrictions::ScopedAllowIO allowio;
  if (!base::PathExists(file_path))
    return;
  base::FilePath adfilter_file_path = adfilter_rules_dir_;
  std::string adfilter_file_name = L"user_adfilter_";
  base::Time now = base::Time::Now();
  adfilter_file_name.append(base::Int64ToString16(now.ToInternalValue()));
  adfilter_file_name.append(L".dat");
  adfilter_file_path = adfilter_file_path.Append(adfilter_file_name);
  if (!base::CopyFile(file_path, adfilter_file_path))
    return;
  if(base::PathExists(adfilter_file_path)) {
    scoped_ptr<abp::FilterMgr> user_filterMgr(abp::FilterMgr::NewInstance("user_FilterMgr"));
    user_filterMgr->SetId(0);
    user_filterMgr->InitFromFileWithoutDescript(adfilter_file_path);
    user_filterMgr->SetFilePath(adfilter_file_path);
    if (CheckUserAdfilterRulse(user_filterMgr.get())) {
      user_filterMgrs_.push_back(user_filterMgr.release());
      UserAdfilterRulseChanged(web);
      return;
    } else {
      base::DeleteFile(adfilter_file_path, false);
      content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
        base::Bind(&AdfilterService::ShowWarningInfobar, this, web,
                   AdfilterWarningInfobar::ADD_WARNING));
    }
  }
  return;
  */
}

void AdfilterService::ReplaceUserAdfilterRulse(
    const base::FilePath& from_file, int id, content::WebContents* web) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
    base::Bind(&AdfilterService::ReplaceUserAdfilterRulseDone,
    this, from_file, id, web));
}

void AdfilterService::ReplaceUserAdfilterRulseDone(
    const base::FilePath& from_file, int id,
    content::WebContents* web) {
    /*
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::ThreadRestrictions::ScopedAllowIO allowio;
  abp::FilterMgr* filterMgr = GetUserAdfilterMgrById(id);
  if (!filterMgr)
    return;
  base::FilePath to_file = filterMgr->GetFilePath();
  if (!base::PathExists(from_file) ||
      !base::PathExists(to_file))
    return;
  scoped_ptr<abp::FilterMgr> user_filterMgr(abp::FilterMgr::NewInstance("user_FilterMgr"));
  user_filterMgr->SetId(0);
  user_filterMgr->InitFromFileWithoutDescript(from_file);
  user_filterMgr->SetFilePath(to_file);
  if (CheckUserAdfilterRulse(user_filterMgr.get())) {
    if (base::CopyFile(from_file, to_file)) {
      if (!filterMgr->IsEnable())
        user_filterMgr->Disable();
      user_filterMgrs_[id -1] = user_filterMgr.release();
      delete filterMgr;
      UserAdfilterRulseChanged(web);
      return;
    }
  } else {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&AdfilterService::ShowWarningInfobar, this, web,
                 AdfilterWarningInfobar::REPLACE_WARNING));
    return;
  }
  return;
  */
}

void AdfilterService::DeleteUserAdfilterRulse(
    int id, content::WebContents* web) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
      base::Bind(&AdfilterService::DeleteUserAdfilterRulseDone,
                  this, id, web));
}

void AdfilterService::DeleteUserAdfilterRulseDone(
    int id, content::WebContents* web) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::ThreadRestrictions::ScopedAllowIO allowio;
  abp::FilterMgr* filterMgr = GetUserAdfilterMgrById(id);
  if (!filterMgr)
    return;
  base::FilePath::StringType file_name = filterMgr->GetFilePath().BaseName().value();
  base::DeleteFile(filterMgr->GetFilePath(), false);
  user_filterMgrs_.erase(user_filterMgrs_.begin() + (id-1));
  delete filterMgr;
  BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
    base::Bind(&AdfilterService::SaveUserAdfilterRulseOnUI,
    this, file_name, true));
  UserAdfilterRulseChanged(web);
}

void AdfilterService::UserAdfilterRulseChanged(content::WebContents* web) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  std::vector<abp::FilterMgr*>::iterator it = user_filterMgrs_.begin();
  for (; it != user_filterMgrs_.end(); ++it) {
    (*it)->SetId(it - user_filterMgrs_.begin() + 1);
  }
  filterMgrs_.clear();
  allFilterMgrs_.clear();
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
    base::Bind(&AdfilterService::GetFilterRules, this, web));
}

void AdfilterService::EnableUserAdfilterRulse(int id, bool is_enable) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
        BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
          base::Bind(&AdfilterService::EnableUserAdfilterRulseDone,
          this, id, is_enable));
}

void AdfilterService::EnableUserAdfilterRulseDone(int id, bool is_enable) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  abp::FilterMgr* filterMgr = GetUserAdfilterMgrById(id);
  if (filterMgr) {
    is_enable ? filterMgr->Enable() : filterMgr->Disable();
    base::FilePath::StringType file_name = filterMgr->GetFilePath().BaseName().value();
    BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&AdfilterService::SaveUserAdfilterRulseOnUI,
      this, file_name, is_enable));
  }
  return;
}

void AdfilterService::SaveUserAdfilterRulseOnUI(base::FilePath::StringType file_name, bool is_enable) {
    /*
  if (profiles_.size() > 0) {
    Profile* profile = NULL;
    profile = profiles_.at(0);
    if (profile && profile->GetOriginalProfile()) {
      PrefService* pref_service = profile->GetOriginalProfile()->GetPrefs();
      ListPrefUpdate list_pref_updata(pref_service, prefs::KUserAdfilterRulseDisableList);
      if (!is_enable)
        list_pref_updata->AppendString(base::WideToASCII(file_name));
      else {
        scoped_ptr<base::Value> string_value(new base::StringValue(file_name));
        base::ListValue::const_iterator it = list_pref_updata->Find(*string_value);
        if (it != list_pref_updata->end()) {
          list_pref_updata->Remove(it - list_pref_updata->begin(), NULL);
        }
      }
    }
  }*/
}

abp::FilterMgr* AdfilterService::GetUserAdfilterMgrById(size_t id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  size_t size = user_filterMgrs_.size();
  if (id > 0 && id <= size) {
    return user_filterMgrs_.at(id-1);
  }
  return NULL;
}






void AdfilterService::Reload() {
  Start();
}
