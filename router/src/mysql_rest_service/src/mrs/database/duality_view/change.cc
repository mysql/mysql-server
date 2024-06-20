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

#include "router/src/mysql_rest_service/src/mrs/database/duality_view/change.h"

#include <utility>
#include "mrs/database/duality_view/delete.h"
#include "mrs/database/duality_view/insert.h"
#include "mrs/database/duality_view/select.h"
#include "mrs/database/duality_view/update.h"
#include "mrs/interface/rest_error.h"
#include "mysqld_error.h"
#include "mysqlrouter/base64.h"

namespace mrs {
namespace database {
namespace dv {

using interface::RestError;

using namespace helper::json::sql;

using MySQLSession = mysqlrouter::MySQLSession;

void DualityViewUpdater::Operation::process(JSONInputObject input) {
  JSONInputObject::MemberReference member;

  // go through PKs in the first iteration
  for (int pk_only = 1; pk_only >= 0; --pk_only) {
    for (const auto &field : table_->fields) {
      if (!field->enabled) continue;

      if (auto column = std::dynamic_pointer_cast<Column>(field)) {
        if ((static_cast<bool>(pk_only) != column->is_primary)) continue;

        if (input.has_new()) member = input.find(field->name.c_str());

        // regular object field
        if (!member.has_new()) {
          on_no_value(*column, member);
        } else {
          on_value(*column, member);
        }
      } else if (auto ref =
                     std::dynamic_pointer_cast<ForeignKeyReference>(field)) {
        if (pk_only) continue;

        if (ref->unnest) {
          if (ref->to_many) {
            // not supposed to get here because unnested 1:n are not updatable
            throw std::logic_error("internal error");
          } else {
            process_to_one(*ref, input);
          }
        } else {
          if (input.has_new()) member = input.find(field->name.c_str());

          // omitted value on 1:1 ref is same as setting to {}
          // on 1:n ref, it's the same as setting to []
          if (ref->to_many) {
            process_to_many(
                *ref, make_input_array(member, table_->table, field->name));
          } else {
            process_to_one(
                *ref, make_input_object(member, table_->table, field->name));
          }
        }
      }
    }
  }
}

mysqlrouter::sqlstring DualityViewUpdater::Operation::join_to_parent(
    std::vector<std::shared_ptr<DualityViewUpdater::Operation>> *parents)
    const {
  mysqlrouter::sqlstring where;

  auto op = shared_from_this();
  while (!op->parent_.expired()) {
    auto parent = op->parent_.lock();

    const auto &ref = op->table()->get_reference_to_parent(*parent->table());

    where.append_preformatted_sep(
        " AND ", format_join_where_expr(*parent->table(), ref));

    if (parents) parents->push_back(parent);
    op = parent;
  }

  return where;
}

void DualityViewUpdater::Operation::append_match_condition(
    mysqlrouter::sqlstring &sql) const {
  mysqlrouter::sqlstring where;
  auto cont = add_row_owner_check(&where, true);

  auto pk = format_where_expr(*table_, primary_key(), row_ownership_.enabled());
  if (!pk.is_empty()) where.append_preformatted_sep(cont ? " AND " : " ", pk);

  sql.append_preformatted(where);
}

void RowChangeOperation::process_to_one(const ForeignKeyReference &fk,
                                        JSONInputObject input) {
  assert(!fk.to_many);

  if (!input.has_new() || input.new_empty()) {  // omitted child
    on_referenced_row(fk, input, {});
    return;
  }

  bool require_pk = true;
  if (fk.unnest) require_pk = false;

  // throws if PK is missing
  auto pk = ref_primary_key(fk, input.new_object(), require_pk);
  if (fk.unnest && pk.empty()) {  // omitted unnested child
    on_referenced_row(fk, input, {});
    return;
  }

  // set FK to the child object (which must exist)
  on_referenced_row(fk, input, pk);

  // update the child object itself (inserts not allowed for 1:1)
  if (fk.ref_table->with_update_any_column()) {
    auto ref_update = add_update_referenced_from_this(fk, pk);

    // recursively update the children of the child object
    ref_update->process(std::move(input));
  } else {
    // On Insert:
    //   if the referenced table is NOUPDATE, then values should not be updated,
    // but nested references should still be followed
    // On Update:
    //   if the references table is NOUPDATE, then values must match or an error
    // is thrown
    auto ref_update = add_dummy_update_referenced_from_this(fk, pk);

    // recursively update the children of the child object
    ref_update->process(std::move(input));
  }
}

std::shared_ptr<RowChangeOperation>
RowChangeOperation::add_update_referenced_from_this(
    const ForeignKeyReference &fk, const PrimaryKeyColumnValues &pk) {
  auto update =
      make_row_update(shared_from_this(), fk.ref_table, pk, row_ownership_);

  run_after(update);

  return update;
}

std::shared_ptr<RowChangeOperation>
RowChangeOperation::add_dummy_update_referenced_from_this(
    const ForeignKeyReference &fk, const PrimaryKeyColumnValues &pk) {
  auto update = make_row_no_update_or_error(shared_from_this(), fk.ref_table,
                                            pk, row_ownership_);

  run_after(update);

  return update;
}

std::shared_ptr<RowChangeOperation>
RowChangeOperation::add_update_referencing_this(
    const ForeignKeyReference &fk, const PrimaryKeyColumnValues &pk,
    bool error_if_not_found) {
  auto update =
      make_row_update(shared_from_this(), fk.ref_table, pk, row_ownership_);

  update->set_error_if_not_found(error_if_not_found);

  run_after(update);

  return update;
}

std::shared_ptr<RowChangeOperation>
RowChangeOperation::add_clear_all_referencing_this(
    const ForeignKeyReference &fk) {
  static mysqlrouter::sqlstring k_null("NULL");

  auto update = std::make_shared<RowUpdateReferencing>(
      shared_from_this(), fk.ref_table, row_ownership_);

  // UPDATE SET fk_column=NULL
  for (auto col : fk.column_mapping) {
    auto column = fk.ref_table->get_column(col.second);

    update->on_value(*column, k_null);
  }

  run_before(update);

  return update;
}

std::shared_ptr<RowChangeOperation>
RowChangeOperation::add_insert_referenced_from_this(
    const ForeignKeyReference &fk) {
  auto insert =
      make_row_insert(shared_from_this(), fk.ref_table, row_ownership_);

  run_after(insert);

  return insert;
}

std::shared_ptr<RowChangeOperation>
RowChangeOperation::add_insert_referencing_this(const ForeignKeyReference &fk) {
  auto insert =
      make_row_insert(shared_from_this(), fk.ref_table, row_ownership_);
  run_after(insert);

  return insert;
}

std::shared_ptr<RowChangeOperation>
RowChangeOperation::add_upsert_referencing_this(const ForeignKeyReference &fk) {
  auto upsert =
      make_row_upsert(shared_from_this(), fk.ref_table, row_ownership_);

  run_after(upsert);

  return upsert;
}

std::shared_ptr<RowDeleteReferencing>
RowChangeOperation::add_delete_referencing_this(
    const ForeignKeyReference &fk, const PrimaryKeyColumnValues &pks) {
  auto deletion = std::make_shared<RowDeleteReferencing>(
      shared_from_this(), fk.ref_table, pks, row_ownership_);

  // must run before new rows are added
  run_before(deletion);

  return deletion;
}

std::shared_ptr<RowDeleteReferencing>
RowChangeOperation::add_delete_all_referencing_this(
    const ForeignKeyReference &fk) {
  auto deletion = std::make_shared<RowDeleteReferencing>(
      shared_from_this(), fk.ref_table, row_ownership_);

  // must run before new rows are added
  run_before(deletion);

  return deletion;
}

void RowChangeOperation::on_value(
    const Column &column, const JSONInputObject::MemberReference &value) {
  mysqlrouter::sqlstring tmp("?");

  if (value.new_value().IsNull()) {
    tmp << value.new_value();
  } else if (column.type == entry::ColumnType::JSON ||
             column.type == entry::ColumnType::GEOMETRY) {
    tmp << helper::json::to_string(value.new_value());
  } else if (column.type == entry::ColumnType::BINARY &&
             value.new_value().IsString()) {
    tmp << (mysqlrouter::sqlstring("FROM_BASE64(?)")
            << value.new_value().GetString());
  } else if (value.new_value().IsBool()) {
    tmp << value.new_value().GetBool();
  } else {
    tmp << value.new_value();
  }

  on_value(column, tmp);
}

void RowChangeOperation::on_value(const Column &column,
                                  const mysqlrouter::sqlstring &value) {
  if (row_ownership_.is_owner_id(*table_, column)) {
    if (column.is_primary)
      pk_[column.column_name] = row_ownership_.owner_user_id();
    return;
  }

  set_column_value(column, value);
}

void RowChangeOperation::on_no_value(const Column &column,
                                     const JSONInputObject::MemberReference &) {
  if (row_ownership_.is_owner_id(*table_, column)) {
    if (column.is_primary)
      pk_[column.column_name] = row_ownership_.owner_user_id();
    return;
  }
}

// FK reference from this table to the PK of another table
void RowChangeOperation::on_referenced_row(
    const ForeignKeyReference &fk,
    [[maybe_unused]] const JSONInputObject &input,
    std::optional<PrimaryKeyColumnValues> child_pk) {
  assert(!fk.to_many);
  assert(!child_pk.has_value() || child_pk->size() == fk.column_mapping.size());

  if (child_pk.has_value()) {
    for (auto col : fk.column_mapping) {
      auto it = child_pk->find(col.second);

      if (it == child_pk->end()) {
        // probably invalid metadata
        throw std::runtime_error(
            "Error processing primary key of referenced object (column " +
            col.second + ")");
      }

      const auto column = table_->get_column(col.first);

      on_value(*column, it->second);
    }
  } else {
    static mysqlrouter::sqlstring k_null("NULL");

    for (auto col : fk.column_mapping) {
      const auto column = table_->get_column(col.first);

      on_value(*column, k_null);
    }
  }
}

// FK reference from another table to the PK of this
void RowChangeOperation::on_referencing_row(
    const ForeignKeyReference &fk, std::shared_ptr<RowChangeOperation> ref_op) {
  assert(fk.to_many);

  if (pk_.empty()) {
    ref_op->pending_fk_to_parent_ = fk;
  } else {
    ref_op->resolve_fk_to_parent(fk, pk_);
  }
}

void RowChangeOperation::on_parent_pk_resolved(
    const PrimaryKeyColumnValues &parent_pk) {
  if (pending_fk_to_parent_) {
    resolve_fk_to_parent(*pending_fk_to_parent_, parent_pk);
    pending_fk_to_parent_ = {};
  }
}

void RowChangeOperation::resolve_fk_to_parent(
    const ForeignKeyReference &fk, const PrimaryKeyColumnValues &parent_pk) {
  for (auto col : fk.column_mapping) {
    auto it = parent_pk.find(col.first);
    if (it == parent_pk.end()) {
      // probably invalid metadata
      throw std::runtime_error(
          "Error processing primary key of referencing object (column " +
          col.second + ")");
    }
    auto column = fk.ref_table->get_column(col.second);
    set_column_sql_value(*column, it->second);
  }
}

void RowChangeOperation::set_column_sql_value(
    const Column &column, const mysqlrouter::sqlstring &value) {
  mysqlrouter::sqlstring tmp("!");
  tmp << column.column_name;
  if (auto it =
          std::find_if(columns_.begin(), columns_.end(),
                       [tmp](const auto &c) { return c.str() == tmp.str(); });
      it != columns_.end()) {
    size_t index = it - columns_.begin();
    columns_[index] = std::move(tmp);
    values_[index] = value;

    not_updatable_[index] =
        (column.is_primary || row_ownership_.is_owner_id(*table_, column));
  } else {
    columns_.emplace_back(std::move(tmp));
    values_.push_back(value);

    not_updatable_.push_back(column.is_primary ||
                             row_ownership_.is_owner_id(*table_, column));
  }

  if (column.is_primary) {
    pk_[column.column_name] = value;
  }
}

void RowChangeOperation::set_column_value(const Column &column,
                                          const mysqlrouter::sqlstring &value) {
  if (value.str() == "NULL") {
    set_column_sql_value(column, value);
  } else if (column.type == entry::ColumnType::BINARY) {
    set_column_sql_value(column, value);
  } else if (column.type == entry::ColumnType::GEOMETRY) {
    set_column_sql_value(column,
                         mysqlrouter::sqlstring("ST_GeomFromGeoJSON(?, 1, ?)")
                             << value << column.srid);
  } else {
    set_column_sql_value(column, value);
  }
}

}  // namespace dv
}  // namespace database
}  // namespace mrs
