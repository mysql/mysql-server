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

INCLUDE_DIRECTORIES(SYSTEM ${GMOCK_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(../src ${PROJECT_SOURCE_DIR}/tests/ ./)

IF(MY_COMPILER_IS_GNU_OR_CLANG)
  ADD_COMPILE_OPTIONS("-Wno-deprecated-declarations")
ENDIF()

ADD_TEST_FILE(test_rest_mrs_plugin.cc
  MODULE mrs
  EXTRA_SOURCES
    ./helper/set_http_component.cc
    ./test_authorize_manager.cc
    ./test_helper_json.cc
    ./test_helper_response_json_template.cc
    ./test_collector_mysql_cache_manager.cc
    ./test_database_rest_table.cc
    ./test_database_rest_sp.cc
    ./test_database_entries.cc
    ./test_database_content_files_options.cc
    ./test_mrs_authentication_rate_control.cc
    ./test_mrs_database_rest_table.cc
    ./test_mrs_database_rest_table_get.cc
    ./test_mrs_database_rest_table_put.cc
    ./test_mrs_database_rest_table_post.cc
    ./test_mrs_database_rest_table_delete.cc
    ./test_mrs_database_filter_object_generator.cc
    ./test_mrs_parse_json.cc
    ./test_helper_media_detector.cc
    ./test_helper_plugin_monitor.cc
    ./test_helper_string.cc
    ./test_helper_jwt.cc
    ./test_helper_gtid.cc
    ./test_mrs_gtid_manager.cc
    ./test_http_utilities.cc
    ./test_rest_handler_authorize.cc
    ./test_rest_handler_object.cc
    ./test_rest_handler_file.cc
    ./test_rest_handler.cc
    ./test_rest_mrs_user_manager.cc
    ./test_rest_user_entry.cc
    ./test_mrs_http_header_accept.cc
    ./test_mrs_json_template.cc
    ./test_mrs_query_entries_db_object.cc
    ./test_mrs_user_cache.cc
    ./test_mrs_route_object.cc
    ./test_mrs_route_manager.cc
    ./test_mrs_route_schema_rest.cc
    ./test_mrs_route_static_file.cc
    ./test_handler_object.cc
    ./test_object_checksum.cc
  LIB_DEPENDS mysql_rest_service test-helpers
  )

