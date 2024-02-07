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

#ifndef ROUTING_START_TRANSACTION_PARSER_INCLUDED
#define ROUTING_START_TRANSACTION_PARSER_INCLUDED

#include "sql_parser.h"

#include <limits>
#include <variant>

#include "mysql/harness/stdx/expected.h"

class StartTransaction {
 public:
  enum class AccessMode {
    ReadOnly,
    ReadWrite,
  };

  StartTransaction() = default;

  StartTransaction(std::optional<AccessMode> access_mode,
                   bool with_consistent_snapshot)
      : access_mode_(access_mode),
        with_consistent_snapshot_{with_consistent_snapshot} {}

  std::optional<AccessMode> access_mode() const { return access_mode_; }
  bool with_consistent_snapshot() const { return with_consistent_snapshot_; }

 private:
  std::optional<AccessMode> access_mode_;

  bool with_consistent_snapshot_{false};
};

class StartTransactionParser : public SqlParser {
 public:
  using SqlParser::SqlParser;

  stdx::expected<std::variant<std::monostate, StartTransaction>, std::string>
  parse();

  stdx::expected<
      std::variant<std::monostate, StartTransaction::AccessMode, bool>,
      std::string>
  transaction_characteristics();
};

#endif
