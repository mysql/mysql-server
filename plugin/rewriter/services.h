#ifndef SERVICES_INCLUDED
#define SERVICES_INCLUDED
/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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
  @file plugin/rewriter/services.h

  Conversion layer between the parser service and this plugin. This plugin is
  written in C++, while the parser service is written in C.

  The layer handles:

  - Copying between server and plugin memory. This is necessary on some
    platforms (e.g. Windows) where dynamicly linked libraries have their own
    heap.

  - Wrapping raw const char * in std::string classes.
*/

#include <mysql/service_parser.h>
#include <string>
#include <vector>

#include "my_inttypes.h"

namespace services {

class Session {
 public:
  Session(MYSQL_THD current_session);

  MYSQL_THD thd() { return m_current_session; }

 private:
  MYSQL_THD m_previous_session;
  MYSQL_THD m_current_session;
};

class Digest {
  uchar m_buf[PARSER_SERVICE_DIGEST_LENGTH];

 public:
  /**
    Copies the digest buffer from the server.

    @retval false Server reported success.
    @retval true Server reported failure.
  */
  bool load(MYSQL_THD thd);

  /// Needed because we use a C hash table to store digests.
  const uchar *c_ptr() const { return m_buf; }
};

class Literal_visitor {
 public:
  virtual ~Literal_visitor() {}
  virtual bool visit(MYSQL_ITEM item) = 0;
};

/**
  This class may inherited and passed to parse() in order to handle conditions
  raised by the server.
*/
class Condition_handler {
 public:
  /**
    This function will be called by the server via this API before raising a
    condition. The Condition_handler subclass may then decide to handle the
    condition by returning true, in which case the server does not raise it.

    @param sql_errno The condition number.
    @param sqlstate The SQLSTATE, allocated in the server.
    @param message The condition's message, allocated in the server.

    @retval true The condition is handled entirely by this object.
    @retval false The condition is not handled.
  */
  virtual bool handle(int sql_errno, const char *sqlstate,
                      const char *message) = 0;

  virtual ~Condition_handler() = 0;
};

std::string print_digest(const uchar *digest);

void set_current_database(MYSQL_THD thd, const std::string &db);

bool parse(MYSQL_THD thd, const std::string &query, bool is_prepared,
           Condition_handler *handler);

bool parse(MYSQL_THD thd, const std::string &query, bool is_prepared);

bool is_supported_statement(MYSQL_THD thd);

int get_number_params(MYSQL_THD thd);

bool visit_parse_tree(MYSQL_THD thd, Literal_visitor *visitor);

/// Prints an Item as an std::string.
std::string print_item(MYSQL_ITEM item);

std::string get_current_query_normalized(MYSQL_THD thd);

std::vector<int> get_parameter_positions(MYSQL_THD thd);
}  // namespace services

#endif  // SERVICES_INCLUDED
