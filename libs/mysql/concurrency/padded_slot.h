// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_CONCURRENCY_PADDED_SLOT
#define MYSQL_CONCURRENCY_PADDED_SLOT

#include "my_compiler.h"  // MY_COMPILER_DIAGNOSTIC_*
#include "mysql/concurrency/cache_line_size.h"

namespace mysql::concurrency::detail {

/// @brief Padded slot for Sync_bounded_queue, aligning the struct to cache line
/// size to minimize false sharing in concurrent access.
/// @details Holds an element, validity flag, mutex, and condition variables for
/// producer-consumer synchronization. Each slot is padded to prevent adjacent
/// slots from sharing cache lines, reducing contention in multi-threaded
/// environments.
/// @tparam T Element type.
/// @tparam Mutex_tp Mutex type
/// @tparam Cv_tp Condition variable type
/// @tparam cache_line_size_tp Padding size (default:
/// hardware_destructive_interference_size).
/// @see Sync_bounded_queue
template <typename T, typename Mutex_tp, typename Cv_tp,
          size_t cache_line_size_tp = hardware_destructive_interference_size>
struct alignas(cache_line_size_tp) Padded_slot {
  /// The stored element in the queue slot.
  T element{};
  /// Validity flag: true if the slot contains an element ready for consumption,
  /// false if empty.
  bool validity{false};
  /// Mutex protecting access to the element and flags.
  Mutex_tp validity_mt;
  /// Condition variable for producers waiting for the slot to become free (full
  /// queue).
  Cv_tp producer_cv;
  /// Condition variable for consumers waiting for the slot to be filled (empty
  /// queue).
  Cv_tp consumer_cv;
};

}  // namespace mysql::concurrency::detail

#endif  // MYSQL_CONCURRENCY_PADDED_SLOT
