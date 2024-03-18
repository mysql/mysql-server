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

#include "router/src/mysql_rest_service/src/mrs/database/query_rest_table_updater.h"

#include <set>
#include "helper/json/rapid_json_to_text.h"
#include "helper/json/to_sqlstring.h"
#include "mrs/database/helper/object_checksum.h"
#include "mrs/database/helper/object_query.h"
#include "mrs/interface/rest_error.h"
#include "mysqld_error.h"

#include <iostream>

// TODO(alfredo) - refactor, replace to_many with is_foreign

namespace mrs {
namespace database {

using interface::RestError;

/*

## Reference Types

- 1:1
    - column in base table with the PK of the referenced row
    - the referenced row can be assumed to be owned by the parent
- n:1
    - physically identical to 1:1
    - the referenced row is only referenced and hence, not owned by parent
- 1:n
    - column in the referenced table with the PK of the base table
- n:m
    - joiner table has columns with PK of the base table and referenced table

# INSERT

## Changes

- in root object
    - simple column
        - insert in row
    - 1:1 reference (owned)
        - assign to NULL
        - insert ref row using new root ID; update root row with new ref row
ID
    - n:1 reference (non-owned)
        - assign to NULL
        - assign given ref row ID
    - 1:n reference (owned)
        - insert ref rows using new root ID
    - 1:n reference (non-owned)
        - N/A
    - n:m reference (owned)
        - ?
    - n:m reference (non-owned)
        - for each ref ID, add row with new root ID and the ref ID

- if nested objects have references, recursively create them

# UPDATE

- in root object
    - simple column
        - update row
    - 1:1 reference (owned)
        - value -> NULL - assign root row to NULL; delete ref row
        - NULL -> value - insert ref row using root ID; update root row with
new ref row ID
        - value -> value - update ref row
    - n:1 reference (non-owned)
        - value -> NULL - assign root row to NULL
        - NULL -> value - assign given ref row ID
        - value -> value - assign given ref row ID
    - 1:n reference (owned)
        - delete removed rows
        - insert new rows using root ID
        - update ref rows
    - 1:n reference (non-owned)
        - N/A
    - n:m reference (owned)
        - ?
    - n:m reference (non-owned)
        - delete removed rows
        - add rows with newly added ref IDs

# DELETE

- simple column - N/A
- 1:1 reference (owned) - delete ref, recurse
- n:1 reference (non-owned) - N/A
- 1:n reference (owned) - delete matches, recurse
- 1:n reference (non-owned) - N/A
- n:m reference (owned) - ?
- n:m reference (non-owned) - delete join rows, recurse

## Primary Key Types
- pre-defined
- auto-incremented
- generated with UUID
- ownerId

*/

/*

## Concurrent Updates with Etag

The basic algorithm for performing concurrent updates with etag is:

1. Start transaction
2. Compute ETag and lock rows to be updated
  - If row lock fails, abort
3. Compare ETag with the one sent in request
  - If ETag doesn't match, abort
4. Update rows
5. Commit

*/

using namespace helper::json::sql;

using MySQLSession = mysqlrouter::MySQLSession;

class RowInsert;
class RowUpdate;

namespace {

auto k_null = mysqlrouter::sqlstring("NULL");

mysqlrouter::sqlstring join_sqlstrings(
    const std::vector<mysqlrouter::sqlstring> &strings,
    const std::string &sep) {
  mysqlrouter::sqlstring str;
  for (const auto &s : strings) {
    str.append_preformatted_sep(sep, s);
  }
  return str;
}

inline std::string join_json_pointer(const std::string &jptr,
                                     const std::string &elem) {
  if (jptr == "/")
    return jptr + elem;
  else
    return jptr + "/" + elem;
}

std::shared_ptr<RowInsert> make_row_insert(
    std::shared_ptr<TableUpdater::Operation> parent,
    std::shared_ptr<entry::Table> table,
    const ObjectRowOwnership &row_ownership);

}  // namespace

class TableUpdater::Operation
    : public QueryLog,
      public std::enable_shared_from_this<TableUpdater::Operation> {
 protected:
  size_t affected_ = 0;

 public:
  std::weak_ptr<Operation> parent_;
  std::shared_ptr<entry::Table> table_;
  const ObjectRowOwnership &row_ownership_;

 public:
  Operation(std::shared_ptr<Operation> parent,
            std::shared_ptr<entry::Table> table,
            const ObjectRowOwnership &row_ownership)
      : parent_(parent), table_(table), row_ownership_(row_ownership) {}

  virtual void run(MySQLSession *session) = 0;

  virtual void will_run([[maybe_unused]] MySQLSession *session) {}

  virtual void did_run([[maybe_unused]] MySQLSession *session) {}

  const std::shared_ptr<entry::Table> &table() const { return table_; }

  size_t affected() const { return affected_; }

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

  void execute(MySQLSession *session) override {
    QueryLog::execute(session);
    affected_ = session->affected_rows();
  }
};

//

class RowChangeOperation : public TableUpdater::Operation {
 protected:
  RowChangeOperation(std::shared_ptr<Operation> parent,
                     std::shared_ptr<entry::Table> table,
                     const ObjectRowOwnership &row_ownership)
      : Operation(parent, table, row_ownership) {}

 public:
  virtual void add_value(std::shared_ptr<entry::Column> column,
                         const mysqlrouter::sqlstring &value) = 0;

  virtual const PrimaryKeyColumnValues &primary_key() const {
    throw std::invalid_argument("invalid call");
  }
};

//

class RowInsert : public RowChangeOperation {
 protected:
  friend class RowUpdate;

  PrimaryKeyColumnValues pk_;
  std::vector<mysqlrouter::sqlstring> columns_;
  std::vector<mysqlrouter::sqlstring> values_;
  std::vector<bool> not_updatable_;

  bool references_to_this_resolved_ = false;
  bool references_from_this_resolved_ = false;
  bool ignore_duplicate_key_ = false;

  std::list<std::shared_ptr<RowInsert>> children_;
  std::list<std::shared_ptr<RowInsert>> ref_children_;

 public:
  RowInsert(std::shared_ptr<Operation> parent,
            std::shared_ptr<entry::Table> table,
            const ObjectRowOwnership &row_ownership)
      : RowChangeOperation(parent, table, row_ownership) {}

  bool empty() const { return columns_.empty(); }

  void run(MySQLSession *session) override {
    // handle rows from tables that are referenced from this one
    for (const auto &op : ref_children_) {
      op->run(session);

      op->resolve_references_from_this();
    }

    if (table_->create_allowed()) {
      if (!empty()) {
        on_pre_insert(session);

        query_ = insert_sql();

        try {
          execute(session);
        } catch (const mysqlrouter::MySQLSession::Error &e) {
          if (e.code() != ER_PARSE_ERROR || !ignore_duplicate_key_) throw;
        }
        on_post_insert(session);
      }
    }

    for (const auto &op : children_) {
      op->resolve_references_to_this();
      op->run(session);
    }
  }

  const PrimaryKeyColumnValues &primary_key() const override { return pk_; }

  virtual void on_pre_insert(MySQLSession *) {}
  virtual void on_post_insert(MySQLSession *) {}

  std::shared_ptr<RowInsert> add_referencing_insert(
      std::shared_ptr<entry::JoinedTable> join) {
    // rows in the joined table reference this table
    auto child = make_row_insert(shared_from_this(), join, row_ownership_);
    children_.push_back(child);
    return child;
  }

  std::shared_ptr<RowInsert> add_referenced_insert(
      std::shared_ptr<entry::JoinedTable> join) {
    auto it =
        std::find_if(ref_children_.begin(), ref_children_.end(),
                     [join](const auto &ch) { return ch->table() == join; });
    if (it != ref_children_.end())
      return std::dynamic_pointer_cast<RowInsert>(*it);

    // rows in the joined table are referenced from this table
    // if they're new rows, they need to be inserted first and then their id
    // will be updated in the base row
    auto child = make_row_insert(shared_from_this(), join, row_ownership_);
    ref_children_.push_back(child);

    return child;
  }

  void set_ignore_duplicate_key() { ignore_duplicate_key_ = true; }

  void on_value(std::shared_ptr<entry::DataField> field,
                const mysqlrouter::sqlstring &value) {
    assert(field->source->table.lock() == table_);

    if (field->source->is_foreign) {
      return;
    }

    if (field->enabled || field->source->is_primary) {
      if (row_ownership_.is_owner_id(*field->source)) {
        if (field->source->is_primary)
          pk_[field->source->name] = row_ownership_.owner_user_id();
        return;
      }

      if (field->source->type == entry::ColumnType::BINARY) {
        add_value(field->source, mysqlrouter::sqlstring("FROM_BASE64(?)")
                                     << value);
      } else if (field->source->type == entry::ColumnType::GEOMETRY) {
        add_value(field->source,
                  mysqlrouter::sqlstring("ST_GeomFromGeoJSON(?, 1, ?)")
                      << value << field->source->srid);
      } else {
        add_value(field->source, value);
      }
    }
  }

  void on_no_value(std::shared_ptr<entry::DataField> field) {
    assert(field->source->table.lock() == table_);

    if (field->source->is_foreign) {
      return;
    }

    if (row_ownership_.is_owner_id(*field->source)) {
      if (field->source->is_primary)
        pk_[field->source->name] = row_ownership_.owner_user_id();
      return;
    }

    if (!field->source->is_auto_generated_id()) {
      if (field->source->is_primary)
        throw RestError(
            "Inserted document must contain a primary key, it may be "
            "auto generated by 'ownership' configuration or auto_increment.");
      else if (field->enabled)
        throw RestError("Document has missing field: " +
                        field->source->table.lock()->table + "." + field->name);
    }
  }

  void on_default_value(std::shared_ptr<entry::DataField> field) {
    assert(field->source->table.lock() == table_);

    if (field->source->is_foreign) {
      return;
    }

    if (row_ownership_.is_owner_id(*field->source)) {
      if (field->source->is_primary)
        pk_[field->source->name] = row_ownership_.owner_user_id();
      return;
    }

    if (!field->source->is_auto_generated_id()) {
      if (field->source->is_primary)
        throw RestError(
            "Inserted document must contain a primary key, it may be "
            "auto generated by 'ownership' configuration or auto_increment.");
      else
        add_value(field->source, "DEFAULT");
    }
  }

  void add_value(std::shared_ptr<entry::Column> column,
                 const mysqlrouter::sqlstring &value) override {
    mysqlrouter::sqlstring tmp("!");
    tmp << column->name;

    if (auto it =
            std::find_if(columns_.begin(), columns_.end(),
                         [tmp](const auto &c) { return c.str() == tmp.str(); });
        it != columns_.end()) {
      return;
    }

    columns_.emplace_back(std::move(tmp));
    values_.push_back(value);

    not_updatable_.push_back(column->is_primary ||
                             row_ownership_.is_owner_id(*column));

    if (column->is_primary && column->table.lock() == table_) {
      pk_[column->name] = value;
    }
  }

 protected:
  mysqlrouter::sqlstring insert_sql() const {
    if (row_ownership_.enabled() && parent_.expired()) {
      mysqlrouter::sqlstring sql{"INSERT INTO !.! (!, ?) VALUES (?, ?)"};

      sql << table_->schema << table_->table
          << row_ownership_.owner_column_name()
          << join_sqlstrings(columns_, ", ") << row_ownership_.owner_user_id()
          << join_sqlstrings(values_, ", ");

      return sql;
    } else {
      mysqlrouter::sqlstring sql{"INSERT INTO !.! (?) VALUES (?)"};

      sql << table_->schema << table_->table << join_sqlstrings(columns_, ", ")
          << join_sqlstrings(values_, ", ");

      return sql;
    }
  }

  void resolve_references_to_this() {
    if (references_to_this_resolved_) return;

    if (auto parent =
            std::dynamic_pointer_cast<RowChangeOperation>(parent_.lock())) {
      const auto &parent_pk = parent->primary_key();

      auto join = std::dynamic_pointer_cast<entry::JoinedTable>(table_);
      assert(join);

      // 1:n means PK is at parent and FK is at this table
      if (join->to_many) {
        for (const auto &col : join->column_mapping) {
          auto ppk = parent_pk.find(col.second->name);
          if (ppk == parent_pk.end())
            throw std::invalid_argument(
                "Invalid metadata: invalid base column " + col.second->name +
                " referenced from " + join->table);

          add_value(col.first, ppk->second);
        }
      }
      references_to_this_resolved_ = true;
    }
  }

  void resolve_references_from_this() {
    if (references_from_this_resolved_) return;

    if (auto parent =
            std::dynamic_pointer_cast<RowChangeOperation>(parent_.lock())) {
      const auto &my_pk = primary_key();

      auto join = std::dynamic_pointer_cast<entry::JoinedTable>(table_);
      assert(join);

      for (const auto &col : join->column_mapping) {
        auto mpk = my_pk.find(col.second->name);
        if (mpk == my_pk.end()) {
          if (empty()) {
            // this is a NULL insert
            parent->add_value(col.first, k_null);
          } else {
            throw std::invalid_argument(
                "Invalid metadata: invalid referenced table column " +
                col.second->name);
          }
        } else {
          // point the FK of the parent row to this one
          parent->add_value(col.first, mpk->second);
        }
      }
      references_from_this_resolved_ = true;
    }
  }
};

class AutoIncRowInsert : public RowInsert {
 private:
  std::shared_ptr<entry::Column> gen_id_column_;

 public:
  AutoIncRowInsert(std::shared_ptr<Operation> parent,
                   std::shared_ptr<entry::Table> table,
                   std::shared_ptr<entry::Column> id_column,
                   const ObjectRowOwnership &row_ownership)
      : RowInsert(parent, table, row_ownership), gen_id_column_(id_column) {}

  void on_post_insert(MySQLSession *session) override {
    if (auto pk = pk_.find(gen_id_column_->name);
        pk == pk_.end() || pk->second.str() == "NULL") {
      auto row = session->query_one("SELECT LAST_INSERT_ID()");
      pk_[gen_id_column_->name] = (*row)[0];
    }
  }
};

class ReverseUuidRowInsert : public RowInsert {
 private:
  std::shared_ptr<entry::Column> gen_id_column_;

 public:
  ReverseUuidRowInsert(std::shared_ptr<Operation> parent,
                       std::shared_ptr<entry::Table> table,
                       std::shared_ptr<entry::Column> id_column,
                       const ObjectRowOwnership &row_ownership)
      : RowInsert(parent, table, row_ownership), gen_id_column_(id_column) {}

  void on_pre_insert(MySQLSession *session) override {
    if (pk_.find(gen_id_column_->name) == pk_.end()) {
      // TODO(alfredo) - this query is currently using strlen() which will fail
      // if the UUID has a \0
      auto row = session->query_one("SELECT UUID_TO_BIN(UUID(), 1)");
      auto uuid = mysqlrouter::sqlstring("?") << (*row)[0];
      pk_[gen_id_column_->name] = uuid;

      // RowInsert::on_value(gen_id_column_, uuid);
      add_value(gen_id_column_, uuid);
    }
  }
};

//

class RowDeleteOperation : public TableUpdater::Operation {
 protected:
  std::list<std::shared_ptr<RowDeleteOperation>> children_;
  std::list<std::shared_ptr<RowDeleteOperation>> ref_children_;
  std::string key_snapshot_table_;
  bool needs_key_snapshot_ = false;

  RowDeleteOperation(std::shared_ptr<Operation> parent,
                     std::shared_ptr<entry::Table> table,
                     const ObjectRowOwnership &row_ownership)
      : Operation(parent, table, row_ownership) {}

 public:
  void will_run(MySQLSession *session) override {
    // If there are tables referenced from this, then keep the rows to be
    // deleted in a TEMP TABLE so that we can delete rows referenced by them
    // afterwards. We can't just delete them first because the FK constraints
    // would block them.

    if (auto join = std::dynamic_pointer_cast<entry::JoinedTable>(table_);
        join && needs_key_snapshot_) {
      create_snapshot_table(session);
    }
    for (const auto &ch : ref_children_) {
      ch->will_run(session);
    }
    for (const auto &ch : children_) {
      ch->will_run(session);
    }
  }

  void did_run(MySQLSession *session) override {
    if (auto join = std::dynamic_pointer_cast<entry::JoinedTable>(table_);
        join && needs_key_snapshot_) {
      drop_snapshot_table(session);
    }
    for (const auto &ch : ref_children_) {
      ch->did_run(session);
    }
    for (const auto &ch : children_) {
      ch->did_run(session);
    }
  }

  void run(MySQLSession *session) override {
    for (const auto &ch : ref_children_) {
      ch->run(session);
    }

    if (table_->delete_allowed()) {
      query_ = delete_sql();
      execute(session);
    }

    for (const auto &ch : children_) {
      ch->run(session);
    }
  }

  void set_needs_key_snapshot() { needs_key_snapshot_ = true; }
  bool needs_key_snapshot() const { return needs_key_snapshot_; }

  virtual std::shared_ptr<RowDeleteOperation> add_referencing_delete(
      std::shared_ptr<entry::Table> table);

  virtual std::shared_ptr<RowDeleteOperation> add_referenced_delete(
      std::shared_ptr<entry::Table> table);

  virtual mysqlrouter::sqlstring delete_sql() const {
    mysqlrouter::sqlstring sql{"DELETE FROM !.! as ! ?"};
    sql << table_->schema << table_->table << table_->table_alias;
    sql << join_clause();

    return sql;
  }

  virtual mysqlrouter::sqlstring join_subquery() const = 0;

  virtual mysqlrouter::sqlstring join_clause() const = 0;

  const std::string &key_snapshot_table() const { return key_snapshot_table_; }

  void create_snapshot_table(MySQLSession *session) {
    mysqlrouter::sqlstring sql(
        "CREATE TEMPORARY TABLE IF NOT EXISTS !.! AS (?)");

    sql << table_->schema << table_->table_alias + "$$" << join_subquery();
    key_snapshot_table_ = table_->table_alias + "$$";
    session->execute(sql);
  }

  void drop_snapshot_table(MySQLSession *session) {
    mysqlrouter::sqlstring sql("DROP TEMPORARY TABLE IF EXISTS !.!");
    sql << table_->schema << key_snapshot_table();
    session->execute(sql);
    key_snapshot_table_.clear();
  }
};

class RefRowDelete : public RowDeleteOperation {
 private:
  std::shared_ptr<entry::Table> ref_table_;
  std::shared_ptr<RowUpdate> cond_update_;
  PrimaryKeyColumnValues ref_pk_;

 public:
  RefRowDelete(std::shared_ptr<Operation> parent,
               std::shared_ptr<entry::Table> table,
               std::shared_ptr<entry::Table> ref_table,
               std::shared_ptr<RowUpdate> cond_update,
               const PrimaryKeyColumnValues &ref_pk,
               const ObjectRowOwnership &row_ownership)
      : RowDeleteOperation(parent, table, row_ownership),
        ref_table_(ref_table),
        cond_update_(cond_update),
        ref_pk_(ref_pk) {}

  void run(MySQLSession *session) override;

  mysqlrouter::sqlstring join_subquery() const override {
    auto format_join_columns = [](const entry::JoinedTable &join) {
      mysqlrouter::sqlstring columns;
      for (const auto &c : join.column_mapping) {
        columns.append_preformatted_sep(", ", mysqlrouter::sqlstring("!")
                                                  << c.first->name);
      }
      return columns;
    };

    if (ref_pk_.empty()) return {};
    auto join = std::dynamic_pointer_cast<entry::JoinedTable>(table_);
    assert(join);
    assert(!ref_pk_.empty());

    mysqlrouter::sqlstring sql("SELECT ? FROM !.! ! WHERE ?");
    sql << format_join_columns(*join) << ref_table_->schema << ref_table_->table
        << ref_table_->table_alias << format_where_expr(ref_table_, ref_pk_);

    return sql;
  }

  mysqlrouter::sqlstring join_clause() const override {
    throw std::logic_error("not impl");
    return {""};
  }

 private:
  mysqlrouter::sqlstring delete_sql() const override {
    auto join = std::dynamic_pointer_cast<entry::JoinedTable>(table_);
    assert(join);

    if (!key_snapshot_table().empty()) {
      mysqlrouter::sqlstring cols;
      mysqlrouter::sqlstring subquery_cols;
      for (const auto &c : join->column_mapping) {
        cols.append_preformatted_sep(", ", mysqlrouter::sqlstring("!")
                                               << c.second->name);
        subquery_cols.append_preformatted_sep(
            ", ", mysqlrouter::sqlstring("! as !")
                      << c.first->name << c.second->name);
      }
      mysqlrouter::sqlstring sql(
          "DELETE FROM !.! WHERE (?) IN (SELECT ? FROM !.!)");
      sql << table_->schema << table_->table << cols;
      sql << subquery_cols << table_->schema << key_snapshot_table();
      return sql;
    } else {
      assert(!ref_pk_.empty());

      mysqlrouter::sqlstring sql(
          "DELETE FROM !.! WHERE ! IN (SELECT ! FROM !.! WHERE ?)");
      sql << join->schema << join->table
          << join->column_mapping.begin()->second->name
          << join->column_mapping.begin()->first->name << ref_table_->schema
          << ref_table_->table << format_where_expr(ref_table_, ref_pk_);

      return sql;
    }
  }
};

class ChainedRowDelete : public RowDeleteOperation {
 private:
  std::shared_ptr<RowDeleteOperation> ref_delete_;

 public:
  ChainedRowDelete(std::shared_ptr<Operation> parent,
                   std::shared_ptr<entry::Table> table,
                   std::shared_ptr<RowDeleteOperation> ref_delete,
                   const ObjectRowOwnership &row_ownership)
      : RowDeleteOperation(parent, table, row_ownership),
        ref_delete_(ref_delete) {}

  mysqlrouter::sqlstring join_subquery() const override {
    auto join = std::dynamic_pointer_cast<entry::JoinedTable>(table_);
    assert(join);

    mysqlrouter::sqlstring sql("SELECT ? FROM !.! as ! ? ?");

    mysqlrouter::sqlstring cols;
    for (const auto &c : join->primary_key()) {
      cols.append_preformatted_sep(", ", mysqlrouter::sqlstring("!.!")
                                             << join->table_alias << c->name);
    }
    sql << cols;
    sql << join->schema << join->table << join->table_alias;
    sql << format_left_join(*ref_delete_->table_, *join);

    sql << ref_delete_->join_clause();

    return sql;
  }

  mysqlrouter::sqlstring join_clause() const override {
    auto join = std::dynamic_pointer_cast<entry::JoinedTable>(table_);
    assert(join);

    auto sql = format_left_join(*ref_delete_->table_, *join);
    sql.append_preformatted_sep(" ", ref_delete_->join_clause());

    return sql;
  }

 private:
  mysqlrouter::sqlstring delete_sql() const override {
    auto join = std::dynamic_pointer_cast<entry::JoinedTable>(table_);
    assert(join);

    auto parent = std::dynamic_pointer_cast<RowDeleteOperation>(parent_.lock());

    if (!key_snapshot_table().empty()) {
      mysqlrouter::sqlstring cols;
      for (const auto &c : join->column_mapping) {
        cols.append_preformatted_sep(", ", mysqlrouter::sqlstring("!")
                                               << c.second->name);
      }

      mysqlrouter::sqlstring sql(
          "DELETE FROM !.! WHERE (?) IN (SELECT ? FROM !.!)");
      sql << table_->schema << table_->table << cols;
      sql << cols << table_->schema << key_snapshot_table();
      return sql;
    } else {
      mysqlrouter::sqlstring cols;
      for (const auto &c : join->primary_key()) {
        cols.append_preformatted_sep(", ", mysqlrouter::sqlstring("!")
                                               << c->name);
      }

      mysqlrouter::sqlstring sql(
          "WITH cte (?) AS (?)"
          " DELETE FROM !.! WHERE (?) IN (SELECT * FROM cte)");
      sql << cols;
      sql << join_subquery();
      sql << table_->schema << table_->table;
      sql << cols;
      return sql;
    }
  }
};

std::shared_ptr<RowDeleteOperation> RowDeleteOperation::add_referenced_delete(
    std::shared_ptr<entry::Table> table) {
  auto it =
      std::find_if(children_.begin(), children_.end(),
                   [table](const auto &ch) { return ch->table() == table; });
  if (it != children_.end()) return *it;

  auto del = std::make_shared<ChainedRowDelete>(
      shared_from_this(), table,
      std::dynamic_pointer_cast<RowDeleteOperation>(shared_from_this()),
      row_ownership_);
  children_.push_back(del);
  del->set_needs_key_snapshot();
  return del;
}

std::shared_ptr<RowDeleteOperation> RowDeleteOperation::add_referencing_delete(
    std::shared_ptr<entry::Table> table) {
  auto it =
      std::find_if(ref_children_.begin(), ref_children_.end(),
                   [table](const auto &ch) { return ch->table() == table; });
  if (it != ref_children_.end()) return *it;

  auto del = std::make_shared<ChainedRowDelete>(
      shared_from_this(), table,
      std::dynamic_pointer_cast<RowDeleteOperation>(shared_from_this()),
      row_ownership_);
  ref_children_.push_back(del);
  return del;
}

class RowDelete : public RowDeleteOperation {
 protected:
  PrimaryKeyColumnValues pk_;

 public:
  RowDelete(std::shared_ptr<Operation> parent,
            std::shared_ptr<entry::Table> table,
            const PrimaryKeyColumnValues &pk_values,
            const ObjectRowOwnership &row_ownership)
      : RowDeleteOperation(parent, table, row_ownership), pk_(pk_values) {}

  mysqlrouter::sqlstring join_subquery() const override {
    mysqlrouter::sqlstring sql("SELECT ? FROM !.! WHERE ?");

    mysqlrouter::sqlstring cols;
    for (const auto &c : table_->primary_key()) {
      cols.append_preformatted_sep(", ", mysqlrouter::sqlstring("!")
                                             << c->name);
    }
    sql << cols;
    sql << table_->schema << table_->table;
    sql << format_where_expr(table_, pk_);

    return sql;
  }

  mysqlrouter::sqlstring join_clause() const override {
    mysqlrouter::sqlstring clause("WHERE");

    // TODO(alfredo) - decide whether conflicting owner + PK should succeed or
    // be a no-op
    bool cont = add_row_owner_check(&clause, true);
    if (!pk_.empty())
      clause.append_preformatted_sep(
          cont ? " AND " : " ",
          format_where_expr(table_, table_->table_alias, pk_));
    return clause;
  }
};

class ConditionalRowDelete : public RowDeleteOperation {
 private:
  mysqlrouter::sqlstring condition_;

 public:
  ConditionalRowDelete(std::shared_ptr<Operation> parent,
                       std::shared_ptr<entry::Table> table,
                       mysqlrouter::sqlstring condition,
                       const ObjectRowOwnership &row_ownership)
      : RowDeleteOperation(parent, table, row_ownership),
        condition_(std::move(condition)) {}

  mysqlrouter::sqlstring join_subquery() const override {
    throw std::logic_error("not implemented");
  }

  mysqlrouter::sqlstring join_clause() const override {
    mysqlrouter::sqlstring sql("WHERE");
    bool cont = add_row_owner_check(&sql, true);

    auto join = std::dynamic_pointer_cast<entry::JoinedTable>(table_);
    if (join) {
      auto parent =
          std::dynamic_pointer_cast<RowChangeOperation>(parent_.lock());
      assert(parent);

      auto pk = parent->primary_key();

      {
        mysqlrouter::sqlstring where;
        for (const auto &c : join->column_mapping) {
          mysqlrouter::sqlstring one(" !.!=?");
          one << c.second->table.lock()->table_alias << c.second->name
              << pk.at(c.second->name);
          where.append_preformatted_sep(" AND", one);
        }
        sql.append_preformatted_sep(cont ? " AND" : "", where);
      }
      sql.append_preformatted(condition_);
    } else {
      sql.append_preformatted_sep(cont ? " AND " : "", condition_);
    }

    return sql;
  }
};

class FilteredRowDelete : public RowDeleteOperation {
 private:
  std::vector<std::shared_ptr<RowUpdate>> rows_to_keep_;

 public:
  FilteredRowDelete(std::shared_ptr<Operation> parent,
                    std::shared_ptr<entry::Table> table,
                    std::vector<std::shared_ptr<RowUpdate>> rows_to_keep,
                    const ObjectRowOwnership &row_ownership)
      : RowDeleteOperation(parent, table, row_ownership),
        rows_to_keep_(std::move(rows_to_keep)) {}

  mysqlrouter::sqlstring join_subquery() const override {
    throw std::logic_error("not implemented");
  }

  mysqlrouter::sqlstring join_clause() const override;

 private:
  mysqlrouter::sqlstring condition_from_rows(
      const std::vector<std::shared_ptr<RowUpdate>> &rows_to_keep) const;
};
//

class RowUpdate : public RowChangeOperation {
 private:
  PrimaryKeyColumnValues target_pk_;

  std::shared_ptr<RowInsert> insert_;

  std::optional<bool> inserted_;

  std::list<std::shared_ptr<Operation>> children_;
  std::list<std::shared_ptr<RowUpdate>> ref_children_;
  std::list<std::shared_ptr<RefRowDelete>> ref_deletes_;

 public:
  RowUpdate(std::shared_ptr<Operation> parent,
            std::shared_ptr<entry::Object> object,
            const PrimaryKeyColumnValues &target_pk,
            const ObjectRowOwnership &row_ownership)
      : RowChangeOperation(parent, object->get_base_table(), row_ownership),
        target_pk_(target_pk) {
    assert(!target_pk.empty() || row_ownership.enabled());

    insert_ = std::make_shared<RowInsert>(parent, table_, row_ownership);

    for (const auto &col : target_pk) {
      auto field = object->get_column_field(col.first);
      if (!field) throw std::invalid_argument("Invalid primary key");

      // ignore owner_id coming from the request
      if (row_ownership_.is_owner_id(*field->source)) continue;

      insert_->add_value(field->source, col.second);
    }
  }

  RowUpdate(std::shared_ptr<Operation> parent,
            std::shared_ptr<entry::Table> table,
            const ObjectRowOwnership &row_ownership)
      : RowChangeOperation(parent, table, row_ownership) {
    insert_ = make_row_insert(parent, table, row_ownership);
  }

  const PrimaryKeyColumnValues &primary_key() const override {
    return target_pk_.empty() ? insert_->primary_key() : target_pk_;
  }

  std::optional<bool> inserted() const { return inserted_; }

  void will_run(MySQLSession *session) override {
    // If there are tables referenced from this, then keep the rows to be
    // deleted in a TEMP TABLE so that we can delete rows referenced by them
    // afterwards. We can't just delete them first because the FK constraints
    // would block them.

    for (const auto &ch : ref_deletes_) {
      ch->will_run(session);
    }
    for (const auto &ch : ref_children_) {
      ch->will_run(session);
    }
    for (const auto &ch : children_) {
      ch->will_run(session);
    }
  }

  void did_run(MySQLSession *session) override {
    for (const auto &ch : children_) {
      ch->did_run(session);
    }
    for (const auto &ch : ref_children_) {
      ch->did_run(session);
    }
    for (const auto &ch : ref_deletes_) {
      ch->did_run(session);
    }
  }

  void run(MySQLSession *session) override {
    const auto &pk = primary_key();

    // handle rows from tables that are referenced from this one
    for (const auto &op : ref_children_) {
      op->run(session);

      // if a referenced row was inserted and not updated, then ensure the old
      // ones are deleted
      if (op->inserted_.value_or(false) && !pk.empty()) {
        auto del = add_conditional_delete(
            std::dynamic_pointer_cast<entry::JoinedTable>(op->table_), op);
        del->set_needs_key_snapshot();
        if (del->key_snapshot_table().empty()) {
          del->will_run(session);
        }
      }
      op->insert_->resolve_references_from_this();
    }

    if (table_->update_allowed()) {
      if (pk.empty()) {
        insert_->run(session);
        inserted_ = true;
        // new rows have nothing to delete
      } else {
        if (update_possible() && is_complete_primary_key(pk)) {
          insert_->resolve_references_to_this();

          query_ = update_sql(pk);
          execute(session);
        } else {
          insert_->set_ignore_duplicate_key();
        }

        if (affected_ == 0) {
          insert_->run(session);
          inserted_ = true;
        } else {
          inserted_ = false;
        }

        for (const auto &op : ref_deletes_) {
          op->run(session);
        }
      }
    } else {
      inserted_ = false;
    }

    if (inserted_) resolve_references();

    for (const auto &op : children_) {
      op->run(session);
    }
  }

  std::shared_ptr<RowUpdate> add_referencing_update(
      std::shared_ptr<entry::JoinedTable> join) {
    // rows in the joined table reference this table

    auto child =
        std::make_shared<RowUpdate>(shared_from_this(), join, row_ownership_);
    children_.push_back(child);
    return child;
  }

  std::shared_ptr<RowUpdate> add_referenced_update(
      std::shared_ptr<entry::JoinedTable> join) {
    // rows in the joined table are referenced from this table
    // if they're new rows, they need to be inserted first and then their id
    // will be updated in the base row

    auto it =
        std::find_if(ref_children_.begin(), ref_children_.end(),
                     [join](const auto &ch) { return ch->table() == join; });
    if (it != ref_children_.end())
      return std::dynamic_pointer_cast<RowUpdate>(*it);

    auto child =
        std::make_shared<RowUpdate>(shared_from_this(), join, row_ownership_);
    ref_children_.push_back(child);

    return child;
  }

  void add_nested_delete(
      std::shared_ptr<entry::JoinedTable> join,
      std::vector<std::shared_ptr<RowUpdate>> updates_to_keep) {
    children_.push_back(std::make_shared<FilteredRowDelete>(
        shared_from_this(), join, std::move(updates_to_keep), row_ownership_));
  }

  std::shared_ptr<RefRowDelete> add_conditional_delete(
      std::shared_ptr<entry::JoinedTable> join,
      std::shared_ptr<RowUpdate> cond_update) {
    // deletes referenced rows IF the dependent operation is an INSERT and not
    // an UPDATE

    // must execute after the UPDATE that clears the field, otherwise the FK
    // constraints will block it
    auto it =
        std::find_if(ref_deletes_.begin(), ref_deletes_.end(),
                     [join](const auto &ch) { return ch->table() == join; });
    if (it != ref_deletes_.end()) return *it;

    auto del = std::make_shared<RefRowDelete>(shared_from_this(), join, table_,
                                              cond_update, primary_key(),
                                              row_ownership_);
    ref_deletes_.push_front(del);
    if (!primary_key().empty()) del->set_needs_key_snapshot();
    return del;
  }

  void on_value(std::shared_ptr<entry::DataField> field,
                const mysqlrouter::sqlstring &value) {
    if (field->enabled || !field->source->is_primary || target_pk_.empty()) {
      insert_->on_value(field, value);
    }
  }

  void on_no_value(std::shared_ptr<entry::DataField> field) {
    if (!field->source->is_primary || target_pk_.empty()) {
      insert_->on_no_value(field);
    }
  }

  void add_value(std::shared_ptr<entry::Column> column,
                 const mysqlrouter::sqlstring &value) override {
    insert_->add_value(column, value);
  }

 protected:
  mysqlrouter::sqlstring update_sql(const PrimaryKeyColumnValues &pk) const {
    assert(is_complete_primary_key(pk));

    mysqlrouter::sqlstring sql{"UPDATE !.! ! SET "};

    sql << table_->schema << table_->table << table_->table_alias;

    auto vit = insert_->values_.begin();
    auto pkit = insert_->not_updatable_.begin();
    bool first = true;
    for (auto cit = insert_->columns_.begin(); cit != insert_->columns_.end();
         ++cit, ++vit, ++pkit) {
      if (!*pkit) {
        if (!first) {
          sql.append_preformatted(", ");
        }
        first = false;
        sql.append_preformatted(*cit);
        sql.append_preformatted("=");
        sql.append_preformatted(*vit);
      }
    }

    mysqlrouter::sqlstring where(" WHERE");
    auto cont = add_row_owner_check(&where, false);
    where.append_preformatted_sep(cont ? " AND " : " ",
                                  format_where_expr(table_, pk));
    sql.append_preformatted(where);

    return sql;
  }

  bool update_possible() const {
    // return true if there are any updateable columns (PKs can't be updated)
    return std::any_of(insert_->not_updatable_.begin(),
                       insert_->not_updatable_.end(),
                       [](bool is_pk) { return !is_pk; });
  }

  bool is_complete_primary_key(const PrimaryKeyColumnValues &pk) const {
    return pk.size() == table_->primary_key().size();
  }

  void resolve_references() {
    // propagate PK value from this table to references

    const auto &my_pk = primary_key();

    for (auto op : children_) {
      auto upd = std::dynamic_pointer_cast<RowUpdate>(op);
      if (upd) {
        auto join = std::dynamic_pointer_cast<entry::JoinedTable>(upd->table());
        assert(join);

        for (const auto &col : join->column_mapping) {
          auto mpk = my_pk.find(col.second->name);
          if (mpk == my_pk.end()) {
            assert(0);
            upd->add_value(col.first, k_null);
          } else {
            upd->add_value(col.first, mpk->second);
          }
        }
      }
    }
  }
};

mysqlrouter::sqlstring FilteredRowDelete::join_clause() const {
  auto join = std::dynamic_pointer_cast<entry::JoinedTable>(table_);
  if (join) {
    auto parent = std::dynamic_pointer_cast<RowChangeOperation>(parent_.lock());
    assert(parent);

    auto pk = parent->primary_key();

    mysqlrouter::sqlstring sql("WHERE");
    bool cont = add_row_owner_check(&sql, true);
    if (join->to_many) {
      mysqlrouter::sqlstring where;
      for (const auto &c : join->column_mapping) {
        mysqlrouter::sqlstring one("!.!=?");
        one << c.first->table.lock()->table_alias << c.first->name
            << pk.at(c.second->name);
        where.append_preformatted_sep(" AND ", one);
      }
      sql.append_preformatted_sep(cont ? " AND " : " ", where);
    } else {
      mysqlrouter::sqlstring where;
      for (const auto &c : join->column_mapping) {
        mysqlrouter::sqlstring one("!.!=?");
        one << c.first->table.lock()->table_alias << c.first->name
            << pk.at(c.first->name);
        where.append_preformatted_sep(" AND ", one);
      }
      sql.append_preformatted_sep(cont ? " AND " : " ", where);
    }

    sql.append_preformatted(condition_from_rows(rows_to_keep_));
    return sql;
  } else {
    mysqlrouter::sqlstring sql("WHERE");
    bool cont = add_row_owner_check(&sql, true);
    sql.append_preformatted_sep(cont ? " AND " : "",
                                condition_from_rows(rows_to_keep_));
    return sql;
  }
}

mysqlrouter::sqlstring FilteredRowDelete::condition_from_rows(
    const std::vector<std::shared_ptr<RowUpdate>> &rows_to_keep) const {
  mysqlrouter::sqlstring sql;

  bool first = true;
  for (const auto &update : rows_to_keep) {
    mysqlrouter::sqlstring match_one;
    for (const auto &col : update->primary_key()) {
      mysqlrouter::sqlstring one("!.!=?");
      one << update->table_->table_alias << col.first << col.second;
      match_one.append_preformatted_sep(" AND ", one);
    }
    sql.append_preformatted(first ? mysqlrouter::sqlstring(" AND NOT ((")
                                  : mysqlrouter::sqlstring(") OR ("));

    sql.append_preformatted(match_one);
    first = false;
  }
  if (!first) sql.append_preformatted(mysqlrouter::sqlstring("))"));
  return sql;
}

//

void RefRowDelete::run(MySQLSession *session) {
  assert(!cond_update_ || cond_update_->inserted().has_value());

  if (table_->delete_allowed() &&
      (!cond_update_ || *cond_update_->inserted())) {
    query_ = delete_sql();
    execute(session);
  }
}

//

TableUpdater::TableUpdater(std::shared_ptr<entry::Object> object,
                           const ObjectRowOwnership &row_ownership_info)
    : m_object(object), m_row_ownership_info(row_ownership_info) {
  if (object->unnests_to_value) throw RestError("Object is not updatable");
}

namespace {

std::shared_ptr<entry::Column> get_generated_id_column(
    const entry::Table &table) {
  for (const auto &c : table.columns) {
    if (c->id_generation != entry::IdGenerationType::NONE) {
      return c;
    }
  }
  return nullptr;
}

std::shared_ptr<RowInsert> make_row_insert(
    std::shared_ptr<TableUpdater::Operation> parent,
    std::shared_ptr<entry::Table> table,
    const ObjectRowOwnership &row_ownership) {
  auto column = get_generated_id_column(*table);

  if (!column) {
    return std::make_shared<RowInsert>(parent, table, row_ownership);
  } else {
    switch (column->id_generation) {
      case entry::IdGenerationType::REVERSE_UUID:
        return std::make_shared<ReverseUuidRowInsert>(parent, table, column,
                                                      row_ownership);

      case entry::IdGenerationType::AUTO_INCREMENT:
        return std::make_shared<AutoIncRowInsert>(parent, table, column,
                                                  row_ownership);

      case entry::IdGenerationType::NONE:
        return std::make_shared<RowInsert>(parent, table, row_ownership);
    }

    assert(0);
    throw std::logic_error("internal error");
  }
}

void validate_scalar_value(const entry::Column &column,
                           const rapidjson::Value &value,
                           const std::string &jptr) {
  if (column.is_generated) {
    throw std::runtime_error(jptr + " is generated and cannot have a value");
  }

  if (value.IsNull()) {
    if (column.not_null) throw std::runtime_error(jptr + " cannot be NULL");
    return;
  }

  switch (column.type) {
    case entry::ColumnType::UNKNOWN:
      break;
    case entry::ColumnType::INTEGER:
      if ((!value.IsNumber() || value.IsDouble()) && !value.IsBool())
        throw std::runtime_error(jptr + " has invalid value type");
      break;
    case entry::ColumnType::DOUBLE:
      if (!value.IsDouble())
        throw std::runtime_error(jptr + " has invalid value type");
      break;
    case entry::ColumnType::BOOLEAN:
      if (!value.IsBool() && !value.IsInt())
        throw std::runtime_error(jptr + " has invalid value type");
      break;
    case entry::ColumnType::STRING:
      if (!value.IsString())
        throw std::runtime_error(jptr + " has invalid value type");
      break;
    case entry::ColumnType::BINARY:
      if (!value.IsString())
        throw std::runtime_error(jptr + " has invalid value type");
      break;
    case entry::ColumnType::GEOMETRY:
      if (!value.IsObject() && !value.IsString())
        throw std::runtime_error(jptr + " has invalid value type");
      break;
    case entry::ColumnType::JSON:
      // anything allowed for json
      break;
  }
}

template <typename T>
void process_object_field(std::shared_ptr<entry::DataField> field,
                          const rapidjson::Value &value, std::shared_ptr<T> op,
                          const std::string &jptr) {
  validate_scalar_value(*field->source, value,
                        join_json_pointer(jptr, field->name));

  mysqlrouter::sqlstring tmp("?");
  if (field->source->type == entry::ColumnType::JSON) {
    tmp << helper::json::to_string(value);
  } else if (field->source->type == entry::ColumnType::GEOMETRY) {
    if (value.IsString()) {
      tmp.reset("ST_AsGeoJSON(ST_GeomFromText(?,?))");
      tmp << value.GetString();
      tmp << field->source->srid;
    } else {
      tmp << helper::json::to_string(value);
    }
  } else if (value.IsBool()) {
    tmp << value.GetBool();
  } else {
    tmp << value;
  }
  op->on_value(field, tmp);
}

void process_post_object(std::shared_ptr<entry::Object> object,
                         const ObjectRowOwnership &row_ownership,
                         const rapidjson::Value &doc,
                         std::shared_ptr<RowInsert> insert,
                         const std::string &jptr);

void process_post_object_nested_field(const entry::ReferenceField &field,
                                      const ObjectRowOwnership &row_ownership,
                                      const rapidjson::Value &value,
                                      std::shared_ptr<RowInsert> insert,
                                      const std::string &jptr) {
  if (field.is_array()) {  // 1:n, the FK is at the referenced table
    if (!value.IsArray())
      throw std::runtime_error(join_json_pointer(jptr, field.name) +
                               " expected to be an Array");
    auto prefix = join_json_pointer(jptr, field.name);
    size_t i = 0;
    for (const auto &v : value.GetArray()) {
      process_post_object(field.nested_object, row_ownership, v,
                          insert->add_referencing_insert(field.ref_table()),
                          join_json_pointer(prefix, std::to_string(i++)));
    }
  } else {  // 1:1, the FK is at the base table
    if (value.IsArray())
      throw std::runtime_error(join_json_pointer(jptr, field.name) +
                               " is an Array but wasn't expected to be");

    if (value.IsNull()) {
      for (const auto &c : field.ref_table()->column_mapping) {
        insert->add_value(c.first, k_null);
      }
    } else {
      process_post_object(field.nested_object, row_ownership, value,
                          insert->add_referenced_insert(field.ref_table()),
                          join_json_pointer(jptr, field.name));
    }
  }
}

void process_post_object(std::shared_ptr<entry::Object> object,
                         const ObjectRowOwnership &row_ownership,
                         const rapidjson::Value &doc,
                         std::shared_ptr<RowInsert> insert,
                         const std::string &jptr) {
  if (!doc.IsObject())
    throw std::runtime_error(jptr + " expected to be an Object");

  auto base_table = insert->table();

  std::set<std::string> known_fields{"links", "_metadata"};

  for (auto field : object->fields) {
    if (!field->enabled) continue;

    auto member = doc.FindMember(field->name.c_str());
    if (auto dfield = std::dynamic_pointer_cast<entry::DataField>(field)) {
      if (auto field_table = std::dynamic_pointer_cast<entry::JoinedTable>(
              dfield->source->table.lock());
          field_table && field_table != base_table) {
        // unnested object field
        if (member == doc.MemberEnd()) {
          auto target = insert->add_referenced_insert(field_table);
          target->on_default_value(dfield);
        } else {
          if (!member->value.IsNull()) {
            auto target = insert->add_referenced_insert(field_table);
            process_object_field(dfield, member->value, target, jptr);
          }
          if (field->enabled) known_fields.insert(field->name);
        }
      } else {
        // regular object field
        if (member == doc.MemberEnd()) {
          insert->on_default_value(dfield);
        } else {
          process_object_field(dfield, member->value, insert, jptr);
          if (field->enabled) known_fields.insert(field->name);
        }
      }
    } else if (auto rfield =
                   std::dynamic_pointer_cast<entry::ReferenceField>(field)) {
      if (member != doc.MemberEnd()) {
        known_fields.insert(field->name);

        process_post_object_nested_field(*rfield, row_ownership, member->value,
                                         insert, jptr);
      } else {
        if (!rfield->is_array()) {
          static rapidjson::Value k_null(rapidjson::kNullType);

          // interpret missing REF value on insert as setting to NULL
          process_post_object_nested_field(*rfield, row_ownership, k_null,
                                           insert, jptr);
        }
      }
    }
  }
  // check invalid fields
  for (const auto &member : doc.GetObject()) {
    std::string member_name = member.name.GetString();

    if (known_fields.count(member_name) == 0) {
      throw RestError("Unknown field '" + member_name + "' in JSON document");
    }
  }
}

void safe_run(MySQLSession *session,
              const std::shared_ptr<TableUpdater::Operation> &op,
              MySQLSession::Transaction *transaction_started = nullptr) {
  MySQLSession::Transaction safe_transaction;
  if (!transaction_started) {
    const bool is_consisten_snapshot = true;
    safe_transaction =
        MySQLSession::Transaction(session, is_consisten_snapshot);
    transaction_started = &safe_transaction;
  }

  try {
    op->will_run(session);

    op->run(session);

    op->did_run(session);

    transaction_started->commit();
  } catch (...) {
    throw;
  }
}
}  // namespace

PrimaryKeyColumnValues TableUpdater::handle_post(
    MySQLSession *session, const rapidjson::Document &doc) {
  assert(doc.IsObject());

  auto root_insert =
      make_row_insert({}, get_base_table(), m_row_ownership_info);

  process_post_object(m_object, m_row_ownership_info, doc, root_insert, "/");

  safe_run(session, root_insert);

  m_affected += root_insert->affected();

  return root_insert->primary_key();
}

namespace {

void process_put_object(std::shared_ptr<entry::Object> object,
                        const ObjectRowOwnership &row_ownership,
                        const rapidjson::Value &doc,
                        std::shared_ptr<RowUpdate> update,
                        const std::string &jptr);

void process_put_object_nested_field(const entry::ReferenceField &field,
                                     const ObjectRowOwnership &row_ownership,
                                     const rapidjson::Value &value,
                                     std::shared_ptr<RowUpdate> update,
                                     const std::string &jptr) {
  if (field.is_array()) {  // 1:n, the FK is at the referenced table
    if (!value.IsArray())
      throw std::runtime_error(join_json_pointer(jptr, field.name) +
                               " expected to be an Array");

    std::vector<std::shared_ptr<RowUpdate>> nested_updates;
    std::string prefix = join_json_pointer(jptr, field.name);
    size_t i = 0;
    for (const auto &v : value.GetArray()) {
      auto nested_update = update->add_referencing_update(field.ref_table());
      process_put_object(field.nested_object, row_ownership, v, nested_update,
                         join_json_pointer(prefix, std::to_string(i++)));
      nested_updates.push_back(nested_update);
    }

    update->add_nested_delete(field.ref_table(), nested_updates);
  } else {  // 1:1, the FK is at the base table
    if (value.IsArray())
      throw std::runtime_error(join_json_pointer(jptr, field.name) +
                               " is an Array but wasn't expected to be");

    if (value.IsNull()) {
      static auto k_null = mysqlrouter::sqlstring("NULL");

      for (const auto &c : field.ref_table()->column_mapping) {
        update->add_value(c.first, k_null);
      }

      update->add_conditional_delete(field.ref_table(), nullptr);
    } else {
      auto child_update = update->add_referenced_update(field.ref_table());
      // delete row from ref_table if the child_update is an INSERT and not an
      // UPDATE
      // update->add_conditional_delete(field.ref_table(), child_update);

      process_put_object(field.nested_object, row_ownership, value,
                         child_update, join_json_pointer(jptr, field.name));
    }
  }
}

void process_put_object(std::shared_ptr<entry::Object> object,
                        const ObjectRowOwnership &row_ownership,
                        const rapidjson::Value &doc,
                        std::shared_ptr<RowUpdate> update,
                        const std::string &jptr) {
  if (!doc.IsObject()) {
    throw std::runtime_error(jptr + " expected to be an Object");
  }

  auto base_table = update->table();

  std::set<std::string> known_fields{"links", "_metadata"};

  for (auto field : object->fields) {
    if (!field->enabled) continue;

    if (auto dfield = std::dynamic_pointer_cast<entry::DataField>(field)) {
      auto member = doc.FindMember(field->name.c_str());
      if (!dfield->no_update) {
        if (auto field_table = std::dynamic_pointer_cast<entry::JoinedTable>(
                dfield->source->table.lock());
            field_table && field_table != base_table) {
          // unnested object field
          if (member == doc.MemberEnd()) {
            auto target = update->add_referenced_update(field_table);
            target->on_no_value(dfield);
          } else {
            if (!member->value.IsNull()) {
              auto target = update->add_referenced_update(field_table);
              process_object_field(dfield, member->value, target, jptr);
            }
            if (field->enabled) known_fields.insert(field->name);
          }
        } else {
          // regular object field
          if (member == doc.MemberEnd()) {
            update->on_no_value(dfield);
          } else {
            // PK of the root object in a POST comes from the request, so don't
            // allow overriding it in the document
            if (!(dfield->source->is_primary && jptr.size() == 1))
              process_object_field(dfield, member->value, update, jptr);
            if (field->enabled) known_fields.insert(field->name);
          }
        }
      }
    } else if (auto rfield =
                   std::dynamic_pointer_cast<entry::ReferenceField>(field)) {
      auto member = doc.FindMember(field->name.c_str());
      if (member != doc.MemberEnd()) {
        known_fields.insert(field->name);
        process_put_object_nested_field(*rfield, row_ownership, member->value,
                                        update, jptr);
      } else {
        throw RestError("Document is missing field '" + field->name + "'");
      }
    }
  }
  // check invalid fields
  for (const auto &member : doc.GetObject()) {
    std::string member_name = member.name.GetString();

    if (known_fields.count(member_name) == 0) {
      throw RestError("Unknown field '" + member_name + "' in JSON document");
    }
  }
}

}  // namespace

PrimaryKeyColumnValues TableUpdater::handle_put(
    MySQLSession *session, const rapidjson::Document &doc,
    const PrimaryKeyColumnValues &pk_values) {
  const bool is_consisten_snapshot = true;
  assert(doc.IsObject());

  check_primary_key(pk_values);

  MySQLSession::Transaction transaction{session, is_consisten_snapshot};

  std::shared_ptr<RowUpdate> root_update;
  try {
    check_etag_and_lock_rows(session, doc, pk_values);

    root_update =
        std::make_shared<RowUpdate>(std::shared_ptr<Operation>(nullptr),
                                    m_object, pk_values, m_row_ownership_info);

    process_put_object(m_object, m_row_ownership_info, doc, root_update, "/");
  } catch (...) {
    throw;
  }

  // On success it commits.
  safe_run(session, root_update, &transaction);

  m_affected += root_update->affected();

  return root_update->primary_key();
}

namespace {

void process_delete_object(std::shared_ptr<entry::Object> object,
                           const ObjectRowOwnership &row_ownership,
                           std::shared_ptr<RowDeleteOperation> del,
                           const std::string &jptr);

void process_delete_object_nested_field(const entry::ReferenceField &field,
                                        const ObjectRowOwnership &row_ownership,
                                        std::shared_ptr<RowDeleteOperation> del,
                                        const std::string &jptr) {
  if (field.is_array()) {  // 1:n, the FK is at the referenced table
    process_delete_object(field.nested_object, row_ownership,
                          del->add_referencing_delete(field.ref_table()),
                          join_json_pointer(jptr, field.name));

  } else {  // 1:1, the FK is at the base table
    process_delete_object(field.nested_object, row_ownership,
                          del->add_referenced_delete(field.ref_table()),
                          join_json_pointer(jptr, field.name));
  }
}

void process_delete_object(std::shared_ptr<entry::Object> object,
                           const ObjectRowOwnership &row_ownership,
                           std::shared_ptr<RowDeleteOperation> del,
                           const std::string &jptr) {
  auto base_table = del->table();

  for (auto field : object->fields) {
    if (!field->enabled) continue;

    if (auto dfield = std::dynamic_pointer_cast<entry::DataField>(field)) {
      if (auto field_table = std::dynamic_pointer_cast<entry::JoinedTable>(
              dfield->source->table.lock());
          field_table && field_table != base_table) {
        // unnested
        del->add_referenced_delete(field_table);
      }
    } else if (auto rfield =
                   std::dynamic_pointer_cast<entry::ReferenceField>(field)) {
      process_delete_object_nested_field(*rfield, row_ownership, del, jptr);
    }
  }
}
}  // namespace

uint64_t TableUpdater::handle_delete(MySQLSession *session,
                                     const PrimaryKeyColumnValues &pk_values) {
  // TODO(alfredo) - handle circular references

  if (!get_base_table()->delete_allowed()) {
    return 0;
  }
  check_primary_key(pk_values);

  //  do {
  auto root_delete = std::make_shared<RowDelete>(
      std::shared_ptr<Operation>(nullptr), get_base_table(), pk_values,
      m_row_ownership_info);

  process_delete_object(m_object, m_row_ownership_info, root_delete, "/");

  safe_run(session, root_delete);
  //  } while ();

  m_affected += root_delete->affected();

  return root_delete->affected();
}

uint64_t TableUpdater::handle_delete(MySQLSession *session,
                                     const FilterObjectGenerator &filter) {
  auto result = filter.get_result();
  if (result.is_empty())
    throw std::runtime_error("Filter must contain valid JSON object.");
  if (filter.has_order())
    throw std::runtime_error("Filter must not contain ordering informations.");

  if (!get_base_table()->delete_allowed()) {
    return 0;
  }

  // Note: user given filter may try to filter by the owner_id to access rows
  // they're not allowed, but since the row_owner check is also done, worst
  // case the WHERE will match nothing

  auto root_delete = std::make_shared<ConditionalRowDelete>(
      std::shared_ptr<Operation>(nullptr), get_base_table(), result,
      m_row_ownership_info);

  process_delete_object(m_object, m_row_ownership_info, root_delete, "/");

  safe_run(session, root_delete);

  return root_delete->affected();
}

std::shared_ptr<entry::BaseTable> TableUpdater::get_base_table() const {
  return std::dynamic_pointer_cast<entry::BaseTable>(
      m_object->base_tables.front());
}

void TableUpdater::check_primary_key(const PrimaryKeyColumnValues &pk_values) {
  auto pk_cols = get_base_table()->primary_key();

  for (const auto &col : pk_cols) {
    if (pk_values.find(col->name) == pk_values.end()) {
      if (!m_row_ownership_info.is_owner_id(*col))
        throw RestError("Missing primary key column value");
    }
  }

  if (std::any_of(pk_values.begin(), pk_values.end(), [pk_cols](const auto &c) {
        return std::find_if(pk_cols.begin(), pk_cols.end(),
                            [c](const auto &col) {
                              return c.first == col->name;
                            }) == pk_cols.end();
      })) {
    throw RestError("Invalid primary key column");
  }
}

std::string TableUpdater::compute_etag_and_lock_rows(
    MySQLSession *session, const PrimaryKeyColumnValues &pk_values) const {
  JsonQueryBuilder qb({}, true, true);
  qb.process_object(m_object);

  auto query = qb.query_one(pk_values);
  auto row = session->query_one(query);

  return compute_checksum(m_object, std::string_view((*row)[0]));
}

void TableUpdater::check_etag_and_lock_rows(
    MySQLSession *session, const rapidjson::Value &doc,
    const PrimaryKeyColumnValues &pk_values) const {
  if (doc.HasMember("_metadata")) {
    const auto &metadata = doc["_metadata"];
    if (metadata.IsObject() && metadata.HasMember("etag")) {
      const auto &etag = metadata["etag"];
      if (etag.IsString()) {
        auto checksum = compute_etag_and_lock_rows(session, pk_values);

        if (etag.GetString() == checksum) {
          return;
        } else {
          throw interface::ETagMismatch();
        }
      }
      throw RestError("Invalid etag");
    }
  }
  // if etag is missing, then just don't validate
}

}  // namespace database
}  // namespace mrs
