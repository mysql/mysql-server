/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "gcs_replication_threads_api.h"
#include <rpl_slave.h>

int
Replication_thread_api::initialize_repositories(char* relay_log_name,
                                                char* relay_log_info_name)
{
  DBUG_ENTER("Replication_thread_api::initialize");
  int error= 0;

  /*
    TODO: Modify the slave code allowing the MI/RLI initialization methods
    to receive these variables as input.

    TODO: These variables should belong to the handler and defined in the
    plugin.

    We change the relay log variable names here isolating this relay log from
    the server. Not doing this would imply that the server would take the
    present relay logs as being a remain from a stopped server invoking
    unnecessary methods that could fail due to the wrong context.
  */

  char *orig_relay_log_name= set_relay_log_name(relay_log_name);
  char *orig_relay_log_index_name= set_relay_log_index_name(relay_log_name);
  char *original_relay_info_file= set_relay_log_info_name(relay_log_info_name);

  /*
    Master info repositories are not important here.
    They only function as holders for the SQL thread/Relay log variables.
  */
  if (create_coordinators(INFO_REPOSITORY_DUMMY,
                          &this->mi,
                          INFO_REPOSITORY_FILE,
                          &this->rli))
  {
    error= REPLICATION_THREAD_REPOSITORY_CREATION_ERROR;
    goto end;
  }

  mysql_mutex_lock(&mi->data_lock);
  mysql_mutex_lock(&mi->rli->data_lock);

  if (mi->mi_init_info())
  {
    error= REPLICATION_THREAD_MI_INIT_ERROR;
    goto end;
  }

  mi->set_mi_description_event(
    new Format_description_log_event(BINLOG_VERSION));

  mi->set_auto_position(true);

  if (rli->rli_init_info())
  {
    error= REPLICATION_THREAD_RLI_INIT_ERROR;
    goto end;
  }

  //MTS is disable for now
  mi->rli->opt_slave_parallel_workers= 0;
  mi->rli->replicate_same_server_id= true;
  // Set applier thread InnoDB priority
  mi->rli->set_thd_tx_priority(GCS_APPLIER_THREAD_PRIORITY);

end:

  //return the server variable to their original state
  set_relay_log_name(orig_relay_log_name);
  set_relay_log_index_name(orig_relay_log_index_name);
  set_relay_log_info_name(original_relay_info_file);

  if(error != REPLICATION_THREAD_REPOSITORY_CREATION_ERROR)
  {
    mysql_mutex_unlock(&mi->rli->data_lock);
    mysql_mutex_unlock(&mi->data_lock);
  }

  DBUG_RETURN(error);
}

int
Replication_thread_api::create_coordinators(uint mi_option,
                                            Master_info **mi,
                                            uint rli_option,
                                            Relay_log_info **rli)
{
  DBUG_ENTER("Replication_thread_api::create_coordinators");

  if (!((*mi)= Rpl_info_factory::create_mi(mi_option, NULL, true)))
    DBUG_RETURN(TRUE);

  if (!((*rli)= Rpl_info_factory::create_rli(rli_option, relay_log_recovery,
                                             NULL, false)))
  {
    delete *mi;
    *mi= NULL;
    DBUG_RETURN(TRUE);
  }

  /*
    Setting the cross dependency used all over the code.
  */
  (*mi)->set_relay_log_info(*rli);
  (*rli)->set_master_info(*mi);

  DBUG_RETURN(FALSE);
}

void
Replication_thread_api::initialize_connection_parameters(const string* hostname,
                                                         uint port,
                                                         char* user,
                                                         char* password,
                                                         char* master_log_name,
                                                         int retry_count)
{
  mysql_mutex_lock(&mi->data_lock);
  mysql_mutex_lock(&mi->rli->data_lock);

  if (hostname != NULL)
  {
    (void) strncpy(mi->host, hostname->c_str(),hostname->length()+1);
  }
  else
  {
    mi->host[0]= '\0';
  }

  mi->port= port;

  if (user != NULL)
  {
    mi->set_user(user);
  }

  if (password != NULL)
  {
    mi->set_password(password);
  }

  if(master_log_name != NULL)
  {
    mi->set_master_log_name(master_log_name);
  }
  else
  {
    mi->set_master_log_name("");
  }

  if(retry_count > 0)
    mi->retry_count= retry_count;

  mysql_mutex_unlock(&mi->rli->data_lock);
  mysql_mutex_unlock(&mi->data_lock);
}

void
Replication_thread_api::initialize_view_id_until_condition(const char* view_id)
{
  mi->rli->until_condition= Relay_log_info::UNTIL_SQL_VIEW_ID;
  mi->rli->until_view_id.clear();
  mi->rli->until_view_id.append(view_id);
}

int Replication_thread_api::start_replication_threads(int thread_mask,
                                                      bool wait_for_connection)
{
  DBUG_ENTER("Slave_thread_api::start_replication_thread");

  int error= 0;

  if (mi)
  {
    lock_slave_threads(mi);

    ulong thread_start_id= mi->slave_run_id;
    error= start_slave_threads(false/*need_lock_slave=false*/,
                               true/*wait_for_start=true*/,
                               mi,
                               thread_mask);
    if (error)
    {
      error= REPLICATION_THREAD_START_ERROR;
    }

    unlock_slave_threads(mi);

    if(wait_for_connection)
    {
      mysql_mutex_lock(&mi->run_lock);
      /*
         If the ids are still equal this means the start thread method did not
         wait for the thread to start
      */
      while (thread_start_id == mi->slave_run_id)
      {
        mysql_cond_wait(&mi->start_cond, &mi->run_lock);
      }
      mysql_mutex_unlock(&mi->run_lock);

      while(mi->slave_running != MYSQL_SLAVE_RUN_CONNECT)
      {
        //If there us such a state change then there was an error on connection
        if(mi->slave_running == MYSQL_SLAVE_NOT_RUN)
        {
          error= REPLICATION_THREAD_START_IO_NOT_CONNECTED;
          break;
        }
        usleep(100);
      }
    }
  }
  else
  {
    error= REPLICATION_THREAD_START_NO_INFO_ERROR;
  }

  DBUG_RETURN(error);
}

int Replication_thread_api::purge_relay_logs(bool just_reset)
{
  DBUG_ENTER("Replication_thread_api::purge_relay_logs");

  //Never initialized
  if(rli == NULL)
  {
    DBUG_RETURN(0);
  }

  const char* errmsg= "Unknown error occurred while reseting applier logs";

  /*
    If some thread finishes and the logs are no longer needed, with just
    reset, the logs are simply deleted and the rli variables purged.

    Otherwise, the relay log should be re-initialized. The problem with this
    is that on group replication environments a simple purge may not be
    enough to clean the previous rli state, specially the gtid_retrieved
    variable that is written to the new log when this operation is executed.
    On classic replication the master sent Rotate event solves this issue, but
    for other contexts a new purge is the simplest option.
  */
  if (just_reset)
  {
    if (rli->purge_relay_logs(current_thd, true, &errmsg, true))
    {
      DBUG_RETURN(REPLICATION_THREAD_REPOSITORY_RL_PURGE_ERROR);
    }
  }
  else
  {
    if (rli->purge_relay_logs(current_thd, true, &errmsg))
    {
      DBUG_RETURN(REPLICATION_THREAD_REPOSITORY_RL_PURGE_ERROR);
    }
    if(rli->purge_relay_logs(current_thd, false, &errmsg))
    {
       DBUG_RETURN(REPLICATION_THREAD_REPOSITORY_RL_PURGE_ERROR);
    }
  }

  DBUG_RETURN(0);
}

int Replication_thread_api::purge_master_info()
{
  DBUG_ENTER("Replication_thread_api::purge_master_info");

  mi->clear_in_memory_info(true);

  if (remove_info(mi))
  {
    DBUG_RETURN(REPLICATION_THREAD_REPOSITORY_MI_PURGE_ERROR);
  }
  DBUG_RETURN(0);
}

int Replication_thread_api::stop_threads(bool flush_relay_logs, int thread_mask)
{
  DBUG_ENTER("Replication_thread_api::terminate_sql_thread");

  int error= 0;

  if (mi != NULL && rli != NULL)
  {
    if(thread_mask == -1)
    {
      // Get a mask of _running_ threads
      init_thread_mask(&thread_mask, mi, false /* not inverse*/);
    }

    lock_slave_threads(mi);

    if ((error= terminate_slave_threads(mi, thread_mask,
                                        stop_wait_timeout, false)))
    {
      error= REPLICATION_THREAD_STOP_ERROR;
    }

    if(flush_relay_logs && !error)
    {
      //don't return the possible error and go to the relay log flushing.
      mysql_mutex_t *log_lock= mi->rli->relay_log.get_log_lock();
      mysql_mutex_lock(log_lock);

      /*
        Flushes the relay log regardless of the sync_relay_log option.
        We do not flush the master info as it has no real info and could cause
        the server to identify itself as a master or destroy a real master info
        file/table.
      */
      if (mi->rli->relay_log.is_open() &&
          mi->rli->relay_log.flush_and_sync(true))
      {
        error= REPLICATION_THREAD_STOP_RL_FLUSH_ERROR;
      }

      mysql_mutex_unlock(log_lock);
    }
    unlock_slave_threads(mi);
  }

  DBUG_RETURN(error);
}

void Replication_thread_api::clean_thread_repositories()
{
  //terminate the relay log info structures
  rli->end_info();

  if (mi != NULL)
  {
    delete mi;
    mi= NULL;
  }
  if (rli != NULL)
  {
    delete rli;
    rli= NULL;
  }
}

bool Replication_thread_api::is_io_thread_running()
{
  if(mi != NULL )
  {
    int thread_mask= 0;
    init_thread_mask(&thread_mask, mi, 0 /* not inverse*/);
    return ((thread_mask & SLAVE_IO) != 0);
  }
  return false;
}
bool Replication_thread_api::is_sql_thread_running()
{
  if(mi != NULL )
  {
    int thread_mask= 0;
    init_thread_mask(&thread_mask, mi, 0 /* not inverse*/);
    return ((thread_mask & SLAVE_SQL) != 0);
  }
  return false;
}

int
Replication_thread_api::queue_packet(const char* buf, ulong event_len)
{
  return queue_event(mi, buf, event_len);
}

int
Replication_thread_api::wait_for_gtid_execution(longlong timeout)
{
  DBUG_ENTER("Replication_thread_api::wait_on_event_consumption");

  if (!mi->rli->inited)
    DBUG_RETURN(REPLICATION_THREAD_WAIT_NO_INFO_ERROR);

  const Gtid_set* wait_gtid_set= rli->get_gtid_set();

  mysql_mutex_lock(&rli->data_lock);

  int error=0;

  struct timespec abstime; // for timeout checking
  set_timespec(&abstime, timeout);

  //wait for master update, with optional timeout.

  while (mi->rli->slave_running)
  {
    global_sid_lock->wrlock();
    const Gtid_set* executed_gtids= gtid_state->get_executed_gtids();
    const Owned_gtids* owned_gtids= gtid_state->get_owned_gtids();

    DBUG_PRINT("info", ("Waiting for '%s'. is_subset: %d and "
                        "!is_intersection_nonempty: %d",
            wait_gtid_set->to_string(), wait_gtid_set->is_subset(executed_gtids),
            !owned_gtids->is_intersection_nonempty(wait_gtid_set)));

    executed_gtids->dbug_print("gtid_executed:");
    owned_gtids->dbug_print("owned_gtids:");

    /*
      Since commit is performed after log to binary log, we must also
      check if any GTID of wait_gtid_set is not yet committed.
    */
    if (wait_gtid_set->is_subset(executed_gtids) &&
        !owned_gtids->is_intersection_nonempty(wait_gtid_set))
    {
      global_sid_lock->unlock();
      break;
    }

    global_sid_lock->unlock();
    DBUG_PRINT("info",("Waiting for database update"));
    /*
       We are going to mysql_cond_(timed)wait(); if the SQL thread stops it
       will wake us up.
    */
    if (timeout > 0)
    {
      /*
        Note that mysql_cond_timedwait checks for the timeout
        before for the condition ; i.e. it returns ETIMEDOUT
        if the system time equals or exceeds the time specified by abstime
        before the condition variable is signaled or broadcast, _or_ if
        the absolute time specified by abstime has already passed at the time
        of the call.
        For that reason, mysql_cond_timedwait will do the "timeoutting" job
        even if its condition is always immediately signaled (case of a loaded
        master).
      */
      error= mysql_cond_timedwait(&rli->data_cond, &rli->data_lock, &abstime);
    }
    else
      mysql_cond_wait(&rli->data_cond, &rli->data_lock);
    DBUG_PRINT("info",("Got signal of master update or timed out"));
    if (error == ETIMEDOUT || error == ETIME)
    {
        error= REPLICATION_THREAD_WAIT_TIMEOUT_ERROR;
        break;
    }
    error=0;
  }

  mysql_mutex_unlock(&rli->data_lock);

  DBUG_RETURN(error);
}

rpl_gno
Replication_thread_api::get_last_delivered_gno(rpl_sidno sidno)
{
  DBUG_ENTER("Replication_thread_api::get_last_delivered_gno");
  rpl_gno last_gno= 0;

  global_sid_lock->rdlock();
  last_gno= mi->rli->get_gtid_set()->get_last_gno(sidno);
  global_sid_lock->unlock();

#if !defined(DBUG_OFF)
  const Gtid_set *retrieved_gtid_set= mi->rli->get_gtid_set();
  char *retrieved_gtid_set_string= NULL;
  global_sid_lock->wrlock();
  retrieved_gtid_set->to_string(&retrieved_gtid_set_string);
  global_sid_lock->unlock();
  DBUG_PRINT("info", ("get_last_delivered_gno retrieved_set_string: %s",
                      retrieved_gtid_set_string));
  my_free(retrieved_gtid_set_string);
#endif

  DBUG_RETURN(last_gno);
}

bool Replication_thread_api::is_own_event_channel(my_thread_id id)
{
  DBUG_ENTER("Replication_thread_api::is_own_event_channel");

  bool result= false;

  if(rli != NULL)
  {
    if(rli->info_thd != NULL)
      result= (rli->info_thd->thread_id() == id);
  }

  DBUG_RETURN(result);
}
