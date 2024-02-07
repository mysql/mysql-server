# Copyright (c) 2020, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

SET(XPL_SRC
  account_verification_handler.cc
  admin_cmd_arguments.cc
  admin_cmd_collection_handler.cc
  admin_cmd_handler.cc
  admin_cmd_index.cc
  auth_plain.cc
  buffering_command_delegate.cc
  cache_based_verification.cc
  callback_command_delegate.cc
  capabilities/capability_compression.cc
  capabilities/configurator.cc
  capabilities/handler_auth_mech.cc
  capabilities/handler_client_interactive.cc
  capabilities/handler_connection_attributes.cc
  capabilities/handler_tls.cc
  challenge_response_verification.cc
  client.cc
  crud_cmd_handler.cc
  custom_command_delegates.cc
  delete_statement_builder.cc
  document_id_aggregator.cc
  expect/expect.cc
  expect/expect_condition_field.cc
  expect/expect_stack.cc
  expr_generator.cc
  find_statement_builder.cc
  helper/generate_hash.cc
  helper/multithread/cond.cc
  helper/multithread/initializer.cc
  helper/multithread/mutex.cc
  helper/multithread/rw_lock.cc
  helper/multithread/xsync_point.cc
  index_array_field.cc
  index_field.cc
  insert_statement_builder.cc
  io/connection_type.cc
  io/vio_input_stream.cc
  io/xpl_listener_factory.cc
  io/xpl_listener_tcp.cc
  io/xpl_listener_unix_socket.cc
  json_utils.cc
  meta_schema_validator.cc
  module_cache.cc
  module_mysqlx.cc
  mq/broker_task.cc
  mq/notice_input_queue.cc
  mq/notice_output_queue.cc
  mysql_function_names.cc
  mysql_show_variable_wrapper.cc
  mysql_variables.cc
  native_plain_verification.cc
  native_verification.cc
  ngs/client_list.cc
  ngs/document_id_generator.cc
  ngs/message_cache.cc
  ngs/message_decoder.cc
  ngs/notice_descriptor.cc
  ngs/protocol_decoder.cc
  ngs/protocol_encoder.cc
  ngs/protocol_encoder_compression.cc
  ngs/protocol_flusher.cc
  ngs/protocol_flusher_compression.cc
  ngs/protocol/page_pool.cc
  ngs/scheduler.cc
  ngs/server_client_timeout.cc
  ngs/socket_acceptors_task.cc
  ngs/socket_events.cc
  ngs/thread.cc
  ngs/vio_wrapper.cc
  notices.cc
  operations_factory.cc
  prepare_command_handler.cc
  prepared_statement_builder.cc
  prepare_param_handler.cc
  protocol_monitor.cc
  query_formatter.cc
  query_string_builder.cc
  server/authentication_container.cc
  server/builder/server_builder.cc
  server/builder/ssl_context_builder.cc
  server/server.cc
  server/server_factory.cc
  server/session_scheduler.cc
  services/mysqlx_group_membership_listener.cc
  services/mysqlx_group_member_status_listener.cc
  services/mysqlx_maintenance.cc
  services/registrator.cc
  services/service_audit_api_connection.cc
  services/service_registry_registration.cc
  services/services.cc
  services/service_sys_variables.cc
  services/service_udf_registration.cc
  session.cc
  sha256_password_cache.cc
  sha256_plain_verification.cc
  sha2_plain_verification.cc
  sql_data_context.cc
  sql_data_result.cc
  sql_statement_builder.cc
  sql_user_require.cc
  ssl_context.cc
  ssl_context_options.cc
  ssl_session_options.cc
  statement_builder.cc
  stmt_command_handler.cc
  streaming_command_delegate.cc
  udf/mysqlx_error.cc
  udf/mysqlx_generate_document_id.cc
  udf/mysqlx_get_prepared_statement_id.cc
  udf/registrator.cc
  udf/registry.cc
  update_statement_builder.cc
  variables/status_variables.cc
  variables/system_variables.cc
  view_statement_builder.cc
  xpl_dispatcher.cc
  xpl_log.cc
  xpl_performance_schema.cc
  xpl_plugin.cc
  xpl_regex.cc
)

