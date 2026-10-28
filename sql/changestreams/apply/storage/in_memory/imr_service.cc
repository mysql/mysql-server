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

#include "sql/changestreams/apply/storage/in_memory/imr_service.h"

#include <algorithm>  // min
#include <cassert>
#include <cstdio>  // snprintf
#include <limits>  // numeric_limits
#include <memory>  // make_unique
#include <string>

#include "my_dbug.h"                                 // DBUG_TRACE
#include "my_sys.h"                                  // dirname_length
#include "mysql/components/services/log_builtins.h"  // LogErr
#include "mysql/my_loglevel.h"                       // ERROR_LEVEL
#include "mysqld_error.h"  // ER_CRST_IN_MEMORY_RELAYLOG_INVALID_CONFIG, ER_LOG_PRINTF_MSG
#include "sql/changestreams/apply/resource/resource_map.h"  // Resource_map
#include "sql/changestreams/apply/resource/resource_monitor.h"  // Resource_monitor, Resource_entry
#include "sql/changestreams/apply/storage/in_memory/imr_runtime.h"  // Imr_runtime
#include "sql/changestreams/apply/storage/in_memory/imr_types.h"  // Imr_config
#include "sql/changestreams/apply/storage/in_memory/spill_directory.h"  // ensure_spill_directory, purge_orphan_spill_files
#include "sql/rpl_mi.h"       // Master_info
#include "sql/rpl_msr.h"      // channel_map
#include "sql/rpl_replica.h"  // relay_log_basename
#include "sql/rpl_rli.h"      // Relay_log_info

namespace mysql::csa {

namespace {

/// The server's relay log directory: the directory part of the read-only
/// relay_log_basename, with its trailing separator. Every channel creates its
/// new relay log files there, and init_spill_storage() creates the spill
/// directory under it. Empty while relay_log_basename is not set.
std::string server_relay_log_dir() {
  if (relay_log_basename == nullptr || relay_log_basename[0] == '\0') return {};
  return std::string(relay_log_basename, dirname_length(relay_log_basename));
}

}  // namespace

Imr_service::~Imr_service() {
  // Channels are torn down before the service, so every runtime should already
  // have been removed via remove(). Clearing here is a defensive backstop that
  // also destroys any straggler while Imr_runtime is a complete type.
  m_runtimes.clear();
}

Imr_options_validation Imr_service::validate_options(
    bool limit_specified, unsigned long long effective_limit,
    bool spill_threshold_specified,
    unsigned long long effective_spill_threshold) {
  // The effective values are already unsigned long long; the bound constants
  // are unsigned int and are cast so every %llu argument has a matching type.
  char reason[256];

  if (limit_specified &&
      (effective_limit < kLimitMin || effective_limit > kLimitMax)) {
    snprintf(reason, sizeof(reason),
             "IN_MEMORY_RELAYLOG_LIMIT (%llu bytes) must be between %llu "
             "and %llu bytes",
             effective_limit, static_cast<unsigned long long>(kLimitMin),
             static_cast<unsigned long long>(kLimitMax));
    return {false, ER_CRST_IN_MEMORY_RELAYLOG_INVALID_CONFIG, reason};
  }
  if (spill_threshold_specified &&
      effective_spill_threshold < kSpillThresholdMin) {
    snprintf(reason, sizeof(reason),
             "IN_MEMORY_RELAYLOG_SPILL_THRESHOLD (%llu bytes) must be at "
             "least %llu bytes",
             effective_spill_threshold,
             static_cast<unsigned long long>(kSpillThresholdMin));
    return {false, ER_CRST_IN_MEMORY_RELAYLOG_INVALID_CONFIG, reason};
  }
  // The limit must stay strictly greater than the spill threshold, whichever of
  // the two the statement changes.
  if (effective_spill_threshold >= effective_limit) {
    snprintf(reason, sizeof(reason),
             "IN_MEMORY_RELAYLOG_LIMIT (%llu bytes) must be greater than "
             "IN_MEMORY_RELAYLOG_SPILL_THRESHOLD (%llu bytes)",
             effective_limit, effective_spill_threshold);
    return {false, ER_CRST_IN_MEMORY_RELAYLOG_INVALID_CONFIG, reason};
  }

  return {true, 0, {}};
}

void Imr_service::reconcile(Relay_log_info *rli) {
  DBUG_TRACE;
  assert(rli != nullptr);

  // A runtime is wanted exactly when the channel selects the in-memory path and
  // is CSA-enabled.
  const bool want_runtime =
      rli->is_in_memory_relaylog() && rli->is_csa_enabled();

  // The classic applier's temporary-error retry arrives here
  // without the channel_map lock. It runs only on non-CSA channels.
  if (!want_runtime && rli->imr() == nullptr) return;

  // Mutates m_runtimes; serialized by the channel_map write lock (no own lock).
  channel_map.assert_some_wrlock();

  // IN_MEMORY_RELAYLOG_LIMIT / IN_MEMORY_RELAYLOG_SPILL_THRESHOLD, defaulting
  // to 128 MiB / 16 MiB when never configured.
  Imr_config config;
  config.memory_limit = rli->get_in_memory_relaylog_limit();
  config.spill_threshold = rli->get_in_memory_relaylog_spill_threshold();

  // The server's spill directory, not the directory of the channel's saved
  // relay log position, which can name an older --relay-log directory.
  std::string relay_log_dir = server_relay_log_dir();

  // Embedded in spill file names (see Spill_file_writer::kFileNamePrefix).
  // Immutable, so it never affects the rebuild decision.
  const char *channel = rli->get_channel();
  std::string channel_name(channel != nullptr ? channel : "");

  // The spill directory is fixed for the server's lifetime, so only the bounds
  // can cause a rebuild.
  Imr_runtime *current = rli->imr();
  assert(current == nullptr ||
         current->queue()->relay_log_dir() == relay_log_dir);
  const bool config_matches =
      current != nullptr &&
      current->queue()->memory_limit() == config.memory_limit &&
      current->queue()->spill_threshold() == config.spill_threshold;

  const std::size_t key = rli->get_channel_instance_id();

  const Imr_runtime::Reconcile_action action = Imr_runtime::reconcile_action(
      want_runtime, current != nullptr, config_matches);

  // kDestroy and kRebuild free the queue, so keep the runtime if a role is
  // still attached. A detached queue was reset by the last role out, so it is
  // empty, as the queue destructor requires.
  if (current != nullptr &&
      (action == Imr_runtime::Reconcile_action::kDestroy ||
       action == Imr_runtime::Reconcile_action::kRebuild)) {
    assert(current->fully_detached());
    if (!current->fully_detached()) return;
    assert(current->queue_length() == 0);
  }

  switch (action) {
    case Imr_runtime::Reconcile_action::kLeave:
      return;
    case Imr_runtime::Reconcile_action::kDestroy:
      // A channel with no runtime that wants none returned above.
      assert(current != nullptr);
      // Clear the cache first, then erase the owning entry. Destroy always
      // means erase, so an entry never holds a null handle.
      rli->set_imr_ptr(nullptr);
      m_runtimes.erase(key);
      return;
    case Imr_runtime::Reconcile_action::kRebuild:
    case Imr_runtime::Reconcile_action::kCreate: {
      // A rebuild drops the old runtime first; register_resource() frees its
      // entry. Metric readers hold the channel_map read lock, so see no gap.
      if (current != nullptr) {
        rli->set_imr_ptr(nullptr);
        m_runtimes.erase(key);
      }
      // Hand the memory entry to the queue, which never looks it up by name.
      // No stage: IMR never uses the monitor's blocking wait.
      Resource_instance_monitor &monitor = Resource_monitor::get(key);
      monitor.register_resource(Resource_map::in_memory_relaylog_memory,
                                config.memory_limit);
      Resource_entry *memory_resource =
          monitor.get_resource_entry(Resource_map::in_memory_relaylog_memory);
      // Spill disk limit: 0 means unlimited (no entry). relay_log_space_limit
      // is read-only, so it never causes a rebuild. It allows up to
      // ULLONG_MAX, but the entry stores long long, so clamp.
      Resource_entry *spill_disk_resource = nullptr;
      if (relay_log_space_limit != 0) {
        monitor.register_resource(
            Resource_map::in_memory_relaylog_spill_disk,
            static_cast<std::size_t>(std::min<ulonglong>(
                relay_log_space_limit, std::numeric_limits<long long>::max())));
        spill_disk_resource = monitor.get_resource_entry(
            Resource_map::in_memory_relaylog_spill_disk);
      }
      auto runtime = std::make_unique<Imr_runtime>(
          config, std::move(relay_log_dir), memory_resource,
          std::move(channel_name), spill_disk_resource, key);
      Imr_runtime *p = runtime.get();
      m_runtimes[key] = std::move(runtime);
      rli->set_imr_ptr(p);
      // The new queue is empty, so Retrieved_Gtid_Set must be reset.
      assert(rli->mi != nullptr);
      Imr_runtime::clear_received_gtid_state(rli->mi);
      return;
    }
  }
}

std::size_t Imr_service::runtime_count() const { return m_runtimes.size(); }

void Imr_service::remove(Relay_log_info *rli) {
  DBUG_TRACE;
  // Mutates m_runtimes; serialized by the channel_map write lock (no own lock).
  channel_map.assert_some_wrlock();
  if (rli == nullptr) return;
  // Clear the cache, then erase the owning entry (a no-op without a runtime).
  // Requires a live rli and both roles detached.
  rli->set_imr_ptr(nullptr);
  m_runtimes.erase(rli->get_channel_instance_id());
}

bool Imr_service::attach_receiver(Master_info *mi) {
  Imr_runtime *imr = mi->rli->imr();
  if (imr == nullptr) return false;  // classic channel: nothing to arm
  return imr->attach_role(Imr_runtime::Scope::RECEIVER);
}

void Imr_service::request_receiver_stop(Master_info *mi) {
  // No role mutex: the queue's stop() synchronizes itself.
  if (Imr_runtime *imr = mi->rli->imr())
    imr->request_stop(Imr_runtime::Scope::RECEIVER);
}

void Imr_service::detach_receiver(Master_info *mi) {
  // reconcile() and remove() need both threads stopped, so this is the runtime
  // the role attached to.
  if (Imr_runtime *imr = mi->rli->imr())
    (void)imr->detach_role(Imr_runtime::Scope::RECEIVER, mi);
}

bool Imr_service::attach_applier(Master_info *mi) {
  Imr_runtime *imr = mi->rli->imr();
  if (imr == nullptr) return false;  // classic channel: nothing to arm
  return imr->attach_role(Imr_runtime::Scope::APPLIER);
}

void Imr_service::request_applier_stop(Master_info *mi) {
  // No role mutex: the queue's stop() synchronizes itself.
  if (Imr_runtime *imr = mi->rli->imr())
    imr->request_stop(Imr_runtime::Scope::APPLIER);
}

void Imr_service::detach_applier(Master_info *mi) {
  // reconcile() and remove() need both threads stopped, so this is the runtime
  // the role attached to.
  if (Imr_runtime *imr = mi->rli->imr())
    (void)imr->detach_role(Imr_runtime::Scope::APPLIER, mi);
}

void Imr_service::init_spill_storage() {
  DBUG_TRACE;

  // A stale file that cannot be deleted stays in the spill name space, so IMR
  // channels are then not started (check_replica_configuration_errors()). The
  // purge goes on for the other files, and the server still starts.
  const std::string relay_log_dir = server_relay_log_dir();
  const bool delete_failed =
      !relay_log_dir.empty() && purge_orphan_spill_files(relay_log_dir);

  if (delete_failed) {
    m_spill_cleanup_failed = true;
    LogErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
           "[In-memory relay log] stale spill files could not be removed at "
           "startup (see the errors above). Replication channels that use the "
           "in-memory relay log (CSA channels with IN_MEMORY_RELAYLOG_ENABLED "
           "= ON) will not start until the problem is fixed and the server "
           "is restarted.");
  }

  // Create the directory even if no channel uses IMR yet: CHANGE REPLICATION
  // SOURCE can enable it later, and the writer does not create it.
  if (relay_log_dir.empty()) return;
  // An unusable directory also keeps IMR channels from starting.
  if (ensure_spill_directory(relay_log_dir)) {
    m_spill_dir_unusable = true;
    LogErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
           "[In-memory relay log] the spill file directory is not usable (see "
           "the error above). Replication channels that use the in-memory "
           "relay log (CSA channels with IN_MEMORY_RELAYLOG_ENABLED = ON) will "
           "not start until the path is fixed and the server is restarted.");
  }
}

}  // namespace mysql::csa
