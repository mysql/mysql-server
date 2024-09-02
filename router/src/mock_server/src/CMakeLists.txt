# Copyright (c) 2017, 2024, Oracle and/or its affiliates.
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

################################################################################
# build JSON schema embedder, then use it to generate mysql_server_mock_schema.cc
# (server mock dependency) from mysql_server_mock_schema.js
################################################################################

ADD_CUSTOM_COMMAND(OUTPUT mysql_server_mock_schema.cc mysql_server_mock_schema.h
                   COMMAND json_schema_embedder
                     ${CMAKE_CURRENT_SOURCE_DIR}/mysql_server_mock_schema.js
                     mysql_server_mock_schema.cc
                     mysql_server_mock_schema.h
                     "SqlQueryJsonSchema"
                   DEPENDS mysql_server_mock_schema.js)

ADD_CUSTOM_TARGET(mysql_server_mock_schema
  DEPENDS mysql_server_mock_schema.cc mysql_server_mock_schema.h
  )
ADD_DEPENDENCIES(clang_tidy_prerequisites mysql_server_mock_schema)

SET(common_libraries)
IF(WIN32)
  LIST(APPEND common_libraries ws2_32)
ELSEIF(SOLARIS)
  LIST(APPEND common_libraries -lnsl -lsocket -lpthread)
ELSE()
  LIST(APPEND common_libraries -lpthread)
ENDIF()

SET(MOCK_SERVER_HTTP_CLIENT_SRCS
    mock_server_rest_client.cc)

ADD_LIBRARY(mock_server_rest_client ${MOCK_SERVER_HTTP_CLIENT_SRCS})
TARGET_INCLUDE_DIRECTORIES(mock_server_rest_client PUBLIC
  ${PROJECT_SOURCE_DIR}/src/mock_server/include/
  ${PROJECT_SOURCE_DIR}/src/http/include/
  )
TARGET_LINK_LIBRARIES(mock_server_rest_client http_client extra::rapidjson)
ADD_DEPENDENCIES(mysqlrouter_all mock_server_rest_client)

IF(NOT TARGET duktape)
  RETURN()
ENDIF()

add_harness_plugin(mock_server
  NO_INSTALL
  SOURCES duktape_statement_reader.cc
  x_protocol_decoder.cc
  x_protocol_encoder.cc
  mysql_server_mock.cc
  classic_mock_session.cc
  x_mock_session.cc
  mysql_server_mock_schema.cc
  duk_module_shim.cc
  duk_node_fs.c
  mock_server_plugin.cc
  mock_server_component.cc
  authentication.cc
  protocol_base.cc
  REQUIRES
    mysql_protocol
    duktape
    router_mysqlxmessages
    harness_stdx
    harness-library
    router_utils
    io_component
    harness_tls
  )
ADD_DEPENDENCIES(mock_server
  GenError
  mysql_server_mock_schema
  )

TARGET_INCLUDE_DIRECTORIES(mock_server PUBLIC
  ${PROJECT_SOURCE_DIR}/src/mock_server/include/
  ${PROJECT_SOURCE_DIR}/src/router/include
  ${PROJECT_SOURCE_DIR}/src/mysql_protocol/include/
  ${CMAKE_CURRENT_BINARY_DIR}
  )

add_harness_plugin(rest_mock_server
  NO_INSTALL
  SOURCES rest_mock_server.cc
  REQUIRES mock_server;http_server)
TARGET_INCLUDE_DIRECTORIES(rest_mock_server PUBLIC
  ${PROJECT_SOURCE_DIR}/src/http/include
  )

MYSQL_ADD_EXECUTABLE(mysql_server_mock
  main.cc
  COMPONENT Router
  LINK_LIBRARIES
  ${common_libraries}
  harness-library
  harness_stdx
  harness_net_ts
  SKIP_INSTALL
  )

TARGET_COMPILE_DEFINITIONS(mysql_server_mock
  PRIVATE -Dmysql_protocol_DEFINE_STATIC=1
  )
SET_PATH_TO_CUSTOM_SSL_FOR_APPLE(mysql_server_mock)
