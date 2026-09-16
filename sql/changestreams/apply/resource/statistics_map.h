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

#ifndef MYSQL_CSA_STATISTICS_MAP_H
#define MYSQL_CSA_STATISTICS_MAP_H

#include <atomic>
#include <chrono>
#include <memory>
#include <unordered_map>
#include "mysql/concurrency/condition_variable.h"
#include "mysql/concurrency/mutex.h"
#include "mysql/scheduler/sharded_counter.h"

namespace mysql::csa {

/// Supported statistics:
/// * active_job_cnt - the number of currently executed jobs by thread pool
///   workers (equal to active workers)
/// * active_trx_cnt - the number of currently applied transactions (started
///   transactions, active until "commit", regardless possible wait on commit
///   order)
/// * committed_cnt - transactions committted count
/// * trx_exec_time - transactions execution time (all transactions executed
///   for a particular instance id / channel id)
/// * worker_session - session ids currently utilized by workers
/// * session_worker - workers that currently work on a given session
/// * trx_provided_cnt - provided transaction count (e.g. read)
/// * trx_scheduled_cnt - scheduled transaction count
/// * ap_queue_size - asynchronous provider queue size
/// * applied_events_cnt - the overall, and ever-increasing, number of applied
/// events
class Statistics_map {
 public:
  static constexpr auto active_job_cnt = "active_job_cnt";
  static constexpr auto active_trx_cnt = "active_trx_cnt";
  static constexpr auto committed_cnt = "committed_cnt";
  static constexpr auto trx_exec_time = "trx_exec_time";
  static constexpr auto worker_session = "worker_session";
  static constexpr auto session_worker = "session_worker";
  static constexpr auto trx_provided_cnt = "trx_provided_cnt";
  static constexpr auto trx_scheduled_cnt = "trx_scheduled_cnt";
  static constexpr auto ap_queue_size = "ap_queue_size";
  static constexpr auto applied_events_cnt = "applied_events_cnt";
  [[nodiscard]] static bool init_statistics(std::size_t instance_id,
                                            std::size_t num_threads,
                                            bool enable_extended_statistics);
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STATISTICS_MAP_H
