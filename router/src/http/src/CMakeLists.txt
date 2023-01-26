# Copyright (c) 2018, 2023, Oracle and/or its affiliates.
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

IF(LIBEVENT_VERSION VERSION_LESS "2.1")
  MESSAGE(FATAL_ERROR "At least version 2.1 is needed for libevent.")
ENDIF()

ADD_LIBRARY(http_common
  SHARED
  base64.cc
  http_common.cc
  http_time.cc
  )
TARGET_LINK_LIBRARIES(http_common
  PUBLIC
  harness_tls
  ext::libevent
  PRIVATE
  ${SSL_LIBRARIES}
  )
TARGET_INCLUDE_DIRECTORIES(http_common
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include/)


SET_TARGET_PROPERTIES(http_common PROPERTIES
  OUTPUT_NAME "mysqlrouter_http"
  SOVERSION 1
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/runtime_output_directory
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory)

## the rpath for the bundled openssl
ADD_INSTALL_RPATH_FOR_OPENSSL(http_common)
SET_PATH_TO_CUSTOM_SSL_FOR_APPLE(http_common)

INSTALL(TARGETS http_common
  RUNTIME DESTINATION ${ROUTER_INSTALL_BINDIR} COMPONENT Router
  ARCHIVE DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  LIBRARY DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  NAMELINK_SKIP
  )

## split library code from the plugin to use them _passwd and tests
ADD_LIBRARY(http_auth_backend_lib SHARED
  http_auth_backend.cc
  kdf_sha_crypt.cc
  kdf_pbkdf2.cc
  mcf_error.cc
  http_auth_error.cc
  http_auth_backend_component.cc
  )
GENERATE_EXPORT_HEADER(http_auth_backend_lib
  EXPORT_FILE_NAME
  ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/http_auth_backend_lib_export.h
  )
TARGET_LINK_LIBRARIES(http_auth_backend_lib http_common extra::rapidjson)
SET_TARGET_PROPERTIES(http_auth_backend_lib PROPERTIES
  OUTPUT_NAME "mysqlrouter_http_auth_backend"
  SOVERSION 1
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/runtime_output_directory
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory)
INSTALL(TARGETS http_auth_backend_lib
  RUNTIME DESTINATION ${ROUTER_INSTALL_BINDIR} COMPONENT Router
  ARCHIVE DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  LIBRARY DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  NAMELINK_SKIP
  )
TARGET_INCLUDE_DIRECTORIES(http_auth_backend_lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/
  ${CMAKE_CURRENT_BINARY_DIR}/../include/)
## the rpath for the bundled openssl
ADD_INSTALL_RPATH_FOR_OPENSSL(http_auth_backend_lib)
SET_PATH_TO_CUSTOM_SSL_FOR_APPLE(http_auth_backend_lib)


## the plugin
ADD_HARNESS_PLUGIN(http_auth_backend
  SOURCES
  http_auth_backend_plugin.cc
  http_auth_backend_metadata_cache.cc
  REQUIRES
  http_common;http_auth_backend_lib;router_lib;metadata_cache
  DESTINATION "${ROUTER_INSTALL_PLUGINDIR}"
  )
GENERATE_EXPORT_HEADER(http_auth_backend
  EXPORT_FILE_NAME
  ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/http_auth_backend_export.h
  )
TARGET_INCLUDE_DIRECTORIES(http_auth_backend PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/
  ${CMAKE_CURRENT_BINARY_DIR}/../include/)

## split library code from the plugin to use them with tests
ADD_LIBRARY(http_auth_realm_lib SHARED
  http_auth_realm.cc
  http_auth_realm_component.cc
  )
GENERATE_EXPORT_HEADER(http_auth_realm_lib
  EXPORT_FILE_NAME
  ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/http_auth_realm_lib_export.h
  )
TARGET_LINK_LIBRARIES(http_auth_realm_lib PUBLIC
  http_auth_backend_lib)
TARGET_INCLUDE_DIRECTORIES(http_auth_realm_lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/
  ${CMAKE_CURRENT_BINARY_DIR}/../include/)
SET_TARGET_PROPERTIES(http_auth_realm_lib PROPERTIES
  OUTPUT_NAME "mysqlrouter_http_auth_realm"
  SOVERSION 1
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/runtime_output_directory
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory)
INSTALL(TARGETS http_auth_realm_lib
  RUNTIME DESTINATION ${ROUTER_INSTALL_BINDIR} COMPONENT Router
  ARCHIVE DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  LIBRARY DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  NAMELINK_SKIP
  )
## the rpath for the bundled openssl
ADD_INSTALL_RPATH_FOR_OPENSSL(http_auth_realm_lib)
SET_PATH_TO_CUSTOM_SSL_FOR_APPLE(http_auth_realm_lib)


ADD_HARNESS_PLUGIN(http_auth_realm
  SOURCES
  http_auth_realm_plugin.cc
  REQUIRES
  http_auth_realm_lib;router_lib;http_auth_backend
  DESTINATION "${ROUTER_INSTALL_PLUGINDIR}"
  )
GENERATE_EXPORT_HEADER(http_auth_realm
  EXPORT_FILE_NAME
  ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/http_auth_realm_export.h
  )
TARGET_INCLUDE_DIRECTORIES(http_auth_realm PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/)


ADD_HARNESS_PLUGIN(http_server
  SOURCES
  http_auth.cc
  http_auth_method_basic.cc
  http_server_component.cc
  http_server_plugin.cc
  static_files.cc
  REQUIRES router_lib;http_common;http_auth_realm;
    ${LIBRT};
    harness_stdx
  DESTINATION "${ROUTER_INSTALL_PLUGINDIR}"
  )
TARGET_INCLUDE_DIRECTORIES(http_server PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/)


ADD_LIBRARY(http_client
  SHARED
  http_client.cc
  rest_client.cc
  )
TARGET_LINK_LIBRARIES(http_client
  ${LIBRT}
  http_common)

SET_TARGET_PROPERTIES(http_client PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/runtime_output_directory
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory)
TARGET_INCLUDE_DIRECTORIES(http_client PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/)

## the rpath for the bundled openssl
SET_PATH_TO_CUSTOM_SSL_FOR_APPLE(http_client)

# INSTALL(TARGETS http_client
#   RUNTIME DESTINATION ${ROUTER_INSTALL_BINDIR}
#   COMPONENT Router
#   LIBRARY DESTINATION ${ROUTER_INSTALL_LIBDIR}
#   COMPONENT Router
#   ARCHIVE DESTINATION ${ROUTER_INSTALL_LIBDIR}
#   COMPONENT Router)

MYSQL_ADD_EXECUTABLE(rest_cli
  rest_cli.cc
  COMPONENT Router
  LINK_LIBRARIES
  harness-library
  http_client
  SKIP_INSTALL
  )
MYSQL_ADD_EXECUTABLE(mysqlrouter_passwd
  passwd_cli.cc
  passwd.cc
  COMPONENT Router
  LINK_LIBRARIES
  harness-library
  router_lib
  http_auth_backend_lib
  metadata_cache
  )
ADD_INSTALL_RPATH(mysqlrouter_passwd "${ROUTER_INSTALL_RPATH}")

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/harness/include/)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/router/include/)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/metadata_cache/include/)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/http/include/)
INCLUDE_DIRECTORIES(${PROJECT_BINARY_DIR}/src/http/include/)
