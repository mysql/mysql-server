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

#ifndef X_TESTS_DRIVER_PROCESSOR_COMMANDS_MACRO_H_
#define X_TESTS_DRIVER_PROCESSOR_COMMANDS_MACRO_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "console.h"
#include "processor/block_processor.h"
#include "processor/script_stack.h"


class Execution_context;

class Macro {
 public:
  using Strings = std::list<std::string>;

 public:
  Macro(const std::string &name,
        const Strings &argnames)
      : m_name(name), m_args(argnames) {}

  const std::string &name() const { return m_name; }

  void set_body(const std::string &body) { m_body = body; }

  std::string get(const Strings &args,
                  const Script_stack *stack,
                  const Console &console) const;

 private:
  std::string m_name;
  Strings     m_args;
  std::string m_body;
};

class Macro_container {
 public:
  using Strings = std::list<std::string>;

 public:
  void add(std::shared_ptr<Macro> macro) { m_macros.push_back(macro); }

  std::string get(const std::string &cmd,
                  std::string *r_name,
                  const Script_stack *stack,
                  const Console &console);
  bool call(Execution_context *context, const std::string &cmd);

 private:
  std::list<std::shared_ptr<Macro>> m_macros;
};

#endif  // X_TESTS_DRIVER_PROCESSOR_COMMANDS_MACRO_H_
