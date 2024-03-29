/*
 * Copyright (c) 2017, 2023, Oracle and/or its affiliates.
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

#include "plugin/x/tests/driver/processor/dump_message_block_processor.h"

#include <string>
#include <vector>

#include "my_dbug.h"

#include "plugin/x/tests/driver/common/utils_string_parsing.h"

Block_processor::Result Dump_message_block_processor::feed(
    std::istream &input, const char *linebuf) {
  std::string helper_buffer;
  const char *line_to_process = linebuf;

  if (!is_eating()) {
    std::vector<std::string> args;
    const char *command_bindump = "-->binparse";

    aux::split(args, linebuf, " ", true);

    if (4 != args.size()) return Result::Not_hungry;

    m_is_hex = false;

    if (args[0] != command_bindump || args[3] != "{") {
      const char *command_hexdump = "-->hexparse";
      if (args[0] != command_hexdump || args[3] != "{") {
        return Result::Not_hungry;
      }

      m_is_hex = true;
    }

    helper_buffer = args[2] + " {";
    m_variable_name = args[1];
    line_to_process = helper_buffer.c_str();
  }

  return Send_message_block_processor::feed(input, line_to_process);
}

int Dump_message_block_processor::process(
    const xcl::XProtocol::Client_message_type_id msg_id,
    const xcl::XProtocol::Message &message) {
  DBUG_TRACE;
  std::string bin_message;

  if (m_is_hex) {
    aux::hex(message_serialize(message), bin_message);
  } else {
    bin_message = message_to_bindump(message);
  }

  m_context->m_variables->set(m_variable_name, bin_message);

  return 0;
}
