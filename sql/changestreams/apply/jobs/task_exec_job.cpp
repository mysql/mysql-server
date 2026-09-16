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

#include "sql/changestreams/apply/jobs/task_exec_job.h"
#include "mysql/scheduler/logger_stream.h"
#include "mysql/scheduler/statistics_map.h"
#include "sql/changestreams/apply/psi/psi.h"
#include "sql/changestreams/apply/psi/stage.h"
#include "sql/changestreams/apply/resource/statistics_map.h"

using namespace mysql::scheduler;

using Statistics_monitor = mysql::scheduler::Statistics_monitor;
using Statistics_map_sched = mysql::scheduler::Statistics_map;
using Statistics_map_csa = mysql::csa::Statistics_map;

namespace mysql::csa {

bool Task_exec_job::is_error() const { return m_is_error; }
void Task_exec_job::set_error() { m_is_error = true; }

Task_exec_job::Task_exec_job(Job_ptr job, uint64_t job_id,
                             Session_service_ptr session_service,
                             Stat_monitor_ref stat_monitor,
                             std::atomic<bool> &stop_ref)
    : m_job(job),
      m_job_id(job_id),
      m_session_service(session_service),
      m_stat_monitor(stat_monitor),
      m_applier_stop(stop_ref) {}

Task_result Task_exec_job::operator()(unsigned int thread_id) {
  auto &stat_monitor = m_stat_monitor.get();
  m_started = true;
  stat_monitor.get_stat(Statistics_map_csa::active_job_cnt).add(1, thread_id);
  stat_monitor.get_stat(Statistics_map_csa::active_trx_cnt).add(1, thread_id);
  auto run_res = run(m_job.get(), m_job_id, thread_id);
  stat_monitor.get_stat(Statistics_map_csa::active_job_cnt).add(-1, thread_id);
  if (m_job->is_done()) {
    stat_monitor.get_stat(Statistics_map_csa::active_trx_cnt)
        .add(-1, thread_id);
  }
  return run_res;
}

Task_result Task_exec_job::run(Job_ptr job, uint64_t job_id,
                               unsigned int thread_id) {
  concurrency::set_stage(stage_csa_job_apply.m_key);
  static_cast<void>(job_id);

  if (job->is_done()) {
    /// This is early return for multiple phase tasks returning in early
    /// phases
    return Task_result::success;
  }

  job->set_applier_stop(m_applier_stop);

  auto end_task = [this, job, thread_id](Task_result status) -> Task_result {
    job->set_done();
    if (status == Task_result::success) {
      job->set_success();
    } else {
      job->set_failure();
      set_error();
    }
    if (job->is_attached() && job->detach(thread_id)) {
      status = Task_result::fatal_error;
    }
    if (status != Task_result::success) {
      job->set_error();
      MYSQL_LIB_LOG_DEBUG() << "Job " << job->get_id()
                            << " failed to run! Job gtid: " << job->to_string();
      return Task_result::fatal_error;
    }
    return Task_result::success;
  };

  // attaches the job session info to this physical thread
  if (job->attach(thread_id)) {
    return end_task(Task_result::fatal_error);
  }

  // run the job
  auto is_error = job->run(thread_id);

  if (is_error) {
    if (m_applier_stop || !job->can_be_retried()) {
      return end_task(Task_result::fatal_error);
    }
    job->inc_retries();
    job->restart();
    if (job->detach(thread_id)) {
      end_task(Task_result::fatal_error);
    }
    MYSQL_LIB_LOG_DEBUG() << "Retrying job " << job->get_id() << "!";
    int wait_before_retry = std::min((int)job->get_retries(), 5);
    std::this_thread::sleep_for(std::chrono::seconds(wait_before_retry));
    return run(job, job_id, thread_id);
  }
  return end_task(Task_result::success);
}

}  // namespace mysql::csa
