// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_main.h"

#include <memory>
#include <vector>
#include <algorithm>
#include <functional>
#include "base/macros.h"

#include "content/public/app/content_main_runner.h"

namespace content {

int ContentMain(const ContentMainParams& params) {
  std::unique_ptr<ContentMainRunner> main_runner(ContentMainRunner::Create());

  int exit_code = main_runner->Initialize(params);
  if (exit_code >= 0)
    return exit_code;

  exit_code = main_runner->Run();

  main_runner->Shutdown();
  ExecuteAllPostHandler();

  return exit_code;
}
CR_DEFINE_STATIC_LOCAL(std::vector<std::function<void()>>, post_handlers, ());
//std::vector<std::function<void()>> post_handlers;

void AddPostHandler(std::function<void()> const& post_handler) {
  post_handlers.push_back(post_handler);
}

void ExecuteAllPostHandler() {
  std::for_each(post_handlers.begin(), post_handlers.end(), [](
    std::function<void()>& post_handler) {
    post_handler();
  });
}

}  // namespace content
