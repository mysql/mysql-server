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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_AUTENTICATION_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_AUTENTICATION_H_

#include <vector>

#include "helper/mysql_time.h"
#include "mrs/database/entry/auth_app.h"
#include "mrs/database/helper/query.h"

namespace mrs {
namespace database {

class QueryEntriesAuthApp : protected Query {
 public:
  using AuthApp = entry::AuthApp;
  using Entries = std::vector<AuthApp>;

 public:
  virtual Entries &get_entries() = 0;
  virtual uint64_t get_last_update() = 0;
  virtual void query_entries(MySQLSession *session) = 0;
};

namespace v2 {

class QueryEntriesAuthApp : public mrs::database::QueryEntriesAuthApp {
 public:
  QueryEntriesAuthApp();

  uint64_t get_last_update() override;
  void query_entries(MySQLSession *session) override;
  Entries &get_entries() override;

 protected:
  Entries entries_;
  uint64_t audit_log_id_{0};
  void on_row(const ResultRow &r) override;
};

}  // namespace v2

namespace v3 {

class QueryEntriesAuthApp : public mrs::database::v2::QueryEntriesAuthApp {
 public:
  QueryEntriesAuthApp();
};

}  // namespace v3

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_AUTENTICATION_H_
