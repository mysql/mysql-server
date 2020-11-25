# Copyright (c) 2016, 2020, Oracle and/or its affiliates. All rights reserved.
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
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

SET(XCOM_BASEDIR
  ${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/xcom/xcom/)

SET(XCOM_INCLUDE_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/xcom/)

SET (XCOM_INCLUDE_DIR
     ${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/xcom/)

### DEFINITIONS ###

INCLUDE(cmake/configure.cmake)

CONFIGURE_FILE(cmake/mysql_gcs.h.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/include/mysql/gcs/mysql_gcs.h)

CONFIGURE_FILE(cmake/xcom.h.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/include/xcom/xcom.h)

CONFIGURE_FILE(cmake/xcom_debug.in
  ${CMAKE_CURRENT_BINARY_DIR}/include/xcom/task_debug.h COPYONLY)

CONFIGURE_FILE(cmake/gcs_debug.in
  ${CMAKE_CURRENT_BINARY_DIR}/include/xcom/gcs_debug.h COPYONLY)

CONFIGURE_FILE(cmake/gcs_profile.in
  ${CMAKE_CURRENT_BINARY_DIR}/include/xcom/xcom_profile.h COPYONLY)

### RPC GEN ###

INCLUDE(cmake/rpcgen.cmake)

### INCLUDE FILES ###

INCLUDE_DIRECTORIES(
  ${XCOM_WINDEPS_INCLUDE_DIRS}                   # win deps and sun rpc
  ${XCOM_INCLUDE_DIR}                            # xcom include files
  ${CMAKE_CURRENT_BINARY_DIR}/include/        # generated xcom include files
  )

### SOURCES ###

SET(XCOM_SOURCES
  # XCom
  src/bindings/xcom/xcom/pax_msg.cc
  src/bindings/xcom/xcom/xcom_base.cc
  src/bindings/xcom/xcom/xcom_memory.cc
  src/bindings/xcom/xcom/xcom_vp_str.cc
  src/bindings/xcom/xcom/xcom_cfg.cc
  src/bindings/xcom/xcom/xcom_cache.cc
  src/bindings/xcom/xcom/xcom_transport.cc
  src/bindings/xcom/xcom/node_address.cc
  src/bindings/xcom/xcom/xcom_msg_queue.cc
  src/bindings/xcom/xcom/xcom_statistics.cc
  src/bindings/xcom/xcom/app_data.cc
  src/bindings/xcom/xcom/site_def.cc
  src/bindings/xcom/xcom/xcom_ssl_transport.cc
  src/bindings/xcom/xcom/xcom_recover.cc
  src/bindings/xcom/xcom/task.cc
  src/bindings/xcom/xcom/bitset.cc
  src/bindings/xcom/xcom/simset.cc
  src/bindings/xcom/xcom/node_set.cc
  src/bindings/xcom/xcom/xcom_interface.cc
  src/bindings/xcom/xcom/node_list.cc
  src/bindings/xcom/xcom/task_debug.cc
  src/bindings/xcom/xcom/sock_probe.cc
  src/bindings/xcom/xcom/task_net.cc
  src/bindings/xcom/xcom/synode_no.cc
  src/bindings/xcom/xcom/xcom_detector.cc
  src/bindings/xcom/xcom/xcom_input_request.cc
  src/bindings/xcom/xcom/get_synode_app_data.cc
  src/bindings/xcom/xcom/checked_data.cc
  src/bindings/xcom/xcom/xdr_checked_data.c
  ${XCOM_SUNRPC_SOURCES}
  ${XCOM_RPCGEN_SOURCES})

SET(GCS_INTERFACE_SOURCES
  src/interface/gcs_logging.cc
  src/interface/gcs_logging_system.cc
  src/interface/xplatform/my_xp_cond.cc
  src/interface/xplatform/my_xp_thread.cc
  src/interface/xplatform/my_xp_mutex.cc
  src/interface/xplatform/my_xp_util.cc
  src/interface/gcs_view.cc
  src/interface/gcs_types.cc
  src/interface/gcs_message.cc
  src/interface/gcs_member_identifier.cc
  src/interface/gcs_group_identifier.cc
  src/interface/gcs_psi.cc
  src/interface/gcs_tagged_lock.cc)


SET(GCS_SOURCES
  ${GCS_INTERFACE_SOURCES}
  src/gcs_interface_factory.cc
  src/bindings/xcom/gcs_xcom_communication_interface.cc
  src/bindings/xcom/gcs_xcom_control_interface.cc
  src/bindings/xcom/gcs_xcom_group_management.cc
  src/bindings/xcom/gcs_internal_message_headers.cc
  src/bindings/xcom/gcs_internal_message.cc
  src/bindings/xcom/gcs_xcom_networking.cc
  src/bindings/xcom/gcs_xcom_utils.cc
  src/bindings/xcom/gcs_xcom_state_exchange.cc
  src/bindings/xcom/gcs_xcom_group_member_information.cc
  src/bindings/xcom/gcs_message_stages.cc
  src/bindings/xcom/gcs_xcom_view_identifier.cc
  src/bindings/xcom/gcs_xcom_interface.cc
  src/bindings/xcom/gcs_xcom_notification.cc
  src/bindings/xcom/gcs_message_stage_lz4.cc
  src/bindings/xcom/gcs_xcom_statistics_interface.cc
  src/bindings/xcom/gcs_xcom_proxy.cc
  src/bindings/xcom/gcs_xcom_communication_protocol_changer.cc
  ../../../extra/lz4/xxhash.c # required by gcs_message_stage_split
  src/bindings/xcom/gcs_message_stage_split.cc
  src/bindings/xcom/gcs_xcom_synode.cc
  src/bindings/xcom/gcs_xcom_expels_in_progress.cc)

### Configuration ###

# Used to inhibit false posives on CLang 3.8/8.0
IF(MY_COMPILER_IS_CLANG)
  ADD_COMPILE_FLAGS(${XCOM_SOURCES}
    COMPILE_FLAGS
    "-Wno-conditional-uninitialized")
ENDIF()

# disabling warnings for generated code
IF(MSVC)

  # This disables signed/unsigned mismatch warnings
  # Compiler warning C4018.
  IF(WIN32_CLANG)
    ADD_COMPILE_FLAGS(${XCOM_SUNRPC_SOURCES} ${XCOM_RPCGEN_SOURCES}
      COMPILE_FLAGS "-Wno-sign-compare")
  ELSE()
    ADD_COMPILE_FLAGS(${XCOM_SUNRPC_SOURCES} ${XCOM_RPCGEN_SOURCES}
      COMPILE_FLAGS "/wd4018")
    # Also disable 'identifier' : unreferenced local variable warnings
    # Compiler warning C4101
    ADD_COMPILE_FLAGS(${XCOM_RPCGEN_SOURCES} COMPILE_FLAGS "/wd4101")
  ENDIF()
ELSEIF(MY_COMPILER_IS_GNU_OR_CLANG)
  ADD_COMPILE_FLAGS(
    src/bindings/xcom/xcom/xcom_base.cc
    src/bindings/xcom/xcom/sock_probe.cc
    COMPILE_FLAGS
    "-Wno-unused-function")
  ADD_COMPILE_FLAGS(${XCOM_SUNRPC_SOURCES} ${XCOM_RPCGEN_SOURCES}
    COMPILE_FLAGS
    -Wno-incompatible-pointer-types
    -Wno-unused-function
    -Wno-unused-local-typedefs
    -Wno-unused-parameter
    -Wno-unused-variable
    )
  IF(SOLARIS)
    # rpcgen generates 'void bzero();'
    ADD_COMPILE_FLAGS(${XCOM_RPCGEN_SOURCES}
      COMPILE_FLAGS
      "-Wno-builtin-declaration-mismatch"
      )
  ENDIF()
ENDIF()

ADD_COMPILE_DEFINITIONS(
  ${XCOM_SOURCES}
  ${GCS_SOURCES}
  COMPILE_DEFINITIONS MYSQL_DYNAMIC_PLUGIN)

ADD_COMPILE_DEFINITIONS(../../../extra/lz4/xxhash.c
  COMPILE_DEFINITIONS "XXH_NAMESPACE=GCS_")

# Warn about variable shadowing
IF(MYSQL_MAINTAINER_MODE)
  IF(MY_COMPILER_IS_CLANG)
    ADD_COMPILE_OPTIONS("-Wshadow")
  ELSEIF(MY_COMPILER_IS_GNU)
    # GCC's -Wshadow is too noisy. Starting with GCC 7 we can use the
    # more surgical -Wshadow=compatible-local
    IF(NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0)
      ADD_COMPILE_OPTIONS("-Wshadow=compatible-local")
    ENDIF()
  ENDIF()
ENDIF()

ADD_LIBRARY(mysqlgcs STATIC ${XCOM_SOURCES} ${GCS_SOURCES})
# Dependency on mysys from gcs_logging_system.cc
# but pick up symbols from the server:
# TARGET_LINK_LIBRARIES(mysqlgcs mysys)

IF(TIRPC_LIBRARIES)
  TARGET_LINK_LIBRARIES(mysqlgcs ${TIRPC_LIBRARIES})
ENDIF()

IF(MSVC)
  TARGET_LINK_LIBRARIES(mysqlgcs iphlpapi)
ENDIF()

# convenience target
ADD_CUSTOM_TARGET(libmysqlgcs DEPENDS mysqlgcs)
