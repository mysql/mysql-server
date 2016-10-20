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

#include "dict0types.h"
#include <dd/properties.h>

class THD;
class MDL_ticket;

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
	MDL_ticket**	mdl);

/** Close an internal InnoDB table handle.
@param[in,out]	table	InnoDB table handle
@param[in,out]	thd	current MySQL connection (for mdl)
@param[in,out]	mdl	metadata lock (will be set NULL) */
void
dd_table_close(
	dict_table_t*	table,
	THD*		thd,
	MDL_ticket**	mdl);

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
@param[in,out]	thd	current thread
@param[out]	mdl	metadata lock
@param[in]	name	InnoDB table name
@return handle to non-partitioned table
@retval NULL if the table does not exist */
dict_table_t*
dd_table_open_on_name(
	THD*			thd,
	MDL_ticket**		mdl,
	const char*		name);

#endif
