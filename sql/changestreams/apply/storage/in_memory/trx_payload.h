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

/// @brief The envelope's reference to one transaction's byte stream.
///
/// Holds the envelope's share of the Fetchable_transaction, the receiver's
/// sink into its byte source, and the FDE snapshot. It holds no reservation:
/// the factories hand the admission reservation to the byte source
/// (Event_set_fetchable_memory for memory, its Spill_file_writer for disk),
/// which returns it when the last owner of the Fetchable_transaction (this
/// payload or a job) drops it.
class Trx_payload {
 public:
  /// @param trx The wrapped Fetchable_transaction holding this transaction's
  ///        event byte stream; non-null.
  /// @param sink The byte source of @p trx as a sink; nullptr in tests that
  ///        never stream.
  /// @param fde The FDE snapshot the transaction was opened with.
  Trx_payload(std::shared_ptr<Fetchable_transaction> trx,
              Streaming_event_sink *sink,
              std::shared_ptr<Format_description_log_event> fde);

  /// @brief Create a memory-path payload holding one transaction's events
  ///        in memory.
  ///
  /// Admits the transaction (see Trx_envelope_queue::acquire_admission()),
  /// then creates an empty in-memory byte source as the payload's sink. The
  /// byte source owns the reservation.
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
  /// writer. The payload's sink is a new spill file.
  ///
  /// @param is_trx Whether the admitted unit is a real transaction.
  /// @param fde The active Format_description_log_event (shared ownership);
  ///        also written into the spill file prefix.
  /// @param trx_length The transaction's declared byte size, reserved against
  ///        the spill disk limit.
  /// @param owner_queue The queue whose relay log directory hosts the spill
  ///        file and whose spill disk entry is reserved against. Non-null.
  /// @param owner_envelope The envelope the byte source commits on success.
  /// @return The newly created payload with its sink handle populated, or
  ///         nullptr if a receiver stop was requested during admission.
  static std::unique_ptr<Trx_payload> create_spill(
      bool is_trx, std::shared_ptr<Format_description_log_event> fde,
      std::size_t trx_length, Trx_envelope_queue *owner_queue,
      Transaction_envelope *owner_envelope);

  // Pinned in the envelope: neither copyable nor movable.
  Trx_payload(const Trx_payload &) = delete;
  Trx_payload &operator=(const Trx_payload &) = delete;
  Trx_payload(Trx_payload &&) = delete;
  Trx_payload &operator=(Trx_payload &&) = delete;

  /// @brief Share ownership of the wrapped Fetchable_transaction.
  /// @return A non-null shared_ptr while the payload owns its bytes.
  std::shared_ptr<Fetchable_transaction> fetchable() const { return m_trx; }

  /// @brief The sink that events for this transaction are written into.
  ///
  /// Set at construction and valid for the payload's lifetime.
  ///
  /// @return The non-owning sink pointer, or nullptr for a payload built
  ///         without one (tests).
  Streaming_event_sink *sink() const { return m_sink; }

  /// @brief Read-only FDE snapshot (see Imr_runtime::m_fde_snapshot).
  const std::shared_ptr<Format_description_log_event> &fde() const {
    return m_fde;
  }

 private:
  std::shared_ptr<Fetchable_transaction> m_trx;  ///< Consumed by workers.
  /// Non-owning pointer to the transaction's byte source, which accepts
  /// streamed events.
  Streaming_event_sink *m_sink;
  /// The FDE snapshot the transaction was opened with. Read-only: workers
  /// decode with it without a lock.
  std::shared_ptr<Format_description_log_event> m_fde;
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_IN_MEMORY_TRX_PAYLOAD_H
