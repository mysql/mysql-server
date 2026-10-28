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

#include "sql/changestreams/apply/storage/in_memory/imr_types.h"
#include "sql/changestreams/apply/storage/in_memory/trx_payload.h"

// Only passed by shared_ptr, so a forward declaration is enough.
class Format_description_log_event;

namespace mysql::csa {

class Fetchable_transaction;
class Streaming_event_sink;
class Trx_envelope_queue;

/// @brief Lightweight FIFO-queue entry tracking a single transaction's
/// lifecycle state and its heavy payload.
///
/// Lives from the GTID event until the coordinator sweeps it. The payload is
/// kept separate so its memory is reclaimed at commit, not at dequeue, which
/// avoids head-of-line blocking when workers commit out of order.
///
/// While the receiver still writes the transaction it holds the envelope. A
/// held envelope is never reclaimed and its payload never reset, so a commit
/// that comes first (a worker skipping an executed transaction mid-stream)
/// leaves the payload to release_receiver().
class Transaction_envelope {
 public:
  /// @brief Construct an uncommitted envelope for a transaction of
  ///        @p trx_length.
  ///
  /// @param trx_length The transaction's declared byte size from the GTID
  ///        event.
  /// @param path Whether the transaction is routed to the memory or spill path.
  Transaction_envelope(std::size_t trx_length, Envelope_path path);

  Transaction_envelope(const Transaction_envelope &) = delete;
  Transaction_envelope &operator=(const Transaction_envelope &) = delete;
  Transaction_envelope(Transaction_envelope &&) = delete;
  Transaction_envelope &operator=(Transaction_envelope &&) = delete;

  /// @brief The transaction's declared byte size.
  std::size_t trx_length() const { return m_trx_length; }

  /// @brief Whether this envelope is on the memory or spill path.
  Envelope_path path() const { return m_path; }

  // --- Commit state (serialized under m_mutex) ---

  /// @brief The envelope's state, read under @c m_mutex.
  Envelope_state state() const;

  /// @brief Whether this envelope has been committed, read under @c m_mutex.
  bool is_committed() const;

  /// @brief Worker commit path: mark committed and release the payload.
  ///
  /// Called from the worker's success hook under @c m_mutex only. A payload
  /// still held by the receiver is released by release_receiver() instead.
  ///
  /// @retval false marked committed.
  /// @retval true  no-op, not counted: already committed, or truncated first
  ///         (a skipped transaction the receiver truncated before this hook).
  bool commit();

  /// @brief Whether this envelope was truncated (its transaction was received
  /// incompletely), read under @c m_mutex.
  bool is_truncated() const;

  /// @brief Receiver path: mark this envelope truncated.
  ///
  /// Set when the receiver cannot finish the transaction: a Rotate, a new GTID
  /// or an IO-thread stop while it is open, a write failure, or a failed spill
  /// file setup. A no-op once committed (a mid-stream skip executed it).
  void set_truncated();

  // --- Receiver hold (serialized under m_mutex) ---

  /// @brief Mark the envelope as held by the receiver, which still writes its
  /// payload. Called only by Trx_envelope_queue::enqueue(), before publish.
  void hold_for_receiver();

  /// @brief Release the receiver's hold, and the payload if already committed.
  ///
  /// This is the caller's last access to the envelope, payload and sink: once
  /// it returns, a sweep may free them. Idempotent; a no-op if never held.
  void release_receiver();

  /// @brief Whether the sweep may free this envelope: not held by the
  /// receiver, and committed or truncated. One @c m_mutex acquisition.
  bool is_reclaimable() const;

  /// @brief Whether the receiver holds this envelope, read under @c m_mutex.
  /// For asserts and tests.
  bool is_receiver_held() const;

  // --- Payload handoff (serialized under m_mutex) ---

  /// @brief Admit this envelope's transaction and build its empty destination,
  ///        storing it under @c m_mutex.
  ///
  /// Uses Trx_payload::create_memory() or create_spill() by @c m_path; the
  /// factory performs admission (see Trx_envelope_queue::acquire_admission()).
  ///
  /// @param is_trx Whether the admitted unit is a real transaction.
  /// @param fde The active Format_description_log_event (shared ownership).
  /// @param owner_queue The queue that owns the payload: charged for the byte
  ///        reservation (MEMORY) and hosting the spill file (SPILL).
  /// @retval false success: the payload was admitted and stored.
  /// @retval true  a receiver stop was requested during admission; no payload
  ///         was built and nothing was reserved.
  [[nodiscard]] bool create_payload(
      bool is_trx, std::shared_ptr<Format_description_log_event> fde,
      Trx_envelope_queue *owner_queue);

  /// @brief Truncate the envelope if its destination failed to provision.
  ///
  /// Called after create_payload() and before publish. Only a spill file can
  /// fail here. A truncated envelope is skipped by dispatch and reclaimed by
  /// the sweep; the receiver still reports the failure and stops the IO thread.
  void validate_payload();

  /// @brief Non-owning peek at the payload.
  /// @return The stored payload pointer, or nullptr once it has been reset.
  Trx_payload *payload();

  // --- Sink handle (delegates to the payload) ---

  /// @brief The payload's sink (memory or spill), valid while the payload is
  /// alive.
  ///
  /// @return The non-owning sink pointer, or nullptr when there is no payload.
  Streaming_event_sink *current_sink() const;

 private:
  mutable std::mutex m_mutex;  ///< Guards state, hold flag + payload reference.
  const std::size_t m_trx_length;  ///< From the GTID event.
  const Envelope_path m_path;      ///< MEMORY | SPILL.
  /// ACTIVE until commit() or set_truncated(); the first final state wins.
  Envelope_state m_state{Envelope_state::ACTIVE};
  bool m_receiver_held{false};  ///< The receiver still writes the payload.
  /// Heavy bytes; reset at commit, or at release_receiver() if the commit came
  /// while the receiver held the envelope.
  std::unique_ptr<Trx_payload> m_payload;
};

/// Worker commit hook shared by the memory and spill byte sources.
void commit_envelope_on_success(Transaction_envelope *envelope,
                                const Fetchable_transaction *trx,
                                std::size_t stats_instance_id, bool spill_path);

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_IN_MEMORY_TRANSACTION_ENVELOPE_H
