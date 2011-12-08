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


#include "zgtids.h"
#include "sql_class.h"
#include "binlog.h"
#include "transaction.h"
#include "rpl_slave.h"
#include "rpl_mi.h"


#ifdef HAVE_GTID


/**
  Acquire group ownership for a single group.  This is used to start a
  commit-sequence when @@SESSION.GTID_NEXT_LIST = NULL and
  @@SESSION.GTID_NEXT = SID:GNO.

  @param thd The calling thread.
  @param lock Global sid_lock.
  @param gst Global Gtid_state
  @param sidno SIDNO in @@SESSION.GTID_NEXT
  @param gno GNO in @@SESSION.GTID_NEXT

  @retval GTID_STATEMENT_EXECUTE Success; we have started the
  commit-sequence.  Either the GTID is logged (and will be skipped) or
  we have acquired ownership of it.

  @retval GTID_STATEMENT_CANCEL Failure; the thread was killed or an
  error occurred.  The error has been reported.
*/
static enum_gtid_statement_status
gtid_acquire_ownership(THD *thd, Checkable_rwlock *lock, Gtid_state *gst,
                       rpl_sidno sidno, rpl_gno gno)
{
  DBUG_ENTER("gtid_acquire_ownership");
  enum_gtid_statement_status ret= GTID_STATEMENT_EXECUTE;
  my_thread_id thd_id= thd->id;
  lock->assert_some_rdlock();
  while (true)
  {
    // acquire lock before checking conditions
    gst->lock_sidno(sidno);

    // GTID already logged
    if (gst->is_logged(sidno, gno))
    {
      /*
        Don't skip the statement here, skip it in
        gtid_before_statement_begin_group.
      */
      DBUG_ASSERT(gst->get_owner(sidno, gno) == 0);
      break;
    }
    my_thread_id owner= gst->get_owner(sidno, gno);
    // GTID not owned by anyone: acquire ownership
    if (owner == 0)
    {
      if (gst->acquire_ownership(sidno, gno, thd) != RETURN_STATUS_OK)
        ret= GTID_STATEMENT_CANCEL;
      break;
    }
    // GTID owned by someone (other thread)
    else
    {
      DBUG_ASSERT(owner != thd_id);
      Gtid g= { sidno, gno };
      // The call below releases the read lock on global_sid_lock and
      // the mutex lock on SIDNO.
      gst->wait_for_gtid(thd, g);
      // Re-acquire lock before possibly returning from this function.
      lock->rdlock();

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
  gst->unlock_sidno(sidno);
  DBUG_RETURN(ret);
}


/**
  Acquire ownership of all groups in a Gtid_set.  This is used to
  begin a commit-sequence when @@SESSION.GTID_NEXT_LIST != NULL.
*/
static enum_gtid_statement_status
gtid_acquire_ownerships(THD *thd, Checkable_rwlock *lock, Gtid_state *gst,
                        const Gtid_set *gs)
{
  my_thread_id thd_id= thd->thread_id;
  DBUG_ENTER("gtid_acquire_ownerships");
  lock->assert_some_rdlock();
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
        gst->lock_sidno(g.sidno);
      if (!gst->is_logged(g.sidno, g.gno))
      {
        owner= gst->get_owner(g.sidno, g.gno);
        // break the do-loop and wait for the sid to be updated
        if (owner != 0)
        {
          DBUG_ASSERT(owner != thd_id);
          break;
        }
      }
      last_sidno= g.sidno;
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
        gst->unlock_sidno(sidno);

    // wait. this call releases the read lock on global_sid_lock and
    // the mutex lock on SIDNO
    gst->wait_for_gtid(thd, g);

    // Re-acquire lock before possibly returning from this function.
    lock->rdlock();

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

  /*
    Now the following hold:
     - None of the GTIDs is owned by any other thread.
     - We hold a lock on global_sid_lock.
     - We hold a lock on all SIDNOs that we need.
    So we acquire ownership of all groups that we need.
  */
  enum_return_status ret= RETURN_STATUS_OK;
  Gtid_set::Gtid_iterator git(gs);
  Gtid g= git.get();
  do
  {
    if (!gst->is_logged(g.sidno, g.gno))
    {
      my_thread_id owner= gst->get_owner(g.sidno, g.gno);
      if (owner == 0)
      {
        ret= gst->acquire_ownership(g.sidno, g.gno, thd);
        if (ret != RETURN_STATUS_OK)
          break;
      }
      else
        // in the previous loop, we waited for all groups owned by
        // other threads to become logged, so the only possibility is
        // that this thread owns the group now.
        DBUG_ASSERT(owner == thd_id);
    }
    git.next();
    g= git.get();
  } while (g.sidno != 0);

  // unlock all sidnos
  rpl_sidno max_sidno= gs->get_max_sidno();
  for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    if (gs->contains_sidno(sidno))
      gst->unlock_sidno(sidno);

  DBUG_RETURN(ret == RETURN_STATUS_OK ?
              GTID_STATEMENT_EXECUTE : GTID_STATEMENT_CANCEL);
}


/**
  Check that the @@SESSION.GTID_* variables are consistent.

  @param thd THD object for the current client.
  @param lock Lock protecting the number of SIDNOs
  @param gtid_next_list The @@SESSION.GTID_NEXT_LIST variable (possibly NULL).
  @param gtid_next The @@SESSION.GTID_NEXT variable.
  @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
*/
static enum_return_status gtid_before_statement_check_session_variables(
  Checkable_rwlock *lock,
  const Gtid_set *gtid_next_list, const Gtid_specification *gtid_next)
{
  DBUG_ENTER("gtid_before_statement_check_session_variables");

  if (gtid_next_list != NULL)
  {
    // If GTID_NEXT==SID:GNO, then SID:GNO must be listed in GTID_NEXT_LIST
    if (gtid_next->type == GTID_GROUP &&
        !gtid_next_list->contains_gtid(gtid_next->gtid.sidno,
                                       gtid_next->gtid.gno))
    {
      char buf[Gtid_specification::MAX_TEXT_LENGTH + 1];
      lock->rdlock();
      gtid_next->to_string(&global_sid_map, buf);
      lock->unlock();
      my_error(ER_GTID_NEXT_IS_NOT_IN_GTID_NEXT_LIST, MYF(0), buf);
      RETURN_REPORTED_ERROR;
    }

    // GTID_NEXT cannot be "AUTOMATIC" when GTID_NEXT_LIST != NULL.
    if (gtid_next->type == AUTOMATIC_GROUP)
    {
      my_error(ER_GTID_NEXT_CANT_BE_AUTOMATIC_IF_GTID_NEXT_LIST_IS_NON_NULL,
               MYF(0));
      RETURN_REPORTED_ERROR;
    }
  }

  RETURN_OK;
}


/**
  Begin commit-sequence, i.e., acquire ownership of all groups to be
  re-executed.
*/
static enum_gtid_statement_status
gtid_before_statement_begin_commit_sequence(
  THD *thd, Checkable_rwlock *lock, Gtid_state *gst,
  const Gtid_set *gtid_next_list, const Gtid_specification *gtid_next)
{
  DBUG_ENTER("gtid_before_statement_begin_commit_sequence");

  lock->assert_some_rdlock();

  DBUG_PRINT("info", ("gtid_next: type=%d sidno=%d gno=%lld",
                      gtid_next->type,
                      gtid_next->gtid.sidno, gtid_next->gtid.gno));

  if (!thd->in_active_multi_stmt_transaction())
  {
    if (gst->ensure_sidno() != 0)
      DBUG_RETURN(GTID_STATEMENT_CANCEL);

    if (gtid_next_list != NULL)
    {
      // acquire group ownership for Gtid_set.
      if (!gtid_next_list->is_empty())
        if (gtid_acquire_ownerships(thd, lock, gst, gtid_next_list) !=
            GTID_STATEMENT_EXECUTE)
          DBUG_RETURN(GTID_STATEMENT_CANCEL);
    }
    else if (gtid_next->type == GTID_GROUP)
    {
      // acquire group ownership for single group.
      enum_gtid_statement_status ret=
        gtid_acquire_ownership(thd, lock, gst,
                               gtid_next->gtid.sidno, gtid_next->gtid.gno);
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
gtid_before_statement_begin_group(const THD *thd, Checkable_rwlock *lock,
                                  const Gtid_state *gst,
                                  const Gtid_specification *gtid_next)
{
  DBUG_ENTER("gtid_before_statement_begin_group");

  lock->assert_some_rdlock();

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
      my_thread_id owner= gst->get_owner(gtid_next->gtid.sidno,
                                         gtid_next->gtid.gno);
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
gtid_before_statement(THD *thd, Checkable_rwlock *lock, Gtid_state *gst,
                      Group_cache *gsc, Group_cache *gtc)
{
  DBUG_ENTER("gtid_before_statement");

  const Gtid_set *gtid_next_list= thd->get_gtid_next_list();
  const Gtid_specification *gtid_next= &thd->variables.gtid_next;

  // Sanity check session variables.
  if (gtid_before_statement_check_session_variables(lock, gtid_next_list,
                                                    gtid_next) !=
      RETURN_STATUS_OK)
    DBUG_RETURN(GTID_STATEMENT_CANCEL);

  lock->rdlock();

  // begin commit-sequence, i.e., acquire group ownerships
  enum_gtid_statement_status ret=
    gtid_before_statement_begin_commit_sequence(thd, lock, gst,
                                                gtid_next_list, gtid_next);
  if (ret == GTID_STATEMENT_CANCEL)
  {
    lock->unlock();
    DBUG_RETURN(ret);
  }

  // Begin the group, i.e., check if this statement should be skipped
  // or not.
  DBUG_ASSERT(ret == GTID_STATEMENT_EXECUTE);
  ret= gtid_before_statement_begin_group(thd, lock, gst, gtid_next);

  // Generate a warning if the group should be skipped.  We do not
  // generate warning if log_warnings is false, partially because that
  // would make unit tests fail (ER() is not safe to use in unit
  // tests).
  if (ret == GTID_STATEMENT_SKIP && global_system_variables.log_warnings)
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

  lock->unlock();

  DBUG_RETURN(ret);
}


int gtid_before_flush_trx_cache(THD *thd, Checkable_rwlock *lock,
                                Gtid_state *gst, Group_cache *trx_cache)
{
  DBUG_ENTER("gtid_before_flush_trx_cache");
  enum_return_status ret= RETURN_STATUS_OK;

  /*
    If GTID_NEXT_LIST != NULL, then we have to add empty groups for
    every group in GTID_NEXT_LIST that does not already exist in the
    cache or in the group log.
  */
  Gtid_set *gtid_next_list= thd->get_gtid_next_list();
  if (gtid_next_list != NULL)
  {
    lock->rdlock();
    ret= trx_cache->add_empty_groups_if_missing(gst, gtid_next_list);
    lock->unlock();
  }
  else
  {
    /*
      If GTID_NEXT_LIST = NULL and GTID_NEXT != NULL, then we have to
      add an empty group if the group in GTID_NEXT does not already
      exist in the cache or in the group log.
    */
    Gtid_specification *gtid_next= &thd->variables.gtid_next;
    if (gtid_next->type == GTID_GROUP)
    {
      lock->rdlock();
      ret= trx_cache->add_empty_group_if_missing(gst, gtid_next->gtid.sidno,
                                                 gtid_next->gtid.gno);
      lock->unlock();
    }
  }
  PROPAGATE_REPORTED_ERROR_INT(ret);
  DBUG_RETURN(0);
}


int gtid_rollback(THD *thd)
{
  DBUG_ENTER("gtid_rollback");
  Gtid_set *gtid_next_list= thd->get_gtid_next_list();
  if (gtid_next_list != NULL)
  {
    global_sid_lock.rdlock();
    gtid_state.lock_sidnos(gtid_next_list);
    Gtid_set::Gtid_iterator it(gtid_next_list);
    Gtid gtid= it.get();
    while (gtid.sidno != 0)
    {
      gtid_state.release_ownership(gtid.sidno, gtid.gno);
      gtid= it.get();
    }
    gtid_state.unlock_sidnos(gtid_next_list);
    global_sid_lock.unlock();
  }
  else
  {
    Gtid_specification *gtid_next= &thd->variables.gtid_next;
    if (gtid_next->type == GTID_GROUP)
    {
      rpl_sidno sidno= gtid_next->gtid.sidno;
      rpl_gno gno= gtid_next->gtid.gno;
      global_sid_lock.rdlock();
      gtid_state.lock_sidno(sidno);
      gtid_state.release_ownership(sidno, gno);
      gtid_state.unlock_sidno(sidno);
      global_sid_lock.unlock();
    }
  }
  DBUG_RETURN(0);
}


#endif
