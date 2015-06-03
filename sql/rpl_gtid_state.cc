/* Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "rpl_gtid_persist.h"      // gtid_table_persistor
#include "sql_class.h"             // THD
#include "debug_sync.h"            // DEBUG_SYNC
#include "mysqld.h"                // opt_bin_log


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
  // caller must take both global_sid_lock and lock on the SIDNO.
  global_sid_lock->assert_some_lock();
  gtid_state->assert_sidno_lock_owner(gtid.sidno);
  DBUG_ASSERT(!executed_gtids.contains_gtid(gtid));
  DBUG_PRINT("info", ("gtid=%d:%lld", gtid.sidno, gtid.gno));
  DBUG_ASSERT(thd->owned_gtid.sidno == 0);
  if (owned_gtids.add_gtid_owner(gtid, thd->thread_id()) != RETURN_STATUS_OK)
    goto err;
  if (thd->get_gtid_next_list() != NULL)
  {
#ifdef HAVE_GTID_NEXT_LIST
    thd->owned_gtid_set._add_gtid(gtid);
    thd->owned_gtid.sidno= THD::OWNED_SIDNO_GTID_SET;
    thd->owned_sid.clear();
#else
    DBUG_ASSERT(0);
#endif
  }
  else
  {
    thd->owned_gtid= gtid;
    thd->owned_gtid.dbug_print(NULL, "set owned_gtid in acquire_ownership");
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
  thd->clear_owned_gtids();
  thd->owned_gtid.dbug_print(NULL,
                             "set owned_gtid (clear) in acquire_ownership");
  RETURN_REPORTED_ERROR;
}

#ifdef HAVE_GTID_NEXT_LIST
void Gtid_state::lock_owned_sidnos(const THD *thd)
{
  if (thd->owned_gtid.sidno == THD::OWNED_SIDNO_GTID_SET)
    lock_sidnos(&thd->owned_gtid_set);
  else if (thd->owned_gtid.sidno > 0)
    lock_sidno(thd->owned_gtid.sidno);
}
#endif


void Gtid_state::unlock_owned_sidnos(const THD *thd)
{
  if (thd->owned_gtid.sidno == THD::OWNED_SIDNO_GTID_SET)
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
  if (thd->owned_gtid.sidno == THD::OWNED_SIDNO_GTID_SET)
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

  update_gtids_impl(thd, true);
  DEBUG_SYNC(thd, "end_of_gtid_state_update_on_commit");

  DBUG_VOID_RETURN;
}


void Gtid_state::update_on_rollback(THD *thd)
{
  DBUG_ENTER("Gtid_state::update_on_rollback");

  /*
    The administrative commands [CHECK|REPAIR|OPTIMIZE|ANALYZE] TABLE
    are written to the binary log even when they fail.  When the
    commands fail, they will call update_on_rollback; later they will
    write the binary log.  But we must not do any of the things in
    update_gtids_impl if we are going to write the binary log.  So
    these statements set the skip_gtid_rollback flag, which tells
    update_on_rollback to return early.  When the statements are
    written to the binary log they will call update_on_commit as
    usual.
  */
  if (thd->skip_gtid_rollback)
  {
    DBUG_PRINT("info", ("skipping gtid rollback because "
                        "thd->skip_gtid_rollback is set"));
    DBUG_VOID_RETURN;
  }

  update_gtids_impl(thd, false);

  DBUG_VOID_RETURN;
}


void Gtid_state::update_gtids_impl(THD *thd, bool is_commit)
{
  DBUG_ENTER("Gtid_state::update_gtids_impl");

  /*
    If we don't own anything, there is nothing to do, so we do an
    early return.  Except if there is a GTID consistency violation;
    then we need to decrement the counter, so then we go ahead and
    call update_gtids_impl.
  */
  if (thd->owned_gtid.is_empty() && !thd->has_gtid_consistency_violation)
  {
    thd->pending_gtid_state_update= false;
    DBUG_PRINT("info", ("skipping update_gtids_impl because "
                        "thread does not own anything and does not violate "
                        "gtid consistency"));
    DBUG_VOID_RETURN;
  }

  /*
    This variable is true for anonymous transactions, when the
    'transaction' has been split into multiple transactions in the
    binlog, and the present transaction is not the last one.

    This means two things:

    - We should not release anonymous ownership in case
      gtid_next=anonymous.  If we did, it would be possible for user
      to set GTID_MODE=ON from a concurrent transaction, making it
      impossible to commit the current transaction.

    - We should not decrease the counters for GTID-violating
      statements.  If we did, it would be possible for a concurrent
      client to set ENFORCE_GTID_CONSISTENCY=ON despite there is an
      ongoing transaction that violates GTID consistency.

    The flag is set in two cases:

     1. We are committing the statement cache when there are more
        changes in the transaction cache.

        This happens either because a single statement in the
        beginning of a transaction updates both transactional and
        non-transactional tables, or because we are committing a
        non-transactional update in the middle of a transaction when
        binlog_direct_non_transactional_updates=1.

        In this case, the flag is set further down in this function.

     2. The statement is one of the special statements that may
        generate multiple transactions: CREATE...SELECT, DROP TABLE,
        DROP DATABASE. See comment for THD::owned_gtid in
        sql/sql_class.h.

        In this case, the THD::is_commit_in_middle_of_statement flag
        is set by the caller and the flag becomes true here.
  */
  bool more_transactions_with_same_gtid_next=
    thd->is_commit_in_middle_of_statement;
  DBUG_PRINT("info", ("query='%s' thd->is_commit_in_middle_of_statement=%d",
                      thd->query().str,
                      thd->is_commit_in_middle_of_statement));

  DEBUG_SYNC(thd, "update_gtid_state_before_global_sid_lock");
  global_sid_lock->rdlock();
  DEBUG_SYNC(thd, "update_gtid_state_after_global_sid_lock");

  if (thd->owned_gtid.sidno == THD::OWNED_SIDNO_GTID_SET)
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
        executed_gtids._add_gtid(g);
    }

    if (is_commit && !thd->owned_gtid_set.is_empty())
      thd->rpl_thd_ctx.session_gtids_ctx().
        notify_after_gtid_executed_update(thd);

    thd->variables.gtid_next.set_undefined();
    thd->owned_gtid.dbug_print(NULL,
                               "set owned_gtid (clear; old was gtid_set) "
                               "in update_gtids_impl");
    thd->clear_owned_gtids();
#else
    DBUG_ASSERT(0);
#endif
  }
  else if (thd->owned_gtid.sidno > 0)
  {
    lock_sidno(thd->owned_gtid.sidno);
    DBUG_ASSERT(!executed_gtids.contains_gtid(thd->owned_gtid));
    owned_gtids.remove_gtid(thd->owned_gtid);

    if (is_commit)
    {
      DBUG_EXECUTE_IF(
        "rpl_gtid_update_on_commit_simulate_out_of_memory",
        DBUG_SET("+d,rpl_gtid_get_free_interval_simulate_out_of_memory"););
      /*
        Any session adds transaction owned GTID into global executed_gtids.

        If binlog is disabled, we report @@GLOBAL.GTID_PURGED from
        executed_gtids, since @@GLOBAL.GTID_PURGED and @@GLOBAL.GTID_EXECUTED
        are always same, so we did not save gtid into lost_gtids for every
        transaction for improving performance.

        If binlog is enabled and log_slave_updates is disabled, slave
        SQL thread or slave worker thread adds transaction owned GTID
        into global executed_gtids, lost_gtids and gtids_only_in_table.
      */
      executed_gtids._add_gtid(thd->owned_gtid);
      thd->rpl_thd_ctx.session_gtids_ctx().
        notify_after_gtid_executed_update(thd);
      if (thd->slave_thread && opt_bin_log && !opt_log_slave_updates)
      {
        lost_gtids._add_gtid(thd->owned_gtid);
        gtids_only_in_table._add_gtid(thd->owned_gtid);
      }
    }

    broadcast_owned_sidnos(thd);
    unlock_owned_sidnos(thd);

    thd->clear_owned_gtids();
    if (thd->variables.gtid_next.type == GTID_GROUP)
    {
      DBUG_ASSERT(!more_transactions_with_same_gtid_next);
      thd->variables.gtid_next.set_undefined();
    }
    else
    {
      /*
        Can be UNDEFINED for statements where
        gtid_pre_statement_checks skips the test for undefined,
        e.g. ROLLBACK.
      */
      DBUG_ASSERT(thd->variables.gtid_next.type == AUTOMATIC_GROUP ||
                  thd->variables.gtid_next.type == UNDEFINED_GROUP);
    }
  }
  else if (thd->owned_gtid.sidno == THD::OWNED_SIDNO_ANONYMOUS)
  {
    DBUG_ASSERT(thd->variables.gtid_next.type == ANONYMOUS_GROUP ||
                thd->variables.gtid_next.type == AUTOMATIC_GROUP);
    /*
      If there is more in the transaction cache, set
      more_transactions_with_same_gtid_next to indicate this.

      See comment for the declaration of
      more_transactions_with_same_gtid_next.
    */
    if (opt_bin_log)
    {
      // Needed before is_binlog_cache_empty.
      thd->binlog_setup_trx_data();
      if (!thd->is_binlog_cache_empty(true))
      {
        more_transactions_with_same_gtid_next= true;
        DBUG_PRINT("info", ("Transaction cache is non-empty: setting "
                            "more_transaction_with_same_gtid_next="
                            "true."));
      }
    }
    if (!(more_transactions_with_same_gtid_next &&
          thd->variables.gtid_next.type == ANONYMOUS_GROUP))
    {
      release_anonymous_ownership();
      thd->clear_owned_gtids();
    }
  }
  else
  {
    /*
      Nothing is owned.  There are two cases when this happens:

      - Normally, it is a rollback of an automatic transaction, so
        then is_commit is false and gtid_next=automatic.

      - There is also a corner case. If CREATE...SELECT is logged in
        row format, it gets logged as CREATE without select, followed
        by a transaction containing row events.  For this case, there
        is a commit after the table has been created, before rows have
        been selected.  After this commit,
        thd->pending_gtid_state_update is set to true by the
        CREATE...SELECT code.  This means that later, when the part of
        the statement that selects rows calls
        MYSQL_BIN_BINLOG::commit, if there were no matching rows so
        that there is nothing to commit, MYSQL_BIN_LOG::commit does a
        direct call to gtid_state->update_on_[commit|rollback] since
        other functions called from MYSQL_BIN_LOG::commit did not do
        this.  Then, if GTID_NEXT=AUTOMATIC, nothing will be owned at
        this point.  Thus, to cover this case we add the
        '||thd->pending_gtid_state_update' clause.
    */
    DBUG_ASSERT(!is_commit || thd->pending_gtid_state_update);
    DBUG_ASSERT(thd->variables.gtid_next.type == AUTOMATIC_GROUP);
  }

  global_sid_lock->unlock();

  if (!more_transactions_with_same_gtid_next)
    end_gtid_violating_transaction(thd);

  thd->owned_gtid.dbug_print(NULL,
                             "set owned_gtid (clear) in update_gtids_impl");

  thd->pending_gtid_state_update= false;

  DBUG_VOID_RETURN;
}


void Gtid_state::end_gtid_violating_transaction(THD *thd)
{
  DBUG_ENTER("end_gtid_violating_transaction");
  if (thd->has_gtid_consistency_violation)
  {
    if (thd->variables.gtid_next.type == AUTOMATIC_GROUP)
      end_automatic_gtid_violating_transaction();
    else
    {
      DBUG_ASSERT(thd->variables.gtid_next.type == ANONYMOUS_GROUP);
      end_anonymous_gtid_violating_transaction();
    }
    thd->has_gtid_consistency_violation= false;
  }
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
  DEBUG_SYNC(thd, "begin_wait_for_executed_gtid_set");

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


rpl_gno Gtid_state::get_last_executed_gno(rpl_sidno sidno) const
{
  DBUG_ENTER("Gtid:state::get_last_executed_gno");
  rpl_gno gno= 0;

  gtid_state->lock_sidno(sidno);
  gno= executed_gtids.get_last_gno(sidno);
  gtid_state->unlock_sidno(sidno);

  DBUG_RETURN(gno);
}


enum_return_status Gtid_state::generate_automatic_gtid(THD *thd,
                                                       rpl_sidno specified_sidno,
                                                       rpl_gno specified_gno)
{
  DBUG_ENTER("Gtid_state::generate_automatic_gtid");
  enum_return_status ret= RETURN_STATUS_OK;

  DBUG_ASSERT(thd->variables.gtid_next.type == AUTOMATIC_GROUP);
  DBUG_ASSERT(specified_sidno >= 0);
  DBUG_ASSERT(specified_gno >= 0);
  DBUG_ASSERT(thd->owned_gtid.is_empty());

  sid_lock->rdlock();

  // If GTID_MODE = ON_PERMISSIVE or ON, generate a new GTID
  if (get_gtid_mode(GTID_MODE_LOCK_SID) >= GTID_MODE_ON_PERMISSIVE)
  {
    Gtid automatic_gtid= { specified_sidno, specified_gno };

    if (automatic_gtid.sidno == 0)
      automatic_gtid.sidno= get_server_sidno();

    lock_sidno(automatic_gtid.sidno);

    if (automatic_gtid.gno == 0)
      automatic_gtid.gno= get_automatic_gno(automatic_gtid.sidno);

    if (automatic_gtid.gno != -1)
      acquire_ownership(thd, automatic_gtid);
    else
      ret= RETURN_STATUS_REPORTED_ERROR;

    unlock_sidno(automatic_gtid.sidno);
  }
  else
  {
    // If GTID_MODE = OFF or OFF_PERMISSIVE, just mark this thread as
    // using an anonymous transaction.
    thd->owned_gtid.sidno= THD::OWNED_SIDNO_ANONYMOUS;
    thd->owned_gtid.gno= 0;
    acquire_anonymous_ownership();
    thd->owned_gtid.dbug_print(NULL,
                               "set owned_gtid (anonymous) in generate_automatic_gtid");
  }

  sid_lock->unlock();

  gtid_set_performance_schema_values(thd);

  DBUG_RETURN(ret);
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
  // Can't call sid_locks.unlock() as that requires global_sid_lock.
  mysql_mutex_unlock(thd->current_mutex);
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


enum_return_status Gtid_state::add_lost_gtids(const Gtid_set *gtid_set)
{
  DBUG_ENTER("Gtid_state::add_lost_gtids()");
  sid_lock->assert_some_wrlock();

  gtid_set->dbug_print("add_lost_gtids");

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

  if (save(gtid_set))
    RETURN_REPORTED_ERROR;
  PROPAGATE_REPORTED_ERROR(gtids_only_in_table.add_gtid_set(gtid_set));
  PROPAGATE_REPORTED_ERROR(lost_gtids.add_gtid_set(gtid_set));
  PROPAGATE_REPORTED_ERROR(executed_gtids.add_gtid_set(gtid_set));

  DBUG_RETURN(RETURN_STATUS_OK);
}


int Gtid_state::init()
{
  DBUG_ENTER("Gtid_state::init()");

  global_sid_lock->assert_some_lock();

  rpl_sid server_sid;
  if (server_sid.parse(server_uuid) != 0)
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
  DBUG_ASSERT(thd->owned_gtid.sidno > 0);
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


int Gtid_state::save(const Gtid_set *gtid_set)
{
  DBUG_ENTER("Gtid_state::save(Gtid_set *gtid_set)");
  int ret= gtid_table_persistor->save(gtid_set);
  DBUG_RETURN(ret);
}


int Gtid_state::save_gtids_of_last_binlog_into_table(bool on_rotation)
{
  DBUG_ENTER("Gtid_state::save_gtids_of_last_binlog_into_table");
  int ret= 0;

  /*
    Use local Sid_map, so that we don't need a lock while inserting
    into the table.
  */
  Sid_map sid_map(NULL);
  Gtid_set logged_gtids_last_binlog(&sid_map, NULL);
  // Allocate some intervals on stack to reduce allocation.
  static const int PREALLOCATED_INTERVAL_COUNT= 64;
  Gtid_set::Interval iv[PREALLOCATED_INTERVAL_COUNT];
  logged_gtids_last_binlog.add_interval_memory(PREALLOCATED_INTERVAL_COUNT, iv);
  /*
    logged_gtids_last_binlog= executed_gtids - previous_gtids_logged -
                              gtids_only_in_table
  */
  global_sid_lock->wrlock();
  ret= (logged_gtids_last_binlog.add_gtid_set(&executed_gtids) !=
        RETURN_STATUS_OK);
  if (!ret)
  {
    logged_gtids_last_binlog.remove_gtid_set(&previous_gtids_logged);
    logged_gtids_last_binlog.remove_gtid_set(&gtids_only_in_table);
    if (!logged_gtids_last_binlog.is_empty())
    {
      /* Prepare previous_gtids_logged for next binlog on binlog rotation */
      if (on_rotation)
        ret= previous_gtids_logged.add_gtid_set(&logged_gtids_last_binlog);
      global_sid_lock->unlock();
      /* Save set of GTIDs of the last binlog into gtid_executed table */
      if (!ret)
        ret= save(&logged_gtids_last_binlog);
    }
    else
      global_sid_lock->unlock();
  }
  else
    global_sid_lock->unlock();

  DBUG_RETURN(ret);
}


int Gtid_state::read_gtid_executed_from_table()
{
  return gtid_table_persistor->fetch_gtids(&executed_gtids);
}


int Gtid_state::compress(THD *thd)
{
  return gtid_table_persistor->compress(thd);
}


#ifdef MYSQL_SERVER
bool Gtid_state::warn_on_modify_gtid_table(THD *thd, TABLE_LIST *table)
{
  DBUG_ENTER("Gtid_state::warn_on_modify_gtid_table");
  bool ret= gtid_table_persistor->warn_on_explicit_modification(thd, table);
  DBUG_RETURN(ret);
}
#endif
