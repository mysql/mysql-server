/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#include "sql/duckdb/duckdb_table.h"
#include "lex_string.h"
#include "scope_guard.h"
#include "sql/create_field.h"  // Create_field
#include "sql/dd/properties.h"
#include "sql/dd/types/index_element.h"
#include "sql/dd/types/table.h"
#include "sql/derror.h"
#include "sql/duckdb/duckdb_sql_quote.h"
#include "sql/handler.h"
#include "sql/key_spec.h"
#include "sql/mysqld.h"
#include "sql/sql_class.h"
#include "sql/sql_gipk.h"
#include "sql/sql_lex.h"
#include "sql/table.h"

#include <sstream>
#include "mysql/components/services/log_builtins.h"
#ifdef HAVE_DUCKDB
#include "sql/duckdb/duckdb_context.h"
#endif
#ifdef HAVE_DUCKDB
#include "sql/duckdb/duckdb_query.h"
#endif
#include "sql/sql_partition.h"
#include "sql/sql_table_ext.h"

namespace myduck {

/** Process create fields.
  @param[in]       thd         Thread context
  @param[in, out]  alter_info  Lists of fields, keys to be changed, added
                               or dropped.
*/
static void process_fields(THD *thd, Alter_info *alter_info) {
  List_iterator<Create_field> it(alter_info->create_list);

  Create_field *sql_field;
  for (; (sql_field = it++);) {
    /* Unmark auto_increment. */
    if (!duckdb_support_auto_increment &&
        (sql_field->auto_flags & Field::NEXT_NUMBER)) {
      sql_field->auto_flags &= ~(Field::NEXT_NUMBER);
      // push_warning_printf(thd, Sql_condition::SL_WARNING,
      //                     ER_DUCKDB_TABLE_AUTO_INCREMENT_REMOVED,
      //                     ER_THD(thd,
      //                     ER_DUCKDB_TABLE_AUTO_INCREMENT_REMOVED),
      //                     sql_field->field_name);
    }

    /* Unmark ON_UPDATE_NOW. */
    if (sql_field->auto_flags & Field::ON_UPDATE_NOW) {
      sql_field->auto_flags &= ~(Field::ON_UPDATE_NOW);
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_DUCKDB_TABLE_ON_UPDATE_NOW_REMOVED,
                          ER_THD(thd, ER_DUCKDB_TABLE_ON_UPDATE_NOW_REMOVED),
                          sql_field->field_name);
    }
  }
}

/** Check current index is functional index.
  @param[in]  create  columns in the table

  @return true if has functional index, false otherwise.
*/
static bool is_functional_index(const Key_spec *key,
                                List<Create_field> &create [[maybe_unused]]) {
  for (size_t j = 0; j < key->columns.size(); ++j) {
    Key_part_spec *key_part_spec = key->columns[j];
    if (!key_part_spec->has_expression()) {
      continue;
    }
    return true;
  }

  return false;
}

/**
  Check if key is a unique key without nullable part.
*/
static bool is_not_nullable_uk(Key_spec *key, Alter_info *alter_info) {
  if (key->type != KEYTYPE_UNIQUE) {
    return false;
  }

  for (uint i = 0; i < key->columns.size(); i++) {
    const Key_part_spec *col = key->columns[i];
    if (col->get_field_name() == nullptr) {
      return false;
    }

    List_iterator<Create_field> it(alter_info->create_list);
    Create_field *sql_field = nullptr;
    while ((sql_field = it++) &&
           my_strcasecmp(system_charset_info, col->get_field_name(),
                         sql_field->field_name))
      ;
    if (sql_field != nullptr && sql_field->is_nullable) {
      return false;
    }
  }

  return true;
}

/** Select primary key from all keys.
  The selected key will be new primary key. The alternative order is: primary
  key, RDS implicit primary key, candidate unique key.
  @param[in]   thd            Thread context
  @param[in]   alter_info     Lists of fields, keys to be changed, added
                              or dropped.
  @param[out]  selected_key   Selected primary key.
  @return false if successful, true if error happens.
*/
static bool select_primary_key(THD *thd, Alter_info *alter_info,
                               Key_spec **selected_key) {
  Key_spec *pk = nullptr;
  Key_spec *candidate_uk = nullptr;

  for (const auto &key : alter_info->key_list) {
    /*
      Functional indexes generate virtual columns, which cause the column
      position recorded in the binlog to shift, causing replication
      interruption.
    */
    if (is_functional_index(key, alter_info->create_list)) {
      my_error(ER_DUCKDB_TABLE_STRUCT_INVALID, MYF(0),
               "functional index is not supported");
      return true;
    }

    if (key->type == KEYTYPE_PRIMARY) {
      if (pk != nullptr) {
        if (!thd->slave_thread) {
          my_error(ER_MULTIPLE_PRI_KEY, MYF(0));
          return true;
        }
      }
      pk = key;
    } else if (is_not_nullable_uk(key, alter_info) && !candidate_uk) {
      candidate_uk = key;
    }
  }

  if (pk)
    *selected_key = pk;
  else if (candidate_uk)
    *selected_key = candidate_uk;
  else
    *selected_key = nullptr;

  return false;
}

/** Construct a new primary key based on the alternative primary key.

  If there is a primary key, only the primary key is retained. If there is no
  primary key but candidate UK exists, candidate UK is upgraded to the
  primary key.

  @param[in]  thd           Thread context
  @param[in]  alter_info    Lists of fields, keys to be changed, added
                            or dropped.
  @param[in]  selected_key  Selected primary key.
  @return false if success, true if error happens.
*/
static bool create_new_primary_key(THD *thd, Alter_info *alter_info,
                                   Key_spec *selected_key) {
  Mem_root_array<Key_spec *> new_key_list(thd->mem_root);
  if (selected_key == nullptr) {
    goto finish;
  }

  if (selected_key->type == KEYTYPE_PRIMARY) {
    new_key_list.push_back(selected_key);
  } else {
    List<Key_part_spec> key_col_list;
    for (uint j = 0; j < selected_key->columns.size(); j++) {
      Key_part_spec *new_key_part = selected_key->columns[j];
      key_col_list.push_back(new_key_part);
    }

    Key_spec *new_pk = new (thd->mem_root)
        Key_spec(thd->mem_root, KEYTYPE_PRIMARY, NULL_CSTR,
                 &default_key_create_info, false, true, key_col_list);
    if (new_pk == nullptr) {
      my_error(ER_DA_OOM, MYF(0));
      return true;
    }
    push_warning_printf(
        thd, Sql_condition::SL_WARNING, ER_DUCKDB_TABLE_INDEX_UPGRADED,
        ER_THD(thd, ER_DUCKDB_TABLE_INDEX_UPGRADED), selected_key->name.str);
    new_key_list.push_back(new_pk);
  }

finish:
  alter_info->key_list.clear();
  if (alter_info->key_list.resize(new_key_list.size())) return true;
  std::copy(new_key_list.begin(), new_key_list.end(),
            alter_info->key_list.begin());
  return false;
}

/** Remove flag bit in alter_info if exists.
    If removed, warning will be reported.
  @param[in]       thd               Thread context
  @param[in]       flag              Flag  to be removed
  @param[in]       operation         Operation name
  @param[in, out]  alter_info        Lists of fields, keys to be changed, added
                                     or dropped.
*/
static void remove_flag_if_exists(THD *thd, ulonglong flag,
                                  const char *operation,
                                  Alter_info *alter_info) {
  if (alter_info->flags & flag) {
    alter_info->flags &= ~flag;
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_DUCKDB_ALTER_FLAG_REMOVED,
                        ER_THD(thd, ER_DUCKDB_ALTER_FLAG_REMOVED), operation);
  }
}

/** Remove flags of add index and constraint operations if need.

  @param[in]       thd               Thread context
  @param[in, out]  alter_info        Lists of fields, keys to be changed, added
                                     or dropped.
  @param[in]       has_selected_key  If there is a selected primary key.
*/
static void remove_add_key_and_constraint_flags(THD *thd,
                                                Alter_info *alter_info,
                                                bool has_selected_key) {
  /* MAY_IGNORED_ALTER_FLAGS */
  if (!has_selected_key) {
    remove_flag_if_exists(thd, Alter_info::ALTER_ADD_INDEX, "ADD INDEX",
                          alter_info);
  }

  /* IGNORED_ALTER_FLAGS */
  remove_flag_if_exists(thd, Alter_info::ADD_FOREIGN_KEY, "ADD FOREIGN KEY",
                        alter_info);
  remove_flag_if_exists(thd, Alter_info::ADD_CHECK_CONSTRAINT,
                        "ADD CHECK CONSTRAINT", alter_info);
}

/** Called by mysql_prepare_create_table */
bool prepare_create_duckdb_table(THD *thd, HA_CREATE_INFO *create_info,
                                 Alter_info *alter_info) {
  if (create_info->db_type->db_type != DB_TYPE_DUCKDB) {
    return false;
  }

  /* Remove auto_increment. */
  process_fields(thd, alter_info);

  /* Determine one key as primary key. */
  Key_spec *selected_key = nullptr;
  if (select_primary_key(thd, alter_info, &selected_key)) {
    return true;
  }

  /* We need to remove all non-primary key indexes and constraints. */
  std::for_each(
      alter_info->key_list.begin(), alter_info->key_list.end(),
      [&](const Key_spec *key) {
        if (key != selected_key) {
          push_warning_printf(
              thd, Sql_condition::SL_WARNING, ER_DUCKDB_TABLE_INDEX_REMOVED,
              ER_THD(thd, ER_DUCKDB_TABLE_INDEX_REMOVED), key->name.str);
        }
      });
  alter_info->check_constraint_spec_list.clear();

  if (create_new_primary_key(thd, alter_info, selected_key)) {
    return true;
  }

  /* Remove flags of add key and constraint operations. */
  remove_add_key_and_constraint_flags(thd, alter_info,
                                      (selected_key != nullptr));

  return false;
}

bool is_duckdb_table(const TABLE *table) {
  if (table == nullptr || table->file == nullptr ||
      table->file->ht == nullptr) {
    return false;
  }

  bool res = (table->file->ht->db_type == DB_TYPE_DUCKDB);

  return res;
}

bool is_supported_ddl(Alter_info *alter_info, TABLE *table) {
  ulonglong flags = alter_info->flags;

  /* Do nothing. */
  if (flags == 0) {
    return true;
  }

  if (flags & Alter_info::ALTER_COLUMN_VISIBILITY) {
    for (const Alter_column *alter_column : alter_info->alter_list) {
      if (alter_column->change_type() !=
          Alter_column::Type::SET_COLUMN_INVISIBLE)
        continue;

      my_error(ER_DUCKDB_ALTER_OPERATION_NOT_SUPPORTED, MYF(0),
               "SET COLUMN INVISIBLE");
      return false;
    }
  }

  if (flags & Alter_info::ALTER_ADD_COLUMN) {
    List_iterator<Create_field> new_field_it(alter_info->create_list);
    Create_field *new_field;

    while ((new_field = new_field_it++)) {
      /* Skip modify/change column */
      if (new_field->change != nullptr) {
        continue;
      }

      if (new_field->auto_flags & Field::NEXT_NUMBER) {
        my_error(ER_DUCKDB_ALTER_OPERATION_NOT_SUPPORTED, MYF(0),
                 "ADD AUTO_INCREMENT COLUMN");
        return false;
      }
    }
  }

  /*
    Now we support some simple partition operations for DuckDB. In DuckDB,
    partition table will be converted to non-partition table. Therefore, the
    behavior of the corresponding DDL will also change.

    ALTER_ADD_PARTITION: no operation.
    ALTER_COALESCE_PARTITION: no operation.
    ALTER_REORGANIZE_PARTITION: no operation.
    ALTER_PARTITION: no operation.
    ALTER_ADMIN_PARTITION: no operation.
    ALTER_TABLE_REORG: no operation.
    ALTER_REBUILD_PARTITION: no operation.
    ALTER_ALL_PARTITION: no operation.
    ALTER_REMOVE_PARTITIONING: no operation.

    ALTER_DROP_PARTITION: delete data that matches the partition definition.
    ALTER_TRUNCATE_PARTITION: delete data that matches the partition definition.

    ALTER_EXCHANGE_PARTITION: no support.

    TODO:
    1. drop/truncate partition are supported only for range/list partition.
    2. truncate subpartition is not supported.
    3. exchange partition is not supported, which can be implemented by deleting
    old data ane inserting new data.
  */
  if (table->part_info != nullptr) {
    if (flags & Alter_info::ALTER_EXCHANGE_PARTITION) {
      my_error(ER_DUCKDB_ALTER_OPERATION_NOT_SUPPORTED, MYF(0),
               "EXCHANGE PARTITION");
      return false;
    }

    ulonglong drop_or_truncate =
        Alter_info::ALTER_DROP_PARTITION | Alter_info::ALTER_TRUNCATE_PARTITION;

    if ((flags & drop_or_truncate) &&
        table->part_info->part_type == partition_type::HASH) {
      my_error(ER_DUCKDB_ALTER_OPERATION_NOT_SUPPORTED, MYF(0),
               "DROP/TRUNCATE PARTITION on HASH/KEY partitions");
      return false;
    }
  }

  /*
    We do not to check Alter_info::ALTER_DISCARD_TABLESPACE,
    Alter_info::ALTER_IMPORT_TABLESPACE and Alter_info::ANY_ENGINE_ATTRIBUTE
    because ENGINE  'DuckDB' do not support it now.
  */

  return true;
}

void prepare_alter_duckdb_table(THD *thd, HA_CREATE_INFO *create_info,
                                Alter_info *alter_info) {
  if (create_info->db_type->db_type != DB_TYPE_DUCKDB) {
    return;
  }

  /*
    If drop primary key, we need to try to drop it because there is existed
    primary key on duckdb table.
  */
  bool drop_primary_key = false;
  for (auto it = alter_info->drop_list.begin();
       it != alter_info->drop_list.end();
       /* no op */) {
    Alter_drop::drop_type type = (*it)->type;
    switch (type) {
      case Alter_drop::COLUMN:
        ++it;
        break;

      case Alter_drop::FOREIGN_KEY:
      case Alter_drop::CHECK_CONSTRAINT:
        alter_info->drop_list.erase(it);
        break;

      case Alter_drop::KEY:
      case Alter_drop::ANY_CONSTRAINT:
        if (my_strcasecmp(system_charset_info, (*it)->name, "PRIMARY") == 0) {
          drop_primary_key = true;
          ++it;
        } else {
          alter_info->drop_list.erase(it);
        }
        break;

      default:
        break;
    }
  }
  /* MAY_IGNORED_ALTER_FLAGS */
  if (!drop_primary_key) {
    remove_flag_if_exists(thd, Alter_info::ALTER_DROP_INDEX, "DROP INDEX",
                          alter_info);
    remove_flag_if_exists(thd, Alter_info::DROP_ANY_CONSTRAINT,
                          "DROP ANY CONSTRAINT", alter_info);
  }

  /* IGNORED_ALTER_FLAGS */
  remove_flag_if_exists(thd, Alter_info::DROP_FOREIGN_KEY, "DROP FOREIGN KEY",
                        alter_info);

  remove_flag_if_exists(thd, Alter_info::ALTER_RENAME_INDEX, "RENAME INDEX",
                        alter_info);
  alter_info->alter_rename_key_list.clear();

  remove_flag_if_exists(thd, Alter_info::ALTER_INDEX_VISIBILITY,
                        "ALTER INDEX VISIBILITY", alter_info);
  alter_info->alter_index_visibility_list.clear();

  remove_flag_if_exists(thd, Alter_info::ALTER_ORDER, "ALTER ORDER",
                        alter_info);

  remove_flag_if_exists(thd, Alter_info::ADD_CHECK_CONSTRAINT,
                        "ADD CHECK CONSTRAINT", alter_info);
  alter_info->check_constraint_spec_list.clear();

  remove_flag_if_exists(thd, Alter_info::DROP_CHECK_CONSTRAINT,
                        "DROP CHECK CONSTRAINT", alter_info);
  remove_flag_if_exists(thd, Alter_info::ENFORCE_CHECK_CONSTRAINT,
                        "ENFORCE CHECK CONSTRAINT", alter_info);
  remove_flag_if_exists(thd, Alter_info::SUSPEND_CHECK_CONSTRAINT,
                        "SUSPEND CHECK CONSTRAINT", alter_info);
  remove_flag_if_exists(thd, Alter_info::ENFORCE_ANY_CONSTRAINT,
                        "ENFORCE ANY CONSTRAINT", alter_info);
  remove_flag_if_exists(thd, Alter_info::SUSPEND_ANY_CONSTRAINT,
                        "SUSPEND ANY CONSTRAINT", alter_info);

  alter_info->alter_constraint_enforcement_list.clear();
}

bool report_duckdb_table_struct_error(const std::string &err_msg) {
  my_error(ER_DUCKDB_TABLE_STRUCT_INVALID, MYF(0), err_msg.c_str());
  return true;
}

/** Check current index is functional index.

  @param[in]  index  index to be checked.
  @return true if functional index, false otherwise.
*/
static bool is_functional_index(const dd::Index *index) {
  for (const dd::Index_element *e : index->elements()) {
    const dd::Column &c = e->column();
    if (c.hidden() == dd::Column::enum_hidden_type::HT_HIDDEN_SQL &&
        !c.is_generation_expression_null()) {
      return true;
    }
  }
  return false;
}

bool precheck_convert_to_duckdb(const dd::Table *dd_table) {
  /* Table level */
  if (dd_table->partition_type() != dd::Table::PT_NONE) {
    return report_duckdb_table_struct_error("partition table is not support");
  }

  /* Index level*/
  bool has_candidate_key = false;
  for (const dd::Index *i : dd_table->indexes()) {
    if (!my_strcasecmp(system_charset_info, i->name().c_str(), "PRIMARY")) {
      /* DB_ROW_ID */
      if (i->is_hidden()) continue;
      assert(i->type() == dd::Index::IT_PRIMARY);
      /*
        There are currently no indexes in DuckDB, we don't care whether they are
        prefix/partial indexes or not.
      */
      has_candidate_key |= true;
    }
    /** TODO: BLOB prefix */
    has_candidate_key |= i->is_candidate_key();

    /*
      Functional indexes generate virtual columns, which cause the column
      position recorded in the binlog to shift, causing replication
      interruption.
    */
    if (is_functional_index(i)) {
      return report_duckdb_table_struct_error(
          "functional index is not supported");
    }
  }
  if (!has_candidate_key && duckdb_require_primary_key) {
    my_error(ER_REQUIRES_PRIMARY_KEY, MYF(0));
    return true;
  }

  /* Column level */
  for (const dd::Column *c : dd_table->columns()) {
    std::string name_str = c->name().c_str();
    if (c->hidden() != dd::Column::enum_hidden_type::HT_VISIBLE &&
        c->hidden() != dd::Column::enum_hidden_type::HT_HIDDEN_SE &&
        !is_generated_invisible_primary_key_column_name(name_str.c_str())) {
      return report_duckdb_table_struct_error(
          "invisible column is not supported");
    }

    if (c->type() == dd::enum_column_types::GEOMETRY) {
      return report_duckdb_table_struct_error(
          "geometry column is not supported");
    }
  }

  return false;
}

/** Report error message for unsupported delete operation.
  @return true always.
*/
inline static bool report_unsupported_delete() {
  my_error(ER_DUCKDB_DATA_IMPORT_MODE, MYF(0),
           "Only DELETE operations with "
           "equality conditions on the primary key are permitted, where the "
           "right-hand "
           "side is a constant value");
  return true;
}

/**
  Fill single field of primary key.
  NOTE: only equality conditions on the primary key are permitted, where the
  right-hand side is a constant value.

  @param[in]         item  Item_func_eq
  @param[in, table]  table  TABLE object
  @return false if success, otherwise true.
*/
inline static bool fill_single_pk_field(Item_func_eq *item, TABLE *table) {
  if (item->argument_count() != 2 ||
      item->arguments()[0]->type() != Item::FIELD_ITEM ||
      !item->arguments()[1]->basic_const_item()) {
    return report_unsupported_delete();
  }

  Item_field *ifield = down_cast<Item_field *>(item->arguments()[0]);
  Field *field = ifield->field;
  assert(field->table == table);
  Item *ivalue = item->arguments()[1];

  /* If field belongs to primary key, the bit will be set before. */
  if (!bitmap_is_set(&table->duckdb_pk_set, field->field_index())) {
    my_error(ER_DUCKDB_DATA_IMPORT_MODE, MYF(0),
             "The specified fields include non-primary key fields or the "
             "field is specified multiple times");
    return true;
  }

  if (ivalue->save_in_field(field, true) != TYPE_OK) {
    my_error(ER_DUCKDB_DATA_IMPORT_MODE, MYF(0), "Failed to fill field");
    return true;
  }

  /* Filled, clear bit of current field in primary key. */
  bitmap_clear_bit(&table->duckdb_pk_set, field->field_index());

  return false;
}

/**
  Fill multi fields of primary key.
  @param[in]         item  Item_cond_and
  @param[in, table]  table  TABLE object
  @return false if success, otherwise true.
*/
inline static bool fill_multi_pk_fields(Item_cond_and *item, TABLE *table) {
  List_iterator<Item> li(*item->argument_list());
  Item *it;

  while ((it = li++)) {
    if (it->type() != Item::FUNC_ITEM ||
        down_cast<Item_func *>(it)->functype() != Item_func::EQ_FUNC) {
      return report_unsupported_delete();
    }

    auto it_eq = down_cast<Item_func_eq *>(it);
    if (fill_single_pk_field(it_eq, table)) {
      return true;
    }
  }

  return false;
}

/** Check if all fields of primary key are filled.
  @param[in]  table  TABLE object
  @return true if all fields are filled, false otherwise
*/
inline static bool duckdb_pk_set_filled(TABLE *table) {
  if (bitmap_is_clear_all(&table->duckdb_pk_set)) {
    return true;
  }

  my_error(ER_DUCKDB_DATA_IMPORT_MODE, MYF(0),
           "The full primary key value needs to be specified");
  return false;
}

bool fill_pk_fields(THD *thd, TABLE *table) {
  LEX *lex = thd->lex;
  if (lex == nullptr || lex->query_block == nullptr ||
      lex->query_block->where_cond() == nullptr) {
    return report_unsupported_delete();
  }

  Item *where_cond = lex->query_block->where_cond();

  my_bitmap_map *save_write_bitmap =
      tmp_use_all_columns(table, table->write_set);

  assert(table->s->keys == 1);
  assert(bitmap_is_clear_all(&table->duckdb_pk_set));
  KEY *pk = table->key_info;
  KEY_PART_INFO *key_part = pk->key_part;
  KEY_PART_INFO *key_part_end = key_part + pk->user_defined_key_parts;

  /* Mark all bits of primary key fields before filling. */
  for (; key_part < key_part_end; key_part++) {
    uint16_t field_index = key_part->field->field_index();
    bitmap_set_bit(&table->duckdb_pk_set, field_index);
  }

  enum_check_fields save_check_for_truncated_fields =
      thd->check_for_truncated_fields;
  thd->check_for_truncated_fields = CHECK_FIELD_WARN;

  auto guard = create_scope_guard([&]() {
    table->write_set->bitmap = save_write_bitmap;
    bitmap_clear_all(&table->duckdb_pk_set);
    thd->check_for_truncated_fields = save_check_for_truncated_fields;
  });

  /* Single-column primary key. */
  if (where_cond->type() == Item::FUNC_ITEM &&
      down_cast<Item_func *>(where_cond)->functype() == Item_func::EQ_FUNC) {
    return fill_single_pk_field(down_cast<Item_func_eq *>(where_cond), table) ||
           !duckdb_pk_set_filled(table);
  }

  /* Composite primary key. */
  if (where_cond->type() == Item::COND_ITEM &&
      down_cast<Item_cond *>(where_cond)->functype() ==
          Item_func::COND_AND_FUNC) {
    return fill_multi_pk_fields(down_cast<Item_cond_and *>(where_cond),
                                table) ||
           !duckdb_pk_set_filled(table);
  }

  return report_unsupported_delete();
}

void cleanup_tmp_table(THD *thd, const char *db, const char *tmp_table_name) {
#ifdef HAVE_DUCKDB
  std::string schema_name(db);
  std::string table_name(tmp_table_name);

  std::ostringstream query;
  query << "USE " << myduck::quote_duckdb_ident(schema_name) << ";";
  query << "DROP TABLE IF EXISTS " << myduck::quote_duckdb_ident(table_name)
        << ";";
  duckdb_query(thd, query.str());
  thd->get_duckdb_context()->delete_appender(schema_name, table_name);

  duckdb_query(thd, "COMMIT");
  duckdb_query(thd, "BEGIN");

  std::string message =
      "Cleanup DuckDB tmp table: " + schema_name + "." + table_name;
  LogErr(INFORMATION_LEVEL, ER_DUCKDB, message.c_str());
#else
  (void)thd;
  (void)db;
  (void)tmp_table_name;
#endif
}

/** Get partitions to delete.

  @NOTE: Truncate subpartition is allowed by MySQL but it is not supported
  for DuckDB.

  @param[in]  part_info  partition information
  @param[out] parts      partitions to delete
  @param[in]  truncate   true if truncate partition

  @return false if success, otherwise true.
*/
static bool get_parts_to_delete(partition_info *part_info,
                                std::vector<partition_element *> &parts,
                                bool truncate [[maybe_unused]]) {
  partition_element *pe = nullptr;
  uint part_id = 0;
  List_iterator<partition_element> it(part_info->partitions);

  if (part_info->is_sub_partitioned()) {
    partition_element *head_pe;
    while ((head_pe = it++)) {
      List_iterator<partition_element> it2(head_pe->subpartitions);
      bool first_set = false;
      bool first_value = false;
      while ((pe = it2++)) {
        bool is_set = bitmap_is_set(&part_info->read_partitions, part_id);
        if (!first_set) {
          first_set = true;
          first_value = is_set;
        } else {
          if (is_set != first_value) {
            /* Truncate sub-partition is allowed. */
            assert(truncate);
            my_error(ER_DUCKDB_ALTER_OPERATION_NOT_SUPPORTED, MYF(0),
                     "TRUNCATE SUBPARTITION");
            return true;
          }
        }
        part_id++;
      }

      if (first_value) {
        parts.push_back(head_pe);
      }
    }
  } else {
    while ((pe = it++)) {
      if (bitmap_is_set(&part_info->read_partitions, part_id)) {
        parts.push_back(pe);
      }
      part_id++;
    }
  }

  return false;
}

bool generate_delete_from_partition(partition_info *part_info, bool truncate,
                                    std::string &query) {
  assert(part_info->part_type != partition_type::HASH);

  std::vector<partition_element *> partitions_to_delete;
  if (get_parts_to_delete(part_info, partitions_to_delete, truncate)) {
    return true;
  }

  uint buf_length = 0;
  char *delete_sql = generate_partition_syntax_for_delete(
      part_info, partitions_to_delete, &buf_length);
  query.append(delete_sql, buf_length);

  return false;
}

}  // namespace myduck
