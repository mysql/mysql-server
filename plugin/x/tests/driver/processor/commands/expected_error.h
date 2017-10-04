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

#ifndef X_TESTS_DRIVER_PROCESSOR_COMMANDS_EXPECTED_ERROR_H_
#define X_TESTS_DRIVER_PROCESSOR_COMMANDS_EXPECTED_ERROR_H_

#include <set>

#include "plugin/x/client/mysqlxclient/xerror.h"
#include "plugin/x/tests/driver/formatters/console.h"
#include "plugin/x/tests/driver/processor/script_stack.h"


class Expected_error {
 public:
  Expected_error(const bool &fatal_errors,
                 const Console &console,
                 Script_stack *stack)
      : m_fatal_errors(fatal_errors), m_console(console), m_stack(stack) {}

  void expect_errno(int err) { m_expect_errno.insert(err); }
  bool check_error(const xcl::XError &err);
  bool check_ok();

 private:
  bool check(const xcl::XError &err);
  void print_unexpected_error(const xcl::XError &err);
  void print_expected_error(const xcl::XError &err);

  std::set<int>  m_expect_errno;
  const bool    &m_fatal_errors;
  const Console &m_console;
  Script_stack  *m_stack;
};

#endif  // X_TESTS_DRIVER_PROCESSOR_COMMANDS_EXPECTED_ERROR_H_
