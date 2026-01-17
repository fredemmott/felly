// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

namespace felly::inline overload_types {

/// Allows `std::visit()` on a variant with exhaustiveness checks
template <class... Ts>
struct overload : Ts... {
  using Ts::operator()...;
};

}// namespace felly::inline overload_types
