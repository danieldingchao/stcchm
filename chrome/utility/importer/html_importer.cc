#include "chrome/utility/importer/html_importer.h"

#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/imported_favicon_usage.h"
#include "chrome/utility/importer/importer_util.h"

HtmlImporter::HtmlImporter() {
}

void HtmlImporter::StartImport(const importer::SourceProfile& profile_info,
    uint16_t items,
    ImporterBridge* bridge) {
  bridge_ = bridge;

  bridge_->NotifyStarted();

  if ((items & importer::FAVORITES) && !cancelled()) {
    ImportHTML(profile_info.source_path);
  }
}

void HtmlImporter::ImportHTML(const base::FilePath& path) {
  bridge_->NotifyItemStarted(importer::FAVORITES);

  std::set<GURL> default_urls;
  std::vector<ImportedBookmarkEntry> bookmarks, toolbar_bookmarks;
  std::vector<TemplateURL*> template_urls;
  favicon_base::FaviconUsageDataList favicons;
  base::Time start_time = base::Time::Now();
  importer::ImportBookmarksFromFile(path, default_urls, this, &bookmarks, 
      &template_urls, &favicons);

  if ((!bookmarks.empty())&& !cancelled() ) {
    bridge_->AddBookmarks(bookmarks, L"");
  }

  bridge_->NotifyItemEnded(importer::FAVORITES);
  bridge_->NotifyEnded();
}