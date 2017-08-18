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

#include "macro.h"

#include <algorithm>
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "common/utils_string_parsing.h"
#include "processor/execution_context.h"
#include "processor/stream_processor.h"


std::string Macro::get_expanded_macro_body(
    const Strings &args,
    const Script_stack *stack,
    const Console &console) const {
  if (!m_accepts_variadic_arguments) {
    if (args.size() != m_accepts_args.size()) {

      if (m_accepts_args.empty() &&
          1 == args.size() &&
          args.front().empty())
        return get_expanded_macro_body({}, stack, console);

      console.print_error(*stack, "Invalid number of arguments for macro ",
                          m_name, ", expected:", m_accepts_args.size(),
                          " actual:", args.size(), '\n');

      for (const auto &v : args) {
        console.print_error("  argument: \"", v, "\"\n");
      }

      return "";
    }
  } else {
    if (args.size() < m_accepts_args.size()) {
      console.print_error(*stack, "Invalid number of arguments for macro ",
                          m_name, ", expected at last:", m_accepts_args.size(),
                          " actual:", args.size(), '\n');

      for (const auto &v : args) {
        console.print_error("  argument: \"", v, "\"\n");
      }

      return "";
    }
  }

  std::string text = m_body;
  auto n = m_accepts_args.begin();
  auto v = args.begin();
  size_t index_of_argument = 0;

  for (; index_of_argument < m_accepts_args.size(); index_of_argument++) {
    aux::replace_all(text, *(n++), *(v++));
  }

  if (m_accepts_variadic_arguments) {
    std::string variadic_arguments;

    for (; index_of_argument < args.size() - 1; index_of_argument++) {
      variadic_arguments += *(v++) + '\t';
    }

    if (index_of_argument < args.size())
      variadic_arguments += *(v++);

    aux::replace_all(text, "%VAR_ARGS%", variadic_arguments);
  }

  return text;
}

void Macro_container::add_macro(std::shared_ptr<Macro> macro) {
    m_macros.push_back(macro);
}

void Macro_container::set_compress_option(
    const bool compress) {
  m_compress = compress;
}

std::string Macro_container::get_expanded_macro(
    Execution_context *context,
    const std::string &cmd,
    std::string *r_name,
    const Script_stack *stack,
    const Console &console) {
  Strings args;
  std::string::size_type p = std::min(cmd.find(' '), cmd.find('\t'));

  if (p == std::string::npos) {
    *r_name = cmd;
  } else {
    *r_name = cmd.substr(0, p);
    std::string rest = cmd.substr(p + 1);
    aux::split(args, rest, "\t", m_compress);
  }

  if (r_name->empty()) {
    console.print_error(*stack, "Missing macro name for macro call\n");
    return "";
  }

  context->m_variables->replace(r_name);

  for (auto iter = m_macros.begin();
       iter != m_macros.end();
       ++iter) {
    if ((*iter)->name() == *r_name) {
      return (*iter)->get_expanded_macro_body(args, stack, console);
    }
  }

  console.print_error(*stack, "Undefined macro ", *r_name, '\n');

  return "";
}

bool Macro_container::call(Execution_context *context,
                           const std::string &cmd) {
  std::string name;
  std::string macro = get_expanded_macro(
                          context,
                          cmd,
                          &name,
                          &context->m_script_stack,
                          context->m_console);

  context->m_script_stack.push({0, "macro " + name});

  std::stringstream                stream(macro);
  std::vector<Block_processor_ptr> processors{
      create_macro_block_processors(context)
  };

  const bool r =  0 == process_client_input(
      stream,
      &processors,
      &context->m_script_stack,
      context->m_console);

  context->m_script_stack.pop();

  return r;
}
