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

  SECTION("holds values") {
    constexpr auto v1 = __LINE__;
    constexpr auto v2 = __LINE__;
    CHECK(unique_fd_like {v1}.get() == v1);
    CHECK(unique_fd_like {v2}.get() == v2);
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

  SECTION("invalid values are equivalent") {
    Tracker::reset();
    CHECK(unique_fd_like {-1} == unique_fd_like {-1});
    CHECK(unique_fd_like {-1} == unique_fd_like {-2});
    CHECK(Tracker::call_count == 0);
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

  SECTION("move to new") {
    Tracker::reset();
    constexpr auto value = __LINE__;
    {
      auto u = unique_fd_like {value};
      CHECK(u);
      auto u2 = std::move(u);
      CHECK_FALSE(u);
      CHECK(u2);
      CHECK(Tracker::call_count == 0);
    }
    CHECK(Tracker::call_count == 1);
    CHECK(Tracker::last_value == value);
  }

  SECTION("move to self") {
    Tracker::reset();
    constexpr auto value = __LINE__;
    auto u = unique_fd_like {value};
    // `u = std::move(u)`, but bypass move-to-self compiler warnings
    u = std::move(*std::addressof(u));
    CHECK(u);
    CHECK(u.get() == value);
    CHECK(Tracker::call_count == 0);
  }

  SECTION("move to owning") {
    Tracker::reset();
    constexpr auto v1 = __LINE__;
    constexpr auto v2 = __LINE__;
    {
      auto u1 = unique_fd_like {v1};
      auto u2 = unique_fd_like {v2};
      u2 = std::move(u1);
      CHECK(u2.get() == v1);
      CHECK(Tracker::call_count == 1);
      CHECK(Tracker::last_value == v2);
    }
    CHECK(Tracker::call_count == 2);
    CHECK(Tracker::last_value == v1);
  }

  SECTION("swap") {
    Tracker::reset();
    constexpr auto v1 = __LINE__;
    constexpr auto v2 = __LINE__;
    auto u1 = unique_fd_like {v1};
    auto u2 = unique_fd_like {v2};

    std::swap(u1, u1);
    CHECK(u1.get() == v1);
    CHECK(Tracker::call_count == 0);

    std::swap(u1, u2);
    CHECK(u1.get() == v2);
    CHECK(u2.get() == v1);
    CHECK(Tracker::call_count == 0);

    auto u3 = std::move(u2);
    CHECK(Tracker::call_count == 0);
    CHECK(u3.get() == v1);
    CHECK(u1);
    CHECK_FALSE(u2);
    std::swap(u1, u2);
    CHECK(u2);
    CHECK_FALSE(u1);
    CHECK(Tracker::call_count == 0);
    CHECK(u2.get() == v2);

    u1 = {-1};
    CHECK_FALSE(u1);
    std::swap(u1, u2);
    CHECK(u1);
    CHECK_FALSE(u2);
    CHECK(Tracker::call_count == 0);
    CHECK(u1.get() == v2);
  }

  SECTION("equality") {
    CHECK(unique_fd_like {0} == unique_fd_like {0});
    CHECK(unique_fd_like {-1} == unique_fd_like {-1});
    CHECK_FALSE(unique_fd_like {0} == unique_fd_like {1});
  }
}
