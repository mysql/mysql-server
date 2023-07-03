/* Copyright (c) 2022, Oracle and/or its affiliates.

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

#ifndef XA_SQL_CMD_XA_SECOND_PHASE
#define XA_SQL_CMD_XA_SECOND_PHASE

#include "sql/mdl_context_backup.h"  // MDL_context_backup_manager
#include "sql/sql_cmd.h"             // Sql_cmd
#include "sql/xa.h"                  // xid_t

/**
  @class Sql_cmd_xa_second_phase

  This class abstracts some functionality used by XA statements involved in
  the second phase of the the XA two-phase commit, specifically, `XA
  COMMIT` and `XA ROLLBACK` SQL statements.

  Common usage of the available methods target the process of detached XA
  transactions, code pattern looks like:

      bool Sql_cmd_xa_statement::process_detached_xa_statement(THD *thd) {
        DBUG_TRACE;

        if (this->find_and_initialize_xa_context(thd)) return true;
        if (this->acquire_locks(thd)) return true;
        raii::Sentry<> xa_lock_guard{
            [this]() -> void { this->release_locks(); }};
        this->setup_thd_context(thd);
        if (this->enter_commit_order(thd)) return true;

        this->assign_xid_to_thd(thd);

        // SPECIFIC STATEMENT EXECUTION HERE
        this->m_result = exec_statement(thd);

        this->exit_commit_order(thd);
        this->cleanup_context(thd);

        return this->m_result;
      }

  @see Sql_cmd
*/
class Sql_cmd_xa_second_phase : public Sql_cmd {
 public:
  /**
    Class constructor.

    @param xid_arg XID of the XA transacation about to be committed
   */
  Sql_cmd_xa_second_phase(xid_t *xid_arg);
  virtual ~Sql_cmd_xa_second_phase() override = default;

 protected:
  /** The XID associated with the underlying XA transaction. */
  xid_t *m_xid{nullptr};
  /** The MDL savepoint used to rollback the MDL context when transient errors
      occur */
  MDL_savepoint m_mdl_savepoint;
  /** The detached transaction context, retrieved from the transaction cache */
  std::shared_ptr<Transaction_ctx> m_detached_trx_context{nullptr};
  /** Whether or not the initialization of GTIDs returned an error */
  bool m_gtid_error{false};
  /** Whether or not the OWNED_GTID related structures need to be cleaned */
  bool m_need_clear_owned_gtid{false};
  /** The incremental success of the several initialization and deinitialization
      steps. Is mainly used to steer some of the deinitialization calls */
  bool m_result{false};

  /**
    Tries to find and initialize the `Transaction_ctx` for the underlying
    detached XA transaction.

    Execution is as follows:
    1. Find transaction in the transaction cache.
    2. Ensure that the underlying state regards the detached XA
       transaction.

    @param thd The THD session object used to process the detached XA
               transaction.

    @return false if the transaction context was successfully initialized,
            true otherwise.
   */
  bool find_and_initialize_xa_context(THD *thd);
  /**
    Tries to acquire the locks necessary to finalize the underlying
    detached XA transaction. By function exit, all locks have been acquired
    or none.

    Execution is as follows:
    1. An MDL savepoint is set, in order to allow the rollback to this
       point if a timeout/locking error is triggered.
    2. XID_STATE::m_xa_lock is acquired to prevent concurrent finalization
       of the transaction (triggered from different client connections, for
       instance).
    3. Ensure that between steps 1. and 2. some other session didn't
       finalize the transaction, by confirming that the transaction is in
       the transaction cache and still in prepared state.
    4. Acquire MDL commit locks.

    @param thd The THD session object used to process the detached XA
               transaction.

    @return false if all necessary locks were acquired, true otherwise.
   */
  bool acquire_locks(THD *thd);
  /**
    Release any locks acquires in `acquire_locks` still needing
    to be released.
   */
  void release_locks() const;
  /**
    Initializes the necessary parts of the `thd` parameter, transferring
    some of the detached XA transaction context to the active session. This
    is necessary to use other parts of the infra-structure that rely on
    having the active THD session properly initialized.

    Execution is as follows:
    1. Determine if GTID infra-structure is consistent and ready to
       finalize the transaction.
    2. Check the detached transaction status.
    3. Transfer to the THD session object the state of the detached XA
       transaction w.r.t whether or not the transaction has already been
       binlogged.

    @param thd The THD session object used to process the detached XA
               transaction.
   */
  void setup_thd_context(THD *thd);
  /**
    For replica applier threads, enters the wait on the commit order.

    Execution is as follows:
    1. Enters the wait on the commit order.
    2. If the wait fails (timeout or possible deadlock found), resets the
       overall state to a point where a retry is possible:
       a. Resets the GTID infra-structure.
       b. Rolls back the MDL context to the recorded savepoint.
       c. Resets the transaction binlogging state.

    @param thd The THD session object used to process the detached XA
               transaction.

    @return false if the commit order wait was successful, true otherwise.
   */
  bool enter_commit_order(THD *thd);
  /**
    Sets the XID_STATE of the THD session object parameter as in detached
    state and copies into it the XID of the detached XA transaction.

    @param thd The THD session object used to process the detached XA
               transaction.
   */
  void assign_xid_to_thd(THD *thd) const;
  /**
    For replica applier threads, finishes the wait on the commit order and
    allows other threads to proceed.

    @param thd The THD session object used to process the detached XA
               transaction.
   */
  void exit_commit_order(THD *thd) const;
  /**
    Cleans up the THD context in order to prepare it for re-use.

    Execution is as follows:
    1. The active THD session binlogging state is cleared.
    2. Any MDL context backup, associated with the detached transaction, is
       deleted.
    3. The detached transaction context is deleted from the transaction
       cache.
    4. GTID state is finalized, either committing or rolling back the GTID
       information.

    @param thd The THD session object used to process the detached XA
               transaction.
   */
  void cleanup_context(THD *thd) const;
  /**
    Disposes of member variables that need it, because destructors for `Sql_cmd`
    classes aren't invoked (since they are created in the internal memory pool,
    memory is disposed as a all block).
   */
  void dispose();
};

#endif  // XA_SQL_CMD_XA_SECOND_PHASE
