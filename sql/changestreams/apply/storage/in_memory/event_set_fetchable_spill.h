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

#ifndef MYSQL_CSA_STORAGE_IN_MEMORY_EVENT_SET_FETCHABLE_SPILL_H
#define MYSQL_CSA_STORAGE_IN_MEMORY_EVENT_SET_FETCHABLE_SPILL_H

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "mysql/utils/return_status.h"
#include "sql/binlog/decompressing_event_object_istream.h"  // Decompressing_event_object_istream
#include "sql/binlog_reader.h"  // Relaylog_file_reader
#include "sql/changestreams/apply/storage/common/event_set_fetchable.h"
#include "sql/changestreams/apply/storage/common/streaming_event_sink.h"

namespace mysql::csa {

class Transaction_envelope;
class Fetchable_transaction;
class Spill_file_writer;
class Trx_envelope_queue;

/// @brief Spill-path counterpart of Event_set_fetchable_memory.
///
/// Streams a large transaction to a private relay-log-format file
/// (Spill_file_writer) and serves it back to a worker.
class Event_set_fetchable_spill : public Event_set_fetchable,
                                  public Streaming_event_sink {
 public:
  /// @brief Shared pointer to a Log_event (owning base pointer for the FDE).
  using Log_event_ptr = std::shared_ptr<Log_event>;
  /// @brief Type alias for return status.
  using Return_status = mysql::utils::Return_status;
  /// @brief Type alias for the decompressing event stream.
  using Stream_type = ::binlog::Decompressing_event_object_istream;
  /// @brief Unique pointer to the decompressing stream.
  using Stream_ptr = std::unique_ptr<Stream_type>;

  /// @brief Constructs an EMPTY spill byte source and opens its backing file.
  ///
  /// Provisions the spill file and writes the relay-log prefix (magic + FDE).
  /// On failure the source is left in a writer error state (has_io_error(),
  /// is_error()) and rejects appends.
  ///
  /// @param is_trx Whether this set represents a real transaction.
  /// @param fde The FDE; also serialized into the spill file prefix.
  /// @param relay_log_dir The server's relay log directory.
  /// @param owner_envelope Envelope truncated/committed; nullptr in tests.
  /// @param streaming_open If true, appends are accepted until seal/truncate.
  /// @param channel_name Embedded in the spill file name (see
  ///        Spill_file_writer::kFileNamePrefix).
  /// @param owner_queue Queue whose spill gauges the writer keeps; nullable.
  /// @param reserved_disk_bytes Admission's spill disk reservation, handed to
  ///        the writer, which releases it after deleting the file.
  /// @param stats_instance_id Channel whose CSA statistics count the commit.
  /// @param applier_stop Polled by the consumer wait; nullptr waits untimed.
  Event_set_fetchable_spill(
      bool is_trx, Log_event_ptr fde, std::string relay_log_dir,
      Transaction_envelope *owner_envelope, bool streaming_open = true,
      std::string channel_name = {}, Trx_envelope_queue *owner_queue = nullptr,
      std::size_t reserved_disk_bytes = 0, std::size_t stats_instance_id = 0,
      const std::atomic<bool> *applier_stop = nullptr);

  /// @brief Destructor (defined out-of-line for the unique_ptr member).
  ~Event_set_fetchable_spill() override;

  bool wait_next() override;
  std::optional<Managed_event> fetch_next() override;
  const std::string &get_error_str() const override;
  bool is_done() const override;
  bool is_error() const override;
  bool is_trx() const override;
  void reset(bool reset_events) override;
  Fde_ptr get_fde() override;

  /// @brief Success callback: commit the owning envelope.
  void set_success() override;

  /// @brief Writes one event's raw encoded bytes to the spill file, flushes,
  /// and publishes the new readable end position.
  ///
  /// @param buf Encoded event bytes, as received. Copied synchronously.
  /// @param len Number of bytes at @p buf.
  /// @param seal_after When true, seal the stream together with this append.
  /// @retval false the bytes were written (or the stream is already closed to
  ///         appends).
  /// @retval true  the spill-file write/flush failed; the reason is recorded
  ///         (io_error_message()) and the stream is truncated.
  bool append_event(const char *buf, std::size_t len,
                    bool seal_after = false) override;

  void seal_stream() override;
  void set_stream_truncated() override;

  /// @brief Whether the writer failed: provisioning in the constructor, or a
  /// write or flush in append_event(). Receiver thread only. A reader
  /// (consumer) failure never sets it.
  bool has_io_error() const override;

  /// @brief The writer's failure reason (empty when none): the provisioning
  /// failure from the constructor or the write failure from append_event().
  /// Receiver thread only. A reader (consumer) failure never changes it.
  const std::string &io_error_message() const override;

  /// @brief Record the Fetchable_transaction that owns this batch.
  ///
  /// @param owning_fetchable The owning transaction, or nullptr (tests).
  void set_owning_fetchable(Fetchable_transaction *owning_fetchable) {
    m_fetchable_trx = owning_fetchable;
  }

#ifndef NDEBUG
  /// @brief Debug only, for unit tests: @p hook runs on the receiver thread on
  /// each seal of a stream that is not truncated, before consumers are
  /// notified. With imr_spill_append_failure_test_hook set, it also runs on a
  /// write failure, between the truncation and recording the writer failure.
  void set_seal_hook_for_test(std::function<void()> hook) {
    m_seal_hook_for_test = std::move(hook);
  }
#endif

  /// @brief The current published readable end position (byte offset the
  /// consumer may read up to). Advances monotonically as events are appended.
  std::size_t published_end_position() const;

  /// @brief Whether the stream has been sealed (fully received).
  bool is_sealed() const;

  /// @brief Whether the stream has been marked truncated (incomplete).
  bool is_stream_truncated() const;

  /// @brief Full path of the backing spill file (empty if provisioning failed).
  const std::string &spill_file_name() const;

 private:
  /// @brief Opens the reader (if needed) and creates the decompressing stream.
  void start_reading();
  /// @brief Opens the reader at the first-event offset (past the FDE prefix).
  void safe_open_reader();
  /// @brief Closes the reader if it is open.
  void safe_close_reader();
  /// @brief Blocks until at least one more event is readable, or the stream is
  /// sealed/truncated/errored.
  /// @retval true  At least one more event can be fetched.
  /// @retval false No more events (sealed/truncated/error).
  bool wait_for_event_availability();
  /// @brief Reads and decodes the next event from the decompressing stream,
  /// handling TPLE decompression and end-of-stream/error.
  std::optional<Managed_event> fetch_from_stream();
  /// @brief Whether the reader is currently decompressing a TPLE.
  bool decompressing() const { return m_decompressing; }
  /// @brief Records the writer's failure (m_writer's error text) in the
  /// writer-only state. Receiver thread only; called at most once.
  void set_writer_failure();

  /// @brief Flag indicating if this is a transaction.
  bool m_is_trx{false};
  /// @brief Owning pointer to Format_description_log_event (base type).
  Log_event_ptr m_fde_base{};
  /// @brief Non-owning typed pointer to the Format_description_log_event.
  Fde_ptr m_fde{nullptr};
  /// @brief Non-owning pointer to the owning envelope (truncate/commit target).
  Transaction_envelope *m_envelope{nullptr};
  /// @brief Channel instance id whose CSA statistics set_success() updates.
  std::size_t m_stats_instance_id{0};
  /// @brief Non-owning pointer to the owning Fetchable_transaction (truncate
  /// propagation), set by Trx_payload::create_spill().
  Fetchable_transaction *m_fetchable_trx{nullptr};
  /// @brief Non-owning pointer to the owning queue's applier stop flag.
  const std::atomic<bool> *m_applier_stop{nullptr};
  /// @brief Owns the on-disk spill file and its buffered write stream.
  std::unique_ptr<Spill_file_writer> m_writer;

  /// @brief Flag indicating if the reader is open/initialized.
  bool m_is_initialized{false};
  /// @brief Flag indicating if processing is done (finished or error).
  bool m_is_done{false};
  /// @brief Flag indicating if currently decompressing a TPLE.
  bool m_decompressing{false};
  /// @brief The reader's (consumer's) error message. Accessed only by the
  /// consumer currently reading this batch.
  std::string m_failure_msg{""};
  /// @brief The reader's (consumer's) status. Accessed only by the current
  /// consumer; reset() clears it.
  Return_status m_status;
  /// @brief The writer's failure reason. Written once by the receiver before
  /// m_writer_failed is set; reset() does not clear it.
  std::string m_writer_failure_msg{};
  /// @brief Whether the writer failed. Set once by the receiver (release),
  /// after m_writer_failure_msg; consumers read it (acquire).
  std::atomic<bool> m_writer_failed{false};
  /// @brief File offset of the transaction's first event (end of the FDE
  /// prefix); the reader opens here and reads the FDE at open.
  std::size_t m_start_file_pos{0};
  /// @brief Decompressing stream over the spill file's reader.
  Stream_ptr m_input_stream;
  /// @brief Relay-log file reader over the private spill file.
  Relaylog_file_reader m_reader;

  /// @brief Stream synchronization state (mirrors the relay-log sink).
  mutable std::mutex m_stream_mutex;
  std::condition_variable m_stream_cv;
  /// @brief Readable end position published to the consumer (byte offset).
  std::size_t m_published_end_file_pos{0};
  bool m_stream_open{false};
  bool m_stream_sealed{true};
  bool m_stream_truncated{false};

#ifndef NDEBUG
  /// @brief Debug only: see set_seal_hook_for_test().
  std::function<void()> m_seal_hook_for_test;
#endif
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_IN_MEMORY_EVENT_SET_FETCHABLE_SPILL_H
