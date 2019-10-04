# Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
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
  loader.cc
  utilities.cc
  config_parser.cc
  designator.cc
  dynamic_state.cc
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
  logging/logging.cc
  logging/registry.cc
  random_generator.cc
  socket_operations.cc
  tcp_address.cc
  tty.cc
  vt100.cc
  vt100_filter.cc
  keyring/keyring_manager.cc
  keyring/keyring_memory.cc
  keyring/keyring_file.cc
  keyring/master_key_file.cc
  networking/ip_address.cc
  networking/ipv4_address.cc
  networking/ipv6_address.cc
  networking/resolver.cc
  my_aes_openssl.cc)

IF(WIN32)
  LIST(APPEND harness_source
    filesystem-windows.cc
    utilities-windows.cc
    loader-windows.cc
    logging/eventlog_plugin.cc)
ELSE()
  LIST(APPEND harness_source
    filesystem-posix.cc
    utilities-posix.cc
    loader-posix.cc
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
  ${SHLWAPI_LIBRARIES} ${WINSOCK_LIBRARIES} ${SSL_LIBRARIES} mysys)

SET(INSTALL_INCLUDE_DIR "include/mysql/harness")

CONFIGURE_FILE(plugin.h.in ${MySQLRouter_BINARY_DIR}/${INSTALL_INCLUDE_DIR}/plugin.h
  ESCAPE_QUOTES @ONLY)

# create harness library - static version
ADD_LIBRARY(harness-archive STATIC
  ${harness_source} ${GENERATED_SOURCES_STATIC})
TARGET_LINK_LIBRARIES(harness-archive ${common_libraries})
TARGET_INCLUDE_DIRECTORIES(harness-archive PUBLIC
  ../include ${CMAKE_CURRENT_BINARY_DIR}/src_STATIC)
IF(WIN32)
  SET_TARGET_PROPERTIES(harness-archive PROPERTIES
    COMPILE_FLAGS -DHARNESS_STATIC_DEFINE)
ENDIF()

# create harness library - dynamic version
ADD_LIBRARY(harness-library SHARED
  ${harness_source} ${GENERATED_SOURCES_SHARED})
TARGET_LINK_LIBRARIES(harness-library ${common_libraries})
TARGET_INCLUDE_DIRECTORIES(harness-library PUBLIC
  ../include ${CMAKE_CURRENT_BINARY_DIR}/src_SHARED)

generate_export_header(harness-library
  BASE_NAME HARNESS
  EXPORT_FILE_NAME ${MySQLRouter_BINARY_DIR}/include/harness_export.h)

IF(SOLARIS)
  TARGET_LINK_LIBRARIES(harness-library -lnsl -lsocket)
  TARGET_LINK_LIBRARIES(harness-archive -lnsl -lsocket)
ENDIF()

IF(NOT WIN32)
  SET_TARGET_PROPERTIES(harness-archive harness-library PROPERTIES
    OUTPUT_NAME "mysqlharness"
    PREFIX "lib"
    SOVERSION 1)
ELSE()
  SET_TARGET_PROPERTIES(harness-archive PROPERTIES
    OUTPUT_NAME "mysqlharness_a"
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
SET_PATH_TO_SSL(harness-library ${CMAKE_BINARY_DIR}/library_output_directory)

IF(NOT WIN32)
  INSTALL(TARGETS harness-library
    LIBRARY
    DESTINATION ${ROUTER_INSTALL_LIBDIR} COMPONENT Router
    NAMELINK_SKIP
    )
ELSE()
  INSTALL(TARGETS harness-library
    RUNTIME DESTINATION "${ROUTER_INSTALL_BINDIR}"
    COMPONENT Router)
ENDIF()
