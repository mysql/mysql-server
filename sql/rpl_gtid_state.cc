/* Copyright (c) 2011, 2014, Oracle and/or its affiliates. All rights reserved.

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
#include "rpl_gtid_persist.h"
#include "log.h"
#include "binlog.h"


int Gtid_state::clear(THD *thd)
{
  DBUG_ENTER("Gtid_state::clear()");
  int ret= 0;
  // the wrlock implies that no other thread can hold any of the mutexes
  sid_lock->assert_some_wrlock();
  lost_gtids.clear();
  executed_gtids.clear();
  gtids_only_in_table.clear();
  previous_gtids_logged.clear();
  /* Reset gtid_executed table. */
  if ((ret= gtid_table_persistor->reset(thd)) == 1)
  {
    /*
      Gtid table is not ready to be used, so failed to
      open it. Ignore the error.
    */
    thd->clear_error();
    ret= 0;
  }

  DBUG_RETURN(ret);
}


enum_return_status Gtid_state::acquire_ownership(THD *thd, const Gtid &gtid)
{
  DBUG_ENTER("Gtid_state::acquire_ownership");
  // caller must take lock on the SIDNO.
  global_sid_lock->assert_some_lock();
  gtid_state->assert_sidno_lock_owner(gtid.sidno);
  DBUG_ASSERT(!executed_gtids.contains_gtid(gtid));
  DBUG_PRINT("info", ("group=%d:%lld", gtid.sidno, gtid.gno));
  DBUG_ASSERT(thd->owned_gtid.sidno == 0);
  if (owned_gtids.add_gtid_owner(gtid, thd->thread_id()) != RETURN_STATUS_OK)
    goto err;
  if (thd->get_gtid_next_list() != NULL)
  {
#ifdef HAVE_GTID_NEXT_LIST
    thd->owned_gtid_set._add_gtid(gtid)
    thd->owned_gtid.sidno= -1;
    thd->owned_sid.clear();
#else
    DBUG_ASSERT(0);
#endif
  }
  else
  {
    thd->owned_gtid= gtid;
    thd->owned_sid= sid_map->sidno_to_sid(gtid.sidno);
  }
  RETURN_OK;
err:
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
  thd->owned_sid.clear();
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


void Gtid_state::update_on_commit(THD *thd)
{
  DBUG_ENTER("Gtid_state::update_on_commit");
  if (!thd->owned_gtid.is_null())
  {
    global_sid_lock->rdlock();
    if (!opt_bin_log || (thd->slave_thread && !opt_log_slave_updates))
    {
      Gtid *gtid= &thd->owned_gtid;
#ifndef DBUG_OFF
      lock_sidno(gtid->sidno);
      DBUG_ASSERT(!executed_gtids.contains_gtid(*gtid));
      unlock_sidno(gtid->sidno);
#endif
      /*
        If binlog is disabled, any session adds transaction owned GTID
        into global executed_gtids.
        If binlog is enabled and log_slave_updates is disabled, slave
        SQL thread or slave worker thread adds transaction owned GTID
        into global executed_gtids and gtids_only_in_table.
      */
      lock_sidno(gtid->sidno);
      DBUG_EXECUTE_IF(
        "rpl_gtid_update_on_commit_simulate_out_of_memory",
        DBUG_SET("+d,rpl_gtid_get_free_interval_simulate_out_of_memory"););
      executed_gtids._add_gtid(*gtid);
      if (thd->slave_thread && opt_bin_log && !opt_log_slave_updates)
        gtids_only_in_table._add_gtid(*gtid);
      unlock_sidno(gtid->sidno);
    }

    update_gtids_impl(thd, true);
    global_sid_lock->unlock();
  }
  DBUG_VOID_RETURN;
}


void Gtid_state::update_on_rollback(THD *thd)
{
  DBUG_ENTER("Gtid_state::update_on_rollback");

  if (!thd->owned_gtid.is_null())
  {
    if (thd->skip_gtid_rollback)
    {
      DBUG_PRINT("info",("skipping the gtid_rollback"));
      DBUG_VOID_RETURN;
    }

    global_sid_lock->rdlock();
    update_gtids_impl(thd, false);
    global_sid_lock->unlock();
  }

  DBUG_VOID_RETURN;
}


void Gtid_state::update_gtids_impl(THD *thd, bool is_commit)
{
  DBUG_ENTER("Gtid_state::update_gtids_impl");

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
      if (is_commit)
      {
        executed_gtids._add_gtid(g);
      }
    }
#else
    DBUG_ASSERT(0);
#endif
  }
  else if (thd->owned_gtid.sidno > 0)
  {
    lock_sidno(thd->owned_gtid.sidno);
    owned_gtids.remove_gtid(thd->owned_gtid);

    if (is_commit)
    {
      executed_gtids._add_gtid(thd->owned_gtid);
    }
  }

  /*
    There may be commands that cause implicit commits, e.g.
    SET AUTOCOMMIT=1 may cause the previous statements to commit
    without executing a COMMIT command or be on auto-commit mode.
  */
  broadcast_owned_sidnos(thd);
  unlock_owned_sidnos(thd);
  thd->variables.gtid_next.set_undefined();
  thd->clear_owned_gtids();

  DBUG_VOID_RETURN;
}

int Gtid_state::wait_for_gtid_set(THD* thd, String* gtid_set_text, longlong timeout)
{
  int error= 0;
  int ret_val= 0;
  struct timespec abstime;
  DBUG_ENTER("Gtid_state::wait_for_gtid_set");
  DBUG_PRINT("info", ("Waiting for %s timeout %lld", gtid_set_text->c_ptr_safe(),
             timeout));

  Sid_map sid_map(NULL);
  Gtid_set wait_gtid_set(&sid_map, NULL);
  Gtid_set executed_gtid_set(&sid_map, NULL);

  if (wait_gtid_set.add_gtid_text(gtid_set_text->c_ptr_safe()) != RETURN_STATUS_OK)
  {
    DBUG_RETURN(-1);
  }

  if (timeout > 0)
    set_timespec(&abstime, timeout);

  while (!thd->killed)
  {
    global_sid_lock->wrlock();
    const Gtid_set *executed_gtids= gtid_state->get_executed_gtids();

    if (executed_gtid_set.add_gtid_set(executed_gtids) != RETURN_STATUS_OK)
    {
      global_sid_lock->unlock();
      DBUG_RETURN(-1);
    }
    global_sid_lock->unlock();

    /*
      Removing the values from the wait_gtid_set which are already in the
      executed_gtid_set. This wait will continue till the point the
      wait_gtid_st is empty.
    */
    wait_gtid_set.remove_gtid_set(&executed_gtid_set);

    Gtid_set::Gtid_iterator git(&wait_gtid_set);
    Gtid wait_for= git.get();
    if (wait_for.sidno == 0)
      break;

    // Get the UUID to wait for.
    const rpl_sid &wait_for_uuid= sid_map.sidno_to_sid(wait_for.sidno);

    global_sid_lock->rdlock();

    // Replace the sidno relative to the local sid_map by the
    // sidno relative to global_sid_map
    wait_for.sidno= global_sid_map->add_sid(wait_for_uuid);

    gtid_state->lock_sidno(wait_for.sidno);

    /*
      the lock will be released by the wait_for_gtid function call
      when the waiting condition is met or the function timesout.
    */
    error= (timeout > 0) ? wait_for_gtid(thd, wait_for, &abstime) :
      wait_for_gtid(thd, wait_for, NULL);
    if (error == ETIMEDOUT || error == ETIME)
    {
      ret_val= 1;
      break;
    }
  }

  // return in case the query is interrupted.
  if (thd->killed)
  {
    my_error(ER_QUERY_INTERRUPTED, MYF(0));
    DBUG_RETURN(-1);
  }

  DBUG_RETURN(ret_val);
}

rpl_gno Gtid_state::get_automatic_gno(rpl_sidno sidno) const
{
  DBUG_ENTER("Gtid_state::get_automatic_gno");
  Gtid_set::Const_interval_iterator ivit(&executed_gtids, sidno);
  Gtid next_candidate= { sidno, 1 };
  while (true)
  {
    const Gtid_set::Interval *iv= ivit.get();
    rpl_gno next_interval_start= iv != NULL ? iv->start : MAX_GNO;
    while (next_candidate.gno < next_interval_start &&
           DBUG_EVALUATE_IF("simulate_gno_exhausted", false, true))
    {
      if (owned_gtids.get_owner(next_candidate) == 0)
        DBUG_RETURN(next_candidate.gno);
      next_candidate.gno++;
    }
    if (iv == NULL ||
        DBUG_EVALUATE_IF("simulate_gno_exhausted", true, false))
    {
      my_error(ER_GNO_EXHAUSTED, MYF(0));
      DBUG_RETURN(-1);
    }
    next_candidate.gno= iv->end;
    ivit.next();
  }
}


int Gtid_state::wait_for_gtid(THD *thd, const Gtid &gtid, struct timespec* timeout)
{
  DBUG_ENTER("Gtid_state::wait_for_gtid");
  int error= 0;
  PSI_stage_info old_stage;
  DBUG_PRINT("info", ("SIDNO=%d GNO=%lld owner(sidno,gno)=%u thread_id=%u",
                      gtid.sidno, gtid.gno,
                      owned_gtids.get_owner(gtid), thd->thread_id()));
  DBUG_ASSERT(owned_gtids.get_owner(gtid) != thd->thread_id());
  sid_locks.enter_cond(thd, gtid.sidno,
                       &stage_waiting_for_gtid_to_be_written_to_binary_log,
                       &old_stage);
  //while (get_owner(g.sidno, g.gno) != 0 && !thd->killed && !abort_loop)

  if (timeout)
    error= sid_locks.wait(thd, gtid.sidno, timeout);
  else
    sid_locks.wait(thd, gtid.sidno);
  thd->EXIT_COND(&old_stage);
  DBUG_RETURN(error);
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
    PROPAGATE_REPORTED_ERROR(executed_gtids.ensure_sidno(sidno));
    PROPAGATE_REPORTED_ERROR(gtids_only_in_table.ensure_sidno(sidno));
    PROPAGATE_REPORTED_ERROR(previous_gtids_logged.ensure_sidno(sidno));
    PROPAGATE_REPORTED_ERROR(lost_gtids.ensure_sidno(sidno));
    PROPAGATE_REPORTED_ERROR(owned_gtids.ensure_sidno(sidno));
    PROPAGATE_REPORTED_ERROR(sid_locks.ensure_index(sidno));
    sidno= sid_map->get_max_sidno();
    DBUG_ASSERT(executed_gtids.get_max_sidno() >= sidno);
    DBUG_ASSERT(gtids_only_in_table.get_max_sidno() >= sidno);
    DBUG_ASSERT(previous_gtids_logged.get_max_sidno() >= sidno);
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

  if (!executed_gtids.is_empty())
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
  PROPAGATE_REPORTED_ERROR(executed_gtids.add_gtid_text(text));

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


int Gtid_state::save(THD *thd)
{
  DBUG_ENTER("Gtid_state::save(THD *thd)");
  DBUG_ASSERT(gtid_table_persistor != NULL);
  DBUG_ASSERT(!thd->owned_gtid.is_null());
  int error= 0;

  int ret= gtid_table_persistor->save(thd, &thd->owned_gtid);
  if (1 == ret)
  {
    /*
      Gtid table is not ready to be used, so failed to
      open it. Ignore the error.
    */
    thd->clear_error();
    if (!thd->get_stmt_da()->is_set())
        thd->get_stmt_da()->set_ok_status(0, 0, NULL);
  }
  else if (-1 == ret)
    error= -1;

  DBUG_RETURN(error);
}


int Gtid_state::save(Gtid_set *gtid_set)
{
  DBUG_ENTER("Gtid_state::save(Gtid_set *gtid_set)");
  int ret= gtid_table_persistor->save(gtid_set);
  DBUG_RETURN(ret);
}


int Gtid_state::save_gtids_of_last_binlog_into_table(bool on_rotation)
{
  DBUG_ENTER("Gtid_state::save_gtids_of_last_binlog_into_table");
  int ret= 0;

  Gtid_set logged_gtids_last_binlog(global_sid_map, global_sid_lock);
  /*
    logged_gtids_last_binlog= executed_gtids - previous_gtids_logged -
                              gtids_only_in_table
  */
  global_sid_lock->wrlock();
  if ((ret = (logged_gtids_last_binlog.add_gtid_set(&executed_gtids) !=
              RETURN_STATUS_OK)) == 0)
  {
    logged_gtids_last_binlog.remove_gtid_set(&previous_gtids_logged);
    logged_gtids_last_binlog.remove_gtid_set(&gtids_only_in_table);
    if (!logged_gtids_last_binlog.is_empty())
    {
      /* Save set of GTIDs of the last binlog into gtid_executed table */
      ret= save(&logged_gtids_last_binlog);
      /* Prepare previous_gtids_logged for next binlog on binlog rotation */
      if (!ret && on_rotation)
        ret= previous_gtids_logged.add_gtid_set(&logged_gtids_last_binlog);
    }
  }
  global_sid_lock->unlock();

  DBUG_RETURN(ret);
}


int Gtid_state::fetch_gtids(Gtid_set *gtid_set)
{
  DBUG_ENTER("Gtid_state::fetch_gtids");
  int ret= gtid_table_persistor->fetch_gtids(gtid_set);
  DBUG_RETURN(ret);
}


int Gtid_state::compress(THD *thd)
{
  DBUG_ENTER("Gtid_state::compress");
  int ret= gtid_table_persistor->compress(thd);
  DBUG_RETURN(ret);
}
