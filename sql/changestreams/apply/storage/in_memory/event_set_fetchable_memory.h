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

#ifndef MYSQL_CSA_STORAGE_IN_MEMORY_EVENT_SET_FETCHABLE_MEMORY_H
#define MYSQL_CSA_STORAGE_IN_MEMORY_EVENT_SET_FETCHABLE_MEMORY_H

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "mysql/utils/return_status.h"
#include "sql/binlog/decompressing_event_object_istream.h"  // Decompressing_event_object_istream
#include "sql/changestreams/apply/storage/common/event_set_fetchable.h"
#include "sql/changestreams/apply/storage/common/streaming_event_sink.h"
#include "sql/changestreams/apply/storage/relay_log/ireader_event.h"

namespace mysql::csa {

class Transaction_envelope;
class Fetchable_transaction;

/// @brief In-memory transaction byte source with a commit hook.
///
/// Memory-path counterpart of Event_set_fetchable_cache: holds a transaction's
/// encoded events in RAM and serves them through Event_set_fetchable.
class Event_set_fetchable_memory : public Event_set_fetchable,
                                   public Streaming_event_sink {
 public:
  /// @brief Shared pointer to a Log_event.
  using Log_event_ptr = std::shared_ptr<Log_event>;
  /// @brief Type alias for return status.
  using Return_status = mysql::utils::Return_status;
  /// @brief Vector of events representing a set of events.
  using Event_set_type = std::vector<IReader_event_ptr>;
  /// @brief Type alias for the decompressing event stream.
  using Stream_type = ::binlog::Decompressing_event_object_istream;
  /// @brief Unique pointer to the decompressing stream.
  using Stream_ptr = std::unique_ptr<Stream_type>;

  /// @brief Constructs an EMPTY in-memory byte source to be streamed into.
  ///
  /// @param is_trx Whether this set represents a transaction.
  /// @param fde Shared pointer to the Format_description_event.
  /// @param owner_envelope Envelope committed on success; nullptr in tests.
  /// @param streaming_open If true, appends are accepted until seal/truncate.
  /// @param stats_instance_id Channel whose CSA statistics count the commit.
  /// @param applier_stop Polled by wait_next(); nullptr waits untimed.
  Event_set_fetchable_memory(bool is_trx, Log_event_ptr fde,
                             Transaction_envelope *owner_envelope,
                             bool streaming_open = true,
                             std::size_t stats_instance_id = 0,
                             const std::atomic<bool> *applier_stop = nullptr);

  bool wait_next() override;
  std::optional<Managed_event> fetch_next() override;
  const std::string &get_error_str() const override;
  bool is_done() const override;
  bool is_error() const override;
  bool is_trx() const override;
  void reset(bool reset_events) override;
  Fde_ptr get_fde() override;

  virtual ~Event_set_fetchable_memory() override;

  /// @brief Callback notifying that the transaction was applied successfully.
  ///
  /// Commits the owning envelope under only the per-envelope mutex.
  void set_success() override;

  /// @brief Byte-oriented sink append.
  ///
  /// Wraps a COPY of the transient encoded bytes in a Cached_event_memory.
  /// @return Always false: an in-memory append has no IO write that can fail.
  bool append_event(const char *buf, std::size_t len,
                    bool seal_after = false) override;

  /// @brief Internal event-oriented append seam.
  ///
  /// Pushes a pre-built IReader_event into the batch and does the seal/notify.
  ///
  /// @param event The pre-built encoded event to publish.
  /// @param seal_after When true, seal the stream together with this append.
  /// @param event_length The event's on-wire length, for the event memory
  ///        limit (0 leaves the owner's maximum event length unchanged).
  void append_reader_event(IReader_event_ptr event, bool seal_after = false,
                           std::size_t event_length = 0);

  void seal_stream() override;
  void set_stream_truncated() override;

  /// @brief Record the Fetchable_transaction that owns this batch.
  ///
  /// A memory-path Fetchable_transaction owns exactly one such batch.
  ///
  /// @param owning_fetchable The owning transaction, or nullptr (tests).
  void set_owning_fetchable(Fetchable_transaction *owning_fetchable) {
    m_fetchable_trx = owning_fetchable;
  }

#ifndef NDEBUG
  /// @brief Debug only, for unit tests: @p hook runs on the receiver thread on
  /// each seal of a stream that is not truncated, after m_stream_mutex is
  /// released and before consumers are notified.
  void set_seal_hook_for_test(std::function<void()> hook) {
    m_seal_hook_for_test = std::move(hook);
  }
#endif

 private:
  /// @brief Cached vector of events (filled by the receiver via append_event).
  Event_set_type m_events;

  /// @brief Decompresses and returns the next event from the TPLE stream.
  /// @return Optional Log_event_ptr if successful, empty if failed or ended.
  std::optional<Log_event_ptr> decompress();
  /// @brief Helper to deinitialize the decompression stream and update status.
  void end_decompression();
  /// @brief Helper to initialize the decompression stream and update status.
  void start_decompression();

  /// @brief Flag indicating if processing is done (finished or error).
  bool m_is_done = false;
  /// @brief Index of the next event to fetch.
  std::size_t m_event_id{0};
  /// @brief Detailed error message if any.
  std::string m_failure_msg{""};
  /// @brief Status of the object.
  Return_status m_status;
  /// @brief Flag indicating if this is a transaction.
  bool m_is_trx{false};
  /// @brief Flag indicating if currently decompressing a TPLE.
  bool m_decompressing{false};
  /// @brief Decompressing stream created from TPLE if any.
  Stream_ptr m_decompressing_stream{};
  /// @brief Non-owning pointer to compressed event casted to
  /// Transaction_payload_log_event.
  Transaction_payload_log_event *m_compressed_event_ptr{nullptr};
  /// @brief Compressed event used during decompression.
  Log_event_ptr m_compressed_event{};
  /// @brief Owning pointer to Format_description_event.
  Log_event_ptr m_fde_base{};
  /// @brief Non-owning pointer to Format_description_event.
  Fde_ptr m_fde{};
  /// @brief Non-owning pointer to the owning envelope.
  Transaction_envelope *m_envelope{nullptr};
  /// @brief Channel instance id whose CSA statistics set_success() updates.
  std::size_t m_stats_instance_id{0};

  /// @brief Non-owning pointer to the Fetchable_transaction that owns this
  /// batch (set by Trx_payload::create_memory()).
  Fetchable_transaction *m_fetchable_trx{nullptr};

  /// @brief Non-owning pointer to the owning queue's applier stop flag.
  const std::atomic<bool> *m_applier_stop{nullptr};

  /// @brief Stream synchronization state.
  mutable std::mutex m_stream_mutex;
  std::condition_variable m_stream_cv;
  bool m_stream_open{false};
  bool m_stream_sealed{true};
  bool m_stream_truncated{false};

#ifndef NDEBUG
  /// @brief Debug only: see set_seal_hook_for_test().
  std::function<void()> m_seal_hook_for_test;
#endif
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_IN_MEMORY_EVENT_SET_FETCHABLE_MEMORY_H
