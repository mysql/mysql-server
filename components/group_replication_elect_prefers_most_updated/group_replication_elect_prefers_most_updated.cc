/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#include <my_time.h>
#include <mysql/components/component_implementation.h>
#include <mysql/components/my_service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/component_status_var_service.h>
#include <mysql/components/services/component_sys_var_service.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/components/services/mysql_runtime_error_service.h>
#include <mysqld_error.h>  // ER_*
#include <atomic>
#include "mysql/components/services/group_replication_elect_prefers_most_updated_service.h"
#include "mysql/components/services/registry.h"

REQUIRES_SERVICE_PLACEHOLDER(mysql_runtime_error);
REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_register);
REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_unregister);
REQUIRES_SERVICE_PLACEHOLDER(registry_registration);
REQUIRES_SERVICE_PLACEHOLDER_AS(status_variable_registration,
                                statvar_register_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(log_builtins, log_bi);
REQUIRES_SERVICE_PLACEHOLDER_AS(log_builtins_string, log_bs);

const char component_name[] = LOG_COMPONENT_TAG;
const char component_enable_var_name[] = "enabled";
const std::string comp_enable_full_error_name(std::string(component_name) +
                                              std::string(".") +
                                              std::string(component_name));

static std::atomic<bool> enabled{true};
/* Delta number transactions between most up to date members */
static std::atomic<uint64_t> gr_most_uptodate_delta_to_apply{0};
/* Timestamp last usage of most uptodate method*/
static std::string gr_most_uptodate_timestamp{""};

static int gr_primary_election_uptodate_to_apply(THD *, SHOW_VAR *var,
                                                 char *buf) {
  if (!var || !buf) {
    return 1;
  }

  var->type = SHOW_LONGLONG;
  var->value = buf;
  auto *typed_buf = reinterpret_cast<ulonglong *>(buf);
  *typed_buf = static_cast<ulonglong>(gr_most_uptodate_delta_to_apply.load());
  return 0;
}

static int gr_primary_election_uptodate_timestamp(THD *, SHOW_VAR *var,
                                                  char *buf) {
  assert(SHOW_VAR_FUNC_BUFF_SIZE > MAX_DATE_STRING_REP_LENGTH);
  var->type = SHOW_CHAR;
  var->value = buf;

  strncpy(buf, gr_most_uptodate_timestamp.c_str(), MAX_DATE_STRING_REP_LENGTH);
  return 0;
}

static SHOW_VAR status_func_var[] = {
    {"Gr_latest_primary_election_by_most_uptodate_members_trx_delta",
     reinterpret_cast<char *>(gr_primary_election_uptodate_to_apply), SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Gr_latest_primary_election_by_most_uptodate_member_timestamp",
     reinterpret_cast<char *>(gr_primary_election_uptodate_timestamp),
     SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {nullptr, nullptr, SHOW_UNDEF,
     SHOW_SCOPE_UNDEF}  // null terminator required
};

class Group_replication_primary_election_imp {
 public:
  static DEFINE_BOOL_METHOD(update_primary_election_status,
                            (char *timestamp, uint64_t transactions_delta));
};

DEFINE_BOOL_METHOD(
    Group_replication_primary_election_imp::update_primary_election_status,
    (char *timestamp, uint64_t transactions_delta)) {
  // update global status variables
  gr_most_uptodate_timestamp.assign(timestamp);
  gr_most_uptodate_delta_to_apply = transactions_delta;

  return 0;
}

BEGIN_SERVICE_IMPLEMENTATION(group_replication_primary_election,
                             group_replication_primary_election)
Group_replication_primary_election_imp::update_primary_election_status,
    END_SERVICE_IMPLEMENTATION();

/**
  Register global system variables with component information.

  @return Status of performed operation
  @retval 0 success
  @retval non-zero failure
*/

int register_system_variables() {
  int ret = 0;

  BOOL_CHECK_ARG(bool) enabled_arg;
  enabled_arg.def_val = true;
  if (mysql_service_component_sys_variable_register->register_variable(
          component_name, component_enable_var_name,
          PLUGIN_VAR_BOOL | PLUGIN_VAR_RQCMDARG,
          "Check if most uptodate primary method election enabled.", nullptr,
          nullptr, (void *)&enabled_arg, (void *)&enabled)) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_VALIDATE_PWD_VARIABLE_REGISTRATION_FAILED,
                comp_enable_full_error_name.c_str()); /* purecov: inspected */
    ret = 1;                                          /* purecov: inspected */
  }

  if (!ret) ret = statvar_register_srv->register_variable(status_func_var);

  return ret;
}

/**
  Unregister global system variables with component information.

  @return Status of performed operation
  @retval 0 success
  @retval non-zero failure
*/

int unregister_system_variables() {
  int ret = 0;
  ret = statvar_register_srv->unregister_variable(status_func_var);

  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          component_name, component_enable_var_name)) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_VALIDATE_PWD_VARIABLE_UNREGISTRATION_FAILED,
                comp_enable_full_error_name.c_str()); /* purecov: inspected */
    ret = 1;                                          /* purecov: inspected */
  }
  return ret;
}

/**
  Initialization entry method for Component used when loading the Component.

  @return Status of performed operation
  @retval 0 success
  @retval non-zero failure
*/
mysql_service_status_t group_replication_primary_election_init() {
  return register_system_variables();
}

/**
  De-initialization method for Component used when unloading the Component.

  @return Status of performed operation
  @retval 0 success
  @retval non-zero failure
*/
mysql_service_status_t group_replication_primary_election_deinit() {
  return unregister_system_variables();
}

/* Component provides: page tracking service */
BEGIN_COMPONENT_PROVIDES(group_replication_primary_election_component)
PROVIDES_SERVICE(group_replication_primary_election,
                 group_replication_primary_election),
    END_COMPONENT_PROVIDES();

/* Component dependencies. */
BEGIN_COMPONENT_REQUIRES(group_replication_primary_election_component)
REQUIRES_SERVICE(mysql_runtime_error),
    REQUIRES_SERVICE_AS(status_variable_registration, statvar_register_srv),
    REQUIRES_SERVICE(component_sys_variable_register),
    REQUIRES_SERVICE(component_sys_variable_unregister),
    REQUIRES_SERVICE(registry_registration),
    REQUIRES_SERVICE_AS(log_builtins, log_bi),
    REQUIRES_SERVICE_AS(log_builtins_string, log_bs), END_COMPONENT_REQUIRES();

/* A list of metadata to describe the Component. */
BEGIN_COMPONENT_METADATA(group_replication_primary_election_component)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), END_COMPONENT_METADATA();

/* Declaration of the Component. */
DECLARE_COMPONENT(group_replication_primary_election_component,
                  "mysql:group_replication_elect_prefers_most_updated")
group_replication_primary_election_init,
    group_replication_primary_election_deinit, END_DECLARE_COMPONENT();

/* Defines list of Components contained in this library. Note that for now we
assume that library will have exactly one Component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(
    group_replication_primary_election_component),
    END_DECLARE_LIBRARY_COMPONENTS
