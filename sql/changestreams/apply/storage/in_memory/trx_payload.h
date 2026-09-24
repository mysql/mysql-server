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

#include "sql/changestreams/apply/storage/in_memory/in_memory_types.h"

// The static create_memory() factory takes the active format-description event
// only as a shared_ptr parameter, so an incomplete (forward-declared) type is
// sufficient here.
class Format_description_log_event;

namespace mysql::csa {

// Forward declarations
class Fetchable_transaction;
class Trx_envelope_queue;
class Streaming_event_sink;
class Transaction_envelope;

/// @brief Heavy, RAII memory-accounted holder of one transaction's byte stream.
///
/// A Trx_payload reserves exactly @c trx_length bytes against the owning
/// Trx_envelope_queue's memory-usage counter on construction and releases them
/// on destruction, waking any receiver blocked at the per-channel memory limit.
class Trx_payload {
 public:
  /// @brief Construct the payload, reserving @p trx_length bytes against
  ///        @p owner_queue's memory-usage counter.
  ///
  /// @param trx The wrapped Fetchable_transaction holding this transaction's
  ///        event byte stream; non-null.
  /// @param trx_length The transaction's declared byte size (bytes reserved).
  /// @param owner_queue The queue charged for the reservation; non-null and
  ///        must outlive this payload (held non-owning).
  Trx_payload(std::shared_ptr<Fetchable_transaction> trx,
              std::size_t trx_length, Trx_envelope_queue *owner_queue);

  /// @brief Create a memory-path payload holding one transaction's events
  ///        in memory.
  ///
  /// Sets up an empty in-memory byte source for the transaction, reserves
  /// @p trx_length bytes against @p owner_queue, and wires up the payload's
  /// sink handle so events can be written into it.
  ///
  /// @param is_trx Whether the admitted unit is a real transaction.
  /// @param fde The active Format_description_log_event (shared ownership).
  /// @param trx_length The transaction's declared byte size (bytes reserved).
  /// @param owner_queue The queue charged for the reservation; non-null.
  /// @param owner_envelope The envelope the byte source commits on success.
  /// @return The newly created payload with its sink handle populated.
  static std::unique_ptr<Trx_payload> create_memory(
      bool is_trx, std::shared_ptr<Format_description_log_event> fde,
      std::size_t trx_length, Trx_envelope_queue *owner_queue,
      Transaction_envelope *owner_envelope);

  /// @brief Create a spill-path payload that stores one large transaction's
  ///        events on disk.
  ///
  /// The on-disk counterpart of create_memory(). Sets up an empty spill file
  /// under @p owner_queue's relay log directory, wires up the payload's sink
  /// handle, and reserves zero bytes against the memory counter, since spilled
  /// data lives on disk rather than in the memory budget. The resulting
  /// payload's byte_size() is 0.
  ///
  /// @param is_trx Whether the admitted unit is a real transaction.
  /// @param fde The active Format_description_log_event (shared ownership);
  ///        also written into the spill file prefix.
  /// @param owner_queue The queue whose relay log directory hosts the spill
  ///        file; charged for the (zero) reservation. Non-null.
  /// @param owner_envelope The envelope the byte source commits on success.
  /// @return The newly created zero-reservation payload with its sink handle
  ///         populated.
  static std::unique_ptr<Trx_payload> create_spill(
      bool is_trx, std::shared_ptr<Format_description_log_event> fde,
      Trx_envelope_queue *owner_queue, Transaction_envelope *owner_envelope);

  /// @brief Release the reserved bytes and wake any blocked receiver.
  ///
  /// Returns the reserved bytes to @p owner_queue, which wakes one receiver
  /// waiting on the memory limit.
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
  /// Returns a non-owning pointer to the transaction's byte source, which
  /// accepts streamed events. It is set by create_memory() and stays valid for
  /// the payload's lifetime. Returns nullptr until then.
  ///
  /// @return The non-owning sink pointer, or nullptr if not yet set.
  Streaming_event_sink *sink() const { return m_sink; }

 private:
  std::shared_ptr<Fetchable_transaction> m_trx;  ///< Consumed by workers.
  std::size_t m_trx_length;                      ///< Bytes reserved.
  /// Non-owning pointer to the queue charged for the reservation. Set at construction.
  Trx_envelope_queue *m_owner_queue;
  /// Non-owning pointer to the transaction's byte source, which accepts
  /// streamed events.
  Streaming_event_sink *m_sink{nullptr};
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_IN_MEMORY_TRX_PAYLOAD_H
