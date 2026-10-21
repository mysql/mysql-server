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

#include "health_monitor_periodic_reporter.h"

#include <algorithm>  // for_each
#include <chrono>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>

#include "health_monitor.h"         // log
#include "health_monitor_log.h"     // log_warning()
#include "health_monitor_memory.h"  // m_avail_mb
#include "health_monitor_server.h"  // get_status_variable
#include "health_monitor_supervisor.h"

namespace hm {

// clang-format off

const char *Periodic_Reporter::default_periodic_report =
"[{"
"   \"title\" : \"========== PERIODIC 15min DIAGNOSTICS BEGIN ==========\","
"   \"footer\" : \"========== PERIODIC 15min DIAGNOSTICS END ==========\","
"   \"period\" : 900,"
"   \"queries\" : ["
"      {\"query\" : \"hm_key_global_status\"},"
"      {\"query\" : \"hm_connection_ctrl\"},"
"      {\"query\" : \"hm_connection_host_cache\"},"
"      {\"query\" : \"hm_mem_usage_overview\"},"
"      {\"query\" : \"hm_mem_usage_per_component\"},"
"      {\"query\" : \"hm_tp_group_state\"},"
"      {\"query\" : \"hm_tp_group_stats\"},"
"      {\"query\" : \"hm_tp_thread_state\"},"
"      {\"query\" : \"hm_tp_connections\"}"
"   ]"
"},"
"{"
"   \"title\" : \"========== PERIODIC 60min DIAGNOSTICS BEGIN ==========\","
"   \"footer\" : \"========== PERIODIC 60min DIAGNOSTICS END ==========\","
"   \"period\" : 3600,"
"   \"queries\" : ["
"      {\"query\" : \"hm_tmp_table_space\"},"
"      {\"query\" : \"hm_tmp_table_count\"},"
"      {\"query\" : \"hm_full_table_scans\"},"
"      {\"query\" : \"hm_long_lasting_trx\"},"
"      {\"query\" : \"hm_statement_stats_by_total_mem\"},"
"      {\"query\" : \"hm_error_codes_emitted\"},"
"      {\"query\" : \"hm_innodb_metrics\"},"
"      {\"query\" : \"hm_innodb_buffer_pool_stats\"},"
"      {\"query\" : \"hm_innodb_temp_tablespaces\"},"
"      {\"query\" : \"hm_innodb_temp_table_info\"},"
"      {\"query\" : \"hm_innodb_lock_waits\"},"
"      {\"query\" : \"hm_rpl_group_members\"},"
"      {\"query\" : \"hm_rpl_group_member_stats\"},"
"      {\"query\" : \"hm_rpl_applier_status\"},"
"      {\"query\" : \"hm_rpl_group_communication_information\"},"
"      {\"query\" : \"hm_binlog_compression_stats\"}"
"   ]"
"},"
"{"
"   \"title\" : \"========== PERIODIC 120min DIAGNOSTICS BEGIN ==========\","
"   \"footer\" : \"========== PERIODIC 120min DIAGNOSTICS END ==========\","
"   \"period\" : 7200,"
"   \"queries\" : ["
"      {\"query\" : \"hm_data_volume\"},"
"      {\"query\" : \"hm_data_type_usage_count\"},"
"      {\"query\" : \"hm_basic_global_status\"},"
"      {\"query\" : \"hm_key_usage\"}"
"   ]"
"},"
"{"
"   \"title\" : \"========== PERIODIC 24h DIAGNOSTICS BEGIN ==========\","
"   \"footer\" : \"========== PERIODIC 24h DIAGNOSTICS END ==========\","
"   \"period\" : 86400,"
"   \"queries\" : ["
"      {\"query\" : \"hm_global_variables\"}, "
"      {\"query\" : \"hm_instance_metadata\"}"
"   ]"
"}]";

// clang-format on

Periodic_Reporter::Periodic_Reporter(
    const char *reporter_name, const char *status_name, const char *instr_name,
    const char *os_name, const char *description, sv_bool *sysvar_running,
    sv_char *sysvar_def)
    : Collector(reporter_name, status_name, instr_name, os_name, description),
      m_sysvar_running(sysvar_running),
      m_sysvar_def(sysvar_def) {
  set_params();
}

/**
  Set the timer to 10 sec before starting to process. This means at the first
  round of processing, all reports will be issued after 10 sec, then they will
  be issued according to the defined interval. The initial sample interval is
  set to 10 sec to let the memory collector have enough time to collect its
  data.
 */
void Periodic_Reporter::pre_process() { set_sample_interval(10); }

/**
  Run report only if more than half of the level 3 limit is available. The
  actual report is issued in a virtual method that may be redefined in some
  subclass. This is because the behavior is different, e.g. a periodic report
  must keep track of which reports to issue at which intervals.
*/
void Periodic_Reporter::process() {
  if (!Health_Monitor::available()) return;

  // If the memory collector is running, then check available memory
  if (Health_Monitor::memory_collector()->running()) {
    // If a reading hasn't been taken, don't run the report
    if (Health_Monitor::memory_collector()->m_avail_mb == 0) return;
    // We need 1/2 of the warning level 3 limit (50 MiB) to run the report
    bool const low_memory =
        Health_Monitor::memory_collector()->m_avail_mb <=
        Health_Monitor::memory_collector()->m_warn_level_3_mb / 2;
    // Ignore test values used to provoke warnings
    bool const test_mode =
        Health_Monitor::memory_collector()->m_warn_level_3_mb == k_max_level;
    if (low_memory && !test_mode) {
      log_diag("Diagnostic report unavailable due to low memory");
      return;
    }
  }
  issue_report();

  // If the system variables were updated during report generation, then signal
  // the task supervisor to apply the changes now to avoid a potential deadlock
  // with LOCK_global_system_variables
  if (settings_pending()) {
    Health_Monitor::supervisor()->notify_settings_update();
  }
}

/**
  Periodically report usage of the deprecated features.
  @return Time remaining to issue this report.
*/
int Periodic_Reporter::report_deprecated_usage() {
  // TODO: Remove this when the periodic reporting is in prod.
  deprecation_msg_next = std::max(0, deprecation_msg_next - sample_interval());

  if (deprecation_msg_next == 0) {
    std::string count_val;
    std::string time_val;
    if (!Server::get_status_variable("Deprecated_use_i_s_processlist_count",
                                     count_val) &&
        !Server::get_status_variable(
            "Deprecated_use_i_s_processlist_last_timestamp", time_val) &&
        stol(count_val) != 0) {
      std::stringstream str;
      std::time_t const sec = std::stoll(time_val) / 1000000;
      str << std::put_time(std::gmtime(&sec), "%F %T");
      Health_Monitor::log()->log_message_lu(
          INFORMATION_LEVEL, ER_USAGE_DEPRECATION_COUNTER,
          "information_schema.processlist", count_val.c_str(),
          str.str().c_str());
    }
    // periodically reporting usage of the deprecated foreign key on non
    // standard key(non unique keys and partial columns in indexes)
    if (!Server::get_status_variable(
            "Deprecated_use_fk_on_non_standard_key_count", count_val) &&
        stol(count_val) != 0 &&
        !Server::get_status_variable(
            "Deprecated_use_fk_on_non_standard_key_last_timestamp", time_val)) {
      std::stringstream str;
      std::time_t const sec = std::stoll(time_val) / 1000000;
      str << std::put_time(std::gmtime(&sec), "%F %T");
      Health_Monitor::log()->log_message_lu(
          INFORMATION_LEVEL, ER_USAGE_DEPRECATION_COUNTER,
          "fk_on_non_standard_key", count_val.c_str(), str.str().c_str());
    }
    deprecation_msg_next = DEPRECATION_REPORT_SAMPLE_INTERVAL;
  }
  return deprecation_msg_next;
}

/**
  Process all periodic report definitions, issue those that are due.
  Adjust the time for next emission, and set the sample interval for the
  report manager to wake up again when the next report is to be issued.
*/
void Periodic_Reporter::issue_report() {
  // We have waited for m_sample_interval seconds, so subtract m_sample_interval
  // from the m_next of all periodic reports, and issue reports that have
  // m_next == 0.
  int next_interval = INT_MAX;
  std::for_each(m_rep_mgr.reports()->begin(), m_rep_mgr.reports()->end(),
                [&](std::pair<const std::string, Report> &r) {
                  Report *p = &r.second;
                  if (p->interval == 0) return;
                  // Subtract the last wait time.
                  p->next = std::max(0, p->next - sample_interval());
                  // If due, issue report and set next time for emission.
                  if (p->next == 0) {
                    p->issue_report();
                    p->next = p->interval;
                  }
                  // The next time to wake up the periodic report manager
                  // is the smallest m_next.
                  next_interval = std::min(p->next, next_interval);
                });

  // Issue report for usage of deprecated features.
  next_interval = std::min(report_deprecated_usage(), next_interval);

  // Set the sample interval to the smallest interval.
  set_sample_interval(next_interval);
}

/**
  Periodic Reporter status.
*/
void Periodic_Reporter::log_resource_status() { log_collector_status(true); }

/**
  Periodic_Reporter settings formatted string.
*/
std::string Periodic_Reporter::format_settings() {
  std::stringstream profile;
  profile << "running" << eq << on_off(m_running);
  return profile.str();
}

/**
    Indicate if the Memory Periodic_Reporter settings have changed.
    @return true if settings have changed
 */
bool Periodic_Reporter::settings_changed() {
  m_settings_changed = (m_sysvar_running->val != m_running ||
                        m_rep_mgr.settings_changed(m_sysvar_def->val));
  return m_settings_changed;
}

/**
  Set report parameters from the system variables.
*/
void Periodic_Reporter::set_params() {
  m_running = m_sysvar_running->val;
  m_rep_mgr.update_settings(m_sysvar_def->val);
}

/**
  Return health_monitor reporting variable.
*/
bool Periodic_Reporter::running() const { return m_sysvar_running->val; }

}  // namespace hm
