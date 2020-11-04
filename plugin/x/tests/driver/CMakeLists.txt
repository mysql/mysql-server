# Copyright (c) 2017, 2020, Oracle and/or its affiliates.
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
  driver_command_line_options.cc
  json_to_any_handler.cc
  mysqlx_charset.cc
  mysqlxtest.cc

  common/command_line_options.cc
  common/message_matcher.cc
  common/utils_mysql_parsing.cc
  common/utils_string_parsing.cc

  connector/connection_manager.cc
  connector/mysqlx_all_msgs.cc
  connector/result_fetcher.cc
  connector/session_holder.cc
  connector/warning.cc

  formatters/console.cc
  formatters/message_formatter.cc

  parsers/message_parser.cc

  processor/command_multiline_processor.cc
  processor/command_processor.cc
  processor/dump_message_block_processor.cc
  processor/macro_block_processor.cc
  processor/send_message_block_processor.cc
  processor/sql_block_processor.cc
  processor/sql_stmt_processor.cc
  processor/stream_processor.cc
  processor/compress_single_message_block_processor.cc
  processor/multiple_compress_block_processor.cc

  processor/commands/command.cc
  processor/commands/expected_error.cc
  processor/commands/expected_warnings.cc
  processor/commands/macro.cc
  processor/commands/mysqlxtest_error_names.cc

  ${CMAKE_SOURCE_DIR}/sql-common/net_ns.cc
  )

INCLUDE_DIRECTORIES(
  ${MYSQLX_GENERATE_DIR}
  ${MYSQLX_PROJECT_DIR}
  ${MYSQLX_PROJECT_DIR}/ngs/include
  ${MYSQLX_PROTOCOL_INCLUDE_DIR}
  ${MYSQLX_CLIENT_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

MY_INCLUDE_SYSTEM_DIRECTORIES(LIBEVENT)

MYSQL_ADD_EXECUTABLE(mysqlxtest
  ${XTEST_SRC}
  COMPONENT Test
  DEPENDENCIES GenError GenClientError
  LINK_LIBRARIES
  mysys
  strings
  mysqlclient
  ${MYSQLX_CLIENT_FULL_LIB}
  ${MYSQLX_PROTOCOL_FULL_LIB}
  ${PROTOBUF_LIBRARY}
  ${LZ4_LIBRARY}
)

IF(UNIX_INSTALL_RPATH_ORIGIN_PRIV_LIBDIR)
  ADD_INSTALL_RPATH_FOR_PROTOBUF(mysqlxtest)
ENDIF()

MYSQLX_APPEND_COMPILE_FLAGS(mysqlxtest
  "${MYSQLX_PROTOCOL_FLAGS}"
)

MYSQLX_APPEND_COMPILE_DEFINITIONS(mysqlxtest
  "NGS_DISABLE_LOGGING;USE_MYSQLX_FULL_PROTO"
)

MY_CHECK_CXX_COMPILER_WARNING("-Wunused-but-set-parameter" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  ADD_COMPILE_FLAGS(${XTEST_SRC} COMPILE_FLAGS ${HAS_WARN_FLAG})
ENDIF()
