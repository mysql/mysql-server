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

#include "rpl_mi.h"
#include "rpl_slave.h"
#include "sql_class.h"


void Gtid_state::clear()
{
  DBUG_ENTER("Gtid_state::clear()");
  // the wrlock implies that no other thread can hold any of the mutexes
  sid_lock->assert_some_wrlock();
  logged_gtids.clear();
  lost_gtids.clear();
  DBUG_VOID_RETURN;
}


enum_return_status Gtid_state::acquire_ownership(THD *thd, const Gtid &gtid)
{
  DBUG_ENTER("Gtid_state::acquire_ownership");
  // caller must take lock on the SIDNO.
  global_sid_lock->assert_some_lock();
  gtid_state->assert_sidno_lock_owner(gtid.sidno);
  DBUG_ASSERT(!logged_gtids.contains_gtid(gtid));
  DBUG_PRINT("info", ("group=%d:%lld", gtid.sidno, gtid.gno));
  DBUG_ASSERT(thd->owned_gtid.sidno == 0);
  if (owned_gtids.add_gtid_owner(gtid, thd->thread_id) != RETURN_STATUS_OK)
    goto err2;
  if (thd->get_gtid_next_list() != NULL)
  {
#ifdef HAVE_GTID_NEXT_LIST
    if (thd->owned_gtid_set._add_gtid(gtid) != RETURN_STATUS_OK)
      goto err1;
    thd->owned_gtid.sidno= -1;
#else
    DBUG_ASSERT(0);
#endif
  }
  else
    thd->owned_gtid= gtid;
  RETURN_OK;
#ifdef HAVE_GTID_NEXT_LIST
err1:
  owned_gtids.remove_gtid(gtid);
#endif
err2:
  if (thd->get_gtid_next_list() != NULL)
  {
#ifdef HAVE_GTID_NEXT_LIST
    Gtid_set::Gtid_iterator git(&thd->owned_gtid_set);
    Gtid g= git.get();
    while (g.sidno != 0)
    {
      owned_gtids.remove_gtid(g);
      g= git.get();
    }
#else
    DBUG_ASSERT(0);
#endif
  }
  thd->owned_gtid_set.clear();
  thd->owned_gtid.sidno= 0;
  RETURN_REPORTED_ERROR;
}

#ifdef HAVE_GTID_NEXT_LIST
void Gtid_state::lock_owned_sidnos(const THD *thd)
{
  if (thd->owned_gtid.sidno == -1)
    lock_sidnos(&thd->owned_gtid_set);
  else if (thd->owned_gtid.sidno > 0)
    lock_sidno(thd->owned_gtid.sidno);
}
#endif


void Gtid_state::unlock_owned_sidnos(const THD *thd)
{
  if (thd->owned_gtid.sidno == -1)
  {
#ifdef HAVE_GTID_NEXT_LIST
    unlock_sidnos(&thd->owned_gtid_set);
#else
    DBUG_ASSERT(0);
#endif
  }
  else if (thd->owned_gtid.sidno > 0)
  {
    unlock_sidno(thd->owned_gtid.sidno);
  }
}


void Gtid_state::broadcast_owned_sidnos(const THD *thd)
{
  if (thd->owned_gtid.sidno == -1)
  {
#ifdef HAVE_GTID_NEXT_LIST
    broadcast_sidnos(&thd->owned_gtid_set);
#else
    DBUG_ASSERT(0);
#endif
  }
  else if (thd->owned_gtid.sidno > 0)
  {
    broadcast_sidno(thd->owned_gtid.sidno);
  }
}


enum_return_status Gtid_state::update_on_flush(THD *thd)
{
  DBUG_ENTER("Gtid_state::update_on_flush");
  enum_return_status ret= RETURN_STATUS_OK;

  // Caller must take lock on the SIDNO.
  global_sid_lock->assert_some_lock();

  if (thd->owned_gtid.sidno == -1)
  {
#ifdef HAVE_GTID_NEXT_LIST
    rpl_sidno prev_sidno= 0;
    Gtid_set::Gtid_iterator git(&thd->owned_gtid_set);
    Gtid g= git.get();
    while (g.sidno != 0)
    {
      if (g.sidno != prev_sidno)
        sid_locks.lock(g.sidno);
      if (ret == RETURN_STATUS_OK)
        ret= logged_gtids._add_gtid(g);
      git.next();
      g= git.get();
    }
#else
    DBUG_ASSERT(0);
#endif
  }
  else if (thd->owned_gtid.sidno > 0)
  {
    lock_sidno(thd->owned_gtid.sidno);
    ret= logged_gtids._add_gtid(thd->owned_gtid);
  }

  /*
    There may be commands that cause implicit commits, e.g.
    SET AUTOCOMMIT=1 may cause the previous statements to commit
    without executing a COMMIT command or be on auto-commit mode.
    Although we set GTID_NEXT type to UNDEFINED on
    Gtid_state::update_on_commit(), we also set it here to do it
    as soon as possible.
  */
  thd->variables.gtid_next.set_undefined();
  broadcast_owned_sidnos(thd);
  unlock_owned_sidnos(thd);

  DBUG_RETURN(ret);
}


void Gtid_state::update_on_commit(THD *thd)
{
  DBUG_ENTER("Gtid_state::update_on_commit");
  update_owned_gtids_impl(thd, true);
  DBUG_VOID_RETURN;
}


void Gtid_state::update_on_rollback(THD *thd)
{
  DBUG_ENTER("Gtid_state::update_on_rollback");
  update_owned_gtids_impl(thd, false);
  DBUG_VOID_RETURN;
}


void Gtid_state::update_owned_gtids_impl(THD *thd, bool is_commit)
{
  DBUG_ENTER("Gtid_state::update_owned_gtids_impl");

  // Caller must take lock on the SIDNO.
  global_sid_lock->assert_some_lock();

  if (thd->owned_gtid.sidno == -1)
  {
#ifdef HAVE_GTID_NEXT_LIST
    rpl_sidno prev_sidno= 0;
    Gtid_set::Gtid_iterator git(&thd->owned_gtid_set);
    Gtid g= git.get();
    while (g.sidno != 0)
    {
      if (g.sidno != prev_sidno)
        sid_locks.lock(g.sidno);
      owned_gtids.remove_gtid(g);
      git.next();
      g= git.get();
    }
#else
    DBUG_ASSERT(0);
#endif
  }
  else if (thd->owned_gtid.sidno > 0)
  {
    lock_sidno(thd->owned_gtid.sidno);
    owned_gtids.remove_gtid(thd->owned_gtid);
  }

  /*
    There may be commands that cause implicit commits, e.g.
    SET AUTOCOMMIT=1 may cause the previous statements to commit
    without executing a COMMIT command or be on auto-commit mode.
  */
  thd->variables.gtid_next.set_undefined();
  if (!is_commit)
    broadcast_owned_sidnos(thd);
  unlock_owned_sidnos(thd);

  thd->clear_owned_gtids();

  DBUG_VOID_RETURN;
}


rpl_gno Gtid_state::get_automatic_gno(rpl_sidno sidno) const
{
  DBUG_ENTER("Gtid_state::get_automatic_gno");
  //logged_gtids.ensure_sidno(sidno);
  Gtid_set::Const_interval_iterator ivit(&logged_gtids, sidno);
  Gtid next_candidate= { sidno, 1 };
  while (true)
  {
    const Gtid_set::Interval *iv= ivit.get();
    rpl_gno next_interval_start= iv != NULL ? iv->start : MAX_GNO;
    while (next_candidate.gno < next_interval_start)
    {
      if (owned_gtids.get_owner(next_candidate) == 0)
        DBUG_RETURN(next_candidate.gno);
      next_candidate.gno++;
    }
    if (iv == NULL)
    {
      my_error(ER_GNO_EXHAUSTED, MYF(0));
      DBUG_RETURN(-1);
    }
    next_candidate.gno= iv->end;
    ivit.next();
  }
}


void Gtid_state::wait_for_gtid(THD *thd, const Gtid &gtid)
{
  DBUG_ENTER("Gtid_state::wait_for_gtid");
  // Enter cond, wait, exit cond.
  PSI_stage_info old_stage;
  DBUG_PRINT("info", ("SIDNO=%d GNO=%lld owner(sidno,gno)=%lu thread_id=%lu",
                      gtid.sidno, gtid.gno,
                      owned_gtids.get_owner(gtid), thd->thread_id));
  DBUG_ASSERT(owned_gtids.get_owner(gtid) != thd->thread_id);
  sid_locks.enter_cond(thd, gtid.sidno,
                       &stage_waiting_for_gtid_to_be_written_to_binary_log,
                       &old_stage);
  //while (get_owner(g.sidno, g.gno) != 0 && !thd->killed && !abort_loop)
  sid_locks.wait(gtid.sidno);
  thd->EXIT_COND(&old_stage);

  DBUG_VOID_RETURN;
}


#ifdef HAVE_GTID_NEXT_LIST
void Gtid_state::lock_sidnos(const Gtid_set *gs)
{
  DBUG_ASSERT(gs);
  rpl_sidno max_sidno= gs->get_max_sidno();
  for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    if (gs->contains_sidno(sidno))
      lock_sidno(sidno);
}


void Gtid_state::unlock_sidnos(const Gtid_set *gs)
{
  DBUG_ASSERT(gs);
  rpl_sidno max_sidno= gs->get_max_sidno();
  for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    if (gs->contains_sidno(sidno))
      unlock_sidno(sidno);
}


void Gtid_state::broadcast_sidnos(const Gtid_set *gs)
{
  DBUG_ASSERT(gs);
  rpl_sidno max_sidno= gs->get_max_sidno();
  for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    if (gs->contains_sidno(sidno))
      broadcast_sidno(sidno);
}
#endif


enum_return_status Gtid_state::ensure_sidno()
{
  DBUG_ENTER("Gtid_state::ensure_sidno");
  sid_lock->assert_some_wrlock();
  rpl_sidno sidno= sid_map->get_max_sidno();
  if (sidno > 0)
  {
    // The lock may be temporarily released during one of the calls to
    // ensure_sidno or ensure_index.  Hence, we must re-check the
    // condition after the calls.
    PROPAGATE_REPORTED_ERROR(logged_gtids.ensure_sidno(sidno));
    PROPAGATE_REPORTED_ERROR(lost_gtids.ensure_sidno(sidno));
    PROPAGATE_REPORTED_ERROR(owned_gtids.ensure_sidno(sidno));
    PROPAGATE_REPORTED_ERROR(sid_locks.ensure_index(sidno));
    sidno= sid_map->get_max_sidno();
    DBUG_ASSERT(logged_gtids.get_max_sidno() >= sidno);
    DBUG_ASSERT(lost_gtids.get_max_sidno() >= sidno);
    DBUG_ASSERT(owned_gtids.get_max_sidno() >= sidno);
    DBUG_ASSERT(sid_locks.get_max_index() >= sidno);
  }
  RETURN_OK;
}


enum_return_status Gtid_state::add_lost_gtids(const char *text)
{
  DBUG_ENTER("Gtid_state::add_lost_gtids()");
  sid_lock->assert_some_wrlock();

  DBUG_PRINT("info", ("add_lost_gtids '%s'", text));

  if (!logged_gtids.is_empty())
  {
    BINLOG_ERROR((ER(ER_CANT_SET_GTID_PURGED_WHEN_GTID_EXECUTED_IS_NOT_EMPTY)),
                 (ER_CANT_SET_GTID_PURGED_WHEN_GTID_EXECUTED_IS_NOT_EMPTY,
                 MYF(0)));
    RETURN_REPORTED_ERROR;
  }
  if (!owned_gtids.is_empty())
  {
    BINLOG_ERROR((ER(ER_CANT_SET_GTID_PURGED_WHEN_OWNED_GTIDS_IS_NOT_EMPTY)),
                 (ER_CANT_SET_GTID_PURGED_WHEN_OWNED_GTIDS_IS_NOT_EMPTY,
                 MYF(0)));
    RETURN_REPORTED_ERROR;
  }
  DBUG_ASSERT(lost_gtids.is_empty());

  PROPAGATE_REPORTED_ERROR(lost_gtids.add_gtid_text(text));
  PROPAGATE_REPORTED_ERROR(logged_gtids.add_gtid_text(text));

  DBUG_RETURN(RETURN_STATUS_OK);
}


int Gtid_state::init()
{
  DBUG_ENTER("Gtid_state::init()");

  global_sid_lock->assert_some_lock();

  rpl_sid server_sid;
  if (server_sid.parse(server_uuid) != RETURN_STATUS_OK)
    DBUG_RETURN(1);
  rpl_sidno sidno= sid_map->add_sid(server_sid);
  if (sidno <= 0)
    DBUG_RETURN(1);
  server_sidno= sidno;

  DBUG_RETURN(0);
}
