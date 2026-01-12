// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#include <catch2/catch_test_macros.hpp>
#include <felly/scope_exit.hpp>

namespace {
struct test_exception {};
}// namespace

TEST_CASE("no exception count for scope_exit") {
  STATIC_CHECK(
    sizeof(felly::scope_exit([] {})) < sizeof(felly::scope_success([] {})));
  STATIC_CHECK(
    sizeof(felly::scope_fail([] {})) == sizeof(felly::scope_success([] {})));
}

TEST_CASE("scope_exit") {
  SECTION("on normal scope exit") {
    bool invoked = false;
    {
      const felly::scope_exit guard {[&] { invoked = true; }};
      CHECK_FALSE(invoked);
    }
    CHECK(invoked);
  }

  SECTION("on exception") {
    bool invoked = false;
    try {
      const felly::scope_exit guard {[&] { invoked = true; }};
      CHECK_FALSE(invoked);
      throw test_exception {};
    } catch (const test_exception&) {
      CHECK(invoked);
    }
    CHECK(invoked);
  }
}

TEST_CASE("scope_success") {
  SECTION("on normal scope exit") {
    bool invoked = false;
    {
      const felly::scope_success guard {[&] { invoked = true; }};
      CHECK_FALSE(invoked);
    }
    CHECK(invoked);
  }

  SECTION("on exception") {
    bool invoked = false;
    try {
      const felly::scope_success guard {[&] { invoked = true; }};
      CHECK_FALSE(invoked);
      throw test_exception {};
    } catch (const test_exception&) {
      CHECK_FALSE(invoked);
    }
    CHECK_FALSE(invoked);
  }

  SECTION("in successful exception handler") {
    bool invoked = false;
    try {
      throw test_exception();
    } catch (const test_exception&) {
      const felly::scope_success guard {[&] { invoked = true; }};
    }
    CHECK(invoked);
  }

  SECTION("in re-throwing exception handler") {
    bool invoked = false;
    try {
      try {
        throw test_exception();
      } catch (const test_exception&) {
        const felly::scope_success guard {[&] { invoked = true; }};
        throw;
      }
    } catch (const test_exception&) {
    }
    CHECK_FALSE(invoked);
  }
}

TEST_CASE("scope_fail") {
  SECTION("on normal scope exit") {
    bool invoked = false;
    {
      const felly::scope_fail guard {[&] { invoked = true; }};
      CHECK_FALSE(invoked);
    }
    CHECK_FALSE(invoked);
  }

  SECTION("on exception") {
    bool invoked = false;
    try {
      const felly::scope_fail guard {[&] { invoked = true; }};
      CHECK_FALSE(invoked);
      throw test_exception {};
    } catch (const test_exception&) {
      CHECK(invoked);
    }
    CHECK(invoked);
  }

  SECTION("in successful exception handler") {
    bool invoked = false;
    try {
      throw test_exception();
    } catch (const test_exception&) {
      const felly::scope_fail guard {[&] { invoked = true; }};
    }
    CHECK_FALSE(invoked);
  }

  SECTION("in re-throwing exception handler") {
    bool invoked = false;
    try {
      try {
        throw test_exception();
      } catch (const test_exception&) {
        const felly::scope_fail guard {[&] { invoked = true; }};
        CHECK_FALSE(invoked);
        throw;
      }
    } catch (const test_exception&) {
    }
    CHECK(invoked);
  }
}

TEST_CASE("scope_exit::release") {
  bool invoked = false;
  {
    auto guard = felly::scope_exit([&]{ invoked = true; });
    guard.release();
  }
  CHECK_FALSE(invoked);
  {
    auto guard = felly::scope_exit([&]{ invoked = true; });
    guard.release();
    // Intentionally duplicate the call
    guard.release();
  }
  CHECK_FALSE(invoked);
}

TEST_CASE("scope_exit move constructor") {
  int count = 0;
  {
    auto guard = felly::scope_exit([&]{ ++count; });
    {
      auto guard2 = std::move(guard);
    }
    CHECK(count == 1);
  }
  CHECK(count == 1);
}

TEST_CASE("scope_exit multiple moves") {
  int count = 0;
  {
    felly::scope_exit a([&]{ ++count; });
    auto b = std::move(a);
    auto c = std::move(b);
    CHECK(count == 0);
  }
  CHECK(count == 1);
}

TEST_CASE("scope_exit released then moved") {
  int count = 0;
  {
    felly::scope_exit a([&]{ ++count; });
    a.release();
    auto b = std::move(a);
    CHECK(count == 0);
  }
  CHECK(count == 0);
}