#ifndef QUERY_BUILDER_INCLUDED
#define QUERY_BUILDER_INCLUDED
/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "my_config.h"

#include <string>
#include <vector>

#include "plugin/rewriter/rule.h"
#include "plugin/rewriter/services.h"

/**
  @file query_builder.h

*/

/**
  Class that builds the rewritten query by appending literals in the order
  they appear in the parse tree.
*/
class Query_builder : public services::Literal_visitor {
 public:
  Query_builder(const Pattern *pattern, const Replacement *replacement)
      : m_previous_slot(0),
        m_replacement(replacement->query_string),
        m_slots(replacement->slots()),
        m_slots_iter(m_slots.begin()),
        m_pattern_literals(pattern->literals),
        m_pattern_literals_iter(m_pattern_literals.begin()),
        m_matches_so_far(true) {}

  /**
    Implementation of the visit() function that bridges to add_next_literal().

    @param item The current literal.
  */
  bool visit(MYSQL_ITEM item) { return add_next_literal(item); }

  /**
    To be called after visit() has been called for all literals in the parse
    tree that this Query_builder was visiting. This function finishes the
    string to yield a complete query.
  */
  const std::string &get_built_query() {
    // Append trailing segment of replacement.
    m_built_query += m_replacement.substr(m_previous_slot);
    return m_built_query;
  }

  /**
    Status of the matching of literals that are not parameter markers.

    @retval true The parse tree matches the pattern and it is safe to continue
    adding literals.

    @retval false Some literal has been found to differ between parse tree and
    pattern. Execution must end immediately.
  */
  bool matches() const { return m_matches_so_far; }

 private:
  /**
    The index of the character in 'm_replacement' after the last slot that we
    filled.
  */
  int m_previous_slot;

  /// Query we copy from (replacement string.)
  std::string m_replacement;

  /// The slots in the replacement string.
  std::vector<int> m_slots;
  std::vector<int>::iterator m_slots_iter;

  /// All literals in the pattern, in order of appearance in parse tree.
  std::vector<std::string> m_pattern_literals;
  std::vector<std::string>::iterator m_pattern_literals_iter;

  /// The query under construction.
  std::string m_built_query;

  /**
    Whether the literals in the parse tree match those of the pattern so
    far.
  */
  bool m_matches_so_far;

  /**
    Adds a literal, assumed to be the next in the parse tree, from the query's
    parse tree to this Query_builder.

    @param item Assumed to be a literal.

    @retval true The builder is finished. Either it has been detected that the
    current literal does not match the pattern, or no more literals are needed
    to build the query.
  */
  bool add_next_literal(MYSQL_ITEM item);
};

bool Query_builder::add_next_literal(MYSQL_ITEM item) {
  std::string query_literal = services::print_item(item);
  std::string pattern_literal = *m_pattern_literals_iter;

  if (pattern_literal.compare("?") ==
      0) {  // Literal corresponds to a parameter marker in the pattern.

    if (m_slots_iter != m_slots.end())  // There are more slots to fill
    {
      // The part of the replacement leading up to its corresponding slot.
      m_built_query += m_replacement.substr(m_previous_slot,
                                            *m_slots_iter - m_previous_slot);
      m_built_query += query_literal;

      m_previous_slot = *m_slots_iter++ + sizeof('?');
    }
  } else if (pattern_literal.compare(query_literal) != 0) {
    // The literal does not match the pattern nor a parameter marker, we
    // fail to rewrite.
    m_matches_so_far = false;
    return true;
  }
  return ++m_pattern_literals_iter == m_pattern_literals.end();
}

#endif  // QUERY_BUILDER_INCLUDED
