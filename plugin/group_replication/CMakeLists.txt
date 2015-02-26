# Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.
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

IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
  SET(LIBCPG_PATH /usr/lib/corosync /usr/lib64/corosync /usr/local/lib /usr/local/lib64 CACHE STRING "Default locations for libcpg (cluster library)" )

  CHECK_INCLUDE_FILES(corosync/cpg.h HAVE_COROSYNC_CPG_H)

  IF (NOT HAVE_COROSYNC_CPG_H)
    MESSAGE(STATUS
      "Skipping Group Replication plugin due to Corosync headers not found.")
    RETURN()
  ENDIF (NOT HAVE_COROSYNC_CPG_H)

  find_library(LIBCPG cpg ${LIBCPG_PATH})
  find_library(LIBPTHREAD pthread ${LIBPTHREAD_PATH})
  if (LIBCPG)
    CHECK_LIBRARY_EXISTS (${LIBCPG} cpg_local_get "" HAVE_LIBCPG)
    CHECK_INCLUDE_FILES (corosync/cpg.h HAVE_COROSYNC_CPG_H)
  endif (LIBCPG)

  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/plugin/group_replication/include)
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/plugin/group_replication/gcs/interface)
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/plugin/group_replication/gcs/bindings/corosync)

  SET (GROUP_REPLICATION_SOURCES
       src/applier.cc
       src/binding_factory.cc
       src/certifier.cc
       src/gcs_event_handlers.cc
       src/member_info.cc
       src/observer_server_actions.cc
       src/observer_server_state.cc
       src/observer_trans.cc
       src/pipeline_factory.cc
       src/plugin.cc
       src/plugin_messages.cc
       src/plugin_psi.cc
       src/ps_information.cc
       src/recovery.cc
       src/recovery_message.cc
       src/replication_threads_api.cc
       src/handlers/certification_handler.cc
       src/handlers/event_cataloger.cc
       src/handlers/applier_handler.cc
      )

  ADD_LIBRARY(gcs_unit_test_resource STATIC src/member_info.cc)

  ADD_LIBRARY(gcs_interface STATIC  gcs/interface/gcs_group_identifier.cc
                                    gcs/interface/gcs_member_identifier.cc
                                    gcs/interface/gcs_message.cc
                                    gcs/interface/gcs_view.cc)


  ADD_LIBRARY(gcs_corosync STATIC gcs/bindings/corosync/gcs_corosync_view_identifier.cc
                                  gcs/bindings/corosync/gcs_corosync_view_identifier.h
                                  gcs/bindings/corosync/gcs_state_exchange.cc
                                  gcs/bindings/corosync/gcs_state_exchange.h
                                  gcs/bindings/corosync/gcs_corosync_communication_interface.cc
                                  gcs/bindings/corosync/gcs_corosync_communication_interface.h
                                  gcs/bindings/corosync/gcs_corosync_control_interface.cc
                                  gcs/bindings/corosync/gcs_corosync_control_interface.h
                                  gcs/bindings/corosync/gcs_corosync_interface.cc
                                  gcs/bindings/corosync/gcs_corosync_interface.h
                                  gcs/bindings/corosync/gcs_corosync_statistics_interface.cc
                                  gcs/bindings/corosync/gcs_corosync_statistics_interface.h
                                  gcs/bindings/corosync/gcs_corosync_utils.cc
                                  gcs/bindings/corosync/gcs_corosync_utils.h)

  MYSQL_ADD_PLUGIN(group_replication ${GROUP_REPLICATION_SOURCES}
                   MODULE_ONLY MODULE_OUTPUT_NAME "group_replication")

  if (LIBCPG)
    TARGET_LINK_LIBRARIES(gcs_corosync gcs_interface ${LIBCPG} ${LIBPTHREAD} )
    TARGET_LINK_LIBRARIES(group_replication gcs_interface gcs_corosync)
  endif(LIBCPG)

  if(INSTALL_PLUGINTESTDIR AND INSTALL_MYSQLTESTDIR)
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
  endif()

endif(CMAKE_SYSTEM_NAME MATCHES "Linux")
