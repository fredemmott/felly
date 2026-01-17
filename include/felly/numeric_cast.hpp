// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <cmath>
#include <concepts>
#include <format>
#include <stdexcept>
#include <utility>

namespace felly::inline numeric_cast_types {

struct numeric_cast_range_error : std::range_error {
  using range_error::range_error;

  template <class T>
  numeric_cast_range_error(std::type_identity<T>, const auto value)
    : std::range_error(
        std::format(
          "Value {} out of range {}..{}",
          value,
          std::numeric_limits<T>::lowest(),
          std::numeric_limits<T>::max())) {}
};

template <std::integral T>
[[nodiscard]]
constexpr T numeric_cast(const std::integral auto v) {
  if (!std::in_range<T>(v)) [[unlikely]] {
    throw numeric_cast_range_error(std::type_identity<T> {}, v);
  }
  return static_cast<T>(v);
}

template <std::floating_point T, std::floating_point U>
[[nodiscard]]
constexpr T numeric_cast(const U u) {
  if (std::isnan(u)) {
    return static_cast<T>(u);
  }

  using V = std::common_type_t<T, U>;
  const auto v = static_cast<V>(u);

  constexpr auto Lowest = static_cast<V>(std::numeric_limits<T>::lowest());
  constexpr auto Max = static_cast<V>(std::numeric_limits<T>::max());

  if (v < Lowest || v > Max) [[unlikely]] {
    throw numeric_cast_range_error(std::type_identity<T> {}, v);
  }
  return static_cast<T>(v);
}

template <std::floating_point T, std::integral U>
[[nodiscard]]
constexpr T numeric_cast(const U u) {
  constexpr auto Lowest = std::numeric_limits<T>::lowest();
  constexpr auto Max = std::numeric_limits<T>::max();
  if (u < Lowest || u > Max) [[unlikely]] {
    throw numeric_cast_range_error(std::type_identity<T> {}, u);
  }
  return static_cast<T>(u);
}

template <std::integral T, std::floating_point U>
[[nodiscard]]
constexpr T numeric_cast(const U u) {
  if (std::isnan(u)) [[unlikely]] {
    throw numeric_cast_range_error("Can't convert NaN to an integral type");
  }

  constexpr auto Lowest = static_cast<U>(std::numeric_limits<T>::lowest());
  // - Not directly comparing to `max()` to avoid precision loss issues
  // - Not using std::ldexp as while it's constexpr in C++23, it's not constexpr
  //   in MSVC 2022 C++23:
  // - Not using std::pow() as it's not constexpr in C++26
  //
  // https://github.com/microsoft/STL/issues/2530
  //
  // Might need to keep this for MSVC even when the above is resolved:
  //
  // > A major concern is how to address accuracy issues (i.e. should the
  // > compiler prioritize mathematically-correct results, or UCRT
  // > bug-compatibility).
  //
  // If MSVC chooses bug-compatibility, the standard version may be unusable
  constexpr auto TooHigh = [] constexpr {
    auto exponent = std::numeric_limits<T>::digits;
    U result = 1.0;
    U base = 2.0;
    while (exponent > 0) {
      if ((exponent % 2) != 0) result *= base;
      base *= base;
      exponent /= 2;
    }
    return result;
  }();

  if (u < Lowest || u >= TooHigh) [[unlikely]] {
    throw numeric_cast_range_error(std::type_identity<T> {}, u);
  }
  return static_cast<T>(u);
}

}// namespace felly::inline numeric_cast_types
