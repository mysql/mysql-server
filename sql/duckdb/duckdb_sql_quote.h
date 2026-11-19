
/*****************************************************************************

Copyright (c) 2013, 2026, Alibaba and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#pragma once

#include <cstddef>
#include <string>

namespace myduck {

/** Quote a byte string as a DuckDB SQL string literal.
DuckDB accepts SQL quote doubling and also interprets backslash escapes in
string literals. Escape both forms explicitly so generated SQL preserves
quotes, backslashes, and embedded NUL bytes. NUL is emitted as the C-style
\0 escape; DuckDB parses this non-greedily (always exactly one character),
so trailing digits stay literal -- "NUL '1'" round-trips as '\0' + '1'
rather than being absorbed into a multi-digit octal sequence.
@param[in]  s       raw string buffer
@param[in]  length  raw string length in bytes
@return single-quoted DuckDB string literal */
inline std::string quote_duckdb_string(const char *s, size_t length) {
  std::string out;
  out.reserve(length + 2);
  out += '\'';
  for (size_t i = 0; i < length; i++) {
    switch (s[i]) {
      case '\0':
        out += "\\0";
        break;
      case '\'':
        out += "''";
        break;
      case '\\':
        out += "\\\\";
        break;
      default:
        out += s[i];
    }
  }
  out += '\'';
  return out;
}

/** Quote a string as a DuckDB SQL string literal.
@param[in]  s   raw string bytes
@return single-quoted DuckDB string literal */
inline std::string quote_duckdb_string(const std::string &s) {
  return quote_duckdb_string(s.c_str(), s.length());
}

/** Quote an identifier for DuckDB generated SQL.
The DuckDB integration uses backtick-quoted identifiers to mirror MySQL SQL
text. Double embedded backticks so schema/table/column names round-trip with
the MySQL data dictionary.
@param[in]  s       raw identifier (table/column/schema name)
@param[in]  length  raw identifier length
@return backtick-quoted DuckDB identifier */
inline std::string quote_duckdb_ident(const char *s, size_t length) {
  std::string out;
  out.reserve(length + 2);
  out += '`';
  for (size_t i = 0; i < length; i++) {
    char c = s[i];
    out += c;
    if (c == '`') out += c;
  }
  out += '`';
  return out;
}

/** Quote an identifier for DuckDB generated SQL.
@param[in]  s   raw identifier (table/column/schema name)
@return backtick-quoted DuckDB identifier */
inline std::string quote_duckdb_ident(const std::string &s) {
  return quote_duckdb_ident(s.c_str(), s.length());
}

}  // namespace myduck
