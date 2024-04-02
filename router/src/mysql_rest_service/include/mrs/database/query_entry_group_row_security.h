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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_ENTRY_ROW_SECURITY_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_ENTRY_ROW_SECURITY_H_

#include <vector>

#include "helper/mysql_row.h"
#include "mrs/database/entry/row_group_ownership.h"
#include "mrs/database/helper/query.h"

namespace mrs {
namespace database {

class QueryEntryGroupRowSecurity : private Query {
 public:
  using RowGroupsSecurity = std::vector<entry::RowGroupOwnership>;
  using MatchLevel = entry::RowGroupOwnership::MatchLevel;

  virtual bool query_group_row_security(MySQLSession *session,
                                        entry::UniversalId db_object_id);

  virtual RowGroupsSecurity &get_result();

 private:
  void on_row(const ResultRow &r) override;

  std::vector<entry::RowGroupOwnership> row_group_security_;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_ENTRY_ROW_SECURITY_H_
