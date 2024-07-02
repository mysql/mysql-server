# Copyright (c) 2018, 2024, Oracle and/or its affiliates.
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

IF(WIN32_CLANG)
  MY_CHECK_CXX_COMPILER_WARNING("-Wdeprecated-copy-with-dtor" HAS_WARN_FLAG)
  IF(HAS_WARN_FLAG)
    STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
  ENDIF()
ENDIF()

ROUTER_ADD_SHARED_LIBRARY(http_common
  base64.cc
  http_auth_method_basic.cc
  http/cno/callback_init.cc
  http/cno/cno_instantiation.cc
  http/cno/error_code.cc
  http/base/base_instantiation.cc
  http/base/headers.cc
  http/base/method.cc
  http/base/status_code.cc
  http/base/uri.cc
  http/base/http_time.cc

  OUTPUT_NAME "mysqlrouter_http"

  LINK_LIBRARIES
  PUBLIC
  harness_tls
  router_utils
  harness-library
  ext::cno
  router_tls
  PRIVATE
  OpenSSL::SSL OpenSSL::Crypto
  )

## split library code from the plugin to use them _passwd and tests
ROUTER_ADD_SHARED_LIBRARY(http_auth_backend_lib
  http_auth_backend.cc
  kdf_sha_crypt.cc
  kdf_pbkdf2.cc
  mcf_error.cc
  http_auth_error.cc
  http_auth_backend_component.cc

  OUTPUT_NAME "mysqlrouter_http_auth_backend"

  LINK_LIBRARIES
  http_common extra::rapidjson)

## the plugin
ADD_HARNESS_PLUGIN(http_auth_backend
  SOURCES
  http_auth_backend_plugin.cc
  http_auth_backend_metadata_cache.cc
  REQUIRES
  http_common
  http_auth_backend_lib
  router_utils
  metadata_cache
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
ROUTER_ADD_SHARED_LIBRARY(http_auth_realm_lib
  http_auth_realm.cc
  http_auth_realm_component.cc

  OUTPUT_NAME "mysqlrouter_http_auth_realm"

  LINK_LIBRARIES
  PUBLIC
  http_auth_backend_lib)

ADD_HARNESS_PLUGIN(http_auth_realm
  SOURCES
  http_auth_realm_plugin.cc
  REQUIRES
  http_auth_realm_lib
  router_utils
  http_auth_backend_lib
  DESTINATION "${ROUTER_INSTALL_PLUGINDIR}"
  )
GENERATE_EXPORT_HEADER(http_auth_realm
  EXPORT_FILE_NAME
  ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/http_auth_realm_export.h
  )
TARGET_INCLUDE_DIRECTORIES(http_auth_realm PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/)

# http_server_component
#
# used by the plugins:
#
# - rest_api
# - http_server
#
ROUTER_ADD_SHARED_LIBRARY(http_server_lib
  http/server/bind.cc
  http/server/request.cc
  http/server/server.cc
  http/http_server_context.cc
  http_auth.cc
  http_server_component.cc
  http_request_router.cc

  OUTPUT_NAME "mysqlrouter_http_server"

  LINK_LIBRARIES
  PUBLIC
  io_component
  http_common
  http_auth_realm_lib
  ext::icu
  )

TARGET_INCLUDE_DIRECTORIES(http_server_lib
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}  # http/...
  )


ADD_HARNESS_PLUGIN(http_server
  SOURCES
  http_server_plugin.cc
  static_files.cc
  REQUIRES
    router_utils
    http_common
    http_auth_realm_lib
    http_server_lib
    ${LIBRT}
    harness_stdx
  DESTINATION "${ROUTER_INSTALL_PLUGINDIR}"
  )

TARGET_INCLUDE_DIRECTORIES(http_server
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/)


ROUTER_ADD_SHARED_LIBRARY(http_client
  rest_client.cc
  http/client/client.cc
  http/client/error_code.cc
  http/client/request.cc

  OUTPUT_NAME "http_client"

  SKIP_INSTALL

  LINK_LIBRARIES
  ${LIBRT}
  http_common
  )

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
  router_utils
  http_auth_backend_lib
  )
ADD_INSTALL_RPATH(mysqlrouter_passwd "${ROUTER_INSTALL_RPATH}")

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/harness/include/)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/router/include/)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/metadata_cache/include/)
INCLUDE_DIRECTORIES(${PROJECT_BINARY_DIR}/src/http/include/)
