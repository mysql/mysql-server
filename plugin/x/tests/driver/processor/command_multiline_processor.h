/*
 * Copyright (c) 2017, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_TESTS_DRIVER_PROCESSOR_COMMAND_MULTILINE_PROCESSOR_H_
#define PLUGIN_X_TESTS_DRIVER_PROCESSOR_COMMAND_MULTILINE_PROCESSOR_H_

#include <string>
#include "plugin/x/tests/driver/processor/block_processor.h"
#include "plugin/x/tests/driver/processor/command_processor.h"

class Command_multiline_processor : public Command_processor {
 public:
  using Command_processor::Command_processor;

  Result feed(std::istream &input, const char *command_line) override;

 private:
  bool is_multiline(const char *command_line);
  bool append_and_check_command(const char *command_line,
                                const char **out_full_command,
                                bool *out_wrong_format);

  bool m_eating_multiline{false};
  std::string m_multiline_command;
};

#endif  // PLUGIN_X_TESTS_DRIVER_PROCESSOR_COMMAND_MULTILINE_PROCESSOR_H_
