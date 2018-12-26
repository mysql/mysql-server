/*  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2.0,
    as published by the Free Software Foundation.

    This program is also distributed with certain software (including
    but not limited to OpenSSL) that is licensed under separate terms,
    as designated in a particular file or component or in included license
    documentation.  The authors of MySQL hereby grant you an additional
    permission to link the program and your derivative works with the
    separately licensed software that they have included with MySQL.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License, version 2.0, for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/rewriter/rule.h"

#include "my_config.h"

#include <stddef.h>
#include <string>
#include <vector>

#include "my_dbug.h"
#include "mysqld_error.h"
#include "plugin/rewriter/query_builder.h"
#include "plugin/rewriter/services.h"

using std::string;
using std::vector;

/**
  @file plugin/rewriter/rule.cc
  Implementation of rewrite rule execution.
  Details on parameter extraction:
  It is important to understand that in the case of a rewrite the tree of the
  original query and of the pattern have been found to be similar (by
  comparing the normalized strings,) except that instead of parameter markers
  there may be actual literals. Therefore, the parameters to extract are at the
  exact same positions as the parameter markers in the pattern. Example with
  the where clause of a query:

@verbatim
          Pattern                 Original Query

            AND                       AND
          /     \                   /     \
        EQ      NEQ               EQ      NEQ
      /   \   /     \           /   \    /    \
     A     ? C       ?         A     3  C      5

@endverbatim

  When loading a rule, we will traverse the tree and keep each literal we
  encounter. We later reuse these literals to do the third phase of matching:
  either a literal in the query matches a paramater marker in the pattern, or
  an identical literal.
*/

/// A Condition_handler that silences and records parse errors.
class Parse_error_recorder : public services::Condition_handler {
 public:
  /**
    Handle a condition.
    @param sql_errno The sql error number.
    @param message The sql error text.

    @retval true If the error number is a parser error, we claim we handle the
    error.

    @retval false We don't handle the error.
  */
  bool handle(int sql_errno, const char *, const char *message) {
    DBUG_ASSERT(message != NULL);
    if (m_message.empty()) m_message.assign(message);
    switch (sql_errno) {
      case ER_PARSE_ERROR:
      case ER_EMPTY_QUERY:
      case ER_WARN_LEGACY_SYNTAX_CONVERTED:
      case ER_NO_DB_ERROR:
        return true;
      default:
        return false;
    };
  }

  string first_parse_error_message() { return m_message; }

 private:
  string m_message;
};

/// Class that collects literals from a parse tree in an std::vector.
class Literal_collector : public services::Literal_visitor {
  vector<string> m_literals;

 public:
  bool visit(MYSQL_ITEM item) {
    m_literals.push_back(services::print_item(item));
    return false;
  }

  vector<string> get_literals() { return m_literals; }
};

Pattern::Load_status Pattern::load(MYSQL_THD thd,
                                   const Persisted_rule *diskrule) {
  Parse_error_recorder recorder;

  if (diskrule->pattern_db.has_value())
    services::set_current_database(thd, diskrule->pattern_db.value());
  else
    services::set_current_database(thd, "");

  if (services::parse(thd, diskrule->pattern.value(), true, &recorder)) {
    m_parse_error_message = recorder.first_parse_error_message();
    return PARSE_ERROR;
  }

  if (!services::is_supported_statement(thd)) return NOT_SUPPORTED_STATEMENT;

  // We copy the normalized_pattern to the plugin's memory.
  normalized_pattern = services::get_current_query_normalized(thd);
  number_parameters = services::get_number_params(thd);

  Literal_collector collector;
  services::visit_parse_tree(thd, &collector);
  literals = collector.get_literals();

  if (digest.load(thd)) return NO_DIGEST;

  return OK;
}

/**
  Load the replacement query string.
  It means:
    - extract the number of parameters
    - extract the position of the parameters in the query string
    - copy the replacement in the rewrite rule
*/
bool Replacement::load(MYSQL_THD thd, const string replacement) {
  Parse_error_recorder recorder;
  if (services::parse(thd, replacement, true, &recorder)) {
    m_parse_error_message = recorder.first_parse_error_message();
    return true;
  }

  number_parameters = services::get_number_params(thd);
  if (number_parameters > 0)
    m_param_slots = services::get_parameter_positions(thd);

  query_string = replacement;

  return false;
}

Rewrite_result Rule::create_new_query(MYSQL_THD thd) {
  Query_builder builder(&m_pattern, &m_replacement);

  services::visit_parse_tree(thd, &builder);

  Rewrite_result result;
  if (builder.matches()) {
    result.new_query = builder.get_built_query();
    result.was_rewritten = true;
  } else
    result.was_rewritten = false;

  return result;
}

bool Rule::matches(MYSQL_THD thd) const {
  string normalized_query = services::get_current_query_normalized(thd);
  return normalized_query.compare(m_pattern.normalized_pattern) == 0;
}
