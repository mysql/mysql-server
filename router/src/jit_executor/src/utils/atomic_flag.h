/*
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ROUTER_SRC_JIT_EXECUTOR_UTILS_ATOMIC_FLAG_H_
#define ROUTER_SRC_JIT_EXECUTOR_UTILS_ATOMIC_FLAG_H_

#include <atomic>

namespace shcore {

// We cannot simply use std::atomic_flag, because GCC10 does not have
// std::atomic_flag::test().
#if defined(__cpp_lib_atomic_flag_test)

class atomic_flag : public std::atomic_flag {
 public:
  using std::atomic_flag::atomic_flag;

 private:
  using std::atomic_flag::notify_all;
  using std::atomic_flag::notify_one;
  using std::atomic_flag::wait;
};

#elif 2 == ATOMIC_BOOL_LOCK_FREE  // std::atomic<bool> is always lock-free

/**
 * A lock-free atomic boolean type.
 */
class atomic_flag {
 public:
  constexpr atomic_flag() noexcept = default;

  atomic_flag(const atomic_flag &) = delete;

  atomic_flag &operator=(const atomic_flag &) = delete;
  atomic_flag &operator=(const atomic_flag &) volatile = delete;

  ~atomic_flag() noexcept = default;

  inline void clear(
      std::memory_order order = std::memory_order_seq_cst) volatile noexcept {
    m_flag.store(false, order);
  }

  inline void clear(
      std::memory_order order = std::memory_order_seq_cst) noexcept {
    m_flag.store(false, order);
  }

  inline bool test_and_set(
      std::memory_order order = std::memory_order_seq_cst) volatile noexcept {
    return m_flag.exchange(true, order);
  }

  inline bool test_and_set(
      std::memory_order order = std::memory_order_seq_cst) noexcept {
    return m_flag.exchange(true, order);
  }

  inline bool test(std::memory_order order = std::memory_order_seq_cst) const
      volatile noexcept {
    return m_flag.load(order);
  }

  inline bool test(
      std::memory_order order = std::memory_order_seq_cst) const noexcept {
    return m_flag.load(order);
  }

 private:
  std::atomic_bool m_flag;
};

#else
#error "Missing implementation of shcore::atomic_flag"
#endif

}  // namespace shcore

#endif  // ROUTER_SRC_JIT_EXECUTOR_UTILS_ATOMIC_FLAG_H_
