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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_CHANGES_PATH_ENTRIES_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_CHANGES_PATH_ENTRIES_H_

#include <set>

#include "mrs/database/entry/db_object.h"
#include "mrs/database/query_entries_db_object.h"

namespace mrs {
namespace database {

class QueryChangesDbObject : public QueryEntriesDbObject {
 public:
  using QueryFactory = mrs::interface::QueryFactory;

 public:
  QueryChangesDbObject(SupportedMrsMetadataVersion v,
                       QueryFactory *query_factory,
                       const uint64_t last_audit_id);

  /**
   * Fetch from database the list of all defined object/path entries
   *
   * Except fetching the list, it also tries to fetch matching `audit_log.id`.
   */
  void query_entries(MySQLSession *session) override;

 private:
  std::set<entry::UniversalId> path_entries_fetched;
  uint64_t query_length_;

  void query_path_entries(MySQLSession *session, VectorOfPathEntries *out,
                          const std::string &table_name,
                          const entry::UniversalId &id);
  std::string build_query(const std::string &table_name,
                          const entry::UniversalId &id);
};

class QueryChangesDbObjectLite : public QueryEntriesDbObjectLite {
 public:
  using QueryFactory = mrs::interface::QueryFactory;

 public:
  QueryChangesDbObjectLite(SupportedMrsMetadataVersion v,
                           QueryFactory *query_factory,
                           const uint64_t last_audit_id);

  /**
   * Fetch from database the list of all defined object/path entries
   *
   * Except fetching the list, it also tries to fetch matching `audit_log.id`.
   */
  void query_entries(MySQLSession *session) override;

 private:
  std::set<entry::UniversalId> path_entries_fetched;
  uint64_t query_length_;

  void query_path_entries(MySQLSession *session, VectorOfPathEntries *out,
                          const std::string &table_name,
                          const entry::UniversalId &id);
  std::string build_query(const std::string &table_name,
                          const entry::UniversalId &id);
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_CHANGES_PATH_ENTRIES_H_
