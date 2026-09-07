# Copyright (c) 2026, Oracle and/or its affiliates.
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
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

IF(NOT DEFINED WITH_COMPONENT_MRS AND
   NOT DEFINED WITHOUT_COMPONENT_MRS)
  SET(WITH_COMPONENT_MRS 1)
ENDIF()

IF(NOT WITH_COMPONENT_MRS)
  RETURN()
ENDIF()

DISABLE_MISSING_PROFILE_WARNING()

# The component consumes Router headers and implementation libraries.
# Match Router's MSVC code-analysis policy.
MSVC_CPPCHECK_DISABLE()

IF(NOT WITH_ROUTER)
  # Define reusable Router targets required by this component without
  # including the Router product in the default build or installation.
  SET(ROUTER_BUILD_MRS_DEPENDENCIES_ONLY ON)
  ADD_SUBDIRECTORY(
    ${CMAKE_SOURCE_DIR}/router
    ${CMAKE_BINARY_DIR}/router
    EXCLUDE_FROM_ALL)
  UNSET(ROUTER_BUILD_MRS_DEPENDENCIES_ONLY)
ENDIF()

SET(ROUTER_LIBRARIES
  mysql_rest_service_component_object
  http_server_object
  harness-library_object
  harness_stdx_object
  harness_net_ts
  io_object
  routing_object
  host_cache_object
  routing_plugin_object
  router_openssl_object
  connection_pool_plugin_object
  destination_status_plugin_object
  http_client_object
  http_common_object
  metadata_cache_object
  router_mysql_object
  http_server_lib_object
  router_cluster_object
  connection_pool_object
  router_utils_object
  http_auth_realm_object
  http_auth_realm_lib_object
  http_auth_backend_object
  http_auth_backend_lib_object
  io_component_object
  destination_status_object
  harness_tls_object
  routing_connections_object
  routing_guidelines_object
  mysql_rest_service_component_impl
  mysql_rest_service_db
  mysql_rest_service_json
  mysql_rest_service_helper
  sql_lexer
  routing_frontend_lib
  metadata_cache_x_lite
  routing_x_lite
  mysqlxclient_lite
  mysqlxmessages_lite
  clientlib mytime strings vio mysys
)

SET(MRS_THIRD_PARTY_LIBRARIES
  ext::libprotobuf-lite
  ext::lz4
  OpenSSL::SSL
  OpenSSL::Crypto
)

IF(WIN32)
  LIST(APPEND ROUTER_LIBRARIES auth_win_client)
  LIST(APPEND ROUTER_LIBRARIES shlwapi.dll)
ENDIF()

ADD_DEFINITIONS(-DLOG_COMPONENT_TAG="component_mysql_rest_service")
ADD_DEFINITIONS(-DROUTER_UTILS_STATIC_DEFINE)

IF(CMAKE_BUILD_TYPE_UPPER STREQUAL "DEBUG")
  ADD_DEFINITIONS(-DCOMPONENT_MRS_LOG_DEBUG)
ENDIF()

SET(MRS_COMPONENT_SOURCES
  mrs_component_log.cc
  mrs_component_server_config.cc
  mrs_component_status_vars.cc
  mrs_component_system_vars.cc
  mrs_component_mysql_command_service_session.cc
  mrs_component_command_executor.cc
  mrs_component_embedded_mysql_session.cc
  mrs_component_embedded_mysql_result.cc
  mrs_component_embedded_mysql_cache.cc
  mrs_component_udfs.cc
  mrs_component_table_access.cc
  mrs_component_runtime_services.cc
  mrs_component_backend.cc
  mrs_component.cc
)

# Add the component
MYSQL_ADD_COMPONENT(mysql_rest_service
  ${MRS_COMPONENT_SOURCES}
  LINK_LIBRARIES PRIVATE
    ${ROUTER_LIBRARIES}
    ${MRS_THIRD_PARTY_LIBRARIES}
  MODULE_ONLY
)

# Define the component target name expected from MYSQL_ADD_COMPONENT
SET(MRS_COMPONENT_TARGET "component_mysql_rest_service")

IF(WIN32)
  # The component embeds Router object libraries. Imported symbols may therefore
  # be resolved by definitions linked into the same module, triggering LNK4217.
  TARGET_LINK_OPTIONS(${MRS_COMPONENT_TARGET} PRIVATE "/IGNORE:4217")
ENDIF()

IF(UNIX AND NOT APPLE AND NOT SOLARIS)
  # Prevent symbols from linked static archives from being exported by the
  # component; they are private implementation dependencies.
  TARGET_LINK_OPTIONS(${MRS_COMPONENT_TARGET}
    PRIVATE "-Wl,--exclude-libs,ALL"
  )
ELSEIF(APPLE)
  TARGET_LINK_OPTIONS(${MRS_COMPONENT_TARGET}
    PRIVATE "LINKER:-exported_symbol,_list_components"
  )
ELSEIF(SOLARIS)
  IF(MY_COMPILER_IS_GNU_OR_CLANG)
      TARGET_COMPILE_OPTIONS(${MRS_COMPONENT_TARGET} PRIVATE -fvisibility=hidden)
  ELSE()
    TARGET_COMPILE_OPTIONS(${MRS_COMPONENT_TARGET} PRIVATE -xldscope=hidden)
  ENDIF()
ENDIF()

# Set include directories for static and generated headers
TARGET_INCLUDE_DIRECTORIES(${MRS_COMPONENT_TARGET}
  PRIVATE
    ${CMAKE_SOURCE_DIR}/router/src/router/include
    ${CMAKE_SOURCE_DIR}/router/src/harness/include
    ${CMAKE_BINARY_DIR}/router/
    ${CMAKE_BINARY_DIR}/router/include
    ${CMAKE_BINARY_DIR}/router/src/harness/include
    ${CMAKE_BINARY_DIR}/router/src/router/include
)

ADD_DEPENDENCIES(${MRS_COMPONENT_TARGET} GenError)

IF(NOT WITH_ROUTER)
  ADD_DEPENDENCIES(${MRS_COMPONENT_TARGET} mysqlrouter_mrs_client)
ENDIF()
