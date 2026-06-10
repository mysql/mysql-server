/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.
*/
#include "resource_manager.h"
#include "resource_manager_stats_collector.h"

#include <mysql/components/services/component_status_var_service.h>
#include <mysql/components/services/component_sys_var_service.h>

extern REQUIRES_SERVICE_PLACEHOLDER_AS(component_sys_variable_register,
                                       sysvar_register_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(component_sys_variable_unregister,
                                       sysvar_unregister_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(status_variable_registration,
                                       status_register_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(component_sys_variable_register,
                                       sysvar_register_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(component_sys_variable_unregister,
                                       sysvar_unregister_srv);
extern std::unique_ptr<gr_resource_manager::Resource_manager_stats_collector>
    stats_collector;

// Stores lag thresholds.
gr_resource_manager::Lag_metadata_thresholds lag_metadata_threshold;

namespace gr_resource_manager {
////////////////////////////////////////////////////////////////////////////////
// Status variables
////////////////////////////////////////////////////////////////////////////////

static int gr_applier_lag(THD *, SHOW_VAR *var, char *buf) {
  var->type = SHOW_INT;
  var->value = buf;
  auto *typed_buf = reinterpret_cast<uint *>(buf);
  uint const value = (stats_collector ? stats_collector->get_applier_lag() : 0);
  *typed_buf = static_cast<uint>(value);
  return 0;
}
static int gr_recovery_lag(THD *, SHOW_VAR *var, char *buf) {
  var->type = SHOW_INT;
  var->value = buf;
  auto *typed_buf = reinterpret_cast<uint *>(buf);
  uint const value =
      (stats_collector ? stats_collector->get_recovery_lag() : 0);
  *typed_buf = static_cast<uint>(value);
  return 0;
}
static int gr_memory_percentage_used(THD *, SHOW_VAR *var, char *buf) {
  var->type = SHOW_INT;
  var->value = buf;
  auto *typed_buf = reinterpret_cast<uint *>(buf);
  uint const value =
      (stats_collector ? stats_collector->get_percentage_used_memory() : 0);
  *typed_buf = static_cast<uint>(value);
  return 0;
}
static int gr_applier_hit(THD *, SHOW_VAR *var, char *buf) {
  var->type = SHOW_INT;
  var->value = buf;
  auto *typed_buf = reinterpret_cast<uint *>(buf);
  uint const value =
      (stats_collector ? stats_collector->get_applier_hit_number_of_times()
                       : 0);
  *typed_buf = static_cast<uint>(value);
  return 0;
}
static int gr_recovery_hit(THD *, SHOW_VAR *var, char *buf) {
  var->type = SHOW_INT;
  var->value = buf;
  auto *typed_buf = reinterpret_cast<uint *>(buf);
  uint const value =
      (stats_collector ? stats_collector->get_recovery_hit_number_of_times()
                       : 0);
  *typed_buf = static_cast<uint>(value);
  return 0;
}
static int gr_memory_hit(THD *, SHOW_VAR *var, char *buf) {
  var->type = SHOW_INT;
  var->value = buf;
  auto *typed_buf = reinterpret_cast<uint *>(buf);
  uint const value =
      (stats_collector ? stats_collector->get_memory_hit_number_of_times() : 0);
  *typed_buf = static_cast<uint>(value);
  return 0;
}

static int gr_applier_timestamp(THD *, SHOW_VAR *var, char *buf) {
  var->type = SHOW_CHAR;
  var->value = buf;
  std::string const value =
      (stats_collector ? stats_collector->get_applier_eviction_timestamp()
                       : "");
  strcpy(buf, value.c_str());
  return 0;
}

static int gr_recovery_timestamp(THD *, SHOW_VAR *var, char *buf) {
  var->type = SHOW_CHAR;
  var->value = buf;
  std::string const value =
      (stats_collector ? stats_collector->get_recovery_eviction_timestamp()
                       : "");
  strcpy(buf, value.c_str());
  return 0;
}

static int gr_memory_timestamp(THD *, SHOW_VAR *var, char *buf) {
  var->type = SHOW_CHAR;
  var->value = buf;
  std::string const value =
      (stats_collector ? stats_collector->get_memory_eviction_timestamp() : "");
  strcpy(buf, value.c_str());
  return 0;
}

static int gr_lag_query_last_error_timestamp(THD *, SHOW_VAR *var, char *buf) {
  var->type = SHOW_CHAR;
  var->value = buf;
  std::string const value =
      (stats_collector ? stats_collector->get_lag_query_last_error_timestamp()
                       : "");
  strcpy(buf, value.c_str());
  return 0;
}

static int gr_memory_last_error_timestamp(THD *, SHOW_VAR *var, char *buf) {
  var->type = SHOW_CHAR;
  var->value = buf;
  std::string const value =
      (stats_collector ? stats_collector->get_memory_last_error_timestamp()
                       : "");
  strcpy(buf, value.c_str());
  return 0;
}

static SHOW_VAR status_func_var[] = {
    {"Gr_resource_manager_applier_channel_lag",
     reinterpret_cast<char *>(gr_applier_lag), SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Gr_resource_manager_recovery_channel_lag",
     reinterpret_cast<char *>(gr_recovery_lag), SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Gr_resource_manager_memory_used",
     reinterpret_cast<char *>(gr_memory_percentage_used), SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},

    {"Gr_resource_manager_applier_channel_threshold_hits",
     reinterpret_cast<char *>(gr_applier_hit), SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Gr_resource_manager_recovery_channel_threshold_hits",
     reinterpret_cast<char *>(gr_recovery_hit), SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Gr_resource_manager_memory_threshold_hits",
     reinterpret_cast<char *>(gr_memory_hit), SHOW_FUNC, SHOW_SCOPE_GLOBAL},

    {"Gr_resource_manager_applier_channel_eviction_timestamp",
     reinterpret_cast<char *>(gr_applier_timestamp), SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Gr_resource_manager_recovery_channel_eviction_timestamp",
     reinterpret_cast<char *>(gr_recovery_timestamp), SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Gr_resource_manager_memory_eviction_timestamp",
     reinterpret_cast<char *>(gr_memory_timestamp), SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},

    {"Gr_resource_manager_channel_lag_monitoring_error_timestamp",
     reinterpret_cast<char *>(gr_lag_query_last_error_timestamp), SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Gr_resource_manager_memory_monitoring_error_timestamp",
     reinterpret_cast<char *>(gr_memory_last_error_timestamp), SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {nullptr, nullptr, SHOW_UNDEF,
     SHOW_SCOPE_UNDEF}  // null terminator required
};

int register_status_variables() {
  return status_register_srv->register_variable(status_func_var);
}

int unregister_status_variables() {
  return status_register_srv->unregister_variable(status_func_var);
}

////////////////////////////////////////////////////////////////////////////////
// System variables
////////////////////////////////////////////////////////////////////////////////

static void update_the_variable_used_memory_limit(MYSQL_THD, SYS_VAR *,
                                                  void *var_ptr,
                                                  const void *save) {
  uint const in_val = *static_cast<const uint *>(save);
  *static_cast<uint *>(var_ptr) = in_val;
  lag_metadata_threshold.used_memory_limit = in_val;
}

static void update_the_variable_quarantine_time(MYSQL_THD, SYS_VAR *,
                                                void *var_ptr,
                                                const void *save) {
  uint const in_val = *static_cast<const uint *>(save);
  *static_cast<uint *>(var_ptr) = in_val;
  lag_metadata_threshold.quarantine_time = in_val;
}

static void update_the_variable_applier_lag(MYSQL_THD, SYS_VAR *, void *var_ptr,
                                            const void *save) {
  uint const in_val = *static_cast<const uint *>(save);
  *static_cast<uint *>(var_ptr) = in_val;
  lag_metadata_threshold.applier_lag_limit_in_seconds = in_val;
}

static void update_the_variable_recovery_lag(MYSQL_THD, SYS_VAR *,
                                             void *var_ptr, const void *save) {
  uint const in_val = *static_cast<const uint *>(save);
  *static_cast<uint *>(var_ptr) = in_val;
  lag_metadata_threshold.recovery_lag_limit_in_seconds = in_val;
}

int register_system_variables() {
  INTEGRAL_CHECK_ARG(uint) sys_variable_limits;
  sys_variable_limits.def_val = 3600;
  sys_variable_limits.min_val = 0;
  sys_variable_limits.max_val = 43200;
  sys_variable_limits.blk_sz = 0;
  if (sysvar_register_srv->register_variable(
          "group_replication_resource_manager", "applier_channel_lag",
          PLUGIN_VAR_INT | PLUGIN_VAR_UNSIGNED,
          "Defines threshold for applier lag.", nullptr,
          update_the_variable_applier_lag, (void *)&sys_variable_limits,
          (void *)&lag_metadata_threshold.applier_lag_limit_in_seconds))
    return 1;
  if (sysvar_register_srv->register_variable(
          "group_replication_resource_manager", "recovery_channel_lag",
          PLUGIN_VAR_INT | PLUGIN_VAR_UNSIGNED,
          "Defines threshold for recovery lag.", nullptr,
          update_the_variable_recovery_lag, (void *)&sys_variable_limits,
          (void *)&lag_metadata_threshold.recovery_lag_limit_in_seconds))
    return 1;
  if (sysvar_register_srv->register_variable(
          "group_replication_resource_manager", "quarantine_time",
          PLUGIN_VAR_INT | PLUGIN_VAR_UNSIGNED,
          "Prevents immediate re-ejection of a member.", nullptr,
          update_the_variable_quarantine_time, (void *)&sys_variable_limits,
          (void *)&lag_metadata_threshold.quarantine_time))
    return 1;

  sys_variable_limits.def_val = 100;
  sys_variable_limits.min_val = 0;
  sys_variable_limits.max_val = 100;
  sys_variable_limits.blk_sz = 0;
  if (sysvar_register_srv->register_variable(
          "group_replication_resource_manager", "memory_used_limit",
          PLUGIN_VAR_INT | PLUGIN_VAR_UNSIGNED,
          "Defines percentage threshold for memory lag.", nullptr,
          update_the_variable_used_memory_limit, (void *)&sys_variable_limits,
          (void *)&lag_metadata_threshold.used_memory_limit))
    return 1;
  return 0; /* All system variables registered successfully */
}

int unregister_system_variables() {
  sysvar_unregister_srv->unregister_variable(
      "group_replication_resource_manager", "applier_channel_lag");
  sysvar_unregister_srv->unregister_variable(
      "group_replication_resource_manager", "recovery_channel_lag");
  sysvar_unregister_srv->unregister_variable(
      "group_replication_resource_manager", "memory_used_limit");
  sysvar_unregister_srv->unregister_variable(
      "group_replication_resource_manager", "quarantine_time");
  return 0;
}
}  // namespace gr_resource_manager
