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

#include "formatters/console.h"
#include "processor/block_processor.h"
#include "processor/script_stack.h"


class Execution_context;

class Macro {
 public:
  using Strings = std::list<std::string>;

 public:
  Macro(const std::string &name,
        const Strings &argnames,
        const bool accepts_variadic_arguments)
      : m_name(name),
        m_accepts_args(argnames),
        m_accepts_variadic_arguments(accepts_variadic_arguments)
  {}

  const std::string &name() const { return m_name; }

  void set_macro_body(const std::string &body) { m_body = body; }

  std::string get_expanded_macro_body(
      const Strings &args,
      const Script_stack *stack,
      const Console &console) const;

 private:
  std::string m_name;
  Strings     m_accepts_args;
  std::string m_body;
  bool        m_accepts_variadic_arguments;
};

class Macro_container {
 public:
  using Strings = std::list<std::string>;

 public:
  void add_macro(std::shared_ptr<Macro> macro);
  void set_compress_option(const bool compress);

  bool call(Execution_context *context,
            const std::string &cmd);

 private:
  std::string get_expanded_macro(
      Execution_context *context,
      const std::string &cmd,
      std::string *r_name,
      const Script_stack *stack,
      const Console &console);

  std::list<std::shared_ptr<Macro>> m_macros;
  bool m_compress{true};
};

#endif  // X_TESTS_DRIVER_PROCESSOR_COMMANDS_MACRO_H_
