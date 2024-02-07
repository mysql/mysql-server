/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include "sql_gipk.h"

#include "sql/create_field.h"             // Create_field
#include "sql/dd/properties.h"            // dd::Properties
#include "sql/dd/types/abstract_table.h"  // dd::Abstract_table
#include "sql/dd/types/index.h"           // dd::Index
#include "sql/sql_alter.h"                // Alter_info
#include "sql/sql_class.h"                // THD
#include "sql/sql_lex.h"                  // LEX
#include "sql/sql_table.h"                // primary_key_name

/* Generated invisible primary key column name. */
const char *gipk_column_name = "my_row_id";

bool is_generated_invisible_primary_key_column_name(const char *column_name) {
  return (my_strcasecmp(system_charset_info, column_name, gipk_column_name) ==
          0);
}

/**
  Check if invisible primary key generation is supported for the table's storage
  engine.

  @param  se_handlerton  Handlerton instance of storage engine.

  @retval true   If generating primary key is supported.
  @retval false  Otherwise.
*/
static bool is_generating_invisible_pk_supported_for_se(
    handlerton *se_handlerton) {
  // Invisible PK generation is supported for only InnoDB tables for now.
  return (ha_check_storage_engine_flag(se_handlerton,
                                       HTON_SUPPORTS_GENERATED_INVISIBLE_PK));
}

bool is_generate_invisible_primary_key_mode_active(THD *thd) {
  return (thd->variables.sql_generate_invisible_primary_key &&
          !thd->is_dd_system_thread() && !thd->is_initialize_system_thread());
}

bool is_candidate_table_for_invisible_primary_key_generation(
    const HA_CREATE_INFO *create_info, Alter_info *alter_info) {
  // Check PK generation is supported for the table's storage engine.
  if (!is_generating_invisible_pk_supported_for_se(create_info->db_type))
    return false;

  // Check if primary key is specified for the table.
  const Mem_root_array<Key_spec *> &kl = alter_info->key_list;
  if (std::any_of(kl.begin(), kl.end(), [](const Key_spec *ks) {
        return (ks->type == KEYTYPE_PRIMARY);
      }))
    return false;

  return true;
}

/**
  Validate invisible primary key generation for a candidate table (table
  being created).

  @param   thd          Thread handle.
  @param   alter_info   Alter_info instance describing table being created.

  @retval  false        On success.
  @retval  true         On failure.
*/
static bool validate_invisible_primary_key_generation(THD *thd,
                                                      Alter_info *alter_info) {
  // CREATE TABLE ... SELECT
  if (!thd->lex->query_block->field_list_is_empty()) {
    /*
      Mark statement as unsafe so that decide_logging_format() knows that it
      needs to use row format when binlog_format=MIXED
    */
    thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_CREATE_SELECT_WITH_GIPK);
    /*
      Report an error when binlog_format=STATEMENT.

      Generating invisible primary key for the CREATE TABLE SELECT in SBR mode
      is unsafe. This operation can *not* be replicated safely. Order in which
      auto-increment values generated for the my_row_id column is
      non-deterministic, so replicating this operation is not safe using SBR.
    */
    if (thd->variables.binlog_format == BINLOG_FORMAT_STMT) {
      my_error(ER_CREATE_SELECT_WITH_GIPK_DISALLOWED_IN_SBR, MYF(0));
      return true;
    }
  }

  /*
    Generating invisible PK is *not* supported for the partitioned tables for
    now..
  */
  if (thd->lex->part_info != nullptr) {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0),
             "generating invisible primary key for the partitioned tables");
    return true;
  }

  /*
    Primary key is generated on invisible auto_increment column "my_row_id".
    Check if table already has column with same name or if table already has
    auto_increment column.
  */
  for (Create_field cr_field : alter_info->create_list) {
    if (is_generated_invisible_primary_key_column_name(cr_field.field_name)) {
      my_error(ER_GIPK_COLUMN_EXISTS, MYF(0));
      return true;
    }

    if (cr_field.auto_flags & Field::NEXT_NUMBER) {
      my_error(ER_GIPK_FAILED_AUTOINC_COLUMN_EXISTS, MYF(0));
      return true;
    }
  }

  return false;
}

/**
  Generates invisible primary key for a table.

  @param[in]       thd                 Thread handle.
  @param[in,out]   alter_info          Alter_info instance describing table
                                       being created or altered.

  @retval          false             On success.
  @retval          true              On failure.
*/
static bool generate_invisible_primary_key(THD *thd, Alter_info *alter_info) {
  /*
    Create primary key column "my_row_id bigint unsigned NOT NULL AUTO_INCREMENT
    INVISIBLE" and add it as the first column in the column list.
  */
  Create_field *cr_field = new (thd->mem_root) Create_field();
  if (cr_field == nullptr) return true;  // OOM

  if (cr_field->init(
          thd, gipk_column_name, MYSQL_TYPE_LONGLONG, nullptr, nullptr,
          (UNSIGNED_FLAG | NOT_NULL_FLAG | AUTO_INCREMENT_FLAG), nullptr,
          nullptr, &EMPTY_CSTR, nullptr, nullptr, nullptr, false, 0, nullptr,
          nullptr, {}, dd::Column::enum_hidden_type::HT_HIDDEN_USER, false))
    return true;
  if (alter_info->create_list.push_front(cr_field)) return true;

  // Create primary key and add it the key list.
  List<Key_part_spec> key_parts;
  Key_part_spec *key_part_spec = new (thd->mem_root)
      Key_part_spec({gipk_column_name, strlen(gipk_column_name)}, 0, ORDER_ASC);
  if (key_part_spec == nullptr || key_parts.push_back(key_part_spec))
    return true;  // OOM

  Key_spec *key = new (thd->mem_root)
      Key_spec(thd->mem_root, KEYTYPE_PRIMARY, NULL_CSTR,
               &default_key_create_info, false, true, key_parts);
  if (key == nullptr || alter_info->key_list.push_back(key))
    return true;  // OOM

  return false;
}

bool validate_and_generate_invisible_primary_key(THD *thd,
                                                 Alter_info *alter_info) {
  return (validate_invisible_primary_key_generation(thd, alter_info) ||
          generate_invisible_primary_key(thd, alter_info));
}

bool adjust_generated_invisible_primary_key_column_position(
    THD *thd, handlerton *se_handlerton, TABLE *old_table,
    List<Create_field> *prepared_create_list) {
  if (!table_has_generated_invisible_primary_key(old_table)) return false;

  /*
    Generated invisible primary key is not supported for the partitioned
    tables for now. Check if table with generated primary key is partitioned or
    table is moved to engine for which generating invisible primary key is not
    supported.
  */
  if ((thd->lex->part_info != nullptr) ||
      (!is_generating_invisible_pk_supported_for_se(se_handlerton)))
    return false;

  // Find position of the GIPK column.
  List_iterator<Create_field> fld_it(*prepared_create_list);
  Create_field *fld = nullptr;
  uint pos = 0;
  while ((fld = fld_it++)) {
    if (is_generated_invisible_primary_key_column_name(fld->field_name)) break;
    pos++;
  }

  /*
    Due to GIPK alter restrictions there can be 3 possibilities,
    1) The GIPK column/key stay unchanged
    2) The GIPK column/key is dropped by this ALTER TABLE
    3) The GIPK column/key is dropped and new column with same name
       as GIPK column is added to the table.
  */
  if (pos == 0 || fld == nullptr || fld->field == nullptr) return false;

  /*
    Generated invisible primary key column position is changed. Altering
    this column position is not allowed. Error is reported later while
    applying alter restrictions.
  */
  if (fld->after != nullptr) return false;

  // Adjust GIPK column position.
  /*
    Generated invisible primary key column is neither dropped nor altered
    but new columns are added before generated invisible primary key column.
    Generated invisible primary key column must be at the first position.
  */
  fld_it.remove();
  prepared_create_list->push_front(fld);

  return false;
}

/**
  Check if table being altered is suitable to apply primary key ALTER
  restriction checks.

  @param  se_handlerton  Handlerton instance of table's storage engine.
  @param  old_table      Old definition of table being altered.

  @retval true   if table is suitable to apply ALTER restriction checks.
  @retval false  Otherwise.
*/
static bool is_candidate_table_for_pk_alter_restrictions_check(
    handlerton *se_handlerton, TABLE *old_table) {
  /*
    ALTER restriction checks are applied to table if a) Table is not a
    partitioned table b) primary key generation is supported for the storage
    engine and c) primary key is defined for a table.
  */
  return (old_table->part_info == nullptr &&
          is_generating_invisible_pk_supported_for_se(se_handlerton) &&
          !old_table->s->is_missing_primary_key());
}

bool check_primary_key_alter_restrictions(THD *thd, handlerton *se_handlerton,
                                          Alter_info *alter_info,
                                          TABLE *old_table) {
  // Check if ALTER TABLE statement restrictions are applicable for the table.
  if (!is_candidate_table_for_pk_alter_restrictions_check(se_handlerton,
                                                          old_table))
    return false;

  /*
    Table must have a primary key when gipk mode is active. Check if the new
    definition of a table has primary key.
  */
  if (is_generate_invisible_primary_key_mode_active(thd)) {
    Mem_root_array<Key_spec *> &key_list = alter_info->key_list;
    if (std::count_if(key_list.begin(), key_list.end(), [](const Key_spec *ks) {
          return (ks->type == KEYTYPE_PRIMARY);
        }) == 0) {
      /*
        When GIPK mode is active, dropping existing primary key without adding
        a new primary key in not supported for now. But this restriction will be
        relaxed eventually by automagically generating a primary key.
      */
      my_error(ER_NOT_SUPPORTED_YET, MYF(0),
               "existing primary key drop without adding a new primary key. In "
               "@@sql_generate_invisible_primary_key=ON mode table should have "
               "a primary key. Please add a new primary key to be able to drop "
               "existing primary key.");
      return true;
    }
  }

  if (table_has_generated_invisible_primary_key(old_table)) {
    /*
      Generated invisible primary key is not supported for the partitioned
      tables for now. Check that table with generated primary key is
      partitioned.
    */
    assert(old_table->part_info == nullptr);
    if (thd->lex->part_info != nullptr) {
      my_error(ER_NOT_SUPPORTED_YET, MYF(0),
               "partitioning table with generated invisible primary key");
      return true;
    }

    // Check if generated invisible primary key and key column is dropped.
    bool is_gipk_dropped = false;
    if ((alter_info->flags & Alter_info::ALTER_DROP_COLUMN) &&
        std::any_of(
            alter_info->drop_list.begin(), alter_info->drop_list.end(),
            [](const Alter_drop *d) {
              return (d->type == Alter_drop::COLUMN &&
                      is_generated_invisible_primary_key_column_name(d->name));
            })) {
      /*
        MySQL automatically drops the key when all its columns (or single
        column) is dropped. We stick to this behavior for GIPK column for
        consistency sake, even though it might be not intuitive.
      */
      is_gipk_dropped = true;
    } else if ((alter_info->flags & Alter_info::ALTER_DROP_INDEX) &&
               std::any_of(
                   alter_info->drop_list.begin(), alter_info->drop_list.end(),
                   [](const Alter_drop *d) {
                     return (d->type == Alter_drop::KEY &&
                             (my_strcasecmp(system_charset_info, d->name,
                                            primary_key_name)) == 0);
                   })) {
      /*
        Generated invisible primary key column should be dropped to drop
        primary key.
      */
      my_error(ER_DROP_PK_COLUMN_TO_DROP_GIPK, MYF(0));
      return true;
    }

    /*
      Generated invisible primary key is dropped. Skip CHANGE/MODIFY/ALTER
      restrictions check.
    */
    if (is_gipk_dropped) return false;

    /*
      At this stage, for sure table has a generated invisible primary key and
      is not dropped in the same alter statement.
    */
    assert(table_has_generated_invisible_primary_key(old_table) &&
           !is_gipk_dropped);
    /*
      CHANGING or MODIFYING only visibility attribute of generated primary key
      column is allowed. Other operations are restricted.

      For sure table has a generated invisible primary key at this stage, so it
      is OK to just check first column's definition and only name of the
      column to identify generated invisible primary key column.

      Furthermore, by checking that GIPK column is the first column in table
      we ensure that it was not moved around using "ALTER TABLE ... MODIFY...
      AFTER ...".
    */
    Create_field *cr_field = alter_info->create_list.head();
    /*
      First column should be "my_row_id unsigned BIGINT NOT NULL
      AUTO_INCREMENT". Changing only visibility attribute is allowed.
    */
    if (!is_generated_invisible_primary_key_column_name(cr_field->field_name) ||
        (cr_field->change != nullptr &&
         !is_generated_invisible_primary_key_column_name(cr_field->change)) ||
        cr_field->sql_type != MYSQL_TYPE_LONGLONG ||
        !(cr_field->auto_flags & Field::NEXT_NUMBER) ||
        !(cr_field->flags & UNSIGNED_FLAG) || cr_field->is_nullable) {
      my_error(ER_GIPK_COLUMN_ALTER_NOT_ALLOWED, MYF(0));
      return true;
    }

    /*
      ALTERING visibility attribute of generated primary key column is allowed.
      Other operations are restricted.
    */
    for (const Alter_column *alter_column : alter_info->alter_list) {
      if (alter_column->change_type() == Alter_column::Type::SET_COLUMN_VISIBLE)
        continue;

      if (is_generated_invisible_primary_key_column_name(alter_column->name)) {
        my_error(ER_GIPK_COLUMN_ALTER_NOT_ALLOWED, MYF(0));
        return true;
      }
    }
  }

  return false;
}

bool table_def_has_generated_invisible_primary_key(
    THD *thd, handlerton *se_handlerton,
    const List<Create_field> &create_fields, uint keys, const KEY *keyinfo) {
  /*
    Generated invisible primary key is not supported for the partitioned
    tables for now. Check that table with generated primary key is partitioned
    or table is moved to storage engine for which generating invisible primary
    key is not supported.
  */
  if (thd->lex->part_info != nullptr ||
      !is_generating_invisible_pk_supported_for_se(se_handlerton))
    return false;

  /*
    Check that first KEY instance is of a primary key and key column is first
    column of the table.
  */
  if (keys == 0 || !(keyinfo->flags & HA_NOSAME) ||
      (my_strcasecmp(system_charset_info, keyinfo->name, primary_key_name) !=
       0) ||
      (keyinfo->user_defined_key_parts != 1) ||
      (keyinfo->key_part->fieldnr != 0))
    return false;

  /*
    Check that first column definition has generated invisible primary key
    column attributes i.e. "my_row_id bigint unsigned NOT NULL AUTO_INCREMENT
    INVISIBLE".
  */
  const Create_field *cr_field = create_fields.head();
  return (
      is_generated_invisible_primary_key_column_name(cr_field->field_name) &&
      cr_field->sql_type == MYSQL_TYPE_LONGLONG &&
      (cr_field->flags & UNSIGNED_FLAG) && !cr_field->is_nullable &&
      (cr_field->auto_flags & Field::NEXT_NUMBER) &&
      is_hidden_by_user(cr_field));
}

/**
  Check if column is a generated invisible primary key column.

  @param    field    FIELD instance.

  @retval   true     If column is a generated invisible primary key column.
  @retval   false    Otherwise.
*/
static bool is_generated_invisible_primary_key_column(Field *field) {
  assert(field != nullptr);
  /*
    First column of a table with a) Name: my_row_id b) Type: bigint unsigned
    and c) attributes: NOT NULL AUTO_INCREMENT INVISIBLE, is considered as a
    generated invisible primary key column.
  */
  return ((field->field_index() == 0) &&
          is_generated_invisible_primary_key_column_name(field->field_name) &&
          field->real_type() == MYSQL_TYPE_LONGLONG && field->is_unsigned() &&
          field->is_flag_set(NOT_NULL_FLAG) &&
          (field->auto_flags & Field::NEXT_NUMBER) &&
          field->is_hidden_by_user());
}

/**
  Check if KEY is of a generated invisible primary key.

  @param    key      KEY instance.

  @retval   true     If KEY is of a generated invisible primary key.
  @retval   false    Otherwise.
*/
static bool is_generated_invisible_primary_key(const KEY *key) {
  assert(key != nullptr);
  return (
      (key->flags & HA_NOSAME) &&
      (my_strcasecmp(system_charset_info, key->name, primary_key_name) == 0) &&
      (key->user_defined_key_parts == 1) &&
      is_generated_invisible_primary_key_column(key->key_part->field));
}

/**
  Find generated invisible primary key in KEYs list of a table.

  @param    table    TABLE instance of a table.

  @retval   KEY*     KEY instance of a generated primary key.
  @retval   nullptr  If table does not have a generated invisible primary key.
*/
static const KEY *find_generated_invisible_primary_key(const TABLE *table) {
  assert(table != nullptr && table->s != nullptr);
  // GIPK is not supported for the partitioned table for now.
  if (table->part_info != nullptr) return nullptr;

  if (table->s->keys != 0 &&
      is_generating_invisible_pk_supported_for_se(table->s->db_type()) &&
      is_generated_invisible_primary_key(table->key_info))
    return table->key_info;

  return nullptr;
}

bool table_has_generated_invisible_primary_key(const TABLE *table) {
  return (find_generated_invisible_primary_key(table) != nullptr);
}
