// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <utility>

namespace felly::inline moved_flag_types {

struct moved_flag {
  constexpr moved_flag() = default;

  constexpr moved_flag(const moved_flag&) = default;
  constexpr moved_flag& operator=(const moved_flag&) = default;

  constexpr moved_flag(moved_flag&& other) noexcept {
    mMoved = std::exchange(other.mMoved, true);
  }

  constexpr moved_flag& operator=(moved_flag&& other) noexcept {
    mMoved = std::exchange(other.mMoved, true);
    return *this;
  }

  constexpr operator bool() const noexcept {
    return mMoved;
  }

 private:
  bool mMoved = false;
};

}// namespace felly
