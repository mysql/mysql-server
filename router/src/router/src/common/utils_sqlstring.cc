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

#include "mysqlrouter/utils_sqlstring.h"

#include <cstring>
#include <string>

#include <cassert>

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

// updated as of 5.7
static const char *reserved_keywords[] = {"ACCESSIBLE",
                                          "ADD",
                                          "ALL",
                                          "ALTER",
                                          "ANALYZE",
                                          "AND",
                                          "AS",
                                          "ASC",
                                          "ASENSITIVE",
                                          "BEFORE",
                                          "BETWEEN",
                                          "BIGINT",
                                          "BINARY",
                                          "BLOB",
                                          "BOTH",
                                          "BY",
                                          "CALL",
                                          "CASCADE",
                                          "CASE",
                                          "CHANGE",
                                          "CHAR",
                                          "CHARACTER",
                                          "CHECK",
                                          "COLLATE",
                                          "COLUMN",
                                          "CONDITION",
                                          "CONSTRAINT",
                                          "CONTINUE",
                                          "CONVERT",
                                          "CREATE",
                                          "CROSS",
                                          "CURRENT_DATE",
                                          "CURRENT_TIME",
                                          "CURRENT_TIMESTAMP",
                                          "CURRENT_USER",
                                          "CURSOR",
                                          "DATABASE",
                                          "DATABASES",
                                          "DAY_HOUR",
                                          "DAY_MICROSECOND",
                                          "DAY_MINUTE",
                                          "DAY_SECOND",
                                          "DEC",
                                          "DECIMAL",
                                          "DECLARE",
                                          "DEFAULT",
                                          "DELAYED",
                                          "DELETE",
                                          "DESC",
                                          "DESCRIBE",
                                          "DETERMINISTIC",
                                          "DISTINCT",
                                          "DISTINCTROW",
                                          "DIV",
                                          "DOUBLE",
                                          "DROP",
                                          "DUAL",
                                          "EACH",
                                          "ELSE",
                                          "ELSEIF",
                                          "ENCLOSED",
                                          "ESCAPED",
                                          "EXISTS",
                                          "EXIT",
                                          "EXPLAIN",
                                          "FALSE",
                                          "FETCH",
                                          "FLOAT",
                                          "FLOAT4",
                                          "FLOAT8",
                                          "FOR",
                                          "FORCE",
                                          "FOREIGN",
                                          "FROM",
                                          "FULLTEXT",
                                          "GET",
                                          "GRANT",
                                          "GROUP",
                                          "HAVING",
                                          "HIGH_PRIORITY",
                                          "HOUR_MICROSECOND",
                                          "HOUR_MINUTE",
                                          "HOUR_SECOND",
                                          "IF",
                                          "IGNORE",
                                          "IN",
                                          "INDEX",
                                          "INFILE",
                                          "INNER",
                                          "INOUT",
                                          "INSENSITIVE",
                                          "INSERT",
                                          "INT",
                                          "INT1",
                                          "INT2",
                                          "INT3",
                                          "INT4",
                                          "INT8",
                                          "INTEGER",
                                          "INTERVAL",
                                          "INTO",
                                          "IO_AFTER_GTIDS",
                                          "IO_BEFORE_GTIDS",
                                          "IS",
                                          "ITERATE",
                                          "JOIN",
                                          "KEY",
                                          "KEYS",
                                          "KILL",
                                          "LEADING",
                                          "LEAVE",
                                          "LEFT",
                                          "LIKE",
                                          "LIMIT",
                                          "LINEAR",
                                          "LINES",
                                          "LOAD",
                                          "LOCALTIME",
                                          "LOCALTIMESTAMP",
                                          "LOCK",
                                          "LONG",
                                          "LONGBLOB",
                                          "LONGTEXT",
                                          "LOOP",
                                          "LOW_PRIORITY",
                                          "MATCH",
                                          "MAXVALUE",
                                          "MEDIUMBLOB",
                                          "MEDIUMINT",
                                          "MEDIUMTEXT",
                                          "MIDDLEINT",
                                          "MINUTE_MICROSECOND",
                                          "MINUTE_SECOND",
                                          "MOD",
                                          "MODIFIES",
                                          "NATURAL",
                                          "NONBLOCKING",
                                          "NOT",
                                          "NO_WRITE_TO_BINLOG",
                                          "NULL",
                                          "NUMERIC",
                                          "ON",
                                          "OPTIMIZE",
                                          "OPTION",
                                          "OPTIONALLY",
                                          "OR",
                                          "ORDER",
                                          "OUT",
                                          "OUTER",
                                          "OUTFILE",
                                          "PARTITION",
                                          "PRECISION",
                                          "PRIMARY",
                                          "PROCEDURE",
                                          "PURGE",
                                          "QUALIFY",
                                          "RANGE",
                                          "READ",
                                          "READS",
                                          "READ_WRITE",
                                          "REAL",
                                          "REFERENCES",
                                          "REGEXP",
                                          "RELEASE",
                                          "RENAME",
                                          "REPEAT",
                                          "REPLACE",
                                          "REQUIRE",
                                          "RESIGNAL",
                                          "RESTRICT",
                                          "RETURN",
                                          "REVOKE",
                                          "RIGHT",
                                          "RLIKE",
                                          "SCHEMA",
                                          "SCHEMAS",
                                          "SECOND_MICROSECOND",
                                          "SELECT",
                                          "SENSITIVE",
                                          "SEPARATOR",
                                          "SET",
                                          "SHOW",
                                          "SIGNAL",
                                          "SMALLINT",
                                          "SPATIAL",
                                          "SPECIFIC",
                                          "SQL",
                                          "SQLEXCEPTION",
                                          "SQLSTATE",
                                          "SQLWARNING",
                                          "SQL_BIG_RESULT",
                                          "SQL_CALC_FOUND_ROWS",
                                          "SQL_SMALL_RESULT",
                                          "SSL",
                                          "STARTING",
                                          "STRAIGHT_JOIN",
                                          "TABLE",
                                          "TERMINATED",
                                          "THEN",
                                          "TINYBLOB",
                                          "TINYINT",
                                          "TINYTEXT",
                                          "TO",
                                          "TRAILING",
                                          "TRIGGER",
                                          "TRUE",
                                          "UNDO",
                                          "UNION",
                                          "UNIQUE",
                                          "UNLOCK",
                                          "UNSIGNED",
                                          "UPDATE",
                                          "USAGE",
                                          "USE",
                                          "USING",
                                          "UTC_DATE",
                                          "UTC_TIME",
                                          "UTC_TIMESTAMP",
                                          "VALUES",
                                          "VARBINARY",
                                          "VARCHAR",
                                          "VARCHARACTER",
                                          "VARYING",
                                          "WHEN",
                                          "WHERE",
                                          "WHILE",
                                          "WITH",
                                          "WRITE",
                                          "XOR",
                                          "YEAR_MONTH",
                                          "ZEROFILL",
                                          nullptr};

namespace mysqlrouter {
//--------------------------------------------------------------------------------------------------

/**
 * Escape a string to be used in a SQL query
 * Same code as used by mysql. Handles null bytes in the middle of the string.
 * If wildcards is true then _ and % are masked as well.
 */
std::string escape_sql_string(const char *s, int len, bool wildcards) {
  std::string result;
  result.reserve(len);

  auto end = s + len;
  for (auto ch = s; ch != end; ++ch) {
    char escape = 0;

    switch (*ch) {
      case 0: /* Must be escaped for 'mysql' */
        escape = '0';
        break;
      case '\n': /* Must be escaped for logs */
        escape = 'n';
        break;
      case '\r':
        escape = 'r';
        break;
      case '\\':
        escape = '\\';
        break;
      case '\'':
        escape = '\'';
        break;
      case '"': /* Better safe than sorry */
        escape = '"';
        break;
      case '\032': /* This gives problems on Win32 */
        escape = 'Z';
        break;
      case '_':
        if (wildcards) escape = '_';
        break;
      case '%':
        if (wildcards) escape = '%';
        break;
    }
    if (escape) {
      result.push_back('\\');
      result.push_back(escape);
    } else
      result.push_back(*ch);
  }
  return result;
}

std::string escape_sql_string(const std::string &s, bool wildcards) {
  return escape_sql_string(s.c_str(), s.length(), wildcards);
}

std::string escape_sql_string(const char *s, bool wildcards) {
  return escape_sql_string(s, strlen(s), wildcards);
}

//--------------------------------------------------------------------------------------------------

// NOTE: This is not the same as escape_sql_string, as embedded ` must be
// escaped as ``, not \` and \ ' and " must not be escaped
std::string escape_backticks(const std::string &s) {
  return escape_backticks(s.c_str(), s.length());
}

std::string escape_backticks(const char *s, int length) {
  std::string result;
  result.reserve(length);

  auto end = s + length;
  for (auto ch = s; ch != end; ++ch) {
    char escape = 0;

    switch (*ch) {
      case 0: /* Must be escaped for 'mysql' */
        escape = '0';
        break;
      case '\n': /* Must be escaped for logs */
        escape = 'n';
        break;
      case '\r':
        escape = 'r';
        break;
      case '\032': /* This gives problems on Win32 */
        escape = 'Z';
        break;
      case '`':
        // special case
        result.push_back('`');
        break;
    }
    if (escape) {
      result.push_back('\\');
      result.push_back(escape);
    } else
      result.push_back(*ch);
  }
  return result;
}

//--------------------------------------------------------------------------------------------------

bool is_reserved_word(const std::string &word) {
  for (const char **kw = reserved_keywords; *kw != nullptr; ++kw) {
    if (strcasecmp(word.c_str(), *kw) == 0) return true;
  }
  return false;
}

//--------------------------------------------------------------------------------------------------

std::string quote_identifier(const std::string &identifier,
                             const char quote_char) {
  return quote_char + identifier + quote_char;
}

//--------------------------------------------------------------------------------------------------

/**
 * Quotes the given identifier, but only if it needs to be quoted.
 * http://dev.mysql.com/doc/refman/5.1/en/identifiers.html specifies what is
 * allowed in unquoted identifiers. Leading numbers are not strictly forbidden
 * but discouraged as they may lead to ambiguous behavior.
 */
std::string quote_identifier_if_needed(const std::string &ident,
                                       const char quote_char) {
  bool needs_quotation =
      is_reserved_word(ident);  // check whether it's a reserved keyword
  size_t digits = 0;

  if (!needs_quotation) {
    for (std::string::const_iterator i = ident.begin(); i != ident.end(); ++i) {
      if ((*i >= 'a' && *i <= 'z') || (*i >= 'A' && *i <= 'Z') ||
          (*i >= '0' && *i <= '9') || (*i == '_') || (*i == '$') ||
          ((unsigned char)(*i) > 0x7F)) {
        if (*i >= '0' && *i <= '9') digits++;

        continue;
      }
      needs_quotation = true;
      break;
    }
  }

  if (needs_quotation || digits == ident.length())
    return quote_char + ident + quote_char;
  else
    return ident;
}

const sqlstring sqlstring::empty{""};
const sqlstring sqlstring::null(sqlstring("NULL", 0));
const sqlstring sqlstring::end(sqlstring("", EndOfInput));

sqlstring::sqlstring(const char *format_string, const sqlstringformat format)
    : _format_string_left(format_string), _format(format) {
  append(consume_until_next_escape());
}

sqlstring::sqlstring(const sqlstring &) = default;

sqlstring::sqlstring() : _format(0) {}

std::string sqlstring::consume_until_next_escape() {
  if (_locked_escape) return {};

  std::string::size_type e = _format_string_left.length(), p = 0;
  while (p < e) {
    char ch = _format_string_left[p];
    if (ch == '?' || ch == '!') break;
    ++p;
  }
  if (p > 0) {
    std::string s = _format_string_left.substr(0, p);
    if (p < e)
      _format_string_left = _format_string_left.substr(p);
    else
      _format_string_left.clear();
    return s;
  }
  return "";
}

int sqlstring::next_escape() {
  if (_locked_escape) return _locked_escape;
  if (_format_string_left.empty())
    throw std::invalid_argument(
        "Error formatting SQL query: more arguments than escapes");
  int c = _format_string_left[0];
  _format_string_left = _format_string_left.substr(1);
  return c;
}

void sqlstring::lock_escape(int esc) { _locked_escape = esc; }

void sqlstring::unlock_escape() { _locked_escape = 0; }

sqlstring &sqlstring::append(const std::string &s) {
  _formatted.append(s);
  return *this;
}

sqlstring &sqlstring::append_preformatted(const sqlstring &s) {
  if (!_format_string_left.empty())
    throw std::invalid_argument(
        "cannot append to a sqlstring with escapes left");
  if (!s._format_string_left.empty())
    throw std::invalid_argument("cannot append a sqlstring with escapes left");

  _formatted.append(s._formatted);

  return *this;
}

sqlstring &sqlstring::append_preformatted_sep(const std::string &separator,
                                              const sqlstring &s) {
  if (!_format_string_left.empty())
    throw std::invalid_argument(
        "cannot append to a sqlstring with escapes left");
  if (!s._format_string_left.empty())
    throw std::invalid_argument("cannot append a sqlstring with escapes left");

  if (!_formatted.empty()) _formatted.append(separator);
  _formatted.append(s._formatted);

  return *this;
}

sqlstring::operator std::string() const {
  return _formatted + _format_string_left;
}

std::string sqlstring::str() const { return _formatted + _format_string_left; }

bool sqlstring::is_empty() const {
  if (!_formatted.empty()) return false;

  return _format_string_left.empty();
}

void sqlstring::reset(const char *format_string, const sqlstringformat format) {
  _formatted.resize(0);
  _format_string_left = format_string;
  _format = format;
  _locked_escape = 0;
  append(consume_until_next_escape());
}

bool sqlstring::done() const {
  if (_format_string_left.empty()) return true;
  return _format_string_left[0] != '!' && _format_string_left[0] != '?';
}

sqlstring &sqlstring::operator<<(const double v) {
  int esc = next_escape();
  if (esc != '?')
    throw std::invalid_argument(
        "Error formatting SQL query: invalid escape for numeric argument");

  append(std::to_string(v));
  append(consume_until_next_escape());

  return *this;
}

sqlstring &sqlstring::operator<<(const std::nullptr_t) {
  int esc = next_escape();
  if (esc != '?')
    throw std::invalid_argument(
        "Error formatting SQL query: invalid escape for numeric argument");

  append(std::string("NULL"));
  append(consume_until_next_escape());

  return *this;
}

sqlstring &sqlstring::operator<<(const sqlstringformat format) {
  _format = format;
  return *this;
}

sqlstring &sqlstring::operator<<(const std::string &v) {
  format(next_escape(), v.c_str(), v.length());
  append(consume_until_next_escape());

  return *this;
}

sqlstring &sqlstring::operator<<(const sqlstring &v) {
  if ((v._format._flags & EndOfInput)) {
    if (!done())
      throw std::logic_error(
          "Insufficient number of parameters given to sqlstring");
    return *this;
  }
  next_escape();

  append(v);
  append(consume_until_next_escape());

  return *this;
}

sqlstring &sqlstring::operator<<(const char *v) {
  int esc = next_escape();

  if (esc == '!' && !v)
    throw std::invalid_argument(
        "Error formatting SQL query: NULL value found for identifier");

  format(esc, v, strlen(v));
  append(consume_until_next_escape());

  return *this;
}

}  // namespace mysqlrouter
