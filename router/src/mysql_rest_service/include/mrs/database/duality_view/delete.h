/*
 * Copyright (c) 2024, Oracle and/or its affiliates.
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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_DATABASE_DUALITY_VIEW_DELETE_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_DATABASE_DUALITY_VIEW_DELETE_H_

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
#include "mrs/database/duality_view/change.h"
#include "mrs/database/duality_view/common.h"
#include "mrs/database/duality_view/errors.h"
#include "mrs/database/duality_view/select.h"
#include "mrs/database/entry/object.h"
#include "mrs/database/filter_object_generator.h"
#include "mrs/database/helper/object_row_ownership.h"
#include "mrs/database/helper/query.h"
#include "mysqlrouter/utils_sqlstring.h"

namespace mrs {
namespace database {
namespace dv {

class RowDeleteBase : public RowChangeOperation {
 protected:
  RowDeleteBase(std::shared_ptr<Operation> parent, std::shared_ptr<Table> table,
                const ObjectRowOwnership &row_ownership);

  RowDeleteBase(std::shared_ptr<Operation> parent, std::shared_ptr<Table> table,
                const PrimaryKeyColumnValues &pk_values,
                const ObjectRowOwnership &row_ownership);

 public:
  void run(MySQLSession *session) override;

  void process_to_many(const ForeignKeyReference &ref,
                       JSONInputArray input) override;

  void process_to_one(const ForeignKeyReference &, JSONInputObject) override {
    // no-op
  }

 protected:
  virtual void do_delete(MySQLSession *session);
  virtual mysqlrouter::sqlstring delete_sql() const = 0;

  bool has_undeletable_fks_ = false;
};

class RowDelete : public RowDeleteBase {
 public:
  RowDelete(std::shared_ptr<Table> table,
            const PrimaryKeyColumnValues &pk_values,
            const ObjectRowOwnership &row_ownership)
      : RowDeleteBase({}, table, pk_values, row_ownership) {}

  void process(JSONInputObject input) override;

 private:
  mysqlrouter::sqlstring delete_sql() const override;
};

class RowDeleteMany : public RowDeleteBase {
 public:
  RowDeleteMany(std::shared_ptr<Table> table, mysqlrouter::sqlstring filter,
                const ObjectRowOwnership &row_ownership)
      : RowDeleteBase({}, table, row_ownership), filter_(std::move(filter)) {}

  void process(JSONInputObject input) override;

  void append_match_condition(mysqlrouter::sqlstring &sql) const override;

 private:
  mysqlrouter::sqlstring delete_sql() const override;

  mysqlrouter::sqlstring filter_;
};

class RowDeleteReferencing : public RowDeleteBase {
 public:
  RowDeleteReferencing(std::shared_ptr<Operation> parent,
                       std::shared_ptr<Table> owning_table,
                       const ObjectRowOwnership &row_ownership)
      : RowDeleteBase(parent, owning_table, row_ownership) {}

  RowDeleteReferencing(std::shared_ptr<Operation> parent,
                       std::shared_ptr<Table> owning_table,
                       const PrimaryKeyColumnValues &pk_values,
                       const ObjectRowOwnership &row_ownership)
      : RowDeleteBase(parent, owning_table, pk_values, row_ownership) {}

  void delete_rows(std::vector<PrimaryKeyColumnValues> rows);

 protected:
  mysqlrouter::sqlstring delete_sql() const override;

 private:
  std::vector<PrimaryKeyColumnValues> rows_to_delete_;
};

}  // namespace dv
}  // namespace database
}  // namespace mrs
#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_DATABASE_DUALITY_VIEW_DELETE_H_
