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

#ifndef MYSQL_CSA_STORAGE_IN_MEMORY_QUEUED_TRANSACTION_READER_H
#define MYSQL_CSA_STORAGE_IN_MEMORY_QUEUED_TRANSACTION_READER_H

#include <atomic>
#include <cstdint>
#include <memory>

#include "mysql/scheduler/statistics_instance_monitor.h"
#include "sql/changestreams/apply/resource/resource_monitor.h"
#include "sql/changestreams/apply/storage/common/reader.h"

class Format_description_log_event;
class Relay_log_info;

namespace mysql::csa {

class Channel;
class Trx_envelope_queue;

namespace unittests {
// Unit test fixtures granted access to the queue-only test constructor below.
class Queued_transaction_reader_test;
class Imr_integration_test;
}  // namespace unittests

/// @brief Queue-based transaction reader that builds Job_applier work items.
///
/// This reader is the memory-path counterpart of Relay_log_adaptive_reader.
/// Each read() takes a shared ownership copy of that transaction's
/// Fetchable_transaction, and wraps it in a heap Job_applier for the worker
/// pool.
class Queued_transaction_reader : public Reader {
 public:
  /// @param instance_id Instance (channel) id.
  /// @param rli RLI for the channel; supplies the channel name and commit order
  ///        manager used to build the per-job Channel.
  /// @param queue The per-channel FIFO of transaction envelopes to drain. Must
  ///        be non-null and outlive this reader (held non-owning).
  Queued_transaction_reader(int instance_id, Relay_log_info *rli,
                            Trx_envelope_queue *queue);
  ~Queued_transaction_reader() override;

  Queued_transaction_reader(const Queued_transaction_reader &) = delete;
  Queued_transaction_reader &operator=(const Queued_transaction_reader &) =
      delete;
  Queued_transaction_reader(Queued_transaction_reader &&) = delete;
  Queued_transaction_reader &operator=(Queued_transaction_reader &&) = delete;

  /// @brief Reads the next transaction from the queue as a Job.
  ///
  /// Waits for the next envelope or a stop. Stops before the first transaction
  /// START REPLICA UNTIL excludes, and with an error once a spill file could
  /// not be deleted.
  ///
  /// @return Pointer to a Job on success; empty pointer on stop or idle poll.
  Job_ptr read() override;

  /// @brief Checks whether the reader is stopped.
  /// Coordinator thread only: sql_slave_killed() changes SQL-thread state.
  /// @return true if a stop was requested (locally, or by STOP, KILL or
  ///         shutdown through sql_slave_killed()) or an error occurred, false
  ///         otherwise.
  bool is_stopped() const override;

  /// @brief Checks whether the reader errored out.
  /// @return true if an error occurred, false otherwise.
  bool is_error() const override;

  /// @brief Awakes and stops the reader.
  ///
  /// Sets the local stopped flag and delegates to Trx_envelope_queue::stop(),
  /// which wakes a reader blocked in dispatch_next().
  void stop() override;

 private:
  // Grants the unit tests access to the queue-only test constructor so the
  // read() path can be exercised without a live Relay_log_info/Channel.
  friend class unittests::Queued_transaction_reader_test;
  friend class unittests::Imr_integration_test;

  /// @brief Test-only constructor that wires ONLY the envelope queue.
  ///
  /// @param queue The per-channel FIFO to drain. Must be non-null and outlive
  ///        this reader (held non-owning).
  /// Used by imr_queued_transaction_reader-t and imr_integration-t.
  explicit Queued_transaction_reader(Trx_envelope_queue *queue);

  /// @brief UNTIL stop: sets abort_slave, latches the kill, sets m_stopped.
  /// Coordinator thread only: sql_slave_killed() changes SQL-thread state.
  void stop_for_until();

  /// Relay log context of the applier thread that launches CSA (non-owning).
  Relay_log_info *m_rli{nullptr};
  /// The per-channel envelope queue this reader drains (non-owning).
  Trx_envelope_queue *m_queue{nullptr};
  /// Owning pointer to the channel object passed to each Job_applier.
  std::unique_ptr<Channel> m_channel;
  /// Statistics monitoring object for the current instance.
  scheduler::Statistics_instance_monitor_ref m_stat_monitor;
  /// Resource monitoring object for the current instance.
  Resource_instance_monitor_ref m_resource_monitor;
  /// Previous sequence number recorded to validate timestamps.
  int64_t m_prev_seq{-1};
  /// Payload FDE of the previous checked transaction; a new one (a snapshot
  /// taken for a new source FDE event) resets m_prev_seq.
  std::shared_ptr<Format_description_log_event> m_prev_fde;
  /// Internal error flag.
  bool m_is_error{false};
  /// Stop flag, set by stop() (STOP thread) and by stop_for_until(); read by
  /// the coordinator.
  std::atomic<bool> m_stopped{false};
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_IN_MEMORY_QUEUED_TRANSACTION_READER_H
