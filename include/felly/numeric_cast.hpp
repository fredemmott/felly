// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <climits>
#include <cmath>
#include <concepts>
#include <format>
#include <limits>
#include <stdexcept>
#include <utility>

namespace felly_detail {

template <std::floating_point T>
struct float_traits {
  static constexpr auto sign_bits = 1;
  static constexpr auto mantissa_bits = std::numeric_limits<T>::digits - 1;
  static constexpr auto exponent_bits =
    (sizeof(T) * CHAR_BIT) - mantissa_bits - sign_bits;
};

template <std::size_t>
struct sized_uint;
template <>
struct sized_uint<4> {
  using type = uint32_t;
};
template <>
struct sized_uint<8> {
  using type = uint64_t;
};

#ifdef __cpp_lib_constexpr_cmath
using std::isnan;
#else
template <std::floating_point T>
constexpr bool isnan(const T v) noexcept {
  // We could use 'v != v', but let's do the more-involved thing to remain
  // compatible with fastmath (aka slightlybrokenmath)

  using U = sized_uint<sizeof(T)>::type;
  const auto bits = std::bit_cast<U>(v);

  // sign | exponent | mantissa
  constexpr auto mantissaMask = (1ull << float_traits<T>::mantissa_bits) - 1;
  constexpr auto exponentMask = ((1ull << float_traits<T>::exponent_bits) - 1)
    << float_traits<T>::mantissa_bits;

  return (exponentMask & bits) == exponentMask && (mantissaMask & bits) != 0;
}
#endif

};// namespace felly_detail

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
          (std::numeric_limits<T>::max)())) {}
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
  constexpr auto Max = static_cast<V>((std::numeric_limits<T>::max)());

  if (v < Lowest || v > Max) [[unlikely]] {
    throw numeric_cast_range_error(std::type_identity<T> {}, v);
  }
  return static_cast<T>(v);
}

template <std::floating_point T, std::integral U>
[[nodiscard]]
constexpr T numeric_cast(const U u) {
  constexpr auto Lowest = std::numeric_limits<T>::lowest();
  constexpr auto Max = (std::numeric_limits<T>::max)();
  if (u < Lowest || u > Max) [[unlikely]] {
    throw numeric_cast_range_error(std::type_identity<T> {}, u);
  }
  return static_cast<T>(u);
}

template <std::integral T, std::floating_point U>
[[nodiscard]]
constexpr T numeric_cast(const U u) {
  if (felly_detail::isnan(u)) [[unlikely]] {
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
    using V = felly_detail::sized_uint<sizeof(U)>::type;
    // IEEE float exponent bias
    constexpr V bias =
      (static_cast<V>(1) << (felly_detail::float_traits<U>::exponent_bits - 1))
      - 1;
    constexpr V exponent = std::numeric_limits<T>::digits + bias;
    return std::bit_cast<U>(
      exponent << felly_detail::float_traits<U>::mantissa_bits);
  }();

  if (u < Lowest || u >= TooHigh) [[unlikely]] {
    throw numeric_cast_range_error(std::type_identity<T> {}, u);
  }
  return static_cast<T>(u);
}

}// namespace felly::inline numeric_cast_types
