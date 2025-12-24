// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include "moved_flag.hpp"

#include <concepts>
#include <functional>
#include <stdexcept>
#include <utility>

namespace felly {
/** like `unique_ptr`, but works with `void*` and non-pointer types.
 *
 * For example:
 * - `locale_t` is not guaranteed to be a pointer
 * - even when it is, it can be `void*` (e.g. on macos)
 *
 * A predicate is taken instead of an invalid value because some libraries
 * (e.g. iconv) use `(some_ptr) -1` as the sentinel value. Casting -1
 * to a pointer is never valid in constexpr, so we need this slightly
 * more verbose API.
 */
template <
  class T,
  std::invocable<T> auto TDeleter,
  std::predicate<T> auto TPredicate = std::identity {}>
struct unique_any {
  unique_any() = delete;
  unique_any(const unique_any&) = delete;
  unique_any& operator=(const unique_any&) = delete;

  unique_any(T value) : mValue(value) {
  }

  constexpr unique_any(unique_any&& other) = default;
  constexpr unique_any& operator=(unique_any&& other) noexcept = default;

  ~unique_any() {
    if (*this) {
      std::invoke(TDeleter, mValue);
    }
  }

  template <class Self>
  [[nodiscard]]
  constexpr decltype(auto) get(this Self&& self) {
    if (self.mMoved) {
      throw std::logic_error("Can't access a moved value");
    }
    return std::forward_like<Self>(self.mValue);
  }

  template <class Self>
  constexpr decltype(auto) operator->(this Self&& self) {
    if (self.mMoved) {
      throw std::logic_error("Can't access a moved value");
    }

    if constexpr(std::is_pointer_v<T>) {
      return std::forward_like<Self>(self.mValue);
    } else {
      return std::forward_like<Self>(&self.mValue);
    }
  }

  operator bool() const noexcept {
    return (!mMoved) && TPredicate(mValue);
  }

 private:
  moved_flag mMoved;
  T mValue {};
};

}// namespace felly
