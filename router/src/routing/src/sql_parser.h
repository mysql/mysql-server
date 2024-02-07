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

#ifndef ROUTING_SQL_PARSER_INCLUDED
#define ROUTING_SQL_PARSER_INCLUDED

#include <string>
#include <string_view>

#include "sql/lex.h"
#include "sql_lexer.h"
#include "sql_lexer_thd.h"

class SqlParser {
 public:
  SqlParser(SqlLexer::iterator first, SqlLexer::iterator last)
      : cur_{first}, end_{last} {}

  class TokenText {
   public:
    TokenText() = default;
    TokenText(SqlLexer::TokenId id, std::string_view txt)
        : id_{id}, txt_{txt} {}

    operator bool() const { return !txt_.empty(); }

    [[nodiscard]] std::string_view text() const { return txt_; }
    [[nodiscard]] SqlLexer::TokenId id() const { return id_; }

   private:
    SqlLexer::TokenId id_{};
    std::string_view txt_{};
  };

  TokenText token() const { return {cur_->id, cur_->text}; }

 protected:
  TokenText ident() {
    if (auto ident_tkn = accept(IDENT)) {
      return ident_tkn;
    } else if (auto ident_tkn = accept(IDENT_QUOTED)) {
      return ident_tkn;
    } else {
      return {};
    }
  }

  TokenText accept_if_not(int sym) {
    if (has_error()) return {};

    if (cur_->id != sym) {
      auto id = cur_->id;
      auto txt = cur_->text;
      ++cur_;
      return {id, txt};
    }

    return {};
  }

  TokenText accept(int sym) {
    if (has_error()) return {};

    if (cur_->id == sym) {
      auto id = cur_->id;
      auto txt = cur_->text;
      ++cur_;
      return {id, txt};
    }

    return {};
  }

  TokenText expect(int sym) {
    if (has_error()) return {};

    if (auto txt = accept(sym)) {
      return txt;
    }

    error_ = "expected sym, got ...";

    return {};
  }

  bool has_error() const { return !error_.empty(); }

  SqlLexer::iterator cur_;
  SqlLexer::iterator end_;

  std::string error_{};
};

#endif
