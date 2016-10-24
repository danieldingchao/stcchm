#include <string>
#include "base/file_path.h"
#include "base/at_exit.h"
#include "base/path_service.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/md5.h"

std::string CheckSum(std::string& text){
  return base::MD5String(text);
}

void Encrypt(std::string& text){
  if(!text.length()){
    return ;
  }

  for(size_t i=0;i<text.length();i++){
    text[i]^=((i+7)%13+'A');
  }
}

int main(int argc,char* argv[]){
  base::AtExitManager atexit_mgr;

  CommandLine::Init(argc,argv);
  CommandLine* cmdline =  CommandLine::ForCurrentProcess();
  FilePath in_path = cmdline->GetSwitchValuePath("rule");
  if(!in_path.value().length()){
    printf("usage: --rule=<path> --encrypt\n");
    printf("invalid argc\n");
    return -1;
  }
  
  FilePath out_path = in_path.AddExtension(L".out");
  bool encrypt = cmdline->HasSwitch("encrypt");
  if(encrypt){
    std::string filters;
    file_util::ReadFileToString(in_path,&filters);
    if(!filters.length() || filters[0]!='#'){
      printf("invalid file\n");
      return -1;
    }

    std::string check_sum = CheckSum(filters);
    Encrypt(filters);
    filters.append(check_sum);

    file_util::WriteFile(out_path,&filters[0],filters.length());
    printf("encrypt ok\n");
    return 0;
 }else{
  std::string filters;
  file_util::ReadFileToString(in_path,&filters);
    if(filters.length() < 32 || filters[0]=='#'){
      printf("invalid file\n");
      return -1;
    }

    std::string check_sum = filters.substr(filters.length() - 32);
    filters=filters.substr(0,filters.length()-32);
  Encrypt(filters);
    std::string check_sum_computed = CheckSum(filters);
    if(check_sum!=check_sum_computed){
      printf("invalid checksum\n");
      return -1;
    }

  file_util::WriteFile(out_path,&filters[0],filters.length());
    printf("decrypt ok\n");
    return 0;
  }

  printf("usage: --rule=<path> --encrypt\n");
  return 0;
}
