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

#include "plugin/x/tests/driver/processor/macro_block_processor.h"

#include <list>

#include "plugin/x/tests/driver/common/utils_string_parsing.h"

Block_processor::Result Macro_block_processor::feed(std::istream & /*input*/,
                                                    const char *linebuf) {
  if (m_macro) {
    if (strcmp(linebuf, "-->endmacro") == 0) {
      m_macro->set_macro_body(m_rawbuffer);

      m_context->m_macros.add_macro(m_macro);
      m_context->print_verbose("Macro ", m_macro->name(), " defined\n");

      m_macro.reset();

      return Result::Eaten_but_not_hungry;
    } else {
      m_rawbuffer.append(linebuf).append("\n");
    }

    return Result::Feed_more;
  }

  // -->command
  const char cmd_macro[] = "-->macro ";
  const char cmd_macrov[] = "-->macro_varg ";

  const bool is_macrov = 0 == strncmp(linebuf, cmd_macrov, strlen(cmd_macrov));
  const bool is_macro = 0 == strncmp(linebuf, cmd_macro, strlen(cmd_macro));

  if (is_macrov || is_macro) {
    std::list<std::string> args;
    std::string t(strstr(linebuf, " ") + 1);

    aux::split(args, t, " \t", true);

    if (args.empty()) {
      m_context->print_error(m_context->m_script_stack,
                             "Missing macro name argument for -->macro\n");
      return Result::Indigestion;
    }

    m_rawbuffer.clear();
    std::string name = args.front();
    args.pop_front();
    m_macro.reset(new Macro(name, args, is_macrov));

    return Result::Feed_more;
  }

  return Result::Not_hungry;
}

bool Macro_block_processor::feed_ended_is_state_ok() {
  if (m_macro) {
    m_context->print_error(m_context->m_script_stack,
                           "Unclosed -->macro directive\n");
    return false;
  }

  return true;
}
