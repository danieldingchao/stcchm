// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/aead.h"

#include <openssl/aes.h>
#include <openssl/evp.h>
#include <stddef.h>
#include <stdint.h>
#include <string>

#include "base/strings/string_util.h"
#include "crypto/openssl_util.h"
#include "des.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/md5.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

int main(int argc, char** argv) {
  base::CommandLine::Init(0, NULL);

  base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  if (cmd_line.HasSwitch("input")) {
    base::FilePath input = cmd_line.GetSwitchValuePath("input");
    base::FilePath output = cmd_line.GetSwitchValuePath("output");

    if (!base::PathExists(input))
      return 0;

    int64_t size;
    if (!base::GetFileSize(input, &size))
      return 0;

    std::string contents;
    if (!base::ReadFileToString(input, &contents))
      return false;
    std::string encrypted;
    std::string pwd = "lEmOn123";
    des_encrypt((unsigned char*)pwd.c_str(), contents, &encrypted);
    base::WriteFile(output, encrypted.c_str(), encrypted.length());
    return 0;
  }

  std::vector<base::string16> args = cmd_line.GetArgs();
  base::FilePath input_dir = base::FilePath(args[0]);
  base::FilePath output_dir = base::FilePath(args[1]);

  std::string encrypted;
  std::string pwd = "lEmOn123";

  base::FilePath file = input_dir.AppendASCII("jw1.txt");
  base::FilePath output = output_dir.AppendASCII("jw1.dat");
  base::FilePath md5file = output_dir.AppendASCII("jw1.md5");
  std::string contents;
  if (base::ReadFileToString(file, &contents)) {
    des_encrypt((unsigned char*)pwd.c_str(), contents, &encrypted);
    base::WriteFile(output, encrypted.c_str(), encrypted.length());
    std::string md5 = base::MD5String(contents);
    base::WriteFile(md5file, md5.c_str(), md5.length());
  }

  file = input_dir.AppendASCII("jw2.txt");
  output = output_dir.AppendASCII("jw2.dat");
  md5file = output_dir.AppendASCII("jw2.md5");
  if (base::ReadFileToString(file, &contents)) {
    des_encrypt((unsigned char*)pwd.c_str(), contents, &encrypted);
    base::WriteFile(output, encrypted.c_str(), encrypted.length());
    std::string md5 = base::MD5String(contents);
    base::WriteFile(md5file, md5.c_str(), md5.length());
  }


}

