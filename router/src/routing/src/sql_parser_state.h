/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTING_SQL_PARSER_STATE_INCLUDED
#define ROUTING_SQL_PARSER_STATE_INCLUDED

#include <string_view>

#include "my_alloc.h"  // MEM_ROOT
#include "sql_lexer.h"
#include "sql_lexer_thd.h"  // THD

class SqlParserState {
 public:
  SqlParserState() {
    session_.mem_root = &mem_root_;
    session_.m_parser_state = &parser_state_;
  }

  void statement(std::string_view stmt) {
    // copy the string as the lexer needs a trailing \0
    stmt_ = stmt;

    parser_state_.init(&session_, stmt_.data(), stmt_.size());
  }

  SqlLexer lexer(bool reset_state = true) {
    if (reset_state) parser_state_.reset(stmt_.data(), stmt_.size());

    return {&session_};
  }

  THD *thd() { return &session_; }

  Parser_state *parser_state() { return &parser_state_; }

 private:
  MEM_ROOT mem_root_;
  THD session_;
  Parser_state parser_state_;

  std::string stmt_;
};

#endif
