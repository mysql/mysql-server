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

#include "sql/changestreams/apply/storage/in_memory/imr_runtime.h"

#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "sql/changestreams/apply/storage/in_memory/imr_types.h"  // Imr_config
#include "sql/changestreams/apply/storage/in_memory/queued_transaction_writer.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"
#include "sql/log_event.h"  // Format_description_log_event
#include "sql/rpl_gtid.h"   // Gtid_set
#include "sql/rpl_mi.h"     // Master_info
#include "sql/rpl_rli.h"    // Relay_log_info

namespace mysql::csa {

namespace {

#ifndef NDEBUG
/// The FDE fields that event decoding reads.
bool same_decoding_fields(const Format_description_log_event &a,
                          const Format_description_log_event &b) {
  return a.common_footer->checksum_alg == b.common_footer->checksum_alg &&
         a.binlog_version == b.binlog_version &&
         std::strcmp(a.server_version, b.server_version) == 0 &&
         a.post_header_len == b.post_header_len &&
         a.common_header_len == b.common_header_len;
}
#endif

}  // namespace

Imr_runtime::~Imr_runtime() {
  assert(fully_detached());
  assert(m_receiver_group.envelope == nullptr &&
         m_receiver_group.sink == nullptr);
  assert(m_fde_snapshot == nullptr);
}

Imr_runtime::Reconcile_action Imr_runtime::reconcile_action(
    bool want_runtime, bool has_runtime, bool config_matches) {
  if (!want_runtime) return Reconcile_action::kDestroy;
  if (has_runtime) {
    return config_matches ? Reconcile_action::kLeave
                          : Reconcile_action::kRebuild;
  }
  return Reconcile_action::kCreate;
}

bool Imr_runtime::is_role_owner(Scope scope) const {
  const std::atomic<std::thread::id> &owner =
      scope == Scope::RECEIVER ? m_receiver_owner : m_applier_owner;
  return owner.load(std::memory_order_relaxed) == std::this_thread::get_id();
}

bool Imr_runtime::is_attached(Scope scope) const {
  const bool receiver =
      m_receiver_owner.load(std::memory_order_acquire) != std::thread::id{};
  const bool applier =
      m_applier_owner.load(std::memory_order_acquire) != std::thread::id{};
  switch (scope) {
    case Scope::RECEIVER:
      return receiver;
    case Scope::APPLIER:
      return applier;
  }
  return false;
}

bool Imr_runtime::attach_role(Scope scope) {
  // Ordered against the other role's detach and last-out reset.
  std::lock_guard<std::mutex> role_lock(m_role_mutex);
  // At most one owner per role. A refused second thread changes nothing, and
  // its exit path is a no-op because detach_role() is owner-gated.
  if (is_attached(scope)) return true;
  std::atomic<std::thread::id> &owner =
      scope == Scope::RECEIVER ? m_receiver_owner : m_applier_owner;
  owner.store(std::this_thread::get_id(), std::memory_order_release);
  // The queue rests stopped between sessions; resume() clears only the role's
  // stop flag, never the queue's contents.
  m_queue.resume(scope);
  // A new receiver starts with no recorded failure.
  if (scope == Scope::RECEIVER) m_last_write_failure = {};
  // The previous receiver closed its group before it cleared its owner.
  assert(scope != Scope::RECEIVER || (m_receiver_group.envelope == nullptr &&
                                      m_receiver_group.sink == nullptr));
  return false;
}

bool Imr_runtime::detach_role(Scope scope, Master_info *mi) {
  // Same mutex as the other thread's attach and detach, so the detach, the
  // "last role out" check and the reset are one step.
  std::lock_guard<std::mutex> role_lock(m_role_mutex);
  // Only the owner detaches, so a refused thread's exit path is a no-op.
  if (!is_role_owner(scope)) return false;
  if (scope == Scope::RECEIVER) {
    // Truncate the open group so a running applier rolls it back, and close it
    // to release the envelope hold. Must precede clearing the owner: a
    // last-out reset may then clear the queue the group points into.
    mysql::csa::truncate_transaction(m_receiver_group);
    assert(m_receiver_group.envelope == nullptr &&
           m_receiver_group.sink == nullptr);
    // An idle channel keeps no FDE copy; queued transactions hold their own
    // reference.
    m_fde_snapshot.reset();
  } else {
    // Reclaim the committed prefix the coordinator left behind. The CSA
    // workers are gone, so nothing commits concurrently.
    m_queue.sweep_committed();
  }
  // Disarm the role (idempotent after request_stop()). The queue survives for
  // the other role.
  m_queue.stop(scope);
  // Clear the owner last: from here fully_detached() treats the role as gone.
  std::atomic<std::thread::id> &owner =
      scope == Scope::RECEIVER ? m_receiver_owner : m_applier_owner;
  owner.store(std::thread::id{}, std::memory_order_release);
  // The last role out resets the queue. The caller holds its run lock, so no
  // START, CHANGE REPLICATION SOURCE or RESET REPLICA runs until it is done.
  if (fully_detached() && mi != nullptr) reset(mi);
  return true;
}

void Imr_runtime::request_stop(Scope scope) { m_queue.stop(scope); }

void Imr_runtime::set_receiver_fde(const Format_description_log_event &fde) {
  assert(is_role_owner(Scope::RECEIVER));
  // Copy the Format_description_event part, as Spill_file_writer::open()
  // does: it holds every field that decoding and applying an event read.
  std::shared_ptr<Format_description_log_event> snapshot(
      new Format_description_log_event());
  static_cast<mysql::binlog::event::Format_description_event &>(*snapshot) =
      static_cast<const mysql::binlog::event::Format_description_event &>(fde);
  m_fde_snapshot = std::move(snapshot);
}

bool Imr_runtime::open_transaction(const Format_description_log_event &fde,
                                   std::size_t trx_length, bool is_trx) {
  assert(is_role_owner(Scope::RECEIVER));
  // set_receiver_fde() ran for the FDE event that precedes every
  // transaction of a connection.
  assert(m_fde_snapshot != nullptr);
  // Release builds: a stream with no FDE event before the transaction.
  if (m_fde_snapshot == nullptr) set_receiver_fde(fde);
  // The receiver's decoding fields change only with a source FDE event.
  assert(same_decoding_fields(*m_fde_snapshot, fde));
  m_last_write_failure = {};
  return mysql::csa::open_transaction(m_queue, m_receiver_group, m_fde_snapshot,
                                      trx_length, is_trx,
                                      &m_last_write_failure);
}

bool Imr_runtime::wait_for_admission(std::size_t trx_length) {
  assert(is_role_owner(Scope::RECEIVER));
  // Fits: no wait, and the open group is left to open_transaction().
  if (m_queue.is_admissible(trx_length)) return false;
  // About to wait. A new GTID event means a still-open group is incomplete,
  // and open_transaction() truncates it before its admission anyway. Do it
  // now, so the receiver holds no envelope while it waits.
  mysql::csa::truncate_transaction(m_receiver_group);
  return m_queue.wait_until_admissible(trx_length);
}

bool Imr_runtime::append_event(const char *buf, std::size_t len,
                               bool is_terminal) {
  assert(is_role_owner(Scope::RECEIVER));
  m_last_write_failure = {};
  return mysql::csa::append_transaction_event(
      m_receiver_group, buf, len, is_terminal, &m_last_write_failure);
}

void Imr_runtime::truncate_receiver_group() {
  // All builds: the exit path of a refused second IO thread reaches this.
  if (!is_role_owner(Scope::RECEIVER)) return;
  mysql::csa::truncate_transaction(m_receiver_group);
}

void Imr_runtime::reset(Master_info *mi) {
  assert(mi != nullptr);
  assert(mi->rli != nullptr);
  // No receiver, coordinator, or worker may observe the state while it is
  // cleared; the caller stops and joins both roles first.
  assert(fully_detached());
  assert(m_receiver_group.envelope == nullptr &&
         m_receiver_group.sink == nullptr);
  assert(m_fde_snapshot == nullptr);

  // 1.-3. Parser, monitoring info and Retrieved_Gtid_Set. This must happen
  //       together with dropping the queue (step 4): the queued transactions
  //       are volatile, so the set describing them cannot outlive them.
  clear_received_gtid_state(mi);

  // 4. Queue contents and cursors: back to empty, reusable for a new session.
  m_queue.reset();
}

void Imr_runtime::clear_received_gtid_state(Master_info *mi) {
  assert(mi != nullptr && mi->rli != nullptr);

  // 1. Transaction boundary parser: forget any half-parsed event group.
  mi->transaction_parser.reset();

  // 2. Receiver GTID monitoring info, under the receiver's data_lock.
  mysql_mutex_lock(&mi->data_lock);
  mi->clear_gtid_monitoring_info();
  mysql_mutex_unlock(&mi->data_lock);

  // 3. Retrieved_Gtid_Set and its TSID map, under the RLI TSID write lock.
  Relay_log_info *rli = mi->rli;
  rli->get_tsid_lock()->wrlock();
  const_cast<Gtid_set *>(rli->get_gtid_set())->clear_set_and_tsid_map();
  rli->get_tsid_lock()->unlock();
}

}  // namespace mysql::csa
