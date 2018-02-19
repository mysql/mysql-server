/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file services.cc

  Implementation part of the parser service layer.
*/

#include "plugin/rewriter/services.h"

#include "my_config.h"

#include <stddef.h>

#include "template_utils.h"

using std::string;

namespace services {

string print_digest(const unsigned char *digest) {
  const size_t string_size = PARSER_SERVICE_DIGEST_LENGTH * 2;
  char digest_str[string_size + sizeof('\0')];
  for (int i = 0; i < PARSER_SERVICE_DIGEST_LENGTH; ++i)
    snprintf(digest_str + i * 2, string_size, "%02x", digest[i]);
  return digest_str;
}

bool Digest::load(MYSQL_THD thd) {
  return mysql_parser_get_statement_digest(thd, m_buf) != 0;
}

Session::Session(MYSQL_THD current_session)
    : m_previous_session(current_session),
      m_current_session(mysql_parser_open_session()) {}

Condition_handler::~Condition_handler() {}

/**
  Bridge function between the C++ API offered by this module and the C API of
  the parser service. This layer always uses a Condition_handler object that
  is passed to mysql_parser_parse().
*/
static int handle(int sql_errno, const char *sqlstate, const char *message,
                  void *state) {
  Condition_handler *handler = static_cast<Condition_handler *>(state);
  return handler->handle(sql_errno, sqlstate, message);
}

/// Convenience function, to avoid sprinkling the code with const_casts.
static MYSQL_LEX_STRING make_lex_string(const string &str) {
  MYSQL_LEX_STRING lex_str = {const_cast<char *>(str.c_str()), str.length()};
  return lex_str;
}

void set_current_database(MYSQL_THD thd, const string &db) {
  MYSQL_LEX_STRING db_str = make_lex_string(db);
  mysql_parser_set_current_database(thd, db_str);
}

bool parse(MYSQL_THD thd, const string &query, bool is_prepared,
           Condition_handler *handler) {
  MYSQL_LEX_STRING query_str = make_lex_string(query);
  return mysql_parser_parse(thd, query_str, is_prepared, handle, handler);
}

bool parse(MYSQL_THD thd, const string &query, bool is_prepared) {
  MYSQL_LEX_STRING query_str = make_lex_string(query);
  return mysql_parser_parse(thd, query_str, is_prepared, NULL, NULL);
}

bool is_select_statement(MYSQL_THD thd) {
  return mysql_parser_get_statement_type(thd) == STATEMENT_TYPE_SELECT;
}

int get_number_params(MYSQL_THD thd) {
  return mysql_parser_get_number_params(thd);
}

static int process_item(MYSQL_ITEM item, uchar *arg) {
  Literal_visitor *visitor = pointer_cast<Literal_visitor *>(arg);
  if (visitor->visit(item)) return 1;
  return 0;
}

bool visit_parse_tree(MYSQL_THD thd, Literal_visitor *visitor) {
  uchar *arg = pointer_cast<uchar *>(visitor);
  return mysql_parser_visit_tree(thd, process_item, arg) != 0;
}

/**
  A very limited smart pointer to protect against stack unwinding in case an
  STL class throws an exception. The interface is similar to unique_ptr.
*/
class Lex_str {
  MYSQL_LEX_STRING m_str;
  Lex_str &operator=(const Lex_str &);
  Lex_str(const Lex_str &);

 public:
  Lex_str(MYSQL_LEX_STRING str) : m_str(str) {}

  const MYSQL_LEX_STRING get() { return m_str; }

  ~Lex_str() { mysql_parser_free_string(m_str); }
};

/// Prints an Item as an std::string.
string print_item(MYSQL_ITEM item) {
  Lex_str lex_str(mysql_parser_item_string(item));
  string literal;
  literal.assign(lex_str.get().str, lex_str.get().length);
  return literal;
}

string get_current_query_normalized(MYSQL_THD thd) {
  MYSQL_LEX_STRING normalized_pattern = mysql_parser_get_normalized_query(thd);
  string s;
  s.assign(normalized_pattern.str, normalized_pattern.length);
  return s;
}

/**
  A very limited smart pointer to protect against stack unwinding in case an
  STL class throws an exception. The interface is similar to unique_ptr.
*/
class Array_ptr {
  int *m_ptr;
  Array_ptr &operator=(const Array_ptr &);
  Array_ptr(const Array_ptr &);

 public:
  Array_ptr(int *str) : m_ptr(str) {}

  int *get() { return m_ptr; }
  ~Array_ptr() { delete[] m_ptr; }
};

std::vector<int> get_parameter_positions(MYSQL_THD thd) {
  int number_params = get_number_params(thd);
  Array_ptr parameter_positions(new int[number_params]);
  mysql_parser_extract_prepared_params(thd, parameter_positions.get());
  std::vector<int> positions(parameter_positions.get(),
                             parameter_positions.get() + number_params);
  return positions;
}

}  // namespace services
