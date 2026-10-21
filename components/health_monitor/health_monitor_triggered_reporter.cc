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

#include "health_monitor_triggered_reporter.h"

#include <algorithm>  // for_each

namespace hm {

const char *Triggered_Reporter::default_memory_report =
    "[{"
    "  \"title\" : \"========== MEMORY DIAGNOSTICS BEGIN ==========\","
    "  \"queries\" : ["
    "      {\"query\" : \"hm_mem_usage_overview\"},"
    "      {\"query\" : \"hm_mem_usage_per_component\"},"
    "      {\"query\" : \"hm_processlist_details_by_current_mem\"},"
    "      {\"query\" : \"hm_statement_stats_by_total_mem\"},"
    "      {\"query\" : \"hm_events_allocating_most_mem\"},"
    "      {\"query\" : \"hm_innodb_buffer_pool_stats\"}"
    "    ], "
    "  \"footer\" : \"========== MEMORY DIAGNOSTICS END ==========\""
    " }]";

Triggered_Reporter::Triggered_Reporter(
    const char *reporter_name, const char *status_name, const char *instr_name,
    const char *os_name, const char *description, sv_bool *sysvar_running,
    sv_char *sysvar_def)
    : Periodic_Reporter(reporter_name, status_name, instr_name, os_name,
                        description, sysvar_running, sysvar_def) {}

/**
  Thread process loop: Wait for signal then run reports. Triggered reports
  are issued by sending a signal.
*/
void Triggered_Reporter::run_process() {
  pre_process();  // TODO: Startup message

  mysql_mutex_lock(&m_wait_lock);
  bool done{m_stop};
  mysql_mutex_unlock(&m_wait_lock);

  while (!done) {
    mysql_mutex_lock(&m_wait_lock);
    done = m_stop;
    if (!done) {
      mysql_cond_wait(&m_wait, &m_wait_lock);
      done = m_stop;
    }
    mysql_mutex_unlock(&m_wait_lock);
    if (!done) process();
  }

  post_process();  // TODO: Shutdown message
}

/**
  Run the set of queries defined by the config settings and print results.
*/
void Triggered_Reporter::issue_report() {
  std::for_each(m_rep_mgr.reports()->begin(), m_rep_mgr.reports()->end(),
                [&](std::pair<const std::string, Report> &r) {
                  Report *p = &r.second;
                  p->issue_report();
                });
}

}  // namespace hm
