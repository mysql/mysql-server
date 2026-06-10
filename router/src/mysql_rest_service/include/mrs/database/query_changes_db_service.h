/*
 Copyright (c) 2024, 2026, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_CHANGES_DB_SERVICE_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_CHANGES_DB_SERVICE_H_

#include <set>

#include "mrs/database/entry/universal_id.h"
#include "mrs/database/query_entries_db_service.h"

namespace mrs {
namespace database {

class QueryChangesDbService : public QueryEntriesDbService {
 public:
  QueryChangesDbService(SupportedMrsMetadataVersion v,
                        const uint64_t last_audit_log_id,
                        const std::optional<uint64_t> &router_id);

  void query_entries(MySQLSession *session) override;

 private:
  void query_service_entries(MySQLSession *session, VectorOfEntries *out,
                             const std::string &table_name,
                             const entry::UniversalId id);
  /*
   * Fetch additional service (additionally to those returned by
   * `query_service_entries`)
   *
   * This methods, is required because of "in_developement" feature.
   * It looks though services returned by query_service_entries, and asks
   * for similar services (same host and root_context), and refetches those.
   * The goes it to get a proper "enabled" state of those similar services.
   */
  void query_similar_service_entries(MySQLSession *session,
                                     VectorOfEntries *out,
                                     const DbService &similar_entry);

  std::string build_query(const std::string &table_name,
                          const entry::UniversalId id);
  std::string build_query(const DbService &similar_entry);

  std::set<entry::UniversalId> entries_fetched;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_CHANGES_DB_SERVICE_H_
