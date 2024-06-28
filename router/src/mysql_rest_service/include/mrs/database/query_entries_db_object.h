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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_SCHEMA_ROUTER_ENTRIES_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_SCHEMA_ROUTER_ENTRIES_H_

#include "mrs/database/entry/db_object.h"
#include "mrs/database/helper/query.h"
#include "mrs/interface/query_factory.h"
#include "mrs/interface/supported_mrs_schema_version.h"

namespace mrs {
namespace database {

class QueryEntriesDbObject : protected Query {
 public:
  using DbObject = entry::DbObject;
  using VectorOfPathEntries = std::vector<DbObject>;
  using SupportedMrsMetadataVersion =
      mrs::interface::SupportedMrsMetadataVersion;

 public:
  QueryEntriesDbObject(SupportedMrsMetadataVersion v,
                       mrs::interface::QueryFactory *query_factory);

  virtual uint64_t get_last_update();
  /**
   * Fetch from database the list of all defined object/path entries
   *
   * Except fetching the list, it also tries to fetch matching `audit_log.id`.
   */
  virtual void query_entries(MySQLSession *session);

  VectorOfPathEntries entries;

 protected:
  void on_row(const ResultRow &r) override;
  static std::string skip_starting_slash(const std::string &value);

  SupportedMrsMetadataVersion db_version_;
  uint64_t audit_log_id_{0};
  mrs::interface::QueryFactory *query_factory_;
  std::vector<std::optional<std::string>> db_object_user_ownership_v2_;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_SCHEMA_ROUTER_ENTRIES_H_
