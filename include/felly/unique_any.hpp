// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <compare>
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace felly_detail {

template <class From, class To>
concept const_promotion_to = (!std::same_as<To, From>)
  && std::same_as<To,
                  std::conditional_t<
                    std::is_pointer_v<From>,
                    const std::remove_pointer_t<From>*,
                    const std::remove_pointer_t<From>>>;

template <class T, auto TDeleter>
struct invoke_as_deleter {
  // Allow `c_deleter(T *)` for a `const T*`
  static void operator()(T x)
    requires std::is_pointer_v<T>
    && std::invocable<
               decltype(TDeleter),
               std::remove_const_t<std::remove_pointer_t<T>>*>
  {
    std::invoke(
      TDeleter, const_cast<std::remove_const_t<std::remove_pointer_t<T>>*>(x));
  }

  static void operator()(T& x)
    requires(!std::is_pointer_v<T>) && std::invocable<decltype(TDeleter), T&>
  {
    std::invoke(TDeleter, x);
  }

  static void operator()(T& x)
    requires(!std::is_pointer_v<T>) && std::invocable<decltype(TDeleter), T*>
    && (!std::invocable<decltype(TDeleter), T&>)
  {
    std::invoke(TDeleter, std::addressof(x));
  }
};

template <auto TDeleter, class T>
concept invocable_as_deleter = std::invocable<invoke_as_deleter<T, TDeleter>, T>
  || std::invocable<invoke_as_deleter<T, TDeleter>, T&>;

// clang++18 considers `std::forward_like` here to be a use-before-defined
template <class T, class U>
constexpr decltype(auto) unique_any_forward_like(U&& u) {
  using return_type = std::conditional_t<
    std::is_const_v<std::remove_reference_t<T>>,
    const std::remove_reference_t<U>,
    std::remove_reference_t<U>>;

  if constexpr (std::is_lvalue_reference_v<T>) {
    return static_cast<return_type&>(u);
  } else {
    return static_cast<return_type&&>(u);
  }
}

}// namespace felly_detail

namespace felly::inline unique_any_types {

// Storage for any type using std::optional
template <class T>
class unique_any_optional_storage {
  std::optional<T> storage;

 public:
  [[nodiscard]] constexpr bool has_value() const noexcept {
    return storage.has_value();
  }
  template <class Self>
  [[nodiscard]] constexpr decltype(auto) value(this Self&& self) {
    return felly_detail::unique_any_forward_like<Self>(self.storage.value());
  }

  constexpr void reset() noexcept { storage.reset(); }

  template <class... Args>
  constexpr void emplace(Args&&... args) {
    storage.emplace(std::forward<Args>(args)...);
  }

  friend constexpr auto operator<=>(
    const unique_any_optional_storage& lhs,
    const unique_any_optional_storage& rhs) noexcept
    requires std::three_way_comparable<T>
  {
    return lhs.storage <=> rhs.storage;
  }

  constexpr bool operator==(const unique_any_optional_storage&) const noexcept =
    default;
};

/** Storage for values that can contain their own empty state, such as pointers.
 *
 * This is a space-saving alternative to `unique_any_optional_storage`
 */
template <class T>
class unique_any_direct_storage {
  T storage {};

 public:
  [[nodiscard]] constexpr bool has_value() const noexcept { return true; }
  template <class Self>
  [[nodiscard]] constexpr decltype(auto) value(this Self&& self) noexcept {
    return felly_detail::unique_any_forward_like<Self>(self.storage);
  }
  constexpr void reset() noexcept {
    if constexpr (std::is_trivially_copyable_v<T>) {
      storage = {};
    } else {
      std::destroy_at(&storage);
      std::construct_at(&storage);
    }
  }

  template <std::convertible_to<T> U>
  constexpr void emplace(U&& value) noexcept {
    if constexpr (std::is_trivially_assignable_v<T&, U&&>) {
      storage = std::forward<U>(value);
    } else {
      std::destroy_at(&storage);
      std::construct_at(&storage, std::forward<U>(value));
    }
  }

  friend constexpr auto operator<=>(
    const unique_any_direct_storage&,
    const unique_any_direct_storage&) noexcept = default;
  constexpr bool operator==(const unique_any_direct_storage&) const noexcept =
    default;
};

/** Specializable-class to for an simplified `std::optional`-like container
 * for `unique_any` to use to store a T.
 *
 * You may specialize this for your own types.
 */
template <class T>
struct unique_any_storage : unique_any_optional_storage<T> {};
template <class T>
  requires std::is_pointer_v<T>
struct unique_any_storage<T> : unique_any_direct_storage<T> {};

template <typename S, typename T>
concept unique_any_storage_for = requires(S& s, S&& rs, const S& cs) {
  { cs.has_value() } -> std::convertible_to<bool>;
  { s.reset() } -> std::same_as<void>;
  { s.emplace(std::declval<T>()) } -> std::same_as<void>;
  { s.value() } -> std::convertible_to<T&>;
  { cs.value() } -> std::convertible_to<const T&>;
  { std::move(s).value() } -> std::convertible_to<T&&>;
};

template <typename T>
concept unique_any_traits =
  requires {
    typename T::value_type;
    typename T::storage_type;
  }
  && unique_any_storage_for<
    typename T::storage_type,
    std::remove_const_t<typename T::value_type>>
  && requires(
    typename T::storage_type& storage,
    typename T::value_type& value,
    const typename T::value_type& c_value) {
       { T::destroy(value) } -> std::same_as<void>;
       { T::has_value(c_value) } -> std::convertible_to<bool>;
       { T::emplace(storage, std::move(value)) } -> std::same_as<void>;
     };

template <
  class T,
  auto Deleter,
  std::predicate<T> auto Predicate = std::identity {},
  class TStorage = unique_any_storage<std::remove_const_t<T>>>
  requires felly_detail::invocable_as_deleter<Deleter, T>
struct basic_unique_any_traits {
  using value_type = T;
  using storage_type = TStorage;

  template <class U>
  static constexpr void destroy(U&& value) {
    felly_detail::invoke_as_deleter<T, Deleter> {}(std::forward<U>(value));
  }

  template <class U>
  static constexpr bool has_value(U&& value) noexcept {
    return static_cast<bool>(std::invoke(Predicate, std::forward<U>(value)));
  }

  template <class... Args>
  static constexpr void emplace(storage_type& storage, Args&&... value) {
    storage.emplace(std::forward<Args>(value)...);
  }
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
template <unique_any_traits TTraits>
struct basic_unique_any {
  using value_type = TTraits::value_type;
  using storage_type = TTraits::storage_type;

  basic_unique_any() = delete;
  basic_unique_any(const basic_unique_any&) = delete;
  basic_unique_any& operator=(const basic_unique_any&) = delete;

  constexpr explicit basic_unique_any(std::nullopt_t) noexcept {}

  template <std::convertible_to<value_type> U>
  explicit constexpr basic_unique_any(U&& value) {
    if (TTraits::has_value(value)) {
      TTraits::emplace(storage, std::forward<U>(value));
    }
  }

  template <class... Args>
    requires(!std::is_pointer_v<value_type>)
  explicit constexpr basic_unique_any(std::in_place_t, Args&&... args) {
    TTraits::emplace(storage, std::forward<Args>(args)...);
    if (!TTraits::has_value(storage.value())) {
      storage.reset();
    }
  }
  constexpr basic_unique_any(basic_unique_any&& other) noexcept {
    *this = std::move(other);
  }

  template <class UTraits>
    requires felly_detail::
      const_promotion_to<typename UTraits::value_type, value_type>
    constexpr basic_unique_any(basic_unique_any<UTraits>&& other) noexcept {
    if (other) {
      TTraits::emplace(storage, other.disown());
    }
  }

  constexpr void reset() noexcept {
    if (has_value()) {
      TTraits::destroy(storage.value());
    }
    storage.reset();
  }

  template <std::convertible_to<value_type> U>
  constexpr void reset(U&& u) {
    reset();
    TTraits::emplace(storage, std::forward<U>(u));
  }

  // Like std::unique_ptr's `release()`, but more clearly named
  [[nodiscard]]
  constexpr value_type disown() {
    require_value();
    auto ret = std::move(storage.value());
    storage.reset();
    return std::move(ret);
  }

  constexpr basic_unique_any& operator=(basic_unique_any&& other) noexcept {
    if (this == std::addressof(other)) {
      return *this;
    }

    reset();
    storage = std::exchange(other.storage, storage_type {});
    return *this;
  }

  template <class UTraits>
  constexpr basic_unique_any& operator=(
    basic_unique_any<UTraits>&& other) noexcept
    requires felly_detail::
      const_promotion_to<typename UTraits::value_type, value_type>
  {
    static_assert(!std::same_as<value_type, typename UTraits::value_type>);

    reset();
    if (other) {
      TTraits::emplace(storage, other.disown());
    }
    return *this;
  }

  ~basic_unique_any() { reset(); }

  /** Always-const, as we don't want to allow changing what the deleter needs to
   * do.
   *
   * For example, `some_unique_fd.get() = -1;` could leak
   *
   * For values, use `get()`, `reset()`, and `disown()` instead
   * For objects, use `operator->` instead
   */
  [[nodiscard]]
  constexpr decltype(auto) get() const {
    require_value();
    return std::as_const(storage.value());
  }

  [[nodiscard]]
  constexpr decltype(auto) get()
    requires(!std::is_const_v<value_type>)
  {
    require_value();
    return storage.value();
  }

  [[nodiscard]]
  constexpr decltype(auto) operator&(this auto&& self) {
    // I initially deleted operator& for pointers; I changed to allowing it
    // so that `basic_unique_any` works consistently with varies-by-platform
    // types like `locale_t`, which is int-like on some platforms, but a
    // pointer (void*) on others
    return std::addressof(self.get());
  }

  [[nodiscard]]
  constexpr decltype(auto) operator*(this auto&& self) {
    return self.get();
  }

  constexpr decltype(auto) operator->(this auto&& self) {
    self.require_value();

    if constexpr (std::is_pointer_v<value_type>) {
      return self.get();
    } else {
      return &self.get();
    }
  }

  constexpr operator bool() const noexcept { return has_value(); }

  [[nodiscard]]
  friend constexpr auto operator<=>(
    const basic_unique_any& lhs,
    const basic_unique_any& rhs) noexcept
    requires std::three_way_comparable<value_type>
  = default;

  [[nodiscard]]
  constexpr bool operator==(const basic_unique_any&) const noexcept = default;

  [[nodiscard]]
  constexpr bool operator==(const value_type& other) const noexcept
    requires std::equality_comparable<value_type>
  {
    if (!has_value()) {
      return !TTraits::has_value(other);
    }

    return (storage.value() == other);
  }

 private:
  storage_type storage;

  [[nodiscard]]
  constexpr bool has_value() const noexcept {
    return storage.has_value() && TTraits::has_value(storage.value());
  }

  constexpr void require_value() const {
    if (!has_value()) [[unlikely]] {
      throw std::logic_error("Can't access a moved or invalid value");
    }
  }
};

template <class T, auto TDeleter, auto TPredicate = std::identity {}>
using unique_any =
  basic_unique_any<basic_unique_any_traits<T, TDeleter, TPredicate>>;

}// namespace felly::inline unique_any_types
