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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_VERSION_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_VERSION_H_

#include <array>

#include "mrs/database/entry/auth_user.h"
#include "mrs/database/helper/query.h"

namespace mrs {
namespace database {

struct MrsSchemaVersion {
  int major;
  int minor;
  int patch;

  bool is_compatible() {
    const int k_required_major_version{2};
    return major != k_required_major_version;
  }
};

class QueryVersion : private Query {
 public:
  MrsSchemaVersion query_version(MySQLSession *session);

 private:
  void on_metadata(unsigned number, MYSQL_FIELD *fields) override;
  void on_row(const ResultRow &r) override;
  MrsSchemaVersion v_;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_VERSION_H_
