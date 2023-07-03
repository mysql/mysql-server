/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#include "storage/ndb/plugin/ndb_dd_upgrade_table.h"

#include <algorithm>
#include <string>
#include <unordered_set>

#include "my_dbug.h"               // DBUG_TRACE
#include "mysql/psi/mysql_file.h"  // mysql_file_create
#include "sql/create_field.h"      // Create_field
#include "sql/dd/dd_table.h"       // create_dd_user_table
#include "sql/dd/types/table.h"    // dd::Table
#include "sql/mysqld.h"            // key_file_frm
#include "sql/sql_lex.h"           // lex_start
#include "sql/sql_parse.h"         // free_items
#include "sql/sql_table.h"         // build_tablename
#include "sql/thd_raii.h"          // Implicit_substatement_state_guard
#include "storage/ndb/plugin/ndb_dd_client.h"  // Ndb_dd_client
#include "storage/ndb/plugin/ndb_log.h"        // ndb_log_error
#include "storage/ndb/plugin/ndb_table_guard.h"
#include "storage/ndb/plugin/ndb_thd.h"      // get_thd_ndb
#include "storage/ndb/plugin/ndb_thd_ndb.h"  // Thd_ndb

namespace dd {
class Schema;
class Table;
}  // namespace dd

namespace dd {
namespace ndb_upgrade {

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
    m_thd->work_part_info = nullptr;

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
  DBUG_TRACE;
  thd->work_part_info = nullptr;

  // If partition information is present in TABLE_SHARE
  if (share->partition_info_str_len && table->file) {
    // Setup temporary m_part_info in TABLE_SHARE, this allows
    // ha_ndbcluster::get_num_parts() to return the number of partitions same
    // way as usual while opening table.
    partition_info tmp_part_info;
    tmp_part_info.list_of_part_fields = true;
    {
      // Open the table from NDB and save number of partitions
      Thd_ndb *thd_ndb = get_thd_ndb(thd);
      Ndb_table_guard ndbtab_g(thd_ndb->ndb, share->db.str,
                               share->table_name.str);
      if (!ndbtab_g.get_table()) {
        thd_ndb->push_ndb_error_warning(ndbtab_g.getNdbError());
        thd_ndb->push_warning("Failed to fetch num_parts for: '%s.%s'",
                              share->db.str, share->table_name.str);
        return false;
      }
      tmp_part_info.num_parts = ndbtab_g.get_table()->getPartitionCount();
      DBUG_PRINT("info", ("num_parts: %u", tmp_part_info.num_parts));
    }
    share->m_part_info = &tmp_part_info;

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

bool migrate_table_to_dd(THD *thd, Ndb_dd_client *dd_client,
                         const String_type &schema_name,
                         const String_type &table_name,
                         const unsigned char *frm_data,
                         const unsigned int unpacked_len) {
  DBUG_TRACE;

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

  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  if (frm_file < 0) {
    thd_ndb->push_warning("Failed to create .frm file for table %s.%s",
                          schema_name.c_str(), table_name.c_str());
    ndb_log_error("Failed to create .frm file for table '%s.%s', error: %d",
                  schema_name.c_str(), table_name.c_str(), frm_file);
    return false;
  }

  if (mysql_file_write(frm_file, frm_data, unpacked_len,
                       MYF(MY_WME | MY_NABP))) {
    thd_ndb->push_warning("Failed to write .frm file for table %s.%s",
                          schema_name.c_str(), table_name.c_str());
    ndb_log_error("Failed to write .frm file for table '%s.%s'",
                  schema_name.c_str(), table_name.c_str());
    // Delete frm file
    mysql_file_delete(key_file_frm, index_file, MYF(0));
    return false;
  }

  (void)mysql_file_close(frm_file, MYF(0));

  // Create table share for tables
  int r = create_table_share_for_upgrade(thd, path, &share, &frm_context,
                                         schema_name.c_str(),
                                         table_name.c_str(), false);
  if (r != 0) {
    thd_ndb->push_warning(ER_CANT_CREATE_TABLE_SHARE_FROM_FRM,
                          "Error in creating TABLE_SHARE from %s.frm file",
                          table_name.c_str());
    if (r == -1)
      ndb_log_error("Error in creating TABLE_SHARE from %s.frm file",
                    table_name.c_str());
    // Delete frm file
    mysql_file_delete(key_file_frm, index_file, MYF(0));
    return false;
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
    thd_ndb->push_warning(ER_CANT_CREATE_HANDLER_OBJECT_FOR_TABLE,
                          "Error in creating handler object for table %s.%s",
                          schema_name.c_str(), table_name.c_str());
    ndb_log_error("Error in creating handler object for table %s.%s",
                  schema_name.c_str(), table_name.c_str());
    return false;
  }
  table.file = file;
  table_guard.update_handler(file);

  if (table.file->set_ha_share_ref(&share.ha_share)) {
    thd_ndb->push_warning(ER_CANT_SET_HANDLER_REFERENCE_FOR_TABLE,
                          "Error in setting handler reference for table %s.%s",
                          schema_name.c_str(), table_name.c_str());
    ndb_log_error("Error in setting handler reference for table %s.%s",
                  schema_name.c_str(), table_name.c_str());
    return false;
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
    if (error == HA_ADMIN_NEEDS_DUMP_UPGRADE) {
      thd_ndb->push_warning(ER_TABLE_NEEDS_DUMP_UPGRADE,
                            "Table upgrade required for %s.%s. Please "
                            "dump/reload table to fix it",
                            schema_name.c_str(), table_name.c_str());
      ndb_log_error(
          "Table upgrade required for "
          "`%-.64s`.`%-.64s`. Please dump/reload table to "
          "fix it!",
          schema_name.c_str(), table_name.c_str());
    } else {
      ndb_log_error(
          "Table upgrade required. Please do \"REPAIR TABLE `%s`\" "
          "or dump/reload to fix it",
          table_name.c_str());
      thd_ndb->push_warning(
          ER_TABLE_UPGRADE_REQUIRED,
          "Table definition contains obsolete data types such "
          "as old temporal or decimal types");
    }
    return false;
  }

  /*
    Mark all the keys visible and supported algorithm explicit.
    Unsupported algorithms will get fixed by prepare_key() call.
  */
  for (uint i = 0; i < share.keys; i++) {
    KEY *key_info = &share.key_info[i];
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
    return false;
  }

  // Fix keys and indexes.
  KEY *key_info_buffer;
  uint key_count;

  // Foreign keys are handled at later stage by retrieving info from SE.
  FOREIGN_KEY *dummy_fk_key_info = nullptr;
  uint dummy_fk_key_count = 0;

  if (mysql_prepare_create_table(
          thd, schema_name.c_str(), table_name.c_str(), &create_info,
          &alter_info, file, true,  // NDB tables are auto-partitoned.
          &key_info_buffer, &key_count, &dummy_fk_key_info, &dummy_fk_key_count,
          nullptr, 0, nullptr, 0, 0, false /* No FKs here. */)) {
    return false;
  }

  int select_field_pos = alter_info.create_list.elements;
  create_info.null_bits = 0;
  Create_field *sql_field;
  List_iterator<Create_field> it_create(alter_info.create_list);

  for (int field_no = 0; (sql_field = it_create++); field_no++) {
    if (prepare_create_field(thd, schema_name.c_str(), table_name.c_str(),
                             &create_info, &alter_info.create_list,
                             &select_field_pos, table.file, sql_field,
                             field_no))
      return false;
  }

  // open_table_from_share and partition expression parsing needs a
  // valid Query_block to parse generated columns
  LEX *lex_saved = thd->lex;
  thd->lex = &lex;
  lex_start(thd);
  table_guard.update_lex(lex_saved);

  if (!fill_partition_info_for_upgrade(thd, &share, &frm_context, &table))
    return false;

  // Store names of all tablespaces used by partitions
  std::unordered_set<std::string> tablespace_names;
  if (thd->work_part_info != nullptr) {
    List_iterator<partition_element> partition_iter(
        thd->work_part_info->partitions);
    partition_element *partition_elem;

    while ((partition_elem = partition_iter++)) {
      if (partition_elem->tablespace_name != nullptr) {
        // Add name of all partitions to take MDL
        tablespace_names.insert(partition_elem->tablespace_name);
      }
      if (thd->work_part_info->is_sub_partitioned()) {
        // Add name of all sub partitions to take MDL
        List_iterator<partition_element> sub_it(partition_elem->subpartitions);
        partition_element *sub_elem;
        while ((sub_elem = sub_it++)) {
          if (sub_elem->tablespace_name != nullptr) {
            tablespace_names.insert(sub_elem->tablespace_name);
          }
        }
      }
    }
  }

  // Add name of the tablespace used by the table
  if (share.tablespace != nullptr) tablespace_names.insert(share.tablespace);

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
  for (const std::string &tablespace_name : tablespace_names) {
    if (!dd_client->mdl_lock_tablespace(tablespace_name.c_str(), true)) {
      thd_ndb->push_warning(ER_CANT_LOCK_TABLESPACE,
                            "Unable to acquire lock on tablespace %s",
                            tablespace_name.c_str());
      ndb_log_error("Unable to acquire lock on tablespace %s",
                    tablespace_name.c_str());
      return false;
    }
  }

  /*
    Generated columns are fixed here as open_table_from_share()
    asserts that Field objects in TABLE_SHARE doesn't have
    expressions assigned.
  */
  if (!fix_generated_columns_for_upgrade(thd, &table, alter_info.create_list)) {
    thd_ndb->push_warning(
        ER_CANT_UPGRADE_GENERATED_COLUMNS_TO_DD,
        "Error in processing generated columns for table %s.%s",
        schema_name.c_str(), table_name.c_str());
    ndb_log_error("Error in processing generated columns for table '%s.%s'",
                  schema_name.c_str(), table_name.c_str());
    return false;
  }

  // Set sql_mode=0 for handling default values, it will be restored via RAII.
  thd->variables.sql_mode = 0;

  const dd::Schema *schema_def = nullptr;
  if (!dd_client->get_schema(schema_name.c_str(), &schema_def) || !schema_def) {
    thd_ndb->push_warning(ER_BAD_DB_ERROR, "Unknown database '%s'",
                          schema_name.c_str());
    ndb_log_error("Unknown database '%s'", schema_name.c_str());
    return false;
  }

  Implicit_substatement_state_guard substatement_guard(thd);

  const String_type to_table_name(table_name);
  std::unique_ptr<dd::Table> table_def = dd::create_dd_user_table(
      thd, *schema_def, to_table_name, &create_info, alter_info.create_list,
      key_info_buffer, key_count, Alter_info::ENABLE, nullptr, 0, nullptr,
      table.file);
  if (!table_def) {
    thd_ndb->push_warning(ER_DD_ERROR_CREATING_ENTRY,
                          "Error in Creating DD entry for %s.%s",
                          schema_name.c_str(), table_name.c_str());
    ndb_log_error("Error in Creating DD entry for %s.%s", schema_name.c_str(),
                  table_name.c_str());
    return false;
  }

  // Set storage engine specific metadata in the new DD table object
  if (table.file->ha_upgrade_table(thd, schema_name.c_str(), table_name.c_str(),
                                   table_def.get(), &table)) {
    thd_ndb->push_warning(ER_DD_CANT_FIX_SE_DATA,
                          "Failed to set SE specific data for table %s.%s",
                          schema_name.c_str(), table_name.c_str());
    ndb_log_error("Failed to set SE specific data for table %s.%s",
                  schema_name.c_str(), table_name.c_str());
    return false;
  }

  // As a final step, store the newly created DD table object
  if (!dd_client->store_table(table_def.get())) {
    thd_ndb->push_warning(ER_DD_ERROR_CREATING_ENTRY,
                          "Error in Creating DD entry for %s.%s",
                          schema_name.c_str(), table_name.c_str());
    ndb_log_error("Error in Creating DD entry for %s.%s", schema_name.c_str(),
                  table_name.c_str());
    return false;
  }
  return true;
}

}  // namespace ndb_upgrade
}  // namespace dd
