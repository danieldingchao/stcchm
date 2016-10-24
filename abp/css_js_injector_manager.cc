#include "abp/css_js_injector_manager.h"

#include "abp/abp_matcher.h"
#include "abp/abp_parser.h"
#include "abp/abp.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/strings/string_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/debug/alias.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include <map>
#include <vector>
#include <utility>

namespace abp{

    bool IsValidLine(const base::StringPiece& text){
        return text.length() && text[0] != '[';
    }
//////////////////////////////////////////////////////////////////////////
    const char kInjectCssJsMainFile[] = "main.dat";
    const char kInjectCssJsDir[] = "mt";

    CssJsInjecorManager::CssJsInjecorManager():
        mEnableStatus(0),
        encrypted(false){
    }
    void CssJsInjecorManager::InitFromDir(const base::FilePath& file_path) {
        mPath = file_path.AppendASCII(kInjectCssJsDir);
        base::FilePath main_file = mPath.AppendASCII(kInjectCssJsMainFile);
        int64_t file_size = 0;
        if (!base::GetFileSize(main_file, &file_size) || file_size>10 * 1024 * 1024 || file_size < 10)
            return;

        char* file_buf = new char[file_size];
        if (base::ReadFile(main_file, file_buf, file_size) != file_size)
            return;
        base::StringPiece file_buf_;
        file_buf_.set(file_buf, file_size);
        if (file_buf_[0] != '#')
            encrypted = true;
        base::StringPiece file_content_ = abp::Decrypt(file_buf_);

        std::vector<base::StringPiece> lines;
        Tokenize(file_content_, "\r\n", &lines);

        Process(lines);
    }

    void CssJsInjecorManager::reLoadFromDir(const base::FilePath& file_path){
        mHostRuleMaps.clear();
        mHostRuleMaps.clear();
        mDomainRuleMaps.clear();
        mMatchAllRules.clear();
        mEnableStatus = false;
        InitFromDir(file_path);
    }

    std::string CssJsInjecorManager::getDataVersion() {
        return mVersion;
    }

    bool CssJsInjecorManager::GetInjectRulesForUrl(const std::string& url, std::vector<abp::InjectRule>& rules) {
        if (!mEnableStatus)
            return false;
        rules.insert(rules.end(), mMatchAllRules.begin(), mMatchAllRules.end());

        std::string host = abp::AbpExtractHostFromUrl(url);
        std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(GURL(url),
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

        std::map<std::string, std::vector<InjectRule>>::iterator it;
        it = mDomainRuleMaps.find(domain);
        if (it != mDomainRuleMaps.end()) {
            std::vector<InjectRule>* rule = (std::vector<InjectRule>*)&(it->second);
            rules.insert(rules.end(), rule->begin(), rule->end());
        }

        it = mHostRuleMaps.find(host);
        if (it != mHostRuleMaps.end()) {
            std::vector<InjectRule>* rule = (std::vector<InjectRule>*)&(it->second);
            rules.insert(rules.end(), rule->begin(), rule->end());
        }

        return false;
    }

    void CssJsInjecorManager::Process(const std::vector<base::StringPiece>& lines){
        int cur_line = 0;
        int total_line = lines.size();
        while (cur_line < total_line){
            if (lines[cur_line][0] == '['){
                base::StringPiece section = lines[cur_line];
                std::string section_string;
                TrimWhitespaceASCII(section.as_string(), base::TRIM_ALL, &section_string);
                section = section_string;
                if (section == "[ChaoZhuo Rules]"){
                    ProcessHeaderSection(lines, cur_line);
                }
                else if (section == "[Subscriptions]"){
                    ProcessFilterSection(lines, cur_line);
                }
                else{
                    cur_line++;
                }
            }
            else{
                cur_line++;
            }
        }
    }


    void CssJsInjecorManager::ProcessFilterSection(const std::vector<base::StringPiece>& lines,
            int& cur_line){
            int total_line = lines.size();
            cur_line++;
            while (cur_line<total_line && IsValidLine(lines[cur_line])) {
                ProcessLine(lines[cur_line]);
                cur_line++;
            }
        }

    void CssJsInjecorManager::ProcessHeaderSection(const std::vector<base::StringPiece>& lines,
        int &cur_line){
        int total_line = lines.size();
        cur_line++;
        while (cur_line<total_line && IsValidLine(lines[cur_line])) {
            ProcessHeaderLine(lines[cur_line]);
            cur_line++;
        }
    }

    void CssJsInjecorManager::ProcessHeaderLine(const base::StringPiece& text){
        std::vector<base::StringPiece> kv;
        Tokenize(text, "=", &kv);
        if (kv.size() == 2) {
            if (kv[0] == "enabled")    mEnableStatus = atoi(kv[1].as_string().c_str());
            if (kv[0] == "version")      mVersion = kv[1].as_string();
        }
    }

    void CssJsInjecorManager::ProcessLine(const base::StringPiece& buf) {
        std::vector<base::StringPiece> lines;
        Tokenize(buf, " ", &lines);
        if (lines.size() < 4)
            return;

        std::string address = lines[0].as_string();
        std::string file = lines[1].as_string();
        std::string type = lines[2].as_string();
        std::string time = lines[3].as_string();

        std::string content;
        std::map<std::string, std::string>::iterator it;
        it = mInjectFileContent.find(lines[1].as_string());
        if (it != mInjectFileContent.end()) {
            content = (std::string)it->second;
        }
        else {
            std::string file_name = lines[1].as_string();
            base::FilePath rule_file = mPath.AppendASCII(file_name);
            int64_t file_size = 0;
            if (!base::GetFileSize(rule_file, &file_size) || file_size > 10 * 1024 * 1024)
                return;
            char* file_buf = new char[file_size + 1];
            memset(file_buf, 0, file_size + 1);
            if (base::ReadFile(rule_file, file_buf, file_size) != file_size)
                return;
            base::StringPiece file_buf_;
            file_buf_.set(file_buf, file_size);
            if (encrypted) {
                base::StringPiece dep = Decrypt(file_buf_);
                content = dep.as_string();
            }
            else {
                content = file_buf_.as_string();
            }
            mInjectFileContent.insert((std::pair <std::string, std::string>(file_name, content)));
            delete file_buf;
        }


        InjectRule rule;
        rule.content = content;
        rule.type = atoi(type.c_str());
        rule.time = atoi(time.c_str());
        if (address.find("*") == 0) {
            if (address.length() == 1)
                mMatchAllRules.push_back(rule);
            else if (address[1] == '.') {
                std::string regaddress = address.substr(2);
                std::map<std::string, std::vector<InjectRule>>::iterator it;
                it = mDomainRuleMaps.find(regaddress);
                if (it != mDomainRuleMaps.end()) {
                    std::vector<InjectRule>* rules = (std::vector<InjectRule>*)&(it->second);
                    rules->push_back(rule);
                }
                else {
                    std::vector<InjectRule> rules;
                    rules.push_back(rule);
                    mDomainRuleMaps.insert(std::pair < std::string, std::vector<InjectRule>>(regaddress, rules));
                }
            }
        }
        else {
            std::map<std::string, std::vector<InjectRule>>::iterator it;
            it = mHostRuleMaps.find(address);
            if (it != mHostRuleMaps.end()) {
                std::vector<InjectRule>* rules = (std::vector<InjectRule>*)&(it->second);
                rules->push_back(rule);
            }
            else {
                std::vector<InjectRule> rules;
                rules.push_back(rule);
                mHostRuleMaps.insert(std::pair < std::string, std::vector<InjectRule>>(address, rules));
            }
        }
    }
//////////////////////////////////////////////////////////////////////////

}
