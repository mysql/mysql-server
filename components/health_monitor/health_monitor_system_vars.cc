/* Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#include "health_monitor_system_vars.h"

#include <algorithm>
#include <array>
#include <string>

#include "health_monitor.h"
#include "health_monitor_configuration.h"
#include "health_monitor_disk.h"
#include "health_monitor_disk_forecaster.h"
#include "health_monitor_log.h"
#include "health_monitor_periodic_reporter.h"
#include "health_monitor_query.h"
#include "health_monitor_triggered_reporter.h"
#include "health_monitor_util.h"

namespace hm {

static constexpr auto sysvar_prefix{"health_monitor"};
static constexpr char default_filename[]{""};

// Central storage for all sysvars, constructed on first use.
System_vars::Vars &System_vars::vars() {
  static Vars v;  // initialized on first call
  return v;
}

// clang-format off

// Accessors (General)
sv_int&   System_vars::status_interval() { return vars().status_interval; }

// Accessors (Disk Collector)
sv_bool&  System_vars::disk_running()         { return vars().disk_running; }
sv_int&   System_vars::disk_sample_interval() { return vars().disk_sample_interval; }
sv_int&   System_vars::disk_retention()       { return vars().disk_retention; }
sv_char&  System_vars::disk_monitored()       { return vars().disk_monitored; }

// Read and store the global data directory for automatic path selection.
void System_vars::refresh_disk_monitored_datadir(MYSQL_THD thd) {
  // Use a fixed buffer large enough for the data directory path
  constexpr size_t kDatadirBufferSize = 4096;
  std::array<char, kDatadirBufferSize> buffer{};
  void *value = buffer.data();
  size_t value_length = buffer.size();

  // Read @@GLOBAL.datadir
  const bool read_failed =
      sysvar_reader_srv->get(thd, "GLOBAL", "mysql_server", "datadir", &value,
                             &value_length) != 0 ||
      value == nullptr || value_length == 0 || value_length > buffer.size();
  if (read_failed) {
    vars().disk_monitored_datadir.store(std::nullopt);
    return;
  }
  // Cache the path, copy the exact byte count, don't assume null terminator
  vars().disk_monitored_datadir.store(
      std::string{static_cast<const char *>(value), value_length});
}

void System_vars::request_disk_monitored_datadir_refresh() {
  vars().disk_monitored_datadir.request_refresh();
}

uint64_t System_vars::disk_monitored_generation() {
  return vars().disk_monitored_datadir.generation();
}

// Select the configured path or the stored data directory.
bool System_vars::select_disk_monitored_path(
    const std::string &configured_path, std::string &effective_path,
    uint64_t &generation) {
  auto &cache = vars().disk_monitored_datadir;
  if (configured_path.empty() && cache.begin_refresh()) {
    Result_Set result;
    const bool read_failed =
        Query{"SELECT @@GLOBAL.datadir"}.run(result) != 0 || result.size() < 3 ||
        result[2].empty() || result[2][0].empty();
    if (read_failed) {
      cache.store(std::nullopt);
    } else {
      cache.store(result[2][0]);
    }
  }
  return cache.select(configured_path, effective_path, generation);
}
sv_bool&  System_vars::disk_fallback_enable()           { return vars().disk_fallback_enable; }
sv_bool&  System_vars::disk_fallback_force()            { return vars().disk_fallback_force; }
sv_int&   System_vars::disk_low_space_duration()        { return vars().disk_low_space_duration; }
sv_ulong& System_vars::disk_low_space_level()           { return vars().disk_low_space_level; }
sv_int&   System_vars::disk_low_space_percent()         { return vars().disk_low_space_percent; }
sv_ulong& System_vars::disk_low_space_critical_level()  { return vars().disk_low_space_critical_level; }
sv_int&   System_vars::disk_warn_level_1()              { return vars().disk_warn_level_1; }
sv_int&   System_vars::disk_warn_level_2()              { return vars().disk_warn_level_2; }
sv_int&   System_vars::disk_warn_level_3()              { return vars().disk_warn_level_3; }
sv_bool&  System_vars::disk_recovery_enable()           { return vars().disk_recovery_enable; }
sv_int&   System_vars::disk_recovery_level()            { return vars().disk_recovery_level; }
sv_int&   System_vars::disk_recovery_time_1()           { return vars().disk_recovery_time_1; }
sv_int&   System_vars::disk_recovery_time_2()           { return vars().disk_recovery_time_2; }
sv_int&   System_vars::disk_recovery_max_cycles()       { return vars().disk_recovery_max_cycles; }
sv_int&   System_vars::disk_recovery_increment()        { return vars().disk_recovery_increment; }
sv_ulong& System_vars::disk_recovery_window()           { return vars().disk_recovery_window; }

// Accessors (Disk Forecaster)
sv_bool&  System_vars::disk_forecast_running()          { return vars().disk_forecast_running; }
sv_int&   System_vars::disk_forecast_sample_interval()  { return vars().disk_forecast_sample_interval; }
sv_int&   System_vars::disk_forecast_report_interval()  { return vars().disk_forecast_report_interval; }
sv_ulong& System_vars::disk_forecast_run_mode()         { return vars().disk_forecast_run_mode; }
sv_char&  System_vars::disk_forecast_test_filename()    { return vars().disk_forecast_test_filename; }

// Accessors (Memory Collector)
sv_bool&  System_vars::memory_running()                 { return vars().memory_running; }
sv_int&   System_vars::memory_sample_interval()         { return vars().memory_sample_interval; }
sv_int&   System_vars::memory_retention()               { return vars().memory_retention; }
sv_ulong& System_vars::memory_warn_level_1()            { return vars().memory_warn_level_1; }
sv_ulong& System_vars::memory_warn_level_2()            { return vars().memory_warn_level_2; }
sv_ulong& System_vars::memory_warn_level_3()            { return vars().memory_warn_level_3; }

// Accessors (Memory Reporter)
sv_bool&  System_vars::memory_reporting()               { return vars().memory_reporting; }
sv_char&  System_vars::memory_report_definition()       { return vars().memory_report_definition; }

// Accessors (Periodic Reporter)
sv_bool&  System_vars::periodic_reporting()             { return vars().periodic_reporting; }
sv_char&  System_vars::periodic_report_definition()     { return vars().periodic_report_definition; }

// Accessors (System Collector)
sv_bool&  System_vars::system_running()                 { return vars().system_running; }
sv_int&   System_vars::system_sample_interval()         { return vars().system_sample_interval; }
sv_int&   System_vars::system_retention()               { return vars().system_retention; }
sv_bool&  System_vars::system_cpu_stats_history()       { return vars().system_cpu_stats_history; }
sv_bool&  System_vars::system_disk_stats_history()      { return vars().system_disk_stats_history; }
sv_bool&  System_vars::system_memory_stats_history()    { return vars().system_memory_stats_history; }
sv_bool&  System_vars::system_process_stats_history()   { return vars().system_process_stats_history; }

// clang-format on

//------------------------------------------------------------------------------
// System variable registration
//------------------------------------------------------------------------------
namespace {

/**
  Register one system variable.

  The PERSIST_AS_READ_ONLY flag ensures that persisted or otherwise
  preconfigured system variables are loaded during component initialization,
  before the Health Monitor can react to its environment.
*/
int register_system_var(Sysvar_base &sysvar, void *args) {
  if (sysvar_register_srv->register_variable(
          sysvar_prefix, sysvar.name,
          (sysvar.type | PLUGIN_VAR_PERSIST_AS_READ_ONLY), sysvar.desc,
          sysvar.check_func, sysvar.update_func, args, sysvar.val_ptr) != 0) {
    log_error("Health Monitor: Failed to register system variable '%s'",
              sysvar.name);
    return 1;
  }
  sysvar.registered = true;
  return 0;
}

/** Unregister one system variable. */
int unregister_system_var(Sysvar_base &sysvar) {
  if (!sysvar.registered) return 0;

  if (sysvar_unregister_srv->unregister_variable(sysvar_prefix, sysvar.name) !=
      0) {
    log_error("Health Monitor: Failed to unregister system variable '%s'",
              sysvar.name);
    return 1;
  }
  sysvar.registered = false;
  return 0;
}

}  // namespace

//------------------------------------------------------------------------------
// Registration callbacks
//------------------------------------------------------------------------------
namespace {
using Check_FN = int(MYSQL_THD, SYS_VAR *, void *, struct st_mysql_value *);

Check_FN check_status_interval;
Check_FN check_disk_sample_interval;
Check_FN check_disk_retention;
Check_FN check_disk_monitored;
Check_FN check_disk_low_space_duration;
Check_FN check_disk_low_space_level;
Check_FN check_disk_low_space_percent;
Check_FN check_disk_low_space_critical_level;
Check_FN check_disk_warning_level;
Check_FN check_disk_recovery_level;
Check_FN check_disk_recovery_time;
Check_FN check_disk_recovery_max_cycles;
Check_FN check_disk_recovery_increment;
Check_FN check_disk_recovery_window;
Check_FN check_memory_sample_interval;
Check_FN check_memory_retention;
Check_FN check_memory_warning_level;
Check_FN check_report_definition;
Check_FN check_system_sample_interval;
Check_FN check_system_retention;

void update_bool_var(MYSQL_THD, SYS_VAR *, void *val_ptr, const void *checked);
void update_integer_var(MYSQL_THD, SYS_VAR *, void *val_ptr,
                        const void *checked);
void update_ullong_var(MYSQL_THD, SYS_VAR *, void *val_ptr,
                       const void *checked);
void update_string_var(MYSQL_THD, SYS_VAR *, void *val_ptr,
                       const void *checked);
}  // namespace

// clang-format off

//------------------------------------------------------------------------------
// Initialize metadata (names, types, defaults, callbacks)
//------------------------------------------------------------------------------
int System_vars::initialize_all() {
  // General
  status_interval().name = "status_interval";
  status_interval().desc = "Health Monitor status message interval, in multiples of the sample interval";
  status_interval().type = PLUGIN_VAR_INT;
  status_interval().min  = 0;
  status_interval().max  = 1000;
  status_interval().def  = 10;
  status_interval().check_func  = check_status_interval;
  status_interval().update_func = update_integer_var;

  // Disk Collector
  disk_running().name = "disk_running";
  disk_running().desc = "Health Monitor is active and collecting storage data";
  disk_running().type = PLUGIN_VAR_BOOL;
  disk_running().def  = true;
  disk_running().update_func = update_bool_var;

  disk_sample_interval().name = "disk_sample_rate";
  disk_sample_interval().desc = "Health Monitor storage sample collection interval, in seconds";
  disk_sample_interval().type = PLUGIN_VAR_INT;
  disk_sample_interval().min  = 1;
  disk_sample_interval().max  = seconds_per_day;
  disk_sample_interval().def  = seconds_per_minute;
  disk_sample_interval().check_func  = check_disk_sample_interval;
  disk_sample_interval().update_func = update_integer_var;

  disk_retention().name = "disk_retention";
  disk_retention().desc = "Health Monitor how long samples are preserved, in seconds";
  disk_retention().type = PLUGIN_VAR_INT;
  disk_retention().min  = 1;
  disk_retention().max  = seconds_per_day * 10;
  disk_retention().def  = seconds_per_day;
  disk_retention().check_func  = check_disk_retention;
  disk_retention().update_func = update_integer_var;

  disk_monitored().name = "disk_monitored";
  disk_monitored().desc = "Path on the filesystem to monitor. Empty selects the data directory filesystem.";
  disk_monitored().type = PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC;
  disk_monitored().def  = configuration::k_disk_monitored_default;
  disk_monitored().check_func  = check_disk_monitored;
  disk_monitored().update_func = update_string_var;

  disk_fallback_enable().name = "disk_fallback_enable";
  disk_fallback_enable().desc = "Enable low disk space fencing.";
  disk_fallback_enable().type = PLUGIN_VAR_BOOL;
  disk_fallback_enable().def  = configuration::k_disk_fallback_enable_default;
  disk_fallback_enable().update_func = update_bool_var;

  disk_fallback_force().name = "disk_fallback_force";
  disk_fallback_force().desc =
      "When initiating low disk space fallback mode, the Health Monitor will"
      "terminate foreground sessions until SUPER_READ_ONLY mode can be set.";
  disk_fallback_force().type = PLUGIN_VAR_BOOL;
  disk_fallback_force().def  = false;
  disk_fallback_force().update_func = update_bool_var;

  disk_low_space_duration().name = "disk_low_space_duration";
  disk_low_space_duration().desc =
      "Health Monitor low disk space duration, in seconds. The amount of time "
      "that available disk space must remain below the lesser of either "
      "disk_low_space_level or disk_low_space_percent before the Health "
      "Monitor puts the server into super-read-only mode.";
  disk_low_space_duration().type = PLUGIN_VAR_INT;
  disk_low_space_duration().min  = 1;
  disk_low_space_duration().max  = seconds_per_day;
  disk_low_space_duration().def  = seconds_per_minute * 5;
  disk_low_space_duration().check_func  = check_disk_low_space_duration;
  disk_low_space_duration().update_func = update_integer_var;

  disk_low_space_level().name = "disk_low_space_level";
  disk_low_space_level().desc = "Health Monitor low disk space threshold (reserve), in MiB.";
  disk_low_space_level().type = PLUGIN_VAR_LONGLONG | PLUGIN_VAR_UNSIGNED;
  disk_low_space_level().min  = 0;
  disk_low_space_level().max  = max_uint64_t - 1;
  disk_low_space_level().def  = 5000;  // MiB
  disk_low_space_level().check_func  = check_disk_low_space_level;
  disk_low_space_level().update_func = update_ullong_var;

  disk_low_space_percent().name = "disk_low_space_percent";
  disk_low_space_percent().desc =
      "Health Monitor low disk space threshold (reserve) as a percent of total allocated space.";
  disk_low_space_percent().type = PLUGIN_VAR_INT;
  disk_low_space_percent().min  = 0;
  disk_low_space_percent().max  = 100;
  disk_low_space_percent().def  = 4;
  disk_low_space_percent().check_func  = check_disk_low_space_percent;
  disk_low_space_percent().update_func = update_integer_var;

  disk_low_space_critical_level().name = "disk_low_space_critical_level";
  disk_low_space_critical_level().desc = "Health Monitor critical low disk space threshold, in MiB.";
  disk_low_space_critical_level().type = PLUGIN_VAR_LONGLONG | PLUGIN_VAR_UNSIGNED;
  disk_low_space_critical_level().min  = 0;
  disk_low_space_critical_level().max  = max_uint64_t - 1;
  disk_low_space_critical_level().def  = 2000;  // MiB
  disk_low_space_critical_level().check_func  = check_disk_low_space_critical_level;
  disk_low_space_critical_level().update_func = update_ullong_var;

  disk_warn_level_1().name = "disk_usage_warning_level_1";
  disk_warn_level_1().desc = "Disk usage warning level 1, percent above reserve threshold.";
  disk_warn_level_1().type = PLUGIN_VAR_INT;
  disk_warn_level_1().min  = 0;
  disk_warn_level_1().max  = 100;
  disk_warn_level_1().def  = 20;
  disk_warn_level_1().check_func  = check_disk_warning_level;
  disk_warn_level_1().update_func = update_integer_var;

  disk_warn_level_2().name = "disk_usage_warning_level_2";
  disk_warn_level_2().desc = "Disk usage warning level 2.";
  disk_warn_level_2().type = PLUGIN_VAR_INT;
  disk_warn_level_2().min  = 0;
  disk_warn_level_2().max  = 100;
  disk_warn_level_2().def  = 10;
  disk_warn_level_2().check_func  = check_disk_warning_level;
  disk_warn_level_2().update_func = update_integer_var;

  disk_warn_level_3().name = "disk_usage_warning_level_3";
  disk_warn_level_3().desc = "Disk usage warning level 3.";
  disk_warn_level_3().type = PLUGIN_VAR_INT;
  disk_warn_level_3().min  = 0;
  disk_warn_level_3().max  = 100;
  disk_warn_level_3().def  = 5;
  disk_warn_level_3().check_func  = check_disk_warning_level;
  disk_warn_level_3().update_func = update_integer_var;

  disk_recovery_enable().name = "disk_recovery_enable";
  disk_recovery_enable().desc = "Enable/disable restoring normal operating mode.";
  disk_recovery_enable().type = PLUGIN_VAR_BOOL;
  disk_recovery_enable().def  = true;
  disk_recovery_enable().update_func = update_bool_var;

  disk_recovery_level().name = "disk_recovery_level";
  disk_recovery_level().desc =
      "Recovery threshold, percent above reserve. Must be reached to restore normal mode.";
  disk_recovery_level().type = PLUGIN_VAR_INT;
  disk_recovery_level().min  = 0;
  disk_recovery_level().max  = 100;
  disk_recovery_level().def  = disk_warn_level_3().def;
  disk_recovery_level().check_func  = check_disk_recovery_level;
  disk_recovery_level().update_func = update_integer_var;

  disk_recovery_time_1().name = "disk_recovery_time_1";
  disk_recovery_time_1().desc =
      "Time above recovery level before OFFLINE_MODE=OFF (seconds).";
  disk_recovery_time_1().type = PLUGIN_VAR_INT;
  disk_recovery_time_1().min  = 1;
  disk_recovery_time_1().max  = seconds_per_day;
  disk_recovery_time_1().def  = seconds_per_minute * 5;
  disk_recovery_time_1().check_func  = check_disk_recovery_time;
  disk_recovery_time_1().update_func = update_integer_var;

  disk_recovery_time_2().name = "disk_recovery_time_2";
  disk_recovery_time_2().desc =
      "Time above recovery level before SUPER_READ_ONLY=OFF (seconds).";
  disk_recovery_time_2().type = PLUGIN_VAR_INT;
  disk_recovery_time_2().min  = 1;
  disk_recovery_time_2().max  = seconds_per_day;
  disk_recovery_time_2().def  = seconds_per_minute * 5;
  disk_recovery_time_2().check_func  = check_disk_recovery_time;
  disk_recovery_time_2().update_func = update_integer_var;

  disk_recovery_max_cycles().name = "disk_recovery_max_cycles";
  disk_recovery_max_cycles().desc = "Maximum number of recovery cycles.";
  disk_recovery_max_cycles().type = PLUGIN_VAR_INT;
  disk_recovery_max_cycles().min  = 0;
  disk_recovery_max_cycles().max  = max_int;
  disk_recovery_max_cycles().def  = 3;
  disk_recovery_max_cycles().check_func  = check_disk_recovery_max_cycles;
  disk_recovery_max_cycles().update_func = update_integer_var;

  disk_recovery_increment().name = "disk_recovery_increment";
  disk_recovery_increment().desc = "Recovery time increment per cycle (seconds).";
  disk_recovery_increment().type = PLUGIN_VAR_INT;
  disk_recovery_increment().min  = 0;
  disk_recovery_increment().max  = seconds_per_day;
  disk_recovery_increment().def  = seconds_per_minute * 5;
  disk_recovery_increment().check_func  = check_disk_recovery_increment;
  disk_recovery_increment().update_func = update_integer_var;

  disk_recovery_window().name = "disk_recovery_window";
  disk_recovery_window().desc =
      "Recovery window (seconds) after which cycle count resets.";
  disk_recovery_window().type = PLUGIN_VAR_LONGLONG | PLUGIN_VAR_UNSIGNED;
  disk_recovery_window().min  = 0;
  disk_recovery_window().max  = max_uint64_t - 1;
  disk_recovery_window().def  = seconds_per_day;
  disk_recovery_window().check_func  = check_disk_recovery_window;
  disk_recovery_window().update_func = update_ullong_var;

  // Disk Forecaster
  disk_forecast_running().name = "disk_forecast_running";
  disk_forecast_running().desc = "Disk Forecaster is active and collecting disk metrics";
  disk_forecast_running().type = PLUGIN_VAR_BOOL;
  disk_forecast_running().def  = true;
  disk_forecast_running().update_func = update_bool_var;

  disk_forecast_sample_interval().name = "disk_forecast_sample_interval";
  disk_forecast_sample_interval().desc = "Disk Forecaster metric collection interval (seconds)";
  disk_forecast_sample_interval().type = PLUGIN_VAR_INT;
  disk_forecast_sample_interval().min  = 10;
  disk_forecast_sample_interval().max  = seconds_per_day;
  disk_forecast_sample_interval().def  = Disk_Forecaster::default_sample_interval();
  disk_forecast_sample_interval().update_func = update_integer_var;

  disk_forecast_report_interval().name = "disk_forecast_report_interval";
  disk_forecast_report_interval().desc = "Disk Forecaster reporting interval (minutes)";
  disk_forecast_report_interval().type = PLUGIN_VAR_INT;
  disk_forecast_report_interval().min  = 1;
  disk_forecast_report_interval().max  = minutes_per_day * 7;
  disk_forecast_report_interval().def  = Disk_Forecaster::default_report_interval() / 60;
  disk_forecast_report_interval().update_func = update_integer_var;

  disk_forecast_run_mode().name = "disk_forecast_run_mode";
  disk_forecast_run_mode().desc = "Disk Forecaster operational mode: NORMAL, TEST, STANDBY, SIMULATION";
  disk_forecast_run_mode().type = PLUGIN_VAR_ENUM;
  disk_forecast_run_mode().def  = static_cast<unsigned long>(hm::Disk_Forecaster::Dfx_Run_Mode::NORMAL);
  disk_forecast_run_mode().dynamic = true;
  disk_forecast_run_mode().update_func = update_ullong_var;

  disk_forecast_test_filename().name = "disk_forecast_test_filename";
  disk_forecast_test_filename().desc = "File name of the Disk Forecaster test data file";
  disk_forecast_test_filename().type = PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC;
  disk_forecast_test_filename().def  = default_filename;
  disk_forecast_test_filename().dynamic = true;
  disk_forecast_test_filename().update_func = update_string_var;

  // Memory Collector
  memory_running().name = "memory_running";
  memory_running().desc = "Health Monitor is active and collecting memory data";
  memory_running().type = PLUGIN_VAR_BOOL;
  memory_running().def  = true;
  memory_running().update_func = update_bool_var;

  memory_sample_interval().name = "memory_sample_rate";
  memory_sample_interval().desc = "Health Monitor memory sample collection interval (seconds)";
  memory_sample_interval().type = PLUGIN_VAR_INT;
  memory_sample_interval().min  = 1;
  memory_sample_interval().max  = seconds_per_day;
  memory_sample_interval().def  = seconds_per_minute;
  memory_sample_interval().check_func  = check_memory_sample_interval;
  memory_sample_interval().update_func = update_integer_var;

  memory_retention().name = "memory_retention";
  memory_retention().desc = "How long memory samples are preserved (seconds)";
  memory_retention().type = PLUGIN_VAR_INT;
  memory_retention().min  = 1;
  memory_retention().max  = seconds_per_day * 10;
  memory_retention().def  = seconds_per_day;
  memory_retention().check_func  = check_memory_retention;
  memory_retention().update_func = update_integer_var;

  memory_warn_level_1().name = "memory_usage_warning_level_1";
  memory_warn_level_1().desc = "Memory warning level 1 (MiB)";
  memory_warn_level_1().type = PLUGIN_VAR_LONGLONG | PLUGIN_VAR_UNSIGNED;
  memory_warn_level_1().min  = 0;
  memory_warn_level_1().max  = max_uint64_t - 1;
  memory_warn_level_1().def  = 1024;
  memory_warn_level_1().check_func  = check_memory_warning_level;
  memory_warn_level_1().update_func = update_ullong_var;

  memory_warn_level_2().name = "memory_usage_warning_level_2";
  memory_warn_level_2().desc = "Memory warning level 2 (MiB)";
  memory_warn_level_2().type = PLUGIN_VAR_LONGLONG | PLUGIN_VAR_UNSIGNED;
  memory_warn_level_2().min  = 0;
  memory_warn_level_2().max  = max_uint64_t - 1;
  memory_warn_level_2().def  = 500;
  memory_warn_level_2().check_func  = check_memory_warning_level;
  memory_warn_level_2().update_func = update_ullong_var;

  memory_warn_level_3().name = "memory_usage_warning_level_3";
  memory_warn_level_3().desc = "Memory warning level 3 (MiB)";
  memory_warn_level_3().type = PLUGIN_VAR_LONGLONG | PLUGIN_VAR_UNSIGNED;
  memory_warn_level_3().min  = 0;
  memory_warn_level_3().max  = max_uint64_t - 1;
  memory_warn_level_3().def  = 100;
  memory_warn_level_3().check_func  = check_memory_warning_level;
  memory_warn_level_3().update_func = update_ullong_var;

  // Memory Reporter
  memory_reporting().name = "memory_reporting";
  memory_reporting().desc = "Health Monitor is reporting memory data when active";
  memory_reporting().type = PLUGIN_VAR_BOOL;
  memory_reporting().def  = true;
  memory_reporting().update_func = update_bool_var;

  memory_report_definition().name = "memory_report_definition";
  memory_report_definition().desc = "Health Monitor memory reports";
  memory_report_definition().type = PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC;
  memory_report_definition().def  = Triggered_Reporter::default_memory_report;
  memory_report_definition().check_func  = check_report_definition;
  memory_report_definition().update_func = update_string_var;

  // Periodic Reporter
  periodic_reporting().name = "periodic_reporting";
  periodic_reporting().desc = "Health Monitor is reporting periodically";
  periodic_reporting().type = PLUGIN_VAR_BOOL;
  periodic_reporting().def  = false;
  periodic_reporting().update_func = update_bool_var;

  periodic_report_definition().name = "periodic_report_definition";
  periodic_report_definition().desc = "Health Monitor periodic reports";
  periodic_report_definition().type = PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC;
  periodic_report_definition().def  = Periodic_Reporter::default_periodic_report;
  periodic_report_definition().check_func  = check_report_definition;
  periodic_report_definition().update_func = update_string_var;

  // System Collector
  system_running().name = "system_running";
  system_running().desc = "Health Monitor is active and collecting system statistics";
  system_running().type = PLUGIN_VAR_BOOL;
  system_running().def  = true;
  system_running().update_func = update_bool_var;

  system_sample_interval().name = "system_sample_rate";
  system_sample_interval().desc = "System stats collection interval (seconds)";
  system_sample_interval().type = PLUGIN_VAR_INT;
  system_sample_interval().min  = 1;
  system_sample_interval().max  = seconds_per_day;
  system_sample_interval().def  = seconds_per_minute;
  system_sample_interval().check_func  = check_system_sample_interval;
  system_sample_interval().update_func = update_integer_var;

  system_retention().name = "system_retention";
  system_retention().desc = "How long system stat samples are preserved (seconds)";
  system_retention().type = PLUGIN_VAR_INT;
  system_retention().min  = 1;
  system_retention().max  = seconds_per_day * 10;
  system_retention().def  = seconds_per_day;
  system_retention().check_func  = check_system_retention;
  system_retention().update_func = update_integer_var;

  system_cpu_stats_history().name = "system_cpu_stats_history";
  system_cpu_stats_history().desc = "CPU stats history enabled";
  system_cpu_stats_history().type = PLUGIN_VAR_BOOL;
  system_cpu_stats_history().def  = true;
  system_cpu_stats_history().update_func = update_bool_var;

  system_disk_stats_history().name = "system_disk_stats_history";
  system_disk_stats_history().desc = "Disk stats history enabled";
  system_disk_stats_history().type = PLUGIN_VAR_BOOL;
  system_disk_stats_history().def  = true;
  system_disk_stats_history().update_func = update_bool_var;

  system_memory_stats_history().name = "system_memory_stats_history";
  system_memory_stats_history().desc = "Memory stats history enabled";
  system_memory_stats_history().type = PLUGIN_VAR_BOOL;
  system_memory_stats_history().def  = true;
  system_memory_stats_history().update_func = update_bool_var;

  system_process_stats_history().name = "system_process_stats_history";
  system_process_stats_history().desc = "MySQL server process stats history enabled";
  system_process_stats_history().type = PLUGIN_VAR_BOOL;
  system_process_stats_history().def  = true;
  system_process_stats_history().update_func = update_bool_var;

  vars().registered = false;
  return 0;
}

// clang-format on

//------------------------------------------------------------------------------
// Register all / unregister all called during init/deinit.
//------------------------------------------------------------------------------
// NOLINTBEGIN(readability-braces-around-statements)
int System_vars::register_all() {
  BOOL_CHECK_ARG(bool) bool_arg{false};
  ENUM_CHECK_ARG(enum) enum_arg;
  INTEGRAL_CHECK_ARG(int) int_arg{0, 0, 0, 0};
  INTEGRAL_CHECK_ARG(uint64_t) uint_arg{0, 0, 0, 0};
  STR_CHECK_ARG(str) str_arg{nullptr};
  // General
  int_arg.def_val = status_interval().def;
  int_arg.min_val = status_interval().min;
  int_arg.max_val = status_interval().max;
  if (register_system_var(status_interval(), &int_arg)) return 1;

  // Disk Collector
  bool_arg.def_val = disk_running().def;
  if (register_system_var(disk_running(), &bool_arg)) return 1;

  int_arg.def_val = disk_sample_interval().def;
  int_arg.min_val = disk_sample_interval().min;
  int_arg.max_val = disk_sample_interval().max;
  if (register_system_var(disk_sample_interval(), &int_arg)) return 1;

  int_arg.def_val = disk_retention().def;
  int_arg.min_val = disk_retention().min;
  int_arg.max_val = disk_retention().max;
  if (register_system_var(disk_retention(), &int_arg)) return 1;

  str_arg.def_val = const_cast<char *>(disk_monitored().def);
  if (register_system_var(disk_monitored(), &str_arg)) return 1;
  if (disk_monitored().val == nullptr || disk_monitored().val[0] == '\0') {
    refresh_disk_monitored_datadir(nullptr);
  }

  bool_arg.def_val = disk_fallback_enable().def;
  if (register_system_var(disk_fallback_enable(), &bool_arg)) return 1;

  bool_arg.def_val = disk_fallback_force().def;
  if (register_system_var(disk_fallback_force(), &bool_arg)) return 1;

  int_arg.def_val = disk_low_space_duration().def;
  int_arg.min_val = disk_low_space_duration().min;
  int_arg.max_val = disk_low_space_duration().max;
  if (register_system_var(disk_low_space_duration(), &int_arg)) return 1;

  uint_arg.def_val = disk_low_space_level().def;
  uint_arg.min_val = disk_low_space_level().min;
  uint_arg.max_val = disk_low_space_level().max;
  if (register_system_var(disk_low_space_level(), &uint_arg)) return 1;

  int_arg.def_val = disk_low_space_percent().def;
  int_arg.min_val = disk_low_space_percent().min;
  int_arg.max_val = disk_low_space_percent().max;
  if (register_system_var(disk_low_space_percent(), &int_arg)) return 1;

  uint_arg.def_val = disk_low_space_critical_level().def;
  uint_arg.min_val = disk_low_space_critical_level().min;
  uint_arg.max_val = disk_low_space_critical_level().max;
  if (register_system_var(disk_low_space_critical_level(), &uint_arg)) return 1;

  int_arg.def_val = disk_warn_level_1().def;
  int_arg.min_val = disk_warn_level_1().min;
  int_arg.max_val = disk_warn_level_1().max;
  if (register_system_var(disk_warn_level_1(), &int_arg)) return 1;

  int_arg.def_val = disk_warn_level_2().def;
  if (register_system_var(disk_warn_level_2(), &int_arg)) return 1;

  int_arg.def_val = disk_warn_level_3().def;
  if (register_system_var(disk_warn_level_3(), &int_arg)) return 1;

  bool_arg.def_val = disk_recovery_enable().def;
  if (register_system_var(disk_recovery_enable(), &bool_arg)) return 1;

  int_arg.def_val = disk_recovery_level().def;
  int_arg.min_val = disk_recovery_level().min;
  int_arg.max_val = disk_recovery_level().max;
  if (register_system_var(disk_recovery_level(), &int_arg)) return 1;

  int_arg.def_val = disk_recovery_time_1().def;
  int_arg.min_val = disk_recovery_time_1().min;
  int_arg.max_val = disk_recovery_time_1().max;
  if (register_system_var(disk_recovery_time_1(), &int_arg)) return 1;

  int_arg.def_val = disk_recovery_time_2().def;
  int_arg.min_val = disk_recovery_time_2().min;
  int_arg.max_val = disk_recovery_time_2().max;
  if (register_system_var(disk_recovery_time_2(), &int_arg)) return 2;

  int_arg.def_val = disk_recovery_max_cycles().def;
  int_arg.min_val = disk_recovery_max_cycles().min;
  int_arg.max_val = disk_recovery_max_cycles().max;
  if (register_system_var(disk_recovery_max_cycles(), &int_arg)) return 1;

  int_arg.def_val = disk_recovery_increment().def;
  int_arg.min_val = disk_recovery_increment().min;
  int_arg.max_val = disk_recovery_increment().max;
  if (register_system_var(disk_recovery_increment(), &int_arg)) return 1;

  uint_arg.def_val = disk_recovery_window().def;
  uint_arg.min_val = disk_recovery_window().min;
  uint_arg.max_val = disk_recovery_window().max;
  if (register_system_var(disk_recovery_window(), &uint_arg)) return 1;

  // Disk Forecaster
  bool_arg.def_val = disk_forecast_running().def;
  if (register_system_var(disk_forecast_running(), &bool_arg)) return 1;

  int_arg.def_val = disk_forecast_sample_interval().def;
  int_arg.min_val = disk_forecast_sample_interval().min;
  int_arg.max_val = disk_forecast_sample_interval().max;
  if (register_system_var(disk_forecast_sample_interval(), &int_arg)) return 1;

  int_arg.def_val = disk_forecast_report_interval().def;
  int_arg.min_val = disk_forecast_report_interval().min;
  int_arg.max_val = disk_forecast_report_interval().max;
  if (register_system_var(disk_forecast_report_interval(), &int_arg)) return 1;

  // Typelib for enum (NOTE: C-designated initializer is a GNU extension)
  static TYPE_LIB run_mode_typelib_t{
      .count = 4,
      .name = "run_mode_typelib_t",
      .type_names = hm::Disk_Forecaster::dfx_run_mode_names,
      .type_lengths = nullptr};
  enum_arg.def_val = disk_forecast_run_mode().def;
  enum_arg.typelib = &run_mode_typelib_t;
  if (register_system_var(disk_forecast_run_mode(), &enum_arg)) return 1;

  str_arg.def_val = const_cast<char *>(disk_forecast_test_filename().def);
  if (register_system_var(disk_forecast_test_filename(), &str_arg)) return 1;

  // Memory Collector
  bool_arg.def_val = memory_running().def;
  if (register_system_var(memory_running(), &bool_arg)) return 1;

  int_arg.def_val = memory_sample_interval().def;
  int_arg.min_val = memory_sample_interval().min;
  int_arg.max_val = memory_sample_interval().max;
  if (register_system_var(memory_sample_interval(), &int_arg)) return 1;

  int_arg.def_val = memory_retention().def;
  int_arg.min_val = memory_retention().min;
  int_arg.max_val = memory_retention().max;
  if (register_system_var(memory_retention(), &int_arg)) return 1;

  uint_arg.def_val = memory_warn_level_1().def;
  uint_arg.min_val = memory_warn_level_1().min;
  uint_arg.max_val = memory_warn_level_1().max;
  if (register_system_var(memory_warn_level_1(), &uint_arg)) return 1;

  uint_arg.def_val = memory_warn_level_2().def;
  if (register_system_var(memory_warn_level_2(), &uint_arg)) return 1;

  uint_arg.def_val = memory_warn_level_3().def;
  if (register_system_var(memory_warn_level_3(), &uint_arg)) return 1;

  // Memory Reporter
  bool_arg.def_val = memory_reporting().def;
  if (register_system_var(memory_reporting(), &bool_arg)) return 1;

  str_arg.def_val = const_cast<char *>(memory_report_definition().def);
  if (register_system_var(memory_report_definition(), &str_arg)) return 1;

  // Periodic Reporter
  bool_arg.def_val = periodic_reporting().def;
  if (register_system_var(periodic_reporting(), &bool_arg)) return 1;

  str_arg.def_val = const_cast<char *>(periodic_report_definition().def);
  if (register_system_var(periodic_report_definition(), &str_arg)) return 1;

  // System Collector
  bool_arg.def_val = system_running().def;
  if (register_system_var(system_running(), &bool_arg)) return 1;

  int_arg.def_val = system_sample_interval().def;
  int_arg.min_val = system_sample_interval().min;
  int_arg.max_val = system_sample_interval().max;
  if (register_system_var(system_sample_interval(), &int_arg)) return 1;

  int_arg.def_val = system_retention().def;
  int_arg.min_val = system_retention().min;
  int_arg.max_val = system_retention().max;
  if (register_system_var(system_retention(), &int_arg)) return 1;

  bool_arg.def_val = system_cpu_stats_history().def;
  if (register_system_var(system_cpu_stats_history(), &bool_arg)) return 1;

  bool_arg.def_val = system_disk_stats_history().def;
  if (register_system_var(system_disk_stats_history(), &bool_arg)) return 1;

  bool_arg.def_val = system_memory_stats_history().def;
  if (register_system_var(system_memory_stats_history(), &bool_arg)) return 1;

  bool_arg.def_val = system_process_stats_history().def;
  if (register_system_var(system_process_stats_history(), &bool_arg)) return 1;

  vars().registered = true;
  return 0;
}
// NOLINTEND(readability-braces-around-statements)

int System_vars::unregister_all() {
  // General
  unregister_system_var(status_interval());

  // Disk Collector
  unregister_system_var(disk_running());
  unregister_system_var(disk_sample_interval());
  unregister_system_var(disk_retention());
  unregister_system_var(disk_monitored());
  unregister_system_var(disk_fallback_enable());
  unregister_system_var(disk_fallback_force());
  unregister_system_var(disk_low_space_duration());
  unregister_system_var(disk_low_space_level());
  unregister_system_var(disk_low_space_percent());
  unregister_system_var(disk_low_space_critical_level());
  unregister_system_var(disk_warn_level_1());
  unregister_system_var(disk_warn_level_2());
  unregister_system_var(disk_warn_level_3());
  unregister_system_var(disk_recovery_enable());
  unregister_system_var(disk_recovery_level());
  unregister_system_var(disk_recovery_time_1());
  unregister_system_var(disk_recovery_time_2());
  unregister_system_var(disk_recovery_max_cycles());
  unregister_system_var(disk_recovery_increment());
  unregister_system_var(disk_recovery_window());

  // Disk Forecaster
  unregister_system_var(disk_forecast_running());
  unregister_system_var(disk_forecast_sample_interval());
  unregister_system_var(disk_forecast_report_interval());
  unregister_system_var(disk_forecast_run_mode());
  unregister_system_var(disk_forecast_test_filename());

  // Memory Collector
  unregister_system_var(memory_running());
  unregister_system_var(memory_sample_interval());
  unregister_system_var(memory_retention());
  unregister_system_var(memory_warn_level_1());
  unregister_system_var(memory_warn_level_2());
  unregister_system_var(memory_warn_level_3());

  // Memory Reporter
  unregister_system_var(memory_reporting());
  unregister_system_var(memory_report_definition());

  // Periodic Reporter
  unregister_system_var(periodic_reporting());
  unregister_system_var(periodic_report_definition());

  // System Collector
  unregister_system_var(system_running());
  unregister_system_var(system_sample_interval());
  unregister_system_var(system_retention());
  unregister_system_var(system_cpu_stats_history());
  unregister_system_var(system_disk_stats_history());
  unregister_system_var(system_memory_stats_history());
  unregister_system_var(system_process_stats_history());

  vars().registered = false;
  return 0;
}

namespace {
// NOLINTBEGIN(readability-named-parameter)

//------------------------------------------------------------------------------
// Check functions for system variables
//------------------------------------------------------------------------------

/**
  Generic range check for signed integer Sysvar.

  @param[in] sysvar      System variable type
  @param[out] new_value  Target for the new value
  @param[in] value       Proposed new value

  @return 0 if successful, 1 otherwise
*/
int check_range_int(sv_int &sysvar, MYSQL_THD, SYS_VAR *, void *new_value,
                    st_mysql_value *value) {
  long long val{0};
  if (!new_value || !value) {
    return 1;
  }
  if (value->val_int(value, &val) != 0) {
    return 1;
  }
  // Bounds
  if (val < sysvar.min || val > sysvar.max) {
    return 1;
  }
  *static_cast<int *>(new_value) = val;
  return 0;
}

/**
  Unsigned 64-bit variant (val_int still used, guard negatives).
*/
int check_range_u64(sv_ulong &meta, MYSQL_THD, SYS_VAR *, void *new_value,
                    st_mysql_value *value) {
  long long orig{0};
  if (!new_value || !value) {
    return 1;
  }
  if (value->val_int(value, &orig) != 0) {
    return 1;
  }
  // Check if value is negative
  if (!value->is_unsigned(value) && orig < 0) {  // NOLINT
    return 1;
  }
  // Check bounds
  // Note that st_mysql_value stores unsigned integers as signed
  const auto val = static_cast<uint64_t>(orig);
  if (val < meta.min || val > meta.max) {
    return 1;
  }
  *static_cast<uint64_t *>(new_value) = val;
  return 0;
}

/**
  Thin forwarders, each matches the fixed prototype for the service API.
*/
int check_status_interval(MYSQL_THD thd, SYS_VAR *sv, void *new_value,
                          st_mysql_value *value) {
  return check_range_int(System_vars::status_interval(), thd, sv, new_value,
                         value);
}

int check_disk_sample_interval(MYSQL_THD thd, SYS_VAR *sv, void *new_value,
                               st_mysql_value *value) {
  return check_range_int(System_vars::disk_sample_interval(), thd, sv,
                         new_value, value);
}

int check_disk_retention(MYSQL_THD thd, SYS_VAR *sv, void *new_value,
                         st_mysql_value *value) {
  return check_range_int(System_vars::disk_retention(), thd, sv, new_value,
                         value);
}

/** Validate the configured path monitored by the Disk Collector. */
int check_disk_monitored(MYSQL_THD, SYS_VAR *, void *new_value,
                         st_mysql_value *value) {
  if (new_value == nullptr || value == nullptr) return 1;

  int length{0};
  const char *path = value->val_str(value, nullptr, &length);
  if (length < 0 ||
      !configuration::valid_monitored_path(path, static_cast<size_t>(length))) {
    return 1;
  }

  *static_cast<const char **>(new_value) = path;
  return 0;
}

}  // namespace

namespace {

int check_disk_low_space_duration(MYSQL_THD thd, SYS_VAR *sv, void *new_value,
                                  struct st_mysql_value *value) {
  return check_range_int(System_vars::disk_low_space_duration(), thd, sv,
                         new_value, value);
}

int check_disk_low_space_level(MYSQL_THD thd, SYS_VAR *sv, void *new_value,
                               st_mysql_value *value) {
  return check_range_u64(System_vars::disk_low_space_level(), thd, sv,
                         new_value, value);
}

int check_disk_low_space_percent(MYSQL_THD thd, SYS_VAR *sv, void *new_value,
                                 struct st_mysql_value *value) {
  return check_range_int(System_vars::disk_low_space_percent(), thd, sv,
                         new_value, value);
}

int check_disk_low_space_critical_level(MYSQL_THD thd, SYS_VAR *sv,
                                        void *new_value,
                                        struct st_mysql_value *value) {
  return check_range_u64(System_vars::disk_low_space_critical_level(), thd, sv,
                         new_value, value);
}

int check_disk_warning_level(MYSQL_THD thd, SYS_VAR *sv, void *new_value,
                             struct st_mysql_value *value) {
  return check_range_int(System_vars::disk_warn_level_1(), thd, sv, new_value,
                         value);
}

int check_disk_recovery_level(MYSQL_THD thd, SYS_VAR *sv, void *new_value,
                              struct st_mysql_value *value) {
  return check_range_int(System_vars::disk_recovery_level(), thd, sv, new_value,
                         value);
}

int check_disk_recovery_time(MYSQL_THD thd, SYS_VAR *sv, void *new_value,
                             struct st_mysql_value *value) {
  // Recovery time 1 and time 2 have the same min/max
  return check_range_int(System_vars::disk_recovery_time_1(), thd, sv,
                         new_value, value);
}

int check_disk_recovery_max_cycles(MYSQL_THD thd, SYS_VAR *sv, void *new_value,
                                   struct st_mysql_value *value) {
  return check_range_int(System_vars::disk_recovery_max_cycles(), thd, sv,
                         new_value, value);
}

int check_disk_recovery_increment(MYSQL_THD thd, SYS_VAR *sv, void *new_value,
                                  struct st_mysql_value *value) {
  return check_range_int(System_vars::disk_recovery_increment(), thd, sv,
                         new_value, value);
}

/** Check that the disk_recovery_window is valid. */
int check_disk_recovery_window(MYSQL_THD thd, SYS_VAR *sv, void *new_value,
                               struct st_mysql_value *value) {
  return check_range_u64(System_vars::disk_recovery_window(), thd, sv,
                         new_value, value);
}

/** Check that the memory sample interval is valid. */
int check_memory_sample_interval(MYSQL_THD thd, SYS_VAR *sv, void *new_value,
                                 struct st_mysql_value *value) {
  return check_range_int(System_vars::memory_sample_interval(), thd, sv,
                         new_value, value);
}

/** Check that the memory_retention is valid. */
int check_memory_retention(MYSQL_THD thd, SYS_VAR *sv, void *new_value,
                           struct st_mysql_value *value) {
  return check_range_int(System_vars::memory_retention(), thd, sv, new_value,
                         value);
}

/** Check for a valid memory warning level. */
int check_memory_warning_level(MYSQL_THD thd, SYS_VAR *sv, void *new_value,
                               struct st_mysql_value *value) {
  // Warning levels have the same min/max
  return check_range_u64(System_vars::memory_warn_level_1(), thd, sv, new_value,
                         value);
}

int check_report_definition(MYSQL_THD, SYS_VAR *, void *new_value,
                            struct st_mysql_value *value) {
  if (new_value == nullptr || value == nullptr) return 1;
  int length;
  const char *new_def = value->val_str(value, nullptr, &length);
  if (new_def == nullptr || length == 0) return 1;
  assert(new_def[length] == '\0');

  // Validate the new definition.
  if (!Report_Definition_Manager::settings_valid(new_def)) return 1;

  // If valid, assign new value.
  *static_cast<const char **>(new_value) = new_def;
  return 0;
}

int check_system_sample_interval(MYSQL_THD thd, SYS_VAR *sv, void *new_value,
                                 struct st_mysql_value *value) {
  return check_range_int(System_vars::system_sample_interval(), thd, sv,
                         new_value, value);
}

int check_system_retention(MYSQL_THD thd, SYS_VAR *sv, void *new_value,
                           struct st_mysql_value *value) {
  return check_range_int(System_vars::system_retention(), thd, sv, new_value,
                         value);
}

void update_bool_var(MYSQL_THD, SYS_VAR *, void *val_ptr, const void *checked) {
  *(static_cast<bool *>(val_ptr)) = *(static_cast<const bool *>(checked));
  Health_Monitor::system_vars_update();
}

void update_integer_var(MYSQL_THD, SYS_VAR *, void *val_ptr,
                        const void *checked) {
  *(static_cast<int *>(val_ptr)) = *(static_cast<const int *>(checked));
  Health_Monitor::system_vars_update();
}

void update_ullong_var(MYSQL_THD, SYS_VAR *, void *val_ptr,
                       const void *checked) {
  *(static_cast<uint64_t *>(val_ptr)) =
      *(static_cast<const uint64_t *>(checked));
  Health_Monitor::system_vars_update();
}

void update_string_var(MYSQL_THD, SYS_VAR *, void *val_ptr,
                       const void *checked) {
  const char *new_value =
      *static_cast<const char **>(const_cast<void *>(checked));  // NOLINT
  *static_cast<const char **>(val_ptr) = new_value;
  if (val_ptr == System_vars::disk_monitored().val_ptr &&
      (new_value == nullptr || new_value[0] == '\0')) {
    System_vars::request_disk_monitored_datadir_refresh();
  }
  Health_Monitor::system_vars_update();
}

// NOLINTEND(readability-named-parameter)
}  // namespace

}  // namespace hm
