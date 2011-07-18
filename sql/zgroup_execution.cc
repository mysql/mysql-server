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

#ifdef HAVE_UGID


static int ugid_acquire_group_ownership(THD *thd, Checkable_rwlock *lock,
                                        Group_log_state *gls,
                                        rpl_sidno sidno, rpl_gno gno)
{
  DBUG_ENTER("ugid_acquire_group_ownership");
  //printf("ugid_acquire_group_ownership(sidno=%d gno=%lld)\n", sidno, gno);
  int ret= 0;
  lock->assert_some_rdlock();
  gls->lock_sidno(sidno);
  while (true) {
    if (gls->is_ended(sidno, gno))
    {
      DBUG_ASSERT(gls->get_owner(sidno, gno).is_none());
      // @todo: push warning /sven
      ret= 1;
      //printf("group %d %lld was ended\n", sidno, gno);
      break;
    }
    Rpl_owner_id owner= gls->get_owner(sidno, gno);
    if (owner.is_none())
    {
      //printf("no owner. acquirign ownership\n");
      gls->acquire_ownership(sidno, gno, thd);
      break;
    }
    else if (owner.equals(thd))
    {
      //printf("i own\n");
      break;
    }
    else
    {
      //printf("other owner. waiting.\n");
      lock->unlock();
      Group g= { sidno, gno };
      gls->wait_for_sidno(thd, &mysql_bin_log.sid_map, g, owner);
      lock->rdlock();
      if (thd->killed || abort_loop)
        DBUG_RETURN(1);
      gls->lock_sidno(sidno);
    }
  } while (false);
  gls->unlock_sidno(sidno);
  DBUG_RETURN(ret);
}


static int ugid_acquire_group_ownerships(THD *thd, Checkable_rwlock *lock,
                                         Group_log_state *gls, Group_set *gs)
{
  DBUG_ENTER("ugid_acquire_group_ownerships");
  //printf("ugid_acquire_group_ownerships\n");
  lock->assert_some_rdlock();
  // first check if we need to wait for any group
  while (true)
  {
    Group_set::Group_iterator git(gs);
    Group g= git.get();
    Rpl_owner_id owner;
    owner.set_to_none();
    rpl_sidno last_sidno= 0;
    DBUG_ASSERT(g.sidno != 0);
    do {
      // we lock all SIDNOs in order
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
    // while waiting
    for (rpl_sidno sidno= 1; sidno < g.sidno; sidno++)
      if (gs->contains_sidno(sidno))
        gls->unlock_sidno(sidno);
    lock->unlock();

    // wait
    gls->wait_for_sidno(thd, &mysql_bin_log.sid_map, g, owner);

    // at this point, we don't hold any locks. re-acquire the global
    // read lock that was held when this function was invoked
    lock->rdlock();
    if (thd->killed || abort_loop)
      DBUG_RETURN(1);
  }

  // now we know that we don't have to wait for any other
  // thread. so we acquire ownership of all groups that we need

  // return 1 if all groups are skipped
  int ret= 1;
  Group_set::Group_iterator git(gs);
  Group g= git.get();
  do {
    if (!gls->is_ended(g.sidno, g.gno))
    {
      Rpl_owner_id owner= gls->get_owner(g.sidno, g.gno);
      if (owner.is_none())
      {
        /*
        printf("acquire_ownership(g.sidno=%d, g.gno=%lld, thd=%p)\n",
               g.sidno, g.gno, thd);
        */
        gls->acquire_ownership(g.sidno, g.gno, thd);
        ret= 0;
      }
      else if (owner.equals(thd))
        ret= 0;
      else
        // in the previous loop, we waited for all groups owned
        // by other threads to become partial
        DBUG_ASSERT(gls->is_partial(g.sidno, g.gno));
    }
    git.next();
    g= git.get();
  } while (g.sidno != 0);

  // unlock all sidnos
  rpl_sidno max_sidno= gs->get_max_sidno();
  for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    if (gs->contains_sidno(sidno))
      gls->unlock_sidno(sidno);

  DBUG_RETURN(ret);
}


int ugid_before_statement(THD *thd, Checkable_rwlock *lock,
                          Group_log_state *gls,
                          Group_cache *gsc, Group_cache *gtc)
{
  DBUG_ENTER("ugid_before_statement");
  //printf("ugid_before_statement\n");

  /*
    1. Check that the @@SESSION.UGID_* variables are consistent.
  */
  Group_set *ugid_next_list= thd->get_ugid_next_list();
  Ugid_specification *ugid_next= &thd->variables.ugid_next;
  if (ugid_next_list != NULL)
  {
    // If UGID_NEXT==SID:GNO, then SID:GNO must be listed in UGID_NEXT_LIST
    if (ugid_next->type == Ugid_specification::UGID &&
        !ugid_next_list->contains_group(ugid_next->group.sidno,
                                        ugid_next->group.gno))
    {
      char buf[Ugid_specification::MAX_TEXT_LENGTH + 1];
      lock->rdlock();
      //printf("(%d %lld) not in ugid_next_list=", ugid_next->group.sidno, ugid_next->group.gno);
      ugid_next_list->print();
      abort();
      ugid_next->to_string(buf);
      lock->unlock();
      my_error(ER_UGID_NEXT_IS_NOT_IN_UGID_NEXT_LIST, MYF(0), buf);
      goto skip_no_unlock;
    }

    // UGID_NEXT cannot be "AUTOMATIC" when UGID_NEXT_LIST != NULL.
    if (ugid_next->type == Ugid_specification::AUTOMATIC)
    {
      my_error(ER_UGID_NEXT_CANT_BE_AUTOMATIC_IF_UGID_NEXT_LIST_IS_NON_NULL,
               MYF(0));
      goto skip_no_unlock;
    }
  }

  // If UGID_NEXT=="SID:GNO", then SID:GNO must not be ended in this
  // master-super-group.
  if (ugid_next->type == Ugid_specification::UGID &&
      gtc->group_is_ended(ugid_next->group.sidno, ugid_next->group.gno))
  {
    char buf[Ugid_specification::MAX_TEXT_LENGTH + 1];
    lock->rdlock();
    ugid_next->to_string(buf);
    lock->unlock();
    my_error(ER_UGID_NEXT_IS_ENDED_IN_GROUP_CACHE, MYF(0), buf);
    goto skip_no_unlock;
  }

  // If UGID_END==1, then UGID_NEXT must not be "AUTOMATIC" or
  // "ANOYMOUS".
  if ((ugid_next->type == Ugid_specification::AUTOMATIC ||
       ugid_next->type == Ugid_specification::ANONYMOUS) &&
      thd->variables.ugid_end)
  {
    my_error(ER_UGID_END_IS_ON_BUT_UGID_NEXT_IS_AUTO_OR_ANON, MYF(0));
    goto skip_no_unlock;
  }

  /*
    2. Begin super-group
  */
  lock->rdlock();

  // The group statement cache should be empty when a new statement
  // starts.
  DBUG_ASSERT(gsc->is_empty());

  //printf("thd->server_status & SERVER_STATUS_IN_MASTER_SUPER_GROUP=%d ugid_next_list=%p ugid_next->type=%d ugid_next_list->is_empty=%d\n", thd->server_status & SERVER_STATUS_IN_MASTER_SUPER_GROUP, ugid_next_list, ugid_next->type, ugid_next_list ? ugid_next_list->is_empty(): -1);
  /*if (ugid_next_list)
    ugid_next_list->print();    /// @todo remove this
  */
  if ((thd->server_status & SERVER_STATUS_IN_MASTER_SUPER_GROUP) == 0)
  {
    if (gls->ensure_sidno() != GS_SUCCESS)
    {
      my_error(ER_OUT_OF_RESOURCES, MYF(0), ER(ER_OUT_OF_RESOURCES));
      goto skip_unlock;
    }

    // acquire group ownership
    if (ugid_next_list != NULL)
    {
      if (!ugid_next_list->is_empty())
        if (ugid_acquire_group_ownerships(thd, lock, gls, ugid_next_list))
          goto skip_unlock;
    }
    else
    {
      if (ugid_next->type == Ugid_specification::UGID)
        if (ugid_acquire_group_ownership(thd, lock, gls,
                                         ugid_next->group.sidno,
                                         ugid_next->group.gno))
          goto skip_unlock;
    }

    thd->server_status |= SERVER_STATUS_IN_MASTER_SUPER_GROUP;
  }

  /*
    3. Begin group.
  */

  /*
    At this point, we are in a super-group and have acquired ownership
    of all groups in the super-group.  We now need to skip the
    statement if the current thread does not own the group.
  */
  if (ugid_next->type == Ugid_specification::UGID)
    if (!gls->get_owner(ugid_next->group.sidno,
                        ugid_next->group.gno).equals(thd))
      goto skip_unlock;

  lock->unlock();
  DBUG_RETURN(0);

skip_unlock:
  lock->unlock();
skip_no_unlock:
  DBUG_RETURN(1);
}


void ugid_flush_group_cache(THD *thd, Checkable_rwlock *lock,
                            Group_log_state *gls, Group_cache *gc,
                            Group_cache *trx_cache)
{
  DBUG_ENTER("flush_group_cache");
  lock->rdlock();
  gc->generate_automatic_gno(thd, gls);
  gc->write_to_log(trx_cache);
  gc->update_group_log_state(thd, gls);
  lock->unlock();
  gc->clear();
  DBUG_VOID_RETURN;
}


int ugid_before_flush_trx_cache(THD *thd, Checkable_rwlock *lock,
                                Group_log_state *gls, Group_cache *trx_cache)
{
  DBUG_ENTER("ugid_before_flush_trx_cache");
  enum_group_status ret= GS_SUCCESS;

  if (thd->variables.ugid_end)
  {
    Ugid_specification *ugid_next= &thd->variables.ugid_next;
    /*
      If UGID_NEXT != NULL and UGID_END = 1, but the group is not
      ended in the binary log and not ended in the transaction group
      cache, then we have to end it by a dummy subgroup.
    */
    if (ugid_next->type == Ugid_specification::UGID)
    {
      if (!trx_cache->group_is_ended(ugid_next->group.sidno, 
                                     ugid_next->group.gno))
      {
        lock->rdlock();
        if (!gls->is_ended(ugid_next->group.sidno, ugid_next->group.gno))
          ret= trx_cache->add_dummy_subgroup(ugid_next->group.sidno,
                                             ugid_next->group.gno, true);
        lock->unlock();
      }
    }
  }
  if (ret != GS_SUCCESS)
    DBUG_RETURN(1); /// @todo generate error in log /sven

  /*printf("ugid_before_flush_trx_cache commit=%d\n",
         thd->variables.ugid_commit);
  */
  if (thd->variables.ugid_commit)
  {
    /*
      If UGID_COMMIT = 1 and UGID_NEXT_LIST != NULL, then we have to
      add dummy groups for every group in UGID_NEXT_LIST that does not
      already exist in the cache or in the group log.
    */
    Group_set *ugid_next_list= thd->get_ugid_next_list();
    if (ugid_next_list != NULL)
    {
      lock->rdlock();
      //printf("hello\n");
      //ugid_next_list->print();
      ret= trx_cache->add_dummy_subgroups_if_missing(gls, ugid_next_list);
      lock->unlock();
    }
    else
    {
      /*
        If UGID_COMMIT = 1 and UGID_NEXT_LIST = NULL and UGID_NEXT !=
        NULL, then we have to add a dummy group if the group in
        UGID_NEXT does not already exist in the cache or in the group
        log.
      */
      Ugid_specification *ugid_next= &thd->variables.ugid_next;
      if (ugid_next->type == Ugid_specification::UGID)
      {
        lock->rdlock();
        ret= trx_cache->add_dummy_subgroup_if_missing(gls,
                                                      ugid_next->group.sidno,
                                                      ugid_next->group.gno);
        lock->unlock();
      }
    }
  }
  if (ret != GS_SUCCESS)
    DBUG_RETURN(1); /// @todo generate error in log /sven

  DBUG_RETURN(0);
}


int ugid_after_flush_trx_cache(THD *thd, Group_cache *gc)
{
  DBUG_ENTER("ugid_after_flush_trx_cache");
  int ret= 0;
  if (thd->variables.ugid_commit)
  {
    ret= trans_commit(thd);
    thd->server_status &= ~SERVER_STATUS_IN_MASTER_SUPER_GROUP;
  }
  DBUG_RETURN(ret);
}


#endif
