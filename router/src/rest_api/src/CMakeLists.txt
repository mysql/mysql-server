# Copyright (c) 2019, 2024, Oracle and/or its affiliates.
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

ADD_LIBRARY(rest_api_lib STATIC
  rest_api_utils.cc
)
TARGET_INCLUDE_DIRECTORIES(rest_api_lib
  PUBLIC
    ${PROJECT_SOURCE_DIR}/src/harness/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
TARGET_LINK_LIBRARIES(rest_api_lib PUBLIC router_lib;http_server;http_auth_realm_lib)

ADD_HARNESS_PLUGIN(rest_api
  SOURCES
  rest_api_component.cc
  rest_api_plugin.cc
  rest_api_handler.cc
  rest_api.cc
  REQUIRES router_lib;http_server;rest_api_lib;http_auth_backend
  DESTINATION "${ROUTER_INSTALL_PLUGINDIR}")

TARGET_INCLUDE_DIRECTORIES(rest_api
  PUBLIC
    ${PROJECT_SOURCE_DIR}/src/harness/include/
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_BINARY_DIR}/../include
    )

GENERATE_EXPORT_HEADER(rest_api
  EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/rest_api_export.h)

TARGET_INCLUDE_DIRECTORIES(rest_api
  PUBLIC
    ${PROJECT_SOURCE_DIR}/src/harness/include/
    ${PROJECT_SOURCE_DIR}/src/http/include/
    ${CMAKE_CURRENT_BINARY_DIR}
    )
