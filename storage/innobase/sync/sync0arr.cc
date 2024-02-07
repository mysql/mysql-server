/*****************************************************************************

Copyright (c) 1995, 2024, Oracle and/or its affiliates.
Copyright (c) 2008, Google Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file sync/sync0arr.cc
 The wait array used in synchronization primitives

 Created 9/5/1995 Heikki Tuuri
 *******************************************************/

#include "sync0arr.h"

#include <sys/types.h>
#include <time.h>

#ifndef UNIV_NO_ERR_MSGS
#include "lock0lock.h"
#endif /* !UNIV_NO_ERR_MSGS */

#include "os0event.h"
#include "os0file.h"
#include "srv0srv.h"
#include "sync0arr_impl.h"
#include "sync0debug.h"
#include "sync0sync.h"

/** User configured sync array size */
ulong srv_sync_array_size = 1;

/** Locally stored copy of srv_sync_array_size */
ulint sync_array_size;

/** The global array of wait cells for implementation of the database's own
mutexes and read-write locks */
sync_array_t **sync_wait_array;

/** count of how many times an object has been signalled */
static ulint sg_count;

#define sync_array_exit(a) mutex_exit(&(a)->mutex)
#define sync_array_enter(a) mutex_enter(&(a)->mutex)

/** Detects a deadlock of one or more threads because of waits of semaphores.
Reports a fatal error (and thus does not return) in case it finds one.
The return value is only used in recursive calls (depth>0).
@param[in]  arr     The wait array we limit our search for cycle to.
                    The caller must own the arr->mutex.
@param[in]  cell    The cell to check
@param[in]  depth   The recursion depth
@return true iff deadlock detected (there might be false negatives) */
static bool sync_array_detect_deadlock(sync_array_t *arr, sync_cell_t *cell,
                                       size_t depth);

#ifdef UNIV_DEBUG
/** Validates the integrity of the wait array. Checks
 that the number of reserved cells equals the count variable. */
static void sync_array_validate(sync_array_t *arr) /*!< in: sync wait array */
{
  ulint count = 0;

  sync_array_enter(arr);

  for (ulint i = 0; i < arr->next_free_slot; i++) {
    const sync_cell_t *cell;

    cell = &arr->cells[i];

    if (cell->latch.mutex != nullptr) {
      count++;
    }
  }

  ut_a(count == arr->n_reserved);

  sync_array_exit(arr);
}
#endif /* UNIV_DEBUG */

/** Constructor
Creates a synchronization wait array. It is protected by a mutex
which is automatically reserved when the functions operating on it
are called.
@param[in]      num_cells               Number of cells to create */
sync_array_t::sync_array_t(ulint num_cells) UNIV_NOTHROW : n_reserved(),
                                                           n_cells(),
                                                           cells(),
                                                           mutex(),
                                                           res_count(),
                                                           next_free_slot(),
                                                           first_free_slot() {
  ut_a(num_cells > 0);

  cells = ut::new_arr_withkey<sync_cell_t>(UT_NEW_THIS_FILE_PSI_KEY,
                                           ut::Count{num_cells});

  n_cells = num_cells;

  first_free_slot = ULINT_UNDEFINED;

  /* Then create the mutex to protect the wait array */
  mutex_create(LATCH_ID_SYNC_ARRAY_MUTEX, &mutex);
}

/** Destructor */
sync_array_t::~sync_array_t() UNIV_NOTHROW {
  ut_a(n_reserved == 0);

  ut_d(sync_array_validate(this));

  /* Release the mutex protecting the wait array */

  mutex_free(&mutex);

  ut::delete_arr(cells);
}

sync_cell_t *sync_array_get_nth_cell(sync_array_t *arr, ulint n) {
  ut_a(n < arr->n_cells);

  return (&arr->cells[n]);
}

/** Frees the resources in a wait array. */
static void sync_array_free(sync_array_t *arr) /*!< in, own: sync wait array */
{
  ut::delete_(arr);
}

/** Returns the event that the thread owning the cell waits for. */
static os_event_t sync_cell_get_event(
    sync_cell_t *cell) /*!< in: non-empty sync array cell */
{
  ulint type = cell->request_type;

  if (type == SYNC_MUTEX) {
    return (cell->latch.mutex->event());

  } else if (type == SYNC_BUF_BLOCK) {
    return (cell->latch.bpmutex->event());

  } else if (type == RW_LOCK_X_WAIT) {
    return (cell->latch.lock->wait_ex_event);

  } else { /* RW_LOCK_S and RW_LOCK_X wait on the same event */

    return (cell->latch.lock->event);
  }
}

sync_cell_t *sync_array_reserve_cell(sync_array_t *arr, void *object,
                                     ulint type, ut::Location location) {
  sync_cell_t *cell;

  sync_array_enter(arr);

  if (arr->first_free_slot != ULINT_UNDEFINED) {
    /* Try and find a slot in the free list */
    ut_ad(arr->first_free_slot < arr->next_free_slot);
    cell = sync_array_get_nth_cell(arr, arr->first_free_slot);
    arr->first_free_slot = cell->line;
  } else if (arr->next_free_slot < arr->n_cells) {
    /* Try and find a slot after the currently allocated slots */
    cell = sync_array_get_nth_cell(arr, arr->next_free_slot);
    ++arr->next_free_slot;
  } else {
    sync_array_exit(arr);

    // We should return NULL and if there is more than
    // one sync array, try another sync array instance.
    return (nullptr);
  }

  ++arr->res_count;

  ut_ad(arr->n_reserved < arr->n_cells);
  ut_ad(arr->next_free_slot <= arr->n_cells);

  ++arr->n_reserved;

  /* Reserve the cell. */
  ut_ad(cell->latch.mutex == nullptr);

  cell->request_type = type;

  if (cell->request_type == SYNC_MUTEX) {
    cell->latch.mutex = reinterpret_cast<WaitMutex *>(object);
  } else if (cell->request_type == SYNC_BUF_BLOCK) {
    cell->latch.bpmutex = reinterpret_cast<BlockWaitMutex *>(object);
  } else {
    cell->latch.lock = reinterpret_cast<rw_lock_t *>(object);
  }

  cell->waiting = false;

  cell->file = location.filename;
  cell->line = location.line;

  sync_array_exit(arr);

  cell->thread_id = std::this_thread::get_id();

  cell->reservation_time = std::chrono::steady_clock::now();

  /* Make sure the event is reset and also store the value of
  signal_count at which the event was reset. */
  os_event_t event = sync_cell_get_event(cell);
  cell->signal_count = os_event_reset(event);

  return (cell);
}

/** Frees the cell. NOTE! sync_array_wait_event frees the cell
 automatically! */
void sync_array_free_cell(
    sync_array_t *arr,  /*!< in: wait array */
    sync_cell_t *&cell) /*!< in/out: the cell in the array */
{
  sync_array_enter(arr);

  ut_a(cell->latch.mutex != nullptr);

  cell->waiting = false;
  cell->signal_count = 0;
  cell->latch.mutex = nullptr;

  /* Setup the list of free slots in the array */
  cell->line = arr->first_free_slot;

  arr->first_free_slot = cell - arr->cells;

  ut_a(arr->n_reserved > 0);
  arr->n_reserved--;

  if (arr->next_free_slot > arr->n_cells / 2 && arr->n_reserved == 0) {
#ifdef UNIV_DEBUG
    for (ulint i = 0; i < arr->next_free_slot; ++i) {
      cell = sync_array_get_nth_cell(arr, i);

      ut_ad(!cell->waiting);
      ut_ad(cell->latch.mutex == nullptr);
      ut_ad(cell->signal_count == 0);
    }
#endif /* UNIV_DEBUG */
    arr->next_free_slot = 0;
    arr->first_free_slot = ULINT_UNDEFINED;
  }
  sync_array_exit(arr);

  cell = nullptr;
}
void sync_array_detect_deadlock() {
  for (ulint i = 0; i < sync_array_size; ++i) {
    auto arr = sync_wait_array[i];
    sync_array_enter(arr);
    ut_d(rw_lock_debug_mutex_enter());
    ut_a(arr->last_scan % 2 == 0);
    ++arr->last_scan;
    size_t count{0};
    for (size_t i = 0; count < arr->n_reserved; ++i) {
      auto cell = sync_array_get_nth_cell(arr, i);
      if (cell->latch.mutex) {
        ++count;
        if (cell->last_scan == arr->last_scan + 1) {
          continue;
        }
        ut_a(cell->last_scan != arr->last_scan);
        sync_array_detect_deadlock(arr, cell, 0);
      }
    }
    ++arr->last_scan;
    ut_a(arr->last_scan % 2 == 0);
    ut_d(rw_lock_debug_mutex_exit());
    sync_array_exit(arr);
  }
}

/** This function should be called when a thread starts to wait on
a wait array cell. In the debug version this function checks
if the wait for a semaphore will result in a deadlock, in which
case prints info and asserts. */
void sync_array_wait_event(
    sync_array_t *arr,  /*!< in: wait array */
    sync_cell_t *&cell) /*!< in: index of the reserved cell */
{
  sync_array_enter(arr);

  ut_ad(!cell->waiting);
  ut_ad(cell->latch.mutex);
  ut_ad(std::this_thread::get_id() == cell->thread_id);

  cell->waiting = true;
#ifdef UNIV_DEBUG
  /* We use simple enter to the mutex below, because if
  we cannot acquire it at once, mutex_enter would call
  recursively sync_array routines, leading to trouble.
  rw_lock_debug_mutex freezes the debug lists. */

  rw_lock_debug_mutex_enter();
  ut_a(arr->last_scan % 2 == 0);
  ++arr->last_scan;
  sync_array_detect_deadlock(arr, cell, 0);
  ++arr->last_scan;
  ut_a(arr->last_scan % 2 == 0);
  rw_lock_debug_mutex_exit();
#endif
  sync_array_exit(arr);

  os_event_wait_low(sync_cell_get_event(cell), cell->signal_count);

  sync_array_free_cell(arr, cell);

  cell = nullptr;
}
/** Reports info about a mutex (seen locked a moment ago) into a file.
 @param[in] file    File where to print.
 @param[in] mutex   The mutex to describe.
 */
template <typename Mutex>
static void sync_array_mutex_print(FILE *file, const Mutex *mutex) {
  const auto &policy = mutex->policy();
#ifdef UNIV_DEBUG
  const char *name = policy.get_enter_filename();
  if (name == nullptr) {
    /* The mutex might have been released. */
    name = "NULL";
  }
#endif /* UNIV_DEBUG */
  const std::thread::id owner = mutex->peek_owner();

  fprintf(file,
          "Mutex at %p, %s, locked by %s\n"
#ifdef UNIV_DEBUG
          "Last time reserved in file %s line %lu"
#endif /* UNIV_DEBUG */
          "\n",
          (void *)mutex, policy.to_string().c_str(),
          (owner == std::thread::id{} ? "nobody" : to_string(owner).c_str())
#ifdef UNIV_DEBUG
              ,
          name, (ulong)policy.get_enter_line()
#endif /* UNIV_DEBUG */
  );
}

void sync_array_cell_print(FILE *file, const sync_cell_t *cell) {
  rw_lock_t *rwlock;
  ulint type;
  ulint writer;

  type = cell->request_type;

  fprintf(file,
          "--Thread %s has waited at %s line " ULINTPF " for %" PRId64
          " seconds the semaphore:\n",
          to_string(cell->thread_id).c_str(), innobase_basename(cell->file),
          cell->line,
          static_cast<int64_t>(
              std::chrono::duration_cast<std::chrono::seconds>(
                  std::chrono::steady_clock::now() - cell->reservation_time)
                  .count()));

  if (type == SYNC_MUTEX) {
    sync_array_mutex_print(file, cell->latch.mutex);
  } else if (type == SYNC_BUF_BLOCK) {
    sync_array_mutex_print(file, cell->latch.bpmutex);
  } else if (type == RW_LOCK_X || type == RW_LOCK_X_WAIT ||
             type == RW_LOCK_SX || type == RW_LOCK_S) {
    fputs(type == RW_LOCK_X
              ? "X-lock on"
              : type == RW_LOCK_X_WAIT
                    ? "X-lock (wait_ex) on"
                    : type == RW_LOCK_SX ? "SX-lock on" : "S-lock on",
          file);

    rwlock = cell->latch.lock;

    fprintf(file, " RW-latch at %p created in file %s line %lu\n",
            (void *)rwlock, innobase_basename(rwlock->clocation.filename),
            (ulong)rwlock->clocation.line);

    writer = rw_lock_get_writer(rwlock);

    if (writer != RW_LOCK_NOT_LOCKED) {
      fprintf(file, "a writer (thread id %s) has reserved it in mode %s\n",
              to_string(rwlock->writer_thread.load()).c_str(),
              writer == RW_LOCK_X
                  ? "exclusive"
                  : (writer == RW_LOCK_SX ? "SX" : "wait exclusive"));
    }

    const auto readers_count = rw_lock_get_reader_count(rwlock);
    fprintf(file, "number of readers " ULINTPF, readers_count);
    if (readers_count == 1) {
      fprintf(file, " (thread id %s)",
              to_string(rwlock->reader_thread.recover_if_single()).c_str());
    }
    fprintf(file,
            ", waiters flag %d"
            ", lock_word: %lx\n"
            "Last time read locked in file %s line %lu\n"
            "Last time write locked in file %s line %lu\n",
            rwlock->waiters.load(),
            static_cast<ulong>(rwlock->lock_word.load()),
            innobase_basename(rwlock->last_s_file_name),
            static_cast<ulong>(rwlock->last_s_line), rwlock->last_x_file_name,
            static_cast<ulong>(rwlock->last_x_line));
  } else {
    ut_error;
  }

  if (!cell->waiting) {
    fputs("wait has ended\n", file);
  }
}

/** Looks for a cell with the given thread id.
 @return pointer to cell or NULL if not found */
static sync_cell_t *sync_array_find_thread(
    sync_array_t *arr,      /*!< in: wait array */
    std::thread::id thread) /*!< in: thread id */
{
  for (ulint i = 0; i <= arr->next_free_slot; i++) {
    sync_cell_t *cell;

    cell = sync_array_get_nth_cell(arr, i);

    if (cell->latch.mutex != nullptr && cell->thread_id == thread) {
      return (cell); /* Found */
    }
  }

  return (nullptr); /* Not found */
}

/** Recursion step for deadlock detection.
@param[in]  arr     The wait array we limit our search for cycle to.
                    The caller must own the arr->mutex.
@param[in]  thread  The thread which blocking other threads, which we want to
                    find in the arr, to know if and why it's blocked, too
@param[in]  depth   The recursion depth
@return true iff deadlock detected (there might be false negatives) */
static bool sync_array_deadlock_step(sync_array_t *arr, std::thread::id thread,
                                     size_t depth) {
  const auto new_cell = sync_array_find_thread(arr, thread);
  if (new_cell == nullptr) {
    return false;
  }
  if (new_cell->last_scan == arr->last_scan) {
    /* Deadlock */
    fputs(
        "########################################\n"
        "DEADLOCK of threads detected!\n",
        stderr);

    return true;
  }
  if (new_cell->last_scan == arr->last_scan + 1) {
    /* Already processed */
    return false;
  }
  return sync_array_detect_deadlock(arr, new_cell, depth + 1);
}

/** A helper for sync_array_detect_deadlock() to handle the case when the cell
contains a thread waiting for a mutex.
@param[in] mutex    the mutex to check
@param[in] arr      wait array; NOTE! the caller must own the mutex to array
@param[in] cell     the cell which contains the mutex
@param[in] depth    recursion depth
*/
template <typename Mutex>
static bool sync_array_detect_mutex_deadlock(const Mutex *mutex,
                                             sync_array_t *arr,
                                             sync_cell_t *cell, size_t depth) {
  const std::thread::id thread = mutex->peek_owner();
  if (thread != std::thread::id{}) {
    if (sync_array_deadlock_step(arr, thread, depth)) {
      sync_array_cell_print(stderr, cell);

      return true;
    }
  }

  /* No deadlock */
  return false;
}
template <typename F>
bool sync_array_detect_rwlock_deadlock(sync_cell_t *cell, sync_array_t *arr,
                                       const size_t depth, F &&conflicts) {
  auto lock = cell->latch.lock;
  const auto waiter = cell->thread_id;
#ifdef UNIV_DEBUG
  for (auto debug : lock->debug_list) {
    /* If pass != 0, then we do not know which threads are responsible for
    releasing the lock, and no deadlock can be detected. */
    if (debug->pass) {
      continue;
    }
    const auto holder = debug->thread_id;
    if (std::forward<F>(conflicts)(debug->lock_type, waiter == holder)) {
      if (sync_array_deadlock_step(arr, holder, depth)) {
        sync_array_cell_print(stderr, cell);
        rw_lock_debug_print(stderr, debug);
        return true;
      }
    }
  }
#else
  /* We don't have lock->debug_list, so can't identify all threads owning the
  latch, but we still have some clues available.
  We can identify the only thread which has (wait) x-lock by looking at
  lock->writer_thread, unless the lock was passed to another thread which
  requires the lock->recursive to be false.
  We don't track all s-locks, but if there is exactly one s-lock, then we can
  identify its owner with lock->reader_thread.
  You might be worried about race-condition: could it happen that the holder we
  identify here, will soon release the latch, and thus we will report a "fake"
  deadlock? Not really, because the first thing sync_array_deadlock_step() will
  do is to check if the holder is itself waiting for something in the arr we
  keep latched - if it isn't waiting, we will ignore it, and if it is, then it's
  not executing thus can't release the rw_lock we analyze here. */
  const std::thread::id none{};
  std::thread::id suspects[2]{};
  size_t suspects_cnt = 0;
  {
    if (lock->recursive.load()) {
      /* You might be worried about a race-condition: could it happen that the
      recursive was set to true by some other thread, which now released the
      lock, and then the writer_thread acquired it to pass it to somebody else,
      and thus set recursive to false?
      We double check that recursive is still true after loading writer_thread,
      and we only report a deadlock if writer_thread is itself not executing.
      So if a deadlock is reported it must be writer_thread who set recursive to
      true and is still holding this latch.
      Note that we always pass RW_LOCK_X as the granted request_type of the
      blocking thread, even though it could still be waiting for RW_LOCK_WAIT_X.
      This doesn't really matter as existing callbacks do not differentiate,
      and more importantly they should rather care about "kind of access right"
      than "state of latching" (debug_list's lock_type), or "awaited event"
      (cell's request type). Alas we use the same enum for these concepts.*/
      const auto thread = lock->writer_thread.load();
      if (lock->recursive.load() && thread != none &&
          std::forward<F>(conflicts)(RW_LOCK_X, thread == waiter)) {
        suspects[suspects_cnt++] = thread;
      }
    }
  }
  {
    if (rw_lock_get_reader_count(lock) == 1) {
      /* You might be worried about a race-condition: could it happen that the
      number of s-lockers has changed from 1 to say 3, and the XOR we recover in
      the line below corresponds to some unrelated fourth thread?
      For example 0x101 xor 0x110 xor 0x111 = 0x100.
      This isn't a problem in practice, because conflicts(RW_LOCKS,..) is true
      only if the waiter waits for RW_LOCK_X_WAIT, which means it already has
      announced its presence via lock_word, so no more s-locks should be granted
      to not starve it. Thus the number of readers can only decrease. We double
      check it is still 1 after recovering the xor, so it can't be 0 nor torn.*/
      const auto thread = lock->reader_thread.recover_if_single();
      if (rw_lock_get_reader_count(lock) == 1 && thread != none &&
          std::forward<F>(conflicts)(RW_LOCK_S, thread == waiter)) {
        suspects[suspects_cnt++] = thread;
      }
    }
  }
  for (size_t i = 0; i < suspects_cnt; ++i) {
    if (sync_array_deadlock_step(arr, suspects[i], depth)) {
      sync_array_cell_print(stderr, cell);
      return true;
    }
  }
#endif /* UNIV_DEBUG */
  return false;
}

static bool sync_array_detect_deadlock_low(sync_array_t *arr, sync_cell_t *cell,
                                           size_t depth) {
  ut_a(arr);
  ut_a(cell);
  ut_ad(cell->latch.mutex != nullptr);
  ut_ad(depth < 100);

  if (!cell->waiting) {
    /* No deadlock here */
    return (false);
  }

  switch (cell->request_type) {
    case SYNC_MUTEX: {
      return sync_array_detect_mutex_deadlock(cell->latch.mutex, arr, cell,
                                              depth);
    }

    case SYNC_BUF_BLOCK: {
      return sync_array_detect_mutex_deadlock(cell->latch.bpmutex, arr, cell,
                                              depth);
    }
    case RW_LOCK_X:
      /* The x-lock request can block infinitely only if someone (cannot be the
      cell thread) has (wait) x-lock or sx-lock, and he is blocked by start
      thread */
      return sync_array_detect_rwlock_deadlock(
          cell, arr, depth, [](auto request_type, bool is_my) {
            return !is_my && request_type != RW_LOCK_S;
          });
    case RW_LOCK_X_WAIT:
      /* The (wait) x-lock request can block infinitely only if someone (can be
      also the cell thread) is holding an s-lock */
      return sync_array_detect_rwlock_deadlock(
          cell, arr, depth,
          [](auto request_type, bool) { return request_type == RW_LOCK_S; });
    case RW_LOCK_SX:
      /* The sx-lock request can block infinitely only if someone (cannot be
      the cell thread) is holding a (wait) x-lock or sx-lock, and he is blocked
      by start thread */
      return sync_array_detect_rwlock_deadlock(
          cell, arr, depth, [](auto request_type, bool is_my) {
            return !is_my && request_type != RW_LOCK_S;
          });
    case RW_LOCK_S:
      /* The s-lock request can block infinitely only if someone (can also be
      the cell thread) is holding a (wait) x-lock, and he is blocked by start
      thread */
      return sync_array_detect_rwlock_deadlock(
          cell, arr, depth, [](auto request_type, bool) {
            return request_type == RW_LOCK_X || request_type == RW_LOCK_X_WAIT;
          });
    default:
      ut_error;
  }
}
static bool sync_array_detect_deadlock(sync_array_t *const arr,
                                       sync_cell_t *const cell, size_t depth) {
  // there's an ongoing scan
  ut_a(arr->last_scan % 2 == 1);
  // do not visit a cell which is already on stack
  ut_a(cell->last_scan != arr->last_scan);
  // do not waste time processing already processed cell
  ut_a(cell->last_scan != arr->last_scan + 1);
  // mark the fact the cell is on stack
  cell->last_scan = arr->last_scan;
  const bool deadlocked = sync_array_detect_deadlock_low(arr, cell, depth);
  // mark the fact the cell as fully processed;
  cell->last_scan++;
  if (deadlocked && depth == 0) {
#ifdef UNIV_NO_ERR_MSGS
    ib::fatal(UT_LOCATION_HERE)
#else
    ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_1157)
#endif /* UNIV_NO_ERR_MSGS */
        << "######################################## Deadlock Detected!";
  }
  return deadlocked;
}

/** Determines if we can wake up the thread waiting for a semaphore. */
static bool sync_arr_cell_can_wake_up(
    sync_cell_t *cell) /*!< in: cell to search */
{
  rw_lock_t *lock;

  switch (cell->request_type) {
    case SYNC_MUTEX:
      return !cell->latch.mutex->is_locked();

    case SYNC_BUF_BLOCK:
      return !cell->latch.bpmutex->is_locked();

    case RW_LOCK_X:
    case RW_LOCK_SX:
      lock = cell->latch.lock;

      os_rmb;
      if (lock->lock_word > X_LOCK_HALF_DECR) {
        /* Either unlocked or only read locked. */

        return (true);
      }

      break;

    case RW_LOCK_X_WAIT:

      lock = cell->latch.lock;

      /* lock_word == 0 means all readers or sx have left */
      os_rmb;
      if (lock->lock_word == 0) {
        return (true);
      }
      break;

    case RW_LOCK_S:

      lock = cell->latch.lock;

      /* lock_word > 0 means no writer or reserved writer */
      os_rmb;
      if (lock->lock_word > 0) {
        return (true);
      }
  }

  return (false);
}

/** Increments the signalled count. */
void sync_array_object_signalled() { ++sg_count; }

/** If the wakeup algorithm does not work perfectly at semaphore releases,
 this function will do the waking (see the comment in mutex_exit). This
 function should be called about every 1 second in the server.

 Note that there's a race condition between this thread and mutex_exit
 changing the lock_word and calling signal_object, so sometimes this finds
 threads to wake up even when nothing has gone wrong. */
static void sync_array_wake_threads_if_sema_free_low(
    sync_array_t *arr) /* in/out: wait array */
{
  sync_array_enter(arr);

  for (ulint i = 0; i < arr->next_free_slot; ++i) {
    sync_cell_t *cell;

    cell = sync_array_get_nth_cell(arr, i);

    if (cell->latch.mutex != nullptr && sync_arr_cell_can_wake_up(cell)) {
      os_event_t event;

      event = sync_cell_get_event(cell);

      os_event_set(event);
    }
  }

  sync_array_exit(arr);
}

/** If the wakeup algorithm does not work perfectly at semaphore releases,
 this function will do the waking (see the comment in mutex_exit). This
 function should be called about every 1 second in the server.

 Note that there's a race condition between this thread and mutex_exit
 changing the lock_word and calling signal_object, so sometimes this finds
 threads to wake up even when nothing has gone wrong. */
void sync_arr_wake_threads_if_sema_free(void) {
  for (ulint i = 0; i < sync_array_size; ++i) {
    sync_array_wake_threads_if_sema_free_low(sync_wait_array[i]);
  }
}

/** Prints warnings of long semaphore waits to stderr.
 @return true if fatal semaphore wait threshold was exceeded */
static bool sync_array_print_long_waits_low(
    sync_array_t *arr,       /*!< in: sync array instance */
    std::thread::id *waiter, /*!< out: longest waiting thread */
    const void **sema,       /*!< out: longest-waited-for semaphore */
    bool *noticed)           /*!< out: true if long wait noticed */
{
  bool fatal = false;
  std::chrono::steady_clock::duration longest_diff{};
  auto fatal_timeout = get_srv_fatal_semaphore_wait_threshold();

  /* For huge tables, skip the check during CHECK TABLE etc... */
  if (0 < srv_fatal_semaphore_wait_extend.load()) {
    return false;
  }

#ifdef UNIV_DEBUG_VALGRIND
  /* Increase the timeouts if running under valgrind because it executes
  extremely slowly. UNIV_DEBUG_VALGRIND does not necessary mean that
  we are running under valgrind but we have no better way to tell.
  See Bug#58432 innodb.innodb_bug56143 fails under valgrind
  for an example */
  constexpr std::chrono::minutes timeout{40};
  fatal_timeout *= 10;
#else
  constexpr std::chrono::minutes timeout{4};
#endif

  for (ulint i = 0; i < arr->next_free_slot; i++) {
    sync_cell_t *cell;
    void *latch;

    cell = sync_array_get_nth_cell(arr, i);

    latch = cell->latch.mutex;

    if (latch == nullptr || !cell->waiting) {
      continue;
    }

    const auto time_diff =
        std::chrono::steady_clock::now() - cell->reservation_time;

    if (time_diff > timeout) {
#ifdef UNIV_NO_ERR_MSGS
      ib::warn()
#else
      ib::warn(ER_IB_MSG_1160)
#endif /* UNIV_NO_ERR_MSGS */
          << "A long semaphore wait:";

      sync_array_cell_print(stderr, cell);
      *noticed = true;
    }

    if (time_diff > fatal_timeout) {
      fatal = true;
    }

    if (longest_diff < time_diff) {
      longest_diff = time_diff;
      *sema = latch;
      *waiter = cell->thread_id;
    }
  }

  return fatal;
}

/** Prints warnings of long semaphore waits to stderr.
 @return true if fatal semaphore wait threshold was exceeded */
bool sync_array_print_long_waits(
    std::thread::id *waiter, /*!< out: longest waiting thread */
    const void **sema)       /*!< out: longest-waited-for semaphore */
{
  ulint i;
  bool fatal = false;
  bool noticed = false;

  for (i = 0; i < sync_array_size; ++i) {
    sync_array_t *arr = sync_wait_array[i];

    sync_array_enter(arr);

    if (sync_array_print_long_waits_low(arr, waiter, sema, &noticed)) {
      fatal = true;
    }

    sync_array_exit(arr);
  }

  if (noticed) {
    fprintf(stderr,
            "InnoDB: ###### Starts InnoDB Monitor"
            " for 30 secs to print diagnostic info:\n");

    /* If some crucial semaphore is reserved, then also the InnoDB
    Monitor can hang, and we do not get diagnostics. Since in
    many cases an InnoDB hang is caused by a pwrite() or a pread()
    call hanging inside the operating system, let us print right
    now the values of pending calls of these. */

    fprintf(stderr, "InnoDB: Pending preads %lu, pwrites %lu\n",
            (ulong)os_n_pending_reads, (ulong)os_n_pending_writes);

    srv_innodb_needs_monitoring++;

#ifndef UNIV_NO_ERR_MSGS
    lock_set_timeout_event();
#endif /* !UNIV_NO_ERR_MSGS */

    std::this_thread::sleep_for(std::chrono::seconds(30));

    srv_innodb_needs_monitoring--;
    fprintf(stderr,
            "InnoDB: ###### Diagnostic info printed"
            " to the standard error stream\n");
  }

  return (fatal);
}

/** Prints info of the wait array. */
static void sync_array_print_info_low(
    FILE *file,        /*!< in: file where to print */
    sync_array_t *arr) /*!< in: wait array */
{
  ulint i;
  ulint count = 0;

  fprintf(file, "OS WAIT ARRAY INFO: reservation count " ULINTPF "\n",
          arr->res_count);

  for (i = 0; count < arr->n_reserved; ++i) {
    sync_cell_t *cell;

    cell = sync_array_get_nth_cell(arr, i);

    if (cell->latch.mutex != nullptr) {
      count++;
      sync_array_cell_print(file, cell);
    }
  }
}

/** Prints info of the wait array. */
static void sync_array_print_info(FILE *file, /*!< in: file where to print */
                                  sync_array_t *arr) /*!< in: wait array */
{
  sync_array_enter(arr);

  sync_array_print_info_low(file, arr);

  sync_array_exit(arr);
}

/** Create the primary system wait array(s), they are protected by an OS mutex
 */
void sync_array_init(ulint n_threads) /*!< in: Number of slots to
                                      create in all arrays */
{
  ut_a(sync_wait_array == nullptr);
  ut_a(srv_sync_array_size > 0);
  ut_a(n_threads > 0);

  sync_array_size = srv_sync_array_size;

  sync_wait_array = ut::new_arr_withkey<sync_array_t *>(
      UT_NEW_THIS_FILE_PSI_KEY, ut::Count{sync_array_size});

  ulint n_slots = 1 + (n_threads - 1) / sync_array_size;

  for (ulint i = 0; i < sync_array_size; ++i) {
    sync_wait_array[i] =
        ut::new_withkey<sync_array_t>(UT_NEW_THIS_FILE_PSI_KEY, n_slots);
  }
}

/** Close sync array wait sub-system. */
void sync_array_close(void) {
  for (ulint i = 0; i < sync_array_size; ++i) {
    sync_array_free(sync_wait_array[i]);
  }

  ut::delete_arr(sync_wait_array);
  sync_wait_array = nullptr;
}

/** Print info about the sync array(s). */
void sync_array_print(FILE *file) /*!< in/out: Print to this stream */
{
  for (ulint i = 0; i < sync_array_size; ++i) {
    sync_array_print_info(file, sync_wait_array[i]);
  }

  fprintf(file, "OS WAIT ARRAY INFO: signal count " ULINTPF "\n", sg_count);
}
