/* Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
   02110-1301 USA */

#include "rpl_gtid.h"

#include "sql_class.h"
#include "binlog.h"
#include "transaction.h"
#include "rpl_slave.h"
#include "rpl_mi.h"
#include "sql_parse.h"


/**
  Acquire group ownership for a single group.  This is used to start a
  commit-sequence when SET GTID_NEXT is executed.

  @param thd The calling thread.

  @retval 0 Success; we have started the commit-sequence.  Either the
  GTID is logged (and will be skipped) or we have acquired ownership
  of it.

  @retval 1 Failure; the thread was killed or an error occurred.  The
  error has been reported.
*/
int gtid_acquire_ownership_single(THD *thd)
{
  DBUG_ENTER("gtid_acquire_ownership_single");
  int ret= 0;
  const Gtid gtid_next= thd->variables.gtid_next.gtid;
  while (true)
  {
    global_sid_lock->rdlock();
    // acquire lock before checking conditions
    gtid_state->lock_sidno(gtid_next.sidno);

    // GTID already logged
    if (gtid_state->is_logged(gtid_next))
    {
      /*
        Don't skip the statement here, skip it in
        gtid_pre_statement_checks.
      */
      break;
    }
    my_thread_id owner= gtid_state->get_owner(gtid_next);
    // GTID not owned by anyone: acquire ownership
    if (owner == 0)
    {
      if (gtid_state->acquire_ownership(thd, gtid_next) != RETURN_STATUS_OK)
        ret= 1;
      thd->owned_gtid= gtid_next;
      break;
    }
    // GTID owned by someone (other thread)
    else
    {
      DBUG_ASSERT(owner != thd->id);
      // The call below releases the read lock on global_sid_lock and
      // the mutex lock on SIDNO.
      gtid_state->wait_for_gtid(thd, gtid_next);

      // global_sid_lock and mutex are now released

      // Check if thread was killed.
      if (thd->killed || abort_loop)
        DBUG_RETURN(1);
#ifdef HAVE_REPLICATION
      // If this thread is a slave SQL thread or slave SQL worker
      // thread, we need this additional condition to determine if it
      // has been stopped by STOP SLAVE [SQL_THREAD].
      if ((thd->system_thread &
           (SYSTEM_THREAD_SLAVE_SQL | SYSTEM_THREAD_SLAVE_WORKER)) != 0)
      {
        // TODO: error is *not* reported on cancel
        DBUG_ASSERT(active_mi != NULL && active_mi->rli != NULL);
        if (active_mi->rli->abort_slave)
          DBUG_RETURN(1);
      }
#endif // HAVE_REPLICATION
    }
  }
  gtid_state->unlock_sidno(gtid_next.sidno);
  global_sid_lock->unlock();
  DBUG_RETURN(ret);
}


/**
  Acquire ownership of all groups in a Gtid_set.  This is used to
  begin a commit-sequence when @@SESSION.GTID_NEXT_LIST != NULL.
*/
#ifdef HAVE_GTID_NEXT_LIST
int gtid_acquire_ownership_multiple(THD *thd)
{
  const Gtid_set *gtid_next_list= thd->get_gtid_next_list_const();
  rpl_sidno greatest_sidno= 0;
  DBUG_ENTER("gtid_acquire_ownership_multiple");
  // first check if we need to wait for any group
  while (true)
  {
    Gtid_set::Gtid_iterator git(gtid_next_list);
    Gtid g= git.get();
    my_thread_id owner= 0;
    rpl_sidno last_sidno= 0;
    global_sid_lock->rdlock();
    while (g.sidno != 0)
    {
      // lock all SIDNOs in order
      if (g.sidno != last_sidno)
        gtid_state->lock_sidno(g.sidno);
      if (!gtid_state->is_logged(g))
      {
        owner= gtid_state->get_owner(g);
        // break the do-loop and wait for the sid to be updated
        if (owner != 0)
        {
          DBUG_ASSERT(owner != thd->id);
          break;
        }
      }
      last_sidno= g.sidno;
      greatest_sidno= g.sidno;
      git.next();
      g= git.get();
    }

    // we don't need to wait for any groups, and all SIDNOs in the
    // set are locked
    if (g.sidno == 0)
      break;

    // unlock all previous sidnos to avoid blocking them
    // while waiting.  keep lock on g.sidno
    for (rpl_sidno sidno= 1; sidno < g.sidno; sidno++)
      if (gtid_next_list->contains_sidno(sidno))
        gtid_state->unlock_sidno(sidno);

    // wait. this call releases the read lock on global_sid_lock and
    // the mutex lock on SIDNO
    gtid_state->wait_for_gtid(thd, g);

    // global_sid_lock and mutex are now released

    // at this point, we don't hold any locks. re-acquire the global
    // read lock that was held when this function was invoked
    if (thd->killed || abort_loop)
      DBUG_RETURN(1);
#ifdef HAVE_REPLICATION
    // If this thread is a slave SQL thread or slave SQL worker
    // thread, we need this additional condition to determine if it
    // has been stopped by STOP SLAVE [SQL_THREAD].
    if ((thd->system_thread &
         (SYSTEM_THREAD_SLAVE_SQL | SYSTEM_THREAD_SLAVE_WORKER)) != 0)
    {
      DBUG_ASSERT(active_mi != NULL && active_mi->rli != NULL);
      if (active_mi->rli->abort_slave)
        DBUG_RETURN(1);
    }
#endif // HAVE_REPLICATION
  }

  // global_sid_lock is now held
  thd->owned_gtid_set.ensure_sidno(greatest_sidno);

  /*
    Now the following hold:
     - None of the GTIDs in GTID_NEXT_LIST is owned by any thread.
     - We hold a lock on global_sid_lock.
     - We hold a lock on all SIDNOs in GTID_NEXT_LIST.
    So we acquire ownership of all groups that we need.
  */
  int ret= 0;
  Gtid_set::Gtid_iterator git(gtid_next_list);
  Gtid g= git.get();
  do
  {
    if (!gtid_state->is_logged(g))
    {
      if (gtid_state->acquire_ownership(thd, g) != RETURN_STATUS_OK ||
          thd->owned_gtid_set._add_gtid(g))
      {
        /// @todo release ownership on error
        ret= 1;
        break;
      }
    }
    git.next();
    g= git.get();
  } while (g.sidno != 0);

  // unlock all sidnos
  rpl_sidno max_sidno= gtid_next_list->get_max_sidno();
  for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    if (gtid_next_list->contains_sidno(sidno))
      gtid_state->unlock_sidno(sidno);

  global_sid_lock->unlock();

  DBUG_RETURN(ret);
}
#endif


/**
  Check if current transaction should be skipped, that is, if GTID_NEXT
  was already logged.

  @param  thd    The calling thread.

  @retval true   Transaction was already logged.
  @retval false  Transaction must be executed.
*/
static inline bool is_already_logged_transaction(const THD *thd)
{
  DBUG_ENTER("is_already_logged_transaction");

  const Gtid_specification *gtid_next= &thd->variables.gtid_next;
  const Gtid_set *gtid_next_list= thd->get_gtid_next_list_const();

  if (gtid_next_list == NULL)
  {
    if (gtid_next->type == GTID_GROUP)
    {
      if (thd->owned_gtid.sidno == 0)
        DBUG_RETURN(true);
      else
        DBUG_ASSERT(thd->owned_gtid.equals(gtid_next->gtid));
    }
    else
      DBUG_ASSERT(thd->owned_gtid.sidno == 0);
  }
  else
  {
#ifdef HAVE_GTID_NEXT_LIST
    if (gtid_next->type == GTID_GROUP)
    {
      DBUG_ASSERT(gtid_next_list->contains_gtid(gtid_next->gtid));
      if (!thd->owned_gtid_set.contains_gtid(gtid_next->gtid))
        DBUG_RETURN(true);
    }
#else
    DBUG_ASSERT(0);/*NOTREACHED*/
#endif
  }

  DBUG_RETURN(false);
}


/**
  Debug code executed when a transaction is skipped.

  @param  thd     The calling thread.

  @retval GTID_STATEMENT_SKIP  Indicate that statement should be
                               skipped by caller.
*/
static inline enum_gtid_statement_status skip_statement(const THD *thd)
{
  DBUG_ENTER("skip_statement");

  DBUG_PRINT("info", ("skipping statement '%s'. "
                      "gtid_next->type=%d sql_command=%d "
                      "thd->thread_id=%lu",
                      thd->query(),
                      thd->variables.gtid_next.type,
                      thd->lex->sql_command,
                      thd->thread_id));

#ifndef DBUG_OFF
  const Gtid_set* logged_gtids= gtid_state->get_logged_gtids();
  global_sid_lock->rdlock();
  DBUG_ASSERT(logged_gtids->contains_gtid(thd->variables.gtid_next.gtid));
  global_sid_lock->unlock();
#endif

  DBUG_RETURN(GTID_STATEMENT_SKIP);
}


enum_gtid_statement_status gtid_pre_statement_checks(const THD *thd)
{
  DBUG_ENTER("gtid_pre_statement_checks");

  if (enforce_gtid_consistency && !thd->is_ddl_gtid_compatible())
  {
    // error message has been generated by thd->is_ddl_gtid_compatible()
    DBUG_RETURN(GTID_STATEMENT_CANCEL);
  }

  const Gtid_specification *gtid_next= &thd->variables.gtid_next;
  if (stmt_causes_implicit_commit(thd, CF_IMPLICIT_COMMIT_BEGIN) &&
      thd->in_active_multi_stmt_transaction() &&
      gtid_next->type != AUTOMATIC_GROUP)
  {
    my_error(ER_CANT_DO_IMPLICIT_COMMIT_IN_TRX_WHEN_GTID_NEXT_IS_SET, MYF(0));
    DBUG_RETURN(GTID_STATEMENT_CANCEL);
  }

  /*
    never skip BEGIN/COMMIT.

    @todo: add flag to sql_command_flags to detect if statement
    controls transactions instead of listing the commands in the
    condition below

    @todo: figure out how to handle SQLCOM_XA_*
  */
  enum_sql_command sql_command= thd->lex->sql_command;
  if (sql_command == SQLCOM_COMMIT || sql_command == SQLCOM_BEGIN ||
      sql_command == SQLCOM_ROLLBACK ||
      ((sql_command == SQLCOM_SELECT ||
        (sql_command == SQLCOM_SET_OPTION && !thd->lex->is_set_password_sql)) &&
       !thd->lex->uses_stored_routines()))
    DBUG_RETURN(GTID_STATEMENT_EXECUTE);

  /*
    If a transaction updates both non-transactional and transactional
    or more then one non-transactional tables it must be stopped, this
    is the case when on master all updated tables are transactional but
    on slave at least one is non-transactional, e.g.:

    On master, tables are transactional:
      CREATE TABLE t1 (a INT) Engine=InnoDB;
      CREATE TABLE t2 (a INT) Engine=InnoDB;
    On slave, one table is non-transactional:
      CREATE TABLE t1 (a INT) Engine=MyISAM;
      CREATE TABLE t2 (a INT) Engine=InnoDB;
    On master, user executes:
      BEGIN;
      INSERT INTO t1 VALUES (1);
      INSERT INTO t2 VALUES (1);
      COMMIT;
    On slave, the second statement must error due to a second statement
    being executed after a statement that updated a non-transactional
    table.
  */
  if (UNDEFINED_GROUP == gtid_next->type)
  {
    char buf[Gtid::MAX_TEXT_LENGTH + 1];
    global_sid_lock->rdlock();
    gtid_next->to_string(global_sid_map, buf);
    global_sid_lock->unlock();
    my_error(ER_GTID_NEXT_TYPE_UNDEFINED_GROUP, MYF(0), buf);
    DBUG_RETURN(GTID_STATEMENT_CANCEL);
  }

  const Gtid_set *gtid_next_list= thd->get_gtid_next_list_const();

  DBUG_PRINT("info", ("gtid_next_list=%p gtid_next->type=%d "
                      "thd->owned_gtid.gtid.{sidno,gno}={%d,%lld} "
                      "thd->thread_id=%lu",
                      gtid_next_list, gtid_next->type,
                      thd->owned_gtid.sidno,
                      thd->owned_gtid.gno,
                      (ulong)thd->thread_id));

  const bool skip_transaction= is_already_logged_transaction(thd);
  if (gtid_next_list == NULL)
  {
    if (skip_transaction)
      DBUG_RETURN(skip_statement(thd));
    DBUG_RETURN(GTID_STATEMENT_EXECUTE);
  }
  else
  {
#ifdef HAVE_GTID_NEXT_LIST
    switch (gtid_next->type)
    {
    case AUTOMATIC_GROUP:
      my_error(ER_GTID_NEXT_CANT_BE_AUTOMATIC_IF_GTID_NEXT_LIST_IS_NON_NULL,
               MYF(0));
      DBUG_RETURN(GTID_STATEMENT_CANCEL);
    case GTID_GROUP:
      if (skip_transaction)
        DBUG_RETURN(skip_statement(thd));
      /*FALLTHROUGH*/
    case ANONYMOUS_GROUP:
      DBUG_RETURN(GTID_STATEMENT_EXECUTE);
    case INVALID_GROUP:
      DBUG_ASSERT(0);/*NOTREACHED*/
    }
#else
    DBUG_ASSERT(0);/*NOTREACHED*/
#endif
  }
  DBUG_ASSERT(0);/*NOTREACHED*/
  DBUG_RETURN(GTID_STATEMENT_CANCEL);
}


void gtid_post_statement_checks(THD *thd)
{
  DBUG_ENTER("gtid_post_statement_checks");
  const enum_sql_command sql_command= thd->lex->sql_command;

  /*
    If transaction is terminated we set GTID_NEXT type to
    UNDEFINED_GROUP, to prevent that the same GTID is used for another
    transaction (same GTID here means that user only set
    GTID_NEXT= GTID_GROUP once for two transactions).

    If the current statement:
      implict commits
      OR
      is SQLCOM_SET_OPTION AND is SET PASSWORD
      OR
      is commit
      OR
      is rollback
    that means the transaction is terminated and we set GTID_NEXT type
    to UNDEFINED_GROUP.

    SET AUTOCOMMIT=1 statement is handled on Gtid_state::update_on_flush().
  */
  if (thd->variables.gtid_next.type == GTID_GROUP &&
      thd->get_command() != COM_STMT_PREPARE &&
      (stmt_causes_implicit_commit(thd, CF_IMPLICIT_COMMIT_BEGIN) ||
       (sql_command == SQLCOM_SET_OPTION && thd->lex->is_set_password_sql) ||
       sql_command == SQLCOM_COMMIT ||
       sql_command == SQLCOM_ROLLBACK))
    thd->variables.gtid_next.set_undefined();

  DBUG_VOID_RETURN;
}


int gtid_rollback(THD *thd)
{
  DBUG_ENTER("gtid_rollback");

  global_sid_lock->rdlock();
  gtid_state->update_on_rollback(thd);
  global_sid_lock->unlock();

  DBUG_RETURN(0);
}
