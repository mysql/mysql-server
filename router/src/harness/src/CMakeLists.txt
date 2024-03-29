# Copyright (c) 2019, 2023, Oracle and/or its affiliates.
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
INCLUDE(Plugin)


# Generate dynamic_state_schema.cc
# With *one* common custom command, generating dynamic_state_schema.[cc|h]
# we had spurious build failures on Windows:
# CustomBuild: Failed to open output file 'dynamic_state_schema.cc':
# Permission denied
# Seems like Visual Studio wants to generate dynamic_state_schema.cc
# at least twice, once for the static library, once for the dynamic one.
# If both generation jobs run simultaneously, one of them may fail.
# The solution is to have separate commands, for the STATIC and SHARED targets.
FUNCTION(ADD_DYNAMIC_STATE_SCHEMA_COMMAND SHARED_OR_STATIC)
  SET(SUBDIR src_${SHARED_OR_STATIC})
  SET(GENERATED_SOURCES_${SHARED_OR_STATIC}
    ${CMAKE_CURRENT_BINARY_DIR}/${SUBDIR}/dynamic_state_schema.cc
    ${CMAKE_CURRENT_BINARY_DIR}/${SUBDIR}/dynamic_state_schema.h
    )
  SET(GENERATED_SOURCES_${SHARED_OR_STATIC}
    ${GENERATED_SOURCES_${SHARED_OR_STATIC}} PARENT_SCOPE)
  SET_SOURCE_FILES_PROPERTIES(${GENERATED_SOURCES_${SHARED_OR_STATIC}}
    PROPERTIES GENERATED TRUE)
  FILE(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${SUBDIR})

  ADD_CUSTOM_COMMAND(
    OUTPUT ${GENERATED_SOURCES_${SHARED_OR_STATIC}}
    COMMAND json_schema_embedder
    ${CMAKE_CURRENT_SOURCE_DIR}/dynamic_state_schema.js
    dynamic_state_schema.cc
    dynamic_state_schema.h
    "StateFileJsonSchema"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${SUBDIR}
    COMMENT "Generating sources for ${SHARED_OR_STATIC} harness library"
    DEPENDS dynamic_state_schema.js
    )
ENDFUNCTION()

ADD_DYNAMIC_STATE_SCHEMA_COMMAND(SHARED)
ADD_DYNAMIC_STATE_SCHEMA_COMMAND(STATIC)

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
  ${SHLWAPI_LIBRARIES} ${WINSOCK_LIBRARIES} ${SSL_LIBRARIES})

SET(INSTALL_INCLUDE_DIR "include/mysql/harness")

## for plugin.h.in
SET(MYSQL_HARNESS_ARCH_CPU ${MYSQL_MACHINE_TYPE})
SET(MYSQL_HARNESS_ARCH_OS ${CMAKE_SYSTEM_NAME})
SET(MYSQL_HARNESS_ARCH_COMPILER ${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION})
SET(MYSQL_HARNESS_ARCH_RUNTIME "*")

CONFIGURE_FILE(plugin.h.in ${MySQLRouter_BINARY_DIR}/${INSTALL_INCLUDE_DIR}/plugin.h
  ESCAPE_QUOTES @ONLY)

# create harness library - static version
ADD_LIBRARY(harness-archive STATIC
  ${harness_source} ${GENERATED_SOURCES_STATIC})
ADD_DEPENDENCIES(mysqlrouter_all harness-archive)

TARGET_LINK_LIBRARIES(harness-archive {common_libraries} extra::rapidjson)
TARGET_INCLUDE_DIRECTORIES(harness-archive PUBLIC
  ../include ${CMAKE_CURRENT_BINARY_DIR}/src_STATIC)
IF(WIN32)
  SET_TARGET_PROPERTIES(harness-archive PROPERTIES
    COMPILE_FLAGS -DHARNESS_STATIC_DEFINE)
ENDIF()

# create harness library - dynamic version
ADD_LIBRARY(harness-library SHARED
  ${harness_source} ${GENERATED_SOURCES_SHARED})
ADD_DEPENDENCIES(mysqlrouter_all harness-library)

# mysys is need for my_print_stacktrace
TARGET_LINK_LIBRARIES(harness-library
  ${common_libraries} mysys extra::rapidjson)
TARGET_INCLUDE_DIRECTORIES(harness-library PUBLIC
  ../include ${CMAKE_CURRENT_BINARY_DIR}/src_SHARED)

generate_export_header(harness-library
  BASE_NAME HARNESS
  EXPORT_FILE_NAME ${MySQLRouter_BINARY_DIR}/include/harness_export.h)

IF(SOLARIS)
  TARGET_LINK_LIBRARIES(harness-library -lnsl -lsocket)
  TARGET_LINK_LIBRARIES(harness-archive -lnsl -lsocket)
ENDIF()

IF(WIN32)
  SET_TARGET_PROPERTIES(harness-archive PROPERTIES
    OUTPUT_NAME "mysqlharness_a"
    PREFIX "lib"
    SOVERSION 1)
ELSE()
  SET_TARGET_PROPERTIES(harness-archive harness-library PROPERTIES
    OUTPUT_NAME "mysqlharness"
    PREFIX "lib"
    SOVERSION 1)
ENDIF()

SET_TARGET_PROPERTIES(harness-library PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory)

IF(WIN32)
  ADD_CUSTOM_COMMAND(TARGET harness-library POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_BINARY_DIR}/library_output_directory/${CMAKE_CFG_INTDIR}/harness-library.dll"
    "${CMAKE_BINARY_DIR}/runtime_output_directory/${CMAKE_CFG_INTDIR}/harness-library.dll"
   )
ENDIF()

ADD_INSTALL_RPATH_FOR_OPENSSL(harness-library)
SET_PATH_TO_CUSTOM_SSL_FOR_APPLE(harness-library)

IF(WIN32)
  INSTALL(TARGETS harness-library
    RUNTIME DESTINATION "${ROUTER_INSTALL_BINDIR}"
    COMPONENT Router)
ELSE()
  INSTALL(TARGETS harness-library
    LIBRARY
    DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
    NAMELINK_SKIP
    )
ENDIF()

ADD_LIBRARY(harness_stdx SHARED
  stdx/filesystem.cc
  stdx/io/file_handle.cc
)
ADD_DEPENDENCIES(mysqlrouter_all harness_stdx)
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

SET_TARGET_PROPERTIES(harness_stdx PROPERTIES
  OUTPUT_NAME "mysqlharness_stdx"
  SOVERSION 1
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/runtime_output_directory
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory)
INSTALL(TARGETS harness_stdx
  RUNTIME DESTINATION ${ROUTER_INSTALL_BINDIR} COMPONENT Router
  ARCHIVE DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  LIBRARY DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  NAMELINK_SKIP
  )

## harness_tls

ADD_LIBRARY(harness_tls SHARED
  tls_client_context.cc
  tls_context.cc
  tls_error.cc
  tls_server_context.cc
  )
ADD_DEPENDENCIES(mysqlrouter_all harness_tls)

IF(TARGET copy_openssl_dlls)
  ADD_DEPENDENCIES(harness_tls copy_openssl_dlls)
ENDIF()

## the rpath for the bundled openssl
ADD_INSTALL_RPATH_FOR_OPENSSL(harness_tls)
SET_PATH_TO_CUSTOM_SSL_FOR_APPLE(harness_tls)

GENERATE_EXPORT_HEADER(harness_tls
  EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/../include/mysql/harness/tls_export.h)

TARGET_INCLUDE_DIRECTORIES(harness_tls
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/
  ${CMAKE_CURRENT_BINARY_DIR}/../include/
  )
TARGET_LINK_LIBRARIES(harness_tls
  PUBLIC ${SSL_LIBRARIES}
  )
SET_TARGET_PROPERTIES(harness_tls PROPERTIES
  OUTPUT_NAME "mysqlharness_tls"
  SOVERSION 1
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/runtime_output_directory
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory
  )
INSTALL(TARGETS harness_tls
  RUNTIME DESTINATION ${ROUTER_INSTALL_BINDIR} COMPONENT Router
  ARCHIVE DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  LIBRARY DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
  NAMELINK_SKIP
  )
