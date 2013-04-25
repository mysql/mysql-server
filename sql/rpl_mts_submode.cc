/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#include "rpl_mts_submode.h"
#include "rpl_rli_pdb.h"
#include "rpl_slave.h"

/*
 Does necessary arrangement before scheduling next event.
 @param:  Relay_log_info rli
 @return: TRUE  if error
          FALSE if no error
*/
bool
Mts_submode_database::schedule_next_event(Relay_log_info *rli, Log_event *ev)
{
  /*nothing to do here*/
  return false;
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

/*
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

/*
  Logic to get least occupied worker when the sql mts_submode= database
  @param
    rli relay log info of coordinator
    ws arrayy of worker threads
    ev event for which we are searching for a worker.
  @return slave worker thread
 */
Slave_worker *
Mts_submode_database::get_least_occupied_worker(Relay_log_info *rli,
                                                DYNAMIC_ARRAY *ws,
                                                Log_event *ev)
{
 long usage= LONG_MAX;
  Slave_worker **ptr_current_worker= NULL, *worker= NULL;
  ulong i= 0;

  DBUG_ENTER("Mts_submode_database::get_least_occupied_worker");

#ifndef DBUG_OFF

  if (DBUG_EVALUATE_IF("mts_distribute_round_robin", 1, 0))
  {
    worker= *((Slave_worker **)dynamic_array_ptr(ws,
                                                 w_rr % ws->elements));
    sql_print_information("Chosing worker id %lu, the following is"
                          " going to be %lu", worker->id, w_rr % ws->elements);
    DBUG_ASSERT(worker != NULL);
    DBUG_RETURN(worker);
  }
#endif

  for (i= 0; i< ws->elements; i++)
  {
    ptr_current_worker= (Slave_worker **) dynamic_array_ptr(ws, i);
    if ((*ptr_current_worker)->usage_partition <= usage)
    {
      worker= *ptr_current_worker;
      usage= (*ptr_current_worker)->usage_partition;
    }
  }
  DBUG_ASSERT(worker != NULL);
  DBUG_RETURN(worker);
}

/*
 Logic to assign the parent id to the transaction
 @param rli Relay_log_info of the coordinator
 @return true is error
         false otherwise
 @return: true if error
          false otherwise
*/
bool
Mts_submode_database::assign_group_parent_id(Relay_log_info* rli,
                                             Log_event * ev)
{
  return false;
}

/* MTS submode master Default constructor */
Mts_submode_master::Mts_submode_master()
{
  type= MTS_PARALLEL_TYPE_BGC;
  first_event= true;
  mts_last_known_commit_parent= SEQ_UNINIT;
  mts_last_known_parent_group_id= -1;
  force_new_group= false;
  defer_new_group= false;
}

/*
 Does necessary arrangement before scheduling next event.
 @param:  Relay_log_info rli
 @return: TRUE  if error
          FALSE if no error
 */
bool
Mts_submode_master::schedule_next_event(Relay_log_info* rli, Log_event *ev)
{
  DBUG_ENTER("Mts_submode_master::schedule_next_event");
  if (ev->get_type_code() == GTID_LOG_EVENT)
    DBUG_RETURN (false);
  /*
    The coordinator waits till the last group was completely applied before
    the events from the next group is scheduled for the workers.
    data locks are handled for a short duration while updating the
    log positions.
  */
  if (!rli->is_new_group)
    rli->delegated_jobs++;
  else
  {
    /*
      We have a new group and we must check if the last group was completey applied before
      we move on to the next group
     */
    // we should check if the SQL thread was already killed before we schecdule
    // the next transaction
    if (sql_slave_killed(rli->info_thd, rli))
      DBUG_RETURN(true);
    DBUG_PRINT("info",("delegated %d, jobs_done %d", rli->delegated_jobs, rli->jobs_done));
    while (rli->delegated_jobs > rli->jobs_done)
    {
      if (mts_checkpoint_routine(rli, 0, true, true /*need_data_lock=true*/))
      {
        DBUG_RETURN(true);
      }
    }
    DBUG_PRINT("info",("delegated %d, jobs_done %d, we can schedule "
                       "the next group.", rli->delegated_jobs, rli->jobs_done));
    //DBUG_ASSERT(rli->delegated_jobs == rli->jobs_done);
    rli->delegated_jobs= 1;
    rli->jobs_done= 0;
  }
  DBUG_RETURN(false);
}

/*
 Logic to attach the temporary tables from the worker threads upon
 event execution
 @param: thd THD instance
         rli Relay_log_info instance
         ev  Query_log_event that is being applied
 @return: void
 */
void
Mts_submode_master::attach_temp_tables(THD *thd, const Relay_log_info* rli,
                                       Query_log_event * ev)
{
  bool shifted= false;
  TABLE *table, *cur_table;
  DBUG_ENTER("Mts_submode_master::attach_temp_tables");
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

/*
 Logic to detach the temporary tables from the worker threads upon
 event execution
 @param: thd THD instance
         rli Relay_log_info instance
         ev  Query_log_event that is being applied
 @return: void
 */
void
Mts_submode_master::detach_temp_tables( THD *thd, const Relay_log_info* rli,
                                        Query_log_event * ev)
{
  DBUG_ENTER("Mts_submode_master::detach_temp_tables");
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

/*
  Logic to get least occupied worker when the sql mts_submode= master_parallel
  @param
    rli relay log info of coordinator
    ws arrayy of worker threads
    ev event for which we are searching for a worker.
  @return slave worker thread
 */

Slave_worker *
Mts_submode_master::get_least_occupied_worker(Relay_log_info *rli,
                                              DYNAMIC_ARRAY *ws,
                                              Log_event * ev)
{
  Slave_committed_queue *gaq= rli->gaq;
  Slave_worker *worker= NULL;
  Slave_job_group* ptr_group;
  DBUG_ENTER("Mts_submode_master::get_least_occupied_worker");
#ifndef DBUG_OFF

  if (DBUG_EVALUATE_IF("mts_distribute_round_robin", 1, 0))
  {
    worker= *((Slave_worker **)dynamic_array_ptr(ws,
                                                 w_rr % ws->elements));
    sql_print_information("Chosing worker id %lu, the following is"
                          " going to be %lu", worker->id, w_rr % ws->elements);
    DBUG_ASSERT(worker != NULL);
    DBUG_RETURN(worker);
  }
#endif
  ptr_group= gaq->get_job_group(rli->gaq->assigned_group_index);
  /*
    The scheduling works as follows, in this sequence
      -If this is an internal event of a transaction  use the last assigned
        worker
      -If the i-th transaction is being scheduled in this group where "i" <=
       number of available workers then schedule the events to the consecutive
       workers
      -If the i-th transaction is being scheduled in this group where "i" >
       number of available workers then schedule this to the forst worker that
       becomes free..
   */
  if (rli->last_assigned_worker)
    worker= rli->last_assigned_worker;
  else
  {
    if (worker_seq < ws->elements)
    {
      worker= *((Slave_worker **)dynamic_array_ptr(ws, worker_seq));
      worker_seq++;
    }
    else
    {
      /* wait and get a free worker */
      do
      {
        worker= get_free_worker(rli);
      } while (!worker);
    }
  }

  DBUG_ASSERT(ptr_group);
  // assert that we have a worker thread for this event
  DBUG_ASSERT(worker != NULL);

  ptr_group->worker_id= worker->id;
  /* The master my have send  db partition info. make sure we never use them*/
  if (ev->get_type_code() == QUERY_EVENT)
    static_cast<Query_log_event*>(ev)->mts_accessed_dbs= 0;
  DBUG_RETURN(worker);
}

/*
 Logic to assign the parent id to the transaction
 @param rli Relay_log_info of the coordinator
 @return true is error
         false otherwise
*/
bool
Mts_submode_master::assign_group_parent_id(Relay_log_info* rli,
                                            Log_event *ev)
{
  Slave_committed_queue *gaq= rli->gaq;
  int64 commit_seq_no;
  /*
    A group id updater must satisfy the following:
      - A query log event ("BEGIN" ) or a GTID EVENT
      - A DDL or an implicit DML commit.
   */
  switch (ev->get_type_code())
  {
  case QUERY_EVENT:
    commit_seq_no= static_cast<Query_log_event*>(ev)->commit_seq_no;
    break;

  case GTID_LOG_EVENT:
    commit_seq_no= static_cast<Gtid_log_event*>(ev)->commit_seq_no;
    break;
  case USER_VAR_EVENT:
    force_new_group= true;
    break;

  default:
    // these can never be a group changer
    commit_seq_no= SEQ_UNINIT;
    break;
  }
  if (first_event && commit_seq_no == SEQ_UNINIT && !force_new_group)
  {
    // This is the first event and the master has not sent us the commit
    // sequence number. The possible reason may be that the master is old and
    // doesnot support BGC based parallelization, or someone tried to start
    // replication from within a transaction.
    return true;  
  }

  if (force_new_group
      ||(commit_seq_no != SEQ_UNINIT /* Not an internal event */ &&
      /* not same as last seq number */
      commit_seq_no != mts_last_known_commit_parent) ||
      /* first event after a submode switch */
      first_event)
  {
    mts_last_known_commit_parent= commit_seq_no;
    mts_last_known_parent_group_id=
      gaq->get_job_group(rli->gaq->assigned_group_index)->parent_seqno=
      rli->mts_groups_assigned-1;
    worker_seq= 0;
    if (ev->get_type_code() == GTID_LOG_EVENT)
      defer_new_group= true;
    else
      rli->is_new_group= true;
    force_new_group= false;
  }
  else
  {
    gaq->get_job_group(rli->gaq->assigned_group_index)->parent_seqno=
      mts_last_known_parent_group_id;
    if (defer_new_group)
    {
      rli->is_new_group= true;
      defer_new_group= false;
    }
    else
      rli->is_new_group= false;
  }
  DBUG_PRINT("info", ("MTS::slave c=%lld, pid= %lld", commit_seq_no,
                      mts_last_known_parent_group_id));
  if (first_event) first_event= false;
  return false;
}

/**
  Protected method to fetch a free  worker. returns NULL if non are free.
  It is up to caller to make sure that it polls using this function for
  any free workers.
 */
Slave_worker*
Mts_submode_master::get_free_worker(Relay_log_info *rli)
{
  Slave_worker *w_i;
  for (uint i= 0; i < rli->workers.elements; i++)
  {
    get_dynamic(&rli->workers, (uchar *) &w_i, i);
    if (w_i->jobs.len == 0)
      return w_i;
  }
  return 0;
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
Mts_submode_master::get_server_and_thread_id(TABLE* table)
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

