/* Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <algorithm>

#include "sql/binlog.h"
#include "sql/debug_sync.h"  // DEBUG_SYNC
#include "sql/rpl_rli_pdb.h" // Slave_worker
#include "sql/rpl_slave_commit_order_manager.h"  // Commit_order_manager
#include "sql/rpl_stage_manager.h"

using std::max;

class Slave_worker;
class Commit_order_manager;

#define YESNO(X) ((X) ? "yes" : "no")

bool Stage_manager::Mutex_queue::append(THD *first) {
  DBUG_TRACE;
  lock();
  DBUG_PRINT("enter", ("first: 0x%llx", (ulonglong)first));
  DBUG_PRINT("info",
             ("m_first: 0x%llx, &m_first: 0x%llx, m_last: 0x%llx",
              (ulonglong)m_first, (ulonglong)&m_first, (ulonglong)m_last));
  int32 count = 1;
  bool empty = (m_first == NULL);
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
  DBUG_ASSERT(m_first || m_last == &m_first);
  DBUG_PRINT("return", ("empty: %s", YESNO(empty)));
  unlock();
  return empty;
}

std::pair<bool, THD *> Stage_manager::Mutex_queue::pop_front() {
  DBUG_TRACE;
  lock();
  THD *result = m_first;
  bool more = true;
  /*
    We do not set next_to_commit to NULL here since this is only used
    in the flush stage. We will have to call fetch_queue last here,
    and will then "cut" the linked list by setting the end of that
    queue to NULL.
  */
  if (result) m_first = result->next_to_commit;
  if (m_first == NULL) {
    more = false;
    m_last = &m_first;
  }
  DBUG_ASSERT(m_size.load() > 0);
  --m_size;
  DBUG_ASSERT(m_first || m_last == &m_first);
  unlock();
  DBUG_PRINT("return",
             ("result: 0x%llx, more: %s", (ulonglong)result, YESNO(more)));
  return std::make_pair(more, result);
}

bool Stage_manager::enroll_for(StageID stage, THD *thd,
                               mysql_mutex_t *stage_mutex) {
  // If the queue was empty: we're the leader for this batch
  DBUG_PRINT("debug",
             ("Enqueue 0x%llx to queue for stage %d", (ulonglong)thd, stage));
  bool leader = m_queue[stage].append(thd);

  if (stage == FLUSH_STAGE && has_commit_order_manager(thd)) {
    Slave_worker *worker = dynamic_cast<Slave_worker *>(thd->rli_slave);
    Commit_order_manager *mngr = worker->get_commit_order_manager();

    mngr->unregister_trx(worker);
  }

  /*
    We do not need to unlock the stage_mutex if it is LOCK_log when rotating
    binlog caused by logging incident log event, since it should be held
    always during rotation.
  */
  bool need_unlock_stage_mutex =
      !(mysql_bin_log.is_rotating_caused_by_incident &&
        stage_mutex == mysql_bin_log.get_log_lock());

  /*
    The stage mutex can be NULL if we are enrolling for the first
    stage.
  */
  if (stage_mutex && need_unlock_stage_mutex) mysql_mutex_unlock(stage_mutex);

#ifndef DBUG_OFF
  DBUG_PRINT("info", ("This is a leader thread: %d (0=n 1=y)", leader));

  DEBUG_SYNC(thd, "after_enrolling_for_stage");

  switch (stage) {
    case Stage_manager::FLUSH_STAGE:
      DEBUG_SYNC(thd, "bgc_after_enrolling_for_flush_stage");
      break;
    case Stage_manager::SYNC_STAGE:
      DEBUG_SYNC(thd, "bgc_after_enrolling_for_sync_stage");
      break;
    case Stage_manager::COMMIT_STAGE:
      DEBUG_SYNC(thd, "bgc_after_enrolling_for_commit_stage");
      break;
    default:
      // not reached
      DBUG_ASSERT(0);
  }

  DBUG_EXECUTE_IF("assert_leader", DBUG_ASSERT(leader););
  DBUG_EXECUTE_IF("assert_follower", DBUG_ASSERT(!leader););
#endif

  /*
    If the queue was not empty, we're a follower and wait for the
    leader to process the queue. If we were holding a mutex, we have
    to release it before going to sleep.
  */
  if (!leader) {
    mysql_mutex_lock(&m_lock_done);
#ifndef DBUG_OFF
    /*
      Leader can be awaiting all-clear to preempt follower's execution.
      With setting the status the follower ensures it won't execute anything
      including thread-specific code.
    */
    thd->get_transaction()->m_flags.ready_preempt = 1;
    if (leader_await_preempt_status) mysql_cond_signal(&m_cond_preempt);
#endif
    while (thd->tx_commit_pending) mysql_cond_wait(&m_cond_done, &m_lock_done);
    mysql_mutex_unlock(&m_lock_done);
  }
  return leader;
}

THD *Stage_manager::Mutex_queue::fetch_and_empty() {
  DBUG_TRACE;
  lock();
  DBUG_PRINT("enter",
             ("m_first: 0x%llx, &m_first: 0x%llx, m_last: 0x%llx",
              (ulonglong)m_first, (ulonglong)&m_first, (ulonglong)m_last));
  THD *result = m_first;
  m_first = NULL;
  m_last = &m_first;
  DBUG_PRINT("info",
             ("m_first: 0x%llx, &m_first: 0x%llx, m_last: 0x%llx",
              (ulonglong)m_first, (ulonglong)&m_first, (ulonglong)m_last));
  DBUG_PRINT("info", ("fetched queue of %d transactions", m_size.load()));
  DBUG_PRINT("return", ("result: 0x%llx", (ulonglong)result));
  DBUG_ASSERT(m_size.load() >= 0);
  m_size.store(0);
  unlock();
  return result;
}

void Stage_manager::wait_count_or_timeout(ulong count, long usec,
                                          StageID stage) {
  long to_wait = DBUG_EVALUATE_IF("bgc_set_infinite_delay", LONG_MAX, usec);
  /*
    For testing purposes while waiting for inifinity
    to arrive, we keep checking the queue size at regular,
    small intervals. Otherwise, waiting 0.1 * infinite
    is too long.
   */
  long delta = DBUG_EVALUATE_IF("bgc_set_infinite_delay", 100000,
                                max<long>(1, (to_wait * 0.1)));

  while (
      to_wait > 0 &&
      (count == 0 || static_cast<ulong>(m_queue[stage].get_size()) < count)) {
#ifndef DBUG_OFF
    if (current_thd) DEBUG_SYNC(current_thd, "bgc_wait_count_or_timeout");
#endif
    my_sleep(delta);
    to_wait -= delta;
  }
}

void Stage_manager::signal_done(THD *queue) {
  mysql_mutex_lock(&m_lock_done);
  for (THD *thd = queue; thd; thd = thd->next_to_commit)
    thd->tx_commit_pending = false;
  mysql_mutex_unlock(&m_lock_done);
  mysql_cond_broadcast(&m_cond_done);
}

#ifndef DBUG_OFF
void Stage_manager::clear_preempt_status(THD *head) {
  DBUG_ASSERT(head);

  mysql_mutex_lock(&m_lock_done);
  while (!head->get_transaction()->m_flags.ready_preempt) {
    leader_await_preempt_status = true;
    mysql_cond_wait(&m_cond_preempt, &m_lock_done);
  }
  leader_await_preempt_status = false;
  mysql_mutex_unlock(&m_lock_done);
}
#endif
