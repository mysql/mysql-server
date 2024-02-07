/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef TEST_SERVER_TELEMETRY_REQUIRED_SERVICES_INCLUDED
#define TEST_SERVER_TELEMETRY_REQUIRED_SERVICES_INCLUDED

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/component_status_var_service.h>
#include <mysql/components/services/component_sys_var_service.h>
#include <mysql/components/services/mysql_current_thread_reader.h>
#include <mysql/components/services/mysql_query_attributes.h>
#include <mysql/components/services/mysql_server_telemetry_traces_service.h>
#include <mysql/components/services/mysql_thd_attributes.h>
#include <mysql/components/services/mysql_thd_store_service.h>
#include <mysql/components/services/pfs_notification.h>
#include <mysql/components/services/psi_statement.h>
#include <mysql/components/services/security_context.h>
#include <mysql/components/services/udf_registration.h>

/* A place to specify component-wide declarations, including declarations of
 *   placeholders for Service dependencies. */

extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_server_telemetry_traces_v1,
                                       telemetry_v1_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_query_attributes_iterator,
                                       qa_iterator_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_query_attribute_string,
                                       qa_string_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_query_attribute_isnull,
                                       qa_isnull_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_converter,
                                       string_converter_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_charset_converter,
                                       charset_converter_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_charset, charset_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_factory,
                                       string_factory_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(component_sys_variable_register,
                                       sysvar_register_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(component_sys_variable_unregister,
                                       sysvar_unregister_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_thd_attributes,
                                       thd_attributes_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_current_thread_reader,
                                       current_thd_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_thd_store, thd_store_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_thd_security_context, thd_scx_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_security_context_options,
                                       scx_options_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(pfs_notification_v3, notification_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(status_variable_registration,
                                       statvar_register_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(udf_registration, udf_registration_srv);

#endif /* TEST_SERVER_TELEMETRY_REQUIRED_SERVICES_INCLUDED */
