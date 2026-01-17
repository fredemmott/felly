// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

/* MSVC reports an incorrect `__cplusplus` value unless
 * `/Zc:__cplusplus` is passed
 *
 * `>=` is checked here for compatibility with clang-cl and CMake; as of
 * 2026-01 C++23 in MSVC still requires `/std:c++23preview`, and CMake passes
 * `-xclang:-std=c++23` when using `clang-cl`. This results in `_MSVC_LANG`
 * having a C++20 value, but `__cplusplus` having the correct C++23 value.
 *
 * Observed with:
 * - CMake 4.1.2
 * - Visual Studio 2022
 * - clang-cl 19.1.5 bundled with Visual Studio
 */
#if defined(_MSVC_LANG) && (_MSVC_LANG >= __cplusplus)
#define FELLY_CPLUSPLUS _MSVC_LANG
#else
#define FELLY_CPLUSPLUS __cplusplus
#endif

#define FELLY_MINIMUM_CPLUSPLUS 202302L
