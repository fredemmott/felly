// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#include <catch2/catch_test_macros.hpp>
#include <felly/unique_ptr.hpp>

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

struct MyType {
  int value {};
  ~MyType() {
    Tracker::call_count++;
    Tracker::last_value = value;
  }
};

}// namespace

// Not duplicating the unique_any<T*> tests - just testing the add-ons
TEST_CASE("unique_ptr") {
  using test_type = felly::unique_ptr<MyType, std::default_delete<MyType> {}>;
  using const_test_type =
    felly::unique_ptr<const MyType, std::default_delete<MyType> {}>;
  SECTION("static checks") {
    STATIC_CHECK(
      std::derived_from<
        test_type,
        felly::unique_any<MyType* const, std::default_delete<MyType> {}>>);
    STATIC_CHECK(sizeof(test_type) == sizeof(void*));
  }
  SECTION("get() is reference to const pointer") {
    constexpr auto value = __LINE__;
    {
      test_type v {new MyType(value)};
      using U = decltype(v.get());
      STATIC_CHECK(std::same_as<U, MyType* const&>);
    }
    {
      const_test_type v {new MyType(value)};
      using U = decltype(v.get());
      STATIC_CHECK(std::same_as<U, const MyType* const&>);
    }
  }

  SECTION("operator& is pointer to immutable pointer") {
    constexpr auto value = __LINE__;
    {
      test_type v {new MyType(value)};
      using U = decltype(&v);
      STATIC_CHECK(std::same_as<U, MyType* const*>);
    }
    {
      const_test_type v {new MyType(value)};
      using U = decltype(&v);
      STATIC_CHECK(std::same_as<U, const MyType* const*>);
    }
  }

  SECTION("std::out_ptr") {
    Tracker::reset();
    constexpr auto value = __LINE__;
    {
      test_type v {std::nullopt};
      [](MyType** pp) { *pp = new MyType(value); }(std::out_ptr(v));
      CHECK(v);
      CHECK(v->value == value);
      CHECK(Tracker::call_count == 0);
    }
    CHECK(Tracker::call_count == 1);
    CHECK(Tracker::last_value == value);
  }

  SECTION("std::inout_ptr") {
    Tracker::reset();
    constexpr auto v1 = __LINE__;
    constexpr auto v2 = __LINE__;
    {
      test_type v {std::nullopt};
      [](MyType** pp) { *pp = new MyType(v1); }(std::inout_ptr(v));
      CHECK(v);
      CHECK(v->value == v1);
    }
    CHECK(Tracker::call_count == 1);
    CHECK(Tracker::last_value == v1);
    Tracker::reset();
    {
      test_type v {new MyType(v1)};
      [](MyType** pp) {
        REQUIRE(pp);
        REQUIRE(*pp);

        CHECK((*pp)->value == v1);
        delete *pp;
        *pp = new MyType(v2);
      }(std::inout_ptr(v));
      CHECK(Tracker::call_count == 1);
      CHECK(Tracker::last_value == v1);
      CHECK(v);
      CHECK(v->value == v2);
    }
    CHECK(Tracker::call_count == 2);
    CHECK(Tracker::last_value == v2);
  }
}
