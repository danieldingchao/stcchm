#include "chrome/browser/adfilter/adfilter_tab_helper.h"

#include "components/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/pref_names.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "ui/base/page_transition_types.h"
#include "content/common/view_messages.h"
#include "content/public/browser/resource_request_details.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/notification_service.h"
#include "chrome/browser/adfilter/adblock_info.h"
#include "chrome/browser/profiles/profile.h"
#include "content/common/frame_messages.h"
#include "base/base64.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "content/public/browser/render_frame_host.h"

using content::NavigationController;
using content::NavigationEntry;
using content::BrowserThread;
using content::WebContents;
using ui::PageTransition;
using content::RenderViewHost;

namespace {
  static const int kMaxBarTipCount = 3;//
}
DEFINE_WEB_CONTENTS_USER_DATA_KEY(AdfilterTabHelper);

AdfilterTabHelper::AdfilterTabHelper(content::WebContents* wrapper)
  :content::WebContentsObserver(wrapper),
    wrapper_(wrapper),
    child_id_(-1),
    routing_id_(-1),
    adblock_indicated_(false),
    chrome_pop_block_number_(0),
    show_warning_bar_(false),
    adfilter_service_(g_browser_process->adfilter_service()){
    if( adfilter_service_ &&web_contents() ){
      adfilter_service_->AddObserver(web_contents(), this);
    }
    Profile* profile = Profile::FromBrowserContext(
        web_contents()->GetBrowserContext());
    adfilterlevel_ = static_cast<AdfilterService::ADFilterLevle>(
        profile->GetPrefs()->GetInteger(prefs::kAdfilterLevel));
    registrar_.Init(profile->GetPrefs());
    PrefChangeRegistrar::NamedChangeCallback callback =
        base::Bind(&AdfilterTabHelper::OnPreferenceChanged,
            base::Unretained(this));
    registrar_.Add(prefs::kAdfilterLevel, callback);
    //add by conyzhou begin:register notify message.
    notify_registrar_.Add(this,
        chrome::NOTIFICATION_ADBLOCK_REFRESH_BLOCKS,
        content::NotificationService::AllSources());

    content::Source<InfoBarService> source(InfoBarService::FromWebContents(wrapper));
    notify_registrar_.Add(this,
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
      source);
}

AdfilterTabHelper::~AdfilterTabHelper() {
  notify_registrar_.RemoveAll();
  if( adfilter_service_ ) {
    StatAdfilterData();
   adfilter_service_->RemoveObserver(this);
  }
}

void AdfilterTabHelper::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
  case chrome::NOTIFICATION_ADBLOCK_REFRESH_BLOCKS: {
            DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
            AdblockInfo web_contents_id =
              *content::Details<AdblockInfo>(details).ptr();
            if (web_contents_id.child_id == child_id_ &&
              web_contents_id.routing_id == routing_id_) {
              NotifyUpdateBlock();
            }
            break;
  }
    case chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED:{
        break;
    }
    default:
      NOTREACHED();
  }
}

void AdfilterTabHelper::OnPreferenceChanged(const std::string& pref_name) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if( pref_name == prefs::kAdfilterLevel ) {
    adfilterlevel_ =
        static_cast<AdfilterService::ADFilterLevle>(
            profile->GetPrefs()->GetInteger(prefs::kAdfilterLevel));
  }
}

void AdfilterTabHelper::OnURLHited(const URLHitResult & AdUrlResult){
  urlhits_.push_back(AdUrlResult);
  NotifyUpdateBlock();
}

void AdfilterTabHelper::OnDocumentComplete(){

}
void AdfilterTabHelper::DidStartLoading() {
}

void AdfilterTabHelper::DidStopLoading() {
  OnDocumentComplete();
}

void AdfilterTabHelper::ResetData(){
  StatAdfilterData();
  urlhits_.clear();
  adblock_indicated_ = false;
  chrome_pop_block_number_ = 0;

}

void AdfilterTabHelper::UpdateSiteInfo() {
  current_page_url =  web_contents()->GetURL();
  child_id_ = web_contents()->GetRenderProcessHost()->GetID();
  routing_id_ = web_contents()->GetRenderViewHost()->GetRoutingID();
}

void AdfilterTabHelper::DidStartProvisionalLoadForFrame(
	content::RenderFrameHost* render_frame_host,
	const GURL& validated_url,
	bool is_error_page,
	bool is_iframe_srcdoc) {
	if (!render_frame_host->GetParent())
    {
      ResetData();
    }
}

void AdfilterTabHelper::DidCommitProvisionalLoadForFrame(
	content::RenderFrameHost* render_frame_host,
	const GURL& url,
	ui::PageTransition transition_type){
  if( !render_frame_host->GetParent() )
    updatecurrenturl(web_contents()->GetURL());
}

void AdfilterTabHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  UpdateSiteInfo();
  if( (adfilterlevel_!=AdfilterService::ADFilter_Uninitialize &&
      adfilterlevel_!=AdfilterService::ADFilter_Disable)  ) {
    //add by conyzhou begin:save current url is in exclude list,save once for
    // efficiency.
    adblock_info_.adblocks_on = true;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&AdfilterService::OnCheckInjectCssJs,
        adfilter_service_.get(), current_page_url, child_id_, routing_id_));
    if( true ) {
      BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind( &AdfilterService::OnUpdateSelectorsForHost,
        adfilter_service_.get(),current_page_url,child_id_,routing_id_));
    }
  }
}

void AdfilterTabHelper::updatecurrenturl(GURL url) {
  UpdateSiteInfo();
}

void AdfilterTabHelper::OnUpdateSelectorsForHostImpl(
    std::string& host, std::vector<std::string>&css){
  if( web_contents() ){
    cssvectors_.clear();
    URLHitResult result;
    result.url = GURL(host);
    for (std::vector<std::string>::iterator iter = css.begin(); iter != css.end(); iter++){
      result.filterRule = *iter;
      result.filterTitle = "css";
      cssvectors_.push_back(result);
    }
    RenderViewHost* pHost = web_contents()->GetRenderViewHost();
    if( pHost ){
      pHost->GetMainFrame()->Send(
          new FrameMsg_CSSSelectorsbyAdfilterService(pHost->GetMainFrame()->GetRoutingID(), host, css));
    }
  }
}

void AdfilterTabHelper::OnInjectCssJs(
    std::vector<int>& types, std::vector<int>& times, std::vector<std::string>& contents){
    if (web_contents()){
        RenderViewHost* pHost = web_contents()->GetRenderViewHost();
        if (pHost){
            pHost->GetMainFrame()->Send(
                new FrameMsg_InjectCssJs(pHost->GetMainFrame()->GetRoutingID(), types, times, contents));
        }
    }
}

void AdfilterTabHelper::GetFilterRulesAsyn() {
  if( adfilter_service_.get() ) {
    adfilter_service_->GetFilterRules(web_contents());
  }
}

void AdfilterTabHelper::OnGetFilterRulesComplete(
    const std::vector<AdFilterRuleFileItem >& rules) {
  for( std::vector<Observer*>::iterator it = observers_.begin();
      it!=observers_.end();
      ++it ) {
    (*it)->OnGetFilterRulesComplete(rules);
  }
}

//add by conyzhou begin:send notify message
void AdfilterTabHelper::NotifyUpdateBlock() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  adblock_info_.child_id = child_id_;
  adblock_info_.routing_id = routing_id_;
  adblock_info_.page_blocks = (IsPageBlocked())?urlhits_.size():0;
  adblock_info_.popup_blocks = chrome_pop_block_number_;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_ADBLOCK_UPDATE_BLOCKS,
      content::Source<AdfilterTabHelper>(this),
      content::Details<AdblockInfo>(&adblock_info_));
}

bool AdfilterTabHelper::IsPageBlocked() {
  return (adfilterlevel_ <= AdfilterService::ADFilter_Disable) ? false:true;
}

void AdfilterTabHelper::DidOpenRequestedURL(content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition) {
    if (disposition != WindowOpenDisposition::SINGLETON_TAB &&
      disposition != WindowOpenDisposition::NEW_FOREGROUND_TAB &&
      disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB &&
      disposition != WindowOpenDisposition::NEW_POPUP &&
      disposition != WindowOpenDisposition::NEW_WINDOW &&
      disposition != WindowOpenDisposition::OFF_THE_RECORD)
      return;
}
void AdfilterTabHelper::StatAdfilterData(){
  if(!adfilter_service_)
      return;
  int pop = 0;
  int request = urlhits_.size();
  int css = cssvectors_.size();
  GURL url = current_page_url;
  if(web_contents()){
    PopupBlockerTabHelper* blocked_helper =
      PopupBlockerTabHelper::FromWebContents(web_contents());
    if(blocked_helper)
      pop = blocked_helper->GetBlockedPopupsCount();
  }

  if(!current_page_url.is_empty() &&
      (pop>0 || request>0 || css >0) &&
      adfilter_service_){
   std::string url_data = url.spec();
   if(!url_data.empty()){
     base::Base64Encode(url.spec(), &url_data);
   }
  }
}
