# Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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

# build JSON schema embedder
ADD_EXECUTABLE(json_schema_embedder json_schema_embedder.cc)

# generate mysql_server_mock_schema.cc, which embeds mysql_server_mock_schema.js
# as a c-string. Server mock build will trigger this as its dependency [_HERE_]
ADD_CUSTOM_COMMAND(OUTPUT mysql_server_mock_schema.cc
                   COMMAND json_schema_embedder
                     ${CMAKE_CURRENT_SOURCE_DIR}/mysql_server_mock_schema.js
                     ${CMAKE_CURRENT_BINARY_DIR}/mysql_server_mock_schema.cc)


SET(common_libraries)
SET(math_libraries)
IF(WIN32)
  LIST(APPEND common_libraries Ws2_32.lib)
ELSEIF(${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
  LIST(APPEND common_libraries -lnsl -lsocket -lpthread)
  LIST(APPEND math_libraries -lm)
ELSE()
  LIST(APPEND common_libraries -lpthread)
  LIST(APPEND math_libraries -lm)
ENDIF()

## Ubuntu 18.02 has duktape-2.2.0
SET(DUKTAPE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/extra/duktape/duktape-2.1.0/)

SET(DUKTAPE_SRCS
  ${DUKTAPE_SOURCE_DIR}/src/duktape.c
  ${DUKTAPE_SOURCE_DIR}/extras/logging/duk_logging.c
  ${DUKTAPE_SOURCE_DIR}/extras/module-node/duk_module_node.c)

CHECK_CXX_COMPILER_FLAG("-Werror" HAVE_ERROR)
IF(HAVE_ERROR)
  ADD_COMPILE_FLAGS(${DUKTAPE_SRCS} COMPILE_FLAGS "-Wno-error")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wmissing-noreturn" HAVE_MISSING_NORETURN)
IF(HAVE_MISSING_NORETURN)
  ADD_COMPILE_FLAGS(${DUKTAPE_SRCS} COMPILE_FLAGS "-Wno-missing-noreturn")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wconditional-uninitialized" HAVE_COND_UNINITIALIZED)
IF(HAVE_COND_UNINITIALIZED)
  ADD_COMPILE_FLAGS(${DUKTAPE_SRCS} COMPILE_FLAGS "-Wno-conditional-uninitialized")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wsuggest-attribute=format" HAVE_SUGGEST_ATTRIBUTE)
IF(HAVE_SUGGEST_ATTRIBUTE)
  ADD_COMPILE_FLAGS(${DUKTAPE_SRCS} COMPILE_FLAGS "-Wno-suggest-attribute=format")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wimplicit-fallthrough" HAVE_IMPLICIT_FALLTHROUGH)
IF(HAVE_IMPLICIT_FALLTHROUGH)
  ADD_COMPILE_FLAGS(${DUKTAPE_SRCS} COMPILE_FLAGS "-Wno-implicit-fallthrough")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wformat-truncation" HAVE_FORMAT_TRUNCATION)
IF(HAVE_FORMAT_TRUNCATION)
  ADD_COMPILE_FLAGS(${DUKTAPE_SRCS} COMPILE_FLAGS "-Wno-format-truncation")
ENDIF()

CHECK_CXX_COMPILER_FLAG("-Wformat-overflow" HAVE_FORMAT_OVERFLOW)
IF(HAVE_FORMAT_OVERFLOW)
  ADD_COMPILE_FLAGS(${DUKTAPE_SRCS} COMPILE_FLAGS "-Wno-format-overflow")
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
  ADD_CUSTOM_COMMAND(TARGET duktape POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_BINARY_DIR}/library_output_directory/${CMAKE_CFG_INTDIR}/duktape.dll"
    "${CMAKE_BINARY_DIR}/runtime_output_directory/${CMAKE_CFG_INTDIR}/duktape.dll"
   )
ENDIF()

add_harness_plugin(mock_server
  NO_INSTALL
  SOURCES json_statement_reader.cc
  duktape_statement_reader.cc
  mysql_protocol_decoder.cc
  mysql_protocol_encoder.cc
  mysql_protocol_utils.cc
  mysql_server_mock.cc
  mysql_server_mock_schema.cc
  duk_module_shim.c
  duk_node_fs.c
  mock_server_plugin.cc
  mock_server_component.cc
  REQUIRES mysql_protocol;duktape)

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
  )

add_harness_plugin(rest_mock_server
  NO_INSTALL
  SOURCES rest_mock_server.cc
  REQUIRES mock_server;http_server)
TARGET_INCLUDE_DIRECTORIES(rest_mock_server PUBLIC
  ${PROJECT_SOURCE_DIR}/src/http/include
  )

ADD_EXECUTABLE(mysql_server_mock
  main.cc
  )

TARGET_LINK_LIBRARIES(mysql_server_mock ${common_libraries} harness-library)
SET_TARGET_PROPERTIES(mysql_server_mock PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/runtime_output_directory)
TARGET_COMPILE_DEFINITIONS(mysql_server_mock
                           PRIVATE -Dmysql_protocol_DEFINE_STATIC=1)
SET_PATH_TO_SSL(mysql_server_mock ${CMAKE_BINARY_DIR}/runtime_output_directory)
