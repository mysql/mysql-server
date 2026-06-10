/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.
*/

#include "resource_manager.h"

#include "resource_manager_stats_collector.h"

#include <mysql/components/component_implementation.h>
#include <mysql/components/services/component_status_var_service.h>
#include <mysql/components/services/component_sys_var_service.h>
#include <mysql/components/services/mysql_admin_session.h>
#include <mysql/components/services/mysql_command_services.h>
#include <mysql/components/services/mysql_cond_service.h>
#include <mysql/components/services/mysql_global_variable_attributes_service.h>
#include <mysql/components/services/mysql_mutex_service.h>
#include <mysql/components/services/mysql_status_variable_reader.h>
#include <mysql/components/services/psi_thread_service.h>

std::unique_ptr<gr_resource_manager::Resource_manager_stats_collector>
    stats_collector{nullptr};

#define STRINGIFY(x) #x

/** Placeholders */
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_factory, cmd_factory_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_thread, cmd_thread_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_options, cmd_options_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_query, cmd_query_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_query_result,
                                cmd_query_result_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_field_info, cmd_field_info_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_error_info, cmd_error_info_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_field_metadata,
                                cmd_field_meta_srv);

REQUIRES_SERVICE_PLACEHOLDER_AS(status_variable_registration,
                                status_register_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(component_sys_variable_register,
                                sysvar_register_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(component_sys_variable_unregister,
                                sysvar_unregister_srv);

/** Logs */
REQUIRES_SERVICE_PLACEHOLDER_AS(log_builtins, log_bi);
REQUIRES_SERVICE_PLACEHOLDER_AS(log_builtins_string, log_bs);

REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_cond_v1, cond_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_mutex_v1, mutex_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(psi_thread_v5, thread_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(registry, mysql_srv_reg);

namespace gr_resource_manager {
int register_system_variables();
int unregister_system_variables();

int register_status_variables();
int unregister_status_variables();

const char *component_name = STRINGIFY(GROUP_REPLICATION_RESOURCE_MANAGER);

/**
  Stop/Unregister the part of the components as passed in the argument.

  @param[in] status_vars Unregister the status variables.
  @param[in] system_vars Unregister the system variables.
  @param[in] thread_stop Stop the thread.
*/
void stop_the_component(bool status_vars, bool system_vars, bool thread_stop) {
  if (status_vars) unregister_status_variables();
  if (system_vars) unregister_system_variables();
  if (thread_stop) stats_collector->stop_thread();
  if (stats_collector) {
    stats_collector.reset();
    stats_collector = nullptr;
  }
}

/**
  Component's init functions. Starts thread.

  Registers everything.

  @returns status
    @retval 1 Error
    @retval 0 Success
*/
static mysql_service_status_t gr_resource_manager_init() {
  stats_collector =
      std::make_unique<gr_resource_manager::Resource_manager_stats_collector>();
  if (stats_collector == nullptr) return 1;

  // Data is fetched from thread, so we need to initialize thread first.
  if (register_status_variables()) {
    stop_the_component(true, false, false);
    return 1;
  }
  if (register_system_variables()) {
    stop_the_component(true, true, false);
    return 1;
  }
  if (stats_collector->start_thread()) {
    stop_the_component(true, true, true);
    return 1;
  }

  return 0;
}

/**
  Component's deinit functions

  Deregisters everything that was registered by init.

  @returns status
    @retval 0 Success
*/
static mysql_service_status_t gr_resource_manager_deinit() {
  stop_the_component(true, true, true);
  return 0;
}

}  // namespace gr_resource_manager

/** ================ Component declaration related stuff ================ */

/**
  Component provides

  Intentionally empty as no services are provided by the component
*/
BEGIN_COMPONENT_PROVIDES(GROUP_REPLICATION_RESOURCE_MANAGER)
END_COMPONENT_PROVIDES();

/** List of dependencies */
BEGIN_COMPONENT_REQUIRES(GROUP_REPLICATION_RESOURCE_MANAGER)
REQUIRES_SERVICE_AS(log_builtins, log_bi),
    REQUIRES_SERVICE_AS(log_builtins_string, log_bs),
    REQUIRES_SERVICE_AS(status_variable_registration, status_register_srv),
    REQUIRES_SERVICE_AS(component_sys_variable_register, sysvar_register_srv),
    REQUIRES_SERVICE_AS(component_sys_variable_unregister,
                        sysvar_unregister_srv),
    REQUIRES_SERVICE_AS(mysql_command_factory, cmd_factory_srv),
    REQUIRES_SERVICE_AS(mysql_command_thread, cmd_thread_srv),
    REQUIRES_SERVICE_AS(mysql_command_factory, cmd_factory_srv),
    REQUIRES_SERVICE_AS(mysql_command_thread, cmd_thread_srv),
    REQUIRES_SERVICE_AS(mysql_command_options, cmd_options_srv),
    REQUIRES_SERVICE_AS(mysql_command_query, cmd_query_srv),
    REQUIRES_SERVICE_AS(mysql_command_query_result, cmd_query_result_srv),
    REQUIRES_SERVICE_AS(mysql_command_field_info, cmd_field_info_srv),
    REQUIRES_SERVICE_AS(mysql_command_error_info, cmd_error_info_srv),
    REQUIRES_SERVICE_AS(mysql_command_field_metadata, cmd_field_meta_srv),
    REQUIRES_SERVICE_AS(registry, mysql_srv_reg),
    REQUIRES_SERVICE_AS(mysql_cond_v1, cond_srv),
    REQUIRES_SERVICE_AS(mysql_mutex_v1, mutex_srv),
    REQUIRES_SERVICE_AS(psi_thread_v5, thread_srv), END_COMPONENT_REQUIRES();

/** Component description */
BEGIN_COMPONENT_METADATA(GROUP_REPLICATION_RESOURCE_MANAGER)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"),
    METADATA("mysql.group_replication_resource_manager", "1"),
    END_COMPONENT_METADATA();

/** Component declaration */

DECLARE_COMPONENT(GROUP_REPLICATION_RESOURCE_MANAGER,
                  STRINGIFY(GROUP_REPLICATION_RESOURCE_MANAGER))
gr_resource_manager::gr_resource_manager_init,
    gr_resource_manager::gr_resource_manager_deinit END_DECLARE_COMPONENT();

/** Component contained in this library */
DECLARE_LIBRARY_COMPONENTS
&COMPONENT_REF(GROUP_REPLICATION_RESOURCE_MANAGER)
    END_DECLARE_LIBRARY_COMPONENTS
