# Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.
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

SET (XCOM_BASEDIR
     ${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/xcom/xcom/)

### DEFINITIONS ###

INCLUDE(configure.cmake)

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/mysql_gcs.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/include/mysql/gcs/mysql_gcs.h)

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/xcom.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/include/xcom/xcom.h)

### RPC GEN ###

INCLUDE(rpcgen.cmake)

### INCLUDE FILES ###

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/libbinlogevents/include/   # byteorder.h
  ${XCOM_WINDEPS_INCLUDE_DIRS}                   # win deps and sun rpc
)

### SOURCES ###

SET(XCOM_SOURCES
  # XCom
  src/bindings/xcom/xcom/pax_msg.c
  src/bindings/xcom/xcom/xcom_base.c
  src/bindings/xcom/xcom/xcom_memory.c
  src/bindings/xcom/xcom/xcom_vp_str.c
  src/bindings/xcom/xcom/xcom_cfg.c
  src/bindings/xcom/xcom/xcom_cache.c
  src/bindings/xcom/xcom/xcom_transport.c
  src/bindings/xcom/xcom/node_address.c
  src/bindings/xcom/xcom/xcom_msg_queue.c
  src/bindings/xcom/xcom/xcom_statistics.c
  src/bindings/xcom/xcom/app_data.c
  src/bindings/xcom/xcom/site_def.c
  src/bindings/xcom/xcom/xcom_ssl_transport.c
  src/bindings/xcom/xcom/xcom_recover.c
  src/bindings/xcom/xcom/task.c
  src/bindings/xcom/xcom/bitset.c
  src/bindings/xcom/xcom/simset.c
  src/bindings/xcom/xcom/node_set.c
  src/bindings/xcom/xcom/xcom_interface.c
  src/bindings/xcom/xcom/node_list.c
  src/bindings/xcom/xcom/task_debug.c
  src/bindings/xcom/xcom/sock_probe.c
  src/bindings/xcom/xcom/task_net.c
  src/bindings/xcom/xcom/synode_no.c
  src/bindings/xcom/xcom/xcom_detector.c
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
  src/interface/gcs_psi.cc)


SET(GCS_SOURCES
  ${GCS_INTERFACE_SOURCES}
  src/gcs_interface_factory.cc
  src/bindings/xcom/gcs_xcom_communication_interface.cc
  src/bindings/xcom/gcs_xcom_control_interface.cc
  src/bindings/xcom/gcs_xcom_group_management.cc
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
  src/bindings/xcom/gcs_xcom_statistics_interface.cc)

ADD_DEFINITIONS(${SSL_DEFINES})

IF(WITH_SSL MATCHES "bundled")
    # add YASSL_MYSQL_COMPATIBLE when building XCom and GCS with SSL.
    # This needs to exist because XCom is implemented in pure C.
    # Setting these flags will make yaSSL code to build with C compatible
    # interface.
    ADD_COMPILE_FLAGS(${XCOM_SOURCES} COMPILE_FLAGS
                      "-DYASSL_MYSQL_COMPATIBLE")
    ADD_COMPILE_FLAGS(${GCS_SOURCES} COMPILE_FLAGS
                      "-DYASSL_MYSQL_COMPATIBLE")
ENDIF()

# add the definition to build XCom with SSL support
ADD_DEFINITIONS(-DXCOM_HAVE_OPENSSL)

### Configuration ###

# Used to inhibit false posives on CLang 3.8/8.0
IF(CMAKE_C_COMPILER_ID MATCHES "Clang")
  ADD_COMPILE_FLAGS(${XCOM_SOURCES}
                    COMPILE_FLAGS
                    "-Wno-conditional-uninitialized")
ENDIF()

# disabling warnings for generated code
IF(MSVC)

  # This disables signed/unsigned mismatch warnings
  # Compiler warning C4018.
  IF(WIN32_CLANG)
    ADD_COMPILE_FLAGS(${XCOM_SUN_RPCS} ${XCOM_RPCGEN_SOURCES}
      COMPILE_FLAGS "-Wno-sign-compare")
  ELSE()
    ADD_COMPILE_FLAGS(${XCOM_SUN_RPCS} ${XCOM_RPCGEN_SOURCES}
      COMPILE_FLAGS "/wd4018")
  ENDIF()
ELSEIF(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR
       CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  ADD_COMPILE_FLAGS(${XCOM_SOURCES} ${XCOM_SUN_RPCS} ${XCOM_RPCGEN_SOURCES}
    COMPILE_FLAGS
    "-Wno-unused-variable -Wno-unused-function -Wno-conversion -Wno-switch")
ENDIF()

ADD_COMPILE_FLAGS(${XCOM_SOURCES} COMPILE_FLAGS "-DMYSQL_DYNAMIC_PLUGIN")
ADD_COMPILE_FLAGS(${GCS_SOURCES} COMPILE_FLAGS "-DMYSQL_DYNAMIC_PLUGIN")

ADD_CONVENIENCE_LIBRARY(${GCS_LIBRARY} ${XCOM_SOURCES} ${GCS_SOURCES})

IF(TIRPC_LIBRARY)
  TARGET_LINK_LIBRARIES(${GCS_LIBRARY} ${TIRPC_LIBRARY})
ENDIF()

# convenience target
ADD_CUSTOM_TARGET(lib${GCS_LIBRARY} DEPENDS ${GCS_LIBRARY})
