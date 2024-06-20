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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_DUALITY_VIEW_UPDATE_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_DUALITY_VIEW_UPDATE_H_

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
// #include "mrs/database/duality_view/delete.h"
#include "mrs/database/duality_view/errors.h"
#include "mrs/database/duality_view/insert.h"
#include "mrs/database/duality_view/select.h"
#include "mrs/database/entry/object.h"
#include "mrs/database/filter_object_generator.h"
#include "mrs/database/helper/object_row_ownership.h"
#include "mrs/database/helper/query.h"
#include "mysqlrouter/utils_sqlstring.h"

namespace mrs {
namespace database {
namespace dv {

class RowUpdateBase : public RowChangeOperation {
 public:
  RowUpdateBase(std::shared_ptr<DualityViewUpdater::Operation> parent,
                std::shared_ptr<Table> table, const PrimaryKeyColumnValues &pk,
                const ObjectRowOwnership &row_ownership);

  RowUpdateBase(std::shared_ptr<DualityViewUpdater::Operation> parent,
                std::shared_ptr<Table> table,
                const ObjectRowOwnership &row_ownership);

  void process_to_many(const ForeignKeyReference &fk,
                       JSONInputArray input) override;

  void run(MySQLSession *session) override;

 protected:
  virtual void do_update(MySQLSession *session) = 0;
};

class RowUpdate : public RowUpdateBase {
 public:
  RowUpdate(std::shared_ptr<DualityViewUpdater::Operation> parent,
            std::shared_ptr<Table> table, const PrimaryKeyColumnValues &pk,
            const ObjectRowOwnership &row_ownership);

  void on_value(const Column &column,
                const JSONInputObject::MemberReference &value) override;

  void on_value(const Column &column,
                const mysqlrouter::sqlstring &value) override;

  void on_no_value(const Column &column,
                   const JSONInputObject::MemberReference &) override;

  void on_referenced_row(
      const ForeignKeyReference &fk, const JSONInputObject &input,
      std::optional<PrimaryKeyColumnValues> child_pk) override;

  void set_error_if_not_found(bool flag) { error_if_not_found_ = flag; }

 protected:
  void do_update(MySQLSession *session) override;

  bool feed_columns(mysqlrouter::sqlstring &sql, bool is_null,
                    const char *separator) const;

  RowUpdate(std::shared_ptr<DualityViewUpdater::Operation> parent,
            std::shared_ptr<Table> table,
            const ObjectRowOwnership &row_ownership)
      : RowUpdateBase(parent, table, row_ownership) {}

 private:
  // whether to error if affected rows = 0
  bool error_if_not_found_ = false;

  mysqlrouter::sqlstring update_sql() const;
};

class RowUpdateReferencing : public RowUpdate {
 public:
  RowUpdateReferencing(std::shared_ptr<DualityViewUpdater::Operation> parent,
                       std::shared_ptr<Table> table,
                       const ObjectRowOwnership &row_ownership);

 protected:
  void do_update(MySQLSession *session) override;

  mysqlrouter::sqlstring update_sql() const;
};

class RowNoUpdateOrIgnore : public RowUpdateBase {
 public:
  RowNoUpdateOrIgnore(std::shared_ptr<DualityViewUpdater::Operation> parent,
                      std::shared_ptr<Table> table,
                      const PrimaryKeyColumnValues &pk,
                      const ObjectRowOwnership &row_ownership)
      : RowUpdateBase(parent, table, pk, row_ownership) {}

 protected:
  void do_update([[maybe_unused]] MySQLSession *session) override { /* no-op */
  }
};

class RowNoUpdateOrError : public RowUpdate {
 public:
  RowNoUpdateOrError(std::shared_ptr<DualityViewUpdater::Operation> parent,
                     std::shared_ptr<Table> table,
                     const PrimaryKeyColumnValues &pk,
                     const ObjectRowOwnership &row_ownership)
      : RowUpdate(parent, table, pk, row_ownership) {}

 protected:
  void do_update(MySQLSession *session) override;

 private:
  mysqlrouter::sqlstring update_sql() const;

  void on_row(const ResultRow &r) override;

  bool input_matches_row_;
};

std::shared_ptr<RowUpdate> make_row_update(
    std::shared_ptr<DualityViewUpdater::Operation> parent,
    std::shared_ptr<Table> table, const PrimaryKeyColumnValues &pk,
    const ObjectRowOwnership &row_ownership);

std::shared_ptr<RowNoUpdateOrIgnore> make_row_no_update_or_ignore(
    std::shared_ptr<DualityViewUpdater::Operation> parent,
    std::shared_ptr<Table> table, const PrimaryKeyColumnValues &pk,
    const ObjectRowOwnership &row_ownership);

std::shared_ptr<RowNoUpdateOrError> make_row_no_update_or_error(
    std::shared_ptr<DualityViewUpdater::Operation> parent,
    std::shared_ptr<Table> table, const PrimaryKeyColumnValues &pk,
    const ObjectRowOwnership &row_ownership);

}  // namespace dv
}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_DUALITY_VIEW_UPDATE_H_
