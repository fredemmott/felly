// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#include <catch2/catch_test_macros.hpp>

#include <felly/non_copyable.hpp>

#include <concepts>

using felly::non_copyable;

TEST_CASE("static copy/move checks") {
  STATIC_CHECK(std::movable<non_copyable>);
  STATIC_CHECK_FALSE(std::copyable<non_copyable>);
  STATIC_CHECK_FALSE(std::copy_constructible<non_copyable>);
  STATIC_CHECK_FALSE(std::is_copy_assignable_v<non_copyable>);
}

TEST_CASE("comparison checks") {
  STATIC_CHECK(std::equality_comparable<non_copyable>);
  STATIC_CHECK(std::totally_ordered<non_copyable>);

  CHECK(non_copyable {} == non_copyable {});
  STATIC_CHECK(non_copyable {} == non_copyable {});

  struct foo : non_copyable {
    int value {};
    constexpr auto operator<=>(const foo&) const noexcept = default;
  };
  STATIC_CHECK(std::totally_ordered<foo>);
  CHECK( foo { .value = 123 } < foo { .value = 456});
  CHECK( foo { .value = 456 } > foo { .value = 123});

  CHECK( foo { .value = 123} == foo { .value = 123});
  CHECK_FALSE( foo { .value = 123} == foo { .value = 456});

  CHECK_FALSE( foo { .value = 123} != foo { .value = 123});
  CHECK( foo { .value = 123} != foo { .value = 456});
}

TEST_CASE("swappable") {
  STATIC_CHECK(std::swappable<non_copyable>);
}
