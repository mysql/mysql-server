/* Copyright (c) 2023, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "health_monitor_supervisor.h"

#include "health_monitor.h"
#include "health_monitor_disk.h"
#include "health_monitor_log.h"
#include "health_monitor_memory.h"
#include "health_monitor_periodic_reporter.h"
#include "health_monitor_system.h"
#include "health_monitor_triggered_reporter.h"

namespace hm {

Supervisor::Supervisor()
    : Collector("Supervisor", "SUPERVISOR", "supervisor", "health_sup",
                "Health Monitor task supervisor") {}

/**
  Record pending settings work and wake Supervisor.
*/
void Supervisor::notify_settings_update() {
  mysql_mutex_lock(&m_wait_lock);
  m_settings_update_pending = true;
  mysql_cond_signal(&m_wait);
  mysql_mutex_unlock(&m_wait_lock);
}

/**
  Thread process loop: Wait for pending work then execute it.
*/
void Supervisor::run_process() {
  pre_process();

  mysql_mutex_lock(&m_wait_lock);
  while (!m_stop) {
    while (!m_stop && !m_settings_update_pending) {
      mysql_cond_wait(&m_wait, &m_wait_lock);
    }
    if (m_stop) break;

    m_settings_update_pending = false;
    mysql_mutex_unlock(&m_wait_lock);
    process();
    mysql_mutex_lock(&m_wait_lock);
  }
  mysql_mutex_unlock(&m_wait_lock);

  post_process();
}

/**
  Execute any pending tasks.
*/
void Supervisor::process() {
  if (!Health_Monitor::available()) return;

  bool processed_settings{false};
  do {
    processed_settings = false;
    const auto process_pending = [&processed_settings](Collector *collector) {
      if (collector->settings_pending()) {
        processed_settings = collector->update_settings() || processed_settings;
      }
    };
    process_pending(Health_Monitor::disk_collector());
    process_pending(Health_Monitor::disk_forecaster());
    process_pending(Health_Monitor::memory_collector());
    process_pending(Health_Monitor::system_collector());
    process_pending(Health_Monitor::periodic_reporter());
    process_pending(Health_Monitor::memory_reporter());
  } while (processed_settings);
}

}  // namespace hm
