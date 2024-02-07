/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _UTILS_SQLSTRING_H_
#define _UTILS_SQLSTRING_H_

#include "mysqlrouter/router_export.h"

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS 1
#endif
#include <inttypes.h>
#include <string>

#include <stdexcept>

namespace mysqlrouter {

enum SqlStringFlags {
  QuoteOnlyIfNeeded = 1 << 0,
  UseAnsiQuotes = 1 << 1,

  EndOfInput = 1 << 7
};

std::string ROUTER_LIB_EXPORT
escape_sql_string(const std::string &string,
                  bool wildcards = false);  // "strings" or 'strings'
std::string ROUTER_LIB_EXPORT
escape_backticks(const std::string &string);  // `identifier`
std::string ROUTER_LIB_EXPORT quote_identifier(const std::string &identifier,
                                               const char quote_char);
std::string ROUTER_LIB_EXPORT
quote_identifier_if_needed(const std::string &ident, const char quote_char);

class ROUTER_LIB_EXPORT sqlstring {
 public:
  struct sqlstringformat {
    int _flags;
    sqlstringformat(const int flags) : _flags(flags) {}
  };

 private:
  std::string _formatted;
  std::string _format_string_left;
  sqlstringformat _format;

  std::string consume_until_next_escape();
  int next_escape();

  sqlstring &append(const std::string &s);

 public:
  static const sqlstring null;
  static const sqlstring end;

  sqlstring();
  sqlstring(const char *format_string, const sqlstringformat format = 0);
  sqlstring(const sqlstring &copy);
  sqlstring &operator=(const sqlstring &) = default;
  bool done() const;

  operator std::string() const;
  std::string str() const;

  //! modifies formatting options
  sqlstring &operator<<(const sqlstringformat);
  //! replaces a ? in the format string with a float numeric value
  sqlstring &operator<<(const float val) { return operator<<((double)val); }
  //! replaces a ? in the format string with a double numeric value
  sqlstring &operator<<(const double);
  //! replaces a ? in the format string with a quoted string value or ! with a
  //! back-quoted identifier value
  sqlstring &operator<<(const std::string &);
  //! replaces a ? in the format string with a quoted string value or ! with a
  //! back-quoted identifier value is the value is NULL, ? will be replaced with
  //! a NULL. ! will raise an exception
  sqlstring &operator<<(const char *);
  //! replaces a ? or ! with the content of the other string verbatim
  sqlstring &operator<<(const sqlstring &);
  //! replaces a ? in the format string with any integer numeric value
  template <typename T>
  sqlstring &operator<<(const T value) {
    int esc = next_escape();
    if (esc != '?')
      throw std::invalid_argument(
          "Error formatting SQL query: invalid escape for numeric argument");
    append(std::to_string(value));
    append(consume_until_next_escape());
    return *this;
  }
};
}  // namespace mysqlrouter

#endif
