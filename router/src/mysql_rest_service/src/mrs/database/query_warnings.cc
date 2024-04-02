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

#include <cassert>
#include <string>

#include "mrs/database/query_warnings.h"

namespace mrs {
namespace database {

using Warnings = QueryWarnings::Warnings;

Warnings QueryWarnings::query_warnings(MySQLSession *session) {
  Warnings result;
  warnings_ = &result;

  query(session, "SHOW WARNINGS");

  return result;
}

void QueryWarnings::on_metadata(unsigned number, MYSQL_FIELD *) {
  assert(warnings_ && "Must be initialized, executed without calling `query`");
  if (3 != number)
    throw std::runtime_error(
        "'SHOW WARNINGS', returned unexpected resultset (expecting three "
        "columns).");
}

void QueryWarnings::on_row(const ResultRow &r) {
  assert(warnings_ && "Must be initialized, executed without calling `query`");
  auto id = std::stoull(r[1]);
  auto message = r[2];

  warnings_->emplace_back(id, message);
}

}  // namespace database
}  // namespace mrs
