/* Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "debug_sync.h"
#include "rpl_mts_submode.h"

#include "hash.h"                           // HASH
#include "log.h"                            // sql_print_information
#include "log_event.h"                      // Query_log_event
#include "rpl_rli.h"                        // Relay_log_info
#include "rpl_rli_pdb.h"                    // db_worker_hash_entry
#include "rpl_slave_commit_order_manager.h" // Commit_order_manager
#include "sql_class.h"                      // THD


/**
 Does necessary arrangement before scheduling next event.
 @param:  Relay_log_info rli
 @return: 1  if  error
          0 no error
*/
int
Mts_submode_database::schedule_next_event(Relay_log_info *rli, Log_event *ev)
{
  /*nothing to do here*/
  return 0;
}

/**
  Logic to attach temporary tables.
  @param: THD thd
          Relay_log_info rli
          Query_log_event ev
  @return: void
*/
void
Mts_submode_database::attach_temp_tables(THD *thd, const Relay_log_info* rli,
                                         Query_log_event* ev)
{
  int i, parts;
  DBUG_ENTER("Mts_submode_database::attach_temp_tables");
  if (!is_mts_worker(thd) || (ev->ends_group() || ev->starts_group()))
    DBUG_VOID_RETURN;
  DBUG_ASSERT(!thd->temporary_tables);
  // in over max-db:s case just one special partition is locked
  parts= ((ev->mts_accessed_dbs == OVER_MAX_DBS_IN_EVENT_MTS) ? 1 :
           ev->mts_accessed_dbs);
  for (i= 0; i < parts; i++)
  {
    mts_move_temp_tables_to_thd(thd,
                                ev->mts_assigned_partitions[i]->temporary_tables);
    ev->mts_assigned_partitions[i]->temporary_tables= NULL;
  }
  DBUG_VOID_RETURN;
}

/**
   Function is called by Coordinator when it identified an event
   requiring sequential execution.
   Creating sequential context for the event includes waiting
   for the assigned to Workers tasks to be completed and their
   resources such as temporary tables be returned to Coordinator's
   repository.
   In case all workers are waited Coordinator changes its group status.

   @param  rli     Relay_log_info instance of Coordinator
   @param  ignore  Optional Worker instance pointer if the sequential context
                   is established due for the ignore Worker. Its resources
                   are to be retained.

   @note   Resources that are not occupied by Workers such as
           a list of temporary tables held in unused (zero-usage) records
           of APH are relocated to the Coordinator placeholder.

   @return non-negative number of released by Workers partitions
           (one partition by one Worker can count multiple times)

           or -1 to indicate there has been a failure on a not-ignored Worker
           as indicated by its running_status so synchronization can't succeed.
*/

int
Mts_submode_database::wait_for_workers_to_finish(Relay_log_info *rli,
                                                 Slave_worker *ignore)
{
  uint ret= 0;
  HASH *hash= &rli->mapping_db_to_worker;
  THD *thd= rli->info_thd;
  bool cant_sync= FALSE;
  char llbuf[22];

  DBUG_ENTER("Mts_submode_database::wait_for_workers_to_finish");

  llstr(rli->get_event_relay_log_pos(), llbuf);
  DBUG_PRINT("info", ("Coordinator and workers enter synchronization "
                      "procedure when scheduling event relay-log: %s "
                      "pos: %s", rli->get_event_relay_log_name(), llbuf));

  for (uint i= 0, ret= 0; i < hash->records; i++)
  {
    db_worker_hash_entry *entry;

    mysql_mutex_lock(&rli->slave_worker_hash_lock);

    entry= (db_worker_hash_entry*) my_hash_element(hash, i);

    DBUG_ASSERT(entry);

    // the ignore Worker retains its active resources
    if (ignore && entry->worker == ignore && entry->usage > 0)
    {
      mysql_mutex_unlock(&rli->slave_worker_hash_lock);
      continue;
    }

    if (entry->usage > 0 && !thd->killed)
    {
      PSI_stage_info old_stage;
      Slave_worker *w_entry= entry->worker;

      entry->worker= NULL; // mark Worker to signal when  usage drops to 0
      thd->ENTER_COND(&rli->slave_worker_hash_cond,
                      &rli->slave_worker_hash_lock,
                      &stage_slave_waiting_worker_to_release_partition,
                      &old_stage);
      do
      {
        mysql_cond_wait(&rli->slave_worker_hash_cond, &rli->slave_worker_hash_lock);
        DBUG_PRINT("info",
                   ("Either got awakened of notified: "
                    "entry %p, usage %lu, worker %lu",
                    entry, entry->usage, w_entry->id));
      } while (entry->usage != 0 && !thd->killed);
      entry->worker= w_entry; // restoring last association, needed only for assert
      mysql_mutex_unlock(&rli->slave_worker_hash_lock);
      thd->EXIT_COND(&old_stage);
      ret++;
    }
    else
    {
      mysql_mutex_unlock(&rli->slave_worker_hash_lock);
    }
    // resources relocation
    mts_move_temp_tables_to_thd(thd, entry->temporary_tables);
    entry->temporary_tables= NULL;
    if (entry->worker->running_status != Slave_worker::RUNNING)
      cant_sync= TRUE;
  }

  if (!ignore)
  {
    DBUG_PRINT("info", ("Coordinator synchronized with workers, "
                        "waited entries: %d, cant_sync: %d",
                        ret, cant_sync));

    rli->mts_group_status= Relay_log_info::MTS_NOT_IN_GROUP;
  }

  DBUG_RETURN(!cant_sync ? ret : -1);
}

/**
 Logic to detach the temporary tables from the worker threads upon
 event execution
 @param: thd THD instance
         rli Relay_log_info instance
         ev  Query_log_event that is being applied
 @return: void
 */
void
Mts_submode_database::detach_temp_tables(THD *thd, const Relay_log_info* rli,
                                         Query_log_event *ev)
{
  int i, parts;
  DBUG_ENTER("Mts_submode_database::detach_temp_tables");
  if (!is_mts_worker(thd))
    DBUG_VOID_RETURN;
  parts= ((ev->mts_accessed_dbs == OVER_MAX_DBS_IN_EVENT_MTS) ?
              1 : ev->mts_accessed_dbs);
  /*
    todo: optimize for a case of

    a. one db
       Only detaching temporary_tables from thd to entry would require
       instead of the double-loop below.

    b. unchanged thd->temporary_tables.
       In such case the involved entries would continue to hold the
       unmodified lists provided that the attach_ method does not
       destroy references to them.
  */
  for (i= 0; i < parts; i++)
  {
    ev->mts_assigned_partitions[i]->temporary_tables= NULL;
  }
  for (TABLE *table= thd->temporary_tables; table;)
  {
    int i;
    char *db_name= NULL;

    // find which entry to go
    for (i= 0; i < parts; i++)
    {
      db_name= ev->mts_accessed_db_names[i];
      if (!strlen(db_name))
        break;
      // Only default database is rewritten.
      if (!rpl_filter->is_rewrite_empty() && !strcmp(ev->get_db(), db_name))
      {
        size_t dummy_len;
        const char *db_filtered= rpl_filter->get_rewrite_db(db_name, &dummy_len);
        // db_name != db_filtered means that db_name is rewritten.
        if (strcmp(db_name, db_filtered))
          db_name= (char*)db_filtered;
      }
      if (strcmp(table->s->db.str, db_name) < 0)
        continue;
      else
      {
        // When rewrite db rules are used we can not rely on
        // mts_accessed_db_names elements order.
        if (!rpl_filter->is_rewrite_empty() &&
            strcmp(table->s->db.str, db_name))
          continue;
        else
          break;
      }
    }
    DBUG_ASSERT(db_name && (
                !strcmp(table->s->db.str, db_name) ||
                !strlen(db_name))
                );
    DBUG_ASSERT(i < ev->mts_accessed_dbs);
    // table pointer is shifted inside the function
    table= mts_move_temp_table_to_entry(table, thd, ev->mts_assigned_partitions[i]);
  }

  DBUG_ASSERT(!thd->temporary_tables);
#ifndef DBUG_OFF
  for (int i= 0; i < parts; i++)
  {
    DBUG_ASSERT(!ev->mts_assigned_partitions[i]->temporary_tables ||
                !ev->mts_assigned_partitions[i]->temporary_tables->prev);
  }
#endif
  DBUG_VOID_RETURN;
}

/**
  Logic to get least occupied worker when the sql mts_submode= database
  @param
    rli relay log info of coordinator
    ws arrayy of worker threads
    ev event for which we are searching for a worker.
  @return slave worker thread
 */
Slave_worker *
Mts_submode_database::get_least_occupied_worker(Relay_log_info *rli,
                                                Slave_worker_array *ws,
                                                Log_event *ev)
{
 long usage= LONG_MAX;
  Slave_worker **ptr_current_worker= NULL, *worker= NULL;

  DBUG_ENTER("Mts_submode_database::get_least_occupied_worker");

#ifndef DBUG_OFF

  if (DBUG_EVALUATE_IF("mts_distribute_round_robin", 1, 0))
  {
    worker= ws->at(w_rr % ws->size());
    sql_print_information("Chosing worker id %lu, the following is"
                          " going to be %lu", worker->id,
                          static_cast<ulong>(w_rr % ws->size()));
    DBUG_ASSERT(worker != NULL);
    DBUG_RETURN(worker);
  }
#endif

  for (Slave_worker **it= ws->begin(); it != ws->end(); ++it)
  {
    ptr_current_worker= it;
    if ((*ptr_current_worker)->usage_partition <= usage)
    {
      worker= *ptr_current_worker;
      usage= (*ptr_current_worker)->usage_partition;
    }
  }
  DBUG_ASSERT(worker != NULL);
  DBUG_RETURN(worker);
}

/* MTS submode master Default constructor */
Mts_submode_logical_clock::Mts_submode_logical_clock()
{
  type= MTS_PARALLEL_TYPE_LOGICAL_CLOCK;
  first_event= true;
  force_new_group= false;
  is_new_group= true;
  delegated_jobs = 0;
  jobs_done= 0;
  last_lwm_timestamp= SEQ_UNINIT;
  last_lwm_index= INDEX_UNDEF;
  is_error= false;
  min_waited_timestamp= SEQ_UNINIT;
  last_committed= SEQ_UNINIT;
  sequence_number= SEQ_UNINIT;
}

/**
   The method finds the minimum logical timestamp (low-water-mark) of
   committed transactions.
   The successful search results in a pair of a logical timestamp value and a GAQ
   index that contains it. last_lwm_timestamp may still be raised though
   the search does not find any satisfying running index.
   Search is implemented as headway scanning of GAQ from a point of a
   previous search's stop position (last_lwm_index).
   Whether the cached (memorized) index value is considered to be stale
   when its timestamp gets less than the current "stable" LWM:

        last_lwm_timestamp <= GAQ.lwm.sequence_number           (*)

   Staleness is caused by GAQ garbage collection that increments the rhs of (*),
   see ::move_queue_head(). When that's diagnozed, the search in GAQ needs
   restarting from the queue tail.

   Formally, the undefined cached value of last_lwm_timestamp is also stale.

              the last time index containg lwm
                  +------+
                  | LWM  |
                  |  |   |
                  V  V   V
   GAQ:   xoooooxxxxxXXXXX...X
                ^   ^
                |   | LWM+1
                |
                +- tne new current_lwm

         <---- logical (commit) time ----

   here `x' stands for committed, `X' for committed and discarded from
   the running range of the queue, `o' for not committed.

   @param  rli         Relay_log_info pointer
   @param  need_look   Either the caller or the function must hold a mutex
                       to avoid race with concurrent GAQ update.

   @return possibly updated current_lwm
*/
longlong Mts_submode_logical_clock::get_lwm_timestamp(Relay_log_info *rli,
                                                      bool need_lock)
{
  longlong lwm_estim;
  Slave_job_group* ptr_g;
  bool is_stale= false;

  if (!need_lock)
    mysql_mutex_lock(&rli->mts_gaq_LOCK);

  /*
    Make the "stable" LWM-based estimate which will be compared
    against the cached "instant" value.
  */
  lwm_estim= rli->gaq->lwm.sequence_number;
  /*
    timestamp continuity invariant: if the queue has any item
    its timestamp is greater on one than the estimate.
  */
  DBUG_ASSERT(lwm_estim == SEQ_UNINIT || rli->gaq->empty() ||
              lwm_estim + 1 ==
              rli->gaq->get_job_group(rli->gaq->entry)->sequence_number);

  last_lwm_index=
    rli->gaq->find_lwm(&ptr_g,
                       /*
                         The underfined "stable" forces the scan's restart
                         as the stale value does.
                       */
                       lwm_estim == SEQ_UNINIT ||
                       (is_stale= clock_leq(last_lwm_timestamp, lwm_estim)) ?
                       rli->gaq->entry :
                       last_lwm_index);
  /*
    if the returned index is sane update the timestamp.
  */
  if (last_lwm_index != rli->gaq->size)
  {
    // non-decreasing lwm invariant
    DBUG_ASSERT(clock_leq(last_lwm_timestamp, ptr_g->sequence_number));

    last_lwm_timestamp= ptr_g->sequence_number;
  }
  else if (is_stale)
  {
    my_atomic_store64(&last_lwm_timestamp, lwm_estim);
  }

  if (!need_lock)
    mysql_mutex_unlock(&rli->mts_gaq_LOCK);

  return last_lwm_timestamp;
};

/**
   The method implements logical timestamp conflict detection
   and resolution through waiting by the calling thread.
   The conflict or waiting condition is like the following

           lwm < last_committed,

   where lwm is a minimum logical timestamp of committed transactions.
   Since the lwm's exact value is not always available its pessimistic
   estimate (an old version) is improved (get_lwm_timestamp()) as the
   first step before to the actual waiting commitment.

   Special cases include:

   When @c last_committed_arg is uninitialized the calling thread must
   proceed without waiting for anyone. Any possible dependency with unknown
   commit parent transaction shall be sorted out by the parent;

   When the gaq index is subsequent to the last lwm index
   there's no dependency of the current transaction with any regardless of
   lwm timestamp should it be SEQ_UNINIT.
   Consequently when GAQ consists of just one item there's none to wait.
   Such latter case is left to the caller to handle.

   @note The caller must make sure the current transaction won't be waiting
         for itself. That is the method should not be caller by a Worker
         whose group assignment is in the GAQ front item.

   @param last_committed_arg  logical timestamp of a parent transaction
   @param gaq_index           Index of the current transaction in GAQ
   @return false as success,
           true  when the error flag is raised or
                 the caller thread is found killed.
*/
bool Mts_submode_logical_clock::
wait_for_last_committed_trx(Relay_log_info* rli,
                            longlong last_committed_arg,
                            longlong lwm_estimate_arg)
{
  THD* thd= rli->info_thd;

  DBUG_ENTER("Mts_submode_logical_clock::wait_for_last_committed_trx");

  if (last_committed_arg == SEQ_UNINIT)
    DBUG_RETURN(false);

  mysql_mutex_lock(&rli->mts_gaq_LOCK);

  DBUG_ASSERT(min_waited_timestamp == SEQ_UNINIT);

  my_atomic_store64(&min_waited_timestamp, last_committed_arg);
  /*
    This transaction is a candidate for insertion into the waiting list.
    That fact is descibed by incrementing waited_timestamp_cnt.
    When the candidate won't make it the counter is decremented at once
    while the mutex is hold.
  */
  if ((!rli->info_thd->killed && !is_error) &&
      !clock_leq(last_committed_arg, get_lwm_timestamp(rli, true)))
  {
    PSI_stage_info old_stage;
    struct timespec ts[2];
    set_timespec_nsec(&ts[0], 0);

    DBUG_ASSERT(rli->gaq->len >= 2); // there's someone to wait

    thd->ENTER_COND(&rli->logical_clock_cond, &rli->mts_gaq_LOCK,
                    &stage_worker_waiting_for_commit_parent, &old_stage);
    do
    {
      mysql_cond_wait(&rli->logical_clock_cond, &rli->mts_gaq_LOCK);
    }
    while ((!rli->info_thd->killed && !is_error) &&
           !clock_leq(last_committed_arg, estimate_lwm_timestamp()));
    my_atomic_store64(&min_waited_timestamp, SEQ_UNINIT);  // reset waiting flag
    mysql_mutex_unlock(&rli->mts_gaq_LOCK);
    thd->EXIT_COND(&old_stage);
    set_timespec_nsec(&ts[1], 0);
    my_atomic_add64(&rli->mts_total_wait_overlap, diff_timespec(&ts[1], &ts[0]));
  }
  else
  {
    my_atomic_store64(&min_waited_timestamp, SEQ_UNINIT);
    mysql_mutex_unlock(&rli->mts_gaq_LOCK);
  }

  DBUG_RETURN(rli->info_thd->killed || is_error);
}

/**
 Does necessary arrangement before scheduling next event.
 The method computes the meta-group status of the being scheduled
 transaction represented by the event argument. When the status
 is found OUT (of the current meta-group) as encoded as is_new_group == true
 the global Scheduler (Coordinator thread) requests full synchronization
 with all Workers.
 The current being assigned group descriptor gets associated with
 the group's logical timestamp aka sequence_number.

 @param:  Relay_log_info* rli
          Log_event *ev
 @return: ER_MTS_CANT_PARALLEL, ER_MTS_INCONSISTENT_DATA
          0 if no error or slave has been killed gracefully
 */
int
Mts_submode_logical_clock::schedule_next_event(Relay_log_info* rli,
                                               Log_event *ev)
{
  longlong last_sequence_number= sequence_number;
  bool gap_successor= false;

  DBUG_ENTER("Mts_submode_logical_clock::schedule_next_event");
  // We should check if the SQL thread was already killed before we schedule
  // the next transaction
  if (sql_slave_killed(rli->info_thd, rli))
    DBUG_RETURN(0);

  Slave_job_group *ptr_group=
    rli->gaq->get_job_group(rli->gaq->assigned_group_index);
  /*
    A group id updater must satisfy the following:
    - A query log event ("BEGIN" ) or a GTID EVENT
    - A DDL or an implicit DML commit.
  */
  switch (ev->get_type_code())
  {
  case binary_log::GTID_LOG_EVENT:
  case binary_log::ANONYMOUS_GTID_LOG_EVENT:
    // TODO: control continuity
    ptr_group->sequence_number= sequence_number=
      static_cast<Gtid_log_event*>(ev)->sequence_number;
    ptr_group->last_committed= last_committed=
      static_cast<Gtid_log_event*>(ev)->last_committed;
    break;

  default:

    sequence_number= last_committed= SEQ_UNINIT;

    break;
  }

  DBUG_PRINT("info", ("sequence_number %lld, last_committed %lld",
                      sequence_number, last_committed));

  if (first_event)
  {
    first_event= false;
  }
  else
  {
    if (unlikely(clock_leq(sequence_number, last_committed) &&
                 last_committed != SEQ_UNINIT))
    {
      /* inconsistent (buggy) timestamps */
      sql_print_error("Transaction is tagged with inconsistent logical "
                      "timestamps: "
                      "sequence_number (%lld) <= last_committed (%lld)",
                      sequence_number, last_committed);
      DBUG_RETURN(ER_MTS_CANT_PARALLEL);
    }
    if (unlikely(clock_leq(sequence_number, last_sequence_number) &&
                 sequence_number != SEQ_UNINIT))
    {
      /* inconsistent (buggy) timestamps */
      sql_print_error("Transaction's sequence number is inconsistent with that "
                      "of a preceding one: "
                      "sequence_number (%lld) <= previous sequence_number (%lld)",
                      sequence_number, last_sequence_number);
      DBUG_RETURN(ER_MTS_CANT_PARALLEL);
    }
    /*
      Being scheduled transaction sequence may have gaps, even in
      relay log. In such case a transaction that succeeds a gap will
      wait for all ealier that were scheduled to finish. It's marked
      as gap successor now.
    */
    compile_time_assert(SEQ_UNINIT == 0);
    if (unlikely(sequence_number > last_sequence_number + 1))
    {
      DBUG_PRINT("info", ("sequence_number gap found, "
                          "last_sequence_number %lld, sequence_number %lld",
                          last_sequence_number, sequence_number));
      DBUG_ASSERT(rli->replicate_same_server_id || true /* TODO: account autopositioning */);
      gap_successor= true;
    }
  }

  /*
    The new group flag is practically the same as the force flag
    when up to indicate syncronization with Workers.
  */
  is_new_group=
    (/* First event after a submode switch; */
     first_event ||
     /* Require a fresh group to be started; */
     // todo: turn `force_new_group' into sequence_number == SEQ_UNINIT condition
     force_new_group ||
     /* Rewritten event without commit point timestamp (todo: find use case) */
     sequence_number == SEQ_UNINIT ||
     /*
       undefined parent (e.g the very first trans from the master),
       or old master.
     */
     last_committed == SEQ_UNINIT ||
     /*
       When gap successor depends on a gap before it the scheduler has
       to serialize this transaction execution with previously
       scheduled ones. Below for simplicity it's assumed that such
       gap-dependency is always the case.
     */
     gap_successor ||
     /*
       previous group did not have sequence number assigned.
       It's execution must be finished until the current group
       can be assigned.
       Dependency of the current group on the previous
       can't be tracked. So let's wait till the former is over.
     */
     last_sequence_number == SEQ_UNINIT);
  /*
    The coordinator waits till all transactions on which the current one
    depends on are applied.
  */
  if (!is_new_group)
  {
    longlong lwm_estimate= estimate_lwm_timestamp();

    if (!clock_leq(last_committed, lwm_estimate) &&
        rli->gaq->assigned_group_index != rli->gaq->entry)
    {
      /*
        "Unlikely" branch.

        The following block improves possibly stale lwm and when the
        waiting condition stays, recompute min_waited_timestamp and go
        waiting.
        At awakening set min_waited_timestamp to commit_parent in the
        subsequent GAQ index (could be NIL).
      */
      if (wait_for_last_committed_trx(rli, last_committed, lwm_estimate))
      {
        /*
          MTS was waiting for a dependent transaction to finish but either it
          has failed or the applier was requested to stop. In any case, this
          transaction wasn't started yet and should not warn about the
          coordinator stopping in a middle of a transaction to avoid polluting
          the server error log.
        */
        rli->reported_unsafe_warning= true;
        DBUG_RETURN(-1);
      }
      /*
        Making the slave's max last committed (lwm) to satisfy this
        transaction's scheduling condition.
      */
      if (gap_successor)
        last_lwm_timestamp= sequence_number - 1;
      DBUG_ASSERT(!clock_leq(sequence_number, estimate_lwm_timestamp()));
    }

    delegated_jobs++;

    DBUG_ASSERT(!force_new_group);
  }
  else
  {
    DBUG_ASSERT(delegated_jobs >= jobs_done);
    DBUG_ASSERT(is_error || (rli->gaq->len + jobs_done == 1 + delegated_jobs));
    DBUG_ASSERT(rli->mts_group_status == Relay_log_info::MTS_IN_GROUP);

    /*
      Under the new group fall the following use cases:
      - events from an OLD (sequence_number unaware) master;
      - malformed (missed BEGIN or GTID_NEXT) group incl. its
        particular form of CREATE..SELECT..from..@user_var (or rand- and
        int- var in place of @user- var).
        The malformed group is handled exceptionally each event is executed
        as a solitary group yet by the same (zero id) worker.
    */
    if (-1 == wait_for_workers_to_finish(rli))
      DBUG_RETURN (ER_MTS_INCONSISTENT_DATA);

    rli->mts_group_status= Relay_log_info::MTS_IN_GROUP; //wait set it to NOT
    DBUG_ASSERT(min_waited_timestamp == SEQ_UNINIT);
    /*
      the instant last lwm timestamp must reset when force flag is up.
    */
    rli->gaq->lwm.sequence_number= last_lwm_timestamp= SEQ_UNINIT;
    delegated_jobs= 1;
    jobs_done= 0;
    force_new_group= false;
    /*
      Not sequenced event can be followed with a logically relating
      e.g User var to be followed by CREATE table.
      It's supported to be executed in one-by-one fashion.
      Todo: remove with the event group parser worklog.
    */
    if (sequence_number == SEQ_UNINIT && last_committed == SEQ_UNINIT)
      rli->last_assigned_worker= *rli->workers.begin();
  }

#ifndef DBUG_OFF
  mysql_mutex_lock(&rli->mts_gaq_LOCK);
  DBUG_ASSERT(is_error || (rli->gaq->len + jobs_done == delegated_jobs));
  mysql_mutex_unlock(&rli->mts_gaq_LOCK);
#endif
  DBUG_RETURN(0);
}

/**
 Logic to attach the temporary tables from the worker threads upon
 event execution
 @param: thd THD instance
         rli Relay_log_info instance
         ev  Query_log_event that is being applied
 @return: void
 */
void
Mts_submode_logical_clock::attach_temp_tables(THD *thd, const Relay_log_info* rli,
                                       Query_log_event * ev)
{
  bool shifted= false;
  TABLE *table, *cur_table;
  DBUG_ENTER("Mts_submode_logical_clock::attach_temp_tables");
  if (!is_mts_worker(thd) || (ev->ends_group() || ev->starts_group()))
    DBUG_VOID_RETURN;
  /* fetch coordinator's rli */
  Relay_log_info *c_rli= static_cast<const Slave_worker *>(rli)->c_rli;
  DBUG_ASSERT(!thd->temporary_tables);
  mysql_mutex_lock(&c_rli->mts_temp_table_LOCK);
  if (!(table= c_rli->info_thd->temporary_tables))
  {
    mysql_mutex_unlock(&c_rli->mts_temp_table_LOCK);
    DBUG_VOID_RETURN;
  }
  c_rli->info_thd->temporary_tables= 0;
  do
  {
    /* store the current table */
    cur_table= table;
    /* move the table pointer to next in list, so that we can isolate the
    current table */
    table= table->next;
    std::pair<uint, my_thread_id> st_id_pair= get_server_and_thread_id(cur_table);
    if (thd->server_id == st_id_pair.first  &&
        thd->variables.pseudo_thread_id == st_id_pair.second)
    {
      /* short the list singling out the current table */
      if (cur_table->prev) //not the first node
        cur_table->prev->next= cur_table->next;
      if (cur_table->next) //not the last node
        cur_table->next->prev= cur_table->prev;
      /* isolate the table */
      cur_table->prev= NULL;
      cur_table->next= NULL;
      mts_move_temp_tables_to_thd(thd, cur_table);
    }
    else
      /* We must shift the C->temp_table pointer to the fist table unused in
         this iteration. If all the tables have ben used C->temp_tables will
         point to NULL */
      if (!shifted)
      {
        c_rli->info_thd->temporary_tables= cur_table;
        shifted= true;
      }
  } while(table);
  mysql_mutex_unlock(&c_rli->mts_temp_table_LOCK);
  DBUG_VOID_RETURN;
}

/**
 Logic to detach the temporary tables from the worker threads upon
 event execution
 @param: thd THD instance
         rli Relay_log_info instance
         ev  Query_log_event that is being applied
 @return: void
 */
void
Mts_submode_logical_clock::detach_temp_tables( THD *thd, const Relay_log_info* rli,
                                        Query_log_event * ev)
{
  DBUG_ENTER("Mts_submode_logical_clock::detach_temp_tables");
  if (!is_mts_worker(thd))
    DBUG_VOID_RETURN;
  /*
    Here in detach section we will move the tables from the worker to the
    coordinaor thread. Since coordinator is shared we need to make sure that
    there are no race conditions which may lead to assert failures and
    non-deterministic results.
  */
  Relay_log_info *c_rli= static_cast<const Slave_worker *>(rli)->c_rli;
  mysql_mutex_lock(&c_rli->mts_temp_table_LOCK);
  mts_move_temp_tables_to_thd(c_rli->info_thd, thd->temporary_tables);
  mysql_mutex_unlock(&c_rli->mts_temp_table_LOCK);
  thd->temporary_tables= 0;
  DBUG_VOID_RETURN;
}

/**
  Logic to get least occupied worker when the sql mts_submode= master_parallel
  @param
    rli relay log info of coordinator
    ws arrayy of worker threads
    ev event for which we are searching for a worker.
  @return slave worker thread or NULL when coordinator is killed by any worker.
 */

Slave_worker *
Mts_submode_logical_clock::get_least_occupied_worker(Relay_log_info *rli,
                                                     Slave_worker_array *ws,
                                                     Log_event * ev)
{
  Slave_worker *worker= NULL;
  PSI_stage_info *old_stage= 0;
  THD* thd= rli->info_thd;
  DBUG_ENTER("Mts_submode_logical_clock::get_least_occupied_worker");
#ifndef DBUG_OFF

  if (DBUG_EVALUATE_IF("mts_distribute_round_robin", 1, 0))
  {
    worker= ws->at(w_rr % ws->size());
    sql_print_information("Chosing worker id %lu, the following is"
                          " going to be %lu", worker->id,
                          static_cast<ulong>(w_rr % ws->size()));
    DBUG_ASSERT(worker != NULL);
    DBUG_RETURN(worker);
  }
  Slave_committed_queue *gaq= rli->gaq;
  Slave_job_group* ptr_group;
  ptr_group= gaq->get_job_group(rli->gaq->assigned_group_index);
#endif
  /*
    The scheduling works as follows, in this sequence
      -If this is an internal event of a transaction  use the last assigned
        worker
      -If the i-th transaction is being scheduled in this group where "i" <=
       number of available workers then schedule the events to the consecutive
       workers
      -If the i-th transaction is being scheduled in this group where "i" >
       number of available workers then schedule this to the forst worker that
       becomes free.
   */
  if (rli->last_assigned_worker)
  {
    worker= rli->last_assigned_worker;
    DBUG_ASSERT(ev->get_type_code() != binary_log::USER_VAR_EVENT || worker->id == 0 ||
                rli->curr_group_seen_begin || rli->curr_group_seen_gtid);
  }
  else
  {
    worker= get_free_worker(rli);

    DBUG_ASSERT(ev->get_type_code() != binary_log::USER_VAR_EVENT ||
                rli->curr_group_seen_begin || rli->curr_group_seen_gtid);

    if (worker == NULL)
    {
      struct timespec ts[2];

      set_timespec_nsec(&ts[0], 0);
      // Update thd info as waiting for workers to finish.
      thd->enter_stage(&stage_slave_waiting_for_workers_to_process_queue,
                       old_stage,
                       __func__, __FILE__, __LINE__);
      while (!worker && !thd->killed)
      {
        /*
          Busy wait with yielding thread control before to next attempt
          to find a free worker. As of current, a worker
          can't have more than one assigned group of events in its
          queue.

          todo: replace this At-Most-One assignment policy with
                First Available Worker as
                this method clearly can't be considered as optimal.
        */
#if !defined(_WIN32)
        sched_yield();
#else
        my_sleep(rli->mts_coordinator_basic_nap);
#endif
        worker= get_free_worker(rli);
      }
      THD_STAGE_INFO(thd, *old_stage);
      set_timespec_nsec(&ts[1], 0);
      rli->mts_total_wait_worker_avail += diff_timespec(&ts[1], &ts[0]);
      rli->mts_wq_no_underrun_cnt++;
      /*
        Even OPTION_BEGIN is set, the 'BEGIN' event is not dispatched to
        any worker thread. So The flag is removed and Coordinator thread
        will not try to finish the group before abort.
      */
      if (worker == NULL)
        rli->info_thd->variables.option_bits&= ~OPTION_BEGIN;
    }
    if (rli->get_commit_order_manager() != NULL && worker != NULL)
      rli->get_commit_order_manager()->register_trx(worker);
  }

  DBUG_ASSERT(ptr_group);
  // assert that we have a worker thread for this event or the slave has
  // stopped.
  DBUG_ASSERT(worker != NULL || thd->killed);
  /* The master my have send  db partition info. make sure we never use them*/
  if (ev->get_type_code() == binary_log::QUERY_EVENT)
    static_cast<Query_log_event*>(ev)->mts_accessed_dbs= 0;

  DBUG_RETURN(worker);
}

/**
  Protected method to fetch a worker having no events assigned.
  The method is supposed to be called by Coordinator, therefore
  comparison like w_i->jobs.len == 0 must (eventually) succeed.

  todo: consider to optimize scan that is getting more expensive with
  more # of Workers.

  @return  a pointer to Worker or NULL if none is free.
*/
Slave_worker*
Mts_submode_logical_clock::get_free_worker(Relay_log_info *rli)
{
  for (Slave_worker **it= rli->workers.begin(); it != rli->workers.end(); ++it)
  {
    Slave_worker *w_i= *it;
    if (w_i->jobs.len == 0)
      return w_i;
  }
  return 0;
}

/**
  Waits for slave workers to finish off the pending tasks before returning.
  Used in this submode to make sure that all assigned jobs have been done.

  @param Relay_log info *rli  coordinator rli.
  @param Slave worker to ignore.
  @return -1 for error.
           0 no error.
 */
int
Mts_submode_logical_clock::
   wait_for_workers_to_finish(Relay_log_info *rli,
                              MY_ATTRIBUTE((unused)) Slave_worker * ignore)
{
  PSI_stage_info *old_stage= 0;
  THD *thd= rli->info_thd;
  DBUG_ENTER("Mts_submode_logical_clock::wait_for_workers_to_finish");
  DBUG_PRINT("info",("delegated %d, jobs_done %d", delegated_jobs,
                          jobs_done));
  // Update thd info as waiting for workers to finish.
  thd->enter_stage(&stage_slave_waiting_for_workers_to_process_queue,
                   old_stage,
                    __func__, __FILE__, __LINE__);
  while (delegated_jobs > jobs_done && !thd->killed && !is_error)
  {
    // Todo: consider to replace with a. GAQ::get_lwm_timestamp() or
    // b. (better) pthread wait+signal similarly to DB type.
    if (mts_checkpoint_routine(rli, 0, true, true /*need_data_lock=true*/))
      DBUG_RETURN(-1);
  }
  DBUG_EXECUTE_IF("wait_for_workers_to_finish_after_wait",
                  {
                    const char act[]= "now WAIT_FOR coordinator_continue";
                    DBUG_ASSERT(!debug_sync_set_action(rli->info_thd,
                                                       STRING_WITH_LEN(act)));
                  });

  // The current commit point sequence may end here (e.g Rotate to new log)
  rli->gaq->lwm.sequence_number= SEQ_UNINIT;
  // Restore previous info.
  THD_STAGE_INFO(thd, *old_stage);
  DBUG_PRINT("info",("delegated %d, jobs_done %d, Workers have finished their"
                     " jobs", delegated_jobs, jobs_done));
  rli->mts_group_status= Relay_log_info::MTS_NOT_IN_GROUP;
  DBUG_RETURN(0);
}

/**
  Protected method to fetch the server_id and pseudo_thread_id from a
  temporary table
  @param  : instance pointer of TABLE structure.
  @return : std:pair<uint, my_thread_id>
  @Note   : It is the caller's responsibility to make sure we call this
            function only for temp tables.
 */
std::pair<uint, my_thread_id>
Mts_submode_logical_clock::get_server_and_thread_id(TABLE* table)
{
  DBUG_ENTER("get_server_and_thread_id");
  char* extra_string= table->s->table_cache_key.str;
  size_t extra_string_len= table->s->table_cache_key.length;
  // assert will fail when called with non temporary tables.
  DBUG_ASSERT(table->s->table_cache_key.length > 0);
  std::pair<uint, my_thread_id>ret_pair= std::make_pair
    (
      /* last 8  bytes contains the server_id + pseudo_thread_id */
      // fetch first 4 bytes to get the server id.
      uint4korr(extra_string + extra_string_len - 8),
      /* next  4 bytes contains the pseudo_thread_id */
      uint4korr(extra_string + extra_string_len - 4)
    );
  DBUG_RETURN(ret_pair);
}

