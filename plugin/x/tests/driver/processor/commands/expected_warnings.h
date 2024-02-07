/*
 * Copyright (c) 2018, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#ifndef PLUGIN_X_TESTS_DRIVER_PROCESSOR_COMMANDS_EXPECTED_WARNINGS_H_
#define PLUGIN_X_TESTS_DRIVER_PROCESSOR_COMMANDS_EXPECTED_WARNINGS_H_

#include <set>
#include <vector>

#include "plugin/x/client/mysqlxclient/xerror.h"
#include "plugin/x/tests/driver/connector/result_fetcher.h"
#include "plugin/x/tests/driver/formatters/console.h"
#include "plugin/x/tests/driver/processor/script_stack.h"

class Expected_warnings {
 public:
  using Warnings = std::vector<Warning>;

 public:
  Expected_warnings(const bool &fatal_errors, const bool &fatal_warnings,
                    const Console &console, Script_stack *stack)
      : m_fatal_errors(fatal_errors),
        m_fatal_warnings(fatal_warnings),
        m_console(console),
        m_stack(stack) {}

  void expect_warning(const int wno) { m_expect_wno.insert(wno); }
  bool check_warnings(const Warnings &warnings);

 private:
  bool check(const Warning &err);
  void clear_expectations();

  void print_unexpected_warning(const Warning &warning);
  void print_expected_warnings();

  std::set<int> m_expect_wno;
  const bool &m_fatal_errors;
  const bool &m_fatal_warnings;
  const Console &m_console;
  Script_stack *m_stack;
};

#endif  // PLUGIN_X_TESTS_DRIVER_PROCESSOR_COMMANDS_EXPECTED_WARNINGS_H_
