/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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


#include "rpl_gtid.h"
#include "sql_class.h"
#include "binlog.h"
#include "transaction.h"
#include "rpl_slave.h"
#include "rpl_mi.h"
#include "sql_parse.h"


/**
  Acquire group ownership for a single group.  This is used to start a
  commit-sequence when @@SESSION.GTID_NEXT_LIST = NULL and
  @@SESSION.GTID_NEXT = SID:GNO.

  @param thd The calling thread.
  @param gtid Gtid in @@SESSION.GTID_NEXT

  @retval GTID_STATEMENT_EXECUTE Success; we have started the
  commit-sequence.  Either the GTID is logged (and will be skipped) or
  we have acquired ownership of it.

  @retval GTID_STATEMENT_CANCEL Failure; the thread was killed or an
  error occurred.  The error has been reported.
*/
static enum_gtid_statement_status
gtid_acquire_ownership(THD *thd, const Gtid gtid)
{
  DBUG_ENTER("gtid_acquire_ownership");
  enum_gtid_statement_status ret= GTID_STATEMENT_EXECUTE;
  global_sid_lock.assert_some_rdlock();
  while (true)
  {
    // acquire lock before checking conditions
    gtid_state.lock_sidno(gtid.sidno);

    // GTID already logged
    if (gtid_state.is_logged(gtid))
    {
      /*
        Don't skip the statement here, skip it in
        gtid_before_statement_begin_group.
      */
      DBUG_ASSERT(gtid_state.get_owner(gtid) == 0);
      break;
    }
    my_thread_id owner= gtid_state.get_owner(gtid);
    // GTID not owned by anyone: acquire ownership
    if (owner == 0)
    {
      thd->owned_gtid_set.ensure_sidno(gtid.sidno);
      if (gtid_state.acquire_ownership(gtid, thd) != RETURN_STATUS_OK)
        ret= GTID_STATEMENT_CANCEL;
      thd->owned_gtid= gtid;
      break;
    }
    // GTID owned by someone (other thread)
    else
    {
      DBUG_ASSERT(owner != thd->id);
      // The call below releases the read lock on global_sid_lock and
      // the mutex lock on SIDNO.
      gtid_state.wait_for_gtid(thd, gtid);
      // Re-acquire lock before possibly returning from this function.
      global_sid_lock.rdlock();

      // Check if thread was killed.
      if (thd->killed || abort_loop)
        DBUG_RETURN(GTID_STATEMENT_CANCEL);
#ifdef HAVE_REPLICATION
      // If this thread is a slave SQL thread or slave SQL worker
      // thread, we need this additional condition to determine if it
      // has been stopped by STOP SLAVE [SQL_THREAD].
      if ((thd->system_thread &
           (SYSTEM_THREAD_SLAVE_SQL | SYSTEM_THREAD_SLAVE_WORKER)) != 0)
      {
        DBUG_ASSERT(active_mi != NULL && active_mi->rli != NULL);
        if (active_mi->rli->abort_slave)
          DBUG_RETURN(GTID_STATEMENT_CANCEL);
      }
#endif // HAVE_REPLICATION
    }
  }
  gtid_state.unlock_sidno(gtid.sidno);
  DBUG_RETURN(ret);
}


/**
  Acquire ownership of all groups in a Gtid_set.  This is used to
  begin a commit-sequence when @@SESSION.GTID_NEXT_LIST != NULL.
*/
static enum_gtid_statement_status gtid_acquire_ownerships(THD *thd,
                                                          const Gtid_set *gs)
{
  rpl_sidno greatest_sidno= 0;
  DBUG_ENTER("gtid_acquire_ownerships");
  global_sid_lock.assert_some_rdlock();
  // first check if we need to wait for any group
  while (true)
  {
    Gtid_set::Gtid_iterator git(gs);
    Gtid g= git.get();
    my_thread_id owner= 0;
    rpl_sidno last_sidno= 0;
    while (g.sidno != 0)
    {
      // lock all SIDNOs in order
      if (g.sidno != last_sidno)
        gtid_state.lock_sidno(g.sidno);
      if (!gtid_state.is_logged(g))
      {
        owner= gtid_state.get_owner(g);
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
      if (gs->contains_sidno(sidno))
        gtid_state.unlock_sidno(sidno);

    // wait. this call releases the read lock on global_sid_lock and
    // the mutex lock on SIDNO
    gtid_state.wait_for_gtid(thd, g);

    // Re-acquire lock before possibly returning from this function.
    global_sid_lock.rdlock();

    // at this point, we don't hold any locks. re-acquire the global
    // read lock that was held when this function was invoked
    if (thd->killed || abort_loop)
      DBUG_RETURN(GTID_STATEMENT_CANCEL);
#ifdef HAVE_REPLICATION
    // If this thread is a slave SQL thread or slave SQL worker
    // thread, we need this additional condition to determine if it
    // has been stopped by STOP SLAVE [SQL_THREAD].
    if ((thd->system_thread &
         (SYSTEM_THREAD_SLAVE_SQL | SYSTEM_THREAD_SLAVE_WORKER)) != 0)
    {
      DBUG_ASSERT(active_mi != NULL && active_mi->rli != NULL);
      if (active_mi->rli->abort_slave)
        DBUG_RETURN(GTID_STATEMENT_CANCEL);
    }
#endif // HAVE_REPLICATION
  }

  thd->owned_gtid_set.ensure_sidno(greatest_sidno);

  /*
    Now the following hold:
     - None of the GTIDs in GTID_NEXT_LIST is owned by any thread.
     - We hold a lock on global_sid_lock.
     - We hold a lock on all SIDNOs in GTID_NEXT_LIST.
    So we acquire ownership of all groups that we need.
  */
  enum_gtid_statement_status ret= GTID_STATEMENT_EXECUTE;
  Gtid_set::Gtid_iterator git(gs);
  Gtid g= git.get();
  do
  {
    if (!gtid_state.is_logged(g))
    {
      DBUG_ASSERT(gtid_state.get_owner(g) == 0);
      if (gtid_state.acquire_ownership(g, thd) != RETURN_STATUS_OK)
      {
        ret= GTID_STATEMENT_CANCEL;
        break;
      }
    }
    git.next();
    g= git.get();
  } while (g.sidno != 0);

  // unlock all sidnos
  rpl_sidno max_sidno= gs->get_max_sidno();
  for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    if (gs->contains_sidno(sidno))
      gtid_state.unlock_sidno(sidno);

  DBUG_RETURN(ret);
}


int gtid_check_session_variables_before_statement(const THD *thd)
{
  DBUG_ENTER("gtid_check_session_variables_before_statement");

  const Gtid_set *gtid_next_list= thd->get_gtid_next_list_const();
  const Gtid_specification *gtid_next= &thd->variables.gtid_next;

  if (gtid_next_list != NULL)
  {
    // If GTID_NEXT==SID:GNO, then SID:GNO must be listed in GTID_NEXT_LIST
    if (gtid_next->type == GTID_GROUP &&
        !gtid_next_list->contains_gtid(gtid_next->gtid.sidno,
                                       gtid_next->gtid.gno))
    {
      char buf[Gtid_specification::MAX_TEXT_LENGTH + 1];
      global_sid_lock.rdlock();
      gtid_next->to_string(&global_sid_map, buf);
      global_sid_lock.unlock();
      my_error(ER_GTID_NEXT_IS_NOT_IN_GTID_NEXT_LIST, MYF(0), buf);
      DBUG_RETURN(1);
    }

    // GTID_NEXT cannot be "AUTOMATIC" when GTID_NEXT_LIST != NULL.
    if (gtid_next->type == AUTOMATIC_GROUP)
    {
      my_error(ER_GTID_NEXT_CANT_BE_AUTOMATIC_IF_GTID_NEXT_LIST_IS_NON_NULL,
               MYF(0));
      DBUG_RETURN(1);
    }
  }

  if (stmt_causes_implicit_commit(thd, CF_IMPLICIT_COMMIT_BEGIN) &&
      thd->in_active_multi_stmt_transaction() &&
      gtid_next->type != AUTOMATIC_GROUP)
  {
    my_error(ER_CANT_DO_IMPLICIT_COMMIT_IN_TRX_WHEN_GTID_NEXT_IS_SET, MYF(0));
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}


/**
  Begin commit-sequence, i.e., acquire ownership of all groups to be
  re-executed.
*/
static enum_gtid_statement_status
gtid_before_statement_begin_commit_sequence(
  THD *thd, const Gtid_set *gtid_next_list, const Gtid_specification *gtid_next)
{
  DBUG_ENTER("gtid_before_statement_begin_commit_sequence");

  global_sid_lock.assert_some_rdlock();

  DBUG_PRINT("info", ("gtid_next: type=%d sidno=%d gno=%lld",
                      gtid_next->type,
                      gtid_next->gtid.sidno, gtid_next->gtid.gno));

  if (!thd->in_active_multi_stmt_transaction())
  {
    //DBUG_ASSERT(!(variables.option_bits & OPTION_BEGIN));
    DBUG_ASSERT(!gtid_state.get_owned_gtids()->
                thread_owns_anything(thd->thread_id));

    if (gtid_next_list != NULL)
    {
      // acquire group ownership for Gtid_set.
      if (!gtid_next_list->is_empty())
      {
        if (gtid_acquire_ownerships(thd, gtid_next_list) !=
            GTID_STATEMENT_EXECUTE)
          DBUG_RETURN(GTID_STATEMENT_CANCEL);
        /// @todo: is this ok? might be controversial to register a handler before the statement executes /sven
        if (!thd->owned_gtid_set.is_empty())
          register_binlog_handler(thd, thd->lex->sql_command == SQLCOM_BEGIN ||
                                  thd->lex->sql_command == SQLCOM_COMMIT ||
                                  (thd->variables.option_bits &
                                   OPTION_NOT_AUTOCOMMIT) != 0);
      }
    }
    else if (gtid_next->type == GTID_GROUP)
    {
      // acquire group ownership for single group.
      enum_gtid_statement_status ret=
        gtid_acquire_ownership(thd, gtid_next->gtid);
      /// @todo: is this ok? might be controversial to register a handler before the statement executes /sven
      if (thd->owned_gtid.sidno != 0)
        register_binlog_handler(thd, thd->lex->sql_command == SQLCOM_BEGIN ||
                                thd->lex->sql_command == SQLCOM_COMMIT ||
                                (thd->variables.option_bits &
                                 OPTION_NOT_AUTOCOMMIT));
      DBUG_RETURN(ret);
    }
  }
  DBUG_RETURN(GTID_STATEMENT_EXECUTE);
}


/**
  Begin a group, i.e., check if the statement should be skipped or
  not.
*/
static enum_gtid_statement_status
gtid_before_statement_begin_group(const THD *thd,
                                  const Gtid_specification *gtid_next)
{
  DBUG_ENTER("gtid_before_statement_begin_group");

  global_sid_lock.assert_some_rdlock();

  if (gtid_next->type == GTID_GROUP)
  {
    /*
      never skip BEGIN/COMMIT.

      @todo: add flag to sql_command_flags to detect if statement
      controls transactions instead of listing the commands in the
      condition below

      @todo: figure out how to handle SQLCOM_XA_*
    */
    enum_sql_command sql_command= thd->lex->sql_command;
    if (sql_command != SQLCOM_COMMIT && sql_command != SQLCOM_BEGIN &&
        sql_command != SQLCOM_ROLLBACK)
    {
      my_thread_id owner= gtid_state.get_owner(gtid_next->gtid);
      if (owner != thd->thread_id)
      {
        DBUG_PRINT("info", ("skipping statement. "
                            "gtid_next->type=%d sql_command=%d "
                            "owner=%lu thd->thread_id=%lu",
                            gtid_next->type, sql_command,
                            owner, thd->thread_id));
        DBUG_RETURN(GTID_STATEMENT_SKIP);
      }
    }
  }
  DBUG_RETURN(GTID_STATEMENT_EXECUTE);
}


enum_gtid_statement_status
gtid_before_statement(THD *thd, Group_cache *gsc, Group_cache *gtc)
{
  DBUG_ENTER("gtid_before_statement");

  const Gtid_set *gtid_next_list= thd->get_gtid_next_list();
  const Gtid_specification *gtid_next= &thd->variables.gtid_next;

#ifndef NO_DBUG
  global_sid_lock.wrlock();
  gtid_state.dbug_print();
  global_sid_lock.unlock();
#endif

  global_sid_lock.rdlock();

  // begin commit-sequence, i.e., acquire group ownerships
  enum_gtid_statement_status ret=
    gtid_before_statement_begin_commit_sequence(thd, gtid_next_list, gtid_next);
  if (ret == GTID_STATEMENT_CANCEL)
  {
    global_sid_lock.unlock();
    DBUG_RETURN(ret);
  }

  // Begin the group, i.e., check if this statement should be skipped
  // or not.
  DBUG_ASSERT(ret == GTID_STATEMENT_EXECUTE);
  ret= gtid_before_statement_begin_group(thd, gtid_next);

  // Generate a warning if the group should be skipped.  We do not
  // generate warning if log_warnings is false, partially because that
  // would make unit tests fail (ER() is not safe to use in unit
  // tests).
  if (ret == GTID_STATEMENT_SKIP && log_warnings)
  {
    char buf[Gtid::MAX_TEXT_LENGTH + 1];
    gtid_next->to_string(&global_sid_map, buf);
    /*
      @todo: tests fail when this is enabled. figure out why /sven
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_SKIPPING_LOGGED_TRANSACTION,
                        ER(ER_SKIPPING_LOGGED_TRANSACTION), buf);
    */
  }

  global_sid_lock.unlock();

  DBUG_RETURN(ret);
}


int gtid_rollback(THD *thd)
{
  DBUG_ENTER("gtid_rollback");

  global_sid_lock.rdlock();
  // gtid_state.update(..., false) can't fail.
#ifndef NO_DBUG
  DBUG_ASSERT(gtid_state.update(thd, false) == RETURN_STATUS_OK);
#else
  gtid_state.update(thd, false);
#endif
  global_sid_lock.unlock();

  DBUG_RETURN(0);
}
