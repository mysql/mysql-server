# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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



FILE(GLOB ngs_HDRS
  "${MYSQLX_PROJECT_DIR}/ngs/include/ngs/*.h"
  "${MYSQLX_PROJECT_DIR}/ngs/include/ngs_common/*.h"
  "${MYSQLX_PROJECT_DIR}/ngs/include/ngs/protocol/*.h"
  "${MYSQLX_PROJECT_DIR}/ngs/include/ngs/capabilities/*.h"
)

FILE(GLOB ngs_SRC
  "${MYSQLX_PROJECT_DIR}/ngs/src/*.cc"
  "${MYSQLX_PROJECT_DIR}/ngs/ngs_common/*.cc"
  "${MYSQLX_PROJECT_DIR}/ngs/src/protocol/*.cc"
  "${MYSQLX_PROJECT_DIR}/ngs/src/capabilities/*.cc"
)

SET(xplugin_HDRS
  "${MYSQLX_PROJECT_DIR}/src/io/xpl_listener_tcp.h"
  "${MYSQLX_PROJECT_DIR}/src/io/xpl_listener_unix_socket.h"
  "${MYSQLX_PROJECT_DIR}/src/io/vio_input_stream.h"
  "${MYSQLX_PROJECT_DIR}/src/xpl_server.h"
  "${MYSQLX_PROJECT_DIR}/src/xpl_session.h"
  "${MYSQLX_PROJECT_DIR}/src/xpl_client.h"
  "${MYSQLX_PROJECT_DIR}/src/xpl_dispatcher.h"
  "${MYSQLX_PROJECT_DIR}/src/xpl_system_variables.h"
  "${MYSQLX_PROJECT_DIR}/src/xpl_common_status_variables.h"
  "${MYSQLX_PROJECT_DIR}/src/xpl_global_status_variables.h"
  "${MYSQLX_PROJECT_DIR}/src/xpl_session_status_variables.h"
  "${MYSQLX_PROJECT_DIR}/src/xpl_log.h"
  "${MYSQLX_PROJECT_DIR}/src/xpl_regex.h"
  "${MYSQLX_PROJECT_DIR}/src/xpl_performance_schema.h"
  "${MYSQLX_PROJECT_DIR}/src/auth_plain.h"
  "${MYSQLX_PROJECT_DIR}/src/auth_challenge_response.h"
  "${MYSQLX_PROJECT_DIR}/src/native_plain_verification.h"
  "${MYSQLX_PROJECT_DIR}/src/native_verification.h"
  "${MYSQLX_PROJECT_DIR}/src/cache_based_verification.h"
  "${MYSQLX_PROJECT_DIR}/src/sha256_plain_verification.h"
  "${MYSQLX_PROJECT_DIR}/src/sha2_plain_verification.h"
  "${MYSQLX_PROJECT_DIR}/src/sha256_password_cache.h"
  "${MYSQLX_PROJECT_DIR}/src/account_verification_handler.h"
  "${MYSQLX_PROJECT_DIR}/src/admin_cmd_handler.h"
  "${MYSQLX_PROJECT_DIR}/src/admin_cmd_arguments.h"
  "${MYSQLX_PROJECT_DIR}/src/admin_cmd_index.h"
  "${MYSQLX_PROJECT_DIR}/src/query_string_builder.h"
  "${MYSQLX_PROJECT_DIR}/src/expr_generator.h"
  "${MYSQLX_PROJECT_DIR}/src/crud_cmd_handler.h"
  "${MYSQLX_PROJECT_DIR}/src/buffering_command_delegate.h"
  "${MYSQLX_PROJECT_DIR}/src/callback_command_delegate.h"
  "${MYSQLX_PROJECT_DIR}/src/streaming_command_delegate.h"
  "${MYSQLX_PROJECT_DIR}/src/sql_data_context.h"
  "${MYSQLX_PROJECT_DIR}/src/sql_data_result.h"
  "${MYSQLX_PROJECT_DIR}/src/xpl_resultset.h"
  "${MYSQLX_PROJECT_DIR}/src/sql_user_require.h"
  "${MYSQLX_PROJECT_DIR}/src/json_utils.h"
  "${MYSQLX_PROJECT_DIR}/src/expect/expect.h"
  "${MYSQLX_PROJECT_DIR}/src/expect/expect_condition.h"
  "${MYSQLX_PROJECT_DIR}/src/expect/expect_stack.h"
  "${MYSQLX_PROJECT_DIR}/src/statement_builder.h"
  "${MYSQLX_PROJECT_DIR}/src/expect/expect_condition_field.h"
  "${MYSQLX_PROJECT_DIR}/src/expect/expect_condition_docid.h"
  "${MYSQLX_PROJECT_DIR}/src/update_statement_builder.h"
  "${MYSQLX_PROJECT_DIR}/src/find_statement_builder.h"
  "${MYSQLX_PROJECT_DIR}/src/insert_statement_builder.h"
  "${MYSQLX_PROJECT_DIR}/src/delete_statement_builder.h"
  "${MYSQLX_PROJECT_DIR}/src/view_statement_builder.h"
  "${MYSQLX_PROJECT_DIR}/src/notices.h"
  "${MYSQLX_PROJECT_DIR}/src/cap_handles_expired_passwords.h"
  "${MYSQLX_PROJECT_DIR}/src/mysql_function_names.h"
  "${MYSQLX_PROJECT_DIR}/src/service_registrator.h"
  "${MYSQLX_PROJECT_DIR}/src/services/mysqlx_maintenance.h"
  "${MYSQLX_PROJECT_DIR}/src/udf/registrator.h"
  "${MYSQLX_PROJECT_DIR}/src/udf/mysqlx_error.h"
  "${MYSQLX_PROJECT_DIR}/src/global_timeouts.h"
  ${ngs_HDRS}
)

SET(xplugin_SRC
  "${MYSQLX_PROJECT_DIR}/src/xpl_log.cc"
  "${MYSQLX_PROJECT_DIR}/src/io/xpl_listener_tcp.cc"
  "${MYSQLX_PROJECT_DIR}/src/io/xpl_listener_unix_socket.cc"
  "${MYSQLX_PROJECT_DIR}/src/io/vio_input_stream.cc"
  "${MYSQLX_PROJECT_DIR}/src/xpl_server.cc"
  "${MYSQLX_PROJECT_DIR}/src/xpl_session.cc"
  "${MYSQLX_PROJECT_DIR}/src/xpl_client.cc"
  "${MYSQLX_PROJECT_DIR}/src/xpl_dispatcher.cc"
  "${MYSQLX_PROJECT_DIR}/src/xpl_system_variables.cc"
  "${MYSQLX_PROJECT_DIR}/src/xpl_regex.cc"
  "${MYSQLX_PROJECT_DIR}/src/xpl_plugin.cc"
  "${MYSQLX_PROJECT_DIR}/src/xpl_performance_schema.cc"
  "${MYSQLX_PROJECT_DIR}/src/io/xpl_listener_factory.cc"
  "${MYSQLX_PROJECT_DIR}/src/mysql_variables.cc"
  "${MYSQLX_PROJECT_DIR}/src/mysql_function_names.cc"
  "${MYSQLX_PROJECT_DIR}/src/mysql_show_variable_wrapper.cc"
  "${MYSQLX_PROJECT_DIR}/src/auth_plain.cc"
  "${MYSQLX_PROJECT_DIR}/src/native_plain_verification.cc"
  "${MYSQLX_PROJECT_DIR}/src/native_verification.cc"
  "${MYSQLX_PROJECT_DIR}/src/cache_based_verification.cc"
  "${MYSQLX_PROJECT_DIR}/src/sha256_plain_verification.cc"
  "${MYSQLX_PROJECT_DIR}/src/sha2_plain_verification.cc"
  "${MYSQLX_PROJECT_DIR}/src/sha256_password_cache.cc"
  "${MYSQLX_PROJECT_DIR}/src/account_verification_handler.cc"
  "${MYSQLX_PROJECT_DIR}/src/admin_cmd_handler.cc"
  "${MYSQLX_PROJECT_DIR}/src/admin_cmd_arguments.cc"
  "${MYSQLX_PROJECT_DIR}/src/admin_cmd_index.cc"
  "${MYSQLX_PROJECT_DIR}/src/query_formatter.cc"
  "${MYSQLX_PROJECT_DIR}/src/query_string_builder.cc"
  "${MYSQLX_PROJECT_DIR}/src/expr_generator.cc"
  "${MYSQLX_PROJECT_DIR}/src/crud_cmd_handler.cc"
  "${MYSQLX_PROJECT_DIR}/src/buffering_command_delegate.cc"
  "${MYSQLX_PROJECT_DIR}/src/callback_command_delegate.cc"
  "${MYSQLX_PROJECT_DIR}/src/streaming_command_delegate.cc"
  "${MYSQLX_PROJECT_DIR}/src/sql_data_context.cc"
  "${MYSQLX_PROJECT_DIR}/src/sql_data_result.cc"
  "${MYSQLX_PROJECT_DIR}/src/sql_user_require.cc"
  "${MYSQLX_PROJECT_DIR}/src/json_utils.cc"
  "${MYSQLX_PROJECT_DIR}/src/expect/expect.cc"
  "${MYSQLX_PROJECT_DIR}/src/expect/expect_stack.cc"
  "${MYSQLX_PROJECT_DIR}/src/expect/expect_condition_field.cc"
  "${MYSQLX_PROJECT_DIR}/src/statement_builder.cc"
  "${MYSQLX_PROJECT_DIR}/src/update_statement_builder.cc"
  "${MYSQLX_PROJECT_DIR}/src/find_statement_builder.cc"
  "${MYSQLX_PROJECT_DIR}/src/delete_statement_builder.cc"
  "${MYSQLX_PROJECT_DIR}/src/view_statement_builder.cc"
  "${MYSQLX_PROJECT_DIR}/src/insert_statement_builder.cc"
  "${MYSQLX_PROJECT_DIR}/src/notices.cc"
  "${MYSQLX_PROJECT_DIR}/src/service_registrator.cc"
  "${MYSQLX_PROJECT_DIR}/src/services/mysqlx_maintenance.cc"
  "${MYSQLX_PROJECT_DIR}/src/udf/registrator.cc"
  "${MYSQLX_PROJECT_DIR}/src/udf/mysqlx_error.cc"
  ${ngs_SRC}
)

SET(xplugin_stubbed_SRC
  "${MYSQLX_PROJECT_DIR}/src/xpl_plugin.cc"
  "${MYSQLX_PROJECT_DIR}/src/xpl_performance_schema.cc"
)

SET(xplugin_all_SRC
  ${xplugin_SRC}
  ${xplugin_stubbed_SRC}
)

SET(xplugin_global_reset_SRC
  "${MYSQLX_PROJECT_DIR}/src/components/global_status_reset.cc"
)