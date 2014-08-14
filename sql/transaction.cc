/* Copyright (c) 2000, 2014, Oracle and/or its affiliates. All rights reserved.

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


#include "sql_priv.h"
#include "transaction.h"
#include "rpl_handler.h"
#include "debug_sync.h"         // DEBUG_SYNC
#include "auth_common.h"            // SUPER_ACL
#include <pfs_transaction_provider.h>
#include <mysql/psi/mysql_transaction.h>

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
  DBUG_ENTER("trans_begin");

  if (trans_check_state(thd))
    DBUG_RETURN(TRUE);

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
    thd->tx_read_only= true;
  else if (flags & MYSQL_START_TRANS_OPT_READ_WRITE)
  {
    /*
      Explicitly starting a RW transaction when the server is in
      read-only mode, is not allowed unless the user has SUPER priv.
      Implicitly starting a RW transaction is allowed for backward
      compatibility.
    */
    const bool user_is_super=
      MY_TEST(thd->security_ctx->master_access & SUPER_ACL);
    if (opt_readonly && !user_is_super)
    {
      my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--read-only");
      DBUG_RETURN(true);
    }
    thd->tx_read_only= false;
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

  /* ha_start_consistent_snapshot() relies on OPTION_BEGIN flag set. */
  if (flags & MYSQL_START_TRANS_OPT_WITH_CONS_SNAPSHOT)
    res= ha_start_consistent_snapshot(thd);
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
    thd->m_transaction_psi= MYSQL_START_TRANSACTION(&thd->m_transaction_state,
                                                 NULL, NULL, thd->tx_isolation,
                                                 thd->tx_read_only, false);
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
  /*
    When gtid mode is enabled, a transaction may cause binlog
    rotation, which inserts a record into the gtid system table
    (which is probably a transactional table). Thence, the flag
    SERVER_STATUS_IN_TRANS may be set again while calling
    ha_commit_trans(...) Consequently, we need to reset it back,
    much like we are doing before calling ha_commit_trans(...).
  */
  if (gtid_mode > GTID_MODE_UPGRADE_STEP_1)
    thd->server_status&= ~SERVER_STATUS_IN_TRANS;
  thd->variables.option_bits&= ~OPTION_BEGIN;
  thd->get_transaction()->reset_unsafe_rollback_flags(Transaction_ctx::SESSION);
  thd->lex->start_transaction_opt= 0;

  /* The transaction should be marked as complete in P_S. */
  DBUG_ASSERT(thd->m_transaction_psi == NULL);

  thd->tx_priority= 0;

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
  thd->tx_isolation= (enum_tx_isolation) thd->variables.tx_isolation;
  thd->tx_read_only= thd->variables.tx_read_only;

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

  thd->get_transaction()->merge_unsafe_rollback_flags();

  if (thd->get_transaction()->is_active(Transaction_ctx::STMT))
  {
    res= ha_commit_trans(thd, FALSE);
    if (! thd->in_active_multi_stmt_transaction())
    {
      thd->tx_isolation= (enum_tx_isolation) thd->variables.tx_isolation;
      thd->tx_read_only= thd->variables.tx_read_only;
    }
  }
  else if (tc_log)
    tc_log->commit(thd, false);

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

  thd->get_transaction()->merge_unsafe_rollback_flags();

  if (thd->get_transaction()->is_active(Transaction_ctx::STMT))
  {
    ha_rollback_trans(thd, FALSE);
    if (! thd->in_active_multi_stmt_transaction())
    {
      thd->tx_isolation= (enum_tx_isolation) thd->variables.tx_isolation;
      thd->tx_read_only= thd->variables.tx_read_only;
    }
  }
  else if (tc_log)
    tc_log->rollback(thd, false);

  /* In autocommit=1 mode the transaction should be marked as complete in P_S */
  DBUG_ASSERT(thd->in_active_multi_stmt_transaction() ||
              thd->m_transaction_psi == NULL);

  thd->get_transaction()->reset(Transaction_ctx::STMT);

  DBUG_RETURN(FALSE);
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

  DBUG_RETURN(MY_TEST(res));
}
