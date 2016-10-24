#ifndef CHROME_BROWSER_ADFILTER_ADFILTER_OBSERVER_H
#define CHROME_BROWSER_ADFILTER_ADFILTER_OBSERVER_H
#pragma once

#include "url/gurl.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/browser/navigation_controller.h"
#include "base/timer/timer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "chrome/browser/adfilter/adblock_info.h"
#include "chrome/browser/adfilter/adfilter_service.h"

namespace content {
  class WebContents;
}

class AdfilterTabHelper: public AdfilterService::Observer,
                         public content::NotificationObserver,
                         public content::WebContentsObserver,
                         public content::WebContentsUserData<AdfilterTabHelper>
{
public:
  class Observer
  {
  public:
    virtual void OnGetFilterRulesComplete(const std::vector< AdFilterRuleFileItem >& rules) = 0;
  protected:
    Observer() {}
    virtual ~Observer() {}
  private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };
public:
  AdfilterTabHelper(content::WebContents* wrapper);
  virtual ~AdfilterTabHelper();
  size_t GetTotalBlockCount() { return urlhits_.size(); }
  bool IsAdFilterEnable() {
    return adfilterlevel_ == AdfilterService::ADFilter_Normal ||
        adfilterlevel_ == AdfilterService::ADFilter_Advanced;
  }
  void SetAdblockIndicated(bool indicated) { adblock_indicated_ = indicated; }
  bool AdblockIndicated() { return adblock_indicated_; }
  void SetChromePopBlockNumber(int number) {chrome_pop_block_number_ = number;}
  int GetChromePopBlockNumber() {return chrome_pop_block_number_;}

  void AddObserver(Observer* observer) { observers_.push_back(observer); }
  void RemoveObserver(Observer* observer) {
    std::vector<Observer*>::iterator it = std::remove(observers_.begin(), observers_.end(), observer);
    if( it!=observers_.end() )
      observers_.erase(it);
  }

  void GetFilterRulesAsyn();
  bool IsShowFilterBanner() {return allow_animateshow_banner_;}
  bool IsShowHttpsWarningBanner() const;

  //content::WebContentsObserver
  virtual void DidStartLoading() override;
  virtual void DidStopLoading() override;
  virtual void DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) override;
  virtual void DidStartProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc) override;
  virtual void DidCommitProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& url,
    ui::PageTransition transition_type) override;
  virtual void DidOpenRequestedURL(content::WebContents* new_contents,
      content::RenderFrameHost* source_render_frame_host,
      const GURL& url,
      const content::Referrer& referrer,
      WindowOpenDisposition disposition,
      ui::PageTransition transition) override;

  //AdfilterService::Observer
  virtual void OnURLHited(const URLHitResult & AdUrlResult) override;
  virtual void OnUpdateSelectorsForHostImpl(std::string& host,std::vector<std::string>&css) override;
  virtual void OnInjectCssJs(std::vector<int>& types, std::vector<int>& times, std::vector<std::string>& contents) override;
  virtual void OnGetFilterRulesComplete(const std::vector<AdFilterRuleFileItem>& rules) override;

  //content::NotificationObserver
  virtual void Observe(int type, const content::NotificationSource& source, const content::NotificationDetails& details) override;
  // Callback for preference change
  void OnPreferenceChanged(const std::string& pref_name);
  const std::vector<URLHitResult>& GetBlockList() const { return urlhits_; }

  void StatAdfilterData();
private:
  void ResetData();
  void updatecurrenturl(GURL);
  void UpdateSiteInfo(); 
  void OnDocumentComplete();
  // Current tab contents.
  content::WebContents* wrapper_;

  std::vector<URLHitResult>  urlhits_;
  std::vector<URLHitResult>  cssvectors_;

  PrefChangeRegistrar registrar_;
  AdfilterService::ADFilterLevle   adfilterlevel_;

  GURL current_page_url;
  scoped_refptr<AdfilterService> adfilter_service_;
  int child_id_;
  int routing_id_;
  //è¿éä¸ºäºå¤çæ¹ä¾¿ç¨ä¸ä¸ªéç¨ç»æä½ä¿å­åä¸ªå¼ï¼
  //widthè¡¨ç¤ºchild_id,heightè¡¨ç¤ºrouting_id
  //xè¡¨ç¤ºæ¦æªçé¡µé¢å¹¿åæ°
  //yè¡¨ç¤ºæ¦æªçå¼¹çªçæ°
  AdblockInfo adblock_info_;
  void NotifyUpdateBlock();
  bool IsPageBlocked();//æ¯å¦å¼å¯äºé¡µé¢æ¦æªå¼å³
  content::NotificationRegistrar notify_registrar_;//æ¥æ¶åè°æ¶æ¯çå¯¹è±¡
  std::vector<Observer*> observers_;

  bool adblock_indicated_;
  int chrome_pop_block_number_;

  bool allow_animateshow_banner_;
  bool show_warning_bar_;

  DISALLOW_COPY_AND_ASSIGN(AdfilterTabHelper);
};

#endif // CHROME_BROWSER_ADFILTER_ADFILTER_OBSERVER_H
