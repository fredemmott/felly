// Copyright 2026 Fred Emmott<fred @fredemmott.com>
// SPDX-License-Identifier: BSL-1.0
#include <catch2/catch_test_macros.hpp>
#include <condition_variable>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "felly/guarded_data.hpp"

using namespace felly::guarded_data_types;

// As of 2026-04-27, macOS's default standard library does not
// include `std::jthread`, at least with the versions on the GitHub Actions
// `macos-latest` runners.
//
// Minimal incomplete polyfill
struct jthread : std::thread {
  using std::thread::thread;
  ~jthread() {
    if (joinable()) {
      join();
    }
  }
};

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

TEST_CASE("guarded_data with condition_variable", "[guarded_data]") {
  using namespace std::chrono_literals;

  SECTION("wait with predicate") {
    guarded_data<bool> ready(false);
    std::condition_variable cv;

    auto lock = ready.lock();

    jthread t([&] {
      auto lock = ready.lock();
      *lock = true;
      cv.notify_one();
    });

    lock.wait(cv, [&] { return *lock; });
    CHECK(*lock);
  }

  SECTION("wait_for with predicate - success") {
    guarded_data<bool> ready(false);
    std::condition_variable cv;

    auto lock = ready.lock();

    jthread t {[&] {
      auto lock = ready.lock();
      *lock = true;
      cv.notify_one();
    }};

    const bool success = lock.wait_for(cv, 100ms, [&] { return *lock; });
    CHECK(success);
    CHECK(*lock);
  }

  SECTION("wait_for with predicate - failure") {
    guarded_data<bool> ready(false);
    std::condition_variable cv;

    auto lock = ready.lock();
    const bool success = lock.wait_for(cv, 1ms, [&] { return *lock; });
    CHECK_FALSE(success);
    CHECK_FALSE(*lock);
  }

  SECTION("wait_until with predicate - success") {
    guarded_data<bool> ready(false);
    std::condition_variable cv;

    auto lock = ready.lock();
    jthread j {[&] {
      auto lock = ready.lock();
      *lock = true;
      cv.notify_one();
    }};

    auto timeout = std::chrono::system_clock::now() + 100ms;
    const bool success = lock.wait_until(cv, timeout, [&] { return *lock; });
    CHECK(success);
  }

  SECTION("wait_until with predicate - failure") {
    guarded_data<bool> ready(false);
    std::condition_variable cv;

    auto lock = ready.lock();
    auto timeout = std::chrono::system_clock::now() + 1ms;
    bool success = lock.wait_until(cv, timeout, [&] { return *lock; });
    CHECK_FALSE(success);
  }

  // This also guards std::stop_source availabilityt
#ifdef __cpp_lib_jthread
  SECTION("std::condition_variable_any compatibility") {
    guarded_data<int> data(0);
    std::condition_variable_any cv;
    std::stop_source ss;

    jthread t([&] {
      std::this_thread::sleep_for(10ms);
      auto lock = data.lock();
      *lock = 42;
      cv.notify_one();
    });

    auto lock = data.lock();
    // Using the overload that returns bool via stop_token
    bool notified = lock.wait(cv, ss.get_token(), [&] { return *lock == 42; });

    CHECK(notified);
    CHECK(*lock == 42);
  }

  SECTION("stop_token interruption") {
    guarded_data<bool> ready(false);
    std::condition_variable_any cv;
    std::stop_source ss;
    bool notified = false;

    {
      jthread waiter(
        [&notified, &ready, &cv](std::stop_token st) {
          auto lock = ready.lock();
          // This should return false when ss.request_stop() is called
          const bool result = lock.wait(cv, st, [&] { return *lock == true; });
          // In this test, we expect it to return false because of the stop
          // request
          CHECK_FALSE(result);
          CHECK(st.stop_requested());
          notified = true;
        },
        ss.get_token());

      // Give the thread a moment to start waiting
      std::this_thread::sleep_for(100ms);
      ss.request_stop();
    }
    CHECK(notified);
  }
#endif
}
