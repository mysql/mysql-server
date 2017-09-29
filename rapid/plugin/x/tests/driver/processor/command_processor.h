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

#ifndef X_TESTS_DRIVER_PROCESSOR_COMMAND_PROCESSOR_H_
#define X_TESTS_DRIVER_PROCESSOR_COMMAND_PROCESSOR_H_

#include "plugin/x/tests/driver/processor/block_processor.h"
#include "plugin/x/tests/driver/processor/commands/command.h"
#include "plugin/x/tests/driver/processor/execution_context.h"


class Command_processor : public Block_processor {
 public:
  explicit Command_processor(Execution_context *context)
      : m_context(context) {}

  Result feed(std::istream &input, const char *command_line) override;

 protected:
  Result execute(std::istream &input, const char *command_line);

  Execution_context *m_context;
  Command m_command;
};

#endif  // X_TESTS_DRIVER_PROCESSOR_COMMAND_PROCESSOR_H_
