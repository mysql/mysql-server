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

#include "router/src/mysql_rest_service/src/mrs/database/duality_view/insert.h"
#include "helper/json/rapid_json_to_text.h"
#include "helper/json/to_sqlstring.h"
#include "mrs/database/duality_view/update.h"
#include "mysqlrouter/base64.h"
namespace mrs {
namespace database {
namespace dv {

using namespace helper::json::sql;

namespace {

mysqlrouter::sqlstring generate_uuid(MySQLSession *session) {
  // query as base64 because query_one() will truncate \0 in binary strings
  // (should be fixed at the source)
  auto row = session->query_one("SELECT TO_BASE64(UUID_TO_BIN(UUID(), 1))");
  auto tmp = mysqlrouter::sqlstring("?");
  tmp << Base64::decode((*row)[0]);
  return tmp;
}

static std::function<mysqlrouter::sqlstring(MySQLSession *)> g_generate_uuid =
    generate_uuid;
}  // namespace

void RowInsert::run(MySQLSession *session) {
  for (auto op : before_) {
    op->run(session);
  }

  on_pre_insert(session);

  query_ = insert_sql();

  execute(session);

  on_post_insert(session);

  for (auto op : after_) {
    op->run(session);
  }
}

void RowInsert::process(JSONInputObject input) {
  if (!table_->with_insert()) throw_ENOINSERT(table_->table);

  RowChangeOperation::process(std::move(input));
}

void RowInsert::process_to_many(const ForeignKeyReference &fk,
                                JSONInputArray input) {
  assert(fk.to_many);

  auto table = fk.ref_table;

  for (size_t i = 0; i < input.size(); i++) {
    auto elem = input.get(i);

    if (table->with_insert() && table->with_update()) {
      auto upsert = add_upsert_referencing_this(fk);

      on_referencing_row(fk, upsert);

      upsert->process(make_input_object(elem, table_->table));
    } else if (table->with_insert()) {
      auto insert = add_insert_referencing_this(fk);

      on_referencing_row(fk, insert);

      insert->process(make_input_object(elem, table_->table));
    } else if (table->with_update()) {
      auto pk = ref_primary_key(fk, elem.new_value(), false);
      assert(!pk.empty());  // validated in JSON check

      // update if PK exists else ENOINSERT
      constexpr bool error_if_not_found = true;

      auto update = add_update_referencing_this(fk, pk, error_if_not_found);

      on_referencing_row(fk, update);

      update->process(make_input_object(elem, table_->table));
    } else {
      throw_ENOINSERT(table->table);
    }
  }
}

std::shared_ptr<RowChangeOperation>
RowInsert::add_dummy_update_referenced_from_this(
    const ForeignKeyReference &fk, const PrimaryKeyColumnValues &pk) {
  auto update = make_row_no_update_or_ignore(shared_from_this(), fk.ref_table,
                                             pk, row_ownership_);

  run_after(update);

  return update;
}

mysqlrouter::sqlstring RowInsert::insert_sql() const {
  mysqlrouter::sqlstring sql;
  const bool is_root = parent_.expired();

  if (row_ownership_.enabled() && is_root) {
    sql = mysqlrouter::sqlstring{"INSERT INTO !.! (!, ?) VALUES (?, ?)"};

    sql << table_->schema << table_->table << row_ownership_.owner_column_name()
        << join_sqlstrings(columns_, ", ") << row_ownership_.owner_user_id()
        << join_sqlstrings(values_, ", ");
  } else {
    sql = mysqlrouter::sqlstring{"INSERT INTO !.! (?) VALUES (?)"};

    sql << table_->schema << table_->table << join_sqlstrings(columns_, ", ")
        << join_sqlstrings(values_, ", ");
  }

  if (upsert_) {
    sql.append_preformatted(" AS new ON DUPLICATE KEY UPDATE ");
    bool first = true;
    for (const auto &c : columns_) {
      mysqlrouter::sqlstring tmp(first ? "!=new.!" : ", !=new.!");
      tmp << c << c;
      sql.append_preformatted(tmp);
      first = false;
    }
  }

  return sql;
}

void AutoIncRowInsert::on_post_insert(MySQLSession *session) {
  if (auto pk = pk_.find(gen_id_column_->column_name);
      pk == pk_.end() || pk->second.str() == "NULL") {
    auto row = session->query_one("SELECT LAST_INSERT_ID()");
    pk_[gen_id_column_->column_name] = (*row)[0];

    // propagate PK to FK references
    for (auto op : after_) {
      op->on_parent_pk_resolved(pk_);
    }
  }
}

void ReverseUuidRowInsert::set_generate_uuid(
    std::function<mysqlrouter::sqlstring(MySQLSession *)> fn) {
  if (!fn)
    g_generate_uuid = generate_uuid;
  else
    g_generate_uuid = fn;
}

void ReverseUuidRowInsert::on_pre_insert(MySQLSession *session) {
  if (auto it = pk_.find(gen_id_column_->column_name);
      it == pk_.end() || it->second.str() == "NULL") {
    set_column_sql_value(*gen_id_column_, g_generate_uuid(session));

    // propagate PK to FK references
    for (auto op : after_) {
      op->on_parent_pk_resolved(pk_);
    }
  }
}

std::shared_ptr<RowInsert> _make_row_insert(
    std::shared_ptr<DualityViewUpdater::Operation> parent,
    std::shared_ptr<Table> table, const ObjectRowOwnership &row_ownership,
    bool upsert) {
  auto column = table->try_get_generated_id_column();

  if (!column) {
    return std::make_shared<RowInsert>(parent, table, row_ownership, upsert);
  } else {
    switch (column->id_generation) {
      case entry::IdGenerationType::REVERSE_UUID:
        return std::make_shared<ReverseUuidRowInsert>(parent, table, column,
                                                      row_ownership, upsert);

      case entry::IdGenerationType::AUTO_INCREMENT:
        return std::make_shared<AutoIncRowInsert>(parent, table, column,
                                                  row_ownership, upsert);

      case entry::IdGenerationType::NONE:
        return std::make_shared<RowInsert>(parent, table, row_ownership,
                                           upsert);
    }

    assert(0);
    throw std::logic_error("internal error");
  }
}

std::shared_ptr<RowInsert> make_row_insert(
    std::shared_ptr<DualityViewUpdater::Operation> parent,
    std::shared_ptr<Table> table, const ObjectRowOwnership &row_ownership) {
  return _make_row_insert(parent, table, row_ownership, false);
}

std::shared_ptr<RowInsert> make_row_upsert(
    std::shared_ptr<DualityViewUpdater::Operation> parent,
    std::shared_ptr<Table> table, const ObjectRowOwnership &row_ownership) {
  return _make_row_insert(parent, table, row_ownership, true);
}

}  // namespace dv
}  // namespace database
}  // namespace mrs
