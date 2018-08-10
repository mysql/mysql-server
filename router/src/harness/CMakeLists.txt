# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

# Basic variables
SET(HARNESS_INSTALL_LIBRARY_DIR "${ROUTER_INSTALL_LIBDIR}" CACHE PATH
    "Installation directory for Harness libraries")
SET(HARNESS_INSTALL_BIN_DIR "bin" CACHE PATH
    "Installation directory for Harness main binaries")
SET(ENABLE_HARNESS_PROGRAM YES CACHE BOOL
  "Whether to build and install the Harness main program")
SET(HARNESS_INSTALL_PLUGINS YES CACHE BOOL
  "Whether to install the Harness provided plugins")

SET(HARNESS_NAME "harness"
  CACHE STRING "Name of Harness")
SET(INSTALL_INCLUDE_DIR "include/mysql/harness")
SET(HARNESS_INSTALL_INCLUDE_PREFIX "include/mysql")
SET(HARNESS_INSTALL_PROGRAM YES
  CACHE BOOL "Whether to install the Harness main program")

INCLUDE(Plugin)

# Platform specifics
IF(WIN32)
  INCLUDE(CheckIncludeFileCXX)
  CHECK_INCLUDE_FILE_CXX("shlwapi.h" Shlwapi_FOUND)
  IF(Shlwapi_FOUND)
    SET(SHLWAPI_LIBRARIES "shlwapi.dll")
  ELSE()
    MESSAGE(FATAL_ERROR "Shlwapi library not found")
  ENDIF()
ELSE()
  SET(SHLWAPI_LIBRARIES)
ENDIF()

IF(WITH_UNIT_TESTS AND GMOCK_FOUND)
  ADD_SUBDIRECTORY(shared)
ENDIF()

INCLUDE(${CMAKE_SOURCE_DIR}/cmake/compile_flags.cmake)
INCLUDE(GenerateExportHeader)

SET(MY_SSL_SOURCE_DIR ${MySQLRouter_SOURCE_DIR}/src/router/src/common)
SET(MY_SSL_INCLUDE_DIRS ${MySQLRouter_SOURCE_DIR}/src/router/include
                        ${SSL_INCLUDE_DIRS})

SET(harness_source
  src/loader.cc src/utilities.cc src/config_parser.cc src/designator.cc
  src/loader_config.cc
  src/common.cc  src/filesystem.cc
  src/arg_handler.cc
  src/dim.cc
  src/hostname_validator.cc
  src/mysql_router_thread.cc
  src/process_launcher.cc
  src/logging/handler.cc
  src/logging/logger.cc
  src/logging/logging.cc
  src/logging/registry.cc
  src/random_generator.cc
  src/socket_operations.cc
  src/tcp_address.cc
  src/keyring/keyring_manager.cc
  src/keyring/keyring_memory.cc
  src/keyring/keyring_file.cc
  src/networking/ip_address.cc
  src/networking/ipv4_address.cc
  src/networking/ipv6_address.cc
  src/networking/resolver.cc)

IF(WITH_SSL STREQUAL "wolfssl")
  SET(MY_AES_IMPL src/my_aes_wolfssl.cc)
ELSE()
  SET(MY_AES_IMPL src/my_aes_openssl.cc)
ENDIF()

SET(harness_source ${harness_source} ${MY_AES_IMPL})

IF(WIN32)
  LIST(APPEND harness_source
    src/filesystem-windows.cc src/utilities-windows.cc src/loader-windows.cc)
ELSE()
  LIST(APPEND harness_source
    src/filesystem-posix.cc src/utilities-posix.cc src/loader-posix.cc)
ENDIF()

INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}/include/mysql/harness
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${MySQLRouter_BINARY_DIR}/include
  ${MY_SSL_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/mysys_ssl/)

IF(WIN32)
  SET(WINSOCK_LIBRARIES Ws2_32.lib)
ENDIF()
SET(common_libraries ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT}
                     ${SHLWAPI_LIBRARIES} ${WINSOCK_LIBRARIES} ${SSL_LIBRARIES})

CONFIGURE_FILE(plugin.h.in ${MySQLRouter_BINARY_DIR}/${INSTALL_INCLUDE_DIR}/plugin.h
  ESCAPE_QUOTES @ONLY)

# create harness library - static version
ADD_LIBRARY(harness-archive STATIC ${harness_source})
TARGET_LINK_LIBRARIES(harness-archive ${common_libraries})
TARGET_INCLUDE_DIRECTORIES(harness-archive PUBLIC include)
IF(WIN32)
  SET_TARGET_PROPERTIES(harness-archive PROPERTIES
    COMPILE_FLAGS -DHARNESS_STATIC_DEFINE)
ENDIF()
IF(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(PUBLIC harness-archive "-pthread")
ENDIF()

# create harness library - dynamic version
ADD_LIBRARY(harness-library SHARED ${harness_source})
TARGET_LINK_LIBRARIES(harness-library ${common_libraries})
TARGET_INCLUDE_DIRECTORIES(harness-library PUBLIC include)

generate_export_header(harness-library
  BASE_NAME HARNESS
  EXPORT_FILE_NAME ${MySQLRouter_BINARY_DIR}/include/harness_export.h)

IF(${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
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

SET_PATH_TO_SSL(harness-library ${CMAKE_BINARY_DIR}/library_output_directory)

# don't install headers until a) a final destination is found and b) API is stable
# FILE(GLOB harness_headers include/*.h)
# INSTALL(FILES ${harness_headers}
#   DESTINATION ${HARNESS_INSTALL_INCLUDE_PREFIX}/${HARNESS_NAME})
IF(NOT WIN32)
  INSTALL(TARGETS harness-archive harness-library
        LIBRARY DESTINATION ${HARNESS_INSTALL_LIBRARY_DIR}
        ARCHIVE DESTINATION ${HARNESS_INSTALL_LIBRARY_DIR}
        COMPONENT Router)
ELSE()
  INSTALL(TARGETS harness-library 
        RUNTIME DESTINATION "${HARNESS_INSTALL_BIN_DIR}"
        COMPONENT Router)
ENDIF()

IF(WITH_UNIT_TESTS AND GMOCK_FOUND)
  ADD_SUBDIRECTORY(tests)
ENDIF()
