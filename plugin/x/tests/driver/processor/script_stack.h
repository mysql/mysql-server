/*
 * Copyright (c) 2017, 2024, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_TESTS_DRIVER_PROCESSOR_SCRIPT_STACK_H_
#define PLUGIN_X_TESTS_DRIVER_PROCESSOR_SCRIPT_STACK_H_

#include <stdio.h>
#include <list>
#include <ostream>
#include <string>

class Script_stack {
 public:
  struct Frame {
    int m_line_number;
    std::string m_context;
  };
  using Stack = std::list<Frame>;
  using const_reverse_iterator = Stack::const_reverse_iterator;

 public:
  const_reverse_iterator rbegin() const { return m_stack.rbegin(); }
  const_reverse_iterator rend() const { return m_stack.rend(); }
  Stack::reference front() { return m_stack.front(); }
  void push(const Frame &frame) { return m_stack.push_front(frame); }
  void pop() { return m_stack.pop_front(); }

 private:
  Stack m_stack;
};

inline std::ostream &operator<<(std::ostream &os, const Script_stack &stack) {
  std::string context;

  for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "in %s, line %i:", it->m_context.c_str(),
             it->m_line_number);
    context.append(tmp);
  }
  return os << context << "ERROR: ";
}

#endif  // PLUGIN_X_TESTS_DRIVER_PROCESSOR_SCRIPT_STACK_H_
