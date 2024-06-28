/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_QUERY_ENTRIES_OBJECT_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_QUERY_ENTRIES_OBJECT_H_

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "mrs/database/entry/object.h"
#include "mrs/database/helper/query.h"

namespace mrs {
namespace database {

class QueryEntryObject : protected QueryLog {
 public:
  using Object = entry::Object;
  using UniversalId = entry::UniversalId;

 public:
  virtual void query_entries(MySQLSession *session,
                             const std::string &schema_name,
                             const std::string &object_name,
                             const UniversalId &db_object_id) = 0;

  std::shared_ptr<Object> object;
};

namespace v2 {
class QueryEntryObject : public mrs::database::QueryEntryObject {
 public:
  void query_entries(MySQLSession *session, const std::string &schema_name,
                     const std::string &object_name,
                     const UniversalId &db_object_id) override;

 protected:
  virtual UniversalId query_object(MySQLSession *session,
                                   const UniversalId &db_object_id,
                                   Object *object);
  virtual void set_query_object_reference(const entry::UniversalId &object_id);

  void on_row(const ResultRow &r) override;

  virtual void on_reference_row(const ResultRow &r);
  virtual void on_field_row(const ResultRow &r);

  bool m_loading_references;

  std::map<entry::UniversalId, std::shared_ptr<entry::Table>> m_tables;
  std::map<entry::UniversalId, std::shared_ptr<entry::Object>> m_objects;
  int m_alias_count = 0;
};

}  // namespace v2

namespace v3 {

class QueryEntryObject : public v2::QueryEntryObject {
 public:
  void query_entries(mysqlrouter::MySQLSession *session,
                     const std::string &schema_name,
                     const std::string &object_name,
                     const UniversalId &db_object_id) override;

 private:
  void on_reference_row(const ResultRow &r) override;
  UniversalId query_object(MySQLSession *session,
                           const UniversalId &db_object_id,
                           Object *object) override;
  void set_query_object_reference(const entry::UniversalId &object_id) override;
};

}  // namespace v3

entry::ColumnType column_datatype_to_type(const std::string &datatype);

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_QUERY_ENTRIES_OBJECT_H_
