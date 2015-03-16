/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "rpl_slave_commit_order_manager.h"

#include "rpl_rli_pdb.h"     // Slave_worker
#include "mysqld.h"          // key_commit_order_manager_mutex ..

Commit_order_manager::Commit_order_manager(uint32 worker_numbers)
  : m_rollback_trx(false), m_workers(worker_numbers), queue_head(QUEUE_EOF),
    queue_tail(QUEUE_EOF)
{
  mysql_mutex_init(key_commit_order_manager_mutex, &m_mutex, NULL);
  for (uint32 i= 0; i < worker_numbers; i++)
  {
    mysql_cond_init(key_commit_order_manager_cond, &m_workers[i].cond);
    m_workers[i].status= OCS_FINISH;
  }
}

void Commit_order_manager::register_trx(Slave_worker *worker)
{
  DBUG_ENTER("Commit_order_manager::register_thread");

  mysql_mutex_lock(&m_mutex);

  m_workers[worker->id].status= OCS_WAIT;
  queue_push(worker->id);

  mysql_mutex_unlock(&m_mutex);
  DBUG_VOID_RETURN;
}

/**
  Waits until it becomes the queue head.

  @retval false All previous threads succeeded so this thread can go
  ahead and commit.
*/
bool Commit_order_manager::wait_for_its_turn(Slave_worker *worker,
                                                  bool all)
{
  DBUG_ENTER("Commit_order_manager::wait_for_older_threads");

  /*
    When prior transaction fail, current trx should stop and wait for signal
    to rollback itself
  */
  if ((all || ending_single_stmt_trans(worker->info_thd, all) || m_rollback_trx) &&
      m_workers[worker->id].status == OCS_WAIT)
  {
    PSI_stage_info old_stage;
    mysql_cond_t *cond= &m_workers[worker->id].cond;
    THD *thd= worker->info_thd;

    DBUG_PRINT("info", ("Worker %lu is waiting for commit signal", worker->id));

    mysql_mutex_lock(&m_mutex);
    thd->ENTER_COND(cond, &m_mutex,
                    &stage_worker_waiting_for_its_turn_to_commit,
                    &old_stage);

    while (queue_front() != worker->id)
      mysql_cond_wait(cond, &m_mutex);

    mysql_mutex_unlock(&m_mutex);
    thd->EXIT_COND(&old_stage);

    m_workers[worker->id].status= OCS_SIGNAL;

    if (m_rollback_trx)
      unregister_trx(worker);
  }

  DBUG_RETURN(m_rollback_trx);
}

void Commit_order_manager::unregister_trx(Slave_worker *worker)
{
  DBUG_ENTER("Commit_order_manager::signal_newer_threads");

  if (m_workers[worker->id].status == OCS_SIGNAL)
  {
    DBUG_PRINT("info", ("Worker %lu is signalling next transaction", worker->id));

    mysql_mutex_lock(&m_mutex);

    DBUG_ASSERT(!queue_empty());

    /* Set next manager as the head and signal the trx to commit. */
    queue_pop();
    if (!queue_empty())
      mysql_cond_signal(&m_workers[queue_front()].cond);

    m_workers[worker->id].status= OCS_FINISH;

    mysql_mutex_unlock(&m_mutex);
  }

  DBUG_VOID_RETURN;
}

void Commit_order_manager::report_rollback(Slave_worker *worker)
{
  DBUG_ENTER("Commit_order_manager::rollback_trx");

  (void) wait_for_its_turn(worker, true);
  /* No worker can set m_rollback_trx unless it is its turn to commit */
  m_rollback_trx= true;
  unregister_trx(worker);

  DBUG_VOID_RETURN;
}

