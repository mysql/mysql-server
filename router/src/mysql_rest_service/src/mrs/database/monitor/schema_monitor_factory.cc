/*
  Copyright (c) 2024, Oracle and/or its affiliates.

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

#include "mrs/interface/schema_monitor_factory.h"

#include "mrs/database/query_changes_auth_app.h"
#include "mrs/database/query_changes_content_file.h"
#include "mrs/database/query_changes_db_object.h"
#include "mrs/database/query_changes_state.h"

namespace mrs {
namespace database {

namespace v2 {

class SchemaMonitorFactory : public mrs::interface::SchemaMonitorFactory {
 public:
  virtual ~SchemaMonitorFactory() = default;

  std::unique_ptr<database::QueryState> create_turn_state_fetcher() override {
    return std::make_unique<QueryState>();
  }

  std::unique_ptr<database::QueryEntryDbObject> create_route_fetcher()
      override {
    return std::make_unique<QueryEntryDbObject>();
  }

  std::unique_ptr<database::QueryEntriesAuthApp> create_authentication_fetcher()
      override {
    return std::make_unique<v2::QueryEntriesAuthApp>();
  }

  std::unique_ptr<database::QueryEntriesContentFile>
  create_content_file_fetcher() override {
    return std::make_unique<QueryEntriesContentFile>();
  }

  std::unique_ptr<database::QueryState> create_turn_state_monitor(
      database::QueryState *state) override {
    return std::make_unique<QueryChangesState>(state);
  }

  std::unique_ptr<database::QueryEntryDbObject> create_route_monitor(
      const uint64_t last_audit_log_id) override {
    return std::make_unique<QueryChangesDbObject>(last_audit_log_id);
  }

  std::unique_ptr<database::QueryEntriesAuthApp> create_authentication_monitor(
      const uint64_t last_audit_log_id) override {
    return std::make_unique<QueryChangesAuthApp<v2::QueryEntriesAuthApp>>(
        last_audit_log_id);
  }

  std::unique_ptr<database::QueryEntriesContentFile>
  create_content_file_monitor(const uint64_t last_audit_log_id) override {
    return std::make_unique<QueryChangesContentFile>(last_audit_log_id);
  }
};

}  // namespace v2

namespace v3 {

class SchemaMonitorFactory : public mrs::interface::SchemaMonitorFactory {
 public:
  virtual ~SchemaMonitorFactory() = default;

  std::unique_ptr<database::QueryState> create_turn_state_fetcher() override {
    return std::make_unique<QueryState>();
  }

  std::unique_ptr<database::QueryEntryDbObject> create_route_fetcher()
      override {
    return std::make_unique<QueryEntryDbObject>();
  }

  std::unique_ptr<database::QueryEntriesAuthApp> create_authentication_fetcher()
      override {
    return std::make_unique<v3::QueryEntriesAuthApp>();
  }

  std::unique_ptr<database::QueryEntriesContentFile>
  create_content_file_fetcher() override {
    return std::make_unique<QueryEntriesContentFile>();
  }

  std::unique_ptr<database::QueryState> create_turn_state_monitor(
      database::QueryState *state) override {
    return std::make_unique<QueryChangesState>(state);
  }

  std::unique_ptr<database::QueryEntryDbObject> create_route_monitor(
      const uint64_t last_audit_log_id) override {
    return std::make_unique<QueryChangesDbObject>(last_audit_log_id);
  }

  std::unique_ptr<database::QueryEntriesAuthApp> create_authentication_monitor(
      const uint64_t last_audit_log_id) override {
    return std::make_unique<QueryChangesAuthApp<v3::QueryEntriesAuthApp>>(
        last_audit_log_id);
  }

  std::unique_ptr<database::QueryEntriesContentFile>
  create_content_file_monitor(const uint64_t last_audit_log_id) override {
    return std::make_unique<QueryChangesContentFile>(last_audit_log_id);
  }
};

}  // namespace v3

std::unique_ptr<mrs::interface::SchemaMonitorFactory>
create_scheme_monitor_factory(
    mrs::interface::SupportedMrsVersion scheme_version) {
  switch (scheme_version) {
    case mrs::interface::kSupportedMrsVersion_2:
      return std::make_unique<mrs::database::v2::SchemaMonitorFactory>();
    case mrs::interface::kSupportedMrsVersion_3:
      return std::make_unique<mrs::database::v3::SchemaMonitorFactory>();
    default:
      assert(false && "Unsupported MRS scheme version.");
  }
  return {};
}

}  // namespace database
}  // namespace mrs
