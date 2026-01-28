// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <mutex>
#include <utility>

namespace felly::inline guarded_data_types {

template <class T>
struct unique_guarded_data_lock {
  unique_guarded_data_lock() = delete;

  unique_guarded_data_lock(std::unique_lock<std::mutex> lock, T* data)
    : mLock(std::move(lock)),
      mData(data) {}
  unique_guarded_data_lock(const unique_guarded_data_lock&) = delete;
  unique_guarded_data_lock& operator=(const unique_guarded_data_lock&) = delete;

  unique_guarded_data_lock& operator=(
    unique_guarded_data_lock&& other) noexcept {
    mLock = std::move(other.mLock);
    mData = std::exchange(other.mData, nullptr);
    return *this;
  }

  unique_guarded_data_lock(unique_guarded_data_lock&& other) noexcept {
    *this = std::move(other);
  }

  operator bool() const noexcept { return mData != nullptr; }

  T const* operator->() const noexcept { return mData; }

  T* operator->() noexcept { return mData; }

  [[nodiscard]]
  const T& get() const noexcept {
    return *mData;
  }

  [[nodiscard]]
  T& get() noexcept {
    return *mData;
  }

  [[nodiscard]]
  const T& operator*() const noexcept {
    return *mData;
  }

  [[nodiscard]]
  T& operator*() noexcept {
    return *mData;
  }

  void unlock() {
    mData = nullptr;
    mLock.unlock();
  }

 private:
  std::unique_lock<std::mutex> mLock;
  T* mData;
};

/* Totally not a Rust mutex.
 *
 * Hides the data so it can only be accessed via a lock guard.
 */
template <class T>
struct guarded_data {
  template <class... Args>
  explicit guarded_data(Args&&... args) : mData {std::forward<Args>(args)...} {}

  [[nodiscard]]
  auto lock() {
    return unique_guarded_data_lock<T>(std::unique_lock {mMutex}, &mData);
  }

  [[nodiscard]]
  auto lock() const {
    return unique_guarded_data_lock<const T>(std::unique_lock {mMutex}, &mData);
  }

 private:
  mutable std::mutex mMutex {};
  T mData;
};

}// namespace felly::inline guarded_data_types
