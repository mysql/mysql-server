/*****************************************************************************

Copyright (c) 1995, 2023, Oracle and/or its affiliates.
Copyright (c) 2008, Google Inc.

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

/** @file include/sync0rw.h
 The read-write lock (for threads, not for database transactions)

 Created 9/11/1995 Heikki Tuuri
 *******************************************************/

#ifndef sync0rw_h
#define sync0rw_h

#include "univ.i"
#ifndef UNIV_HOTBACKUP
#include "os0event.h"
#include "ut0counter.h"
#endif /* !UNIV_HOTBACKUP */
#include <atomic>
#include "ut0mutex.h"

struct rw_lock_t;

#ifndef UNIV_HOTBACKUP

#ifdef UNIV_LIBRARY

#ifdef UNIV_DEBUG

/**
Pass-through version of rw_lock_own(), which normally checks that the
thread has locked the rw-lock in the specified mode.
@param[in]      lock            pointer to rw-lock
@param[in]      lock_type       lock type: RW_LOCK_S, RW_LOCK_X
@return true if success */
static inline bool rw_lock_own(rw_lock_t *lock, ulint lock_type) {
  return (lock != nullptr);
}

#define sync_check_iterate(A) true
#endif /* UNIV_DEBUG */

#define rw_lock_s_lock(L, Loc) ((void)0)
#define rw_lock_s_lock_nowait(L, Loc) true
#define rw_lock_s_unlock(L) ((void)0)
#define rw_lock_x_lock(L, Loc) ((void)0)
#define rw_lock_x_lock_nowait(L, Loc) true
#define rw_lock_x_unlock(L) ((void)0)
#define rw_lock_sx_lock(L, Loc) ((void)0)
#define rw_lock_sx_unlock(L) ((void)0)
#define rw_lock_s_lock_gen(M, P, L) ((void)0)
#define rw_lock_x_lock_gen(M, P, L) ((void)0)
#define rw_lock_sx_lock_gen(M, P, L) ((void)0)
#define sync_check_lock(A, B) ((void)0)
#define rw_lock_own_flagged(A, B) true
#endif /* UNIV_LIBRARY */

#endif /* !UNIV_HOTBACKUP */

/* Latch types; these are used also in btr0btr.h and mtr0mtr.h: keep the
numerical values smaller than 30 (smaller than BTR_MODIFY_TREE and
MTR_MEMO_MODIFY) and the order of the numerical values like below! and they
should be 2pow value to be used also as ORed combination of flag. */
enum rw_lock_type_t {
  RW_S_LATCH = 1,
  RW_X_LATCH = 2,
  RW_SX_LATCH = 4,
  RW_NO_LATCH = 8
};

/* We decrement lock_word by X_LOCK_DECR for each x_lock. It is also the
start value for the lock_word, meaning that it limits the maximum number
of concurrent read locks before the rw_lock breaks. */
/* We decrement lock_word by X_LOCK_HALF_DECR for sx_lock. */
constexpr int32_t X_LOCK_DECR = 0x20000000;
constexpr int32_t X_LOCK_HALF_DECR = 0x10000000;

#ifdef UNIV_DEBUG
struct rw_lock_debug_t;
#endif /* UNIV_DEBUG */

extern ib_mutex_t rw_lock_list_mutex;

#ifndef UNIV_LIBRARY
#ifndef UNIV_HOTBACKUP

/** Creates, or rather, initializes an rw-lock object in a specified memory
 location (which must be appropriately aligned). The rw-lock is initialized
 to the non-locked state. Explicit freeing of the rw-lock with rw_lock_free
 is necessary only if the memory block containing it is freed.
 @param[in] lock pointer to memory
 @param[in] id latch_id
 @param[in] clocation location where created */
void rw_lock_create_func(rw_lock_t *lock,
                         IF_DEBUG(latch_id_t id, ) ut::Location clocation);
/** Calling this function is obligatory only if the memory buffer containing
 the rw-lock is freed. Removes an rw-lock object from the global list. The
 rw-lock is checked to be in the non-locked state. */
void rw_lock_free_func(rw_lock_t *lock); /*!< in/out: rw-lock */
#ifdef UNIV_DEBUG
/** Checks that the rw-lock has been initialized and that there are no
 simultaneous shared and exclusive locks.
 @return true */
[[nodiscard]] bool rw_lock_validate(const rw_lock_t *lock); /*!< in: rw-lock */
#endif                                                      /* UNIV_DEBUG */

/** Low-level function which tries to lock an rw-lock in s-mode. Performs no
spinning.
@param[in]      lock            pointer to rw-lock
@param[in]      pass            pass value; != 0, if the lock will be passed
                                to another thread to unlock
@param[in]      location        location where requested
@return true if success */
[[nodiscard]] static inline bool rw_lock_s_lock_low(rw_lock_t *lock,
                                                    ulint pass [[maybe_unused]],
                                                    ut::Location location);

/** NOTE! Use the corresponding macro, not directly this function, except if
you supply the file name and line number. Lock an rw-lock in shared mode for
the current thread. If the rw-lock is locked in exclusive mode, or there is an
exclusive lock request waiting, the function spins a preset time (controlled
by srv_n_spin_wait_rounds), waiting for the lock, before suspending the thread.
@param[in]      lock            pointer to rw-lock
@param[in]      pass            pass value; != 0, if the lock will be passed to
                                another thread to unlock
@param[in]      location        location where requested */
static inline void rw_lock_s_lock_func(rw_lock_t *lock, ulint pass,
                                       ut::Location location);

/** NOTE! Use the corresponding macro, not directly this function! Lock an
rw-lock in exclusive mode for the current thread if the lock can be obtained
immediately.
@param[in]      lock            pointer to rw-lock
@param[in]      location        location where requested
@return true if success */
[[nodiscard]] static inline bool rw_lock_x_lock_func_nowait(
    rw_lock_t *lock, ut::Location location);

/** Releases a shared mode lock.
@param[in]      pass    pass value; != 0, if the lock will be passed to another
                        thread to unlock
@param[in,out]  lock    rw-lock */
static inline void rw_lock_s_unlock_func(IF_DEBUG(ulint pass, )
                                             rw_lock_t *lock);

/** NOTE! Use the corresponding macro, not directly this function! Lock an
rw-lock in exclusive mode for the current thread. If the rw-lock is locked
in shared or exclusive mode, or there is an exclusive lock request waiting,
the function spins a preset time (controlled by srv_n_spin_wait_rounds),
waiting for the lock, before suspending the thread. If the same thread has an
x-lock on the rw-lock, locking succeed, with the following exception: if pass
!= 0, only a single x-lock may be taken on the lock. NOTE: If the same thread
has an s-lock, locking does not succeed!
@param[in]      lock            pointer to rw-lock
@param[in]      pass            pass value; != 0, if the lock will be passed to
                                another thread to unlock
@param[in]      location        location where requested */
void rw_lock_x_lock_func(rw_lock_t *lock, ulint pass, ut::Location location);

/** Low-level function for acquiring an sx lock.
@param[in]      lock            pointer to rw-lock
@param[in]      pass            pass value; != 0, if the lock will be passed to
                                another thread to unlock
@param[in]      location        location where requested
@return false if did not succeed, true if success. */
[[nodiscard]] bool rw_lock_sx_lock_low(rw_lock_t *lock, ulint pass,
                                       ut::Location location);
/** NOTE! Use the corresponding macro, not directly this function! Lock an
rw-lock in SX mode for the current thread. If the rw-lock is locked
in exclusive mode, or there is an exclusive lock request waiting,
the function spins a preset time (controlled by SYNC_SPIN_ROUNDS), waiting
for the lock, before suspending the thread. If the same thread has an x-lock
on the rw-lock, locking succeed, with the following exception: if pass != 0,
only a single sx-lock may be taken on the lock. NOTE: If the same thread has
an s-lock, locking does not succeed!
@param[in]      lock            pointer to rw-lock
@param[in]      pass            pass value; != 0, if the lock will be passed to
                                another thread to unlock
@param[in]      location        location where requested */
void rw_lock_sx_lock_func(rw_lock_t *lock, ulint pass, ut::Location location);

/** Releases an exclusive mode lock.
@param[in]      pass    pass value; != 0, if the lock will be passed to another
                        thread to unlock
@param[in,out]  lock    rw-lock */
static inline void rw_lock_x_unlock_func(IF_DEBUG(ulint pass, )
                                             rw_lock_t *lock);

/** Releases an sx mode lock.
@param[in]      pass    pass value; != 0, if the lock will be passed to another
                        thread to unlock
@param[in,out]  lock    rw-lock */
static inline void rw_lock_sx_unlock_func(IF_DEBUG(ulint pass, )
                                              rw_lock_t *lock);

/** This function is used in the insert buffer to move the ownership of an
x-latch on a buffer frame to the current thread. The x-latch was set by
the buffer read operation and it protected the buffer frame while the
read was done. The ownership is moved because we want that the current
thread is able to acquire a second x-latch which is stored in an mtr.
This, in turn, is needed to pass the debug checks of index page
operations.
@param[in]      lock    lock which was x-locked in the buffer read. */
void rw_lock_x_lock_move_ownership(rw_lock_t *lock);
/** Returns the value of writer_count for the lock. Does not reserve the lock
mutex, so the caller must be sure it is not changed during the call.
@return value of writer_count
@param[in]      lock    rw-lock */
static inline ulint rw_lock_get_x_lock_count(const rw_lock_t *lock);
/** Returns the number of sx-lock for the lock. Does not reserve the lock
mutex, so the caller must be sure it is not changed during the call.
@param[in]      lock    rw-lock
@return value of writer_count */
static inline ulint rw_lock_get_sx_lock_count(const rw_lock_t *lock);
/** Check if there are threads waiting for the rw-lock.
@param[in]      lock    rw-lock
@return true if waiters, false otherwise */
[[nodiscard]] static inline bool rw_lock_get_waiters(const rw_lock_t *lock);
/** Returns the write-status of the lock - this function made more sense
with the old rw_lock implementation.
@param[in]      lock    rw-lock
@return RW_LOCK_NOT_LOCKED, RW_LOCK_X, RW_LOCK_X_WAIT, RW_LOCK_SX */
static inline ulint rw_lock_get_writer(const rw_lock_t *lock);
/** Returns the number of readers (s-locks).
@param[in]      lock    rw-lock
@return number of readers */
static inline ulint rw_lock_get_reader_count(const rw_lock_t *lock);

/** Decrements lock_word the specified amount if it is greater than 0.
This is used by both s_lock and x_lock operations.
@param[in,out]  lock            rw-lock
@param[in]      amount          amount to decrement
@param[in]      threshold       threshold of judgement
@return true if decr occurs */
[[nodiscard]] static inline bool rw_lock_lock_word_decr(rw_lock_t *lock,
                                                        ulint amount,
                                                        lint threshold);

/** Increments lock_word the specified amount and returns new value.
@param[in,out]  lock    rw-lock
@param[in]      amount  amount to decrement
@return lock->lock_word after increment */
static inline lint rw_lock_lock_word_incr(rw_lock_t *lock, ulint amount);

/** This function sets the lock->writer_thread and lock->recursive fields. Sets
lock->recursive field using atomic release after setting lock->writer thread to
ensure proper memory ordering of the two.
Note that it is assumed that the caller of this function effectively owns
the lock i.e.: nobody else is allowed to modify lock->writer_thread at this
point in time. The protocol is that lock->writer_thread MUST be updated BEFORE
the lock->recursive flag is set.
@param[in,out]  lock            lock to work on
@param[in]      recursive       true if recursion allowed */
static inline void rw_lock_set_writer_id_and_recursion_flag(rw_lock_t *lock,
                                                            bool recursive);

#ifdef UNIV_DEBUG
/** Checks if the thread has locked the rw-lock in the specified mode, with
the pass value == 0. Note that the mode is checked exactly, so if the thread
owns RW_LOCK_X only, the rw_lock_own(..,RW_LOCK_S) will return false.
@param[in]      lock             the rw-lock
@param[in]      lock_type        The exact lock type to check:
                                 RW_LOCK_S, RW_LOCK_SX or RW_LOCK_X
*/
[[nodiscard]] bool rw_lock_own(const rw_lock_t *lock, ulint lock_type);

/** Checks if the thread has locked the rw-lock in the specified mode, with the
pass value == 0.
@param[in]      lock    rw-lock
@param[in]      flags   specify lock types with OR of the rw_lock_flag_t values
@return true if locked */
[[nodiscard]] bool rw_lock_own_flagged(const rw_lock_t *lock,
                                       rw_lock_flags_t flags);
#endif /* UNIV_DEBUG */
#endif /* !UNIV_HOTBACKUP */
/** Checks if somebody has locked the rw-lock in the specified mode.
@param[in]      lock             the rw-lock
@param[in]      lock_type        The exact lock type to check:
                                 RW_LOCK_S, RW_LOCK_SX or RW_LOCK_X
@return true if locked */
[[nodiscard]] bool rw_lock_is_locked(rw_lock_t *lock, ulint lock_type);
#ifdef UNIV_DEBUG
/** Prints debug info of currently locked rw-locks.
@param[in]      file    file where to print */
void rw_lock_list_print_info(FILE *file);

/*#####################################################################*/

/** Prints info of a debug struct.
@param[in] f Output stream
@param[in] info Debug struct */
void rw_lock_debug_print(FILE *f, const rw_lock_debug_t *info);

#endif /* UNIV_DEBUG */

#endif /* !UNIV_LIBRARY */

#ifdef UNIV_DEBUG
/** The structure for storing debug info of an rw-lock.  All access to this
structure must be protected by rw_lock_debug_mutex_enter(). */
struct rw_lock_debug_t {
  /** The thread id of the thread which locked the rw-lock. */
  std::thread::id thread_id;
  /** Pass value given in the lock operation. */
  ulint pass;
  /** Type of the lock: RW_LOCK_X, RW_LOCK_S, RW_LOCK_X_WAIT. */
  ulint lock_type;
  /** Location where the rw-lock was locked. */
  ut::Location location;
  /** Debug structs are linked in a two-way list. */
  UT_LIST_NODE_T(rw_lock_debug_t) list;
};
#endif /* UNIV_DEBUG */

/* NOTE! The structure appears here only for the compiler to know its size.
Do not use its fields directly! */

/** The structure used in the spin lock implementation of a read-write
lock. Several threads may have a shared lock simultaneously in this
lock, but only one writer may have an exclusive lock, in which case no
shared locks are allowed. To prevent starving of a writer blocked by
readers, a writer may queue for x-lock by decrementing lock_word: no
new readers will be let in while the thread waits for readers to
exit. */

struct rw_lock_t
#ifdef UNIV_DEBUG
    : public latch_t
#endif /* UNIV_DEBUG */
{
  rw_lock_t() = default;
  /** rw_lock_t is not a copyable object, the reasoning
  behind this is the same as the reasoning behind why
  std::mutex is not copyable. It is supposed to represent
  a synchronization primitive for which copying semantics
  do not make sense. */
  rw_lock_t(const rw_lock_t &) = delete;
  rw_lock_t &operator=(const rw_lock_t &) = delete;

  /** Holds the state of the lock. */
  std::atomic<int32_t> lock_word;

  /** 1: there are waiters */
  std::atomic<bool> waiters;

  /** Default value false which means the lock is non-recursive.
  The value is typically set to true making normal rw_locks recursive.
  In case of asynchronous IO, when a non-zero value of 'pass' is
  passed then we keep the lock non-recursive.

  This flag also tells us about the state of writer_thread field.
  If this flag is set then writer_thread MUST contain the thread
  id of the current x-holder or wait-x thread.  This flag must be
  reset in x_unlock functions before incrementing the lock_word */
  std::atomic<bool> recursive;

  /** number of granted SX locks. */
  volatile ulint sx_recursive;

  /** Thread id of writer thread. Is only guaranteed to have non-stale value if
  recursive flag is set, otherwise it may contain native thread ID of a
  thread which already released or passed the lock. */
  std::atomic<std::thread::id> writer_thread;

  /** XOR of reader threads' IDs. If there is exactly one reader it should allow
   to retrieve the thread ID of that reader. */
  Atomic_xor_of_thread_id reader_thread;

  /** Used by sync0arr.cc for thread queueing */
  os_event_t event;

  /** Event for next-writer to wait on. A thread must decrement
  lock_word before waiting. */
  os_event_t wait_ex_event;

  /** Location where lock created */
  ut::Location clocation;

  /** last s-lock file/line is not guaranteed to be correct */
  const char *last_s_file_name;

  /** File name where last x-locked */
  const char *last_x_file_name;

  /** If 1 then the rw-lock is a block lock */
  bool is_block_lock;

  /** Line number where last time s-locked */
  uint16_t last_s_line;

  /** Line number where last time x-locked */
  uint16_t last_x_line;

  /** Count of os_waits. May not be accurate */
  uint32_t count_os_wait;

  /** All allocated rw locks are put into a list */
  UT_LIST_NODE_T(rw_lock_t) list;

#ifdef UNIV_PFS_RWLOCK
  /** The instrumentation hook */
  struct PSI_rwlock *pfs_psi;
#endif /* UNIV_PFS_RWLOCK */

#ifndef UNIV_DEBUG
  /** Destructor */
  ~rw_lock_t();
#else
  /** Destructor */
  ~rw_lock_t() override;

  virtual std::string to_string() const override;
  virtual std::string locked_from() const override;

  /** For checking memory corruption. */
  static const uint32_t MAGIC_N = 22643;
  uint32_t magic_n = {MAGIC_N};

  /** In the debug version: pointer to the debug info list of the lock */
  UT_LIST_BASE_NODE_T(rw_lock_debug_t, list) debug_list{};

#endif /* UNIV_DEBUG */

  /** Checks if there is a thread requesting an x-latch waiting for threads to
  release their s-latches.
  @return true iff there is an x-latcher blocked by s-latchers. */
  bool is_x_blocked_by_s() {
    const auto snapshot = lock_word.load();
    return snapshot < 0 && -X_LOCK_DECR < snapshot &&
           snapshot != -X_LOCK_HALF_DECR;
  }
};

#ifndef UNIV_LIBRARY
#ifndef UNIV_HOTBACKUP
/* For performance schema instrumentation, a new set of rwlock
wrap functions are created if "UNIV_PFS_RWLOCK" is defined.
The instrumentations are not planted directly into original
functions, so that we keep the underlying function as they
are. And in case, user wants to "take out" some rwlock from
instrumentation even if performance schema (UNIV_PFS_RWLOCK)
is defined, they can do so by reinstating APIs directly link to
original underlying functions.
The instrumented function names have prefix of "pfs_rw_lock_" vs.
original name prefix of "rw_lock_". Following are list of functions
that have been instrumented:

rw_lock_create()
rw_lock_x_lock()
rw_lock_x_lock_gen()
rw_lock_x_lock_nowait()
rw_lock_x_unlock_gen()
rw_lock_s_lock()
rw_lock_s_lock_gen()
rw_lock_s_lock_nowait()
rw_lock_s_unlock_gen()
rw_lock_sx_lock()
rw_lock_sx_unlock_gen()
rw_lock_free()
*/

#ifdef UNIV_PFS_RWLOCK
/** Performance schema instrumented wrap function for rw_lock_create_func()
NOTE! Please use the corresponding macro rw_lock_create(), not directly this
function!
@param[in]      key             key registered with performance schema
@param[in]      lock            rw lock
@param[in]      id              latch_id
@param[in]      clocation       location where created */
static inline void pfs_rw_lock_create_func(mysql_pfs_key_t key, rw_lock_t *lock,
                                           IF_DEBUG(latch_id_t id, )
                                               ut::Location clocation);

/** Performance schema instrumented wrap function for rw_lock_x_lock_func()
NOTE! Please use the corresponding macro rw_lock_x_lock(), not directly this
function!
@param[in]      lock            pointer to rw-lock
@param[in]      pass            pass value; != 0, if the lock will be passed
                                to another thread to unlock
@param[in]      location        location where requested */
static inline void pfs_rw_lock_x_lock_func(rw_lock_t *lock, ulint pass,
                                           ut::Location location);

/** Performance schema instrumented wrap function for
rw_lock_x_lock_func_nowait()
NOTE! Please use the corresponding macro, not directly this function!
@param[in]      lock            pointer to rw-lock
@param[in]      location        location where requested
@return true if success */
[[nodiscard]] static inline bool pfs_rw_lock_x_lock_func_nowait(
    rw_lock_t *lock, ut::Location location);

/** Performance schema instrumented wrap function for rw_lock_s_lock_func()
NOTE! Please use the corresponding macro rw_lock_s_lock(), not directly this
function!
@param[in]      lock            pointer to rw-lock
@param[in]      pass            pass value; != 0, if the lock will be passed
                                to another thread to unlock
@param[in]      location        location where requested */
static inline void pfs_rw_lock_s_lock_func(rw_lock_t *lock, ulint pass,
                                           ut::Location location);

/** Performance schema instrumented wrap function for rw_lock_s_lock_func()
NOTE! Please use the corresponding macro rw_lock_s_lock(), not directly this
function!
@param[in]      lock            pointer to rw-lock
@param[in]      pass            pass value; != 0, if the lock will be passed
                                to another thread to unlock
@param[in]      location        location where requested
@return true if success */
[[nodiscard]] static inline bool pfs_rw_lock_s_lock_low(rw_lock_t *lock,
                                                        ulint pass,
                                                        ut::Location location);

/** Performance schema instrumented wrap function for rw_lock_x_lock_func()
NOTE! Please use the corresponding macro rw_lock_x_lock(), not directly this
function!
@param[in]      lock            pointer to rw-lock
@param[in]      pass            pass value; != 0, if the lock will be passed
                                to another thread to unlock
@param[in]      location        location where requested */
static inline void pfs_rw_lock_x_lock_func(rw_lock_t *lock, ulint pass,
                                           ut::Location location);

/** Performance schema instrumented wrap function for rw_lock_s_unlock_func()
NOTE! Please use the corresponding macro rw_lock_s_unlock(), not directly this
function!
 @param[in]     pass    pass value; != 0, if the lock may have been passed to
                        another thread to unlock
 @param[in,out] lock    rw-lock */
static inline void pfs_rw_lock_s_unlock_func(IF_DEBUG(ulint pass, )
                                                 rw_lock_t *lock);

/** Performance schema instrumented wrap function for rw_lock_x_unlock_func()
NOTE! Please use the corresponding macro rw_lock_x_unlock(), not directly this
function!
@param[in]      pass    pass value; != 0, if the lock may have been passed to
another thread to unlock
@param[in,out]  lock    rw-lock */
static inline void pfs_rw_lock_x_unlock_func(IF_DEBUG(ulint pass, )
                                                 rw_lock_t *lock);

/** Performance schema instrumented wrap function for rw_lock_sx_lock_func()
NOTE! Please use the corresponding macro rw_lock_sx_lock(), not directly this
function!
@param[in]      lock            pointer to rw-lock
@param[in]      pass            pass value; != 0, if the lock will be passed
                                to another thread to unlock
@param[in]      location        location where requested */
static inline void pfs_rw_lock_sx_lock_func(rw_lock_t *lock, ulint pass,
                                            ut::Location location);

/** Performance schema instrumented wrap function for rw_lock_sx_lock_nowait()
NOTE! Please use the corresponding macro, not directly this function!
@param[in]      lock            pointer to rw-lock
@param[in]      pass            pass value; != 0, if the lock will be passed
                                to another thread to unlock
@param[in]      location        location where requested */
[[nodiscard]] static inline bool pfs_rw_lock_sx_lock_low(rw_lock_t *lock,
                                                         ulint pass,
                                                         ut::Location location);

/** Performance schema instrumented wrap function for rw_lock_sx_unlock_func()
NOTE! Please use the corresponding macro rw_lock_sx_unlock(), not directly this
function!
@param[in]      pass            pass value; != 0, if the lock will be passed to
another thread to unlock
@param[in,out]  lock            pointer to rw-lock */
static inline void pfs_rw_lock_sx_unlock_func(IF_DEBUG(ulint pass, )
                                                  rw_lock_t *lock);

/** Performance schema instrumented wrap function for rw_lock_free_func()
 NOTE! Please use the corresponding macro rw_lock_free(), not directly
 this function! */
static inline void pfs_rw_lock_free_func(rw_lock_t *lock); /*!< in: rw-lock */
#endif                                                     /* UNIV_PFS_RWLOCK */

#ifndef UNIV_PFS_RWLOCK
/** Creates, or rather, initializes an rw-lock object in a specified memory
 location (which must be appropriately aligned). The rw-lock is initialized
 to the non-locked state. Explicit freeing of the rw-lock with rw_lock_free
 is necessary only if the memory block containing it is freed.
 if MySQL performance schema is enabled and "UNIV_PFS_RWLOCK" is
 defined, the rwlock are instrumented with performance schema probes. */
#ifdef UNIV_DEBUG
#define rw_lock_create(K, L, ID) \
  rw_lock_create_func((L), (ID), UT_LOCATION_HERE)
#else /* UNIV_DEBUG */
#define rw_lock_create(K, L, ID) rw_lock_create_func((L), UT_LOCATION_HERE)
#endif /* UNIV_DEBUG */

/** NOTE! The following macros should be used in rw locking and
 unlocking, not the corresponding function. */

static inline void rw_lock_s_lock(rw_lock_t *M, ut::Location L) {
  rw_lock_s_lock_func(M, 0, L);
}

static inline void rw_lock_s_lock_gen(rw_lock_t *M, ulint P, ut::Location L) {
  rw_lock_s_lock_func(M, P, L);
}

static inline bool rw_lock_s_lock_nowait(rw_lock_t *M, ut::Location L) {
  return rw_lock_s_lock_low(M, 0, L);
}

#ifdef UNIV_DEBUG
static inline void rw_lock_s_unlock_gen(rw_lock_t *L, ulint P) {
  rw_lock_s_unlock_func(P, L);
}
#else
static inline void rw_lock_s_unlock_gen(rw_lock_t *L, ulint P) {
  rw_lock_s_unlock_func(L);
}
#endif /* UNIV_DEBUG */

static inline void rw_lock_sx_lock(rw_lock_t *L, ut::Location Loc) {
  rw_lock_sx_lock_func(L, 0, Loc);
}

static inline void rw_lock_sx_lock_gen(rw_lock_t *M, ulint P, ut::Location L) {
  rw_lock_sx_lock_func(M, P, L);
}

static inline bool rw_lock_sx_lock_nowait(rw_lock_t *M, ulint P,
                                          ut::Location L) {
  return rw_lock_sx_lock_low(M, P, L);
}

#ifdef UNIV_DEBUG
static inline void rw_lock_sx_unlock(rw_lock_t *L) {
  rw_lock_sx_unlock_func(0, L);
}
static inline void rw_lock_sx_unlock_gen(rw_lock_t *L, ulint P) {
  rw_lock_sx_unlock_func(P, L);
}
#else  /* UNIV_DEBUG */
static inline void rw_lock_sx_unlock(rw_lock_t *L) {
  rw_lock_sx_unlock_func(L);
}
static inline void rw_lock_sx_unlock_gen(rw_lock_t *L, ulint P) {
  rw_lock_sx_unlock_func(L);
}
#endif /* UNIV_DEBUG */

static inline void rw_lock_x_lock(rw_lock_t *M, ut::Location L) {
  rw_lock_x_lock_func(M, 0, L);
}

static inline void rw_lock_x_lock_gen(rw_lock_t *M, ulint P, ut::Location L) {
  rw_lock_x_lock_func(M, P, L);
}

static inline bool rw_lock_x_lock_nowait(rw_lock_t *M, ut::Location L) {
  return rw_lock_x_lock_func_nowait(M, L);
}

#ifdef UNIV_DEBUG
static inline void rw_lock_x_unlock_gen(rw_lock_t *L, ulint P) {
  rw_lock_x_unlock_func(P, L);
}
#else
static inline void rw_lock_x_unlock_gen(rw_lock_t *L, ulint P) {
  rw_lock_x_unlock_func(L);
}
#endif

#define rw_lock_free(M) rw_lock_free_func(M)

#else /* !UNIV_PFS_RWLOCK */

/* Following macros point to Performance Schema instrumented functions. */
#ifdef UNIV_DEBUG
#define rw_lock_create(K, L, ID) \
  pfs_rw_lock_create_func((K), (L), (ID), UT_LOCATION_HERE)
#else /* UNIV_DEBUG */
#define rw_lock_create(K, L, ID) \
  pfs_rw_lock_create_func((K), (L), UT_LOCATION_HERE)
#endif /* UNIV_DEBUG */

/******************************************************************
NOTE! The following macros should be used in rw locking and
unlocking, not the corresponding function. */

static inline void rw_lock_s_lock(rw_lock_t *M, ut::Location L) {
  pfs_rw_lock_s_lock_func(M, 0, L);
}

static inline void rw_lock_s_lock_gen(rw_lock_t *M, ulint P, ut::Location L) {
  pfs_rw_lock_s_lock_func(M, P, L);
}

static inline bool rw_lock_s_lock_nowait(rw_lock_t *M, ut::Location L) {
  return pfs_rw_lock_s_lock_low(M, 0, L);
}

#ifdef UNIV_DEBUG
static inline void rw_lock_s_unlock_gen(rw_lock_t *L, ulint P) {
  pfs_rw_lock_s_unlock_func(P, L);
}
#else
static inline void rw_lock_s_unlock_gen(rw_lock_t *L, ulint P) {
  pfs_rw_lock_s_unlock_func(L);
}
#endif

static inline void rw_lock_sx_lock(rw_lock_t *M, ut::Location L) {
  pfs_rw_lock_sx_lock_func(M, 0, L);
}

static inline void rw_lock_sx_lock_gen(rw_lock_t *M, ulint P, ut::Location L) {
  pfs_rw_lock_sx_lock_func(M, P, L);
}

static inline bool rw_lock_sx_lock_nowait(rw_lock_t *M, ulint P,
                                          ut::Location L) {
  return pfs_rw_lock_sx_lock_low(M, P, L);
}

#ifdef UNIV_DEBUG
static inline void rw_lock_sx_unlock(rw_lock_t *L) {
  pfs_rw_lock_sx_unlock_func(0, L);
}
static inline void rw_lock_sx_unlock_gen(rw_lock_t *L, ulint P) {
  pfs_rw_lock_sx_unlock_func(P, L);
}
#else
static inline void rw_lock_sx_unlock(rw_lock_t *L) {
  pfs_rw_lock_sx_unlock_func(L);
}
static inline void rw_lock_sx_unlock_gen(rw_lock_t *L, ulint P) {
  pfs_rw_lock_sx_unlock_func(L);
}
#endif

static inline void rw_lock_x_lock(rw_lock_t *M, ut::Location L) {
  pfs_rw_lock_x_lock_func(M, 0, L);
}

static inline void rw_lock_x_lock_gen(rw_lock_t *M, ulint P, ut::Location L) {
  pfs_rw_lock_x_lock_func(M, P, L);
}

static inline bool rw_lock_x_lock_nowait(rw_lock_t *M, ut::Location L) {
  return pfs_rw_lock_x_lock_func_nowait(M, L);
}

#ifdef UNIV_DEBUG
static inline void rw_lock_x_unlock_gen(rw_lock_t *L, ulint P) {
  pfs_rw_lock_x_unlock_func(P, L);
}
#else
static inline void rw_lock_x_unlock_gen(rw_lock_t *L, ulint P) {
  pfs_rw_lock_x_unlock_func(L);
}
#endif

static inline void rw_lock_free(rw_lock_t *M) { pfs_rw_lock_free_func(M); }

#endif /* !UNIV_PFS_RWLOCK */

static inline void rw_lock_s_unlock(rw_lock_t *L) {
  rw_lock_s_unlock_gen(L, 0);
}
static inline void rw_lock_x_unlock(rw_lock_t *L) {
  rw_lock_x_unlock_gen(L, 0);
}

#include "sync0rw.ic"

#endif /* !UNIV_HOTBACKUP */

#endif /* !UNIV_LIBRARY */
typedef UT_LIST_BASE_NODE_T(rw_lock_t, list) rw_lock_list_t;

extern rw_lock_list_t rw_lock_list;

#endif /* sync0rw.h */
