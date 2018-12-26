# Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.
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

ADD_SUBDIRECTORY(libmysqlgcs)

SET(GROUP_REPLICATION_SOURCES
  src/applier.cc
  src/auto_increment.cc
  src/certifier.cc
  src/delayed_plugin_initialization.cc
  src/gcs_event_handlers.cc
  src/gcs_logger.cc
  src/gcs_operations.cc
  src/gcs_plugin_messages.cc
  src/gcs_view_modification_notifier.cc
  src/group_actions/group_action.cc
  src/group_actions/group_action_coordinator.cc
  src/group_actions/multi_primary_migration_action.cc
  src/group_actions/primary_election_action.cc
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
  src/recovery.cc
  src/recovery_channel_state_observer.cc
  src/applier_channel_state_observer.cc
  src/asynchronous_channels_state_observer.cc
  src/recovery_state_transfer.cc
  src/replication_threads_api.cc
  src/member_version.cc
  src/compatibility_module.cc
  src/handlers/applier_handler.cc
  src/handlers/certification_handler.cc
  src/handlers/event_cataloger.cc
  src/plugin_handlers/group_partition_handling.cc
  src/plugin_handlers/persistent_variables_handler.cc
  src/plugin_handlers/primary_election_invocation_handler.cc
  src/plugin_handlers/primary_election_primary_process.cc
  src/plugin_handlers/primary_election_secondary_process.cc
  src/plugin_handlers/primary_election_utils.cc
  src/plugin_handlers/server_ongoing_transactions_handler.cc
  src/plugin_handlers/stage_monitor_handler.cc
  src/plugin_handlers/read_mode_handler.cc
  src/plugin_messages/group_action_message.cc
  src/plugin_messages/group_validation_message.cc
  src/plugin_messages/recovery_message.cc
  src/plugin_messages/single_primary_message.cc
  src/plugin_handlers/primary_election_validation_handler.cc
  src/plugin_observers/channel_observation_manager.cc
  src/plugin_observers/group_event_observer.cc
  src/plugin_observers/group_transaction_observation_manager.cc
  src/sql_service/sql_resultset.cc
  src/sql_service/sql_service_context.cc
  src/sql_service/sql_service_context_base.cc
  src/sql_service/sql_service_interface.cc
  src/sql_service/sql_command_test.cc
  src/sql_service/sql_service_command.cc
  src/services/notification/notification.cc
  src/services/registry.cc
  src/udf/udf_multi_primary.cc
  src/udf/udf_registration.cc
  src/udf/udf_single_primary.cc
  src/udf/udf_utils.cc
  src/udf/udf_write_concurrency.cc
)

SET(GMS_LISTENER_SERVICE_TEST_SOURCES
    src/services/notification/impl/gms_listener_test.cc)

ADD_DEFINITIONS(-DMYSQL_SERVER)
ADD_DEFINITIONS(-DLOG_SUBSYSTEM_TAG="Repl")
ADD_DEFINITIONS(-DLOG_COMPONENT_TAG="group_replication")

IF(WITH_UNIT_TESTS)
  ADD_CONVENIENCE_LIBRARY(gr_unit_test_resource
    src/gcs_plugin_messages.cc
    src/member_info.cc
    src/member_version.cc
    src/compatibility_module.cc
  )
  SET_TARGET_PROPERTIES(gr_unit_test_resource
                        PROPERTIES
                        COMPILE_DEFINITIONS "DISABLE_PSI_MUTEX")
ENDIF()

### Configuration ###

# declare the plugin itself
MYSQL_ADD_PLUGIN(group_replication
                 ${GROUP_REPLICATION_SOURCES}
                 ${GMS_LISTENER_SERVICE_TEST_SOURCES}
                 LINK_LIBRARIES
                 ${LZ4_LIBRARY}
                 ${SSL_LIBRARIES}
                 ${GCS_LIBRARY}
                 MODULE_ONLY MODULE_OUTPUT_NAME "group_replication")
