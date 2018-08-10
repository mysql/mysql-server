/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQLROUTER_TESTS_APP_EXEC_INCLUDED
#define MYSQLROUTER_TESTS_APP_EXEC_INCLUDED

#include <string>
#include <utility>

/** @struct CmdExecResult
 *
 * Struct CmdExecResult contains the result of the execution of a
 * command. The output of the command is stored in the output member,
 * exit code in exit_code and if the command was signaled, the signal
 * will be available through the signal member.
 *
 * The output could possibly include the STDERR.
 */
struct CmdExecResult {
  /** @brief Output of the command */
  std::string output;
  /** @brief Exit code of the command execution */
  int exit_code;
  /** @brief Signal number when the command was signaled */
  int signal;
};

/** @brief Executes the given command
 *
 * Executes the given command and returns the result. If include_stderr
 * is true, messages going to STDERR are included in the output.
 *
 * When working_dir is provided, we change first to the given directory
 * and executed the command from there. We return to previous folder
 * when done.
 *
 * @param cmd Command to executed
 * @param include_stderr Include STDERR messages in output
 * @param working_dir Working directory
 * @return Returns CmdExecResult
 */
CmdExecResult cmd_exec(const std::string &cmd, bool include_stderr = false,
                       std::string working_dir = "",
                       const std::string &env = "");

#endif  // MYSQLROUTER_TESTS_APP_EXEC_INCLUDED
