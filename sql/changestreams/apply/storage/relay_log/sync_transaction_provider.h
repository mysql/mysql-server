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

#ifndef MYSQL_CSA_SYNC_TRANSACTION_PROVIDER_H
#define MYSQL_CSA_SYNC_TRANSACTION_PROVIDER_H

#include <fstream>
#include <functional>
#include <memory>
#include <optional>

#include "mysql/concurrency/condition_variable.h"
#include "mysql/concurrency/mutex.h"
#include "mysql/concurrency/sync_bounded_queue.h"
#include "mysql/concurrency/thread.h"
#include "mysql/scheduler/logger_stream.h"
#include "mysql/scheduler/statistics_monitor.h"
#include "mysql/utils/return_status.h"
#include "sql/binlog.h"
#include "sql/changestreams/apply/core/transaction_provider.h"
#include "sql/changestreams/apply/jobs/job.h"
#include "sql/changestreams/apply/storage/common/reader.h"
#include "sql/changestreams/apply/storage/relay_log/data_source.h"

namespace mysql::csa {

class Sync_transaction_provider;
using Sync_transaction_provider_sptr =
    std::unique_ptr<Sync_transaction_provider>;

/// Implementation of 'Transaction_provider' interface.
/// This implementation uses the relay log reader to read consecutive
/// events from the relay log. Data is read from prefetched stream.
/// Main methods are:
/// - start : runs asynchronous thread fetching transactions from the relay log
/// - stop : stops execution and blocks until thread is joined
/// - next : blocks until fetching the next transaction, stop or timeout
///          when outside of transaction boundary
/// - is_stopped : Checks whether stop has been requested (externally or by
///                the parent thread)
class Sync_transaction_provider : public Transaction_provider {
 public:
  /// @param instance_id Instance (channel) id
  /// @param rli Pointer to relay log info structure
  /// @param max_read_event_bytes The maximum number of bytes in a transaction
  /// which reader can read, decode and cache
  /// @param max_read_payload_bytes The maximum number of bytes in a transaction
  /// which reader can read and cache payload
  Sync_transaction_provider(int instance_id, Relay_log_info *rli,
                            std::size_t max_read_event_bytes,
                            std::size_t max_read_payload_bytes);

  /// Starts asynchronous thread that decodes jobs from the stream
  void start() override;
  /// Stops provider and wakes blocked reader calls.
  void stop() override;
  /// Completes provider shutdown from the owner thread (transaction receiver).
  /// Requires stop to have been called first; otherwise it is a no-op.
  void finish() override;

  /// Consumes the next Job, blocks until fetched
  /// @retval Job smart pointer
  /// @retval Empty pointer in case stop has been requested (check with
  /// 'is_stopped') or we timed out waiting for event. In case we timed out,
  /// we return an empty pointer to wake up parent thread for a while, so that
  /// it can do some maintenance activities, such as checking status or
  /// checking statistics
  Job_ptr next() override;

  /// Checks if stop has been requested
  /// @return True if stop has been requested internally (error) or externally
  /// (log wait for update)
  bool is_stopped() const override;

  /// @brief Check if provider has an error
  /// @return True in case an error occurred, false otherwise
  bool is_error() const override;

 private:
  /// Variable to gracefully stop the thread
  std::atomic<bool> m_is_stopped{false};

  /// Pointer to channel rli object
  Relay_log_info *m_rli;

  /// Shared reader object
  Reader_sptr m_reader;

  /// Statistics monitoring object
  scheduler::Statistics_instance_monitor_ref m_stat_monitor;
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_SYNC_TRANSACTION_PROVIDER_H
