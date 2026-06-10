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

#include "connection_control.h"
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/bits/psi_memory_bits.h>
#include <mysql/components/services/connection_control_pfs.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/components/services/mysql_cond.h>
#include <mysql/components/services/mysql_mutex.h>
#include <mysql/components/services/psi_memory.h>
#include <mysqld_error.h>
#include <template_utils.h>
#include "connection_control_coordinator.h" /* g_connection_event_coordinator */
#include "connection_control_memory.h"
#include "connection_control_pfs_table.h"
#include "connection_delay.h"
#include "connection_delay_api.h" /* connection_delay apis */
#include "failed_attempts_list_imp.h"
#include "option_usage.h"

SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;

using connection_control::Connection_control_statistics;
using connection_control::Connection_control_variables;
using connection_control::Connection_event_coordinator;

Connection_control_statistics g_statistics;
Connection_control_variables g_variables;
connection_control::Failed_attempts_list_imp g_failed_attempts_list;
static Connection_event_coordinator *g_connection_event_coordinator = nullptr;

/* Performance Schema instrumentation */

PSI_memory_key key_connection_delay_memory = PSI_NOT_INSTRUMENTED;

static PSI_memory_info all_connection_delay_memory_info[] = {
    {&key_connection_delay_memory, "component", 0, PSI_VOLATILITY_UNKNOWN,
     "Memory allocated by connection_control component."}};

PSI_mutex_key key_connection_delay_mutex = PSI_NOT_INSTRUMENTED;

static PSI_mutex_info all_connection_delay_mutex_info[] = {
    {&key_connection_delay_mutex, "connection_delay_mutex", 0, 0,
     PSI_DOCUMENT_ME}};

PSI_rwlock_key key_connection_event_delay_lock = PSI_NOT_INSTRUMENTED;

static PSI_rwlock_info all_connection_delay_rwlock_info[] = {
    {&key_connection_event_delay_lock, "connection_event_delay_lock",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME}};

PSI_cond_key key_connection_delay_wait = PSI_NOT_INSTRUMENTED;

static PSI_cond_info all_connection_delay_cond_info[] = {
    {&key_connection_delay_wait, "connection_delay_wait_condition", 0, 0,
     PSI_DOCUMENT_ME}};

static void register_instruments() {
  const char *category = "conn_delay";

  PSI_MEMORY_CALL(register_memory)
  ("connection_control", all_connection_delay_memory_info, 1);

  int const count_mutex = array_elements(all_connection_delay_mutex_info);
  mysql_mutex_register(category, all_connection_delay_mutex_info, count_mutex);

  int const count_rwlock = array_elements(all_connection_delay_rwlock_info);
  mysql_rwlock_register(category, all_connection_delay_rwlock_info,
                        count_rwlock);

  int const count_cond = array_elements(all_connection_delay_cond_info);
  mysql_cond_register(category, all_connection_delay_cond_info, count_cond);
}

mysql_event_tracking_connection_subclass_t Event_tracking_implementation::
    Event_tracking_connection_implementation::filtered_sub_events = 0;

bool Event_tracking_implementation::Event_tracking_connection_implementation::
    callback(const mysql_event_tracking_connection_data *data) {
  try {
    if (data->event_subclass == EVENT_TRACKING_CONNECTION_CONNECT) {
      THD *thd;
      mysql_service_mysql_current_thread_reader->get(&thd);
      /** Notify event coordinator */
      if (g_connection_event_coordinator != nullptr)
        g_connection_event_coordinator->notify_event(thd, data);
    }
  } catch (...) {
    /* Happily ignore any bad behavior */
  }

  return false;
}

/**
  check() function for connection_control_failed_connections_threshold

  Check whether new value is within valid bounds or not.

  @param thd        Not used.
  @param var        Not used.
  @param save       Pointer to which new value to be saved.
  @param value      New value for the option.

  @returns whether new value is within valid bounds or not.
    @retval 0 Value is ok
    @retval 1 Value is not within valid bounds
*/

static int check_failed_connections_threshold(MYSQL_THD thd [[maybe_unused]],
                                              SYS_VAR *var [[maybe_unused]],
                                              void *save [[maybe_unused]],
                                              struct st_mysql_value *value) {
  longlong new_value;
  if (value->val_int(value, &new_value) != 0) {
    return 1; /* NULL value */
  }

  if (new_value >= connection_control::MIN_THRESHOLD &&
      new_value <= connection_control::MAX_THRESHOLD) {
    *(reinterpret_cast<longlong *>(save)) = new_value;
    return 0;
  }

  return 1;
}

/**
  update() function for connection_control_failed_connections_threshold

  Updates g_connection_event_coordinator with new value.
  Also notifies observers about the update.

  @param thd        Not used.
  @param var        Not used.
  @param var_ptr    Variable information
  @param save       New value for
  connection_control_failed_connections_threshold
*/

static void update_failed_connections_threshold(MYSQL_THD thd [[maybe_unused]],
                                                SYS_VAR *var [[maybe_unused]],
                                                void *var_ptr [[maybe_unused]],
                                                const void *save) {
  /*
    This won't result in overflow because we have already checked that this is
    within valid bounds.
  */
  longlong new_value = *(reinterpret_cast<const longlong *>(save));
  g_variables.failed_connections_threshold = static_cast<int64>(new_value);
  g_connection_event_coordinator->notify_sys_var(
      OPT_FAILED_CONNECTIONS_THRESHOLD, &new_value);
}

/**
  check() function for connection_control_min_connection_delay

  Check whether new value is within valid bounds or not.

  @param thd        Not used.
  @param var        Not used.
  @param save       Not used.
  @param value      New value for the option.

  @returns whether new value is within valid bounds or not.
    @retval 0 Value is ok
    @retval 1 Value is not within valid bounds
*/

static int check_min_connection_delay(MYSQL_THD thd [[maybe_unused]],
                                      SYS_VAR *var [[maybe_unused]],
                                      void *save [[maybe_unused]],
                                      struct st_mysql_value *value) {
  long long new_value;
  if (value->val_int(value, &new_value) != 0) {
    return 1; /* NULL value */
  }

  if (new_value >= connection_control::MIN_DELAY &&
      new_value <= connection_control::MAX_DELAY &&
      new_value <= g_variables.max_connection_delay) {
    *(reinterpret_cast<longlong *>(save)) = new_value;
    return 0;
  }
  return 1;
}

/**
  update() function for connection_control_min_connection_delay

  Updates g_connection_event_coordinator with new value.
  Also notifies observers about the update.

  @param thd        Not used.
  @param var        Not used.
  @param var_ptr    Variable information
  @param save       New value for connection_control_min_connection_delay
*/

static void update_min_connection_delay(MYSQL_THD thd [[maybe_unused]],
                                        SYS_VAR *var [[maybe_unused]],
                                        void *var_ptr [[maybe_unused]],
                                        const void *save) {
  longlong new_value = *(reinterpret_cast<const longlong *>(save));
  g_variables.min_connection_delay = static_cast<int64>(new_value);
  g_connection_event_coordinator->notify_sys_var(OPT_MIN_CONNECTION_DELAY,
                                                 &new_value);
}

/**
  check() function for connection_control_max_connection_delay

  Check whether new value is within valid bounds or not.

  @param thd        Not used.
  @param var        Not used.
  @param save       Pointer to which new value to be saved.
  @param value      New value for the option.

  @returns whether new value is within valid bounds or not.
    @retval 0 Value is ok
    @retval 1 Value is not within valid bounds
*/

static int check_max_connection_delay(MYSQL_THD thd [[maybe_unused]],
                                      SYS_VAR *var [[maybe_unused]],
                                      void *save [[maybe_unused]],
                                      struct st_mysql_value *value) {
  long long new_value;
  if (value->val_int(value, &new_value) != 0) {
    return 1; /* NULL value */
  }

  if (new_value >= connection_control::MIN_DELAY &&
      new_value <= connection_control::MAX_DELAY &&
      new_value >= g_variables.min_connection_delay) {
    *(reinterpret_cast<longlong *>(save)) = new_value;
    return 0;
  }
  return 1;
}

/**
  update() function for connection_control_max_connection_delay

  Updates g_connection_event_coordinator with new value.
  Also notifies observers about the update.

  @param thd        Not used.
  @param var        Not used.
  @param var_ptr    Variable information
  @param save       New value for connection_control_max_connection_delay
*/

static void update_max_connection_delay(MYSQL_THD thd [[maybe_unused]],
                                        SYS_VAR *var [[maybe_unused]],
                                        void *var_ptr [[maybe_unused]],
                                        const void *save) {
  longlong new_value = *(reinterpret_cast<const longlong *>(save));
  g_variables.max_connection_delay = static_cast<int64>(new_value);
  g_connection_event_coordinator->notify_sys_var(OPT_MAX_CONNECTION_DELAY,
                                                 &new_value);
}

/**
  Helper function to display value for status variable defined by its index
  within the array of all status variables.

  @param var_index  Status variable index.
  @param var  Status variable structure
  @param buff Value buffer.

  @returns Always returns success.
*/
static int show_status_variable(int var_index, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONGLONG;
  var->value = buff;
  auto *value = reinterpret_cast<longlong *>(buff);
  const int64 current_val = g_statistics.stats_array[var_index].load();
  *value = static_cast<longlong>(current_val);
  return 0;
}

/**
  Function to display value for status variable :
  Connection_control_delay_generated

  @param thd  MYSQL_THD handle. Unused.
  @param var  Status variable structure
  @param buff Value buffer.

  @returns Always returns success.
*/

static int show_delay_generated(MYSQL_THD thd [[maybe_unused]], SHOW_VAR *var,
                                char *buff) {
  return show_status_variable(STAT_CONNECTION_DELAY_TRIGGERED, var, buff);
}

/**
  Function to display value for status variable :
  Component_connection_control_exempted_unknown_users

  @param thd  MYSQL_THD handle. Unused.
  @param var  Status variable structure
  @param buff Value buffer.

  @returns Always returns success.
*/

static int show_exempted_users(MYSQL_THD thd [[maybe_unused]], SHOW_VAR *var,
                               char *buff) {
  return show_status_variable(STAT_CONNECTION_EXEMPTED_USERS, var, buff);
}

static void update_exempt_unknown_users(MYSQL_THD thd [[maybe_unused]],
                                        SYS_VAR *var [[maybe_unused]],
                                        void *var_ptr [[maybe_unused]],
                                        const void *save) {
  bool new_value = *(reinterpret_cast<const bool *>(save));
  g_variables.exempt_unknown_users = new_value;
  g_connection_event_coordinator->notify_sys_var(OPT_EXEMPT_UNKNOWN_USERS,
                                                 &new_value);
}

SHOW_VAR static component_connection_control_status_variables[STAT_LAST + 1] = {
    {.name = "Component_connection_control_delay_generated",
     .value = reinterpret_cast<char *>(&show_delay_generated),
     .type = SHOW_FUNC,
     .scope = SHOW_SCOPE_GLOBAL},
    {.name = "option_tracker_usage:Connection control component",
     .value = reinterpret_cast<char *>(
         &connection_control::
             opt_option_tracker_usage_connection_control_component),
     .type = SHOW_LONGLONG,
     .scope = SHOW_SCOPE_GLOBAL},
    {.name = "Component_connection_control_exempted_unknown_users",
     .value = reinterpret_cast<char *>(&show_exempted_users),
     .type = SHOW_FUNC,
     .scope = SHOW_SCOPE_GLOBAL},
    {.name = nullptr,
     .value = nullptr,
     .type = static_cast<enum_mysql_show_type>(0),
     .scope = static_cast<enum_mysql_show_scope>(0)}};

static int register_status_variables() {
  if (mysql_service_status_variable_registration->register_variable(
          reinterpret_cast<SHOW_VAR *>(
              &component_connection_control_status_variables)) != 0) {
    LogComponentErr(ERROR_LEVEL, ER_CONNECTION_CONTROL_STATUS_VAR_REG_FAILED);
    return 1;
  }
  return 0;
}

static int register_system_variables() {
  INTEGRAL_CHECK_ARG(longlong)
  threshold, min_delay, max_delay;
  BOOL_CHECK_ARG(bool) exempt_unknown;

  threshold.def_val = 3;
  threshold.min_val = 0;
  threshold.max_val = 2147483647;
  threshold.blk_sz = 0;
  if (mysql_service_component_sys_variable_register->register_variable(
          "component_connection_control", "failed_connections_threshold",
          PLUGIN_VAR_LONGLONG | PLUGIN_VAR_RQCMDARG,
          "Failed connection threshold to trigger delay. Default is 3.",
          check_failed_connections_threshold,
          update_failed_connections_threshold, (void *)&threshold,
          (void *)&g_variables.failed_connections_threshold) != 0) {
    LogComponentErr(
        ERROR_LEVEL, ER_CONNECTION_CONTROL_VARIABLE_REGISTRATION_FAILED,
        "component_connection_control.failed_connections_threshold");
    return 1;
  }

  min_delay.def_val = 1000;
  min_delay.min_val = 1000;
  min_delay.max_val = 2147483647;
  min_delay.blk_sz = 0;
  if (mysql_service_component_sys_variable_register->register_variable(
          "component_connection_control", "min_connection_delay",
          PLUGIN_VAR_LONGLONG | PLUGIN_VAR_RQCMDARG,
          "Maximum delay in msec to be introduced. Default is 1000.",
          check_min_connection_delay, update_min_connection_delay,
          (void *)&min_delay, (void *)&g_variables.min_connection_delay) != 0) {
    LogComponentErr(ERROR_LEVEL,
                    ER_CONNECTION_CONTROL_VARIABLE_REGISTRATION_FAILED,
                    "component_connection_control.min_connection_delay");
    goto reg_min_delay_failed;
  }

  max_delay.def_val = 2147483647;
  max_delay.min_val = 1000;
  max_delay.max_val = 2147483647;
  max_delay.blk_sz = 0;
  if (mysql_service_component_sys_variable_register->register_variable(
          "component_connection_control", "max_connection_delay",
          PLUGIN_VAR_LONGLONG | PLUGIN_VAR_RQCMDARG,
          "Maximum delay in msec to be introduced. Default is 2147483647.",
          check_max_connection_delay, update_max_connection_delay,
          (void *)&max_delay, (void *)&g_variables.max_connection_delay) != 0) {
    LogComponentErr(ERROR_LEVEL,
                    ER_CONNECTION_CONTROL_VARIABLE_REGISTRATION_FAILED,
                    "component_connection_control.max_connection_delay");
    goto reg_max_delay_failed;
  }

  exempt_unknown.def_val = false;
  if (mysql_service_component_sys_variable_register->register_variable(
          "component_connection_control", "exempt_unknown_users",
          PLUGIN_VAR_BOOL | PLUGIN_VAR_RQCMDARG,
          "Skip delay for unknown users (like load balancer checking service "
          "availability). Default is OFF.",
          nullptr, update_exempt_unknown_users, (void *)&exempt_unknown,
          (void *)&g_variables.exempt_unknown_users) != 0) {
    LogComponentErr(ERROR_LEVEL,
                    ER_CONNECTION_CONTROL_VARIABLE_REGISTRATION_FAILED,
                    "component_connection_control.exempt_unknown_users");
    goto reg_exempt_unknown_failed;
  }

  return 0;

reg_exempt_unknown_failed:
  mysql_service_component_sys_variable_unregister->unregister_variable(
      "component_connection_control", "max_connection_delay");
reg_max_delay_failed:
  mysql_service_component_sys_variable_unregister->unregister_variable(
      "component_connection_control", "min_connection_delay");
reg_min_delay_failed:
  mysql_service_component_sys_variable_unregister->unregister_variable(
      "component_connection_control", "failed_connections_threshold");
  return 1;
}

static int unregister_status_variables() {
  if (mysql_service_status_variable_registration->unregister_variable(
          reinterpret_cast<SHOW_VAR *>(
              &component_connection_control_status_variables)) != 0) {
    LogComponentErr(ERROR_LEVEL, ER_CONNECTION_CONTROL_STATUS_VAR_UNREG_FAILED);
    return 1;
  }
  return 0;
}

static int unregister_system_variables() {
  int error = 0;
  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          "component_connection_control", "failed_connections_threshold") !=
      0) {
    LogComponentErr(
        ERROR_LEVEL, ER_CONNECTION_CONTROL_VARIABLE_UNREGISTRATION_FAILED,
        "component_connection_control.failed_connections_threshold");
    error = 1;
  }

  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          "component_connection_control", "min_connection_delay") != 0) {
    LogComponentErr(ERROR_LEVEL,
                    ER_CONNECTION_CONTROL_VARIABLE_UNREGISTRATION_FAILED,
                    "component_connection_control.min_connection_delay");
    error = 1;
  }

  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          "component_connection_control", "max_connection_delay") != 0) {
    LogComponentErr(ERROR_LEVEL,
                    ER_CONNECTION_CONTROL_VARIABLE_UNREGISTRATION_FAILED,
                    "component_connection_control.max_connection_delay");
    error = 1;
  }

  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          "component_connection_control", "exempt_unknown_users") != 0) {
    LogComponentErr(ERROR_LEVEL,
                    ER_CONNECTION_CONTROL_VARIABLE_UNREGISTRATION_FAILED,
                    "component_connection_control.exempt_unknown_users");
    error = 1;
  }

  return error;
}

/**
  logger services initialization method for Component used when
  loading the Component.
*/
static void log_service_init() {
  log_bi = mysql_service_log_builtins;
  log_bs = mysql_service_log_builtins_string;
}

/**
  Component initialization function

  @returns initialization status
    @retval 0 Success
    @retval 1 Failure
*/

static mysql_service_status_t connection_control_init() {
  /*
    Declare all performance schema instrumentation up front,
    so it is discoverable.
  */
  register_instruments();

  if (connection_control::register_pfs_table()) {
    return 1;
  }

  log_service_init();

  if (register_system_variables() != 0) {
    connection_control::unregister_pfs_table();
    return 1;
  }
  if (register_status_variables() != 0) {
    unregister_system_variables();
    connection_control::unregister_pfs_table();
    return 1;
  }
  if (connection_control::connection_control_component_option_usage_init()) {
    unregister_status_variables();
    unregister_system_variables();
    connection_control::unregister_pfs_table();
    return 1;
  }
  g_connection_event_coordinator = new Connection_event_coordinator();
  init_connection_delay_event(g_connection_event_coordinator);
  return 0;
}

/**
  Component deinitialization

  @returns success
*/

static mysql_service_status_t connection_control_deinit() {
  delete g_connection_event_coordinator;
  g_connection_event_coordinator = nullptr;
  connection_control::deinit_connection_delay_event();

  if (connection_control::connection_control_component_option_usage_deinit()) {
    LogComponentErr(ERROR_LEVEL, ER_CONNECTION_CONTROL_FAILED_DEINIT,
                    "connection_control_component_option_usage");
  }

  if (unregister_status_variables() != 0) {
    LogComponentErr(ERROR_LEVEL, ER_CONNECTION_CONTROL_FAILED_DEINIT,
                    "status_variable");
  }

  if (unregister_system_variables() != 0) {
    LogComponentErr(ERROR_LEVEL, ER_CONNECTION_CONTROL_FAILED_DEINIT,
                    "system_variable");
  }

  if (connection_control::unregister_pfs_table()) {
    LogComponentErr(ERROR_LEVEL, ER_CONNECTION_CONTROL_FAILED_DEINIT,
                    "performance_schema_table");
  }

  return 0;
}

IMPLEMENTS_SERVICE_EVENT_TRACKING_CONNECTION(connection_control);

BEGIN_COMPONENT_PROVIDES(connection_control)
PROVIDES_SERVICE_EVENT_TRACKING_CONNECTION(connection_control),
    END_COMPONENT_PROVIDES();

REQUIRES_MYSQL_RWLOCK_SERVICE_PLACEHOLDER;
REQUIRES_MYSQL_COND_SERVICE_PLACEHOLDER;
REQUIRES_MYSQL_MUTEX_SERVICE_PLACEHOLDER;
REQUIRES_PSI_MEMORY_SERVICE_PLACEHOLDER;

REQUIRES_SERVICE_PLACEHOLDER(pfs_plugin_column_string_v2);
REQUIRES_SERVICE_PLACEHOLDER(mysql_thd_security_context);
REQUIRES_SERVICE_PLACEHOLDER(pfs_plugin_column_integer_v1);
REQUIRES_SERVICE_PLACEHOLDER(pfs_plugin_table_v1);
REQUIRES_SERVICE_PLACEHOLDER(mysql_security_context_options);
REQUIRES_SERVICE_PLACEHOLDER(log_builtins);
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_string);
REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_register);
REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_unregister);
REQUIRES_SERVICE_PLACEHOLDER(status_variable_registration);
REQUIRES_SERVICE_PLACEHOLDER(registry_registration);
REQUIRES_SERVICE_PLACEHOLDER(mysql_current_thread_reader);

/* A list of dependencies.
   The dynamic_loader fetches the references for the below services at the
   component load time and disposes off them at unload.
*/
BEGIN_COMPONENT_REQUIRES(connection_control)
REQUIRES_MYSQL_MUTEX_SERVICE, REQUIRES_MYSQL_RWLOCK_SERVICE,
    REQUIRES_MYSQL_COND_SERVICE, REQUIRES_PSI_MEMORY_SERVICE,
    REQUIRES_SERVICE(log_builtins), REQUIRES_SERVICE(log_builtins_string),
    REQUIRES_SERVICE(pfs_plugin_column_integer_v1),
    REQUIRES_SERVICE(pfs_plugin_table_v1),
    REQUIRES_SERVICE(pfs_plugin_column_string_v2),
    REQUIRES_SERVICE(registry_registration),
    REQUIRES_SERVICE(component_sys_variable_register),
    REQUIRES_SERVICE(component_sys_variable_unregister),
    REQUIRES_SERVICE(status_variable_registration),
    REQUIRES_SERVICE(mysql_current_thread_reader),
    REQUIRES_SERVICE(mysql_thd_security_context),
    REQUIRES_SERVICE(mysql_security_context_options), END_COMPONENT_REQUIRES();

/* component description */
BEGIN_COMPONENT_METADATA(connection_control)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"),
    METADATA("connection_control_service", "1"), END_COMPONENT_METADATA();

/* component declaration */
DECLARE_COMPONENT(connection_control, "mysql:connection_control")
connection_control_init, connection_control_deinit END_DECLARE_COMPONENT();

/* components contained in this library.
   for now assume that each library will have exactly one component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(connection_control)
    END_DECLARE_LIBRARY_COMPONENTS
