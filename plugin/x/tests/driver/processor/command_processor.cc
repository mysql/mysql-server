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

#include "plugin/x/tests/driver/processor/command_processor.h"

#include "my_dbug.h"

#include "plugin/x/tests/driver/processor/commands/command.h"
#include "plugin/x/tests/driver/processor/execution_context.h"

Block_processor::Result Command_processor::feed(std::istream &input,
                                                const char *command_line) {
  bool out_command_has_prefix;
  const bool command_found = m_command.is_command_registred(
      command_line, nullptr, &out_command_has_prefix);

  if (command_found && out_command_has_prefix) {
    return execute(input, command_line);
  }

  return Result::Not_hungry;
}

Block_processor::Result Command_processor::execute(std::istream &input,
                                                   const char *command_line) {
  DBUG_TRACE;
  DBUG_LOG("debug", "executing following cmd \"" << command_line << "\"");

  const auto execution_result =
      m_command.process(input, m_context, command_line);

  switch (execution_result) {
    case Command::Result::Stop_with_failure:
      return Result::Indigestion;

    case Command::Result::Stop_with_success:
      return Result::Everyone_not_hungry;

    default:
      return Result::Eaten_but_not_hungry;
  }
}
