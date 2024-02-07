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

#ifndef SQL_GIPK_INCLUDED
#define SQL_GIPK_INCLUDED

#include "my_inttypes.h"

class Alter_info;
class Create_field;
class KEY;
template <class T>
class List;
struct TABLE;
class THD;
struct HA_CREATE_INFO;
struct handlerton;

/**
  Check if generate invisible primary key mode is active.

  Note: For bootstreap and initialize system threads, generate invisible
        primary key mode is not applicable. If there are any system table
        without explicit primary key then primary key is not generated for
        them while bootstrapping or initializing.


  @param[in]   thd     Thread handle.

  @returns true    if @@sql_generate_invisible_primary_key is ON for thread
                   other than bootstrap and initialize system threads.
*/
bool is_generate_invisible_primary_key_mode_active(THD *thd);

/**
  Check if column_name matches generated invisible primary key column name.

  @param[in]   column_name   Name of a column.

  @retval   true   column_name matches generated invisible primary key
                   column name.
  @retval   false  Otherwise.
*/
bool is_generated_invisible_primary_key_column_name(const char *column_name);

/**
  Check if table being created is suitable for invisible primary key
  generation.

  Primary key is generated only if
    a) explicit primary key is not defined for a table
    b) primary key generation is supported for the storage engine.

  @param[in] create_info  HA_CREATE_INFO instance describing table being
                          created.
  @param[in] alter_info   Alter_info instance describing table being created.

  @retval true   if table is suitable for primary key generation.
  @retval false  Otherwise.
*/
bool is_candidate_table_for_invisible_primary_key_generation(
    const HA_CREATE_INFO *create_info, Alter_info *alter_info);

/**
  Validate and generate invisible primary key for a candidate table (table
  being created).

  Primary key is generated if,
    a) Table is a non-partitioned table. Generating invisible primary
       key is not supported for partitioned tables for now.
    b) Table does *not* have a column with auto_increment attribute.
    c) Table does *not* have a column with name "my_row_id".
    d) Table is *not* created using CREATE TABLE ... SELECT in
       binlog_format=STATEMENT mode.
  Otherwise an error is reported in the validation of phase.

  @param[in]       thd                 Thread handle.
  @param[in,out]   alter_info          Alter_info instance describing table
                                       being created or altered.

  @retval          false             On success.
  @retval          true              On failure.
*/
bool validate_and_generate_invisible_primary_key(THD *thd,
                                                 Alter_info *alter_info);

/**
  Adjust generated invisible primary key column position in prepared fields
  list for the ALTER TABLE statement. Make sure generated invisible column is
  positioned at the first place.

  @param[in]      thd                  Thread handle.
  @param[in]      se_handlerton        Handlerton instance of table's storage
                                       engine
  @param[in]      old_table            Old definition of a table being altered.
  @param[in,out]  prepared_create_list Create_field list prepared for ALTER
                                       in prepare_fields_and_keys().

  @retval         false             On success.
  @retval         true              On failure.
*/
bool adjust_generated_invisible_primary_key_column_position(
    THD *thd, handlerton *se_handlerton, TABLE *old_table,
    List<Create_field> *prepared_create_list);

/**
  Check ALTER restrictions on primary key and column.

  Following ALTER restrictions are applicable on primary key and column,

    *) When sql_generate_invisible_primary_key is enabled then, primary key
       is allowed to drop only if new table definition has a primary key.

    *) Generated invisible primary key is allowed to drop only if primary key
       column is also dropped. This restriction is applicable irrespective of
       sql_generate_invisible_primary_key's state.

    *) CHANGE/MODIFY OR ALTER operations on generated invisible primary key
       columns are *not* allowed except ALTER operation to change column
       visibility attribute. This restriction is applicable irrespective of
       sql_generate_invisible_primary_key's state.

  @param[in]   thd           Thread handle.
  @param[in]   se_handlerton Handlerton instance of table's storage engine
  @param[in]   alter_info    Alter_info instance describing new table definition
                             of a table being altered.
  @param[in]   old_table     Old definition of a table being altered.

  @retval  false        On success.
  @retval  true         On failure.
*/
bool check_primary_key_alter_restrictions(THD *thd, handlerton *se_handlerton,
                                          Alter_info *alter_info,
                                          TABLE *old_table);

/**
  Check that definition of a table being created or altered has a generated
  invisible primary key definition.

  @param[in]  thd            Thread handle.
  @param[in]  se_handlerton  Handlerton instance of table's storage engine
  @param[in]  create_fields  List of Create_field instance for the table
                             columns.
  @param[in]  keys           Number of KEY structs in the 'keyinfo'.
  @param[in]  keyinfo        An array of KEY structs for the indexes.

  @returns true if table definition has a generated invisible primary key
                otherwise returns false.
*/
bool table_def_has_generated_invisible_primary_key(
    THD *thd, handlerton *se_handlerton,
    const List<Create_field> &create_fields, uint keys, const KEY *keyinfo);

/**
  Check if table has a generated invisible primary key.

  @param[in]  table    TABLE instance of a table.

  @retval   true     If table has a generated invisible primary key.
  @retval   false    Otherwise.
*/
bool table_has_generated_invisible_primary_key(const TABLE *table);
#endif  // SQL_GIPK_INCLUDED
