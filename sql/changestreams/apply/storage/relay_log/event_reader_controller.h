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

#ifndef MYSQL_CSA_RELAY_LOG_EVENT_READER_CONTROLLER_H
#define MYSQL_CSA_RELAY_LOG_EVENT_READER_CONTROLLER_H

#include <memory>
#include <optional>
#include "mysql/binlog/event/trx_boundary_parser.h"
#include "mysql/concurrency/condition_variable.h"
#include "mysql/concurrency/mutex.h"
#include "mysql/concurrency/sync_bounded_queue.h"
#include "mysql/concurrency/thread.h"
#include "mysql/utils/return_status.h"
#include "sql/changestreams/apply/context/tune.h"
#include "sql/changestreams/apply/core/defs.h"
#include "sql/changestreams/apply/jobs/job_applier.h"
#include "sql/changestreams/apply/storage/relay_log/event_file_metadata.h"
#include "sql/changestreams/apply/storage/relay_log/log_prefetcher.h"
#include "sql/changestreams/apply/storage/relay_log/log_purge_controller.h"
#include "sql/changestreams/apply/storage/relay_log/prefetched_relaylog_reader.h"
#include "sql/changestreams/apply/storage/relay_log/reader_controller_read_type.h"
#include "sql/changestreams/apply/storage/relay_log/relay_log_deleter.h"
#include "sql/log_event.h"  // Format_description_log_event
#include "sql/rpl_applier_reader.h"
#include "sql/rpl_rli.h"  // Relay_log_info

namespace mysql::csa {

/// @brief The Event Reader / Controller class. This class uses the low level
/// reader to read consecutive events
/// from the relay log. Also, it exposed functions to remove
/// consumed relay log files.
/// Class provides methods to:
/// - initialize internal data (open)
/// - deinitialize internal data (close)
/// - fetch the next event from the relay log (read_next)
/// - fetch the next event metadata from the relay log
///   (read_next)
/// - register consumed relay log for purge and purge registered consecutive
///   logs (concurrent_purge), which implements the `Log_purge_controller`
///   interface
/// @details This class may be seen as 'Rpl_applier_reader' created for the CSA.
/// `Relay_log_decoder` works with prefetched relay log files or stream build
/// on top of the IO_CACHE. The first one is used when reading from inactive
/// files. When reading from active files, `Event_reader_controller` utilizes
/// the IO_CACHE implementation, since it allows the applier to fetch
/// data from cache instead of fetching data from disk. Data will be consumed
/// from cache provided that the applier keeps up with Receiver thread and
/// cache is 'large' enough to keep the recent data.
/// When reading from inactive files, the 'Event_reader_controller' utilizes
/// the prefetcher class utility to fetch data. When reading from active
/// files, the Event Reader / Controller needs to rely on the 'MYSQL_BIN_LOG'
/// synchronization primitives and supply the implementation of passive
/// waiting for data (is_data_available, wait_data_ready).
/// Following the legacy design, streams implemented on top of prefetcher
/// are allowed to move to the next file upon the caller request. Therefore,
/// the 'Event_reader_controller' is responsible for checking file boundaries
/// and reopening the streams on top of new files when needed.
class Event_reader_controller : public Log_purge_controller {
 public:
  Event_reader_controller(Relay_log_info *rli, Log_prefetcher_sptr prefetcher);
  /// Opens the first relay log
  /// @retval false Success
  /// @retval true failure
  bool open();
  /// Closes readers, stops prefetcher, clears internal state including error
  /// state
  void close();

  // template <Reader_controller_read_type read_type>
  // Reader_return_type<read_type>::type read(unsigned int return_timeout_ms) {
  //   if constexpr (read_type == Reader_controller_read_type::event) {
  //     auto tt =
  //   } else if constexpr (read_type == Reader_controller_read_type::metadata)
  //   { } else if constexpr (read_type ==
  //   Reader_controller_read_type::metadata) { } else {
  //     static_assert("not supported");
  //   }
  // }

  /// @brief Fetches next: event data, event metadata, event metadata
  /// plus raw payload, depending on read type
  /// @param return_timeout_ms If specified, will wait up to `return_timeout_ms`
  /// miliseconds. If timeout is reached, returns true.
  /// @param read_type Type of read: full event, event metadata or event
  /// metadata and raw payload
  /// @return Next: event data, event metadata, event metadata + raw payload
  /// depending on read type. Empty object if an error occurred
  std::optional<Event_file_metadata> read_next(
      unsigned int return_timeout_ms, Reader_controller_read_type read_type);

  /// @brief Register log_filename as a log ready to be purged. This function
  /// will purge registered logs if they are in order according to
  /// the index file content.
  /// @param[in] log_filename Registeres this log as a log ready to be purged.
  /// If registered logs for purging are in order, purges up to log_filename,
  /// included.
  bool concurrent_purge(const std::string &log_filename) override;

  /// @brief Obtain currently processed file
  /// @return Current file name
  const std::string &get_file_name() const { return m_file_name; }

  /// @brief Check if reader has an error
  /// @return True in case an error occurred, false otherwise
  bool is_error() const { return m_is_error; }

  /// Stop the reader
  void stop();

  /// Check if reader is stopped
  /// @return True when stop was requested; false otherwise
  bool is_stopped() const;

 private:
  /// When next_log is true, opens the next file. Otherwise, opens the current
  /// file
  /// @param next_log When true, moves to next log after the current
  /// @param offset Requested file offset
  /// @retval false Success
  /// @retval true failure
  bool move_to_log(bool next_log = true, my_off_t offset = 0);

  /// Moves to the next log file
  /// @retval false Success
  /// @retval true failure
  bool move_to_next_log();

  /// Purge relay log files up to to_log
  /// @param to_log Purge logs up to this log, exclusively
  /// @retval false Success
  /// @retval true Error
  bool purge_applied_logs(const char *to_log);

  /// In case we read from active file, we use this function to passively
  /// wait for new event
  /// @param return_timeout_ms Timeout after which we will exit from waiting
  /// @retval false New event is available
  /// @retval true Timeout occurred
  bool wait_for_new_event(unsigned int return_timeout_ms);

  /// @brief If cache is truncated, reopen the reader to avoid reading trash
  /// data
  /// @details Hack function that solves problem of relay log IO_CACHE
  /// truncation on active relay log files
  /// @return true if failure when reopening the file, false on success
  /// @see Rpl_applier_reader::reopen_log_reader_if_needed
  bool check_cache_truncated();

  /// Sets internal error to msg
  /// @param msg Error message
  /// @return Error state : true
  bool set_error(const char *msg);

  /// Checks whether there is data in the current file
  /// @retval true Data is available
  /// @retval false No data
  bool is_data_available();

  /// Waits until data is available, stopped or return_timeout_ms is reached
  /// @param return_timeout_ms Timeout after which we will exit from waiting
  /// @retval true Data is available
  /// @retval false Data is not ready / stopped / error
  bool wait_data_ready(unsigned int return_timeout_ms);

  /// Implements internal logic to choose between active and inactive file
  /// reading
  /// @param next_log True if open was called on the next log
  /// @param prev_file Previous relay log file processed
  void choose_reader(bool next_log, const std::string &prev_file);

  /// Flag which is true in case any error occurred. Otherwise, it is set to
  /// false.
  bool m_is_error{false};
  /// Error message if any
  std::string m_error_msg{""};
  /// non-owning RLI object pointer
  Relay_log_info *m_rli{nullptr};
  /// Relay log prefetcher
  Log_prefetcher_sptr m_prefetcher;
  /// Reader for active files
  Relaylog_file_reader m_active_reader;
  /// Reader for inactive files
  Prefetched_relaylog_reader m_inactive_reader;
  /// Non-owning pointer to currently used reader (m_inactive_reader or
  /// m_active_reader)
  IBasic_binlog_file_reader *m_current_reader{nullptr};
  /// @brief Stores the current file as obtaining from stream
  std::string m_file_name;
  /// Here we keep the list of logs to be purged (in case later logs are
  /// applied before), protected with m_rli->data_lock
  std::unordered_set<std::string> m_logs_to_purge;
  /// Flag indicating whether currently opened log file is active
  bool m_using_prefetcher{false};
  /// This flag is true in case we are using active file reader and
  /// reading from the active relay log file
  bool m_active_file_reading{false};
  /// Variable to decide on whether we want to run prefetcher
  bool m_enable_prefetcher{tune::prefetcher_enable};
  /// Stop flag
  std::atomic<bool> m_is_stopped{false};
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_RELAY_LOG_EVENT_READER_CONTROLLER_H
