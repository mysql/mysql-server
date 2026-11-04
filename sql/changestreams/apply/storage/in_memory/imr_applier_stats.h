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

#ifndef MYSQL_CSA_STORAGE_IN_MEMORY_IMR_APPLIER_STATS_H
#define MYSQL_CSA_STORAGE_IN_MEMORY_IMR_APPLIER_STATS_H

namespace mysql::csa {

/// @brief In-memory relay log transactions completed by the applier during
/// its current run, per path.
///
/// A snapshot returned by Csa_service::get_imr_applier_stats(). The counters
/// themselves live in the channel's CSA statistics (Statistics_monitor), where
/// the IMR commit hooks update them (Statistics_map::count_imr_completed()).
struct Imr_applier_stats {
  /// Transactions completed from the memory path
  long long memory_trx_count{0};
  /// Sum of trx_length of the transactions completed from the memory path
  long long memory_trx_bytes{0};
  /// Transactions completed from the spill path
  long long spill_trx_count{0};
  /// Sum of trx_length of the transactions completed from the spill path
  long long spill_trx_bytes{0};
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_IN_MEMORY_IMR_APPLIER_STATS_H
