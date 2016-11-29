/*****************************************************************************

Copyright (c) 2015, 2016, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file include/dict0dd.h
Data dictionary interface */

#ifndef dict0dd_h
#define dict0dd_h

#include "univ.i"
#include "dict0dict.h"
#include "dict0types.h"
#include "dict0mem.h"
#include <dd/properties.h>
#include "sess0sess.h"

#include "dd/dd.h"
#include "dd/dictionary.h"
#include "dd/cache/dictionary_client.h"
#include "dd/properties.h"
#include "dd/sdi_tablespace.h"    // dd::sdi_tablespace::store
#include "dd/dd_table.h"
#include "dd/dd_schema.h"
#include "dd/types/table.h"
#include "dd/types/index.h"
#include "dd/types/column.h"
#include "dd/types/index_element.h"
#include "dd/types/partition.h"
#include "dd/types/partition_index.h"
#include "dd/types/object_type.h"
#include "dd/types/tablespace.h"
#include "dd/types/tablespace_file.h"
#include "dd_table_share.h"
#include "dd/types/foreign_key.h"
#include "dd/types/foreign_key_element.h"

using namespace dd::cache;
class THD;
class MDL_ticket;

/** Handler name for InnoDB */
static constexpr char handler_name[] = "InnoDB";

/** InnoDB private keys for dd::Tablespace */
enum dd_space_keys {
	/** Tablespace flags */
	DD_SPACE_FLAGS,
	/** Tablespace identifier */
	DD_SPACE_ID,
	/** Sentinel */
	DD_SPACE__LAST
};

/** InnoDB private key strings for dd::Tablespace.
@see dd_space_keys */
extern const char* const	dd_space_key_strings[DD_SPACE__LAST];

/** InnoDB private keys for dd::Table */
enum dd_table_keys {
	/** Auto-increment counter */
	DD_TABLE_AUTOINC,
	/** DATA DIRECTORY (static metadata) */
	DD_TABLE_DATA_DIRECTORY,
	/** Dynamic metadata version */
	DD_TABLE_VERSION,
	/** Sentinel */
	DD_TABLE__LAST
};

/** InnoDB private key strings for dd::Table. @see dd_table_keys */
extern const char* const	dd_table_key_strings[DD_TABLE__LAST];

/** InnoDB private keys for dd::Index or dd::Partition_index */
enum dd_index_keys {
	/** Index identifier */
	DD_INDEX_ID,
	/** Root page number */
	DD_INDEX_ROOT,
	/** Creating transaction ID */
	DD_INDEX_TRX_ID,
	/** Sentinel */
	DD_INDEX__LAST
};

static const char innobase_hton_name[]= "InnoDB";

/** InnoDB private key strings for dd::Index or dd::Partition_index.
@see dd_index_keys */
extern const char* const	dd_index_key_strings[DD_INDEX__LAST];

/** dd::Partition::options() key for INDEX DIRECTORY */
static const dd::String_type	index_file_name_key("index_file_name");
/** dd::Partition::options() key for DATA DIRECTORY */
static const dd::String_type	data_file_name_key("data_file_name");

/** Set the AUTO_INCREMENT attribute.
@param[in,out]	se_private_data	dd::Table::se_private_data
@param[in]	autoinc		the auto-increment value */
void
dd_set_autoinc(
	dd::Properties&	se_private_data,
	uint64		autoinc);

/** Acquire a metadata lock.
@param[in,out]	thd	current thread
@param[out]	mdl	metadata lock
@param[in]	name	table name
@param[in]	trylock	whether to skip the normal MDL timeout
@retval false if acquired, or trylock timed out
@retval true if failed (my_error() will have been called) */
bool
dd_mdl_acquire(
	THD*			thd,
	MDL_ticket**		mdl,
	const table_name_t&	name,
	bool			trylock);

/** Release a metadata lock.
@param[in,out]	thd	current thread
@param[in,out]	mdl	metadata lock */
void
dd_mdl_release(
	THD*		thd,
	MDL_ticket**	mdl);

/** Set the AUTO_INCREMENT attribute.
@param[in,out]	se_private_data	dd::Table::se_private_data
@param[in]	autoinc		the auto-increment value */
void dd_set_autoinc(dd::Properties& se_private_data, uint64 autoinc);

/** Open a persistent InnoDB table based on table id.
@param[in]	table_id	table identifier
@param[in,out]	thd		current MySQL connection (for mdl)
@param[in,out]	mdl		metadata lock (*mdl set if table_id was found); mdl=NULL if we are resurrecting table IX locks in recovery
@return table
@retval NULL if the table does not exist or cannot be opened */
dict_table_t*
dd_table_open_on_id(
	table_id_t	table_id,
	THD*		thd,
	MDL_ticket**	mdl,
	bool		dict_locked);

/** Close an internal InnoDB table handle.
@param[in,out]	table	InnoDB table handle
@param[in,out]	thd	current MySQL connection (for mdl)
@param[in,out]	mdl	metadata lock (will be set NULL)
@param[in]	dict_locked	whether we hold dict_sys mutex */
void
dd_table_close(
	dict_table_t*	table,
	THD*		thd,
	MDL_ticket**	mdl,
	bool		dict_locked);

/** Set the discard flag for a dd table.
@param[in,out]	thd	current thread
@param[in]	name	InnoDB table name
@param[in]	discard	discard flag
@retval false if fail. */
bool
dd_table_set_discard_flag(
	THD*			thd,
	const char*		name,
	bool			discard);

/** Open an internal handle to a persistent InnoDB table by name.
@param[in,out]	thd		current thread
@param[out]	mdl		metadata lock
@param[in]	name		InnoDB table name
@param[in]	dict_locked	has dict_sys mutex locked
@param[in]	ignore_err	whether to ignore err
@return handle to non-partitioned table
@retval NULL if the table does not exist */
dict_table_t*
dd_table_open_on_name(
	THD*			thd,
	MDL_ticket**		mdl,
	const char*		name,
	bool			dict_locked,
	ulint			ignore_err);

/** Returns a table object based on table id.
@param[in]	table_id	table id
@param[in]	dict_locked	TRUE=data dictionary locked
@param[in]	table_op	operation to perform
@return table, NULL if does not exist */
dict_table_t*
dd_table_open_on_id_in_mem(
	table_id_t	table_id,
	ibool		dict_locked,
	dict_table_op_t	table_op);

/** Open or load a table definition based on a Global DD object.
@param[in,out]	client		data dictionary client
@param[in]	table		MySQL table definition
@param[in]	norm_name	Table Name
@param[in,out]	uncached	NULL if the table should be added to the cache;
				if not, *uncached=true will be assigned
				when ib_table was allocated but not cached
				(used during delete_table and rename_table)
@param[out]	ib_table	InnoDB table handle
@param[in]	dd_table	Global DD table or partition object
@param[in]	skip_mdl	whether meta-data locking is skipped
@param[in]	thd		thread THD
@retval	0			on success
@retval	HA_ERR_TABLE_CORRUPT	if the table is marked as corrupted
@retval	HA_ERR_TABLESPACE_MISSING	if the file is not found */

dict_table_t*
dd_open_table(
	dd::cache::Dictionary_client*	client,
	const TABLE*			table,
	const char*			norm_name,
	bool*				uncached,
	dict_table_t*&			ib_table,
	const dd::Table*		dd_table,
	bool				skip_mdl,
	THD*				thd);

/** Get dd tablespace by dd space id
Note: It'll get an uncached dd space obj
@param[in]	dd client	dd client
@param[in]	dd_space_id	dd tablespace id
@param[in,out]	dd_space	dd tablespace
@retval false if fail. */
bool
dd_tablespace_get_on_id(
	Dictionary_client*	client,
	dd::Object_id		dd_space_id,
	dd::Tablespace**	dd_space);

/** Update dd tablespace for rename
@param[in]	dd_space_id	dd tablespace id
@param[in]	new_path	new data file path
@retval false if fail. */
bool
dd_tablespace_update_for_rename(
	dd::Object_id		dd_space_id,
	const char*		new_path);

/** Obtain the private handler of InnoDB session specific data.
@param[in,out]  thd     MySQL thread handler.
@return reference to private handler */
MY_ATTRIBUTE((warn_unused_result))
innodb_session_t*&
thd_to_innodb_session(
	THD*    thd);

/** Parse a table name
@param[in]	tbl_name	table name including database and table name
@param[in,out]	dd_db_name	database name buffer to be filled
@param[in,out]	dd_tbl_name	table name buffer to be filled */
UNIV_INLINE
void
innobase_parse_tbl_name(
	const char*	tbl_name,
	char*		dd_db_name,
	char*		dd_tbl_name);

/** Look up a column in a table using the system_charset_info collation.
@param[in]      dd_table        data dictionary table
@param[in]      name            column name
@return the column
@retval nullptr if not found */
UNIV_INLINE
const dd::Column*
dd_find_column(
	dd::Table* dd_table,
	const char* name);

/** Add a hidden column when creating a table.
@param[in,out]  dd_table        table containing user columns and indexes
@param[in]      name            hidden column name
@param[in]      length          length of the column, in bytes
@return the added column, or NULL if there already was a column by that name */
UNIV_INLINE
dd::Column*
dd_add_hidden_column(
        dd::Table*      dd_table,
        const char*     name,
        uint            length);

/** Add a hidden index element at the end.
@param[in,out]  index   created index metadata
@param[in]      column  column of the index */
UNIV_INLINE
void
dd_add_hidden_element(dd::Index* index, const dd::Column* column);

/** Initialize a hidden unique B-tree index.
@param[in,out]  index   created index metadata
@param[in]      name    name of the index
@param[in]      column  column of the index
@return the initialized index */
UNIV_INLINE
dd::Index*
dd_set_hidden_unique_index(
        dd::Index*              index,
        const char*             name,
        const dd::Column*       column);

#include "dict0dd.ic"
#endif
