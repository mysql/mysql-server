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

#ifndef MYSQL_CSA_STORAGE_IN_MEMORY_IMR_TYPES_H
#define MYSQL_CSA_STORAGE_IN_MEMORY_IMR_TYPES_H

#include <cstddef>

namespace mysql::csa {

/// @brief Indicates where a transaction envelope's payload is stored.
enum class Envelope_path { MEMORY, SPILL };

/// @brief An envelope's result; COMMITTED and TRUNCATED are final, first wins.
enum class Envelope_state { ACTIVE, COMMITTED, TRUNCATED };

/// @brief Validated per-channel in-memory relay log configuration: the two
/// persisted IMR bounds the Trx_envelope_queue is built with. Built by
/// Imr_service::reconcile() from options checked by validate_options(), so
/// @c spill_threshold is always strictly below @c memory_limit.
struct Imr_config {
  /// Hard per-channel memory bound in bytes (IN_MEMORY_RELAYLOG_LIMIT): the
  /// most the channel's memory-path payloads may hold before the receiver
  /// blocks.
  std::size_t memory_limit{0};

  /// Size in bytes above which a transaction takes the spill path instead of
  /// the memory path (IN_MEMORY_RELAYLOG_SPILL_THRESHOLD).
  std::size_t spill_threshold{0};
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_IN_MEMORY_IMR_TYPES_H
