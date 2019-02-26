# Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

### DEFINITIONS ###

INCLUDE(configure.cmake)
INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/mysql_gcs.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/include/mysql/gcs/mysql_gcs.h)

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/xcom.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/include/xcom/xcom.h)

### RPC GEN ###

INCLUDE(rpcgen.cmake)

### INCLUDE FILES ###

INCLUDE_DIRECTORIES(
  ${CMAKE_BINARY_DIR}/include/                   # config.h
  ${CMAKE_SOURCE_DIR}/libbinlogevents/include/   # byteorder.h
  ${SSL_INCLUDE_DIRS}                            # SSL include dirs
  ${LZ4_INCLUDE_DIR}                             # LZ4
  ${XCOM_WINDEPS_INCLUDE_DIRS}                   # win deps and sun rpc
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include/sql_service
  ${CMAKE_CURRENT_SOURCE_DIR}/libmysqlgcs/include
  ${CMAKE_CURRENT_SOURCE_DIR}/libmysqlgcs/include/mysql
  ${CMAKE_CURRENT_SOURCE_DIR}/libmysqlgcs/include/mysql/gcs
  ${CMAKE_CURRENT_SOURCE_DIR}/libmysqlgcs/src/bindings/xcom/xcom
  ${CMAKE_CURRENT_BINARY_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/xdr_gen)

### SOURCES ###

SET(XCOM_SOURCES
  # XCom
  libmysqlgcs/src/bindings/xcom/xcom/pax_msg.c
  libmysqlgcs/src/bindings/xcom/xcom/xcom_base.c
  libmysqlgcs/src/bindings/xcom/xcom/xcom_memory.c
  libmysqlgcs/src/bindings/xcom/xcom/xcom_vp_str.c
  libmysqlgcs/src/bindings/xcom/xcom/xcom_cfg.c
  libmysqlgcs/src/bindings/xcom/xcom/xcom_cache.c
  libmysqlgcs/src/bindings/xcom/xcom/xcom_transport.c
  libmysqlgcs/src/bindings/xcom/xcom/node_address.c
  libmysqlgcs/src/bindings/xcom/xcom/xcom_msg_queue.c
  libmysqlgcs/src/bindings/xcom/xcom/xcom_statistics.c
  libmysqlgcs/src/bindings/xcom/xcom/app_data.c
  libmysqlgcs/src/bindings/xcom/xcom/site_def.c
  libmysqlgcs/src/bindings/xcom/xcom/xcom_ssl_transport.c
  libmysqlgcs/src/bindings/xcom/xcom/xcom_recover.c
  libmysqlgcs/src/bindings/xcom/xcom/task.c
  libmysqlgcs/src/bindings/xcom/xcom/bitset.c
  libmysqlgcs/src/bindings/xcom/xcom/simset.c
  libmysqlgcs/src/bindings/xcom/xcom/node_set.c
  libmysqlgcs/src/bindings/xcom/xcom/xcom_interface.c
  libmysqlgcs/src/bindings/xcom/xcom/node_list.c
  libmysqlgcs/src/bindings/xcom/xcom/task_debug.c
  libmysqlgcs/src/bindings/xcom/xcom/sock_probe.c
  libmysqlgcs/src/bindings/xcom/xcom/task_net.c
  libmysqlgcs/src/bindings/xcom/xcom/synode_no.c
  libmysqlgcs/src/bindings/xcom/xcom/xcom_detector.c
  ${XCOM_SUNRPC_SOURCES}
  ${XCOM_RPCGEN_SOURCES})

SET(GCS_INTERFACE_SOURCES
  libmysqlgcs/src/interface/gcs_logging.cc
  libmysqlgcs/src/interface/xplatform/my_xp_cond.cc
  libmysqlgcs/src/interface/xplatform/my_xp_thread.cc
  libmysqlgcs/src/interface/xplatform/my_xp_mutex.cc
  libmysqlgcs/src/interface/xplatform/my_xp_util.cc
  libmysqlgcs/src/interface/gcs_view.cc
  libmysqlgcs/src/interface/gcs_types.cc
  libmysqlgcs/src/interface/gcs_message.cc
  libmysqlgcs/src/interface/gcs_member_identifier.cc
  libmysqlgcs/src/interface/gcs_group_identifier.cc)


SET(GCS_SOURCES
  ${GCS_INTERFACE_SOURCES}
  libmysqlgcs/src/gcs_interface_factory.cc
  libmysqlgcs/src/bindings/xcom/gcs_xcom_communication_interface.cc
  libmysqlgcs/src/bindings/xcom/gcs_xcom_control_interface.cc
  libmysqlgcs/src/bindings/xcom/gcs_xcom_group_management.cc
  libmysqlgcs/src/bindings/xcom/gcs_internal_message.cc
  libmysqlgcs/src/bindings/xcom/gcs_xcom_networking.cc
  libmysqlgcs/src/bindings/xcom/gcs_xcom_utils.cc
  libmysqlgcs/src/bindings/xcom/gcs_xcom_state_exchange.cc
  libmysqlgcs/src/bindings/xcom/gcs_xcom_group_member_information.cc
  libmysqlgcs/src/bindings/xcom/gcs_message_stages.cc
  libmysqlgcs/src/bindings/xcom/gcs_xcom_view_identifier.cc
  libmysqlgcs/src/bindings/xcom/gcs_xcom_interface.cc
  libmysqlgcs/src/bindings/xcom/gcs_xcom_notification.cc
  libmysqlgcs/src/bindings/xcom/gcs_message_stage_lz4.cc
  libmysqlgcs/src/bindings/xcom/gcs_xcom_statistics_interface.cc)

SET(GROUP_REPLICATION_SOURCES
  src/applier.cc
  src/auto_increment.cc
  src/certifier.cc
  src/channel_observation_manager.cc
  src/delayed_plugin_initialization.cc
  src/gcs_event_handlers.cc
  src/gcs_logger.cc
  src/gcs_operations.cc
  src/gcs_plugin_messages.cc
  src/gcs_view_modification_notifier.cc
  src/group_partition_handling.cc
  src/member_info.cc
  src/observer_server_actions.cc
  src/observer_server_channels.cc
  src/observer_server_state.cc
  src/observer_trans.cc
  src/pipeline_factory.cc
  src/pipeline_stats.cc
  src/plugin.cc
  src/plugin_psi.cc
  src/plugin_utils.cc
  src/ps_information.cc
  src/read_mode_handler.cc
  src/recovery.cc
  src/recovery_channel_state_observer.cc
  src/applier_channel_state_observer.cc
  src/asynchronous_channels_state_observer.cc
  src/recovery_message.cc
  src/recovery_state_transfer.cc
  src/replication_threads_api.cc
  src/single_primary_message.cc
  src/member_version.cc
  src/compatibility_module.cc
  src/handlers/applier_handler.cc
  src/handlers/certification_handler.cc
  src/handlers/event_cataloger.cc
  src/sql_service/sql_resultset.cc
  src/sql_service/sql_service_context.cc
  src/sql_service/sql_service_context_base.cc
  src/sql_service/sql_service_interface.cc
  src/sql_service/sql_command_test.cc
  src/sql_service/sql_service_command.cc
)


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


IF(WITH_UNIT_TESTS)
  ADD_CONVENIENCE_LIBRARY(gr_unit_test_resource
    src/gcs_plugin_messages.cc
    src/member_info.cc
    src/member_version.cc
    src/compatibility_module.cc
    ${GCS_INTERFACE_SOURCES}
  )
  SET_TARGET_PROPERTIES(gr_unit_test_resource
                        PROPERTIES
                        COMPILE_DEFINITIONS "DISABLE_ALL_PSI")
ENDIF()

### Setting some switches on for Debugging ###
IF(WITH_GCS_LOG_TRACE)
  ADD_COMPILE_FLAGS(${XCOM_SOURCES} COMPILE_FLAGS
                                             "-DWITH_LOG_DEBUG -DWITH_LOG_TRACE")
  ADD_COMPILE_FLAGS(${GCS_SOURCES} COMPILE_FLAGS
                                             "-DWITH_LOG_DEBUG -DWITH_LOG_TRACE")
ELSEIF(WITH_GCS_LOG_DEBUG)
  ADD_COMPILE_FLAGS(${XCOM_SOURCES} COMPILE_FLAGS
                                             "-DWITH_LOG_DEBUG")
  ADD_COMPILE_FLAGS(${GCS_SOURCES} COMPILE_FLAGS
                                             "-DWITH_LOG_DEBUG")
ENDIF()

MY_CHECK_C_COMPILER_FLAG("-Wcast-function-type" HAVE_CAST_FUNCTION_TYPE)
IF(HAVE_CAST_FUNCTION_TYPE)
  ADD_COMPILE_FLAGS(libmysqlgcs/src/bindings/xcom/xcom/xcom_transport.c
                    COMPILE_FLAGS "-Wno-cast-function-type")
ENDIF()

### Configuration ###

# disabling warnings for generated code
IF(MSVC)
  ADD_COMPILE_FLAGS(${XCOM_SUN_RPCS} ${XCOM_RPCGEN_SOURCES}
                    COMPILE_FLAGS "/wd4018")
ELSEIF(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR
       CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  ADD_COMPILE_FLAGS(${XCOM_SUN_RPCS} ${XCOM_RPCGEN_SOURCES}
    COMPILE_FLAGS
    "-Wno-unused-variable -Wno-unused-function -Wno-conversion -Wno-switch")
ENDIF()

# declare the plugin itself
MYSQL_ADD_PLUGIN(group_replication
                 ${XCOM_SOURCES}
                 ${GCS_SOURCES}
                 ${GROUP_REPLICATION_SOURCES}
                 LINK_LIBRARIES
                 ${LZ4_LIBRARY}
                 ${SSL_LIBRARIES}
                 ${TIRPC_LIBRARIES}
                 MODULE_ONLY MODULE_OUTPUT_NAME "group_replication")

### INSTALLATION ###

IF(INSTALL_MYSQLTESTDIR)
  INSTALL(
    FILES tests/mtr/my.cnf
          tests/mtr/rpl_1slave_base.cnf
    DESTINATION "${INSTALL_MYSQLTESTDIR}/suite/group_replication"
    COMPONENT Test
  )
  INSTALL(
    DIRECTORY tests/mtr/t tests/mtr/r tests/mtr/inc
    DESTINATION "${INSTALL_MYSQLTESTDIR}/suite/group_replication"
    USE_SOURCE_PERMISSIONS
    COMPONENT Test
    PATTERN "collections" EXCLUDE
  )
ENDIF()
