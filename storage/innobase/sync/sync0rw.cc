/*****************************************************************************

Copyright (c) 1995, 2022, Oracle and/or its affiliates.
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

/** @file sync/sync0rw.cc
 The read-write lock (for thread synchronization)

 Created 9/11/1995 Heikki Tuuri
 *******************************************************/

#include "sync0rw.h"

#include <my_sys.h>
#include <sys/types.h>

#include "ha_prototypes.h"
#include "mem0mem.h"
#include "os0event.h"
#include "os0thread.h"
#include "srv0mon.h"
#include "srv0srv.h"
#include "sync0debug.h"

/*
        IMPLEMENTATION OF THE RW_LOCK
        =============================
The status of a rw_lock is held in lock_word. The initial value of lock_word is
X_LOCK_DECR. lock_word is decremented by 1 for each s-lock and by X_LOCK_DECR
or 1 for each x-lock. This describes the lock state for each value of lock_word:

lock_word == X_LOCK_DECR:       Unlocked.
X_LOCK_HALF_DECR < lock_word < X_LOCK_DECR:
                                S locked, no waiting writers.
                                (X_LOCK_DECR - lock_word) is the number
                                of S locks.
lock_word == X_LOCK_HALF_DECR:  SX locked, no waiting writers.
0 < lock_word < X_LOCK_HALF_DECR:
                                SX locked AND S locked, no waiting writers.
                                (X_LOCK_HALF_DECR - lock_word) is the number
                                of S locks.
lock_word == 0:                 X locked, no waiting writers.
-X_LOCK_HALF_DECR < lock_word < 0:
                                S locked, with a waiting writer.
                                (-lock_word) is the number of S locks.
lock_word == -X_LOCK_HALF_DECR: X locked and SX locked, no waiting writers.
-X_LOCK_DECR < lock_word < -X_LOCK_HALF_DECR:
                                S locked, with a waiting writer
                                which has SX lock.
                                -(lock_word + X_LOCK_HALF_DECR) is the number
                                of S locks.
lock_word == -X_LOCK_DECR:      X locked with recursive X lock (2 X locks).
-(X_LOCK_DECR + X_LOCK_HALF_DECR) < lock_word < -X_LOCK_DECR:
                                X locked. The number of the X locks is:
                                2 - (lock_word + X_LOCK_DECR)
lock_word == -(X_LOCK_DECR + X_LOCK_HALF_DECR):
                                X locked with recursive X lock (2 X locks)
                                and SX locked.
lock_word < -(X_LOCK_DECR + X_LOCK_HALF_DECR):
                                X locked and SX locked.
                                The number of the X locks is:
                                2 - (lock_word + X_LOCK_DECR + X_LOCK_HALF_DECR)

 LOCK COMPATIBILITY MATRIX
    S SX  X
 S  +  +  -
 SX +  -  -
 X  -  -  -

The lock_word is always read and updated atomically and consistently, so that
it always represents the state of the lock, and the state of the lock changes
with a single atomic operation. This lock_word holds all of the information
that a thread needs in order to determine if it is eligible to gain the lock
or if it must spin or sleep. The one exception to this is that writer_thread
must be verified before recursive write locks: to solve this scenario, we make
writer_thread readable by all threads, but only writeable by the x-lock or
sx-lock holder.

The other members of the lock obey the following rules to remain consistent:

recursive:      This flag is true iff the lock->writer_thread is allowed to take
                the x or sx lock recursively.

                In particular you should follow these rules:
                - make sure to change recursive to `false` before
                  releasing the lock
                - make sure to change recursive to `false` (but do not reset the
                  `writer_thread` as it will remove debug info) before passing
                  the lock to other thread
                - make sure to put your thread's native handle before setting
                  recursive to `true`
                - make sure that when you want to read both `recursive` and
                  `writer_thread` you do this in this precise order
writer_thread:  Is used only in recursive x-locking.
                This field is initialized to an impossible thread native handle
                value at lock creation time and is updated atomically when sx or
                x-lock is being acquired or when move_ownership is called.
                A thread is only allowed to set the value of this field to its
                thread_id i.e.: a thread cannot set writer_thread to some other
                thread's id.
                The only reasonable way (except reporting in debug info) to use
                this field is to compare it to own thread's native handle
                AFTER checking that lock->recursive flag is set, to see if we
                are the current writer.
waiters:        May be set to 1 anytime, but to avoid unnecessary wake-up
                signals, it should only be set to 1 when there are threads
                waiting on event. Must be 1 when a writer starts waiting to
                ensure the current x-locking thread sends a wake-up signal
                during unlock. May only be reset to 0 immediately before a
                a wake-up signal is sent to event. On most platforms, a
                memory barrier is required after waiters is set, and before
                verifying lock_word is still held, to ensure some unlocker
                really does see the flags new value.
event:          Threads wait on event for read or writer lock when another
                thread has an x-lock or an x-lock reservation (wait_ex). A
                thread may only wait on event after performing the following
                actions in order:
                   (1) Record the counter value of event (with os_event_reset).
                   (2) Set waiters to 1.
                   (3) Verify lock_word <= 0.
                (1) must come before (2) to ensure signal is not missed.
                (2) must come before (3) to ensure a signal is sent.
                These restrictions force the above ordering.
                Immediately before sending the wake-up signal, we should:
                   (1) Verify lock_word == X_LOCK_DECR (unlocked)
                   (2) Reset waiters to 0.
wait_ex_event:  A thread may only wait on the wait_ex_event after it has
                performed the following actions in order:
                   (1) Decrement lock_word by X_LOCK_DECR.
                   (2) Record counter value of wait_ex_event (os_event_reset,
                       called from sync_array_reserve_cell).
                   (3) Verify that lock_word < 0.
                (1) must come first to ensures no other threads become reader
                or next writer, and notifies unlocker that signal must be sent.
                (2) must come before (3) to ensure the signal is not missed.
                These restrictions force the above ordering.
                Immediately before sending the wake-up signal, we should:
                   Verify lock_word == 0 (waiting thread holds x_lock)
*/

/** The global list of rw-locks */
rw_lock_list_t rw_lock_list{};
ib_mutex_t rw_lock_list_mutex;

#ifdef UNIV_DEBUG
/** Creates a debug info struct. */
static rw_lock_debug_t *rw_lock_debug_create(void);
/** Frees a debug info struct. */
static void rw_lock_debug_free(rw_lock_debug_t *info);

/** Creates a debug info struct.
 @return own: debug info struct */
static rw_lock_debug_t *rw_lock_debug_create(void) {
  return ((rw_lock_debug_t *)ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY,
                                                sizeof(rw_lock_debug_t)));
}

/** Frees a debug info struct. */
static void rw_lock_debug_free(rw_lock_debug_t *info) { ut::free(info); }
#endif /* UNIV_DEBUG */

void rw_lock_create_func(rw_lock_t *lock,
                         IF_DEBUG(latch_id_t id, ) ut::Location clocation) {
#if !defined(UNIV_PFS_RWLOCK)
  /* It should have been created in pfs_rw_lock_create_func() */
  new (lock) rw_lock_t();
#endif /* !UNIV_PFS_RWLOCK */
  ut_ad(lock->magic_n == rw_lock_t::MAGIC_N);

  /* If this is the very first time a synchronization object is
  created, then the following call initializes the sync system. */

  lock->lock_word = X_LOCK_DECR;
  lock->waiters = false;

  lock->recursive.store(false, std::memory_order_relaxed);
  lock->sx_recursive = 0;
  lock->writer_thread = std::thread::id{};

#ifdef UNIV_DEBUG
  lock->m_rw_lock = true;

  lock->m_id = id;
  ut_a(lock->m_id != LATCH_ID_NONE);

#endif /* UNIV_DEBUG */

  lock->clocation = clocation;

  /* This should hold in practice. If it doesn't then we need to
  split the source file anyway. Or create the locks on lines
  less than 65536. cline is uint16_t. */
  ut_ad(clocation.line <=
        std::numeric_limits<decltype(lock->clocation.line)>::max());

  lock->count_os_wait = 0;
  lock->last_s_file_name = "not yet reserved";
  lock->last_x_file_name = "not yet reserved";
  lock->last_s_line = 0;
  lock->last_x_line = 0;
  lock->event = os_event_create();
  lock->wait_ex_event = os_event_create();

  lock->is_block_lock = false;

  mutex_enter(&rw_lock_list_mutex);

  ut_ad(UT_LIST_GET_FIRST(rw_lock_list) == nullptr ||
        UT_LIST_GET_FIRST(rw_lock_list)->magic_n == rw_lock_t::MAGIC_N);

  UT_LIST_ADD_FIRST(rw_lock_list, lock);

  mutex_exit(&rw_lock_list_mutex);
}

/** Calling this function is obligatory only if the memory buffer containing
 the rw-lock is freed. Removes an rw-lock object from the global list. The
 rw-lock is checked to be in the non-locked state. */
void rw_lock_free_func(rw_lock_t *lock) /*!< in/out: rw-lock */
{
  /* We did an in-place new in rw_lock_create_func() */
  lock->~rw_lock_t();
}

rw_lock_t::~rw_lock_t() {
  os_rmb;
  ut_ad(rw_lock_validate(this));
  ut_a(lock_word == X_LOCK_DECR);

  mutex_enter(&rw_lock_list_mutex);

  os_event_destroy(event);

  os_event_destroy(wait_ex_event);

  UT_LIST_REMOVE(rw_lock_list, this);

  mutex_exit(&rw_lock_list_mutex);

  ut_ad(magic_n == MAGIC_N);
  ut_d(magic_n = 0);
}

void rw_lock_s_lock_spin(rw_lock_t *lock, ulint pass, ut::Location location) {
  ulint i = 0; /* spin round count */
  sync_array_t *sync_arr;
  uint64_t count_os_wait = 0;

  /* We reuse the thread id to index into the counter, cache
  it here for efficiency. */

  ut_ad(rw_lock_validate(lock));

lock_loop:

  /* Spin waiting for the writer field to become free */
  os_rmb;
  while (i < srv_n_spin_wait_rounds && lock->lock_word <= 0) {
    if (srv_spin_wait_delay) {
      ut_delay(ut::random_from_interval(0, srv_spin_wait_delay));
    }

    i++;
  }

  if (i >= srv_n_spin_wait_rounds) {
    std::this_thread::yield();
  }

  /* We try once again to obtain the lock */
  if (rw_lock_s_lock_low(lock, pass, location)) {
    if (count_os_wait > 0) {
      lock->count_os_wait += static_cast<uint32_t>(count_os_wait);
    }

    return; /* Success */
  } else {
    if (i < srv_n_spin_wait_rounds) {
      goto lock_loop;
    }

    ++count_os_wait;

    sync_cell_t *cell;

    sync_arr =
        sync_array_get_and_reserve_cell(lock, RW_LOCK_S, location, &cell);

    /* Set waiters before checking lock_word to ensure wake-up
    signal is sent. This may lead to some unnecessary signals. */
    rw_lock_set_waiter_flag(lock);

    if (rw_lock_s_lock_low(lock, pass, location)) {
      sync_array_free_cell(sync_arr, cell);

      if (count_os_wait > 0) {
        lock->count_os_wait += static_cast<uint32_t>(count_os_wait);
      }

      return; /* Success */
    }

    /* see comments in trx_commit_low() to
    before_trx_state_committed_in_memory explaining
    this care to invoke the following sync check.*/
#ifdef UNIV_DEBUG
    if (lock->get_level() != SYNC_DICT_OPERATION) {
      DEBUG_SYNC_C("rw_s_lock_waiting");
    }
#endif
    sync_array_wait_event(sync_arr, cell);

    i = 0;

    goto lock_loop;
  }
}

/** This function is used in the insert buffer to move the ownership of an
 x-latch on a buffer frame to the current thread. The x-latch was set by
 the buffer read operation and it protected the buffer frame while the
 read was done. The ownership is moved because we want that the current
 thread is able to acquire a second x-latch which is stored in an mtr.
 This, in turn, is needed to pass the debug checks of index page
 operations. */
void rw_lock_x_lock_move_ownership(
    rw_lock_t *lock) /*!< in: lock which was x-locked in the
                     buffer read */
{
  ut_ad(rw_lock_is_locked(lock, RW_LOCK_X));

  rw_lock_set_writer_id_and_recursion_flag(lock, true);
}

/** Function for the next writer to call. Waits for readers to exit.
 The caller must have already decremented lock_word by X_LOCK_DECR.
 @param[in] lock pointer to rw-lock
 @param[in] pass pass value; != 0, if the lock will be passed to another thread
 to unlock
 @param[in] threshold threshold to wait for
 @param[in] file_name file name where lock requested
 @param[in] line line where requested */
static inline void rw_lock_x_lock_wait_func(rw_lock_t *lock,
                                            IF_DEBUG(ulint pass, )
                                                lint threshold,
                                            const char *file_name, ulint line) {
  ulint i = 0;
  sync_array_t *sync_arr;
  uint64_t count_os_wait = 0;

  os_rmb;
  ut_ad(lock->lock_word <= threshold);

  while (lock->lock_word < threshold) {
    if (srv_spin_wait_delay) {
      ut_delay(ut::random_from_interval(0, srv_spin_wait_delay));
    }

    if (i < srv_n_spin_wait_rounds) {
      i++;
      os_rmb;
      continue;
    }

    /* If there is still a reader, then go to sleep.*/
    sync_cell_t *cell;

    sync_arr = sync_array_get_and_reserve_cell(lock, RW_LOCK_X_WAIT,
                                               {file_name, line}, &cell);

    i = 0;

    /* Check lock_word to ensure wake-up isn't missed.*/
    if (lock->lock_word < threshold) {
      ++count_os_wait;

      /* Add debug info as it is needed to detect possible
      deadlock. We must add info for WAIT_EX thread for
      deadlock detection to work properly. */
      ut_d(rw_lock_add_debug_info(lock, pass, RW_LOCK_X_WAIT,
                                  {file_name, line}));

      sync_array_wait_event(sync_arr, cell);

      ut_d(rw_lock_remove_debug_info(lock, pass, RW_LOCK_X_WAIT));

      /* It is possible to wake when lock_word < 0.
      We must pass the while-loop check to proceed.*/

    } else {
      sync_array_free_cell(sync_arr, cell);
      break;
    }
  }

  if (count_os_wait > 0) {
    lock->count_os_wait += static_cast<uint32_t>(count_os_wait);
  }
}

/** Function for the next writer to call. Waits for readers to exit.
 The caller must have already decremented lock_word by X_LOCK_DECR.
 @param[in] lock pointer to rw-lock
 @param[in] pass pass value; != 0, if the lock will be passed to another thread
 to unlock
 @param[in] treshold threshold to wait for
 @param[in] file_name file name where lock requested
 @param[in] line line where requested */
static inline void rw_lock_x_lock_wait(rw_lock_t *lock,
                                       ulint pass [[maybe_unused]],
                                       lint treshold, const char *file_name,
                                       ulint line) {
  rw_lock_x_lock_wait_func(lock, IF_DEBUG(pass, ) treshold, file_name, line);
}

/** Low-level function for acquiring an exclusive lock.
 @return false if did not succeed, true if success. */
static inline bool rw_lock_x_lock_low(
    rw_lock_t *lock,       /*!< in: pointer to rw-lock */
    ulint pass,            /*!< in: pass value; != 0, if the lock will
                           be passed to another thread to unlock */
    const char *file_name, /*!< in: file name where lock requested */
    ulint line)            /*!< in: line where requested */
{
  if (rw_lock_lock_word_decr(lock, X_LOCK_DECR, X_LOCK_HALF_DECR)) {
    /* lock->recursive == true implies that the lock->writer_thread is the
    current writer. As we are going to write our own thread id in that field it
    must be the case that the current writer_thread value is not the current
    writer anymore, thus recursive flag must be false.  */
    ut_a(!lock->recursive.load(std::memory_order_relaxed));

    /* Decrement occurred: we are writer or next-writer. */
    rw_lock_set_writer_id_and_recursion_flag(lock, !pass);

    rw_lock_x_lock_wait(lock, pass, 0, file_name, line);

  } else {
    if (!pass && lock->recursive.load(std::memory_order_acquire) &&
        lock->writer_thread.load(std::memory_order_relaxed) ==
            std::this_thread::get_id()) {
      /* Decrement failed: An X or SX lock is held by either
      this thread or another. Try to relock. */
      /* Other s-locks can be allowed. If it is request x
      recursively while holding sx lock, this x lock should
      be along with the latching-order. */

      /* The existing X or SX lock is from this thread */
      if (rw_lock_lock_word_decr(lock, X_LOCK_DECR, 0)) {
        /* There is at least one SX-lock from this
        thread, but no X-lock. */

        /* Wait for any the other S-locks to be
        released. */
        rw_lock_x_lock_wait(lock, pass, -X_LOCK_HALF_DECR, file_name, line);

      } else {
        /* At least one X lock by this thread already
        exists. Add another. */
        if (lock->lock_word == 0 || lock->lock_word == -X_LOCK_HALF_DECR) {
          lock->lock_word -= X_LOCK_DECR;
        } else {
          ut_ad(lock->lock_word <= -X_LOCK_DECR);
          --lock->lock_word;
        }
      }
    } else {
      /* Another thread locked before us */
      return false;
    }
  }

  ut_d(rw_lock_add_debug_info(lock, pass, RW_LOCK_X, {file_name, line}));

  lock->last_x_file_name = file_name;
  ut_ad(line <= std::numeric_limits<decltype(lock->last_x_line)>::max());
  lock->last_x_line = line;

  return true;
}

bool rw_lock_sx_lock_low(rw_lock_t *lock, ulint pass, ut::Location location) {
  if (rw_lock_lock_word_decr(lock, X_LOCK_HALF_DECR, X_LOCK_HALF_DECR)) {
    /* lock->recursive == true implies that the lock->writer_thread is the
    current writer. As we are going to write our own thread id in that field it
    must be the case that the current writer_thread value is not the current
    writer anymore, thus recursive flag must be false.  */
    ut_a(!lock->recursive.load(std::memory_order_relaxed));

    /* Decrement occurred: we are the SX lock owner. */
    rw_lock_set_writer_id_and_recursion_flag(lock, !pass);

    lock->sx_recursive = 1;

  } else {
    /* Decrement failed: It already has an X or SX lock by this
    thread or another thread. If it is this thread, relock,
    else fail. */
    if (!pass && lock->recursive.load(std::memory_order_acquire) &&
        lock->writer_thread.load(std::memory_order_relaxed) ==
            std::this_thread::get_id()) {
      /* This thread owns an X or SX lock */
      if (lock->sx_recursive++ == 0) {
        /* This thread is making first SX-lock request
        and it must be holding at least one X-lock here
        because:

        * There can't be a WAIT_EX thread because we are
          the thread which has it's thread_id written in
          the writer_thread field and we are not waiting.

        * Any other X-lock thread cannot exist because
          it must update recursive flag only after
          updating the thread_id. Had there been
          a concurrent X-locking thread which succeeded
          in decrementing the lock_word it must have
          written it's thread_id before setting the
          recursive flag. As we cleared the if()
          condition above therefore we must be the only
          thread working on this lock and it is safe to
          read and write to the lock_word. */

        ut_ad((lock->lock_word == 0) ||
              ((lock->lock_word <= -X_LOCK_DECR) &&
               (lock->lock_word > -(X_LOCK_DECR + X_LOCK_HALF_DECR))));
        lock->lock_word -= X_LOCK_HALF_DECR;
      }
    } else {
      /* Another thread locked before us */
      return false;
    }
  }

  ut_d(rw_lock_add_debug_info(lock, pass, RW_LOCK_SX, location));

  lock->last_x_file_name = location.filename;

  ut_ad(location.line <=
        std::numeric_limits<decltype(lock->last_x_line)>::max());
  lock->last_x_line = location.line;

  return true;
}

void rw_lock_x_lock_func(rw_lock_t *lock, ulint pass, ut::Location location) {
  ulint i = 0;
  sync_array_t *sync_arr;
  uint64_t count_os_wait = 0;
  bool spinning = false;

  ut_ad(rw_lock_validate(lock));
  ut_ad(!rw_lock_own(lock, RW_LOCK_S));

lock_loop:

  if (rw_lock_x_lock_low(lock, pass, location.filename, location.line)) {
    if (count_os_wait > 0) {
      lock->count_os_wait += static_cast<uint32_t>(count_os_wait);
    }

    /* Locking succeeded */
    return;

  } else {
    if (!spinning) {
      spinning = true;
    }

    /* Spin waiting for the lock_word to become free */
    os_rmb;
    while (i < srv_n_spin_wait_rounds && lock->lock_word <= X_LOCK_HALF_DECR) {
      if (srv_spin_wait_delay) {
        ut_delay(ut::random_from_interval(0, srv_spin_wait_delay));
      }

      i++;
    }

    if (i >= srv_n_spin_wait_rounds) {
      std::this_thread::yield();

    } else {
      goto lock_loop;
    }
  }

  sync_cell_t *cell;

  sync_arr = sync_array_get_and_reserve_cell(lock, RW_LOCK_X, location, &cell);

  /* Waiters must be set before checking lock_word, to ensure signal
  is sent. This could lead to a few unnecessary wake-up signals. */
  rw_lock_set_waiter_flag(lock);

  if (rw_lock_x_lock_low(lock, pass, location.filename, location.line)) {
    sync_array_free_cell(sync_arr, cell);

    if (count_os_wait > 0) {
      lock->count_os_wait += static_cast<uint32_t>(count_os_wait);
    }

    /* Locking succeeded */
    return;
  }

  ++count_os_wait;

  sync_array_wait_event(sync_arr, cell);

  i = 0;

  goto lock_loop;
}

void rw_lock_sx_lock_func(rw_lock_t *lock, ulint pass, ut::Location location) {
  ulint i = 0;
  sync_array_t *sync_arr;
  uint64_t count_os_wait = 0;

  ut_ad(rw_lock_validate(lock));
  ut_ad(!rw_lock_own(lock, RW_LOCK_S));

lock_loop:

  if (rw_lock_sx_lock_low(lock, pass, location)) {
    if (count_os_wait > 0) {
      lock->count_os_wait += static_cast<uint32_t>(count_os_wait);
    }

    /* Locking succeeded */
    return;

  } else {
    /* Spin waiting for the lock_word to become free */
    os_rmb;
    while (i < srv_n_spin_wait_rounds && lock->lock_word <= X_LOCK_HALF_DECR) {
      if (srv_spin_wait_delay) {
        ut_delay(ut::random_from_interval(0, srv_spin_wait_delay));
      }

      i++;
    }

    if (i >= srv_n_spin_wait_rounds) {
      std::this_thread::yield();

    } else {
      goto lock_loop;
    }
  }

  sync_cell_t *cell;

  sync_arr = sync_array_get_and_reserve_cell(lock, RW_LOCK_SX, location, &cell);

  /* Waiters must be set before checking lock_word, to ensure signal
  is sent. This could lead to a few unnecessary wake-up signals. */
  rw_lock_set_waiter_flag(lock);

  if (rw_lock_sx_lock_low(lock, pass, location)) {
    sync_array_free_cell(sync_arr, cell);

    if (count_os_wait > 0) {
      lock->count_os_wait += static_cast<uint32_t>(count_os_wait);
    }

    /* Locking succeeded */
    return;
  }

  ++count_os_wait;

  sync_array_wait_event(sync_arr, cell);

  i = 0;

  goto lock_loop;
}

#ifdef UNIV_DEBUG

/** Checks that the rw-lock has been initialized and that there are no
 simultaneous shared and exclusive locks.
 @return true */
bool rw_lock_validate(const rw_lock_t *lock) /*!< in: rw-lock */
{
  int32_t lock_word;

  ut_ad(lock);
  lock_word = lock->lock_word;

  ut_ad(lock->magic_n == rw_lock_t::MAGIC_N);
  ut_ad(lock_word > -(2 * X_LOCK_DECR));
  ut_ad(lock_word <= X_LOCK_DECR);

  return (true);
}

/** Checks if somebody has locked the rw-lock in the specified mode.
 @return true if locked */
bool rw_lock_is_locked(rw_lock_t *lock, /*!< in: rw-lock */
                       ulint lock_type) /*!< in: lock type: RW_LOCK_S,
                                        RW_LOCK_X or RW_LOCK_SX */
{
  ut_ad(rw_lock_validate(lock));

  switch (lock_type) {
    case RW_LOCK_S:
      return (rw_lock_get_reader_count(lock) > 0);

    case RW_LOCK_X:
      return (rw_lock_get_writer(lock) == RW_LOCK_X);

    case RW_LOCK_SX:
      return (rw_lock_get_sx_lock_count(lock) > 0);

    default:
      ut_error;
  }
}

void rw_lock_add_debug_info(rw_lock_t *lock, ulint pass, ulint lock_type,
                            ut::Location location) {
  rw_lock_debug_t *info = rw_lock_debug_create();

  rw_lock_debug_mutex_enter();

  info->pass = pass;
  info->lock_type = lock_type;
  info->location = location;
  info->thread_id = std::this_thread::get_id();

  UT_LIST_ADD_FIRST(lock->debug_list, info);

  rw_lock_debug_mutex_exit();

  const auto lock_word = lock->lock_word.load();

  /*
  The lock->lock_word and lock->sx_recursive already have been updated to
  reflect current acquisition before calling this function. We'd like to call
  sync_check_relock() for recursive acquisition of X or SX, which doesn't really
  have to wait, because the thread already has relevant access right.

  There are 4 cases to consider:

  1. We already held at least one X, and requested X again.
  This means lock_type == RW_LOCK_X and lock_word < -X_LOCK_HALF_DECR.
  Actually, we can even say that lock_word <= -X_LOCK_DECR, as values between
  -X_LOCK_DECR and -X_LOCK_HALF_DECR only happen while waiting for X and there
  are still some S granted, which we know can't be true as we hold at least one
  X.
  */
  if (lock_type == RW_LOCK_X && lock_word < -X_LOCK_HALF_DECR) {
    ut_a(lock_word <= -X_LOCK_DECR);
  }
  const bool case1 = lock_type == RW_LOCK_X && lock_word <= -X_LOCK_DECR;

  /*
  2. We already held at least one X, and requested SX.
  This means lock_type == RW_LOCK_SX and lock_word < 0.
  Actually, we can even say that lock_word <= -X_LOCK_HALF_DECR, because values
  between -X_LOCK_HALF_DECR and 0 only happen while waiting for X and there are
  still some S granted, which we know can't be true as we hold at least one X.
  */
  if (lock_type == RW_LOCK_SX && lock_word < 0) {
    ut_a(lock_word <= -X_LOCK_HALF_DECR);
  }
  const bool case2 = lock_type == RW_LOCK_SX && lock_word <= -X_LOCK_HALF_DECR;

  /*
  3. We already held at least one SX, and requested SX again.
  This means lock_type == RW_LOCK_SX and 2 <= lock->sx_recursive.
  Note that first SX has set lock->sx_recursive to 1, and second incremented
  it. Also note, that cases 2 and 3 are not mutually exclusive.
  */
  const bool case3 = lock_type == RW_LOCK_SX && 2 <= lock->sx_recursive;

  /*
  4. We already held at least one SX (but no X), and requested X now.
  This would manifest as
  lock_type == RW_LOCK_X and lock_word == -X_LOCK_HALF_DECR.
  This might have to wait for other threads to release their S, and thus is
  blocking and constitutes an escalation, and thus ...
  THIS IS NOT CONSIDERED TO BE A RELOCK!
  */

  if (pass == 0 && lock_type != RW_LOCK_X_WAIT) {
    if (case1 || case2 || case3) {
      sync_check_relock(lock);
    } else {
      sync_check_lock_validate(lock);
      sync_check_lock_granted(lock);
    }
  }
}

/** Removes a debug information struct for an rw-lock. */
void rw_lock_remove_debug_info(rw_lock_t *lock, /*!< in: rw-lock */
                               ulint pass,      /*!< in: pass value */
                               ulint lock_type) /*!< in: lock type */
{
  ut_ad(lock);

  if (pass == 0 && lock_type != RW_LOCK_X_WAIT) {
    sync_check_unlock(lock);
  }

  rw_lock_debug_mutex_enter();

  for (auto info : lock->debug_list) {
    if (pass == info->pass &&
        (pass != 0 || info->thread_id == std::this_thread::get_id()) &&
        info->lock_type == lock_type) {
      /* Found! */
      UT_LIST_REMOVE(lock->debug_list, info);

      rw_lock_debug_mutex_exit();

      rw_lock_debug_free(info);

      return;
    }
  }

  rw_lock_debug_mutex_exit();
  ut_error;
}

bool rw_lock_own(const rw_lock_t *lock, ulint lock_type) {
  ut_ad(lock);
  ut_ad(rw_lock_validate(lock));

  rw_lock_debug_mutex_enter();

  for (const rw_lock_debug_t *info : lock->debug_list) {
    if (info->thread_id == std::this_thread::get_id() && info->pass == 0 &&
        info->lock_type == lock_type) {
      rw_lock_debug_mutex_exit();
      /* Found! */

      return true;
    }
  }
  rw_lock_debug_mutex_exit();

  return false;
}

/** For collecting the debug information for a thread's rw-lock */
typedef std::vector<const rw_lock_debug_t *> Infos;

/** Get the thread debug info
@param[in]      infos           The rw-lock mode owned by the threads
@param[in]      lock            rw-lock to check */
static void rw_lock_get_debug_info(const rw_lock_t *lock, Infos *infos) {
  ut_ad(rw_lock_validate(lock));

  rw_lock_debug_mutex_enter();

  for (auto info : lock->debug_list) {
    if (info->thread_id == std::this_thread::get_id()) {
      infos->push_back(info);
    }
  }

  rw_lock_debug_mutex_exit();
}

bool rw_lock_own_flagged(const rw_lock_t *lock, rw_lock_flags_t flags) {
  Infos infos;

  rw_lock_get_debug_info(lock, &infos);

  for (const rw_lock_debug_t *info : infos) {
    ut_ad(info->thread_id == std::this_thread::get_id());

    if (info->pass != 0) {
      continue;
    }

    switch (info->lock_type) {
      case RW_LOCK_S:

        if (flags & RW_LOCK_FLAG_S) {
          return (true);
        }
        break;

      case RW_LOCK_X:

        if (flags & RW_LOCK_FLAG_X) {
          return (true);
        }
        break;

      case RW_LOCK_SX:

        if (flags & RW_LOCK_FLAG_SX) {
          return (true);
        }
    }
  }

  return (false);
}

/** Prints debug info of currently locked rw-locks. */
void rw_lock_list_print_info(FILE *file) /*!< in: file where to print */
{
  ulint count = 0;

  mutex_enter(&rw_lock_list_mutex);

  fputs(
      "-------------\n"
      "RW-LATCH INFO\n"
      "-------------\n",
      file);

  for (const rw_lock_t *lock : rw_lock_list) {
    count++;

    if (lock->lock_word != X_LOCK_DECR) {
      fprintf(file, "RW-LOCK: %p ", (void *)lock);

      if (rw_lock_get_waiters(lock)) {
        fputs(" Waiters for the lock exist\n", file);
      } else {
        putc('\n', file);
      }

      rw_lock_debug_mutex_enter();

      for (auto info : lock->debug_list) {
        rw_lock_debug_print(file, info);
      }

      rw_lock_debug_mutex_exit();
    }
  }

  fprintf(file, "Total number of rw-locks " ULINTPF "\n", count);
  mutex_exit(&rw_lock_list_mutex);
}

/** Prints info of a debug struct.
@param[in] f Output stream
@param[in] info Debug struct */
void rw_lock_debug_print(FILE *f, const rw_lock_debug_t *info) {
  ulint rwt = info->lock_type;

  fprintf(f, "Locked: thread %s file %s line %zu  ",
          to_string(info->thread_id).c_str(),
          sync_basename(info->location.filename), info->location.line);

  switch (rwt) {
    case RW_LOCK_S:
      fputs("S-LOCK", f);
      break;
    case RW_LOCK_X:
      fputs("X-LOCK", f);
      break;
    case RW_LOCK_SX:
      fputs("SX-LOCK", f);
      break;
    case RW_LOCK_X_WAIT:
      fputs("WAIT X-LOCK", f);
      break;
    default:
      ut_error;
  }

  if (info->pass != 0) {
    fprintf(f, " pass value %lu", (ulong)info->pass);
  }

  fprintf(f, "\n");
}

/** Print where it was locked from
@return the string representation */
std::string rw_lock_t::locked_from() const {
  /* Note: For X locks it can be locked form multiple places because
  the same thread can call X lock recursively. */

  std::ostringstream msg;
  Infos infos;

  rw_lock_get_debug_info(this, &infos);

  ulint i = 0;

  for (const rw_lock_debug_t *info : infos) {
    ut_ad(info->thread_id == std::this_thread::get_id());

    if (i++ > 0) {
      msg << ", ";
    }

    msg << info->location.filename << ":" << info->location.line;
  }

  return (msg.str());
}

/** Print the rw-lock information.
@return the string representation */
std::string rw_lock_t::to_string() const {
  std::ostringstream msg;

  msg << "RW-LATCH: "
      << "thread id " << ::to_string(std::this_thread::get_id())
      << " addr: " << this << " Locked from: " << locked_from().c_str();

  return (msg.str());
}
#endif /* UNIV_DEBUG */
