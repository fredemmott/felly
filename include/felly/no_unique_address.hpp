// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

// MSVC recognizes [[no_unique_address]], but ignores it and requires its
// own attribute, to avoid an ABI break in libraries that were built under the
// generic '*all* unrecognized attributes do nothing' behavior
#if __has_cpp_attribute(msvc::no_unique_address)
#define FELLY_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#define FELLY_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif
