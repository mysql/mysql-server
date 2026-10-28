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

#ifndef MYSQL_CSA_RESOURCE_MAP_H
#define MYSQL_CSA_RESOURCE_MAP_H

#include "sql/changestreams/apply/resource/resource_monitor.h"

namespace mysql::csa {

/// Resources used by a channel. Contains specific resources names and
/// registeres them at channel start (init_resources).
struct Resource_map {
  /// Declared channel memory. Each transaction declares that it will use this
  /// specific amount of memory during apply phase
  static constexpr auto declared_channel_memory = "declared_channel_memory";
  /// In-memory relay log memory: the per-channel hard bound
  /// (IN_MEMORY_RELAYLOG_LIMIT) on memory-path payload bytes. Registered by
  /// Imr_service::reconcile(); the queue uses the entry, never the name.
  static constexpr auto in_memory_relaylog_memory = "in_memory_relaylog_memory";
  /// In-memory relay log spill disk: the per-channel soft bound
  /// (relay_log_space_limit) on the declared size of spill files still on
  /// disk. Registered only when the limit is non-zero; see
  /// Trx_envelope_queue::acquire_admission().
  static constexpr auto in_memory_relaylog_spill_disk =
      "in_memory_relaylog_spill_disk";
  /// Registeres named resources for the channel identified by the given
  /// instance_id
  /// @param instance_id Unique id representing a given CSA channel
  /// @param channel_memory The amount of memory available for the channel
  /// (configured via CRST, APPLIER_EVENT_MEMORY_LIMIT)
  /// @param waiting_for_channel_memory_stage Optional pointer to stage set
  /// during waiting
  /// @return False on success, true on failure
  [[nodiscard]] static bool init_resources(
      std::size_t instance_id, std::size_t channel_memory,
      PSI_stage_info *waiting_for_channel_memory_stage);
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_RESOURCE_MAP_H
