/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/rpl_applier_reader.h"
#include "include/mutex_lock.h"
#include "include/mysqld_errmsg.h"  // ER_OUT_OF_RESOURCES_MSG
#include "include/scope_guard.h"
#include "mysql/components/services/log_builtins.h"
#include "sql/log.h"
#include "sql/mysqld.h"
#include "sql/rpl_replica.h"
#include "sql/rpl_rli.h"
#include "sql/rpl_rli_pdb.h"
#include "sql/sql_backup_lock.h"
#include "string_with_len.h"

/**
   It manages a stage and the related mutex and makes the process of
   locking and entering stage/unlock and exiting stage as monolithic operations.
   - locking and entering stage can be done in the constructor automatically.
   - unlocking and exiting stage are done in the destructor automatically.
     So the caller just need to initialize an Stage_controller object in
     a proper code block. It will exit the stage automatically when the object
     is destroyed.
 */
class Rpl_applier_reader::Stage_controller {
 public:
  enum State { INACTIVE, LOCKED, IN_STAGE };

  Stage_controller(THD *thd, mysql_mutex_t *mutex, mysql_cond_t *cond,
                   const PSI_stage_info &new_stage, enum State state = INACTIVE)
      : m_thd(thd), m_mutex(mutex), m_cond(cond), m_new_stage(new_stage) {
    if (state >= LOCKED) {
      lock();
      if (state == IN_STAGE) enter_stage();
    }
  }

  ~Stage_controller() {
    if (m_state >= LOCKED) {
      mysql_mutex_unlock(m_mutex);
      if (m_state == IN_STAGE) m_thd->EXIT_COND(&m_old_stage);
    }
  }

  void lock() {
    assert(m_state == INACTIVE);
    mysql_mutex_lock(m_mutex);
    m_state = LOCKED;
  }

  void enter_stage() {
    assert(m_state == LOCKED);
    m_thd->ENTER_COND(m_cond, m_mutex, &m_new_stage, &m_old_stage);
    m_state = IN_STAGE;
  }

 private:
  THD *m_thd = nullptr;
  mysql_mutex_t *m_mutex = nullptr;
  mysql_cond_t *m_cond = nullptr;
  PSI_stage_info m_old_stage;
  const PSI_stage_info &m_new_stage;
  enum State m_state = INACTIVE;
};

Rpl_applier_reader::Rpl_applier_reader(Relay_log_info *rli)
    : m_relaylog_file_reader(
          opt_replica_sql_verify_checksum,
          std::max(replica_max_allowed_packet,
                   binlog_row_event_max_size + MAX_LOG_EVENT_HEADER)),
      m_rli(rli) {}

Rpl_applier_reader::~Rpl_applier_reader() { close(); }

bool Rpl_applier_reader::open(const char **errmsg) {
  bool ret = true;
  Relay_log_info *rli = m_rli;

  if (rli->relay_log.find_log_pos(&m_linfo, rli->get_group_relay_log_name(),
                                  true /*need_index_lock*/)) {
    *errmsg = "Could not find relay log file.";
    return true;
  }

  Format_description_log_event *fdle = nullptr;
  if (m_relaylog_file_reader.open(m_linfo.log_file_name,
                                  rli->get_group_relay_log_pos(), &fdle))
    goto err;

  {  // Begin context block for `m_rli->data_lock` mutex acquisition
    MUTEX_LOCK(lock, &m_rli->data_lock);
    bool is_fdle_allocated_here{fdle == nullptr};
    if (is_fdle_allocated_here) {
      fdle = new Format_description_log_event();
    }
    if (rli->set_rli_description_event(fdle)) {
      if (is_fdle_allocated_here) delete fdle;
      return true;  // Release acquired lock on `m_rli->data_lock`
    }

    /**
       group_relay_log_name may be different from the one in index file. For
       example group_relay_log_name includes a full path. But the one in index
       file has relative path. So set group_relay_log_name to the one in index
       file. It guarantees MYSQL_BIN_LOG::purge works well.
    */
    rli->set_group_relay_log_name(m_linfo.log_file_name);
    rli->set_event_relay_log_pos(rli->get_group_relay_log_pos());
    rli->set_event_relay_log_name(rli->get_group_relay_log_name());
    if (relay_log_purge == 0 && rli->log_space_limit > 0) {
      rli->log_space_limit = 0;
      LogErr(WARNING_LEVEL, ER_RELAY_LOG_SPACE_LIMIT_DISABLED);
    }
  }  // Release acquired lock on `m_rli->data_lock`

  m_reading_active_log = m_rli->relay_log.is_active(m_linfo.log_file_name);
  ret = false;

#ifndef NDEBUG
  debug_print_next_event_positions();
#endif
err:
  if (ret) *errmsg = m_relaylog_file_reader.get_error_str();
  return ret;
}

void Rpl_applier_reader::close() {
  m_relaylog_file_reader.close();
  m_reading_active_log = true;
  m_log_end_pos = 0;
  m_errmsg = nullptr;
}

Log_event *Rpl_applier_reader::read_next_event() {
  DBUG_TRACE;
  Log_event *ev = nullptr;

  /*
    data_lock is needed when accessing members of Relay_log_info, and this
    function temporarily releases it while waiting.
  */
  mysql_mutex_assert_owner(&m_rli->data_lock);

  DBUG_EXECUTE_IF("block_applier_updates", {
    const char act[] =
        "now SIGNAL applier_read_blocked WAIT_FOR resume_applier_read";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });
  DBUG_EXECUTE_IF("force_sql_thread_error", return nullptr;);

  auto &applier_metrics = m_rli->get_applier_metrics();
  if (m_reading_active_log &&
      m_relaylog_file_reader.position() >= m_log_end_pos) {
    applier_metrics.get_work_from_source_wait_metric().increment_counter();
    while (true) {
      if (sql_slave_killed(m_rli->info_thd, m_rli)) return nullptr;

#ifndef NDEBUG
      debug_print_next_event_positions();
#endif

      if (read_active_log_end_pos()) break;

      /*
        At this point the coordinator has no job to delegate to workers.
        However, workers are executing their assigned jobs and as such
        the checkpoint routine must be periodically invoked.

        mta_checkpoint_routine has to be called before enter_stage().
        Otherwise, it will cause a deadlock with STOP REPLICA or other
        thread has the same lock pattern.
        STOP REPLICA Thread                   Coordinator Thread
        =================                   ==================
        lock LOCK_thd_data                  lock LOCK_binlog_end_pos
                                            enter_stage(LOCK_binlog_end_pos)
        lock LOCK_binlog_end_pos
        in THD::awake
                                            lock LOCK_thd_data in
                                            mta_checkpoint_routine()
                                              flush_info()
                                                ...
                                                close_thread_table()
      */
      mysql_mutex_unlock(&m_rli->data_lock);
      {
        auto data_lock_guard =
            create_scope_guard([this] { mysql_mutex_lock(&m_rli->data_lock); });
        if ((m_rli->is_time_for_mta_checkpoint() ||
             DBUG_EVALUATE_IF("check_replica_debug_group", 1, 0)) &&
            mta_checkpoint_routine(m_rli, false)) {
          m_errmsg = "Failed to synchronize worker threads";
          return nullptr;
        }
      }

      /* Lock LOCK_binlog_end_pos before wait */
      Stage_controller stage_controller(
          m_rli->info_thd, m_rli->relay_log.get_binlog_end_pos_lock(),
          m_rli->relay_log.get_log_cond(), stage_replica_has_read_all_relay_log,
          Stage_controller::LOCKED);

      /* Check it again to avoid missing update signals from receiver thread */
      if (read_active_log_end_pos()) break;

      if (m_rli->is_until_satisfied_all_transactions_read_from_relay_log()) {
        // Make it stop on the next execution
        m_rli->abort_slave = true;
        return nullptr;
      }

      reset_seconds_behind_master();
      /* It should be protected by relay_log.LOCK_binlog_end_pos */
      if (m_rli->ign_master_log_name_end[0]) return generate_rotate_event();

      stage_controller.enter_stage();
      if (sql_slave_killed(m_rli->info_thd, m_rli)) return nullptr;

      if (wait_for_new_event()) return nullptr;
    }
  }

  m_rli->set_event_start_pos(m_relaylog_file_reader.position());
  applier_metrics.get_time_to_read_from_relay_log_metric().start_timer();
  ev = m_relaylog_file_reader.read_event_object();
  applier_metrics.get_time_to_read_from_relay_log_metric().stop_timer();
  if (ev != nullptr) {
    m_rli->set_future_event_relay_log_pos(m_relaylog_file_reader.position());
    ev->future_event_relay_log_pos = m_rli->get_future_event_relay_log_pos();
    return ev;
  }

  if (m_relaylog_file_reader.get_error_type() == Binlog_read_error::READ_EOF &&
      !m_reading_active_log) {
    bool force_purging = false;
    if (m_rli->is_receiver_waiting_for_rl_space.load() &&
        !m_rli->is_in_group()) {
      force_purging = true;
      if (m_rli->is_parallel_exec()) {
        mysql_mutex_unlock(&m_rli->data_lock);
        auto data_lock_guard =
            create_scope_guard([this] { mysql_mutex_lock(&m_rli->data_lock); });
        if (m_rli->current_mts_submode->wait_for_workers_to_finish(m_rli) ==
            -1) {
          m_errmsg = "Failed to compute mta checkpoint";
          return nullptr;
        }
      }
    }
    if (!move_to_next_log(force_purging)) return read_next_event();
  }

  // if we fail to move to a new log when the thread is killed, ignore it
  if (!current_thd || !current_thd->is_killed())
    LogErr(ERROR_LEVEL, ER_RPL_REPLICA_ERROR_READING_RELAY_LOG_EVENTS,
           m_rli->get_for_channel_str(),
           m_errmsg ? m_errmsg : m_relaylog_file_reader.get_error_str());

  return nullptr;
}

bool Rpl_applier_reader::read_active_log_end_pos() {
  m_log_end_pos = m_rli->relay_log.get_binlog_end_pos();
  m_reading_active_log = m_rli->relay_log.is_active(m_linfo.log_file_name);
  if (m_log_end_pos > m_relaylog_file_reader.position() ||
      !m_reading_active_log) {
    reopen_log_reader_if_needed();
    return true;
  }
  return false;
}

Rotate_log_event *Rpl_applier_reader::generate_rotate_event() {
  DBUG_TRACE;
  Rotate_log_event *ev = nullptr;
  ev = new Rotate_log_event(m_rli->ign_master_log_name_end, 0,
                            m_rli->ign_master_log_pos_end,
                            Rotate_log_event::DUP_NAME);
  m_rli->ign_master_log_name_end[0] = 0;
  if (unlikely(!ev)) {
    m_errmsg =
        "Replica SQL thread failed to create a Rotate event "
        "(out of memory?), SHOW REPLICA STATUS may be inaccurate";
    return nullptr;
  }
  ev->server_id = 0;  // don't be ignored by slave SQL thread
  return ev;
}

bool Rpl_applier_reader::wait_for_new_event() {
  mysql_mutex_assert_owner(m_rli->relay_log.get_binlog_end_pos_lock());
  mysql_mutex_assert_owner(&m_rli->data_lock);
  /*
    We can, and should release data_lock while we are waiting for
    update. If we do not, show replica status will block
  */
  mysql_mutex_unlock(&m_rli->data_lock);

  auto &applier_metrics = m_rli->get_applier_metrics();
  applier_metrics.get_work_from_source_wait_metric().start_timer();

  int ret = 0;
  if (m_rli->is_parallel_exec() &&
      (opt_mta_checkpoint_period != 0 ||
       DBUG_EVALUATE_IF("check_replica_debug_group", 1, 0))) {
    std::chrono::nanoseconds timeout =
        std::chrono::nanoseconds{opt_mta_checkpoint_period * 1000000ULL};
    DBUG_EXECUTE_IF("check_replica_debug_group",
                    { timeout = std::chrono::nanoseconds{10000000}; });
    ret = m_rli->relay_log.wait_for_update(timeout);
  } else
    ret = m_rli->relay_log.wait_for_update();

  applier_metrics.get_work_from_source_wait_metric().stop_timer();

  // re-acquire data lock since we released it earlier
  mysql_mutex_lock(&m_rli->data_lock);
  assert(ret == 0 || is_timeout(ret));
  return ret != 0 && !is_timeout(ret);
}

bool Rpl_applier_reader::reopen_log_reader_if_needed() {
  /*
    The SQL thread was reading from the "active" relay log file but that file
    was truncated by the I/O thread. We will re-init the reader of the
    SQL thread in order to avoid reading content that was in the source's
    cache but no longer exists on the relay log file because of the truncation.

    Suppose:
    a) the relay log file have a full transaction (many events) with a
       total of finishing at position 8190;
    b) the I/O thread is receiving and queuing a 32K event;
    c) the disk had only space to write 30K of the event;

    In this situation, the I/O thread will be waiting for disk space, and the
    SQL thread would be allowed to read up to the end of the last fully queued
    event (that would be the 8190 position of the file).

    Starting the SQL thread, it would read until the relay_log.binlog_end_pos
    (the 8190), but, because of some optimizations, the source(IO_CACHE) will
    read a full "buffer" (8192 bytes) from the file. The additional 2 bytes
    belong to the not yet complete queued event, and should not be read by the
    SQL thread.

    If the I/O thread is killed, it will truncate the relay log file at position
    8190. This means that the SQL thread reader(Its source IO_CACHE) have 2
    bytes that doesn't belong to the relay log file anymore and should
    re-initialize its source to remove such data from it.
  */
  mysql_mutex_assert_owner(&m_rli->data_lock);
  if (m_rli->is_relay_log_truncated()) {
    m_rli->clear_relay_log_truncated();

    my_off_t pos = m_relaylog_file_reader.position();
    m_relaylog_file_reader.close();
    if (m_relaylog_file_reader.open(m_linfo.log_file_name) ||
        m_relaylog_file_reader.seek(pos))
      return true;
  }
  return false;
}

bool Rpl_applier_reader::move_to_next_log(bool force) {
  // Current relay file can be purged if:
  // - group_relay_log_pos has reached the end of it, which is rarely true
  //   because Rotate_log_event(generated by the replica) doesn't advance
  //   the group relay log position
  // - 'force' parameter is equal to true
  bool should_purge_current_relay_log =
      (m_rli->get_group_relay_log_pos() == m_rli->get_event_relay_log_pos() &&
       strcmp(m_rli->get_group_relay_log_name(),
              m_rli->get_event_relay_log_name()) == 0) ||
      force;

  m_relaylog_file_reader.close();

  if (!m_rli->relay_log.is_open() ||
      m_rli->relay_log.find_next_log(&m_linfo, true)) {
    m_errmsg = "error switching to the next log";
    return true;
  }

  m_rli->set_event_relay_log_pos(BIN_LOG_HEADER_SIZE);
  m_rli->set_event_relay_log_name(m_linfo.log_file_name);

  if (!m_rli->is_in_group()) {
    /*
      To make the code be simpler, it is better to remove the following 'if'
      code block. should_purge_current_relay_log is rarely true. So it is ok
      not to purge current relay log.
    */
    if (should_purge_current_relay_log) {
      /*
        Move group relay log to next file, so that current relay log will be
        purged.
      */
      m_rli->set_group_relay_log_pos(BIN_LOG_HEADER_SIZE);
      m_rli->set_group_relay_log_name(m_linfo.log_file_name);
    }

    DBUG_EXECUTE_IF("wait_before_purge_applied_logs", {
      const char dbug_wait[] =
          "now SIGNAL signal.rpl_before_applier_purge_logs WAIT_FOR "
          "signal.rpl_unblock_purge";
      assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(dbug_wait)));
    });

    if (purge_applied_logs()) return true;
  } else {
    m_rli->force_flush_postponed_due_to_split_trans = true;
  }

  /* Reset the relay-log-change-notified status of slave workers */
  if (m_rli->is_parallel_exec()) {
    DBUG_PRINT("info", ("next_event: MTA group relay log changes to %s %lu\n",
                        m_rli->get_group_relay_log_name(),
                        (ulong)m_rli->get_group_relay_log_pos()));
    m_rli->reset_notified_relay_log_change();
  }

  m_reading_active_log = m_rli->relay_log.is_active(m_linfo.log_file_name);
  m_log_end_pos = m_relaylog_file_reader.position();
  return m_relaylog_file_reader.open(m_linfo.log_file_name);
}

bool Rpl_applier_reader::purge_applied_logs() {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&m_rli->data_lock);

  if (!relay_log_purge) return false;

  // lock BACKUP lock for the duration of PURGE operation
  Shared_backup_lock_guard backup_lock{current_thd};
  switch (backup_lock) {
    case Shared_backup_lock_guard::Lock_result::locked:
      break;
    case Shared_backup_lock_guard::Lock_result::not_locked: {
      LogErr(WARNING_LEVEL, ER_LOG_CANNOT_PURGE_BINLOG_WITH_BACKUP_LOCK);
      return false;
    }
    case Shared_backup_lock_guard::Lock_result::oom: {
      m_errmsg = ER_OUT_OF_RESOURCES_MSG;
      return true;
    }
  }

  CONDITIONAL_SYNC_POINT("purge_applied_logs_after_backup_lock");

  if (m_rli->flush_info(Relay_log_info::RLI_FLUSH_IGNORE_SYNC_OPT)) {
    m_errmsg = "Error purging processed logs";
    return true;
  }

  m_rli->relay_log.lock_index();

  DBUG_EXECUTE_IF("crash_before_purge_logs", DBUG_SUICIDE(););

  mysql_mutex_lock(&m_rli->log_space_lock);
  // we can copy to a non-atomic variable and back under the log_space_lock
  auto current_log_space = m_rli->log_space_total.load();
  if (m_rli->relay_log.purge_logs(
          m_rli->get_group_relay_log_name(), false /* include */,
          false /*need_lock_index*/, false /*need_update_threads*/,
          &current_log_space, true) != 0)
    m_errmsg = "Error purging processed logs";
  m_rli->log_space_total.store(current_log_space);
  // modify variable before signaling the receiver and under the log_space_lock
  m_rli->coordinator_log_after_purge = m_rli->get_group_relay_log_name();
  mysql_cond_broadcast(&m_rli->log_space_cond);
  mysql_mutex_unlock(&m_rli->log_space_lock);

  /* Need to update the log pos because purge_logs has been called. */
  if (m_errmsg == nullptr &&
      m_rli->relay_log.find_log_pos(&m_linfo, m_rli->get_event_relay_log_name(),
                                    false /*need_lock_index*/)) {
    m_errmsg = "error switching to the next log";
  }

  m_rli->relay_log.unlock_index();
  return m_errmsg != nullptr;
}

#ifndef NDEBUG
void Rpl_applier_reader::debug_print_next_event_positions() {
  DBUG_PRINT(
      "info",
      ("assertion skip %u file pos %llu event relay log pos %llu file %s\n",
       m_rli->slave_skip_counter.load(), m_relaylog_file_reader.position(),
       m_rli->get_event_relay_log_pos(), m_rli->get_event_relay_log_name()));

  /* This is an assertion which sometimes fails, let's try to track it */
  DBUG_PRINT("info", ("m_relaylog_file_reader->position() %llu "
                      "m_rli->event_relay_log_pos=%llu",
                      m_relaylog_file_reader.position(),
                      m_rli->get_event_relay_log_pos()));

  assert(m_relaylog_file_reader.position() >= BIN_LOG_HEADER_SIZE);
  assert(m_relaylog_file_reader.position() ==
             m_rli->get_event_relay_log_pos() ||
         (m_rli->is_parallel_exec() ||
          // TODO: double check that this is safe:
          (m_rli->info_thd != nullptr &&
           m_rli->info_thd->variables.binlog_trx_compression)));

  DBUG_PRINT(
      "info",
      ("next_event group source %s %lu group relay %s %lu event %s %lu\n",
       m_rli->get_group_master_log_name(),
       (ulong)m_rli->get_group_master_log_pos(),
       m_rli->get_group_relay_log_name(),
       (ulong)m_rli->get_group_relay_log_pos(),
       m_rli->get_event_relay_log_name(),
       (ulong)m_rli->get_event_relay_log_pos()));

  DBUG_PRINT("info",
             ("m_rli->relay_log.get_binlog_end_pos()= %llu", m_log_end_pos));
  DBUG_PRINT("info",
             ("active_log= %s", m_reading_active_log ? "true" : "false"));
}
#endif

void Rpl_applier_reader::reset_seconds_behind_master() {
  /*
    We say in Seconds_Behind_Source that we have "caught up". Note that for
    example if network link is broken but I/O slave thread hasn't noticed it
    (replica_net_timeout not elapsed), then we'll say "caught up" whereas we're
    not really caught up. Fixing that would require internally cutting timeout
    in smaller pieces in network read. Another example: SQL has caught up on
    I/O, now I/O has read a new event and is queuing it; the false "0" will
    exist until SQL finishes executing the new event; it will be look abnormal
    only if the events have old timestamps (then you get "many", 0, "many").

    Transient phases like this can be fixed with implementing Heartbeat event
    which provides the slave the status of the master at time the master does
    not have any new update to send. Seconds_Behind_Source would be zero only
    when master has no more updates in binlog for slave. The heartbeat can be
    sent in a (small) fraction of replica_net_timeout. Until it's done
    m_rli->last_master_timestamp is temporarily (for time of waiting for the
    following event) reset whenever EOF is reached.

    Note, in MTS case Seconds_Behind_Source resetting follows
    slightly different schema where reaching EOF is not enough.  The status
    parameter is updated per some number of processed group of events. The
    number can't be greater than @@global.replica_checkpoint_group and anyway
    SBM updating rate does not exceed @@global.replica_checkpoint_period. Notice
    that SBM is set to a new value after processing the terminal event (e.g
    Commit) of a group.  Coordinator resets SBM when notices no more groups left
    neither to read from Relay-log nor to process by Workers.
  */
  if (!m_rli->is_parallel_exec() || m_rli->gaq->empty())
    m_rli->last_master_timestamp = 0;
}
