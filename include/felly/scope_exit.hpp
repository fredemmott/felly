// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <concepts>
#include <exception>
#include <functional>
#include <type_traits>

namespace felly::detail {

struct empty {};

enum class basic_scope_exit_execution_policy {
  Always,
  OnFailure,
  OnSuccess,
};

// Roughly equivalent to `std::experimental::scope_exit`, but that isn't wideley
// available yet
template <basic_scope_exit_execution_policy TWhen, std::invocable<> TCallback>
class basic_scope_exit final {
 private:
  using enum basic_scope_exit_execution_policy;
#ifdef _MSC_VER
  [[msvc::no_unique_address]]
#else
  [[no_unique_address]]
#endif
  std::conditional_t<TWhen != Always, int, empty> mInitialUncaught {};

  std::remove_cvref_t<TCallback> mCallback;

 public:
  constexpr basic_scope_exit(TCallback&& f)
    : mCallback(std::forward<TCallback>(f)) {
    if constexpr (TWhen != Always) {
      mInitialUncaught = std::uncaught_exceptions();
    }
  }

  constexpr ~basic_scope_exit() noexcept
  {
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
  basic_scope_exit(basic_scope_exit&&) noexcept = delete;
  basic_scope_exit& operator=(const basic_scope_exit&) = delete;
  basic_scope_exit& operator=(basic_scope_exit&&) noexcept = delete;
};
}// namespace felly::detail

namespace felly::inline scope_exit_types {
template <class T = std::function<void()>>
using scope_exit
  = detail::basic_scope_exit<detail::basic_scope_exit_execution_policy::Always, T>;
template <class T = std::function<void()>>
using scope_fail = 
  detail::basic_scope_exit<detail::basic_scope_exit_execution_policy::OnFailure, T>;
template <class T = std::function<void()>>
using scope_success = 
  detail::basic_scope_exit<detail::basic_scope_exit_execution_policy::OnSuccess, T>;
}// namespace felly
