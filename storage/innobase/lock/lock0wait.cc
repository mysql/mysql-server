/*****************************************************************************

Copyright (c) 1996, 2019, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file lock/lock0wait.cc
 The transaction lock system

 Created 25/5/2010 Sunny Bains
 *******************************************************/

#define LOCK_MODULE_IMPLEMENTATION

#include <mysql/service_thd_wait.h>
#include <sys/types.h>

#include "ha_prototypes.h"
#include "lock0lock.h"
#include "lock0priv.h"
#include "os0thread-create.h"
#include "que0que.h"
#include "row0mysql.h"
#include "srv0mon.h"
#include "srv0start.h"

#include "my_dbug.h"

/** Print the contents of the lock_sys_t::waiting_threads array. */
static void lock_wait_table_print(void) {
  ut_ad(lock_wait_mutex_own());

  const srv_slot_t *slot = lock_sys->waiting_threads;

  for (uint32_t i = 0; i < srv_max_n_threads; i++, ++slot) {
    fprintf(stderr,
            "Slot %lu: thread type %lu,"
            " in use %lu, susp %lu, timeout %lu, time %lu\n",
            (ulong)i, (ulong)slot->type, (ulong)slot->in_use,
            (ulong)slot->suspended, slot->wait_timeout,
            (ulong)(ut_time_monotonic() - slot->suspend_time));
  }
}

/** Release a slot in the lock_sys_t::waiting_threads. Adjust the array last
 pointer if there are empty slots towards the end of the table. */
static void lock_wait_table_release_slot(
    srv_slot_t *slot) /*!< in: slot to release */
{
#ifdef UNIV_DEBUG
  srv_slot_t *upper = lock_sys->waiting_threads + srv_max_n_threads;
#endif /* UNIV_DEBUG */

  lock_wait_mutex_enter();
  /* We omit trx_mutex_enter and lock_mutex_enter here, because we are only
  going to touch thr->slot, which is a member used only by lock0wait.cc and is
  sufficiently protected by lock_wait_mutex. Yes, there are readers who read
  the thr->slot holding only trx->mutex and lock_sys->mutex, but they do so,
  when they are sure that we were not woken up yet, so our thread can't be here.
  See comments in lock_wait_release_thread_if_suspended() for more details. */

  ut_ad(slot->in_use);
  ut_ad(slot->thr != nullptr);
  ut_ad(slot->thr->slot != nullptr);
  ut_ad(slot->thr->slot == slot);

  /* Must be within the array boundaries. */
  ut_ad(slot >= lock_sys->waiting_threads);
  ut_ad(slot < upper);

  slot->thr->slot = nullptr;
  slot->thr = nullptr;
  slot->in_use = FALSE;

  /* This operation is guarded by lock_wait_mutex_enter/exit and we don't care
  about its relative ordering with other operations in this critical section. */
  lock_sys->n_waiting.fetch_sub(1, std::memory_order_relaxed);
  /* Scan backwards and adjust the last free slot pointer. */
  for (slot = lock_sys->last_slot;
       slot > lock_sys->waiting_threads && !slot->in_use; --slot) {
    /* No op */
  }

  /* Either the array is empty or the last scanned slot is in use. */
  ut_ad(slot->in_use || slot == lock_sys->waiting_threads);

  lock_sys->last_slot = slot + 1;

  /* The last slot is either outside of the array boundary or it's
  on an empty slot. */
  ut_ad(lock_sys->last_slot == upper || !lock_sys->last_slot->in_use);

  ut_ad(lock_sys->last_slot >= lock_sys->waiting_threads);
  ut_ad(lock_sys->last_slot <= upper);

  lock_wait_mutex_exit();
}

/** Counts number of calls to lock_wait_table_reserve_slot.
It is protected by lock_wait_mutex.
Current value of this counter is stored in the slot a transaction has chosen
for sleeping during suspension, and thus serves as "reservation number" which
can be used to check if the owner of the slot has changed (perhaps multiple
times, in "ABA" manner). */
static uint64_t lock_wait_table_reservations = 0;

/** Reserves a slot in the thread table for the current user OS thread.
 @return reserved slot */
static srv_slot_t *lock_wait_table_reserve_slot(
    que_thr_t *thr,     /*!< in: query thread associated
                        with the user OS thread */
    ulong wait_timeout) /*!< in: lock wait timeout value */
{
  srv_slot_t *slot;

  ut_ad(lock_wait_mutex_own());
  ut_ad(trx_mutex_own(thr_get_trx(thr)));

  slot = lock_sys->waiting_threads;

  for (uint32_t i = srv_max_n_threads; i--; ++slot) {
    if (!slot->in_use) {
      slot->reservation_no = lock_wait_table_reservations++;
      slot->in_use = TRUE;
      slot->thr = thr;
      slot->thr->slot = slot;

      if (slot->event == nullptr) {
        slot->event = os_event_create(nullptr);
        ut_a(slot->event);
      }

      os_event_reset(slot->event);
      slot->suspended = TRUE;
      slot->suspend_time = ut_time_monotonic();
      slot->wait_timeout = wait_timeout;

      if (slot == lock_sys->last_slot) {
        ++lock_sys->last_slot;
      }

      ut_ad(lock_sys->last_slot <=
            lock_sys->waiting_threads + srv_max_n_threads);

      /* We call lock_wait_request_check_for_cycles() because the
      node representing the `thr` only now becomes visible to the thread which
      analyzes contents of lock_sys->waiting_threads. The edge itself was
      created by lock_create_wait_for_edge() during RecLock::add_to_waitq() or
      lock_table(), but at that moment the source of the edge was not yet in the
      lock_sys->waiting_threads, so the node and the outgoing edge were not yet
      visible.
      I hope this explains why we do waste time on calling
      lock_wait_request_check_for_cycles() from lock_create_wait_for_edge().*/
      lock_wait_request_check_for_cycles();
      return (slot);
    }
  }

  ib::error(ER_IB_MSG_646)
      << "There appear to be " << srv_max_n_threads
      << " user"
         " threads currently waiting inside InnoDB, which is the upper"
         " limit. Cannot continue operation. Before aborting, we print"
         " a list of waiting threads.";
  lock_wait_table_print();

  ut_error;
}

void lock_wait_request_check_for_cycles() { lock_set_timeout_event(); }

/** Puts a user OS thread to wait for a lock to be released. If an error
 occurs during the wait trx->error_state associated with thr is
 != DB_SUCCESS when we return. DB_LOCK_WAIT_TIMEOUT and DB_DEADLOCK
 are possible errors. DB_DEADLOCK is returned if selective deadlock
 resolution chose this transaction as a victim. */
void lock_wait_suspend_thread(que_thr_t *thr) /*!< in: query thread associated
                                              with the user OS thread */
{
  srv_slot_t *slot;
  trx_t *trx;
  ibool was_declared_inside_innodb;
  ib_time_monotonic_ms_t start_time = 0;
  ulong lock_wait_timeout;

  trx = thr_get_trx(thr);

  if (trx->mysql_thd != nullptr) {
    DEBUG_SYNC_C("lock_wait_suspend_thread_enter");
  }

  /* InnoDB system transactions (such as the purge, and
  incomplete transactions that are being rolled back after crash
  recovery) will use the global value of
  innodb_lock_wait_timeout, because trx->mysql_thd == NULL. */
  lock_wait_timeout = trx_lock_wait_timeout_get(trx);

  lock_wait_mutex_enter();

  trx_mutex_enter(trx);

  trx->error_state = DB_SUCCESS;

  if (thr->state == QUE_THR_RUNNING) {
    ut_ad(thr->is_active);

    /* The lock has already been released or this transaction
    was chosen as a deadlock victim: no need to suspend */

    if (trx->lock.was_chosen_as_deadlock_victim) {
      trx->error_state = DB_DEADLOCK;
      trx->lock.was_chosen_as_deadlock_victim = false;

      ut_d(trx->lock.in_rollback = true);
    }

    lock_wait_mutex_exit();
    trx_mutex_exit(trx);
    return;
  }

  ut_ad(!thr->is_active);

  slot = lock_wait_table_reserve_slot(thr, lock_wait_timeout);

  if (thr->lock_state == QUE_THR_LOCK_ROW) {
    srv_stats.n_lock_wait_count.inc();
    srv_stats.n_lock_wait_current_count.inc();

    start_time = ut_time_monotonic_us();
  }
  /* This operation is guarded by lock_wait_mutex_enter/exit and we don't care
  about its relative ordering with other operations in this critical section. */
  lock_sys->n_waiting.fetch_add(1, std::memory_order_relaxed);
  lock_wait_mutex_exit();

  /* We hold trx->mutex here, which is required to call
  lock_set_lock_and_trx_wait. This means that the value in
  trx->lock.wait_lock_type which we are about to read comes from the latest
  call to lock_set_lock_and_trx_wait before we obtained the trx->mutex, which is
  precisely what we want for our stats */
  auto lock_type = trx->lock.wait_lock_type;
  trx_mutex_exit(trx);

  ulint had_dict_lock = trx->dict_operation_lock_mode;

  switch (had_dict_lock) {
    case 0:
      break;
    case RW_S_LATCH:
      /* Release foreign key check latch */
      row_mysql_unfreeze_data_dictionary(trx);

      DEBUG_SYNC_C("lock_wait_release_s_latch_before_sleep");
      break;
    case RW_X_LATCH:
      /* We may wait for rec lock in dd holding
      dict_operation_lock for creating FTS AUX table */
      ut_ad(!mutex_own(&dict_sys->mutex));
      rw_lock_x_unlock(dict_operation_lock);
      break;
  }

  /* Suspend this thread and wait for the event. */

  was_declared_inside_innodb = trx->declared_to_be_inside_innodb;

  if (was_declared_inside_innodb) {
    /* We must declare this OS thread to exit InnoDB, since a
    possible other thread holding a lock which this thread waits
    for must be allowed to enter, sooner or later */

    srv_conc_force_exit_innodb(trx);
  }

  ut_a(lock_type == LOCK_REC || lock_type == LOCK_TABLE);
  thd_wait_begin(trx->mysql_thd, lock_type == LOCK_REC ? THD_WAIT_ROW_LOCK
                                                       : THD_WAIT_TABLE_LOCK);

  DEBUG_SYNC_C("lock_wait_will_wait");

  os_event_wait(slot->event);

  DEBUG_SYNC_C("lock_wait_has_finished_waiting");

  thd_wait_end(trx->mysql_thd);

  /* After resuming, reacquire the data dictionary latch if
  necessary. */

  if (was_declared_inside_innodb) {
    /* Return back inside InnoDB */

    srv_conc_force_enter_innodb(trx);
  }

  if (had_dict_lock == RW_S_LATCH) {
    row_mysql_freeze_data_dictionary(trx);
  } else if (had_dict_lock == RW_X_LATCH) {
    rw_lock_x_lock(dict_operation_lock);
  }

  const auto wait_time = ut_time_monotonic() - slot->suspend_time;

  /* Release the slot for others to use */

  lock_wait_table_release_slot(slot);

  if (thr->lock_state == QUE_THR_LOCK_ROW) {
    const auto finish_time = ut_time_monotonic_us();

    const uint64_t diff_time =
        (finish_time > start_time) ? (uint64_t)(finish_time - start_time) : 0;

    srv_stats.n_lock_wait_current_count.dec();
    srv_stats.n_lock_wait_time.add(diff_time);

    /* Only update the variable if we successfully
    retrieved the start and finish times. See Bug#36819. */
    if (diff_time > lock_sys->n_lock_max_wait_time && start_time != 0) {
      lock_sys->n_lock_max_wait_time = diff_time;
    }

    /* Record the lock wait time for this thread */
    thd_set_lock_wait_time(trx->mysql_thd, diff_time);

    DBUG_EXECUTE_IF("lock_instrument_slow_query_log", os_thread_sleep(1000););
  }

  /* The transaction is chosen as deadlock victim during sleep. */
  if (trx->error_state == DB_DEADLOCK) {
    ut_d(trx->lock.in_rollback = true);
    return;
  }

  if (lock_wait_timeout < 100000000 && wait_time > (double)lock_wait_timeout &&
      !trx_is_high_priority(trx)) {
    trx->error_state = DB_LOCK_WAIT_TIMEOUT;

    MONITOR_INC(MONITOR_TIMEOUT);
  }

  if (trx_is_interrupted(trx)) {
    trx->error_state = DB_INTERRUPTED;
  }
}

/** Releases a user OS thread waiting for a lock to be released, if the
 thread is already suspended. Please do not call it directly, but rather use the
 lock_reset_wait_and_release_thread_if_suspended() wrapper.
 @param[in]   thr   query thread associated with the user OS thread */
static void lock_wait_release_thread_if_suspended(que_thr_t *thr) {
  auto trx = thr_get_trx(thr);
  /* We need a guarantee that for each time a thread is suspended there is at
  most one time it gets released - or more precisely: that there is at most
  one reason for it to be woken up. Otherwise it could happen that two
  different threads will think that they successfully woken up the transaction
  and that the transaction understands the reason it was woken up is the one
  they had in mind, say: one thread woken it up because of deadlock and another
  because of timeout. If the two reasons require different behavior after
  waking up, then we will be in trouble. Current implementation makes sure
  that we wake up a thread only once by observing several rules:
    1. the only way to wake up a trx is to call os_event_set
    2. the only call to os_event_set is in lock_wait_release_thread_if_suspended
    3. calls to lock_wait_release_thread_if_suspended are always performed after
    a call to lock_reset_lock_and_trx_wait(lock), and the sequence of the two is
    in a critical section guarded by lock_mutex_enter
    4. the lock_reset_lock_and_trx_wait(lock) asserts that
    lock->trx->lock.wait_lock == lock and sets lock->trx->lock.wait_lock = NULL
  Together all this facts imply, that it is impossible for a single trx to be
  woken up twice (unless it got to sleep again) because doing so requires
  reseting wait_lock to NULL.

  We now hold exclusive lock_sys latch. */
  ut_ad(lock_mutex_own());
  ut_ad(trx_mutex_own(trx));

  /* We don't need the lock_wait_mutex here, because we know that the thread
  had a reason to go to sleep (we have seen trx->lock.wait_lock !=NULL), and we
  know that we are the first ones to wake it up (we are the thread which has
  changed the trx->lock.wait_lock to NULL), so it either sleeps, or did not
  yet started the sleep. We hold the trx->mutex which is required to go to
  sleep. So, while holding the trx->mutex we can check if thr->slot is already
  assigned and if so, then we need to wake up the thread. If the thr->slot is
  not yet assigned, then we know that the thread had not yet gone to sleep, and
  we know that before doing so, it will need to acquire trx->mutex and will
  verify once more if it has to go to sleep by checking if thr->state is
  QUE_THR_RUNNING which we indeed have already set before calling
  lock_wait_release_thread_if_suspended, so we don't need to do anything in this
  case - trx will simply not go to sleep. */
  ut_ad(thr->state == QUE_THR_RUNNING);
  ut_ad(trx->lock.wait_lock == nullptr);

  if (thr->slot != nullptr && thr->slot->in_use && thr->slot->thr == thr) {
    if (trx->lock.was_chosen_as_deadlock_victim) {
      trx->error_state = DB_DEADLOCK;
      trx->lock.was_chosen_as_deadlock_victim = false;

      ut_d(trx->lock.in_rollback = true);
    }

    os_event_set(thr->slot->event);
  }
}

void lock_reset_wait_and_release_thread_if_suspended(lock_t *lock) {
  ut_ad(lock_mutex_own());
  ut_ad(trx_mutex_own(lock->trx));
  ut_ad(lock->trx->lock.wait_lock == lock);

  /* Reset the wait flag and the back pointer to lock in trx */

  lock_reset_lock_and_trx_wait(lock);

  /* We clear blocking_trx here and not in lock_reset_lock_and_trx_wait(), as
  lock_reset_lock_and_trx_wait() is called also when the wait_lock is being
  moved from one page to another during B-tree reorganization, in which case
  blocking_trx should not change - in such cases a new wait lock is created
  and assigned to trx->lock.wait_lock, but the information about blocking trx
  is not so easy to restore, so it is easier to simply not clear blocking_trx
  until we are 100% sure that we want to wake up the trx, which is now.
  Actually, clearing blocking_trx is not strictly required from correctness
  perspective, it rather serves for:
  1. performance optimization, as lock_wait_snapshot_waiting_threads() can omit
     this trx when building wait-for-graph
  2. debugging, as reseting blocking_trx makes it easier to spot it was not
     properly set on subsequent waits. */
  lock->trx->lock.blocking_trx.store(nullptr);

  /* We only release locks for which someone is waiting, and the trx which
  decided to wait for the lock should have already set trx->lock.que_state to
  TRX_QUE_LOCK_WAIT and called que_thr_stop() before releasing the lock-sys
  latch. */
  ut_ad(lock->trx_que_state() == TRX_QUE_LOCK_WAIT);

  /* The following function releases the trx from lock wait */

  que_thr_t *thr = que_thr_end_lock_wait(lock->trx);

  if (thr != nullptr) {
    lock_wait_release_thread_if_suspended(thr);
  }
}

/** Check if the thread lock wait has timed out. Release its locks if the
 wait has actually timed out. */
static void lock_wait_check_and_cancel(
    const srv_slot_t *slot) /*!< in: slot reserved by a user
                            thread when the wait started */
{
  trx_t *trx;

  const auto suspend_time = slot->suspend_time;

  ut_ad(lock_wait_mutex_own());

  ut_ad(slot->in_use);

  ut_ad(slot->suspended);

  const auto wait_time = ut_time_monotonic() - suspend_time;

  trx = thr_get_trx(slot->thr);

  if (trx_is_interrupted(trx) ||
      (slot->wait_timeout < 100000000 &&
       (wait_time > (int64_t)slot->wait_timeout || wait_time < 0))) {
    /* Timeout exceeded or a wrap-around in system
    time counter: cancel the lock request queued
    by the transaction and release possible
    other transactions waiting behind; it is
    possible that the lock has already been
    granted: in that case do nothing */

    lock_mutex_enter();

    trx_mutex_enter(trx);

    if (trx->lock.wait_lock != nullptr && !trx_is_high_priority(trx)) {
      ut_a(trx->lock.que_state == TRX_QUE_LOCK_WAIT);

      lock_cancel_waiting_and_release(trx->lock.wait_lock, false);
    }

    lock_mutex_exit();

    trx_mutex_exit(trx);
  }
}

/** A snapshot of information about a single slot which was in use at the moment
of taking the snapshot */
struct waiting_trx_info_t {
  /** The transaction which was using this slot. */
  trx_t *trx;
  /** The transaction for which the owner of the slot is waiting for. */
  trx_t *waits_for;
  /** The slot this info is about */
  srv_slot_t *slot;
  /** The slot->reservation_no at the moment of taking the snapshot */
  uint64_t reservation_no;
};

/** As we want to quickly find a given trx_t within the snapshot, we use a
sorting criterion which is based on trx only. We use the pointer address, as
any deterministic rule without ties will do. */
bool operator<(const waiting_trx_info_t &a, const waiting_trx_info_t &b) {
  return a.trx < b.trx;
}

/** Check all slots for user threads that are waiting on locks, and if they have
exceeded the time limit. */
static void lock_wait_check_slots_for_timeouts() {
  ut_ad(!lock_wait_mutex_own());
  lock_wait_mutex_enter();

  for (auto slot = lock_sys->waiting_threads; slot < lock_sys->last_slot;
       ++slot) {
    /* We are doing a read without the lock mutex and/or the trx mutex. This is
    OK because a slot can't be freed or reserved without the lock wait mutex. */
    if (slot->in_use) {
      lock_wait_check_and_cancel(slot);
    }
  }

  lock_wait_mutex_exit();
}

/** Takes a snapshot of the content of slots which are in use
@param[out]   infos   Will contain the information about slots which are in use
*/
static void lock_wait_snapshot_waiting_threads(
    ut::vector<waiting_trx_info_t> &infos) {
  ut_ad(!lock_wait_mutex_own());
  infos.clear();
  lock_wait_mutex_enter();
  /*
  We own lock_wait_mutex, which protects lock_wait_table_reservations and
  reservation_no.
  We want to make a snapshot of the wait-for graph as quick as possible to not
  keep the lock_wait_mutex too long.
  Anything more fancy than push_back seems to impact performance.

  Note: one should be able to prove that we don't really need a "consistent"
  snapshot - the algorithm should still work if we split the loop into several
  smaller "chunks" snapshotted independently and stitch them together. Care must
  be taken to "merge" duplicates keeping the freshest version (reservation_no)
  of slot for each trx.
  So, if (in future) this loop turns out to be a bottleneck (say, by increasing
  congestion on lock_wait_mutex), one can try to release and require the lock
  every X iterations and modify the lock_wait_build_wait_for_graph() to handle
  duplicates in a smart way.
  */
  for (auto slot = lock_sys->waiting_threads; slot < lock_sys->last_slot;
       ++slot) {
    if (slot->in_use) {
      auto from = thr_get_trx(slot->thr);
      auto to = from->lock.blocking_trx.load();
      if (to != nullptr) {
        infos.push_back({from, to, slot, slot->reservation_no});
      }
    }
  }
  lock_wait_mutex_exit();
}

/** Analyzes content of the snapshot with information about slots in use, and
builds a (subset of) list of edges from waiting transactions to blocking
transactions, such that for each waiter we have one outgoing edge.
@param[in]    infos     information about all waiting transactions
@param[out]   outgoing  The outgoing[from] will contain either the index such
                        that infos[outgoing[from]].trx is the reason
                        infos[from].trx has to wait, or -1 if the reason for
                        waiting is not among transactions in infos[].trx. */
static void lock_wait_build_wait_for_graph(
    ut::vector<waiting_trx_info_t> &infos, ut::vector<int> &outgoing) {
  /** We are going to use int and uint to store positions within infos */
  ut_ad(infos.size() < (1 << 20));
  const auto n = static_cast<uint>(infos.size());
  outgoing.clear();
  outgoing.resize(n, -1);
  /* This particular implementation sorts infos by ::trx, and then uses
  lower_bound to find index in infos corresponding to ::wait_for, which has
  O(nlgn) complexity and modifies infos, but has a nice property of avoiding any
  allocations.
  An alternative O(n) approach would be to use a hash table to map infos[i].trx
  to i.
  Using unordered_map<trx_t*,int> however causes too much (de)allocations as
  its bucket chains are implemented as a linked lists - overall it works much
  slower than sort.
  The fastest implementation was to use custom implementation of a hash table
  with open addressing and double hashing with a statically allocated
  2 * srv_max_n_threads buckets. This however did not increase transactions per
  second, so introducing a custom implementation seems unjustified here. */
  sort(infos.begin(), infos.end());
  waiting_trx_info_t needle{};
  for (uint from = 0; from < n; ++from) {
    ut_ad(from == 0 || infos[from - 1].trx < infos[from].trx);
    needle.trx = infos[from].waits_for;
    auto it = std::lower_bound(infos.begin(), infos.end(), needle);

    if (it == infos.end() || it->trx != needle.trx) {
      continue;
    }
    auto to = it - infos.begin();
    ut_ad(from != static_cast<uint>(to));
    outgoing[from] = static_cast<int>(to);
  }
}

/** Notifies the chosen_victim that it should roll back
@param[in,out]    chosen_victim   the transaction that should be rolled back */
static void lock_wait_rollback_deadlock_victim(trx_t *chosen_victim) {
  ut_ad(!trx_mutex_own(chosen_victim));
  /* The call to lock_cancel_waiting_and_release requires exclusive latch on
  whole lock_sys in case of table locks.*/
  ut_ad(lock_mutex_own());
  trx_mutex_enter(chosen_victim);
  chosen_victim->lock.was_chosen_as_deadlock_victim = true;
  ut_a(chosen_victim->lock.wait_lock);
  ut_a(chosen_victim->lock.que_state == TRX_QUE_LOCK_WAIT);
  lock_cancel_waiting_and_release(chosen_victim->lock.wait_lock, true);
  trx_mutex_exit(chosen_victim);
}

/** Given the `infos` about transactions and indexes in `infos` which form a
deadlock cycle, identifies the transaction with the largest `reservation_no`,
that is the one which was the latest to join the cycle.
@param[in]    cycle_ids   indexes in `infos` array, of transactions forming the
                          deadlock cycle
@param[in]    infos       information about all waiting transactions
@return position in cycle_ids, such that infos[cycle_ids[pos]].reservation_no is
        the largest */
static size_t lock_wait_find_latest_pos_on_cycle(
    const ut::vector<uint> &cycle_ids,
    const ut::vector<waiting_trx_info_t> &infos) {
  size_t latest_pos = 0;
  for (size_t pos = 1; pos < cycle_ids.size(); ++pos) {
    if (infos[cycle_ids[latest_pos]].reservation_no <
        infos[cycle_ids[pos]].reservation_no) {
      latest_pos = pos;
    }
  }
  return latest_pos;
}

/** Rotates the deadlock cycle so that it starts from desired item.
@param[in]    first_pos   the position which should become first after rotation
@param[in]    cycle_ids   the array to rotate
@return a copy of cycle_ids rotated in such a way, that the element which
was at position `first_pos` is now the first */
static ut::vector<uint> lock_wait_rotate_so_pos_is_first(
    size_t first_pos, const ut::vector<uint> &cycle_ids) {
  auto rotated_ids = cycle_ids;
  std::rotate(rotated_ids.begin(), rotated_ids.begin() + first_pos,
              rotated_ids.end());
  return rotated_ids;
}

/** A helper, which extracts transactions with given indexes from the `infos`
array.
@param[in]    ids     indexes of transactions in the `infos` array to extract
@param[in]    infos   information about all waiting transactions
@return  An array formed by infos[ids[i]].trx */
template <typename T>
static ut::vector<T> lock_wait_map_ids_to_trxs(
    const ut::vector<uint> &ids, const ut::vector<waiting_trx_info_t> &infos) {
  ut::vector<T> trxs;
  trxs.reserve(ids.size());
  for (auto id : ids) {
    trxs.push_back(infos[id].trx);
  }
  return trxs;
}

/** Orders the transactions from deadlock cycle in such a backward-compatible
way, from the point of view of algorithm with picks the victim. From correctness
point of view this could be a no-op, but we have test cases which assume some
determinism in that which trx is selected as the victim. These tests usually
depend on the old behaviour in which the order in which trxs attempted to wait
was meaningful. In the past we have only considered two candidates for victim:
  (a) the transaction which closed the cycle by adding last wait-for edge
  (b) the transaction which is waiting for (a)
and it favored to pick (a) in case of ties.
To make these test pass we find the trx with most recent reservation_no (a),
and the one before it in the cycle (b), and move them to the end of collection
So that we consider them in order: ...,...,...,(b),(a), resolving ties by
picking the latest one as victim.
In other words we rotate the cycle so that the most recent waiter becomes the
last.
@param[in]    cycle_ids   indexes in `infos` array, of transactions forming the
                          deadlock cycle
@param[in]    infos       information about all waiting transactions
@return transactions from the deadlock cycle sorted in the optimal way for
        choosing a victim */
static ut::vector<trx_t *> lock_wait_order_for_choosing_victim(
    const ut::vector<uint> &cycle_ids,
    const ut::vector<waiting_trx_info_t> &infos) {
  size_t latest_pos = lock_wait_find_latest_pos_on_cycle(cycle_ids, infos);
  size_t first_pos = (latest_pos + 1) % cycle_ids.size();
  return lock_wait_map_ids_to_trxs<trx_t *>(
      lock_wait_rotate_so_pos_is_first(first_pos, cycle_ids), infos);
}

/** Given an array with information about all waiting transactions and indexes
in it which form a deadlock cycle, picks the transaction to rollback.
@param[in]    cycle_ids   indexes in `infos` array, of transactions forming the
                          deadlock cycle
@param[in]    infos       information about all waiting transactions
@return the transaction chosen as a victim */
static trx_t *lock_wait_choose_victim(
    const ut::vector<uint> &cycle_ids,
    const ut::vector<waiting_trx_info_t> &infos) {
  /* We are iterating over various transactions comparing their trx_weight_ge,
  which is computed based on number of locks held thus we need exclusive latch
  on the whole lock_sys. In theory number of locks should not change while the
  transaction is waiting, but instead of proving that they can not wake up, it
  is easier to assert that we hold the mutex */
  ut_ad(lock_mutex_own());
  ut_ad(!cycle_ids.empty());
  trx_t *chosen_victim = nullptr;
  auto sorted_trxs = lock_wait_order_for_choosing_victim(cycle_ids, infos);

  for (auto *trx : sorted_trxs) {
    if (chosen_victim == nullptr) {
      chosen_victim = trx;
      continue;
    }

    if (trx_is_high_priority(chosen_victim) || trx_is_high_priority(trx)) {
      auto victim = trx_arbitrate(trx, chosen_victim);

      if (victim != nullptr) {
        if (victim == trx) {
          chosen_victim = trx;
        } else {
          ut_a(victim == chosen_victim);
        }
        continue;
      }
    }

    if (trx_weight_ge(chosen_victim, trx)) {
      /* The joining transaction is 'smaller',
      choose it as the victim and roll it back. */
      chosen_victim = trx;
    }
  }

  ut_a(chosen_victim);
  return chosen_victim;
}

/** Given an array with information about all waiting transactions and indexes
in it which form a deadlock cycle, checks if the transactions allegedly forming
the deadlock have actually stayed in slots since we've last checked, as opposed
to say, not leaving a slot, and/or re-entering the slot ("ABA" situation).
This is done by comparing the current reservation_no for each slot, with the
reservation_no from the `info` snapshot.
@param[in]    cycle_ids   indexes in `infos` array, of transactions forming the
                          deadlock cycle
@param[in]    infos       information about all waiting transactions
@return true if all given transactions resided in their slots for the whole time
since the snapshot was taken, and in particular are still there right now */
static bool lock_wait_trxs_are_still_in_slots(
    const ut::vector<uint> &cycle_ids,
    const ut::vector<waiting_trx_info_t> &infos) {
  ut_ad(lock_wait_mutex_own());
  for (auto id : cycle_ids) {
    const auto slot = infos[id].slot;
    if (!slot->in_use || slot->reservation_no != infos[id].reservation_no) {
      return false;
    }
    ut_ad(thr_get_trx(slot->thr) == infos[id].trx);
  }
  return true;
}

/** Given an array with information about all waiting transactions and indexes
in it which form a deadlock cycle, checks if the transactions allegedly forming
the deadlock have actually still wait for a lock, as opposed to being already
notified about lock being granted or timeout, but still being present in the
slot. This is done by checking trx->lock.wait_lock under lock_sys mutex.
@param[in]    cycle_ids   indexes in `infos` array, of transactions forming the
                          deadlock cycle
@param[in]    infos       information about all waiting transactions
@return true if all given transactions are still waiting for locks*/
static bool lock_wait_trxs_are_still_waiting(
    const ut::vector<uint> &cycle_ids,
    const ut::vector<waiting_trx_info_t> &infos) {
  ut_ad(lock_wait_mutex_own());
  /* We are iterating over various transaction which may have locks in different
  tables/rows, thus we need exclusive latch on the whole lock_sys to make sure
  no one will wake them up (say, a high priority trx could abort them) */
  ut_ad(lock_mutex_own());

  for (auto id : cycle_ids) {
    const auto trx = infos[id].trx;
    if (trx->lock.wait_lock == nullptr) {
      /* trx is on its way to being woken up, so this cycle is a false positive.
      As this particular cycle will resolve itself, we ignore it. */
      return false;
    }
    ut_a(trx->lock.que_state == TRX_QUE_LOCK_WAIT);
  }
  return true;
}

/* A helper function which rotates the deadlock cycle, so that the order of
transactions in it is suitable for notification. From correctness perspective
this could be a no-op, but we have test cases which assume some determinism in
that in which order trx from cycle are reported. These tests usually depend on
the old behaviour in which the order in which transactions attempted to wait was
meaningful. In the past we reported:
- the transaction which closed the cycle by adding last wait-for edge as (2)
- the transaction which is waiting for (2) as (1)
To make these test pass we find the trx with most recent reservation_no (2),
and the one before it in the cycle (1), and move (1) to the beginning.
@param[in]    cycle_ids   indexes in `infos` array, of transactions forming the
                          deadlock cycle
@param[in]    infos       information about all waiting transactions
@return the indexes from cycle_ids sorted rotated in backward-compatible way
*/
static ut::vector<uint> lock_wait_rotate_cycle_ids_for_notification(
    const ut::vector<uint> &cycle_ids,
    const ut::vector<waiting_trx_info_t> &infos) {
  size_t latest_pos = lock_wait_find_latest_pos_on_cycle(cycle_ids, infos);
  size_t previous_pos = (latest_pos - 1 + cycle_ids.size()) % cycle_ids.size();
  return lock_wait_rotate_so_pos_is_first(previous_pos, cycle_ids);
}

/** A helper function which rotates the deadlock cycle, so that the order of
transactions in it is suitable for notification. From correctness perspective
this could be a no-op, but we have tests which depend on deterministic output
from such notifications, and we want to be backward compatible.
@param[in]    cycle_ids   indexes in `infos` array, of transactions forming the
                          deadlock cycle
@param[in]    infos       information about all waiting transactions
@return the transactions from cycle_ids rotated in backward-compatible way */
static ut::vector<const trx_t *> lock_wait_trxs_rotated_for_notification(
    const ut::vector<uint> &cycle_ids,
    const ut::vector<waiting_trx_info_t> &infos) {
  return lock_wait_map_ids_to_trxs<const trx_t *>(
      lock_wait_rotate_cycle_ids_for_notification(cycle_ids, infos), infos);
}

/** Handles a deadlock found, by notifying about it, and rolling back the
chosen victim.
@param[in,out]  chosen_victim the transaction to roll back
@param[in]      cycle_ids     indexes in `infos` array, of transactions forming
                              the deadlock cycle
@param[in]      infos         information about all waiting transactions */
static void lock_wait_handle_deadlock(
    trx_t *chosen_victim, const ut::vector<uint> &cycle_ids,
    const ut::vector<waiting_trx_info_t> &infos) {
  lock_notify_about_deadlock(
      lock_wait_trxs_rotated_for_notification(cycle_ids, infos), chosen_victim);

  lock_wait_rollback_deadlock_victim(chosen_victim);
}

/** Given an array with information about all waiting transactions and indexes
in it which form a deadlock cycle, checks if the transactions allegedly forming
the deadlock cycle, indeed are still waiting, and if so, chooses a victim and
handles the deadlock.
@param[in]    cycle_ids   indexes in `infos` array, of transactions forming the
                          deadlock cycle
@param[in]    infos       information about all waiting transactions
@return true if the cycle found was indeed a deadlock cycle, false if it was a
false positive */
static bool lock_wait_check_candidate_cycle(
    ut::vector<uint> &cycle_ids, const ut::vector<waiting_trx_info_t> &infos) {
  ut_ad(!lock_wait_mutex_own());
  ut_ad(!lock_mutex_own());
  lock_wait_mutex_enter();
  /*
  We have released all mutexes after we have built the `infos` snapshot and
  before we've got here. So, while it is true that the edges form a cycle, it
  may also be true that some of these transactions were already rolled back, and
  memory pointed by infos[i].trx or infos[i].waits_for is no longer the trx it
  used to be (as we reuse trx_t objects). It may even segfault if we try to
  access it (because trx_t object could be freed). So we need to somehow verify
  that the pointer is still valid without accessing it. We do that by checking
  if slot->reservation_no has changed since taking a snapshot.
  If it has not changed, then we know that the trx's pointer still points to the
  same trx as the trx is sleeping, and thus has not finished and wasn't freed.
  So, we start by first checking that the slots still contain the trxs we are
  interested in. This requires lock_wait_mutex, but not lock_mutex.
  */
  if (!lock_wait_trxs_are_still_in_slots(cycle_ids, infos)) {
    lock_wait_mutex_exit();
    return false;
  }
  /*
  At this point we are sure that we can access memory pointed by infos[i].trx
  and that transactions are still in their slots. (And, as `cycle_ids` is a
  cycle, we also know that infos[cycle_ids[i]].wait_for is equal to
  infos[cycle_ids[i+1]].trx, so infos[cycle_ids[i]].wait_for can also be safely
  accessed).
  This however does not mean necessarily that they are still waiting.
  They might have been already notified that they should wake up (by calling
  lock_wait_release_thread_if_suspended()), but they had not yet chance to act
  upon it (it is the trx being woken up who is responsible for cleaning up the
  `slot` it used).
  So, the slot can be still in use and contain a transaction, which was already
  decided to be rolledback for example. However, we can recognize this situation
  by looking at trx->lock.wait_lock, as each call to
  lock_wait_release_thread_if_suspended() is performed only after
  lock_reset_lock_and_trx_wait() resets trx->lock.wait_lock to NULL.
  Checking trx->lock.wait_lock must be done under lock_mutex.
  */
  lock_mutex_enter();
  if (!lock_wait_trxs_are_still_waiting(cycle_ids, infos)) {
    lock_mutex_exit();
    lock_wait_mutex_exit();
    return false;
  }

  /*
  We can now release lock_wait_mutex, because:

  1. we have verified that trx->lock.wait_lock is not NULL for cycle_ids
  2. we hold lock_sys->mutex
  3. lock_sys->mutex is required to change trx->lock.wait_lock to NULL
  4. only after changing trx->lock.wait_lock to NULL a trx can finish

  So as long as we hold lock_sys->mutex we can access trxs.
  */

  lock_wait_mutex_exit();

  trx_t *const chosen_victim = lock_wait_choose_victim(cycle_ids, infos);
  ut_a(chosen_victim);

  lock_wait_handle_deadlock(chosen_victim, cycle_ids, infos);

  lock_mutex_exit();
  return true;
}

/** Generates a list of `cycle_ids` by following `outgoing` edges from the
`start`.
@param[in,out]    cycle_ids   will contain the list of ids on cycle
@param[in]        start       the first id on the cycle
@param[in]        outgoing    the ids of edge endpoints: id -> outgoing[id] */
static void lock_wait_extract_cycle_ids(ut::vector<uint> &cycle_ids,
                                        const uint start,
                                        const ut::vector<int> &outgoing) {
  cycle_ids.clear();
  uint id = start;
  do {
    cycle_ids.push_back(id);
    id = outgoing[id];
  } while (id != start);
}

/** Assumming that `infos` contains information about all waiting transactions,
and `outgoing[i]` is the endpoint of wait-for edge going out of infos[i].trx,
or -1 if the transaction is not waiting, it identifies and handles all cycles
in the wait-for graph
@param[in]      infos         Information about all waiting transactions
@param[in]      outgoing      The ids of edge endpoints. If outgoing[id] == -1,
                              then there is no edge going out of id, otherwise
                              infos[id].trx waits for infos[outgoing[id]].trx */
static void lock_wait_find_and_handle_deadlocks(
    const ut::vector<waiting_trx_info_t> &infos,
    const ut::vector<int> &outgoing) {
  ut_ad(infos.size() == outgoing.size());
  /** We are going to use int and uint to store positions within infos */
  ut_ad(infos.size() < (1 << 20));
  const auto n = static_cast<uint>(infos.size());
  ut::vector<uint> cycle_ids;
  cycle_ids.clear();
  ut::vector<uint> colors;
  colors.clear();
  colors.resize(n, 0);
  uint current_color = 0;
  for (uint start = 0; start < n; ++start) {
    if (colors[start] != 0) {
      /* This node was already fully processed*/
      continue;
    }
    ++current_color;
    for (int id = start; 0 <= id; id = outgoing[id]) {
      /* We don't expect transaction to deadlock with itself only
      and we do not handle cycles of length=1 correctly */
      ut_ad(id != outgoing[id]);
      if (colors[id] == 0) {
        /* This node was never visited yet */
        colors[id] = current_color;
        continue;
      }
      /* This node was already visited:
      - either it has current_color which means we've visited it during current
        DFS descend, which means we have found a cycle, which we need to verify,
      - or, it has a color used in a previous DFS which means that current DFS
        path merges into an already processed portion of wait-for graph, so we
        can stop now */
      if (colors[id] == current_color) {
        /* found a candidate cycle! */
        lock_wait_extract_cycle_ids(cycle_ids, id, outgoing);
        if (lock_wait_check_candidate_cycle(cycle_ids, infos)) {
          MONITOR_INC(MONITOR_DEADLOCK);
        } else {
          MONITOR_INC(MONITOR_DEADLOCK_FALSE_POSITIVES);
        }
      }
      break;
    }
  }
  MONITOR_INC(MONITOR_DEADLOCK_ROUNDS);
  MONITOR_SET(MONITOR_LOCK_THREADS_WAITING, n);
}

/** Takes a snapshot of transactions in waiting currently in slots, searches for
deadlocks among them and resolves them. */
static void lock_wait_check_for_deadlocks() {
  /*
  Note: I was tempted to declare `infos` as `static`, or at least
  declare it in lock_wait_timeout_thread() and reuse the same instance
  over and over again to avoid allocator calls caused by push_back()
  calls inside lock_wait_snapshot_waiting_threads() while we hold
  lock_sys->lock_wait_mutex. I was afraid, that allocator might need
  to block on some internal mutex in order to synchronize with other
  threads using allocator, and this could in turn cause contention on
  lock_wait_mutex. I hoped, that since vectors never shrink,
  and only grow, then keeping a single instance of `infos` alive for the whole
  lifetime of the thread should increase performance, because after some
  initial period of growing, the allocations will never have to occur again.
  But, I've run many many various experiments, with/without static,
  with infos declared outside, with reserve(n) using various values of n
  (128, srv_max_n_threads, even a simple ML predictor), and nothing, NOTHING
  was faster than just using local vector as we do here (at least on
  tetra01, tetra02, when comparing ~70 runs of each algorithm on uniform,
  pareto, 128 and 1024 usrs).
  This was counter-intuitive to me, so I've checked how malloc is implemented
  and it turns out that modern malloc is a variant of ptmalloc2 and uses a
  large number of independent arenas (larger than number of threads), and
  various smart heuristics to map threads to these arenas in a way which
  avoids blocking.
  So, before changing the way we handle allocation of memory in these
  lock_wait_* routines, please, PLEASE, first check empirically if it really
  helps by running many tests and checking for statistical significance,
  as variance seems to be large.
  (And the downside of using `static` is that it is not very obvious
  how our custom deallocator, which tries to update some stats, would
  behave during shutdown).
  Anyway, in case some tests will indicate that taking a snapshot is a
  bottleneck, one can check if declaring this vectors as static solves the
  issue.
  */
  if (innobase_deadlock_detect) {
    ut::vector<waiting_trx_info_t> infos;
    ut::vector<int> outgoing;

    lock_wait_snapshot_waiting_threads(infos);
    lock_wait_build_wait_for_graph(infos, outgoing);

    lock_wait_find_and_handle_deadlocks(infos, outgoing);
  }
}

/** A thread which wakes up threads whose lock wait may have lasted too long,
analyzes wait-for-graph changes, and checks for deadlocks and resolves them */
void lock_wait_timeout_thread() {
  int64_t sig_count = 0;
  os_event_t event = lock_sys->timeout_event;

  ut_ad(!srv_read_only_mode);

  /** The last time we've checked for timeouts. */
  auto last_checked_for_timeouts_at = ut_time();
  do {
    auto now = ut_time();
    if (0.5 < ut_difftime(now, last_checked_for_timeouts_at)) {
      last_checked_for_timeouts_at = now;
      lock_wait_check_slots_for_timeouts();
    }

    lock_wait_check_for_deadlocks();

    /* When someone is waiting for a lock, we wake up every second (at worst)
    and check if a timeout has passed for a lock wait */
    os_event_wait_time_low(event, 1000000, sig_count);
    sig_count = os_event_reset(event);

  } while (srv_shutdown_state.load() == SRV_SHUTDOWN_NONE);
}
