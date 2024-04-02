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
INCLUDE(Plugin)


# Generate dynamic_state_schema.cc and configuration_update_schema.cc
FUNCTION(ADD_SCHEMA_COMMAND SCHEMA_NAME CLASS_NAME)
  SET(GENERATED_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/${SCHEMA_NAME}.cc
    ${CMAKE_CURRENT_BINARY_DIR}/${SCHEMA_NAME}.h
    )
  SET(GENERATED_SOURCES
    ${GENERATED_SOURCES} PARENT_SCOPE)
  SET_SOURCE_FILES_PROPERTIES(${GENERATED_SOURCES}
    PROPERTIES GENERATED TRUE)

  ADD_CUSTOM_COMMAND(
    OUTPUT ${GENERATED_SOURCES}
    COMMAND json_schema_embedder
    ${CMAKE_CURRENT_SOURCE_DIR}/${SCHEMA_NAME}.json
    ${SCHEMA_NAME}.cc
    ${SCHEMA_NAME}.h
    ${CLASS_NAME}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating sources for harness library"
    DEPENDS ${SCHEMA_NAME}.json
    )

  ADD_CUSTOM_TARGET(json_schema_target_${SCHEMA_NAME}
    DEPENDS ${GENERATED_SOURCES}
    )
  ADD_DEPENDENCIES(clang_tidy_prerequisites
    json_schema_target_${SCHEMA_NAME}
    )
ENDFUNCTION(ADD_SCHEMA_COMMAND)

ADD_SCHEMA_COMMAND(dynamic_state_schema StateFileJsonSchema)

ADD_SCHEMA_COMMAND(configuration_update_schema
                   ConfigurationUpdateJsonSchema)

SET(harness_source
  access_rights.cc
  loader.cc
  dynamic_loader.cc
  utilities.cc
  config_option.cc
  config_parser.cc
  plugin_config.cc
  designator.cc
  dynamic_state.cc
  event_state_tracker.cc
  loader_config.cc
  common.cc
  filesystem.cc
  string_utils.cc
  arg_handler.cc
  builtin_plugins.cc
  dim.cc
  hostname_validator.cc
  mysql_router_thread.cc
  process_launcher.cc
  logging/consolelog_plugin.cc
  logging/filelog_plugin.cc
  logging/handler.cc
  logging/logger.cc
  logging/logger_plugin.cc
# logging/logging.cc       empty file
  logging/registry.cc
  log_reopen.cc
  random_generator.cc
  signal_handler.cc
  sd_notify.cc
  socket_operations.cc
  tcp_address.cc
  tls_cipher.cc
  tty.cc
  vt100.cc
  vt100_filter.cc
  keyring/keyring_manager.cc
  keyring/keyring_memory.cc
  keyring/keyring_file.cc
  keyring/master_key_file.cc
  process_state_component.cc
  log_reopen_component.cc
  dynamic_config.cc
  section_config_exposer.cc
  )

IF(WIN32)
  LIST(APPEND harness_source
    filesystem-windows.cc
    utilities-windows.cc
    logging/eventlog_plugin.cc)
ELSE()
  LIST(APPEND harness_source
    filesystem-posix.cc
    utilities-posix.cc
    logging/syslog_plugin.cc)
ENDIF()

INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_SOURCE_DIR}/
  ${MySQLRouter_BINARY_DIR}/include
  )

IF(WIN32)
  INCLUDE(CheckIncludeFileCXX)
  CHECK_INCLUDE_FILE_CXX("shlwapi.h" Shlwapi_FOUND)
  IF(Shlwapi_FOUND)
    SET(SHLWAPI_LIBRARIES "shlwapi.dll")
  ELSE()
    MESSAGE(FATAL_ERROR "Shlwapi library not found")
  ENDIF()

  SET(WINSOCK_LIBRARIES ws2_32)
ELSE()
  SET(SHLWAPI_LIBRARIES)
ENDIF()

SET(common_libraries ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT}
  ${SHLWAPI_LIBRARIES} ${WINSOCK_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto)

SET(INSTALL_INCLUDE_DIR "include/mysql/harness")

## for plugin.h.in
SET(MYSQL_HARNESS_ARCH_CPU ${MYSQL_MACHINE_TYPE})
SET(MYSQL_HARNESS_ARCH_OS ${CMAKE_SYSTEM_NAME})
SET(MYSQL_HARNESS_ARCH_COMPILER ${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION})
SET(MYSQL_HARNESS_ARCH_RUNTIME "*")

CONFIGURE_FILE(plugin.h.in ${MySQLRouter_BINARY_DIR}/${INSTALL_INCLUDE_DIR}/plugin.h
  ESCAPE_QUOTES @ONLY)

# create harness library - dynamic version
ROUTER_ADD_SHARED_LIBRARY(harness-library
  ${harness_source} ${GENERATED_SOURCES}
  DEPENDENCIES
  json_schema_target_dynamic_state_schema
  json_schema_target_configuration_update_schema

  NO_EXPORT_HEADER  # custom include-path

  OUTPUT_NAME "mysqlharness"

  LINK_LIBRARIES
  # mysys is need for my_print_stacktrace
  ${common_libraries} mysys extra::rapidjson
  )

TARGET_INCLUDE_DIRECTORIES(harness-library PUBLIC
  ../include
  ${CMAKE_CURRENT_BINARY_DIR})

generate_export_header(harness-library
  BASE_NAME HARNESS
  EXPORT_FILE_NAME ${MySQLRouter_BINARY_DIR}/include/harness_export.h)

IF(SOLARIS)
  TARGET_LINK_LIBRARIES(harness-library -lnsl -lsocket)
ENDIF()

ROUTER_ADD_SHARED_LIBRARY(harness_stdx
  stdx/filesystem.cc
  stdx/io/file_handle.cc

  NO_EXPORT_HEADER

  OUTPUT_NAME "mysqlharness_stdx"
)

TARGET_SOURCES(harness_stdx
  INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/mysql/harness/stdx/bit.h
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/mysql/harness/stdx/expected.h
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/mysql/harness/stdx/type_traits.h
  )
TARGET_INCLUDE_DIRECTORIES(harness_stdx
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/
  ${CMAKE_CURRENT_BINARY_DIR}/../include/
  )
GENERATE_EXPORT_HEADER(harness_stdx
  EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/../include/mysql/harness/stdx_export.h)

## harness_tls

ROUTER_ADD_SHARED_LIBRARY(harness_tls
  tls_client_context.cc
  tls_context.cc
  tls_error.cc
  tls_server_context.cc

  NO_EXPORT_HEADER  # custom include-path
  OUTPUT_NAME "mysqlharness_tls"

  LINK_LIBRARIES
  PUBLIC OpenSSL::SSL OpenSSL::Crypto
  )

IF(TARGET copy_openssl_dlls)
  ADD_DEPENDENCIES(harness_tls copy_openssl_dlls)
ENDIF()

GENERATE_EXPORT_HEADER(harness_tls
  EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/../include/mysql/harness/tls_export.h)

TARGET_INCLUDE_DIRECTORIES(harness_tls
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/
  ${CMAKE_CURRENT_BINARY_DIR}/../include/
  )
