/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "plugin/x/tests/driver/processor/dump_message_block_processor.h"

#include <string>
#include <vector>

#include "plugin/x/tests/driver/common/utils_string_parsing.h"


std::string Dump_message_block_processor::get_message_name(
    const char *linebuf) {
  const char *command_dump = "-->binparse";
  std::vector<std::string> args;

  aux::split(args, linebuf, " ", true);

  if (4 != args.size()) return "";

  if (args[0] == command_dump && args[3] == "{") {
    m_variable_name = args[1];
    return args[2];
  }

  return "";
}

int Dump_message_block_processor::process(
    const xcl::XProtocol::Client_message_type_id msg_id,
    const xcl::XProtocol::Message &message) {
  std::string bin_message = message_to_bindump(message);

  m_context->m_variables->set(m_variable_name, bin_message);

  return 0;
}
