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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_MONITOR_DB_ACCESS_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_MONITOR_DB_ACCESS_H_

#include "mrs/database/query_changes_content_set.h"
#include "mrs/database/query_entries_content_set.h"
#include "mrs/interface/query_monitor_factory.h"

namespace mrs {
namespace database {

class DbAccess {
 public:
  using QueryMonitorFactory = mrs::interface::QueryMonitorFactory;
  using QueryState = mrs::database::QueryState;
  using QueryEntriesUrlHost = mrs::database::QueryEntriesUrlHost;
  using QueryEntriesDbService = mrs::database::QueryEntriesDbService;
  using QueryEntriesDbSchema = mrs::database::QueryEntriesDbSchema;
  using QueryEntriesDbObject = mrs::database::QueryEntriesDbObject;
  using QueryEntriesAuthApp = mrs::database::QueryEntriesAuthApp;
  using QueryEntriesContentFile = mrs::database::QueryEntriesContentFile;
  using QueryEntriesContentSet = mrs::database::QueryEntriesContentSet;

 public:
  DbAccess(mrs::interface::QueryFactory *query_factory,
           QueryMonitorFactory *query_monitor_factory,
           std::optional<uint64_t> router_id)
      : state{query_monitor_factory->create_turn_state_fetcher(router_id)},
        url_host{query_monitor_factory->create_url_host_fetcher()},
        db_service{query_monitor_factory->create_db_service_fetcher(router_id)},
        db_schema{query_monitor_factory->create_db_schema_fetcher()},
        db_object{
            query_monitor_factory->create_db_object_fetcher(query_factory)},
        authentication{query_monitor_factory->create_authentication_fetcher()},
        auth_user{query_monitor_factory->create_auth_user_monitor(0)},
        content_file{query_monitor_factory->create_content_file_fetcher()},
        content_set{query_monitor_factory->create_content_set_fetcher()},
        router_id_{router_id},
        query_monitor_factory_{query_monitor_factory},
        query_factory_{query_factory} {}

  void query(mysqlrouter::MySQLSession *session) {
    mysqlrouter::MySQLSession::Transaction transaction(session);
    state->query_state(session);
    url_host->query_entries(session);
    db_service->query_entries(session);
    db_schema->query_entries(session);
    db_object->query_entries(session);
    authentication->query_entries(session);
    auth_user->query_changed_ids(session);
    content_file->query_entries(session);
    content_set->query_entries(session);
    transaction.commit();
  }

  void update_access_factory_if_needed() {
    if (!fetcher_updated_) {
      //      state =
      //      query_monitor_factory_->create_turn_state_monitor(state.get());
      url_host = query_monitor_factory_->create_url_host_monitor(
          url_host->get_last_update());
      db_service = query_monitor_factory_->create_db_service_monitor(
          db_service->get_last_update(), router_id_);
      db_schema = query_monitor_factory_->create_db_schema_monitor(
          db_schema->get_last_update());
      db_object = query_monitor_factory_->create_db_object_monitor(
          query_factory_, db_object->get_last_update());
      authentication = query_monitor_factory_->create_authentication_monitor(
          authentication->get_last_update());
      content_file = query_monitor_factory_->create_content_file_monitor(
          content_file->get_last_update());
      content_set = query_monitor_factory_->create_content_set_monitor(
          content_set->get_last_update());
      fetcher_updated_ = true;
    }
  }

  const QueryState::DbState &get_state() const { return state->get_state(); }
  bool get_state_was_changed() const { return state->was_changed(); }
  const QueryEntriesUrlHost::VectorOfEntries &get_host_entries() const {
    return url_host->entries;
  }
  const QueryEntriesDbService::VectorOfEntries &get_service_entries() const {
    return db_service->entries;
  }
  const QueryEntriesDbSchema::VectorOfEntries &get_schema_entries() const {
    return db_schema->entries;
  }
  QueryEntriesDbObject::VectorOfPathEntries get_db_object_entries() const {
    return db_object->get_entries();
  }
  const QueryEntriesAuthApp::Entries &get_auth_app_entries() const {
    return authentication->get_entries();
  }
  const QueryChangesAuthUser::ChangedUsersIds &get_auth_user_changed_ids()
      const {
    return auth_user->get_changed_ids();
  }
  const QueryEntriesContentFile::VectorOfPaths &get_content_file_entries()
      const {
    return content_file->entries;
  }
  const QueryEntriesContentSet::VectorOfContentSets &get_content_set_entries()
      const {
    return content_set->entries;
  }

 private:
  std::unique_ptr<QueryState> state;
  std::unique_ptr<QueryEntriesUrlHost> url_host;
  std::unique_ptr<QueryEntriesDbService> db_service;
  std::unique_ptr<QueryEntriesDbSchema> db_schema;
  std::unique_ptr<QueryEntriesDbObject> db_object;
  std::unique_ptr<QueryEntriesAuthApp> authentication;
  std::unique_ptr<QueryChangesAuthUser> auth_user;
  std::unique_ptr<QueryEntriesContentFile> content_file;
  std::unique_ptr<QueryEntriesContentSet> content_set;

  bool fetcher_updated_{false};
  std::optional<uint64_t> router_id_;
  QueryMonitorFactory *query_monitor_factory_;
  mrs::interface::QueryFactory *query_factory_;
};

}  // namespace database
}  // namespace mrs

#endif /* ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_MONITOR_DB_ACCESS_H_ \
        */
