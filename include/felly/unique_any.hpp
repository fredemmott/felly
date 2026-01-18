// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <compare>
#include <concepts>
#include <functional>
#include <optional>
#include <stdexcept>
#include <tuple>
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
  static void operator()(T x)
    requires std::is_pointer_v<T> && std::invocable<decltype(TDeleter), T>
  {
    std::invoke(TDeleter, x);
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
    // clang++18 considers `std::forward_like` here to be a use-before-defined
    using result_type = std::conditional_t<
      std::is_lvalue_reference_v<Self>,
      std::add_lvalue_reference_t<std::conditional_t<
        std::is_const_v<std::remove_reference_t<Self>>,
        const T,
        T>>,
      std::add_rvalue_reference_t<std::conditional_t<
        std::is_const_v<std::remove_reference_t<Self>>,
        const T,
        T>>>;
    return static_cast<result_type>(self.storage.value());
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

/** Storage for any pointer, assuming that `nullptr` is invalid.
 *
 * If additional values are invalid, this should be handled by `unique_any`'s
 * `TPredicate`, and `unique_any` will take responsibility for handling it.
 *
 * The nullptr is only used as an space-saving equivalent to the
 * `std::optional`'s nullopt
 */
template <class T>
  requires std::is_pointer_v<T>
class unique_any_pointer_storage {
  T storage {};

 public:
  [[nodiscard]] constexpr bool has_value() const noexcept { return storage; }
  [[nodiscard]] constexpr T value() const noexcept { return storage; }
  constexpr void reset() noexcept { storage = nullptr; }
  constexpr void emplace(const T value) noexcept { storage = value; }

  friend constexpr auto operator<=>(
    const unique_any_pointer_storage&,
    const unique_any_pointer_storage&) noexcept = default;
  constexpr bool operator==(const unique_any_pointer_storage&) const noexcept =
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
struct unique_any_storage<T> : unique_any_pointer_storage<T> {};

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
  auto TDeleter,
  std::predicate<T> auto TPredicate = std::identity {}>
  requires felly_detail::invocable_as_deleter<TDeleter, T>
struct unique_any {
  using storage_type = unique_any_storage<T>;

  unique_any() = delete;
  unique_any(const unique_any&) = delete;
  unique_any& operator=(const unique_any&) = delete;

  constexpr explicit unique_any(std::nullopt_t) noexcept {}

  template <std::convertible_to<T> U>
  explicit constexpr unique_any(U&& value) {
    if (std::invoke(TPredicate, value)) {
      storage.emplace(std::forward<U>(value));
    }
  }

  template <class... Args>
    requires(!std::is_pointer_v<T>)
  explicit constexpr unique_any(std::in_place_t, Args&&... args) {
    storage.emplace(std::forward<Args>(args)...);
    if (!std::invoke(TPredicate, storage.value())) {
      storage.reset();
    }
  }
  constexpr unique_any(unique_any&& other) noexcept {
    *this = std::move(other);
  }

  template <
    felly_detail::const_promotion_to<T> U,
    auto UDeleter,
    auto UPredicate>
  constexpr unique_any(unique_any<U, UDeleter, UPredicate>&& other) noexcept {
    if (other) {
      storage.emplace(other.disown());
    }
  }

  constexpr void reset() noexcept {
    if (has_value()) {
      felly_detail::invoke_as_deleter<T, TDeleter> {}(storage.value());
    }
    storage.reset();
  }

  template <std::convertible_to<T> U>
  constexpr void reset(U&& u) {
    reset();
    storage.emplace(std::forward<U>(u));
  }

  // Like std::unique_ptr's `release()`, but more clearly named
  [[nodiscard]]
  constexpr T disown() {
    require_value();
    auto ret = std::move(storage.value());
    storage.reset();
    return std::move(ret);
  }

  constexpr unique_any& operator=(unique_any&& other) noexcept {
    if (this == std::addressof(other)) {
      return *this;
    }

    reset();
    storage = std::exchange(other.storage, storage_type {});
    return *this;
  }

  template <
    felly_detail::const_promotion_to<T> U,
    auto UDeleter,
    auto UPredicate>
  constexpr unique_any& operator=(
    unique_any<U, UDeleter, UPredicate>&& other) noexcept {
    static_assert(!std::same_as<T, U>);

    reset();
    if (other) {
      storage.emplace(other.disown());
    }
    return *this;
  }

  ~unique_any() { reset(); }

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
    return storage.value();
  }

  [[nodiscard]]
  constexpr const T& operator*() {
    return get();
  }

  template <class Self>
  constexpr decltype(auto) operator->(this Self&& self) {
    self.require_value();

    if constexpr (std::is_pointer_v<T>) {
      return self.storage.value();
    } else {
      return &self.storage.value();
    }
  }

  constexpr operator bool() const noexcept { return has_value(); }

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
    if (!has_value()) {
      return !std::invoke(TPredicate, other);
    }

    return (storage.value() == other);
  }

 private:
  storage_type storage;

  [[nodiscard]]
  constexpr bool has_value() const noexcept {
    return storage.has_value() && std::invoke(TPredicate, storage.value());
  }

  constexpr void require_value() const {
    if (!has_value()) [[unlikely]] {
      throw std::logic_error("Can't access a moved or invalid value");
    }
  }
};

// `std::out_ptr` is supported for this
template <class T, auto TDeleter, auto TPredicate = std::identity {}>
struct unique_ptr : unique_any<T*, TDeleter, TPredicate> {
  using pointer = T*;
  using element_type = T;

  using unique_any<T*, TDeleter, TPredicate>::unique_any;

  constexpr unique_ptr()
    : unique_any<T*, TDeleter, TPredicate> {std::nullopt} {}
};

}// namespace felly::inline unique_any_types

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
