/*****************************************************************************

Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

/** @file include/sync0arr_impl.h
 The wait array used in synchronization primitives, implementation details.

 *******************************************************/

#include "sync0arr.h"
#include "sync0rw.h"

/*
                        WAIT ARRAY
                        ==========

The wait array consists of cells each of which has an an event object created
for it. The threads waiting for a mutex, for example, can reserve a cell
in the array and suspend themselves to wait for the event to become signaled.
When using the wait array, remember to make sure that some thread holding
the synchronization object will eventually know that there is a waiter in
the array and signal the object, to prevent infinite wait.  Why we chose
to implement a wait array? First, to make mutexes fast, we had to code
our own implementation of them, which only in usually uncommon cases
resorts to using slow operating system primitives. Then we had the choice of
assigning a unique OS event for each mutex, which would be simpler, or
using a global wait array. In some operating systems, the global wait
array solution is more efficient and flexible, because we can do with
a very small number of OS events, say 200. In NT 3.51, allocating events
seems to be a quadratic algorithm, because 10 000 events are created fast,
but 100 000 events takes a couple of minutes to create.

As of 5.0.30 the above mentioned design is changed. Since now OS can handle
millions of wait events efficiently, we no longer have this concept of each
cell of wait array having one event.  Instead, now the event that a thread
wants to wait on is embedded in the wait object (mutex or rw_lock). We still
keep the global wait array for the sake of diagnostics and also to avoid
infinite wait The error_monitor thread scans the global wait array to signal
any waiting threads who have missed the signal. */

typedef SyncArrayMutex::MutexType WaitMutex;
typedef BlockSyncArrayMutex::MutexType BlockWaitMutex;

/** The latch types that use the sync array. */
union sync_object_t {
  /** RW lock instance */
  rw_lock_t *lock = nullptr;

  /** Mutex instance */
  WaitMutex *mutex;

  /** Block mutex instance */
  BlockWaitMutex *bpmutex;
};

/** A cell where an individual thread may wait suspended until a resource
is released. The suspending is implemented using an operating system
event semaphore. */

struct sync_cell_t {
  sync_object_t latch;         /*!< pointer to the object the
                               thread is waiting for; if NULL
                               the cell is free for use */
  ulint request_type = 0;      /*!< lock type requested on the
                           object */
  const char *file = nullptr;  /*!< in debug version file where
                     requested */
  ulint line = 0;              /*!< in debug version line where
                           requested */
  std::thread::id thread_id{}; /*!< thread id of this waiting
                            thread */
  bool waiting = false;        /*!< true if the thread has already
                       called sync_array_event_wait
                       on this cell */
  int64_t signal_count = 0;    /*!< We capture the signal_count
                           of the latch when we
                           reset the event. This value is
                           then passed on to os_event_wait
                           and we wait only if the event
                           has not been signalled in the
                           period between the reset and
                           wait call. */

  /** Time when the thread reserved the wait cell. */
  std::chrono::steady_clock::time_point reservation_time{};
  /** Odd value means it is currently on-stack in a DFS search for cycles.
  Even value means it was completely processed.
  It is set to (odd) arr->last_scan when first visited, and then incremented
  again when all of its children are processed (and thus it is processed, too).
  @see arr->last_scan */
  uint64_t last_scan{0};
};

/* NOTE: It is allowed for a thread to wait for an event allocated for
the array without owning the protecting mutex (depending on the case:
OS or database mutex), but all changes (set or reset) to the state of
the event must be made while owning the mutex. */

/** Synchronization array */
struct sync_array_t {
  /** Constructor
  Creates a synchronization wait array. It is protected by a mutex
  which is automatically reserved when the functions operating on it
  are called.
  @param[in]    num_cells       Number of cells to create */
  sync_array_t(ulint num_cells) UNIV_NOTHROW;

  /** Destructor */
  ~sync_array_t() UNIV_NOTHROW;

  ulint n_reserved;      /*!< number of currently reserved
                         cells in the wait array */
  ulint n_cells;         /*!< number of cells in the
                         wait array */
  sync_cell_t *cells;    /*!< pointer to wait array */
  SysMutex mutex;        /*!< System mutex protecting the
                         data structure.  As this data
                         structure is used in constructing
                         the database mutex, to prevent
                         infinite recursion in implementation,
                         we fall back to an OS mutex. */
  ulint res_count;       /*!< count of cell reservations
                         since creation of the array */
  ulint next_free_slot;  /*!< the next free cell in the array */
  ulint first_free_slot; /*!< the last slot that was freed */
  /** It is incremented by one at the beginning of search for deadlock cycles,
  and then again after the scan has finished.
  If during a scan we visit a cell with cell->last_scan == arr->last_scan it
  means it is already on the stack, and thus a cycle was found.
  If we visit a cell with cell->last_scan == arr->last_scan+1 it means it was
  already fully processed and no deadlock was found "below" it.
  If it has some other value, the cell wasn't visited by this scan before.*/
  uint64_t last_scan{0};
};

/** Locally stored copy of srv_sync_array_size */
extern ulint sync_array_size;

/** The global array of wait cells for implementation of the database's own
mutexes and read-write locks */
extern sync_array_t **sync_wait_array;

static inline void sync_array_exit(sync_array_t *a) { mutex_exit(&a->mutex); }
static inline void sync_array_enter(sync_array_t *a) { mutex_enter(&a->mutex); }

/** Gets the nth cell in array.
 @param[in] arr Sync array to get cell from.
 @param[in] n Index of cell to retrieve.
 @return cell */
sync_cell_t *sync_array_get_nth_cell(sync_array_t *arr, ulint n);

/** Reports info of a wait array cell into a file.
 @param[in] file File where to print.
 @param[in] cell Sync array cell to report.
 */
void sync_array_cell_print(FILE *file, const sync_cell_t *cell);
