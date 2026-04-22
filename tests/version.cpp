// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: BSL-1.0

#include <felly/version.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("FELLY_CPLUSPLUS >= FELLY_MINIMUM_CPLUSPLUS") {
  STATIC_CHECK(FELLY_CPLUSPLUS >= FELLY_MINIMUM_CPLUSPLUS);
}