# Copyright (c) 2021, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

INCLUDE(GenerateExportHeader)

ADD_LIBRARY(mysql_rest_service_if INTERFACE)

TARGET_INCLUDE_DIRECTORIES(mysql_rest_service_if
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/
    ${CMAKE_CURRENT_BINARY_DIR}/../include/
    )

ADD_LIBRARY(mysql_rest_service_db
  STATIC
  mrs/database/filter_object_generator.cc
  mrs/database/monitor/schema_monitor.cc
  mrs/database/monitor/schema_monitor_factory.cc
  mrs/database/query_entries_auth_privileges.cc
  mrs/database/query_changes_content_file.cc
  mrs/database/query_changes_db_object.cc
  mrs/database/query_changes_state.cc
  mrs/database/query_entries_audit_log.cc
  mrs/database/query_entries_auth_app.cc
  mrs/database/query_entries_auth_role.cc
  mrs/database/query_entries_content_file.cc
  mrs/database/query_entries_db_object.cc
  mrs/database/query_entries_object.cc
  mrs/database/query_entry_auth_user.cc
  mrs/database/query_entry_content_file.cc
  mrs/database/query_entry_fields.cc
  mrs/database/query_factory.cc
  mrs/database/query_entry_group_row_security.cc
  mrs/database/query_uuid.cc
  mrs/database/query_rest_table.cc
  mrs/database/query_rest_table_single_row.cc
  mrs/database/query_rest_sp.cc
  mrs/database/query_rest_function.cc
  mrs/database/query_rest_sp_media.cc
  mrs/database/query_rest_table_updater.cc
  mrs/database/query_state.cc
  mrs/database/query_statistics.cc
  mrs/database/query_version.cc
  mrs/database/query_warnings.cc
  mrs/database/helper/query.cc
  mrs/database/helper/query_table_columns.cc
  mrs/database/helper/query_audit_log_maxid.cc
  mrs/database/helper/query_retry_on_ro.cc
  mrs/database/helper/query_gtid_executed.cc
  mrs/database/helper/object_query.cc
  mrs/database/helper/object_checksum.cc
  mrs/database/helper/sp_function_query.cc
)

TARGET_LINK_LIBRARIES(mysql_rest_service_db
  mysql_rest_service_if
  harness-library
  router_lib
  http_common
  routing)

TARGET_INCLUDE_DIRECTORIES(mysql_rest_service_db
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

ADD_LIBRARY(mysql_rest_service_json
  mrs/json/response_json_template.cc
  mrs/json/json_template_factory.cc
  mrs/json/response_sp_json_template_unnest.cc
  mrs/json/response_sp_json_template_nest.cc
  )

TARGET_LINK_LIBRARIES(mysql_rest_service_json
  mysql_rest_service_if
  harness-library
  router_lib
  http_common
  )

TARGET_INCLUDE_DIRECTORIES(mysql_rest_service_json
  PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    )

ADD_LIBRARY(mysql_rest_service_impl
  collector/counted_mysql_session.cc
  collector/mysql_cache_manager.cc
  helper/media_detector.cc
  helper/media_type.cc
  helper/json/to_string.cc
  helper/json/to_sqlstring.cc
  helper/json/jvalue.cc
  helper/token/jwt.cc
  helper/mysql_column_types.cc
  helper/mysql_numeric_value.cc
  mrs/authentication/helper/crypto.cc
  mrs/authentication/helper/http_result.cc
  mrs/authentication/helper/scram.cc
  mrs/authentication/helper/option_parser.cc
  mrs/authentication/authorize_manager.cc
  mrs/authentication/auth_handler_factory.cc
  mrs/authentication/basic_handler.cc
  mrs/authentication/oauth2_facebook_handler.cc
  mrs/authentication/oauth2_twitter_handler.cc
  mrs/authentication/oauth2_google_handler.cc
  mrs/authentication/oauth2_handler.cc
  mrs/authentication/www_authentication_handler.cc
  mrs/authentication/sasl_handler.cc
  mrs/authentication/scram_handler.cc
  mrs/http/cookie.cc
  mrs/http/header_accept.cc
  mrs/http/utilities.cc
  mrs/http/session_manager.cc
  mrs/users/user_manager.cc
  mrs/object_factory.cc
  mrs/object_manager.cc
  mrs/object.cc
  mrs/object_schema.cc
  mrs/object_static_file.cc
  mrs/router_observation_entities.cc
  mrs/rest/handler.cc
  mrs/rest/handler_authorize.cc
  mrs/rest/handler_authorize_apps.cc
  mrs/rest/handler_authorize_common.cc
  mrs/rest/handler_authorize_ok.cc
  mrs/rest/handler_factory.cc
  mrs/rest/handler_file.cc
  mrs/rest/handler_function.cc
  mrs/rest/handler_user.cc
  mrs/rest/handler_table.cc
  mrs/rest/handler_string.cc
  mrs/rest/handler_object_metadata.cc
  mrs/rest/handler_is_authorized.cc
  mrs/rest/handler_unauthorize.cc
  mrs/rest/handler_schema_metadata.cc
  mrs/rest/handler_sp.cc
  mrs/monitored/gtid_functions.cc
  mrs/monitored/query_retry_on_ro.cc
  )

TARGET_INCLUDE_DIRECTORIES(mysql_rest_service_impl
  PRIVATE
    ${PROJECT_SOURCE_DIR}/src/harness/include/
    ${PROJECT_SOURCE_DIR}/src/metadata_cache/include/
    ${PROJECT_SOURCE_DIR}/src/routing/include/
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    )

TARGET_LINK_LIBRARIES(mysql_rest_service_impl
  http_client
  rest_api
  mysql_rest_service_db
  )

ADD_HARNESS_PLUGIN(mysql_rest_service
  SOURCES
  mysql_rest_service_plugin_config.cc
  mysql_rest_service_plugin.cc
  REQUIRES io_component;router_lib;routing;http_server;mysql_rest_service_impl;mysql_rest_service_db;mysql_rest_service_json
  DESTINATION "${ROUTER_INSTALL_PLUGINDIR}"
  )

TARGET_INCLUDE_DIRECTORIES(mysql_rest_service
  PRIVATE
    ${PROJECT_SOURCE_DIR}/src/harness/include/
    ${PROJECT_SOURCE_DIR}/src/metadata_cache/include/
    ${PROJECT_SOURCE_DIR}/src/routing/include/
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    )



GENERATE_EXPORT_HEADER(mysql_rest_service
  EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/mysql_rest_service_export.h)

