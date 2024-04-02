/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include <array>
#include <stdexcept>

#include "mrs/database/query_version.h"

namespace mrs {
namespace database {

static void throw_invalid_function_result() {
  throw std::runtime_error(
      "Function/view `mysql_rest_service_metadata`.`schema_version`, returned "
      "invalid data.");
}

MrsSchemaVersion QueryVersion::query_version(MySQLSession *session) {
  query(session,
        "SELECT `major`,`minor`,`patch` FROM "
        "mysql_rest_service_metadata.schema_version;");
  return v_;
}

void QueryVersion::on_metadata(unsigned number, MYSQL_FIELD *) {
  if (3 != number) throw_invalid_function_result();
}

void QueryVersion::on_row(const ResultRow &r) {
  if (r.size() != 3) throw_invalid_function_result();
  v_.major = std::stoi(r[0]);
  v_.minor = std::stoi(r[1]);
  v_.patch = std::stoi(r[2]);
}

}  // namespace database
}  // namespace mrs
