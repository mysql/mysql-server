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

#include "start_transaction_parser.h"

#include <charconv>
#include <iomanip>
#include <sstream>

#include "harness_assert.h"
#include "mysql/harness/stdx/expected.h"
#include "sql/sql_yacc.h"

static std::string to_string(SqlParser::TokenText tkn) {
  const auto id = tkn.id();

  if (id >= 32 && id < 127) return {static_cast<char>(id)};

  if (id == END_OF_INPUT) return "<END>";

  for (size_t ndx{}; ndx < sizeof(symbols) / sizeof(symbols[0]); ++ndx) {
    auto sym = symbols[ndx];

    // SYMBOL uses 'unsigned int', the lexer uses 'int' for the same defines.
    if (static_cast<SqlLexer::TokenId>(sym.tok) == id) {
      return {sym.name, sym.length};
    }
  }

  if (id == IDENT || id == IDENT_QUOTED) {
    std::ostringstream oss;

    oss << std::quoted(tkn.text(), '`');

    return oss.str();
  }

  if (id == TEXT_STRING) {
    std::ostringstream oss;

    oss << std::quoted(tkn.text());

    return oss.str();
  }

  return std::string(tkn.text());
}

stdx::expected<std::variant<std::monostate, StartTransaction>, std::string>
StartTransactionParser::parse() {
  using ret_type =
      stdx::expected<std::variant<std::monostate, StartTransaction>,
                     std::string>;

  if (accept(START_SYM)) {
    if (accept(TRANSACTION_SYM)) {
      std::optional<StartTransaction::AccessMode> access_mode;
      bool with_consistent_snapshot{false};

      // [ trx_characteristics [, trx_characteristics ]*]*
      do {
        auto trx_characteristics_res = transaction_characteristics();
        if (!trx_characteristics_res) {
          return stdx::unexpected("You have an error in your SQL syntax; " +
                                  trx_characteristics_res.error());
        }

        auto trx_characteristics = *trx_characteristics_res;

        if (std::holds_alternative<std::monostate>(trx_characteristics)) {
          // no match.
          break;
        }

        if (std::holds_alternative<bool>(trx_characteristics)) {
          with_consistent_snapshot = true;
        }

        if (std::holds_alternative<StartTransaction::AccessMode>(
                trx_characteristics)) {
          if (access_mode) {
            return stdx::unexpected(
                "You have an error in your SQL syntax; START TRANSACTION only "
                "allows one access mode");
          }

          access_mode =
              std::get<StartTransaction::AccessMode>(trx_characteristics);
        }

        if (!accept(',')) break;
      } while (true);

      if (accept(END_OF_INPUT)) {
        return ret_type{
            std::in_place,
            StartTransaction{access_mode, with_consistent_snapshot}};
      }

      return stdx::unexpected(
          "You have an error in your SQL syntax; unexpected input near " +
          to_string(token()));
    }

    // some other START
    return {};
  }

  if (accept(BEGIN_SYM)) {
    if (accept(WORK_SYM)) {
      if (accept(END_OF_INPUT)) {
        return ret_type{std::in_place, StartTransaction{}};
      }

      return stdx::unexpected(
          "You have an error in your SQL syntax; after BEGIN WORK no further "
          "input is expected. Unexpected input near " +
          to_string(token()));
    }

    if (accept(END_OF_INPUT)) {
      return ret_type{std::in_place, StartTransaction{}};
    }
    return stdx::unexpected(
        "You have an error in your SQL syntax; after BEGIN only [WORK] is "
        "expected. Unexpected input near " +
        to_string(token()));
  }

  // not matched.
  return {};
}

stdx::expected<std::variant<std::monostate, StartTransaction::AccessMode, bool>,
               std::string>
StartTransactionParser::transaction_characteristics() {
  if (accept(WITH)) {
    if (accept(CONSISTENT_SYM)) {
      if (accept(SNAPSHOT_SYM)) {
        return true;
      }
      return stdx::unexpected(
          "after WITH CONSISTENT only SNAPSHOT is allowed.");
    }
    return stdx::unexpected("after WITH only CONSISTENT is allowed.");
  }

  if (accept(READ_SYM)) {
    if (accept(ONLY_SYM)) {
      return StartTransaction::AccessMode::ReadOnly;
    }
    if (accept(WRITE_SYM)) {
      return StartTransaction::AccessMode::ReadWrite;
    }
    return stdx::unexpected("after READ only ONLY|WRITE are allowed.");
  }

  return {};
}
