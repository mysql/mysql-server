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

void Gtid_state::clear()
{
  DBUG_ENTER("Gtid_state::clear()");
  // the wrlock implies that no other thread can hold any of the mutexes
  sid_lock->assert_some_wrlock();
  lost_gtids.clear();
  executed_gtids.clear();
  gtids_only_in_table.clear();
  DBUG_VOID_RETURN;
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
  if (owned_gtids.add_gtid_owner(gtid, thd->thread_id) != RETURN_STATUS_OK)
    goto err2;
  if (thd->get_gtid_next_list() != NULL)
  {
#ifdef HAVE_GTID_NEXT_LIST
    if (thd->owned_gtid_set._add_gtid(gtid) != RETURN_STATUS_OK)
      goto err1;
    thd->owned_gtid.sidno= -1;
    thd->owned_sid.clear();
    /* Increase gtid count after acquired ownership of GTID. */
    gtid_counter++;
#else
    DBUG_ASSERT(0);
#endif
  }
  else
  {
    thd->owned_gtid= gtid;
    thd->owned_sid= sid_map->sidno_to_sid(gtid.sidno);
    /* Increase gtid count after acquired ownership of GTID. */
    gtid_counter++;
  }
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


enum_return_status Gtid_state::update_on_flush(THD *thd)
{
  DBUG_ENTER("Gtid_state::update_on_flush");
  enum_return_status ret= RETURN_STATUS_OK;

  // Caller must take lock on the SIDNO.
  global_sid_lock->assert_some_lock();

  /*
    There may be commands that cause implicit commits, e.g.
    SET AUTOCOMMIT=1 may cause the previous statements to commit
    without executing a COMMIT command or be on auto-commit mode.
    Although we set GTID_NEXT type to UNDEFINED on
    Gtid_state::update_on_commit(), we also set it here to do it
    as soon as possible.
  */

  if (thd->owned_gtid.sidno == -1)
  {
#ifdef HAVE_GTID_NEXT_LIST
    lock_sidnos(&thd->owned_gtid_set);
#else
    DBUG_ASSERT(0);
#endif
  }
  else if (thd->owned_gtid.sidno > 0)
    lock_sidno(thd->owned_gtid.sidno);
  thd->variables.gtid_next.set_undefined();
  broadcast_owned_sidnos(thd);
  unlock_owned_sidnos(thd);

  DBUG_RETURN(ret);
}


void Gtid_state::update_on_commit(THD *thd)
{
  DBUG_ENTER("Gtid_state::update_on_commit");
  if (!thd->owned_gtid.is_null())
  {
    global_sid_lock->rdlock();
    update_owned_gtids_impl(thd, true);
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
    /*
      Remove the gtid from executed_gtids variable if
      the transation was rollbacked.
    */
    Gtid_set *executed_gtids=
      const_cast<Gtid_set *>(get_executed_gtids());
    if (executed_gtids->contains_gtid(thd->owned_gtid))
      executed_gtids->_remove_gtid(thd->owned_gtid.sidno, thd->owned_gtid.gno);
    if (!opt_bin_log)
    {
      /*
        Remove the gtid from lost_gtids variable if the transation
        was rollbacked when binlog is disabled.
      */
      Gtid_set *lost_gtids=
        const_cast<Gtid_set *>(get_lost_gtids());
      if (lost_gtids->contains_gtid(thd->owned_gtid))
        lost_gtids->_remove_gtid(thd->owned_gtid.sidno, thd->owned_gtid.gno);
    }

    update_owned_gtids_impl(thd, false);
    global_sid_lock->unlock();
  }

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
      /* Decrease gtid count when removing GTID. */
      DBUG_ASSERT(gtid_counter > 0);
      gtid_counter--;
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
    /* Decrease gtid count when removing GTID. */
    DBUG_ASSERT(gtid_counter > 0);
    gtid_counter--;
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
  Gtid_set::Const_interval_iterator ivit(&executed_gtids, sidno);
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
    PROPAGATE_REPORTED_ERROR(executed_gtids.ensure_sidno(sidno));
    PROPAGATE_REPORTED_ERROR(gtids_only_in_table.ensure_sidno(sidno));
    PROPAGATE_REPORTED_ERROR(lost_gtids.ensure_sidno(sidno));
    PROPAGATE_REPORTED_ERROR(owned_gtids.ensure_sidno(sidno));
    PROPAGATE_REPORTED_ERROR(sid_locks.ensure_index(sidno));
    sidno= sid_map->get_max_sidno();
    DBUG_ASSERT(executed_gtids.get_max_sidno() >= sidno);
    DBUG_ASSERT(gtids_only_in_table.get_max_sidno() >= sidno);
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
  /* Save the executed_gtids into gtid table. */
  if (gtid_table_persistor->save(&executed_gtids) == -1)
    RETURN_REPORTED_ERROR;

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


int Gtid_state::generate_automatic_gtid(THD *thd)
{
  DBUG_ENTER("MYSQL_BIN_LOG::generate_gtid(THD *thd)");
  DBUG_ASSERT(thd->variables.gtid_next.type == AUTOMATIC_GROUP);
  DBUG_ASSERT(thd->variables.gtid_next_list.get_gtid_set() == NULL);
  DBUG_ASSERT(gtid_mode > GTID_MODE_UPGRADE_STEP_1);
  int error= 0;

  /* Wait until finish resetting binlog files. */
  if (opt_bin_log)
    mysql_mutex_lock(&LOCK_reset_binlog);

  global_sid_lock->rdlock();
  Gtid automatic_gtid= { 0, 0 };
  automatic_gtid.sidno= get_server_sidno();
  lock_sidno(automatic_gtid.sidno);
  automatic_gtid.gno= get_automatic_gno(automatic_gtid.sidno);
  if (automatic_gtid.gno == -1)
  {
    error= 1;
    sql_print_error("Failed to generate automatic gtid.");
  }
  else
    acquire_ownership(thd, automatic_gtid);
  unlock_sidno(automatic_gtid.sidno);
  global_sid_lock->unlock();
  if (opt_bin_log)
    mysql_mutex_unlock(&LOCK_reset_binlog);

  DBUG_RETURN(error);
}


int Gtid_state::save_gtid_into_table(THD *thd)
{
  DBUG_ENTER("Gtid_state::save_gtid_into_table");
  DBUG_ASSERT(gtid_table_persistor != NULL);
  int error= 0;

  Gtid *gtid= &thd->owned_gtid;
  Gtid_set *executed_gtids= const_cast<Gtid_set *>(get_executed_gtids());
  global_sid_lock->wrlock();
  if (!executed_gtids->contains_gtid(*gtid))
  {
    int err= 0;
    if (!(err= executed_gtids->ensure_sidno(gtid->sidno)))
      err |= executed_gtids->_add_gtid(*gtid);
    /*
      Add every transaction's gtid into global lost_gtids if
      binlog is disabled and gtid_mode is enabled.
    */
    if (!opt_bin_log)
    {
      Gtid_set *lost_gtids= const_cast<Gtid_set *>(get_lost_gtids());
      if (!err && !(err= lost_gtids->ensure_sidno(gtid->sidno)))
        err |= lost_gtids->_add_gtid(*gtid);
    }
    global_sid_lock->unlock();
    if (!err)
    {
      int ret= gtid_table_persistor->save(gtid);
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
        error= 1;
    }
    else
      error= 1;
  }
  else
  {
    global_sid_lock->unlock();
    DBUG_ASSERT(0);
  }

  DBUG_RETURN(error);
}


int Gtid_state::generate_and_save_gtid(THD *thd)
{
  DBUG_ENTER("MYSQL_BIN_LOG::generate_and_save_gtid(THD *thd)");
  int error= 0;
  bool is_gtid_generated= false;

  /* Generate gtid for transaction. */
  if (thd->owned_gtid.is_null() &&
      thd->variables.gtid_next.type == AUTOMATIC_GROUP)
  {
    if (!(error= generate_automatic_gtid(thd)))
      is_gtid_generated= true;
  }

  /* Save gtid into mysql.gtid_executed table. */
  if (is_gtid_generated || thd->variables.gtid_next.type == GTID_GROUP)
    error= save_gtid_into_table(thd);

  DBUG_RETURN(error);
}
