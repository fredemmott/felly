// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#include <catch2/catch_all.hpp>
#include <felly/no_unique_address.hpp>

struct Empty {};

struct NotOptimized {
  Empty a;
  int b;
};

struct Optimized {
  FELLY_NO_UNIQUE_ADDRESS Empty a;
  int b;
};

TEST_CASE("FELLY_NO_UNIQUE_ADDRESS allows address overlapping", "") {
  SECTION("The macro should reduce the size of a struct with an empty member") {
    STATIC_CHECK(sizeof(Optimized) <= sizeof(NotOptimized));
  }

  SECTION("The address of the empty member can overlap with other members") {
    Optimized obj {};
    CHECK(reinterpret_cast<void*>(&obj.a) == reinterpret_cast<void*>(&obj.b));
  }
}
