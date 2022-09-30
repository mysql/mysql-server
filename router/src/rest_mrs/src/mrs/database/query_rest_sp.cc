/*
 Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include "mrs/database/query_rest_sp.h"

namespace mrs {
namespace database {

void QueryRestSP::query_entries(MySQLSession *session,
                                const std::string &schema,
                                const std::string &object,
                                const std::string &url,
                                const std::string &ignore_column) {
  ignore_column_ = ignore_column.c_str();
  query_ = {"CALL !.!"};
  query_ << schema << object;

  response_template.begin(url);
  query(session);
  response_template.end();

  response = response_template.get_result();
}

void QueryRestSP::on_row(const mrs::database::Query::Row &r) {
  response_template.push_json_document(r, columns_, ignore_column_);
}

void QueryRestSP::on_metadata(unsigned int number, MYSQL_FIELD *fields) {
  columns_.clear();
  for (unsigned int i = 0; i < number; ++i) {
    columns_.emplace_back(&fields[i]);
  }
}

}  // namespace database
}  // namespace mrs
