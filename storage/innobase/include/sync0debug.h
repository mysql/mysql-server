/*****************************************************************************

Copyright (c) 2013, 2023, Oracle and/or its affiliates.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/sync0debug.h
 Debug checks for latches, header file

 Created 2012-08-21 Sunny Bains
 *******************************************************/

#ifndef sync0debug_h
#define sync0debug_h

#ifndef UNIV_LIBRARY
#include "sync0types.h"

#include <string>
#include <vector>

/** Initializes the synchronization data structures.
@param[in]      max_threads     Maximum threads that can be created. */
void sync_check_init(size_t max_threads);

/** Frees the resources in synchronization data structures. */
void sync_check_close();

#ifdef UNIV_DEBUG
/** Enable sync order checking. */
void sync_check_enable();

/** Check if it is OK to acquire the latch.
@param[in]      latch   latch type */
void sync_check_lock_validate(const latch_t *latch);

/** Note that the lock has been granted
@param[in]      latch   latch type */
void sync_check_lock_granted(const latch_t *latch);

/** Check if it is OK to acquire the latch.
@param[in]      latch   latch type
@param[in]      level   the level of the mutex */
void sync_check_lock(const latch_t *latch, latch_level_t level);

/**
Check if it is OK to re-acquire the lock. */
void sync_check_relock(const latch_t *latch);

/** Removes a latch from the thread level array if it is found there.
@param[in]      latch           The latch to unlock */
void sync_check_unlock(const latch_t *latch);

/** Checks if the level array for the current thread contains a
mutex or rw-latch at the specified level.
@param[in]      level   to find
@return a matching latch, or NULL if not found */
const latch_t *sync_check_find(latch_level_t level);

/** Checks that the level array for the current thread is empty.
Terminate iteration if the functor returns true.
@param[in,out]   functor        called for each element.
@return true if the functor returns true */
bool sync_check_iterate(sync_check_functor_t &functor);

/** Acquires the debug mutex. We cannot use the mutex defined in sync0sync,
because the debug mutex is also acquired in sync0arr while holding the OS
mutex protecting the sync array, and the ordinary mutex_enter might
recursively call routines in sync0arr, leading to a deadlock on the OS
mutex. */
void rw_lock_debug_mutex_enter();

/** Releases the debug mutex. */
void rw_lock_debug_mutex_exit();

/** For handling sync points in child threads spawned by a foreground thread. */
class Sync_point {
 public:
  /** Constructor.
  @param[in,out] thd            Server connection/session context. */
  explicit Sync_point(const THD *thd) noexcept : m_thd(thd) {}

  Sync_point(const Sync_point &) = default;

  Sync_point &operator=(const Sync_point &) = default;

  /** Destructor. */
  ~Sync_point() = default;

  /** Add a target to the list of sync points, nop for duplicates.
  @param[in] thd                Server connection/session context.
  @param[in] target             Target to add. */
  static void add(const THD *thd, const std::string &target) noexcept;

  /** Check if a target is enabled. Disable it if found.
  @param[in] thd                Server connection/session context.
  @param[in] target             Check if target is enabled.
  @return true if was enabled. */
  static bool enabled(const THD *thd, const std::string &target) noexcept;

  /** Check if a target is enabled. Disable it if found.
  @param[in] target             Check if target is enabled.
  @return true if was enabled. */
  static bool enabled(const std::string &target) noexcept;

  /** Clear the named target.
  @param[in] thd                Server connection/session context.
  @param[in] target             Check if target is enabled. */
  static void erase(const THD *thd, const std::string &target) noexcept;

 private:
  using Targets = std::vector<std::string, ut::allocator<std::string>>;
  using Sync_points = std::vector<Sync_point, ut::allocator<Sync_point>>;

  /** Mutex protecting access to Sync_point infrastructure. */
  static std::mutex s_mutex;

  /** Sync points. */
  static Sync_points s_sync_points;

  /** Server connection/session context. */
  const THD *m_thd{};

  /** List of enabled targets. */
  Targets m_targets{};
};
#endif /* UNIV_DEBUG */

#endif /* !UNIV_LIBRARY */
#endif /* !sync0debug_h */
