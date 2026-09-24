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
  // Reserve trx_length bytes against the channel memory budget.
  m_owner_queue->add_bytes(m_trx_length);
}

Trx_payload::~Trx_payload() {
  // Release the reserved bytes. This wakes the receiver waiting on the memory
  // limit.
  m_owner_queue->release_bytes(m_trx_length);
}

std::unique_ptr<Trx_payload> Trx_payload::create_memory(
    bool is_trx, std::shared_ptr<Format_description_log_event> fde,
    std::size_t trx_length, Trx_envelope_queue *owner_queue,
    Transaction_envelope *owner_envelope) {
  // Create the empty in-memory byte source.
  auto src = std::make_unique<Event_set_fetchable_memory>(
      is_trx, std::move(fde), owner_envelope, /*streaming_open=*/true);
  // Keep both pointers before the unique_ptr is moved.
  Event_set_fetchable_memory *mem_src = src.get();
  Streaming_event_sink *sink = mem_src;
  // Wrap the source as the transaction's single batch.
  auto trx = std::make_shared<Fetchable_transaction>();
  trx->append_batch(std::move(src));
  trx->set_fetching_complete();
  // Link the batch back to its transaction so truncating the stream
  // also marks the transaction truncated.
  mem_src->set_owning_fetchable(trx.get());
  // Construct the payload (reserves trx_length bytes) and record the sink.
  auto payload =
      std::make_unique<Trx_payload>(std::move(trx), trx_length, owner_queue);
  payload->m_sink = sink;
  return payload;
}

std::unique_ptr<Trx_payload> Trx_payload::create_spill(
    bool is_trx, std::shared_ptr<Format_description_log_event> fde,
    Trx_envelope_queue *owner_queue, Transaction_envelope *owner_envelope) {
  // Create the empty on-disk byte source.
  auto src = std::make_unique<Event_set_fetchable_spill>(
      is_trx, std::move(fde), owner_queue->relay_log_dir(), owner_envelope,
      /*streaming_open=*/true);
  Event_set_fetchable_spill *spill_src = src.get();
  Streaming_event_sink *sink = spill_src;
  auto trx = std::make_shared<Fetchable_transaction>();
  trx->append_batch(std::move(src));
  trx->set_fetching_complete();
  spill_src->set_owning_fetchable(trx.get());
  // Construct the payload reserving ZERO bytes.
  auto payload = std::make_unique<Trx_payload>(std::move(trx), /*trx_length=*/0,
                                               owner_queue);
  payload->m_sink = sink;
  return payload;
}

}  // namespace mysql::csa
