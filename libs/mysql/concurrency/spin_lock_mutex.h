// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

/// @defgroup GroupLibsMysqlConcurrency Concurrency
/// @ingroup GroupLibsMysql

#ifndef MYSQL_CONCURRENCY_SPIN_LOCK_MUTEX
#define MYSQL_CONCURRENCY_SPIN_LOCK_MUTEX

#include <atomic>
#include <thread>
#include "mysql/concurrency/mutex.h"

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__i386__) || defined(__x86_64__)
#if defined(__INTEL_COMPILER)
#include <immintrin.h>
#elif defined(__clang__)
#include <emmintrin.h>
#endif
#endif

/// @addtogroup GroupLibsMysqlConcurrency
/// @{

namespace mysql::concurrency {

/// @brief Implementation of a spin-lock mutex. Satisfies requirements of
/// *Mutex*
///  - Lockable
///  - DefaultConstructible
///  - Destructible
///  - not copyable
///  - not movable
/// @note Use with care. In general, is not recommended to use spin locks over
/// mutexes. Spin locks should be used only when expected contention is minimal
/// (e.g. when combining with strip-locking techniques).
class Spin_lock_mutex {
 public:
  // Disable copy-move semantics
  Spin_lock_mutex(const Spin_lock_mutex &) = delete;
  Spin_lock_mutex(Spin_lock_mutex &&) = delete;
  Spin_lock_mutex &operator=(const Spin_lock_mutex &src) = delete;
  Spin_lock_mutex &operator=(Spin_lock_mutex &&src) = delete;

  Spin_lock_mutex([[maybe_unused]] Mutex_key key = {}) {}
  ~Spin_lock_mutex() = default;

  /// @brief Acquire a lock, blocks access to a critical section until "unlock"
  /// is called
  void lock() noexcept;

  /// Non-blocking try-lock operation
  /// @retval true Lock has been acquired
  /// @retval false Lock has NOT been acquired
  bool try_lock() noexcept;

  /// @brief Unblocks access to a critical section for other threads
  void unlock();

 private:
  static inline void spin_yield() noexcept {
// x86 / x86_64
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    _mm_pause();
#elif defined(__i386__) || defined(__x86_64__)
    __builtin_ia32_pause();
// ARM
#elif defined(__arm__) || defined(__aarch64__)
    __asm__ volatile("yield" ::: "memory");
// Other architectures
#else
    std::this_thread::yield();
#endif
  }
  /// Internal synchronization boolean, true means that lock is currently owned
  /// Since C++20, default constructor sets atomic_flag to "clear" state
  /// We use atomic_flag because it is guaranteed to be lock free
  std::atomic_flag m_lock = ATOMIC_FLAG_INIT;
};

}  // namespace mysql::concurrency

/// @}

#endif  //  MYSQL_CONCURRENCY_SPIN_LOCK_MUTEX
