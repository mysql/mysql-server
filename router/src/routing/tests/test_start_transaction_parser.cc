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
#include "start_transaction_parser.h"

#include <gtest/gtest.h>

#include <optional>
#include <variant>

#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/expected_ostream.h"
#include "mysql/harness/tls_context.h"
#include "sql_parser_state.h"

static stdx::expected<std::variant<std::monostate, StartTransaction>,
                      std::string>
start_transaction(SqlLexer &&lexer) {
  return StartTransactionParser(lexer.begin(), lexer.end()).parse();
}

struct StartTransactionParam {
  std::string_view stmt;

  stdx::expected<std::variant<std::monostate, StartTransaction>, std::string>
      expected_result;
};

bool operator==(const StartTransaction &lhs, const StartTransaction &rhs) {
  return lhs.access_mode() == rhs.access_mode() &&
         lhs.with_consistent_snapshot() == rhs.with_consistent_snapshot();
}

std::ostream &operator<<(std::ostream &os, const StartTransaction &val) {
  os << "START TRANSACTION";

  if (val.with_consistent_snapshot()) {
    os << " WITH CONSISTENT SNAPSHOT";

    if (auto access_mode = val.access_mode()) {
      switch (*access_mode) {
        case StartTransaction::AccessMode::ReadOnly:
          os << ", READ ONLY";
          break;
        case StartTransaction::AccessMode::ReadWrite:
          os << ", READ WRITE";
          break;
      }
    }
  } else {
    if (auto access_mode = val.access_mode()) {
      switch (*access_mode) {
        case StartTransaction::AccessMode::ReadOnly:
          os << "READ ONLY";
          break;
        case StartTransaction::AccessMode::ReadWrite:
          os << "READ WRITE";
          break;
      }
    }
  }

  return os;
}

std::ostream &operator<<(
    std::ostream &os,
    const std::variant<std::monostate, StartTransaction> &val) {
  if (std::holds_alternative<std::monostate>(val)) {
    os << "<no match>";
  } else if (std::holds_alternative<StartTransaction>(val)) {
    os << std::get<StartTransaction>(val);
  }
  return os;
}

class StartTransactionTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<StartTransactionParam> {};

TEST_P(StartTransactionTest, works) {
  SqlParserState sql_parser_state;

  // check the charset's are properly initialized
  ASSERT_NE(sql_parser_state.thd()->charset(), nullptr);
  // 8 is laitn1
  ASSERT_EQ(sql_parser_state.thd()->charset()->number, 8);

  sql_parser_state.statement(GetParam().stmt);

  ASSERT_EQ(start_transaction(sql_parser_state.lexer()),
            GetParam().expected_result)
      << GetParam().stmt << "\n"
      << GetParam().expected_result;
}

const StartTransactionParam test_stmts[] = {
    {"begin", StartTransaction()},
    {"begin work", StartTransaction()},

    {"start transaction", StartTransaction()},  //
    {"start transaction with consistent snapshot",
     StartTransaction(std::nullopt, true)},  //
    {"start transaction with consistent snapshot, with consistent snapshot",
     StartTransaction(std::nullopt, true)},  // duplicated snapshot is ok
    {"start transaction with consistent snapshot, read only",
     StartTransaction(StartTransaction::AccessMode::ReadOnly, true)},  //
    {"start transaction read only, with consistent snapshot",
     StartTransaction(StartTransaction::AccessMode::ReadOnly, true)},  //
    {"start transaction read write",
     StartTransaction(StartTransaction::AccessMode::ReadWrite, false)},  //

    {"begin ,",
     stdx::unexpected("You have an error in your SQL syntax; after BEGIN only "
                      "[WORK] is expected. Unexpected input near ,")},
    {"begin work ,",
     stdx::unexpected("You have an error in your SQL syntax; after BEGIN WORK "
                      "no further input is expected. Unexpected input near ,")},
    {"start transaction read foo",
     stdx::unexpected("You have an error in your SQL syntax; after READ only "
                      "ONLY|WRITE are allowed.")},  //
    {"start transaction ,",
     stdx::unexpected(
         "You have an error in your SQL syntax; unexpected input near ,")},  //
                                                                             //
    {"start transaction read write, read write",
     stdx::unexpected("You have an error in your SQL syntax; START TRANSACTION "
                      "only allows one access mode")},  //
};

INSTANTIATE_TEST_SUITE_P(Ddl, StartTransactionTest,
                         ::testing::ValuesIn(test_stmts));

int main(int argc, char *argv[]) {
  TlsLibraryContext lib_ctx;

  SqlLexer::init_library();

  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
