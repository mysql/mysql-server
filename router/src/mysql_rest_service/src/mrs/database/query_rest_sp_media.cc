/*
 Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "mrs/database/query_rest_sp_media.h"

#include <stdexcept>

#include "my_rapidjson_size_t.h"

#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>

namespace mrs {
namespace database {

void QueryRestSPMedia::query_entries(MySQLSession *session,
                                     const std::string &schema,
                                     const std::string &object,
                                     const mysqlrouter::sqlstring &values) {
  items = 0;
  query_ = {"CALL !.!(!)"};
  query_ << schema << object << values;

  auto result = query_one(session);

  if (result->size() < 1)
    throw std::logic_error("Query returned an empty resultset.");

  items = 1;
  response.assign((*result.get())[0], result->get_data_size(0));
}

void QueryRestSPMedia::query_entries(MySQLSession *session, const std::string &,
                                     const std::string &, const std::string &,
                                     const PrimaryKeyColumnValues &) {
  assert(0);
  items = 0;
  query_ = {"SELECT ! FROM !.! WHERE ?"};
  // query_ << column << schema << object
  //        << format_where_expr(object->get_base_table(), pk);
  auto result = session->query_one(
      query_, [this](auto no, auto fields) { on_metadata(no, fields); });

  if (result->size() < 1)
    throw std::logic_error("Query returned an empty resultset.");

  items = 1;
  response.assign((*result.get())[0], result->get_data_size(0));
}

void QueryRestSPMedia::query_entries(
    MySQLSession *session, const std::string &column, const std::string &schema,
    const std::string &object, const uint64_t limit, const uint64_t offset) {
  assert(0);
  items = 0;
  query_ = {"SELECT ! FROM !.! LIMIT ?,?"};
  query_ << column << schema << object << offset << limit;

  auto result = session->query_one(
      query_, [this](auto no, auto fields) { on_metadata(no, fields); });

  if (result->size() < 1)
    throw std::logic_error("Query returned an empty resultset.");

  items = 1;
  response.assign((*result.get())[0], result->get_data_size(0));
}

void QueryRestSPMedia::on_row(const ResultRow &) {}

void QueryRestSPMedia::on_metadata(unsigned int, MYSQL_FIELD *) {}

}  // namespace database
}  // namespace mrs
