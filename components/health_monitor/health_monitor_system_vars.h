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

#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/component_sys_var_service.h>
#include "component_sysvar.h"
#include "health_monitor_monitored_path.h"

namespace hm {

class Sysvar_base {
 public:
  Sysvar_base() = default;
  explicit Sysvar_base(void *val) : val_ptr(val) {}
  const char *name{nullptr};
  const char *desc{nullptr};
  int type{0};
  void *val_ptr{nullptr};
  bool registered{false};
  bool dynamic{false};
  mysql_sys_var_check_func check_func{nullptr};
  mysql_sys_var_update_func update_func{nullptr};
};

template <typename T>
class Sysvar : public Sysvar_base {
 public:
  Sysvar() : Sysvar_base(reinterpret_cast<void *>(&val)) {}
  T val{};
  T min{};
  T max{};
  T def{};
};

using sv_bool = Sysvar<bool>;
using sv_int = Sysvar<int>;
using sv_ulong = Sysvar<uint64_t>;
using sv_char = Sysvar<const char *>;
using sv_double = Sysvar<double>;

class System_vars {
 public:
  System_vars() = default;

  static int initialize_all();
  static int register_all();
  static int unregister_all();

  // Accessors (single source of truth; no static data members)
  static sv_int &status_interval();

  static sv_bool &disk_running();
  static sv_int &disk_sample_interval();
  static sv_int &disk_retention();
  static sv_char &disk_monitored();
  /**
    Read and store the global data directory for automatic path selection.
    This runs during registration. Runtime updates request an asynchronous
    refresh through the collector settings path.
    @param[in] thd THD handle or nullptr for the global value
  */
  static void refresh_disk_monitored_datadir(MYSQL_THD thd);

  /** Request an automatic monitored path refresh after a system variable SET.
   */
  static void request_disk_monitored_datadir_refresh();

  /** Return the generation of the cached automatic monitored path. */
  static uint64_t disk_monitored_generation();

  /**
    Select the effective path monitored by the Disk Collector.
    @param[in] configured_path Configured disk_monitored value
    @param[out] effective_path Configured path or stored data directory
    @param[out] generation Current monitored-path cache generation
    @return true if a usable effective path is available, false otherwise
  */
  static bool select_disk_monitored_path(const std::string &configured_path,
                                         std::string &effective_path,
                                         uint64_t &generation);

  static sv_bool &disk_fallback_enable();
  static sv_bool &disk_fallback_force();
  static sv_int &disk_low_space_duration();
  static sv_ulong &disk_low_space_level();
  static sv_int &disk_low_space_percent();
  static sv_ulong &disk_low_space_critical_level();
  static sv_int &disk_warn_level_1();
  static sv_int &disk_warn_level_2();
  static sv_int &disk_warn_level_3();
  static sv_bool &disk_recovery_enable();
  static sv_int &disk_recovery_level();
  static sv_int &disk_recovery_time_1();
  static sv_int &disk_recovery_time_2();
  static sv_int &disk_recovery_max_cycles();
  static sv_int &disk_recovery_increment();
  static sv_ulong &disk_recovery_window();

  static sv_bool &disk_forecast_running();
  static sv_int &disk_forecast_sample_interval();
  static sv_int &disk_forecast_report_interval();
  static sv_ulong &disk_forecast_run_mode();
  static sv_char &disk_forecast_test_filename();

  static sv_bool &memory_running();
  static sv_int &memory_sample_interval();
  static sv_int &memory_retention();
  static sv_ulong &memory_warn_level_1();
  static sv_ulong &memory_warn_level_2();
  static sv_ulong &memory_warn_level_3();

  static sv_bool &memory_reporting();
  static sv_char &memory_report_definition();

  static sv_bool &periodic_reporting();
  static sv_char &periodic_report_definition();

  static sv_bool &system_running();
  static sv_int &system_sample_interval();
  static sv_int &system_retention();
  static sv_bool &system_cpu_stats_history();
  static sv_bool &system_disk_stats_history();
  static sv_bool &system_memory_stats_history();
  static sv_bool &system_process_stats_history();

 private:
  struct Vars {
    // General
    sv_int status_interval;

    // Disk Collector
    sv_bool disk_running;
    sv_int disk_sample_interval;
    sv_int disk_retention;
    sv_char disk_monitored;
    Monitored_path_cache disk_monitored_datadir;
    sv_bool disk_fallback_enable;
    sv_bool disk_fallback_force;
    sv_int disk_low_space_duration;
    sv_ulong disk_low_space_level;
    sv_int disk_low_space_percent;
    sv_ulong disk_low_space_critical_level;
    sv_int disk_warn_level_1;
    sv_int disk_warn_level_2;
    sv_int disk_warn_level_3;
    sv_bool disk_recovery_enable;
    sv_int disk_recovery_level;
    sv_int disk_recovery_time_1;
    sv_int disk_recovery_time_2;
    sv_int disk_recovery_max_cycles;
    sv_int disk_recovery_increment;
    sv_ulong disk_recovery_window;

    // Disk Forecaster
    sv_bool disk_forecast_running;
    sv_int disk_forecast_sample_interval;
    sv_int disk_forecast_report_interval;
    sv_ulong disk_forecast_run_mode;
    sv_char disk_forecast_test_filename;

    // Memory Collector
    sv_bool memory_running;
    sv_int memory_sample_interval;
    sv_int memory_retention;
    sv_ulong memory_warn_level_1;
    sv_ulong memory_warn_level_2;
    sv_ulong memory_warn_level_3;

    // Memory Reporter
    sv_bool memory_reporting;
    sv_char memory_report_definition;

    // Periodic Reporter
    sv_bool periodic_reporting;
    sv_char periodic_report_definition;

    // System Collector
    sv_bool system_running;
    sv_int system_sample_interval;
    sv_int system_retention;
    sv_bool system_cpu_stats_history;
    sv_bool system_disk_stats_history;
    sv_bool system_memory_stats_history;
    sv_bool system_process_stats_history;

    bool registered{false};
  };

  static Vars &vars();  // lazy singleton of all variables
};

}  // namespace hm
