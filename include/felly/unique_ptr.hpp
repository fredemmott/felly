// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include "unique_any.hpp"

#include <tuple>

namespace felly::inline unique_ptr_types {

/** Optional specialization of `felly::unique_any` for pointers.
 *
 * `felly::unique_any` works fine for pointers; this subclass:
 * - adds a default (empty/nullptr) constructor
 * - supports `std::out_ptr` and `std::inout_ptr`
 */
template <unique_any_traits TTraits>
  requires requires {
    typename TTraits::value_type;
    requires std::is_pointer_v<typename TTraits::value_type>;
  }
struct basic_unique_ptr : basic_unique_any<TTraits> {
  using pointer = std::remove_const_t<typename TTraits::value_type>;
  using element_type = std::remove_pointer_t<pointer>;

  using basic_unique_any<TTraits>::basic_unique_any;

  constexpr basic_unique_ptr() : basic_unique_any<TTraits> {std::nullopt} {}
};

template <
  class T,
  auto TDeleter = unique_any_default_delete<T* const>,
  felly_detail::nullptr_or_predicate<const std::remove_pointer_t<T>*> auto
    TPredicate = nullptr>
using unique_ptr =
  basic_unique_ptr<unique_any_default_traits<T* const, TDeleter, TPredicate>>;

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
template <class TTraits, class Pointer, class... Args>
class inout_ptr_t<felly::basic_unique_ptr<TTraits>, Pointer, Args...> {
  using Smart = felly::basic_unique_ptr<TTraits>;
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
