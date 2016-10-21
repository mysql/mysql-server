/*****************************************************************************

Copyright (c) 2016, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file dict/dict0dd.cc
Data dictionary interface */

#include "dict0dd.h"
#include "dict0dict.h"
#include "dict0crea.h"
#include <dd/properties.h>
#include "dict0mem.h"
#include "rem0rec.h"
#include "data0type.h"
#include "mach0data.h"
#include "dict0dict.h"
#include "fts0priv.h"
#include "ut0crc32.h"


/** Returns a table object based on table id.
@param[in]	table_id	table id
@param[in]	dict_locked	TRUE=data dictionary locked
@param[in]	table_op	operation to perform
@return table, NULL if does not exist */
dict_table_t*
dd_table_open_on_id_in_mem(
	table_id_t	table_id,
	ibool		dict_locked,
	dict_table_op_t	table_op)
{
	dict_table_t*	table;

	if (!dict_locked) {
		mutex_enter(&dict_sys->mutex);
	}

	ut_ad(mutex_own(&dict_sys->mutex));

	/* Look for the table name in the hash table */
	ulint	fold = ut_fold_ull(table_id);

	HASH_SEARCH(id_hash, dict_sys->table_id_hash, fold,
		    dict_table_t*, table, ut_ad(table->cached),
		    table->id == table_id);

	ut_ad(!table || table->cached);

	if (table != NULL) {

		if (table->can_be_evicted) {
			dict_move_to_mru(table);
		}

		table->acquire();

		MONITOR_INC(MONITOR_TABLE_REFERENCE);
	} else if (dict_table_is_sdi(table_id)) {

		/* The table is SDI table */
		space_id_t      space_id = dict_sdi_get_space_id(table_id);
		uint32_t        copy_num = dict_sdi_get_copy_num(table_id);

		/* Create in-memory table oject for SDI table */
		dict_index_t*   sdi_index = dict_sdi_create_idx_in_mem(
			space_id, copy_num, false, 0);

		if (sdi_index == NULL) {
			if (!dict_locked) {
				mutex_exit(&dict_sys->mutex);
			}
			return(NULL);
		}

		table = sdi_index->table;

		ut_ad(table != NULL);

		table->acquire();
	}

	if (!dict_locked) {
		mutex_exit(&dict_sys->mutex);
	}

        return(table);
}
