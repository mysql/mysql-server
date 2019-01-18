# Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.
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

IF(WITH_LIBEVENT STREQUAL "system")
  MYSQL_CHECK_LIBEVENT_VERSION()
  IF(SYSTEM_LIBEVENT_FOUND)
    IF(SYSTEM_LIBEVENT_VERSION VERSION_LESS "2.1")
      MESSAGE(FATAL_ERROR "At least version 2.1 is needed for libevent.")
    ENDIF()
  ENDIF()
ENDIF()

ADD_LIBRARY(http_common
  SHARED
  base64.cc
  http_common.cc
  http_time.cc
  tls_error.cc
  tls_context.cc
  )
TARGET_LINK_LIBRARIES(http_common
  ${LIBEVENT_LIBRARIES}
  ${SSL_LIBRARIES}
  )
TARGET_INCLUDE_DIRECTORIES(http_common
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include/)


SET_TARGET_PROPERTIES(http_common PROPERTIES
  OUTPUT_NAME "mysqlrouter_http"
  SOVERSION 1
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory)

## the rpath for the bundled openssl
SET_PATH_TO_SSL(http_common ${CMAKE_BINARY_DIR}/library_output_directory)

INSTALL(TARGETS http_common
  RUNTIME DESTINATION ${ROUTER_INSTALL_BINDIR}
  COMPONENT Router
  LIBRARY DESTINATION ${ROUTER_INSTALL_LIBDIR}
  COMPONENT Router
  ARCHIVE DESTINATION ${ROUTER_INSTALL_LIBDIR}
  COMPONENT Router)

## split library code from the plugin to use them _passwd and tests
ADD_LIBRARY(http_auth_backend_lib STATIC
  http_auth_backend.cc
  kdf_sha_crypt.cc
  kdf_pbkdf2.cc
  mcf_error.cc
  http_auth_error.cc
  )
TARGET_LINK_LIBRARIES(http_auth_backend_lib http_common)

## the plugin
ADD_HARNESS_PLUGIN(http_auth_backend
  SOURCES
  http_auth_backend_plugin.cc
  http_auth_backend_component.cc
  REQUIRES
  http_common;http_auth_backend_lib;router_lib
  DESTINATION "${ROUTER_INSTALL_PLUGINDIR}"
  )
GENERATE_EXPORT_HEADER(http_auth_backend
  EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/http_auth_backend_export.h)

TARGET_INCLUDE_DIRECTORIES(http_auth_backend
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include/
  PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/../include/)

ADD_HARNESS_PLUGIN(http_auth_realm
  SOURCES
  http_auth_realm_plugin.cc
  http_auth_realm_component.cc
  REQUIRES
  http_auth_backend;router_lib
  DESTINATION "${ROUTER_INSTALL_PLUGINDIR}"
  )
GENERATE_EXPORT_HEADER(http_auth_realm
  EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/http_auth_realm_export.h)
TARGET_INCLUDE_DIRECTORIES(http_auth_realm
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include/
  PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/../include/)


ADD_HARNESS_PLUGIN(http_server
  SOURCES
  http_auth.cc
  http_auth_method_basic.cc
  http_server_component.cc
  http_server_plugin.cc
  static_files.cc
  tls_server_context.cc
  REQUIRES router_lib;http_common;http_auth_realm;${LIBRT};${LIBEVENT_OPENSSL}
  DESTINATION "${ROUTER_INSTALL_PLUGINDIR}"
  )
TARGET_INCLUDE_DIRECTORIES(http_server
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include/)


ADD_LIBRARY(http_client
  SHARED
  http_client.cc
  rest_client.cc
  tls_client_context.cc
  )
TARGET_LINK_LIBRARIES(http_client
  ${LIBEVENT_LIBRARIES}
  ${LIBEVENT_OPENSSL}
  ${LIBRT}
  http_common)

SET_TARGET_PROPERTIES(http_client PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory)
TARGET_INCLUDE_DIRECTORIES(http_client
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include/)

## the rpath for the bundled openssl
SET_PATH_TO_SSL(http_client ${CMAKE_BINARY_DIR}/library_output_directory)

# INSTALL(TARGETS http_client
#   RUNTIME DESTINATION ${ROUTER_INSTALL_BINDIR}
#   COMPONENT Router
#   LIBRARY DESTINATION ${ROUTER_INSTALL_LIBDIR}
#   COMPONENT Router
#   ARCHIVE DESTINATION ${ROUTER_INSTALL_LIBDIR}
#   COMPONENT Router)


IF(WIN32)
  # unit-tests link against http_server.dll
  FUNCTION(COPY_TARGET_FOR_UNITTEST tgt)
    ADD_CUSTOM_COMMAND(TARGET ${tgt} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:${tgt}>
      "${CMAKE_BINARY_DIR}/runtime_output_directory/${CMAKE_CFG_INTDIR}/$<TARGET_FILE_NAME:${tgt}>"
    )
  ENDFUNCTION()

  COPY_TARGET_FOR_UNITTEST(http_common)
  COPY_TARGET_FOR_UNITTEST(http_client)
  COPY_TARGET_FOR_UNITTEST(http_server)
  COPY_TARGET_FOR_UNITTEST(http_auth_realm)
  COPY_TARGET_FOR_UNITTEST(http_auth_backend)
ENDIF()

ADD_EXECUTABLE(rest_cli
  rest_cli.cc)
ADD_DEPENDENCIES(mysqlrouter_all rest_cli)
TARGET_LINK_LIBRARIES(rest_cli
  harness-library
  http_client ${CMAKE_THREAD_LIBS_INIT})

MYSQL_ADD_EXECUTABLE(mysqlrouter_passwd
  passwd_cli.cc
  passwd.cc
  COMPONENT Router
  )
ADD_DEPENDENCIES(mysqlrouter_all mysqlrouter_passwd)
TARGET_LINK_LIBRARIES(mysqlrouter_passwd
  harness-library
  router_lib
  http_auth_backend_lib mysys)
SET_PROPERTY(TARGET mysqlrouter_passwd PROPERTY INSTALL_RPATH "${ROUTER_INSTALL_RPATH}")

MY_INCLUDE_SYSTEM_DIRECTORIES(LIBEVENT)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/harness/include/)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/router/include/)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/metadata_cache/include/)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/http/include/)
INCLUDE_DIRECTORIES(${PROJECT_BINARY_DIR}/src/http/include/)
