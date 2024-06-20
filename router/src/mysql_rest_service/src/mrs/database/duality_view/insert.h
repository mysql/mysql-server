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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_DUALITY_VIEW_INSERT_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_DUALITY_VIEW_INSERT_H_

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

class RowInsert : public RowChangeOperation {
 protected:
  friend class RowUpdate;

 public:
  RowInsert(std::shared_ptr<Operation> parent, std::shared_ptr<Table> table,
            const ObjectRowOwnership &row_ownership, bool upsert)
      : RowChangeOperation(parent, table, row_ownership), upsert_(upsert) {}

  void run(MySQLSession *session) override;

  virtual void on_pre_insert(MySQLSession *) {}
  virtual void on_post_insert(MySQLSession *) {}

  void process(JSONInputObject input) override;

 protected:
  void process_to_many(const ForeignKeyReference &fk,
                       JSONInputArray input) override;

  std::shared_ptr<RowChangeOperation> add_dummy_update_referenced_from_this(
      const ForeignKeyReference &fk, const PrimaryKeyColumnValues &pk) override;

 protected:
  mysqlrouter::sqlstring insert_sql() const;

  void resolve_references_to_this();
  void resolve_references_from_this();

 protected:
  bool references_to_this_resolved_ = false;
  bool references_from_this_resolved_ = false;
  bool upsert_ = false;
};

class AutoIncRowInsert : public RowInsert {
 public:
  AutoIncRowInsert(std::shared_ptr<Operation> parent,
                   std::shared_ptr<Table> table, const Column *generated_column,
                   const ObjectRowOwnership &row_ownership, bool upsert)
      : RowInsert(parent, table, row_ownership, upsert),
        gen_id_column_(generated_column) {
    assert(gen_id_column_);
  }

  void on_post_insert(MySQLSession *session) override;

 private:
  const Column *gen_id_column_;
};

class ReverseUuidRowInsert : public RowInsert {
 public:
  ReverseUuidRowInsert(std::shared_ptr<Operation> parent,
                       std::shared_ptr<Table> table,
                       const Column *generated_column,
                       const ObjectRowOwnership &row_ownership, bool upsert)
      : RowInsert(parent, table, row_ownership, upsert),
        gen_id_column_(generated_column) {
    assert(gen_id_column_);
  }

  void on_pre_insert(MySQLSession *session) override;

  static void set_generate_uuid(
      std::function<mysqlrouter::sqlstring(MySQLSession *)> fn);

 private:
  const Column *gen_id_column_;
};

std::shared_ptr<RowInsert> make_row_insert(
    std::shared_ptr<DualityViewUpdater::Operation> parent,
    std::shared_ptr<Table> table, const ObjectRowOwnership &row_ownership);

std::shared_ptr<RowInsert> make_row_upsert(
    std::shared_ptr<DualityViewUpdater::Operation> parent,
    std::shared_ptr<Table> table, const ObjectRowOwnership &row_ownership);

}  // namespace dv
}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_DUALITY_VIEW_INSERT_H_
