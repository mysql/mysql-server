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

#ifndef PLUGIN_X_TESTS_DRIVER_DRIVER_COMMAND_LINE_OPTIONS_H_
#define PLUGIN_X_TESTS_DRIVER_DRIVER_COMMAND_LINE_OPTIONS_H_

#include <map>
#include <string>
#include <vector>

#include "plugin/x/generated/mysqlx_version.h"
#include "plugin/x/tests/driver/common/command_line_options.h"
#include "plugin/x/tests/driver/formatters/console.h"
#include "plugin/x/tests/driver/processor/execution_context.h"
#include "print_version.h"
#include "welcome_copyright_notice.h"

class Driver_command_line_options : public Command_line_options {
 public:
  Driver_command_line_options(const int argc, char **argv);

  void print_version();
  void print_help();

 public:
  bool m_run_without_auth;
  std::string m_run_file;
  bool m_has_file;
  bool m_cap_expired_password;
  bool m_client_interactive;
  bool m_connect_attrs;

  Execution_context::Options m_context_options;
  Console::Options m_console_options;
  Connection_options m_connection_options;
  std::map<std::string, std::string> m_variables;

  std::string m_uri;
  bool m_daemon;
  std::string m_sql;
  int m_expected_error_code{0};

  bool is_expected_error_set() const { return 0 != m_expected_error_code; }

 private:
  void set_variable_option(const std::string &set_expression);
  std::string get_socket_name();
  xcl::Internet_protocol set_protocol(const int mode);
};

#endif  // PLUGIN_X_TESTS_DRIVER_DRIVER_COMMAND_LINE_OPTIONS_H_
