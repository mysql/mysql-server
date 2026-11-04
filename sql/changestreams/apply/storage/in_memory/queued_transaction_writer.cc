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

// Receiver-side writer for the in-memory relay log. See
// queued_transaction_writer.h.

#include "sql/changestreams/apply/storage/in_memory/queued_transaction_writer.h"

#include <cassert>
#include <string>
#include <utility>

#include "my_dbug.h"  // DBUG_EXECUTE_IF
#include "sql/changestreams/apply/storage/common/streaming_event_sink.h"
#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"
#include "sql/current_thd.h"  // current_thd
#include "sql/debug_sync.h"   // DBUG_SIGNAL_WAIT_FOR

namespace mysql::csa {

namespace {

/// Close the receiver's open group: clear it, then release the hold. The
/// release is the receiver's last touch of the envelope, payload and sink;
/// clearing first means the group never points to a released envelope.
void close_group(Open_group &group) {
  Transaction_envelope *envelope = group.envelope;
  group = {};
  if (envelope != nullptr) envelope->release_receiver();
}

}  // namespace

bool open_transaction(Trx_envelope_queue &queue, Open_group &group,
                      std::shared_ptr<Format_description_log_event> fde,
                      std::size_t trx_length, bool is_trx,
                      Receiver_write_failure *failure) {
  // A new GTID while a group is open means that transaction is incomplete
  // (e.g. a reconnect whose Rotate did not arrive). Truncate it so its worker
  // rolls back instead of waiting. This also releases its envelope first, so
  // the receiver never waits in admission on bytes it holds itself.
  truncate_transaction(group);
  // enqueue() holds the envelope before publishing it, so the envelope and its
  // sink stay alive until this group is closed. nullptr means a stop was
  // requested during admission.
  Transaction_envelope *env = queue.enqueue(trx_length, is_trx, std::move(fde),
                                            /*receiver_held=*/true);
  if (env == nullptr) {
    if (failure != nullptr) failure->stopped = true;
    return true;
  }
  group.envelope = env;
  group.sink = env->current_sink();

  // Detect a spill file that could not be created. enqueue() still returns
  // the envelope in that case, already marked truncated so no worker applies
  // it, so only the sink shows the failure. On failure, release the envelope
  // so the queue can reclaim it, and return true: the IO thread stops at this
  // GTID event, before any of the transaction's events are written, so the
  // GTID is not added to Retrieved_Gtid_Set and is fetched again on restart.
  // has_io_error() reports only the receiver's own write failures, never a
  // worker's failure to read the file.
  //
  // Test hook: pause after the envelope is visible to the applier and before
  // this check, so a test can make a worker's read fail in between.
  DBUG_SIGNAL_WAIT_FOR(current_thd, "imr_pause_after_enqueue_publish",
                       "receiver_reached_after_enqueue_publish",
                       "receiver_continue_after_enqueue_publish");
  if (group.sink != nullptr && group.sink->has_io_error()) {
    // Read before close_group(): the release may free the sink.
    if (failure != nullptr) {
      failure->reason =
          "[In-memory relay log] failed to admit incoming transaction: ";
      failure->reason += group.sink->io_error_message();
    }
    close_group(group);
    return true;
  }
  return false;
}

bool append_transaction_event(Open_group &group, const char *buf,
                              std::size_t len, bool is_terminal,
                              Receiver_write_failure *failure) {
  // Defensive: no group is open, so there is nothing to append into.
  if (group.sink == nullptr) {
    if (failure != nullptr)
      failure->reason =
          "[In-memory relay log] no open transaction group for the event";
    return true;
  }

  // A true return is a spill write failure; the sink already truncated the
  // stream. The caller stops the IO thread, and the GTID is never recorded
  // because the terminal bookkeeping runs only on a successful append.
  if (group.sink->append_event(buf, len, /*seal_after=*/is_terminal)) {
    // Read before close_group(): the release may free the sink.
    if (failure != nullptr) {
      failure->reason =
          "[In-memory relay log] failed to append event to incoming "
          "transaction: ";
      failure->reason += group.sink->io_error_message();
    }
    close_group(group);
    return true;
  }
  // Close only after append_event() returns: the sink still notifies its
  // stream after the terminal event is visible, and a worker may commit then.
  if (is_terminal) close_group(group);
  return false;
}

void truncate_transaction(Open_group &group) {
  // Checked on the envelope, not the sink, so a group can never leak a hold.
  if (group.envelope == nullptr) return;
  // The sink marks the envelope truncated and then still notifies its stream;
  // the hold keeps both alive until close_group() releases it.
  if (group.sink != nullptr) group.sink->set_stream_truncated();
  close_group(group);
}

}  // namespace mysql::csa
