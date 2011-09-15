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


#include "my_global.h"
#include "zgroups.h"
#include "rpl_mi.h"
#include "rpl_slave.h"
#include "sql_class.h"


#ifdef HAVE_UGID


void Group_log_state::clear()
{
  DBUG_ENTER("Group_log_state::clear()");
  sid_lock->rdlock();
  rpl_sidno max_sidno= sid_map->get_max_sidno();
  DBUG_PRINT("info", ("max_sidno=%d", max_sidno));
  for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    sid_locks.lock(sidno);

  ended_groups.clear();
  owned_groups.clear();

  for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    sid_locks.unlock(sidno);
  sid_lock->unlock();
  DBUG_VOID_RETURN;
}


enum_group_status
Group_log_state::acquire_ownership(rpl_sidno sidno, rpl_gno gno, const THD *thd)
{
  DBUG_ENTER("Group_log_state::acquire_ownership");
  //ended_groups.ensure_sidno(sidno);
  //printf("Group_log_state::acquire_ownership(sidno=%d gno=%lld)\n", sidno, gno);
  DBUG_ASSERT(!ended_groups.contains_group(sidno, gno));
  DBUG_PRINT("info", ("acquire ownership of group %d:%lld", sidno, gno));
  Rpl_owner_id owner;
  owner.copy_from(thd);
  enum_group_status ret= owned_groups.add(sidno, gno, owner);
  DBUG_RETURN(ret);
}


enum_group_status Group_log_state::end_group(rpl_sidno sidno, rpl_gno gno)
{
  DBUG_ENTER("Group_log_state::end_group");
  DBUG_PRINT("info", ("ending group %d:%lld", sidno, gno));
  owned_groups.remove(sidno, gno);
  enum_group_status ret= ended_groups._add(sidno, gno);
  DBUG_RETURN(ret);
}


rpl_gno Group_log_state::get_automatic_gno(rpl_sidno sidno) const
{
  DBUG_ENTER("Group_log_state::end_automatic_group");
  //ended_groups.ensure_sidno(sidno);
  Group_set::Const_interval_iterator ivit(&ended_groups, sidno);
  rpl_gno next_candidate= 1;
  //printf("get_automatic_gno\n");
  while (true)
  {
    Group_set::Interval *iv= ivit.get();
    rpl_gno next_interval_start= iv != NULL ? iv->start : MAX_GNO;
    while (next_candidate < next_interval_start)
    {
      //printf("next_candidate=%lld\n", next_candidate);
      if (owned_groups.get_owner(sidno, next_candidate).is_none())
        DBUG_RETURN(next_candidate);
      next_candidate++;
    }
    /*
      @todo: check for error
    if (iv == NULL)
      my_error();
    */
    next_candidate= iv->end;
    ivit.next();
  }
}


void Group_log_state::wait_for_sidno(THD *thd, const Sid_map *sm,
                                     Group g, Rpl_owner_id owner)
{
  DBUG_ENTER("Group_log_state::wait_for_sidno");
  // Enter cond, wait, exit cond.
  PSI_stage_info old_stage;
  sid_locks.enter_cond(thd, g.sidno,
                       &stage_waiting_for_group_to_be_written_to_binary_log,
                       &old_stage);
  while (!is_partial(g.sidno, g.gno) && !thd->killed && !abort_loop)
    sid_locks.wait(g.sidno);
  thd->EXIT_COND(&old_stage);

  DBUG_VOID_RETURN;
}


void Group_log_state::lock_sidnos(const Group_set *gs)
{
  rpl_sidno max_sidno= gs ? gs->get_max_sidno() : sid_map->get_max_sidno();
  for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    if (!gs || gs->contains_sidno(sidno))
      lock_sidno(sidno);
}


void Group_log_state::unlock_sidnos(const Group_set *gs)
{
  rpl_sidno max_sidno= gs ? gs->get_max_sidno() : sid_map->get_max_sidno();
  for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    if (!gs || gs->contains_sidno(sidno))
      unlock_sidno(sidno);
}


void Group_log_state::broadcast_sidnos(const Group_set *gs)
{
  rpl_sidno max_sidno= gs->get_max_sidno();
  for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    if (gs->contains_sidno(sidno))
      broadcast_sidno(sidno);
}


enum_group_status Group_log_state::ensure_sidno()
{
  DBUG_ENTER("Group_log_state::ensure_sidno");
  sid_lock->assert_some_rdlock();
  rpl_sidno sidno= sid_map->get_max_sidno();
  if (sidno > 0)
  {
    // The lock may be temporarily released during one of the calls to
    // ensure_sidno or ensure_index.  Hence, we must re-check the
    // condition after the calls.
    do
    {
      GROUP_STATUS_THROW(ended_groups.ensure_sidno(sidno));
      GROUP_STATUS_THROW(owned_groups.ensure_sidno(sidno));
      GROUP_STATUS_THROW(sid_locks.ensure_index(sidno));
    } while (ended_groups.get_max_sidno() < sidno ||
             owned_groups.get_max_sidno() < sidno ||
             sid_locks.get_max_index() < sidno);
  }
  DBUG_RETURN(GS_SUCCESS);
}


#endif /* HAVE_UGID */
