# Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
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

SET (GROUP_REPLICATION_SOURCES
     applier.cc
     binding_factory.cc
     certifier.cc
     gcs_event_handlers.cc
     member_info.cc
     observer_server_actions.cc
     observer_server_state.cc
     observer_trans.cc
     pipeline_factory.cc
     plugin.cc
     plugin_messages.cc
     plugin_psi.cc
     ps_information.cc
     recovery.cc
     recovery_message.cc
     replication_threads_api.cc
     handlers/certification_handler.cc
     handlers/event_cataloger.cc
     handlers/applier_handler.cc
    )

ADD_LIBRARY(gcs_unit_test_resource STATIC member_info.cc)

MYSQL_ADD_PLUGIN(group_replication ${GROUP_REPLICATION_SOURCES}
                 MODULE_ONLY MODULE_OUTPUT_NAME "group_replication")

if (LIBCPG)
    TARGET_LINK_LIBRARIES(group_replication gcs_interface gcs_corosync)
endif(LIBCPG)

SET_TARGET_PROPERTIES(group_replication PROPERTIES
                      LIBRARY_OUTPUT_DIRECTORY
                      ${PROJECT_BINARY_DIR}/plugin/group_replication)

SET_TARGET_PROPERTIES(gcs_unit_test_resource PROPERTIES
                      ARCHIVE_OUTPUT_DIRECTORY
                      ${PROJECT_BINARY_DIR}/plugin/group_replication)