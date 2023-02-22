# Copyright (c) 2020, 2023, Oracle and/or its affiliates.
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

SET(MYSQL_SERVER_COMPONENT_SOURCES
  audit_api_message_service_imp.cc
  backup_lock_service.cc
  clone_protocol_service.cc
  component_status_var_service.cc
  component_sys_var_service.cc
  dynamic_loader_path_filter.cc
  host_application_signal_imp.cc
  keyring_iterator_service_imp.cc
  log_builtins.cc
  log_builtins_filter.cc
  log_sink_buffer.cc
  log_sink_perfschema.cc
  log_sink_trad.cc
  log_source_backtrace.cc
  mysql_cond_service.cc
  mysql_connection_attributes_iterator_imp.cc
  mysql_current_thread_reader_imp.cc
  mysql_mutex_service.cc
  mysql_psi_system_service.cc
  mysql_runtime_error.cc
  mysql_rwlock_service.cc
  mysql_server_runnable_imp.cc
  mysql_string_service.cc
  ongoing_transaction_query_service.cc
  page_track_service.cc
  persistent_dynamic_loader.cc
  plugin_registry_service.cc
  security_context_imp.cc
  server_component.cc
  system_variable_source_imp.cc
  table_access_service.cc
  udf_metadata_imp.cc
  audit_api_connection_service_imp.cc
  mysql_query_attributes_imp.cc
  mysql_server_keyring_lockable_imp.cc
  mysql_system_variable_update_imp.cc
  mysql_thd_attributes_imp.cc
  transaction_delegate_control_imp.cc
  mysql_audit_print_service_longlong_data_source_imp.cc
  mysql_audit_print_service_double_data_source_imp.cc
  mysql_command_services_imp.cc
  mysql_command_consumer_imp.cc
  mysql_command_delegates.cc
  mysql_command_backend.cc
  mysql_status_variable_reader_imp.cc
  storing_auto_thd.cc
  mysql_thd_store_imp.cc
  set_variables_helper.cc
  )

# This static library is used to build mysqld binary and in some unit test cases
ADD_LIBRARY(mysql_server_component_services STATIC
  ${MYSQL_SERVER_COMPONENT_SOURCES})
TARGET_COMPILE_DEFINITIONS(mysql_server_component_services
  PRIVATE WITH_MYSQL_COMPONENTS_TEST_DRIVER)
TARGET_LINK_LIBRARIES(mysql_server_component_services PRIVATE extra::rapidjson)
TARGET_LINK_LIBRARIES(mysql_server_component_services PRIVATE ext::zlib)
ADD_DEPENDENCIES(mysql_server_component_services GenError)
