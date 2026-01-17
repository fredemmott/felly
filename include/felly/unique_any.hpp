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

template <class T>
class unique_any_storage {
  std::optional<T> storage;

 public:
  [[nodiscard]] constexpr bool has_value() const noexcept {
    return storage.has_value();
  }
  template <class Self>
  [[nodiscard]] constexpr decltype(auto) value(this Self&& self) {
    return std::forward_like<Self>(self.storage.value());
  }

  constexpr void reset() noexcept { storage.reset(); }

  template <class... Args>
  constexpr void emplace(Args&&... args) {
    storage.emplace(std::forward<Args>(args)...);
  }

  friend constexpr auto operator<=>(
    const unique_any_storage& lhs,
    const unique_any_storage& rhs) noexcept
    requires std::three_way_comparable<T>
  {
    return lhs.storage <=> rhs.storage;
  }

  constexpr bool operator==(const unique_any_storage&) const noexcept = default;
};

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
  using storage_type = unique_any_storage<T>;

  unique_any() = delete;
  unique_any(const unique_any&) = delete;
  unique_any& operator=(const unique_any&) = delete;

  template <std::convertible_to<T> U>
  explicit constexpr unique_any(U&& value) {
    if (TPredicate(value)) {
      storage.emplace(std::forward<U>(value));
    }
  }

  template <class... Args>
  explicit constexpr unique_any(std::in_place_t, Args&&... args) {
    storage.emplace(std::forward<Args>(args)...);
    if (!TPredicate(storage.value())) {
      storage.reset();
    }
  }

  constexpr unique_any(unique_any&& other) noexcept {
    *this = std::move(other);
  }

  constexpr unique_any& operator=(unique_any&& other) noexcept {
    if (this == std::addressof(other)) {
      return *this;
    }

    if (storage.has_value()) {
      std::invoke(TDeleter, storage.value());
    }
    storage = std::exchange(other.storage, storage_type {});
    return *this;
  }

  ~unique_any() {
    if (storage.has_value()) {
      std::invoke(TDeleter, storage.value());
    }
  }

  template <class Self>
  [[nodiscard]]
  constexpr decltype(auto) get(this Self&& self) {
    if (!self.storage.has_value()) {
      throw std::logic_error("Can't access a moved or invalid value");
    }
    return std::forward_like<Self>(self.storage.value());
  }

  template <class Self>
  constexpr decltype(auto) operator->(this Self&& self) {
    if (!self.storage) {
      throw std::logic_error("Can't access a moved or invalid value");
    }

    if constexpr (std::is_pointer_v<T>) {
      return std::forward_like<Self>(*self.storage);
    } else {
      return std::forward_like<Self>(&*self.storage);
    }
  }

  constexpr operator bool() const noexcept {
    // We never store an invalid value
    return storage.has_value();
  }

  [[nodiscard]]
  friend constexpr auto operator<=>(
    const unique_any& lhs,
    const unique_any& rhs) noexcept
    requires std::three_way_comparable<T>
  = default;

  [[nodiscard]]
  constexpr bool operator==(const unique_any&) const noexcept = default;

  [[nodiscard]]
  constexpr bool operator==(const T& other) const noexcept
    requires std::equality_comparable<T>
  {
    if (!storage.has_value()) {
      return !TPredicate(other);
    }

    return (storage.value() == other);
  }

 private:
  storage_type storage;
};

}// namespace felly::inline unique_any_types
