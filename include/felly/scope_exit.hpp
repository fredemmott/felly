// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include "no_unique_address.hpp"

#include <concepts>
#include <exception>
#include <functional>
#include <type_traits>
#include <utility>

namespace felly::detail {

struct empty {};

enum class basic_scope_exit_execution_policy {
  Always,
  OnFailure,
  OnSuccess,
};

// Roughly equivalent to `std::experimental::scope_exit`, but that isn't widely
// available yet
template <basic_scope_exit_execution_policy TWhen, std::invocable<> TCallback>
class [[nodiscard]] basic_scope_exit {
  using enum basic_scope_exit_execution_policy;
  FELLY_NO_UNIQUE_ADDRESS
  std::conditional_t<TWhen != Always, int, empty> mInitialUncaught {};

  std::remove_cvref_t<TCallback> mCallback;
  bool mOwned {true};

 public:
  constexpr basic_scope_exit(TCallback&& f)
    : mCallback(std::forward<TCallback>(f)) {
    if constexpr (TWhen != Always) {
      mInitialUncaught = std::uncaught_exceptions();
    }
  }

  constexpr ~basic_scope_exit() noexcept {
    if (!mOwned) {
      return;
    }

    if constexpr (TWhen == Always) {
      std::invoke(mCallback);
    } else if constexpr (TWhen == OnFailure) {
      if (std::uncaught_exceptions() > mInitialUncaught) {
        std::invoke(mCallback);
      }
    } else if constexpr (TWhen == OnSuccess) {
      if (std::uncaught_exceptions() == mInitialUncaught) {
        std::invoke(mCallback);
      }
    }
  }

  basic_scope_exit() = delete;
  basic_scope_exit(const basic_scope_exit&) = delete;
  basic_scope_exit& operator=(const basic_scope_exit&) = delete;
  basic_scope_exit(basic_scope_exit&& other) noexcept
    : mInitialUncaught(other.mInitialUncaught),
      mCallback(std::move(other.mCallback)),
      mOwned(std::exchange(other.mOwned, false)) {}

  /// If we currently own a callback, invoking it could be surprising here; it
  /// could also cause noexcept issues
  basic_scope_exit& operator=(basic_scope_exit&&) noexcept = delete;

  void release() noexcept { mOwned = false; }
};
}// namespace felly::detail

namespace felly::inline scope_exit_types {
template <std::invocable<> T>
struct scope_exit
  : detail::
      basic_scope_exit<detail::basic_scope_exit_execution_policy::Always, T> {};
template <class T>
scope_exit(T) -> scope_exit<T>;

template <std::invocable<> T>
struct scope_fail : detail::basic_scope_exit<
                      detail::basic_scope_exit_execution_policy::OnFailure,
                      T> {};
template <class T>
scope_fail(T) -> scope_fail<T>;

template <class T = std::function<void()>>
struct scope_success : detail::basic_scope_exit<
                         detail::basic_scope_exit_execution_policy::OnSuccess,
                         T> {};
template <class T>
scope_success(T) -> scope_success<T>;
}// namespace felly::inline scope_exit_types
