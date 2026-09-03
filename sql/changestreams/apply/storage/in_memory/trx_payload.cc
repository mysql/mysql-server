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

#include "sql/changestreams/apply/storage/in_memory/trx_payload.h"

#include <utility>

#include "sql/changestreams/apply/jobs/fetchable_transaction.h"
#include "sql/changestreams/apply/storage/in_memory/event_set_fetchable_memory.h"
#include "sql/changestreams/apply/storage/in_memory/event_set_fetchable_spill.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"

namespace mysql::csa {

Trx_payload::Trx_payload(std::shared_ptr<Fetchable_transaction> trx,
                         std::size_t trx_length,
                         Trx_envelope_queue *owner_queue)
    : m_trx(std::move(trx)),
      m_trx_length(trx_length),
      m_owner_queue(owner_queue) {
  // The reservation was taken at admission; nothing to reserve here.
}

Trx_payload::~Trx_payload() {
  // Release the reservation taken at admission (0 bytes for spill).
  m_owner_queue->release_bytes(m_trx_length);
}

std::unique_ptr<Trx_payload> Trx_payload::create_memory(
    bool is_trx, std::shared_ptr<Format_description_log_event> fde,
    std::size_t trx_length, Trx_envelope_queue *owner_queue,
    Transaction_envelope *owner_envelope) {
  // On a receiver stop nothing is reserved: return null so enqueue() aborts.
  if (owner_queue->acquire_admission(trx_length, Envelope_path::MEMORY)) {
    return nullptr;
  }
  auto src = std::make_unique<Event_set_fetchable_memory>(
      is_trx, fde, owner_envelope, /*streaming_open=*/true,
      owner_queue->channel_instance_id(), owner_queue->applier_stop_flag());
  // Keep both pointers before the unique_ptr is moved.
  Event_set_fetchable_memory *mem_src = src.get();
  Streaming_event_sink *sink = mem_src;
  // Wrap the source as the transaction's single batch.
  auto trx = std::make_shared<Fetchable_transaction>();
  trx->append_batch(std::move(src));
  // Not marked complete here: completion is set at seal time, so a truncated
  // transaction rolls back (see
  // Event_set_fetchable_memory::append_reader_event()).
  // Link the batch back to its transaction so truncating the stream
  // also marks the transaction truncated.
  mem_src->set_owning_fetchable(trx.get());
  auto payload =
      std::make_unique<Trx_payload>(std::move(trx), trx_length, owner_queue);
  payload->m_sink = sink;
  payload->m_fde = std::move(fde);
  return payload;
}

std::unique_ptr<Trx_payload> Trx_payload::create_spill(
    bool is_trx, std::shared_ptr<Format_description_log_event> fde,
    std::size_t trx_length, Trx_envelope_queue *owner_queue,
    Transaction_envelope *owner_envelope) {
  // Reserves spill disk space, not memory. On stop, return null as above.
  if (owner_queue->acquire_admission(trx_length, Envelope_path::SPILL)) {
    return nullptr;
  }
  // owner_queue lets the writer keep the spill gauges current. The writer owns
  // the disk reservation and releases it after deleting the file.
  auto src = std::make_unique<Event_set_fetchable_spill>(
      is_trx, fde, owner_queue->relay_log_dir(), owner_envelope,
      /*streaming_open=*/true, owner_queue->channel_name(), owner_queue,
      /*reserved_disk_bytes=*/trx_length, owner_queue->channel_instance_id(),
      owner_queue->applier_stop_flag());
  Event_set_fetchable_spill *spill_src = src.get();
  Streaming_event_sink *sink = spill_src;
  auto trx = std::make_shared<Fetchable_transaction>();
  trx->append_batch(std::move(src));
  // See create_memory(): completeness is signalled at the normal seal, not
  // here.
  spill_src->set_owning_fetchable(trx.get());
  // A spill payload holds no memory reservation.
  auto payload = std::make_unique<Trx_payload>(std::move(trx), /*trx_length=*/0,
                                               owner_queue);
  payload->m_sink = sink;
  payload->m_fde = std::move(fde);
  return payload;
}

}  // namespace mysql::csa
