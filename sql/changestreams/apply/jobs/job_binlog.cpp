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

#include "sql/changestreams/apply/jobs/job_binlog.h"
#include "mysql/binlog/event/binlog_event.h"  // Log_event_type_helper
#include "mysql/scheduler/dispatch_reason.h"
#include "mysql/scheduler/statistics_monitor.h"
#include "sql/changestreams/apply/resource/statistics_map.h"

using namespace mysql::binlog::event;
using Statistics_map = mysql::csa::Statistics_map;
using Statistics_monitor = mysql::scheduler::Statistics_monitor;

namespace mysql::csa {

Job_binlog::Job_binlog(Channel *channel, unsigned int max_retries,
                       std::shared_ptr<Fetchable_transaction> fetch_object,
                       Stat_monitor_ref stat_monitor)
    : Job(max_retries),
      m_channel(channel),
      m_stat_monitor(stat_monitor),
      m_fetch_metadata(std::move(fetch_object)) {
  restart_internal(false);
}

Job_binlog::~Job_binlog() {}

Channel *Job_binlog::get_channel() const { return m_channel; }

std::string Job_binlog::get_trx_id() const { return m_trx_gtid.to_string(); }

const std::string &Job_binlog::get_channel_id() const {
  return m_channel->get_name();
}

unsigned long long Job_binlog::get_last_committed() const {
  if (!m_first_event) return SEQ_UNINIT;
  auto type = m_first_event.get_event()->common_header->type_code;
  if (!Log_event_type_helper::is_any_gtid_event(type)) {
    assert(false);
    return SEQ_UNINIT;
  }
  return ((Gtid_log_event *)m_first_event.get_event().get())->last_committed;
}

unsigned int Job_binlog::get_instance_id() const {
  return m_channel->get_channel_id();
}

void Job_binlog::set_success() {
  assert(m_fetch_metadata);
  if (m_phase == Transaction_phase::done && m_fetch_metadata) {
    if (!m_fetch_metadata->is_truncated()) {
      m_fetch_metadata->set_success();
    }
    m_fetch_metadata.reset();
  }
}

unsigned long long Job_binlog::get_sequence_number() const {
  if (!m_first_event) return SEQ_UNINIT;
  auto type = m_first_event.get_event()->common_header->type_code;
  if (!Log_event_type_helper::is_any_gtid_event(type)) {
    assert(false);
    return SEQ_UNINIT;
  }
  return ((Gtid_log_event *)m_first_event.get_event().get())->sequence_number;
}

bool Job_binlog::is_complete() { return m_fetch_metadata->is_fetching_done(); }

void Job_binlog::set_done() {
  if (m_phase == Transaction_phase::done || is_error()) {
    m_is_done = true;
  }
}

void Job_binlog::skip() {
  m_phase = Transaction_phase::done;
  m_is_done = true;
  set_success();
}

bool Job_binlog::run(Thread_id thread_id) {
  bool is_error = false;
  auto dispatch_reason = scheduler::current_dispatch_reason();
  // if transaction is atomic DDL, it will execute in "prepare"
  if (m_phase == Transaction_phase::done) return false;
  auto &stat_monitor = m_stat_monitor.get();
  auto &trx_exec_timer = stat_monitor.get_stat(Statistics_map::trx_exec_time);
  trx_exec_timer.start_time(thread_id);
  if (m_channel->get_commit_order_manager() && is_trx() &&
      m_phase != Transaction_phase::retry_commit) {
    assert(m_phase != Transaction_phase::done);
    if (m_phase == Transaction_phase::prepare) {
      if (dispatch_reason == scheduler::Dispatch_reason::unblocked) {
        is_error = this->commit_register(thread_id);
      } else {
        is_error = this->prepare(thread_id);
      }
    } else if (m_phase == Transaction_phase::commit_register) {
      is_error = this->commit_register(thread_id);
    } else if (m_phase == Transaction_phase::commit_binlog) {
      is_error = this->commit(thread_id);
    }
  } else {
    assert(m_phase == Transaction_phase::prepare ||
           m_phase == Transaction_phase::retry_commit);
    // execute in one phase
    if (this->prepare(thread_id)) return true;
    if (this->commit_register(thread_id)) return true;
    if (this->commit(thread_id)) return true;
  }
  trx_exec_timer.stop_time(thread_id);
  return is_error;
}

bool Job_binlog::is_trx() const { return m_fetch_metadata->is_trx(); }

bool Job_binlog::restart() { return restart_internal(true); }

bool Job_binlog::restart_internal(bool all) {
  assert(m_phase != Transaction_phase::done);
  m_next_event = 0;
  Job::restart();
  if (m_phase == Transaction_phase::commit_binlog) {
    m_phase = Transaction_phase::retry_commit;
  }
  if (all) {
    m_first_event.get_event().reset();
  }
  m_fetch_metadata->reset_fetching(all);
  if (m_fetch_metadata->is_trx() && !m_first_event) {
    if (m_fetch_metadata->wait_next()) {
      auto first_fetch_result = m_fetch_metadata->fetch_next();
      if (first_fetch_result.has_value()) {
        m_first_event = first_fetch_result.value();
        Gtid_log_event *gev =
            dynamic_cast<Gtid_log_event *>(m_first_event.get_event().get());
        if (gev) {
          m_trx_gtid = mysql::gtid::Gtid(gev->get_tsid(), gev->get_gno());
        }
      }
    }
    m_fetch_metadata->reset_fetching(false);
  }
  return false;
}

unsigned long long Job_binlog::get_trx_length() const {
  if (!m_first_event) return 0;
  auto type = m_first_event.get_event()->common_header->type_code;
  if (!Log_event_type_helper::is_any_gtid_event(type)) {
    assert(false);
    return 0;
  }
  Gtid_log_event *gev =
      dynamic_cast<Gtid_log_event *>(m_first_event.get_event().get());
  return gev->transaction_length;
}

}  // namespace mysql::csa
