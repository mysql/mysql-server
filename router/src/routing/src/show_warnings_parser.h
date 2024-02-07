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

#ifndef ROUTING_SHOW_WARNINGS_PARSER_INCLUDED
#define ROUTING_SHOW_WARNINGS_PARSER_INCLUDED

#include "sql_parser.h"

#include <limits>
#include <variant>

#include "mysql/harness/stdx/expected.h"

class ShowWarnings {
 public:
  enum class Verbosity {
    Warning,
    Error,
  };

  ShowWarnings(Verbosity verbosity,
               uint64_t row_count = std::numeric_limits<uint64_t>().max(),
               uint64_t offset = 0)
      : verbosity_(verbosity), row_count_{row_count}, offset_{offset} {}

  Verbosity verbosity() const { return verbosity_; }
  uint64_t row_count() const { return row_count_; }
  uint64_t offset() const { return offset_; }

 private:
  Verbosity verbosity_;

  uint64_t row_count_;
  uint64_t offset_;
};

class ShowWarningCount {
 public:
  enum class Scope { Local, Session, None };

  using Verbosity = ShowWarnings::Verbosity;

  ShowWarningCount(Verbosity verbosity, Scope scope)
      : verbosity_(verbosity), scope_{scope} {}

  Verbosity verbosity() const { return verbosity_; }
  Scope scope() const { return scope_; }

 private:
  Verbosity verbosity_;
  Scope scope_;
};

struct Limit {
  uint64_t row_count{std::numeric_limits<uint64_t>::max()};
  uint64_t offset{};
};

class ShowWarningsParser : public SqlParser {
 public:
  using SqlParser::SqlParser;

  stdx::expected<std::variant<std::monostate, ShowWarningCount, ShowWarnings>,
                 std::string>
  parse();

 protected:
  stdx::expected<Limit, std::string> limit();

  stdx::expected<ShowWarnings::Verbosity, std::string> warning_count_ident();
};

#endif
