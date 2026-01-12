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

  STATIC_CHECK(sizeof(felly::scope_exit([] {})) == sizeof([] {}));
}

TEST_CASE("scope_exit") {
  SECTION("on normal scope exit") {
    bool flag = false;
    {
      const felly::scope_exit guard {[&flag] { flag = true; }};
      CHECK_FALSE(flag);
    }
    CHECK(flag);
  }

  SECTION("on exception") {
    bool flag = false;
    try {
      const felly::scope_exit guard {[&flag] { flag = true; }};
      CHECK_FALSE(flag);
      throw test_exception {};
    } catch (const test_exception&) {
      CHECK(flag);
    }
    CHECK(flag);
  }
}

TEST_CASE("scope_success") {
  SECTION("on normal scope exit") {
    bool flag = false;
    {
      const felly::scope_success guard {[&flag] { flag = true; }};
      CHECK_FALSE(flag);
    }
    CHECK(flag);
  }

  SECTION("on exception") {
    bool flag = false;
    try {
      const felly::scope_success guard {[&flag] { flag = true; }};
      CHECK_FALSE(flag);
      throw test_exception {};
    } catch (const test_exception&) {
      CHECK_FALSE(flag);
    }
    CHECK_FALSE(flag);
  }

  SECTION("in successful exception handler") {
    bool flag = false;
    try {
      throw test_exception();
    } catch (const test_exception&) {
      const felly::scope_success guard {[&flag] { flag = true; }};
    }
    CHECK(flag);
  }

  SECTION("in re-throwing exception handler") {
    bool flag = false;
    try {
      try {
        throw test_exception();
      } catch (const test_exception&) {
        const felly::scope_success guard {[&flag] { flag = true; }};
        throw;
      }
    } catch (const test_exception&) {
    }
    CHECK_FALSE(flag);
  }
}

TEST_CASE("scope_fail") {
  SECTION("on normal scope exit") {
    bool flag = false;
    {
      const felly::scope_fail guard {[&flag] { flag = true; }};
      CHECK_FALSE(flag);
    }
    CHECK_FALSE(flag);
  }

  SECTION("on exception") {
    bool flag = false;
    try {
      const felly::scope_fail guard {[&flag] { flag = true; }};
      CHECK_FALSE(flag);
      throw test_exception {};
    } catch (const test_exception&) {
      CHECK(flag);
    }
    CHECK(flag);
  }

  SECTION("in successful exception handler") {
    bool flag = false;
    try {
      throw test_exception();
    } catch (const test_exception&) {
      const felly::scope_fail guard {[&flag] { flag = true; }};
    }
    CHECK_FALSE(flag);
  }

  SECTION("in re-throwing exception handler") {
    bool flag = false;
    try {
      try {
        throw test_exception();
      } catch (const test_exception&) {
        const felly::scope_fail guard {[&flag] { flag = true; }};
        CHECK_FALSE(flag);
        throw;
      }
    } catch (const test_exception&) {
    }
    CHECK(flag);
  }
}
