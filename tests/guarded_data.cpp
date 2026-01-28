// Copyright 2026 Fred Emmott<fred @fredemmott.com>
// SPDX-License-Identifier: MIT
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <thread>
#include <vector>

#include "felly/guarded_data.hpp"

using namespace felly::guarded_data_types;

TEST_CASE("guarded_data basic usage", "[guarded_data]") {
  SECTION("Initializes and allows access to data") {
    guarded_data<std::string> guarded("Hello World");

    auto locked = guarded.lock();
    CHECK(locked->length() == 11);
    CHECK(*locked == "Hello World");

    *locked = "Modified";
    CHECK(locked.get() == "Modified");
    CHECK(locked->length() == 8);
  }

  SECTION("Const access") {
    const guarded_data<int> guarded(42);

    auto locked = guarded.lock();
    CHECK(*locked == 42);
  }

  SECTION("Manual unlock releases the mutex") {
    guarded_data<int> guarded(100);
    auto locked = guarded.lock();
    CHECK(locked);

    locked.unlock();
    CHECK(!locked);

    // After manual unlock, the pointer is nullified in this implementation
    // Attempting to use operator-> would be UB, but we can verify the state
    // if we added a check, but typically we just verify it doesn't hang
    // when we try to lock it again.
    auto locked2 = guarded.lock();
    CHECK(locked2);
    CHECK(*locked2 == 100);
  }

  SECTION("Repeated manual unlock") {
    guarded_data<int> guarded(100);
    auto locked = guarded.lock();
    locked.unlock();
    CHECK(!locked);
    CHECK_THROWS(locked.unlock());
    CHECK(!locked);
  }
}

TEST_CASE("guarded_data thread safety", "[guarded_data]") {
  SECTION("Multiple threads can safely increment a value") {
    guarded_data<bool> flag(false);
    constexpr int Iterations = 10000;
    std::atomic<std::size_t> races = 0;

    auto increment_task = [&] {
      for (int i = 0; i < Iterations; ++i) {
        auto lock = flag.lock();
        if (*lock) ++races;
        *lock = true;
        std::this_thread::yield();
        if (!*lock) ++races;
        *lock = false;
      }
    };

    std::thread t1(increment_task);
    std::thread t2(increment_task);

    t1.join();
    t2.join();

    CHECK(races == 0);
  }
}

TEST_CASE("guarded_data move semantics", "[guarded_data]") {
  guarded_data<std::vector<int>> guarded {1, 2, 3};

  auto lock1 = guarded.lock();
  CHECK(lock1);
  CHECK(lock1->size() == 3);

  auto lock2 = std::move(lock1);
  CHECK(lock2);
  CHECK(!lock1);
  CHECK(lock2->size() == 3);

  SECTION("move to self") {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-move"
#endif
    lock2 = std::move(lock2);
#ifdef __clang__
#pragma clang diagnostic pop
#endif
    CHECK(lock2);
    CHECK(lock2->size() == 3);
  }

  SECTION("moved-from twice") {
    CHECK(!lock1);
    auto lock3 = std::move(lock1);
    CHECK(!lock3);
  }
}
