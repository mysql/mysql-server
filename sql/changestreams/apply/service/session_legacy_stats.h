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

#ifndef MYSQL_CSA_SESSION_LEGACY_STATS_H
#define MYSQL_CSA_SESSION_LEGACY_STATS_H

#include <cstring>
#include "sql/rpl_gtid.h"       // Trx_monitoring_info
#include "sql/rpl_reporting.h"  // Slave_reporting_capability::Error

namespace mysql::csa {

/// @brief Aggregates MTA legacy statistics
struct Session_legacy_stats {
  /// Ongoing trx
  Trx_monitoring_info m_ongoing_trx;
  /// Last applied trx
  Trx_monitoring_info m_applied_trx;
  /// Last error
  Slave_reporting_capability::Error m_error;
  /// Channel id
  std::string m_channel_id;
  /// PSF worker id (1...N)
  std::size_t m_psf_worker_id;
  /// Thread (psi) id or 0 if unavailable
  ulonglong m_thread_id;
  /// Worker running state: true - active, false - inactive (idle channel)
  bool m_is_on{false};
  /// @brief Resets temporary data (error, transaction information, thread id)
  void clear();
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_SESSION_LEGACY_STATS_H
