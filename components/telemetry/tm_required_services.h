/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef TELEMETRY_REQUIRED_SERVICES_H_INCLUDED
#define TELEMETRY_REQUIRED_SERVICES_H_INCLUDED

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/component_status_var_service.h>
#include <mysql/components/services/component_sys_var_service.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/components/services/mysql_cond_service.h>
#include <mysql/components/services/mysql_connection_attributes_iterator.h>
#include <mysql/components/services/mysql_current_thread_reader.h>
#include <mysql/components/services/mysql_mutex_service.h>
#include <mysql/components/services/mysql_query_attributes.h>
#include <mysql/components/services/mysql_server_telemetry_logs_service.h>
#include <mysql/components/services/mysql_server_telemetry_metrics_service.h>
#include <mysql/components/services/mysql_server_telemetry_traces_service.h>
#include <mysql/components/services/mysql_string.h>
#include <mysql/components/services/mysql_thd_store_service.h>
#include <mysql/components/services/pfs_notification.h>
#include <mysql/components/services/psi_idle_service.h>
#include <mysql/components/services/psi_metric_service.h>
#include <mysql/components/services/psi_stage_service.h>
#include <mysql/components/services/psi_thread_service.h>
#include <mysql/components/services/registry.h>

#include <mysql/components/services/telemetry_resource_provider_service.h>
#include <mysql/components/services/telemetry_secret_provider_service.h>

namespace telemetry {

extern REQUIRES_SERVICE_PLACEHOLDER_AS(status_variable_registration,
                                       statvar_register_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(component_sys_variable_register,
                                       sysvar_register_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(component_sys_variable_unregister,
                                       sysvar_unregister_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(log_builtins, log_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(log_builtins_string, log_string_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(pfs_notification_v3, notification_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_current_thread_reader,
                                       current_thd_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_connection_attributes_iterator,
                                       con_attr_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_thd_store, thd_store_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_server_telemetry_traces_v1,
                                       telemetry_traces_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_server_telemetry_metrics_v1,
                                       telemetry_metrics_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_server_telemetry_logs,
                                       telemetry_logs_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_query_attributes_iterator,
                                       qa_iter_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_query_attribute_string,
                                       qa_string_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_get_data_in_charset,
                                       string_get_data_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_factory,
                                       string_factory_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_converter,
                                       string_converter_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_cond_v1, cond_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_mutex_v1, mutex_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(psi_metric_v1, metric_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(psi_stage_v1, stage_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(psi_thread_v7, thread_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(psi_idle_v1, idle_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(registry_registration, reg_reg);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(registry, reg_srv);
}  // namespace telemetry

#endif /* TELEMETRY_REQUIRED_SERVICES_H_INCLUDED */
