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

template <auto TDeleter>
struct invoke_as_deleter_t {
  // General case
  template <class T>
  static void operator()(T&& v)
    requires requires { std::invoke(TDeleter, std::forward<T>(v)); }
  {
    std::invoke(TDeleter, std::forward<T>(v));
  }

  // Allow `c_deleter(T*) for a `T`
  template <class T>
    requires(!std::is_pointer_v<T>)
    && std::
      invocable<decltype(TDeleter), std::add_pointer_t<std::remove_cvref_t<T>>>
  static void operator()(T& x) {
    std::invoke(
      TDeleter, std::addressof(const_cast<std::remove_cvref_t<T>&>(x)));
  }

  // Allow `c_deleter(T *)` for a `const T*`
  template <class T>
  static void operator()(T x)
    requires std::is_pointer_v<T> && std::is_const_v<std::remove_pointer_t<T>>
    && (!std::invocable<decltype(TDeleter), T>)
    && (std::invocable<
        decltype(TDeleter),
        std::remove_const_t<std::remove_pointer_t<T>>*>)
  {
    std::invoke(
      TDeleter, const_cast<std::remove_const_t<std::remove_pointer_t<T>>*>(x));
  }
};

template <auto TDeleter, class T>
concept invocable_as_deleter =
  requires(T v) { invoke_as_deleter_t<TDeleter> {}(v); };

template <auto TDeleter, class T>
  requires invocable_as_deleter<TDeleter, T>
void invoke_as_deleter(T&& v) {
  invoke_as_deleter_t<TDeleter> {}(std::forward<T>(v));
}

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

template <class T, class U>
concept nullptr_or_predicate =
  std::same_as<T, std::nullptr_t> || std::predicate<T, U>;

}// namespace felly_detail

namespace felly::inline unique_any_types {

template <
  class T,
  auto TDeleter,
  felly_detail::nullptr_or_predicate<const T&> auto TPredicate = nullptr>
  requires felly_detail::
    invocable_as_deleter<TDeleter, std::remove_const_t<T>&&>
  struct unique_any_optional_storage_traits {
  using value_type = T;
  using mutable_value_type = std::remove_const_t<value_type>;
  using storage_type = std::optional<mutable_value_type>;

  static constexpr bool has_predicate =
    !std::same_as<decltype(TPredicate), std::nullptr_t>;

  static_assert(
    std::movable<mutable_value_type>,
    "stored type must be movable; if this is an unexpected failure, check C++ "
    "rule of three/five/zero");

  static constexpr auto default_value() noexcept { return storage_type {}; };

  static constexpr void destroy(storage_type& s) noexcept {
    auto temp = std::move(s).value();
    s.reset();
    felly_detail::invoke_as_deleter_t<TDeleter> {}(temp);
  }

  [[nodiscard]] static constexpr bool has_value(
    [[maybe_unused]] const value_type& v) noexcept {
    if constexpr (has_predicate) {
      return std::invoke(TPredicate, v);
    } else {
      return true;
    }
  }

  [[nodiscard]] static constexpr bool has_value(
    const storage_type& s) noexcept {
    return s.has_value() && has_value(s.value());
  }

  template <class U>
  [[nodiscard]] static constexpr decltype(auto) value(U&& s) {
    return felly_detail::unique_any_forward_like<U>(s).value();
  }

  template <class... Args>
  constexpr static void construct(storage_type& s, Args&&... args) {
    s.emplace(std::forward<Args>(args)...);
  }
};

template <
  class T,
  auto TDeleter = std::default_delete<std::remove_const_t<T>> {},
  felly_detail::nullptr_or_predicate<const std::remove_pointer_t<T>*> auto
    TPredicate = nullptr>
  requires std::is_pointer_v<T>
  && felly_detail::invocable_as_deleter<TDeleter, std::remove_const_t<T>>
struct unique_any_pointer_traits {
  using value_type = T;
  using storage_type = std::remove_const_t<T>;

  static constexpr auto has_predicate =
    !std::same_as<decltype(TPredicate), std::nullptr_t>;

  static constexpr storage_type default_value() noexcept { return {}; };

  static constexpr void destroy(storage_type& storage) {
    felly_detail::invoke_as_deleter<TDeleter>(std::exchange(storage, nullptr));
  }

  [[nodiscard]] static constexpr bool has_value(
    const storage_type& s) noexcept {
    if constexpr (has_predicate) {
      return std::invoke(TPredicate, s);
    } else {
      return static_cast<bool>(s);
    }
  }

  template <class U>
  [[nodiscard]] static constexpr decltype(auto) value(U&& s) {
    return felly_detail::unique_any_forward_like<U>(s);
  }

  constexpr static void construct(
    storage_type& s,
    std::add_const_t<value_type> p) {
    s = p;
  }
};

template <class T>
inline constexpr auto unique_any_default_delete = std::is_pointer_v<T>
  ? std::default_delete<std::remove_const_t<std::remove_pointer_t<T>>> {}
  : [](const T&) {};

template <class T, auto TDeleter, auto TPredicate>
struct unique_any_default_traits
  : unique_any_optional_storage_traits<T, TDeleter, TPredicate> {};
template <class T, auto TDeleter, auto TPredicate>
  requires std::is_pointer_v<T>
struct unique_any_default_traits<T, TDeleter, TPredicate>
  : unique_any_pointer_traits<T, TDeleter, TPredicate> {};

template <typename T>
concept unique_any_traits =
  requires {
    typename T::value_type;
    typename T::storage_type;
  }
  && requires(
    const typename T::storage_type& const_storage,
    typename T::storage_type& mutable_storage,
    const typename T::value_type& const_value,
    typename T::value_type& mutable_value) {
       {
         T::default_value()
       } -> std::convertible_to<typename T::storage_type&&>;
       {
         T::construct(mutable_storage, std::move(mutable_value))
       } -> std::same_as<void>;
       { T::destroy(mutable_storage) } -> std::same_as<void>;
       { T::has_value(const_storage) } -> std::convertible_to<bool>;
       { T::has_value(const_value) } -> std::convertible_to<bool>;
       {
         T::value(mutable_storage)
       } -> std::convertible_to<typename T::value_type&>;
       {
         T::value(const_storage)
       } -> std::convertible_to<const typename T::value_type&>;
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
      TTraits::construct(storage, std::forward<U>(value));
    }
  }

  template <class... Args>
  explicit constexpr basic_unique_any(std::in_place_t, Args&&... args)
    requires(!std::is_pointer_v<value_type>)
    && requires(storage_type& storage) {
         TTraits::construct(storage, std::forward<Args>(args)...);
       }
  {
    TTraits::construct(storage, std::forward<Args>(args)...);
    if (!has_value()) {
      TTraits::destroy(storage);
    }
  }

  constexpr basic_unique_any(basic_unique_any&& other) noexcept {
    if (other.has_value()) {
      std::swap(storage, other.storage);
    }
  }

  template <class UTraits>
    requires felly_detail::
      const_promotion_to<typename UTraits::value_type, value_type>
    constexpr basic_unique_any(basic_unique_any<UTraits>&& other) noexcept {
    if (other) {
      TTraits::construct(storage, other.disown());
    }
  }

  constexpr void reset() noexcept {
    if (has_value()) {
      TTraits::destroy(storage);
    }
  }

  template <std::convertible_to<value_type> U>
  constexpr void reset(U&& u) {
    if (has_value()) {
      TTraits::destroy(storage);
    }
    if (TTraits::has_value(u)) {
      TTraits::construct(storage, std::forward<U>(u));
    }
  }

  // Like std::unique_ptr's `release()`, but more clearly named
  [[nodiscard]]
  constexpr value_type disown() {
    require_value();
    auto ret = std::move(TTraits::value(storage));
    storage = TTraits::default_value();
    return ret;
  }

  constexpr basic_unique_any& operator=(basic_unique_any&& other) noexcept {
    if (this == std::addressof(other)) {
      return *this;
    }

    reset();
    if (other.has_value()) {
      TTraits::construct(storage, other.disown());
    }
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
      TTraits::construct(storage, other.disown());
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
    return std::as_const(TTraits::value(storage));
  }

  [[nodiscard]]
  constexpr decltype(auto) get()
    requires(!std::is_const_v<value_type>)
  {
    require_value();
    return TTraits::value(storage);
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
    requires std::three_way_comparable<storage_type>
  = default;

  [[nodiscard]]
  constexpr bool operator==(const basic_unique_any&) const noexcept = default;

  [[nodiscard]]
  constexpr bool operator==(const value_type& other) const noexcept
    requires std::equality_comparable<storage_type>
  {
    if (!has_value()) {
      return !TTraits::has_value(other);
    }

    return (get() == other);
  }

 private:
  storage_type storage {TTraits::default_value()};

  [[nodiscard]]
  constexpr bool has_value() const noexcept {
    return TTraits::has_value(storage);
  }

  constexpr void require_value() const {
    if (!has_value()) [[unlikely]] {
      throw std::logic_error("Can't access a moved or invalid value");
    }
  }
};

template <
  class T,
  auto TDeleter = unique_any_default_delete<T>,
  felly_detail::nullptr_or_predicate<const T&> auto TPredicate = nullptr>
using unique_any =
  basic_unique_any<unique_any_default_traits<T, TDeleter, TPredicate>>;

}// namespace felly::inline unique_any_types
