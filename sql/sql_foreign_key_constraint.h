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
#pragma once

class THD;
struct TABLE;
struct Table_share_foreign_key_info;

enum class enum_fk_dml_type {
  FK_INSERT,
  FK_UPDATE,
  FK_DELETE,
  // REPLACE executes as DELETE (of the conflicting row) + INSERT (the new row)
  // For the DELETE, FK checks must use the before-image (record[1]) to build
  // the FK search key; FK_DELETE_REPLACE signals this path (not record[0],
  // the after-image).
  FK_DELETE_REPLACE
};

/**
 * @brief Check if TABLE instance for foreign key is already opened.
 *
 * @param thd          Thread handle.
 * @param db_name      DB name.
 * @param table_name   Table name.
 * @param fk_name      Foreign key name.
 * @param[out] is_unused_table  Is set true if table exists in open table
 *                              list but not in use. For instance tables locked
 *                              under lock table, exists in open table list but
 *                              not used. query_id for such tables is reset to
 *                              0 after statement execution.
 *
 * @return true        If table for foreign key is already opened.
 * @return false       If table is not opened.
 */
bool is_foreign_key_table_opened(THD *thd, const char *db_name,
                                 const char *table_name, const char *fk_name,
                                 bool *is_unused_table);

/**
 * @brief Check all foreign key constraints on parent tables for DML operation
 *        on a child table.
 *
 * @param thd           Thread handle.
 * @param table_c       TABLE instance of a child table.
 * @param dml_type      DML operation type.
 * @param ignore_fk     Skip FK check for this foreign key info
 *
 * @return true         On error.
 * @return false        On Success.
 */
bool check_all_parent_fk_ref(
    THD *thd, const TABLE *table_c, enum_fk_dml_type dml_type,
    const Table_share_foreign_key_info *ignore_fk = nullptr);

/**
 * @brief Check all foreign key constraints on child tables for DML operation
 *        on a parent table.
 *
 * @param thd           Thread handle.
 * @param table_p       TABLE instance of a parent table.
 * @param dml_type      DML operation type.
 *
 * @return true         On error.
 * @return false        On Success.
 */
bool check_all_child_fk_ref(THD *thd, const TABLE *table_p,
                            enum_fk_dml_type dml_type);
