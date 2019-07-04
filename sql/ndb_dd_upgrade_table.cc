/* Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/ndb_dd_upgrade_table.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <algorithm>
#include <string>

#include "lex_string.h"
#include "m_string.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_dbug.h"
#include "my_dir.h"
#include "my_io.h"
#include "my_loglevel.h"
#include "my_sys.h"
#include "my_user.h"  // parse_user
#include "mysql/psi/mysql_file.h"
#include "mysql/psi/psi_base.h"
#include "mysql/udf_registration_types.h"
#include "mysql_com.h"
#include "mysqld_error.h"                    // ER_*
#include "sql/dd/cache/dictionary_client.h"  // dd::cache::Dictionary_client
#include "sql/dd/dd_schema.h"                // Schema_MDL_locker
#include "sql/dd/dd_table.h"                 // create_dd_user_tableW
#include "sql/dd/dictionary.h"
#include "sql/dd/impl/utils.h"  // execute_query
#include "sql/dd/properties.h"
#include "sql/dd/types/foreign_key.h"  // dd::Foreign_key
#include "sql/dd/types/table.h"        // dd::Table
#include "sql/dd/upgrade_57/upgrade.h"
#include "sql/field.h"
#include "sql/handler.h"  // legacy_db_type
#include "sql/key.h"
#include "sql/lock.h"  // Tablespace_hash_set
#include "sql/log.h"
#include "sql/mdl.h"
#include "sql/mysqld.h"  // mysql_real_data_home
#include "sql/ndb_log.h"
#include "sql/ndb_thd.h"
#include "sql/ndb_thd_ndb.h"  // Thd_ndb
#include "sql/parse_file.h"   // File_option
#include "sql/partition_element.h"
#include "sql/partition_info.h"  // partition_info
#include "sql/psi_memory_key.h"  // key_memory_TABLE
#include "sql/sp_head.h"         // sp_head
#include "sql/sql_alter.h"
#include "sql/sql_base.h"  // open_tables
#include "sql/sql_const.h"
#include "sql/sql_lex.h"  // new_empty_query_block
#include "sql/sql_list.h"
#include "sql/sql_parse.h"  // check_string_char_length
#include "sql/sql_table.h"  // build_tablename
#include "sql/system_variables.h"
#include "sql/table.h"     // Table_check_intact
#include "sql/thd_raii.h"  // Disable_autocommit_guard
#include "sql/thr_malloc.h"
#include "sql/transaction.h"  // trans_commit
#include "sql_string.h"
#include "thr_lock.h"

class Sroutine_hash_entry;
namespace dd {
class Schema;
class Table;
}  // namespace dd

namespace dd {
namespace ndb_upgrade {

/*
  Custom version of standard offsetof() macro which can be used to get
  offsets of members in class for non-POD types (according to the current
  version of C++ standard offsetof() macro can't be used in such cases and
  attempt to do so causes warnings to be emitted, OTOH in many cases it is
  still OK to assume that all instances of the class has the same offsets
  for the same members).

  This is temporary solution which should be removed once File_parser class
  and related routines are refactored.
*/

#define my_offsetof_upgrade(TYPE, MEMBER) \
  ((size_t)((char *)&(((TYPE *)0x10)->MEMBER) - (char *)0x10))

/**
  Bootstrap thread executes SQL statements.
  Any error in the execution of SQL statements causes call to my_error().
  At this moment, error handler hook is set to my_message_stderr.
  my_message_stderr() prints the error messages to standard error stream but
  it does not follow the standard error format. Further, the error status is
  not set in Diagnostics Area.

  This class is to create RAII error handler hooks to be used when executing
  statements from bootstrap thread.

  It will print the error in the standard error format.
  Diagnostics Area error status will be set to avoid asserts.
  Error will be handler by caller function.
*/

class Bootstrap_error_handler {
 private:
  void (*m_old_error_handler_hook)(uint, const char *, myf);

  //  Set the error in DA. Optionally print error in log.
  static void my_message_bootstrap(uint error, const char *str, myf MyFlags) {
    set_abort_on_error(error);
    my_message_sql(error, str, MyFlags | (m_log_error ? ME_ERRORLOG : 0));
  }

  // Set abort on error flag and enable error logging for certain fatal error.
  static void set_abort_on_error(uint error) {
    switch (error) {
      case ER_WRONG_COLUMN_NAME: {
        abort_on_error = true;
        m_log_error = true;
        break;
      }
      default:
        break;
    }
  }

 public:
  Bootstrap_error_handler() {
    m_old_error_handler_hook = error_handler_hook;
    error_handler_hook = my_message_bootstrap;
  }

  // Mark as error is set.
  void set_log_error(bool log_error) { m_log_error = log_error; }

  ~Bootstrap_error_handler() { error_handler_hook = m_old_error_handler_hook; }
  static bool m_log_error;
  static bool abort_on_error;
};

bool Bootstrap_error_handler::m_log_error = true;
bool Bootstrap_error_handler::abort_on_error = false;

/**
  RAII to handle MDL locks while upgrading.
*/

class Upgrade_MDL_guard {
  MDL_ticket *m_mdl_ticket_schema;
  MDL_ticket *m_mdl_ticket_table;
  bool m_tablespace_lock;

  THD *m_thd;

 public:
  bool acquire_lock(const String_type &db_name, const String_type &table_name) {
    return dd::acquire_exclusive_schema_mdl(m_thd, db_name.c_str(), false,
                                            &m_mdl_ticket_schema) ||
           dd::acquire_exclusive_table_mdl(m_thd, db_name.c_str(),
                                           table_name.c_str(), false,
                                           &m_mdl_ticket_table);
  }
  bool acquire_lock_tablespace(Tablespace_hash_set *tablespace_names) {
    m_tablespace_lock = true;
    return lock_tablespace_names(m_thd, tablespace_names,
                                 m_thd->variables.lock_wait_timeout);
  }

  Upgrade_MDL_guard(THD *thd)
      : m_mdl_ticket_schema(nullptr),
        m_mdl_ticket_table(nullptr),
        m_tablespace_lock(false),
        m_thd(thd) {}
  ~Upgrade_MDL_guard() {
    if (m_mdl_ticket_schema != nullptr)
      dd::release_mdl(m_thd, m_mdl_ticket_schema);
    if ((m_mdl_ticket_table != nullptr) || m_tablespace_lock)
      m_thd->mdl_context.release_transactional_locks();
  }
};

/**
  RAII to handle cleanup after table upgrading.
*/

class Table_upgrade_guard {
  THD *m_thd;
  TABLE *m_table;
  sql_mode_t m_sql_mode;
  handler *m_handler;
  bool m_is_table_open;
  LEX *m_lex_saved;
  Item *m_free_list_saved;

 public:
  void update_handler(handler *handler) { m_handler = handler; }

  void update_lex(LEX *lex) { m_lex_saved = lex; }

  Table_upgrade_guard(THD *thd, TABLE *table)
      : m_thd(thd),
        m_table(table),
        m_handler(nullptr),
        m_is_table_open(false),
        m_lex_saved(nullptr) {
    m_sql_mode = m_thd->variables.sql_mode;
    m_thd->variables.sql_mode = m_sql_mode;

    /*
      During table upgrade, allocation for the Item objects could happen in the
      mem_root set for this scope. Hence saving current free_list state. Item
      objects stored in THD::free_list during table upgrade are deallocated in
      the destructor of the class.
    */
    m_free_list_saved = thd->item_list();
    m_thd->reset_item_list();
  }

  ~Table_upgrade_guard() {
    m_thd->variables.sql_mode = m_sql_mode;
    m_thd->work_part_info = 0;

    // Free item list for partitions
    if (m_table->s->m_part_info) free_items(m_table->s->m_part_info->item_list);

    // Free items allocated during table upgrade and restore old free list.
    m_thd->free_items();
    m_thd->set_item_list(m_free_list_saved);

    // Restore thread lex
    if (m_lex_saved != nullptr) {
      lex_end(m_thd->lex);
      m_thd->lex = m_lex_saved;
    }

    /*
      Free item list for generated columns
      Items being freed were allocated by fix_generated_columns_for_upgrade(),
      and TABLE instance might have its own items allocated which will be freed
      by closefrm() call.
    */
    if (m_table->s->field) {
      for (Field **ptr = m_table->s->field; *ptr; ptr++) {
        if ((*ptr)->gcol_info) free_items((*ptr)->gcol_info->item_list);
      }
    }

    // Close the table. It was opened using ha_open for FK information.
    if (m_is_table_open) {
      (void)closefrm(m_table, false);
    }

    free_table_share(m_table->s);

    destroy(m_handler);
  }
};

/**
  Fill HA_CREATE_INFO from TABLE_SHARE.
*/

static void fill_create_info_for_upgrade(HA_CREATE_INFO *create_info,
                                         const TABLE *table) {
  /*
    Storage Engine names will be resolved when reading .frm file.
    We can assume here that SE is present and initialized.
  */
  create_info->db_type = table->s->db_type();

  create_info->init_create_options_from_share(table->s, 0);

  create_info->row_type = table->s->row_type;

  // DD framework handles only these options
  uint db_create_options = table->s->db_create_options;
  db_create_options &=
      (HA_OPTION_PACK_RECORD | HA_OPTION_PACK_KEYS | HA_OPTION_NO_PACK_KEYS |
       HA_OPTION_CHECKSUM | HA_OPTION_NO_CHECKSUM | HA_OPTION_DELAY_KEY_WRITE |
       HA_OPTION_NO_DELAY_KEY_WRITE | HA_OPTION_STATS_PERSISTENT |
       HA_OPTION_NO_STATS_PERSISTENT);
  create_info->table_options = db_create_options;
}

/**
   Create partition information for upgrade.
   This function uses the same method to create partition information
   as done by open_table_from_share().
*/

static bool fill_partition_info_for_upgrade(THD *thd, TABLE_SHARE *share,
                                            const FRM_context *frm_context,
                                            TABLE *table) {
  thd->work_part_info = nullptr;

  // If partition information is present in TABLE_SHARE
  if (share->partition_info_str_len && table->file) {
    // Parse partition expression and create Items.
    if (unpack_partition_info(thd, table, share,
                              frm_context->default_part_db_type, false))
      return false;

    // dd::create_dd_user_table() uses thd->part_info to get partition values.
    thd->work_part_info = table->part_info;
    // This assignment is necessary to free the partition_info
    share->m_part_info = table->part_info;
    /*
      For normal TABLE instances, free_items() is called by closefrm().
      For this scenario, free_items() will be called by destructor of
      Table_upgrade_guard.
    */
    share->m_part_info->item_list = table->part_info->item_list;
  }
  return true;
}

/**
  Fix generated columns.

  @param[in]  thd            Thread handle.
  @param[in]  table          TABLE object.
  @param[in]  create_fields  List of Create_fields

  @retval true   ON SUCCESS
  @retval false  ON FAILURE

*/

static bool fix_generated_columns_for_upgrade(
    THD *thd, TABLE *table, List<Create_field> &create_fields) {
  Create_field *sql_field;
  bool error_reported = false;
  bool error = true;

  if (table->s->vfields) {
    List_iterator<Create_field> itc(create_fields);
    Field **field_ptr;

    for (field_ptr = table->s->field; (sql_field = itc++); field_ptr++) {
      // Field has generated col information.
      if (sql_field->gcol_info && (*field_ptr)->gcol_info) {
        if (unpack_value_generator(
                thd, table, &(*field_ptr)->gcol_info, VGS_GENERATED_COLUMN,
                (*field_ptr)->field_name, *field_ptr, false, &error_reported)) {
          error = false;
          break;
        }
        sql_field->gcol_info->expr_item = (*field_ptr)->gcol_info->expr_item;
      }
    }
  }

  return error;
}

/**
  Call handler API to get storage engine specific metadata. Storage Engine
  should fill table id and version

  @param[in]    thd             Thread Handle
  @param[in]    schema_name     Name of schema
  @param[in]    table_name      Name of table
  @param[in]    table           TABLE object

  @retval true   ON SUCCESS
  @retval false  ON FAILURE
*/

static bool set_se_data_for_user_tables(THD *thd,
                                        const String_type &schema_name,
                                        const String_type &table_name,
                                        TABLE *table) {
  Disable_autocommit_guard autocommit_guard(thd);
  dd::Schema_MDL_locker mdl_locker(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  const dd::Schema *sch = nullptr;
  if (thd->dd_client()->acquire<dd::Schema>(schema_name.c_str(), &sch))
    return false;

  dd::Table *table_def = nullptr;
  if (thd->dd_client()->acquire_for_modification(
          schema_name.c_str(), table_name.c_str(), &table_def)) {
    // Error is reported by the dictionary subsystem.
    return false;
  }

  if (!table_def) {
    /*
       Should never hit this case as the caller of this function stores
       the information in dictionary.
    */
    ndb_log_error("Error in fetching %s.%s table data from dictionary",
                  table_name.c_str(), schema_name.c_str());
    return false;
  }

  if (table->file->ha_upgrade_table(thd, schema_name.c_str(),
                                    table_name.c_str(), table_def, table)) {
    trans_rollback_stmt(thd);
    trans_rollback(thd);
    return false;
  }

  if (thd->dd_client()->update<dd::Table>(table_def)) {
    trans_rollback_stmt(thd);
    trans_rollback(thd);
    return false;
  }

  return !(trans_commit_stmt(thd) || trans_commit(thd));
}

/**
  Set names of parent keys (unique constraint names matching FK
  in parent tables) for the FKs in which table participates.

  @param  thd         Thread context.
  @param  schema_name Name of schema.
  @param  table_name  Name of table.
  @param  hton        Table's handlerton.

  @retval true  - Success.
  @retval false - Failure.
*/

static bool fix_fk_parent_key_names(THD *thd, const String_type &schema_name,
                                    const String_type &table_name,
                                    handlerton *hton) {
  if (!(hton->flags & HTON_SUPPORTS_FOREIGN_KEYS)) {
    // Shortcut. No need to process FKs for engines which don't support them.
    return true;
  }

  Disable_autocommit_guard autocommit_guard(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  dd::Table *table_def = nullptr;

  if (thd->dd_client()->acquire_for_modification(
          schema_name.c_str(), table_name.c_str(), &table_def)) {
    // Error is reported by the dictionary subsystem.
    return false;
  }

  if (!table_def) {
    /*
      Should never hit this case as the caller of this function stores
      the information in dictionary.
    */
    ndb_log_error("Error in fetching %s.%s table data from dictionary",
                  schema_name.c_str(), table_name.c_str());
    return false;
  }

  for (dd::Foreign_key *fk : *(table_def->foreign_keys())) {
    const dd::Table *parent_table_def = nullptr;

    if (my_strcasecmp(table_alias_charset,
                      fk->referenced_table_schema_name().c_str(),
                      schema_name.c_str()) == 0 &&
        my_strcasecmp(table_alias_charset, fk->referenced_table_name().c_str(),
                      table_name.c_str()) == 0) {
      // This FK references the same table as on which it is defined.
      parent_table_def = table_def;
    } else {
      if (thd->dd_client()->acquire(fk->referenced_table_schema_name().c_str(),
                                    fk->referenced_table_name().c_str(),
                                    &parent_table_def))
        return false;
    }

    if (parent_table_def == nullptr) {
      /*
        This is legal situaton. Parent table was not upgraded yet or
        simply doesn't exist. In the former case our FKs will be
        updated with the correct parent key names once parent table
        is upgraded.
      */
    } else {
      bool is_self_referencing_fk = (parent_table_def == table_def);
      if (prepare_fk_parent_key(hton, parent_table_def, nullptr, nullptr,
                                is_self_referencing_fk, fk))
        return false;
    }
  }

  /*
    Adjust parent key names for FKs belonging to already upgraded tables,
    which reference the table being upgraded here. Also adjust the
    foreign key parent collection, both for this table and for other
    tables being referenced by this one.
  */
  if (adjust_fk_children_after_parent_def_change(
          thd,
          true,  // Check charsets.
          schema_name.c_str(), table_name.c_str(), hton, table_def, nullptr,
          false) ||  // Don't invalidate
                     // TDC we don't have
                     // proper MDL.
      adjust_fk_parents(thd, schema_name.c_str(), table_name.c_str(), true,
                        nullptr)) {
    trans_rollback_stmt(thd);
    trans_rollback(thd);
    return false;
  }

  if (thd->dd_client()->update(table_def)) {
    trans_rollback_stmt(thd);
    trans_rollback(thd);
    return false;
  }

  return !(trans_commit_stmt(thd) || trans_commit(thd));
}

/**
  THD::mem_root is only switched with the given mem_root and switched back
  on destruction. This does not free any mem_root.
 */
class Thd_mem_root_guard {
  THD *m_thd;
  MEM_ROOT *m_thd_prev_mem_root;

 public:
  Thd_mem_root_guard(THD *thd, MEM_ROOT *mem_root) {
    m_thd = thd;
    m_thd_prev_mem_root = m_thd->mem_root;
    m_thd->mem_root = mem_root;
  }
  ~Thd_mem_root_guard() { m_thd->mem_root = m_thd_prev_mem_root; }
};

/**
  Read .frm files and enter metadata for tables
*/

bool migrate_table_to_dd(THD *thd, const String_type &schema_name,
                         const String_type &table_name,
                         const unsigned char *frm_data,
                         const unsigned int unpacked_len,
                         bool is_fix_view_cols_and_deps) {
  DBUG_ENTER("migrate_table_to_dd");

  FRM_context frm_context;
  TABLE_SHARE share;
  TABLE table;
  Field **ptr, *field;
  handler *file = nullptr;
  MEM_ROOT root(PSI_NOT_INSTRUMENTED, 65536);
  Thd_mem_root_guard root_guard(thd, &root);

  // Write .frm file to data directory
  File frm_file;
  char index_file[FN_REFLEN];
  char path[FN_REFLEN + 1];
  build_table_filename(path, sizeof(path) - 1 - reg_ext_length,
                       schema_name.c_str(), table_name.c_str(), "", 0);

  frm_file = mysql_file_create(key_file_frm,
                               fn_format(index_file, path, "", reg_ext,
                                         MY_UNPACK_FILENAME | MY_APPEND_EXT),
                               CREATE_MODE, O_RDWR | O_TRUNC, MYF(MY_WME));

  if (frm_file < 0) {
    ndb_log_error("Could not create frm file, error: %d", frm_file);
    DBUG_RETURN(false);
  }

  if (mysql_file_write(frm_file, frm_data, unpacked_len,
                       MYF(MY_WME | MY_NABP))) {
    ndb_log_error("Could not write frm file ");
    // Delete frm file
    mysql_file_delete(key_file_frm, index_file, MYF(0));
    DBUG_RETURN(false);
  }

  (void)mysql_file_close(frm_file, MYF(0));

  // Create table share for tables
  if (create_table_share_for_upgrade(thd, path, &share, &frm_context,
                                     schema_name.c_str(), table_name.c_str(),
                                     is_fix_view_cols_and_deps)) {
    ndb_log_error("Error in creating TABLE_SHARE from %s.frm file",
                  table_name.c_str());
    // Delete frm file
    mysql_file_delete(key_file_frm, index_file, MYF(0));
    DBUG_RETURN(false);
  }

  // Delete frm file
  mysql_file_delete(key_file_frm, index_file, MYF(0));

  // Fix pointers in TABLE, TABLE_SHARE
  table.s = &share;
  table.in_use = thd;

  // Object to handle cleanup.
  LEX lex;
  Table_upgrade_guard table_guard(thd, &table);

  // Get the handler
  if (!(file = get_new_handler(&share, share.partition_info_str_len != 0,
                               thd->mem_root, share.db_type()))) {
    ndb_log_error("Error in creating handler object for table %s.%s",
                  schema_name.c_str(), table_name.c_str());
    DBUG_RETURN(false);
  }
  table.file = file;
  table_guard.update_handler(file);

  if (table.file->set_ha_share_ref(&share.ha_share)) {
    ndb_log_error("Error in setting handler reference for table %s.%s",
                  table_name.c_str(), schema_name.c_str());
    DBUG_RETURN(false);
  }

  /*
    Fix pointers in TABLE, TABLE_SHARE and fields.
    These steps are necessary for correct handling of
    default values by Create_field constructor.
  */
  table.s->db_low_byte_first = table.file->low_byte_first();
  table.use_all_columns();
  table.record[0] = table.record[1] = share.default_values;
  table.null_row = 0;
  table.field = share.field;
  table.key_info = share.key_info;

  /*
    Storage engine finds the auto_increment column
    based on TABLE::found_next_number_field. auto_increment value is
    maintained by Storage Engine, and it is calculated dynamically
    every time SE opens the table. Without setting this value, SE will
    not set auto_increment value for the table.
  */
  if (share.found_next_number_field)
    table.found_next_number_field =
        table.field[(uint)(share.found_next_number_field - share.field)];

  // Set table_name variable and table in fields
  const char *alias = "";
  for (ptr = share.field; (field = *ptr); ptr++) {
    field->table = &table;
    field->table_name = &alias;
  }

  // Check presence of old data types, always check for "temporal upgrade"
  // since it's not possible to upgrade such tables
  const bool check_temporal_upgrade = true;
  const int error = check_table_for_old_types(&table, check_temporal_upgrade);
  if (error) {
    if (error == HA_ADMIN_NEEDS_DUMP_UPGRADE)
      ndb_log_error(
          "Table upgrade required for "
          "`%-.64s`.`%-.64s`. Please dump/reload table to "
          "fix it!",
          schema_name.c_str(), table_name.c_str());
    else
      ndb_log_error(
          "Table upgrade required. Please do \"REPAIR TABLE `%s`\" "
          "or dump/reload to fix it",
          table_name.c_str());
    Thd_ndb *thd_ndb = get_thd_ndb(thd);
    thd_ndb->push_warning(
        "Table definition contains obsolete data types such "
        "as old temporal or decimal types");
    DBUG_RETURN(false);
  }

  uint i = 0;
  KEY *key_info = share.key_info;

  /*
    Mark all the keys visible and supported algorithm explicit.
    Unsupported algorithms will get fixed by prepare_key() call.
  */
  key_info = share.key_info;
  for (i = 0; i < share.keys; i++, key_info++) {
    key_info->is_visible = true;
    /*
      Fulltext and Spatical indexes will get fixed by
      mysql_prepare_create_table()
    */
    if (key_info->algorithm != HA_KEY_ALG_SE_SPECIFIC &&
        !(key_info->flags & HA_FULLTEXT) && !(key_info->flags & HA_SPATIAL) &&
        table.file->is_index_algorithm_supported(key_info->algorithm))
      key_info->is_algorithm_explicit = true;
  }

  // Fill create_info to be passed to the DD framework.
  HA_CREATE_INFO create_info;
  Alter_info alter_info(thd->mem_root);
  Alter_table_ctx alter_ctx;

  fill_create_info_for_upgrade(&create_info, &table);

  if (prepare_fields_and_keys(thd, nullptr, &table, &create_info, &alter_info,
                              &alter_ctx, create_info.used_fields)) {
    DBUG_RETURN(false);
  }

  // Fix keys and indexes.
  KEY *key_info_buffer;
  uint key_count;

  // Foreign keys are handled at later stage by retrieving info from SE.
  FOREIGN_KEY *dummy_fk_key_info = NULL;
  uint dummy_fk_key_count = 0;

  if (mysql_prepare_create_table(
          thd, schema_name.c_str(), table_name.c_str(), &create_info,
          &alter_info, file, true,  // NDB tables are auto-partitoned.
          &key_info_buffer, &key_count, &dummy_fk_key_info, &dummy_fk_key_count,
          nullptr, 0, nullptr, 0, 0, false /* No FKs here. */)) {
    DBUG_RETURN(false);
  }

  int select_field_pos = alter_info.create_list.elements;
  create_info.null_bits = 0;
  Create_field *sql_field;
  List_iterator<Create_field> it_create(alter_info.create_list);

  for (int field_no = 0; (sql_field = it_create++); field_no++) {
    if (prepare_create_field(thd, &create_info, &alter_info.create_list,
                             &select_field_pos, table.file, sql_field,
                             field_no))
      DBUG_RETURN(false);
  }

  // open_table_from_share and partition expression parsing needs a
  // valid SELECT_LEX to parse generated columns
  LEX *lex_saved = thd->lex;
  thd->lex = &lex;
  lex_start(thd);
  table_guard.update_lex(lex_saved);

  if (!fill_partition_info_for_upgrade(thd, &share, &frm_context, &table))
    DBUG_RETURN(false);

  // Add name of all tablespaces used by partitions to the hash set.
  Tablespace_hash_set tablespace_name_set(PSI_INSTRUMENT_ME);
  if (thd->work_part_info != nullptr) {
    List_iterator<partition_element> partition_iter(
        thd->work_part_info->partitions);
    partition_element *partition_elem;

    while ((partition_elem = partition_iter++)) {
      if (partition_elem->tablespace_name != nullptr) {
        // Add name of all partitions to take MDL
        tablespace_name_set.insert(partition_elem->tablespace_name);
      }
      if (thd->work_part_info->is_sub_partitioned()) {
        // Add name of all sub partitions to take MDL
        List_iterator<partition_element> sub_it(partition_elem->subpartitions);
        partition_element *sub_elem;
        while ((sub_elem = sub_it++)) {
          if (sub_elem->tablespace_name != nullptr) {
            tablespace_name_set.insert(sub_elem->tablespace_name);
          }
        }
      }
    }
  }

  // Add name of the tablespace used by table to the hash set.
  if (share.tablespace != nullptr) tablespace_name_set.insert(share.tablespace);

  /*
    Acquire lock on tablespace names

    No lock is needed when creating DD objects from system thread
    handling server bootstrap/initialization.
    And in cases when lock is required it is X MDL and not IX lock
    the code acquires.

    However since IX locks on tablespaces used for table creation we
    still have to acquire locks. IX locks are acquired on tablespaces
    to satisfy asserts in dd::create_table()).
  */
  Upgrade_MDL_guard mdl_guard(thd);
  if ((tablespace_name_set.size() != 0) &&
      mdl_guard.acquire_lock_tablespace(&tablespace_name_set)) {
    ndb_log_error("Unable to acquire lock on tablespace name %s",
                  share.tablespace);
    DBUG_RETURN(false);
  }

  /*
    Generated columns are fixed here as open_table_from_share()
    asserts that Field objects in TABLE_SHARE doesn't have
    expressions assigned.
  */
  Bootstrap_error_handler bootstrap_error_handler;
  bootstrap_error_handler.set_log_error(false);
  if (!fix_generated_columns_for_upgrade(thd, &table, alter_info.create_list)) {
    ndb_log_error("Error in processing generated columns");
    DBUG_RETURN(false);
  }
  bootstrap_error_handler.set_log_error(true);

  FOREIGN_KEY *fk_key_info_buffer = NULL;
  uint fk_number = 0;

  // Set sql_mode=0 for handling default values, it will be restored vai RAII.
  thd->variables.sql_mode = 0;
  // Disable autocommit option in thd variable
  Disable_autocommit_guard autocommit_guard(thd);

  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Schema *sch_obj = nullptr;
  String_type to_table_name(table_name);

  if (thd->dd_client()->acquire(schema_name, &sch_obj)) {
    // Error is reported by the dictionary subsystem.
    DBUG_RETURN(false);
  }

  if (!sch_obj) {
    my_error(ER_BAD_DB_ERROR, MYF(0), schema_name.c_str());
    DBUG_RETURN(false);
  }

  Disable_gtid_state_update_guard disabler(thd);

  std::unique_ptr<dd::Table> table_def = dd::create_dd_user_table(
      thd, *sch_obj, to_table_name, &create_info, alter_info.create_list,
      key_info_buffer, key_count, Alter_info::ENABLE, fk_key_info_buffer,
      fk_number, nullptr, table.file);

  if (!table_def || thd->dd_client()->store(table_def.get())) {
    ndb_log_error("Error in Creating DD entry for %s.%s", schema_name.c_str(),
                  table_name.c_str());
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    DBUG_RETURN(false);
  }

  if (trans_commit_stmt(thd) || trans_commit(thd)) {
    ndb_log_error("Error in Creating DD entry for %s.%s", schema_name.c_str(),
                  table_name.c_str());
    DBUG_RETURN(false);
  }

  if (!set_se_data_for_user_tables(thd, schema_name, to_table_name, &table)) {
    ndb_log_error("Error in fixing SE data for %s.%s", schema_name.c_str(),
                  table_name.c_str());
    DBUG_RETURN(false);
  }

  if (!fix_fk_parent_key_names(thd, schema_name, to_table_name,
                               share.db_type())) {
    DBUG_RETURN(false);
  }

  DBUG_RETURN(true);
}

}  // namespace ndb_upgrade
}  // namespace dd
