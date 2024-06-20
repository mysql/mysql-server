/*
 * Copyright (c) 2023, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms, as
 * designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_DUALITY_VIEW_CHANGE_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_DUALITY_VIEW_CHANGE_H_

#include "router/src/mysql_rest_service/src/mrs/database/query_rest_table_updater.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "helper/json/rapid_json_to_text.h"
#include "helper/json/to_sqlstring.h"
#include "mrs/database/duality_view/common.h"
#include "mrs/database/duality_view/errors.h"
#include "mrs/database/duality_view/json_input.h"
#include "mrs/database/duality_view/select.h"
#include "mrs/interface/rest_error.h"
#include "mysqld_error.h"

namespace mrs {
namespace database {
namespace dv {

class RowInsert;
class RowDeleteReferencing;

// merge with RowChange and rename to RowChange
class DualityViewUpdater::Operation
    : public QueryLog,
      public std::enable_shared_from_this<DualityViewUpdater::Operation> {
 protected:
  size_t affected_ = 0;

 public:
  std::weak_ptr<Operation> parent_;
  std::shared_ptr<Table> table_;
  const ObjectRowOwnership &row_ownership_;

 public:
  Operation(std::shared_ptr<Operation> parent, std::shared_ptr<Table> table,
            const ObjectRowOwnership &row_ownership)
      : parent_(parent), table_(table), row_ownership_(row_ownership) {}

  Operation(std::shared_ptr<Operation> parent, std::shared_ptr<Table> table,
            const PrimaryKeyColumnValues &pk,
            const ObjectRowOwnership &row_ownership)
      : parent_(parent),
        table_(table),
        row_ownership_(row_ownership),
        pk_(pk) {}

  virtual const PrimaryKeyColumnValues &primary_key() const { return pk_; }

  virtual void run(MySQLSession *session) = 0;

  const std::shared_ptr<Table> &table() const { return table_; }

  std::shared_ptr<Operation> parent() const {
    if (!parent_.expired()) return parent_.lock();
    return {};
  }

  bool is_root() const { return parent_.expired() == true; }

  size_t affected() const { return affected_; }

  virtual void process(JSONInputObject input);

  virtual void on_parent_pk_resolved(
      const PrimaryKeyColumnValues & /* parent_pk */) {}

  virtual void append_match_condition(mysqlrouter::sqlstring &sql) const;

 protected:
  virtual void on_value(const Column & /* column */,
                        const JSONInputObject::MemberReference & /* value */) {}

  virtual void on_no_value(const Column & /* column */,
                           const JSONInputObject::MemberReference &) {}

  virtual void process_to_many(const ForeignKeyReference &ref,
                               JSONInputArray input) = 0;

  virtual void process_to_one(const ForeignKeyReference &ref,
                              JSONInputObject input) = 0;

 protected:
  bool add_row_owner_check(mysqlrouter::sqlstring *sql,
                           bool qualify_table) const {
    if (row_ownership_.enabled()) {
      sql->append_preformatted_sep(
          " ", qualify_table
                   ? row_ownership_.owner_check_expr(table_->table_alias)
                   : row_ownership_.owner_check_expr());
      return true;
    }
    return false;
  }

  mysqlrouter::sqlstring join_to_parent(
      std::vector<std::shared_ptr<DualityViewUpdater::Operation>> *parents)
      const;

  void execute(MySQLSession *session) override {
    QueryLog::execute(session);
    affected_ = session->affected_rows();
  }

 protected:
  PrimaryKeyColumnValues pk_;
};

//

class RowChangeOperation : public DualityViewUpdater::Operation {
 protected:
  RowChangeOperation(std::shared_ptr<Operation> parent,
                     std::shared_ptr<Table> table,
                     const ObjectRowOwnership &row_ownership)
      : Operation(parent, table, row_ownership) {}

  RowChangeOperation(std::shared_ptr<Operation> parent,
                     std::shared_ptr<Table> table,
                     const PrimaryKeyColumnValues &pk,
                     const ObjectRowOwnership &row_ownership)
      : Operation(parent, table, pk, row_ownership) {}

 public:
  bool empty() const { return columns_.empty(); }

  void process_to_one(const ForeignKeyReference &ref,
                      JSONInputObject input) override;

 public:
  std::shared_ptr<RowChangeOperation> add_update_referenced_from_this(
      const ForeignKeyReference &fk, const PrimaryKeyColumnValues &pk);

  virtual std::shared_ptr<RowChangeOperation>
  add_dummy_update_referenced_from_this(const ForeignKeyReference &fk,
                                        const PrimaryKeyColumnValues &pk);

  std::shared_ptr<RowChangeOperation> add_update_referencing_this(
      const ForeignKeyReference &fk, const PrimaryKeyColumnValues &pk,
      bool error_if_not_found = true);

  std::shared_ptr<RowChangeOperation> add_clear_all_referencing_this(
      const ForeignKeyReference &fk);

  std::shared_ptr<RowChangeOperation> add_insert_referenced_from_this(
      const ForeignKeyReference &fk);

  std::shared_ptr<RowChangeOperation> add_insert_referencing_this(
      const ForeignKeyReference &fk);

  std::shared_ptr<RowChangeOperation> add_upsert_referencing_this(
      const ForeignKeyReference &fk);

  std::shared_ptr<RowDeleteReferencing> add_delete_referencing_this(
      const ForeignKeyReference &fk, const PrimaryKeyColumnValues &pk);

  std::shared_ptr<RowDeleteReferencing> add_delete_all_referencing_this(
      const ForeignKeyReference &fk);

  void on_value(const Column &column,
                const JSONInputObject::MemberReference &value) override;

  virtual void on_value(const Column &column,
                        const mysqlrouter::sqlstring &value);

  void on_no_value(const Column &column,
                   const JSONInputObject::MemberReference &) override;

  // FK reference from this table to the PK of another table
  virtual void on_referenced_row(
      const ForeignKeyReference &fk, const JSONInputObject &input,
      std::optional<PrimaryKeyColumnValues> child_pk);

  // FK reference from another table to the PK of this
  void on_referencing_row(const ForeignKeyReference &fk,
                          std::shared_ptr<RowChangeOperation> ref_op);

  virtual void set_column_sql_value(const Column &column,
                                    const mysqlrouter::sqlstring &value);

  void set_column_value(const Column &column,
                        const mysqlrouter::sqlstring &value);

  void on_parent_pk_resolved(const PrimaryKeyColumnValues &parent_pk) override;

  void resolve_fk_to_parent(const ForeignKeyReference &fk,
                            const PrimaryKeyColumnValues &parent_pk);

 protected:
  virtual void run_before(std::shared_ptr<Operation> op) {
    before_.push_back(op);
  }

  void cancel_before(std::shared_ptr<Operation> op) {
    auto it = std::find(before_.begin(), before_.end(), op);
    assert(it != before_.end());
    before_.erase(it);
  }

  virtual void run_after(std::shared_ptr<Operation> op) {
    after_.push_back(op);
  }

 protected:
  std::vector<mysqlrouter::sqlstring> columns_;
  std::vector<mysqlrouter::sqlstring> values_;
  std::vector<bool> not_updatable_;
  std::optional<ForeignKeyReference> pending_fk_to_parent_;

  std::list<std::shared_ptr<Operation>> before_;
  std::list<std::shared_ptr<Operation>> after_;
};

}  // namespace dv
}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_DUALITY_VIEW_CHANGE_H_
