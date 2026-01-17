// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <felly/unique_any.hpp>

#include <optional>

namespace {

struct Tracker {
  static inline int call_count = 0;
  static inline std::optional<int> last_value = 0;

  static void reset() {
    call_count = 0;
    last_value.reset();
  }
};

void FDLikeIntDeleter(const int fd) {
  Tracker::call_count++;
  Tracker::last_value = fd;
}

[[nodiscard]]
constexpr bool FDLikeIntIsValid(const int fd) {
  return fd >= 0;
}

using unique_fd_like =
  felly::unique_any<int, FDLikeIntDeleter, &FDLikeIntIsValid>;
}// namespace

TEST_CASE("unique_any - basic behavior") {
  SECTION("static checks") {
    STATIC_CHECK(std::swappable<unique_fd_like>);
    STATIC_CHECK(std::movable<unique_fd_like>);
    STATIC_CHECK_FALSE(std::copyable<unique_fd_like>);

    STATIC_CHECK(std::equality_comparable<unique_fd_like>);
    STATIC_CHECK(std::totally_ordered<unique_fd_like>);
  }

  SECTION("ordering") {
    CHECK(unique_fd_like {0} < unique_fd_like {1});
    CHECK_FALSE(unique_fd_like {0} > unique_fd_like {1});

    CHECK(unique_fd_like {1} > unique_fd_like {0});
    CHECK_FALSE(unique_fd_like {1} < unique_fd_like {0});

    CHECK(unique_fd_like {0} <= unique_fd_like {0});
    CHECK(unique_fd_like {0} >= unique_fd_like {0});

    CHECK(unique_fd_like {0} == unique_fd_like {0});
    CHECK_FALSE(unique_fd_like {0} == unique_fd_like {1});

    CHECK_FALSE(unique_fd_like {0} != unique_fd_like {0});
    CHECK(unique_fd_like {0} != unique_fd_like {1});
  }

  SECTION("deleter called on scope exit") {
    Tracker::reset();
    constexpr auto value = __LINE__;
    std::ignore = unique_fd_like {value};
    CHECK(Tracker::call_count == 1);
    CHECK(Tracker::last_value == value);
  }

  SECTION("move") {
    Tracker::reset();
    constexpr auto value = __LINE__;
    {
      auto u = unique_fd_like {value};
      auto u2 = std::move(u);
      CHECK(Tracker::call_count == 0);
    }
    CHECK(Tracker::call_count == 1);
    CHECK(Tracker::last_value == value);
  }

  SECTION("is-valid test") {
    Tracker::reset();
    {
      auto valid = unique_fd_like {0};
      auto invalid = unique_fd_like {-1};
      CHECK(Tracker::call_count == 0);
      CHECK_FALSE(Tracker::last_value.has_value());

      CHECK(valid);
      CHECK_FALSE(invalid);
    }
    CHECK(Tracker::call_count == 1);
    CHECK(Tracker::last_value == 0);
  }
}
