// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#include <felly/overload.hpp>
#include <format>
#include <variant>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("variant visit") {
  using MyType = std::variant<int, float>;
  const MyType myInt { 123 };
  const MyType myFloat { 1.23f };

  constexpr auto visitor = felly::overload {
    [](const int x) {
      return std::format("visit int {}", x);
    },
    [](const float x) {
      return std::format("visit float {:0.2f}", x);
    },
  };

  CHECK(std::visit(visitor, myInt) == "visit int 123");
  CHECK(std::visit(visitor, myFloat) == "visit float 1.23");
}