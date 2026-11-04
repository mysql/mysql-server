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
// Receiver-side writer for the in-memory relay log, the producer counterpart of
// Queued_transaction_reader. Imr_runtime's receiver write path drives these
// operations with its Open_group: open_transaction() at the GTID event,
// append_transaction_event() per event, truncate_transaction() on an
// incomplete group.
// =============================================================================

#include <cstddef>
#include <memory>
#include <string>

class Format_description_log_event;

namespace mysql::csa {

class Trx_envelope_queue;
class Transaction_envelope;
class Streaming_event_sink;

/// The receiver's open transaction group. Both fields are null outside a
/// group. Owned by the receiver; the envelope is held (hold_for_receiver())
/// from open_transaction() until close, which releases it last.
struct Open_group {
  Transaction_envelope *envelope{nullptr};
  Streaming_event_sink *sink{nullptr};
};

/// Why the last receiver write call failed; filled only on failure.
struct Receiver_write_failure {
  bool stopped{false};  ///< a receiver stop was requested in admission
  std::string reason;   ///< non-empty for a real write failure
};

/// @brief Open a transaction group at the GTID event.
///
/// A group still open in @p group is truncated and closed first.
///
/// @param[in,out] queue      The per-channel envelope queue to admit into.
/// @param[in,out] group      The receiver's open group. On success, both fields
///                           are set and the envelope stays held until the
///                           group is closed; otherwise both fields are null.
/// @param[in]     fde        The active Format_description_log_event (shared
///                           ownership); must be non-null.
/// @param[in]     trx_length The transaction's declared byte size, as read
///                           from the GTID event.
/// @param[in]     is_trx     Whether the admitted unit is a real transaction.
/// @param[out]    failure    Filled on failure; may be null.
/// @retval false success — the group opened and the envelope is held.
/// @retval true  error — a stop during admission, or the spill file failed to
///               provision. Both fields of @p group are null; on a
///               provisioning failure the envelope is already truncated and
///               released, and the reason is recorded in @p failure.
bool open_transaction(Trx_envelope_queue &queue, Open_group &group,
                      std::shared_ptr<Format_description_log_event> fde,
                      std::size_t trx_length, bool is_trx = true,
                      Receiver_write_failure *failure = nullptr);

/// @brief Append one received event's bytes to the open group; a terminal
/// event also seals the stream.
///
/// @param[in,out] group       The receiver's open group; closed after a
///                            terminal or failed append.
/// @param[in]     buf         Transient event bytes (copied by the sink).
/// @param[in]     len         Number of bytes at @p buf.
/// @param[in]     is_terminal Whether this event terminates (seals) the group.
/// @param[out]    failure     Filled on failure; may be null.
/// @retval false success — the event was appended.
/// @retval true  error — no group was open, or the sink's append failed (the
///               sink truncated the group; its reason is copied to
///               @p failure).
bool append_transaction_event(Open_group &group, const char *buf,
                              std::size_t len, bool is_terminal,
                              Receiver_write_failure *failure = nullptr);

/// @brief Truncate the open group.
///
/// @param[in,out] group The receiver's open group: its stream is marked
///                      truncated, then the group is closed (both fields null,
///                      envelope released). A no-op when no group is open.
void truncate_transaction(Open_group &group);

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_IN_MEMORY_QUEUED_TRANSACTION_WRITER_H
