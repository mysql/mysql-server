/*****************************************************************************

Copyright (c) 1996, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/
class THD;
struct TABLE;

/* This is used only during upgrade. We don't use ids
from DICT_HDR during upgrade because unlike bootstrap case,
the ids are moved after user table creation.  Since we
want to create dictionary tables with fixed ids, we use
in-memory counter for upgrade */
extern uint	dd_upgrade_indexes_num;
extern uint	dd_upgrade_tables_num;

namespace dd {
class Table;
}

/** Migrate table from InnoDB Dictionary (INNODB SYS_*) tables to new Data
Dictionary. Since FTS tables contain table_id in their physical file name
and during upgrade we reserve DICT_MAX_DD_TABLES for dictionary tables.
So we rename FTS tablespace files
@param[in]	thd		Server thread object
@param[in]	db_name		database name
@param[in]	table_name	table name
@param[in,out]	dd_table	new dictionary table object to be filled
@param[in]	srv_table	server table object
@return false on success, true on failure. */
bool dd_upgrade_table(THD* thd, const char* db_name, const char* table_name,
		      dd::Table* dd_table, TABLE* srv_table);

/** Migrate tablespace entries from InnoDB SYS_TABLESPACES to new data
dictionary. FTS Tablespaces are not registered as they are handled differently.
FTS tablespaces have table_id in their name and we increment table_id of each
table by DICT_MAX_DD_TABLES
@param[in,out]	thd		THD
@return MySQL error code*/
int dd_upgrade_tablespace(THD* thd);

/** Upgrade innodb undo logs after upgrade. Also increment the table_id
offset by DICT_MAX_DD_TABLES. This offset increment is because the
first 256 table_ids are reserved for dictionary
@param[in,out]	thd		THD
@return MySQL error code*/
int dd_upgrade_logs(THD* thd);

/** If upgrade is successful, this API is used to flush innodb
dirty pages to disk. In case of server crash, this function
sets storage engine for rollback any changes.
@param[in,out]	thd		THD
@param[in]	failed_upgrade	true when upgrade failed
@return MySQL error code*/
int dd_upgrade_finish(THD* thd, bool failed_upgrade);
