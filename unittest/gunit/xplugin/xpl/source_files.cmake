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

SET(XPL_TEST_SRC
  account_verification_handler_t.cc
  admin_cmd_arguments_object_t.cc
  admin_cmd_index_t.cc
  admin_create_collection_t.cc
  admin_get_collection_options_t.cc
  admin_modify_collection_options_t.cc
  broker_input_queue_task_t.cc
  callback_command_delegate_t.cc
  capabilities_configurator_t.cc
  capabilities_handlers_t.cc
  crud_statement_builder_t.cc
  cursor_t.cc
  delete_statement_builder_t.cc
  document_id_generator_t.cc
  expect_noerror_t.cc
  expr_generator_t.cc
  expr_generator_parametric_t.cc
  find_statement_builder_t.cc
  getter_any_t.cc
  index_array_field_t.cc
  index_field_t.cc
  insert_statement_builder_t.cc
  json_utils_t.cc
  listener_tcp_t.cc
  listener_unix_socket_t.cc
  message_builder_t.cc
  meta_schema_validator_t.cc
  mock/component_services.cc
  mock/mock.cc
  mock/srv_session_services.cc
  mysqlx_pb_wrapper.cc
  prepared_statement_builder_t.cc
  prepare_param_handler_t.cc
  protocol_decoder_t.cc
  query_string_builder_t.cc
  row_builder_t.cc
  sasl_authentication_t.cc
  sasl_challenge_response_auth_t.cc
  sasl_plain_auth_t.cc
  scheduler_t.cc
  server_client_timeout_t.cc
  set_variable_adaptor_t.cc
  sha256_cache_t.cc
  socket_acceptor_task_t.cc
  socket_events_t.cc
  sql_data_context_t.cc
  sql_statement_builder_t.cc
  stubs/command_service.cc
  stubs/log.cc
  stubs/log_subsystem.cc
  stubs/misc.cc
  stubs/pfs.cc
  stubs/plugin.cc
  stubs/security_context_service.cc
  stubs/sql_session_service.cc
  sync_variable_t.cc
  timeouts_t.cc
  update_statement_builder_t.cc
  user_password_verification_t.cc
  view_statement_builder_t.cc
  xdatetime_t.cc
  xdecimal_t.cc
  xmessage_buffer_t.cc
  xpl_regex_t.cc
  xrow_buffer_t.cc
)

