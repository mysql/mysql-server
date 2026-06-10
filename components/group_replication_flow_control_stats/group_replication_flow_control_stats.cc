/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include <my_inttypes.h>
#include <my_time.h>
#include <mysql/components/component_implementation.h>
#include <mysql/components/my_service.h>
#include <mysql/components/services/component_status_var_service.h>
#include <mysql/components/services/group_replication_flow_control_metrics_service.h>
#include <mysql/components/services/mysql_runtime_error_service.h>
#include <mysqld_error.h>  // ER_*

REQUIRES_SERVICE_PLACEHOLDER_AS(status_variable_registration,
                                statvar_register_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(group_replication_flow_control_metrics_service,
                                gr_flow_control_service);
REQUIRES_SERVICE_PLACEHOLDER(mysql_runtime_error);

static int gr_flow_control_throttle_count(THD *, SHOW_VAR *var, char *buf) {
  var->type = SHOW_LONGLONG;
  var->value = buf;
  auto *typed_buf = reinterpret_cast<ulonglong *>(buf);
  uint64_t value;
  gr_flow_control_service->get_throttle_count(&value);
  *typed_buf = static_cast<ulonglong>(value);

  return 0;
}

static int gr_flow_control_throttle_time_sum(THD *, SHOW_VAR *var, char *buf) {
  var->type = SHOW_LONGLONG;
  var->value = buf;
  auto *typed_buf = reinterpret_cast<ulonglong *>(buf);
  uint64_t value;
  gr_flow_control_service->get_throttle_time_sum(&value);
  *typed_buf = static_cast<ulonglong>(value);
  return 0;
}

static int gr_flow_control_throttle_active_count(THD *, SHOW_VAR *var,
                                                 char *buf) {
  var->type = SHOW_LONGLONG;
  var->value = buf;
  auto *typed_buf = reinterpret_cast<ulonglong *>(buf);
  uint64_t value;
  gr_flow_control_service->get_throttle_active_count(&value);
  *typed_buf = static_cast<ulonglong>(value);
  return 0;
}

static int gr_flow_control_throttle_last_throttle_timestamp(THD *,
                                                            SHOW_VAR *var,
                                                            char *buf) {
  assert(SHOW_VAR_FUNC_BUFF_SIZE > MAX_DATE_STRING_REP_LENGTH);
  var->type = SHOW_CHAR;
  var->value = buf;
  auto *typed_buf = reinterpret_cast<char *>(buf);
  gr_flow_control_service->get_throttle_last_throttle_timestamp(typed_buf);
  return 0;
}

static SHOW_VAR status_func_var[] = {
    {"Gr_flow_control_throttle_count",
     reinterpret_cast<char *>(gr_flow_control_throttle_count), SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Gr_flow_control_throttle_time_sum",
     reinterpret_cast<char *>(gr_flow_control_throttle_time_sum), SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Gr_flow_control_throttle_active_count",
     reinterpret_cast<char *>(gr_flow_control_throttle_active_count), SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Gr_flow_control_throttle_last_throttle_timestamp",
     reinterpret_cast<char *>(gr_flow_control_throttle_last_throttle_timestamp),
     SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {nullptr, nullptr, SHOW_UNDEF,
     SHOW_SCOPE_UNDEF}  // null terminator required
};

int register_status_variables() {
  return statvar_register_srv->register_variable(status_func_var);
}

int unregister_status_variables() {
  return statvar_register_srv->unregister_variable(status_func_var);
}

/**
  Initialization entry method for Component used when loading the Component.

  @return Status of performed operation
  @retval 0 success
  @retval non-zero failure
*/
mysql_service_status_t group_replication_flow_control_stats_init() {
  return register_status_variables();
}

/**
  De-initialization method for Component used when unloading the Component.

  @return Status of performed operation
  @retval 0 success
  @retval non-zero failure
*/
mysql_service_status_t group_replication_flow_control_stats_deinit() {
  return unregister_status_variables();
}

/* Component provides: page tracking service */
BEGIN_COMPONENT_PROVIDES(group_replication_flow_control_stats_component)
END_COMPONENT_PROVIDES();

/* Component dependencies. */
BEGIN_COMPONENT_REQUIRES(group_replication_flow_control_stats_component)
REQUIRES_SERVICE_AS(status_variable_registration, statvar_register_srv),
    REQUIRES_SERVICE_AS(group_replication_flow_control_metrics_service,
                        gr_flow_control_service),
    REQUIRES_SERVICE(mysql_runtime_error), END_COMPONENT_REQUIRES();

/* A list of metadata to describe the Component. */
BEGIN_COMPONENT_METADATA(group_replication_flow_control_stats_component)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), END_COMPONENT_METADATA();

/* Declaration of the Component. */
DECLARE_COMPONENT(group_replication_flow_control_stats_component,
                  "mysql:group_replication_flow_control_stats")
group_replication_flow_control_stats_init,
    group_replication_flow_control_stats_deinit, END_DECLARE_COMPONENT();

/* Defines list of Components contained in this library. Note that for now we
assume that library will have exactly one Component. */
DECLARE_LIBRARY_COMPONENTS
&COMPONENT_REF(group_replication_flow_control_stats_component),
    END_DECLARE_LIBRARY_COMPONENTS
