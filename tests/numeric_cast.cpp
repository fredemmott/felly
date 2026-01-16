// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <felly/numeric_cast.hpp>

#include <limits>

using namespace felly;

TEST_CASE("numeric_cast: Integral to Integral", "[numeric_cast][integral]") {
  SECTION("unsigned narrowing at limits") {
    constexpr auto Lowest = std::numeric_limits<uint32_t>::lowest();
    CHECK(numeric_cast<uint32_t>(static_cast<uint64_t>(Lowest)) == Lowest);
    constexpr auto Max = std::numeric_limits<uint32_t>::max();
    CHECK(numeric_cast<uint32_t>(static_cast<uint64_t>(Max)) == Max);
  }

  SECTION("signed narrowing at limits") {
    constexpr auto Lowest = std::numeric_limits<int32_t>::lowest();
    CHECK(numeric_cast<int32_t>(static_cast<int64_t>(Lowest)) == Lowest);
    constexpr auto Max = std::numeric_limits<int32_t>::max();
    CHECK(numeric_cast<int32_t>(static_cast<int64_t>(Max)) == Max);
  }

  SECTION("Out of range: Positive overflow") {
    constexpr auto TooHigh
      = static_cast<long long>(std::numeric_limits<int>::max()) + 1;
    CHECK_THROWS_AS(numeric_cast<int>(TooHigh), numeric_cast_range_error);
  }

  SECTION("Out of range: Negative overflow") {
    constexpr auto TooSmall
      = static_cast<long long>(std::numeric_limits<int>::lowest()) - 1;
    CHECK_THROWS_AS(numeric_cast<int>(TooSmall), numeric_cast_range_error);
  }

  SECTION("Signed vs Unsigned") {
    CHECK_THROWS_AS(numeric_cast<uint32_t>(-1), numeric_cast_range_error);
    CHECK(numeric_cast<int32_t>(100u) == 100);
  }
}

TEST_CASE(
  "numeric_cast: Floating Point to Floating Point",
  "[numeric_cast][float]") {
  SECTION("Standard conversion") {
    CHECK(numeric_cast<float>(1.5) == 1.5f);
  }

  SECTION("NaN handling") {
    CHECK(std::isnan(numeric_cast<float>(NAN)));
    CHECK(std::isnan(numeric_cast<double>(NAN)));
  }

  SECTION("At limits") {
    constexpr auto Lowest = std::numeric_limits<float>::lowest();
    constexpr auto Max = std::numeric_limits<float>::max();
    CHECK(numeric_cast<float>(static_cast<double>(Lowest)) == Lowest);
    CHECK(numeric_cast<float>(static_cast<double>(Max)) == Max);
  }

  SECTION("Past limits") {
    constexpr auto EpsilonF
      = static_cast<double>(std::numeric_limits<float>::epsilon());
    constexpr auto LowestF = std::numeric_limits<float>::lowest();
    constexpr auto MaxF = std::numeric_limits<float>::max();
    constexpr auto TooLow = static_cast<double>(LowestF) + (LowestF * EpsilonF);
    constexpr auto TooHigh = static_cast<double>(MaxF) + (MaxF * EpsilonF);
    CHECK_THROWS_AS(numeric_cast<float>(TooLow), numeric_cast_range_error);
    CHECK_THROWS_AS(numeric_cast<float>(TooHigh), numeric_cast_range_error);
  }
}

TEST_CASE(
  "numeric_cast: Float to Integral",
  "[numeric_cast][float_to_integral]") {
  SECTION("nan") {
    CHECK_THROWS_AS(numeric_cast<int>(std::numeric_limits<float>::quiet_NaN()), numeric_cast_range_error);
  }
  SECTION("unsigned narrowing at limits") {
    constexpr auto Lowest = std::numeric_limits<uint32_t>::lowest();
    CHECK(numeric_cast<uint32_t>(static_cast<double>(Lowest)) == Lowest);
    constexpr auto Max = std::numeric_limits<uint32_t>::max();
    CHECK(numeric_cast<uint32_t>(static_cast<double>(Max)) == Max);
  }

  SECTION("signed narrowing at limits") {
    constexpr auto Lowest = std::numeric_limits<int32_t>::lowest();
    CHECK(numeric_cast<int32_t>(static_cast<double>(Lowest)) == Lowest);
    constexpr auto Max = std::numeric_limits<int32_t>::max();
    CHECK(numeric_cast<int32_t>(static_cast<double>(Max)) == Max);
  }

  SECTION("Out of range: precision loss at limit") {
    constexpr auto Max = std::numeric_limits<uint32_t>::max();
    // float(0xffffffff) is larger than 0xffffffff
    CHECK(
      static_cast<double>(Max) < static_cast<double>(static_cast<float>(Max)));
    CHECK_THROWS_AS(
      numeric_cast<uint32_t>(static_cast<float>(Max)),
      numeric_cast_range_error);
  }

  SECTION("Out of range: Positive overflow") {
    constexpr auto TooHigh
      = static_cast<double>(std::numeric_limits<int>::max()) + 1;
    CHECK_THROWS_AS(numeric_cast<int>(TooHigh), numeric_cast_range_error);
  }

  SECTION("Out of range: Negative overflow") {
    constexpr auto TooLow
      = static_cast<double>(std::numeric_limits<int>::lowest()) - 1;
    CHECK_THROWS_AS(numeric_cast<int>(TooLow), numeric_cast_range_error);
  }
}

TEST_CASE(
  "numeric_cast: Integral to Float",
  "[numeric_cast][integral_to_float]") {

  constexpr auto High  = static_cast<uint32_t>(1) << 31;
  CHECK(numeric_cast<float>(High) == High);

  constexpr auto Low = std::numeric_limits<int32_t>::lowest();
  CHECK(numeric_cast<float>(Low) == Low);
}
