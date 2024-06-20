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

#include "mrs/database/duality_view/delete.h"

#include "mrs/database/duality_view/errors.h"
#include "mrs/database/duality_view/update.h"

namespace mrs {
namespace database {
namespace dv {

//
// There are 3 cases for delete:
// 1 - delete root object by pk
// 2 - delete multiple root objects matched by filter
// 3 - keep root object but delete one or more elements of a nested 1:n list
//
// Note that delete is not supported for 1:1 nested objects
//
// - Case 1 becomes a (single?) multi-table cascading DELETE that matches by PK
// of the root table
// - Case 2 is the same, except the root table rows are matched by the filter
// - Case 3 becomes one multi-table cascading DELETE per list that had objects
// removed and must recursively JOIN with the parent until the root object and
// NOT IN the set of keys of objects that were not removed
//
// Notes:
// - All deletes must always cascade through nested lists (but not nested 1:1
// which are not deletable)
//      - An 1:1 join will never be part of a delete chain

RowDeleteBase::RowDeleteBase(
    std::shared_ptr<DualityViewUpdater::Operation> parent,
    std::shared_ptr<Table> table, const ObjectRowOwnership &row_ownership)
    : RowChangeOperation(parent, table, row_ownership) {}

RowDeleteBase::RowDeleteBase(
    std::shared_ptr<DualityViewUpdater::Operation> parent,
    std::shared_ptr<Table> table, const PrimaryKeyColumnValues &pk_values,
    const ObjectRowOwnership &row_ownership)
    : RowChangeOperation(parent, table, pk_values, row_ownership) {}

void RowDeleteBase::process_to_many(const ForeignKeyReference &ref,
                                    JSONInputArray) {
  auto table = ref.ref_table;

  if (table->with_delete()) {
    // delete all rows referencing the parent
    auto del = add_delete_all_referencing_this(ref);

    on_referencing_row(ref, del);

    del->process({});
  } else if (table->with_update()) {
    // set all FKs of rows referencing the parent to NULL

    add_clear_all_referencing_this(ref);

    // don't delete children of this
  } else {
    has_undeletable_fks_ = true;
  }
}

void RowDeleteBase::run(MySQLSession *session) {
  for (const auto &ch : before_) {
    ch->run(session);
  }

  do_delete(session);

  for (const auto &ch : after_) {
    ch->run(session);
  }
}

void RowDeleteBase::do_delete(MySQLSession *session) {
  query_ = delete_sql();

  if (!query_.is_empty()) {
    try {
      execute(session);
    } catch (const MySQLSession::Error &e) {
      // if a FK constraint fails and there was a 1:n child list that was not
      // deletable, we assume constraint errors are because of those
      if (e.code() == ER_ROW_IS_REFERENCED_2 && has_undeletable_fks_) {
        throw_ENODELETE();
      }
      throw;
    }
  }
}

void RowDelete::process(JSONInputObject input) {
  if (!table_->with_delete()) throw_ENODELETE(table_->table);

  RowChangeOperation::process(std::move(input));
}

mysqlrouter::sqlstring RowDelete::delete_sql() const {
  mysqlrouter::sqlstring sql("DELETE FROM !.! ! WHERE ");

  sql << table_->schema << table_->table << table_->table_alias;
  append_match_condition(sql);

  return sql;
}

void RowDeleteMany::process(JSONInputObject input) {
  if (!table_->with_delete()) throw_ENODELETE(table_->table);

  RowChangeOperation::process(std::move(input));
}

mysqlrouter::sqlstring RowDeleteMany::delete_sql() const {
  mysqlrouter::sqlstring sql("DELETE FROM !.! ! WHERE ");

  sql << table_->schema << table_->table << table_->table_alias;
  append_match_condition(sql);

  return sql;
}

void RowDeleteMany::append_match_condition(mysqlrouter::sqlstring &sql) const {
  mysqlrouter::sqlstring where;
  auto cont = add_row_owner_check(&where, true);
  where.append_preformatted_sep(cont ? " AND " : " ", filter_);
  sql.append_preformatted(where);
}

void RowDeleteReferencing::delete_rows(
    std::vector<PrimaryKeyColumnValues> rows) {
  rows_to_delete_ = std::move(rows);
}

mysqlrouter::sqlstring RowDeleteReferencing::delete_sql() const {
  std::vector<std::shared_ptr<Operation>> parents;
  mysqlrouter::sqlstring sql("DELETE !");

  sql << table_->table_alias;

  auto where = join_to_parent(&parents);

  if (!parents.empty()) {
    mysqlrouter::sqlstring base(" FROM !.! !");
    base << table_->schema << table_->table << table_->table_alias;
    sql.append_preformatted(base);

    for (const auto &p : parents) {
      const auto t = p->table();
      mysqlrouter::sqlstring join(" INNER JOIN !.! !");
      join << t->schema << t->table << t->table_alias;
      sql.append_preformatted(join);
    }
    sql.append_preformatted(" WHERE ");
    parents.back()->append_match_condition(sql);
  } else {
    sql.append_preformatted(" WHERE ");
    append_match_condition(sql);
  }
  sql.append_preformatted(" AND ");
  sql.append_preformatted(where);

  if (!rows_to_delete_.empty()) {
    mysqlrouter::sqlstring delete_set;
    for (const auto &pk : rows_to_delete_) {
      delete_set.append_preformatted_sep(") OR (",
                                         format_where_expr(*table_, pk));
    }
    sql.append_preformatted(" AND ((");
    sql.append_preformatted(delete_set);
    sql.append_preformatted("))");
  }
  return sql;
}

}  // namespace dv
}  // namespace database
}  // namespace mrs
