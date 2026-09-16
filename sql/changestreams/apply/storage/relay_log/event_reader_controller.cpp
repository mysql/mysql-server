// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include "sql/changestreams/apply/storage/relay_log/event_reader_controller.h"
#include <fstream>
#include "include/mysqld_errmsg.h"  // ER_OUT_OF_RESOURCES_MSG
#include "sql/changestreams/apply/psi/psi.h"
#include "sql/sql_backup_lock.h"

using namespace mysql::binlog::event;

namespace mysql::csa {

Event_reader_controller::Event_reader_controller(Relay_log_info *rli,
                                                 Log_prefetcher_sptr prefetcher)
    : m_rli(rli),
      m_prefetcher(prefetcher),
      m_active_reader(
          opt_replica_sql_verify_checksum,
          std::max(replica_max_allowed_packet,
                   binlog_row_event_max_size + MAX_LOG_EVENT_HEADER)),
      m_inactive_reader(
          opt_replica_sql_verify_checksum,
          std::max(replica_max_allowed_packet,
                   binlog_row_event_max_size + MAX_LOG_EVENT_HEADER)) {
  // initialize, don't start reading, prefetcher is working though
  if (m_enable_prefetcher) {
    m_inactive_reader.ifile()->set_prefetcher(prefetcher);
    m_prefetcher->start_prefetcher();
  }
}

bool Event_reader_controller::set_error(const char *msg) {
  m_is_error = true;
  m_error_msg.assign(msg);
  // ignore errors on stop
  if (!is_stopped()) {
    m_rli->report(ERROR_LEVEL, ER_REPLICA_RELAY_LOG_READ_FAILURE,
                  ER_THD(current_thd, ER_REPLICA_RELAY_LOG_READ_FAILURE), msg);
  }
  return true;
}

bool Event_reader_controller::open() {
  m_file_name.assign(m_rli->get_group_relay_log_name());
  auto ret = move_to_log(false, BIN_LOG_HEADER_SIZE);
  if (!ret) {
    MUTEX_LOCK(lock, &m_rli->data_lock);
    m_rli->set_group_relay_log_name(m_file_name.c_str());
    m_rli->set_group_relay_log_pos(BIN_LOG_HEADER_SIZE);
    if (relay_log_purge == 0 && m_rli->log_space_limit > 0) {
      m_rli->log_space_limit = 0;
      LogErr(WARNING_LEVEL, ER_RELAY_LOG_SPACE_LIMIT_DISABLED);
    }
  }
  return ret;
}

void Event_reader_controller::choose_reader(bool next_log,
                                            const std::string &prev_file) {
  if (m_enable_prefetcher) {
    if (!m_using_prefetcher ||
        (next_log &&
         m_prefetcher->is_waiting_for_next_file(prev_file, m_file_name))) {
      m_using_prefetcher = m_prefetcher->open(m_file_name.c_str());
    }
  }
  if (m_using_prefetcher) {
    m_current_reader = &m_inactive_reader;
    m_active_file_reading = false;
  } else {
    m_current_reader = &m_active_reader;
    m_active_file_reading = m_rli->relay_log.is_active(m_file_name.c_str());
  }
}

bool Event_reader_controller::move_to_log(bool next_log, my_off_t offset) {
  if (m_current_reader) {
    m_current_reader->close();
  }
  m_rli->relay_log.lock_index();
  auto guard_unlock_index =
      create_scope_guard([this] { m_rli->relay_log.unlock_index(); });
  Log_info m_linfo;
  if (!m_file_name.empty() &&
      m_rli->relay_log.find_log_pos(&m_linfo, m_file_name.c_str(), false)) {
    return set_error("Cannot find current log position");
  }
  if (!m_rli->relay_log.is_open()) {
    return set_error("Relay log is closed");
  }
  std::string prev_file = m_file_name;
  if (next_log || m_file_name.empty()) {
    if (m_rli->relay_log.find_next_log(&m_linfo, false)) {
      return set_error("Cannot find next log");
    }
  }
  m_file_name.assign(m_linfo.log_file_name);
  guard_unlock_index.reset();  // unlock index here
  choose_reader(next_log, prev_file);
  if (m_current_reader->open(m_file_name.c_str(), offset)) {
    const auto *reader_error = m_current_reader->get_error_str();
    if (reader_error != nullptr && reader_error[0] != '\0') {
      return set_error(reader_error);
    }
    return set_error("Cannot open relay log file.");
  }
  return false;
}

void Event_reader_controller::close() {
  if (m_enable_prefetcher) {
    m_prefetcher->stop();
  }
  m_current_reader->close();
  m_current_reader = nullptr;
  m_using_prefetcher = false;
  m_is_error = false;
  m_error_msg = "";
}

bool Event_reader_controller::check_cache_truncated() {
  mysql_mutex_assert_owner(&m_rli->data_lock);
  if (!m_using_prefetcher && m_rli->is_relay_log_truncated()) {
    m_rli->clear_relay_log_truncated();
    my_off_t pos = m_current_reader->position();
    m_current_reader->close();
    if (m_current_reader->open(m_file_name.c_str()) ||
        m_current_reader->seek(pos))
      return true;
  }
  return false;
}

bool Event_reader_controller::is_data_available() {
  /// For prefetcher, we don't wait for data here, because passive waiting for
  /// data is implemented in the prefetcher itself.
  /// For active relay log file reading, we need implement passive waiting
  /// within the `Event_reader_controller` and using the MYSQL_BIN_LOG
  /// synchronization primitives.
  if (m_using_prefetcher) {
    return true;
  }
  // for active files, we need to check relay_log data
  auto log_end_pos = m_rli->relay_log.get_binlog_end_pos();
  m_active_file_reading = m_rli->relay_log.is_active(m_file_name.c_str());
  if (log_end_pos > m_current_reader->position() || !m_active_file_reading) {
    check_cache_truncated();
    return true;
  }
  return false;
}

bool Event_reader_controller::wait_data_ready(unsigned int return_timeout_ms) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&m_rli->data_lock);
  bool is_error{false};
  bool flag_timeout{false};

  auto stop_waiting = [ this, &is_error, &flag_timeout ]() -> auto{
    return is_stopped() || is_data_available() || is_error || flag_timeout ||
           !m_active_file_reading;
  };

  bool do_stop = stop_waiting();

  while (!do_stop) {
    m_rli->relay_log.lock_binlog_end_pos();
    auto scoped_unlock_binlog_end_pos = create_scope_guard(
        [this] { m_rli->relay_log.unlock_binlog_end_pos(); });
    do_stop = stop_waiting();
    if (do_stop) {
      break;
    }
    if (m_rli->is_until_satisfied_all_transactions_read_from_relay_log()) {
      // Make it stop on the next execution
      m_rli->abort_slave = true;
      return false;
    }
    mysql_mutex_unlock(&m_rli->data_lock);
    int ret = 0;

    concurrency::set_thd_stage(current_thd,
                               stage_replica_has_read_all_relay_log);

    if (return_timeout_ms > 0) {
      std::chrono::nanoseconds timeout =
          std::chrono::nanoseconds{return_timeout_ms * 1000000};
      ret = m_rli->relay_log.wait_for_update(timeout);
    } else {
      // std::chrono::nanoseconds timeout =
      //     std::chrono::nanoseconds{100 * 1000000};
      ret = m_rli->relay_log.wait_for_update();
    }

    scoped_unlock_binlog_end_pos.reset();
    flag_timeout = is_timeout(ret) && return_timeout_ms;
    is_error = !is_timeout(ret) && ret != 0;
    if (!flag_timeout) {
      concurrency::set_thd_stage(current_thd, stage_csa_working);
    }
    mysql_mutex_lock(&m_rli->data_lock);
  }
  if (is_stopped() || is_error || flag_timeout) {
    return false;
  }
  return true;
}

bool Event_reader_controller::is_stopped() const {
  return sql_slave_killed(m_rli->info_thd, m_rli) || m_is_stopped;
}

void Event_reader_controller::stop() {
  // set flags before poking the reader thread
  m_is_stopped = true;
  m_rli->abort_slave = true;
  // poke the reader to wake up
  m_rli->relay_log.update_binlog_end_pos(true);
}

std::optional<Event_file_metadata> Event_reader_controller::read_next(
    unsigned int return_timeout_ms, Reader_controller_read_type read_type) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&m_rli->data_lock);

  DBUG_EXECUTE_IF("block_applier_updates", {
    const char act[] =
        "now SIGNAL applier_read_blocked WAIT_FOR resume_applier_read";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });
  DBUG_EXECUTE_IF("force_sql_thread_error", {
    set_error(
        "Could not parse relay log event entry. The possible reasons "
        "are: the source's "
        "binary log is corrupted (you can check this by running "
        "'mysqlbinlog' on the "
        "binary log), the replica's relay log is corrupted (you can "
        "check this by running "
        "'mysqlbinlog' on the relay log), a network problem, the server "
        "was unable to "
        "fetch a keyring key required to open an encrypted relay log "
        "file, or a bug in "
        "the source's or replica's MySQL code. If you want to check the "
        "source's binary "
        "log or replica's relay log, you will be able to know their "
        "names by issuing "
        "'SHOW REPLICA STATUS' on this replica.");
    return {};
  });

  if (is_error()) {
    return {};
  }

  if (!wait_data_ready(return_timeout_ms)) {
    // purge if space full
    if (m_rli->is_receiver_waiting_for_rl_space.load()) {
      // concurrent_purge locks data_lock
      mysql_mutex_unlock(&m_rli->data_lock);
      concurrent_purge("");
      mysql_mutex_lock(&m_rli->data_lock);
    }
    return {};
  }

  auto pos = m_current_reader->position();
  auto opt_header = m_current_reader->read_event_metadata();
  if (opt_header.has_value()) {
    auto file = m_file_name;
    auto &metadata = opt_header.value();

    bool ignore_read_type =
        metadata.get_type() == FORMAT_DESCRIPTION_EVENT ||
        metadata.get_type() == INCIDENT_EVENT ||
        Log_event_type_helper::is_any_gtid_event(metadata.get_type());

    // special cases
    if (metadata.get_type() == mysql::binlog::event::QUERY_EVENT) {
      auto *ev = m_current_reader->read_event_payload(metadata);
      if (!ev) {
        set_error(m_current_reader->get_error_str());
        return {};
      }
      Query_log_event *qev = dynamic_cast<Query_log_event *>(ev);
      auto is_atomic_ddl = qev->ddl_xid != mysql::binlog::event::INVALID_XID;
      std::string query{""};
      query.assign(qev->query, qev->q_len);
      return Event_file_metadata(metadata, file, pos, query, is_atomic_ddl, ev);
    } else if (ignore_read_type) {
      auto *ev = m_current_reader->read_event_payload(metadata);
      if (!ev) {
        set_error(m_current_reader->get_error_str());
        return {};
      }
      return Event_file_metadata(metadata, file, pos, ev);
    }

    if (read_type == Reader_controller_read_type::event) {
      auto *ev = m_current_reader->read_event_payload(metadata);
      if (!ev) {
        set_error(m_current_reader->get_error_str());
        return {};
      }
      return Event_file_metadata(metadata, file, pos, ev);
    } else if (read_type == Reader_controller_read_type::cache_metadata) {
      auto data = m_current_reader->read_payload(metadata);
      if (!data) {
        set_error(m_current_reader->get_error_str());
        return {};
      }
      return Event_file_metadata(metadata, file, pos, nullptr, data);
    }
    // have metadata
    if (m_current_reader->skip_event_payload(metadata)) {
      set_error("Seek failure");
      return {};
    }
    return Event_file_metadata(metadata, file, pos, nullptr);
  }

  if (m_current_reader->get_error_type() == Binlog_read_error::READ_EOF &&
      !m_active_file_reading) {
    if (!move_to_log()) return read_next(return_timeout_ms, read_type);
  }

  // if we fail to move to a new log when the thread is killed, ignore it
  if (!current_thd || !current_thd->is_killed()) {
    set_error(m_current_reader->get_error_str());
  }
  return {};
}

bool Event_reader_controller::purge_applied_logs(const char *to_log) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&m_rli->data_lock);

  if (!relay_log_purge) return true;

  // lock BACKUP lock for the duration of PURGE operation
  assert(current_thd);
  Shared_backup_lock_guard backup_lock{current_thd};
  switch (backup_lock) {
    case Shared_backup_lock_guard::Lock_result::locked:
      break;
    case Shared_backup_lock_guard::Lock_result::not_locked: {
      LogErr(WARNING_LEVEL, ER_LOG_CANNOT_PURGE_BINLOG_WITH_BACKUP_LOCK);
      return true;
    }
    case Shared_backup_lock_guard::Lock_result::oom: {
      return set_error(ER_OUT_OF_RESOURCES_MSG);
    }
  }

  if (m_rli->flush_info(Relay_log_info::RLI_FLUSH_IGNORE_SYNC_OPT &
                        Relay_log_info::RLI_FLUSH_IGNORE_GTID_ONLY) &&
      (!current_thd || !current_thd->is_killed())) {
    return set_error("Error purging processed logs - flush error.");
  }

  m_rli->relay_log.lock_index();

  mysql_mutex_lock(&m_rli->log_space_lock);
  // we can copy to a non-atomic variable and back under the log_space_lock
  auto current_log_space = m_rli->log_space_total.load();
  if (m_rli->relay_log.purge_logs(
          to_log, false /* include */, false /*need_lock_index*/,
          false /*need_update_threads*/, &current_log_space, true) != 0)
    set_error("Error purging processed logs");
  m_rli->log_space_total.store(current_log_space);
  // modify variable before signaling the receiver and under the log_space_lock
  m_rli->coordinator_log_after_purge = to_log;
  mysql_cond_broadcast(&m_rli->log_space_cond);
  mysql_mutex_unlock(&m_rli->log_space_lock);

  m_rli->relay_log.unlock_index();
  return m_is_error;
}

bool Event_reader_controller::concurrent_purge(const std::string &to_log) {
  DBUG_TRACE;
  MUTEX_LOCK(lock, &m_rli->data_lock);
  // when relay log purge is OFF, still subscribe files for deletion
  // inserting to the list of files up for removal
  if (!to_log.empty()) {
    m_logs_to_purge.insert(to_log);
  }
  auto [list_of_files, status] = m_rli->relay_log.get_filename_list();
  if (status.is_error()) return true;
  auto fn_it = list_of_files.begin();
  while (fn_it != list_of_files.end() && m_logs_to_purge.contains(*fn_it)) {
    ++fn_it;
  }
  if (fn_it == list_of_files.end() || fn_it == list_of_files.begin()) {
    // skipping one after last: cannot remove last, possible active log
    // skipping first: no log to remove
    return false;
  }
  std::string next_fn = *fn_it;
  m_rli->set_group_relay_log_name(next_fn.c_str());
  m_rli->set_group_relay_log_pos(BIN_LOG_HEADER_SIZE);
  m_rli->set_event_relay_log_name(m_file_name.c_str());
  // remove up to the current, exclusively
  if (purge_applied_logs(next_fn.c_str())) {
    if (is_error()) {
      return true;
    }
    return false;  // no error, purge was rejected
  }
  for (auto rit = list_of_files.begin(); rit != fn_it; ++rit) {
    m_logs_to_purge.erase(*rit);
  }
  return false;
}

}  // namespace mysql::csa
