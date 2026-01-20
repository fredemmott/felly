// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>

#include <optional>
#include "felly/moved_flag.hpp"
#include "felly/non_copyable.hpp"
#include "felly/unique_any.hpp"

#include <catch2/catch_template_test_macros.hpp>

#include "felly/numeric_cast.hpp"

namespace {

struct Tracker {
  static inline int call_count = 0;
  static inline std::optional<int> last_value = 0;

  static void reset() {
    call_count = 0;
    last_value.reset();
  }

  static void track(const int value) {
    ++call_count;
    last_value = value;
  }
};

struct fd_like_traits {
  using value_type = const int;
  using storage_type = int;

  static constexpr auto default_value() noexcept { return -1; }
  static void destroy(storage_type& s) {
    Tracker::track(s);
    s = default_value();
  }
  static constexpr bool has_value(const int value) { return value >= 0; }

  template <class S>
  static constexpr decltype(auto) value(S&& storage) {
    return std::forward<S>(storage);
  }

  static constexpr void construct(storage_type& storage, const int value) {
    storage = value;
  }
};
static_assert(felly::unique_any_traits<fd_like_traits>);

struct Aggregate {
  int value {};
};
struct negative_pointer_traits {
  using value_type = Aggregate* const;
  using storage_type = Aggregate*;
  static constexpr auto default_value() { return nullptr; }

  static void destroy(storage_type& s) {
    Tracker::track(s->value);
    delete std::exchange(s, nullptr);
  }

  [[nodiscard]]
  static constexpr bool has_value(const storage_type& s) {
    return s && std::bit_cast<intptr_t>(s) != -1;
  }

  template <class S>
  static constexpr decltype(auto) value(S&& storage) {
    return std::forward<S>(storage);
  }

  static constexpr void construct(storage_type& storage, Aggregate* value) {
    storage = value;
  }
};

struct WithTrackedDestructor {
  constexpr WithTrackedDestructor() = default;
  constexpr WithTrackedDestructor(WithTrackedDestructor&&) = default;
  constexpr WithTrackedDestructor& operator=(WithTrackedDestructor&&) = default;
  explicit constexpr WithTrackedDestructor(const int value) : value(value) {}
  int value {};

  ~WithTrackedDestructor() {
    if (moved) {
      return;
    }
    Tracker::track(value);
  }

  felly::non_copyable nocopy;
  felly::moved_flag moved;
};

}// namespace

using unique_fd_like_with_traits = felly::basic_unique_any<fd_like_traits>;
using unique_fd_like_inline =
  felly::unique_any<const int, &Tracker::track, [](const int v) {
    return v >= 0;
  }>;

static_assert(sizeof(unique_fd_like_with_traits) == sizeof(int));
static_assert(sizeof(unique_fd_like_inline) == sizeof(std::optional<int>));

TEMPLATE_TEST_CASE(
  "unique_any - basic functionality",
  "",
  unique_fd_like_with_traits,
  unique_fd_like_inline) {
  SECTION("static checks") {
    STATIC_CHECK(std::same_as<const int, typename TestType::value_type>);
    STATIC_CHECK(std::swappable<TestType>);
    STATIC_CHECK(std::movable<TestType>);
    STATIC_CHECK_FALSE(std::copyable<TestType>);

    STATIC_CHECK(std::equality_comparable<TestType>);
    STATIC_CHECK(std::totally_ordered<TestType>);
  }

  SECTION("holds values") {
    constexpr auto v1 = __LINE__;
    constexpr auto v2 = __LINE__;
    CHECK(TestType {v1}.get() == v1);
    CHECK(*TestType {v1} == v1);
    CHECK(TestType {v2}.get() == v2);
    CHECK(*TestType {v2} == v2);
  }

  SECTION("operator& const correctness") {
    constexpr auto value = __LINE__;
    TestType u {value};
    auto p = &u;
    STATIC_CHECK(std::is_pointer_v<decltype(p)>);
    STATIC_CHECK(std::is_const_v<std::remove_reference_t<decltype(*p)>>);
    STATIC_CHECK(std::is_same_v<decltype(p), typename TestType::value_type*>);
    CHECK(*p == value);
  }

  SECTION("is-valid test") {
    Tracker::reset();
    {
      auto valid = TestType {0};
      auto invalid = TestType {-1};
      CHECK(Tracker::call_count == 0);
      CHECK_FALSE(Tracker::last_value.has_value());

      CHECK(valid);
      CHECK_FALSE(invalid);

      CHECK_FALSE(TestType {std::nullopt});
    }
    CHECK(Tracker::call_count == 1);
    CHECK(Tracker::last_value == 0);
  }

  SECTION("invalid values are equivalent") {
    Tracker::reset();
    CHECK(TestType {-1} == TestType {-1});
    CHECK(TestType {-1} == TestType {-2});
    CHECK(Tracker::call_count == 0);
  }

  SECTION("ordering") {
    CHECK(TestType {0} < TestType {1});
    CHECK_FALSE(TestType {0} > TestType {1});

    CHECK(TestType {1} > TestType {0});
    CHECK_FALSE(TestType {1} < TestType {0});

    CHECK(TestType {0} <= TestType {0});
    CHECK(TestType {0} >= TestType {0});

    CHECK(TestType {0} == TestType {0});
    CHECK_FALSE(TestType {0} == TestType {1});

    CHECK_FALSE(TestType {0} != TestType {0});
    CHECK(TestType {0} != TestType {1});
  }

  SECTION("deleter called on scope exit") {
    Tracker::reset();
    constexpr auto value = __LINE__;
    std::ignore = TestType {value};
    CHECK(Tracker::call_count == 1);
    CHECK(Tracker::last_value == value);
  }

  SECTION("move to new") {
    Tracker::reset();
    constexpr auto value = __LINE__;
    {
      auto u = TestType {value};
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
    auto u = TestType {value};
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
      auto u1 = TestType {v1};
      auto u2 = TestType {v2};
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
    auto u1 = TestType {v1};
    auto u2 = TestType {v2};

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

    u1 = TestType {-1};
    CHECK_FALSE(u1);
    std::swap(u1, u2);
    CHECK(u1);
    CHECK_FALSE(u2);
    CHECK(Tracker::call_count == 0);
    CHECK(u1.get() == v2);
  }

  SECTION("equality") {
    CHECK(TestType {0} == TestType {0});
    CHECK(TestType {-1} == TestType {-1});
    CHECK_FALSE(TestType {0} == TestType {1});
  }

  SECTION("disown()") {
    Tracker::reset();
    {
      constexpr auto value = __LINE__;
      auto u = TestType {value};
      CHECK(u.disown() == value);
      CHECK_FALSE(u);
    }
    CHECK(Tracker::call_count == 0);
    CHECK_FALSE(Tracker::last_value.has_value());
  }
  SECTION("reset()") {
    Tracker::reset();
    constexpr auto v1 = __LINE__;
    {
      auto u = TestType {v1};
      CHECK(u);
      u.reset();
      CHECK_FALSE(u);

      CHECK(Tracker::call_count == 1);
      CHECK(Tracker::last_value == v1);
    }
    CHECK(Tracker::call_count == 1);
    CHECK(Tracker::last_value == v1);
  }

  SECTION("reset() with new value") {
    Tracker::reset();
    constexpr auto v1 = __LINE__;
    constexpr auto v2 = __LINE__;
    {
      auto u = TestType {v1};
      u.reset(v2);
      CHECK(Tracker::call_count == 1);
      CHECK(Tracker::last_value == v1);
    }
    CHECK(Tracker::call_count == 2);
    CHECK(Tracker::last_value == v2);
  }

  SECTION("get() is an immutable reference") {
    static constexpr auto value = __LINE__;
    TestType v {value};
    CHECK(v.get() == value);
    using U = decltype(v.get());
    STATIC_CHECK(std::is_reference_v<U>);
    STATIC_CHECK(std::is_const_v<std::remove_reference_t<U>>);
  }

  SECTION("operator*() is an immutable reference") {
    static constexpr auto value = __LINE__;
    TestType v {value};
    CHECK(*v == value);
    using U = decltype(*v);
    STATIC_CHECK(std::is_reference_v<U>);
    STATIC_CHECK(std::is_const_v<std::remove_reference_t<U>>);
  }
}

TEST_CASE("unique_any - standard pointers") {
  using test_type = felly::unique_any<
    WithTrackedDestructor*,
    std::default_delete<WithTrackedDestructor> {}>;
  static_assert(std::derived_from<
                test_type,
                felly::unique_any<
                  WithTrackedDestructor*,
                  std::default_delete<WithTrackedDestructor> {}>>);
  SECTION("size") { STATIC_CHECK(sizeof(test_type) == sizeof(void*)); }

  SECTION("is-valid") {
    CHECK_FALSE(test_type {nullptr});
    CHECK_FALSE(test_type {std::nullopt});
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

  SECTION("get()") {
    constexpr auto value = __LINE__;
    auto u = test_type {new WithTrackedDestructor(value)};
    using TGet = decltype(u.get());
    STATIC_CHECK(std::same_as<TGet, WithTrackedDestructor*&>);
    STATIC_CHECK(std::is_reference_v<TGet>);
    STATIC_CHECK(std::is_pointer_v<std::remove_reference_t<TGet>>);
    CHECK(u.get()->value == value);
  }

  SECTION("operator->") {
    constexpr auto v1 = __LINE__;
    constexpr auto v2 = __LINE__;
    auto v = test_type {new WithTrackedDestructor(v1)};
    CHECK(v->value == v1);
    v->value = v2;
    CHECK(v->value == v2);
  }

  SECTION("comparison with nullptr") {
    Tracker::reset();
    CHECK(test_type {nullptr} == nullptr);
    CHECK(test_type {new WithTrackedDestructor()} != nullptr);
  }
}

TEST_CASE("unique_any - -1 pointers") {
  using test_type = felly::basic_unique_any<negative_pointer_traits>;
  // negative pointers can't be constexpr, which is why we don't take an
  // invalid value template parameter
  const auto Invalid = reinterpret_cast<Aggregate*>(-1);

  SECTION("size") { STATIC_CHECK(sizeof(test_type) == sizeof(void*)); }

  SECTION("is-valid") {
    CHECK_FALSE(test_type {Invalid});
    CHECK_FALSE(test_type {nullptr});
    CHECK(test_type {new Aggregate()});
  }

  SECTION("comparison with nullptr") {
    CHECK(test_type {Invalid} == nullptr);
    CHECK(test_type {nullptr} == test_type {std::nullopt});
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

TEST_CASE("unique_any - const pointers") {
  using mutable_test_type = felly::unique_any<
    WithTrackedDestructor*,
    std::default_delete<const WithTrackedDestructor> {}>;
  using const_test_type = felly::unique_any<
    const WithTrackedDestructor*,
    std::default_delete<const WithTrackedDestructor> {}>;

  SECTION("basic operation") {
    Tracker::reset();
    constexpr auto value = __LINE__;
    {
      const_test_type source {new const WithTrackedDestructor(value)};
      STATIC_CHECK_FALSE(std::is_assignable_v<decltype((source->value)), int>);
      auto dest = std::move(source);
      CHECK(dest->value == value);
      STATIC_CHECK_FALSE(std::is_assignable_v<decltype((dest->value)), int>);

      // Just to confirm the checks above what we're trying to check
      mutable_test_type mutable_source {new WithTrackedDestructor(value)};
      STATIC_CHECK(
        std::is_assignable_v<decltype((mutable_source->value)), int>);
    }
  }

  SECTION("move-construct from non-const") {
    Tracker::reset();
    constexpr auto value = __LINE__;
    {
      mutable_test_type source {new WithTrackedDestructor(value)};
      const_test_type dest = std::move(source);
      CHECK(Tracker::call_count == 0);
      CHECK(dest->value == value);
    }
    CHECK(Tracker::call_count == 1);
    CHECK(Tracker::last_value == value);
  }

  SECTION("move-assign from non-const") {
    Tracker::reset();
    constexpr auto value = __LINE__;
    {
      mutable_test_type source {new WithTrackedDestructor(value)};
      const_test_type dest {nullptr};
      dest = std::move(source);
      CHECK(Tracker::call_count == 0);
      CHECK(dest->value == value);
    }
    CHECK(Tracker::call_count == 1);
    CHECK(Tracker::last_value == value);
  }
}

TEST_CASE("unique_any - optional-backed storage") {
  struct value_type : felly::non_copyable {
    constexpr value_type() = default;
    constexpr explicit value_type(const int value) : value {value} {}
    int value {};
    constexpr bool operator==(const value_type&) const noexcept = default;
    constexpr bool operator==(const int other) const noexcept {
      return value == other;
    }
  };

  using test_type = felly::unique_any<value_type, [](value_type& p) {
    Tracker::track(p.value);
  }>;
  using const_test_type =
    felly::unique_any<const value_type, [](value_type& p) {
      Tracker::track(p.value);
    }>;
  using delete_by_address_test_type =
    felly::unique_any<value_type, [](value_type* p) {
      Tracker::track(p->value);
    }>;

  SECTION("size") {
    STATIC_CHECK(sizeof(test_type) == sizeof(std::optional<value_type>));
  }

  SECTION("is-valid") {
    CHECK_FALSE(test_type {std::nullopt});
    CHECK(test_type {std::in_place, 1});
  }
  SECTION("deleter not called for invalid") {
    Tracker::reset();
    std::ignore = test_type {std::nullopt};
    CHECK(Tracker::call_count == 0);
  }

  SECTION("deleter called for valid") {
    Tracker::reset();
    constexpr auto value = __LINE__;
    std::ignore = test_type {std::in_place, value};
    CHECK(Tracker::call_count == 1);
    CHECK(Tracker::last_value == value);
  }

  SECTION("operator&") {
    constexpr auto v1 = __LINE__;
    constexpr auto v2 = __LINE__;
    auto u = test_type {std::in_place, v1};
    auto p = &u;
    CHECK(u->value == v1);
    CHECK(p->value == v1);
    STATIC_CHECK(std::is_pointer_v<decltype(p)>);
    p->value = v2;
    CHECK(u->value == v2);
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

  SECTION("default in-place construction") {
    Tracker::reset();
    {
      const auto u = test_type {std::in_place};
      CHECK(u);
      CHECK(Tracker::call_count == 0);
    }
    CHECK(Tracker::call_count == 1);
    CHECK(Tracker::last_value == 0);
  }

  SECTION("get") {
    constexpr auto value = __LINE__;
    auto u = test_type {std::in_place, value};
    CHECK(u.get().value == value);

    using TGet = decltype(u.get());
    STATIC_CHECK(std::same_as<TGet, value_type&>);
    u.get().value = 123;
  }

  SECTION("disown") {
    constexpr auto value = __LINE__;
    const auto ret = test_type {std::in_place, value}.disown();
    STATIC_CHECK(std::same_as<value_type, std::remove_cvref_t<decltype(ret)>>);
    CHECK(ret.value == value);
  }

  SECTION("deleter not called for invalid") {
    Tracker::reset();
    std::ignore = test_type {std::nullopt};
    CHECK(Tracker::call_count == 0);
  }
  SECTION("deleter called for valid") {
    Tracker::reset();
    constexpr auto value = __LINE__;
    std::ignore = test_type {std::in_place, value};
    CHECK(Tracker::call_count == 1);
    CHECK(Tracker::last_value == value);
  }

  SECTION("moveable to const") {
    Tracker::reset();
    constexpr auto value1 = __LINE__;
    constexpr auto value2 = __LINE__;
    {
      auto orig = test_type {std::in_place, value1};
      CHECK(orig);
      CHECK(orig->value == value1);
      auto moved_mutable = std::move(orig);
      CHECK_FALSE(orig);
      CHECK(moved_mutable);
      CHECK(moved_mutable->value == value1);
      moved_mutable->value = value2;
      const_test_type moved_const = std::move(moved_mutable);
      CHECK_FALSE(moved_mutable);
      CHECK(moved_const);

      STATIC_CHECK(!std::is_assignable_v<decltype((moved_const->value)), int>);
      STATIC_CHECK(std::is_assignable_v<decltype((moved_mutable->value)), int>);
      CHECK(Tracker::call_count == 0);
    }
    CHECK(Tracker::call_count == 1);
    CHECK(Tracker::last_value == value2);
  }

  SECTION("cleanup by address") {
    Tracker::reset();
    constexpr auto value = __LINE__;
    {
      delete_by_address_test_type u {std::in_place, value};
      CHECK(u.get().value == value);
      CHECK(Tracker::call_count == 0);
    }
    CHECK(Tracker::call_count == 1);
    CHECK(Tracker::last_value == value);
  }
}

TEST_CASE("unique_any - void*") {
  // void* and aliases are frequently used as opaque handles
  using test_type = felly::unique_any<void* const, [](void* const p) {
    Tracker::track(felly::numeric_cast<int>(std::bit_cast<intptr_t>(p)));
  }>;

  SECTION("basic behavior") {
    Tracker::reset();
    CHECK_FALSE(test_type {nullptr});
    CHECK(Tracker::call_count == 0);
    constexpr auto value = __LINE__;
    CHECK(test_type {std::bit_cast<void*>(static_cast<intptr_t>(value))});
    CHECK(Tracker::call_count == 1);
    CHECK(Tracker::last_value == value);
  }
}
struct init_test_traits : felly::unique_any_optional_storage_traits<
                            WithTrackedDestructor,
                            [](auto...) {}> {
  static constexpr auto test_value = __LINE__;
  template <class... Args>
  static constexpr void construct(
    std::optional<WithTrackedDestructor>& storage,
    Args&&... args) {
    if constexpr (sizeof...(args) == 0) {
      storage.emplace(test_value);
    } else {
      storage.emplace(std::forward<Args>(args)...);
    }
  }
};

TEST_CASE("unique-any - custom init function") {
  using test_type = felly::basic_unique_any<init_test_traits>;
  {
    Tracker::reset();
    const test_type u {std::in_place};
    CHECK(u);
    CHECK(u->value == init_test_traits::test_value);
    CHECK(Tracker::call_count == 0);
  }
  CHECK(Tracker::call_count == 1);
  CHECK(Tracker::last_value == init_test_traits::test_value);
}
