# Copyright (c) 2021, 2026, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
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

# enable std::string support for rapidjson
ADD_DEFINITIONS(-DRAPIDJSON_HAS_STDSTRING=1)

TARGET_INCLUDE_DIRECTORIES(mysql_rest_service_if
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/
    ${CMAKE_CURRENT_BINARY_DIR}/../include/
    )

ADD_STATIC_LIBRARY(mysql_rest_service_helper
  helper/media_detector.cc
  helper/media_type.cc
  helper/json/to_string.cc
  helper/json/to_sqlstring.cc
  helper/json/jvalue.cc
  helper/json/merge.cc
  helper/token/jwt.cc
  helper/mysql_column_types.cc
  helper/mysql_numeric_value.cc
  helper/digester/sha256_digest.cc

  COMPILE_DEFINITIONS
  PRIVATE MYSQL_ROUTER_LOG_DOMAIN="mrs_helper"

  LINK_LIBRARIES
  mysql_rest_service_if
  harness-library
  router_lib
  http_common

  INCLUDE_DIRECTORIES
  PRIVATE ${PROJECT_SOURCE_DIR}/src/router/include/
  )

SET(mysql_rest_service_db_includes
  ${PROJECT_SOURCE_DIR}/src/http/include/
)

IF(MRS_JIT_EXECUTOR_LIB)
  list(APPEND mysql_rest_service_db_includes
    ${CMAKE_CURRENT_BINARY_DIR}/../../jit_executor/include/
    ${PROJECT_SOURCE_DIR}/src/jit_executor/include/)
ENDIF()

ADD_STATIC_LIBRARY(mysql_rest_service_db
  mrs/database/filter_object_generator.cc
  mrs/database/monitor/schema_monitor.cc
  mrs/database/monitor/slow_query_monitor.cc
  mrs/database/monitor/schema_monitor_factory.cc
  mrs/database/query_entries_auth_privileges.cc
  mrs/database/query_changes_auth_user.cc
  mrs/database/query_changes_content_file.cc
  mrs/database/query_changes_content_set.cc
  mrs/database/query_changes_db_object.cc
  mrs/database/query_changes_db_schema.cc
  mrs/database/query_changes_db_service.cc
  mrs/database/query_changes_url_host.cc
  mrs/database/query_entries_audit_log.cc
  mrs/database/query_entries_auth_app.cc
  mrs/database/query_entries_auth_role.cc
  mrs/database/query_entries_content_file.cc
  mrs/database/query_entries_content_set.cc
  mrs/database/query_entries_db_object.cc
  mrs/database/query_entries_db_schema.cc
  mrs/database/query_entries_db_service.cc
  mrs/database/query_entries_object.cc
  mrs/database/query_entries_url_host.cc
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
  mrs/database/query_rest_task.cc
  mrs/database/query_rest_task_status.cc
  mrs/database/query_rest_table_updater.cc
  mrs/database/query_router_info.cc
  mrs/database/query_state.cc
  mrs/database/query_statistics.cc
  mrs/database/query_version.cc
  mrs/database/query_warnings.cc
  mrs/database/metadata_logger.cc
  mrs/database/mysql_task_monitor.cc
  mrs/database/helper/query.cc
  mrs/database/helper/bind.cc
  mrs/database/helper/query_table_columns.cc
  mrs/database/helper/query_audit_log_maxid.cc
  mrs/database/helper/query_retry_on_ro.cc
  mrs/database/helper/query_gtid_executed.cc
  mrs/database/helper/object_checksum.cc
  mrs/database/helper/sp_function_query.cc
  mrs/database/entry/object.cc
  mrs/database/json_mapper/json_input.cc
  mrs/database/json_mapper/common.cc
  mrs/database/json_mapper/change.cc
  mrs/database/json_mapper/delete.cc
  mrs/database/json_mapper/insert.cc
  mrs/database/json_mapper/update.cc
  mrs/database/json_mapper/select.cc
  mrs/database/json_mapper/check.cc

  COMPILE_DEFINITIONS
  PRIVATE MYSQL_ROUTER_LOG_DOMAIN="mrs_db"

  LINK_LIBRARIES
  mysql_rest_service_if
  harness-library
  router_lib

  INCLUDE_DIRECTORIES
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE
  ${mysql_rest_service_db_includes}
)

# Downgrade -Werror to warning for gcc
IF(MY_COMPILER_IS_GNU)
  MY_CHECK_CXX_COMPILER_WARNING("error=maybe-uninitialized" HAS_WARN_FLAG)
  IF(HAS_WARN_FLAG)
    ADD_COMPILE_FLAGS(
      mrs/database/query_entries_object.cc
      COMPILE_FLAGS ${HAS_WARN_FLAG}
      )
  ENDIF()
ENDIF()

ADD_STATIC_LIBRARY(mysql_rest_service_json
  mrs/json/response_json_template.cc
  mrs/json/json_template_factory.cc
  mrs/json/json_template_unnest.cc
  mrs/json/json_template_nest.cc
  mrs/json/json_template_nest_without_outparams.cc

  LINK_LIBRARIES
  mysql_rest_service_if
  harness-library
  router_lib
  http_common

  INCLUDE_DIRECTORIES
  PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}

  COMPILE_DEFINITIONS
  PRIVATE MYSQL_ROUTER_LOG_DOMAIN="mrs_json"
  )

SET(mysql_rest_service_impl_libs
  http_client
  rest_api
  mysql_rest_service_db
)

SET(mysql_rest_service_includes
  ${PROJECT_SOURCE_DIR}/src/harness/include/
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/include/
  ${PROJECT_SOURCE_DIR}/src/routing/include/
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

IF(MRS_JIT_EXECUTOR_LIB)
  list(APPEND mysql_rest_service_impl_libs jit_executor_plugin)
  list(APPEND mysql_rest_service_includes ${PROJECT_SOURCE_DIR}/src/jit_executor/include/)
ENDIF()

SET(mysql_rest_service_impl_SRC
  collector/counted_mysql_session.cc
  collector/mysql_cache_manager.cc
  collector/mysql_fixed_pool_manager.cc
  mrs/authentication/helper/crypto.cc
  mrs/authentication/helper/http_result.cc
  mrs/authentication/helper/scram.cc
  mrs/authentication/helper/option_parser.cc
  mrs/authentication/authorize_manager.cc
  mrs/authentication/auth_handler_factory.cc
  mrs/authentication/mysql_handler.cc
  mrs/authentication/oauth2_facebook_handler.cc
  mrs/authentication/oauth2_google_handler.cc
  mrs/authentication/oauth2_oidc_handler.cc
  mrs/authentication/oauth2_handler.cc
  mrs/authentication/www_authentication_handler.cc
  mrs/authentication/sasl_handler.cc
  mrs/authentication/scram_handler.cc
  mrs/endpoint/content_file_endpoint.cc
  mrs/endpoint/content_set_endpoint.cc
  mrs/endpoint/db_object_endpoint.cc
  mrs/endpoint/db_schema_endpoint.cc
  mrs/endpoint/db_service_endpoint.cc
  mrs/endpoint/endpoint_factory.cc
  mrs/endpoint/handler_factory.cc
  mrs/endpoint/option_endpoint.cc
  mrs/endpoint/handler/authentication/handler_authorize_auth_apps.cc
  mrs/endpoint/handler/authentication/handler_authorize_base.cc
  mrs/endpoint/handler/authentication/handler_authorize_completed.cc
  mrs/endpoint/handler/authentication/handler_authorize_login.cc
  mrs/endpoint/handler/authentication/handler_authorize_logout.cc
  mrs/endpoint/handler/authentication/handler_authorize_status.cc
  mrs/endpoint/handler/authentication/handler_authorize_user.cc
  mrs/endpoint/handler/persistent/persistent_data_content_file.cc
  mrs/endpoint/handler/handler_content_file.cc
  mrs/endpoint/handler/handler_db_service_debug.cc
  mrs/endpoint/handler/handler_db_service_metadata.cc
  mrs/endpoint/handler/handler_db_schema_metadata_catalog.cc
  mrs/endpoint/handler/handler_db_schema_metadata.cc
  mrs/endpoint/handler/handler_db_object_table.cc
  mrs/endpoint/handler/handler_db_object_sp.cc
  mrs/endpoint/handler/handler_db_object_script.cc
  mrs/endpoint/handler/handler_db_object_function.cc
  mrs/endpoint/handler/handler_db_object_metadata.cc
  mrs/endpoint/handler/handler_db_object_openapi.cc
  mrs/endpoint/handler/handler_db_schema_openapi.cc
  mrs/endpoint/handler/handler_db_service_openapi.cc
  mrs/endpoint/handler/handler_debug.cc
  mrs/endpoint/handler/handler_string.cc
  mrs/endpoint/handler/handler_redirection.cc
  mrs/endpoint/url_host_endpoint.cc
  mrs/http/cookie.cc
  mrs/http/header_accept.cc
  mrs/http/utilities.cc
  mrs/http/session_manager.cc
  mrs/users/user_manager.cc
  mrs/endpoint_manager.cc
  mrs/router_observation_entities.cc
  mrs/rest/handler.cc
  mrs/rest/rest_request_handler.cc
  mrs/rest/response_cache.cc
  mrs/rest/openapi_object_creator.cc
  mrs/monitored/gtid_functions.cc
  mrs/monitored/query_retry_on_ro.cc
)

IF(MRS_JIT_EXECUTOR_LIB)
  list(APPEND mysql_rest_service_impl_SRC mrs/file_system/db_service_file_system.cc)
ENDIF()


ADD_STATIC_LIBRARY(mysql_rest_service_impl
  ${mysql_rest_service_impl_SRC}

  INCLUDE_DIRECTORIES
  PRIVATE
  ${mysql_rest_service_includes}

  LINK_LIBRARIES
  ${mysql_rest_service_impl_libs}

  COMPILE_DEFINITIONS
  PRIVATE MYSQL_ROUTER_LOG_DOMAIN="mrs_endpoint"
  )

SET(mysql_rest_service_reqs
  io_component
  router_lib
  routing
  http_server
  mysql_rest_service_impl
  mysql_rest_service_db
  mysql_rest_service_json
  mysql_rest_service_helper
)

IF(MRS_JIT_EXECUTOR_LIB)
  list(APPEND mysql_rest_service_reqs jit_executor_plugin)
ENDIF()

ADD_HARNESS_PLUGIN(mysql_rest_service
  SOURCES
  mysql_rest_service_plugin_config.cc
  mysql_rest_service_plugin.cc
  REQUIRES "${mysql_rest_service_reqs}"
  DESTINATION "${ROUTER_INSTALL_PLUGINDIR}"
)

TARGET_INCLUDE_DIRECTORIES(mysql_rest_service
  PRIVATE
    ${mysql_rest_service_includes}
)

GENERATE_EXPORT_HEADER(mysql_rest_service
  EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/mysql_rest_service_export.h)

