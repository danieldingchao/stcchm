// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ABP_EXPORT_H_
#define ABP_EXPORT_H_
#pragma once

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(ABP_IMPLEMENTATION)
#define ABP_EXPORT __declspec(dllexport)
#else
#define ABP_EXPORT __declspec(dllimport)
#endif  // defined(ABP_IMPLEMENTATION)

#else // defined(WIN32)
#if defined(ABP_IMPLEMENTATION)
#define ABP_EXPORT __attribute__((visibility("default")))
#else
#define ABP_EXPORT
#endif
#endif

#else // defined(COMPONENT_BUILD)
#define ABP_EXPORT
#endif

#endif  // ABP_EXPORT_H_
