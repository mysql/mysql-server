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

#include "plugin/x/tests/driver/processor/sql_block_processor.h"

#include <iostream>
#include <memory>
#include <stack>
#include <utility>
#include <vector>

#include "my_dbug.h"

#include "plugin/x/tests/driver/common/utils_mysql_parsing.h"
#include "plugin/x/tests/driver/connector/result_fetcher.h"
#include "plugin/x/tests/driver/connector/warning.h"
#include "plugin/x/tests/driver/processor/variable_names.h"

Block_processor::Result Sql_block_processor::feed(std::istream & /*input*/,
                                                  const char *linebuf) {
  if (m_sql) {
    if (strcmp(linebuf, "-->endsql") == 0) {
      {
        int r = run_sql_batch(m_cm->active_xsession(), m_rawbuffer,
                              m_context->m_options.m_quiet);
        if (r != 0) {
          return Result::Indigestion;
        }
      }
      m_sql = false;

      return Result::Eaten_but_not_hungry;
    } else {
      m_rawbuffer.append(linebuf).append("\n");
    }

    return Result::Feed_more;
  }

  // -->command
  if (strcmp(linebuf, "-->sql") == 0) {
    m_rawbuffer.clear();
    m_sql = true;
    // feed everything until -->endraw to the mysql client

    return Result::Feed_more;
  }

  return Result::Not_hungry;
}

bool Sql_block_processor::feed_ended_is_state_ok() {
  if (!m_sql) return true;

  m_context->print_error(m_context->m_script_stack,
                         "Unclosed -->sql directive\n");

  return false;
}

int Sql_block_processor::run_sql_batch(xcl::XSession *conn,
                                       const std::string &sql_batch,
                                       const bool be_quiet) {
  DBUG_TRACE;
  std::string delimiter = ";";
  std::vector<std::pair<size_t, size_t>> ranges;
  std::stack<std::string> input_context_stack;
  std::string sql = sql_batch;

  m_context->m_variables->set(k_variable_result_rows_affected, "0");
  m_context->m_variables->set(k_variable_result_last_insert_id, "0");

  m_context->m_variables->replace(&sql);

  shcore::mysql::splitter::determineStatementRanges(
      sql.data(), sql.length(), delimiter, ranges, "\n", input_context_stack);

  xcl::XError error;
  std::vector<Warning> warnings;

  std::unique_ptr<Result_fetcher> result;
  for (const auto &st : ranges) {
    try {
      if (!be_quiet) {
        auto sql_to_display = m_context->m_variables->unreplace(
            sql.substr(st.first, st.second), false);
        m_context->print("RUN ", sql_to_display, "\n");
      }

      result.reset(new Result_fetcher(
          conn->execute_sql(sql.substr(st.first, st.second), &error)));

      if (error) {
        throw error;
      }

      do {
        if (result->is_out_params()) {
          m_context->print("Output parameters:\n");
        }
        std::stringstream s;
        s << (result.get());
        if (m_context->m_options.m_show_query_result) {
          const auto value = m_context->m_variables->unreplace(s.str(), false);

          if (!value.empty()) m_context->print(value);
        }
      } while (result->next_data_set());

      error = result->get_last_error();

      if (error) {
        throw error;
      }

      const auto affected_rows = result->affected_rows();
      const auto insert_id = result->last_insert_id();

      m_context->m_variables->set(k_variable_result_rows_affected,
                                  std::to_string(affected_rows));
      m_context->m_variables->set(k_variable_result_last_insert_id,
                                  std::to_string(insert_id));

      if (m_context->m_options.m_show_query_result) {
        if (affected_rows >= 0)
          m_context->print(affected_rows, " rows affected\n");

        if (insert_id > 0)
          m_context->print("last insert id: ", insert_id, "\n");

        if (!result->info_message().empty())
          m_context->print(result->info_message(), "\n");
      }

      handle_warnings(result.get(), &warnings);
    } catch (xcl::XError &err) {
      handle_warnings(result.get(), &warnings);
      error = err;
      m_context->m_variables->clear_unreplace();
      m_context->print_error("While executing ",
                             sql.substr(st.first, st.second), ":\n");

      if (!m_context->m_expected_error.check_error(err)) {
        // We do not need to check the result of check_warnings,
        // the execution already failed.
        // The propose of line below is to make the user aware
        // of other issues by printing info about unexpected
        // warnings.
        m_context->m_expected_warnings.check_warnings(warnings);
        return 1;
      }
    }
  }

  const bool is_success = !error;

  if (!m_context->m_expected_warnings.check_warnings(warnings)) return 1;

  if (is_success) {
    if (!m_context->m_expected_error.check_ok()) return 1;
  }

  m_context->m_variables->clear_unreplace();

  return 0;
}

void Sql_block_processor::handle_warnings(
    Result_fetcher *fetcher, std::vector<Warning> *out_warnings_aggregation) {
  if (nullptr == fetcher) return;

  auto current_warnings = fetcher->get_warnings();

  for (const auto &w : current_warnings) {
    out_warnings_aggregation->push_back(w);
  }

  if (m_context->m_options.m_show_warnings) {
    if (!current_warnings.empty()) m_context->print("Warnings generated:\n");

    for (const auto &w : current_warnings) {
      m_context->print(w, "\n");
    }
  }
}
