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

#include "single_command_processor.h"

#include "processor/commands/command.h"
#include "processor/execution_context.h"


Block_processor::Result Single_command_processor::feed(
    std::istream &input,
    const char *linebuf) {
  if (m_command.is_command_syntax(linebuf)) {
    {
      Command::Result r =
          m_command.process(input, m_context, linebuf);
      if (Command::Result::Stop_with_failure == r)
        return Result::Indigestion;
      else if (Command::Result::Stop_with_success == r)
        return Result::Everyone_not_hungry;
    }

    return Result::Eaten_but_not_hungry;
  } else if (linebuf[0] == '#' || linebuf[0] == 0) {  // # comment
    return Result::Eaten_but_not_hungry;
  }

  return Result::Not_hungry;
}
