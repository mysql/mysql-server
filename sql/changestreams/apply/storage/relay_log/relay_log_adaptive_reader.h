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

#ifndef MYSQL_CSA_RELAY_LOG_ADAPTIVE_READER_H
#define MYSQL_CSA_RELAY_LOG_ADAPTIVE_READER_H

#include <memory>
#include <vector>
#include "mysql/binlog/event/trx_boundary_parser.h"
#include "mysql/scheduler/statistics_instance_monitor.h"
#include "mysql/utils/return_status.h"
#include "sql/changestreams/apply/context/tune.h"
#include "sql/changestreams/apply/jobs/job_applier.h"
#include "sql/changestreams/apply/resource/sliding_window_counter.h"
#include "sql/changestreams/apply/storage/common/reader.h"
#include "sql/changestreams/apply/storage/relay_log/event_reader_controller.h"
#include "sql/changestreams/apply/storage/relay_log/event_set_fetchable_cache.h"
#include "sql/changestreams/apply/storage/relay_log/event_set_fetchable_relay_log.h"
#include "sql/changestreams/apply/storage/relay_log/ireader_event.h"
#include "sql/changestreams/apply/storage/relay_log/reader_controller_read_type.h"
#include "sql/changestreams/apply/storage/relay_log/relay_log_deleter.h"
#include "sql/log_event.h"  // Format_description_log_event
#include "sql/rpl_applier_reader.h"
#include "sql/rpl_rli.h"  // Relay_log_info

namespace mysql::csa {

/// @brief Iterates over relay log and returns fetchable Jobs
/// This reader reads consecutive events from the relay log and caches events.
/// When 'max_read_event_bytes' is reached for a single transaction, the
/// reader switches to "metadata" mode. From this point, it will read
/// only event metadata from the relay log and supply event set batches
/// that are able to fetch themselves from the relay log on demand (durring
/// apply)
class Relay_log_adaptive_reader : public Reader {
 public:
  /// @param instance_id Instance (channel) id
  /// @param rli RLI for the channel
  /// @param max_read_event_bytes The maximum number of bytes in a transaction
  /// which reader can read, decode and cache
  /// @param max_read_payload_bytes The maximum number of bytes in a transaction
  /// which reader can read and cache payload
  Relay_log_adaptive_reader(int instance_id, Relay_log_info *rli,
                            std::size_t max_read_event_bytes,
                            std::size_t max_read_payload_bytes);
  virtual ~Relay_log_adaptive_reader() override;

  Relay_log_adaptive_reader(const Relay_log_adaptive_reader &) = delete;
  Relay_log_adaptive_reader &operator=(const Relay_log_adaptive_reader &) =
      delete;
  Relay_log_adaptive_reader(Relay_log_adaptive_reader &&) = delete;
  Relay_log_adaptive_reader &operator=(Relay_log_adaptive_reader &&) = delete;

  /// @brief Reads the next Job (full transaction) from the relay log and
  /// supplies a fetchable job object. This function may block for
  /// a while in case it is reading from an active relay log and when being
  /// outside of transaction boundary.
  /// @return Pointer to Job in case reading succeeded. Empty pointer in case
  /// of failure or stop of the reader
  Job_ptr read() override;

  /// Checks whether CSA is stopped
  /// @brief True if stop was requested, false otherwise
  bool is_stopped() const override;

  /// Checks whether reader errored out
  /// True - reader errored out. False - no error.
  bool is_error() const override;

  /// Awakes and stops the reader
  void stop() override;

 private:
  /// Tunes reader parameters based on CSA statistics
  void tune();

  /// @brief Pointer to the relay log context of applier thread that launches
  /// CSA. It contains pointer to the actual relay log object needed for
  /// reading transactions
  Relay_log_info *m_rli{nullptr};
  /// Applier reader, object used to read event METADATA (non owning pointer)
  Event_reader_controller *m_reader{nullptr};
  /// Owning pointer of applier reader
  Log_purge_controller_sptr m_shared_controller;
  /// Deleter for the current file
  Relay_log_deleter_handle m_delete_handler{nullptr};

  Log_prefetcher_sptr m_prefetcher;

  /// @brief Current FDE pointer. Source's FDEs are attached to transactions
  std::shared_ptr<Log_event> m_current_fde;

  using Transaction_boundary_parser =
      mysql::binlog::event::Transaction_boundary_parser;
  Transaction_boundary_parser m_transaction_boundary_parser{
      Transaction_boundary_parser::TRX_BOUNDARY_PARSER_APPLIER};

  // Stateful metadata stream for currently assembled transaction.
  std::shared_ptr<Fetchable_transaction> m_active_fetchable_transaction;
  // Start metadata for current on-demand batch.
  Event_file_metadata m_start_batch_metadata;
  // Current read mode and next read mode computed from GTID trx length.
  Reader_controller_read_type m_read_type{Reader_controller_read_type::event};
  Reader_controller_read_type m_next_read_type{
      Reader_controller_read_type::event};
  // Currently open stream batch for cached events.
  Event_set_fetchable_cache *m_open_cache_batch{nullptr};
  // Currently open stream batch for metadata-mode events.
  Event_set_fetchable_relay_log *m_open_stream_batch{nullptr};
  // Maximum event size seen in currently assembled transaction.
  std::size_t m_current_transaction_max_event_length{0};
  // Reader-side transaction boundary state persisted across read() calls.
  bool m_is_in_trx{false};

  /// Previous sequence number recorded to validate timestamps
  int64_t m_prev_seq{-1};
  /// Internal error flag
  bool m_is_error{false};
  /// Stop flag, set externally or by reached until condition
  bool m_stopped{false};
  /// Unique instance id for statistics monitoring
  unsigned int m_instance_id{0};
  /// Statistics monitoring object for the current instance
  scheduler::Statistics_instance_monitor_ref m_stat_monitor;
  /// Resource monitoring object for the current instance
  Resource_instance_monitor_ref m_resource_monitor;
  /// @brief The maximum number of bytes that this reader can cache for a single
  /// transaction with decoding. When this limit is reached, reader switches to
  /// payload cache read mode.
  std::size_t m_max_read_event_bytes{tune::provider_max_read_event_bytes};
  /// @brief The maximum number of bytes that this reader can cache for a single
  /// transaction without decoding. When this limit is reached, reader
  /// switches to reading of transaction metadata
  std::size_t m_max_read_payload_bytes{tune::provider_max_read_payload_bytes};
  /// Owning pointer to channel object
  std::unique_ptr<Channel> m_channel;
  /// Threshold below which we ask workers to read transactions in order to
  /// increase reader and receiver throughput
  double m_worker_min_load_threshold{0.3};
  /// Threshold above which we go back to previous settings
  double m_worker_max_load_threshold{0.8};
  /// Used to calculate workers load
  Sliding_window_counter m_thp_task_exec_time{0};
  /// Used to calculate workers load
  Sliding_window_counter m_thp_worker_exec_time{0};
  /// Time point at which we tuned parameters for the last time
  std::chrono::time_point<std::chrono::system_clock> m_last_tune_time{
      std::chrono::system_clock::now()};
  /// We tune parameters each m_tune_period_ms milliseconds
  long int m_tune_period_ms{5000};
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_RELAY_LOG_ADAPTIVE_READER_H
