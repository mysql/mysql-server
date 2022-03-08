/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTING_SQL_LEXER_INCLUDED
#define ROUTING_SQL_LEXER_INCLUDED

#include <string_view>

#include "sql/lexer_yystype.h"

class THD;

class SqlLexer {
 public:
  using TokenId = int;

  SqlLexer(THD *session);

  class iterator {
   public:
    struct Token {
      std::string_view text;
      TokenId id;
    };

    using lexer_state = Lexer_yystype;
    using value_type = Token;
    using pointer = value_type *;
    using const_pointer = const value_type *;

    iterator(THD *session);

    iterator(THD *session, Token token)
        : session_(session), token_{std::move(token)} {}

    value_type operator*() const { return token_; }
    pointer operator->() { return &token_; }
    const_pointer operator->() const { return &token_; }

    iterator operator++(int);
    iterator &operator++();

    friend bool operator==(const iterator &a, const iterator &b);
    friend bool operator!=(const iterator &a, const iterator &b);

   private:
    Token next_token();
    std::string_view get_token_text(TokenId token_id) const;

    THD *session_;
    lexer_state st;

    Token token_;
  };

  iterator begin() { return iterator(session_); }
  iterator end() { return iterator(nullptr); }

 private:
  THD *session_;
};

#endif
