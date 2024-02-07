/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTING_SQL_LEXER_PARSER_STATE_INCLUDED
#define ROUTING_SQL_LEXER_PARSER_STATE_INCLUDED

#include <cstdlib>

#include "sql_lexer_input_stream.h"
#include "sql_lexer_parser_input.h"
#include "sql_lexer_yacc_state.h"

class THD;

/**
  Internal state of the parser.
  The complete state consist of:
  - input parameters that control the parser behavior
  - state data used during lexical parsing,
  - state data used during syntactic parsing.
*/
class Parser_state {
 protected:
  /**
    Constructor for special parsers of partial SQL clauses (DD)

    @param grammar_selector_token   See Lex_input_stream::grammar_selector_token
  */
  explicit Parser_state(int grammar_selector_token)
      : m_input(), m_lip(grammar_selector_token), m_yacc(), m_comment(false) {}

 public:
  Parser_state() : m_input(), m_lip(~0U), m_yacc(), m_comment(false) {}

  /**
     Object initializer. Must be called before usage.

     @retval false OK
     @retval true  Error
  */
  bool init(THD *thd, const char *buff, size_t length) {
    return m_lip.init(thd, buff, length);
  }

  void reset(const char *found_semicolon, size_t length) {
    m_lip.reset(found_semicolon, length);
    m_yacc.reset();
  }

  /// Signal that the current query has a comment
  void add_comment() { m_comment = true; }
  /// Check whether the current query has a comment
  bool has_comment() const { return m_comment; }

 public:
  Parser_input m_input;
  Lex_input_stream m_lip;
  Yacc_state m_yacc;
#if 0
  /**
    Current performance digest instrumentation.
  */
  PSI_digest_locker *m_digest_psi;
#endif
 private:
  bool m_comment;  ///< True if current query contains comments
};

#endif
