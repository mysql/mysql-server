/*
 * Copyright (c) 2023, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_QUERY_REST_TABLE_UPDATER_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_QUERY_REST_TABLE_UPDATER_H_

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "mrs/database/entry/object.h"
#include "mrs/database/filter_object_generator.h"
#include "mrs/database/helper/object_row_ownership.h"
#include "mrs/database/helper/query.h"
#include "mrs/database/json_mapper/errors.h"
#include "mrs/database/json_mapper/select.h"
#include "mrs/database/query_uuid.h"
#include "mrs/interface/query_retry.h"
#include "mysqlrouter/utils_sqlstring.h"

namespace mrs {
namespace database {
namespace dv {

class JsonMappingUpdater : public Query {
 public:
  using Object = entry::Object;

  explicit JsonMappingUpdater(
      std::shared_ptr<Object> view,
      const ObjectRowOwnership &row_ownership_info = {});

  void check(const rapidjson::Document &doc, bool for_update = false) const;

  PrimaryKeyColumnValues insert(MySQLSession *session,
                                const rapidjson::Document &doc);

  PrimaryKeyColumnValues update(MySQLSession *session,
                                const PrimaryKeyColumnValues &pk_values,
                                const rapidjson::Document &doc,
                                bool upsert = false);

  uint64_t delete_(MySQLSession *session,
                   const PrimaryKeyColumnValues &pk_values);

  uint64_t delete_(MySQLSession *session, const FilterObjectGenerator &filter);

  bool has_references() const;

  const ObjectRowOwnership &row_ownership_info() const {
    return m_row_ownership_info;
  }

  size_t affected() const { return m_affected; }

  class Operation;

 private:
  std::shared_ptr<Object> view_;
  ObjectRowOwnership m_row_ownership_info;
  size_t m_affected = 0;

 private:
  std::string select_one(MySQLSession *session,
                         const PrimaryKeyColumnValues &pk_values,
                         bool &is_owned, RowLockType lock_rows) const;

  void check_etag(const std::string &original_doc,
                  const rapidjson::Document &new_doc) const;
};

}  // namespace dv
}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_QUERY_REST_TABLE_UPDATER_H_
