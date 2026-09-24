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

#ifndef MYSQL_CSA_STORAGE_IN_MEMORY_TRANSACTION_ENVELOPE_H
#define MYSQL_CSA_STORAGE_IN_MEMORY_TRANSACTION_ENVELOPE_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>

#include "sql/changestreams/apply/storage/in_memory/in_memory_types.h"
#include "sql/changestreams/apply/storage/in_memory/trx_payload.h"

// create_memory_destination() threads the active format-description event
// through to the byte source only as a shared_ptr, so an incomplete
// (forward-declared) type is sufficient here.
class Format_description_log_event;

namespace mysql::csa {

class Streaming_event_sink;
class Trx_envelope_queue;

/// @brief Lightweight FIFO-queue entry tracking a single transaction's
/// lifecycle state and its heavy payload.
///
/// A Transaction_envelope lives from GTID receipt (when it is enqueued in
/// source order) until the coordinator sweeps it past the committed head. It is
/// deliberately lightweight. Keeping the payload separate lets memory be
/// reclaimed at commit (when the payload is reset) rather than at dequeue,
/// avoiding head-of-line memory blocking when workers commit out of source order.
class Transaction_envelope {
 public:
  /// @brief Construct an uncommitted envelope for a transaction of
  ///        @p trx_length.
  ///
  /// @param stream_seqno The monotonic enqueue-order position assigned by the
  ///        queue.
  /// @param trx_length The transaction's declared byte size from the GTID
  ///        event.
  /// @param path Whether the transaction is routed to the memory or spill path.
  Transaction_envelope(std::uint64_t stream_seqno, std::size_t trx_length,
                       Envelope_path path);

  Transaction_envelope(const Transaction_envelope &) = delete;
  Transaction_envelope &operator=(const Transaction_envelope &) = delete;
  Transaction_envelope(Transaction_envelope &&) = delete;
  Transaction_envelope &operator=(Transaction_envelope &&) = delete;

  /// @brief The monotonic enqueue-order position of this envelope.
  std::uint64_t stream_seqno() const { return m_stream_seqno; }

  /// @brief The transaction's declared byte size.
  std::size_t trx_length() const { return m_trx_length; }

  /// @brief Whether this envelope is on the memory or spill path.
  Envelope_path path() const { return m_path; }

  // --- Commit state (serialized under m_mutex) ---

  /// @brief Whether this envelope has been committed, read under @c m_mutex.
  bool is_committed() const;

  /// @brief Worker commit path: mark committed and release the payload.
  ///
  /// Invoked once from the worker's success hook. Under only the per-envelope
  /// @c m_mutex, marks the envelope committed and resets the payload.
  ///
  /// @retval false success: the envelope was marked committed and its payload
  ///         was released.
  /// @retval true  failure: the envelope was already committed; commit flag and
  ///         payload are unchanged.
  bool commit();

  /// @brief Whether this envelope was truncated (its transaction was received
  /// incompletely), read under @c m_mutex.
  bool is_truncated() const;

  /// @brief Receiver path: mark this envelope truncated.
  ///
  /// Set when the receiver could not finish this transaction (IO-thread stop
  /// mid-transaction, rotate, or a queue write failure).
  /// TODO: rotate should be no-op.
  void set_truncated();

  // --- Payload handoff (serialized under m_mutex) ---

  /// @brief Take ownership of the heavy payload for this envelope.
  ///
  /// Attached at admission for a memory-path envelope, keeping the payload
  /// non-null from admission until commit.
  ///
  /// @param payload The payload to store (ownership is transferred in).
  void attach_payload(std::unique_ptr<Trx_payload> payload);

  /// @brief Create and attach this envelope's empty MEMORY-path destination.
  ///
  /// Builds the payload via Trx_payload::create_memory() (using this envelope's
  /// @c m_trx_length) and attaches it.
  ///
  /// @param is_trx Whether the admitted unit is a real transaction.
  /// @param fde The active Format_description_log_event (shared ownership).
  /// @param owner_queue The queue charged for the payload's byte reservation.
  void create_memory_destination(
      bool is_trx, std::shared_ptr<Format_description_log_event> fde,
      Trx_envelope_queue *owner_queue);

  /// @brief Create and attach this envelope's empty SPILL-path destination.
  ///
  /// The spill-path counterpart of create_memory_destination(). Builds the
  /// zero-reservation payload via Trx_payload::create_spill() and attaches it.
  ///
  /// @param is_trx Whether the admitted unit is a real transaction.
  /// @param fde The active Format_description_log_event (shared ownership).
  /// @param owner_queue The queue hosting the spill file and charged for the
  ///        (zero) reservation.
  void create_spill_destination(
      bool is_trx, std::shared_ptr<Format_description_log_event> fde,
      Trx_envelope_queue *owner_queue);

  /// @brief Non-owning peek at the payload.
  /// @return The stored payload pointer, or nullptr once it has been reset.
  Trx_payload *payload();

  /// @brief Drop the payload, releasing its bytes and nulling the reference.
  void reset_payload();

  // --- Sink handle (delegates to the payload) ---

  /// @brief The transaction's memory-path sink, delegating to the payload.
  ///
  /// Returns the non-owning sink pointer held by the payload. Valid only while
  /// the payload is alive (admission→commit).
  ///
  /// @return The non-owning sink pointer, or nullptr when there is no payload.
  Streaming_event_sink *current_sink() const;
  
  private:
  /// TODO: remove m_stream_seqno, and merge state for commit/truncate
  mutable std::mutex m_mutex;    ///< Guards commit flag + payload reference.
  std::size_t m_trx_length;      ///< From the GTID event.
  std::uint64_t m_stream_seqno;  ///< Monotonic enqueue-order position.
  Envelope_path m_path;          ///< MEMORY | SPILL.
  bool m_committed{false};       ///< Commit terminal flag; set once at commit.
  bool m_truncated{false};       ///< Truncate terminal flag; set by the receiver.
  std::unique_ptr<Trx_payload> m_payload;  ///< Heavy bytes; reset at commit.
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_IN_MEMORY_TRANSACTION_ENVELOPE_H
