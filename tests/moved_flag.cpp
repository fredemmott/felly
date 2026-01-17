// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#include <catch2/catch_test_macros.hpp>

#include <felly/moved_flag.hpp>

using felly::moved_flag;

TEST_CASE("move construction") {
  moved_flag a;
  CHECK_FALSE(a);
  const auto b = std::move(a);
  CHECK(a);
  CHECK_FALSE(b);
}

TEST_CASE("double-move") {
  moved_flag a;
  auto b = std::move(a);
  auto c = std::move(a);
  CHECK(a);
  CHECK_FALSE(b);
  CHECK(c);
}

TEST_CASE("chained move") {
  moved_flag a;
  auto b = std::move(a);
  auto c = std::move(b);
  CHECK(a);
  CHECK(b);
  CHECK_FALSE(c);
}

TEST_CASE("copy-then-move construction") {
  moved_flag a;
  auto b = a;
  CHECK_FALSE(a);
  CHECK_FALSE(b);
  auto a2 = std::move(a);
  CHECK(a);
  CHECK_FALSE(a2);
  CHECK_FALSE(b);
  auto b2 = std::move(b);
  CHECK(b);
  CHECK_FALSE(b2);
}

TEST_CASE("move-then-copy construction") {
  moved_flag moved_from;
  auto moved_to = std::move(moved_from);
  auto moved_from_copy = moved_from;
  auto moved_to_copy = moved_to;
  CHECK(moved_from);
  CHECK(moved_from_copy);
  CHECK_FALSE(moved_to);
  CHECK_FALSE(moved_to_copy);
}

TEST_CASE("swap") {
  moved_flag a;
  auto b = std::move(a);
  CHECK(a);
  CHECK_FALSE(b);
  std::swap(a, b);
  CHECK_FALSE(a);
  CHECK(b);
}

TEST_CASE("assignment") {
  SECTION("move") {
    moved_flag a;
    moved_flag b;
    [[maybe_unused]] auto ignored = std::move(b);
    CHECK_FALSE(a);
    CHECK(b);
    b = std::move(a);
    CHECK(a);
    CHECK_FALSE(b);
  }

  SECTION("copy") {
    moved_flag a;
    moved_flag b;
    CHECK_FALSE(a);
    CHECK_FALSE(b);
    b = a;
    CHECK_FALSE(a);
    CHECK_FALSE(b);
    a = std::move(b);
    CHECK(b);
    CHECK_FALSE(a);
    b = a;
    CHECK_FALSE(a);
    CHECK_FALSE(b);
    a = std::move(b);
    a = b;
    CHECK(a);
    CHECK(b);
  }
}

TEST_CASE("comparable") {
  STATIC_CHECK(std::totally_ordered<moved_flag>);
  struct foo {
    int value {};
    moved_flag moved {};
    constexpr auto operator<=>(const foo&) const noexcept = default;
  };
  STATIC_CHECK(std::totally_ordered<foo>);
}
