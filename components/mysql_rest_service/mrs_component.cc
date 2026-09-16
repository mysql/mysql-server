/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mrs_component.h"

#include "mysql/components/component_implementation.h"
#include "mysql/components/my_service.h"
#include "mysql/components/services/mysql_runtime_error_service.h"
#include "scope_guard.h"

#include "mrs_component_backend.h"
#include "mrs_component_command_executor.h"
#include "mrs_component_log.h"
#include "mrs_component_mysql_command_service_session.h"
#include "mrs_component_server_config.h"
#include "mrs_component_system_vars.h"

/**
  Component Service Placeholders
*/

// Runtime error service
REQUIRES_SERVICE_PLACEHOLDER(mysql_runtime_error);

// Logging services, declared in componentlog_plugin.cc
// REQUIRES_SERVICE_PLACEHOLDER_AS(log_builtins, log_bi);
// REQUIRES_SERVICE_PLACEHOLDER_AS(log_builtins_string, log_bs);

// Sysvar reader
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_system_variable_reader, sysvar_reader);

// Sysvar registry
REQUIRES_SERVICE_PLACEHOLDER_AS(component_sys_variable_register,
                                sysvar_register);
REQUIRES_SERVICE_PLACEHOLDER_AS(component_sys_variable_unregister,
                                sysvar_unregister);

// Statusvar reader and registry
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_status_variable_string, statusvar_reader);
REQUIRES_SERVICE_PLACEHOLDER_AS(status_variable_registration,
                                statusvar_register);

// Command-service sessions and text query execution
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_factory, cmd_factory);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_options, cmd_options);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_query, cmd_query);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_thread, cmd_thread);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_query_result, cmd_query_result);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_field_info, cmd_field_info);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_error_info, cmd_error_info);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_field_metadata, cmd_field_meta);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_session_state, cmd_session_state);
REQUIRES_PSI_THREAD_SERVICE_PLACEHOLDER;

// Prepared statement execution
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_factory, stmt_factory);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_execute, stmt_execute);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_metadata, stmt_metadata);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_bind, stmt_bind);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_get_string, stmt_get_string);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_get_time, stmt_get_time);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_get_double, stmt_get_double);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_get_unsigned_integer,
                                stmt_get_unsigned_integer);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_get_integer, stmt_get_integer);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_diagnostics, stmt_diagnostics);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_resultset_metadata,
                                stmt_resultset_metadata);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_result, stmt_result);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_attributes, stmt_attributes);

// MySQL thread context
REQUIRES_SERVICE_PLACEHOLDER(mysql_current_thread_reader);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_thd_attributes, thd_attributes);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_thd_security_context,
                                thread_security_ctx);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_security_context_factory,
                                security_ctx_factory);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_account_database_security_context_lookup,
                                security_ctx_lookup);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_security_context_options,
                                thread_security_ctx_opts);

// Dynamic privilege management
REQUIRES_SERVICE_PLACEHOLDER_AS(dynamic_privilege_register,
                                dynamic_privilege_registry);
REQUIRES_SERVICE_PLACEHOLDER_AS(global_grants_check, global_grants_checker);

// MySQL String
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_factory, string_factory);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_get_data_in_charset,
                                string_get_data_in_charset);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_converter, string_converter);

// Table access
REQUIRES_SERVICE_PLACEHOLDER_AS(table_access_factory_v1, table_access_factory);
REQUIRES_SERVICE_PLACEHOLDER_AS(table_access_v1, table_access);
REQUIRES_SERVICE_PLACEHOLDER_AS(field_varchar_access_v1, field_varchar_access);
REQUIRES_SERVICE_PLACEHOLDER_AS(table_access_scan_v1, table_access_scan);
REQUIRES_SERVICE_PLACEHOLDER_AS(field_access_nullability_v1,
                                field_access_nullability);
REQUIRES_SERVICE_PLACEHOLDER_AS(field_any_access_v1, field_any_access);

// UDF registry
REQUIRES_SERVICE_PLACEHOLDER_AS(udf_registration, udf_registry);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_udf_metadata, udf_metadata);

// DBUG_EXECUTE_IF for Components
#if !defined(NDEBUG)
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_debug_keyword_service,
                                debug_keyword_service);
REQUIRES_SERVICE_PLACEHOLDER(mysql_debug_sync_service);
#endif

namespace mrs_component {

namespace {

PSI_thread_key mrs_io_thread_key{};
PSI_thread_info mrs_io_thread_info{
    &mrs_io_thread_key,
    "mrs_io",
    "mrs_io",
    PSI_FLAG_USER,
    PSI_VOLATILITY_PROVISIONING,
    "MySQL REST Service embedded I/O worker",
};

}  // namespace

// TODO: Instrument other MRS threads that interact with SQL sessions.
void register_io_thread_instrumentation() {
  if (mrs_io_thread_key != 0) return;

  PSI_THREAD_CALL(register_thread)
  ("mysql_rest_service", &mrs_io_thread_info, 1);
}

void initialize_io_thread_instrumentation(size_t index) {
  initialize_worker_command_thread();

  static thread_local char identity;
  auto *psi =
      PSI_THREAD_CALL(new_thread)(mrs_io_thread_key, index, &identity, 0);
  PSI_THREAD_CALL(set_thread_os_id)(psi);
  PSI_THREAD_CALL(set_thread)(psi);
}

void deinitialize_io_thread_instrumentation(size_t) {
  deinitialize_worker_command_thread();
}

std::unique_ptr<MRSComponent> s_mrs_component{nullptr};  // main handler

// main handler accessor
MRSComponent &mrs_component() {
  assert(s_mrs_component != nullptr);

  return *s_mrs_component;
}

/**
  Read all needed Server system variables
*/
int MRSComponent::server_config_init() {
  MRS_LOG_INFO("Fetching required system variables from the Server");
  return server_config().fetch_required_sysvars();
}

/**
  Initialize all system variables
*/
int MRSComponent::system_vars_init() {
  m_system_vars_registered = false;
  return system_vars().initialize_all();
}

/**
  Register all system variables
*/
int MRSComponent::system_vars_register() {
  if (m_system_vars_registered) {
    MRS_LOG_INFO("Component's System Variables already registered");
    return 0;
  }

  if (system_vars().register_all() != 0) {
    return 1;
  }

  m_system_vars_registered = true;

  return 0;
}

/**
  Unregister all system variables
*/
int MRSComponent::system_vars_unregister() {
  if (system_vars().unregister_all() != 0) return 1;

  m_system_vars_registered = false;

  return 0;
}

/**
  Initialize all status variables
*/
int MRSComponent::status_vars_init() {
  m_status_vars_registered = false;
  return status_vars().initialize_all();
}

/**
  Register all status variables
*/
int MRSComponent::status_vars_register() {
  if (m_status_vars_registered) {
    MRS_LOG_INFO("Component's Status Variables already registered");
    return 0;
  }

  if (status_vars().register_all() != 0) {
    return 1;
  }

  m_status_vars_registered = true;

  return 0;
}

/**
  Unregister all status variables
*/
int MRSComponent::status_vars_unregister() {
  if (status_vars().unregister_all() != 0) return 1;

  m_status_vars_registered = false;

  return 0;
}

int MRSComponent::admin_privilege_register() {
  if (m_admin_privilege_registered) {
    MRS_LOG_INFO("Dynamic privilege '%s' already registered",
                 kMysqlRestServiceAdmin);
    return 0;
  }

  if (dynamic_privilege_registry->register_privilege(
          kMysqlRestServiceAdmin, std::strlen(kMysqlRestServiceAdmin)) != 0) {
    MRS_LOG_ERROR("Failed to register dynamic privilege '%s'",
                  kMysqlRestServiceAdmin);
    return 1;
  }

  m_admin_privilege_registered = true;
  return 0;
}

int MRSComponent::admin_privilege_unregister() {
  if (!m_admin_privilege_registered) return 0;

  if (dynamic_privilege_registry->unregister_privilege(
          kMysqlRestServiceAdmin, std::strlen(kMysqlRestServiceAdmin)) != 0) {
    MRS_LOG_ERROR("Failed to unregister dynamic privilege '%s'",
                  kMysqlRestServiceAdmin);
    return 1;
  }

  m_admin_privilege_registered = false;
  return 0;
}

/**
  Initialize all UDFs
*/
int MRSComponent::udfs_init() {
  m_udfs_registered = false;
  return udfs().initialize_all();
}

/**
  Register all UDFs
*/
int MRSComponent::udfs_register() {
  if (m_udfs_registered) {
    MRS_LOG_INFO("Component's UDFs already registered");
    return 0;
  }

  if (udfs().register_all() != 0) {
    return 1;
  }

  m_udfs_registered = true;

  return 0;
}

/**
  Unregister all UDFs
*/
int MRSComponent::udfs_unregister() {
  if (udfs().unregister_all() != 0) return 1;

  m_udfs_registered = false;

  return 0;
}

int MRSComponent::backend_init() {
  // The automatic install-time start may use the installer's context to
  // provision internal accounts.
  if (backend().start(AccountProvisioning::kAllowInstallerContext) != 0)
    return 1;

  return 0;
}

int MRSComponent::backend_start() {
  // Lifecycle callers do not provide account-provisioning authority. A pending
  // install-time operation may still resume with the installer's context.
  if (backend().start(AccountProvisioning::kValidateOnly) != 0) return 1;

  return 0;
}

int MRSComponent::backend_restart() {
  if (backend().restart() != 0) return 1;

  return 0;
}

int MRSComponent::backend_stop() {
  if (backend().stop() != 0) return 1;

  return 0;
}

/**
  Initialize the Main handler class
*/
mysql_service_status_t MRSComponent::component_init() {
  auto error = [](int errcode) {
    mysql_error_service_printf(errcode, MYF(0));
    return 1;
  };

  set_initialized(false);

  auto rollback = create_scope_guard([this] { (void)component_deinit(); });

  MRS_LOG_SYSTEM("Starting...");

  MRS_LOG_INFO("Initializing Server Configuration handler");

  if (server_config_init() != 0) return error(ER_MRS_CONFIG_INIT_ERROR);

  MRS_LOG_INFO("Initializing System Variables handler");

  if (system_vars_init() != 0) return error(ER_MRS_SYSVARS_INIT_ERROR);

  MRS_LOG_INFO("Registering System Variables");

  if (system_vars_register() != 0) return error(ER_MRS_SYSVARS_REGISTER_ERROR);

  MRS_LOG_INFO("Initializing Status Variables handler");

  if (status_vars_init() != 0) return error(ER_MRS_STATUSVARS_INIT_ERROR);

  MRS_LOG_INFO("Registering Status Variables");

  if (status_vars_register() != 0)
    return error(ER_MRS_STATUSVARS_REGISTER_ERROR);

  MRS_LOG_INFO("Registering dynamic privilege '%s'", kMysqlRestServiceAdmin);

  if (admin_privilege_register() != 0) return error(ER_MRS_UDFS_REGISTER_ERROR);

  MRS_LOG_INFO("Initializing UDFs handler");

  if (udfs_init() != 0) return error(ER_MRS_UDFS_INIT_ERROR);

  MRS_LOG_INFO("Registering UDFs");

  if (udfs_register() != 0) return error(ER_MRS_UDFS_REGISTER_ERROR);

  set_initialized(true);
  // Lifecycle UDFs may control the backend only after all component SQL
  // interfaces have been registered successfully.
  backend().enable_lifecycle_operations();

  MRS_LOG_INFO("Initializing Backend");

  if (backend_init() != 0) return error(ER_MRS_BACKEND_INIT_ERROR);

  rollback.release();
  return 0;
}

/**
  Deinitialize the Main handler class
*/
mysql_service_status_t MRSComponent::component_deinit() {
  MRS_LOG_SYSTEM("Stopping...");

  MRS_LOG_INFO("Stopping Backend");
  // Disable lifecycle requests before unregistering their SQL interfaces.
  const int backend_error = backend().shutdown();
  set_initialized(false);
  // Do not release component interfaces while backend threads may remain.
  if (backend_error != 0) return 1;

  int error = 0;

  MRS_LOG_INFO("Unregistering System Variables");
  error |= system_vars_unregister();

  MRS_LOG_INFO("Unregistering Status Variables");
  error |= status_vars_unregister();

  MRS_LOG_INFO("Unregistering UDFs");
  error |= udfs_unregister();

  MRS_LOG_INFO("Unregistering dynamic privilege '%s'", kMysqlRestServiceAdmin);
  error |= admin_privilege_unregister();

  if (error == 0) MRS_LOG_SYSTEM("Stopped.");

  return error != 0;
}

/**
  Initialize the MySQL Rest Service Component.
*/
static mysql_service_status_t mysql_rest_service_init() {
  s_mrs_component = std::make_unique<MRSComponent>();

  if (mrs_component().component_init() != 0) return 1;

  return 0;
}

/**
  Deinitialize the MySQL Rest Service Component.
*/
static mysql_service_status_t mysql_rest_service_deinit() {
  if (mrs_component().component_deinit() != 0) return 1;

  s_mrs_component.reset();

  return 0;
}

}  //  namespace mrs_component

// clang-format off

/**
  Component Declaration
*/
BEGIN_COMPONENT_PROVIDES(component_mysql_rest_service)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(component_mysql_rest_service)
// Dependencies
    REQUIRES_SERVICE(mysql_runtime_error),
    REQUIRES_SERVICE_AS(log_builtins, log_bi),
    REQUIRES_SERVICE_AS(log_builtins_string, log_bs),
    REQUIRES_SERVICE_AS(mysql_system_variable_reader, sysvar_reader),
    REQUIRES_SERVICE_AS(component_sys_variable_register, sysvar_register),
    REQUIRES_SERVICE_AS(component_sys_variable_unregister, sysvar_unregister),
    REQUIRES_SERVICE_AS(status_variable_registration, statusvar_register),
    REQUIRES_SERVICE_AS(mysql_status_variable_string, statusvar_reader),
    REQUIRES_SERVICE_AS(mysql_command_factory, cmd_factory),
    REQUIRES_SERVICE_AS(mysql_command_options, cmd_options),
    REQUIRES_SERVICE_AS(mysql_command_query, cmd_query),
    REQUIRES_SERVICE_AS(mysql_command_thread, cmd_thread),
    REQUIRES_SERVICE_AS(mysql_command_query_result, cmd_query_result),
    REQUIRES_SERVICE_AS(mysql_command_field_info, cmd_field_info),
    REQUIRES_SERVICE_AS(mysql_command_error_info, cmd_error_info),
    REQUIRES_SERVICE_AS(mysql_command_field_metadata, cmd_field_meta),
    REQUIRES_SERVICE_AS(mysql_command_session_state, cmd_session_state),
    REQUIRES_PSI_THREAD_SERVICE,
    REQUIRES_SERVICE_AS(mysql_stmt_factory, stmt_factory),
    REQUIRES_SERVICE_AS(mysql_stmt_execute, stmt_execute),
    REQUIRES_SERVICE_AS(mysql_stmt_metadata, stmt_metadata),
    REQUIRES_SERVICE_AS(mysql_stmt_bind, stmt_bind),
    REQUIRES_SERVICE_AS(mysql_stmt_get_string, stmt_get_string),
    REQUIRES_SERVICE_AS(mysql_stmt_get_time, stmt_get_time),
    REQUIRES_SERVICE_AS(mysql_stmt_get_double, stmt_get_double),
    REQUIRES_SERVICE_AS(mysql_stmt_get_unsigned_integer,
                                stmt_get_unsigned_integer),
    REQUIRES_SERVICE_AS(mysql_stmt_get_integer, stmt_get_integer),
    REQUIRES_SERVICE_AS(mysql_stmt_diagnostics, stmt_diagnostics),
    REQUIRES_SERVICE_AS(mysql_stmt_resultset_metadata,
                                stmt_resultset_metadata),
    REQUIRES_SERVICE_AS(mysql_stmt_result, stmt_result),
    REQUIRES_SERVICE_AS(mysql_stmt_attributes, stmt_attributes),
    REQUIRES_SERVICE(mysql_current_thread_reader),
    REQUIRES_SERVICE_AS(mysql_thd_attributes, thd_attributes),
    REQUIRES_SERVICE_AS(mysql_thd_security_context, thread_security_ctx),
    REQUIRES_SERVICE_AS(mysql_security_context_factory,
                        security_ctx_factory),
    REQUIRES_SERVICE_AS(mysql_account_database_security_context_lookup,
                        security_ctx_lookup),
    REQUIRES_SERVICE_AS(mysql_security_context_options,
                        thread_security_ctx_opts),
    REQUIRES_SERVICE_AS(dynamic_privilege_register,
                        dynamic_privilege_registry),
    REQUIRES_SERVICE_AS(global_grants_check, global_grants_checker),
    REQUIRES_SERVICE_AS(mysql_string_factory, string_factory),
    REQUIRES_SERVICE_AS(mysql_string_get_data_in_charset,
                                string_get_data_in_charset),
    REQUIRES_SERVICE_AS(mysql_string_converter, string_converter),
    REQUIRES_SERVICE_AS(table_access_factory_v1, table_access_factory),
    REQUIRES_SERVICE_AS(table_access_v1, table_access),
    REQUIRES_SERVICE_AS(field_varchar_access_v1, field_varchar_access),
    REQUIRES_SERVICE_AS(table_access_scan_v1, table_access_scan),
    REQUIRES_SERVICE_AS(field_access_nullability_v1, field_access_nullability),
    REQUIRES_SERVICE_AS(field_any_access_v1, field_any_access),
    REQUIRES_SERVICE_AS(udf_registration, udf_registry),
    REQUIRES_SERVICE_AS(mysql_udf_metadata, udf_metadata),
#if !defined(NDEBUG)
    REQUIRES_SERVICE_AS(mysql_debug_keyword_service, debug_keyword_service),
    REQUIRES_SERVICE(mysql_debug_sync_service),
#endif
END_COMPONENT_REQUIRES();

// Component description/metadata
BEGIN_COMPONENT_METADATA(component_mysql_rest_service)
  METADATA("mysql.author", "Oracle Corporation"),
  METADATA("mysql.license", "GPL"),
END_COMPONENT_METADATA();

// Component declaration
DECLARE_COMPONENT(component_mysql_rest_service,
                  "mysql:component_mysql_rest_service")
  mrs_component::mysql_rest_service_init,
  mrs_component::mysql_rest_service_deinit
END_DECLARE_COMPONENT();

// Components contained in this library
DECLARE_LIBRARY_COMPONENTS
  &COMPONENT_REF(component_mysql_rest_service)
END_DECLARE_LIBRARY_COMPONENTS

    // clang-format on
