/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef RPL_SLAVE_COMMIT_ORDER_MANAGER
#define RPL_SLAVE_COMMIT_ORDER_MANAGER
#include <stddef.h>
#include <memory>
#include <vector>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "mysql/components/services/mysql_cond_bits.h"
#include "mysql/components/services/mysql_mutex_bits.h"
#include "mysql/psi/mysql_cond.h"
#include "mysql/psi/mysql_mutex.h"
#include "sql/rpl_rli_pdb.h" // get_thd_worker

class THD;

class Commit_order_manager
{
public:
  Commit_order_manager(uint32 worker_numbers);
  ~Commit_order_manager();

  /**
    Register the worker into commit order queue when coordinator dispatches a
    transaction to the worker.
    @param[in] worker The worker which the transaction will be dispatched to.
  */
  void register_trx(Slave_worker *worker);

  /**
    Wait for its turn to commit or unregister.

    @param[in] worker The worker which is executing the transaction.
    @param[in] all    If it is a real transation commit.

    @return
      @retval false  All previous transactions succeed, so this transaction can
                     go ahead and commit.
      @retval true   One or more previous transactions rollback, so this
                     transaction should rollback.
  */
  bool wait_for_its_turn(Slave_worker *worker, bool all);

  /**
    Unregister the transaction from the commit order queue and signal the next
    one to go ahead.

    @param[in] worker The worker which is executing the transaction.
  */
  void unregister_trx(Slave_worker *worker);
  /**
    Wait its turn to rollback and report the rollback.

    @param[in] worker The worker which is executing the transaction.
  */
  void report_rollback(Slave_worker *worker);

  /**
    Wait its turn to unregister and unregister the transaction. It is called for
    the cases that trx is already committed, but nothing is binlogged.

    @param[in] worker The worker which is executing the transaction.
  */
  void report_commit(Slave_worker *worker)
  {
    wait_for_its_turn(worker, true);
    unregister_trx(worker);
  }

  void report_deadlock(Slave_worker *worker);
private:
  enum order_commit_status
  {
    OCS_WAIT,
    OCS_SIGNAL,
    OCS_FINISH
  };

  struct worker_info
  {
    uint32 next;
    mysql_cond_t cond;
    enum order_commit_status status;
  };

  mysql_mutex_t m_mutex;
  bool m_rollback_trx;

  /* It stores order commit information of all workers. */
  std::vector<worker_info> m_workers;
  /*
    They are used to construct a transaction queue with trx_info::next together.
    both head and tail point to a slot of m_trx_vector, when the queue is not
    empty, otherwise their value are QUEUE_EOF.
  */
  uint32 queue_head;
  uint32 queue_tail;
  static const uint32 QUEUE_EOF= 0xFFFFFFFF;
  bool queue_empty() { return queue_head == QUEUE_EOF; }

  void queue_pop()
  {
    queue_head= m_workers[queue_head].next;
    if (queue_head == QUEUE_EOF)
      queue_tail= QUEUE_EOF;
  }

  void queue_push(uint32 index)
  {
    if (queue_head == QUEUE_EOF)
      queue_head= index;
    else
      m_workers[queue_tail].next= index;
    queue_tail= index;
    m_workers[index].next= QUEUE_EOF;
  }

  uint32 queue_front() { return queue_head; }

  // Copy constructor is not implemented
  Commit_order_manager(const Commit_order_manager&);
  Commit_order_manager& operator=(const Commit_order_manager&);
};

/**
   Check if order commit deadlock happens.

   Worker1(trx1)                     Worker2(trx2)
   =============                     =============
   ...                               ...
   Engine acquires lock A
   ...                               Engine acquires lock A(waiting for
                                     trx1 to release it.
   COMMIT(waiting for
   trx2 to commit first).

   Currently, there are two corner cases can cause the deadlock.
   - Case 1
     CREATE TABLE t1(c1 INT PRIMARY KEY, c2 INT, INDEX(c2)) ENGINE = InnoDB;
     INSERT INTO t1 VALUES(1, NULL),(2, 2), (3, NULL), (4, 4), (5, NULL), (6, 6)

     INSERT INTO t1 VALUES(7, NULL);
     DELETE FROM t1 WHERE c2 <= 3;

   - Case 2
     ANALYZE TABLE t1;
     INSERT INTO t2 SELECT * FROM mysql.innodb_table_stats

   Since this is not a real lock deadlock, it could not be handled by engine.
   slave need to handle it separately.
   Worker1(trx1)                     Worker2(trx2)
   =============                     =============
   ...                               ...
   Engine acquires lock A
   ...                               Engine acquires lock A.
                                     1. found trx1 is holding the lock.)
                                     2. report the lock wait to server code by
                                        calling thd_report_row_lock_wait().
                                        Then this function is called to check
                                        if it causes a order commit deadlock.
                                        Report the deadlock to worker1.
                                     3. waiting for trx1 to release it.
   COMMIT(waiting for
   trx2 to commit first).
   Found the deadlock flag set
   by worker2 and then
   return with ER_LOCK_DEADLOCK.

   Rollback the transaction
                                    Get lock A and go ahead.
                                    ...
   Retry the transaction

   To conclude, The transaction A which is waiting for transaction B to commit
   and is holding a lock which is required by transaction B will be rolled back
   and try again later.

   @param[in] thd_self     The THD object of self session which is acquiring
                           a lock hold by another session.
   @param[in] thd_wait_for The THD object of a session which is holding
                           a lock being acquired by current session.
*/
inline void commit_order_manager_check_deadlock(THD* thd_self,
                                                THD *thd_wait_for)
{
  DBUG_ENTER("commit_order_manager_check_deadlock");

  Slave_worker *self_w= get_thd_worker(thd_self);
  Slave_worker *wait_for_w= get_thd_worker(thd_wait_for);
  Commit_order_manager *mngr= self_w->get_commit_order_manager();

  /* Check if both workers are working for the same channel */
  if (mngr != NULL && self_w->c_rli == wait_for_w->c_rli &&
      wait_for_w->sequence_number() > self_w->sequence_number())
  {
    DBUG_PRINT("info", ("Found slave order commit deadlock"));
    mngr->report_deadlock(wait_for_w);
  }
  DBUG_VOID_RETURN;
}

#endif /*RPL_SLAVE_COMMIT_ORDER_MANAGER*/
