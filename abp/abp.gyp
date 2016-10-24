# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'abp',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
		'../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
      ],
      'defines': [ 'ABP_IMPLEMENTATION' ],	  
      'sources': [
        'abp.cc',
        'abp.h',
        'abp_filter.cc',
        'abp_filter.h',
        'abp_matcher.cc',
        'abp_matcher.h',
        'abp_parser.cc',
        'abp_parser.h',
        'css_js_injector_manager.cc',
        'css_js_injector_manager.h',		
		'abp_export.h'
      ],
    },
    {
      'target_name': 'abp_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'abp',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'run_all_unittests.cc',
        'abp_unittest.cc',
        'abp_filter_unittest.cc',
        'abp_matcher_unittest.cc',
        'abp_parser_unittest.cc',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      'target_name': 'abp_tools',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'abp',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'abp_tools.cc',
      ],
      'include_dirs': [
        '..',
      ],
    },
  ],
}
