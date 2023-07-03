# Copyright (c) 2020, 2022, Oracle and/or its affiliates.
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

SET(XTEST_SRC
  common/command_line_options.cc
  common/message_matcher.cc
  common/utils_mysql_parsing.cc
  common/utils_string_parsing.cc
  connector/connection_manager.cc
  connector/mysqlx_all_msgs.cc
  connector/result_fetcher.cc
  connector/session_holder.cc
  connector/warning.cc
  driver_command_line_options.cc
  formatters/console.cc
  formatters/message_formatter.cc
  json_to_any_handler.cc
  mysqlxtest.cc
  parsers/message_parser.cc
  processor/command_multiline_processor.cc
  processor/command_processor.cc
  processor/commands/command.cc
  processor/commands/expected_error.cc
  processor/commands/expected_warnings.cc
  processor/commands/macro.cc
  processor/commands/mysqlxtest_error_names.cc
  processor/compress_single_message_block_processor.cc
  processor/dump_message_block_processor.cc
  processor/macro_block_processor.cc
  processor/multiple_compress_block_processor.cc
  processor/send_message_block_processor.cc
  processor/sql_block_processor.cc
  processor/sql_stmt_processor.cc
  processor/stream_processor.cc
)

