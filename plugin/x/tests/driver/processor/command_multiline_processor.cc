/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

#include "processor/command_multiline_processor.h"

#include <algorithm>

namespace details {}  // namespace details

Block_processor::Result Command_multiline_processor::feed(
    std::istream &input, const char *command_line) {
  if (!is_multiline(command_line)) {
    return Result::Not_hungry;
  }

  const char *out_full_command = nullptr;
  bool out_wrong_format = false;

  if (!append_and_check_command(command_line, &out_full_command,
                                &out_wrong_format)) {
    return out_wrong_format ? Result::Indigestion : Result::Feed_more;
  }

  return execute(input, out_full_command);
}

bool Command_multiline_processor::is_multiline(const char *command_line) {
  if (m_eating_multiline) return true;

  bool out_has_command_prefix;
  const bool command_found = m_command.is_command_registred(
      command_line, nullptr, &out_has_command_prefix);

  if (command_found && !out_has_command_prefix) {
    m_multiline_command = "";
    m_eating_multiline = true;

    return true;
  }

  return false;
}

bool Command_multiline_processor::append_and_check_command(
    const char *command_line, const char **out_full_command,
    bool *out_wrong_format) {
  const char *end_of_command = strstr(command_line, ";");

  if (nullptr == end_of_command) {
    m_multiline_command += command_line;

    return false;
  } else {
    m_eating_multiline = false;
    m_multiline_command.append(command_line, end_of_command - command_line);
  }

  const bool are_whitespaces_only =
      std::all_of(end_of_command + 1, end_of_command + strlen(end_of_command),
                  [](const char element) -> bool {
                    return element == ' ' || element == '\t';
                  });

  m_eating_multiline = false;
  *out_full_command = m_multiline_command.c_str();

  if (!are_whitespaces_only) {
    m_context->print_error("Multiline command must not have any ",
                           "characters after command end (';')\n");
    *out_wrong_format = true;

    return false;
  }

  return true;
}
