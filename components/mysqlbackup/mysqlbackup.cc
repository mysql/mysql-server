/* Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#include "mysqlbackup.h"

#include <atomic>

#include "backup_comp_constants.h"
#include "backup_page_tracker.h"
#include "mysql/components/library_mysys/my_memory.h"
#include "mysql/components/services/psi_memory.h"
#include "mysql/service_security_context.h"
#include "mysqld_error.h"
#include "string_with_len.h"

/// This file contains a definition of the mysqlbackup component.

// Component global variables.
static char *mysqlbackup_component_version{nullptr};
char *mysqlbackup_backup_id = nullptr;  // non-static is used in other files

// Status of registration of the system variable. Note that there should
// be multiple such flags, if more system variables are intoduced, so
// that we can keep track of the register/unregister status for each
// variable.
static std::atomic<bool> mysqlbackup_component_sys_var_registered{false};

/**
   Method to check if the current user has got backup privilege.

   @param[in]  opaque_thd     Current thread context.

   @return true, if the seurity context of the thread has backup_admin
   privileges
   @retval false otherwise
*/
bool have_backup_admin_privilege(void *opaque_thd) {
  // get the security context of the thread
  Security_context_handle ctx = nullptr;
  if (mysql_service_mysql_thd_security_context->get(opaque_thd, &ctx) || !ctx) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(WARNING_LEVEL)
        .lookup(ER_VALIDATE_PWD_FAILED_TO_GET_SECURITY_CTX);
    return false;
  }

  if (mysql_service_global_grants_check->has_global_grant(
          ctx, STRING_WITH_LEN("BACKUP_ADMIN")))
    return true;

  return false;
}

/**
  Register UDF(s)

  @return Status
  @retval 0 success
  @retval non-zero failure
*/
mysql_service_status_t register_udfs() {
  mysql_service_status_t retval = 0;
  // register backup page track udfs
  retval = Backup_page_tracker::register_udfs();
  return (retval);
}

/**
  Unregister UDF(s)

  @return Status
  @retval 0 success
  @retval non-zero failure
*/
mysql_service_status_t unregister_udfs() {
  int retval = 0;
  retval = Backup_page_tracker::unregister_udfs();
  return (retval);
}

// Server status variables defined by this component. Note that there
// should be multiple such arrays, each null-terminated, if more status
// variables are introduced, so that we can keep track of the
// register/unregister status for each variable.
static SHOW_VAR mysqlbackup_status_variables[] = {
    {Backup_comp_constants::backup_component_version,
     (char *)&mysqlbackup_component_version, SHOW_CHAR_PTR, SHOW_SCOPE_GLOBAL},
    {nullptr, nullptr, SHOW_LONG, SHOW_SCOPE_GLOBAL}};

/**
  Register the server status variables defined by this component.

  @return Status
  @retval false success
  @retval true failure
*/
static bool register_status_variables() {
  if (mysqlbackup_component_version) {
    std::string msg{
        "Status variable mysqlbackup.component_version is not NULL. Most "
        "likely the status variable does already exist."};
    LogErr(ERROR_LEVEL, ER_MYSQLBACKUP_MSG, msg.c_str());
    return (true);
  }

  // Give the global variable a valid value before registering.
  mysqlbackup_component_version = static_cast<char *>(my_malloc(
      PSI_NOT_INSTRUMENTED, strlen(MYSQL_SERVER_VERSION) + 1, MYF(0)));
  strncpy(mysqlbackup_component_version, MYSQL_SERVER_VERSION,
          strlen(MYSQL_SERVER_VERSION) + 1);

  if (mysqlbackup_component_version == nullptr) {
    std::string msg{std::string("Cannot register status variable '") +
                    mysqlbackup_status_variables[0].name +
                    "' due to insufficient memory."};
    LogErr(ERROR_LEVEL, ER_MYSQLBACKUP_MSG, msg.c_str());
    return (true);
  }

  if (mysql_service_status_variable_registration->register_variable(
          (SHOW_VAR *)&mysqlbackup_status_variables)) {
    std::string msg{std::string(mysqlbackup_status_variables[0].name) +
                    " register failed."};
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_MYSQLBACKUP_MSG, msg.c_str());

    // Free the memory allocated for the global variable
    my_free(mysqlbackup_component_version);
    mysqlbackup_component_version = nullptr;
    return (true);
  }

  return (false);
}

/**
  Unregister the server status variables defined by this component.

  @return Status
  @retval false success
  @retval true failure
*/
static bool unregister_status_variables() {
  if (mysql_service_status_variable_registration->unregister_variable(
          (SHOW_VAR *)&mysqlbackup_status_variables)) {
    if (mysqlbackup_component_version == nullptr) {
      // Status variable is already un-registered.
      return false;
    }

    std::string msg{std::string(mysqlbackup_status_variables[0].name) +
                    " unregister failed."};
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_MYSQLBACKUP_MSG, msg.c_str());
    return (true);
  }

  // Free the global variable only after a successful unregister.
  my_free(mysqlbackup_component_version);
  mysqlbackup_component_version = nullptr;

  return (false);
}

/**
  Method to set the system variable "mysqlbackup.backupid". Will check if the
  user has SUPER or BACKUP_ADMIN privilege.

  @return Status
  @retval 0 on success, errorno on failure
*/
static int mysqlbackup_backup_id_check(MYSQL_THD thd,
                                       SYS_VAR *self [[maybe_unused]],
                                       void *save,
                                       struct st_mysql_value *value) {
  if (!have_backup_admin_privilege(thd))
    return (ER_SPECIFIC_ACCESS_DENIED_ERROR);
  int value_len = 0;
  *static_cast<const char **>(save) =
      value->val_str(value, nullptr, &value_len);
  return (0);
}
/**
  Update function for mysqlbackup_backup_id.
*/
static void mysqlbackup_backup_id_update(MYSQL_THD, SYS_VAR *, void *var_ptr,
                                         const void *save) {
  // *(const char **)var_ptr = *(const char **)save;
  *(const char **)var_ptr =
      *(static_cast<const char **>(const_cast<void *>(save)));
  Backup_page_tracker::backup_id_update();
}

/**
  Register the server system variables defined by this component.

  @return Status
  @retval false success
  @retval true failure
*/
static bool register_system_variables() {
  if (mysqlbackup_component_sys_var_registered) {
    // System variable is already registered.
    return (false);
  }

  STR_CHECK_ARG(str) str_arg;
  str_arg.def_val = nullptr;

  if (mysql_service_component_sys_variable_register->register_variable(
          Backup_comp_constants::mysqlbackup, Backup_comp_constants::backupid,
          PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_RQCMDARG |
              PLUGIN_VAR_NOPERSIST,
          "Backup id of an ongoing backup.", mysqlbackup_backup_id_check,
          mysqlbackup_backup_id_update, (void *)&str_arg,
          (void *)&mysqlbackup_backup_id)) {
    std::string msg{std::string(Backup_comp_constants::mysqlbackup) + "." +
                    Backup_comp_constants::backupid + " register failed."};
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_MYSQLBACKUP_MSG, msg.c_str());
    // More backup variables to be registered here
    return (true);
  }

  // System variable is registered successfully.
  mysqlbackup_component_sys_var_registered = true;

  return (false);
}

/**
  Unregister the server system variables defined by this component.

  @return Status
  @retval false success
  @retval true failure
*/
static bool unregister_system_variables() {
  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          Backup_comp_constants::mysqlbackup,
          Backup_comp_constants::backupid)) {
    if (!mysqlbackup_component_sys_var_registered) {
      // System variable is already un-registered.
      return (false);
    }

    std::string msg{std::string(Backup_comp_constants::mysqlbackup) + "." +
                    Backup_comp_constants::backupid + " unregister failed."};
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_MYSQLBACKUP_MSG, msg.c_str());
    return (true);
  }

  // System variable is un-registered successfully.
  mysqlbackup_component_sys_var_registered = false;

  return (false);
}

/**
  Types for the logging service.
*/
SERVICE_TYPE(log_builtins) * log_bi;
SERVICE_TYPE(log_builtins_string) * log_bs;

/**
  Initialize logging service.

  @return Status
  @retval false success
  @retval true failure
*/
static bool initialize_log_service() {
  log_bi = mysql_service_log_builtins;
  log_bs = mysql_service_log_builtins_string;
  return false;
}

/**
  Deinitialize logging service.

  @return Status
  @retval false success
  @retval true failure
*/
static bool deinitialize_log_service() { return false; }

/**
  Initialize the component when loading the component.

  @return Status
  @retval 0 success
  @retval non-zero failure
*/
mysql_service_status_t mysqlbackup_init() {
  int failpoint = 0;
  do {
    if (initialize_log_service()) break;
    failpoint = 1;
    if (register_system_variables()) break;
    failpoint = 2;
    if (register_status_variables()) break;
    failpoint = 3;
    if (register_udfs()) break;
    failpoint = 4;
  } while (false);
  /* If failed before the last initialization succeeded, deinitialize. */
  switch (failpoint) {
    case 3:
      unregister_status_variables();
      [[fallthrough]];
    case 2:
      unregister_system_variables();
      [[fallthrough]];
    case 1:
      deinitialize_log_service();
      [[fallthrough]];
    case 0:
      return (1);
  }
  return (0);
}

/**
  Deinitialize the component when unloading the component.

  @return Status
  @retval 0 success
  @retval non-zero failure
*/
mysql_service_status_t mysqlbackup_deinit() {
  mysql_service_status_t failed = 0;
  Backup_page_tracker::deinit();
  if (unregister_udfs()) failed = 1;
  if (unregister_status_variables()) failed = 1;
  if (unregister_system_variables()) failed = 1;
  if (deinitialize_log_service()) failed = 1;
  // Reset variables to the state they had at the first dlopen().
  mysqlbackup_component_version = nullptr;
  mysqlbackup_backup_id = nullptr;
  mysqlbackup_component_sys_var_registered = false;
  return (failed);
}

/**
  This component does not provide any services.
*/
BEGIN_COMPONENT_PROVIDES(mysqlbackup)
END_COMPONENT_PROVIDES();

/**
  A block for specifying dependencies of this Component. Note that for each
  dependency we need to have a placeholder, a extern to placeholder in
  header file of the Component, and an entry on requires list below.
*/
REQUIRES_SERVICE_PLACEHOLDER(log_builtins);
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_string);
REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_register);
REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_unregister);
REQUIRES_SERVICE_PLACEHOLDER(mysql_system_variable_reader);
REQUIRES_SERVICE_PLACEHOLDER(status_variable_registration);
REQUIRES_SERVICE_PLACEHOLDER(udf_registration);
REQUIRES_SERVICE_PLACEHOLDER(mysql_thd_security_context);
REQUIRES_SERVICE_PLACEHOLDER(mysql_runtime_error);
REQUIRES_SERVICE_PLACEHOLDER(mysql_security_context_options);
REQUIRES_SERVICE_PLACEHOLDER(mysql_page_track);
REQUIRES_SERVICE_PLACEHOLDER(global_grants_check);
REQUIRES_SERVICE_PLACEHOLDER(mysql_current_thread_reader);
REQUIRES_SERVICE_PLACEHOLDER(psi_memory_v2);

/**
  A list of dependencies.
  The dynamic_loader fetches the references for the below services at the
  component load time and disposes off them at unload.
*/
BEGIN_COMPONENT_REQUIRES(mysqlbackup)
REQUIRES_SERVICE(registry), REQUIRES_SERVICE(log_builtins),
    REQUIRES_SERVICE(log_builtins_string),
    REQUIRES_SERVICE(component_sys_variable_register),
    REQUIRES_SERVICE(mysql_system_variable_reader),
    REQUIRES_SERVICE(component_sys_variable_unregister),
    REQUIRES_SERVICE(status_variable_registration),
    REQUIRES_SERVICE(udf_registration),
    REQUIRES_SERVICE(mysql_thd_security_context),
    REQUIRES_SERVICE(mysql_runtime_error),
    REQUIRES_SERVICE(mysql_security_context_options),
    REQUIRES_SERVICE(mysql_page_track), REQUIRES_SERVICE(global_grants_check),
    REQUIRES_SERVICE(mysql_current_thread_reader),
    REQUIRES_SERVICE(psi_memory_v2), END_COMPONENT_REQUIRES();

/**
  A list of metadata to describe the Component.
*/
BEGIN_COMPONENT_METADATA(mysqlbackup)
METADATA("mysql.mysqlbackup", "Oracle Corporation"),
    METADATA("mysql.license", "Commercial"), END_COMPONENT_METADATA();

/**
  Declaration of the Component.
*/
DECLARE_COMPONENT(mysqlbackup, "mysql:mysqlbackup")
mysqlbackup_init, mysqlbackup_deinit, END_DECLARE_COMPONENT();

/**
  Defines list of Components contained in this library. Note that for now
  we assume that library will have exactly one Component.
*/
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(mysqlbackup)
    END_DECLARE_LIBRARY_COMPONENTS
