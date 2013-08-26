/* Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

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
#include "sql_acl.h"            // SUPER_ACL

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
  enum xa_states xa_state= thd->transaction.xid_state.xa_state;
  DBUG_ENTER("trans_check");

  /*
    Always commit statement transaction before manipulating with
    the normal one.
  */
  DBUG_ASSERT(thd->transaction.stmt.is_empty());

  if (unlikely(thd->in_sub_stmt))
    my_error(ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG, MYF(0));
  if (xa_state != XA_NOTR)
    my_error(ER_XAER_RMFAIL, MYF(0), xa_state_names[xa_state]);
  else
    DBUG_RETURN(FALSE);

  DBUG_RETURN(TRUE);
}


/**
  Mark a XA transaction as rollback-only if the RM unilaterally
  rolled back the transaction branch.

  @note If a rollback was requested by the RM, this function sets
        the appropriate rollback error code and transits the state
        to XA_ROLLBACK_ONLY.

  @return TRUE if transaction was rolled back or if the transaction
          state is XA_ROLLBACK_ONLY. FALSE otherwise.
*/
static bool xa_trans_rolled_back(XID_STATE *xid_state)
{
  if (xid_state->rm_error)
  {
    switch (xid_state->rm_error) {
    case ER_LOCK_WAIT_TIMEOUT:
      my_error(ER_XA_RBTIMEOUT, MYF(0));
      break;
    case ER_LOCK_DEADLOCK:
      my_error(ER_XA_RBDEADLOCK, MYF(0));
      break;
    default:
      my_error(ER_XA_RBROLLBACK, MYF(0));
    }
    xid_state->xa_state= XA_ROLLBACK_ONLY;
  }

  return (xid_state->xa_state == XA_ROLLBACK_ONLY);
}


/**
  Rollback the active XA transaction.

  @note Resets rm_error before calling ha_rollback(), so
        the thd->transaction.xid structure gets reset
        by ha_rollback() / THD::transaction::cleanup().

  @return TRUE if the rollback failed, FALSE otherwise.
*/

static bool xa_trans_force_rollback(THD *thd)
{
  /*
    We must reset rm_error before calling ha_rollback(),
    so thd->transaction.xid structure gets reset
    by ha_rollback()/THD::transaction::cleanup().
  */
  thd->transaction.xid_state.rm_error= 0;
  if (ha_rollback_trans(thd, true))
  {
    my_error(ER_XAER_RMERR, MYF(0));
    return true;
  }
  return false;
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
    res= test(ha_commit_trans(thd, TRUE));
  }

  thd->variables.option_bits&= ~OPTION_BEGIN;
  thd->transaction.all.reset_unsafe_rollback_flags();

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
      test(thd->security_ctx->master_access & SUPER_ACL);
    if (opt_readonly && !user_is_super)
    {
      my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--read-only");
      DBUG_RETURN(true);
    }
    thd->tx_read_only= false;
  }

  thd->variables.option_bits|= OPTION_BEGIN;
  thd->server_status|= SERVER_STATUS_IN_TRANS;
  if (thd->tx_read_only)
    thd->server_status|= SERVER_STATUS_IN_TRANS_READONLY;
  DBUG_PRINT("info", ("setting SERVER_STATUS_IN_TRANS"));

  /* ha_start_consistent_snapshot() relies on OPTION_BEGIN flag set. */
  if (flags & MYSQL_START_TRANS_OPT_WITH_CONS_SNAPSHOT)
    res= ha_start_consistent_snapshot(thd);

  DBUG_RETURN(test(res));
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

#ifndef DBUG_OFF
  char buf1[256], buf2[256];
  DBUG_PRINT("enter", ("stmt.ha_list: %s, all.ha_list: %s",
                       ha_list_names(thd->transaction.stmt.ha_list, buf1),
                       ha_list_names(thd->transaction.all.ha_list, buf2)));

  thd->transaction.stmt.dbug_unsafe_rollback_flags("stmt");
  thd->transaction.all.dbug_unsafe_rollback_flags("all");
#endif

  if (trans_check_state(thd))
    DBUG_RETURN(TRUE);

  thd->server_status&=
    ~(SERVER_STATUS_IN_TRANS | SERVER_STATUS_IN_TRANS_READONLY);
  DBUG_PRINT("info", ("clearing SERVER_STATUS_IN_TRANS"));
  res= ha_commit_trans(thd, TRUE);
  thd->variables.option_bits&= ~OPTION_BEGIN;
  thd->transaction.all.reset_unsafe_rollback_flags();
  thd->lex->start_transaction_opt= 0;

  DBUG_RETURN(test(res));
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

#ifndef DBUG_OFF
  char buf1[256], buf2[256];
  DBUG_PRINT("enter", ("stmt.ha_list: %s, all.ha_list: %s",
                       ha_list_names(thd->transaction.stmt.ha_list, buf1),
                       ha_list_names(thd->transaction.all.ha_list, buf2)));

  thd->transaction.stmt.dbug_unsafe_rollback_flags("stmt");
  thd->transaction.all.dbug_unsafe_rollback_flags("all");
#endif

  /*
    Ensure that trans_check_state() was called before trans_commit_implicit()
    by asserting that conditions that are checked in the former function are
    true.
  */
  DBUG_ASSERT(thd->transaction.stmt.is_empty() &&
              !thd->in_sub_stmt &&
              thd->transaction.xid_state.xa_state == XA_NOTR);

  if (thd->in_multi_stmt_transaction_mode() ||
      (thd->variables.option_bits & OPTION_TABLE_LOCK))
  {
    /* Safety if one did "drop table" on locked tables */
    if (!thd->locked_tables_mode)
      thd->variables.option_bits&= ~OPTION_TABLE_LOCK;
    thd->server_status&=
      ~(SERVER_STATUS_IN_TRANS | SERVER_STATUS_IN_TRANS_READONLY);
    DBUG_PRINT("info", ("clearing SERVER_STATUS_IN_TRANS"));
    res= test(ha_commit_trans(thd, TRUE));
  }
  else if (tc_log)
    tc_log->commit(thd, true);

  thd->variables.option_bits&= ~OPTION_BEGIN;
  thd->transaction.all.reset_unsafe_rollback_flags();

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

#ifndef DBUG_OFF
  char buf1[256], buf2[256];
  DBUG_PRINT("enter", ("stmt.ha_list: %s, all.ha_list: %s",
                       ha_list_names(thd->transaction.stmt.ha_list, buf1),
                       ha_list_names(thd->transaction.all.ha_list, buf2)));

  thd->transaction.stmt.dbug_unsafe_rollback_flags("stmt");
  thd->transaction.all.dbug_unsafe_rollback_flags("all");
#endif

  if (trans_check_state(thd))
    DBUG_RETURN(TRUE);

  thd->server_status&=
    ~(SERVER_STATUS_IN_TRANS | SERVER_STATUS_IN_TRANS_READONLY);
  DBUG_PRINT("info", ("clearing SERVER_STATUS_IN_TRANS"));
  res= ha_rollback_trans(thd, TRUE);
  thd->variables.option_bits&= ~OPTION_BEGIN;
  thd->transaction.all.reset_unsafe_rollback_flags();
  thd->lex->start_transaction_opt= 0;

  DBUG_RETURN(test(res));
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
  DBUG_ASSERT(thd->transaction.stmt.is_empty() && !thd->in_sub_stmt);

  thd->server_status&=
    ~(SERVER_STATUS_IN_TRANS | SERVER_STATUS_IN_TRANS_READONLY);
  DBUG_PRINT("info", ("clearing SERVER_STATUS_IN_TRANS"));
  res= ha_rollback_trans(thd, true);
  /*
    We don't reset OPTION_BEGIN flag below to simulate implicit start
    of new transacton in @@autocommit=1 mode. This is necessary to
    preserve backward compatibility.
  */
  thd->transaction.all.reset_unsafe_rollback_flags();

  /* Rollback should clear transaction_rollback_request flag. */
  DBUG_ASSERT(! thd->transaction_rollback_request);

  DBUG_RETURN(test(res));
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
#ifndef DBUG_OFF
  char buf1[256], buf2[256];
  DBUG_PRINT("enter", ("stmt.ha_list: %s, all.ha_list: %s",
                       ha_list_names(thd->transaction.stmt.ha_list, buf1),
                       ha_list_names(thd->transaction.all.ha_list, buf2)));
#endif

  int res= FALSE;
  /*
    We currently don't invoke commit/rollback at end of
    a sub-statement.  In future, we perhaps should take
    a savepoint for each nested statement, and release the
    savepoint when statement has succeeded.
  */
  DBUG_ASSERT(! thd->in_sub_stmt);

#ifndef DBUG_OFF
  DBUG_PRINT("enter", ("stmt.ha_list: %s, all.ha_list: %s",
                       ha_list_names(thd->transaction.stmt.ha_list, buf1),
                       ha_list_names(thd->transaction.all.ha_list, buf2)));

  thd->transaction.stmt.dbug_unsafe_rollback_flags("stmt");
  thd->transaction.all.dbug_unsafe_rollback_flags("all");
#endif

  thd->transaction.merge_unsafe_rollback_flags();

  if (thd->transaction.stmt.ha_list)
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

  thd->transaction.stmt.reset();

  DBUG_RETURN(test(res));
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

#ifndef DBUG_OFF
  char buf1[256], buf2[256];
  DBUG_PRINT("enter", ("stmt.ha_list: %s, all.ha_list: %s",
                       ha_list_names(thd->transaction.stmt.ha_list, buf1),
                       ha_list_names(thd->transaction.all.ha_list, buf2)));

  thd->transaction.stmt.dbug_unsafe_rollback_flags("stmt");
  thd->transaction.all.dbug_unsafe_rollback_flags("all");
#endif

  thd->transaction.merge_unsafe_rollback_flags();

  if (thd->transaction.stmt.ha_list)
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

  thd->transaction.stmt.reset();

  DBUG_RETURN(FALSE);
}

/* Find a named savepoint in the current transaction. */
static SAVEPOINT **
find_savepoint(THD *thd, LEX_STRING name)
{
  SAVEPOINT **sv= &thd->transaction.savepoints;

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

  enum xa_states xa_state= thd->transaction.xid_state.xa_state;
  if (xa_state != XA_NOTR && xa_state != XA_ACTIVE)
  {
    my_error(ER_XAER_RMFAIL, MYF(0), xa_state_names[xa_state]);
    DBUG_RETURN(TRUE);
  }

  sv= find_savepoint(thd, name);

  if (*sv) /* old savepoint of the same name exists */
  {
    newsv= *sv;
    ha_release_savepoint(thd, *sv);
    *sv= (*sv)->prev;
  }
  else if ((newsv= (SAVEPOINT *) alloc_root(&thd->transaction.mem_root,
                                            savepoint_alloc_size)) == NULL)
  {
    my_error(ER_OUT_OF_RESOURCES, MYF(0));
    DBUG_RETURN(TRUE);
  }

  newsv->name= strmake_root(&thd->transaction.mem_root, name.str, name.length);
  newsv->length= name.length;

  /*
    if we'll get an error here, don't add new savepoint to the list.
    we'll lose a little bit of memory in transaction mem_root, but it'll
    be free'd when transaction ends anyway
  */
  if (ha_savepoint(thd, newsv))
    DBUG_RETURN(TRUE);

  newsv->prev= thd->transaction.savepoints;
  thd->transaction.savepoints= newsv;

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

#ifndef DBUG_OFF
  char buf1[256], buf2[256];
  DBUG_PRINT("enter", ("stmt.ha_list: %s, all.ha_list: %s",
                       ha_list_names(thd->transaction.stmt.ha_list, buf1),
                       ha_list_names(thd->transaction.all.ha_list, buf2)));

  thd->transaction.stmt.dbug_unsafe_rollback_flags("stmt");
  thd->transaction.all.dbug_unsafe_rollback_flags("all");
#endif

  if (sv == NULL)
  {
    my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "SAVEPOINT", name.str);
    DBUG_RETURN(TRUE);
  }

  enum xa_states xa_state= thd->transaction.xid_state.xa_state;
  if (xa_state != XA_NOTR && xa_state != XA_ACTIVE)
  {
    my_error(ER_XAER_RMFAIL, MYF(0), xa_state_names[xa_state]);
    DBUG_RETURN(TRUE);
  }

  if (ha_rollback_to_savepoint(thd, sv))
    res= TRUE;
  else if (thd->transaction.all.cannot_safely_rollback() && !thd->slave_thread)
    thd->transaction.push_unsafe_rollback_warnings(thd);

  thd->transaction.savepoints= sv;

  /*
    Release metadata locks that were acquired during this savepoint unit
    unless binlogging is on. Releasing locks with binlogging on can break
    replication as it allows other connections to drop these tables before
    rollback to savepoint is written to the binlog.
  */
  bool binlog_on= mysql_bin_log.is_open() && thd->variables.sql_log_bin;
  if (!res && !binlog_on)
    thd->mdl_context.rollback_to_savepoint(sv->mdl_savepoint);

  DBUG_RETURN(test(res));
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

  enum xa_states xa_state= thd->transaction.xid_state.xa_state;
  if (xa_state != XA_NOTR && xa_state != XA_ACTIVE)
  {
    my_error(ER_XAER_RMFAIL, MYF(0), xa_state_names[xa_state]);
    DBUG_RETURN(TRUE);
  }

  if (ha_release_savepoint(thd, sv))
    res= TRUE;

  thd->transaction.savepoints= sv->prev;

  DBUG_RETURN(test(res));
}


/**
  Starts an XA transaction with the given xid value.

  @param thd    Current thread

  @retval FALSE  Success
  @retval TRUE   Failure
*/

bool trans_xa_start(THD *thd)
{
  enum xa_states xa_state= thd->transaction.xid_state.xa_state;
  DBUG_ENTER("trans_xa_start");

  if (xa_state == XA_IDLE && thd->lex->xa_opt == XA_RESUME)
  {
    bool not_equal= !thd->transaction.xid_state.xid.eq(thd->lex->xid);
    if (not_equal)
      my_error(ER_XAER_NOTA, MYF(0));
    else
      thd->transaction.xid_state.xa_state= XA_ACTIVE;
    DBUG_RETURN(not_equal);
  }

  /* TODO: JOIN is not supported yet. */
  if (thd->lex->xa_opt != XA_NONE)
    my_error(ER_XAER_INVAL, MYF(0));
  else if (xa_state != XA_NOTR)
    my_error(ER_XAER_RMFAIL, MYF(0), xa_state_names[xa_state]);
  else if (thd->locked_tables_mode || thd->in_active_multi_stmt_transaction())
    my_error(ER_XAER_OUTSIDE, MYF(0));
  else if (!trans_begin(thd))
  {
    DBUG_ASSERT(thd->transaction.xid_state.xid.is_null());
    thd->transaction.xid_state.xa_state= XA_ACTIVE;
    thd->transaction.xid_state.rm_error= 0;
    thd->transaction.xid_state.xid.set(thd->lex->xid);
    if (xid_cache_insert(&thd->transaction.xid_state))
    {
      thd->transaction.xid_state.xa_state= XA_NOTR;
      thd->transaction.xid_state.xid.null();
      trans_rollback(thd);
      DBUG_RETURN(true);
    }
    DBUG_RETURN(FALSE);
  }

  DBUG_RETURN(TRUE);
}


/**
  Put a XA transaction in the IDLE state.

  @param thd    Current thread

  @retval FALSE  Success
  @retval TRUE   Failure
*/

bool trans_xa_end(THD *thd)
{
  DBUG_ENTER("trans_xa_end");

  /* TODO: SUSPEND and FOR MIGRATE are not supported yet. */
  if (thd->lex->xa_opt != XA_NONE)
    my_error(ER_XAER_INVAL, MYF(0));
  else if (thd->transaction.xid_state.xa_state != XA_ACTIVE)
    my_error(ER_XAER_RMFAIL, MYF(0),
             xa_state_names[thd->transaction.xid_state.xa_state]);
  else if (!thd->transaction.xid_state.xid.eq(thd->lex->xid))
    my_error(ER_XAER_NOTA, MYF(0));
  else if (!xa_trans_rolled_back(&thd->transaction.xid_state))
    thd->transaction.xid_state.xa_state= XA_IDLE;

  DBUG_RETURN(thd->is_error() ||
              thd->transaction.xid_state.xa_state != XA_IDLE);
}


/**
  Put a XA transaction in the PREPARED state.

  @param thd    Current thread

  @retval FALSE  Success
  @retval TRUE   Failure
*/

bool trans_xa_prepare(THD *thd)
{
  DBUG_ENTER("trans_xa_prepare");

  if (thd->transaction.xid_state.xa_state != XA_IDLE)
    my_error(ER_XAER_RMFAIL, MYF(0),
             xa_state_names[thd->transaction.xid_state.xa_state]);
  else if (!thd->transaction.xid_state.xid.eq(thd->lex->xid))
    my_error(ER_XAER_NOTA, MYF(0));
  else if (ha_prepare(thd))
  {
    xid_cache_delete(&thd->transaction.xid_state);
    thd->transaction.xid_state.xa_state= XA_NOTR;
    my_error(ER_XA_RBROLLBACK, MYF(0));
  }
  else
    thd->transaction.xid_state.xa_state= XA_PREPARED;

  DBUG_RETURN(thd->is_error() ||
              thd->transaction.xid_state.xa_state != XA_PREPARED);
}


/**
  Commit and terminate the a XA transaction.

  @param thd    Current thread

  @retval FALSE  Success
  @retval TRUE   Failure
*/

bool trans_xa_commit(THD *thd)
{
  bool res= TRUE;
  enum xa_states xa_state= thd->transaction.xid_state.xa_state;
  DBUG_ENTER("trans_xa_commit");

  if (!thd->transaction.xid_state.xid.eq(thd->lex->xid))
  {
    /*
      xid_state.in_thd is always true beside of xa recovery procedure.
      Note, that there is no race condition here between xid_cache_search
      and xid_cache_delete, since we always delete our own XID
      (thd->lex->xid == thd->transaction.xid_state.xid).
      The only case when thd->lex->xid != thd->transaction.xid_state.xid
      and xid_state->in_thd == 0 is in the function
      xa_cache_insert(XID, xa_states), which is called before starting
      client connections, and thus is always single-threaded.
    */
    XID_STATE *xs= xid_cache_search(thd->lex->xid);
    res= !xs || xs->in_thd;
    if (res)
      my_error(ER_XAER_NOTA, MYF(0));
    else
    {
      res= xa_trans_rolled_back(xs);
      ha_commit_or_rollback_by_xid(thd, thd->lex->xid, !res);
      xid_cache_delete(xs);
    }
    DBUG_RETURN(res);
  }

  if (xa_trans_rolled_back(&thd->transaction.xid_state))
  {
    xa_trans_force_rollback(thd);
    res= thd->is_error();
  }
  else if (xa_state == XA_IDLE && thd->lex->xa_opt == XA_ONE_PHASE)
  {
    int r= ha_commit_trans(thd, TRUE);
    if ((res= test(r)))
      my_error(r == 1 ? ER_XA_RBROLLBACK : ER_XAER_RMERR, MYF(0));
  }
  else if (xa_state == XA_PREPARED && thd->lex->xa_opt == XA_NONE)
  {
    MDL_request mdl_request;

    /*
      Acquire metadata lock which will ensure that COMMIT is blocked
      by active FLUSH TABLES WITH READ LOCK (and vice versa COMMIT in
      progress blocks FTWRL).

      We allow FLUSHer to COMMIT; we assume FLUSHer knows what it does.
    */
    mdl_request.init(MDL_key::COMMIT, "", "", MDL_INTENTION_EXCLUSIVE,
                     MDL_TRANSACTION);

    if (thd->mdl_context.acquire_lock(&mdl_request,
                                      thd->variables.lock_wait_timeout))
    {
      ha_rollback_trans(thd, TRUE);
      my_error(ER_XAER_RMERR, MYF(0));
    }
    else
    {
      DEBUG_SYNC(thd, "trans_xa_commit_after_acquire_commit_lock");

      if (tc_log)
        res= test(tc_log->commit(thd, /* all */ true));
      else
        res= test(ha_commit_low(thd, /* all */ true));

      if (res)
        my_error(ER_XAER_RMERR, MYF(0));
    }
  }
  else
  {
    my_error(ER_XAER_RMFAIL, MYF(0), xa_state_names[xa_state]);
    DBUG_RETURN(TRUE);
  }

  thd->variables.option_bits&= ~OPTION_BEGIN;
  thd->transaction.all.reset_unsafe_rollback_flags();
  thd->server_status&=
    ~(SERVER_STATUS_IN_TRANS | SERVER_STATUS_IN_TRANS_READONLY);
  DBUG_PRINT("info", ("clearing SERVER_STATUS_IN_TRANS"));
  xid_cache_delete(&thd->transaction.xid_state);
  thd->transaction.xid_state.xa_state= XA_NOTR;

  DBUG_RETURN(res);
}


/**
  Roll back and terminate a XA transaction.

  @param thd    Current thread

  @retval FALSE  Success
  @retval TRUE   Failure
*/

bool trans_xa_rollback(THD *thd)
{
  bool res= TRUE;
  enum xa_states xa_state= thd->transaction.xid_state.xa_state;
  DBUG_ENTER("trans_xa_rollback");

  if (!thd->transaction.xid_state.xid.eq(thd->lex->xid))
  {
    XID_STATE *xs= xid_cache_search(thd->lex->xid);
    if (!xs || xs->in_thd)
      my_error(ER_XAER_NOTA, MYF(0));
    else
    {
      xa_trans_rolled_back(xs);
      ha_commit_or_rollback_by_xid(thd, thd->lex->xid, 0);
      xid_cache_delete(xs);
    }
    DBUG_RETURN(thd->get_stmt_da()->is_error());
  }

  if (xa_state != XA_IDLE && xa_state != XA_PREPARED && xa_state != XA_ROLLBACK_ONLY)
  {
    my_error(ER_XAER_RMFAIL, MYF(0), xa_state_names[xa_state]);
    DBUG_RETURN(TRUE);
  }

  res= xa_trans_force_rollback(thd);

  thd->variables.option_bits&= ~OPTION_BEGIN;
  thd->transaction.all.reset_unsafe_rollback_flags();
  thd->server_status&=
    ~(SERVER_STATUS_IN_TRANS | SERVER_STATUS_IN_TRANS_READONLY);
  DBUG_PRINT("info", ("clearing SERVER_STATUS_IN_TRANS"));
  xid_cache_delete(&thd->transaction.xid_state);
  thd->transaction.xid_state.xa_state= XA_NOTR;

  DBUG_RETURN(res);
}
