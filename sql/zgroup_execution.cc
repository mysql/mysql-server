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


#include "zgroups.h"
#include "sql_class.h"
#include "binlog.h"
#include "transaction.h"


#ifdef HAVE_GTID


/**
  Acquire group ownership for a single group.  This is used to start a
  master-super-group when @@SESSION.GTID_NEXT_LIST = NULL and
  @@SESSION.GTID_NEXT = SID:GNO.
*/
static enum_gtid_statement_status
gtid_acquire_group_ownership(THD *thd, Checkable_rwlock *lock,
                             Group_log_state *gls,
                             rpl_sidno sidno, rpl_gno gno)
{
  DBUG_ENTER("gtid_acquire_group_ownership");
  enum_gtid_statement_status ret= GTID_STATEMENT_EXECUTE;
  lock->assert_some_rdlock();
  gls->lock_sidno(sidno);
  while (true) {
    if (gls->is_ended(sidno, gno))
    {
      DBUG_ASSERT(gls->get_owner(sidno, gno).is_none());
      ret= GTID_STATEMENT_SKIP;
      break;
    }
    Rpl_owner_id owner= gls->get_owner(sidno, gno);
    if (owner.is_none())
    {
      gls->acquire_ownership(sidno, gno, thd);
      break;
    }
    else if (owner.equals(thd))
    {
      break;
    }
    else
    {
      Group g= { sidno, gno };
      gls->wait_for_sidno(thd, &mysql_bin_log.sid_map, g, owner);
      if (thd->killed || abort_loop)
        DBUG_RETURN(GTID_STATEMENT_CANCEL);
      gls->lock_sidno(sidno);
    }
  } while (false);
  gls->unlock_sidno(sidno);
  DBUG_RETURN(ret);
}


/**
  Acquire ownership of all groups in a GTID_set.  This is used to
  begin a master-super-group when @@SESSION.GTID_NEXT_LIST != NULL.
*/
static enum_gtid_statement_status
gtid_acquire_group_ownerships(THD *thd, Checkable_rwlock *lock,
                              Group_log_state *gls, const GTID_set *gs)
{
  DBUG_ENTER("gtid_acquire_group_ownerships");
  lock->assert_some_rdlock();
  // first check if we need to wait for any group
  while (true)
  {
    GTID_set::GTID_iterator git(gs);
    Group g= git.get();
    Rpl_owner_id owner;
    owner.set_to_none();
    rpl_sidno last_sidno= 0;
    DBUG_ASSERT(g.sidno != 0);
    do {
      // lock all SIDNOs in order
      if (g.sidno != last_sidno)
        gls->lock_sidno(g.sidno);
      if (!gls->is_ended(g.sidno, g.gno))
      {
        owner= gls->get_owner(g.sidno, g.gno);
        // break the do-loop and wait for the sid to be updated
        if (!owner.is_none() && !owner.equals(thd) &&
            !gls->is_partial(g.sidno, g.gno))
          break;
      }
      last_sidno= g.sidno;
      git.next();
      g= git.get();
    } while (g.sidno != 0);

    // we don't need to wait for any groups, and all SIDNOs in the
    // set are locked
    if (g.sidno == 0)
      break;

    // unlock all previous sidnos to avoid blocking them
    // while waiting.  keep lock on g.sidno
    for (rpl_sidno sidno= 1; sidno < g.sidno; sidno++)
      if (gs->contains_sidno(sidno))
        gls->unlock_sidno(sidno);

    // wait
    gls->wait_for_sidno(thd, &mysql_bin_log.sid_map, g, owner);

    // at this point, we don't hold any locks. re-acquire the global
    // read lock that was held when this function was invoked
    if (thd->killed || abort_loop)
      DBUG_RETURN(GTID_STATEMENT_CANCEL);
  }

  // now we know that we don't have to wait for any other
  // thread. so we acquire ownership of all groups that we need
  enum_return_status ret= RETURN_STATUS_OK;
  GTID_set::GTID_iterator git(gs);
  Group g= git.get();
  do {
    if (!gls->is_ended(g.sidno, g.gno))
    {
      Rpl_owner_id owner= gls->get_owner(g.sidno, g.gno);
      if (owner.is_none())
      {
        ret= gls->acquire_ownership(g.sidno, g.gno, thd);
        if (ret != RETURN_STATUS_OK)
          break;
      }
      else
        // in the previous loop, we waited for all groups owned
        // by other threads to become partial or ended
        DBUG_ASSERT(owner.equals(thd) ||
                    gls->is_partial(g.sidno, g.gno) ||
                    gls->is_ended(g.sidno, g.gno));
    }
    git.next();
    g= git.get();
  } while (g.sidno != 0);

  // unlock all sidnos
  rpl_sidno max_sidno= gs->get_max_sidno();
  for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    if (gs->contains_sidno(sidno))
      gls->unlock_sidno(sidno);

  DBUG_RETURN(ret == RETURN_STATUS_OK ?
              GTID_STATEMENT_EXECUTE : GTID_STATEMENT_CANCEL);
}


/**
  Check that the @@SESSION.GTID_* variables are consistent.

  @param thd THD object for the current client.
  @param lock Lock protecting the number of SIDNOs
  @param gsc Group statement cache.
  @param gtc Group transaction cache.
  @param gtid_next_list The @@SESSION.GTID_NEXT_LIST variable (possibly NULL).
  @param gtid_next The @@SESSION.GTID_NEXT variable.
  @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
*/
static enum_return_status
gtid_before_statement_check_session_variables(
  const THD *thd, Checkable_rwlock *lock,
  const Group_cache *gsc, const Group_cache *gtc,
  const GTID_set *gtid_next_list, const Gtid_specification *gtid_next)
{
  DBUG_ENTER("gtid_before_statement_check_session_variables");

  // The group statement cache must be empty in any case when a new
  // statement starts.
  DBUG_ASSERT(gsc->is_empty());

  if (gtid_next_list != NULL)
  {
    // If GTID_NEXT==SID:GNO, then SID:GNO must be listed in GTID_NEXT_LIST
    if (gtid_next->type == Gtid_specification::GTID &&
        !gtid_next_list->contains_group(gtid_next->group.sidno,
                                        gtid_next->group.gno))
    {
      char buf[Gtid_specification::MAX_TEXT_LENGTH + 1];
      lock->rdlock();
      gtid_next->to_string(buf);
      lock->unlock();
      my_error(ER_GTID_NEXT_IS_NOT_IN_GTID_NEXT_LIST, MYF(0), buf);
      RETURN_REPORTED_ERROR;
    }

    // GTID_NEXT cannot be "AUTOMATIC" when GTID_NEXT_LIST != NULL.
    if (gtid_next->type == Gtid_specification::AUTOMATIC)
    {
      my_error(ER_GTID_NEXT_CANT_BE_AUTOMATIC_IF_GTID_NEXT_LIST_IS_NON_NULL,
               MYF(0));
      RETURN_REPORTED_ERROR;
    }
  }

  // If GTID_NEXT=="SID:GNO", then SID:GNO must not be ended in this
  // master-super-group.
  if (gtid_next->type == Gtid_specification::GTID &&
      gtc->group_is_ended(gtid_next->group.sidno, gtid_next->group.gno))
  {
    char buf[Gtid_specification::MAX_TEXT_LENGTH + 1];
    lock->rdlock();
    gtid_next->to_string(buf);
    lock->unlock();
    my_error(ER_GTID_NEXT_IS_ENDED_IN_GROUP_CACHE, MYF(0), buf);
    RETURN_REPORTED_ERROR;
  }

  // If GTID_END==1, then GTID_NEXT must not be "AUTOMATIC" or
  // "ANOYMOUS".
  if ((gtid_next->type == Gtid_specification::AUTOMATIC ||
       gtid_next->type == Gtid_specification::ANONYMOUS) &&
      thd->variables.gtid_end)
  {
    my_error(ER_GTID_END_IS_ON_BUT_GTID_NEXT_IS_AUTO_OR_ANON, MYF(0));
    RETURN_REPORTED_ERROR;
  }

  // If GTID_NEXT_LIST == NULL and GTID_NEXT == "SID:GNO", then
  // GTID_END cannot be 1 unless GTID_COMMIT is 1.  Rationale:
  // otherwise there would be no way to end the master-super-group.
  if (gtid_next_list == NULL &&
      gtid_next->type == Gtid_specification::GTID &&
      thd->variables.gtid_end &&
      !thd->variables.gtid_commit)
  {
    my_error(ER_GTID_END_REQUIRES_GTID_COMMIT_WHEN_GTID_NEXT_LIST_IS_NULL,
             MYF(0));
    RETURN_REPORTED_ERROR;
  }

  RETURN_OK;
}


/**
  Begin master-super-group, i.e., acquire ownership of all groups to
  be re-executed.
*/
static enum_gtid_statement_status
gtid_before_statement_begin_master_super_group(
  THD *thd, Checkable_rwlock *lock, Group_log_state *gls,
  const GTID_set *gtid_next_list, const Gtid_specification *gtid_next)
{
  DBUG_ENTER("gtid_before_statement_begin_master_super_group");

  lock->assert_some_rdlock();

  if (!thd->variables.gtid_has_ongoing_commit_sequence)
  {
    if (gls->ensure_sidno() != 0)
      DBUG_RETURN(GTID_STATEMENT_CANCEL);

    if (gtid_next_list != NULL)
    {
      // acquire group ownership for GTID_set.
      if (!gtid_next_list->is_empty())
        if (gtid_acquire_group_ownerships(thd, lock, gls, gtid_next_list) !=
            GTID_STATEMENT_EXECUTE)
          DBUG_RETURN(GTID_STATEMENT_CANCEL);
      thd->variables.gtid_has_ongoing_commit_sequence= 1;
    }
    else
    {
      if (gtid_next->type == Gtid_specification::GTID)
      {
        // acquire group ownership for single group.
        enum_gtid_statement_status ret=
          gtid_acquire_group_ownership(thd, lock, gls,
                                       gtid_next->group.sidno,
                                       gtid_next->group.gno);
        if (ret != GTID_STATEMENT_CANCEL)
          thd->variables.gtid_has_ongoing_commit_sequence= 1;
        DBUG_RETURN(ret);
      }
      else if (gtid_next->type == Gtid_specification::ANONYMOUS)
      {
        // No need to acquire group ownership.  But we are entering a
        // master-super-group, so set the flag.
        thd->variables.gtid_has_ongoing_commit_sequence= 1;
      }
      else
      {
        DBUG_ASSERT(gtid_next->type == Gtid_specification::AUTOMATIC);
        // We are not entering a master-super-group; do nothing.
      }
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
                                  const Group_log_state *gls,
                                  const Gtid_specification *gtid_next)
{
  DBUG_ENTER("gtid_before_statement_begin_group");

  lock->assert_some_rdlock();

  if (gtid_next->type == Gtid_specification::GTID)
    if (!gls->get_owner(gtid_next->group.sidno,
                        gtid_next->group.gno).equals(thd))
      DBUG_RETURN(GTID_STATEMENT_SKIP);
  DBUG_RETURN(GTID_STATEMENT_EXECUTE);
}


enum_gtid_statement_status
gtid_before_statement(THD *thd, Checkable_rwlock *lock, Group_log_state *gls,
                      Group_cache *gsc, Group_cache *gtc)
{
  DBUG_ENTER("gtid_before_statement");

  const GTID_set *gtid_next_list= thd->get_gtid_next_list();
  const Gtid_specification *gtid_next= &thd->variables.gtid_next;

  // Sanity check session variables.
  if (gtid_before_statement_check_session_variables(thd, lock, gsc, gtc,
                                                    gtid_next_list, gtid_next))
    DBUG_RETURN(GTID_STATEMENT_CANCEL);

  lock->rdlock();

  // begin master-super-group, i.e., acquire group ownerships and set
  // the thd->variables.has_ongoing_commit_sequence to true.
  enum_gtid_statement_status ret=
    gtid_before_statement_begin_master_super_group(thd, lock, gls,
                                                   gtid_next_list, gtid_next);
  if (ret == GTID_STATEMENT_CANCEL)
  {
    lock->unlock();
    DBUG_RETURN(ret);
  }

  // Begin the group, i.e., check if this statement should be skipped
  // or not.
  if (ret == GTID_STATEMENT_EXECUTE)
    ret= gtid_before_statement_begin_group(thd, lock, gls, gtid_next);

  // Generate a warning if the group should be skipped.  We do not
  // generate warning if log_warnings is false, partially because that
  // would make unit tests fail (ER() is not safe to use in unit
  // tests).
  if (ret == GTID_STATEMENT_SKIP && global_system_variables.log_warnings)
  {
    char buf[Group::MAX_TEXT_LENGTH + 1];
    gtid_next->to_string(buf);
    /*
      @todo: tests fail when this is enabled. figure out why /sven
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_SKIPPING_LOGGED_GROUP,
                        ER(ER_SKIPPING_LOGGED_GROUP), buf);
    */
  }

  lock->unlock();

  DBUG_RETURN(ret);
}


int gtid_before_flush_trx_cache(THD *thd, Checkable_rwlock *lock,
                                Group_log_state *gls, Group_cache *trx_cache)
{
  DBUG_ENTER("gtid_before_flush_trx_cache");
  enum_return_status ret= RETURN_STATUS_OK;

  if (thd->variables.gtid_end)
  {
    Gtid_specification *gtid_next= &thd->variables.gtid_next;
    /*
      If GTID_NEXT != NULL and GTID_END = 1, but the group is not
      ended in the binary log and not ended in the transaction group
      cache, then we have to end it by a empty subgroup.
    */
    if (gtid_next->type == Gtid_specification::GTID)
    {
      if (!trx_cache->group_is_ended(gtid_next->group.sidno, 
                                     gtid_next->group.gno))
      {
        lock->rdlock();
        if (!gls->is_ended(gtid_next->group.sidno, gtid_next->group.gno))
          ret= trx_cache->add_empty_subgroup(gtid_next->group.sidno,
                                             gtid_next->group.gno, true);
        lock->unlock();
        PROPAGATE_REPORTED_ERROR_INT(ret);
      }
    }
  }

  if (thd->variables.gtid_commit)
  {
    /*
      If GTID_COMMIT = 1 and GTID_NEXT_LIST != NULL, then we have to
      add empty groups for every group in GTID_NEXT_LIST that does not
      already exist in the cache or in the group log.
    */
    GTID_set *gtid_next_list= thd->get_gtid_next_list();
    if (gtid_next_list != NULL)
    {
      lock->rdlock();
      ret= trx_cache->add_empty_subgroups_if_missing(gls, gtid_next_list);
      lock->unlock();
    }
    else
    {
      /*
        If GTID_COMMIT = 1 and GTID_NEXT_LIST = NULL and GTID_NEXT !=
        NULL, then we have to add a empty group if the group in
        GTID_NEXT does not already exist in the cache or in the group
        log.
      */
      Gtid_specification *gtid_next= &thd->variables.gtid_next;
      if (gtid_next->type == Gtid_specification::GTID)
      {
        lock->rdlock();
        ret= trx_cache->add_empty_subgroup_if_missing(gls,
                                                      gtid_next->group.sidno,
                                                      gtid_next->group.gno);
        lock->unlock();
      }
    }
  }
  PROPAGATE_REPORTED_ERROR_INT(ret);
  DBUG_RETURN(0);
}


#endif
