/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


#include "transaction.h"
#include "rpl_handler.h"
#include "debug_sync.h"         // DEBUG_SYNC
#include "auth_common.h"            // SUPER_ACL
#include <pfs_transaction_provider.h>
#include <mysql/psi/mysql_transaction.h>
#include "rpl_context.h"
#include "sql_class.h"
#include "log.h"
#include "binlog.h"

/**
  Helper: Tell tracker (if any) that transaction ended.
*/
void trans_track_end_trx(THD *thd)
{
  if (thd->variables.session_track_transaction_info > TX_TRACK_NONE)
  {
    ((Transaction_state_tracker *)
     thd->session_tracker.get_tracker(TRANSACTION_INFO_TRACKER))->end_trx(thd);
  }
}

/**
  Helper: transaction ended, SET TRANSACTION one-shot variables
  revert to session values. Let the transaction state tracker know.
*/
void trans_reset_one_shot_chistics(THD *thd)
{
  if (thd->variables.session_track_transaction_info > TX_TRACK_NONE)
  {
    Transaction_state_tracker *tst= (Transaction_state_tracker *)
      thd->session_tracker.get_tracker(TRANSACTION_INFO_TRACKER);

    tst->set_read_flags(thd, TX_READ_INHERIT);
    tst->set_isol_level(thd, TX_ISOL_INHERIT);
  }

  thd->tx_isolation= (enum_tx_isolation) thd->variables.tx_isolation;
  thd->tx_read_only= thd->variables.tx_read_only;
}

/**
  Check if we have a condition where the transaction state must
  not be changed (committed or rolled back). Currently we check
  that we are not executing a stored program and that we don't
  have an active XA transaction.

  @return TRUE if the commit/rollback cannot be executed,
          FALSE otherwise.
*/

bool trans_check_state(THD *thd)
{
  DBUG_ENTER("trans_check");

  /*
    Always commit statement transaction before manipulating with
    the normal one.
  */
  DBUG_ASSERT(thd->get_transaction()->is_empty(Transaction_ctx::STMT));

  if (unlikely(thd->in_sub_stmt))
  {
    my_error(ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG, MYF(0));
    DBUG_RETURN(true);
  }

  if (thd->get_transaction()->xid_state()->check_in_xa(true))
    DBUG_RETURN(true);

  DBUG_RETURN(false);
}


/**
  Begin a new transaction.

  @note Beginning a transaction implicitly commits any current
        transaction and releases existing locks.

  @param thd     Current thread
  @param flags   Transaction flags

  @retval FALSE  Success
  @retval TRUE   Failure
*/

bool trans_begin(THD *thd, uint flags)
{
  int res= FALSE;
  Transaction_state_tracker *tst= NULL;

  DBUG_ENTER("trans_begin");

  if (trans_check_state(thd))
    DBUG_RETURN(TRUE);

  if (thd->variables.session_track_transaction_info > TX_TRACK_NONE)
    tst= (Transaction_state_tracker *)
      thd->session_tracker.get_tracker(TRANSACTION_INFO_TRACKER);

  thd->locked_tables_list.unlock_locked_tables(thd);

  DBUG_ASSERT(!thd->locked_tables_mode);

  if (thd->in_multi_stmt_transaction_mode() ||
      (thd->variables.option_bits & OPTION_TABLE_LOCK))
  {
    thd->variables.option_bits&= ~OPTION_TABLE_LOCK;
    thd->server_status&=
      ~(SERVER_STATUS_IN_TRANS | SERVER_STATUS_IN_TRANS_READONLY);
    DBUG_PRINT("info", ("clearing SERVER_STATUS_IN_TRANS"));
    res= MY_TEST(ha_commit_trans(thd, TRUE));
  }

  thd->variables.option_bits&= ~OPTION_BEGIN;
  thd->get_transaction()->reset_unsafe_rollback_flags(Transaction_ctx::SESSION);

  if (res)
    DBUG_RETURN(TRUE);

  /*
    Release transactional metadata locks only after the
    transaction has been committed.
  */
  thd->mdl_context.release_transactional_locks();

  // The RO/RW options are mutually exclusive.
  DBUG_ASSERT(!((flags & MYSQL_START_TRANS_OPT_READ_ONLY) &&
                (flags & MYSQL_START_TRANS_OPT_READ_WRITE)));
  if (flags & MYSQL_START_TRANS_OPT_READ_ONLY)
  {
    thd->tx_read_only= true;
    if (tst)
      tst->set_read_flags(thd, TX_READ_ONLY);
  }
  else if (flags & MYSQL_START_TRANS_OPT_READ_WRITE)
  {
    /*
      Explicitly starting a RW transaction when the server is in
      read-only mode, is not allowed unless the user has SUPER priv.
      Implicitly starting a RW transaction is allowed for backward
      compatibility.
    */
    if (check_readonly(thd, true))
      DBUG_RETURN(true);
    thd->tx_read_only= false;
    /*
      This flags that tx_read_only was set explicitly, rather than
      just from the session's default.
    */
    if (tst)
      tst->set_read_flags(thd, TX_READ_WRITE);
  }

  DBUG_EXECUTE_IF("dbug_set_high_prio_trx", {
    DBUG_ASSERT(thd->tx_priority==0);
    thd->tx_priority= 1;
  });

  thd->variables.option_bits|= OPTION_BEGIN;
  thd->server_status|= SERVER_STATUS_IN_TRANS;
  if (thd->tx_read_only)
    thd->server_status|= SERVER_STATUS_IN_TRANS_READONLY;
  DBUG_PRINT("info", ("setting SERVER_STATUS_IN_TRANS"));

  if (tst)
    tst->add_trx_state(thd, TX_EXPLICIT);

  /* ha_start_consistent_snapshot() relies on OPTION_BEGIN flag set. */
  if (flags & MYSQL_START_TRANS_OPT_WITH_CONS_SNAPSHOT)
  {
    if (tst)
      tst->add_trx_state(thd, TX_WITH_SNAPSHOT);
    res= ha_start_consistent_snapshot(thd);
  }

  /*
    Register transaction start in performance schema if not done already.
    We handle explicitly started transactions here, implicitly started
    transactions (and single-statement transactions in autocommit=1 mode)
    are handled in trans_register_ha().
    We can't handle explicit transactions in the same way as implicit
    because we want to correctly attribute statements which follow
    BEGIN but do not touch any transactional tables.
  */
#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  if (thd->m_transaction_psi == NULL)
  {
    thd->m_transaction_psi= MYSQL_START_TRANSACTION(&thd->m_transaction_state,
                                                 NULL, NULL, thd->tx_isolation,
                                                 thd->tx_read_only, false);
    DEBUG_SYNC(thd, "after_set_transaction_psi_before_set_transaction_gtid");
    gtid_set_performance_schema_values(thd);
  }
#endif

  DBUG_RETURN(MY_TEST(res));
}


/**
  Commit the current transaction, making its changes permanent.

  @param thd     Current thread

  @retval FALSE  Success
  @retval TRUE   Failure
*/

bool trans_commit(THD *thd)
{
  int res;
  DBUG_ENTER("trans_commit");

  if (trans_check_state(thd))
    DBUG_RETURN(TRUE);

  thd->server_status&=
    ~(SERVER_STATUS_IN_TRANS | SERVER_STATUS_IN_TRANS_READONLY);
  DBUG_PRINT("info", ("clearing SERVER_STATUS_IN_TRANS"));
  res= ha_commit_trans(thd, TRUE);
  if (res == FALSE)
    if (thd->rpl_thd_ctx.session_gtids_ctx().
        notify_after_transaction_commit(thd))
      sql_print_warning("Failed to collect GTID to send in the response packet!");
  /*
    When gtid mode is enabled, a transaction may cause binlog
    rotation, which inserts a record into the gtid system table
    (which is probably a transactional table). Thence, the flag
    SERVER_STATUS_IN_TRANS may be set again while calling
    ha_commit_trans(...) Consequently, we need to reset it back,
    much like we are doing before calling ha_commit_trans(...).

    We would really only need to do this when gtid_mode=on.  However,
    checking gtid_mode requires holding a lock, which is costly.  So
    we clear the bit unconditionally.  This has no side effects since
    if gtid_mode=off the bit is already cleared.
  */
  thd->server_status&= ~SERVER_STATUS_IN_TRANS;
  thd->variables.option_bits&= ~OPTION_BEGIN;
  thd->get_transaction()->reset_unsafe_rollback_flags(Transaction_ctx::SESSION);
  thd->lex->start_transaction_opt= 0;

  /* The transaction should be marked as complete in P_S. */
  DBUG_ASSERT(thd->m_transaction_psi == NULL);

  thd->tx_priority= 0;

  trans_track_end_trx(thd);

  DBUG_RETURN(MY_TEST(res));
}


/**
  Implicitly commit the current transaction.

  @note A implicit commit does not releases existing table locks.

  @param thd     Current thread

  @retval FALSE  Success
  @retval TRUE   Failure
*/

bool trans_commit_implicit(THD *thd)
{
  bool res= FALSE;
  DBUG_ENTER("trans_commit_implicit");

  /*
    Ensure that trans_check_state() was called before trans_commit_implicit()
    by asserting that conditions that are checked in the former function are
    true.
  */
  DBUG_ASSERT(thd->get_transaction()->is_empty(Transaction_ctx::STMT) &&
              !thd->in_sub_stmt &&
              !thd->get_transaction()->xid_state()->check_in_xa(false));

  if (thd->in_multi_stmt_transaction_mode() ||
      (thd->variables.option_bits & OPTION_TABLE_LOCK))
  {
    /* Safety if one did "drop table" on locked tables */
    if (!thd->locked_tables_mode)
      thd->variables.option_bits&= ~OPTION_TABLE_LOCK;
    thd->server_status&=
      ~(SERVER_STATUS_IN_TRANS | SERVER_STATUS_IN_TRANS_READONLY);
    DBUG_PRINT("info", ("clearing SERVER_STATUS_IN_TRANS"));
    res= MY_TEST(ha_commit_trans(thd, TRUE));
  }
  else if (tc_log)
    tc_log->commit(thd, true);

  if (res == FALSE)
    if (thd->rpl_thd_ctx.session_gtids_ctx().
        notify_after_transaction_commit(thd))
      sql_print_warning("Failed to collect GTID to send in the response packet!");
  thd->variables.option_bits&= ~OPTION_BEGIN;
  thd->get_transaction()->reset_unsafe_rollback_flags(Transaction_ctx::SESSION);

  /* The transaction should be marked as complete in P_S. */
  DBUG_ASSERT(thd->m_transaction_psi == NULL);

  /*
    Upon implicit commit, reset the current transaction
    isolation level and access mode. We do not care about
    @@session.completion_type since it's documented
    to not have any effect on implicit commit.
  */
  trans_reset_one_shot_chistics(thd);

  trans_track_end_trx(thd);

  DBUG_RETURN(res);
}


/**
  Rollback the current transaction, canceling its changes.

  @param thd     Current thread

  @retval FALSE  Success
  @retval TRUE   Failure
*/

bool trans_rollback(THD *thd)
{
  int res;
  DBUG_ENTER("trans_rollback");

  if (trans_check_state(thd))
    DBUG_RETURN(TRUE);

  thd->server_status&=
    ~(SERVER_STATUS_IN_TRANS | SERVER_STATUS_IN_TRANS_READONLY);
  DBUG_PRINT("info", ("clearing SERVER_STATUS_IN_TRANS"));
  res= ha_rollback_trans(thd, TRUE);
  thd->variables.option_bits&= ~OPTION_BEGIN;
  thd->get_transaction()->reset_unsafe_rollback_flags(
      Transaction_ctx::SESSION);
  thd->lex->start_transaction_opt= 0;

  /* The transaction should be marked as complete in P_S. */
  DBUG_ASSERT(thd->m_transaction_psi == NULL);

  thd->tx_priority= 0;

  trans_track_end_trx(thd);

  DBUG_RETURN(MY_TEST(res));
}


/**
  Implicitly rollback the current transaction, typically
  after deadlock was discovered.

  @param thd     Current thread

  @retval False Success
  @retval True  Failure

  @note ha_rollback_low() which is indirectly called by this
        function will mark XA transaction for rollback by
        setting appropriate RM error status if there was
        transaction rollback request.
*/

bool trans_rollback_implicit(THD *thd)
{
  int res;
  DBUG_ENTER("trans_rollback_implict");

  /*
    Always commit/rollback statement transaction before manipulating
    with the normal one.
    Don't perform rollback in the middle of sub-statement, wait till
    its end.
  */
  DBUG_ASSERT(thd->get_transaction()->is_empty(Transaction_ctx::STMT) &&
              !thd->in_sub_stmt);

  thd->server_status&=
    ~(SERVER_STATUS_IN_TRANS | SERVER_STATUS_IN_TRANS_READONLY);
  DBUG_PRINT("info", ("clearing SERVER_STATUS_IN_TRANS"));
  res= ha_rollback_trans(thd, true);
  thd->variables.option_bits&= ~OPTION_BEGIN;
  thd->get_transaction()->reset_unsafe_rollback_flags(
    Transaction_ctx::SESSION);

  /* Rollback should clear transaction_rollback_request flag. */
  DBUG_ASSERT(!thd->transaction_rollback_request);
  /* The transaction should be marked as complete in P_S. */
  DBUG_ASSERT(thd->m_transaction_psi == NULL);

  trans_track_end_trx(thd);

  DBUG_RETURN(MY_TEST(res));
}


/**
  Commit the single statement transaction.

  @note Note that if the autocommit is on, then the following call
        inside InnoDB will commit or rollback the whole transaction
        (= the statement). The autocommit mechanism built into InnoDB
        is based on counting locks, but if the user has used LOCK
        TABLES then that mechanism does not know to do the commit.

  @param thd     Current thread

  @retval FALSE  Success
  @retval TRUE   Failure
*/

bool trans_commit_stmt(THD *thd)
{
  DBUG_ENTER("trans_commit_stmt");
  int res= FALSE;
  /*
    We currently don't invoke commit/rollback at end of
    a sub-statement.  In future, we perhaps should take
    a savepoint for each nested statement, and release the
    savepoint when statement has succeeded.
  */
  DBUG_ASSERT(! thd->in_sub_stmt);

  /*
    Some code in MYSQL_BIN_LOG::commit and ha_commit_low() is not safe
    for attachable transactions.
  */
  DBUG_ASSERT(!thd->is_attachable_ro_transaction_active());

  thd->get_transaction()->merge_unsafe_rollback_flags();

  if (thd->get_transaction()->is_active(Transaction_ctx::STMT))
  {
    res= ha_commit_trans(thd, FALSE);
    if (! thd->in_active_multi_stmt_transaction())
      trans_reset_one_shot_chistics(thd);
  }
  else if (tc_log)
    tc_log->commit(thd, false);
  if (res == FALSE && !thd->in_active_multi_stmt_transaction())
    if (thd->rpl_thd_ctx.session_gtids_ctx().
        notify_after_transaction_commit(thd))
      sql_print_warning("Failed to collect GTID to send in the response packet!");
  /* In autocommit=1 mode the transaction should be marked as complete in P_S */
  DBUG_ASSERT(thd->in_active_multi_stmt_transaction() ||
              thd->m_transaction_psi == NULL);

  thd->get_transaction()->reset(Transaction_ctx::STMT);

  DBUG_RETURN(MY_TEST(res));
}


/**
  Rollback the single statement transaction.

  @param thd     Current thread

  @retval FALSE  Success
  @retval TRUE   Failure
*/
bool trans_rollback_stmt(THD *thd)
{
  DBUG_ENTER("trans_rollback_stmt");

  /*
    We currently don't invoke commit/rollback at end of
    a sub-statement.  In future, we perhaps should take
    a savepoint for each nested statement, and release the
    savepoint when statement has succeeded.
  */
  DBUG_ASSERT(! thd->in_sub_stmt);

  /*
    Some code in MYSQL_BIN_LOG::rollback and ha_rollback_low() is not safe
    for attachable transactions.
  */
  DBUG_ASSERT(!thd->is_attachable_ro_transaction_active());

  thd->get_transaction()->merge_unsafe_rollback_flags();

  if (thd->get_transaction()->is_active(Transaction_ctx::STMT))
  {
    ha_rollback_trans(thd, FALSE);
    if (! thd->in_active_multi_stmt_transaction())
      trans_reset_one_shot_chistics(thd);
  }
  else if (tc_log)
    tc_log->rollback(thd, false);

  if (!thd->owned_gtid.is_empty() &&
      !thd->in_active_multi_stmt_transaction())
  {
    /*
      To a failed single statement transaction on auto-commit mode,
      we roll back its owned gtid if it does not modify
      non-transational table or commit its owned gtid if it has modified
      non-transactional table when rolling back it if binlog is disabled,
      as we did when binlog is enabled.
      We do not need to check if binlog is enabled here, since we already
      released its owned gtid in MYSQL_BIN_LOG::rollback(...) right before
      this if binlog is enabled.
    */
    if (thd->get_transaction()->has_modified_non_trans_table(
          Transaction_ctx::STMT))
      gtid_state->update_on_commit(thd);
    else
      gtid_state->update_on_rollback(thd);
  }

  /* In autocommit=1 mode the transaction should be marked as complete in P_S */
  DBUG_ASSERT(thd->in_active_multi_stmt_transaction() ||
              thd->m_transaction_psi == NULL ||
              /* Todo: BUG#20488921 is in the way. */
              DBUG_EVALUATE_IF("simulate_xa_commit_log_failure", true, false));

  thd->get_transaction()->reset(Transaction_ctx::STMT);

  DBUG_RETURN(FALSE);
}


/**
  Commit the attachable transaction.

  @note This is slimmed down version of trans_commit_stmt() which commits
        attachable transaction but skips code which is unnecessary and
        unsafe for them (like dealing with GTIDs).

  @param thd     Current thread

  @retval False - Success
  @retval True  - Failure
*/
bool trans_commit_attachable(THD *thd)
{
  DBUG_ENTER("trans_commit_attachable");
  int res= 0;

  /* This function only handles attachable transactions. */
  DBUG_ASSERT(thd->is_attachable_ro_transaction_active());

  /*
    Since the attachable transaction is AUTOCOMMIT we only need to commit
    statement transaction.
  */
  DBUG_ASSERT(! thd->get_transaction()->is_active(Transaction_ctx::SESSION));

  /* Attachable transactions should not do anything unsafe. */
  DBUG_ASSERT(!thd->get_transaction()->
                 cannot_safely_rollback(Transaction_ctx::STMT));


  if (thd->get_transaction()->is_active(Transaction_ctx::STMT))
  {
    res= ha_commit_attachable(thd);
  }

  DBUG_ASSERT(thd->m_transaction_psi == NULL);

  thd->get_transaction()->reset(Transaction_ctx::STMT);

  DBUG_RETURN(MY_TEST(res));
}


/* Find a named savepoint in the current transaction. */
static SAVEPOINT **
find_savepoint(THD *thd, LEX_STRING name)
{
  SAVEPOINT **sv= &thd->get_transaction()->m_savepoints;

  while (*sv)
  {
    if (my_strnncoll(system_charset_info, (uchar *) name.str, name.length,
                     (uchar *) (*sv)->name, (*sv)->length) == 0)
      break;
    sv= &(*sv)->prev;
  }

  return sv;
}


/**
  Set a named transaction savepoint.

  @param thd    Current thread
  @param name   Savepoint name

  @retval FALSE  Success
  @retval TRUE   Failure
*/

bool trans_savepoint(THD *thd, LEX_STRING name)
{
  SAVEPOINT **sv, *newsv;
  DBUG_ENTER("trans_savepoint");

  if (!(thd->in_multi_stmt_transaction_mode() || thd->in_sub_stmt) ||
      !opt_using_transactions)
    DBUG_RETURN(FALSE);

  if (thd->get_transaction()->xid_state()->check_has_uncommitted_xa())
    DBUG_RETURN(true);

  sv= find_savepoint(thd, name);

  if (*sv) /* old savepoint of the same name exists */
  {
    newsv= *sv;
    ha_release_savepoint(thd, *sv);
    *sv= (*sv)->prev;
  }
  else if ((newsv= (SAVEPOINT *) thd->get_transaction()->allocate_memory(
      savepoint_alloc_size)) == NULL)
  {
    my_error(ER_OUT_OF_RESOURCES, MYF(0));
    DBUG_RETURN(TRUE);
  }

  newsv->name= thd->get_transaction()->strmake(name.str, name.length);
  newsv->length= name.length;

  /*
    if we'll get an error here, don't add new savepoint to the list.
    we'll lose a little bit of memory in transaction mem_root, but it'll
    be free'd when transaction ends anyway
  */
  if (ha_savepoint(thd, newsv))
    DBUG_RETURN(TRUE);

  newsv->prev= thd->get_transaction()->m_savepoints;
  thd->get_transaction()->m_savepoints= newsv;

  /*
    Remember locks acquired before the savepoint was set.
    They are used as a marker to only release locks acquired after
    the setting of this savepoint.
    Note: this works just fine if we're under LOCK TABLES,
    since mdl_savepoint() is guaranteed to be beyond
    the last locked table. This allows to release some
    locks acquired during LOCK TABLES.
  */
  newsv->mdl_savepoint= thd->mdl_context.mdl_savepoint();

  if (thd->is_current_stmt_binlog_row_enabled_with_write_set_extraction())
  {
    thd->get_transaction()->get_transaction_write_set_ctx()
        ->add_savepoint(name.str);
  }

  DBUG_RETURN(FALSE);
}


/**
  Rollback a transaction to the named savepoint.

  @note Modifications that the current transaction made to
        rows after the savepoint was set are undone in the
        rollback.

  @note Savepoints that were set at a later time than the
        named savepoint are deleted.

  @param thd    Current thread
  @param name   Savepoint name

  @retval FALSE  Success
  @retval TRUE   Failure
*/

bool trans_rollback_to_savepoint(THD *thd, LEX_STRING name)
{
  int res= FALSE;
  SAVEPOINT *sv= *find_savepoint(thd, name);
  DBUG_ENTER("trans_rollback_to_savepoint");

  if (sv == NULL)
  {
    my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "SAVEPOINT", name.str);
    DBUG_RETURN(TRUE);
  }

  if (thd->get_transaction()->xid_state()->check_has_uncommitted_xa())
    DBUG_RETURN(true);

  /**
    Checking whether it is safe to release metadata locks acquired after
    savepoint, if rollback to savepoint is successful.
  
    Whether it is safe to release MDL after rollback to savepoint depends
    on storage engines participating in transaction:
  
    - InnoDB doesn't release any row-locks on rollback to savepoint so it
      is probably a bad idea to release MDL as well.
    - Binary log implementation in some cases (e.g when non-transactional
      tables involved) may choose not to remove events added after savepoint
      from transactional cache, but instead will write them to binary
      log accompanied with ROLLBACK TO SAVEPOINT statement. Since the real
      write happens at the end of transaction releasing MDL on tables
      mentioned in these events (i.e. acquired after savepoint and before
      rollback ot it) can break replication, as concurrent DROP TABLES
      statements will be able to drop these tables before events will get
      into binary log,
  
    For backward-compatibility reasons we always release MDL if binary
    logging is off.
  */
  bool mdl_can_safely_rollback_to_savepoint=
                (!(mysql_bin_log.is_open() && thd->variables.sql_log_bin) ||
                 ha_rollback_to_savepoint_can_release_mdl(thd));

  if (ha_rollback_to_savepoint(thd, sv))
    res= TRUE;
  else if (thd->get_transaction()->cannot_safely_rollback(
           Transaction_ctx::SESSION) &&
           !thd->slave_thread)
    thd->get_transaction()->push_unsafe_rollback_warnings(thd);

  thd->get_transaction()->m_savepoints= sv;

  if (!res && mdl_can_safely_rollback_to_savepoint)
    thd->mdl_context.rollback_to_savepoint(sv->mdl_savepoint);

  if (thd->is_current_stmt_binlog_row_enabled_with_write_set_extraction())
  {
    thd->get_transaction()->get_transaction_write_set_ctx()
        ->rollback_to_savepoint(name.str);
  }

  DBUG_RETURN(MY_TEST(res));
}


/**
  Remove the named savepoint from the set of savepoints of
  the current transaction.

  @note No commit or rollback occurs. It is an error if the
        savepoint does not exist.

  @param thd    Current thread
  @param name   Savepoint name

  @retval FALSE  Success
  @retval TRUE   Failure
*/

bool trans_release_savepoint(THD *thd, LEX_STRING name)
{
  int res= FALSE;
  SAVEPOINT *sv= *find_savepoint(thd, name);
  DBUG_ENTER("trans_release_savepoint");

  if (sv == NULL)
  {
    my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "SAVEPOINT", name.str);
    DBUG_RETURN(TRUE);
  }

  if (thd->get_transaction()->xid_state()->check_has_uncommitted_xa())
    DBUG_RETURN(true);

  if (ha_release_savepoint(thd, sv))
    res= TRUE;

  thd->get_transaction()->m_savepoints= sv->prev;

  if (thd->is_current_stmt_binlog_row_enabled_with_write_set_extraction())
  {
    thd->get_transaction()->get_transaction_write_set_ctx()
        ->del_savepoint(name.str);
  }

  DBUG_RETURN(MY_TEST(res));
}
