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

#ifndef MRS_COMPONENT_H
#define MRS_COMPONENT_H

#include <atomic>
#include <memory>
#include <string>

#include "components/mysql_rest_service/mrs_component_backend.h"
#include "mysql/components/component_implementation.h"
#include "mysql/components/service.h"
#include "mysql/components/services/component_status_var_service.h"
#include "mysql/components/services/dynamic_privilege.h"
#include "mysql/components/services/mysql_command_services.h"
#include "mysql/components/services/mysql_command_session_state.h"
#include "mysql/components/services/mysql_debug_keyword_service.h"
#include "mysql/components/services/mysql_debug_sync_service.h"
#include "mysql/components/services/mysql_statement_service.h"
#include "mysql/components/services/mysql_status_variable_reader.h"
#include "mysql/components/services/mysql_string.h"
#include "mysql/components/services/mysql_system_variable.h"
#include "mysql/components/services/mysql_thd_attributes.h"
#include "mysql/components/services/psi_thread.h"
#include "mysql/components/services/security_context.h"
#include "mysql/components/services/table_access_service.h"
#include "mysql/components/services/udf_metadata.h"
#include "mysql/components/services/udf_registration.h"

#include "components/mysql_rest_service/mrs_component_log.h"
#include "components/mysql_rest_service/mrs_component_server_config.h"
#include "components/mysql_rest_service/mrs_component_status_vars.h"
#include "components/mysql_rest_service/mrs_component_system_vars.h"
#include "components/mysql_rest_service/mrs_component_udfs.h"

#include <mysqld_error.h> /* Errors */

/**
  Component Service placeholders
*/

// Logging services
extern REQUIRES_SERVICE_PLACEHOLDER_AS(log_builtins, log_bi);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(log_builtins_string, log_bs);

// Sysvar reader
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_system_variable_reader,
                                       sysvar_reader);

// Sysvar registry
extern REQUIRES_SERVICE_PLACEHOLDER_AS(component_sys_variable_register,
                                       sysvar_register);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(component_sys_variable_unregister,
                                       sysvar_unregister);

// Statusvar reader and registry
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_status_variable_string,
                                       statusvar_reader);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(status_variable_registration,
                                       statusvar_register);

// Command-service sessions and text query execution
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_factory, cmd_factory);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_options, cmd_options);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_query, cmd_query);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_thread, cmd_thread);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_query_result,
                                       cmd_query_result);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_field_info,
                                       cmd_field_info);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_error_info,
                                       cmd_error_info);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_field_metadata,
                                       cmd_field_meta);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_session_state,
                                       cmd_session_state);

// Prepared statement execution
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_factory, stmt_factory);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_execute, stmt_execute);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_metadata, stmt_metadata);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_bind, stmt_bind);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_get_string, stmt_get_string);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_get_time, stmt_get_time);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_get_double, stmt_get_double);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_get_unsigned_integer,
                                       stmt_get_unsigned_integer);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_get_integer,
                                       stmt_get_integer);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_diagnostics,
                                       stmt_diagnostics);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_resultset_metadata,
                                       stmt_resultset_metadata);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_result, stmt_result);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_stmt_attributes, stmt_attributes);

// MySQL thread context
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_current_thread_reader);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_thd_attributes, thd_attributes);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_thd_security_context,
                                       thread_security_ctx);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_security_context_factory,
                                       security_ctx_factory);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(
    mysql_account_database_security_context_lookup, security_ctx_lookup);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_security_context_options,
                                       thread_security_ctx_opts);

// Dynamic privilege management
extern REQUIRES_SERVICE_PLACEHOLDER_AS(dynamic_privilege_register,
                                       dynamic_privilege_registry);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(global_grants_check,
                                       global_grants_checker);

// MySQL String
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_factory, string_factory);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_get_data_in_charset,
                                       string_get_data_in_charset);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_converter,
                                       string_converter);

// Table access
extern REQUIRES_SERVICE_PLACEHOLDER_AS(table_access_factory_v1,
                                       table_access_factory);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(table_access_v1, table_access);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(field_varchar_access_v1,
                                       field_varchar_access);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(table_access_scan_v1, table_access_scan);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(field_access_nullability_v1,
                                       field_access_nullability);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(field_any_access_v1, field_any_access);

// UDF registry
extern REQUIRES_SERVICE_PLACEHOLDER_AS(udf_registration, udf_registry);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_udf_metadata, udf_metadata);

// DBUG_EXECUTE_IF for Components
#if !defined(NDEBUG)
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_debug_keyword_service,
                                       debug_keyword_service);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_debug_sync_service);
#endif

namespace mrs_component {

void register_io_thread_instrumentation();
void initialize_io_thread_instrumentation(size_t index);
void deinitialize_io_thread_instrumentation(size_t index);

constexpr const char kMysqlRestServiceAdmin[] = "MYSQL_REST_SERVICE_ADMIN";

class MRSComponent;
class ServerConfig;
class SystemVars;

extern std::unique_ptr<MRSComponent> s_mrs_component;  // main handler

/**
  Returns the active MRS component instance.
*/
MRSComponent &mrs_component();

/**
  Coordinates registration and lifecycle of the MySQL REST Service component.
*/
class MRSComponent {
 public:
  MRSComponent() = default;
  ~MRSComponent() = default;

  MRSComponent(const MRSComponent &) = delete;
  MRSComponent &operator=(const MRSComponent &) = delete;
  MRSComponent(MRSComponent &&) = delete;
  MRSComponent &operator=(MRSComponent &&) = delete;

  [[nodiscard]] const char *name() const { return m_name.c_str(); }

  int server_config_init();
  int system_vars_init();
  int system_vars_register();
  int system_vars_unregister();
  int status_vars_init();
  int status_vars_register();
  int status_vars_unregister();
  int admin_privilege_register();
  int admin_privilege_unregister();
  int udfs_init();
  int udfs_register();
  int udfs_unregister();

  int backend_init();
  int backend_start();
  int backend_restart();
  int backend_stop();

  mysql_service_status_t component_init();
  mysql_service_status_t component_deinit();

  [[nodiscard]] bool initialized() const { return m_initialized; }
  void set_initialized(bool initialized) { m_initialized = initialized; }

  ServerConfig &server_config() { return m_server_config; }
  SystemVars &system_vars() { return m_system_vars; }
  StatusVars &status_vars() { return m_status_vars; }
  UDFs &udfs() { return m_udfs; }
  Backend &backend() { return m_backend; }
  const Log &log() const { return m_log; }

 private:
  const std::string m_name{"MySQL Rest Service Component"};

  std::atomic<bool> m_initialized = false;
  bool m_system_vars_registered = false;
  bool m_status_vars_registered = false;
  bool m_admin_privilege_registered = false;
  bool m_udfs_registered = false;

  ServerConfig m_server_config;
  SystemVars m_system_vars;
  StatusVars m_status_vars;
  UDFs m_udfs;
  Log m_log;
  Backend m_backend;
};

}  // namespace mrs_component

#endif  // MRS_COMPONENT_H
