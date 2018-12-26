/* Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "rpl_rli.h"

#include "my_dir.h"                // MY_STAT
#include "log.h"                   // sql_print_error
#include "log_event.h"             // Log_event
#include "m_string.h"
#include "rpl_group_replication.h" // set_group_replication_retrieved_certifi...
#include "rpl_info_factory.h"      // Rpl_info_factory
#include "rpl_mi.h"                // Master_info
#include "rpl_msr.h"               // channel_map
#include "rpl_rli_pdb.h"           // Slave_worker
#include "sql_base.h"              // close_thread_tables
#include "strfunc.h"               // strconvert
#include "transaction.h"           // trans_commit_stmt
#include "debug_sync.h"
#include "pfs_file_provider.h"
#include "mysql/psi/mysql_file.h"

#include <algorithm>
using std::min;
using std::max;

/*
  Please every time you add a new field to the relay log info, update
  what follows. For now, this is just used to get the number of
  fields.
*/
const char* info_rli_fields[]=
{
  "number_of_lines",
  "group_relay_log_name",
  "group_relay_log_pos",
  "group_master_log_name",
  "group_master_log_pos",
  "sql_delay",
  "number_of_workers",
  "id",
  "channel_name"
};

Relay_log_info::Relay_log_info(bool is_slave_recovery
#ifdef HAVE_PSI_INTERFACE
                               ,PSI_mutex_key *param_key_info_run_lock,
                               PSI_mutex_key *param_key_info_data_lock,
                               PSI_mutex_key *param_key_info_sleep_lock,
                               PSI_mutex_key *param_key_info_thd_lock,
                               PSI_mutex_key *param_key_info_data_cond,
                               PSI_mutex_key *param_key_info_start_cond,
                               PSI_mutex_key *param_key_info_stop_cond,
                               PSI_mutex_key *param_key_info_sleep_cond
#endif
                               , uint param_id, const char *param_channel,
                               bool is_rli_fake
                              )
   :Rpl_info("SQL"
#ifdef HAVE_PSI_INTERFACE
             ,param_key_info_run_lock, param_key_info_data_lock,
             param_key_info_sleep_lock, param_key_info_thd_lock,
             param_key_info_data_cond, param_key_info_start_cond,
             param_key_info_stop_cond, param_key_info_sleep_cond
#endif
             , param_id, param_channel
            ),
   replicate_same_server_id(::replicate_same_server_id),
   cur_log_fd(-1), relay_log(&sync_relaylog_period, SEQ_READ_APPEND),
   is_relay_log_recovery(is_slave_recovery),
   save_temporary_tables(0),
   cur_log_old_open_count(0), error_on_rli_init_info(false),
   group_relay_log_pos(0), event_relay_log_number(0),
   event_relay_log_pos(0), event_start_pos(0),
   group_master_log_pos(0),
   gtid_set(global_sid_map, global_sid_lock),
   rli_fake(is_rli_fake),
   gtid_retrieved_initialized(false),
   is_group_master_log_pos_invalid(false),
   log_space_total(0), ignore_log_space_limit(0),
   sql_force_rotate_relay(false),
   last_master_timestamp(0), slave_skip_counter(0),
   abort_pos_wait(0), until_condition(UNTIL_NONE),
   until_log_pos(0),
   until_sql_gtids(global_sid_map),
   until_sql_gtids_first_event(true),
   trans_retries(0), retried_trans(0),
   tables_to_lock(0), tables_to_lock_count(0),
   rows_query_ev(NULL), last_event_start_time(0), deferred_events(NULL),
   workers(PSI_NOT_INSTRUMENTED),
   workers_array_initialized(false),
   curr_group_assigned_parts(PSI_NOT_INSTRUMENTED),
   curr_group_da(PSI_NOT_INSTRUMENTED),
   slave_parallel_workers(0),
   exit_counter(0),
   max_updated_index(0),
   recovery_parallel_workers(0), checkpoint_seqno(0),
   checkpoint_group(opt_mts_checkpoint_group),
   recovery_groups_inited(false), mts_recovery_group_cnt(0),
   mts_recovery_index(0), mts_recovery_group_seen_begin(0),
   mts_group_status(MTS_NOT_IN_GROUP),
   stats_exec_time(0), stats_read_time(0),
   least_occupied_workers(PSI_NOT_INSTRUMENTED),
   current_mts_submode(0),
   reported_unsafe_warning(false), rli_description_event(NULL),
   commit_order_mngr(NULL),
   sql_delay(0), sql_delay_end(0), m_flags(0), row_stmt_start_timestamp(0),
   long_find_row_note_printed(false),
   thd_tx_priority(0),
   is_engine_ha_data_detached(false)
{
  DBUG_ENTER("Relay_log_info::Relay_log_info");

#ifdef HAVE_PSI_INTERFACE
  relay_log.set_psi_keys(key_RELAYLOG_LOCK_index,
                         key_RELAYLOG_LOCK_commit,
                         key_RELAYLOG_LOCK_commit_queue,
                         key_RELAYLOG_LOCK_done,
                         key_RELAYLOG_LOCK_flush_queue,
                         key_RELAYLOG_LOCK_log,
                         PSI_NOT_INSTRUMENTED, /* Relaylog doesn't support LOCK_binlog_end_pos */
                         key_RELAYLOG_LOCK_sync,
                         key_RELAYLOG_LOCK_sync_queue,
                         key_RELAYLOG_LOCK_xids,
                         key_RELAYLOG_COND_done,
                         key_RELAYLOG_update_cond,
                         key_RELAYLOG_prep_xids_cond,
                         key_file_relaylog,
                         key_file_relaylog_index,
                         key_file_relaylog_cache,
                         key_file_relaylog_index_cache);
#endif

  group_relay_log_name[0]= event_relay_log_name[0]=
    group_master_log_name[0]= 0;
  until_log_name[0]= ign_master_log_name_end[0]= 0;
  set_timespec_nsec(&last_clock, 0);
  memset(&cache_buf, 0, sizeof(cache_buf));
  cached_charset_invalidate();
  inited_hash_workers= FALSE;
  channel_open_temp_tables.atomic_set(0);
  /*
    For applier threads, currently_executing_gtid is set to automatic
    when they are not executing any transaction.
  */
  currently_executing_gtid.set_automatic();

  if (!rli_fake)
  {
    mysql_mutex_init(key_relay_log_info_log_space_lock,
                     &log_space_lock, MY_MUTEX_INIT_FAST);
    mysql_cond_init(key_relay_log_info_log_space_cond, &log_space_cond);
    mysql_mutex_init(key_mutex_slave_parallel_pend_jobs, &pending_jobs_lock,
                     MY_MUTEX_INIT_FAST);
    mysql_cond_init(key_cond_slave_parallel_pend_jobs, &pending_jobs_cond);
    mysql_mutex_init(key_mutex_slave_parallel_worker_count, &exit_count_lock,
                   MY_MUTEX_INIT_FAST);
    mysql_mutex_init(key_mts_temp_table_LOCK, &mts_temp_table_LOCK,
                     MY_MUTEX_INIT_FAST);
    mysql_mutex_init(key_mts_gaq_LOCK, &mts_gaq_LOCK,
                     MY_MUTEX_INIT_FAST);
    mysql_cond_init(key_cond_mts_gaq, &logical_clock_cond);

    relay_log.init_pthread_objects();
    force_flush_postponed_due_to_split_trans= false;
  }
  do_server_version_split(::server_version, slave_version_split);

  DBUG_VOID_RETURN;
}

/**
   The method to invoke at slave threads start
*/
void Relay_log_info::init_workers(ulong n_workers)
{
  /*
    Parallel slave parameters initialization is done regardless
    whether the feature is or going to be active or not.
  */
  mts_groups_assigned= mts_events_assigned= pending_jobs= wq_size_waits_cnt= 0;
  mts_wq_excess_cnt= mts_wq_no_underrun_cnt= mts_wq_overfill_cnt= 0;
  mts_total_wait_overlap= 0;
  mts_total_wait_worker_avail= 0;
  mts_last_online_stat= 0;

  workers.reserve(n_workers);
  workers_array_initialized= true; //set after init
}

/**
   The method to invoke at slave threads stop
*/
void Relay_log_info::deinit_workers()
{
  workers.clear();
}

Relay_log_info::~Relay_log_info()
{
  DBUG_ENTER("Relay_log_info::~Relay_log_info");

  if(!rli_fake)
  {
    if (recovery_groups_inited)
      bitmap_free(&recovery_groups);
    delete current_mts_submode;

    if(workers_copy_pfs.size())
    {
      for (int i= static_cast<int>(workers_copy_pfs.size()) - 1; i >= 0; i--)
        delete workers_copy_pfs[i];
      workers_copy_pfs.clear();
    }

    mysql_mutex_destroy(&log_space_lock);
    mysql_cond_destroy(&log_space_cond);
    mysql_mutex_destroy(&pending_jobs_lock);
    mysql_cond_destroy(&pending_jobs_cond);
    mysql_mutex_destroy(&exit_count_lock);
    mysql_mutex_destroy(&mts_temp_table_LOCK);
    mysql_mutex_destroy(&mts_gaq_LOCK);
    mysql_cond_destroy(&logical_clock_cond);
    relay_log.cleanup();
  }

  set_rli_description_event(NULL);

  DBUG_VOID_RETURN;
}

/**
   Method is called when MTS coordinator senses the relay-log name
   has been changed.
   It marks each Worker member with this fact to make an action
   at time it will distribute a terminal event of a group to the Worker.

   Worker receives the new name at the group commiting phase
   @c Slave_worker::slave_worker_ends_group().
*/
void Relay_log_info::reset_notified_relay_log_change()
{
  if (!is_parallel_exec())
    return;
  for (Slave_worker **it= workers.begin(); it != workers.end(); ++it)
  {
    Slave_worker *w= *it;
    w->relay_log_change_notified= FALSE;
  }
}

/**
   This method is called in mts_checkpoint_routine() to mark that each
   worker is required to adapt to a new checkpoint data whose coordinates
   are passed to it through GAQ index.

   Worker notices the new checkpoint value at the group commit to reset
   the current bitmap and starts using the clean bitmap indexed from zero
   of being reset checkpoint_seqno. 

    New seconds_behind_master timestamp is installed.

   @param shift            number of bits to shift by Worker due to the
                           current checkpoint change.
   @param new_ts           new seconds_behind_master timestamp value
                           unless zero. Zero could be due to FD event
                           or fake rotate event.
   @param need_data_lock   False if caller has locked @c data_lock
   @param update_timestamp if true, this function will update the
                           rli->last_master_timestamp.
*/
void Relay_log_info::reset_notified_checkpoint(ulong shift, time_t new_ts,
                                               bool need_data_lock,
                                               bool update_timestamp)
{
  /*
    If this is not a parallel execution we return immediately.
  */
  if (!is_parallel_exec())
    return;

  for (Slave_worker **it= workers.begin(); it != workers.end(); ++it)
  {
    Slave_worker *w= *it;
    /*
      Reseting the notification information in order to force workers to
      assign jobs with the new updated information.
      Notice that the bitmap_shifted is accumulated to indicate how many
      consecutive jobs were successfully processed. 

      The worker when assigning a new job will set the value back to
      zero.
    */
    w->checkpoint_notified= FALSE;
    w->bitmap_shifted= w->bitmap_shifted + shift;
    /*
      Zero shift indicates the caller rotates the master binlog.
      The new name will be passed to W through the group descriptor
      during the first post-rotation time scheduling.
    */
    if (shift == 0)
      w->master_log_change_notified= false;

    DBUG_PRINT("mts", ("reset_notified_checkpoint shift --> %lu, "
                       "worker->bitmap_shifted --> %lu, worker --> %u.",
                       shift, w->bitmap_shifted,
                       static_cast<unsigned>(it - workers.begin())));
  }
  /*
    There should not be a call where (shift == 0 && checkpoint_seqno != 0).
    Then the new checkpoint sequence is updated by subtracting the number
    of consecutive jobs that were successfully processed.
  */
  DBUG_ASSERT(current_mts_submode->get_type() != MTS_PARALLEL_TYPE_DB_NAME ||
              !(shift == 0 && checkpoint_seqno != 0));
  checkpoint_seqno= checkpoint_seqno - shift;
  DBUG_PRINT("mts", ("reset_notified_checkpoint shift --> %lu, "
             "checkpoint_seqno --> %u.", shift, checkpoint_seqno));

  if (update_timestamp)
  {
    if (need_data_lock)
      mysql_mutex_lock(&data_lock);
    else
      mysql_mutex_assert_owner(&data_lock);
    last_master_timestamp= new_ts;
    if (need_data_lock)
      mysql_mutex_unlock(&data_lock);
  }
}

/**
   Reset recovery info from Worker info table and 
   mark MTS recovery is completed.

   @return false on success true when @c reset_notified_checkpoint failed.
*/
bool Relay_log_info::mts_finalize_recovery()
{
  bool ret= false;
  uint i;
  uint repo_type= get_rpl_info_handler()->get_rpl_info_type();

  DBUG_ENTER("Relay_log_info::mts_finalize_recovery");

  for (Slave_worker **it= workers.begin(); !ret && it != workers.end(); ++it)
  {
    Slave_worker *w= *it;
    ret= w->reset_recovery_info();
    DBUG_EXECUTE_IF("mts_debug_recovery_reset_fails", ret= true;);
  }
  /*
    The loop is traversed in the worker index descending order due
    to specifics of the Worker table repository that does not like
    even temporary holes. Therefore stale records are deleted
    from the tail.
  */
  DBUG_EXECUTE_IF("enable_mts_wokrer_failure_in_recovery_finalize",
                  {DBUG_SET("+d,mts_worker_thread_init_fails");});
  for (i= recovery_parallel_workers; i > workers.size() && !ret; i--)
  {
    Slave_worker *w=
      Rpl_info_factory::create_worker(repo_type, i - 1, this, true);
    /*
      If an error occurs during the above create_worker call, the newly created
      worker object gets deleted within the above function call itself and only
      NULL is returned. Hence the following check has been added to verify
      that a valid worker object exists.
    */
    if (w)
    {
      ret= w->remove_info();
      delete w;
    }
    else
    {
      ret= true;
      goto err;
    }
  }
  recovery_parallel_workers= slave_parallel_workers;

err:
  DBUG_RETURN(ret);
}

static inline int add_relay_log(Relay_log_info* rli,LOG_INFO* linfo)
{
  MY_STAT s;
  DBUG_ENTER("add_relay_log");
  if (!mysql_file_stat(key_file_relaylog,
                       linfo->log_file_name, &s, MYF(0)))
  {
    sql_print_error("log %s listed in the index, but failed to stat.",
                    linfo->log_file_name);
    DBUG_RETURN(1);
  }
  rli->log_space_total += s.st_size;
#ifndef DBUG_OFF
  char buf[22];
  DBUG_PRINT("info",("log_space_total: %s", llstr(rli->log_space_total,buf)));
#endif
  DBUG_RETURN(0);
}

int Relay_log_info::count_relay_log_space()
{
  LOG_INFO flinfo;
  DBUG_ENTER("Relay_log_info::count_relay_log_space");
  log_space_total= 0;
  if (relay_log.find_log_pos(&flinfo, NullS, 1))
  {
    sql_print_error("Could not find first log while counting relay log space.");
    DBUG_RETURN(1);
  }
  do
  {
    if (add_relay_log(this, &flinfo))
      DBUG_RETURN(1);
  } while (!relay_log.find_next_log(&flinfo, 1));
  /*
     As we have counted everything, including what may have written in a
     preceding write, we must reset bytes_written, or we may count some space
     twice.
  */
  relay_log.reset_bytes_written();
  DBUG_RETURN(0);
}

/**
   Resets UNTIL condition for Relay_log_info
 */

void Relay_log_info::clear_until_condition()
{
  DBUG_ENTER("clear_until_condition");

  until_condition= Relay_log_info::UNTIL_NONE;
  until_log_name[0]= 0;
  until_log_pos= 0;
  until_sql_gtids.clear();
  until_sql_gtids_first_event= true;
  DBUG_VOID_RETURN;
}

/**
  Opens and intialize the given relay log. Specifically, it does what follows:

  - Closes old open relay log files.
  - If we are using the same relay log as the running IO-thread, then sets.
    rli->cur_log to point to the same IO_CACHE entry.
  - If not, opens the 'log' binary file.

  @todo check proper initialization of
  group_master_log_name/group_master_log_pos. /alfranio

  @param rli[in] Relay information (will be initialized)
  @param log[in] Name of relay log file to read from. NULL = First log
  @param pos[in] Position in relay log file
  @param need_data_lock[in] If true, this function will acquire the
  relay_log.data_lock(); otherwise the caller should already have
  acquired it.
  @param errmsg[out] On error, this function will store a pointer to
  an error message here
  @param keep_looking_for_fd[in] If true, this function will
  look for a Format_description_log_event.  We only need this when the
  SQL thread starts and opens an existing relay log and has to execute
  it (possibly from an offset >4); then we need to read the first
  event of the relay log to be able to parse the events we have to
  execute.

  @retval 0 ok,
  @retval 1 error.  In this case, *errmsg is set to point to the error
  message.
*/

int Relay_log_info::init_relay_log_pos(const char* log,
                                       ulonglong pos, bool need_data_lock,
                                       const char** errmsg,
                                       bool keep_looking_for_fd)
{
  DBUG_ENTER("Relay_log_info::init_relay_log_pos");
  DBUG_PRINT("info", ("pos: %lu", (ulong) pos));

  *errmsg=0;
  const char* errmsg_fmt= 0;
  static char errmsg_buff[MYSQL_ERRMSG_SIZE + FN_REFLEN];
  mysql_mutex_t *log_lock= relay_log.get_log_lock();

  if (need_data_lock)
    mysql_mutex_lock(&data_lock);
  else
    mysql_mutex_assert_owner(&data_lock);

  /*
    By default the relay log is in binlog format 3 (4.0).
    Even if format is 4, this will work enough to read the first event
    (Format_desc) (remember that format 4 is just lenghtened compared to format
    3; format 3 is a prefix of format 4).
  */
  set_rli_description_event(new Format_description_log_event(3));

  mysql_mutex_lock(log_lock);

  /* Close log file and free buffers if it's already open */
  if (cur_log_fd >= 0)
  {
    end_io_cache(&cache_buf);
    mysql_file_close(cur_log_fd, MYF(MY_WME));
    cur_log_fd = -1;
  }

  group_relay_log_pos= event_relay_log_pos= pos;

  /*
    Test to see if the previous run was with the skip of purging
    If yes, we do not purge when we restart
  */
  if (relay_log.find_log_pos(&linfo, NullS, 1))
  {
    *errmsg="Could not find first log during relay log initialization";
    goto err;
  }

  if (log && relay_log.find_log_pos(&linfo, log, 1))
  {
    errmsg_fmt= "Could not find target log file mentioned in "
                "relay log info in the index file '%s' during "
                "relay log initialization";
    sprintf(errmsg_buff, errmsg_fmt, relay_log.get_index_fname());
    *errmsg= errmsg_buff;
    goto err;
  }

  set_group_relay_log_name(linfo.log_file_name);
  set_event_relay_log_name(linfo.log_file_name);

  if (relay_log.is_active(linfo.log_file_name))
  {
    /*
      The IO thread is using this log file.
      In this case, we will use the same IO_CACHE pointer to
      read data as the IO thread is using to write data.
    */
    my_b_seek((cur_log=relay_log.get_log_file()), (off_t)0);
    if (check_binlog_magic(cur_log, errmsg))
      goto err;
    cur_log_old_open_count=relay_log.get_open_count();
  }
  else
  {
    /*
      Open the relay log and set cur_log to point at this one
    */
    if ((cur_log_fd=open_binlog_file(&cache_buf,
                                     linfo.log_file_name,errmsg)) < 0)
      goto err;
    cur_log = &cache_buf;
  }
  /*
    In all cases, check_binlog_magic() has been called so we're at offset 4 for
    sure.
  */
  if (pos > BIN_LOG_HEADER_SIZE) /* If pos<=4, we stay at 4 */
  {
    Log_event* ev;
    while (keep_looking_for_fd)
    {
      /*
        Read the possible Format_description_log_event; if position
        was 4, no need, it will be read naturally.
      */
      DBUG_PRINT("info",("looking for a Format_description_log_event"));

      if (my_b_tell(cur_log) >= pos)
        break;

      /*
        Because of we have data_lock and log_lock, we can safely read an
        event
      */
      if (!(ev= Log_event::read_log_event(cur_log, 0,
                                          rli_description_event,
                                          opt_slave_sql_verify_checksum)))
      {
        DBUG_PRINT("info",("could not read event, cur_log->error=%d",
                           cur_log->error));
        if (cur_log->error) /* not EOF */
        {
          *errmsg= "I/O error reading event at position 4";
          goto err;
        }
        break;
      }
      else if (ev->get_type_code() == binary_log::FORMAT_DESCRIPTION_EVENT)
      {
        DBUG_PRINT("info",("found Format_description_log_event"));
        set_rli_description_event((Format_description_log_event *)ev);
        /*
          As ev was returned by read_log_event, it has passed is_valid(), so
          my_malloc() in ctor worked, no need to check again.
        */
        /*
          Ok, we found a Format_description event. But it is not sure that this
          describes the whole relay log; indeed, one can have this sequence
          (starting from position 4):
          Format_desc (of slave)
          Previous-GTIDs (of slave IO thread, if GTIDs are enabled)
          Rotate (of master)
          Format_desc (of master)
          So the Format_desc which really describes the rest of the relay log
          can be the 3rd or the 4th event (depending on GTIDs being enabled or
          not, it can't be further than that, because we rotate
          the relay log when we queue a Rotate event from the master).
          But what describes the Rotate is the first Format_desc.
          So what we do is:
          go on searching for Format_description events, until you exceed the
          position (argument 'pos') or until you find an event other than
          Previous-GTIDs, Rotate or Format_desc.
        */
      }
      else
      {
        DBUG_PRINT("info",("found event of another type=%d",
                           ev->get_type_code()));
        keep_looking_for_fd=
          (ev->get_type_code() == binary_log::ROTATE_EVENT ||
           ev->get_type_code() == binary_log::PREVIOUS_GTIDS_LOG_EVENT);
        delete ev;
      }
    }
    my_b_seek(cur_log,(off_t)pos);
#ifndef DBUG_OFF
  {
    char llbuf1[22], llbuf2[22];
    DBUG_PRINT("info", ("my_b_tell(cur_log)=%s >event_relay_log_pos=%s",
                        llstr(my_b_tell(cur_log),llbuf1),
                        llstr(get_event_relay_log_pos(),llbuf2)));
  }
#endif

  }

err:
  /*
    If we don't purge, we can't honour relay_log_space_limit ;
    silently discard it
  */
  if (!relay_log_purge)
  {
    log_space_limit= 0; // todo: consider to throw a warning at least
  }
  mysql_cond_broadcast(&data_cond);

  mysql_mutex_unlock(log_lock);

  if (need_data_lock)
    mysql_mutex_unlock(&data_lock);
  if (!rli_description_event->is_valid() && !*errmsg)
    *errmsg= "Invalid Format_description log event; could be out of memory";

  DBUG_RETURN ((*errmsg) ? 1 : 0);
}

/**
  Update the error number, message and timestamp fields. This function is
  different from va_report() as va_report() also logs the error message in the
  log apart from updating the error fields.

  SYNOPSIS
  @param[in]  level          specifies the level- error, warning or information,
  @param[in]  err_code       error number,
  @param[in]  buff_coord     error message to be used.

*/
void Relay_log_info::fill_coord_err_buf(loglevel level, int err_code,
                                      const char *buff_coord) const
{
  mysql_mutex_lock(&err_lock);

  if(level == ERROR_LEVEL)
  {
    m_last_error.number = err_code;
    my_snprintf(m_last_error.message, sizeof(m_last_error.message), "%.*s",
                MAX_SLAVE_ERRMSG - 1, buff_coord);
    m_last_error.update_timestamp();
  }

  mysql_mutex_unlock(&err_lock);
}

/**
  Waits until the SQL thread reaches (has executed up to) the
  log/position or timed out.

  SYNOPSIS
  @param[in]  thd             client thread that sent @c SELECT @c MASTER_POS_WAIT,
  @param[in]  log_name        log name to wait for,
  @param[in]  log_pos         position to wait for,
  @param[in]  timeout         @c timeout in seconds before giving up waiting.
                              @c timeout is double whereas it should be ulong; but this is
                              to catch if the user submitted a negative timeout.

  @retval  -2   improper arguments (log_pos<0)
                or slave not running, or master info changed
                during the function's execution,
                or client thread killed. -2 is translated to NULL by caller,
  @retval  -1   timed out
  @retval  >=0  number of log events the function had to wait
                before reaching the desired log/position
 */

int Relay_log_info::wait_for_pos(THD* thd, String* log_name,
                                    longlong log_pos,
                                    double timeout)
{
  int event_count = 0;
  ulong init_abort_pos_wait;
  int error=0;
  struct timespec abstime; // for timeout checking
  PSI_stage_info old_stage;
  DBUG_ENTER("Relay_log_info::wait_for_pos");

  if (!inited)
    DBUG_RETURN(-2);

  DBUG_PRINT("enter",("log_name: '%s'  log_pos: %lu  timeout: %lu",
                      log_name->c_ptr_safe(), (ulong) log_pos, (ulong) timeout));

  DEBUG_SYNC(thd, "begin_master_pos_wait");

  set_timespec_nsec(&abstime, (ulonglong)timeout * 1000000000ULL);
  mysql_mutex_lock(&data_lock);
  thd->ENTER_COND(&data_cond, &data_lock,
                  &stage_waiting_for_the_slave_thread_to_advance_position,
                  &old_stage);
  /*
     This function will abort when it notices that some CHANGE MASTER or
     RESET MASTER has changed the master info.
     To catch this, these commands modify abort_pos_wait ; We just monitor
     abort_pos_wait and see if it has changed.
     Why do we have this mechanism instead of simply monitoring slave_running
     in the loop (we do this too), as CHANGE MASTER/RESET SLAVE require that
     the SQL thread be stopped?
     This is becasue if someones does:
     STOP SLAVE;CHANGE MASTER/RESET SLAVE; START SLAVE;
     the change may happen very quickly and we may not notice that
     slave_running briefly switches between 1/0/1.
  */
  init_abort_pos_wait= abort_pos_wait;

  /*
    We'll need to
    handle all possible log names comparisons (e.g. 999 vs 1000).
    We use ulong for string->number conversion ; this is no
    stronger limitation than in find_uniq_filename in sql/log.cc
  */
  ulong log_name_extension;
  char log_name_tmp[FN_REFLEN]; //make a char[] from String

  strmake(log_name_tmp, log_name->ptr(), min<uint32>(log_name->length(), FN_REFLEN-1));

  char *p= fn_ext(log_name_tmp);
  char *p_end;
  if (!*p || log_pos<0)
  {
    error= -2; //means improper arguments
    goto err;
  }
  // Convert 0-3 to 4
  log_pos= max(log_pos, static_cast<longlong>(BIN_LOG_HEADER_SIZE));
  /* p points to '.' */
  log_name_extension= strtoul(++p, &p_end, 10);
  /*
    p_end points to the first invalid character.
    If it equals to p, no digits were found, error.
    If it contains '\0' it means conversion went ok.
  */
  if (p_end==p || *p_end)
  {
    error= -2;
    goto err;
  }

  /* The "compare and wait" main loop */
  while (!thd->killed &&
         init_abort_pos_wait == abort_pos_wait &&
         slave_running)
  {
    bool pos_reached;
    int cmp_result= 0;

    DBUG_PRINT("info",
               ("init_abort_pos_wait: %ld  abort_pos_wait: %ld",
                init_abort_pos_wait, abort_pos_wait));
    DBUG_PRINT("info",("group_master_log_name: '%s'  pos: %lu",
                       group_master_log_name, (ulong) group_master_log_pos));

    /*
      group_master_log_name can be "", if we are just after a fresh
      replication start or after a CHANGE MASTER TO MASTER_HOST/PORT
      (before we have executed one Rotate event from the master) or
      (rare) if the user is doing a weird slave setup (see next
      paragraph).  If group_master_log_name is "", we assume we don't
      have enough info to do the comparison yet, so we just wait until
      more data. In this case master_log_pos is always 0 except if
      somebody (wrongly) sets this slave to be a slave of itself
      without using --replicate-same-server-id (an unsupported
      configuration which does nothing), then group_master_log_pos
      will grow and group_master_log_name will stay "".
      Also in case the group master log position is invalid (e.g. after
      CHANGE MASTER TO RELAY_LOG_POS ), we will wait till the first event
      is read and the log position is valid again.
    */
    if (*group_master_log_name && !is_group_master_log_pos_invalid)
    {
      char *basename= (group_master_log_name +
                       dirname_length(group_master_log_name));
      /*
        First compare the parts before the extension.
        Find the dot in the master's log basename,
        and protect against user's input error :
        if the names do not match up to '.' included, return error
      */
      char *q= (fn_ext(basename)+1);
      if (strncmp(basename, log_name_tmp, (int)(q-basename)))
      {
        error= -2;
        break;
      }
      // Now compare extensions.
      char *q_end;
      ulong group_master_log_name_extension= strtoul(q, &q_end, 10);
      if (group_master_log_name_extension < log_name_extension)
        cmp_result= -1 ;
      else
        cmp_result= (group_master_log_name_extension > log_name_extension) ? 1 : 0 ;

      pos_reached= ((!cmp_result && group_master_log_pos >= (ulonglong)log_pos) ||
                    cmp_result > 0);
      if (pos_reached || thd->killed)
        break;
    }

    //wait for master update, with optional timeout.

    DBUG_PRINT("info",("Waiting for master update"));
    /*
      We are going to mysql_cond_(timed)wait(); if the SQL thread stops it
      will wake us up.
    */
    thd_wait_begin(thd, THD_WAIT_BINLOG);
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
      error= mysql_cond_timedwait(&data_cond, &data_lock, &abstime);
    }
    else
      mysql_cond_wait(&data_cond, &data_lock);
    thd_wait_end(thd);
    DBUG_PRINT("info",("Got signal of master update or timed out"));
    if (error == ETIMEDOUT || error == ETIME)
    {
#ifndef DBUG_OFF
      /*
        Doing this to generate a stack trace and make debugging
        easier. 
      */
      if (DBUG_EVALUATE_IF("debug_crash_slave_time_out", 1, 0))
        DBUG_ASSERT(0);
#endif
      error= -1;
      break;
    }
    error=0;
    event_count++;
    DBUG_PRINT("info",("Testing if killed or SQL thread not running"));
  }

err:
  mysql_mutex_unlock(&data_lock);
  thd->EXIT_COND(&old_stage);
  DBUG_PRINT("exit",("killed: %d  abort: %d  slave_running: %d \
improper_arguments: %d  timed_out: %d",
                     thd->killed_errno(),
                     (int) (init_abort_pos_wait != abort_pos_wait),
                     (int) slave_running,
                     (int) (error == -2),
                     (int) (error == -1)));
  if (thd->killed || init_abort_pos_wait != abort_pos_wait ||
      !slave_running)
  {
    error= -2;
  }
  DBUG_RETURN( error ? error : event_count );
}

int Relay_log_info::wait_for_gtid_set(THD* thd, String* gtid,
                                      double timeout)
{
  DBUG_ENTER("Relay_log_info::wait_for_gtid_set(thd, String, timeout)");

  DBUG_PRINT("info", ("Waiting for %s timeout %lf", gtid->c_ptr_safe(),
             timeout));

  Gtid_set wait_gtid_set(global_sid_map);
  global_sid_lock->rdlock();

  if (wait_gtid_set.add_gtid_text(gtid->c_ptr_safe()) != RETURN_STATUS_OK)
  {
    global_sid_lock->unlock();
    DBUG_PRINT("exit",("improper gtid argument"));
    DBUG_RETURN(-2);

  }
  global_sid_lock->unlock();

  DBUG_RETURN(wait_for_gtid_set(thd, &wait_gtid_set, timeout));
}

/*
  TODO: This is a duplicated code that needs to be simplified.
  This will be done while developing all possible sync options.
  See WL#3584's specification.

  /Alfranio
*/
int Relay_log_info::wait_for_gtid_set(THD* thd, const Gtid_set* wait_gtid_set,
                                      double timeout)
{
  int event_count = 0;
  ulong init_abort_pos_wait;
  int error=0;
  struct timespec abstime; // for timeout checking
  PSI_stage_info old_stage;
  DBUG_ENTER("Relay_log_info::wait_for_gtid_set(thd, gtid_set, timeout)");

  if (!inited)
    DBUG_RETURN(-2);

  DEBUG_SYNC(thd, "begin_wait_for_gtid_set");

  set_timespec_nsec(&abstime, (ulonglong) timeout * 1000000000ULL);

  mysql_mutex_lock(&data_lock);
  thd->ENTER_COND(&data_cond, &data_lock,
                  &stage_waiting_for_the_slave_thread_to_advance_position,
                  &old_stage);
  /*
     This function will abort when it notices that some CHANGE MASTER or
     RESET MASTER has changed the master info.
     To catch this, these commands modify abort_pos_wait ; We just monitor
     abort_pos_wait and see if it has changed.
     Why do we have this mechanism instead of simply monitoring slave_running
     in the loop (we do this too), as CHANGE MASTER/RESET SLAVE require that
     the SQL thread be stopped?
     This is becasue if someones does:
     STOP SLAVE;CHANGE MASTER/RESET SLAVE; START SLAVE;
     the change may happen very quickly and we may not notice that
     slave_running briefly switches between 1/0/1.
  */
  init_abort_pos_wait= abort_pos_wait;

  /* The "compare and wait" main loop */
  while (!thd->killed &&
         init_abort_pos_wait == abort_pos_wait &&
         slave_running)
  {
    DBUG_PRINT("info",
               ("init_abort_pos_wait: %ld  abort_pos_wait: %ld",
                init_abort_pos_wait, abort_pos_wait));

    //wait for master update, with optional timeout.

    global_sid_lock->wrlock();
    const Gtid_set* executed_gtids= gtid_state->get_executed_gtids();
    const Owned_gtids* owned_gtids= gtid_state->get_owned_gtids();

    char *wait_gtid_set_buf;
    wait_gtid_set->to_string(&wait_gtid_set_buf);
    DBUG_PRINT("info", ("Waiting for '%s'. is_subset: %d and "
                        "!is_intersection_nonempty: %d",
                        wait_gtid_set_buf,
                        wait_gtid_set->is_subset(executed_gtids),
                        !owned_gtids->is_intersection_nonempty(wait_gtid_set)));
    my_free(wait_gtid_set_buf);
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

    DBUG_PRINT("info",("Waiting for master update"));

    /*
      We are going to mysql_cond_(timed)wait(); if the SQL thread stops it
      will wake us up.
    */
    thd_wait_begin(thd, THD_WAIT_BINLOG);
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
      error= mysql_cond_timedwait(&data_cond, &data_lock, &abstime);
    }
    else
      mysql_cond_wait(&data_cond, &data_lock);
    thd_wait_end(thd);
    DBUG_PRINT("info",("Got signal of master update or timed out"));
    if (error == ETIMEDOUT || error == ETIME)
    {
#ifndef DBUG_OFF
      /*
        Doing this to generate a stack trace and make debugging
        easier. 
      */
      if (DBUG_EVALUATE_IF("debug_crash_slave_time_out", 1, 0))
        DBUG_ASSERT(0);
#endif
      error= -1;
      break;
    }
    error=0;
    event_count++;
    DBUG_PRINT("info",("Testing if killed or SQL thread not running"));
  }

  mysql_mutex_unlock(&data_lock);
  thd->EXIT_COND(&old_stage);
  DBUG_PRINT("exit",("killed: %d  abort: %d  slave_running: %d \
improper_arguments: %d  timed_out: %d",
                     thd->killed_errno(),
                     (int) (init_abort_pos_wait != abort_pos_wait),
                     (int) slave_running,
                     (int) (error == -2),
                     (int) (error == -1)));
  if (thd->killed || init_abort_pos_wait != abort_pos_wait ||
      !slave_running)
  {
    error= -2;
  }
  DBUG_RETURN( error ? error : event_count );
}

int Relay_log_info::inc_group_relay_log_pos(ulonglong log_pos,
                                            bool need_data_lock)
{
  int error= 0;
  DBUG_ENTER("Relay_log_info::inc_group_relay_log_pos");

  if (need_data_lock)
    mysql_mutex_lock(&data_lock);
  else
    mysql_mutex_assert_owner(&data_lock);

  inc_event_relay_log_pos();
  group_relay_log_pos= event_relay_log_pos;
  strmake(group_relay_log_name,event_relay_log_name,
          sizeof(group_relay_log_name)-1);

  notify_group_relay_log_name_update();

  /*
    In 4.x we used the event's len to compute the positions here. This is
    wrong if the event was 3.23/4.0 and has been converted to 5.0, because
    then the event's len is not what is was in the master's binlog, so this
    will make a wrong group_master_log_pos (yes it's a bug in 3.23->4.0
    replication: Exec_master_log_pos is wrong). Only way to solve this is to
    have the original offset of the end of the event the relay log. This is
    what we do in 5.0: log_pos has become "end_log_pos" (because the real use
    of log_pos in 4.0 was to compute the end_log_pos; so better to store
    end_log_pos instead of begin_log_pos.
    If we had not done this fix here, the problem would also have appeared
    when the slave and master are 5.0 but with different event length (for
    example the slave is more recent than the master and features the event
    UID). It would give false MASTER_POS_WAIT, false Exec_master_log_pos in
    SHOW SLAVE STATUS, and so the user would do some CHANGE MASTER using this
    value which would lead to badly broken replication.
    Even the relay_log_pos will be corrupted in this case, because the len is
    the relay log is not "val".
    With the end_log_pos solution, we avoid computations involving lengthes.
  */
  DBUG_PRINT("info", ("log_pos: %lu  group_master_log_pos: %lu",
                      (long) log_pos, (long) group_master_log_pos));

  if (log_pos > 0)  // 3.23 binlogs don't have log_posx
    group_master_log_pos= log_pos;
  /*
    If the master log position was invalidiated by say, "CHANGE MASTER TO
    RELAY_LOG_POS=N", it is now valid,
   */
  if (is_group_master_log_pos_invalid)
    is_group_master_log_pos_invalid= false;

  /*
    In MTS mode FD or Rotate event commit their solitary group to
    Coordinator's info table. Callers make sure that Workers have been
    executed all assignements.
    Broadcast to master_pos_wait() waiters should be done after
    the table is updated.
  */
  DBUG_ASSERT(!is_parallel_exec() ||
              mts_group_status != Relay_log_info::MTS_IN_GROUP);
  /*
    We do not force synchronization at this point, note the
    parameter false, because a non-transactional change is
    being committed.

    For that reason, the synchronization here is subjected to
    the option sync_relay_log_info.

    See sql/rpl_rli.h for further information on this behavior.
  */
  error= flush_info(FALSE);

  mysql_cond_broadcast(&data_cond);
  if (need_data_lock)
    mysql_mutex_unlock(&data_lock);
  DBUG_RETURN(error);
}


void Relay_log_info::close_temporary_tables()
{
  TABLE *table,*next;
  int num_closed_temp_tables= 0;
  DBUG_ENTER("Relay_log_info::close_temporary_tables");

  for (table=save_temporary_tables ; table ; table=next)
  {
    next=table->next;
    /*
      Don't ask for disk deletion. For now, anyway they will be deleted when
      slave restarts, but it is a better intention to not delete them.
    */
    DBUG_PRINT("info", ("table: 0x%lx", (long) table));
    close_temporary(table, 1, 0);
    num_closed_temp_tables++;
  }
  save_temporary_tables= 0;
  slave_open_temp_tables.atomic_add(-num_closed_temp_tables);
  channel_open_temp_tables.atomic_add(-num_closed_temp_tables);
  DBUG_VOID_RETURN;
}

/**
  Purges relay logs. It assumes to have a run lock on rli and that no
  slave thread are running.

  @param[in]   THD         connection,
  @param[in]   just_reset  if false, it tells that logs should be purged
                           and @c init_relay_log_pos() should be called,
  @errmsg[out] errmsg      store pointer to an error message.

  @retval 0 successfuly executed,
  @retval 1 otherwise error, where errmsg is set to point to the error message.
*/

int Relay_log_info::purge_relay_logs(THD *thd, bool just_reset,
                                     const char** errmsg, bool delete_only)
{
  int error=0;
  const char *ln;
  /* name of the index file if opt_relaylog_index_name is set*/
  const char* log_index_name;
  /*
    Buffer to add channel name suffix when relay-log-index option is
    provided
   */
  char relay_bin_index_channel[FN_REFLEN];

  const char *ln_without_channel_name;
  /*
    Buffer to add channel name suffix when relay-log option is provided.
   */
  char relay_bin_channel[FN_REFLEN];

  char buffer[FN_REFLEN];

  mysql_mutex_t *log_lock= relay_log.get_log_lock();

  DBUG_ENTER("Relay_log_info::purge_relay_logs");

  /*
    Even if inited==0, we still try to empty master_log_* variables. Indeed,
    inited==0 does not imply that they already are empty.

    It could be that slave's info initialization partly succeeded: for example
    if relay-log.info existed but *relay-bin*.* have been manually removed,
    init_info reads the old relay-log.info and fills rli->master_log_*, then
    init_info checks for the existence of the relay log, this fails and 
    init_info leaves inited to 0.
    In that pathological case, master_log_pos* will be properly reinited at
    the next START SLAVE (as RESET SLAVE or CHANGE MASTER, the callers of
    purge_relay_logs, will delete bogus *.info files or replace them with
    correct files), however if the user does SHOW SLAVE STATUS before START
    SLAVE, he will see old, confusing master_log_*. In other words, we reinit
    master_log_* for SHOW SLAVE STATUS to display fine in any case.
  */
  group_master_log_name[0]= 0;
  group_master_log_pos= 0;

  /*
    Following the the relay log purge, the master_log_pos will be in sync
    with relay_log_pos, so the flag should be cleared. Refer bug#11766010.
  */

  is_group_master_log_pos_invalid= false;

  if (!inited)
  {
    DBUG_PRINT("info", ("inited == 0"));
    if (error_on_rli_init_info ||
        /*
          mi->reset means that the channel was reset but still exists. Channel
          shall have the index and the first relay log file.

          Those files shall be remove in a following RESET SLAVE ALL (even when
          channel was not inited again).
        */
        (mi->reset && delete_only))
    {
      ln_without_channel_name= relay_log.generate_name(opt_relay_logname,
                                                       "-relay-bin", buffer);

      ln= add_channel_to_relay_log_name(relay_bin_channel, FN_REFLEN,
                                        ln_without_channel_name);
      if (opt_relaylog_index_name)
      {
        char index_file_withoutext[FN_REFLEN];
        relay_log.generate_name(opt_relaylog_index_name,"",
                                index_file_withoutext);

        log_index_name= add_channel_to_relay_log_name(relay_bin_index_channel,
                                                      FN_REFLEN,
                                                      index_file_withoutext);
      }
      else
        log_index_name= 0;

      if (relay_log.open_index_file(log_index_name, ln, TRUE))
      {
        sql_print_error("Unable to purge relay log files. Failed to open relay "
                        "log index file:%s.", relay_log.get_index_fname());
        DBUG_RETURN(1);
      }
      mysql_mutex_lock(&mi->data_lock);
      mysql_mutex_lock(log_lock);
      if (relay_log.open_binlog(ln, 0,
                                (max_relay_log_size ? max_relay_log_size :
                                 max_binlog_size), true,
                                true/*need_lock_index=true*/,
                                true/*need_sid_lock=true*/,
                                mi->get_mi_description_event()))
      {
        mysql_mutex_unlock(log_lock);
        mysql_mutex_unlock(&mi->data_lock);
        sql_print_error("Unable to purge relay log files. Failed to open relay "
                        "log file:%s.", relay_log.get_log_fname());
        DBUG_RETURN(1);
      }
      mysql_mutex_unlock(log_lock);
      mysql_mutex_unlock(&mi->data_lock);
    }
    else
      DBUG_RETURN(0);
  }
  else
  {
    DBUG_ASSERT(slave_running == 0);
    DBUG_ASSERT(mi->slave_running == 0);
  }
  /* Reset the transaction boundary parser and clear the last GTID queued */
  mi->transaction_parser.reset();
  mi->clear_last_gtid_queued();

  slave_skip_counter= 0;
  mysql_mutex_lock(&data_lock);

  /*
    we close the relay log fd possibly left open by the slave SQL thread,
    to be able to delete it; the relay log fd possibly left open by the slave
    I/O thread will be closed naturally in reset_logs() by the
    close(LOG_CLOSE_TO_BE_OPENED) call
  */
  if (cur_log_fd >= 0)
  {
    end_io_cache(&cache_buf);
    my_close(cur_log_fd, MYF(MY_WME));
    cur_log_fd= -1;
  }

  /**
    Clear the retrieved gtid set for this channel.
    global_sid_lock->wrlock() is needed.
  */
  global_sid_lock->wrlock();
  (const_cast<Gtid_set *>(get_gtid_set()))->clear();
  global_sid_lock->unlock();

  if (relay_log.reset_logs(thd, delete_only))
  {
    *errmsg = "Failed during log reset";
    error=1;
    goto err;
  }

  /* Save name of used relay log file */
  set_group_relay_log_name(relay_log.get_log_fname());
  set_event_relay_log_name(relay_log.get_log_fname());
  group_relay_log_pos= event_relay_log_pos= BIN_LOG_HEADER_SIZE;
  if (!delete_only && count_relay_log_space())
  {
    *errmsg= "Error counting relay log space";
    error= 1;
    goto err;
  }
  if (!just_reset)
    error= init_relay_log_pos(group_relay_log_name,
                              group_relay_log_pos,
                              false/*need_data_lock=false*/, errmsg, 0);
  if (!inited && error_on_rli_init_info)
    relay_log.close(LOG_CLOSE_INDEX | LOG_CLOSE_STOP_EVENT,
                    true/*need_lock_log=true*/,
                    true/*need_lock_index=true*/);
err:
#ifndef DBUG_OFF
  char buf[22];
#endif
  DBUG_PRINT("info",("log_space_total: %s",llstr(log_space_total,buf)));
  mysql_mutex_unlock(&data_lock);
  DBUG_RETURN(error);
}

/*
   When --relay-bin option is not provided, the names of the
   relay log files are host-relay-bin.0000x or
   host-relay-bin-CHANNEL.00000x in the case of MSR.
   However, if that option is provided, then the names of the
   relay log files are <relay-bin-option>.0000x or
   <relay-bin-option>-CHANNEL.00000x in the case of MSR.

   The function adds a channel suffix (according to the channel to file name
   conventions and conversions) to the relay log file.

   @todo: truncate the log file if length exceeds.
*/

const char*
Relay_log_info::add_channel_to_relay_log_name(char *buff, uint buff_size,
                                              const char *base_name)
{
  char *ptr;
  char channel_to_file[FN_REFLEN];
  uint errors, length;
  uint base_name_len;
  uint suffix_buff_size;

  DBUG_ASSERT(base_name !=NULL);

  base_name_len= strlen(base_name);
  suffix_buff_size= buff_size - base_name_len;

  ptr= strmake(buff, base_name, buff_size-1);

  if (channel[0])
  {

   /* adding a "-" */
    ptr= strmake(ptr, "-", suffix_buff_size-1);

    /*
      Convert the channel name to the file names charset.
      Channel name is in system_charset which is UTF8_general_ci
      as it was defined as utf8 in the mysql.slaveinfo tables.
    */
    length= strconvert(system_charset_info, channel, &my_charset_filename,
                       channel_to_file, NAME_LEN, &errors);
    ptr= strmake(ptr, channel_to_file, suffix_buff_size-length-1);

  }

  return (const char*)buff;
}


/**
     Checks if condition stated in UNTIL clause of START SLAVE is reached.

     Specifically, it checks if UNTIL condition is reached. Uses caching result
     of last comparison of current log file name and target log file name. So
     cached value should be invalidated if current log file name changes (see
     @c Relay_log_info::notify_... functions).

     This caching is needed to avoid of expensive string comparisons and
     @c strtol() conversions needed for log names comparison. We don't need to
     compare them each time this function is called, we only need to do this
     when current log name changes. If we have @c UNTIL_MASTER_POS condition we
     need to do this only after @c Rotate_log_event::do_apply_event() (which is
     rare, so caching gives real benifit), and if we have @c UNTIL_RELAY_POS
     condition then we should invalidate cached comarison value after
     @c inc_group_relay_log_pos() which called for each group of events (so we
     have some benefit if we have something like queries that use
     autoincrement or if we have transactions).

     Should be called ONLY if @c until_condition @c != @c UNTIL_NONE !

     @param master_beg_pos    position of the beginning of to be executed event
                              (not @c log_pos member of the event that points to
                              the beginning of the following event)

     @retval true   condition met or error happened (condition seems to have
                    bad log file name),
     @retval false  condition not met.
*/

bool Relay_log_info::is_until_satisfied(THD *thd, Log_event *ev)
{
  char error_msg[]= "Slave SQL thread is stopped because UNTIL "
                    "condition is bad.";
  DBUG_ENTER("Relay_log_info::is_until_satisfied");

  switch (until_condition)
  {
  case UNTIL_MASTER_POS:
  case UNTIL_RELAY_POS:
  {
    const char *log_name= NULL;
    ulonglong log_pos= 0;

    if (until_condition == UNTIL_MASTER_POS)
    {
      if (ev && ev->server_id == (uint32) ::server_id && !replicate_same_server_id)
        DBUG_RETURN(false);
      /*
        Rotate events originating from the slave have server_id==0,
        and their log_pos is relative to the slave, so in case their
        log_pos is greater than the log_pos we are waiting for, they
        can cause the slave to stop prematurely. So we ignore such
        events.
      */
      if (ev && ev->server_id == 0)
        DBUG_RETURN(false);
      log_name= group_master_log_name;
      if (!ev || is_in_group() || !ev->common_header->log_pos)
        log_pos= group_master_log_pos;
      else
        log_pos= ev->common_header->log_pos - ev->common_header->data_written;
    }
    else
    { /* until_condition == UNTIL_RELAY_POS */
      log_name= group_relay_log_name;
      log_pos= group_relay_log_pos;
    }

#ifndef DBUG_OFF
    {
      char buf[32];
      DBUG_PRINT("info", ("group_master_log_name='%s', group_master_log_pos=%s",
                          group_master_log_name, llstr(group_master_log_pos, buf)));
      DBUG_PRINT("info", ("group_relay_log_name='%s', group_relay_log_pos=%s",
                          group_relay_log_name, llstr(group_relay_log_pos, buf)));
      DBUG_PRINT("info", ("(%s) log_name='%s', log_pos=%s",
                          until_condition == UNTIL_MASTER_POS ? "master" : "relay",
                          log_name, llstr(log_pos, buf)));
      DBUG_PRINT("info", ("(%s) until_log_name='%s', until_log_pos=%s",
                          until_condition == UNTIL_MASTER_POS ? "master" : "relay",
                          until_log_name, llstr(until_log_pos, buf)));
    }
#endif

    if (until_log_names_cmp_result == UNTIL_LOG_NAMES_CMP_UNKNOWN)
    {
      /*
        We have no cached comparison results so we should compare log names
        and cache result.
        If we are after RESET SLAVE, and the SQL slave thread has not processed
        any event yet, it could be that group_master_log_name is "". In that case,
        just wait for more events (as there is no sensible comparison to do).
      */

      if (*log_name)
      {
        const char *basename= log_name + dirname_length(log_name);

        const char *q= (const char*)(fn_ext(basename)+1);
        if (strncmp(basename, until_log_name, (int)(q-basename)) == 0)
        {
          /* Now compare extensions. */
          char *q_end;
          ulong log_name_extension= strtoul(q, &q_end, 10);
          if (log_name_extension < until_log_name_extension)
            until_log_names_cmp_result= UNTIL_LOG_NAMES_CMP_LESS;
          else
            until_log_names_cmp_result=
              (log_name_extension > until_log_name_extension) ?
              UNTIL_LOG_NAMES_CMP_GREATER : UNTIL_LOG_NAMES_CMP_EQUAL ;
        }
        else
        {
          /* Base names do not match, so we abort */
          sql_print_error("%s", error_msg);
          DBUG_RETURN(true);
        }
      }
      else
        DBUG_RETURN(until_log_pos == 0);
    }

    if (((until_log_names_cmp_result == UNTIL_LOG_NAMES_CMP_EQUAL &&
          log_pos >= until_log_pos) ||
         until_log_names_cmp_result == UNTIL_LOG_NAMES_CMP_GREATER))
    {
      char buf[22];
      sql_print_information("Slave SQL thread stopped because it reached its"
                            " UNTIL position %s", llstr(until_pos(), buf));
      DBUG_RETURN(true);
    }
    DBUG_RETURN(false);
  }

  case UNTIL_SQL_BEFORE_GTIDS:
    /*
      We only need to check once if executed_gtids set
      contains any of the until_sql_gtids.
    */
    if (until_sql_gtids_first_event)
    {
      until_sql_gtids_first_event= false;
      global_sid_lock->wrlock();
      /* Check if until GTIDs were already applied. */
      const Gtid_set* executed_gtids= gtid_state->get_executed_gtids();
      if (until_sql_gtids.is_intersection_nonempty(executed_gtids))
      {
        char *buffer;
        until_sql_gtids.to_string(&buffer);
        global_sid_lock->unlock();
        sql_print_information("Slave SQL thread stopped because "
                              "UNTIL SQL_BEFORE_GTIDS %s is already "
                              "applied", buffer);
        my_free(buffer);
        DBUG_RETURN(true);
      }
      global_sid_lock->unlock();
    }
    if (ev != NULL && ev->get_type_code() == binary_log::GTID_LOG_EVENT)
    {
      Gtid_log_event *gev= (Gtid_log_event *)ev;
      global_sid_lock->rdlock();
      if (until_sql_gtids.contains_gtid(gev->get_sidno(false), gev->get_gno()))
      {
        char *buffer;
        until_sql_gtids.to_string(&buffer);
        global_sid_lock->unlock();
        sql_print_information("Slave SQL thread stopped because it reached "
                              "UNTIL SQL_BEFORE_GTIDS %s", buffer);
        my_free(buffer);
        DBUG_RETURN(true);
      }
      global_sid_lock->unlock();
    }
    DBUG_RETURN(false);
    break;

  case UNTIL_SQL_AFTER_GTIDS:
    {
      global_sid_lock->wrlock();
      const Gtid_set* executed_gtids= gtid_state->get_executed_gtids();
      if (until_sql_gtids.is_subset(executed_gtids))
      {
        char *buffer;
        until_sql_gtids.to_string(&buffer);
        global_sid_lock->unlock();
        sql_print_information("Slave SQL thread stopped because it reached "
                              "UNTIL SQL_AFTER_GTIDS %s", buffer);
        my_free(buffer);
        DBUG_RETURN(true);
      }
      global_sid_lock->unlock();
      DBUG_RETURN(false);
    }
    break;

  case UNTIL_SQL_AFTER_MTS_GAPS:
  case UNTIL_DONE:
    /*
      TODO: this condition is actually post-execution or post-scheduling
            so the proper place to check it before SQL thread goes
            into next_event() where it can wait while the condition
            has been satisfied already.
            It's deployed here temporarily to be fixed along the regular UNTIL
            support for MTS is provided.
    */
    if (mts_recovery_group_cnt == 0)
    {
      sql_print_information("Slave SQL thread stopped according to "
                            "UNTIL SQL_AFTER_MTS_GAPS as it has "
                            "processed all gap transactions left from "
                            "the previous slave session.");
      until_condition= UNTIL_DONE;
      DBUG_RETURN(true);
    }
    else
    {
      DBUG_RETURN(false);
    }
    break;

  case UNTIL_SQL_VIEW_ID:
    if (ev != NULL && ev->get_type_code() == binary_log::VIEW_CHANGE_EVENT)
    {
      View_change_log_event *view_event= (View_change_log_event *)ev;

      if (until_view_id.compare(view_event->get_view_id()) == 0)
      {
        set_group_replication_retrieved_certification_info(view_event);
        until_view_id_found= true;
        DBUG_RETURN(false);
      }
    }

    if (until_view_id_found && ev != NULL && ev->ends_group())
    {
      until_view_id_commit_found= true;
      DBUG_RETURN(false);
    }

    if (until_view_id_commit_found && ev == NULL)
    {
      DBUG_RETURN(true);
    }

    DBUG_RETURN(false);
    break;

  case UNTIL_NONE:
    DBUG_ASSERT(0);
    break;
  }

  DBUG_ASSERT(0);
  DBUG_RETURN(false);
}

void Relay_log_info::cached_charset_invalidate()
{
  DBUG_ENTER("Relay_log_info::cached_charset_invalidate");

  /* Full of zeroes means uninitialized. */
  memset(cached_charset, 0, sizeof(cached_charset));
  DBUG_VOID_RETURN;
}


bool Relay_log_info::cached_charset_compare(char *charset) const
{
  DBUG_ENTER("Relay_log_info::cached_charset_compare");

  if (memcmp(cached_charset, charset, sizeof(cached_charset)))
  {
    memcpy(const_cast<char*>(cached_charset), charset, sizeof(cached_charset));
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


int Relay_log_info::stmt_done(my_off_t event_master_log_pos)
{
  int error= 0;

  clear_flag(IN_STMT);

  DBUG_ASSERT(!belongs_to_client());
  /* Worker does not execute binlog update position logics */
  DBUG_ASSERT(!is_mts_worker(info_thd));

  /*
    Replication keeps event and group positions to specify the
    set of events that were executed.
    Event positions are incremented after processing each event
    whereas group positions are incremented when an event or a
    set of events is processed such as in a transaction and are
    committed or rolled back.

    A transaction can be ended with a Query Event, i.e. either
    commit or rollback, or by a Xid Log Event. Query Event is
    used to terminate pseudo-transactions that are executed
    against non-transactional engines such as MyIsam. Xid Log
    Event denotes though that a set of changes executed
    against a transactional engine is about to commit.

    Events' positions are incremented at stmt_done(). However,
    transactions that are ended with Xid Log Event have their
    group position incremented in the do_apply_event() and in
    the do_apply_event_work().

    Notice that the type of the engine, i.e. where data and
    positions are stored, against what events are being applied
    are not considered in this logic.

    Regarding the code that follows, notice that the executed
    group coordinates don't change if the current event is internal
    to the group. The same applies to MTS Coordinator when it
    handles a Format Descriptor event that appears in the middle
    of a group that is about to be assigned.
  */
  if ((!is_parallel_exec() && is_in_group()) ||
      mts_group_status != MTS_NOT_IN_GROUP)
  {
    inc_event_relay_log_pos();
  }
  else
  {
    if (is_parallel_exec())
    {

      DBUG_ASSERT(!is_mts_worker(info_thd));

      /*
        Format Description events only can drive MTS execution to this
        point. It is a special event group that is handled with
        synchronization. For that reason, the checkpoint routine is
        called here.
      */
      error= mts_checkpoint_routine(this, 0, false,
                                    true/*need_data_lock=true*/);
    }
    if (!error)
      error= inc_group_relay_log_pos(event_master_log_pos,
                                     true/*need_data_lock=true*/);
  }

  return error;
}

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
void Relay_log_info::cleanup_context(THD *thd, bool error)
{
  DBUG_ENTER("Relay_log_info::cleanup_context");

  DBUG_ASSERT(info_thd == thd);
  /*
    1) Instances of Table_map_log_event, if ::do_apply_event() was called on them,
    may have opened tables, which we cannot be sure have been closed (because
    maybe the Rows_log_event have not been found or will not be, because slave
    SQL thread is stopping, or relay log has a missing tail etc). So we close
    all thread's tables. And so the table mappings have to be cancelled.
    2) Rows_log_event::do_apply_event() may even have started statements or
    transactions on them, which we need to rollback in case of error.
    3) If finding a Format_description_log_event after a BEGIN, we also need
    to rollback before continuing with the next events.
    4) so we need this "context cleanup" function.
  */
  if (error)
  {
    trans_rollback_stmt(thd); // if a "statement transaction"
    trans_rollback(thd);      // if a "real transaction"
  }
  if (rows_query_ev)
  {
    /*
      In order to avoid invalid memory access, THD::reset_query() should be
      called before deleting the rows_query event.
    */
    info_thd->reset_query();
    delete rows_query_ev;
    rows_query_ev= NULL;
    DBUG_EXECUTE_IF("after_deleting_the_rows_query_ev",
                    {
                      const char action[]="now SIGNAL deleted_rows_query_ev WAIT_FOR go_ahead";
                      DBUG_ASSERT(!debug_sync_set_action(info_thd,
                                                       STRING_WITH_LEN(action)));
                    };);
  }
  m_table_map.clear_tables();
  slave_close_thread_tables(thd);
  if (error)
  {
    /*
      trans_rollback above does not rollback XA transactions.
      It could be done only after necessarily closing tables which dictates
      the following placement.
    */
    XID_STATE *xid_state= thd->get_transaction()->xid_state();
    if (!xid_state->has_state(XID_STATE::XA_NOTR))
    {
      DBUG_ASSERT(DBUG_EVALUATE_IF("simulate_commit_failure",1,
                                   xid_state->has_state(XID_STATE::XA_ACTIVE)));

      xa_trans_force_rollback(thd);
      xid_state->reset();
      cleanup_trans_state(thd);
    }
    thd->mdl_context.release_transactional_locks();
  }
  clear_flag(IN_STMT);
  /*
    Cleanup for the flags that have been set at do_apply_event.
  */
  thd->variables.option_bits&= ~OPTION_NO_FOREIGN_KEY_CHECKS;
  thd->variables.option_bits&= ~OPTION_RELAXED_UNIQUE_CHECKS;

  /*
    Reset state related to long_find_row notes in the error log:
    - timestamp
    - flag that decides whether the slave prints or not
  */
  reset_row_stmt_start_timestamp();
  unset_long_find_row_note_printed();

  /*
    If the slave applier changed the current transaction isolation level,
    it need to be restored to the session default value once having the
    current transaction cleared.

    We should call "trans_reset_one_shot_chistics()" only if the "error"
    flag is "true", because "cleanup_context()" is called at the end of each
    set of Table_maps/Rows representing a statement (when the rows event
    is tagged with the STMT_END_F) with the "error" flag as "false".

    So, without the "if (error)" below, the isolation level might be reset
    in the middle of a pure row based transaction.
  */
  if (error)
    trans_reset_one_shot_chistics(thd);

  DBUG_VOID_RETURN;
}

void Relay_log_info::clear_tables_to_lock()
{
  DBUG_ENTER("Relay_log_info::clear_tables_to_lock()");
#ifndef DBUG_OFF
  /**
    When replicating in RBR and MyISAM Merge tables are involved
    open_and_lock_tables (called in do_apply_event) appends the 
    base tables to the list of tables_to_lock. Then these are 
    removed from the list in close_thread_tables (which is called 
    before we reach this point).

    This assertion just confirms that we get no surprises at this
    point.
   */
  uint i=0;
  for (TABLE_LIST *ptr= tables_to_lock ; ptr ; ptr= ptr->next_global, i++) ;
  DBUG_ASSERT(i == tables_to_lock_count);
#endif  

  while (tables_to_lock)
  {
    uchar* to_free= reinterpret_cast<uchar*>(tables_to_lock);
    if (tables_to_lock->m_tabledef_valid)
    {
      tables_to_lock->m_tabledef.table_def::~table_def();
      tables_to_lock->m_tabledef_valid= FALSE;
    }

    /*
      If blob fields were used during conversion of field values 
      from the master table into the slave table, then we need to 
      free the memory used temporarily to store their values before
      copying into the slave's table.
    */
    if (tables_to_lock->m_conv_table)
      free_blobs(tables_to_lock->m_conv_table);

    tables_to_lock=
      static_cast<RPL_TABLE_LIST*>(tables_to_lock->next_global);
    tables_to_lock_count--;
    my_free(to_free);
  }
  DBUG_ASSERT(tables_to_lock == NULL && tables_to_lock_count == 0);
  DBUG_VOID_RETURN;
}

void Relay_log_info::slave_close_thread_tables(THD *thd)
{
  thd->get_stmt_da()->set_overwrite_status(true);
  DBUG_ENTER("Relay_log_info::slave_close_thread_tables(THD *thd)");
  thd->is_error() ? trans_rollback_stmt(thd) : trans_commit_stmt(thd);
  thd->get_stmt_da()->set_overwrite_status(false);

  close_thread_tables(thd);
  /*
    - If transaction rollback was requested due to deadlock
    perform it and release metadata locks.
    - If inside a multi-statement transaction,
    defer the release of metadata locks until the current
    transaction is either committed or rolled back. This prevents
    other statements from modifying the table for the entire
    duration of this transaction.  This provides commit ordering
    and guarantees serializability across multiple transactions.
    - If in autocommit mode, or outside a transactional context,
    automatically release metadata locks of the current statement.
  */
  if (thd->transaction_rollback_request)
  {
    trans_rollback_implicit(thd);
    thd->mdl_context.release_transactional_locks();
  }
  else if (! thd->in_multi_stmt_transaction_mode())
    thd->mdl_context.release_transactional_locks();
  else
    thd->mdl_context.release_statement_locks();

  clear_tables_to_lock();
  DBUG_VOID_RETURN;
}


/**
  Execute a SHOW RELAYLOG EVENTS statement.

  When multiple replication channels exist on this slave
  and no channel name is specified through FOR CHANNEL clause
  this function errors out and exits.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @retval FALSE success
  @retval TRUE failure
*/
bool mysql_show_relaylog_events(THD* thd)
{

  Master_info *mi =0;
  List<Item> field_list;
  bool res;
  DBUG_ENTER("mysql_show_relaylog_events");

  DBUG_ASSERT(thd->lex->sql_command == SQLCOM_SHOW_RELAYLOG_EVENTS);

  channel_map.wrlock();

  if (!thd->lex->mi.for_channel && channel_map.get_num_instances() > 1)
  {
    my_error(ER_SLAVE_MULTIPLE_CHANNELS_CMD, MYF(0));
    res= true;
    goto err;
  }

  Log_event::init_show_field_list(&field_list);
  if (thd->send_result_metadata(&field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
  {
    res= true;
    goto err;
  }

  mi= channel_map.get_mi(thd->lex->mi.channel);

  if (!mi && strcmp(thd->lex->mi.channel, channel_map.get_default_channel()))
  {
    my_error(ER_SLAVE_CHANNEL_DOES_NOT_EXIST, MYF(0), thd->lex->mi.channel);
    res= true;
    goto err;
  }

  if (mi == NULL)
  {
    my_error(ER_SLAVE_CONFIGURATION, MYF(0));
    res= true;
    goto err;
  }

  res= show_binlog_events(thd, &mi->rli->relay_log);

err:
  channel_map.unlock();

  DBUG_RETURN(res);
}
#endif


int Relay_log_info::rli_init_info()
{
  int error= 0;
  enum_return_check check_return= ERROR_CHECKING_REPOSITORY;
  const char *msg= NULL;
  /* Store the GTID of a transaction spanned in multiple relay log files */
  Gtid gtid_partial_trx= {0, 0};

  DBUG_ENTER("Relay_log_info::rli_init_info");

  mysql_mutex_assert_owner(&data_lock);

  /*
    If Relay_log_info is issued again after a failed init_info(), for
    instance because of missing relay log files, it will generate new
    files and ignore the previous failure, to avoid that we set
    error_on_rli_init_info as true.
    This a consequence of the behaviour change, in the past server was
    stopped when there were replication initialization errors, now it is
    not and so init_info() must be aware of previous failures.
  */
  if (error_on_rli_init_info)
    goto err;

  if (inited)
  {
    /*
      We have to reset read position of relay-log-bin as we may have
      already been reading from 'hotlog' when the slave was stopped
      last time. If this case pos_in_file would be set and we would
      get a crash when trying to read the signature for the binary
      relay log.

      We only rewind the read position if we are starting the SQL
      thread. The handle_slave_sql thread assumes that the read
      position is at the beginning of the file, and will read the
      "signature" and then fast-forward to the last position read.
    */
    bool hot_log= FALSE;
    /* 
      my_b_seek does an implicit flush_io_cache, so we need to:

      1. check if this log is active (hot)
      2. if it is we keep log_lock until the seek ends, otherwise 
         release it right away.

      If we did not take log_lock, SQL thread might race with IO
      thread for the IO_CACHE mutex.

    */
    mysql_mutex_t *log_lock= relay_log.get_log_lock();
    mysql_mutex_lock(log_lock);
    hot_log= relay_log.is_active(linfo.log_file_name);

    if (!hot_log)
      mysql_mutex_unlock(log_lock);

    my_b_seek(cur_log, (my_off_t) 0);

    if (hot_log)
      mysql_mutex_unlock(log_lock);
    DBUG_RETURN(recovery_parallel_workers ? mts_recovery_groups(this) : 0);
  }

  cur_log_fd = -1;
  slave_skip_counter= 0;
  abort_pos_wait= 0;
  log_space_limit= relay_log_space_limit;
  log_space_total= 0;
  tables_to_lock= 0;
  tables_to_lock_count= 0;

  char pattern[FN_REFLEN];
  (void) my_realpath(pattern, slave_load_tmpdir, 0);
  /*
   @TODO:
    In MSR, sometimes slave fail with the following error:
    Unable to use slave's temporary directory /tmp - 
    Can't create/write to file '/tmp/SQL_LOAD-92d1eee0-9de4-11e3-8874-68730ad50fcb'    (Errcode: 17 - File exists), Error_code: 1

   */
  if (fn_format(pattern, PREFIX_SQL_LOAD, pattern, "",
                MY_SAFE_PATH | MY_RETURN_REAL_PATH) == NullS)
  {
    sql_print_error("Unable to use slave's temporary directory '%s'.",
                    slave_load_tmpdir);
    DBUG_RETURN(1);
  }
  unpack_filename(slave_patternload_file, pattern);
  slave_patternload_file_size= strlen(slave_patternload_file);

  /*
    The relay log will now be opened, as a SEQ_READ_APPEND IO_CACHE.
    Note that the I/O thread flushes it to disk after writing every
    event, in flush_info within the master info.
  */
  /*
    For the maximum log size, we choose max_relay_log_size if it is
    non-zero, max_binlog_size otherwise. If later the user does SET
    GLOBAL on one of these variables, fix_max_binlog_size and
    fix_max_relay_log_size will reconsider the choice (for example
    if the user changes max_relay_log_size to zero, we have to
    switch to using max_binlog_size for the relay log) and update
    relay_log.max_size (and mysql_bin_log.max_size).
  */
  {
    /* Reports an error and returns, if the --relay-log's path
       is a directory.*/
    if (opt_relay_logname &&
        opt_relay_logname[strlen(opt_relay_logname) - 1] == FN_LIBCHAR)
    {
      sql_print_error("Path '%s' is a directory name, please specify \
a file name for --relay-log option.", opt_relay_logname);
      DBUG_RETURN(1);
    }

    /* Reports an error and returns, if the --relay-log-index's path
       is a directory.*/
    if (opt_relaylog_index_name &&
        opt_relaylog_index_name[strlen(opt_relaylog_index_name) - 1]
        == FN_LIBCHAR)
    {
      sql_print_error("Path '%s' is a directory name, please specify \
a file name for --relay-log-index option.", opt_relaylog_index_name);
      DBUG_RETURN(1);
    }

    char buf[FN_REFLEN];
    /* The base name of the relay log file considering multisource rep */
    const char *ln;
    /*
      relay log name without channel prefix taking into account
      --relay-log option.
    */
    const char *ln_without_channel_name;
    static bool name_warning_sent= 0;

    /*
      Buffer to add channel name suffix when relay-log option is provided.
    */
    char relay_bin_channel[FN_REFLEN];
    /*
      Buffer to add channel name suffix when relay-log-index option is provided
    */
    char relay_bin_index_channel[FN_REFLEN];

    /* name of the index file if opt_relaylog_index_name is set*/
    const char* log_index_name;


    ln_without_channel_name= relay_log.generate_name(opt_relay_logname,
                                "-relay-bin", buf);

    ln= add_channel_to_relay_log_name(relay_bin_channel, FN_REFLEN,
                                      ln_without_channel_name);

    /* We send the warning only at startup, not after every RESET SLAVE */
    if (!opt_relay_logname && !opt_relaylog_index_name && !name_warning_sent)
    {
      /*
        User didn't give us info to name the relay log index file.
        Picking `hostname`-relay-bin.index like we do, causes replication to
        fail if this slave's hostname is changed later. So, we would like to
        instead require a name. But as we don't want to break many existing
        setups, we only give warning, not error.
      */
      sql_print_warning("Neither --relay-log nor --relay-log-index were used;"
                        " so replication "
                        "may break when this MySQL server acts as a "
                        "slave and has his hostname changed!! Please "
                        "use '--relay-log=%s' to avoid this problem.",
                        ln_without_channel_name);
      name_warning_sent= 1;
    }

    relay_log.is_relay_log= TRUE;

    /*
       If relay log index option is set, convert into channel specific
       index file. If the opt_relaylog_index has an extension, we strip
       it too. This is inconsistent to relay log names.
    */
    if (opt_relaylog_index_name)
    {
      char index_file_withoutext[FN_REFLEN];
      relay_log.generate_name(opt_relaylog_index_name,"",
                              index_file_withoutext);

      log_index_name= add_channel_to_relay_log_name(relay_bin_index_channel,
                                                   FN_REFLEN,
                                                   index_file_withoutext);
    }
    else
      log_index_name= 0;



    if (relay_log.open_index_file(log_index_name, ln, TRUE))
    {
      sql_print_error("Failed in open_index_file() called from Relay_log_info::rli_init_info().");
      DBUG_RETURN(1);
    }
#ifndef DBUG_OFF
    global_sid_lock->wrlock();
    gtid_set.dbug_print("set of GTIDs in relay log before initialization");
    global_sid_lock->unlock();
#endif
    /*
      In the init_gtid_set below we pass the mi->transaction_parser.
      This will be useful to ensure that we only add a GTID to
      the Retrieved_Gtid_Set for fully retrieved transactions. Also, it will
      be useful to ensure the Retrieved_Gtid_Set behavior when auto
      positioning is disabled (we could have transactions spanning multiple
      relay log files in this case).
      We will skip this initialization if relay_log_recovery is set in order
      to save time, as neither the GTIDs nor the transaction_parser state
      would be useful when the relay log will be cleaned up later when calling
      init_recovery.
    */
    if (!is_relay_log_recovery &&
        !gtid_retrieved_initialized &&
        relay_log.init_gtid_sets(&gtid_set, NULL,
                                 opt_slave_sql_verify_checksum,
                                 true/*true=need lock*/,
                                 &mi->transaction_parser, &gtid_partial_trx))
    {
      sql_print_error("Failed in init_gtid_sets() called from Relay_log_info::rli_init_info().");
      DBUG_RETURN(1);
    }
    gtid_retrieved_initialized= true;
#ifndef DBUG_OFF
    global_sid_lock->wrlock();
    gtid_set.dbug_print("set of GTIDs in relay log after initialization");
    global_sid_lock->unlock();
#endif
    if (!gtid_partial_trx.is_empty())
    {
      /*
        The init_gtid_set has found an incomplete transaction in the relay log.
        We add this transaction's GTID to the last_gtid_queued so the IO thread
        knows which GTID to add to the Retrieved_Gtid_Set when reaching the end
        of the incomplete transaction.
      */
      mi->set_last_gtid_queued(gtid_partial_trx);
    }
    else
    {
      mi->clear_last_gtid_queued();
    }
    /*
      Configures what object is used by the current log to store processed
      gtid(s). This is necessary in the MYSQL_BIN_LOG::MYSQL_BIN_LOG to
      corretly compute the set of previous gtids.
    */
    relay_log.set_previous_gtid_set_relaylog(&gtid_set);
    /*
      note, that if open() fails, we'll still have index file open
      but a destructor will take care of that
    */

    mysql_mutex_t *log_lock= relay_log.get_log_lock();
    mysql_mutex_lock(log_lock);

    if (relay_log.open_binlog(ln, 0,
                              (max_relay_log_size ? max_relay_log_size :
                               max_binlog_size), true,
                              true/*need_lock_index=true*/,
                              true/*need_sid_lock=true*/,
                              mi->get_mi_description_event()))
    {
      mysql_mutex_unlock(log_lock);
      sql_print_error("Failed in open_log() called from Relay_log_info::rli_init_info().");
      DBUG_RETURN(1);
    }

    mysql_mutex_unlock(log_lock);

  }

   /*
    This checks if the repository was created before and thus there
    will be values to be read. Please, do not move this call after
    the handler->init_info(). 
  */
  if ((check_return= check_info()) == ERROR_CHECKING_REPOSITORY)
  {
    msg= "Error checking relay log repository";
    error= 1;
    goto err;
  }

  if (handler->init_info())
  {
    msg= "Error reading relay log configuration";
    error= 1;
    goto err;
  }

  if (check_return == REPOSITORY_DOES_NOT_EXIST)
  {
    /* Init relay log with first entry in the relay index file */
    if (init_relay_log_pos(NullS, BIN_LOG_HEADER_SIZE,
                           false/*need_data_lock=false (lock should be held
                                  prior to invoking this function)*/,
                           &msg, 0))
    {
      error= 1;
      goto err;
    }
    group_master_log_name[0]= 0;
    group_master_log_pos= 0;
  }
  else
  {
    if (read_info(handler))
    {
      msg= "Error reading relay log configuration";
      error= 1;
      goto err;
    }

    if (is_relay_log_recovery && init_recovery(mi, &msg))
    {
      error= 1;
      goto err;
    }

    if (init_relay_log_pos(group_relay_log_name,
                           group_relay_log_pos,
                           false/*need_data_lock=false (lock should be held
                                  prior to invoking this function)*/,
                           &msg, 0))
    {
      char llbuf[22];
      sql_print_error("Failed to open the relay log '%s' (relay_log_pos %s).",
                      group_relay_log_name,
                      llstr(group_relay_log_pos, llbuf));
      error= 1;
      goto err;
    }

#ifndef DBUG_OFF
    {
      char llbuf1[22], llbuf2[22];
      DBUG_PRINT("info", ("my_b_tell(cur_log)=%s event_relay_log_pos=%s",
                          llstr(my_b_tell(cur_log),llbuf1),
                          llstr(event_relay_log_pos,llbuf2)));
      DBUG_ASSERT(event_relay_log_pos >= BIN_LOG_HEADER_SIZE);
      DBUG_ASSERT((my_b_tell(cur_log) == event_relay_log_pos));
    }
#endif
  }

  inited= 1;
  error_on_rli_init_info= false;
  if (flush_info(TRUE))
  {
    msg= "Error reading relay log configuration";
    error= 1;
    goto err;
  }

  if (count_relay_log_space())
  {
    msg= "Error counting relay log space";
    error= 1;
    goto err;
  }

  /*
    In case of MTS the recovery is deferred until the end of
    load_mi_and_rli_from_repositories.
  */
  if (!mi->rli->mts_recovery_group_cnt)
    is_relay_log_recovery= FALSE;
  DBUG_RETURN(error);

err:
  handler->end_info();
  inited= 0;
  error_on_rli_init_info= true;
  if (msg)
    sql_print_error("%s.", msg);
  relay_log.close(LOG_CLOSE_INDEX | LOG_CLOSE_STOP_EVENT,
                  true/*need_lock_log=true*/,
                  true/*need_lock_index=true*/);
  DBUG_RETURN(error);
}

void Relay_log_info::end_info()
{
  DBUG_ENTER("Relay_log_info::end_info");

  error_on_rli_init_info= false;
  if (!inited)
    DBUG_VOID_RETURN;

  handler->end_info();

  if (cur_log_fd >= 0)
  {
    end_io_cache(&cache_buf);
    (void)my_close(cur_log_fd, MYF(MY_WME));
    cur_log_fd= -1;
  }
  inited = 0;
  relay_log.close(LOG_CLOSE_INDEX | LOG_CLOSE_STOP_EVENT,
                  true/*need_lock_log=true*/,
                  true/*need_lock_index=true*/);
  relay_log.harvest_bytes_written(&log_space_total);
  /*
    Delete the slave's temporary tables from memory.
    In the future there will be other actions than this, to ensure persistance
    of slave's temp tables after shutdown.
  */
  close_temporary_tables();

  DBUG_VOID_RETURN;
}

int Relay_log_info::flush_current_log()
{
  DBUG_ENTER("Relay_log_info::flush_current_log");
  /*
    When we come to this place in code, relay log may or not be initialized;
    the caller is responsible for setting 'flush_relay_log_cache' accordingly.
  */
  IO_CACHE *log_file= relay_log.get_log_file();
  if (flush_io_cache(log_file))
    DBUG_RETURN(2);

  DBUG_RETURN(0);
}

void Relay_log_info::set_master_info(Master_info* info)
{
  mi= info;
}

/**
  Stores the file and position where the execute-slave thread are in the
  relay log:

    - As this is only called by the slave thread or on STOP SLAVE, with the
      log_lock grabbed and the slave thread stopped, we don't need to have
      a lock here.
    - If there is an active transaction, then we don't update the position
      in the relay log.  This is to ensure that we re-execute statements
      if we die in the middle of an transaction that was rolled back.
    - As a transaction never spans binary logs, we don't have to handle the
      case where we do a relay-log-rotation in the middle of the transaction.
      If this would not be the case, we would have to ensure that we
      don't delete the relay log file where the transaction started when
      we switch to a new relay log file.

  @retval  0   ok,
  @retval  1   write error, otherwise.
*/

/**
  Store the file and position where the slave's SQL thread are in the
  relay log.

  Notes:

  - This function should be called either from the slave SQL thread,
    or when the slave thread is not running.  (It reads the
    group_{relay|master}_log_{pos|name} and delay fields in the rli
    object.  These may only be modified by the slave SQL thread or by
    a client thread when the slave SQL thread is not running.)

  - If there is an active transaction, then we do not update the
    position in the relay log.  This is to ensure that we re-execute
    statements if we die in the middle of an transaction that was
    rolled back.

  - As a transaction never spans binary logs, we don't have to handle
    the case where we do a relay-log-rotation in the middle of the
    transaction.  If transactions could span several binlogs, we would
    have to ensure that we do not delete the relay log file where the
    transaction started before switching to a new relay log file.

  - Error can happen if writing to file fails or if flushing the file
    fails.

  @param rli The object representing the Relay_log_info.

  @todo Change the log file information to a binary format to avoid
  calling longlong2str.

  @return 0 on success, 1 on error.
*/
int Relay_log_info::flush_info(const bool force)
{
  DBUG_ENTER("Relay_log_info::flush_info");

  if (!inited)
    DBUG_RETURN(0);

  /* 
    We update the sync_period at this point because only here we
    now that we are handling a relay log info. This needs to be
    update every time we call flush because the option maybe 
    dinamically set.
  */
  mysql_mutex_lock(&mts_temp_table_LOCK);
  handler->set_sync_period(sync_relayloginfo_period);

  if (write_info(handler))
    goto err;

  if (handler->flush_info(force || force_flush_postponed_due_to_split_trans))
    goto err;

  force_flush_postponed_due_to_split_trans= false;
  mysql_mutex_unlock(&mts_temp_table_LOCK);
  DBUG_RETURN(0);

err:
  sql_print_error("Error writing relay log configuration.");
  mysql_mutex_unlock(&mts_temp_table_LOCK);
  DBUG_RETURN(1);
}

size_t Relay_log_info::get_number_info_rli_fields() 
{ 
  return sizeof(info_rli_fields)/sizeof(info_rli_fields[0]);
}

bool Relay_log_info::read_info(Rpl_info_handler *from)
{
  int lines= 0;
  char *first_non_digit= NULL;
  ulong temp_group_relay_log_pos= 0;
  ulong temp_group_master_log_pos= 0;
  int temp_sql_delay= 0;
  int temp_internal_id= internal_id;

  DBUG_ENTER("Relay_log_info::read_info");

  /*
    Should not read RLI from file in client threads. Client threads
    only use RLI to execute BINLOG statements.

    @todo Uncomment the following assertion. Currently,
    Relay_log_info::init() is called from init_master_info() before
    the THD object Relay_log_info::sql_thd is created. That means we
    cannot call belongs_to_client() since belongs_to_client()
    dereferences Relay_log_info::sql_thd. So we need to refactor
    slightly: the THD object should be created by Relay_log_info
    constructor (or passed to it), so that we are guaranteed that it
    exists at this point. /Sven
  */
  //DBUG_ASSERT(!belongs_to_client());

  /*
    Starting from 5.1.x, relay-log.info has a new format. Now, its
    first line contains the number of lines in the file. By reading
    this number we can determine which version our master.info comes
    from. We can't simply count the lines in the file, since
    versions before 5.1.x could generate files with more lines than
    needed. If first line doesn't contain a number, or if it
    contains a number less than LINES_IN_RELAY_LOG_INFO_WITH_DELAY,
    then the file is treated like a file from pre-5.1.x version.
    There is no ambiguity when reading an old master.info: before
    5.1.x, the first line contained the binlog's name, which is
    either empty or has an extension (contains a '.'), so can't be
    confused with an integer.

    So we're just reading first line and trying to figure which
    version is this.
  */

  /*
    The first row is temporarily stored in mi->master_log_name, if
    it is line count and not binlog name (new format) it will be
    overwritten by the second row later.
  */
  if (from->prepare_info_for_read() ||
      from->get_info(group_relay_log_name, sizeof(group_relay_log_name),
                     (char *) ""))
    DBUG_RETURN(TRUE);

  lines= strtoul(group_relay_log_name, &first_non_digit, 10);

  if (group_relay_log_name[0]!='\0' &&
      *first_non_digit=='\0' && lines >= LINES_IN_RELAY_LOG_INFO_WITH_DELAY)
  {
    /* Seems to be new format => read group relay log name */
    if (from->get_info(group_relay_log_name, sizeof(group_relay_log_name),
                       (char *) ""))
      DBUG_RETURN(TRUE);
  }
  else
     DBUG_PRINT("info", ("relay_log_info file is in old format."));

  if (from->get_info(&temp_group_relay_log_pos,
                     (ulong) BIN_LOG_HEADER_SIZE) ||
      from->get_info(group_master_log_name,
                     sizeof(group_relay_log_name),
                     (char *) "") ||
      from->get_info(&temp_group_master_log_pos,
                     0UL))
    DBUG_RETURN(TRUE);

  if (lines >= LINES_IN_RELAY_LOG_INFO_WITH_DELAY)
  {
    if (from->get_info(&temp_sql_delay, 0))
      DBUG_RETURN(TRUE);
  }

  if (lines >= LINES_IN_RELAY_LOG_INFO_WITH_WORKERS)
  {
    if (from->get_info(&recovery_parallel_workers, 0UL))
      DBUG_RETURN(TRUE);
  }

  if (lines >= LINES_IN_RELAY_LOG_INFO_WITH_ID)
  {
    if (from->get_info(&temp_internal_id, 1))
      DBUG_RETURN(TRUE);
  }

  if (lines >= LINES_IN_RELAY_LOG_INFO_WITH_CHANNEL)
  {
    /* the default value is empty string"" */
    if (from->get_info(channel, sizeof(channel), (char*)""))
      DBUG_RETURN(TRUE);
  }

  group_relay_log_pos=  temp_group_relay_log_pos;
  group_master_log_pos= temp_group_master_log_pos;
  sql_delay= (int32) temp_sql_delay;
  internal_id= (uint) temp_internal_id;

  DBUG_ASSERT(lines < LINES_IN_RELAY_LOG_INFO_WITH_ID ||
             (lines >= LINES_IN_RELAY_LOG_INFO_WITH_ID && internal_id == 1));
  DBUG_RETURN(FALSE);
}

bool Relay_log_info::set_info_search_keys(Rpl_info_handler *to)
{
  DBUG_ENTER("Relay_log_info::set_info_search_keys");

  if (to->set_info(LINES_IN_RELAY_LOG_INFO_WITH_CHANNEL, channel))
    DBUG_RETURN(TRUE);

  DBUG_RETURN(FALSE);
}


bool Relay_log_info::write_info(Rpl_info_handler *to)
{
  DBUG_ENTER("Relay_log_info::write_info");

  /*
    @todo Uncomment the following assertion. See todo in
    Relay_log_info::read_info() for details. /Sven
  */
  //DBUG_ASSERT(!belongs_to_client());

  if (to->prepare_info_for_write() ||
      to->set_info((int) LINES_IN_RELAY_LOG_INFO_WITH_ID) ||
      to->set_info(group_relay_log_name) ||
      to->set_info((ulong) group_relay_log_pos) ||
      to->set_info(group_master_log_name) ||
      to->set_info((ulong) group_master_log_pos) ||
      to->set_info((int) sql_delay) ||
      to->set_info(recovery_parallel_workers) ||
      to->set_info((int) internal_id) ||
      to->set_info(channel))
    DBUG_RETURN(TRUE);

  DBUG_RETURN(FALSE);
}

/**
   The method is run by SQL thread/MTS Coordinator.
   It replaces the current FD event with a new one.
   A version adaptation routine is invoked for the new FD
   to align the slave applier execution context with the master version.

   Since FD are shared by Coordinator and Workers in the MTS mode,
   deletion of the old FD is done through decrementing its usage counter.
   The destructor runs when the later drops to zero,
   also see @c Slave_worker::set_rli_description_event().
   The usage counter of the new FD is incremented.

   Although notice that MTS worker runs it, inefficiently (see assert),
   once at its destruction time.

   @param  a pointer to be installed into execution context 
           FormatDescriptor event
*/

void Relay_log_info::set_rli_description_event(Format_description_log_event *fe)
{
  DBUG_ENTER("Relay_log_info::set_rli_description_event");
  DBUG_ASSERT(!info_thd || !is_mts_worker(info_thd) || !fe);

  if (fe)
  {
    ulong fe_version= adapt_to_master_version(fe);

    if (info_thd)
    {
      // See rpl_rli_pdb.h:Slave_worker::set_rli_description_event.
      if (!is_in_group() &&
          (info_thd->variables.gtid_next.type == AUTOMATIC_GROUP ||
           info_thd->variables.gtid_next.type == UNDEFINED_GROUP))
      {
        DBUG_PRINT("info", ("Setting gtid_next.type to NOT_YET_DETERMINED_GROUP"));
        info_thd->variables.gtid_next.set_not_yet_determined();
      }

      if (is_parallel_exec() && fe_version > 0)
      {
        /*
          Prepare for workers' adaption to a new FD version. Workers
          will see notification through scheduling of a first event of
          a new post-new-FD.
        */
        for (Slave_worker **it= workers.begin(); it != workers.end(); ++it)
          (*it)->fd_change_notified= false;
      }
    }
  }
  if (rli_description_event &&
      rli_description_event->usage_counter.atomic_add(-1) == 1)
    delete rli_description_event;
#ifndef DBUG_OFF
  else
    /* It must be MTS mode when the usage counter greater than 1. */
    DBUG_ASSERT(!rli_description_event || is_parallel_exec());
#endif
  rli_description_event= fe;
  if (rli_description_event)
    rli_description_event->usage_counter.atomic_add(1);

  DBUG_VOID_RETURN;
}

struct st_feature_version
{
  /*
    The enum must be in the version non-descending top-down order,
    the last item formally corresponds to highest possible server
    version (never reached, thereby no adapting actions here);
    enumeration starts from zero.
  */
  enum
  {
    WL6292_TIMESTAMP_EXPLICIT_DEFAULT= 0,
    _END_OF_LIST // always last
  } item;
  /*
    Version where the feature is introduced.
  */
  uchar version_split[3];
  /*
    Action to perform when according to FormatDescriptor event Master 
    is found to be feature-aware while previously it has *not* been.
  */
  void (*upgrade) (THD*);
  /*
    Action to perform when according to FormatDescriptor event Master 
    is found to be feature-*un*aware while previously it has been.
  */
  void (*downgrade) (THD*);
};

void wl6292_upgrade_func(THD *thd)
{
  thd->variables.explicit_defaults_for_timestamp= false;
  if (global_system_variables.explicit_defaults_for_timestamp)
    thd->variables.explicit_defaults_for_timestamp= true;

  return;
}

void wl6292_downgrade_func(THD *thd)
{
  if (global_system_variables.explicit_defaults_for_timestamp)
    thd->variables.explicit_defaults_for_timestamp= false;

  return;
}

/**
   Sensitive to Master-vs-Slave version difference features
   should be listed in the version non-descending order.
*/
static st_feature_version s_features[]=
{
  // order is the same as in the enum
  { st_feature_version::WL6292_TIMESTAMP_EXPLICIT_DEFAULT,
    {5, 6, 6}, wl6292_upgrade_func, wl6292_downgrade_func },
  { st_feature_version::_END_OF_LIST,
    {255, 255, 255}, NULL, NULL }
};

/**
   The method computes the incoming "master"'s FD server version and that
   of the currently installed (if ever) rli_description_event, to
   invoke more specific method to compare the two and adapt slave applier execution
   context to the new incoming master's version.

   This method is specifically for STS applier/MTS Coordinator as well as
   for a user thread applying binlog events.

   @param  fdle  a pointer to new Format Description event that is being
                 set up a new execution context.
   @return 0                when the versions are equal,
           master_version   otherwise
*/
ulong Relay_log_info::adapt_to_master_version(Format_description_log_event *fdle)
{
  ulong master_version, current_version, slave_version;

  slave_version= version_product(slave_version_split);
  /* When rli_description_event is uninitialized yet take the slave's version */
  master_version= !fdle ? slave_version : fdle->get_product_version();
  current_version= !rli_description_event ? slave_version :
    rli_description_event->get_product_version();

  return adapt_to_master_version_updown(master_version, current_version);
}

/**
  The method compares two supplied versions and carries out down- or
  up- grade customization of execution context of the slave applier
  (thd).

  The method is invoked in the STS case through
  Relay_log_info::adapt_to_master_version() right before a new master
  FD is installed into the applier execution context; in the MTS
  case it's done by the Worker when it's assigned with a first event
  after the latest new FD has been installed.

  Comparison of the current (old, existing) and the master (new,
  incoming) versions yields adaptive actions.
  To explain that, let's denote V_0 as the current, and the master's
  one as V_1.
  In the downgrade case (V_1 < V_0) a server feature that is undefined
  in V_1 but is defined starting from some V_f of [V_1 + 1, V_0] range
  (+1 to mean V_1 excluded) are invalidated ("removed" from execution context)
  by running so called here downgrade action.
  Conversely in the upgrade case a feature defined in [V_0 + 1, V_1] range
  is validated ("added" to execution context) by running its upgrade action.
  A typical use case showing how adaptive actions are necessary for the slave
  applier is when the master version is lesser than the slave's one.
  In such case events generated on the "older" master may need to be applied
  in their native server context. And such context can be provided by downgrade
  actions.
  Conversely, when the old master events are run out and a newer master's events
  show up for applying, the execution context will be upgraded through
  the namesake actions.

  Notice that a relay log may have two FD events, one the slave local
  and the other from the Master. As there's no concern for the FD
  originator this leads to two adapt_to_master_version() calls.
  It's not harmful as can be seen from the following example.
  Say the currently installed FD's version is
  V_m, then at relay-log rotation the following transition takes
  place:

     V_m  -adapt-> V_s -adapt-> V_m.

  here and further `m' subscript stands for the master, `s' for the slave.
  It's clear that in this case an ineffective V_m -> V_m transition occurs.

  At composing downgrade/upgrade actions keep in mind that the slave applier
  version transition goes the following route:
  The initial version is that of the slave server (V_ss).
  It changes to a magic 4.0 at the slave relay log initialization.
  In the following course versions are extracted from each FD read out,
  regardless of what server generated it. Here is a typical version
  transition sequence underscored with annotation:

   V_ss -> 4.0 -> V(FD_s^1) -> V(FD_m^2)   --->   V(FD_s^3) -> V(FD_m^4)  ...

    ----------     -----------------     --------  ------------------     ---
     bootstrap       1st relay log       rotation      2nd log            etc

  The upper (^) subscipt enumerates Format Description events, V(FD^i) stands
  for a function extrating the version data from the i:th FD.

  There won't be any action to execute when info_thd is undefined,
  e.g at bootstrap.

  @param  master_version   an upcoming new version
  @param  current_version  the current version
  @return 0                when the new version is equal to the current one,
          master_version   otherwise
*/
ulong Relay_log_info::adapt_to_master_version_updown(ulong master_version,
                                                     ulong current_version)
{
  THD *thd= info_thd;
  /*
    When the SQL thread or MTS Coordinator executes this method
    there's a constraint on current_version argument.
  */
  DBUG_ASSERT(!thd ||
              thd->rli_fake != NULL ||
              thd->system_thread == SYSTEM_THREAD_SLAVE_WORKER ||
              (thd->system_thread == SYSTEM_THREAD_SLAVE_SQL &&
               (!rli_description_event ||
                current_version ==
                rli_description_event->get_product_version())));

  if (master_version == current_version)
    return 0;
  else if (!thd)
    return master_version;

  bool downgrade= master_version < current_version;
   /*
    find item starting from and ending at for which adaptive actions run
    for downgrade or upgrade branches.
    (todo: convert into bsearch when number of features will grow significantly)
  */
  long i, i_first= st_feature_version::_END_OF_LIST, i_last= i_first;

  for (i= 0; i < st_feature_version::_END_OF_LIST; i++)
  {
    ulong ver_f= version_product(s_features[i].version_split);

    if ((downgrade ? master_version : current_version) < ver_f && 
        i_first == st_feature_version::_END_OF_LIST)
      i_first= i;
    if ((downgrade ? current_version : master_version) < ver_f)
    {
      i_last= i;
      DBUG_ASSERT(i_last >= i_first);
      break;
    }
  }

  /* 
     actions, executed in version non-descending st_feature_version order
  */
  for (i= i_first; i < i_last; i++)
  {
    /* Run time check of the st_feature_version items ordering */
    DBUG_ASSERT(!i ||
                version_product(s_features[i - 1].version_split) <=
                version_product(s_features[i].version_split));

    DBUG_ASSERT((downgrade ? master_version : current_version) <
                version_product(s_features[i].version_split) &&
                (downgrade ? current_version : master_version  >=
                 version_product(s_features[i].version_split)));

    if (downgrade && s_features[i].downgrade)
    {
      s_features[i].downgrade(thd);
    }
    else if (s_features[i].upgrade)
    {
      s_features[i].upgrade(thd);
    }
  }

  return master_version;
}

void Relay_log_info::relay_log_number_to_name(uint number,
                                              char name[FN_REFLEN+1])
{
  char *str= NULL;
  char relay_bin_channel[FN_REFLEN+1];
  const char *relay_log_basename_channel=
    add_channel_to_relay_log_name(relay_bin_channel, FN_REFLEN+1,
                                  relay_log_basename);

  /* str points to closing null of relay log basename channel */
  str= strmake(name, relay_log_basename_channel, FN_REFLEN+1);
  *str++= '.';
  sprintf(str, "%06u", number);
}

uint Relay_log_info::relay_log_name_to_number(const char *name)
{
  return static_cast<uint>(atoi(fn_ext(name)+1));
}

bool is_mts_db_partitioned(Relay_log_info * rli)
{
  return (rli->current_mts_submode->get_type() ==
    MTS_PARALLEL_TYPE_DB_NAME);
}

const char* Relay_log_info::get_for_channel_str(bool upper_case) const
{
  if (rli_fake)
    return "";
  else
    return mi->get_for_channel_str(upper_case);
}

enum_return_status Relay_log_info::add_gtid_set(const Gtid_set *gtid_set)
{
  DBUG_ENTER("Relay_log_info::add_gtid_set(gtid_set)");

  enum_return_status return_status= this->gtid_set.add_gtid_set(gtid_set);

  DBUG_RETURN(return_status);
}

void Relay_log_info::detach_engine_ha_data(THD *thd)
{
  is_engine_ha_data_detached= true;
    /*
      In case of slave thread applier or processing binlog by client,
      detach the engine ha_data ("native" engine transaction)
      in favor of dynamically created.
    */
  plugin_foreach(thd, detach_native_trx,
                 MYSQL_STORAGE_ENGINE_PLUGIN, NULL);
}

void Relay_log_info::reattach_engine_ha_data(THD *thd)
{
  is_engine_ha_data_detached = false;
  /*
    In case of slave thread applier or processing binlog by client,
    reattach the engine ha_data ("native" engine transaction)
    in favor of dynamically created.
  */
  plugin_foreach(thd, reattach_native_trx, MYSQL_STORAGE_ENGINE_PLUGIN, NULL);
}
