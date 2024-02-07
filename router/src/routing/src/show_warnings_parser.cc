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

#include "show_warnings_parser.h"

#include <charconv>

#include "harness_assert.h"

stdx::expected<std::variant<std::monostate, ShowWarningCount, ShowWarnings>,
               std::string>
ShowWarningsParser::parse() {
  using ret_type = stdx::expected<
      std::variant<std::monostate, ShowWarningCount, ShowWarnings>,
      std::string>;

  if (accept(SHOW)) {
    if (accept(WARNINGS)) {
      stdx::expected<Limit, std::string> limit_res;

      if (accept(LIMIT)) {  // optional limit
        limit_res = limit();
      }

      if (accept(END_OF_INPUT)) {
        if (limit_res) {
          return ret_type{
              std::in_place,
              ShowWarnings{ShowWarnings::Verbosity::Warning,
                           limit_res->row_count, limit_res->offset}};
        }

        return ret_type{std::in_place,
                        ShowWarnings{ShowWarnings::Verbosity::Warning}};
      }

      // unexpected input after SHOW WARNINGS [LIMIT ...]
      return {};
    } else if (accept(ERRORS)) {
      stdx::expected<Limit, std::string> limit_res;

      if (accept(LIMIT)) {
        limit_res = limit();
      }

      if (accept(END_OF_INPUT)) {
        if (limit_res) {
          return ret_type{
              std::in_place,
              ShowWarnings{ShowWarningCount::Verbosity::Error,
                           limit_res->row_count, limit_res->offset}};
        }

        return ret_type{std::in_place,
                        ShowWarnings{ShowWarningCount::Verbosity::Error}};
      }

      // unexpected input after SHOW ERRORS [LIMIT ...]
      return {};
    } else if (accept(COUNT_SYM) && accept('(') && accept('*') && accept(')')) {
      if (accept(WARNINGS)) {
        if (accept(END_OF_INPUT)) {
          return ret_type{std::in_place,
                          ShowWarningCount{ShowWarningCount::Verbosity::Warning,
                                           ShowWarningCount::Scope::Session}};
        }

        // unexpected input after SHOW COUNT(*) WARNINGS
        return {};
      } else if (accept(ERRORS)) {
        if (accept(END_OF_INPUT)) {
          return ret_type{std::in_place,
                          ShowWarningCount{ShowWarningCount::Verbosity::Error,
                                           ShowWarningCount::Scope::Session}};
        }

        // unexpected input after SHOW COUNT(*) ERRORS
        return {};
      }

      // unexpected input after SHOW COUNT(*), expected WARNINGS|ERRORS.
      return {};
    } else {
      // unexpected input after SHOW, expected WARNINGS|ERRORS|COUNT
      return {};
    }
  } else if (accept(SELECT_SYM)) {
    // match
    //
    // SELECT @@((LOCAL|SESSION).)?warning_count|error_count;
    //
    if (accept('@')) {
      if (accept('@')) {
        if (accept(SESSION_SYM)) {
          if (accept('.')) {
            auto ident_res = warning_count_ident();
            if (ident_res && accept(END_OF_INPUT)) {
              return ret_type{
                  std::in_place,
                  ShowWarningCount(*ident_res,
                                   ShowWarningCount::Scope::Session)};
            }
          }
        } else if (accept(LOCAL_SYM)) {
          if (accept('.')) {
            auto ident_res = warning_count_ident();
            if (ident_res && accept(END_OF_INPUT)) {
              return ret_type{
                  std::in_place,
                  ShowWarningCount(*ident_res, ShowWarningCount::Scope::Local)};
            }
          }
        } else {
          auto ident_res = warning_count_ident();
          if (ident_res && accept(END_OF_INPUT)) {
            return ret_type{
                std::in_place,
                ShowWarningCount(*ident_res, ShowWarningCount::Scope::None)};
          }
        }
      }
    }
  }

  // not matched.
  return {};
}

// convert a NUM to a number
//
// NUM is a bare number.
//
// no leading minus or plus [both independent symbols '-' and '+']
// no 0x... [HEX_NUM],
// no 0b... [BIN_NUM],
// no (1.0) [DECIMAL_NUM]
static uint64_t sv_to_num(std::string_view s) {
  uint64_t v{};

  auto conv_res = std::from_chars(s.data(), s.data() + s.size(), v);
  if (conv_res.ec == std::errc{}) {
    return v;
  } else {
    // NUM is a number, it should always convert.
    harness_assert_this_should_not_execute();
  }
}

// accept: NUM [, NUM]
stdx::expected<Limit, std::string> ShowWarningsParser::limit() {
  if (auto num1_tkn = expect(NUM)) {
    auto num1 = sv_to_num(num1_tkn.text());  // offset_or_row_count
    if (accept(',')) {
      if (auto num2_tkn = expect(NUM)) {
        auto num2 = sv_to_num(num2_tkn.text());  // row_count

        return Limit{num2, num1};
      }
    } else {
      return Limit{num1, 0};
    }
  }

  return stdx::unexpected(error_);
}

stdx::expected<ShowWarnings::Verbosity, std::string>
ShowWarningsParser::warning_count_ident() {
  if (auto sess_var_tkn = ident()) {
    if (sess_var_tkn.text() == "warning_count") {
      return ShowWarnings::Verbosity::Warning;
    } else if (sess_var_tkn.text() == "error_count") {
      return ShowWarnings::Verbosity::Error;
    }
  }

  return stdx::unexpected(error_);
}
