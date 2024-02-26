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

#ifndef PLUGIN_X_TESTS_DRIVER_PROCESSOR_EXECUTION_CONTEXT_H_
#define PLUGIN_X_TESTS_DRIVER_PROCESSOR_EXECUTION_CONTEXT_H_

#include <istream>
#include <map>
#include <string>
#include <utility>

#include "my_io.h"
#include "plugin/x/tests/driver/connector/connection_manager.h"
#include "plugin/x/tests/driver/formatters/console.h"
#include "plugin/x/tests/driver/processor/commands/expected_error.h"
#include "plugin/x/tests/driver/processor/commands/expected_warnings.h"
#include "plugin/x/tests/driver/processor/commands/macro.h"
#include "plugin/x/tests/driver/processor/script_stack.h"
#include "plugin/x/tests/driver/processor/variable_container.h"

class Execution_context {
 public:
  struct Options {
    bool m_quiet{false};
    bool m_bindump{false};
    bool m_show_warnings{false};
    bool m_fatal_errors{true};
    bool m_fatal_warnings{false};
    bool m_show_query_result{true};
    std::string m_import_path{FN_CURLIB, FN_LIBCHAR, '\0'};
  };

 public:
  Execution_context(const Options &options, Connection_manager *cm,
                    Variable_container *variables, const Console &console)
      : m_options(options),
        m_connection(cm),
        m_variables(variables),
        m_expected_error(m_options.m_fatal_errors, console, &m_script_stack),
        m_expected_warnings(m_options.m_fatal_errors,
                            m_options.m_fatal_warnings, console,
                            &m_script_stack),
        m_console(console) {}

  void set_options(const Options &options) { m_options = options; }

  Options m_options;
  std::string m_command_name;
  std::string m_command_arguments;
  Connection_manager *m_connection;
  Script_stack m_script_stack;
  Variable_container *m_variables;
  Expected_error m_expected_error;
  Expected_warnings m_expected_warnings;
  const Console &m_console;
  Macro_container m_macros;
  std::map<std::string, std::vector<xcl::Column_metadata>> m_stored_metadata;

  xcl::XSession *session() { return m_connection->active_xsession(); }

  template <typename... T>
  void print(T &&... values) {
    m_console.print(std::forward<T>(values)...);
  }

  template <typename... T>
  void print_verbose(T &&... values) {
    m_console.print_verbose(std::forward<T>(values)...);
  }

  template <typename... T>
  void print_error(T &&... values) const {
    m_console.print_error(std::forward<T>(values)...);
  }

  template <typename... T>
  void print_error_red(T &&... values) const {
    m_console.print_error_red(std::forward<T>(values)...);
  }
};

#endif  // PLUGIN_X_TESTS_DRIVER_PROCESSOR_EXECUTION_CONTEXT_H_
