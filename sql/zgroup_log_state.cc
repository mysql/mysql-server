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


const char *Group_log_state::COND_MESSAGE_WAIT_FOR_LIVE_SQL_THREAD=
  "Waiting for group %s to be ended. The group is owned by the SQL thread, "
  "which is currently executing.";
const char *Group_log_state::COND_MESSAGE_WAIT_FOR_DEAD_SQL_THREAD=
  "Waiting for group %s to be ended. The group is owned by the SQL thread, "
  "which is currently stopped. "
  "The group can be resumed by the slave if you start it again, or by "
  "a client after setting @@SESSION.UGID_CONTINUE_ORPHAN = 1.";
const char *Group_log_state::COND_MESSAGE_WAIT_FOR_LIVE_CLIENT=
  "Waiting for group %s to be ended. "
  "The group is owned by client %u, which is currently executing.";
const char *Group_log_state::COND_MESSAGE_WAIT_FOR_DEAD_CLIENT=
  "Waiting for group %s to be ended. "
  "The group is owned by client thread %u, which has disconnected. "
  "The group can only be resumed by a client after setting "
  "@@SESSION.UGID_CONTINUE_ORPHAN = 1.";
const char *Group_log_state::COND_MESSAGE_WAIT_FOR_VERY_OLD_CLIENT=
  "Waiting for group %s to be ended. "
  "The group is owned by a thread that existed on a previous server "
  "instance. "
  "The group can only be resumed by a client after setting "
  "@@SESSION.UGID_CONTINUE_ORPHAN = 1.";
const char *Group_log_state::COND_MESSAGE_WAIT_FOR_SQL_THREAD_NO_REPLICATION=
  "Waiting for group %s to be ended. "
  "The group is owned by the SQL thread, but the current server instance "
  "is not configured for replication. "
  "The group can only be resumed by a client after setting "
  "@@SESSION.UGID_CONTINUE_ORPHAN = 1.";
const uint Group_log_state::COND_MESSAGE_MAX_TEXT_LENGTH= 1024;


void Group_log_state::clear()
{
  sid_lock->rdlock();
  rpl_sidno max_sidno= sid_map->get_max_sidno();
  for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    sid_locks.lock(sidno);

  ended_groups.clear();
  owned_groups.clear();

  for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    sid_locks.unlock(sidno);
  sid_lock->unlock();
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
  // Generate message.
  char buf[Group_log_state::COND_MESSAGE_MAX_TEXT_LENGTH + 1];
  char group[Group::MAX_TEXT_LENGTH + 1];
  g.to_string(sm, group);

  // Assert that the buffer has space for any of the messages (would
  // have been better to do at compile-time, but this is C++...)
#define ASSERT_LENGTH(x)                                        \
  DBUG_ASSERT(Group_log_state::COND_MESSAGE_MAX_TEXT_LENGTH <   \
              strlen(COND_MESSAGE_WAIT_FOR_##x) + 2 * 22);
  ASSERT_LENGTH(VERY_OLD_CLIENT);
  ASSERT_LENGTH(LIVE_CLIENT);
  ASSERT_LENGTH(DEAD_CLIENT);
  ASSERT_LENGTH(LIVE_SQL_THREAD);
  ASSERT_LENGTH(DEAD_SQL_THREAD);
  ASSERT_LENGTH(SQL_THREAD_NO_REPLICATION);

  if (owner.is_client())
  {
    if (owner.is_very_old_client())
      sprintf(buf, COND_MESSAGE_WAIT_FOR_VERY_OLD_CLIENT, group);
    else if (owner.is_live_client())
      sprintf(buf, COND_MESSAGE_WAIT_FOR_LIVE_CLIENT, group, owner.thread_id);
    else
      sprintf(buf, COND_MESSAGE_WAIT_FOR_DEAD_CLIENT, group, owner.thread_id);
  }
  else
  {
#ifdef HAVE_REPLICATION
    mysql_mutex_lock(&LOCK_active_mi);
    bool running= active_mi->rli->slave_running;
    mysql_mutex_unlock(&LOCK_active_mi);
    if (running)
      sprintf(buf, COND_MESSAGE_WAIT_FOR_LIVE_SQL_THREAD, group);
    else
      sprintf(buf, COND_MESSAGE_WAIT_FOR_DEAD_SQL_THREAD, group);
#else
    sprintf(buf, COND_MESSAGE_WAIT_FOR_SQL_THREAD_NO_REPLICATION, group);
#endif
  }
  // Enter cond, wait, exit cond.
  const char *prev_proc_info= sid_locks.enter_cond(thd, buf, g.sidno);
  while (!is_partial(g.sidno, g.gno) && !thd->killed && !abort_loop)
    sid_locks.wait(g.sidno);
  thd->exit_cond(prev_proc_info);

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
