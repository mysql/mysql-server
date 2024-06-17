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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_INTERFACE_SCHEMA_MONITOR_FACTORY_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_INTERFACE_SCHEMA_MONITOR_FACTORY_H_

#include "mrs/database/query_entries_auth_app.h"
#include "mrs/database/query_entries_content_file.h"
#include "mrs/database/query_entries_db_object.h"
#include "mrs/database/query_state.h"

namespace mrs {
namespace interface {

class SchemaMonitorFactory {
 public:
  virtual ~SchemaMonitorFactory() = default;

  virtual std::unique_ptr<database::QueryState> create_turn_state_fetcher() = 0;
  virtual std::unique_ptr<database::QueryEntryDbObject>
  create_route_fetcher() = 0;
  virtual std::unique_ptr<database::QueryEntriesAuthApp>
  create_authentication_fetcher() = 0;
  virtual std::unique_ptr<database::QueryEntriesContentFile>
  create_content_file_fetcher() = 0;

  virtual std::unique_ptr<database::QueryState> create_turn_state_monitor(
      database::QueryState *state) = 0;
  virtual std::unique_ptr<database::QueryEntryDbObject> create_route_monitor(
      const uint64_t last_audit_log_id) = 0;
  virtual std::unique_ptr<database::QueryEntriesAuthApp>
  create_authentication_monitor(const uint64_t last_audit_log_id) = 0;
  virtual std::unique_ptr<database::QueryEntriesContentFile>
  create_content_file_monitor(const uint64_t last_audit_log_id) = 0;
};

enum SupportedMrsMetadataVersion {
  kSupportedMrsMetadataVersion_2,
  kSupportedMrsMetadataVersion_3
};

}  // namespace interface
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_INTERFACE_SCHEMA_MONITOR_FACTORY_H_
