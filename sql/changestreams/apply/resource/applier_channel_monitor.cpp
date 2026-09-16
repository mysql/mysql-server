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
// permission to link the derivative works with the separately licensed software
// that they have either included with the program or referenced in the
// documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include "sql/changestreams/apply/resource/applier_channel_monitor.h"

#include "mysql/scheduler/statistics_monitor.h"
#include "sql/changestreams/apply/resource/statistics_map.h"
#include "sql/changestreams/apply/service/csa_channel.h"

namespace mysql::csa {

Applier_channel_monitor::Applier_channel_monitor(Csa_channel &channel)
    : m_channel(channel), m_time_refresh(std::chrono::system_clock::now()) {}

void Applier_channel_monitor::init_monitoring(
    std::chrono::milliseconds refresh_time) {
  m_time_refresh = std::chrono::system_clock::now();
  m_refresh_interval = refresh_time;
  refresh_values();
}

void Applier_channel_monitor::refresh_values() {
  m_previous_applied_events = m_current_applied_events;
  m_previous_clock_value = m_current_clock_value;
  m_previous_commit_clock_value = m_current_commit_clock_value;
  // refresh current values
  auto &stat_monitor =
      scheduler::Statistics_monitor::get(m_channel.channel_instance_id);
  m_current_applied_events =
      stat_monitor.get_stat(Statistics_map::applied_events_cnt).get();
  m_current_clock_value = m_channel.scheduler_clock->now();
  m_current_commit_clock_value = m_channel.commit_order_clock->now();
  m_active_trx = m_channel.scheduler->get_scheduled_tasks_count();
}

void Applier_channel_monitor::check_applier_progress() {
  auto now_time = std::chrono::system_clock::now();
  auto current_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                              now_time - m_time_refresh)
                              .count();
  if (m_allowed_unblocks == 0) {
    // starting work, set allowed number of clock unblocks
    m_allowed_unblocks = (m_channel.session_service->get_session_number());
  }

  if (current_duration >= m_refresh_interval.count()) {
    refresh_values();
    // update time of statistic refresh
    m_time_refresh = now_time;
    // if state has changed (clock updated / new event applied) or there are no
    // active transactions, return true
    if (m_current_unblock_counter) {
      // when unblocking, reset only if event count changed
      if (m_current_applied_events > m_previous_applied_events ||
          m_active_trx == 0) {
        m_current_unblock_counter = 0;
        return;
      }
    } else if (m_active_trx == 0 ||
               m_current_clock_value > m_previous_clock_value ||
               m_current_applied_events > m_previous_applied_events ||
               m_current_commit_clock_value > m_previous_commit_clock_value) {
      m_current_unblock_counter = 0;
      return;
    }
    if (m_current_unblock_counter < m_allowed_unblocks) {
      // applier is stalled, try to unblock by using scheduler interface
      m_channel.scheduler->request_unblock();
      ++m_current_unblock_counter;
      ++m_total_unblock_counter;
    }
  }
}

std::size_t Applier_channel_monitor::get_allowed_unblocks() const {
  return m_allowed_unblocks;
}

std::size_t Applier_channel_monitor::get_current_unblock_counter() const {
  return m_current_unblock_counter;
}

std::size_t Applier_channel_monitor::get_total_unblock_counter() const {
  return m_total_unblock_counter;
}

}  // namespace mysql::csa
