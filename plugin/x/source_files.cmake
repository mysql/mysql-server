# Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
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

SET(ngs_SRC
  ngs/src/client.cc
  ngs/src/client_list.cc
  ngs/src/client_session.cc
  ngs/src/document_id_generator.cc
  ngs/src/message_cache.cc
  ngs/src/message_decoder.cc
  ngs/src/notice_descriptor.cc
  ngs/src/protocol_decoder.cc
  ngs/src/protocol_encoder.cc
  ngs/src/protocol_flusher.cc
  ngs/src/scheduler.cc
  ngs/src/server.cc
  ngs/src/server_client_timeout.cc
  ngs/src/socket_acceptors_task.cc
  ngs/src/socket_events.cc
  ngs/src/thread.cc
  ngs/src/vio_wrapper.cc

  ngs/src/protocol/page_pool.cc
)

SET(xplugin_SRC
  src/helper/multithread/mutex.cc
  src/helper/multithread/cond.cc
  src/helper/multithread/rw_lock.cc
  src/helper/generate_hash.cc
  src/expect/expect.cc
  src/expect/expect_stack.cc
  src/expect/expect_condition_field.cc
  src/io/xpl_listener_factory.cc
  src/io/xpl_listener_tcp.cc
  src/io/xpl_listener_unix_socket.cc
  src/io/vio_input_stream.cc
  src/io/connection_type.cc
  src/mq/broker_task.cc
  src/mq/notice_input_queue.cc
  src/mq/notice_output_queue.cc
  src/services/service_registrator.cc
  src/services/mysqlx_group_membership_listener.cc
  src/services/mysqlx_group_member_status_listener.cc
  src/services/mysqlx_maintenance.cc
  src/udf/registrator.cc
  src/udf/mysqlx_error.cc
  src/xpl_log.cc
  src/xpl_server.cc
  src/xpl_session.cc
  src/xpl_client.cc
  src/xpl_dispatcher.cc
  src/xpl_system_variables.cc
  src/xpl_regex.cc
  src/xpl_plugin.cc
  src/xpl_performance_schema.cc
  src/mysql_variables.cc
  src/mysql_function_names.cc
  src/mysql_show_variable_wrapper.cc
  src/auth_plain.cc
  src/native_plain_verification.cc
  src/native_verification.cc
  src/cache_based_verification.cc
  src/sha256_plain_verification.cc
  src/sha2_plain_verification.cc
  src/sha256_password_cache.cc
  src/account_verification_handler.cc
  src/admin_cmd_handler.cc
  src/admin_cmd_collection_handler.cc
  src/admin_cmd_arguments.cc
  src/admin_cmd_index.cc
  src/query_formatter.cc
  src/query_string_builder.cc
  src/expr_generator.cc
  src/json_generator.cc
  src/crud_cmd_handler.cc
  src/buffering_command_delegate.cc
  src/callback_command_delegate.cc
  src/streaming_command_delegate.cc
  src/custom_command_delegates.cc
  src/sql_data_context.cc
  src/sql_data_result.cc
  src/sql_user_require.cc
  src/json_utils.cc
  src/statement_builder.cc
  src/update_statement_builder.cc
  src/find_statement_builder.cc
  src/delete_statement_builder.cc
  src/view_statement_builder.cc
  src/insert_statement_builder.cc
  src/notices.cc
  src/prepared_statement_builder.cc
  src/prepare_command_handler.cc
  src/prepare_param_handler.cc
  src/stmt_command_handler.cc
  src/udf/registry.cc
  src/udf/mysqlx_generate_document_id.cc
  src/udf/mysqlx_get_prepared_statement_id.cc
  src/xpl_plugin.cc
  src/xpl_performance_schema.cc
  src/sql_statement_builder.cc
  src/document_id_aggregator.cc
  src/operations_factory.cc
  src/ssl_context.cc
  src/ssl_context_options.cc
  src/ssl_session_options.cc
  src/capabilities/configurator.cc
  src/capabilities/handler_auth_mech.cc
  src/capabilities/handler_client_interactive.cc
  src/capabilities/handler_connection_attributes.cc
  src/capabilities/handler_tls.cc
  src/index_field.cc
  src/index_array_field.cc

  ${ngs_SRC}
)

SET(xplugin_global_reset_SRC
  src/components/global_status_reset.cc
)
