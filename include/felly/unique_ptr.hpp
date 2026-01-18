// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include "unique_any.hpp"

namespace felly::inline unique_ptr_types {

/** Optional specialization of `felly::unique_any` for pointers.
 *
 * `felly::unique_any` works fine for pointers; this subclass:
 * - adds a default (empty/nullptr) constructor
 * - supports `std::out_ptr` and `std::inout_ptr`
 */
template <class T, auto TDeleter, auto TPredicate = std::identity {}>
struct unique_ptr : unique_any<T*, TDeleter, TPredicate> {
  using pointer = T*;
  using element_type = T;

  using unique_any<T*, TDeleter, TPredicate>::unique_any;

  constexpr unique_ptr()
    : unique_any<T*, TDeleter, TPredicate> {std::nullopt} {}
};
}// namespace felly::inline unique_ptr_types

namespace std {
/* Support `std::inout_ptr`
 *
 * This specialization is needed because we use `disown()` instead of
 * `release()`.
 *
 * A specialization isn't needed for `out_ptr_t` because `std::out_ptr` doesn't
 * use `release()`
 */
template <class T, auto TDeleter, auto TPredicate, class Pointer, class... Args>
class inout_ptr_t<
  felly::unique_ptr<T, TDeleter, TPredicate>,
  Pointer,
  Args...> {
  using Smart = felly::unique_ptr<T, TDeleter, TPredicate>;
  Smart& smart;
  Pointer ptr {};
  std::tuple<Args...> args;

 public:
  explicit constexpr inout_ptr_t(Smart& smart, Args&&... args) noexcept
    : smart(smart),
      args(std::forward<Args>(args)...) {
    if (smart) {
      ptr = smart.disown();
    }
  }
  inout_ptr_t(const inout_ptr_t&) = delete;

  constexpr ~inout_ptr_t() {
    std::apply(
      [this]<typename... Ts>(Ts&&... resetArgs) {
        smart.reset(ptr, std::forward<Ts>(resetArgs)...);
      },
      std::move(args));
  }

  constexpr operator Pointer*() noexcept { return std::addressof(ptr); }

  constexpr operator void**() noexcept
    requires(!std::same_as<Pointer, void*>)
  {
    return reinterpret_cast<void**>(std::addressof(ptr));
  }
};
}// namespace std
