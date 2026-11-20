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

#pragma once

#include "sql/handler.h"
#include "sql/sql_alter.h"
namespace myduck {

static constexpr char DUCKDB_ENGINE_NAME[] = "DUCKDB";
static constexpr char DUCKDB_AUTO_INC[] = "duckdb_auto_inc_val";

/** Prepares the creation of a DuckDB table by modifying the create_info and
  alter_info objects to align with DuckDB's constraints and limitations.
  Called by mysql_prepare_create_table.
  @param thd pointer to THD
  @param create_info pointer to HA_CREATE_INFO containing table creation
  parameters
  @param alter_info pointer to Alter_info containing additional table alteration
  information

  @return Return false if success.
*/
bool prepare_create_duckdb_table(THD *thd, HA_CREATE_INFO *create_info,
                                 Alter_info *alter_info);

/** Flags which are not supported by DuckDB. */
const ulonglong UNSUPPORT_ALTER_FLAGS =
    /* PARTITION OPERATIONS. */
    Alter_info::ALTER_EXCHANGE_PARTITION |
    /* COLUMN OPERATIONS. */
    Alter_info::ALTER_COLUMN_ORDER |
    /* HTON UNSUPPORTED OPERATIONS. */
    Alter_info::ALTER_DISCARD_TABLESPACE | Alter_info::ALTER_IMPORT_TABLESPACE |
    Alter_info::ALTER_SECONDARY_LOAD | Alter_info::ALTER_SECONDARY_UNLOAD |
    Alter_info::ANY_ENGINE_ATTRIBUTE;

/** Flags which are ignored by DuckDB. */
const ulonglong IGNORED_ALTER_FLAGS =
    /* INDEX OPERATIONS. */
    Alter_info::ALTER_RENAME_INDEX | Alter_info::ALTER_INDEX_VISIBILITY |
    /* FOREIGN KEY OPERATIONS. */
    Alter_info::ADD_FOREIGN_KEY | Alter_info::DROP_FOREIGN_KEY |
    /* CHECK CONSTRAINT OPERATIONS. */
    Alter_info::ADD_CHECK_CONSTRAINT | Alter_info::DROP_CHECK_CONSTRAINT |
    Alter_info::ENFORCE_CHECK_CONSTRAINT |
    Alter_info::SUSPEND_CHECK_CONSTRAINT |
    /* ANY CONSTRAINT OPERATIONS. */
    Alter_info::ENFORCE_ANY_CONSTRAINT | Alter_info::SUSPEND_ANY_CONSTRAINT |
    /* ALTER ORDER */
    Alter_info::ALTER_ORDER;

/** Flags which may be ignored by DuckDB. */
const ulonglong MAY_IGNORED_ALTER_FLAGS =
    Alter_info::ALTER_ADD_INDEX | Alter_info::ALTER_DROP_INDEX |
    Alter_info::DROP_ANY_CONSTRAINT | Alter_info::ALTER_COLUMN_VISIBILITY;

/** Prepare alter_info for ALTER DuckDB table.
  For alter_info->flags, the information contained in IGNORED_ALTER_FLAGS will
  be ignored.
  @param[in]      thd          thread handle.
  @param[in]      create_info  A structure describing the table to be created
  @param[in, out] alter_info   Alter_info describing which columns, defaults or
                               indexes are dropped or modified. */
void prepare_alter_duckdb_table(THD *thd, HA_CREATE_INFO *create_info,
                                Alter_info *alter_info);

/** Checks whether the given table is a DuckDB table.
  @param table pointer to TABLE object
  @return true if the table is a DuckDB table, false otherwise
*/
bool is_duckdb_table(const TABLE *table);

/** Checks whether the given ALTER TABLE operation is supported by DuckDB.
  @param alter_info  pointer to Alter_info object containing the ALTER TABLE
  operation
  @param table       TABLE object
  @return true if the operation is supported, false otherwise
*/
bool is_supported_ddl(Alter_info *alter_info, TABLE *table);

/** Report error message of DuckDB table struct to user.
  @param[in]  err_msg   error message
  @return true always */
bool report_duckdb_table_struct_error(const std::string &err_msg);

/** Precheck if the table can be converted to DuckDB table.
  @param[in]  table  table to be converted
  @return true if the table can be converted to DuckDB table, false otherwise
*/
bool precheck_convert_to_duckdb(const dd::Table *table);

/** Fill primary key fields for the given table.
  @param[in]       thd    Thread context
  @param[in, out]  table  TABLE object

  @return false if success, otherwise true.
*/
bool fill_pk_fields(THD *thd, TABLE *table);

/** Cleanup temporary table in DuckDB.
  @param [in]  thd  thread handle
  @param [in]  db   database name
  @param [in]  tmp_table_name  temporary table name
*/
void cleanup_tmp_table(THD *thd, const char *db, const char *tmp_table_name);

/** Generate delete from partition query.
  @param[in]  part_info  partition info
  @param[in]  truncate   true if truncate, false if drop
  @param[out] query      delete from query

  @return false if success, otherwise true.
*/
bool generate_delete_from_partition(partition_info *part_info, bool truncate,
                                    std::string &query);
}  // namespace myduck
