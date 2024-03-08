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

#include "plugin/x/tests/driver/processor/sql_stmt_processor.h"

#include <algorithm>
#include <stack>
#include <vector>

#include "plugin/x/tests/driver/common/utils_mysql_parsing.h"

Block_processor::Result Sql_stmt_processor::feed(std::istream & /*input*/,
                                                 const char *linebuf) {
  if (!m_sql) {
    auto check_characters = [](const char c) {
      if (' ' == c) return true;

      if ('\t' == c) return true;

      return false;
    };

    if (std::all_of(linebuf, linebuf + strlen(linebuf), check_characters)) {
      return Result::Not_hungry;
    }

    m_rawbuffer.clear();
    m_sql = true;
  }

  m_rawbuffer.append(linebuf).append("\n");

  if (m_sql) {
    std::string delimiter = ";";
    std::vector<std::pair<size_t, size_t>> ranges;
    std::stack<std::string> input_context_stack;

    if ('-' == m_rawbuffer[0] && '-' == m_rawbuffer[1]) {
      m_context->m_console.print_error(m_context->m_script_stack,
                                       "Invalid SQL, line begins with '--'.");
      return Result::Indigestion;
    }

    const auto number_of_full_stmts =
        shcore::mysql::splitter::determineStatementRanges(
            m_rawbuffer.data(), m_rawbuffer.length(), delimiter, ranges, "\n",
            input_context_stack);

    if (0 < number_of_full_stmts) {
      bool fault = true;

      if (1 == number_of_full_stmts) {
        fault = 0 != run_sql_batch(m_cm->active_xsession(), m_rawbuffer,
                                   m_context->m_options.m_quiet);
      }

      if (fault) {
        return Result::Indigestion;
      }

      m_sql = false;
      return Result::Eaten_but_not_hungry;
    }
  }

  return Result::Feed_more;
}

bool Sql_stmt_processor::feed_ended_is_state_ok() {
  if (!m_sql) return true;

  m_context->print_error(
      m_context->m_script_stack,
      "Missing delimiter at end of statement (delimiter is ';')\n");

  return false;
}
