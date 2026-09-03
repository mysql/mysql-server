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

#ifndef MYSQL_CSA_STORAGE_IN_MEMORY_TRX_PAYLOAD_H
#define MYSQL_CSA_STORAGE_IN_MEMORY_TRX_PAYLOAD_H

#include <cstddef>
#include <memory>

#include "sql/changestreams/apply/storage/in_memory/imr_types.h"

// Used only through shared_ptr here, so a forward declaration suffices.
class Format_description_log_event;

namespace mysql::csa {

// Forward declarations
class Fetchable_transaction;
class Trx_envelope_queue;
class Streaming_event_sink;
class Transaction_envelope;

/// @brief RAII holder of one transaction's byte stream and its memory
/// reservation.
///
/// A memory-path payload owns @c trx_length bytes of the queue's memory
/// budget, reserved by create_memory() and released by the destructor. A
/// spill-path payload holds 0 bytes.
class Trx_payload {
 public:
  /// @brief Takes no reservation; the factory already did.
  ///
  /// @param trx The wrapped Fetchable_transaction holding this transaction's
  ///        event byte stream; non-null.
  /// @param trx_length The transaction's declared byte size (bytes the
  ///        destructor releases; 0 for a spill payload).
  /// @param owner_queue The queue charged for the reservation; non-null and
  ///        must outlive this payload (held non-owning).
  Trx_payload(std::shared_ptr<Fetchable_transaction> trx,
              std::size_t trx_length, Trx_envelope_queue *owner_queue);

  /// @brief Create a memory-path payload holding one transaction's events
  ///        in memory.
  ///
  /// Admits the transaction (see Trx_envelope_queue::acquire_admission()),
  /// then creates an empty in-memory byte source as the payload's sink.
  ///
  /// @param is_trx Whether the admitted unit is a real transaction.
  /// @param fde The active Format_description_log_event (shared ownership).
  /// @param trx_length The transaction's declared byte size (bytes reserved).
  /// @param owner_queue The queue charged for the reservation; non-null.
  /// @param owner_envelope The envelope the byte source commits on success.
  /// @return The newly created payload with its sink handle populated, or
  ///         nullptr if a receiver stop was requested during admission (nothing
  ///         is reserved and no payload is built).
  static std::unique_ptr<Trx_payload> create_memory(
      bool is_trx, std::shared_ptr<Format_description_log_event> fde,
      std::size_t trx_length, Trx_envelope_queue *owner_queue,
      Transaction_envelope *owner_envelope);

  /// @brief Create a spill-path payload that stores one large transaction's
  ///        events on disk.
  ///
  /// Reserves spill disk space instead of memory (see
  /// Trx_envelope_queue::acquire_admission()) and hands it to the spill file
  /// writer. The payload's sink is a new spill file; byte_size() is 0.
  ///
  /// @param is_trx Whether the admitted unit is a real transaction.
  /// @param fde The active Format_description_log_event (shared ownership);
  ///        also written into the spill file prefix.
  /// @param trx_length The transaction's declared byte size, reserved against
  ///        the spill disk limit.
  /// @param owner_queue The queue whose relay log directory hosts the spill
  ///        file and whose spill disk entry is reserved against. Non-null.
  /// @param owner_envelope The envelope the byte source commits on success.
  /// @return The newly created zero-reservation payload with its sink handle
  ///         populated, or nullptr if a receiver stop was requested during
  ///         admission.
  static std::unique_ptr<Trx_payload> create_spill(
      bool is_trx, std::shared_ptr<Format_description_log_event> fde,
      std::size_t trx_length, Trx_envelope_queue *owner_queue,
      Transaction_envelope *owner_envelope);

  /// @brief Returns the reserved bytes to the owning queue (none for spill).
  ~Trx_payload();

  // Pinned in the envelope: neither copyable nor movable.
  Trx_payload(const Trx_payload &) = delete;
  Trx_payload &operator=(const Trx_payload &) = delete;
  Trx_payload(Trx_payload &&) = delete;
  Trx_payload &operator=(Trx_payload &&) = delete;

  /// @brief Share ownership of the wrapped Fetchable_transaction.
  /// @return A non-null shared_ptr while the payload owns its bytes.
  std::shared_ptr<Fetchable_transaction> fetchable() const { return m_trx; }

  /// @brief The number of bytes reserved at construction.
  std::size_t byte_size() const { return m_trx_length; }

  /// @brief The sink that events for this transaction are written into.
  ///
  /// Set by the factories and valid for the payload's lifetime.
  ///
  /// @return The non-owning sink pointer, or nullptr for a payload built by
  ///         the constructor alone.
  Streaming_event_sink *sink() const { return m_sink; }

  /// @brief Read-only FDE snapshot (see Imr_runtime::m_fde_snapshot).
  const std::shared_ptr<Format_description_log_event> &fde() const {
    return m_fde;
  }

 private:
  std::shared_ptr<Fetchable_transaction> m_trx;  ///< Consumed by workers.
  std::size_t m_trx_length;                      ///< Bytes reserved.
  /// Non-owning pointer to the queue charged for the reservation. Set at
  /// construction.
  Trx_envelope_queue *m_owner_queue;
  /// Non-owning pointer to the transaction's byte source, which accepts
  /// streamed events.
  Streaming_event_sink *m_sink{nullptr};
  /// The FDE snapshot passed to the factory; set by create_memory() /
  /// create_spill(). Read-only: workers decode with it without a lock.
  std::shared_ptr<Format_description_log_event> m_fde;
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_IN_MEMORY_TRX_PAYLOAD_H
