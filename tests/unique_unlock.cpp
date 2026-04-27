// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: BSL-1.0

#include <catch2/catch_test_macros.hpp>
#include <felly/unique_unlock.hpp>

#include <mutex>

TEST_CASE("unique_unlock") {
  std::mutex m;
  std::unique_lock lock {m};
  CHECK(lock.owns_lock());
  {
    const felly::unique_unlock unlock {lock};
    CHECK(unlock.owns_lock());
    CHECK_FALSE(lock.owns_lock());
  }
  CHECK(lock.owns_lock());

  SECTION("move-construct") {
    felly::unique_unlock a {lock};
    CHECK(a.owns_lock());
    felly::unique_unlock b {std::move(a)};
    CHECK(b.owns_lock());
    CHECK_FALSE(a.owns_lock());
    CHECK_FALSE(lock.owns_lock());
  }
  CHECK(lock.owns_lock());

  SECTION("move-assign") {
    felly::unique_unlock a {lock};
    felly::unique_unlock b {std::move(a)};
    a = std::move(b);
    CHECK(a.owns_lock());
    CHECK_FALSE(b.owns_lock());
    CHECK_FALSE(lock.owns_lock());
  }

  CHECK(lock.owns_lock());

  SECTION("move-assign over valid lock") {
    std::mutex mutexB;
    std::unique_lock lockB {mutexB};

    felly::unique_unlock a {lock};
    felly::unique_unlock b {lockB};

    a = std::move(b);
    CHECK(lock.owns_lock());
    CHECK_FALSE(lockB.owns_lock());
    CHECK(a.owns_lock());
    CHECK_FALSE(b.owns_lock());
  }

  CHECK(lock.owns_lock());
}
