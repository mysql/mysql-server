# Copyright (c) 2021, 2022, Oracle and/or its affiliates.
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

ADD_HARNESS_PLUGIN(rest_mrs
  SOURCES
  collector/mysql_cache_manager.cc
  helper/media_detector.cc
  helper/json/to_string.cc
  helper/mysql_column_types.cc
  mrs/authentication/auth_manager.cc
  mrs/authentication/auth_handler_factory.cc
  mrs/authentication/basic_handler.cc
  mrs/authentication/oauth2_facebook_handler.cc
  mrs/authentication/oauth2_twitter_handler.cc
  mrs/authentication/oauth2_google_handler.cc
  mrs/authentication/oauth2_handler.cc
  mrs/authentication/www_authentication_handler.cc
  mrs/database/filter_object_generator.cc
  mrs/database/schema_monitor.cc
  mrs/database/query_changes_auth_app.cc
  mrs/database/query_entries_auth_privileges.cc
  mrs/database/query_changes_content_file.cc
  mrs/database/query_changes_db_object.cc
  mrs/database/query_entries_audit_log.cc
  mrs/database/query_entries_auth_app.cc
  mrs/database/query_entries_content_file.cc
  mrs/database/query_entries_db_object.cc
  mrs/database/query_entry_auth_user.cc
  mrs/database/query_entry_content_file.cc
  mrs/database/query_entry_parameter.cc
  mrs/database/query_factory.cc
  mrs/database/query_entry_group_row_security.cc
  mrs/database/query_rest_table.cc
  mrs/database/query_rest_table_single_row.cc
  mrs/database/query_rest_sp.cc
  mrs/database/query_rest_sp_media.cc
  mrs/database/helper/query.cc
  mrs/database/helper/query_state.cc
  mrs/database/helper/query_table_columns.cc
  mrs/database/helper/query_audit_log_maxid.cc
  mrs/json/response_json_template.cc
  mrs/http/cookie.cc
  mrs/http/utilities.cc
  mrs/http/session_manager.cc
  mrs/users/user_manager.cc
  mrs/route_factory.cc
  mrs/route_manager.cc
  mrs/route_schema_rest.cc
  mrs/route_object.cc
  mrs/route_static_file.cc
  mrs/rest/handler.cc
  mrs/rest/handler_authorize.cc
  mrs/rest/handler_authorize_ok.cc
  mrs/rest/handler_factory.cc
  mrs/rest/handler_file.cc
  mrs/rest/handler_object.cc
  mrs/rest/handler_object_metadata.cc
  mrs/rest/handler_is_authorized.cc
  mrs/rest/handler_unauthorize.cc
  mrs/rest/handler_schema_metadata.cc
  mrs/rest/handler_sp.cc
  rest_mrs_plugin.cc
  REQUIRES io_component;harness-library;router_lib;routing;http_server;rest_api;http_client;${LIBEVENT_LIBRARIES}
  DESTINATION "${ROUTER_INSTALL_PLUGINDIR}"
  )

TARGET_INCLUDE_DIRECTORIES(rest_mrs
  PRIVATE
    ${PROJECT_SOURCE_DIR}/src/harness/include/
    ${PROJECT_SOURCE_DIR}/src/metadata_cache/include/
    ${PROJECT_SOURCE_DIR}/src/routing/include/
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/../include/
    ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
GENERATE_EXPORT_HEADER(rest_mrs
  EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/rest_mrs_export.h)

MY_INCLUDE_SYSTEM_DIRECTORIES(LIBEVENT)