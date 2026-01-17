// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>

#include <optional>

#include "felly/non_copyable.hpp"
#include "felly/unique_any.hpp"

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

struct Aggregate {
  int value {};
};

struct WithTrackedDestructor {
  int value {};
  ~WithTrackedDestructor() {
    Tracker::call_count++;
    Tracker::last_value = value;
  }
};

[[nodiscard]]
constexpr bool Win32HandleIsValid(const void* const handle) {
  return handle && (std::bit_cast<intptr_t>(handle) != -1);
}

}// namespace

TEST_CASE("unique_any - basic values") {
  using unique_fd_like =
    felly::unique_any<int, FDLikeIntDeleter, &FDLikeIntIsValid>;

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

    u1 = unique_fd_like {-1};
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

TEST_CASE("unique_any - standard pointers") {
  using test_type = felly::unique_any<
    WithTrackedDestructor*,
    std::default_delete<WithTrackedDestructor> {}>;

  SECTION("is-valid") {
    CHECK_FALSE(test_type {nullptr});
    CHECK(test_type {new WithTrackedDestructor()});
  }

  SECTION("destructor called") {
    Tracker::reset();
    std::ignore = test_type {nullptr};
    CHECK(Tracker::call_count == 0);
    constexpr auto value = __LINE__;
    std::ignore = test_type {new WithTrackedDestructor(value)};
    CHECK(Tracker::call_count == 1);
    CHECK(Tracker::last_value == value);
  }

  SECTION("comparison with nullptr") {
    Tracker::reset();
    CHECK(test_type {nullptr} == nullptr);
    CHECK(test_type {new WithTrackedDestructor()} != nullptr);
  }
}

TEST_CASE("unique_any - -1 pointers") {
  using test_type = felly::unique_any<
    Aggregate*,
    [](const Aggregate* p) {
      Tracker::call_count++;
      Tracker::last_value = p->value;
      delete p;
    },
    &Win32HandleIsValid>;
  // negative pointers can't be constexpr, which is why we don't take an
  // invalid value template parameter
  const auto Invalid = reinterpret_cast<Aggregate*>(-1);

  SECTION("is-valid") {
    CHECK_FALSE(test_type {Invalid});
    CHECK_FALSE(test_type {nullptr});
    CHECK(test_type {new Aggregate()});
  }

  SECTION("comparison with nullptr") {
    CHECK(test_type {Invalid} == nullptr);
    CHECK(test_type {nullptr} == Invalid);
    CHECK(test_type {nullptr} == test_type {Invalid});
    CHECK(test_type {nullptr} == nullptr);
    CHECK(test_type {new Aggregate()} != nullptr);
    CHECK(test_type {new Aggregate()} != test_type {Invalid});
  }

  SECTION("valid is deleted") {
    Tracker::reset();
    constexpr auto value = __LINE__;
    std::ignore = test_type {new Aggregate {value}};
    CHECK(Tracker::call_count == 1);
    CHECK(Tracker::last_value == value);
  }
  SECTION("invalid is not deleted") {
    Tracker::reset();
    std::ignore = test_type {Invalid};
    CHECK(Tracker::call_count == 0);
  }

  SECTION("nullptr is not deleted") {
    Tracker::reset();
    std::ignore = test_type {nullptr};
    CHECK(Tracker::call_count == 0);
  }
}

TEST_CASE("non-copyable values") {
  struct value_type : felly::non_copyable {
    constexpr value_type() = default;
    constexpr explicit value_type(const int value) : value {value} {}

    int value {};
    constexpr operator bool() const noexcept { return value != 0; }
    constexpr bool operator==(const value_type&) const noexcept = default;
    constexpr bool operator==(const int other) const noexcept {
      return value == other;
    }
  };

  using test_type = felly::unique_any<value_type, [](const value_type& p) {
    Tracker::call_count++;
    Tracker::last_value = p.value;
  }>;

  SECTION("is-valid") {
    CHECK_FALSE(test_type {std::in_place});
    CHECK(test_type {std::in_place, 1});
  }
  SECTION("deleter not called for invalid") {
    Tracker::reset();
    std::ignore = test_type {std::in_place};
    CHECK(Tracker::call_count == 0);
  }

  SECTION("deleter called for valid") {
    Tracker::reset();
    constexpr auto value = __LINE__;
    std::ignore = test_type {std::in_place, value};
    CHECK(Tracker::call_count == 1);
    CHECK(Tracker::last_value == value);
  }

  SECTION("moveable") {
    Tracker::reset();
    constexpr auto value = __LINE__;
    auto u = test_type {std::in_place, value};
    CHECK(u);
    {
      auto u2 = std::move(u);
      CHECK_FALSE(u);
      CHECK(u2);
      CHECK(Tracker::call_count == 0);
    }
    CHECK(Tracker::call_count == 1);
    CHECK(Tracker::last_value == value);
  }

  SECTION("get") {
    constexpr auto value = __LINE__;
    CHECK(test_type {std::in_place, value}.get() == value);
  }
}
