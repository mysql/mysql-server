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
#include "zgtids.h"
#include "rpl_mi.h"
#include "rpl_slave.h"
#include "sql_class.h"


#ifdef HAVE_GTID


Gtid_state gtid_state(&global_sid_lock, &global_sid_map);


void Gtid_state::clear()
{
  DBUG_ENTER("Gtid_state::clear()");
  // the wrlock implies that no other thread can hold any of the mutexes
  sid_lock->assert_some_wrlock();
  logged_gtids.clear();
  lost_gtids.clear();
  DBUG_VOID_RETURN;
}


enum_return_status
Gtid_state::acquire_ownership(rpl_sidno sidno, rpl_gno gno, const THD *thd)
{
  DBUG_ENTER("Gtid_state::acquire_ownership");
  DBUG_ASSERT(!logged_gtids.contains_gtid(sidno, gno));
  DBUG_PRINT("info", ("group=%d:%lld", sidno, gno));
  PROPAGATE_REPORTED_ERROR(owned_gtids.add(sidno, gno, thd->thread_id));
  RETURN_OK;
}


enum_return_status Gtid_state::log_group(rpl_sidno sidno, rpl_gno gno)
{
  DBUG_ENTER("Gtid_state::log_group");
  DBUG_PRINT("info", ("group=%d:%lld", sidno, gno));
  owned_gtids.remove(sidno, gno);
  PROPAGATE_REPORTED_ERROR(logged_gtids._add(sidno, gno));
  RETURN_OK;
}


rpl_gno Gtid_state::get_automatic_gno(rpl_sidno sidno) const
{
  DBUG_ENTER("Gtid_state::get_automatic_gno");
  //logged_gtids.ensure_sidno(sidno);
  Gtid_set::Const_interval_iterator ivit(&logged_gtids, sidno);
  rpl_gno next_candidate= 1;
  while (true)
  {
    Gtid_set::Interval *iv= ivit.get();
    rpl_gno next_interval_start= iv != NULL ? iv->start : MAX_GNO;
    while (next_candidate < next_interval_start)
    {
      if (owned_gtids.get_owner(sidno, next_candidate) == 0)
        DBUG_RETURN(next_candidate);
      next_candidate++;
    }
    if (iv == NULL)
    {
      my_error(ER_GNO_EXHAUSTED, MYF(0));
      DBUG_RETURN(-1);
    }
    next_candidate= iv->end;
    ivit.next();
  }
}


void Gtid_state::wait_for_gtid(THD *thd, Gtid g)
{
  DBUG_ENTER("Gtid_state::wait_for_sidno");
  // Enter cond, wait, exit cond.
  PSI_stage_info old_stage;
  sid_locks.enter_cond(thd, g.sidno,
                       &stage_waiting_for_group_to_be_written_to_binary_log,
                       &old_stage);
  while (get_owner(g.sidno, g.gno) != 0 && !thd->killed && !abort_loop)
  {
    sid_lock->unlock();
    sid_locks.wait(g.sidno);
    sid_lock->rdlock();
  }
  thd->EXIT_COND(&old_stage);

  DBUG_VOID_RETURN;
}


void Gtid_state::lock_sidnos(const Gtid_set *gs)
{
  rpl_sidno max_sidno= gs ? gs->get_max_sidno() : sid_map->get_max_sidno();
  for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    if (!gs || gs->contains_sidno(sidno))
      lock_sidno(sidno);
}


void Gtid_state::unlock_sidnos(const Gtid_set *gs)
{
  rpl_sidno max_sidno= gs ? gs->get_max_sidno() : sid_map->get_max_sidno();
  for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    if (!gs || gs->contains_sidno(sidno))
      unlock_sidno(sidno);
}


void Gtid_state::broadcast_sidnos(const Gtid_set *gs)
{
  rpl_sidno max_sidno= gs->get_max_sidno();
  for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    if (gs->contains_sidno(sidno))
      broadcast_sidno(sidno);
}


enum_return_status Gtid_state::ensure_sidno()
{
  DBUG_ENTER("Gtid_state::ensure_sidno");
  sid_lock->assert_some_lock();
  rpl_sidno sidno= sid_map->get_max_sidno();
  if (sidno > 0)
  {
    // The lock may be temporarily released during one of the calls to
    // ensure_sidno or ensure_index.  Hence, we must re-check the
    // condition after the calls.
    do
    {
      PROPAGATE_REPORTED_ERROR(logged_gtids.ensure_sidno(sidno));
      PROPAGATE_REPORTED_ERROR(lost_gtids.ensure_sidno(sidno));
      PROPAGATE_REPORTED_ERROR(owned_gtids.ensure_sidno(sidno));
      PROPAGATE_REPORTED_ERROR(sid_locks.ensure_index(sidno));
      sidno= sid_map->get_max_sidno();
    } while (logged_gtids.get_max_sidno() < sidno ||
             owned_gtids.get_max_sidno() < sidno ||
             sid_locks.get_max_index() < sidno);
  }
  RETURN_OK;
}


int Gtid_state::init()
{
  DBUG_ENTER("Gtid_state::init()");

  global_sid_lock.assert_some_lock();

  rpl_sid server_sid;
  if (server_sid.parse(server_uuid) != RETURN_STATUS_OK)
    DBUG_RETURN(1);
  rpl_sidno sidno= sid_map->add(&server_sid);
  if (sidno <= 0)
    DBUG_RETURN(1);
  server_sidno= sidno;
  if (ensure_sidno() != RETURN_STATUS_OK)
    DBUG_RETURN(1);

  DBUG_RETURN(0);
}


#endif /* HAVE_GTID */
