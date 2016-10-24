#pragma once 

#include "abp/abp_export.h"
#include "abp/abp.h"
#include "base/logging.h"
#include "base/files/file_path.h"
#include <map>

namespace abp{
    //////////////////////////////////////////////////////////////////////////

    typedef ABP_EXPORT struct InjectRule{
        std::string content;
        int type;
        int time;
    } InjectRule_;

    class ABP_EXPORT CssJsInjecorManager {
		public:
            CssJsInjecorManager();
         void InitFromDir(const base::FilePath& file_path);
         void reLoadFromDir(const base::FilePath& file_path);
         bool GetInjectRulesForUrl(const std::string& url, std::vector<abp::InjectRule>& rules);
         std::string getDataVersion();
    private:
         void Process(const std::vector<base::StringPiece>& lines);
         void ProcessLine(const base::StringPiece& line);
         void ProcessHeaderLine(const base::StringPiece& text);
         void ProcessHeaderSection(const std::vector<base::StringPiece>& lines, int &cur_line);
         void ProcessFilterSection(const std::vector<base::StringPiece>& lines, int& cur_line);
         int mEnableStatus;
         std::string mVersion;


         std::map<std::string, std::vector<abp::InjectRule>> mHostRuleMaps;
         std::map<std::string, std::vector<abp::InjectRule>> mDomainRuleMaps;
         std::vector<abp::InjectRule> mMatchAllRules;
         bool encrypted;
         std::map<std::string, std::string> mInjectFileContent;
         base::FilePath mPath;
    };
}