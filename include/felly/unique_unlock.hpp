// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <utility>

namespace felly::inline unique_unlock_types {

template <class T>
struct unique_unlock {
  unique_unlock() = delete;
  explicit unique_unlock(T& lock) : mLock(&lock) { lock.unlock(); }

  ~unique_unlock() {
    if (mLock) {
      mLock->lock();
    }
  }

  unique_unlock(unique_unlock&& other) noexcept {
    mLock = std::exchange(other.mLock, nullptr);
  }

  unique_unlock& operator=(unique_unlock&& other) noexcept {
    if (std::addressof(other) == this) {
      return *this;
    }

    if (mLock) {
      mLock->lock();
    }
    mLock = std::exchange(other.mLock, nullptr);
    return *this;
  }

  unique_unlock(const unique_unlock&) = delete;
  unique_unlock& operator=(const unique_unlock&) = delete;

  [[nodiscard]]
  bool owns_lock() const noexcept {
    return mLock != nullptr;
  }

 private:
  T* mLock {nullptr};
};

}// namespace felly::inline unique_unlock_types
