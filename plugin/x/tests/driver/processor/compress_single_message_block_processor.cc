/*
 * Copyright (c) 2019, 2022, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/tests/driver/processor/compress_single_message_block_processor.h"

#include <string>
#include <vector>

#include "my_dbug.h"

#include "plugin/x/tests/driver/common/utils_string_parsing.h"

Block_processor::Result Compress_single_message_block_processor::feed(
    std::istream &input, const char *linebuf) {
  std::string helper_buffer;
  const char *line_to_process = linebuf;

  if (!is_eating()) {
    std::vector<std::string> args;
    const char *command_dump = "-->compress_and_send";

    aux::split(args, linebuf, " ", true);

    if (3 != args.size()) return Result::Not_hungry;

    if (args[0] != command_dump || args[2] != "{") return Result::Not_hungry;

    helper_buffer = args[1] + " {";
    line_to_process = helper_buffer.c_str();
  }

  return Send_message_block_processor::feed(input, line_to_process);
}

int Compress_single_message_block_processor::process(
    const xcl::XProtocol::Client_message_type_id msg_id,
    const xcl::XProtocol::Message &message) {
  DBUG_TRACE;
  auto error =
      m_context->m_connection->active_xprotocol()->send_compressed_frame(
          msg_id, message);

  if (error) {
    if (!m_context->m_expected_error.check_error(error)) return 1;
  } else {
    if (!m_context->m_expected_error.check_ok()) return 1;
  }

  return 0;
}
