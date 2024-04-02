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

#include "mrs/database/query_uuid.h"

namespace mrs {
namespace database {

using UserId = QueryUuid::UserId;

QueryUuid::QueryUuid() {
  query_ = "SELECT `mysql_rest_service_metadata`.`get_sequence_id`();";
}

void QueryUuid::generate_uuid(MySQLSession *session) { execute(session); }

UserId QueryUuid::get_result() {
  UserId result;
  memcpy(result.raw, uuid_.data(), uuid_.size());
  return result;
}

void QueryUuid::on_metadata(unsigned number, MYSQL_FIELD *fields) {
  if (1 != number)
    throw std::runtime_error(
        "Function `mysql_rest_service_metadata`.`get_sequence_id`, returned "
        "invalid data.");

  if (fields[0].length != uuid_.size())
    throw std::runtime_error("Generated UUID has invalid size.");
}

void QueryUuid::on_row(const ResultRow &r) {
  memcpy(uuid_.data(), r[0], uuid_.size());
}

}  // namespace database
}  // namespace mrs
