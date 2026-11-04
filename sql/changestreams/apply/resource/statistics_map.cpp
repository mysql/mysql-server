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

#include "sql/changestreams/apply/resource/statistics_map.h"
#include "mysql/scheduler/constants.h"
#include "mysql/scheduler/statistics_monitor.h"
#include "sql/changestreams/apply/context/tune.h"

using Constants = mysql::scheduler::Constants;

namespace mysql::csa {

bool Statistics_map::init_statistics(std::size_t instance_id,
                                     std::size_t num_threads,
                                     bool enable_extended_statistics,
                                     bool enable_imr_statistics) {
  assert(instance_id < Constants::max_instances);
  if (instance_id >= Constants::max_instances) {
    return true;
  }
  auto &stat_monitor = mysql::scheduler::Statistics_monitor::get(instance_id);
  stat_monitor.init();
  stat_monitor.register_stat(Statistics_map::active_job_cnt, num_threads,
                             enable_extended_statistics);
  stat_monitor.register_stat(Statistics_map::active_trx_cnt, num_threads,
                             enable_extended_statistics);
  stat_monitor.register_stat(Statistics_map::committed_cnt, num_threads,
                             enable_extended_statistics);
  stat_monitor.register_stat(Statistics_map::trx_exec_time, num_threads,
                             enable_extended_statistics);
  stat_monitor.register_stat(Statistics_map::worker_session, num_threads, true);
  stat_monitor.register_stat(Statistics_map::session_worker,
                             tune::csa_session_default_cache_size, true);
  stat_monitor.register_stat(Statistics_map::trx_provided_cnt, 1,
                             enable_extended_statistics);
  stat_monitor.register_stat(Statistics_map::trx_scheduled_cnt, 1,
                             enable_extended_statistics);
  stat_monitor.register_stat(Statistics_map::ap_queue_size, 1,
                             enable_extended_statistics);
  stat_monitor.register_stat(Statistics_map::applied_events_cnt, num_threads,
                             true);
  // In-memory relay log counters: one shard, since the IMR commit hook that
  // updates them has no worker id (see count_imr_completed()).
  for (const char *name :
       {Statistics_map::imr_mem_trx_cnt, Statistics_map::imr_mem_trx_len,
        Statistics_map::imr_spl_trx_cnt, Statistics_map::imr_spl_trx_len})
    stat_monitor.register_stat(name, 1, enable_imr_statistics);
  return false;
}

void Statistics_map::count_imr_completed(std::size_t instance_id,
                                         bool spill_path,
                                         std::size_t trx_length) {
  if (instance_id >= Constants::max_instances) return;
  auto &stat_monitor = mysql::scheduler::Statistics_monitor::get(instance_id);
  // find_stat(), not get_stat(): get_stat() would insert a missing name.
  auto count =
      stat_monitor.find_stat(spill_path ? Statistics_map::imr_spl_trx_cnt
                                        : Statistics_map::imr_mem_trx_cnt);
  auto bytes =
      stat_monitor.find_stat(spill_path ? Statistics_map::imr_spl_trx_len
                                        : Statistics_map::imr_mem_trx_len);
  if (count.has_value()) count->get().add(1, 0);
  if (bytes.has_value())
    bytes->get().add(static_cast<long long>(trx_length), 0);
}

}  // namespace mysql::csa
