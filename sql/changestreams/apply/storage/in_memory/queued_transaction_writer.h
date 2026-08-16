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

#ifndef MYSQL_CSA_STORAGE_IN_MEMORY_QUEUED_TRANSACTION_WRITER_H
#define MYSQL_CSA_STORAGE_IN_MEMORY_QUEUED_TRANSACTION_WRITER_H

// =============================================================================
// Receiver-side "writer" mechanism for the in-memory relay log — the producer
// counterpart to Queued_transaction_reader.
//
// Provides the streaming operations the receiver drives to push a transaction's
// events into the per-channel Trx_envelope_queue: open_transaction() (at the
// GTID event), append_transaction_event() (per body/terminal event), and
// truncate_transaction() (on an incomplete group).
//
// The thin Master_info adapters that map these operations onto the running
// receiver live in sql/rpl_replica.cc.
// =============================================================================

#include <cstddef>
#include <memory>

class Format_description_log_event;

namespace mysql::csa {

class Trx_envelope_queue;
class Streaming_event_sink;

/// @brief Open a transaction group at the GTID event (minimal-state variant of
/// the receiver's on-GTID hook).
///
/// TODO: refactor to avoid passing current_sink arg as output pointer.
///
/// @param[in,out] queue        The per-channel envelope queue to admit into.
/// @param[out]    current_sink Set to the opened group's sink on success, or to
///                             nullptr when @c enqueue() reports a stop.
/// @param[in]     fde          The active Format_description_log_event (shared
///                             ownership); must be non-null.
/// @param[in]     trx_length   The transaction's declared byte size, as read
///                             from the GTID event.
/// @param[in]     is_trx       Whether the admitted unit is a real transaction.
/// @retval false success — the group opened; @p current_sink is non-null.
/// @retval true  error — a stop was requested while blocked in admission; @p
///               current_sink is left nullptr (queuing-error indication).
bool open_transaction(Trx_envelope_queue &queue,
                      Streaming_event_sink *&current_sink,
                      std::shared_ptr<Format_description_log_event> fde,
                      std::size_t trx_length, bool is_trx = true);

/// @brief Append one received event's bytes to the open group (minimal-state
/// variant of the receiver's on-body hook).
///
/// The raw encoded bytes are forwarded straight to @p current_sink. When
/// @p is_terminal, the append also seals the stream.
///
/// @param[in,out] current_sink The open group's sink; cleared to nullptr after
///                             a terminal append. A null sink is a defensive
///                             no-op returning true (error).
/// @param[in]     buf          The transient event bytes (copied in by the
/// sink).
/// @param[in]     len          Number of bytes at @p buf.
/// @param[in]     is_terminal  Whether this event terminates (seals) the group.
/// @retval false success — the event was appended.
/// @retval true  error — @p current_sink was null (nothing appended).
bool append_transaction_event(Streaming_event_sink *&current_sink,
                              const char *buf, std::size_t len,
                              bool is_terminal);

/// @brief Truncate the open group (minimal-state variant of the receiver's
/// on-truncate hook).
///
/// @param[in,out] current_sink The open group's sink, cleared on truncation.
void truncate_transaction(Streaming_event_sink *&current_sink);

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_IN_MEMORY_QUEUED_TRANSACTION_WRITER_H
