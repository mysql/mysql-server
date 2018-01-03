/*****************************************************************************

Copyright (c) 1996, 2017, Oracle and/or its affiliates. All Rights Reserved.

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
class THD;
struct TABLE;

/* This is used only during upgrade. We don't use ids
from DICT_HDR during upgrade because unlike bootstrap case,
the ids are moved after user table creation.  Since we
want to create dictionary tables with fixed ids, we use
in-memory counter for upgrade */
extern uint32_t	dd_upgrade_indexes_num;

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

/** Add server version number to tablespace while upgrading.
@param[in]	space_id		space id of tablespace
@return false on success, true on failure. */
bool upgrade_space_version(const uint32 space_id);

/** Add server version number to tablespace while upgrading.
@param[in]	tablespace		dd::Tablespace
@return false on success, true on failure. */
bool upgrade_space_version(dd::Tablespace* tablespace);

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
