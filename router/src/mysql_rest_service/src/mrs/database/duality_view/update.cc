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

#include "mrs/database/duality_view/update.h"
#include "mrs/database/duality_view/delete.h"
#include "mrs/database/duality_view/errors.h"
#include "mrs/database/duality_view/select.h"

namespace mrs {
namespace database {
namespace dv {

RowUpdateBase::RowUpdateBase(
    std::shared_ptr<DualityViewUpdater::Operation> parent,
    std::shared_ptr<Table> table, const PrimaryKeyColumnValues &pk,
    const ObjectRowOwnership &row_ownership)
    : RowChangeOperation(parent, table, pk, row_ownership) {
  assert(!pk.empty() || row_ownership.enabled());
}

RowUpdateBase::RowUpdateBase(
    std::shared_ptr<DualityViewUpdater::Operation> parent,
    std::shared_ptr<Table> table, const ObjectRowOwnership &row_ownership)
    : RowChangeOperation(parent, table, row_ownership) {}

void RowUpdateBase::run(MySQLSession *session) {
  for (auto op : before_) {
    op->run(session);
  }

  do_update(session);

  for (auto op : after_) {
    op->run(session);
  }
}

void RowUpdateBase::process_to_many(const ForeignKeyReference &fk,
                                    JSONInputArray input) {
  assert(fk.to_many);

  // - unchanged elements produce no actions nor errors
  // - removed elements are deleted with NOT IN (new_list_of_pks) condition
  // - new and updated elements are upserted
  //      -

  auto table = fk.ref_table;

  rapidjson::Value::ConstMemberIterator old_v;

  std::vector<PrimaryKeyColumnValues> rows_deleted;

  input.sort_old<PrimaryKeyColumnValues>(
      [fk](const rapidjson::Value &value) {
        return ref_primary_key(fk, value, false);
      },
      rows_deleted);

  for (size_t i = 0; i < input.size(); i++) {
    auto elem = input.get(i);
    if (input.has_old()) {
      if (!elem.has_old()) {
        // add item
        std::shared_ptr<RowChangeOperation> op;
        if (fk.ref_table->with_update() && fk.ref_table->with_insert()) {
          op = add_upsert_referencing_this(fk);
        } else if (fk.ref_table->with_update()) {
          op = add_update_referencing_this(
              fk, ref_primary_key(fk, elem.new_value(), true), true);
        } else if (fk.ref_table->with_insert()) {
          op = add_insert_referencing_this(fk);
        } else {
          throw_ENOINSERT(fk.ref_table->table);
        }

        on_referencing_row(fk, op);

        op->process(make_input_object(elem, fk.ref_table->table));
      } else {
        auto pk = ref_primary_key(fk, elem.new_value(), true);

        auto update = add_update_referencing_this(fk, pk);

        on_referencing_row(fk, update);

        update->process(make_input_object(elem, fk.ref_table->table));
      }

      continue;
    }
  }

  if (!rows_deleted.empty()) {
    if (!fk.ref_table->with_delete()) {
      if (!fk.ref_table->with_update()) {
        throw_ENODELETE(fk.ref_table->table);
      } else {
        // abandon rows by setting FK to NULL
      }
    }
    auto del = add_delete_referencing_this(fk, primary_key());
    del->delete_rows(std::move(rows_deleted));
  }
}

RowUpdate::RowUpdate(std::shared_ptr<DualityViewUpdater::Operation> parent,
                     std::shared_ptr<Table> table,
                     const PrimaryKeyColumnValues &pk,
                     const ObjectRowOwnership &row_ownership)
    : RowUpdateBase(parent, table, pk, row_ownership) {
  //   for (const auto &col : pk) {
  //     auto field = table->get_column_field(col.first);
  //     if (!field) throw std::invalid_argument("Invalid primary key");

  //     // ignore owner_id coming from the request
  //     if (row_ownership_.is_owner_id(*field->source)) continue;

  //     add_value(field->source, col.second);
  //   }
}

void RowUpdate::do_update(MySQLSession *session) {
  query_ = update_sql();

  if (!query_.is_empty()) {
    execute(session);

    // TODO XXX check affected_rows() behavior
    if (error_if_not_found_ && session->affected_rows() == 0) {
      throw_ENOINSERT(table_->table);
    }
  }
}

void RowUpdate::on_referenced_row(
    const ForeignKeyReference &fk, const JSONInputObject &input,
    std::optional<PrimaryKeyColumnValues> child_pk) {
  assert(!fk.to_many);
  assert(!child_pk.has_value() || child_pk->size() == fk.column_mapping.size());

  auto old_child_pk = ref_primary_key(fk, input.old_object(), false);

  if (child_pk.has_value()) {
    for (auto col : fk.column_mapping) {
      auto it = child_pk->find(col.second);

      if (it == child_pk->end()) {
        // probably invalid metadata
        throw std::runtime_error(
            "Error processing primary key of referenced object (column " +
            col.second + ")");
      }

      if (old_child_pk == *child_pk) {
        // old and new FK values are the same, so no-op
        continue;
      }

      const auto column = table_->get_column(col.first);

      if (!table_->with_update(*column))
        throw_ENOUPDATE(table_->table, column->with_update.has_value()
                                           ? column->column_name
                                           : "");
      on_value(*column, it->second);
    }
  } else {
    if (old_child_pk.empty()) {
      // both new and old FK values are null, so no-op
      return;
    }
    for (auto col : fk.column_mapping) {
      const auto column = table_->get_column(col.first);

      if (!table_->with_update(*column))
        throw_ENOUPDATE(table_->table, column->with_update.has_value()
                                           ? column->column_name
                                           : "");

      on_value(*column, mysqlrouter::sqlstring("NULL"));
    }
  }
}

void RowUpdate::on_value(const Column &column,
                         const JSONInputObject::MemberReference &value) {
  if (!table_->with_update(column) && value.new_value() != value.old_value()) {
    if (column.is_primary) throw_immutable_id(table_->table);

    throw_ENOUPDATE(table_->table,
                    column.with_update.has_value() ? column.name : "");
  }
  RowChangeOperation::on_value(column, value);
}

void RowUpdate::on_value(const Column &column,
                         const mysqlrouter::sqlstring &value) {
  if (column.is_primary && is_root()) {
    const auto &pk = primary_key();
    if (auto it = pk.find(column.column_name);
        it != pk.end() && it->second.str() != value.str()) {
      throw_immutable_id(table_->table);
    }
    return;
  }

  RowChangeOperation::on_value(column, value);
}

void RowUpdate::on_no_value(const Column &column,
                            const JSONInputObject::MemberReference &input) {
  if (column.is_primary && !column.is_row_owner) {
    throw_immutable_id(table_->table);
  }
  RowChangeOperation::on_no_value(column, input);
}

bool RowUpdate::feed_columns(mysqlrouter::sqlstring &sql, bool is_null,
                             const char *separator) const {
  mysqlrouter::sqlstring tmp;
  auto vit = values_.begin();
  auto pkit = not_updatable_.begin();
  bool first = true;
  for (auto cit = columns_.begin(); cit != columns_.end();
       ++cit, ++vit, ++pkit) {
    if (!*pkit) {
      if (!first) {
        tmp.append_preformatted(separator);
      }
      first = false;
      tmp.append_preformatted(*cit);
      if (is_null && vit->str() == "NULL") {
        tmp.append_preformatted(" IS NULL");
      } else {
        tmp.append_preformatted("=");
        tmp.append_preformatted(*vit);
      }
    }
  }
  sql << tmp;

  return !tmp.is_empty();
}

mysqlrouter::sqlstring RowUpdate::update_sql() const {
  if (values_.empty()) return {};

  //   assert(is_complete_primary_key(pk));
  mysqlrouter::sqlstring sql{"UPDATE !.! ! SET ? WHERE "};

  sql << table_->schema << table_->table << table_->table_alias;

  if (!feed_columns(sql, false, ", ")) return {};

  append_match_condition(sql);

  return sql;
}

RowUpdateReferencing::RowUpdateReferencing(
    std::shared_ptr<DualityViewUpdater::Operation> parent,
    std::shared_ptr<Table> table, const ObjectRowOwnership &row_ownership)
    : RowUpdate(parent, table, row_ownership) {}

void RowUpdateReferencing::do_update(MySQLSession *session) {
  query_ = update_sql();
  if (!query_.is_empty()) execute(session);
}

mysqlrouter::sqlstring RowUpdateReferencing::update_sql() const {
  std::vector<std::shared_ptr<Operation>> parents;
  mysqlrouter::sqlstring sql{"UPDATE !.! ! "};

  sql << table_->schema << table_->table << table_->table_alias;

  auto where = join_to_parent(&parents);

  for (const auto &p : parents) {
    const auto t = p->table();
    mysqlrouter::sqlstring join(" INNER JOIN !.! !");
    join << t->schema << t->table << t->table_alias;
    sql.append_preformatted(join);
  }

  mysqlrouter::sqlstring set(" SET ?");
  if (!feed_columns(set, false, ", ")) return {};

  sql.append_preformatted(set);

  sql.append_preformatted(" WHERE ");
  parents.back()->append_match_condition(sql);

  sql.append_preformatted(" AND ");
  sql.append_preformatted(where);

  return sql;
}

mysqlrouter::sqlstring RowNoUpdateOrError::update_sql() const {
  if (values_.empty()) return {};

  mysqlrouter::sqlstring sql{"SELECT (?) FROM !.! ! WHERE "};

  feed_columns(sql, true, " AND ");

  sql << table_->schema << table_->table << table_->table_alias;

  append_match_condition(sql);

  return sql;
}

void RowNoUpdateOrError::do_update(MySQLSession *session) {
  query_ = update_sql();
  if (!query_.is_empty()) {
    input_matches_row_ = false;
    execute(session);
    if (!input_matches_row_) {
      throw_ENOUPDATE(table_->table);
    }
  }
}

void RowNoUpdateOrError::on_row(const ResultRow &row) {
  input_matches_row_ = (strcmp(row[0], "1") == 0);
}

std::shared_ptr<RowUpdate> make_row_update(
    std::shared_ptr<DualityViewUpdater::Operation> parent,
    std::shared_ptr<Table> table, const PrimaryKeyColumnValues &pk,
    const ObjectRowOwnership &row_ownership) {
  return std::make_shared<RowUpdate>(parent, table, pk, row_ownership);
}

std::shared_ptr<RowNoUpdateOrIgnore> make_row_no_update_or_ignore(
    std::shared_ptr<DualityViewUpdater::Operation> parent,
    std::shared_ptr<Table> table, const PrimaryKeyColumnValues &pk,
    const ObjectRowOwnership &row_ownership) {
  return std::make_shared<RowNoUpdateOrIgnore>(parent, table, pk,
                                               row_ownership);
}

std::shared_ptr<RowNoUpdateOrError> make_row_no_update_or_error(
    std::shared_ptr<DualityViewUpdater::Operation> parent,
    std::shared_ptr<Table> table, const PrimaryKeyColumnValues &pk,
    const ObjectRowOwnership &row_ownership) {
  return std::make_shared<RowNoUpdateOrError>(parent, table, pk, row_ownership);
}

}  // namespace dv
}  // namespace database
}  // namespace mrs
