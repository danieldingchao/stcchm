#ifndef CHROME_BROWSER_IMPORTER_HTML_IMPORTER_H_
#define CHROME_BROWSER_IMPORTER_HTML_IMPORTER_H_

#include "chrome/utility/importer/importer.h"

namespace base {
class FilePath;
}

class HtmlImporter : public Importer {
 public:
  HtmlImporter();
  virtual ~HtmlImporter() {}

  // Importer methods.
  virtual void StartImport(const importer::SourceProfile& browser_info,
      uint16_t items, ImporterBridge* bridge);

 private:
  void ImportHTML(const base::FilePath& path);

  DISALLOW_COPY_AND_ASSIGN(HtmlImporter);
};

#endif  // CHROME_BROWSER_IMPORTER_HTML_IMPORTER_H_
