// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#ifdef _WIN32
#define ASAN_DEFAULT_EXPORT __declspec(dllexport)
#else
#define ASAN_DEFAULT_EXPORT __attribute__((visibility("default")))
#endif

extern "C" ASAN_DEFAULT_EXPORT const char* __asan_default_options() {
  return "halt_on_error=1"
         ":detect_stack_use_after_return=1"
         ":check_initialization_order=1"
         ":strict_init_order=1"
#ifndef _MSC_VER
         // As of 2026-01-29, this is unsupported by VS 2022, and is a hard
         // failure
         ":detect_leaks=1"
#endif
    ;
}
