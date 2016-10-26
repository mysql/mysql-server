# Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT("xcom")

# SET the required libraries list to empty. It will be
# filled in as configuration evolves
SET (XCOM_REQUIRED_LIBS "")

# Configure platforms low-level specific directives
# Note: It must be called before setting C flags. Otherwise, the flags
#       will cause wrong platform directives.
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform.cmake)

OPTION(WITH_XCOM_SSL "Coonfigure XCom with SSL code" ON)
OPTION(WITH_SSL_STANDALONE "Configure XCom to build standalone from the server" OFF)

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/cmake/utils.cmake)

IF(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR
   CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # C++ is used only to compile Yassl. However, we don't own
  # it and by consequence we don't force any error or warning.
  # Remove the warning options for Yassl
  STRING(REGEX REPLACE "-W[a-z+-]* +" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  SET_CXX_FLAGS_NOT_EXIST(-fPIC -Wno-error)

  STRING(REPLACE "-Wno-error" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  SET_C_FLAGS_NOT_EXIST(-fPIC -Wall -Wextra -Wconversion -Wwrite-strings)
  SET_C_FLAGS_NOT_EXIST(-Wmissing-format-attribute -Wlogical-op -Wundef)

  # Avoid the warnings:
  # %lu is expecting a long but not longlong on 32bit platform
  # OSX defined uint64_t as unsigned long long on 64bit platform
  SET_C_FLAGS_NOT_EXIST(-Wno-format -Wno-format-security)

  # Disable socket FD micros related warnings. In some versions of GCC,
  # FD_XXX functions and htons cause warnings that:
  # conversion to unsigned short from int
  # conversion to long unsigned int from int
  SET_SOURCE_FILES_PROPERTIES(task.c xcom_base.c
    PROPERTIES COMPILE_FLAGS "-Wno-conversion")

  # -Wlogical-op is not supported on some platforms.
  CHECK_C_COMPILER_FLAG(-Wlogical-op HAS_LOGICAL_OP)
  IF (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR NOT HAS_LOGICAL_OP)
    STRING(REPLACE "-Wlogical-op" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  ENDIF()

ENDIF()


IF(WITH_TRACE_OUTPUT)
  ADD_DEFINITIONS(-DWITH_LOG_DEBUG -DWITH_LOG_TRACE)
ELSEIF(WITH_DEBUG_OUTPUT)
  ADD_DEFINITIONS(-DWITH_LOG_DEBUG)
ENDIF()

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DYASSL_MYSQL_COMPATIBLE")

IF(WITH_XCOM_SSL AND WITH_SSL_STANDALONE)
  FIND_PACKAGE (Threads REQUIRED)
  INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssl.cmake)
ENDIF()

ADD_DEFINITIONS(-DHAVE_CONFIG_H)
ADD_DEFINITIONS(-DXCOM_EMBEDDED)

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/include/config.h)

IF(WITH_XCOM_SSL)
  ADD_DEFINITIONS(-DXCOM_HAVE_OPENSSL)

  IF(NOT WITH_SSL)
    MESSAGE("Setting the bundled SSL because nothing was provided.")
    SET(WITH_SSL bundled)
  ENDIF()

  IF(WITH_SSL_STANDALONE)
    ADD_DEFINITIONS(-DWITH_SSL_STANDALONE)
    MYSQL_CHECK_SSL()
  ENDIF()

  IF(DEFINED SSL_INCLUDE_DIRS)
    INCLUDE_DIRECTORIES(${SSL_INCLUDE_DIRS})
  ENDIF()

  IF(DEFINED SSL_DEFINES)
    ADD_DEFINITIONS(${SSL_DEFINES})
  ENDIF()

  IF(NOT DEFINED SSL_LIBRARIES)
    MESSAGE(FATAL_ERROR "XCom configured to build with SSL, but no SSL library location provided. Check if you are in standalone mode.")
  ENDIF()

  SET (XCOM_REQUIRED_LIBS ${XCOM_REQUIRED_LIBS} ${SSL_LIBRARIES})
ENDIF()



# Configure RPC (and XDR)
#
# This defines SUNRPC_SRCS as well
#
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/cmake/rpcgen.cmake)

SET(XCOM_SRCS
  ${GEN_RPC_C_FILES}
app_data.c
bitset.c
node_address.c
node_list.c
node_set.c
pax_msg.c
simset.c
site_def.c
sock_probe.c
synode_no.c
task_debug.c
task_net.c
task.c
xcom_cfg.c
xcom_base.c
xcom_cache.c
xcom_detector.c
xcom_interface.c
xcom_memory.c
xcom_msg_queue.c
xcom_recover.c
xcom_statistics.c
xcom_ssl_transport.c
xcom_transport.c
xcom_vp_str.c
  ${GEN_RPC_H_FILES}
  ${GEN_RPC_H_PLATFORM_FILES}
)

message(STATUS "XCOM_SRCS ${XCOM_SRCS}")
message(STATUS "GEN_RPC_C_FILES ${GEN_RPC_C_FILES}")
message(STATUS "GEN_RPC_H_FILES ${GEN_RPC_H_FILES} ${GEN_RPC_H_PLATFORM_FILES}")

# Compiler switch to disable no unused warning for generated files
IF(WIN32)
  # Disable warnings for generated files. Not much we can do about those
  SET_SOURCE_FILES_PROPERTIES(${GEN_RPC_C_FILES}
                              PROPERTIES COMPILE_FLAGS "/wd4101")
ENDIF()

IF(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR
   CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Disable warnings for generated files. Not much we can do about those
  SET_SOURCE_FILES_PROPERTIES(${GEN_RPC_C_FILES}
                              PROPERTIES COMPILE_FLAGS "-Wno-unused-variable -Wno-unused-function -w -Wno-conversion -Wno-switch")
ENDIF()
# Not needed on solaris as rpcgen on solaris outputs clean code

SET_SOURCE_FILES_PROPERTIES(${XCOM_SRCS}
                            PROPERTIES OBJECT_DEPENDS ${GEN_RPC_H_FILES} ${GEN_RPC_H_PLATFORM_FILES})

ADD_LIBRARY(xcom STATIC ${XCOM_SRCS} ${SUNRPC_SRCS})

#IF (NOT DEFINED NO_SHARED_XCOM_LIB)
#  ADD_LIBRARY(xcom_shared SHARED ${XCOM_SRCS} ${SUNRPC_SRCS})
  if (NOT DEFINED NO_SHARED_XCOM_LIB)
  ADD_LIBRARY(xcom_shared SHARED ${XCOM_SRCS} ${SUNRPC_SRCS})


# Set dependency between xcom and xcom_shared so that
# we avoid a race condition when building in parallel
# while calling rpcgen for the xdr files
  ADD_DEPENDENCIES(xcom_shared xcom)
ENDIF()

IF (NOT NO_XCOM_EXAMPLES)
  IF (EXISTS ${PROJECT_SOURCE_DIR}/examples)
    # Include examples
    ADD_SUBDIRECTORY(examples)
  ELSE()
    MESSAGE (WARNING "Unable to find examples sources! Skipping examples.")
  ENDIF()
ENDIF()

TARGET_LINK_LIBRARIES(xcom ${XCOM_REQUIRED_LIBS})

IF (NOT DEFINED NO_SHARED_XCOM_LIB)
  TARGET_LINK_LIBRARIES(xcom_shared ${XCOM_REQUIRED_LIBS})
ENDIF()

