/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

#include "sql/rpl_replica_until_options.h"

#include <stdlib.h>
#include <string.h>
#include <memory>

#include "m_string.h"
#include "my_sys.h"
#include "mysql/binlog/event/binlog_event.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/my_loglevel.h"
#include "mysql/service_mysql_alloc.h"
#include "mysqld_error.h"
#include "sql/log.h"
#include "sql/log_event.h"
#include "sql/rpl_group_replication.h"
#include "sql/rpl_rli.h"
#include "strmake.h"

int Until_position::init(const char *log_name, my_off_t log_pos) {
  m_until_log_pos = log_pos;
  strmake(m_until_log_name, log_name, sizeof(m_until_log_name) - 1);

  /* Preparing members for effective until condition checking */
  const char *p = fn_ext(m_until_log_name);
  char *p_end;
  if (*p) {
    // p points to '.'
    m_until_log_name_extension = strtoul(++p, &p_end, 10);
    /*
      p_end points to the first invalid character. If it equals
      to p, no digits were found, error. If it contains '\0' it
      means  conversion went ok.
    */
    if (p_end == p || *p_end) return ER_BAD_REPLICA_UNTIL_COND;
  } else
    return ER_BAD_REPLICA_UNTIL_COND;

  m_log_names_cmp_result = LOG_NAMES_CMP_UNKNOWN;
  return 0;
}

bool Until_position::check_position(const char *log_name, my_off_t log_pos) {
  DBUG_TRACE;

  DBUG_PRINT("info", ("log_name='%s', log_pos=%llu", log_name, log_pos));
  DBUG_PRINT("info", ("until_log_name='%s', until_log_pos=%llu",
                      m_until_log_name, m_until_log_pos));

  if (m_rli->is_mts_in_group() || m_rli->is_in_group()) return false;

  if (m_log_names_cmp_result == LOG_NAMES_CMP_UNKNOWN) {
    /*
      If we are after RESET REPLICA, and the SQL slave thread has not processed
      any event yet, it could be that group_master_log_name is "". In that case,
      just wait for more events (as there is no sensible comparison to do).
    */
    if (log_name == nullptr || strcmp("", log_name) == 0) return false;

    const char *basename = log_name + dirname_length(log_name);
    const char *q = (const char *)(fn_ext(basename) + 1);
    if (strncmp(basename, m_until_log_name, (int)(q - basename)) == 0) {
      char *q_end;
      ulong log_name_extension = strtoul(q, &q_end, 10);

      /* Now compare extensions. */
      if (log_name_extension < m_until_log_name_extension)
        m_log_names_cmp_result = LOG_NAMES_CMP_LESS;
      else
        m_log_names_cmp_result =
            (log_name_extension > m_until_log_name_extension)
                ? LOG_NAMES_CMP_GREATER
                : LOG_NAMES_CMP_EQUAL;
    } else {
      /* Base names do not match, so we abort */
      LogErr(ERROR_LEVEL, ER_REPLICA_SQL_THREAD_STOPPED_UNTIL_CONDITION_BAD,
             m_until_log_name, m_until_log_pos);
      return true;
    }
  }

  if (m_log_names_cmp_result == LOG_NAMES_CMP_LESS ||
      (m_log_names_cmp_result == LOG_NAMES_CMP_EQUAL &&
       log_pos < m_until_log_pos))
    return false;

  LogErr(INFORMATION_LEVEL,
         ER_REPLICA_SQL_THREAD_STOPPED_UNTIL_POSITION_REACHED, m_until_log_pos);
  return true;
}

bool Until_master_position::check_at_start_slave() {
  strmake(m_current_log_name, m_rli->get_group_master_log_name(),
          sizeof(m_current_log_name) - 1);
  m_current_log_pos = m_rli->get_group_master_log_pos();
  DBUG_PRINT("info", ("source log name is changed, %s", m_current_log_name));

  return check_position(m_current_log_name, m_current_log_pos);
}

bool Until_master_position::check_before_dispatching_event(
    const Log_event *ev) {
  /*
    When slave_io creates a new relay log it will store master's
    Format_description_log_event into the relay log with 0 log_pos.
    That format_description_log_event event should be skipped.
  */
  if (!ev->is_artificial_event() && !ev->is_relay_log_event() &&
      ev->server_id != 0 && ev->common_header->log_pos != 0) {
    m_current_log_pos = ev->common_header->log_pos;
    DBUG_PRINT("info", ("source log pos is %llu", m_current_log_pos));

    /*
      Master's events will be ignored in the cases that
      - It is in the ignore server id list.
      - Its server id is same to slave's sever id and replicate_same_server_id
        is not set.
      It will cause a hole in the replicated events.
      So we also need to check the position just before dispatching an event.
    */
    return check_position(m_current_log_name,
                          m_current_log_pos - ev->common_header->data_written);
  }
  return false;
}

bool Until_master_position::check_after_dispatching_event() {
  if (m_log_names_cmp_result == LOG_NAMES_CMP_UNKNOWN)
    return check_at_start_slave();
  else
    return check_position(m_current_log_name, m_current_log_pos);
}

bool Until_master_position::check_all_transactions_read_from_relay_log() {
  return false;
}

bool Until_relay_position::check_at_start_slave() {
  return check_position(m_rli->get_group_relay_log_name(),
                        m_rli->get_group_relay_log_pos());
}

bool Until_relay_position::check_before_dispatching_event(const Log_event *) {
  return false;
}

bool Until_relay_position::check_after_dispatching_event() {
  return check_position(m_rli->get_event_relay_log_name(),
                        m_rli->get_event_relay_log_pos());
}

bool Until_relay_position::check_all_transactions_read_from_relay_log() {
  return false; /* purecov: inspected */
}

int Until_gtids::init(const char *gtid_set_str) {
  enum_return_status ret;

  global_tsid_lock->wrlock();
  ret = m_gtids.add_gtid_text(gtid_set_str);
  global_tsid_lock->unlock();

  if (ret != RETURN_STATUS_OK) return ER_BAD_REPLICA_UNTIL_COND;
  return 0;
}

bool Until_before_gtids::check_at_start_slave() {
  DBUG_TRACE;
  global_tsid_lock->wrlock();
  if (m_gtids.is_intersection_nonempty(gtid_state->get_executed_gtids())) {
    char *buffer;
    m_gtids.to_string(&buffer);
    global_tsid_lock->unlock();

    LogErr(INFORMATION_LEVEL,
           ER_REPLICA_SQL_THREAD_STOPPED_BEFORE_GTIDS_ALREADY_APPLIED, buffer);
    my_free(buffer);
    return true;
  }
  global_tsid_lock->unlock();
  return false;
}

bool Until_before_gtids::check_before_dispatching_event(const Log_event *ev) {
  DBUG_TRACE;
  if (mysql::binlog::event::Log_event_type_helper::is_assigned_gtid_event(
          ev->get_type_code())) {
    Gtid_log_event *gev =
        const_cast<Gtid_log_event *>(down_cast<const Gtid_log_event *>(ev));
    global_tsid_lock->rdlock();
    if (m_gtids.contains_gtid(gev->get_sidno(false), gev->get_gno())) {
      char *buffer;
      m_gtids.to_string(&buffer);
      global_tsid_lock->unlock();
      LogErr(INFORMATION_LEVEL,
             ER_REPLICA_SQL_THREAD_STOPPED_BEFORE_GTIDS_REACHED, buffer);
      my_free(buffer);
      return true;
    }
    global_tsid_lock->unlock();
  }
  return false;
}

bool Until_before_gtids::check_after_dispatching_event() { return false; }

bool Until_before_gtids::check_all_transactions_read_from_relay_log() {
  return false;
}

Until_after_gtids::~Until_after_gtids() {}

void Until_after_gtids::last_transaction_executed_message() {
  char *buffer;
  m_gtids.to_string(&buffer);
  LogErr(SYSTEM_LEVEL, ER_REPLICA_SQL_THREAD_STOPPED_AFTER_GTIDS_REACHED,
         buffer);
  my_free(buffer);
}

bool Until_after_gtids::check_all_transactions_executed() {
  const Checkable_rwlock::Guard global_sid_lock_guard(
      *global_tsid_lock, Checkable_rwlock::WRITE_LOCK);
  if (m_gtids.is_subset(gtid_state->get_executed_gtids())) {
    last_transaction_executed_message();
    return true;
  }
  return false;
}

bool Until_after_gtids::check_at_start_slave() {
  DBUG_TRACE;
  if (check_all_transactions_executed()) return true;
  if (m_gtids_known_to_channel == nullptr) {
    m_gtids_known_to_channel =
        std::make_unique<Gtid_set>(global_tsid_map, nullptr);
  }
  return false;
}

bool Until_after_gtids::check_before_dispatching_event(const Log_event *ev) {
  DBUG_TRACE;

  if (ev->get_type_code() == mysql::binlog::event::GTID_LOG_EVENT) {
    global_tsid_lock->wrlock();
    /*
     Below check is needed when last transaction is received from other source
     while transactions scheduled by this channel are in execution.
     This means next GTID cannot be dispatched to worker since all GTIDs
     specified by customer have been received.
     Example:
     Channel1 START SQL_AFTER_GTID=UUID:1-3.
     Channel1 receives UUID:1-2(workers are still executing)
     NOTE: check_after_dispatching_event will not wait since last transaction
     is not received.
     Channel2 receives UUID:3(executed)
     Channel1 receives UUID:4(received)
     UUID:4 cannot be executed because UUID:1-3 have been received.
     However UUID:1-2 are still being executed by ther worker.
    */
    m_gtids_known_to_channel->add_gtid_set(gtid_state->get_executed_gtids());
    if (m_gtids.is_subset(m_gtids_known_to_channel.get())) {
      global_tsid_lock->unlock();
      if (!wait_for_gtid_set()) {
        const Checkable_rwlock::Guard global_sid_lock_guard(
            *global_tsid_lock, Checkable_rwlock::READ_LOCK);
        last_transaction_executed_message();
      }
      return true;
    }

    Gtid_log_event *gev =
        const_cast<Gtid_log_event *>(down_cast<const Gtid_log_event *>(ev));
    m_gtids_known_to_channel->_add_gtid(gev->get_sidno(false), gev->get_gno());
    global_tsid_lock->unlock();
  } else if (ev->ends_group()) {
    const Checkable_rwlock::Guard global_sid_lock_guard(
        *global_tsid_lock, Checkable_rwlock::WRITE_LOCK);
    m_gtids_known_to_channel->add_gtid_set(gtid_state->get_executed_gtids());
    if (m_gtids.is_subset(m_gtids_known_to_channel.get())) {
      m_last_transaction_in_execution = true;
    }
  }

  return false;
}

bool Until_after_gtids::check_after_dispatching_event() {
  if (m_last_transaction_in_execution) {
    if (!wait_for_gtid_set()) {
      const Checkable_rwlock::Guard global_sid_lock_guard(
          *global_tsid_lock, Checkable_rwlock::READ_LOCK);
      last_transaction_executed_message();
    }
    return true;
  }
  return false;
}

bool Until_after_gtids::wait_for_gtid_set() {
  constexpr double worker_wait_timeout = 1;
  bool status = false;

  global_tsid_lock->rdlock();
  while (gtid_state->wait_for_gtid_set(current_thd, &m_gtids,
                                       worker_wait_timeout)) {
    global_tsid_lock->unlock();
    /*
     If some error happend in the worker, or shutdown we need to unblock and
     stop the coordinator.
    */
    if (m_rli->sql_thread_kill_accepted || m_rli->is_error()) {
      global_tsid_lock->rdlock();
      status = true;
      break;
    }
    global_tsid_lock->rdlock();
  }
  global_tsid_lock->unlock();
  return status;
}

bool Until_after_gtids::check_all_transactions_read_from_relay_log() {
  DBUG_TRACE;
  return check_all_transactions_executed();
}

int Until_view_id::init(const char *view_id) {
  until_view_id_found = false;
  until_view_id_commit_found = false;

  try {
    m_view_id.assign(view_id);
  } catch (...) {
    return ER_OUTOFMEMORY;
  }
  return 0;
}

bool Until_view_id::check_at_start_slave() { return false; }

bool Until_view_id::check_before_dispatching_event(const Log_event *ev) {
  if (ev->get_type_code() == mysql::binlog::event::VIEW_CHANGE_EVENT) {
    View_change_log_event *view_event = const_cast<View_change_log_event *>(
        static_cast<const View_change_log_event *>(ev));

    if (m_view_id.compare(view_event->get_view_id()) == 0) {
      set_group_replication_retrieved_certification_info(view_event);
      until_view_id_found = true;
      return false;
    }
  }

  if (until_view_id_found && ev->ends_group()) {
    until_view_id_commit_found = true;
    return false;
  }

  return false;
}

bool Until_view_id::check_after_dispatching_event() {
  return until_view_id_commit_found;
}

bool Until_view_id::check_all_transactions_read_from_relay_log() {
  return false;
}

void Until_mts_gap::init() {
  m_rli->opt_replica_parallel_workers = m_rli->recovery_parallel_workers;
}

bool Until_mts_gap::check_at_start_slave() { return false; }

bool Until_mts_gap::check_before_dispatching_event(const Log_event *) {
  if (m_rli->mts_recovery_group_cnt == 0) {
    LogErr(INFORMATION_LEVEL, ER_REPLICA_SQL_THREAD_STOPPED_GAP_TRX_PROCESSED);
    m_rli->until_condition = Relay_log_info::UNTIL_DONE;
    return true;
  }
  return false;
}

bool Until_mts_gap::check_after_dispatching_event() { return false; }

bool Until_mts_gap::check_all_transactions_read_from_relay_log() {
  return false; /* purecov: inspected */
}
