// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <compare>
#include <concepts>
#include <functional>
#include <optional>
#include <stdexcept>
#include <utility>

namespace felly::inline unique_any_types {
/** like `unique_ptr`, but works with `void*` and non-pointer types.
 *
 * For example:
 * - `locale_t` is not guaranteed to be a pointer
 * - even when it is, it can be `void*` (e.g. on macos)
 *
 * A predicate is taken instead of an invalid value because some libraries
 * (e.g. iconv, Win32 file HANDLEs) use `(some_ptr) -1` as the sentinel value.
 * Casting -1 to a pointer is never valid in constexpr, so we need this slightly
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

  constexpr unique_any(T value) {
    if (TPredicate(value)) {
      mValue = std::move(value);
    }
  }

  template <class... Args>
  constexpr unique_any(std::in_place_t, Args&&... args) {
    mValue = T {std::forward<Args>(args)...};
    if (!TPredicate(*mValue)) {
      mValue.reset();
    }
  }

  constexpr unique_any(unique_any&& other) noexcept {
    *this = std::move(other);
  }

  constexpr unique_any& operator=(unique_any&& other) noexcept {
    if (this == std::addressof(other)) {
      return *this;
    }

    if (mValue) {
      std::invoke(TDeleter, *mValue);
    }
    mValue = std::exchange(other.mValue, std::nullopt);
    return *this;
  }

  ~unique_any() {
    if (*this) {
      std::invoke(TDeleter, *mValue);
    }
  }

  template <class Self>
  [[nodiscard]]
  constexpr decltype(auto) get(this Self&& self) {
    if (!self.mValue) {
      throw std::logic_error("Can't access a moved value");
    }
    return std::forward_like<Self>(self.mValue);
  }

  template <class Self>
  constexpr decltype(auto) operator->(this Self&& self) {
    if (!self.mValue) {
      throw std::logic_error("Can't access a moved value");
    }

    if constexpr (std::is_pointer_v<T>) {
      return std::forward_like<Self>(*self.mValue);
    } else {
      return std::forward_like<Self>(&*self.mValue);
    }
  }

  constexpr operator bool() const noexcept {
    return (mValue) && TPredicate(*mValue);
  }

  [[nodiscard]]
  friend constexpr auto operator<=>(
    const unique_any& lhs,
    const unique_any& rhs) noexcept
    requires std::three_way_comparable_with<T, T, std::strong_ordering>
  = default;

  [[nodiscard]]
  constexpr bool operator==(const unique_any&) const noexcept = default;

  [[nodiscard]]
  constexpr bool operator==(const T& other) const noexcept
    requires std::equality_comparable<T>
  {
    if (!mValue) {
      return !TPredicate(other);
    }

    return (*mValue == other);
  }

 private:
  std::optional<T> mValue;
};

}// namespace felly::inline unique_any_types
