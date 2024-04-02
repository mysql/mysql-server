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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_CONTENT_SET_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_CONTENT_SET_H_

#include <vector>

#include "mrs/database/entry/content_file.h"
#include "mrs/database/helper/query.h"

namespace mrs {
namespace database {

class QueryEntriesContentFile : protected Query {
 public:
  using VectorOfPaths = std::vector<database::entry::ContentFile>;

 public:
  QueryEntriesContentFile();

  virtual uint64_t get_last_update();
  virtual void query_entries(MySQLSession *session);

  VectorOfPaths entries;

 protected:
  void on_row(const ResultRow &row) override;

  uint64_t audit_log_id_;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_CONTENT_SET_H_
