/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/sql_foreign_key_constraint.h"

#include <set>

#include "scope_guard.h"
#include "sql/dd/dd.h"  // dd::get_dictionary
#include "sql/dd/dd_table.h"
#include "sql/dd/dictionary.h"                 // dd::get_dictionary
#include "sql/dd/types/foreign_key.h"          // dd::Foreign_key
#include "sql/dd/types/foreign_key_element.h"  // dd::Foreign_key
#include "sql/field.h"
#include "sql/histograms/table_histograms.h"
#include "sql/key.h"
#include "sql/mysqld.h"
#include "sql/sql_base.h"
#include "sql/sql_class.h"
#include "sql/table.h"                     // TABLE_SHARE_FOREIGN_KEY_INFO
#include "sql/table_trigger_dispatcher.h"  // Table_trigger_dispatcher
#include "sql/trigger_chain.h"

/**
  With the ON CASCADE DELETE/UPDATE clause, deleting from the parent table
  can trigger recursive cascading calls. This defines the maximum number
  of such cascading deletes or updates allowed. If this limit is exceeded,
  the delete operation on the parent table will fail, and the user must drop
  the excessive foreign key constraint before proceeding.
*/
constexpr uint32_t FK_MAX_CASCADE_DEPTH = 15;

/**
  With the ON CASCADE DELETE/UPDATE clause and triggers on child tables,
  multiple foreign key chains can form. This constant defines the maximum
  number of tables allowed across such cascades. Exceeding the limit causes
  the parent-table operation to fail.
*/
constexpr uint32_t FK_MAX_TABLES_IN_CASCADE_CHAIN = 30;

/**
  Class to store all foreign key names during CASCADE. Used to identify
  circular referencing.
*/
class Foreign_key_chain {
 private:
  using Name_pair = std::multiset<std::pair<const char *, const char *>>;
  using Visited_keys =
      std::multiset<std::tuple<const char *, const char *, int>>;

  Name_pair m_foreign_keys;
  Visited_keys m_visited_child_keys;

  // Used to detect circular referencing of tables on different field
  // InnoDB FK gives error t1(f1) -> t2(f1) -> t1(f2)
  Name_pair m_parent_tables;
  Visited_keys m_visited_parent_keys;

 public:
  void add_foreign_key(const char *db_name, const char *fk_name) {
    m_foreign_keys.insert(Name_pair::key_type(db_name, fk_name));
  }

  void remove_foreign_key(const char *db_name, const char *fk_name) {
    m_foreign_keys.erase(
        m_foreign_keys.find(Name_pair::key_type(db_name, fk_name)));
  }

  void add_parent_table(const char *db_name, const char *tbl_name) {
    m_parent_tables.insert(Name_pair::key_type(db_name, tbl_name));
  }

  void remove_parent_table(const char *db_name, const char *tbl_name) {
    m_parent_tables.erase(
        m_parent_tables.find(Name_pair::key_type(db_name, tbl_name)));
  }

  bool foreign_key_exists(const char *db_name, const char *fk_name) {
    return m_foreign_keys.contains(Name_pair::key_type(db_name, fk_name));
  }

  bool table_exists(const char *db_name, const char *tbl_name) {
    return m_parent_tables.contains(Name_pair::key_type(db_name, tbl_name));
  }

  void mark_child_visited(const char *db_name, const char *tbl_name,
                          int key_pos) {
    m_visited_child_keys.insert(
        Visited_keys::key_type(db_name, tbl_name, key_pos));
  }

  bool is_child_visited(const char *db_name, const char *tbl_name,
                        int key_pos) {
    return m_visited_child_keys.contains(
        Visited_keys::key_type(db_name, tbl_name, key_pos));
  }

  void mark_parent_visited(const char *db_name, const char *tbl_name,
                           int key_pos) {
    m_visited_parent_keys.insert(
        Visited_keys::key_type(db_name, tbl_name, key_pos));
  }

  bool is_parent_visited(const char *db_name, const char *tbl_name,
                         int key_pos) {
    return m_visited_parent_keys.contains(
        Visited_keys::key_type(db_name, tbl_name, key_pos));
  }

  uint size() const { return m_foreign_keys.size(); }
  bool is_empty() const { return m_foreign_keys.empty(); }
};

/**
  Helper to temporarily enable OPTION_NO_FOREIGN_KEY_CHECKS within a scope
  and restore the thread option bits during exit.
  This is used during foreign key cascade.
*/
class No_fk_checks_guard {
 public:
  explicit No_fk_checks_guard(THD *thd)
      : m_thd(thd), m_saved_bits(thd->variables.option_bits) {
    m_thd->variables.option_bits |= OPTION_NO_FOREIGN_KEY_CHECKS;
  }
  ~No_fk_checks_guard() { m_thd->variables.option_bits = m_saved_bits; }
  No_fk_checks_guard(const No_fk_checks_guard &) = delete;
  No_fk_checks_guard &operator=(const No_fk_checks_guard &) = delete;

 private:
  THD *m_thd;
  ulonglong m_saved_bits;
};

static bool check_all_child_fk_ref(THD *thd, const TABLE *table,
                                   enum_fk_dml_type dml_type,
                                   Foreign_key_chain *chain);

/**
 * @brief  Helper function to prepare error message.
 *
 * @param thd                   Thread handle.
 * @param check_child_access    Check child table privileges.
 * @param check_parent_access   Check parent table privileges.
 * @param tbl                   TABLE instance.
 * @param fk                    Foreign Key Info instance.
 *
 * @return std::string       Error message.
 */
static std::string build_fk_error_message(THD *thd, bool check_child_access,
                                          bool check_parent_access,
                                          const TABLE *tbl,
                                          TABLE_SHARE_FOREIGN_KEY_INFO *fk) {
  char quote_char = (thd->variables.sql_mode & MODE_ANSI_QUOTES) ? '"' : '`';
  std::ostringstream str_buf;
  str_buf << "";

  // build a Table_ref and perform ACL check.
  auto acl_denied = [&](const LEX_CSTRING &db_name,
                        const LEX_CSTRING &tbl_name) -> bool {
    Table_ref table(db_name.str, db_name.length, tbl_name.str, tbl_name.length,
                    TL_READ);
    if (check_some_access(thd, TABLE_OP_ACLS, &table) ||
        (table.grant.privilege & TABLE_OP_ACLS) == 0) {
      return true;
    }
    return false;
  };

  if (check_child_access) {
    if (acl_denied(tbl->s->db, tbl->s->table_name)) return str_buf.str();
  }
  if (check_parent_access) {
    if (acl_denied(fk->referenced_table_db, fk->referenced_table_name))
      return str_buf.str();
  }

  str_buf << " (" << quote_char << tbl->s->db.str;
  str_buf << quote_char << "." << quote_char;
  str_buf << tbl->s->table_name.str << quote_char << ", CONSTRAINT ";
  str_buf << quote_char << fk->fk_name.str << quote_char;
  str_buf << " FOREIGN KEY (";
  for (uint k = 0; k < fk->columns; k++) {
    if (k != 0) str_buf << ", ";
    str_buf << quote_char << fk->referencing_column_names[k].str;
    str_buf << quote_char;
  }
  str_buf << ") REFERENCES ";

  std::ostringstream str_tbl_buf;
  if (my_strcasecmp(table_alias_charset, tbl->s->db.str,
                    fk->referenced_table_db.str) != 0) {
    str_tbl_buf << quote_char;
    str_tbl_buf << fk->referenced_table_db.str;
    str_tbl_buf << quote_char << ".";
  }
  str_tbl_buf << quote_char;
  str_tbl_buf << fk->referenced_table_name.str;
  str_tbl_buf << quote_char;

  char tbl_name_buf[(NAME_LEN + 1) * 2 + 1];
  my_stpcpy(tbl_name_buf, str_tbl_buf.str().c_str());
  if (lower_case_table_names == 2)
    my_casedn_str(system_charset_info, tbl_name_buf);

  str_buf << tbl_name_buf << " (";
  for (uint k = 0; k < fk->columns; k++) {
    if (k != 0) str_buf << ", ";
    str_buf << quote_char << fk->referenced_column_names[k].str;
    str_buf << quote_char;
  }
  str_buf << ")";
  if (dd::Foreign_key::RULE_CASCADE == fk->delete_rule) {
    str_buf << " ON DELETE CASCADE";
  } else if (dd::Foreign_key::RULE_SET_NULL == fk->delete_rule) {
    str_buf << " ON DELETE SET NULL";
  } else if (dd::Foreign_key::RULE_RESTRICT == fk->delete_rule) {
    str_buf << " ON DELETE RESTRICT";
  }

  if (dd::Foreign_key::RULE_CASCADE == fk->update_rule) {
    str_buf << " ON UPDATE CASCADE";
  } else if (dd::Foreign_key::RULE_SET_NULL == fk->update_rule) {
    str_buf << " ON UPDATE SET NULL";
  } else if (dd::Foreign_key::RULE_RESTRICT == fk->update_rule) {
    str_buf << " ON UPDATE RESTRICT";
  }

  str_buf << ")";
  return str_buf.str();
}

/**
 * @brief Reports ER_ROW_IS_REFERENCED_2 error with fk information
 *
 * @param thd                 Thread Handle.
 * @param table_c             TABLE handle
 * @param fk                  Foreign Key Information
 *
 * @return true               Propagates error
 */
static inline bool report_row_referenced_error(
    THD *thd, const TABLE *table_c, TABLE_SHARE_FOREIGN_KEY_INFO *fk) {
  std::string fk_str = build_fk_error_message(thd, true, false, table_c, fk);
  my_error(ER_ROW_IS_REFERENCED_2, MYF(0), fk_str.c_str());
  return true;
}

/**
 * @brief Reports ER_NO_REFERENCED_ROW_2 error with fk information
 *
 * @param thd                 Thread Handle.
 * @param table_c             TABLE handle
 * @param fk                  Foreign Key Information
 *
 * @return true               Propagates error
 */
static inline bool report_no_referenced_row_error(
    THD *thd, const TABLE *table_c, TABLE_SHARE_FOREIGN_KEY_INFO *fk) {
  std::string fk_str = build_fk_error_message(thd, false, true, table_c, fk);
  my_error(ER_NO_REFERENCED_ROW_2, MYF(0), fk_str.c_str());
  return true;
}

/**
 * @brief Find TABLE instance of a foreign key table from the THD::open_tables
 *        list.
 *
 * @param thd                 Thread Handle.
 * @param db_name             DB name.
 * @param table_name          Table name.
 * @param fk_name             Foreign key name.
 *
 * @return TABLE*             TABLE instance if found, otherwise nullptr.
 */
static TABLE *find_fk_table_from_open_tables(THD *thd, const char *db_name,
                                             const char *table_name,
                                             const char *fk_name) {
  for (TABLE *table = thd->open_tables; table; table = table->next) {
    if (table->open_for_fk_name == nullptr) continue;
    // Evengthough, FK names are unique to DB, table_name should also be
    // compared because both parent table and child table use the same foreign
    // key name while opening table handles.
    if (my_strcasecmp(table_alias_charset, table->s->db.str, db_name) == 0 &&
        my_strcasecmp(table_alias_charset, table->s->table_name.str,
                      table_name) == 0 &&
        my_strcasecmp(system_charset_info, table->open_for_fk_name, fk_name) ==
            0) {
      DBUG_PRINT("fk", ("find_fk_table_from_open_tables() found %s.%s.%s in "
                        "THD::open_tables",
                        table->s->db.str, table->s->table_name.str,
                        table->open_for_fk_name));
      return table;
    }
  }

  // With FOREIGN_KEY_CHECKS = OFF, table used in a foreign key is allowed to
  // drop.
  DBUG_PRINT("fk", ("find_fk_table_from_open_tables() could not find %s.%s in "
                    "THD::open_tables",
                    db_name, table_name));
  return nullptr;
}

/**
 * @brief Function to get index of key matching col_names in keys list.
 *
 * @param table            TABLE instance.
 * @param num_columns      Number of columns.
 * @param col_names        List of column names.
 *
 * @return index of key in keys list on success, UINT_MAX if key not found.
 */
static uint get_key_index(const TABLE *table, uint num_columns,
                          LEX_CSTRING *col_names) {
  for (int key_num = 0; key_num < (int)table->s->keys; key_num++) {
    KEY *key_info = &table->key_info[key_num];
    if (num_columns > key_info->user_defined_key_parts) continue;

    uint matched_columns = 0;
    bool mismatch = false;

    for (uint kp = 0; kp < key_info->user_defined_key_parts && kp < num_columns;
         kp++) {
      Field *key_field = key_info->key_part[kp].field;
      DBUG_PRINT("fk",
                 ("get_key_index(): Matching column name %s with key "
                  "%s's column name %s",
                  col_names[kp].str, key_info->name, key_field->field_name));
      if (my_strcasecmp(system_charset_info, key_field->field_name,
                        col_names[kp].str) != 0) {
        mismatch = true;
        break;
      }
      ++matched_columns;
    }

    if (!mismatch && matched_columns == num_columns) {
      DBUG_PRINT("fk", ("get_key_index(): Found matching key %s at index %d",
                        key_info->name, key_num));
      return key_num;
    }
  }

  // This point should never be reached because keys that are part of
  // foreign keys cannot be dropped.
  assert(false);
  return UINT_MAX;
}

/**
 * @brief Function to check if any of the foreign key columns updated.
 *
 * @param table             TABLE instance for a table.
 * @param num_key_cols      Number of columns.
 * @param col_names         List of columns.
 *
 * @return true             If any column is updated.
 * @return false            If columns are not updated.
 */
static bool is_column_updated(const TABLE *table, uint num_key_cols,
                              LEX_CSTRING *col_names) {
  for (uint k = 0; k < num_key_cols; k++) {
    for (Field **field_ptr = table->field; *field_ptr; ++field_ptr) {
      Field *const f = *field_ptr;
      if (my_strcasecmp(system_charset_info, f->field_name, col_names[k].str) ==
          0) {
        if (bitmap_is_set(table->write_set, f->field_index()) &&
            (f->cmp_binary_offset(table->s->rec_buff_length) ||
             (f->is_null(table->s->rec_buff_length) != f->is_null(0)))) {
          DBUG_PRINT("fk", ("is_column_updated(): Column %s is updated",
                            f->field_name));
          // If any column in the key is updated, including being set null.
          return true;
        }
        break;
      }
    }
  }

  DBUG_PRINT("fk", ("is_column_updated(): Key columns are not updated"));
  return false;
}

/**
 * @brief  Function to check value of foreign key columns and parent
 *         columns are same in case self referencing foreign key.
 *
 * @param table_c           TABLE instance of a table.
 * @param fk                Foreign key information.
 *
 * @return true             If values are same.
 * @return false            otherwise.
 */
static bool is_self_fk_value_same(const TABLE *table_c,
                                  TABLE_SHARE_FOREIGN_KEY_INFO *fk) {
  uint key1_idx =
      get_key_index(table_c, fk->columns, fk->referencing_column_names);
  uint key2_idx =
      get_key_index(table_c, fk->columns, fk->referenced_column_names);

  KEY *key_info_fk = table_c->key_info + key1_idx;
  KEY *key_info_pk = table_c->key_info + key2_idx;
  uchar key_value[MAX_KEY_LENGTH];
  key_copy_fk(key_value, sizeof(key_value), table_c->record[0], key_info_fk,
              key_info_pk, true);
  KEY_PART_INFO *key_part = table_c->key_info[key2_idx].key_part;

  if (key_cmp(key_part, key_value, key_part->length, false) == 0) {
    DBUG_PRINT("fk", ("is_self_fk_value_same(): Values of foreign key columns "
                      "and parent key columns are same"));
    return true;
  }

  DBUG_PRINT("fk", ("is_self_fk_value_same(): Values of foreign key columns "
                    "and parent key columns are not same"));
  return false;
}

/**
 * @brief  Check if MDL lock is already acquired on the table. If not, then
 *         acquire MDL lock on the schema and table.
 *
 * @param thd            Thread Handle.
 * @param db_name        DB name.
 * @param table_name     Table name.
 * @param mdl_type       MDL lock type.
 *
 * @return true          On error.
 * @return false         On success.
 *
 */
static bool check_and_acquire_MDL_lock(THD *thd, const char *db_name,
                                       const char *table_name,
                                       enum_mdl_type mdl_type) {
  if (lower_case_table_names == 2) {
    char *db_copy = strmake_root(thd->mem_root, db_name, strlen(db_name));
    if (db_copy == nullptr) return true;  // OOM

    char *tbl_copy =
        strmake_root(thd->mem_root, table_name, strlen(table_name));
    if (tbl_copy == nullptr) return true;  // OOM

    my_casedn_str(&my_charset_utf8mb3_tolower_ci, db_copy);
    my_casedn_str(&my_charset_utf8mb3_tolower_ci, tbl_copy);

    db_name = db_copy;
    table_name = tbl_copy;
  }
  if (thd->mdl_context.owns_equal_or_stronger_lock(MDL_key::TABLE, db_name,
                                                   table_name, mdl_type)) {
    DBUG_PRINT(
        "fk",
        ("check_and_acquire_MDL_lock(): Lock is already acquired on %s.%s",
         db_name, table_name));
    return false;
  }

  DBUG_PRINT("fk", ("check_and_acquire_MDL_lock(): Lock is not already on "
                    "%s.%s. Acquiring lock.",
                    db_name, table_name));

  // MDL Request for table.
  MDL_request *mdl_request = new (thd->mem_root) MDL_request;
  if (mdl_request == nullptr) return true;
  MDL_REQUEST_INIT(mdl_request, MDL_key::TABLE, db_name, table_name, mdl_type,
                   MDL_TRANSACTION);

  // Acquire MDL lock on table.
  return (thd->mdl_context.acquire_lock(mdl_request,
                                        thd->variables.lock_wait_timeout));
}

/**
 * @brief Function to open table for foreign key validation.
 *
 * @param thd               Thread Handle.
 * @param db_name           DB name.
 * @param table_name        Table name.
 *
 * @return TABLE*           TABLE instance on success, nullptr otherwise.
 */
static TABLE *open_table_for_fk(THD *thd, const char *db_name,
                                const char *table_name) {
  if (lower_case_table_names == 2) {
    char *db_copy = strmake_root(thd->mem_root, db_name, strlen(db_name));
    if (db_copy == nullptr) return nullptr;  // OOM

    char *tbl_copy =
        strmake_root(thd->mem_root, table_name, strlen(table_name));
    if (tbl_copy == nullptr) return nullptr;  // OOM

    my_casedn_str(&my_charset_utf8mb3_tolower_ci, db_copy);
    my_casedn_str(&my_charset_utf8mb3_tolower_ci, tbl_copy);

    db_name = db_copy;
    table_name = tbl_copy;
  }

  // Get table share.
  char tbl_key[MAX_DBKEY_LENGTH];
  size_t tbl_key_len = create_table_def_key(db_name, table_name, tbl_key);
  mysql_mutex_lock(&LOCK_open);
  TABLE_SHARE *share =
      get_table_share(thd, db_name, table_name, tbl_key, tbl_key_len, false);
  mysql_mutex_unlock(&LOCK_open);

  // Open table from share.
  TABLE *tbl = new (thd->mem_root) TABLE;
  if (open_table_from_share(thd, share, "",
                            (uint)(HA_OPEN_KEYFILE | HA_OPEN_RNDFILE |
                                   HA_GET_INDEX | HA_TRY_READ_ONLY),
                            EXTRA_RECORD, 0, tbl, false, nullptr))
    return nullptr;

  DBUG_PRINT("fk",
             ("open_table_for_fk(): opened table %s.%s for foreign check.",
              db_name, table_name));

  return tbl;
}

/**
 * @brief Function to check if CASCADE action is defined for a foreign
 *        key on UPDATE or DELETE operations.
 *
 * @param table          TABLE instance of a table.
 * @param dml_type       DML operation type.
 * @param fk_name        Foreign key information.
 *
 * @return true          If CASCADE action for a DML operation is set.
 * @return false         Otherwise.
 */
static bool is_fk_cascade(TABLE *table, enum_fk_dml_type dml_type,
                          const char *fk_name) {
  TABLE_SHARE *share_c = table->s;
  for (TABLE_SHARE_FOREIGN_KEY_INFO *fk_c = share_c->foreign_key;
       fk_c < share_c->foreign_key + share_c->foreign_keys; ++fk_c) {
    bool is_delete = (dml_type == enum_fk_dml_type::FK_DELETE);
    dd::Foreign_key::enum_rule fk_opt =
        is_delete ? fk_c->delete_rule : fk_c->update_rule;

    if (fk_opt == dd::Foreign_key::RULE_CASCADE ||
        fk_opt == dd::Foreign_key::RULE_SET_NULL) {
      if (my_strcasecmp(system_charset_info, fk_c->fk_name.str, fk_name) == 0) {
        DBUG_PRINT(
            "fk",
            ("is_fk_cascade(): For fk %s on table %s.%s CASCADE is defined.",
             fk_name, table->s->db.str, table->s->table_name.str));
        return true;
      }
    }
  }

  DBUG_PRINT(
      "fk",
      ("is_fk_cascade(): For fk %s on table %s.%s CASCADE is not defined.",
       fk_name, table->s->db.str, table->s->table_name.str));
  return false;
}

/**
  In stored functions/triggers, sub-statements follow the open-table
  call flow, so query_id ends up being set on the unused TABLE instance.
  SQL FK handling does not go through the open-table call path. Therefore,
  we set query_id for the TABLE instance if enable_cascade_triggers=ON

  @param thd    Thread descriptor
  @param table  Table Handle
*/
static void set_query_id(THD *thd, TABLE *table) {
  if (is_cascade_triggers_enabled(thd)) {
    table->query_id = thd->query_id;
  }
}

/**
 * @brief Function to get TABLE instance of a other table in FK relationship.
 *        Table is first searched in the open table list. If table is not
 *        opened or scan is already opened, then table is opened.
 *
 * @param      thd                Thread Handle.
 * @param      db_name            DB name.
 * @param      table_name         Table name.
 * @param      fk_name            Foreign key name.
 *                                If lock is not acquired, then MDL is acquired
 *                                before opening the table.
 * @param      dml_type           DML operation type.
 *                                If lock is not acquired, then MDL is acquired
 *                                before opening the table.
 * @param      open_table         Do not search table in open_tables list,
 *                                instead open table always.
 * @param[out] table              TABLE instance.
 * @param[out] table_exists       Flag is set to true, if table does not exists.
 * @param[out] is_table_opened    Flag is set to true, if a table is opened.
 *
 * @return     true               On error.
 * @return     false              On success.
 */
static bool get_foreign_key_table(THD *thd, const char *db_name,
                                  const char *table_name, const char *fk_name,
                                  enum_fk_dml_type dml_type, bool open_table,
                                  TABLE **table, bool *table_exists,
                                  bool *is_table_opened) {
  *table_exists = true;
  *is_table_opened = false;
  *table = nullptr;
  enum_mdl_type mdl_type = MDL_SHARED_READ;
  DBUG_PRINT("fk", ("get_foreign_key_table(): table = %s.%s, open_table = %d",
                    db_name, table_name, open_table));
  if (!open_table) {
    // search by fk_name
    TABLE *fk_tbl =
        find_fk_table_from_open_tables(thd, db_name, table_name, fk_name);
    if (fk_tbl != nullptr && fk_tbl->file->inited == 0) {
      // Table is already opened and scan on table is not yet started.
      if (!is_fk_cascade(fk_tbl, dml_type, fk_name) ||
          fk_tbl->reginfo.lock_type == TL_WRITE) {
        DBUG_PRINT("fk",
                   ("get_foreign_key_table(): table = %s.%s, lock_type = %d",
                    db_name, table_name, fk_tbl->reginfo.lock_type));
        *table = fk_tbl;

        set_query_id(thd, fk_tbl);
        return false;
      }
      mdl_type = MDL_SHARED_WRITE;
    }
  }

  // Either table is not opened or TABLE instance of a table is already used
  // for FK validation.
  if (check_and_acquire_MDL_lock(thd, db_name, table_name, mdl_type)) {
    return true;  // Error is already reported.
  }

  *table_exists = false;
  if (dd::table_exists(thd->dd_client(), db_name, table_name, table_exists)) {
    return true;  // Error is already reported.
  }
  if (*table_exists == false) {
    DBUG_PRINT("fk", ("get_foreign_key_table(): table %s.%s does not exists.",
                      db_name, table_name));
    return false;
  }

  *table = open_table_for_fk(thd, db_name, table_name);
  if (*table != nullptr) {
    *is_table_opened = true;
    (*table)->open_for_fk_name = fk_name;
    set_query_id(thd, *table);
  }
  return false;
}

/**
 * @brief Function to copy value to child table columns during UPDATE CASCADE
 *        operation.
 *
 * @param thd                    Thread Handle.
 * @param table_c                TABLE instance of a child table.
 * @param child_key_idx          Index of key in the child table.
 * @param table_p                TABLE instance of a parent table.
 * @param parent_key_idx         Index of key in the parent table.
 * @param fk                     Foreign key information.
 *
 * @return false                 On Success.
 * @return true                  On failure.
 */
static bool set_updated_key_value(THD *thd, TABLE *table_c, int child_key_idx,
                                  const TABLE *table_p, int parent_key_idx,
                                  TABLE_SHARE_FOREIGN_KEY_INFO *fk) {
  uint num_key_cols = fk->columns;
  KEY *key_info_p = &table_p->key_info[parent_key_idx];
  KEY *key_info_c = &table_c->key_info[child_key_idx];

  for (uint k = 0; k < key_info_p->user_defined_key_parts; k++) {
    if (k >= num_key_cols) break;
    Field *key_field_p = key_info_p->key_part[k].field;
    Field *key_field_c = key_info_c->key_part[k].field;
    DBUG_PRINT(
        "fk",
        ("set_updated_key_value(): parent table name: %s.%s, parent column "
         "name: %s, child table name: %s.%s, child column name = %s",
         table_p->s->db.str, table_p->s->table_name.str,
         key_field_p->field_name, table_c->s->db.str,
         table_c->s->table_name.str, key_field_c->field_name));

    if (key_field_p->is_null()) {
      if (!key_field_c->is_nullable()) {
        return true;
      }
      if (set_field_to_null_with_conversions(key_field_c, false)) {
        return true;
      }
      DBUG_PRINT(
          "fk", ("set_updated_key_value(): parent key column %s is null, value "
                 "is set to child column.",
                 key_field_p->field_name));
    } else if (fields_are_memcpyable(key_field_c, key_field_p)) {
      // Mark the destination field NOT NULL when copying a
      // non-NULL value from the parent.
      key_field_c->set_notnull();

      const size_t length = key_field_c->pack_length();
      assert(key_field_c->field_ptr() != key_field_p->field_ptr() ||
             length == 0);
      memcpy(key_field_c->field_ptr(), key_field_p->field_ptr(), length);
      DBUG_PRINT(
          "fk",
          ("set_updated_key_value(): value copied from parent key column %s to"
           " child column %s.",
           key_field_p->field_name, key_field_c->field_name));
    } else {
      // Do not allow copying from CHAR(m) to CHAR(n) when m > n
      // similar to InnoDB FK behavior
      DBUG_PRINT(
          "fk", ("set_updated_key_value() parent column type: %d char_length %d"
                 " child column type: %d char_length %d.",
                 key_field_p->type(), key_field_p->char_length(),
                 key_field_c->type(), key_field_c->char_length()));
      if ((key_field_c->type() == enum_field_types::MYSQL_TYPE_STRING) &&
          (key_field_p->type() == enum_field_types::MYSQL_TYPE_STRING) &&
          (key_field_p->char_length() > key_field_c->char_length())) {
        return true;
      }
      if (is_string_type(key_field_c->type()) &&
          is_string_type(key_field_p->type()) &&
          key_field_p->charset() != key_field_c->charset()) {
        // CASCADE with different character set is allowed in InnoDB FK
        // SQL FK reports error as FK is not supported on varying charsets
        return true;
      }

      // Force padding when copying from char to varchar key fields.
      auto sql_mode_guard =
          create_scope_guard([thd, saved_sql_mode = thd->variables.sql_mode]() {
            thd->variables.sql_mode = saved_sql_mode;
          });
      if ((key_field_c->type() == enum_field_types::MYSQL_TYPE_VARCHAR) &&
          (key_field_p->type() == enum_field_types::MYSQL_TYPE_STRING)) {
        // InnoDB FK pads space even for NO_PAD collation
        thd->variables.sql_mode |= MODE_PAD_CHAR_TO_FULL_LENGTH;
      }
      field_conv_slow(key_field_c, key_field_p);
      // field_conv_slow may set the THD's error state if an invalid conversion
      // is attempted when cascading an update from a parent column to a child
      // that have differing types
      if (thd->is_error()) {
        return true;
      }

      // Mark the destination field NOT NULL when copying a
      // non-NULL value from the parent.
      key_field_c->set_notnull();

      DBUG_PRINT(
          "fk",
          ("set_updated_key_value(): value copied from parent key column %s to"
           " child column %s.",
           key_field_p->field_name, key_field_c->field_name));
    }
  }
  return false;
}

/**
 * @brief Function to set the null value for key columns.
 *
 * @param table               TABLE instance of a table.
 * @param num_key_cols        Number of columns.
 * @param col_names           List of columns.
 *
 * @return false                 On Success.
 * @return true                  On failure.
 */
static bool set_key_value_null(TABLE *table, uint num_key_cols,
                               LEX_CSTRING *col_names) {
  for (uint kc = 0; kc < num_key_cols; kc++) {
    for (Field **field_ptr = table->field; *field_ptr; ++field_ptr) {
      if (my_strcasecmp(system_charset_info, (*field_ptr)->field_name,
                        col_names[kc].str) == 0) {
        DBUG_PRINT("fk",
                   ("set_key_value_null(): Null value is set for child key "
                    "column %s.%s.%s",
                    table->s->db.str, table->s->table_name.str,
                    (*field_ptr)->field_name));
        if (!(*field_ptr)->is_gcol() && (*field_ptr)->is_nullable()) {
          (*field_ptr)->set_null();
          bitmap_set_bit(table->write_set, (*field_ptr)->field_index());
          break;
        } else {
          return true;
        }
      }
    }
  }
  return false;
}

/**
  Determine whether a cascaded update from the current state of the
  parent table record's foreign keys to the corresponding child table's
  foreign keys is legal.

  An example of an illegal cascade is when a parent table foreign key
  column is a VARCHAR with a corresponding child foreign key CHAR
  column AND the length of the value in the parent column exceeds the
  number of characters allowed in the child column.

  @param table_p     parent table from which to check cascade.
  @param key_info_p  descriptor of the foreign key index in the parent table.
  @param key_info_c  descriptor of the foreign key index in the child table.

  @retval  true if the cascaded update is legal, false if the cascaded update is
           not legal.

*/
bool is_cascade_from_parent_legal(const TABLE *table_p, const KEY *key_info_p,
                                  const KEY *key_info_c) {
  uchar update_parent_key_value[MAX_KEY_LENGTH];
  int key_len = 0;
  auto st =
      key_copy_fk(update_parent_key_value, sizeof(update_parent_key_value),
                  table_p->record[0], key_info_p, key_info_c, false, &key_len);
  return st == copy_status::ok;
}

/**
 * Extracts the current value of FK into a buffer.
 *
 * @param[out] out      Buffer to receive the FK columns' value.
 * @param table         TABLE instance containing the row.
 * @param fk            FK info structure (for column list).
 * @param record        Row buffer to extract from (table->record[0]).
 * @return              total number of bytes used in out
 */
static int extract_fk_from_record(uchar *out, const TABLE *table,
                                  const TABLE_SHARE_FOREIGN_KEY_INFO *fk,
                                  const uchar *record) {
  // Find the key index in table for the referencing columns (FK columns)
  uint key_idx =
      get_key_index(table, fk->columns, fk->referencing_column_names);
  const KEY *key_info = &table->key_info[key_idx];
  int key_len = 0;
  key_copy_fk(out, MAX_KEY_LENGTH, record, key_info, key_info, true, &key_len);
  return key_len;
}

/**
 * @brief Executes BEFORE triggers for cascade operations when enabled
 *        and validate constraints if trigger updates any field.
 *
 * @param thd                 Thread handle.
 * @param table               Table on which triggers should be fired.
 * @param fk                  Foreign key information
 * @param event               Trigger event type.
 * @param old_row_is_record1  If record1 contains old or new field.
 *
 * @return true if trigger execution reports an error; false otherwise.
 */
static bool process_before_triggers(THD *thd, TABLE *table,
                                    const TABLE_SHARE_FOREIGN_KEY_INFO *fk,
                                    enum_trigger_event_type event,
                                    bool old_row_is_record1) {
  if (table->triggers == nullptr) return false;

  Trigger_chain *tc = table->triggers->get_triggers(event, TRG_ACTION_BEFORE);
  if (tc == nullptr) return false;

  DBUG_PRINT("fk", ("SQL FK firing BEFORE %s trigger on child %s",
                    (event == TRG_EVENT_DELETE) ? "DELETE" : "UPDATE",
                    table->s->table_name.str));

  uchar fk_value_before[MAX_KEY_LENGTH];
  if (event == TRG_EVENT_UPDATE) {
    extract_fk_from_record(fk_value_before, table, fk, table->record[0]);
  }

  table->triggers->enable_fields_temporary_nullability(thd);

  bool rc = table->triggers->process_triggers(thd, event, TRG_ACTION_BEFORE,
                                              old_row_is_record1);

  table->triggers->disable_fields_temporary_nullability();

  if (!rc && tc->has_updated_trigger_fields(table->write_set)) {
    uchar fk_value_after[MAX_KEY_LENGTH];
    int fk_len =
        extract_fk_from_record(fk_value_after, table, fk, table->record[0]);
    rc = memcmp(fk_value_before, fk_value_after, fk_len) != 0;
    if (rc) {
      my_error(ER_FK_CASCADE_TRIGGER_UPDATING_FK_COLUMNS_NOT_SUPPORTED, MYF(0));
    }

    /*
      Re-calculate generated fields to cater for cases when base columns are
      updated by the triggers.
    */
    if (!rc && table->has_gcol()) {
      // Dont save old value while re-calculating generated fields.
      // Before image will already be saved in the first calculation.
      table->blobs_need_not_keep_old_value();
      rc = update_generated_write_fields(table->write_set, table);
    }

    if (!rc) {
      rc = check_record(thd, table->field);
    }

    if (!rc) {
      if (invoke_table_check_constraints(thd, table)) {
        rc = thd->is_error();
      }
    }
  }

  table->triggers->reset_field_nulls();

  return rc;
}

/**
 * @brief Executes AFTER triggers for cascade operations when enabled.
 *
 * @param thd                 Thread handle.
 * @param table               Table on which triggers should be fired.
 * @param event               Trigger event type.
 * @param old_row_is_record1  If record1 contains old or new field.
 *
 * @return true if trigger execution reports an error; false otherwise.
 */
static bool execute_after_triggers(THD *thd, TABLE *table,
                                   enum_trigger_event_type event,
                                   bool old_row_is_record1) {
  if (table->triggers == nullptr) return false;

  DBUG_PRINT("fk", ("SQL FK firing AFTER %s trigger on child %s",
                    (event == TRG_EVENT_DELETE) ? "DELETE" : "UPDATE",
                    table->s->table_name.str));

  return table->triggers->process_triggers(thd, event, TRG_ACTION_AFTER,
                                           old_row_is_record1);
}

/**
 * @brief Helper function to apply ON DELETE/ON UPDATE RESTRICT or NOACTION to
 *        child table on DML operation.
 * Note: ON DELETE|UPDATE SET DEFAULT also behaves like RESTRICT
 *
 * @param thd                Thread Handle.
 * @param table_c            TABLE instance of a child table.
 * @param fk_c               Foreign key information.
 * @param dml_type           DML operation type.
 *
 * @return true              On error.
 * @return false             On success.
 */
static bool on_delete_on_update_restrict_or_no_action(
    THD *thd, const TABLE *table_c, TABLE_SHARE_FOREIGN_KEY_INFO *fk_c,
    enum_fk_dml_type dml_type) {
  DBUG_PRINT("fk", ("on_delete_on_update_restrict_or_no_action():"
                    "child table = %s.%s,  FK name: %s",
                    table_c->s->db.str, table_c->s->table_name.str,
                    fk_c->fk_name.str));
  if (((dml_type != enum_fk_dml_type::FK_UPDATE) &&
       (dd::Foreign_key::RULE_RESTRICT == fk_c->delete_rule ||
        dd::Foreign_key::RULE_SET_DEFAULT == fk_c->delete_rule ||
        dd::Foreign_key::RULE_NO_ACTION == fk_c->delete_rule)) ||
      ((dml_type == enum_fk_dml_type::FK_UPDATE) &&
       (dd::Foreign_key::RULE_RESTRICT == fk_c->update_rule ||
        dd::Foreign_key::RULE_SET_DEFAULT == fk_c->update_rule ||
        dd::Foreign_key::RULE_NO_ACTION == fk_c->update_rule))) {
    return report_row_referenced_error(thd, table_c, fk_c);
  }

  return false;
}

/**
 * @brief  Helper function to apply ON DELETE CASCADE to child table on DML
 *         operation.
 *
 * @param thd                 Thread Handle.
 * @param table_c             TABLE instance of a child table.
 * @param fk_c                Foreign key information.
 * @param dml_type            DML operation type.
 * @param key_value           Key value buffer.
 * @param key_len             Length of key value buffer.
 * @param chain               Foreign key chain.
 * @param[out] error          Error value.
 *
 * @return true               On error.
 * @return false              On success.
 */
static bool on_delete_cascade(THD *thd, TABLE *table_c,
                              TABLE_SHARE_FOREIGN_KEY_INFO *fk_c,
                              enum_fk_dml_type dml_type, uchar *key_value,
                              int key_len, Foreign_key_chain *chain,
                              int *error) {
  if ((dml_type != enum_fk_dml_type::FK_UPDATE) &&
      dd::Foreign_key::RULE_CASCADE == fk_c->delete_rule) {
    table_c->use_all_columns();

    DBUG_PRINT("fk", ("on_delete_cascade(): child table = %s.%s, FK name: %s",
                      table_c->s->db.str, table_c->s->table_name.str,
                      fk_c->fk_name.str));

    do {
      if (is_cascade_triggers_enabled(thd) &&
          process_before_triggers(thd, table_c, fk_c, TRG_EVENT_DELETE,
                                  false)) {
        return true;
      }

      // binlog sequence(child binlog is applied before parent at replica)
      // during CASCADE breaks foreign key check, so FK checks are skipped
      // for binlog events generated during CASCADE.
      {
        No_fk_checks_guard fk_guard(thd);
        if ((*error = table_c->file->ha_delete_row(table_c->record[0]))) {
          break;
        }
      }
      DBUG_PRINT("fk", ("on_delete_cascade(): child table row deleted."));

      if (check_all_child_fk_ref(thd, table_c, dml_type, chain)) {
        return thd->is_error();
      }

      if (is_cascade_triggers_enabled(thd) &&
          execute_after_triggers(thd, table_c, TRG_EVENT_DELETE, false)) {
        return true;
      }
    } while (!(*error = table_c->file->ha_index_next_same(table_c->record[0],
                                                          key_value, key_len)));

    if (*error != HA_ERR_END_OF_FILE) {
      DBUG_PRINT(
          "fk",
          ("on_delete_cascade(): child row delete failed. error = %d", *error));
      *error = 1;
    } else {
      *error = 0;
    }
  }

  return false;
}

/**
 * @brief  Function to apply ON UPDATE CASCADE to child table on DML
 *         operation.
 *
 * @param thd                 Thread Handle.
 * @param table_p             TABLE instance of a parent table.
 * @param table_c             TABLE instance of a child table.
 * @param fk_c                Foreign key information.
 * @param dml_type            DML operation type.
 * @param key_info_p          Parent table key information.
 * @param parent_key_idx      Parent table key index.
 * @param key_info_c          Child table key information.
 * @param child_key_idx       Child table key index.
 * @param key_value           Key value buffer.
 * @param key_len             Length of key value buffer.
 * @param chain               Foreign key chain.
 * @param[out] error          Error value.
 *
 * @return true               On error.
 * @return false              On success.
 */
static bool on_update_cascade(THD *thd, const TABLE *table_p, TABLE *table_c,
                              TABLE_SHARE_FOREIGN_KEY_INFO *fk_c,
                              KEY *key_info_p, uint parent_key_idx,
                              KEY *key_info_c, uint child_key_idx,
                              enum_fk_dml_type dml_type, uchar *key_value,
                              int key_len, Foreign_key_chain *chain,
                              int *error) {
  if ((dml_type == enum_fk_dml_type::FK_UPDATE) &&
      dd::Foreign_key::RULE_CASCADE == fk_c->update_rule) {
    DBUG_PRINT(
        "fk", ("on_update_cascade(): parent table = %s.%s, child table = "
               "%s.%s, FK name = %s, parent key name = %s, child key name = %s",
               table_p->s->db.str, table_p->s->table_name.str,
               table_c->s->db.str, table_c->s->table_name.str,
               fk_c->fk_name.str, key_info_p->name, key_info_c->name));
    if (!is_cascade_from_parent_legal(table_p, key_info_p, key_info_c)) {
      // In this situation, an attempt is being made to update a parent
      // row with a column value that would be invalid in the corresponding
      // child column, AND there already exist child row(s) with the
      // previous (presumably valid) parent column value, so we return
      // an error here.
      return report_row_referenced_error(thd, table_c, fk_c);
    }

    if (table_p->s == table_c->s) {
      // InnoDB FK fails for self referencing UPDATE CASCADE
      // SQL FK should behave the same
      return report_row_referenced_error(thd, table_c, fk_c);
    }

    table_c->use_all_columns();

    // The set_updated_key_value function needs to read the parent data in
    // order to cascaded it to the child. so allowing read temporarily
    MY_BITMAP saved_read_set;
    bool saved_read_set_inited =
        (bitmap_init(&saved_read_set, nullptr, table_p->s->fields) == 0);
    if (saved_read_set_inited) {
      bitmap_copy(&saved_read_set, table_p->read_set);
    }
    auto restore_parent_read_set = create_scope_guard([&] {
      if (saved_read_set_inited) {
        bitmap_copy(table_p->read_set, &saved_read_set);
        bitmap_free(&saved_read_set);
      }
    });
    bitmap_set_all(table_p->read_set);

    do {
      // Copy the original child record from table_c->record[0] into
      // table_c->record[1], as the following set_updated_key_value call
      // will modify table_c->record[0] and the table_c->file->ha_update_row
      // function needs accurate "old" and "new" record states.
      memcpy(table_c->record[1], table_c->record[0],
             table_c->s->rec_buff_length);
      if (set_updated_key_value(thd, table_c, child_key_idx, table_p,
                                parent_key_idx, fk_c)) {
        return report_row_referenced_error(thd, table_c, fk_c);
      }

      // If the table has generated columns dependent on the foreign key,
      // ensure any changes to the foreign key value are also reflected in those
      // generated columns.
      if (table_c->has_gcol() &&
          update_generated_write_fields(table_c->write_set, table_c)) {
        return report_row_referenced_error(thd, table_c, fk_c);
      }

      if (is_cascade_triggers_enabled(thd) &&
          process_before_triggers(thd, table_c, fk_c, TRG_EVENT_UPDATE, true)) {
        return true;
      }

      // binlog sequence(child binlog is applied before parent at replica)
      // during CASCADE breaks foreign key check, so FK checks are skipped
      // for binlog events generated during CASCADE.
      {
        No_fk_checks_guard fk_guard(thd);
        if ((*error = table_c->file->ha_update_row(table_c->record[1],
                                                   table_c->record[0]))) {
          break;
        }
      }
      DBUG_PRINT("fk",
                 ("on_update_cascade(): Updated new value to child table %s.%s",
                  table_c->s->db.str, table_c->s->table_name.str));

      // BEFORE UPDATE triggers could have modified FK columns referring to
      // other parent tables using SET NEW.column syntax, so
      // check_all_parent_fk_ref() call is required.
      if (check_all_parent_fk_ref(thd, table_c, enum_fk_dml_type::FK_UPDATE,
                                  fk_c) ||
          check_all_child_fk_ref(thd, table_c, enum_fk_dml_type::FK_UPDATE,
                                 chain)) {
        return thd->is_error();
      }

      if (is_cascade_triggers_enabled(thd) &&
          execute_after_triggers(thd, table_c, TRG_EVENT_UPDATE, true)) {
        return true;
      }
    } while (!(*error = table_c->file->ha_index_next_same(table_c->record[0],
                                                          key_value, key_len)));
    if (*error != HA_ERR_END_OF_FILE) {
      DBUG_PRINT("fk",
                 ("on_update_cascade(): Updated new value to child table "
                  "%s.%s failed. error = %d",
                  table_c->s->db.str, table_c->s->table_name.str, *error));
      if (*error == HA_ERR_FOUND_DUPP_KEY) {
        // Cascading a key update from a parent table to a child table leads
        // to a duplicate key error in the child table.
        char rec_buf[MAX_KEY_LENGTH];
        String rec(rec_buf, sizeof(rec_buf), system_charset_info);
        key_unpack(&rec, const_cast<TABLE *>(table_p), key_info_p);
        const uint key_nr = table_c->file->get_dup_key(*error);
        assert((int)key_nr >= 0);
        my_error(ER_FOREIGN_DUPLICATE_KEY_WITH_CHILD_INFO, MYF(0),
                 table_p->s->table_name.str, rec.c_ptr_safe(),
                 table_c->s->table_name.str, table_c->key_info[key_nr].name);
      } else if (*error == HA_FTS_INVALID_DOCID) {
        // Using the existing HA_FTS_INVALID_DOCID error message for this
        // condition might seem to be a good idea, but in this situation the
        // InnoDB engine produces a ER_ROW_IS_REFERENCED_2 error, so we follow
        // suit here to remain (bug) compatible.
        thd->clear_error();
        report_row_referenced_error(thd, table_c, fk_c);
      }
      *error = 1;
    } else {
      *error = 0;
    }
  }
  return false;
}

/**
 * @brief  Helper function to apply ON DELETE/ON UPDATE SET NULL to child table
 *          on DML operation.
 *
 * @param thd                 Thread Handle.
 * @param table_p             TABLE instance of a parent table.
 * @param table_c             TABLE instance of a child table.
 * @param fk_c                Foreign key information.
 * @param dml_type            DML operation type.
 * @param key_value           Key value buffer.
 * @param key_len             Length of key value buffer.
 * @param chain               Foreign key chain.
 * @param[out] error          Error value.
 *
 * @return true               On error.
 * @return false              On success.
 */
static bool on_delete_on_update_set_null(THD *thd, const TABLE *table_p,
                                         TABLE *table_c,
                                         TABLE_SHARE_FOREIGN_KEY_INFO *fk_c,
                                         enum_fk_dml_type dml_type,
                                         uchar *key_value, int key_len,
                                         Foreign_key_chain *chain, int *error) {
  if (((dml_type != enum_fk_dml_type::FK_UPDATE) &&
       dd::Foreign_key::RULE_SET_NULL == fk_c->delete_rule) ||
      ((dml_type == enum_fk_dml_type::FK_UPDATE) &&
       dd::Foreign_key::RULE_SET_NULL == fk_c->update_rule)) {
    DBUG_PRINT("fk", ("on_delete_on_update_set_null(): parent table = %s.%s, "
                      "child table = %s.%s, FK name = %s",
                      table_p->s->db.str, table_p->s->table_name.str,
                      table_c->s->db.str, table_c->s->table_name.str,
                      fk_c->fk_name.str));
    if (dml_type == enum_fk_dml_type::FK_UPDATE && table_p->s == table_c->s) {
      // InnoDB FK fails for self referencing UPDATE SET NULL
      // SQL FK should behave the same
      return report_row_referenced_error(thd, table_c, fk_c);
    }

    table_c->use_all_columns();

    do {
      memcpy(table_c->record[1], table_c->record[0],
             table_c->s->rec_buff_length);
      if (set_key_value_null(table_c, fk_c->columns,
                             fk_c->referencing_column_names)) {
        return report_row_referenced_error(thd, table_c, fk_c);
      }

      if (table_c->has_gcol() &&
          update_generated_write_fields(table_c->write_set, table_c)) {
        return report_row_referenced_error(thd, table_c, fk_c);
      }

      if (is_cascade_triggers_enabled(thd) &&
          process_before_triggers(thd, table_c, fk_c, TRG_EVENT_UPDATE, true)) {
        return true;
      }

      // binlog sequence(child binlog is applied before parent at replica)
      // during CASCADE breaks foreign key check, so FK checks are skipped
      // for binlog events generated during CASCADE.
      {
        No_fk_checks_guard fk_guard(thd);
        if ((*error = table_c->file->ha_update_row(table_c->record[1],
                                                   table_c->record[0]))) {
          break;
        }
      }
      DBUG_PRINT("fk", ("on_delete_on_update_set_null(): Updated child table "
                        "%s.%s null value",
                        table_c->s->db.str, table_c->s->table_name.str));

      if (check_all_parent_fk_ref(thd, table_c, enum_fk_dml_type::FK_UPDATE,
                                  fk_c) ||
          check_all_child_fk_ref(thd, table_c, enum_fk_dml_type::FK_UPDATE,
                                 chain)) {
        return thd->is_error();
      }

      if (is_cascade_triggers_enabled(thd) &&
          execute_after_triggers(thd, table_c, TRG_EVENT_UPDATE, true)) {
        return true;
      }
    } while (!(*error = table_c->file->ha_index_next_same(table_c->record[0],
                                                          key_value, key_len)));
    if (*error != HA_ERR_END_OF_FILE) {
      DBUG_PRINT("fk",
                 ("on_delete_on_update_set_null(): Failed to update child "
                  "table %s.%s. error = %d",
                  table_c->s->db.str, table_c->s->table_name.str, *error));
      *error = 1;
    } else {
      *error = 0;
    }
  }

  return false;
}

/**
 * @brief Checks foreign key constraint on child table.
 *
 * @param thd              Thread Handle.
 * @param table_p          TABLE instance of a parent table.
 * @param table_c          TABLE instance of a child table.
 * @param fk_c             Foreign key information.
 * @param dml_type         DML operation type.
 * @param chain            Foreign key chain.
 *
 * @return true            On error.
 * @return false           On success.
 */
static bool check_child_fk_ref(THD *thd, const TABLE *table_p, TABLE *table_c,
                               TABLE_SHARE_FOREIGN_KEY_INFO *fk_c,
                               enum_fk_dml_type dml_type,
                               Foreign_key_chain *chain) {
  assert(dml_type == enum_fk_dml_type::FK_UPDATE ||
         dml_type == enum_fk_dml_type::FK_DELETE ||
         dml_type == enum_fk_dml_type::FK_DELETE_REPLACE);

  DBUG_PRINT(
      "fk", ("check_child_fk_ref(): parent table = %s.%s, child table = "
             "%s.%s, FK name = %s",
             table_p->s->db.str, table_p->s->table_name.str, table_c->s->db.str,
             table_c->s->table_name.str, fk_c->fk_name.str));

  uint parent_key_idx =
      get_key_index(table_p, fk_c->columns, fk_c->referenced_column_names);
  KEY *key_info_p = table_p->key_info + parent_key_idx;

  uint child_key_idx =
      get_key_index(table_c, fk_c->columns, fk_c->referencing_column_names);

  KEY *key_info_c = table_c->key_info + child_key_idx;

  if (dml_type == enum_fk_dml_type::FK_DELETE) {
    // If any key field of the existing record is null, it cannot have any
    // related child rows...
    if (is_any_key_fld_value_null(table_p->record[0], key_info_p)) return false;
  } else {
    if (is_any_key_fld_value_null(table_p->record[1], key_info_p)) return false;

    if (!is_column_updated(table_p, fk_c->columns,
                           fk_c->referenced_column_names))
      return false;
  }

  bool fk_added_to_chain = false;
  if (chain) {
    if (table_p->s != table_c->s) {
      // detect circular foreign key references if it is not self referencing
      if (chain->foreign_key_exists(table_c->s->db.str, fk_c->fk_name.str)) {
        DBUG_PRINT(
            "fk",
            ("check_child_fk_ref(): Foreign_key_chain cycle detected : %s",
             fk_c->fk_name.str));
        return false;
      }
      if (dml_type == enum_fk_dml_type::FK_UPDATE &&
          dd::Foreign_key::RULE_RESTRICT != fk_c->update_rule &&
          dd::Foreign_key::RULE_SET_DEFAULT != fk_c->update_rule &&
          dd::Foreign_key::RULE_NO_ACTION != fk_c->update_rule) {
        // detect if table is involved in update cascade before
        DBUG_PRINT("fk", ("update cascade on same table checking: %s",
                          table_c->s->table_name.str));
        if (chain->table_exists(table_c->s->db.str,
                                table_c->s->table_name.str) &&
            !chain->is_parent_visited(table_c->s->db.str,
                                      table_c->s->table_name.str,
                                      child_key_idx)) {
          DBUG_PRINT("fk", ("update cascade on same table detected: %s",
                            table_c->s->table_name.str));
          return report_row_referenced_error(thd, table_c, fk_c);
        }

        // detect if value is already cascaded via another path
        if (chain->is_child_visited(table_c->s->db.str,
                                    table_c->s->table_name.str,
                                    child_key_idx)) {
          DBUG_PRINT("fk", ("Foreign_key_chain already visited: %s %d",
                            table_c->s->table_name.str, child_key_idx));
          // InnoDB FK returns ER_NO_REFERENCED_ROW_2
          return report_no_referenced_row_error(thd, table_c, fk_c);
        }
        chain->mark_child_visited(table_c->s->db.str,
                                  table_c->s->table_name.str, child_key_idx);
        DBUG_PRINT("fk", ("Foreign_key_chain visiting: %s %d",
                          table_c->s->table_name.str, child_key_idx));

        // Add parent table and key info into the chain
        chain->add_parent_table(table_p->s->db.str, table_p->s->table_name.str);
        chain->mark_parent_visited(table_p->s->db.str,
                                   table_p->s->table_name.str, parent_key_idx);
        DBUG_PRINT("fk", ("Foreign_key_chain visiting parent: %s %d",
                          table_p->s->table_name.str, parent_key_idx));
      } else if (dml_type == enum_fk_dml_type::FK_DELETE &&
                 dd::Foreign_key::RULE_RESTRICT != fk_c->delete_rule &&
                 dd::Foreign_key::RULE_SET_DEFAULT != fk_c->delete_rule &&
                 dd::Foreign_key::RULE_NO_ACTION != fk_c->delete_rule) {
        if (chain->table_exists(table_c->s->db.str,
                                table_c->s->table_name.str)) {
          DBUG_PRINT("fk", ("delete cascade on same table detected: %s",
                            table_c->s->table_name.str));
          return false;
        }
        // Add parent table and key info into the chain
        chain->add_parent_table(table_p->s->db.str, table_p->s->table_name.str);
        chain->mark_parent_visited(table_p->s->db.str,
                                   table_p->s->table_name.str, parent_key_idx);
        DBUG_PRINT("fk", ("Foreign_key_chain visiting parent: %s %d",
                          table_p->s->table_name.str, parent_key_idx));
      }
      chain->add_foreign_key(table_c->s->db.str, fk_c->fk_name.str);
      fk_added_to_chain = true;
      thd->inc_fk_cascade_chain_tables();
    }
    DBUG_PRINT("fk", ("check_child_fk_ref(): Added %s to Foreign_key_chain %d",
                      fk_c->fk_name.str, chain->size()));
  }
  auto cleanup_chain_guard = create_scope_guard([&] {
    if (chain && fk_added_to_chain) {
      chain->remove_foreign_key(table_c->s->db.str, fk_c->fk_name.str);
      thd->dec_fk_cascade_chain_tables();
      DBUG_PRINT("fk",
                 ("check_child_fk_ref(): Removed %s from Foreign_key_chain %d",
                  fk_c->fk_name.str, chain->size()));
    }
  });

  DBUG_PRINT("fk", ("check_child_fk_ref(): Parent key: name = %s, index = %d, "
                    "Child key: name = %s, index = %d",
                    key_info_p->name, parent_key_idx, key_info_c->name,
                    child_key_idx));

  // prepare key value to search
  key_part_map key_map = HA_WHOLE_KEY;
  if (fk_c->columns != key_info_c->actual_key_parts) {
    // if child key contains hidden parts or uses partial key
    key_map = make_prev_keypart_map(fk_c->columns);
  }

  uchar key_value[MAX_KEY_LENGTH];
  const uchar *p_rec = (dml_type == enum_fk_dml_type::FK_DELETE)
                           ? table_p->record[0]
                           : table_p->record[1];

  int key_len = 0;
  auto copy_result = key_copy_fk(key_value, sizeof(key_value), p_rec,
                                 key_info_p, key_info_c, false, &key_len);
  if (copy_result != copy_status::ok) {
    if (copy_result == copy_status::charset_mismatch) {
      // Handling varying charset may lead to data corruption, so give error
      return report_row_referenced_error(thd, table_c, fk_c);
    }
    // There can't be a matching child row if there's a validation error
    return false;
  }

  int error = 0;
  // Do index scan and check if value exists.
  if ((error = table_c->file->ha_index_init(child_key_idx, true))) {
    table_c->file->print_error(error, MYF(0));
    return true;
  }

  auto cleanup_table_c_index_guard =
      create_scope_guard([table_c] { table_c->file->ha_index_end(); });

  if (!(error = table_c->file->ha_index_read_map(table_c->record[0], key_value,
                                                 key_map, HA_READ_KEY_EXACT))) {
    // In case of self-referencing, if PK and FK value are same, skip adding to
    // chain so that it does not affect cascade depth check.
    if (table_p->s == table_c->s && !is_self_fk_value_same(table_c, fk_c)) {
      chain->add_foreign_key(table_c->s->db.str, fk_c->fk_name.str);
      fk_added_to_chain = true;
    }
    // Check the chain size here rather than when it's incremented above to
    // produce the same behaviour as the InnoDB FK recursion depth check.
    if (chain->size() >= FK_MAX_CASCADE_DEPTH) {
      my_error(ER_FK_DEPTH_EXCEEDED, MYF(0), FK_MAX_CASCADE_DEPTH);
      return true;
    }

    // Check if cascade chain with trigger cascade not exceed the limit
    if (thd->fk_cascade_chain_tables() >= FK_MAX_TABLES_IN_CASCADE_CHAIN) {
      my_error(ER_FK_MAX_TABLES_IN_CASCADE_CHAIN_EXCEEDED, MYF(0),
               FK_MAX_TABLES_IN_CASCADE_CHAIN);
      return true;
    }

    DBUG_PRINT("fk", ("check_child_fk_ref(): Found row in child table %s.%s",
                      table_c->s->db.str, table_c->s->table_name.str));

    // Before propagating cascade, populate the generated columns in record[0],
    // so that it can be used to fill search key buffer in further cascades.
    table_c->use_all_columns();
    if (table_c->has_gcol() &&
        update_generated_read_fields(table_c->record[0], table_c,
                                     child_key_idx)) {
      return true;
    }

    if (on_delete_on_update_restrict_or_no_action(thd, table_c, fk_c,
                                                  dml_type) ||
        on_delete_cascade(thd, table_c, fk_c, dml_type, key_value, key_len,
                          chain, &error) ||
        on_update_cascade(thd, table_p, table_c, fk_c, key_info_p,
                          parent_key_idx, key_info_c, child_key_idx, dml_type,
                          key_value, key_len, chain, &error) ||
        on_delete_on_update_set_null(thd, table_p, table_c, fk_c, dml_type,
                                     key_value, key_len, chain, &error)) {
      return true;  // Error is already reported.
    }
  } else {
    if (error != HA_ERR_END_OF_FILE && error != HA_ERR_KEY_NOT_FOUND) {
      table_c->file->print_error(error, MYF(0));
      return true;
    }
    DBUG_PRINT("fk", ("check_child_fk_ref(): Failed to find row in child "
                      "table %s.%s, error = %d.",
                      table_c->s->db.str, table_c->s->table_name.str, error));
    error = 0;
  }

  return (error != 0);
}

/**
 * @brief  Check all foreign key constraints on child tables for DML operation
 *         on a parent table.
 *
 * @param thd           Thread handle.
 * @param table_p       TABLE instance of a parent table.
 * @param dml_type      DML operation type.
 * @param chain         Foreign key chain.
 *
 * @return true         On error.
 * @return false        On Success.
 */
static bool check_all_child_fk_ref(THD *thd, const TABLE *table_p,
                                   enum_fk_dml_type dml_type,
                                   Foreign_key_chain *chain) {
  DBUG_PRINT("fk", ("check_all_child_fk_ref(): table: %s.%s",
                    table_p->s->db.str, table_p->s->table_name.str));
  bool owns_fk_chain = false;
  if (chain == nullptr) {
    DBUG_PRINT(
        "fk",
        ("check_all_child_fk_ref(): Foreign_key_chain created for table: %s.%s",
         table_p->s->db.str, table_p->s->table_name.str));
    chain = new Foreign_key_chain();
    if (!chain) return true;  // OOM
    owns_fk_chain = true;
  }
  auto free_chain_guard = create_scope_guard([&] {
    if (owns_fk_chain) {
      DBUG_PRINT("fk", ("Foreign_key_chain deleted for table: %s",
                        table_p->s->table_name.str));
      delete chain;
      chain = nullptr;
    }
  });

  TABLE_SHARE *share_p = table_p->s;
  for (TABLE_SHARE_FOREIGN_KEY_PARENT_INFO *fk_p = share_p->foreign_key_parent;
       fk_p < share_p->foreign_key_parent + share_p->foreign_key_parents;
       ++fk_p) {
    bool table_exists = false;
    bool is_table_opened = false;

    TABLE *table_c = nullptr;
    // If table exists then get it from the THD::open_tables list.
    if (get_foreign_key_table(thd, fk_p->referencing_table_db.str,
                              fk_p->referencing_table_name.str,
                              fk_p->fk_name.str, dml_type, false, &table_c,
                              &table_exists, &is_table_opened)) {
      return true;  // Error is already reported.
    }

    if (!table_exists) {
      // With FKC = OFF, child table can be dropped
      my_error(ER_ROW_IS_REFERENCED_2, MYF(0), "");
      return true;
    }

    auto opened_table_guard = create_scope_guard([table_c, &is_table_opened]() {
      if (is_table_opened) {
        mysql_mutex_lock(&LOCK_open);
        // Release the TABLE's histograms back to the share.
        if (table_c->histograms != nullptr) {
          table_c->s->m_histograms->release(table_c->histograms);
          table_c->histograms = nullptr;
        }
        closefrm(table_c, true);
        mysql_mutex_unlock(&LOCK_open);
      }
    });

    TABLE_SHARE *share_c = table_c->s;
    for (TABLE_SHARE_FOREIGN_KEY_INFO *fk_c = share_c->foreign_key;
         fk_c < share_c->foreign_key + share_c->foreign_keys; ++fk_c) {
      if (my_strcasecmp(table_alias_charset, fk_c->fk_name.str,
                        fk_p->fk_name.str) == 0) {
        int lock_type = F_WRLCK;
        if (is_table_opened) {
          // Take appropriate external lock
          if (((dml_type != enum_fk_dml_type::FK_UPDATE) &&
               (dd::Foreign_key::RULE_RESTRICT == fk_c->delete_rule ||
                dd::Foreign_key::RULE_SET_DEFAULT == fk_c->delete_rule ||
                dd::Foreign_key::RULE_NO_ACTION == fk_c->delete_rule)) ||
              ((dml_type == enum_fk_dml_type::FK_UPDATE) &&
               (dd::Foreign_key::RULE_RESTRICT == fk_c->update_rule ||
                dd::Foreign_key::RULE_SET_DEFAULT == fk_c->update_rule ||
                dd::Foreign_key::RULE_NO_ACTION == fk_c->update_rule))) {
            lock_type = F_RDLCK;
          }

          if (table_c->file->ha_external_lock(thd, lock_type)) {
            return true;
          }

          if (lock_type == F_RDLCK) {
            table_c->file->ha_extra(HA_EXTRA_ENABLE_LOCKING_RECORD);
          }
        }
        auto external_lock_guard =
            create_scope_guard([thd, table_c, &is_table_opened, &lock_type]() {
              if (is_table_opened) {
                if (lock_type == F_RDLCK) {
                  table_c->file->ha_extra(HA_EXTRA_RESET_LOCKING_RECORD);
                }
                table_c->file->ha_external_lock(thd, F_UNLCK);
              }
            });

        const bool is_cascade_action = (lock_type == F_WRLCK);
        if (is_cascade_action) {
          /*
            For self referencing foreign key, table handle is opened
            during execution phase, so second handle is not added to
            query table list. As prelocking is done on base table
            handle, triggers can be loaded here on second table handle for
            cascade operation.
          */
          if (is_table_opened && table_c->s == table_p->s) {
            if (table_c->triggers &&
                !table_c->triggers->has_load_been_finalized()) {
              if (table_c->triggers->finalize_load(thd)) return true;
            }
          }

          // Updating a table in a FK CASCADE action induced by a trigger or
          // stored function is not allowed as the table is already used during
          // FK cascade handling.
          if (is_cascade_triggers_enabled(thd)) {
            for (TABLE *table = thd->open_tables; table != nullptr;
                 table = table->next) {
              if (table->s == table_c->s && table->query_id &&
                  table->query_id != table_c->query_id) {
                my_error(ER_CANT_UPDATE_USED_TABLE_IN_FK_CASCADE, MYF(0),
                         table_c->s->table_name.str);
                return true;
              }
            }
          }
        }

        auto trigger_load_guard =
            create_scope_guard([table_c, &is_table_opened]() {
              if (is_table_opened && table_c->triggers &&
                  table_c->triggers->has_load_been_finalized()) {
                table_c->triggers->~Table_trigger_dispatcher();
              }
            });

        if (check_child_fk_ref(thd, table_p, table_c, fk_c, dml_type, chain))
          return true;
        break;
      }
    }
  }

  return false;
}

/* @brief Checks foreign key constraint on parent table.
 *
 * @param thd             Thread handle.
 * @param table_c         TABLE instance of a child table.
 * @param table_p         TABLE instance of a parent table.
 * @param fk              Foreign key information.
 * @param dml_type        DML operation type.
 *
 * @return true           Foreign key constraint violated.
 * @return false          Otherwise.
 */
static bool check_parent_fk_ref(THD *thd, const TABLE *table_c, TABLE *table_p,
                                TABLE_SHARE_FOREIGN_KEY_INFO *fk,
                                enum_fk_dml_type dml_type) {
  assert(dml_type == enum_fk_dml_type::FK_UPDATE ||
         dml_type == enum_fk_dml_type::FK_INSERT);

  DBUG_PRINT(
      "fk", ("check_parent_fk_ref(): child table = %s.%s, parent table "
             "= %s.%s, FK name=%s",
             table_c->s->db.str, table_c->s->table_name.str, table_p->s->db.str,
             table_p->s->table_name.str, fk->fk_name.str));

  if (dml_type == enum_fk_dml_type::FK_UPDATE) {
    if (!is_column_updated(table_c, fk->columns, fk->referencing_column_names))
      return false;
  }

  int error = 0;
  uint child_key_idx =
      get_key_index(table_c, fk->columns, fk->referencing_column_names);

  // Generate auto increment value early for INSERT to perform FK check
  if (table_c->next_number_field &&
      table_c->s->next_number_index == child_key_idx &&
      !table_c->autoinc_field_has_explicit_non_null_value &&
      dml_type == enum_fk_dml_type::FK_INSERT) {
    if (table_c->file->update_auto_increment()) return true;
    if (thd->first_successful_insert_id_in_cur_stmt == 0)
      thd->first_successful_insert_id_in_cur_stmt =
          table_c->file->insert_id_for_cur_row;
  }

  uint parent_key_idx =
      get_key_index(table_p, fk->columns, fk->referenced_column_names);

  KEY *key_info_p = table_p->key_info + parent_key_idx;
  KEY *key_info_c = table_c->key_info + child_key_idx;
  DBUG_PRINT("fk", ("check_parent_fk_ref(): Parent index: name = %s, index = "
                    "%d, Child index: name = %s, index = %d",
                    key_info_p->name, parent_key_idx, key_info_c->name,
                    child_key_idx));

  // MATCH SIMPLE - If any one of the column is NULL, RIC is satisfied
  if (is_any_key_fld_value_null(table_c->record[0], key_info_c)) return false;

  // prepare key value to search
  uchar key_value[MAX_KEY_LENGTH];
  int key_len = 0;
  auto copy_result =
      key_copy_fk(key_value, sizeof(key_value), table_c->record[0], key_info_c,
                  key_info_p, true, &key_len);
  DBUG_PRINT(
      "fk",
      ("check_parent_fk_ref(): child key length = %d, copied key len = %d",
       key_info_c->key_length, key_len));

  // A copy_result value of not ok indicates that the key value from the
  // child cannot possibly match a parent row.
  if (copy_result != copy_status::ok) {
    return report_no_referenced_row_error(thd, table_c, fk);
  }

  key_part_map key_map = HA_WHOLE_KEY;
  if (fk->columns != key_info_p->actual_key_parts) {
    // if parent key contains hidden parts or uses partial key
    key_map = make_prev_keypart_map(fk->columns);
  }

  DBUG_EXECUTE_IF("check_parent_fk_index_init_failure",
                  { DBUG_SET("+d,ha_index_init_fail"); });
  // Check if value exists.
  if ((error = table_p->file->ha_index_init(parent_key_idx, true))) {
    table_p->file->print_error(error, MYF(0));
    DBUG_EXECUTE_IF("check_parent_fk_index_init_failure",
                    { DBUG_SET("-d,ha_index_init_fail"); });
    return true;
  }

  auto close_index_guard =
      create_scope_guard([table_p] { table_p->file->ha_index_end(); });

  error = table_p->file->ha_index_read_map(table_p->record[0], key_value,
                                           key_map, HA_READ_KEY_EXACT);
  if (error != 0) {
    if (error != HA_ERR_END_OF_FILE && error != HA_ERR_KEY_NOT_FOUND) {
      table_p->file->print_error(error, MYF(0));
      return true;
    }
    DBUG_PRINT("fk",
               ("check_parent_fk_ref(): Row not found in parent table %s.%s",
                table_p->s->db.str, table_p->s->table_name.str));
    return report_no_referenced_row_error(thd, table_c, fk);
  }

  DBUG_PRINT("fk", ("check_parent_fk_ref(): Row found in parent table %s.%s",
                    table_p->s->db.str, table_p->s->table_name.str));
  return false;
}

/**
 * @brief  Helper function to skip foreign checks.
 *
 * @param thd          Thread Handle.
 * @param table        TABLE instance of a table.
 *
 * @return true        If foreign key check for a table should be skipped.
 * @return false       Otherwise.
 */
static bool skip_foreign_key_checks(THD *thd, const TABLE *table) {
  // Skip foreign key checks if FOREIGN_KEY_CHECKS variables is set.
  if (thd->variables.option_bits & OPTION_NO_FOREIGN_KEY_CHECKS) return true;

  // Skip foreign key checks for data-dictionary tables.
  if (dd::get_dictionary()->is_dd_schema_name(table->s->db.str)) return true;

  return false;
}

bool check_all_parent_fk_ref(THD *thd, const TABLE *table_c,
                             enum_fk_dml_type dml_type,
                             const TABLE_SHARE_FOREIGN_KEY_INFO *ignore_fk) {
  if (skip_foreign_key_checks(thd, table_c)) return false;

  DBUG_PRINT("fk", ("check_all_parent_fk_ref() on table: %s.%s",
                    table_c->s->db.str, table_c->s->table_name.str));

  const TABLE_SHARE *share_c = table_c->s;
  for (TABLE_SHARE_FOREIGN_KEY_INFO *fk = share_c->foreign_key;
       fk < share_c->foreign_key + share_c->foreign_keys; ++fk) {
    /**
      During FK cascade, skip parent table foreign key value check because
      parent row is updated after updating the child row.
    */
    if (fk == ignore_fk) continue;

    bool self_ref_key =
        ((my_strcasecmp(table_alias_charset, table_c->s->db.str,
                        fk->referenced_table_db.str) == 0) &&
         (my_strcasecmp(table_alias_charset, table_c->s->table_name.str,
                        fk->referenced_table_name.str) == 0));

    bool is_self_ref_fk_with_same_value =
        ((dml_type != enum_fk_dml_type::FK_UPDATE) && self_ref_key &&
         is_self_fk_value_same(table_c, fk));
    if (is_self_ref_fk_with_same_value) continue;

    // If self referencing key and value is not same. Open new table instance
    // for the parent (self).
    bool open_table = self_ref_key;

    bool table_exists = false;
    bool is_table_opened = false;
    TABLE *table_p = nullptr;
    if (get_foreign_key_table(thd, fk->referenced_table_db.str,
                              fk->referenced_table_name.str, fk->fk_name.str,
                              dml_type, open_table, &table_p, &table_exists,
                              &is_table_opened)) {
      return true;
    }

    if (!table_exists) {
      // MATCH SIMPLE - If any one of the column is NULL, RIC is satisfied
      uint key_idx =
          get_key_index(table_c, fk->columns, fk->referencing_column_names);
      if (is_any_key_fld_value_null(table_c->record[0],
                                    table_c->key_info + key_idx))
        continue;
      return report_no_referenced_row_error(thd, table_c, fk);
    }
    assert(table_c != nullptr);

    if (is_table_opened) {
      if (table_p->file->ha_external_lock(thd, F_RDLCK)) {
        closefrm(table_p, true);
        return true;
      }
      table_p->file->ha_extra(HA_EXTRA_ENABLE_LOCKING_RECORD);
    }
    auto opened_table_guard =
        create_scope_guard([thd, table_p, is_table_opened]() {
          if (is_table_opened) {
            table_p->file->ha_extra(HA_EXTRA_RESET_LOCKING_RECORD);
            table_p->file->ha_external_lock(thd, F_UNLCK);

            mysql_mutex_lock(&LOCK_open);
            // Release the TABLE's histograms back to the share.
            if (table_p->histograms != nullptr) {
              table_p->s->m_histograms->release(table_p->histograms);
              table_p->histograms = nullptr;
            }

            closefrm(table_p, true);
            mysql_mutex_unlock(&LOCK_open);
          }
        });

    if (check_parent_fk_ref(thd, table_c, table_p, fk, dml_type)) {
      return true;
    }
  }

  return false;
}

bool check_all_child_fk_ref(THD *thd, const TABLE *table,
                            enum_fk_dml_type dml_type) {
  if (skip_foreign_key_checks(thd, table)) return false;

  if (!thd->in_sub_stmt) thd->reset_fk_cascade_chain_tables();

  return check_all_child_fk_ref(thd, table, dml_type, nullptr);
}

bool is_foreign_key_table_opened(THD *thd, const char *db_name,
                                 const char *table_name, const char *fk_name,
                                 bool *is_unused_table) {
  TABLE *fk_table =
      find_fk_table_from_open_tables(thd, db_name, table_name, fk_name);
  if (fk_table == nullptr) return false;
  *is_unused_table = static_cast<bool>(fk_table->query_id == 0);
  return true;
}
