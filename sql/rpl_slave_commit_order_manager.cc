/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/rpl_slave_commit_order_manager.h"

#include "debug_sync.h"  // debug_sync_set_action
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_sys.h"
#include "mysql/components/services/psi_stage_bits.h"
#include "mysql/psi/mysql_cond.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysqld_error.h"
#include "sql/binlog.h"
#include "sql/mdl.h"
#include "sql/mysqld.h"       // key_commit_order_manager_mutex ..
#include "sql/rpl_rli_pdb.h"  // Slave_worker
#include "sql/sql_class.h"
#include "sql/sql_error.h"

Commit_order_manager::Commit_order_manager(uint32 worker_numbers)
    : m_rollback_trx(false),
      m_workers(worker_numbers),
      queue_head(QUEUE_EOF),
      queue_tail(QUEUE_EOF) {
  mysql_mutex_init(key_commit_order_manager_mutex, &m_mutex, NULL);
  for (uint32 i = 0; i < worker_numbers; i++) {
    mysql_cond_init(key_commit_order_manager_cond, &m_workers[i].cond);
    m_workers[i].status = OCS_FINISH;
  }
}

Commit_order_manager::~Commit_order_manager() {
  mysql_mutex_destroy(&m_mutex);

  for (uint32 i = 0; i < m_workers.size(); i++) {
    mysql_cond_destroy(&m_workers[i].cond);
  }
}

void Commit_order_manager::register_trx(Slave_worker *worker) {
  DBUG_ENTER("Commit_order_manager::register_trx");

  mysql_mutex_lock(&m_mutex);

  m_workers[worker->id].status = OCS_WAIT;
  queue_push(worker->id);

  mysql_mutex_unlock(&m_mutex);
  DBUG_VOID_RETURN;
}

/**
  Waits until it becomes the queue head.

  @retval false All previous threads succeeded so this thread can go
  ahead and commit.
*/
bool Commit_order_manager::wait_for_its_turn(Slave_worker *worker, bool all) {
  DBUG_ENTER("Commit_order_manager::wait_for_its_turn");

  /*
    When prior transaction fail, current trx should stop and wait for signal
    to rollback itself
  */
  if ((all || ending_single_stmt_trans(worker->info_thd, all) ||
       m_rollback_trx) &&
      m_workers[worker->id].status == OCS_WAIT) {
    PSI_stage_info old_stage;
    mysql_cond_t *cond = &m_workers[worker->id].cond;
    THD *thd = worker->info_thd;

    DBUG_PRINT("info", ("Worker %lu is waiting for commit signal", worker->id));

    mysql_mutex_lock(&m_mutex);
    thd->ENTER_COND(cond, &m_mutex,
                    &stage_worker_waiting_for_its_turn_to_commit, &old_stage);

    while (queue_front() != worker->id) {
      if (unlikely(worker->found_order_commit_deadlock())) {
        mysql_mutex_unlock(&m_mutex);
        thd->EXIT_COND(&old_stage);
        DBUG_RETURN(true);
      }
      mysql_cond_wait(cond, &m_mutex);
    }

    mysql_mutex_unlock(&m_mutex);
    thd->EXIT_COND(&old_stage);

    m_workers[worker->id].status = OCS_SIGNAL;

    if (m_rollback_trx) {
      unregister_trx(worker);

      DBUG_PRINT("info", ("thd has seen an error signal from old thread"));
      thd->get_stmt_da()->set_overwrite_status(true);
      my_error(ER_SLAVE_WORKER_STOPPED_PREVIOUS_THD_ERROR, MYF(0));
    }
  }

  DBUG_RETURN(m_rollback_trx);
}

void Commit_order_manager::unregister_trx(Slave_worker *worker) {
  DBUG_ENTER("Commit_order_manager::unregister_trx");

  if (m_workers[worker->id].status == OCS_SIGNAL) {
    DBUG_PRINT("info",
               ("Worker %lu is signalling next transaction", worker->id));

    mysql_mutex_lock(&m_mutex);

    DBUG_ASSERT(!queue_empty());

    /* Set next manager as the head and signal the trx to commit. */
    queue_pop();
    if (!queue_empty()) mysql_cond_signal(&m_workers[queue_front()].cond);

    m_workers[worker->id].status = OCS_FINISH;

    mysql_mutex_unlock(&m_mutex);
  }

  DBUG_VOID_RETURN;
}

void Commit_order_manager::report_rollback(Slave_worker *worker) {
  DBUG_ENTER("Commit_order_manager::report_rollback");

  (void)wait_for_its_turn(worker, true);
  /* No worker can set m_rollback_trx unless it is its turn to commit */
  m_rollback_trx = true;
  unregister_trx(worker);

  DBUG_VOID_RETURN;
}

void Commit_order_manager::report_deadlock(Slave_worker *worker) {
  DBUG_ENTER("Commit_order_manager::report_deadlock");
  mysql_mutex_lock(&m_mutex);
  worker->report_order_commit_deadlock();
  DBUG_EXECUTE_IF("rpl_fake_cod_deadlock", {
    const char act[] = "now signal reported_deadlock";
    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });
  mysql_cond_signal(&m_workers[worker->id].cond);
  mysql_mutex_unlock(&m_mutex);
  DBUG_VOID_RETURN;
}
