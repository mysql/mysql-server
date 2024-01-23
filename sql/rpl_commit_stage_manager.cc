/* Copyright (c) 2019, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <algorithm>

#include "mutex_lock.h"            // MUTEX_LOCK
#include "mysql/psi/mysql_cond.h"  // mysql_cond_timedwait
#include "sql/binlog.h"
#include "sql/binlog/group_commit/bgc_ticket_manager.h"  // Bgc_ticket_manager
#include "sql/debug_sync.h"                              // DEBUG_SYNC
#include "sql/mysqld.h"       //PSI_stage_info stage_wait_on_commit_ticket
#include "sql/raii/sentry.h"  // raii::Sentry<>
#include "sql/rpl_commit_stage_manager.h"
#include "sql/rpl_replica_commit_order_manager.h"  // Commit_order_manager
#include "sql/rpl_rli_pdb.h"                       // Slave_worker

class Slave_worker;
class Commit_order_manager;

#define YESNO(X) ((X) ? "yes" : "no")

bool Commit_stage_manager::Mutex_queue::append(THD *first) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("first: 0x%llx", (ulonglong)first));
  DBUG_PRINT("info",
             ("m_first: 0x%llx, &m_first: 0x%llx, m_last: 0x%llx",
              (ulonglong)m_first, (ulonglong)&m_first, (ulonglong)m_last));
  int32 count = 1;
  bool empty = (m_first == nullptr);

  *m_last = first;
  DBUG_PRINT("info",
             ("m_first: 0x%llx, &m_first: 0x%llx, m_last: 0x%llx",
              (ulonglong)m_first, (ulonglong)&m_first, (ulonglong)m_last));
  /*
    Go to the last THD instance of the list. We expect lists to be
    moderately short. If they are not, we need to track the end of
    the queue as well.
  */

  while (first->next_to_commit) {
    count++;
    first = first->next_to_commit;
  }
  m_size += count;

  m_last = &first->next_to_commit;
  DBUG_PRINT("info",
             ("m_first: 0x%llx, &m_first: 0x%llx, m_last: 0x%llx",
              (ulonglong)m_first, (ulonglong)&m_first, (ulonglong)m_last));
  assert(m_first || m_last == &m_first);
  DBUG_PRINT("return", ("empty: %s", YESNO(empty)));
  return empty;
}

std::pair<bool, THD *> Commit_stage_manager::Mutex_queue::pop_front() {
  DBUG_TRACE;
  lock();
  THD *result = m_first;
  bool more = true;
  /*
    We do not set next_to_commit to nullptr here since this is only used
    in the flush stage. We will have to call fetch_queue last here,
    and will then "cut" the linked list by setting the end of that
    queue to nullptr.
  */
  if (result) m_first = result->next_to_commit;
  if (m_first == nullptr) {
    more = false;
    m_last = &m_first;
  }
  assert(m_size.load() > 0);
  --m_size;
  assert(m_first || m_last == &m_first);
  unlock();
  DBUG_PRINT("return",
             ("result: 0x%llx, more: %s", (ulonglong)result, YESNO(more)));
  return std::make_pair(more, result);
}

void Commit_stage_manager::init(PSI_mutex_key key_LOCK_flush_queue,
                                PSI_mutex_key key_LOCK_sync_queue,
                                PSI_mutex_key key_LOCK_commit_queue,
                                PSI_mutex_key key_LOCK_after_commit_queue,
                                PSI_mutex_key key_LOCK_done,
                                PSI_mutex_key key_LOCK_wait_for_group_turn,
                                PSI_cond_key key_COND_done,
                                PSI_cond_key key_COND_flush_queue,
                                PSI_cond_key key_COND_wait_for_group_turn) {
  if (m_is_initialized) return;
  m_is_initialized = true;

  mysql_mutex_init(key_LOCK_done, &m_lock_done, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_COND_done, &m_stage_cond_binlog);
  mysql_cond_init(key_COND_done, &m_stage_cond_commit_order);
  mysql_cond_init(key_COND_flush_queue, &m_stage_cond_leader);
  mysql_cond_init(key_COND_wait_for_group_turn,
                  &this->m_cond_wait_for_ticket_turn);
#ifndef NDEBUG
  leader_thd = nullptr;

  /**
    reuse key_COND_done 'cos a new PSI object would be wasteful in !NDEBUG
  */
  mysql_cond_init(key_COND_done, &m_cond_preempt);
#endif

  /**
    Initialize mutex for flush, sync, commit and after commit stage queue. The
    binlog flush stage and commit order flush stage share same mutex.
  */
  mysql_mutex_init(key_LOCK_flush_queue, &m_queue_lock[BINLOG_FLUSH_STAGE],
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_sync_queue, &m_queue_lock[SYNC_STAGE],
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_commit_queue, &m_queue_lock[COMMIT_STAGE],
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_after_commit_queue,
                   &m_queue_lock[AFTER_COMMIT_STAGE], MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_wait_for_group_turn,
                   &this->m_lock_wait_for_ticket_turn, MY_MUTEX_INIT_FAST);

  m_queue[BINLOG_FLUSH_STAGE].init(&m_queue_lock[BINLOG_FLUSH_STAGE]);
  m_queue[SYNC_STAGE].init(&m_queue_lock[SYNC_STAGE]);
  m_queue[COMMIT_STAGE].init(&m_queue_lock[COMMIT_STAGE]);
  m_queue[AFTER_COMMIT_STAGE].init(&m_queue_lock[AFTER_COMMIT_STAGE]);
  m_queue[COMMIT_ORDER_FLUSH_STAGE].init(&m_queue_lock[BINLOG_FLUSH_STAGE]);
}

void Commit_stage_manager::deinit() {
  if (!m_is_initialized) return;
  m_is_initialized = false;

  mysql_mutex_destroy(&m_queue_lock[BINLOG_FLUSH_STAGE]);
  mysql_mutex_destroy(&m_queue_lock[SYNC_STAGE]);
  mysql_mutex_destroy(&m_queue_lock[COMMIT_STAGE]);
  mysql_mutex_destroy(&m_queue_lock[AFTER_COMMIT_STAGE]);

  mysql_cond_destroy(&m_stage_cond_binlog);
  mysql_cond_destroy(&m_stage_cond_commit_order);
  mysql_cond_destroy(&m_stage_cond_leader);
  mysql_mutex_destroy(&m_lock_done);
  mysql_cond_destroy(&this->m_cond_wait_for_ticket_turn);
  mysql_mutex_destroy(&this->m_lock_wait_for_ticket_turn);
}

void Commit_stage_manager::wait_for_ticket_turn(THD *thd,
                                                bool update_ticket_manager) {
  auto &ticket_ctx = thd->rpl_thd_ctx.binlog_group_commit_ctx();
  if (ticket_ctx.has_waited()) return;

  auto &ticket_manager = binlog::Bgc_ticket_manager::instance();
  binlog::BgcTicket ticket(ticket_ctx.get_session_ticket());

  CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP("before_wait_on_ticket");
  // Check, first, if the session ticket is already being processed, to avoid
  // acquiring the mutex
  if (ticket != ticket_manager.get_front_ticket() &&
      ticket > ticket_manager.get_coalesced_ticket() && !thd->killed) {
    CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP("inside_wait_on_ticket");
    PSI_stage_info old_stage;
    {
      MUTEX_LOCK(guard, &this->m_lock_wait_for_ticket_turn);
      thd->ENTER_COND(&this->m_cond_wait_for_ticket_turn,
                      &this->m_lock_wait_for_ticket_turn,
                      &stage_wait_on_commit_ticket, &old_stage);
      struct timespec abstime;
      while (ticket != ticket_manager.get_front_ticket() &&
             ticket > ticket_manager.get_coalesced_ticket() && !thd->killed) {
        // in rare cases View Changes cause ticket changes with no broadcast
        set_timespec(&abstime, 1);
        mysql_cond_timedwait(&this->m_cond_wait_for_ticket_turn,
                             &this->m_lock_wait_for_ticket_turn, &abstime);
      }
    }
    thd->EXIT_COND(&old_stage);
  }

#ifndef NDEBUG
  if (Binlog_group_commit_ctx::manual_ticket_setting()->load()) {
    assert(ticket >= ticket_manager.get_coalesced_ticket());
  }
#endif

  if (update_ticket_manager) {
    this->update_session_ticket_state(thd);
  }
}

bool Commit_stage_manager::append_to(StageID stage, THD *thd) {
  this->wait_for_ticket_turn(thd, false);
  raii::Sentry<> _ticket_guard{
      [thd, this]() -> void { this->update_session_ticket_state(thd); }};

  lock_queue(stage);
  return m_queue[stage].append(thd);
}

bool Commit_stage_manager::enroll_for(StageID stage, THD *thd,
                                      mysql_mutex_t *stage_mutex,
                                      mysql_mutex_t *enter_mutex) {
  DBUG_TRACE;

  // If the queue was empty: we're the leader for this batch
  DBUG_PRINT("debug",
             ("Enqueue 0x%llx to queue for stage %d", (ulonglong)thd, stage));

  thd->rpl_thd_ctx.binlog_group_commit_ctx().assign_ticket();
  bool leader = this->append_to(stage, thd);

  /*
   if its FLUSH stage queue (BINLOG_FLUSH_STAGE or COMMIT_ORDER_FLUSH_STAGE)
   and not empty then this thread should not become leader as other queue
   already has leader. The leader acquires enter_mutex.
  */
  if (leader) {
    if (stage == COMMIT_ORDER_FLUSH_STAGE) {
      leader = m_queue[BINLOG_FLUSH_STAGE].is_empty();
    } else if (stage == BINLOG_FLUSH_STAGE &&
               !m_queue[COMMIT_ORDER_FLUSH_STAGE].is_empty()) {
      /*
        The current thread is the first one in the binlog queue, but there is
        already a leader for the commit order queue. Then we need to change
        leader, so the commit order leader changes to follower and the current
        threads becomes leader.

        The reason we need to change leader is that the commit order leader
        cannot be leader for binlog threads, since commit order threads have to
        leave the commit group before the binlog threads are done.

        The process to change leader is as follows:
        1. The first thread to enter the flush stage is a commit order thread.
           It becomes commit order leader.
        2. The commit order leader tries to acquire the stage mutex. This may
           take some time, since the mutex is held by the leader for the
           previous commit group.
        3. Meanwhile, a binlog thread enters the flush stage. It reaches this
           point, and waits for signal from the commit order leader.
        4. The commit order leader gets the stage mutex. Then it checks if any
           binlog thread entered the flush stage, finds that one did, and
           decides to change leader.
        5. The commit order leader signals the binlog leader, becomes follower,
           and waits for the commit to complete (just like other followers do).
        6. The binlog leader wakes up by the signal that the commit order leader
           sent in step 5, and performs the group commit.
      */
      CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP("before_binlog_leader_wait");
      while (thd->tx_commit_pending)
        mysql_cond_wait(&m_stage_cond_leader,
                        &m_queue_lock[BINLOG_FLUSH_STAGE]);
    }
  }

  unlock_queue(stage);

  /* Notify next transaction in commit order that it can enter the queue. */
  if (stage == BINLOG_FLUSH_STAGE) {
    Commit_order_manager::finish_one(thd);
    CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP("after_binlog_leader_wait");
  } else if (stage == COMMIT_ORDER_FLUSH_STAGE) {
    Commit_order_manager::finish_one(thd);
  }

  /*
    The stage mutex can be nullptr if we are enrolling for the first
    stage.
  */
  if (stage_mutex) mysql_mutex_unlock(stage_mutex);

#ifndef NDEBUG
  DBUG_PRINT("info", ("This is a leader thread: %d (0=n 1=y)", leader));

  DEBUG_SYNC(thd, "after_enrolling_for_stage");

  switch (stage) {
    case BINLOG_FLUSH_STAGE:
      DEBUG_SYNC(thd, "bgc_after_enrolling_for_flush_stage");
      CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP(
          "bgc_after_enrolling_for_flush_stage");
      break;
    case SYNC_STAGE:
      DEBUG_SYNC(thd, "bgc_after_enrolling_for_sync_stage");
      CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP(
          "bgc_after_enrolling_for_sync_stage");
      break;
    case COMMIT_STAGE:
      DEBUG_SYNC(thd, "bgc_after_enrolling_for_commit_stage");
      CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP("after_writing_to_tc_log");
      break;
    case AFTER_COMMIT_STAGE:
      DEBUG_SYNC(thd, "bgc_after_enrolling_for_after_commit_stage");
      CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP(
          "bgc_after_enrolling_for_after_commit_stage");
      break;
    case COMMIT_ORDER_FLUSH_STAGE:
      break;
    default:
      // not reached
      assert(0);
  }

  DBUG_EXECUTE_IF("assert_leader", assert(leader););
  DBUG_EXECUTE_IF("assert_follower", assert(!leader););
#endif

  /*
    If the queue was not empty, we're a follower and wait for the
    leader to process the queue. If we were holding a mutex, we have
    to release it before going to sleep.
  */
  if (!leader) {
    CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP("before_follower_wait");
    mysql_mutex_lock(&m_lock_done);
#ifndef NDEBUG
    /*
      Leader can be awaiting all-clear to preempt follower's execution.
      With setting the status the follower ensures it won't execute anything
      including thread-specific code.
    */
    thd->get_transaction()->m_flags.ready_preempt = true;
    if (leader_await_preempt_status) mysql_cond_signal(&m_cond_preempt);
#endif
    while (thd->tx_commit_pending) {
      if (stage == COMMIT_ORDER_FLUSH_STAGE) {
        mysql_cond_wait(&m_stage_cond_commit_order, &m_lock_done);
      } else {
        mysql_cond_wait(&m_stage_cond_binlog, &m_lock_done);
      }
    }

    mysql_mutex_unlock(&m_lock_done);
    return false;
  }

#ifndef NDEBUG
  if (stage == Commit_stage_manager::SYNC_STAGE)
    DEBUG_SYNC(thd, "bgc_between_flush_and_sync");
#endif

  if (leader && enter_mutex != nullptr) {
    mysql_mutex_lock(enter_mutex);
  }

  if (stage == COMMIT_ORDER_FLUSH_STAGE) {
    CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP(
        "after_commit_order_thread_becomes_leader");
    lock_queue(stage);

    if (!m_queue[BINLOG_FLUSH_STAGE].is_empty()) {
      mysql_mutex_unlock(enter_mutex);

      THD *binlog_leader = m_queue[BINLOG_FLUSH_STAGE].get_leader();
      binlog_leader->tx_commit_pending = false;

      mysql_cond_signal(&m_stage_cond_leader);
      unlock_queue(stage);

      mysql_mutex_lock(&m_lock_done);
      /* wait for signal from binlog leader */
      CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP(
          "before_commit_order_leader_waits_for_binlog_leader");
      while (thd->tx_commit_pending)
        mysql_cond_wait(&m_stage_cond_commit_order, &m_lock_done);
      mysql_mutex_unlock(&m_lock_done);

      leader = false;
      return leader;
    }
  }

  return leader;
}

THD *Commit_stage_manager::Mutex_queue::fetch_and_empty_acquire_lock() {
  lock();
  THD *ret = fetch_and_empty();
  unlock();
  return ret;
}

THD *Commit_stage_manager::Mutex_queue::fetch_and_empty_skip_acquire_lock() {
  assert_owner();
  return fetch_and_empty();
}

THD *Commit_stage_manager::Mutex_queue::fetch_and_empty() {
  DBUG_TRACE;
  DBUG_PRINT("enter",
             ("m_first: 0x%llx, &m_first: 0x%llx, m_last: 0x%llx",
              (ulonglong)m_first, (ulonglong)&m_first, (ulonglong)m_last));
  THD *result = m_first;
  m_first = nullptr;
  m_last = &m_first;
  DBUG_PRINT("info",
             ("m_first: 0x%llx, &m_first: 0x%llx, m_last: 0x%llx",
              (ulonglong)m_first, (ulonglong)&m_first, (ulonglong)m_last));
  DBUG_PRINT("info", ("fetched queue of %d transactions", m_size.load()));
  DBUG_PRINT("return", ("result: 0x%llx", (ulonglong)result));
  assert(m_size.load() >= 0);
  m_size.store(0);
  return result;
}

void Commit_stage_manager::wait_count_or_timeout(ulong count, long usec,
                                                 StageID stage) {
  long to_wait = DBUG_EVALUATE_IF("bgc_set_infinite_delay", LONG_MAX, usec);
  /*
    For testing purposes while waiting for inifinity
    to arrive, we keep checking the queue size at regular,
    small intervals. Otherwise, waiting 0.1 * infinite
    is too long.
   */
  long delta = DBUG_EVALUATE_IF("bgc_set_infinite_delay", 100000,
                                std::max<long>(1, (to_wait * 0.1)));

  while (
      to_wait > 0 &&
      (count == 0 || static_cast<ulong>(m_queue[stage].get_size()) < count)) {
#ifndef NDEBUG
    if (current_thd) DEBUG_SYNC(current_thd, "bgc_wait_count_or_timeout");
#endif
    my_sleep(delta);
    to_wait -= delta;
  }
}

THD *Commit_stage_manager::fetch_queue_acquire_lock(StageID stage) {
  DBUG_PRINT("debug", ("Fetching queue for stage %d", stage));
  return m_queue[stage].fetch_and_empty_acquire_lock();
}

THD *Commit_stage_manager::fetch_queue_skip_acquire_lock(StageID stage) {
  DBUG_PRINT("debug", ("Fetching queue for stage %d", stage));
  return m_queue[stage].fetch_and_empty_skip_acquire_lock();
}

void Commit_stage_manager::process_final_stage_for_ordered_commit_group(
    THD *first) {
  if (first != nullptr) {
    gtid_state->update_commit_group(first);
    signal_done(first, Commit_stage_manager::COMMIT_ORDER_FLUSH_STAGE);
  }
}

void Commit_stage_manager::signal_done(THD *queue, StageID stage) {
  mysql_mutex_lock(&m_lock_done);

  for (THD *thd = queue; thd; thd = thd->next_to_commit) {
    thd->tx_commit_pending = false;
    thd->rpl_thd_ctx.binlog_group_commit_ctx().reset();
  }

  /* if thread belong to commit order wake only commit order queue threads */
  if (stage == COMMIT_ORDER_FLUSH_STAGE)
    mysql_cond_broadcast(&m_stage_cond_commit_order);
  else
    mysql_cond_broadcast(&m_stage_cond_binlog);

  mysql_mutex_unlock(&m_lock_done);
}

void Commit_stage_manager::signal_end_of_ticket(bool force) {
  auto &ticket_manager = binlog::Bgc_ticket_manager::instance();
  // Check, first, if there are any tickets other than the active, to avoid
  // taking the mutex.
  if (force ||
      ticket_manager.get_front_ticket() != ticket_manager.get_back_ticket()) {
    auto [previous_front, current_front] = ticket_manager.pop_front_ticket();
    // if pop was successful - front changed, notify waiting threads
    if (force || previous_front != current_front) {
      MUTEX_LOCK(guard, &this->m_lock_wait_for_ticket_turn);
      mysql_cond_broadcast(&this->m_cond_wait_for_ticket_turn);
    }
  }
}

void Commit_stage_manager::update_session_ticket_state(THD *thd) {
  auto &ticket_ctx = thd->rpl_thd_ctx.binlog_group_commit_ctx();
  if (ticket_ctx.has_waited()) return;
  if (ticket_ctx.get_session_ticket() >
      binlog::Bgc_ticket_manager::instance().get_coalesced_ticket())
    this->update_ticket_manager(1, ticket_ctx.get_session_ticket());
  ticket_ctx.mark_as_already_waited();
}

void Commit_stage_manager::update_ticket_manager(
    std::uint64_t sessions_count, const binlog::BgcTicket &session_ticket) {
  auto &ticket_manager = binlog::Bgc_ticket_manager::instance();
  ticket_manager.add_processed_sessions_to_front_ticket(sessions_count,
                                                        session_ticket);

  DBUG_EXECUTE_IF("rpl_end_of_ticket_blocked", {
    const char act[] =
        "now signal signal.end_of_ticket_waiting wait_for "
        "signal.end_of_ticket_continue";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });

  this->signal_end_of_ticket();
}

void Commit_stage_manager::finish_session_ticket(THD *thd) {
  auto &ticket_ctx = thd->rpl_thd_ctx.binlog_group_commit_ctx();
  if (ticket_ctx.get_session_ticket().is_set()) {
    this->wait_for_ticket_turn(thd);
    ticket_ctx.reset();
  }
}

void Commit_stage_manager::disable_manual_session_tickets() {
  if (!Binlog_group_commit_ctx::manual_ticket_setting()->load()) return;
  Binlog_group_commit_ctx::manual_ticket_setting()->store(false);
  binlog::Bgc_ticket_manager::instance().coalesce();
  Commit_stage_manager::get_instance().signal_end_of_ticket(true);
}

void Commit_stage_manager::enable_manual_session_tickets() {
  Binlog_group_commit_ctx::manual_ticket_setting()->store(true);
}

#ifndef NDEBUG
void Commit_stage_manager::clear_preempt_status(THD *head) {
  assert(head);

  mysql_mutex_lock(&m_lock_done);
  while (!head->get_transaction()->m_flags.ready_preempt) {
    leader_await_preempt_status = true;
    mysql_cond_wait(&m_cond_preempt, &m_lock_done);
  }
  leader_await_preempt_status = false;
  mysql_mutex_unlock(&m_lock_done);
}
#endif

Commit_stage_manager &Commit_stage_manager::get_instance() {
  static Commit_stage_manager shared_instance;
  return shared_instance;
}
