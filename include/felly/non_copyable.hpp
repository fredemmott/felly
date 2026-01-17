// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <compare>

namespace felly::inline non_copyable_types {

/** A type that is moveable, but not copyable.
 *
 * The primary difference from boost::non_copyable is that it is moveable.
 *
 * This is most useful as shorthand supertype, but can also be used as a member.
 * If you use it as a member, you likely want to use [[no_unique_address]]
 */
struct non_copyable {
  constexpr non_copyable() = default;
  constexpr ~non_copyable() = default;

  non_copyable(const non_copyable&) = delete;
  non_copyable& operator=(const non_copyable&) = delete;

  constexpr non_copyable(non_copyable&&) noexcept = default;
  constexpr non_copyable& operator=(non_copyable&&) noexcept = default;

  // Keep users compatible with common idioms
  constexpr auto operator<=>(const non_copyable&) const noexcept = default;
  friend constexpr void swap(non_copyable&, non_copyable&) noexcept {}
};


}// namespace felly::inline non_copyable_types
