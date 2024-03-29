/* Copyright (c) 2014, 2023, Oracle and/or its affiliates.

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

#ifndef RPL_REPLICA_COMMIT_ORDER_MANAGER
#define RPL_REPLICA_COMMIT_ORDER_MANAGER
#include <stddef.h>
#include <memory>
#include <vector>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "mysql/components/services/bits/mysql_cond_bits.h"
#include "mysql/components/services/bits/mysql_mutex_bits.h"
#include "sql/changestreams/apply/commit_order_queue.h"  // Commit_order_queue
#include "sql/rpl_rli_pdb.h"                             // get_thd_worker

class THD;
class Commit_order_lock_graph;

/**
  On a replica and only on a replica, this class is responsible for
  committing the applied transactions in the same order as was observed on
  the source.

  The key components of the commit order management are:
  - This class, that wraps the commit order management, allowing for API
    clients to schedule workers for committing, make workers wait for their
    turn to commit, finish up a scheduled worker task and allow for others
    to progress.
  - A commit order queue of type `cs::apply::Commit_order_queue` that holds
    the sequence by which worker threads should commit and the committing
    order state for each of the scheduled workers.
  - The MDL infra-structure which allows for: one worker to wait for
    another to finish when transactions need to be committed in order;
    detect deadlocks involving workers waiting on each other for their turn
    to commit and non-worker threads waiting on meta-data locks held by
    worker threads.

  The worker thread progress stages relevant to the commit order management
  are:
  - REGISTERED: the worker thread as been added to the commit order queue
    by the coordinator and is allowed to start applying the transaction.
  - FINISHED APPLYING: the worker thread just finished applying the
    transaction and checks if it needs to wait for a preceding worker to
    finish committing.
  - REQUESTED GRANT: the worker thread waits on the MDL graph for the
    preceding worker to finish committing.
  - WAITED: the worker thread finished waiting (either is the first in the
    commit order queue or has just been grantted permission to continue).
  - RELEASE NEXT: the worker thread removes itself from the commit order
    queue, checks if there is any worker waiting on the commit order and
    releases such worker iff is the preceding worker for the waiting
    worker.
  - FINISHED: the worker marks itself as available to take on another
    transaction to apply.

  The progress of the worker within the stages:

                                     +-------------------------+
                                     |                         |
                                     v                         |
                                [REGISTERED]                   |
                                     |                         |
                                     v                         |
                            [FINISHED APPLYING]                |
                                     |                         |
                                Worker is                      |
                            first in the queue?                |
                                   /   \                       |
                              yes /     \ no                   |
                                 /       v                     |
                                 \    [REQUESTED GRANT]        |
                                  \     /                      |
                                   \   /                       |
                                    \ /                        |
                                     |                         |
                                     v                         |
                                 [WAITED]                      |
                                     |                         |
                                     v                         |
                              [RELEASE NEXT]                   |
                                     |                         |
                                     v                         |
                                [FINISHED]                     |
                                     |                         |
                                     +-------------------------+

  Lock-free structures and atomic access to variables are used to manage
  the commit order queue and to keep the worker stage transitions. This
  means that there is no atomicity in regards to changes performed in the
  queue or in the MDL graph within a given stage. Hence, stages maybe
  skipped and sequentially scheduled worker threads may overlap in the
  same stage.

  In the context of the following tables, let W1 be a worker that is
  scheduled to commit before some other worker W2.

  The behavior of W2 (rows) towards W1 (columns) in regards to
  thread synchronization, based on the stage of each thread:
+------------+-----------------------------------------------------------------+
|     \   W1 | REGISTERED | FINISHED | REQUESTED | WAITED | RELEASE | FINISHED |
| W2   \     |            | APPLYING |   GRANT   |        |  NEXT   |          |
+------------+------------+----------+-----------+--------+---------+----------+
| REGISTERED |            |          |           |        |         |          |
+------------+------------+----------+-----------+--------+---------+----------+
| FIN. APPL. |            |          |           |        |         |          |
+------------+------------+----------+-----------+--------+---------+----------+
| REQ. GRANT |    WAIT    |   WAIT   |   WAIT    |  WAIT  |  WAIT   |          |
+------------+------------+----------+-----------+--------+---------+----------+
| WAITED     |            |          |           |        |         |          |
+------------+------------+----------+-----------+--------+---------+----------+
| REL. NEXT  |            |          |           |        |  WAIT   |          |
+------------+------------+----------+-----------+--------+---------+----------+
| FINISHED   |            |          |           |        |         |          |
+------------------------------------------------------------------------------+

  The W2 wait when both worker threads are in the RELEASE NEXT stage
  happens in the case W2 never entered the REQUESTED GRANT stage. This case
  may happen if W1 being in RELEASE NEXT removes itself from the queue
  before W2 enters FINISHED APPLYING and then W2 reaches the RELEASE NEXT
  stage before W1 exits it:

             [W1]                                [W2]

     stage = RELEASE NEXT                 stage = REGISTERED
               |                                  |
               v                                  |
          queue.pop()                             v
               |                     stage = FINISHED_APPLYING
               |                                  |
               v                                  v
     next_worker.stage                   queue.front() == W2
        == FINISHED_APPLYING                      |
               |                                  |
               |                                  v
               |                           stage = WAITED
               |                                  |
               |                                  v
               |                          stage = RELEASE NEXT
               |                                  |
               v                                  v
     next_worker.release()                   queue.pop()

  The commit order queue includes mechanisms that block the popping until
  the preceding worker finishes the releasing operation. This wait will
  only be active for the amount of time that takes for W1 to change the
  values of the MDL graph structures needed to release W2, which is a very
  small amount of cycles.

  The behavior of W1 (rows) towards W2 (columns)in regards to thread
  synchronization, based on the stage of each thread:
+------------+-----------------------------------------------------------------+
|     \   W2 | REGISTERED | FINISHED | REQUESTED | WAITED | RELEASE | FINISHED |
| W1   \     |            | APPLYING |   GRANT   |        |  NEXT   |          |
+------------+------------+----------+-----------+--------+---------+----------+
| REGISTERED |            |          |           |        |         |          |
+------------+------------+----------+-----------+--------+---------+----------+
| FIN. APPL. |            |          |           |        |         |          |
+------------+------------+----------+-----------+--------+---------+----------+
| REQ. GRANT |            |          |           |        |         |          |
+------------+------------+----------+-----------+--------+---------+----------+
| WAITED     |            |          |           |        |         |          |
+------------+------------+----------+-----------+--------+---------+----------+
| REL. NEXT  |            |  GRANT   |   GRANT   |        |         |          |
+------------+------------+----------+-----------+--------+---------+----------+
| FINISHED   |            |          |           |        |         |          |
+------------------------------------------------------------------------------+

  The W1 grant to W2 may happen when W2 is either in the FINISHED APPLYING
  or REQUESTED GRANT stages. W1 must also signal the grant when W2 is in
  FINISHED APPLYING because W1 has no way to determine if W2 has already
  evaluated the first element of the queue or not, that is, W1 can't
  determine if W2 will proceed to the REQUESTED GRANT or to the WAITED
  stage. Therefore, W1 will signal in both cases.

 */
class Commit_order_manager {
 public:
  Commit_order_manager(uint32 worker_numbers);
  // Copy logic is not available
  Commit_order_manager(const Commit_order_manager &) = delete;
  ~Commit_order_manager();

  // Copy logic is not available
  Commit_order_manager &operator=(const Commit_order_manager &) = delete;

  /**
    Initializes the MDL context for a given worker in the commit order queue.

    @param worker The worker to initialize the context for
   */
  void init_worker_context(Slave_worker &worker);

  /**
    Register the worker into commit order queue when coordinator dispatches a
    transaction to the worker.

    @param[in] worker The worker which the transaction will be dispatched to.
  */
  void register_trx(Slave_worker *worker);

 private:
  /**
    Determines if the worker passed as a parameter must wait on the MDL graph
    for other workers to commit and, if it must, will wait for it's turn to
    commit.

    @param worker The worker to determine the commit waiting status for.

    @return false if the worker is ready to commit, true if not.
   */
  bool wait_on_graph(Slave_worker *worker);
  /**
    Wait for its turn to commit or unregister.

    @param[in] worker The worker which is executing the transaction.

    @retval false  All previous transactions succeed, so this transaction can
                   go ahead and commit.
    @retval true   One or more previous transactions rollback, so this
                   transaction should rollback.
  */
  bool wait(Slave_worker *worker);

  /**
    Unregister the thread from the commit order queue and signal
    the next thread to awake.

    @param[in] worker     The worker which is executing the transaction.
  */
  void finish_one(Slave_worker *worker);

  /**
    Unregister the transaction from the commit order queue and signal the next
    one to go ahead.

    @param[in] worker     The worker which is executing the transaction.
  */
  void finish(Slave_worker *worker);

  /**
    Reset server_status value of the commit group.

    @param[in] first_thd  The first thread of the commit group that needs
                          server_status to be updated.
  */
  void reset_server_status(THD *first_thd);

  /**
    Get rollback status.

    @retval true   Transactions in the queue should rollback.
    @retval false  Transactions in the queue shouldn't rollback.
  */
  bool get_rollback_status();

  /**
    Set rollback status to true.
  */
  void set_rollback_status();

  /**
    Unset rollback status to false.
  */
  void unset_rollback_status();

  void report_deadlock(Slave_worker *worker);

  std::atomic<bool> m_rollback_trx;

  /* It stores order commit order information of all workers. */
  cs::apply::Commit_order_queue m_workers;

  /**
    Flush record of transactions for all the waiting threads and then
    awake them from their wait. It also calls gtid_state->update_commit_group()
    which updates both the THD and the Gtid_state for whole commit group to
    reflect that the transaction set of transactions has ended.

    @param[in] worker  The worker which is executing the transaction.
  */
  void flush_engine_and_signal_threads(Slave_worker *worker);

 public:
  /**
    Determines if the worker holding the commit order wait ticket
    `wait_for_commit is in deadlock with the MDL context encapsulated in
    the visitor parameter.

    @param wait_for_commit The wait ticket being held by the worker thread.
    @param gvisitor The MDL graph visitor to check for deadlocks against.

    @return true if a deadlock has been found and false otherwise.
   */
  bool visit_lock_graph(Commit_order_lock_graph &wait_for_commit,
                        MDL_wait_for_graph_visitor &gvisitor);

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
       INSERT INTO t1 VALUES(1, NULL),(2, 2), (3, NULL), (4, 4), (5, NULL), (6,
     6)

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
                                       1. found trx1 is holding the lock.
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
     and is holding a lock which is required by transaction B will be rolled
     back and try again later.

     @param[in] thd_self     The THD object of self session which is acquiring
                             a lock hold by another session.
     @param[in] thd_wait_for The THD object of a session which is holding
                             a lock being acquired by current session.
  */
  static void check_and_report_deadlock(THD *thd_self, THD *thd_wait_for);

  /**
    Wait for its turn to commit or unregister.

    @param[in] thd  The THD object of current thread.

    @retval false  All previous transactions succeed, so this transaction can
                   go ahead and commit.
    @retval true   The transaction is marked to rollback.
  */
  static bool wait(THD *thd);

  /**
    Wait for its turn to unregister and signal the next one to go ahead. In case
    error happens while processing transaction, notify the following transaction
    to rollback.

    @param[in] thd    The THD object of current thread.
    @param[in] error  If true failure in transaction execution
  */
  static void wait_and_finish(THD *thd, bool error);

  /**
    Get transaction rollback status.

    @param[in] thd    The THD object of current thread.

    @retval true   Current transaction should rollback.
    @retval false  Current transaction shouldn't rollback.
  */
  static bool get_rollback_status(THD *thd);

  /**
    Unregister the thread from the commit order queue and signal
    the next thread to awake.

    @param[in] thd    The THD object of current thread.
  */
  static void finish_one(THD *thd);

  /**
    Determines whether current thread needs to wait for its turn to commit and
    unregister from the commit order queue. The sql commands ALTER TABLE, DROP
    TABLE, DROP DB, OPTIMIZE TABLE, ANALYZE TABLE and REPAIR TABLE are allowed
    to wait for its turn to commit and unregister from the commit order queue as
    exception in MYSQL_BIN_LOG::ordered_commit(), as these transactions have
    multiple commits and so not determined if the call is ending transaction.

    @param[in] thd  The THD object of current thread.

    @retval true   Allow thread to wait for it turn
    @retval false  Do not allow thread to wait for it turn
  */
  static bool wait_for_its_turn_before_flush_stage(THD *thd);
};

/**
  MDL subgraph inspector class to be used as a ticket to wait on by worker
  threads. Each worker will create its own instance of this class and will use
  its own THD MDL_context to search for deadlocks.
 */
class Commit_order_lock_graph : public MDL_wait_for_subgraph {
 public:
  /**
    Constructor for the class.

    @param ctx The worker THD MDL context object.
    @param mngr The Commit_order_manager instance associated with the current
                channel's Relay_log_info object.
    @param worker_id The identifier of the worker targeted by this object.

   */
  Commit_order_lock_graph(MDL_context &ctx, Commit_order_manager &mngr,
                          uint32 worker_id);
  /**
    Default destructor.
   */
  virtual ~Commit_order_lock_graph() override = default;

  /**
    Retrieves the MDL context object associated with the underlying worker.

    @return A pointer to the MDL context associated with the underlying worker
            thread.
   */
  MDL_context *get_ctx() const;
  /**
    Retrieves the identifier for the underlying worker thread.

    @return The identifier for the underlying worker thread.
   */
  uint32 get_worker_id() const;
  /**
    Determines if the underlying worker is in deadlock with the MDL context
    encapsulated in the visitor parameter.

    @param dvisitor The MDL graph visitor to check for deadlocks against.

    @return true if a deadlock was found and false otherwise,
   */
  bool accept_visitor(MDL_wait_for_graph_visitor *dvisitor) override;
  /**
    Retrieves the deadlock weight to be used to replace a visitor victim's, when
    more than one deadlock is found.
   */
  uint get_deadlock_weight() const override;

 private:
  /** The MDL context object associated with the underlying worker. */
  MDL_context &m_ctx;
  /**
    The Commit_order_manager instance associated with the underlying worker
    channel's Relay_log_info object.
  */
  Commit_order_manager &m_mngr;
  /** The identifier for the underlying worker thread. */
  uint32 m_worker_id{0};
};

/**
  Determines whether current thread shall run the procedure here
  to check whether it waits for its turn (and when its turn comes
  unregister from the commit order queue).

  The sql commands ALTER TABLE, ANALYZE TABLE, DROP DB, DROP EVENT,
  DROP FUNCTION, DROP PROCEDURE, DROP TRIGGER, DROP TABLE, DROP VIEW,
  OPTIMIZE TABLE and REPAIR TABLE shall run this procedure here, as
  an exception, because these transactions have multiple intermediate
  commits. Therefore cannot predetermine when the last commit is
  done.

  @param[in] thd  The THD object of current thread.

  @retval false  Commit_order_manager object is not initialized
  @retval true   Commit_order_manager object is initialized
*/
bool has_commit_order_manager(const THD *thd);

#endif /*RPL_REPLICA_COMMIT_ORDER_MANAGER*/
