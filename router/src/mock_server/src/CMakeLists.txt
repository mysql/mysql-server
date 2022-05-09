# Copyright (c) 2017, 2022, Oracle and/or its affiliates.
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


SET(common_libraries)
SET(math_libraries)
IF(WIN32)
  LIST(APPEND common_libraries ws2_32)
ELSEIF(SOLARIS)
  LIST(APPEND common_libraries -lnsl -lsocket -lpthread)
  LIST(APPEND math_libraries -lm)
ELSE()
  LIST(APPEND common_libraries -lpthread)
  LIST(APPEND math_libraries -lm)
ENDIF()

SET(DUKTAPE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/extra/duktape/duktape-2.7.0/)

SET(DUKTAPE_SRCS
  ${DUKTAPE_SOURCE_DIR}/src/duktape.c
  ${DUKTAPE_SOURCE_DIR}/extras/logging/duk_logging.c
  ${DUKTAPE_SOURCE_DIR}/extras/module-node/duk_module_node.c)

SET(MOCK_SERVER_HTTP_CLIENT_SRCS
    mock_server_rest_client.cc)

# disable compiler warnings in duktape
MY_CHECK_CXX_COMPILER_WARNING("-Wunused-function" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  ADD_COMPILE_FLAGS(${DUKTAPE_SRCS} COMPILE_FLAGS "${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Werror" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  ADD_COMPILE_FLAGS(${DUKTAPE_SRCS} COMPILE_FLAGS "${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wmissing-noreturn" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  ADD_COMPILE_FLAGS(${DUKTAPE_SRCS} COMPILE_FLAGS "${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wconditional-uninitialized" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  ADD_COMPILE_FLAGS(${DUKTAPE_SRCS} COMPILE_FLAGS "${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wsuggest-attribute=format" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  ADD_COMPILE_FLAGS(${DUKTAPE_SRCS} COMPILE_FLAGS "${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wimplicit-fallthrough" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  ADD_COMPILE_FLAGS(${DUKTAPE_SRCS} COMPILE_FLAGS "${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wformat-truncation" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  ADD_COMPILE_FLAGS(${DUKTAPE_SRCS} COMPILE_FLAGS "${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wformat-overflow" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  ADD_COMPILE_FLAGS(${DUKTAPE_SRCS} COMPILE_FLAGS "${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wparentheses-equality" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  ADD_COMPILE_FLAGS(${DUKTAPE_SRCS} COMPILE_FLAGS "${HAS_WARN_FLAG}")
ENDIF()
MY_CHECK_CXX_COMPILER_WARNING("-Wmissing-profile" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  ADD_COMPILE_FLAGS(${DUKTAPE_SRCS} COMPILE_FLAGS "${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wmaybe-uninitialized" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  ADD_COMPILE_FLAGS(${DUKTAPE_SRCS} COMPILE_FLAGS "${HAS_WARN_FLAG}")
ENDIF()

ADD_LIBRARY(duktape SHARED ${DUKTAPE_SRCS})
TARGET_INCLUDE_DIRECTORIES(duktape SYSTEM PUBLIC
  ${DUKTAPE_SOURCE_DIR}/src/
  ${DUKTAPE_SOURCE_DIR}/extras/logging/
  ${DUKTAPE_SOURCE_DIR}/extras/module-node/
  )
TARGET_LINK_LIBRARIES(duktape ${math_libraries})

SET_TARGET_PROPERTIES(duktape PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory)

IF(WIN32)
  # LNK4049: locally defined symbol 'symbol' imported
  # LNK4217: locally defined symbol 'symbol' imported in function 'function'
  STRING_APPEND(CMAKE_SHARED_LINKER_FLAGS " /ignore:4049 /ignore:4217")

  ADD_CUSTOM_COMMAND(TARGET duktape POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_BINARY_DIR}/library_output_directory/${CMAKE_CFG_INTDIR}/duktape.dll"
    "${CMAKE_BINARY_DIR}/runtime_output_directory/${CMAKE_CFG_INTDIR}/duktape.dll"
   )
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
    router_lib
    harness_tls
  )

TARGET_INCLUDE_DIRECTORIES(mock_server SYSTEM PUBLIC
  ${DUKTAPE_SOURCE_DIR}/src/
  ${DUKTAPE_SOURCE_DIR}/extras/logging/
  ${DUKTAPE_SOURCE_DIR}/extras/module-node/
  ${DUKTAPE_SOURCE_DIR}/extras/module-duktape/
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

ADD_LIBRARY(mock_server_rest_client ${MOCK_SERVER_HTTP_CLIENT_SRCS})
TARGET_INCLUDE_DIRECTORIES(mock_server_rest_client PUBLIC
  ${PROJECT_SOURCE_DIR}/src/mock_server/include/
  ${PROJECT_SOURCE_DIR}/src/http/include/
  )
TARGET_LINK_LIBRARIES(mock_server_rest_client http_client)
ADD_DEPENDENCIES(mysqlrouter_all mock_server_rest_client)

TARGET_COMPILE_DEFINITIONS(mysql_server_mock
  PRIVATE -Dmysql_protocol_DEFINE_STATIC=1
  )
SET_PATH_TO_CUSTOM_SSL_FOR_APPLE(mysql_server_mock)
