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

// Receiver-side "writer" mechanism for the in-memory relay log — the producer
// counterpart to Queued_transaction_reader. See queued_transaction_writer.h
// for the full description. The thin Master_info adapters live in
// sql/rpl_replica.cc.

#include "sql/changestreams/apply/storage/in_memory/queued_transaction_writer.h"

#include <utility>

#include "sql/changestreams/apply/storage/common/streaming_event_sink.h"
#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"

namespace mysql::csa {

bool open_transaction(Trx_envelope_queue &queue,
                      Streaming_event_sink *&current_sink,
                      std::shared_ptr<Format_description_log_event> fde,
                      std::size_t trx_length, bool is_trx) {
  // enqueue() creates a reachable sink immediately on return.
  // A nullptr envelope means a stop was requested while blocked in admission.
  Transaction_envelope *env = queue.enqueue(trx_length, is_trx, std::move(fde));
  if (env == nullptr) {
    current_sink = nullptr;
    return true;
  }
  current_sink = env->current_sink();
  return false;
}

bool append_transaction_event(Streaming_event_sink *&current_sink,
                              const char *buf, std::size_t len,
                              bool is_terminal) {
  // Defensive: no group is open, so there is nothing to append into.
  if (current_sink == nullptr) return true;

  // Forward the raw encoded bytes straight to the sink.
  // On the terminal event, the append also seals the stream.
  current_sink->append_event(buf, len, /*seal_after=*/is_terminal);
  if (is_terminal) current_sink = nullptr;
  return false;
}

void truncate_transaction(Streaming_event_sink *&current_sink) {
  if (current_sink == nullptr) return;
  current_sink->set_stream_truncated();
  current_sink = nullptr;
}

}  // namespace mysql::csa
