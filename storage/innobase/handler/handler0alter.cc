/*****************************************************************************

Copyright (c) 2005, 2012, Oracle and/or its affiliates. All Rights Reserved.

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

/**************************************************//**
@file handler/handler0alter.cc
Smart ALTER TABLE
*******************************************************/

#include <unireg.h>
#include <mysqld_error.h>
#include <log.h>
#include <debug_sync.h>
#include <mysql/innodb_priv.h>
#include <sql_alter.h>
#include <sql_class.h>

#include "dict0crea.h"
#include "dict0dict.h"
#include "dict0priv.h"
#include "dict0stats.h"
#include "dict0stats_bg.h"
#include "log0log.h"
#include "rem0types.h"
#include "row0log.h"
#include "row0merge.h"
#include "srv0space.h"
#include "trx0trx.h"
#include "trx0roll.h"
#include "ha_prototypes.h"
#include "handler0alter.h"
#include "srv0mon.h"
#include "fts0priv.h"
#include "pars0pars.h"

#include "ha_innodb.h"

/** Operations for creating an index in place */
static const Alter_inplace_info::HA_ALTER_FLAGS INNOBASE_ONLINE_CREATE
	= Alter_inplace_info::ADD_INDEX
	| Alter_inplace_info::ADD_UNIQUE_INDEX;

/** Operations for rebuilding a table in place */
static const Alter_inplace_info::HA_ALTER_FLAGS INNOBASE_INPLACE_REBUILD
	= Alter_inplace_info::ADD_PK_INDEX
	| Alter_inplace_info::DROP_PK_INDEX
	| Alter_inplace_info::CHANGE_CREATE_OPTION
	| Alter_inplace_info::ALTER_COLUMN_NULLABLE
	| Alter_inplace_info::ALTER_COLUMN_NOT_NULLABLE
	| Alter_inplace_info::ALTER_COLUMN_ORDER
	| Alter_inplace_info::DROP_COLUMN
	| Alter_inplace_info::ADD_COLUMN
	/*
	| Alter_inplace_info::ALTER_COLUMN_TYPE
	| Alter_inplace_info::ALTER_COLUMN_EQUAL_PACK_LENGTH
	*/
	;

/** Operations for creating indexes or rebuilding a table */
static const Alter_inplace_info::HA_ALTER_FLAGS INNOBASE_INPLACE_CREATE
	= INNOBASE_ONLINE_CREATE | INNOBASE_INPLACE_REBUILD;

/** Operations for altering a table that InnoDB does not care about */
static const Alter_inplace_info::HA_ALTER_FLAGS INNOBASE_INPLACE_IGNORE
	= Alter_inplace_info::ALTER_COLUMN_DEFAULT
	| Alter_inplace_info::ALTER_COLUMN_COLUMN_FORMAT
	| Alter_inplace_info::ALTER_COLUMN_STORAGE_TYPE
	| Alter_inplace_info::ALTER_RENAME;

/** Operations that InnoDB can perform online */
static const Alter_inplace_info::HA_ALTER_FLAGS INNOBASE_ONLINE_OPERATIONS
	= INNOBASE_INPLACE_IGNORE
	| INNOBASE_ONLINE_CREATE
	| Alter_inplace_info::DROP_INDEX
	| Alter_inplace_info::DROP_UNIQUE_INDEX
	| Alter_inplace_info::DROP_FOREIGN_KEY
	| Alter_inplace_info::ALTER_COLUMN_NAME
	| Alter_inplace_info::ADD_FOREIGN_KEY;

/* Report an InnoDB error to the client by invoking my_error(). */
static UNIV_COLD __attribute__((nonnull))
void
my_error_innodb(
/*============*/
	dberr_t		error,	/*!< in: InnoDB error code */
	const char*	table,	/*!< in: table name */
	ulint		flags)	/*!< in: table flags */
{
	switch (error) {
	case DB_MISSING_HISTORY:
		my_error(ER_TABLE_DEF_CHANGED, MYF(0));
		break;
	case DB_RECORD_NOT_FOUND:
		my_error(ER_KEY_NOT_FOUND, MYF(0), table);
		break;
	case DB_DEADLOCK:
		my_error(ER_LOCK_DEADLOCK, MYF(0));
		break;
	case DB_LOCK_WAIT_TIMEOUT:
		my_error(ER_LOCK_WAIT_TIMEOUT, MYF(0));
		break;
	case DB_INTERRUPTED:
		my_error(ER_QUERY_INTERRUPTED, MYF(0));
		break;
	case DB_OUT_OF_MEMORY:
		my_error(ER_OUT_OF_RESOURCES, MYF(0));
		break;
	case DB_OUT_OF_FILE_SPACE:
		my_error(ER_RECORD_FILE_FULL, MYF(0), table);
		break;
	case DB_TOO_BIG_INDEX_COL:
		my_error(ER_INDEX_COLUMN_TOO_LONG, MYF(0),
			 DICT_MAX_FIELD_LEN_BY_FORMAT_FLAG(flags));
		break;
	case DB_TOO_MANY_CONCURRENT_TRXS:
		my_error(ER_TOO_MANY_CONCURRENT_TRXS, MYF(0));
		break;
	case DB_LOCK_TABLE_FULL:
		my_error(ER_LOCK_TABLE_FULL, MYF(0));
		break;
	case DB_UNDO_RECORD_TOO_BIG:
		my_error(ER_UNDO_RECORD_TOO_BIG, MYF(0));
		break;
	case DB_CORRUPTION:
		my_error(ER_NOT_KEYFILE, MYF(0), table);
		break;
	case DB_TOO_BIG_RECORD:
		my_error(ER_TOO_BIG_ROWSIZE, MYF(0),
			 page_get_free_space_of_empty(
				 flags & DICT_TF_COMPACT) / 2);
		break;
	case DB_INVALID_NULL:
		/* TODO: report the row, as we do for DB_DUPLICATE_KEY */
		my_error(ER_INVALID_USE_OF_NULL, MYF(0));
		break;
#ifdef UNIV_DEBUG
	case DB_SUCCESS:
	case DB_DUPLICATE_KEY:
	case DB_TABLESPACE_EXISTS:
	case DB_ONLINE_LOG_TOO_BIG:
		/* These codes should not be passed here. */
		ut_error;
#endif /* UNIV_DEBUG */
	default:
		my_error(ER_GET_ERRNO, MYF(0), error);
		break;
	}
}

/** Determine if fulltext indexes exist in a given table.
@param table_share	MySQL table
@return			whether fulltext indexes exist on the table */
static
bool
innobase_fulltext_exist(
/*====================*/
	const TABLE_SHARE*	table_share)
{
	for (uint i = 0; i < table_share->keys; i++) {
		if (table_share->key_info[i].flags & HA_FULLTEXT) {
			return(true);
		}
	}

	return(false);
}

/*******************************************************************//**
Determine if ALTER TABLE needs to rebuild the table.
@param ha_alter_info		the DDL operation
@return whether it is necessary to rebuild the table */
static __attribute__((nonnull, warn_unused_result))
bool
innobase_need_rebuild(
/*==================*/
	const Alter_inplace_info*	ha_alter_info)
{
	if (ha_alter_info->handler_flags
	    == Alter_inplace_info::CHANGE_CREATE_OPTION
	    && !(ha_alter_info->create_info->used_fields
		 & (HA_CREATE_USED_ROW_FORMAT
		    | HA_CREATE_USED_KEY_BLOCK_SIZE))) {
		/* Any other CHANGE_CREATE_OPTION than changing
		ROW_FORMAT or KEY_BLOCK_SIZE is ignored. */
		return(false);
	}

	return(!!(ha_alter_info->handler_flags & INNOBASE_INPLACE_REBUILD));
}

/** Check if InnoDB supports a particular alter table in-place
@param altered_table	TABLE object for new version of table.
@param ha_alter_info	Structure describing changes to be done
by ALTER TABLE and holding data used during in-place alter.

@retval HA_ALTER_INPLACE_NOT_SUPPORTED	Not supported
@retval HA_ALTER_INPLACE_NO_LOCK	Supported
@retval HA_ALTER_INPLACE_SHARED_LOCK_AFTER_PREPARE Supported, but requires
lock during main phase and exclusive lock during prepare phase.
@retval HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE	Supported, prepare phase
requires exclusive lock (any transactions that have accessed the table
must commit or roll back first, and no transactions can access the table
while prepare_inplace_alter_table() is executing)
*/
UNIV_INTERN
enum_alter_inplace_result
ha_innobase::check_if_supported_inplace_alter(
/*==========================================*/
	TABLE*			altered_table,
	Alter_inplace_info*	ha_alter_info)
{
	DBUG_ENTER("check_if_supported_inplace_alter");

	if (srv_read_only_mode) {
		ha_alter_info->unsupported_reason =
			innobase_get_err_msg(ER_READ_ONLY_MODE);
		DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
	} else if (srv_sys_space.created_new_raw() || srv_force_recovery) {
		ha_alter_info->unsupported_reason =
			innobase_get_err_msg(ER_READ_ONLY_MODE);
		DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
	}

	if (altered_table->s->fields > REC_MAX_N_USER_FIELDS) {
		/* Deny the inplace ALTER TABLE. MySQL will try to
		re-create the table and ha_innobase::create() will
		return an error too. This is how we effectively
		deny adding too many columns to a table. */
		ha_alter_info->unsupported_reason =
			innobase_get_err_msg(ER_TOO_MANY_FIELDS);
		DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
	}

	update_thd();
	trx_search_latch_release_if_reserved(prebuilt->trx);

	if (ha_alter_info->handler_flags
	    & ~(INNOBASE_ONLINE_OPERATIONS | INNOBASE_INPLACE_REBUILD)) {
		if (ha_alter_info->handler_flags
			& (Alter_inplace_info::ALTER_COLUMN_EQUAL_PACK_LENGTH
			   | Alter_inplace_info::ALTER_COLUMN_TYPE))
			ha_alter_info->unsupported_reason = innobase_get_err_msg(
				ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_COLUMN_TYPE);
		DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
	}

	/* Only support online add foreign key constraint when
	check_foreigns is turned off */
	if ((ha_alter_info->handler_flags
	     & Alter_inplace_info::ADD_FOREIGN_KEY)
	    && prebuilt->trx->check_foreigns) {
		ha_alter_info->unsupported_reason = innobase_get_err_msg(
			ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_FK_CHECK);
		DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
	}

	if (!(ha_alter_info->handler_flags & ~INNOBASE_INPLACE_IGNORE)) {
		DBUG_RETURN(HA_ALTER_INPLACE_NO_LOCK);
	}

	/* InnoDB cannot IGNORE when creating unique indexes. IGNORE
	should silently delete some duplicate rows. Our inplace_alter
	code will not delete anything from existing indexes. */
	if (ha_alter_info->ignore
	    && (ha_alter_info->handler_flags
		& (Alter_inplace_info::ADD_PK_INDEX
		   | Alter_inplace_info::ADD_UNIQUE_INDEX))) {
		ha_alter_info->unsupported_reason = innobase_get_err_msg(
			ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_IGNORE);
		DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
	}

	/* DROP PRIMARY KEY is only allowed in combination with ADD
	PRIMARY KEY. */
	if ((ha_alter_info->handler_flags
	     & (Alter_inplace_info::ADD_PK_INDEX
		| Alter_inplace_info::DROP_PK_INDEX))
	    == Alter_inplace_info::DROP_PK_INDEX) {
		ha_alter_info->unsupported_reason = innobase_get_err_msg(
			ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_NOPK);
		DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
	}

	/* If a column change from NOT NULL to NULL,
	and there's a implict pk on this column. the
	table should be rebuild. The change should
	only go through the "Copy" method.*/
	if ((ha_alter_info->handler_flags
	     & Alter_inplace_info::ALTER_COLUMN_NULLABLE)) {
		uint primary_key = altered_table->s->primary_key;

		/* See if MYSQL table has no pk but we do.*/
		if (UNIV_UNLIKELY(primary_key >= MAX_KEY)
		    && !row_table_got_default_clust_index(prebuilt->table)) {
			ha_alter_info->unsupported_reason = innobase_get_err_msg(
				ER_PRIMARY_CANT_HAVE_NULL);
			DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
		}
	}

	/* We should be able to do the operation in-place.
	See if we can do it online (LOCK=NONE). */
	bool	online = true;

	List_iterator_fast<Create_field> cf_it(
		ha_alter_info->alter_info->create_list);

	/* Fix the key parts. */
	for (KEY* new_key = ha_alter_info->key_info_buffer;
	     new_key < ha_alter_info->key_info_buffer
		     + ha_alter_info->key_count;
	     new_key++) {
		for (KEY_PART_INFO* key_part = new_key->key_part;
		     key_part < new_key->key_part + new_key->user_defined_key_parts;
		     key_part++) {
			const Create_field*	new_field;

			DBUG_ASSERT(key_part->fieldnr
				    < altered_table->s->fields);

			cf_it.rewind();
			for (uint fieldnr = 0; (new_field = cf_it++);
			     fieldnr++) {
				if (fieldnr == key_part->fieldnr) {
					break;
				}
			}

			DBUG_ASSERT(new_field);

			key_part->field = altered_table->field[
				key_part->fieldnr];
			/* In some special cases InnoDB emits "false"
			duplicate key errors with NULL key values. Let
			us play safe and ensure that we can correctly
			print key values even in such cases .*/
			key_part->null_offset = key_part->field->null_offset();
			key_part->null_bit = key_part->field->null_bit;

			if (new_field->field) {
				/* This is an existing column. */
				continue;
			}

			/* This is an added column. */
			DBUG_ASSERT(ha_alter_info->handler_flags
				    & Alter_inplace_info::ADD_COLUMN);

			/* We cannot replace a hidden FTS_DOC_ID
			with a user-visible FTS_DOC_ID. */
			if (prebuilt->table->fts
			    && innobase_fulltext_exist(altered_table->s)
			    && !my_strcasecmp(
				    system_charset_info,
				    key_part->field->field_name,
				    FTS_DOC_ID_COL_NAME)) {
				ha_alter_info->unsupported_reason = innobase_get_err_msg(
					ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_HIDDEN_FTS);
				DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
			}

			DBUG_ASSERT((MTYP_TYPENR(key_part->field->unireg_check)
				     == Field::NEXT_NUMBER)
				    == !!(key_part->field->flags
					  & AUTO_INCREMENT_FLAG));

			if (key_part->field->flags & AUTO_INCREMENT_FLAG) {
				/* We cannot assign an AUTO_INCREMENT
				column values during online ALTER. */
				DBUG_ASSERT(key_part->field == altered_table
					    -> found_next_number_field);
				ha_alter_info->unsupported_reason = innobase_get_err_msg(
					ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_AUTOINC);
				online = false;
			}
		}
	}

	DBUG_ASSERT(!prebuilt->table->fts || prebuilt->table->fts->doc_col
		    <= table->s->fields);
	DBUG_ASSERT(!prebuilt->table->fts || prebuilt->table->fts->doc_col
		    < dict_table_get_n_user_cols(prebuilt->table));

	if (prebuilt->table->fts
	    && innobase_fulltext_exist(altered_table->s)) {
		/* FULLTEXT indexes are supposed to remain. */
		/* Disallow DROP INDEX FTS_DOC_ID_INDEX */

		for (uint i = 0; i < ha_alter_info->index_drop_count; i++) {
			if (!my_strcasecmp(
				    system_charset_info,
				    ha_alter_info->index_drop_buffer[i]->name,
				    FTS_DOC_ID_INDEX_NAME)) {
				ha_alter_info->unsupported_reason = innobase_get_err_msg(
					ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_CHANGE_FTS);
				DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
			}
		}

		/* InnoDB can have a hidden FTS_DOC_ID_INDEX on a
		visible FTS_DOC_ID column as well. Prevent dropping or
		renaming the FTS_DOC_ID. */

		for (Field** fp = table->field; *fp; fp++) {
			if (!((*fp)->flags
			      & (FIELD_IS_RENAMED | FIELD_IS_DROPPED))) {
				continue;
			}

			if (!my_strcasecmp(
				    system_charset_info,
				    (*fp)->field_name,
				    FTS_DOC_ID_COL_NAME)) {
				ha_alter_info->unsupported_reason = innobase_get_err_msg(
					ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_CHANGE_FTS);
				DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
			}
		}
	}

	prebuilt->trx->will_lock++;

	if (!online) {
		/* We already determined that only a non-locking
		operation is possible. */
	} else if (((ha_alter_info->handler_flags
		     & Alter_inplace_info::ADD_PK_INDEX)
		    || innobase_need_rebuild(ha_alter_info))
		   && (innobase_fulltext_exist(altered_table->s)
		       || (prebuilt->table->flags2
			   & DICT_TF2_FTS_HAS_DOC_ID))) {
		/* Refuse to rebuild the table online, if
		fulltext indexes are to survive the rebuild,
		or if the table contains a hidden FTS_DOC_ID column. */
		online = false;
		/* If the table already contains fulltext indexes,
		refuse to rebuild the table natively altogether. */
		if (prebuilt->table->fts) {
			ha_alter_info->unsupported_reason = innobase_get_err_msg(
				ER_INNODB_FT_LIMIT);
			DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
		}
		ha_alter_info->unsupported_reason = innobase_get_err_msg(
			ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_FTS);
	} else if ((ha_alter_info->handler_flags
		    & Alter_inplace_info::ADD_INDEX)) {
		/* Building a full-text index requires a lock.
		We could do without a lock if the table already contains
		an FTS_DOC_ID column, but in that case we would have
		to apply the modification log to the full-text indexes. */

		for (uint i = 0; i < ha_alter_info->index_add_count; i++) {
			const KEY* key =
				&ha_alter_info->key_info_buffer[
					ha_alter_info->index_add_buffer[i]];
			if (key->flags & HA_FULLTEXT) {
				DBUG_ASSERT(!(key->flags & HA_KEYFLAG_MASK
					      & ~(HA_FULLTEXT
						  | HA_PACK_KEY
						  | HA_GENERATED_KEY
						  | HA_BINARY_PACK_KEY)));
				ha_alter_info->unsupported_reason = innobase_get_err_msg(
					ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_FTS);
				online = false;
				break;
			}
		}
	}

	DBUG_RETURN(online
		    ? HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE
		    : HA_ALTER_INPLACE_SHARED_LOCK_AFTER_PREPARE);
}

/*************************************************************//**
Initialize the dict_foreign_t structure with supplied info
@return true if added, false if duplicate foreign->id */
static __attribute__((nonnull(1,3,5,7)))
bool
innobase_init_foreign(
/*==================*/
	dict_foreign_t*	foreign,		/*!< in/out: structure to
						initialize */
	char*		constraint_name,	/*!< in/out: constraint name if
						exists */
	dict_table_t*	table,			/*!< in: foreign table */
	dict_index_t*	index,			/*!< in: foreign key index */
	const char**	column_names,		/*!< in: foreign key column
						names */
	ulint		num_field,		/*!< in: number of columns */
	const char*	referenced_table_name,	/*!< in: referenced table
						name */
	dict_table_t*	referenced_table,	/*!< in: referenced table */
	dict_index_t*	referenced_index,	/*!< in: referenced index */
	const char**	referenced_column_names,/*!< in: referenced column
						names */
	ulint		referenced_num_field)	/*!< in: number of referenced
						columns */
{
        if (constraint_name) {
                ulint   db_len;

                /* Catenate 'databasename/' to the constraint name specified
                by the user: we conceive the constraint as belonging to the
                same MySQL 'database' as the table itself. We store the name
                to foreign->id. */

                db_len = dict_get_db_name_len(table->name);

                foreign->id = static_cast<char*>(mem_heap_alloc(
                        foreign->heap, db_len + strlen(constraint_name) + 2));

                ut_memcpy(foreign->id, table->name, db_len);
                foreign->id[db_len] = '/';
                strcpy(foreign->id + db_len + 1, constraint_name);
        }

	ut_ad(mutex_own(&dict_sys->mutex));

	/* Check if any existing foreign key has the same id */

	for (const dict_foreign_t* existing_foreign
		= UT_LIST_GET_FIRST(table->foreign_list);
	     existing_foreign != 0;
	     existing_foreign = UT_LIST_GET_NEXT(
		     foreign_list, existing_foreign)) {

		if (ut_strcmp(existing_foreign->id, foreign->id) == 0) {
			return(false);
		}
	}

        foreign->foreign_table = table;
        foreign->foreign_table_name = mem_heap_strdup(
                foreign->heap, table->name);
        dict_mem_foreign_table_name_lookup_set(foreign, TRUE);

        foreign->foreign_index = index;
        foreign->n_fields = (unsigned int) num_field;

        foreign->foreign_col_names = static_cast<const char**>(
                mem_heap_alloc(foreign->heap, num_field * sizeof(void*)));

        for (ulint i = 0; i < foreign->n_fields; i++) {
                foreign->foreign_col_names[i] = mem_heap_strdup(
                        foreign->heap, column_names[i]);
        }

	foreign->referenced_index = referenced_index;
	foreign->referenced_table = referenced_table;

	foreign->referenced_table_name = mem_heap_strdup(
		foreign->heap, referenced_table_name);
        dict_mem_referenced_table_name_lookup_set(foreign, TRUE);

        foreign->referenced_col_names = static_cast<const char**>(
                mem_heap_alloc(foreign->heap,
			       referenced_num_field * sizeof(void*)));

        for (ulint i = 0; i < foreign->n_fields; i++) {
                foreign->referenced_col_names[i]
                        = mem_heap_strdup(foreign->heap,
					  referenced_column_names[i]);
        }

	return(true);
}

/*************************************************************//**
Check whether the foreign key options is legit
@return true if it is */
static __attribute__((nonnull, warn_unused_result))
bool
innobase_check_fk_option(
/*=====================*/
	dict_foreign_t*	foreign)	/*!< in:InnoDB Foreign key */
{
	if (foreign->type & (DICT_FOREIGN_ON_UPDATE_SET_NULL
			     | DICT_FOREIGN_ON_DELETE_SET_NULL)
	    && foreign->foreign_index) {

		for (ulint j = 0; j < foreign->n_fields; j++) {
			if ((dict_index_get_nth_col(
				foreign->foreign_index, j)->prtype)
				& DATA_NOT_NULL) {

				/* It is not sensible to define
				SET NULL if the column is not
				allowed to be NULL! */
				return(false);
			}
		}
	}

	return(true);
}

/*************************************************************//**
Set foreign key options
@return true if successfully set */
static __attribute__((nonnull, warn_unused_result))
bool
innobase_set_foreign_key_option(
/*============================*/
	dict_foreign_t*	foreign,	/*!< in:InnoDB Foreign key */
	Foreign_key*	fk_key)		/*!< in: Foreign key info from
					MySQL */
{
	ut_ad(!foreign->type);

	switch (fk_key->delete_opt) {
	case Foreign_key::FK_OPTION_NO_ACTION:
	case Foreign_key::FK_OPTION_RESTRICT:
	case Foreign_key::FK_OPTION_DEFAULT:
		foreign->type = DICT_FOREIGN_ON_DELETE_NO_ACTION;
		break;
	case Foreign_key::FK_OPTION_CASCADE:
		foreign->type = DICT_FOREIGN_ON_DELETE_CASCADE;
		break;
	case Foreign_key::FK_OPTION_SET_NULL:
		foreign->type = DICT_FOREIGN_ON_DELETE_SET_NULL;
		break;
	}

	switch (fk_key->update_opt) {
	case Foreign_key::FK_OPTION_NO_ACTION:
	case Foreign_key::FK_OPTION_RESTRICT:
	case Foreign_key::FK_OPTION_DEFAULT:
		foreign->type |= DICT_FOREIGN_ON_UPDATE_NO_ACTION;
		break;
	case Foreign_key::FK_OPTION_CASCADE:
		foreign->type |= DICT_FOREIGN_ON_UPDATE_CASCADE;
		break;
	case Foreign_key::FK_OPTION_SET_NULL:
		foreign->type |= DICT_FOREIGN_ON_UPDATE_SET_NULL;
		break;
	}

	return(innobase_check_fk_option(foreign));
}

/*******************************************************************//**
Check if a foreign key constraint can make use of an index
that is being created.
@return	useable index, or NULL if none found */
static __attribute__((nonnull, warn_unused_result))
const KEY*
innobase_find_equiv_index(
/*======================*/
	const char*const*	col_names,
					/*!< in: column names */
	uint			n_cols,	/*!< in: number of columns */
	const KEY*		keys,	/*!< in: index information */
	const uint*		add,	/*!< in: indexes being created */
	uint			n_add)	/*!< in: number of indexes to create */
{
	for (uint i = 0; i < n_add; i++) {
		const KEY*	key = &keys[add[i]];

		if (key->user_defined_key_parts < n_cols) {
no_match:
			continue;
		}

		for (uint j = 0; j < n_cols; j++) {
			const KEY_PART_INFO&	key_part = key->key_part[j];
			uint32			col_len
				= key_part.field->pack_length();

			/* The MySQL pack length contains 1 or 2 bytes
			length field for a true VARCHAR. */

			if (key_part.field->type() == MYSQL_TYPE_VARCHAR) {
				col_len -= static_cast<const Field_varstring*>(
					key_part.field)->length_bytes;
			}

			if (key_part.length < col_len) {

				/* Column prefix indexes cannot be
				used for FOREIGN KEY constraints. */
				goto no_match;
			}

			if (innobase_strcasecmp(col_names[j],
						key_part.field->field_name)) {
				/* Name mismatch */
				goto no_match;
			}
		}

		return(key);
	}

	return(NULL);
}

/*************************************************************//**
Found an index whose first fields are the columns in the array
in the same order and is not marked for deletion
@return matching index, NULL if not found */
static
dict_index_t*
innobase_find_fk_index(
/*===================*/
	Alter_inplace_info*	ha_alter_info,
					/*!< in: alter table info */
	dict_table_t*		table,	/*!< in: table */
	const char**		columns,/*!< in: array of column names */
	ulint			n_cols) /*!< in: number of columns */

{
        dict_index_t*	index;
        dict_index_t*	found_index = NULL;

        index = dict_table_get_first_index(table);

        while (index != NULL) {
                if (index->type & DICT_FTS) {
                        goto next_rec;
		} else if (dict_foreign_qualify_index(
			table, columns, n_cols, index, NULL, TRUE, FALSE)) {
			/* Check if this index is in the drop list */
			if (index) {
				KEY**	drop_key;

				drop_key = ha_alter_info->index_drop_buffer;

				for (uint i = 0;
				     i < ha_alter_info->index_drop_count;
				     i++) {
					if (innobase_strcasecmp(
						drop_key[i]->name,
						index->name) == 0) {
						goto next_rec;
					}
				}
			}

			found_index = index;
			break;
		}

next_rec:
                index = dict_table_get_next_index(index);
	}

	return(found_index);
}

/*************************************************************//**
Create InnoDB foreign key structure from MySQL alter_info
@retval true if successful
@retval false on error (will call my_error()) */
static
bool
innobase_get_foreign_key_info(
/*==========================*/
	Alter_inplace_info*
			ha_alter_info,	/*!< in: alter table info */
	const TABLE_SHARE*
			table_share,	/*!< in: the TABLE_SHARE */
	dict_table_t*	table,		/*!< in: table */
	dict_foreign_t**add_fk,		/*!< out: foreign constraint added */
	ulint*		n_add_fk,	/*!< out: number of foreign
					constraints added */
	mem_heap_t*	heap,		/*!< in: memory heap */
	const trx_t*	trx)		/*!< in: user transaction */
{
	Key*		key;
	Foreign_key*	fk_key;
	ulint		i = 0;
	dict_table_t*	referenced_table = NULL;
	char*		referenced_table_name = NULL;
	ulint		num_fk = 0;
	Alter_info*	alter_info = ha_alter_info->alter_info;

	*n_add_fk = 0;

	List_iterator<Key> key_iterator(alter_info->key_list);

	while ((key=key_iterator++)) {
		if (key->type == Key::FOREIGN_KEY) {
			const char*	column_names[MAX_NUM_FK_COLUMNS];
			dict_index_t*	index = NULL;
			const char*	referenced_column_names[MAX_NUM_FK_COLUMNS];
			dict_index_t*	referenced_index = NULL;
			ulint		num_col = 0;
			ulint		referenced_num_col = 0;
			bool		correct_option;
			char*		db_namep = NULL;
			char*		tbl_namep = NULL;
			ulint		db_name_len = 0;
			ulint		tbl_name_len = 0;
#ifdef __WIN__
			char		db_name[MAX_DATABASE_NAME_LEN];
			char		tbl_name[MAX_TABLE_NAME_LEN];
#endif

			fk_key= static_cast<Foreign_key*>(key);

			if (fk_key->columns.elements > 0) {
				Key_part_spec* column;
				List_iterator<Key_part_spec> key_part_iterator(
					fk_key->columns);

				/* Get all the foreign key column info for the
				current table */
				while ((column = key_part_iterator++)) {
					column_names[i] =
						 column->field_name.str;
					ut_ad(i < MAX_NUM_FK_COLUMNS);
					i++;
				}

				index = innobase_find_fk_index(
					ha_alter_info, table, column_names, i);

				/* MySQL would add a index in the creation
				list if no such index for foreign table,
				so we have to use DBUG_EXECUTE_IF to simulate
				the scenario */
				DBUG_EXECUTE_IF("innodb_test_no_foreign_idx",
						index = NULL;);

				/* Check whether there exist such
				index in the the index create clause */
				if (!index && !innobase_find_equiv_index(
					column_names, i,
					ha_alter_info->key_info_buffer,
					ha_alter_info->index_add_buffer,
					ha_alter_info->index_add_count)) {
					my_error(
						ER_FK_NO_INDEX_CHILD,
						MYF(0),
						fk_key->name.str,
						table_share->table_name.str);
					goto err_exit;
				}

				num_col = i;
			}

			add_fk[num_fk] = dict_mem_foreign_create();

#ifndef __WIN__
			tbl_namep = fk_key->ref_table.str;
			tbl_name_len = fk_key->ref_table.length;
			db_namep = fk_key->ref_db.str;
			db_name_len = fk_key->ref_db.length;
#else
			ut_ad(fk_key->ref_table.str);

			memcpy(tbl_name, fk_key->ref_table.str,
			       fk_key->ref_table.length);
			tbl_name[fk_key->ref_table.length] = 0;
			innobase_casedn_str(tbl_name);
			tbl_name_len = strlen(tbl_name);
			tbl_namep = &tbl_name[0];

			if (fk_key->ref_db.str != NULL) {
				memcpy(db_name, fk_key->ref_db.str,
				       fk_key->ref_db.length);
				db_name[fk_key->ref_db.length] = 0;
				innobase_casedn_str(db_name);
				db_name_len = strlen(db_name);
				db_namep = &db_name[0];
			}
#endif
			mutex_enter(&dict_sys->mutex);

			referenced_table_name = dict_get_referenced_table(
				table->name,
				db_namep,
				db_name_len,
				tbl_namep,
				tbl_name_len,
				&referenced_table,
				add_fk[num_fk]->heap);

			/* Test the case when referenced_table failed to
			open, if trx->check_foreigns is not set, we should
			still be able to add the foreign key */
			DBUG_EXECUTE_IF("innodb_test_open_ref_fail",
					referenced_table = NULL;);

			if (!referenced_table && trx->check_foreigns) {
				mutex_exit(&dict_sys->mutex);
				my_error(ER_FK_CANNOT_OPEN_PARENT,
					 MYF(0), tbl_namep);

				goto err_exit;
			}

			i = 0;

			if (fk_key->ref_columns.elements > 0) {
				Key_part_spec* column;
				List_iterator<Key_part_spec> key_part_iterator(
					fk_key->ref_columns);

				while ((column = key_part_iterator++)) {
					referenced_column_names[i] =
						 column->field_name.str;
					ut_ad(i < MAX_NUM_FK_COLUMNS);
					i++;
				}

				if (referenced_table) {
					referenced_index =
						dict_foreign_find_index(
							referenced_table,
							referenced_column_names,
							i, NULL,
							TRUE, FALSE);

					DBUG_EXECUTE_IF(
						"innodb_test_no_reference_idx",
						referenced_index = NULL;);

					/* Check whether there exist such
					index in the the index create clause */
					if (!referenced_index) {
						mutex_exit(&dict_sys->mutex);
						my_error(
							ER_FK_NO_INDEX_PARENT,
							MYF(0),
							fk_key->name.str,
							tbl_namep);
						goto err_exit;
					}
				} else {
					ut_a(!trx->check_foreigns);
				}

				referenced_num_col = i;
			}

			if (!innobase_init_foreign(
				add_fk[num_fk], fk_key->name.str,
				table, index, column_names,
				num_col, referenced_table_name,
				referenced_table, referenced_index,
				referenced_column_names, referenced_num_col)) {
					mutex_exit(&dict_sys->mutex);
					my_error(
						ER_FK_DUP_NAME,
						MYF(0),
						add_fk[num_fk]->id);
					goto err_exit;
			}

			mutex_exit(&dict_sys->mutex);

			correct_option = innobase_set_foreign_key_option(
						add_fk[num_fk], fk_key);

			DBUG_EXECUTE_IF("innodb_test_wrong_fk_option",
					correct_option = false;);

			if (!correct_option) {
				my_error(ER_FK_INCORRECT_OPTION,
					 MYF(0),
					 table_share->table_name.str,
					 add_fk[num_fk]->id);
				goto err_exit;
			}

			num_fk++;
			i = 0;
		}

	}

	*n_add_fk = num_fk;

	return(true);
err_exit:
	for (i = 0; i <= num_fk; i++) {
		if (add_fk[i]) {
			dict_foreign_free(add_fk[i]);
		}
	}

	return(false);
}

/*************************************************************//**
Copies an InnoDB column to a MySQL field.  This function is
adapted from row_sel_field_store_in_mysql_format(). */
static
void
innobase_col_to_mysql(
/*==================*/
	const dict_col_t*	col,	/*!< in: InnoDB column */
	const uchar*		data,	/*!< in: InnoDB column data */
	ulint			len,	/*!< in: length of data, in bytes */
	Field*			field)	/*!< in/out: MySQL field */
{
	uchar*	ptr;
	uchar*	dest	= field->ptr;
	ulint	flen	= field->pack_length();

	switch (col->mtype) {
	case DATA_INT:
		ut_ad(len == flen);

		/* Convert integer data from Innobase to little-endian
		format, sign bit restored to normal */

		for (ptr = dest + len; ptr != dest; ) {
			*--ptr = *data++;
		}

		if (!(field->flags & UNSIGNED_FLAG)) {
			((byte*) dest)[len - 1] ^= 0x80;
		}

		break;

	case DATA_VARCHAR:
	case DATA_VARMYSQL:
	case DATA_BINARY:
		field->reset();

		if (field->type() == MYSQL_TYPE_VARCHAR) {
			/* This is a >= 5.0.3 type true VARCHAR. Store the
			length of the data to the first byte or the first
			two bytes of dest. */

			dest = row_mysql_store_true_var_len(
				dest, len, flen - field->key_length());
		}

		/* Copy the actual data */
		memcpy(dest, data, len);
		break;

	case DATA_BLOB:
		/* Skip MySQL BLOBs when reporting an erroneous row
		during index creation or table rebuild. */
		field->set_null();
		break;

#ifdef UNIV_DEBUG
	case DATA_MYSQL:
		ut_ad(flen >= len);
		ut_ad(DATA_MBMAXLEN(col->mbminmaxlen)
		      >= DATA_MBMINLEN(col->mbminmaxlen));
		memcpy(dest, data, len);
		break;

	default:
	case DATA_SYS_CHILD:
	case DATA_SYS:
		/* These column types should never be shipped to MySQL. */
		ut_ad(0);

	case DATA_FIXBINARY:
	case DATA_FLOAT:
	case DATA_DOUBLE:
	case DATA_DECIMAL:
		/* Above are the valid column types for MySQL data. */
		ut_ad(flen == len);
		/* fall through */
	case DATA_CHAR:
		/* We may have flen > len when there is a shorter
		prefix on a CHAR column. */
		ut_ad(flen >= len);
#else /* UNIV_DEBUG */
	default:
#endif /* UNIV_DEBUG */
		memcpy(dest, data, len);
	}
}

/*************************************************************//**
Copies an InnoDB record to table->record[0]. */
UNIV_INTERN
void
innobase_rec_to_mysql(
/*==================*/
	struct TABLE*		table,	/*!< in/out: MySQL table */
	const rec_t*		rec,	/*!< in: record */
	const dict_index_t*	index,	/*!< in: index */
	const ulint*		offsets)/*!< in: rec_get_offsets(
					rec, index, ...) */
{
	uint	n_fields	= table->s->fields;

	ut_ad(n_fields == dict_table_get_n_user_cols(index->table)
	      - !!(DICT_TF2_FLAG_IS_SET(index->table,
					DICT_TF2_FTS_HAS_DOC_ID)));

	for (uint i = 0; i < n_fields; i++) {
		Field*		field	= table->field[i];
		ulint		ipos;
		ulint		ilen;
		const uchar*	ifield;

		field->reset();

		ipos = dict_index_get_nth_col_or_prefix_pos(index, i, TRUE);

		if (ipos == ULINT_UNDEFINED
		    || rec_offs_nth_extern(offsets, ipos)) {
null_field:
			field->set_null();
			continue;
		}

		ifield = rec_get_nth_field(rec, offsets, ipos, &ilen);

		/* Assign the NULL flag */
		if (ilen == UNIV_SQL_NULL) {
			ut_ad(field->real_maybe_null());
			goto null_field;
		}

		field->set_notnull();

		innobase_col_to_mysql(
			dict_field_get_col(
				dict_index_get_nth_field(index, ipos)),
			ifield, ilen, field);
	}
}

/*************************************************************//**
Copies an InnoDB index entry to table->record[0]. */
UNIV_INTERN
void
innobase_fields_to_mysql(
/*=====================*/
	struct TABLE*		table,	/*!< in/out: MySQL table */
	const dict_index_t*	index,	/*!< in: InnoDB index */
	const dfield_t*		fields)	/*!< in: InnoDB index fields */
{
	uint	n_fields	= table->s->fields;

	ut_ad(n_fields == dict_table_get_n_user_cols(index->table)
	      - !!(DICT_TF2_FLAG_IS_SET(index->table,
					DICT_TF2_FTS_HAS_DOC_ID)));

	for (uint i = 0; i < n_fields; i++) {
		Field*		field	= table->field[i];
		ulint		ipos;

		field->reset();

		ipos = dict_index_get_nth_col_or_prefix_pos(index, i, TRUE);

		if (ipos == ULINT_UNDEFINED
		    || dfield_is_ext(&fields[ipos])
		    || dfield_is_null(&fields[ipos])) {

			field->set_null();
		} else {
			field->set_notnull();

			const dfield_t*	df	= &fields[ipos];

			innobase_col_to_mysql(
				dict_field_get_col(
					dict_index_get_nth_field(index, ipos)),
				static_cast<const uchar*>(dfield_get_data(df)),
				dfield_get_len(df), field);
		}
	}
}

/*************************************************************//**
Copies an InnoDB row to table->record[0]. */
UNIV_INTERN
void
innobase_row_to_mysql(
/*==================*/
	struct TABLE*		table,	/*!< in/out: MySQL table */
	const dict_table_t*	itab,	/*!< in: InnoDB table */
	const dtuple_t*		row)	/*!< in: InnoDB row */
{
	uint  n_fields	= table->s->fields;

	/* The InnoDB row may contain an extra FTS_DOC_ID column at the end. */
	ut_ad(row->n_fields == dict_table_get_n_cols(itab));
	ut_ad(n_fields == row->n_fields - DATA_N_SYS_COLS
	      - !!(DICT_TF2_FLAG_IS_SET(itab, DICT_TF2_FTS_HAS_DOC_ID)));

	for (uint i = 0; i < n_fields; i++) {
		Field*		field	= table->field[i];
		const dfield_t*	df	= dtuple_get_nth_field(row, i);

		field->reset();

		if (dfield_is_ext(df) || dfield_is_null(df)) {
			field->set_null();
		} else {
			field->set_notnull();

			innobase_col_to_mysql(
				dict_table_get_nth_col(itab, i),
				static_cast<const uchar*>(dfield_get_data(df)),
				dfield_get_len(df), field);
		}
	}
}

/*************************************************************//**
Resets table->record[0]. */
UNIV_INTERN
void
innobase_rec_reset(
/*===============*/
	TABLE*			table)		/*!< in/out: MySQL table */
{
	uint	n_fields	= table->s->fields;
	uint	i;

	for (i = 0; i < n_fields; i++) {
		table->field[i]->set_default();
	}
}

/*******************************************************************//**
This function checks that index keys are sensible.
@return	0 or error number */
static __attribute__((nonnull, warn_unused_result))
int
innobase_check_index_keys(
/*======================*/
	const Alter_inplace_info*	info,
				/*!< in: indexes to be created or dropped */
	const dict_table_t*		innodb_table)
				/*!< in: Existing indexes */
{
	for (uint key_num = 0; key_num < info->index_add_count;
	     key_num++) {
		const KEY&	key = info->key_info_buffer[
			info->index_add_buffer[key_num]];

		/* Check that the same index name does not appear
		twice in indexes to be created. */

		for (ulint i = 0; i < key_num; i++) {
			const KEY&	key2 = info->key_info_buffer[
				info->index_add_buffer[i]];

			if (0 == strcmp(key.name, key2.name)) {
				my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0),
					 key.name);

				return(ER_WRONG_NAME_FOR_INDEX);
			}
		}

		/* Check that the same index name does not already exist. */

		const dict_index_t* index;

		for (index = dict_table_get_first_index(innodb_table);
		     index; index = dict_table_get_next_index(index)) {

			if (!strcmp(key.name, index->name)) {
				break;
			}
		}

		if (index) {
			/* If a key by the same name is being created and
			dropped, the name clash is OK. */
			for (uint i = 0; i < info->index_drop_count;
			     i++) {
				const KEY*	drop_key
					= info->index_drop_buffer[i];

				if (0 == strcmp(key.name, drop_key->name)) {
					goto name_ok;
				}
			}

			my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key.name);

			return(ER_WRONG_NAME_FOR_INDEX);
		}

name_ok:
		for (ulint i = 0; i < key.user_defined_key_parts; i++) {
			const KEY_PART_INFO&	key_part1
				= key.key_part[i];
			const Field*		field
				= key_part1.field;
			ibool			is_unsigned;

			switch (get_innobase_type_from_mysql_type(
					&is_unsigned, field)) {
			default:
				break;
			case DATA_INT:
			case DATA_FLOAT:
			case DATA_DOUBLE:
			case DATA_DECIMAL:
				/* Check that MySQL does not try to
				create a column prefix index field on
				an inappropriate data type. */

				if (field->type() == MYSQL_TYPE_VARCHAR) {
					if (key_part1.length
					    >= field->pack_length()
					    - ((Field_varstring*) field)
					    ->length_bytes) {
						break;
					}
				} else {
					if (key_part1.length
					    >= field->pack_length()) {
						break;
					}
				}

				my_error(ER_WRONG_KEY_COLUMN, MYF(0),
					 field->field_name);
				return(ER_WRONG_KEY_COLUMN);
			}

			/* Check that the same column does not appear
			twice in the index. */

			for (ulint j = 0; j < i; j++) {
				const KEY_PART_INFO&	key_part2
					= key.key_part[j];

				if (key_part1.fieldnr != key_part2.fieldnr) {
					continue;
				}

				my_error(ER_WRONG_KEY_COLUMN, MYF(0),
					 field->field_name);
				return(ER_WRONG_KEY_COLUMN);
			}
		}
	}

	return(0);
}

/*******************************************************************//**
Create index field definition for key part */
static __attribute__((nonnull(2,3)))
void
innobase_create_index_field_def(
/*============================*/
	const TABLE*		altered_table,	/*!< in: MySQL table that is
						being altered, or NULL
						if a new clustered index is
						not being created */
	const KEY_PART_INFO*	key_part,	/*!< in: MySQL key definition */
	index_field_t*		index_field)	/*!< out: index field
						definition for key_part */
{
	const Field*	field;
	ibool		is_unsigned;
	ulint		col_type;

	DBUG_ENTER("innobase_create_index_field_def");

	ut_ad(key_part);
	ut_ad(index_field);

	field = altered_table
		? altered_table->field[key_part->fieldnr]
		: key_part->field;
	ut_a(field);

	index_field->col_no = key_part->fieldnr;

	col_type = get_innobase_type_from_mysql_type(&is_unsigned, field);

	if (DATA_BLOB == col_type
	    || (key_part->length < field->pack_length()
		&& field->type() != MYSQL_TYPE_VARCHAR)
	    || (field->type() == MYSQL_TYPE_VARCHAR
		&& key_part->length < field->pack_length()
			- ((Field_varstring*) field)->length_bytes)) {

		index_field->prefix_len = key_part->length;
	} else {
		index_field->prefix_len = 0;
	}

	DBUG_VOID_RETURN;
}

/*******************************************************************//**
Create index definition for key */
static __attribute__((nonnull))
void
innobase_create_index_def(
/*======================*/
	const TABLE*		altered_table,	/*!< in: MySQL table that is
						being altered */
	const KEY*		keys,		/*!< in: key definitions */
	ulint			key_number,	/*!< in: MySQL key number */
	bool			new_clustered,	/*!< in: true if generating
						a new clustered index
						on the table */
	bool			key_clustered,	/*!< in: true if this is
						the new clustered index */
	index_def_t*		index,		/*!< out: index definition */
	mem_heap_t*		heap)		/*!< in: heap where memory
						is allocated */
{
	const KEY*	key = &keys[key_number];
	ulint		i;
	ulint		len;
	ulint		n_fields = key->user_defined_key_parts;
	char*		index_name;

	DBUG_ENTER("innobase_create_index_def");
	DBUG_ASSERT(!key_clustered || new_clustered);

	index->fields = static_cast<index_field_t*>(
		mem_heap_alloc(heap, n_fields * sizeof *index->fields));

	index->ind_type = 0;
	index->key_number = key_number;
	index->n_fields = n_fields;
	len = strlen(key->name) + 1;
	index->name = index_name = static_cast<char*>(
		mem_heap_alloc(heap, len + !new_clustered));

	if (!new_clustered) {
		*index_name++ = TEMP_INDEX_PREFIX;
	}

	memcpy(index_name, key->name, len);

	if (key->flags & HA_NOSAME) {
		index->ind_type |= DICT_UNIQUE;
	}

	if (key_clustered) {
		DBUG_ASSERT(!(key->flags & HA_FULLTEXT));
		index->ind_type |= DICT_CLUSTERED;
	} else if (key->flags & HA_FULLTEXT) {
		DBUG_ASSERT(!(key->flags & HA_KEYFLAG_MASK
			      & ~(HA_FULLTEXT
				  | HA_PACK_KEY
				  | HA_BINARY_PACK_KEY)));
		DBUG_ASSERT(!(key->flags & HA_NOSAME));
		DBUG_ASSERT(!index->ind_type);
		index->ind_type |= DICT_FTS;
	}

	if (!new_clustered) {
		altered_table = NULL;
	}

	for (i = 0; i < n_fields; i++) {
		innobase_create_index_field_def(
			altered_table, &key->key_part[i], &index->fields[i]);
	}

	DBUG_VOID_RETURN;
}

/*******************************************************************//**
Check whether the table has the FTS_DOC_ID column
@return whether there exists an FTS_DOC_ID column */
static
bool
innobase_fts_check_doc_id_col(
/*==========================*/
	const dict_table_t*	table,  /*!< in: InnoDB table with
					fulltext index */
	const TABLE*		altered_table,
					/*!< in: MySQL table with
					fulltext index */
	ulint*			fts_doc_col_no)
					/*!< out: The column number for
					Doc ID, or ULINT_UNDEFINED
					if it is of wrong type */
{
	*fts_doc_col_no = ULINT_UNDEFINED;

	const uint n_cols = altered_table->s->fields;
	uint i;

	for (i = 0; i < n_cols; i++) {
		const Field*	field = altered_table->s->field[i];

		if (my_strcasecmp(system_charset_info,
				  field->field_name, FTS_DOC_ID_COL_NAME)) {
			continue;
		}

		if (strcmp(field->field_name, FTS_DOC_ID_COL_NAME)) {
			my_error(ER_WRONG_COLUMN_NAME, MYF(0),
				 field->field_name);
		} else if (field->type() != MYSQL_TYPE_LONGLONG
			   || field->pack_length() != 8
			   || field->real_maybe_null()
			   || !(field->flags & UNSIGNED_FLAG)) {
			my_error(ER_INNODB_FT_WRONG_DOCID_COLUMN, MYF(0),
				 field->field_name);
		} else {
			*fts_doc_col_no = i;
		}

		return(true);
	}

	if (!table) {
		return(false);
	}

	for (; i + DATA_N_SYS_COLS < (uint) table->n_cols; i++) {
		const char*     name = dict_table_get_col_name(table, i);

		if (strcmp(name, FTS_DOC_ID_COL_NAME) == 0) {
#ifdef UNIV_DEBUG
			const dict_col_t*       col;

			col = dict_table_get_nth_col(table, i);

			/* Because the FTS_DOC_ID does not exist in
			the MySQL data dictionary, this must be the
			internally created FTS_DOC_ID column. */
			ut_ad(col->mtype == DATA_INT);
			ut_ad(col->len == 8);
			ut_ad(col->prtype & DATA_NOT_NULL);
			ut_ad(col->prtype & DATA_UNSIGNED);
#endif /* UNIV_DEBUG */
			*fts_doc_col_no = i;
			return(true);
		}
	}

	return(false);
}

/*******************************************************************//**
Check whether the table has a unique index with FTS_DOC_ID_INDEX_NAME
on the Doc ID column.
@return	the status of the FTS_DOC_ID index */
UNIV_INTERN
enum fts_doc_id_index_enum
innobase_fts_check_doc_id_index(
/*============================*/
	const dict_table_t*	table,		/*!< in: table definition */
	const TABLE*		altered_table,	/*!< in: MySQL table
						that is being altered */
	ulint*			fts_doc_col_no)	/*!< out: The column number for
						Doc ID, or ULINT_UNDEFINED
						if it is being created in
						ha_alter_info */
{
	const dict_index_t*	index;
	const dict_field_t*	field;

	if (altered_table) {
		/* Check if a unique index with the name of
		FTS_DOC_ID_INDEX_NAME is being created. */

		for (uint i = 0; i < altered_table->s->keys; i++) {
			const KEY& key = altered_table->s->key_info[i];

			if (innobase_strcasecmp(
				    key.name, FTS_DOC_ID_INDEX_NAME)) {
				continue;
			}

			if ((key.flags & HA_NOSAME)
			    && key.user_defined_key_parts == 1
			    && !strcmp(key.name, FTS_DOC_ID_INDEX_NAME)
			    && !strcmp(key.key_part[0].field->field_name,
				       FTS_DOC_ID_COL_NAME)) {
				if (fts_doc_col_no) {
					*fts_doc_col_no = ULINT_UNDEFINED;
				}
				return(FTS_EXIST_DOC_ID_INDEX);
			} else {
				return(FTS_INCORRECT_DOC_ID_INDEX);
			}
		}
	}

	if (!table) {
		return(FTS_NOT_EXIST_DOC_ID_INDEX);
	}

	for (index = dict_table_get_first_index(table);
	     index; index = dict_table_get_next_index(index)) {

		/* Check if there exists a unique index with the name of
		FTS_DOC_ID_INDEX_NAME */
		if (innobase_strcasecmp(index->name, FTS_DOC_ID_INDEX_NAME)) {
			continue;
		}

		if (!dict_index_is_unique(index)
		    || dict_index_get_n_unique(index) > 1
		    || strcmp(index->name, FTS_DOC_ID_INDEX_NAME)) {
			return(FTS_INCORRECT_DOC_ID_INDEX);
		}

		/* Check whether the index has FTS_DOC_ID as its
		first column */
		field = dict_index_get_nth_field(index, 0);

		/* The column would be of a BIGINT data type */
		if (strcmp(field->name, FTS_DOC_ID_COL_NAME) == 0
		    && field->col->mtype == DATA_INT
		    && field->col->len == 8
		    && field->col->prtype & DATA_NOT_NULL) {
			if (fts_doc_col_no) {
				*fts_doc_col_no = dict_col_get_no(field->col);
			}
			return(FTS_EXIST_DOC_ID_INDEX);
		} else {
			return(FTS_INCORRECT_DOC_ID_INDEX);
		}
	}


	/* Not found */
	return(FTS_NOT_EXIST_DOC_ID_INDEX);
}
/*******************************************************************//**
Check whether the table has a unique index with FTS_DOC_ID_INDEX_NAME
on the Doc ID column in MySQL create index definition.
@return	FTS_EXIST_DOC_ID_INDEX if there exists the FTS_DOC_ID index,
FTS_INCORRECT_DOC_ID_INDEX if the FTS_DOC_ID index is of wrong format */
UNIV_INTERN
enum fts_doc_id_index_enum
innobase_fts_check_doc_id_index_in_def(
/*===================================*/
	ulint		n_key,		/*!< in: Number of keys */
	const KEY*	key_info)	/*!< in: Key definition */
{
	/* Check whether there is a "FTS_DOC_ID_INDEX" in the to be built index
	list */
	for (ulint j = 0; j < n_key; j++) {
		const KEY*	key = &key_info[j];

		if (innobase_strcasecmp(key->name, FTS_DOC_ID_INDEX_NAME)) {
			continue;
		}

		/* Do a check on FTS DOC ID_INDEX, it must be unique,
		named as "FTS_DOC_ID_INDEX" and on column "FTS_DOC_ID" */
		if (!(key->flags & HA_NOSAME)
		    || key->user_defined_key_parts != 1
		    || strcmp(key->name, FTS_DOC_ID_INDEX_NAME)
		    || strcmp(key->key_part[0].field->field_name,
			      FTS_DOC_ID_COL_NAME)) {
			return(FTS_INCORRECT_DOC_ID_INDEX);
		}

		return(FTS_EXIST_DOC_ID_INDEX);
	}

	return(FTS_NOT_EXIST_DOC_ID_INDEX);
}
/*******************************************************************//**
Create an index table where indexes are ordered as follows:

IF a new primary key is defined for the table THEN

	1) New primary key
	2) The remaining keys in key_info

ELSE

	1) All new indexes in the order they arrive from MySQL

ENDIF

@return	key definitions */
static __attribute__((nonnull, warn_unused_result, malloc))
index_def_t*
innobase_create_key_defs(
/*=====================*/
	mem_heap_t*			heap,
			/*!< in/out: memory heap where space for key
			definitions are allocated */
	const Alter_inplace_info*	ha_alter_info,
			/*!< in: alter operation */
	const TABLE*			altered_table,
			/*!< in: MySQL table that is being altered */
	ulint&				n_add,
			/*!< in/out: number of indexes to be created */
	ulint&				n_fts_add,
			/*!< out: number of FTS indexes to be created */
	bool				got_default_clust,
			/*!< in: whether the table lacks a primary key */
	ulint&				fts_doc_id_col,
			/*!< in: The column number for Doc ID */
	bool&				add_fts_doc_id,
			/*!< in: whether we need to add new DOC ID
			column for FTS index */
	bool&				add_fts_doc_idx)
			/*!< in: whether we need to add new DOC ID
			index for FTS index */
{
	index_def_t*		indexdef;
	index_def_t*		indexdefs;
	bool			new_primary;
	const uint*const	add
		= ha_alter_info->index_add_buffer;
	const KEY*const		key_info
		= ha_alter_info->key_info_buffer;

	DBUG_ENTER("innobase_create_key_defs");
	DBUG_ASSERT(!add_fts_doc_id || add_fts_doc_idx);
	DBUG_ASSERT(ha_alter_info->index_add_count == n_add);

	/* If there is a primary key, it is always the first index
	defined for the innodb_table. */

	new_primary = n_add > 0
		&& !my_strcasecmp(system_charset_info,
				  key_info[*add].name, "PRIMARY");
	n_fts_add = 0;

	/* If there is a UNIQUE INDEX consisting entirely of NOT NULL
	columns and if the index does not contain column prefix(es)
	(only prefix/part of the column is indexed), MySQL will treat the
	index as a PRIMARY KEY unless the table already has one. */

	if (n_add > 0 && !new_primary && got_default_clust
	    && (key_info[*add].flags & HA_NOSAME)
	    && !(key_info[*add].flags & HA_KEY_HAS_PART_KEY_SEG)) {
		uint	key_part = key_info[*add].user_defined_key_parts;

		new_primary = true;

		while (key_part--) {
			const uint	maybe_null
				= key_info[*add].key_part[key_part].key_type
				& FIELDFLAG_MAYBE_NULL;
			DBUG_ASSERT(!maybe_null
				    == !key_info[*add].key_part[key_part].
				    field->real_maybe_null());

			if (maybe_null) {
				new_primary = false;
				break;
			}
		}
	}

	const bool rebuild = new_primary || add_fts_doc_id
		|| innobase_need_rebuild(ha_alter_info);
	/* Reserve one more space if new_primary is true, and we might
	need to add the FTS_DOC_ID_INDEX */
	indexdef = indexdefs = static_cast<index_def_t*>(
		mem_heap_alloc(
			heap, sizeof *indexdef
			* (ha_alter_info->key_count
			   + rebuild
			   + got_default_clust)));

	if (rebuild) {
		ulint	primary_key_number;

		if (new_primary) {
			DBUG_ASSERT(n_add > 0);
			primary_key_number = *add;
		} else if (got_default_clust) {
			/* Create the GEN_CLUST_INDEX */
			index_def_t*	index = indexdef++;

			index->fields = NULL;
			index->n_fields = 0;
			index->ind_type = DICT_CLUSTERED;
			index->name = mem_heap_strdup(
				heap, innobase_index_reserve_name);
			index->key_number = ~0;
			primary_key_number = ULINT_UNDEFINED;
			goto created_clustered;
		} else {
			primary_key_number = 0;
		}

		/* Create the PRIMARY key index definition */
		innobase_create_index_def(
			altered_table, key_info, primary_key_number,
			TRUE, TRUE, indexdef++, heap);

created_clustered:
		n_add = 1;

		for (ulint i = 0; i < ha_alter_info->key_count; i++) {
			if (i == primary_key_number) {
				continue;
			}
			/* Copy the index definitions. */
			innobase_create_index_def(
				altered_table, key_info, i, TRUE, FALSE,
				indexdef, heap);

			if (indexdef->ind_type & DICT_FTS) {
				n_fts_add++;
			}

			indexdef++;
			n_add++;
		}

		if (n_fts_add > 0) {
			if (!add_fts_doc_id
			    && !innobase_fts_check_doc_id_col(
				    NULL, altered_table,
				    &fts_doc_id_col)) {
				fts_doc_id_col = altered_table->s->fields;
				add_fts_doc_id = true;
			}

			if (!add_fts_doc_idx) {
				fts_doc_id_index_enum	ret;
				ulint			doc_col_no;

				ret = innobase_fts_check_doc_id_index(
					NULL, altered_table, &doc_col_no);

				/* This should have been checked before */
				ut_ad(ret != FTS_INCORRECT_DOC_ID_INDEX);

				if (ret == FTS_NOT_EXIST_DOC_ID_INDEX) {
					add_fts_doc_idx = true;
				} else {
					ut_ad(ret == FTS_EXIST_DOC_ID_INDEX);
					ut_ad(doc_col_no == ULINT_UNDEFINED
					      || doc_col_no == fts_doc_id_col);
				}
			}
		}
	} else {
		/* Create definitions for added secondary indexes. */

		for (ulint i = 0; i < n_add; i++) {
			innobase_create_index_def(
				altered_table, key_info, add[i], FALSE, FALSE,
				indexdef, heap);

			if (indexdef->ind_type & DICT_FTS) {
				n_fts_add++;
			}

			indexdef++;
		}
	}

	DBUG_ASSERT(indexdefs + n_add == indexdef);

	if (add_fts_doc_idx) {
		index_def_t*	index = indexdef++;

		index->fields = static_cast<index_field_t*>(
			mem_heap_alloc(heap, sizeof *index->fields));
		index->n_fields = 1;
		index->fields->col_no = fts_doc_id_col;
		index->fields->prefix_len = 0;
		index->ind_type = DICT_UNIQUE;

		if (rebuild) {
			index->name = mem_heap_strdup(
				heap, FTS_DOC_ID_INDEX_NAME);
			ut_ad(!add_fts_doc_id
			      || fts_doc_id_col == altered_table->s->fields);
		} else {
			char*	index_name;
			index->name = index_name = static_cast<char*>(
				mem_heap_alloc(
					heap,
					1 + sizeof FTS_DOC_ID_INDEX_NAME));
			*index_name++ = TEMP_INDEX_PREFIX;
			memcpy(index_name, FTS_DOC_ID_INDEX_NAME,
			       sizeof FTS_DOC_ID_INDEX_NAME);
		}

		/* TODO: assign a real MySQL key number for this */
		index->key_number = ULINT_UNDEFINED;
		n_add++;
	}

	DBUG_ASSERT(indexdef > indexdefs);
	DBUG_ASSERT((ulint) (indexdef - indexdefs)
		    <= ha_alter_info->key_count
		    + add_fts_doc_idx + got_default_clust);
	DBUG_ASSERT(ha_alter_info->index_add_count <= n_add);
	DBUG_RETURN(indexdefs);
}

/*******************************************************************//**
Check each index column size, make sure they do not exceed the max limit
@return	true if index column size exceeds limit */
static __attribute__((nonnull, warn_unused_result))
bool
innobase_check_column_length(
/*=========================*/
	ulint		max_col_len,	/*!< in: maximum column length */
	const KEY*	key_info)	/*!< in: Indexes to be created */
{
	for (ulint key_part = 0; key_part < key_info->user_defined_key_parts; key_part++) {
		if (key_info->key_part[key_part].length > max_col_len) {
			return(true);
		}
	}
	return(false);
}

struct ha_innobase_inplace_ctx : public inplace_alter_handler_ctx
{
	/** Dummy query graph */
	que_thr_t*	thr;
	/** InnoDB indexes being created */
	dict_index_t**	add;
	/** MySQL key numbers for the InnoDB indexes that are being created */
	const ulint*	add_key_numbers;
	/** number of InnoDB indexes being created */
	const ulint	num_to_add;
	/** InnoDB indexes being dropped */
	dict_index_t**	drop;
	/** number of InnoDB indexes being dropped */
	const ulint	num_to_drop;
	/** InnoDB foreign key constraints being dropped */
	dict_foreign_t** drop_fk;
	/** number of InnoDB foreign key constraints being dropped */
	const ulint	num_to_drop_fk;
	/** InnoDB foreign key constraints being added */
	dict_foreign_t** add_fk;
	/** number of InnoDB foreign key constraints being dropped */
	const ulint	num_to_add_fk;
	/** whether to create the indexes online */
	bool		online;
	/** memory heap */
	mem_heap_t*	heap;
	/** dictionary transaction */
	trx_t*		trx;
	/** table where the indexes are being created or dropped */
	dict_table_t*	indexed_table;
	/** mapping of old column numbers to new ones, or NULL */
	const ulint*	col_map;
	/** added AUTO_INCREMENT column position, or ULINT_UNDEFINED */
	const ulint	add_autoinc;
	/** default values of ADD COLUMN, or NULL */
	const dtuple_t*	add_cols;
	/** autoinc sequence to use */
	ib_sequence_t	sequence;

	ha_innobase_inplace_ctx(trx_t* user_trx,
				dict_index_t** add_arg,
				const ulint* add_key_numbers_arg,
				ulint num_to_add_arg,
				dict_index_t** drop_arg,
				ulint num_to_drop_arg,
				dict_foreign_t** drop_fk_arg,
				ulint num_to_drop_fk_arg,
				dict_foreign_t** add_fk_arg,
				ulint num_to_add_fk_arg,
				bool online_arg,
				mem_heap_t* heap_arg,
				trx_t* trx_arg,
				dict_table_t* indexed_table_arg,
				const ulint* col_map_arg,
				ulint add_autoinc_arg,
				ulonglong autoinc_col_min_value_arg,
				ulonglong autoinc_col_max_value_arg,
				const dtuple_t*	add_cols_arg) :
		inplace_alter_handler_ctx(),
		add (add_arg), add_key_numbers (add_key_numbers_arg),
		num_to_add (num_to_add_arg),
		drop (drop_arg), num_to_drop (num_to_drop_arg),
		drop_fk (drop_fk_arg), num_to_drop_fk (num_to_drop_fk_arg),
		add_fk (add_fk_arg), num_to_add_fk (num_to_add_fk_arg),
		online (online_arg), heap (heap_arg), trx (trx_arg),
		indexed_table (indexed_table_arg),
		col_map (col_map_arg), add_autoinc (add_autoinc_arg),
		add_cols (add_cols_arg),
		sequence(user_trx ? user_trx->mysql_thd : 0,
			 autoinc_col_min_value_arg, autoinc_col_max_value_arg)
	{
#ifdef UNIV_DEBUG
		for (ulint i = 0; i < num_to_add; i++) {
			ut_ad(!add[i]->to_be_dropped);
		}
		for (ulint i = 0; i < num_to_drop; i++) {
			ut_ad(drop[i]->to_be_dropped);
		}
#endif /* UNIV_DEBUG */

		thr = pars_complete_graph_for_exec(NULL, user_trx, heap);
	}

	~ha_innobase_inplace_ctx()
	{
		mem_heap_free(heap);
	}

private:
	// Disable copying
	ha_innobase_inplace_ctx(const ha_innobase_inplace_ctx&);
	ha_innobase_inplace_ctx& operator=(const ha_innobase_inplace_ctx&);
};

/********************************************************************//**
Drop any indexes that we were not able to free previously due to
open table handles. */
static
void
online_retry_drop_indexes_low(
/*==========================*/
	dict_table_t*	table,	/*!< in/out: table */
	trx_t*		trx)	/*!< in/out: transaction */
{
	ut_ad(mutex_own(&dict_sys->mutex));
	ut_ad(trx->dict_operation_lock_mode == RW_X_LATCH);
	ut_ad(trx_get_dict_operation(trx) == TRX_DICT_OP_INDEX);

	/* We can have table->n_ref_count > 1, because other threads
	may have prebuilt->table pointing to the table. However, these
	other threads should be between statements, waiting for the
	next statement to execute, or for a meta-data lock. */
	ut_ad(table->n_ref_count >= 1);

	if (table->drop_aborted) {
		row_merge_drop_indexes(trx, table, TRUE);
	}
}

/********************************************************************//**
Drop any indexes that we were not able to free previously due to
open table handles. */
static __attribute__((nonnull))
void
online_retry_drop_indexes(
/*======================*/
	dict_table_t*	table,		/*!< in/out: table */
	THD*		user_thd)	/*!< in/out: MySQL connection */
{
	if (table->drop_aborted) {
		trx_t*	trx = innobase_trx_allocate(user_thd);

		trx_start_for_ddl(trx, TRX_DICT_OP_INDEX);

		row_mysql_lock_data_dictionary(trx);
		online_retry_drop_indexes_low(table, trx);
		trx_commit_for_mysql(trx);
		row_mysql_unlock_data_dictionary(trx);
		trx_free_for_mysql(trx);
	}

#ifdef UNIV_DEBUG
	mutex_enter(&dict_sys->mutex);
	dict_table_check_for_dup_indexes(table, CHECK_ALL_COMPLETE);
	mutex_exit(&dict_sys->mutex);
	ut_a(!table->drop_aborted);
#endif /* UNIV_DEBUG */
}

/********************************************************************//**
Commit a dictionary transaction and drop any indexes that we were not
able to free previously due to open table handles. */
static __attribute__((nonnull))
void
online_retry_drop_indexes_with_trx(
/*===============================*/
	dict_table_t*	table,	/*!< in/out: table */
	trx_t*		trx)	/*!< in/out: transaction */
{
	ut_ad(trx_state_eq(trx, TRX_STATE_NOT_STARTED));
	ut_ad(trx->dict_operation_lock_mode == RW_X_LATCH);

	/* Now that the dictionary is being locked, check if we can
	drop any incompletely created indexes that may have been left
	behind in rollback_inplace_alter_table() earlier. */
	if (table->drop_aborted) {

		trx->table_id = 0;

		trx_start_for_ddl(trx, TRX_DICT_OP_INDEX);

		online_retry_drop_indexes_low(table, trx);
		trx_commit_for_mysql(trx);
	}
}

/** Determines if InnoDB is dropping a foreign key constraint.
@param foreign		the constraint
@param drop_fk		constraints being dropped
@param n_drop_fk	number of constraints that are being dropped
@return whether the constraint is being dropped */
inline __attribute__((pure, nonnull, warn_unused_result))
bool
innobase_dropping_foreign(
/*======================*/
	const dict_foreign_t*	foreign,
	dict_foreign_t**	drop_fk,
	ulint			n_drop_fk)
{
	while (n_drop_fk--) {
		if (*drop_fk++ == foreign) {
			return(true);
		}
	}

	return(false);
}

/** Determines if an InnoDB FOREIGN KEY constraint depends on a
column that is being dropped or modified to NOT NULL.
@param user_table	InnoDB table as it is before the ALTER operation
@param col_name		Name of the column being altered
@param drop_fk		constraints being dropped
@param n_drop_fk	number of constraints that are being dropped
@param drop		true=drop column, false=set NOT NULL
@retval true		Not allowed (will call my_error())
@retval false		Allowed
*/
static __attribute__((pure, nonnull, warn_unused_result))
bool
innobase_check_foreigns_low(
/*========================*/
	const dict_table_t*	user_table,
	dict_foreign_t**	drop_fk,
	ulint			n_drop_fk,
	const char*		col_name,
	bool			drop)
{
	ut_ad(mutex_own(&dict_sys->mutex));

	/* Check if any FOREIGN KEY constraints are defined on this
	column. */
	for (const dict_foreign_t* foreign = UT_LIST_GET_FIRST(
		     user_table->foreign_list);
	     foreign;
	     foreign = UT_LIST_GET_NEXT(foreign_list, foreign)) {
		if (!drop && !(foreign->type
			       & (DICT_FOREIGN_ON_DELETE_SET_NULL
				  | DICT_FOREIGN_ON_UPDATE_SET_NULL))) {
			continue;
		}

		if (innobase_dropping_foreign(foreign, drop_fk, n_drop_fk)) {
			continue;
		}

		for (unsigned f = 0; f < foreign->n_fields; f++) {
			if (!strcmp(foreign->foreign_col_names[f],
				    col_name)) {
				my_error(drop
					 ? ER_FK_COLUMN_CANNOT_DROP
					 : ER_FK_COLUMN_NOT_NULL, MYF(0),
					 col_name, foreign->id);
				return(true);
			}
		}
	}

	if (!drop) {
		/* SET NULL clauses on foreign key constraints of
		child tables affect the child tables, not the parent table.
		The column can be NOT NULL in the parent table. */
		return(false);
	}

	/* Check if any FOREIGN KEY constraints in other tables are
	referring to the column that is being dropped. */
	for (const dict_foreign_t* foreign = UT_LIST_GET_FIRST(
		     user_table->referenced_list);
	     foreign;
	     foreign = UT_LIST_GET_NEXT(referenced_list, foreign)) {
		if (innobase_dropping_foreign(foreign, drop_fk, n_drop_fk)) {
			continue;
		}

		for (unsigned f = 0; f < foreign->n_fields; f++) {
			char display_name[FN_REFLEN];

			if (strcmp(foreign->referenced_col_names[f],
				   col_name)) {
				continue;
			}

			char* buf_end = innobase_convert_name(
				display_name, (sizeof display_name) - 1,
				foreign->foreign_table_name,
				strlen(foreign->foreign_table_name),
				NULL, TRUE);
			*buf_end = '\0';
			my_error(ER_FK_COLUMN_CANNOT_DROP_CHILD,
				 MYF(0), col_name, foreign->id,
				 display_name);

			return(true);
		}
	}

	return(false);
}

/** Determines if an InnoDB FOREIGN KEY constraint depends on a
column that is being dropped or modified to NOT NULL.
@param ha_alter_info	Data used during in-place alter
@param altered_table	MySQL table that is being altered
@param old_table	MySQL table as it is before the ALTER operation
@param user_table	InnoDB table as it is before the ALTER operation
@param drop_fk		constraints being dropped
@param n_drop_fk	number of constraints that are being dropped
@retval true		Not allowed (will call my_error())
@retval false		Allowed
*/
static __attribute__((pure, nonnull, warn_unused_result))
bool
innobase_check_foreigns(
/*====================*/
	Alter_inplace_info*	ha_alter_info,
	const TABLE*		altered_table,
	const TABLE*		old_table,
	const dict_table_t*	user_table,
	dict_foreign_t**	drop_fk,
	ulint			n_drop_fk)
{
	List_iterator_fast<Create_field> cf_it(
		ha_alter_info->alter_info->create_list);

	for (Field** fp = old_table->field; *fp; fp++) {
		cf_it.rewind();
		const Create_field* new_field;

		ut_ad(!(*fp)->real_maybe_null()
		      == !!((*fp)->flags & NOT_NULL_FLAG));

		while ((new_field = cf_it++)) {
			if (new_field->field == *fp) {
				break;
			}
		}

		if (!new_field || (new_field->flags & NOT_NULL_FLAG)) {
			if (innobase_check_foreigns_low(
				    user_table, drop_fk, n_drop_fk,
				    (*fp)->field_name, !new_field)) {
				return(true);
			}
		}
	}

	return(false);
}

/** Convert a default value for ADD COLUMN.

@param heap	Memory heap where allocated
@param dfield	InnoDB data field to copy to
@param field	MySQL value for the column
@param comp	nonzero if in compact format */
static __attribute__((nonnull))
void
innobase_build_col_map_add(
/*=======================*/
	mem_heap_t*	heap,
	dfield_t*	dfield,
	const Field*	field,
	ulint		comp)
{
	if (field->is_real_null()) {
		dfield_set_null(dfield);
		return;
	}

	ulint	size	= field->pack_length();

	byte*	buf	= static_cast<byte*>(mem_heap_alloc(heap, size));

	row_mysql_store_col_in_innobase_format(
		dfield, buf, TRUE, field->ptr, size, comp);
}

/** Construct the translation table for reordering, dropping or
adding columns.

@param ha_alter_info	Data used during in-place alter
@param altered_table	MySQL table that is being altered
@param table		MySQL table as it is before the ALTER operation
@param new_table	InnoDB table corresponding to MySQL altered_table
@param old_table	InnoDB table corresponding to MYSQL table
@param add_cols		Default values for ADD COLUMN, or NULL if no ADD COLUMN
@param heap		Memory heap where allocated
@return	array of integers, mapping column numbers in the table
to column numbers in altered_table */
static __attribute__((nonnull(1,2,3,4,5,7), warn_unused_result))
const ulint*
innobase_build_col_map(
/*===================*/
	Alter_inplace_info*	ha_alter_info,
	const TABLE*		altered_table,
	const TABLE*		table,
	const dict_table_t*	new_table,
	const dict_table_t*	old_table,
	dtuple_t*		add_cols,
	mem_heap_t*		heap)
{
	DBUG_ENTER("innobase_build_col_map");
	DBUG_ASSERT(altered_table != table);
	DBUG_ASSERT(new_table != old_table);
	DBUG_ASSERT(dict_table_get_n_cols(new_table)
		    >= altered_table->s->fields + DATA_N_SYS_COLS);
	DBUG_ASSERT(dict_table_get_n_cols(old_table)
		    >= table->s->fields + DATA_N_SYS_COLS);
	DBUG_ASSERT(!!add_cols == !!(ha_alter_info->handler_flags
				     & Alter_inplace_info::ADD_COLUMN));
	DBUG_ASSERT(!add_cols || dtuple_get_n_fields(add_cols)
		    == dict_table_get_n_cols(new_table));

	ulint*	col_map = static_cast<ulint*>(
		mem_heap_alloc(heap, old_table->n_cols * sizeof *col_map));

	List_iterator_fast<Create_field> cf_it(
		ha_alter_info->alter_info->create_list);
	uint i = 0;

	/* Any dropped columns will map to ULINT_UNDEFINED. */
	for (uint old_i = 0; old_i + DATA_N_SYS_COLS < old_table->n_cols;
	     old_i++) {
		col_map[old_i] = ULINT_UNDEFINED;
	}

	while (const Create_field* new_field = cf_it++) {
		for (uint old_i = 0; table->field[old_i]; old_i++) {
			const Field* field = table->field[old_i];
			if (new_field->field == field) {
				col_map[old_i] = i;
				goto found_col;
			}
		}

		innobase_build_col_map_add(
			heap, dtuple_get_nth_field(add_cols, i),
			altered_table->s->field[i],
			dict_table_is_comp(new_table));
found_col:
		i++;
	}

	DBUG_ASSERT(i == altered_table->s->fields);

	i = table->s->fields;

	/* Add the InnoDB hidden FTS_DOC_ID column, if any. */
	if (i + DATA_N_SYS_COLS < old_table->n_cols) {
		/* There should be exactly one extra field,
		the FTS_DOC_ID. */
		DBUG_ASSERT(DICT_TF2_FLAG_IS_SET(old_table,
						 DICT_TF2_FTS_HAS_DOC_ID));
		DBUG_ASSERT(i + DATA_N_SYS_COLS + 1 == old_table->n_cols);
		DBUG_ASSERT(!strcmp(dict_table_get_col_name(
					    old_table, table->s->fields),
				    FTS_DOC_ID_COL_NAME));
		if (altered_table->s->fields + DATA_N_SYS_COLS
		    < new_table->n_cols) {
			DBUG_ASSERT(DICT_TF2_FLAG_IS_SET(
					    new_table,
					    DICT_TF2_FTS_HAS_DOC_ID));
			DBUG_ASSERT(altered_table->s->fields
				    + DATA_N_SYS_COLS + 1
				    == new_table->n_cols);
			col_map[i] = altered_table->s->fields;
		} else {
			DBUG_ASSERT(!DICT_TF2_FLAG_IS_SET(
					    new_table,
					    DICT_TF2_FTS_HAS_DOC_ID));
			col_map[i] = ULINT_UNDEFINED;
		}

		i++;
	} else {
		DBUG_ASSERT(!DICT_TF2_FLAG_IS_SET(
				    old_table,
				    DICT_TF2_FTS_HAS_DOC_ID));
	}

	for (; i < old_table->n_cols; i++) {
		col_map[i] = i + new_table->n_cols - old_table->n_cols;
	}

	DBUG_RETURN(col_map);
}

/** Drop newly create FTS index related auxiliary table during
FIC create index process, before fts_add_index is called
@param table    table that was being rebuilt online
@param trx	transaction
@return		DB_SUCCESS if successful, otherwise last error code
*/
static
dberr_t
innobase_drop_fts_index_table(
/*==========================*/
        dict_table_t*   table,
	trx_t*		trx)
{
	dberr_t		ret_err = DB_SUCCESS;

	for (dict_index_t* index = dict_table_get_first_index(table);
	     index != NULL;
	     index = dict_table_get_next_index(index)) {
		if (index->type & DICT_FTS) {
			dberr_t	err;

			err = fts_drop_index_tables(trx, index);

			if (err != DB_SUCCESS) {
				ret_err = err;
			}
		}
	}

	return(ret_err);
}

/** Update internal structures with concurrent writes blocked,
while preparing ALTER TABLE.

@param ha_alter_info	Data used during in-place alter
@param altered_table	MySQL table that is being altered
@param old_table	MySQL table as it is before the ALTER operation
@param user_table	InnoDB table that is being altered
@param user_trx		User transaction, for locking the table
@param table_name	Table name in MySQL
@param flags		Table and tablespace flags
@param flags2		Additional table flags
@param heap		Memory heap, or NULL
@param drop_index	Indexes to be dropped, or NULL
@param n_drop_index	Number of indexes to drop
@param drop_foreign	Foreign key constraints to be dropped, or NULL
@param n_drop_foreign	Number of foreign key constraints to drop
@param fts_doc_id_col	The column number of FTS_DOC_ID
@param add_autoinc_col	The number of an added AUTO_INCREMENT column,
			or ULINT_UNDEFINED if none was added
@param add_fts_doc_id	Flag: add column FTS_DOC_ID?
@param add_fts_doc_id_idx Flag: add index (FTS_DOC_ID)?

@retval true		Failure
@retval false		Success
*/
static __attribute__((warn_unused_result, nonnull(1,2,3,4)))
bool
prepare_inplace_alter_table_dict(
/*=============================*/
	Alter_inplace_info*	ha_alter_info,
	const TABLE*		altered_table,
	const TABLE*		old_table,
	dict_table_t*		user_table,
	trx_t*			user_trx,
	const char*		table_name,
	ulint			flags,
	ulint			flags2,
	mem_heap_t*		heap,
	dict_index_t**		drop_index,
	ulint			n_drop_index,
	dict_foreign_t**	drop_foreign,
	ulint			n_drop_foreign,
	dict_foreign_t**	add_foreign,
	ulint			n_add_foreign,
	ulint			fts_doc_id_col,
	ulint			add_autoinc_col,
	ulonglong		autoinc_col_max_value,
	bool			add_fts_doc_id,
	bool			add_fts_doc_id_idx)
{
	trx_t*			trx;
	bool			dict_locked	= false;
	dict_index_t**		add_index;	/* indexes to be created */
	ulint*			add_key_nums;	/* MySQL key numbers */
	ulint			n_add_index;
	index_def_t*		index_defs;	/* index definitions */
	dict_index_t*		fts_index	= NULL;
	dict_table_t*		indexed_table	= user_table;
	ulint			new_clustered	= 0;
	dberr_t			error;
	THD*			user_thd	= user_trx->mysql_thd;
	const ulint*		col_map		= NULL;
	dtuple_t*		add_cols	= NULL;
	ulint			num_fts_index;

	DBUG_ENTER("prepare_inplace_alter_table_dict");
	DBUG_ASSERT((add_autoinc_col != ULINT_UNDEFINED)
		    == (autoinc_col_max_value > 0));
	DBUG_ASSERT(!n_drop_index == !drop_index);
	DBUG_ASSERT(!n_drop_foreign == !drop_foreign);
	DBUG_ASSERT(!add_fts_doc_id || add_fts_doc_id_idx);
	DBUG_ASSERT(!add_fts_doc_id_idx
		    || innobase_fulltext_exist(altered_table->s));

	trx_start_if_not_started_xa(user_trx);

	/* Create a background transaction for the operations on
	the data dictionary tables. */
	trx = innobase_trx_allocate(user_thd);

	trx_start_for_ddl(trx, TRX_DICT_OP_INDEX);

	if (!heap) {
		heap = mem_heap_create(1024);
	}

	/* Create table containing all indexes to be built in this
	ALTER TABLE ADD INDEX so that they are in the correct order
	in the table. */

	n_add_index = ha_alter_info->index_add_count;

	index_defs = innobase_create_key_defs(
		heap, ha_alter_info, altered_table, n_add_index,
		num_fts_index, row_table_got_default_clust_index(indexed_table),
		fts_doc_id_col, add_fts_doc_id, add_fts_doc_id_idx);

	new_clustered = DICT_CLUSTERED & index_defs[0].ind_type;

	const bool locked =
		!ha_alter_info->online
		|| add_autoinc_col != ULINT_UNDEFINED
		|| num_fts_index > 0
		|| (innobase_need_rebuild(ha_alter_info)
		    && innobase_fulltext_exist(altered_table->s));

	if (num_fts_index > 1) {
		my_error(ER_INNODB_FT_LIMIT, MYF(0));
		goto error_handled;
	}

	if (locked && ha_alter_info->online) {
		/* This should have been blocked in
		check_if_supported_inplace_alter(). */
		ut_ad(0);
		my_error(ER_NOT_SUPPORTED_YET, MYF(0),
			 thd_query_string(user_thd)->str);
		goto error_handled;
	}

	/* The primary index would be rebuilt if a FTS Doc ID
	column is to be added, and the primary index definition
	is just copied from old table and stored in indexdefs[0] */
	DBUG_ASSERT(!add_fts_doc_id || new_clustered);
	DBUG_ASSERT(!!new_clustered ==
		    (innobase_need_rebuild(ha_alter_info)
		     || add_fts_doc_id));

	/* Allocate memory for dictionary index definitions */

	add_index = (dict_index_t**) mem_heap_alloc(
		heap, n_add_index * sizeof *add_index);
	add_key_nums = (ulint*) mem_heap_alloc(
		heap, n_add_index * sizeof *add_key_nums);

	/* This transaction should be dictionary operation, so that
	the data dictionary will be locked during crash recovery. */

	ut_ad(trx->dict_operation == TRX_DICT_OP_INDEX);

	/* Acquire a lock on the table before creating any indexes. */

	if (locked) {
		error = row_merge_lock_table(
			user_trx, indexed_table, LOCK_S);

		if (error != DB_SUCCESS) {

			goto error_handling;
		}
	} else {
		error = DB_SUCCESS;
	}

	/* Latch the InnoDB data dictionary exclusively so that no deadlocks
	or lock waits can happen in it during an index create operation. */

	row_mysql_lock_data_dictionary(trx);
	dict_locked = true;

	/* Wait for background stats processing to stop using the table that
	we are going to alter. We know bg stats will not start using it again
	until we are holding the data dict locked and we are holding it here
	at least until checking ut_ad(user_table->n_ref_count == 1) below.
	XXX what may happen if bg stats opens the table after we
	have unlocked data dictionary below? */
	dict_stats_wait_bg_to_stop_using_tables(user_table, NULL, trx);

	online_retry_drop_indexes_low(indexed_table, trx);

	ut_d(dict_table_check_for_dup_indexes(
		     indexed_table, CHECK_ABORTED_OK));

	/* If a new clustered index is defined for the table we need
	to drop the original table and rebuild all indexes. */

	if (new_clustered) {
		char*	new_table_name = dict_mem_create_temporary_tablename(
			heap, indexed_table->name, indexed_table->id);
		ulint	n_cols;

		if (innobase_check_foreigns(
			    ha_alter_info, altered_table, old_table,
			    user_table, drop_foreign, n_drop_foreign)) {
			goto new_clustered_failed;
		}

		n_cols = altered_table->s->fields;

		if (add_fts_doc_id) {
			n_cols++;
			DBUG_ASSERT(flags2 & DICT_TF2_FTS);
			DBUG_ASSERT(add_fts_doc_id_idx);
			flags2 |= DICT_TF2_FTS_ADD_DOC_ID
				| DICT_TF2_FTS_HAS_DOC_ID
				| DICT_TF2_FTS;
		}

		DBUG_ASSERT(!add_fts_doc_id_idx || (flags2 & DICT_TF2_FTS));

		/* Create the table. */
		trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);

		if (dict_table_get_low(new_table_name)) {
			my_error(ER_TABLE_EXISTS_ERROR, MYF(0),
				 new_table_name);
			goto new_clustered_failed;
		}

		/* The initial space id 0 may be overridden later. */
		indexed_table = dict_mem_table_create(
			new_table_name, 0, n_cols, flags, flags2);

		if (DICT_TF_HAS_DATA_DIR(flags)) {
			indexed_table->data_dir_path =
				mem_heap_strdup(indexed_table->heap,
				user_table->data_dir_path);
		}

		for (uint i = 0; i < altered_table->s->fields; i++) {
			const Field*	field = altered_table->field[i];
			ulint		is_unsigned;
			ulint		field_type
				= (ulint) field->type();
			ulint		col_type
				= get_innobase_type_from_mysql_type(
					&is_unsigned, field);
			ulint		charset_no;
			ulint		col_len;

			/* we assume in dtype_form_prtype() that this
			fits in two bytes */
			ut_a(field_type <= MAX_CHAR_COLL_NUM);

			if (!field->real_maybe_null()) {
				field_type |= DATA_NOT_NULL;
			}

			if (field->binary()) {
				field_type |= DATA_BINARY_TYPE;
			}

			if (is_unsigned) {
				field_type |= DATA_UNSIGNED;
			}

			if (dtype_is_string_type(col_type)) {
				charset_no = (ulint) field->charset()->number;

				if (charset_no > MAX_CHAR_COLL_NUM) {
					dict_mem_table_free(indexed_table);
					my_error(ER_WRONG_KEY_COLUMN, MYF(0),
						 field->field_name);
					goto new_clustered_failed;
				}
			} else {
				charset_no = 0;
			}

			col_len = field->pack_length();

			/* The MySQL pack length contains 1 or 2 bytes
			length field for a true VARCHAR. Let us
			subtract that, so that the InnoDB column
			length in the InnoDB data dictionary is the
			real maximum byte length of the actual data. */

			if (field->type() == MYSQL_TYPE_VARCHAR) {
				uint32	length_bytes
					= static_cast<const Field_varstring*>(
						field)->length_bytes;

				col_len -= length_bytes;

				if (length_bytes == 2) {
					field_type |= DATA_LONG_TRUE_VARCHAR;
				}
			}

			if (dict_col_name_is_reserved(field->field_name)) {
				dict_mem_table_free(indexed_table);
				my_error(ER_WRONG_COLUMN_NAME, MYF(0),
					 field->field_name);
				goto new_clustered_failed;
			}

			dict_mem_table_add_col(
				indexed_table, heap,
				field->field_name,
				col_type,
				dtype_form_prtype(field_type, charset_no),
				col_len);
		}

		if (add_fts_doc_id) {
			fts_add_doc_id_column(indexed_table, heap);
			indexed_table->fts->doc_col = fts_doc_id_col;
			ut_ad(fts_doc_id_col == altered_table->s->fields);
		} else if (indexed_table->fts) {
			indexed_table->fts->doc_col = fts_doc_id_col;
		}

		error = row_create_table_for_mysql(indexed_table, trx, false);

		switch (error) {
			dict_table_t*	temp_table;
		case DB_SUCCESS:
			/* We need to bump up the table ref count and
			before we can use it we need to open the
			table. The new_table must be in the data
			dictionary cache, because we are still holding
			the dict_sys->mutex. */
			ut_ad(mutex_own(&dict_sys->mutex));
			temp_table = dict_table_open_on_name(
				indexed_table->name, TRUE, FALSE,
				DICT_ERR_IGNORE_NONE);
			ut_a(indexed_table == temp_table);
			/* n_ref_count must be 1, because purge cannot
			be executing on this very table as we are
			holding dict_operation_lock X-latch. */
			DBUG_ASSERT(indexed_table->n_ref_count == 1);
			break;
		case DB_TABLESPACE_EXISTS:
			my_error(ER_TABLESPACE_EXISTS, MYF(0),
				 new_table_name);
			goto new_clustered_failed;
		case DB_DUPLICATE_KEY:
			my_error(HA_ERR_TABLE_EXIST, MYF(0),
				 altered_table->s->table_name.str);
			goto new_clustered_failed;
		default:
			my_error_innodb(error, table_name, flags);
		new_clustered_failed:
			DBUG_ASSERT(trx != user_trx);
			trx_rollback_to_savepoint(trx, NULL);

			ut_ad(user_table->n_ref_count == 1);

			online_retry_drop_indexes_with_trx(user_table, trx);

			goto err_exit;
		}

		if (ha_alter_info->handler_flags
		    & Alter_inplace_info::ADD_COLUMN) {

			add_cols = dtuple_create(
				heap, dict_table_get_n_cols(indexed_table));

			dict_table_copy_types(add_cols, indexed_table);
		}

		col_map = innobase_build_col_map(
			ha_alter_info, altered_table, old_table,
			indexed_table, user_table,
			add_cols, heap);
	} else {
		DBUG_ASSERT(!innobase_need_rebuild(ha_alter_info));

		if (!indexed_table->fts
		    && innobase_fulltext_exist(altered_table->s)) {
			indexed_table->fts = fts_create(indexed_table);
			indexed_table->fts->doc_col = fts_doc_id_col;
		}
	}

	/* Assign table_id, so that no table id of
	fts_create_index_tables() will be written to the undo logs. */
	DBUG_ASSERT(indexed_table->id != 0);
	trx->table_id = indexed_table->id;

	/* Create the indexes in SYS_INDEXES and load into dictionary. */

	for (ulint num_created = 0; num_created < n_add_index; num_created++) {

		add_index[num_created] = row_merge_create_index(
			trx, indexed_table, &index_defs[num_created]);

		add_key_nums[num_created] = index_defs[num_created].key_number;

		if (!add_index[num_created]) {
			error = trx->error_state;
			DBUG_ASSERT(error != DB_SUCCESS);
			goto error_handling;
		}

		if (add_index[num_created]->type & DICT_FTS) {
			DBUG_ASSERT(num_fts_index);
			DBUG_ASSERT(!fts_index);
			DBUG_ASSERT(add_index[num_created]->type == DICT_FTS);
			fts_index = add_index[num_created];
		}

		/* If only online ALTER TABLE operations have been
		requested, allocate a modification log. If the table
		will be locked anyway, the modification
		log is unnecessary. When rebuilding the table
		(new_clustered), we will allocate the log for the
		clustered index of the old table, later. */
		if (new_clustered
		    || locked
		    || user_table->ibd_file_missing
		    || dict_table_is_discarded(user_table)) {
			/* No need to allocate a modification log. */
			ut_ad(!add_index[num_created]->online_log);
		} else if (add_index[num_created]->type & DICT_FTS) {
			/* Fulltext indexes are not covered
			by a modification log. */
		} else {
			DBUG_EXECUTE_IF("innodb_OOM_prepare_inplace_alter",
					error = DB_OUT_OF_MEMORY;
					goto error_handling;);
			rw_lock_x_lock(&add_index[num_created]->lock);
			bool ok = row_log_allocate(add_index[num_created],
						   NULL, true, NULL, NULL);
			rw_lock_x_unlock(&add_index[num_created]->lock);

			if (!ok) {
				error = DB_OUT_OF_MEMORY;
				goto error_handling;
			}
		}
	}

	ut_ad(new_clustered == (indexed_table != user_table));

	DBUG_EXECUTE_IF("innodb_OOM_prepare_inplace_alter",
			error = DB_OUT_OF_MEMORY;
			goto error_handling;);

	if (new_clustered && !locked) {
		/* Allocate a log for online table rebuild. */
		dict_index_t* clust_index = dict_table_get_first_index(
			user_table);

		rw_lock_x_lock(&clust_index->lock);
		bool ok = row_log_allocate(
			clust_index, indexed_table,
			!(ha_alter_info->handler_flags
			  & Alter_inplace_info::ADD_PK_INDEX),
			add_cols, col_map);
		rw_lock_x_unlock(&clust_index->lock);

		if (!ok) {
			error = DB_OUT_OF_MEMORY;
			goto error_handling;
		}

		/* Assign a consistent read view for
		row_merge_read_clustered_index(). */
		trx_assign_read_view(user_trx);
	}

	if (fts_index) {
		/* Ensure that the dictionary operation mode will
		not change while creating the auxiliary tables. */
		trx_dict_op_t	op = trx_get_dict_operation(trx);

#ifdef UNIV_DEBUG
		switch (op) {
		case TRX_DICT_OP_NONE:
			break;
		case TRX_DICT_OP_TABLE:
		case TRX_DICT_OP_INDEX:
			goto op_ok;
		}
		ut_error;
op_ok:
#endif /* UNIV_DEBUG */
		ut_ad(trx->dict_operation_lock_mode == RW_X_LATCH);
		ut_ad(mutex_own(&dict_sys->mutex));
#ifdef UNIV_SYNC_DEBUG
		ut_ad(rw_lock_own(&dict_operation_lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

		DICT_TF2_FLAG_SET(indexed_table, DICT_TF2_FTS);

		/* This function will commit the transaction and reset
		the trx_t::dict_operation flag on success. */

		error = fts_create_index_tables(trx, fts_index);

		DBUG_EXECUTE_IF("innodb_test_fail_after_fts_index_table",
				error = DB_LOCK_WAIT_TIMEOUT;
				goto error_handling;);

		if (error != DB_SUCCESS) {
			goto error_handling;
		}

		trx_start_for_ddl(trx, op);

		if (!indexed_table->fts
		    || ib_vector_size(indexed_table->fts->indexes) == 0) {
			error = fts_create_common_tables(
				trx, indexed_table, user_table->name, TRUE);

			DBUG_EXECUTE_IF("innodb_test_fail_after_fts_common_table",
					error = DB_LOCK_WAIT_TIMEOUT;
					goto error_handling;);

			if (error != DB_SUCCESS) {
				goto error_handling;
			}

			indexed_table->fts->fts_status |= TABLE_DICT_LOCKED;

			error = innobase_fts_load_stopword(
				indexed_table, trx, user_thd)
				? DB_SUCCESS : DB_ERROR;
			indexed_table->fts->fts_status &= ~TABLE_DICT_LOCKED;

			if (error != DB_SUCCESS) {
				goto error_handling;
			}
		}

		ut_ad(trx_get_dict_operation(trx) == op);
	}

	DBUG_ASSERT(error == DB_SUCCESS);

	/* Commit the data dictionary transaction in order to release
	the table locks on the system tables.  This means that if
	MySQL crashes while creating a new primary key inside
	row_merge_build_indexes(), indexed_table will not be dropped
	by trx_rollback_active().  It will have to be recovered or
	dropped by the database administrator. */
	trx_commit_for_mysql(trx);

	row_mysql_unlock_data_dictionary(trx);
	dict_locked = false;

	ut_a(trx->lock.n_active_thrs == 0);

error_handling:
	/* After an error, remove all those index definitions from the
	dictionary which were defined. */

	switch (error) {
	case DB_SUCCESS:
		ut_a(!dict_locked);

		ut_d(mutex_enter(&dict_sys->mutex));
		ut_d(dict_table_check_for_dup_indexes(
			     user_table, CHECK_PARTIAL_OK));
		ut_d(mutex_exit(&dict_sys->mutex));
		ha_alter_info->handler_ctx = new ha_innobase_inplace_ctx(
			user_trx, add_index, add_key_nums, n_add_index,
			drop_index, n_drop_index,
			drop_foreign, n_drop_foreign,
			add_foreign, n_add_foreign,
			!locked, heap, trx, indexed_table, col_map,
			add_autoinc_col,
			ha_alter_info->create_info->auto_increment_value,
			autoinc_col_max_value,
			add_cols);
		DBUG_RETURN(false);
	case DB_TABLESPACE_EXISTS:
		my_error(ER_TABLESPACE_EXISTS, MYF(0), "(unknown)");
		break;
	case DB_DUPLICATE_KEY:
		my_error(ER_DUP_KEY, MYF(0), "SYS_INDEXES");
		break;
	default:
		my_error_innodb(error, table_name, user_table->flags);
	}

error_handled:

	user_trx->error_info = NULL;
	trx->error_state = DB_SUCCESS;

	if (!dict_locked) {
		row_mysql_lock_data_dictionary(trx);
	}

	if (new_clustered) {
		if (indexed_table != user_table) {

			if (DICT_TF2_FLAG_IS_SET(indexed_table, DICT_TF2_FTS)) {
				innobase_drop_fts_index_table(
					indexed_table, trx);
			}

			dict_table_close(indexed_table, TRUE, FALSE);

#ifdef UNIV_DDL_DEBUG
			/* Nobody should have initialized the stats of the
			newly created table yet. When this is the case, we
			know that it has not been added for background stats
			gathering. */
			ut_a(!indexed_table->stat_initialized);
#endif /* UNIV_DDL_DEBUG */

			row_merge_drop_table(trx, indexed_table);

			/* Free the log for online table rebuild, if
			one was allocated. */

			dict_index_t* clust_index = dict_table_get_first_index(
				user_table);

			rw_lock_x_lock(&clust_index->lock);

			if (clust_index->online_log) {
				ut_ad(!locked);
				row_log_abort_sec(clust_index);
				clust_index->online_status
					= ONLINE_INDEX_COMPLETE;
			}

			rw_lock_x_unlock(&clust_index->lock);
		}

		trx_commit_for_mysql(trx);
		/* n_ref_count must be 1, because purge cannot
		be executing on this very table as we are
		holding dict_operation_lock X-latch. */
		DBUG_ASSERT(user_table->n_ref_count == 1 || !locked);

		online_retry_drop_indexes_with_trx(user_table, trx);
	} else {
		ut_ad(indexed_table == user_table);
		row_merge_drop_indexes(trx, user_table, TRUE);
		trx_commit_for_mysql(trx);
	}

	ut_d(dict_table_check_for_dup_indexes(user_table, CHECK_ALL_COMPLETE));
	ut_ad(!user_table->drop_aborted);

err_exit:
	/* Clear the to_be_dropped flag in the data dictionary cache. */
	for (ulint i = 0; i < n_drop_index; i++) {
		DBUG_ASSERT(*drop_index[i]->name != TEMP_INDEX_PREFIX);
		DBUG_ASSERT(drop_index[i]->to_be_dropped);
		drop_index[i]->to_be_dropped = 0;
	}

	row_mysql_unlock_data_dictionary(trx);

	trx_free_for_mysql(trx);
	mem_heap_free(heap);

	trx_commit_for_mysql(user_trx);

	/* There might be work for utility threads.*/
	srv_active_wake_master_thread();

	DBUG_RETURN(true);
}

/* Check whether an index is needed for the foreign key constraint.
If so, if it is dropped, is there an equivalent index can play its role.
@return true if the index is needed and can't be dropped */
static __attribute__((warn_unused_result))
bool
innobase_check_foreign_key_index(
/*=============================*/
	Alter_inplace_info*	ha_alter_info,	/*!< in: Structure describing
						changes to be done by ALTER
						TABLE */
	dict_index_t*		index,		/*!< in: index to check */
	dict_table_t*		indexed_table,	/*!< in: table that owns the
						foreign keys */
	trx_t*			trx,		/*!< in/out: transaction */
	dict_foreign_t**	drop_fk,	/*!< in: Foreign key constraints
						to drop */
	ulint			n_drop_fk)	/*!< in: Number of foreign keys
						to drop */
{
	dict_foreign_t*	foreign;

	ut_ad(!index->to_be_dropped);

	/* Check if the index is referenced. */
	foreign = dict_table_get_referenced_constraint(indexed_table, index);

	ut_ad(!foreign || indexed_table
	      == foreign->referenced_table);

	if (foreign
	    && !dict_foreign_find_index(
		    indexed_table,
		    foreign->referenced_col_names,
		    foreign->n_fields, index,
		    /*check_charsets=*/TRUE,
		    /*check_null=*/FALSE)
	    && !innobase_find_equiv_index(
		    foreign->referenced_col_names,
		    foreign->n_fields,
		    ha_alter_info->key_info_buffer,
		    ha_alter_info->index_add_buffer,
		    ha_alter_info->index_add_count)
	    ) {
		trx->error_info = index;
		return(true);
	}

	/* Check if this index references some
	other table */
	foreign = dict_table_get_foreign_constraint(
		indexed_table, index);

	ut_ad(!foreign || indexed_table
	      == foreign->foreign_table);

	if (foreign
	    && !innobase_dropping_foreign(
		    foreign, drop_fk, n_drop_fk)
	    && !dict_foreign_find_index(
		    indexed_table,
		    foreign->foreign_col_names,
		    foreign->n_fields, index,
		    /*check_charsets=*/TRUE,
		    /*check_null=*/FALSE)
	    && !innobase_find_equiv_index(
		    foreign->foreign_col_names,
		    foreign->n_fields,
		    ha_alter_info->key_info_buffer,
		    ha_alter_info->index_add_buffer,
		    ha_alter_info->index_add_count)
	    ) {
		trx->error_info = index;
		return(true);
	}

	return(false);
}

/** Allows InnoDB to update internal structures with concurrent
writes blocked (provided that check_if_supported_inplace_alter()
did not return HA_ALTER_INPLACE_NO_LOCK).
This will be invoked before inplace_alter_table().

@param altered_table	TABLE object for new version of table.
@param ha_alter_info	Structure describing changes to be done
by ALTER TABLE and holding data used during in-place alter.

@retval true		Failure
@retval false		Success
*/
UNIV_INTERN
bool
ha_innobase::prepare_inplace_alter_table(
/*=====================================*/
	TABLE*			altered_table,
	Alter_inplace_info*	ha_alter_info)
{
	dict_index_t**	drop_index;	/*!< Index to be dropped */
	ulint		n_drop_index;	/*!< Number of indexes to drop */
	dict_foreign_t**drop_fk;	/*!< Foreign key constraints to drop */
	ulint		n_drop_fk;	/*!< Number of foreign keys to drop */
	dict_foreign_t**add_fk = NULL;	/*!< Foreign key constraints to drop */
	ulint		n_add_fk;	/*!< Number of foreign keys to drop */
	dict_table_t*	indexed_table;	/*!< Table where indexes are created */
	mem_heap_t*     heap;
	int		error;
	ulint		flags;
	ulint		flags2;
	ulint		max_col_len;
	ulint		add_autoinc_col_no	= ULINT_UNDEFINED;
	ulonglong	autoinc_col_max_value	= 0;
	ulint		fts_doc_col_no		= ULINT_UNDEFINED;
	bool		add_fts_doc_id		= false;
	bool		add_fts_doc_id_idx	= false;

	DBUG_ENTER("prepare_inplace_alter_table");
	DBUG_ASSERT(!ha_alter_info->handler_ctx);
	DBUG_ASSERT(ha_alter_info->create_info);

	if (srv_read_only_mode) {
		DBUG_RETURN(false);
	}

	MONITOR_ATOMIC_INC(MONITOR_PENDING_ALTER_TABLE);

#ifdef UNIV_DEBUG
	for (dict_index_t* index = dict_table_get_first_index(prebuilt->table);
	     index;
	     index = dict_table_get_next_index(index)) {
		ut_ad(!index->to_be_dropped);
	}
#endif /* UNIV_DEBUG */

	ut_d(mutex_enter(&dict_sys->mutex));
	ut_d(dict_table_check_for_dup_indexes(
		     prebuilt->table, CHECK_ABORTED_OK));
	ut_d(mutex_exit(&dict_sys->mutex));

	if (!(ha_alter_info->handler_flags & ~INNOBASE_INPLACE_IGNORE)) {
		/* Nothing to do */
		goto func_exit;
	}

	if (ha_alter_info->handler_flags
	    == Alter_inplace_info::CHANGE_CREATE_OPTION
	    && !innobase_need_rebuild(ha_alter_info)) {
		goto func_exit;
	}

	if (ha_alter_info->handler_flags
	    & Alter_inplace_info::CHANGE_CREATE_OPTION) {
		if (const char* invalid_opt = create_options_are_invalid(
			    user_thd, altered_table,
			    ha_alter_info->create_info,
			    prebuilt->table->space != 0)) {
			my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0),
				 table_type(), invalid_opt);
			goto err_exit_no_heap;
		}
	}

	/* Check if any index name is reserved. */
	if (innobase_index_name_is_reserved(
		    user_thd,
		    ha_alter_info->key_info_buffer,
		    ha_alter_info->key_count)) {
err_exit_no_heap:
		DBUG_ASSERT(prebuilt->trx->dict_operation_lock_mode == 0);
		if (ha_alter_info->handler_flags & ~INNOBASE_INPLACE_IGNORE) {
			online_retry_drop_indexes(prebuilt->table, user_thd);
		}
		DBUG_RETURN(true);
	}

	indexed_table = prebuilt->table;

	/* Check that index keys are sensible */
	error = innobase_check_index_keys(ha_alter_info, indexed_table);

	if (error) {
		goto err_exit_no_heap;
	}

	/* Prohibit renaming a column to something that the table
	already contains. */
	if (ha_alter_info->handler_flags
	    & Alter_inplace_info::ALTER_COLUMN_NAME) {
		List_iterator_fast<Create_field> cf_it(
			ha_alter_info->alter_info->create_list);

		for (Field** fp = table->field; *fp; fp++) {
			if (!((*fp)->flags & FIELD_IS_RENAMED)) {
				continue;
			}

			const char* name = 0;

			cf_it.rewind();
			while (Create_field* cf = cf_it++) {
				if (cf->field == *fp) {
					name = cf->field_name;
					goto check_if_ok_to_rename;
				}
			}

			ut_error;
check_if_ok_to_rename:
			/* Prohibit renaming a column from FTS_DOC_ID
			if full-text indexes exist. */
			if (!my_strcasecmp(system_charset_info,
					   (*fp)->field_name,
					   FTS_DOC_ID_COL_NAME)
			    && innobase_fulltext_exist(altered_table->s)) {
				my_error(ER_INNODB_FT_WRONG_DOCID_COLUMN,
					 MYF(0), name);
				goto err_exit_no_heap;
			}

			/* Prohibit renaming a column to an internal column. */
			const char*	s = prebuilt->table->col_names;
			unsigned j;
			/* Skip user columns.
			MySQL should have checked these already.
			We want to allow renaming of c1 to c2, c2 to c1. */
			for (j = 0; j < table->s->fields; j++) {
				s += strlen(s) + 1;
			}

			for (; j < prebuilt->table->n_def; j++) {
				if (!my_strcasecmp(
					    system_charset_info, name, s)) {
					my_error(ER_WRONG_COLUMN_NAME, MYF(0),
						 s);
					goto err_exit_no_heap;
				}

				s += strlen(s) + 1;
			}
		}
	}

	if (!innobase_table_flags(altered_table,
				  ha_alter_info->create_info,
				  user_thd,
				  srv_file_per_table
				  || indexed_table->space != 0,
				  &flags, &flags2)) {
		goto err_exit_no_heap;
	}

	max_col_len = DICT_MAX_FIELD_LEN_BY_FORMAT_FLAG(flags);

	/* Check each index's column length to make sure they do not
	exceed limit */
	for (ulint i = 0; i < ha_alter_info->index_add_count; i++) {
		const KEY* key = &ha_alter_info->key_info_buffer[
			ha_alter_info->index_add_buffer[i]];

		if (key->flags & HA_FULLTEXT) {
			/* The column length does not matter for
			fulltext search indexes. But, UNIQUE
			fulltext indexes are not supported. */
			DBUG_ASSERT(!(key->flags & HA_NOSAME));
			DBUG_ASSERT(!(key->flags & HA_KEYFLAG_MASK
				      & ~(HA_FULLTEXT
					  | HA_PACK_KEY
					  | HA_BINARY_PACK_KEY)));
			continue;
		}

		if (innobase_check_column_length(max_col_len, key)) {
			my_error(ER_INDEX_COLUMN_TOO_LONG, MYF(0),
				 max_col_len);
			goto err_exit_no_heap;
		}
	}

	/* Check existing index definitions for too-long column
	prefixes as well, in case max_col_len shrunk. */
	for (const dict_index_t* index
		     = dict_table_get_first_index(indexed_table);
	     index;
	     index = dict_table_get_next_index(index)) {
		if (index->type & DICT_FTS) {
			DBUG_ASSERT(index->type == DICT_FTS
				    || (index->type & DICT_CORRUPT));
			continue;
		}

		for (ulint i = 0; i < dict_index_get_n_fields(index); i++) {
			const dict_field_t* field
				= dict_index_get_nth_field(index, i);
			if (field->prefix_len > max_col_len) {
				my_error(ER_INDEX_COLUMN_TOO_LONG, MYF(0),
					 max_col_len);
				goto err_exit_no_heap;
			}
		}
	}

	n_drop_index = 0;
	n_drop_fk = 0;

	if (ha_alter_info->handler_flags
	    & Alter_inplace_info::DROP_FOREIGN_KEY) {
		DBUG_ASSERT(ha_alter_info->alter_info->drop_list.elements > 0);

		heap = mem_heap_create(1024);

		drop_fk = static_cast<dict_foreign_t**>(
			mem_heap_alloc(
				heap,
				ha_alter_info->alter_info->drop_list.elements
				* sizeof(dict_foreign_t*)));

		List_iterator<Alter_drop> drop_it(
			ha_alter_info->alter_info->drop_list);

		while (Alter_drop* drop = drop_it++) {
			if (drop->type != Alter_drop::FOREIGN_KEY) {
				continue;
			}

			for (dict_foreign_t* foreign = UT_LIST_GET_FIRST(
				     prebuilt->table->foreign_list);
			     foreign != NULL;
			     foreign = UT_LIST_GET_NEXT(
				     foreign_list, foreign)) {
				const char* fid = strchr(foreign->id, '/');

				DBUG_ASSERT(fid);
				/* If no database/ prefix was present in
				the FOREIGN KEY constraint name, compare
				to the full constraint name. */
				fid = fid ? fid + 1 : foreign->id;

				if (!my_strcasecmp(system_charset_info,
						   fid, drop->name)) {
					drop_fk[n_drop_fk++] = foreign;
					goto found_fk;
				}
			}

			my_error(ER_CANT_DROP_FIELD_OR_KEY, MYF(0),
				 drop->name);
			goto err_exit;
found_fk:
			continue;
		}

		DBUG_ASSERT(n_drop_fk > 0);
		DBUG_ASSERT(n_drop_fk
			    == ha_alter_info->alter_info->drop_list.elements);
	} else {
		drop_fk = NULL;
		heap = NULL;
	}

	if (ha_alter_info->index_drop_count) {
		dict_index_t*	drop_primary = NULL;

		DBUG_ASSERT(ha_alter_info->handler_flags
			    & (Alter_inplace_info::DROP_INDEX
			       | Alter_inplace_info::DROP_UNIQUE_INDEX
			       | Alter_inplace_info::DROP_PK_INDEX));
		/* Check which indexes to drop. */
		if (!heap) {
			heap = mem_heap_create(1024);
		}
		drop_index = static_cast<dict_index_t**>(
			mem_heap_alloc(
				heap, (ha_alter_info->index_drop_count + 1)
				* sizeof *drop_index));

		for (uint i = 0; i < ha_alter_info->index_drop_count; i++) {
			const KEY*	key
				= ha_alter_info->index_drop_buffer[i];
			dict_index_t*	index
				= dict_table_get_index_on_name_and_min_id(
					indexed_table, key->name);

			if (!index) {
				push_warning_printf(
					user_thd,
					Sql_condition::SL_WARNING,
					HA_ERR_WRONG_INDEX,
					"InnoDB could not find key "
					"with name %s", key->name);
			} else {
				ut_ad(!index->to_be_dropped);
				if (!dict_index_is_clust(index)) {
					drop_index[n_drop_index++] = index;
				} else {
					drop_primary = index;
				}
			}
		}

		/* If all FULLTEXT indexes were removed, drop an
		internal FTS_DOC_ID_INDEX as well, unless it exists in
		the table. */

		if (innobase_fulltext_exist(table->s)
		    && !innobase_fulltext_exist(altered_table->s)
		    && !DICT_TF2_FLAG_IS_SET(
			indexed_table, DICT_TF2_FTS_HAS_DOC_ID)) {
			dict_index_t*	fts_doc_index
				= dict_table_get_index_on_name(
					indexed_table, FTS_DOC_ID_INDEX_NAME);

			// Add some fault tolerance for non-debug builds.
			if (fts_doc_index == NULL) {
				goto check_if_can_drop_indexes;
			}

			DBUG_ASSERT(!fts_doc_index->to_be_dropped);

			for (uint i = 0; i < table->s->keys; i++) {
				if (!my_strcasecmp(
					    system_charset_info,
					    FTS_DOC_ID_INDEX_NAME,
					    table->s->key_info[i].name)) {
					/* The index exists in the MySQL
					data dictionary. Do not drop it,
					even though it is no longer needed
					by InnoDB fulltext search. */
					goto check_if_can_drop_indexes;
				}
			}

			drop_index[n_drop_index++] = fts_doc_index;
		}

check_if_can_drop_indexes:
		/* Check if the indexes can be dropped. */

		/* Prevent a race condition between DROP INDEX and
		CREATE TABLE adding FOREIGN KEY constraints. */
		row_mysql_lock_data_dictionary(prebuilt->trx);

		if (prebuilt->trx->check_foreigns) {
			for (uint i = 0; i < n_drop_index; i++) {
			     dict_index_t*	index = drop_index[i];

				if (innobase_check_foreign_key_index(
					ha_alter_info, index, indexed_table,
					prebuilt->trx, drop_fk, n_drop_fk)) {
					row_mysql_unlock_data_dictionary(
						prebuilt->trx);
					prebuilt->trx->error_info = index;
					print_error(HA_ERR_DROP_INDEX_FK,
						    MYF(0));
					goto err_exit;
				}
			}

			/* If a primary index is dropped, need to check
			any depending foreign constraints get affected */
			if (drop_primary
			    && innobase_check_foreign_key_index(
				ha_alter_info, drop_primary, indexed_table,
				prebuilt->trx, drop_fk, n_drop_fk)) {
				row_mysql_unlock_data_dictionary(prebuilt->trx);
				print_error(HA_ERR_DROP_INDEX_FK, MYF(0));
				goto err_exit;
			}
		}

		if (!n_drop_index) {
			drop_index = NULL;
		} else {
			/* Flag all indexes that are to be dropped. */
			for (ulint i = 0; i < n_drop_index; i++) {
				ut_ad(!drop_index[i]->to_be_dropped);
				drop_index[i]->to_be_dropped = 1;
			}
		}

		row_mysql_unlock_data_dictionary(prebuilt->trx);
	} else {
		drop_index = NULL;
	}

	n_add_fk = 0;

	if (ha_alter_info->handler_flags
	    & Alter_inplace_info::ADD_FOREIGN_KEY) {
		ut_ad(!prebuilt->trx->check_foreigns);

		if (!heap) {
			heap = mem_heap_create(1024);
		}

		add_fk = static_cast<dict_foreign_t**>(
			mem_heap_zalloc(
				heap,
				ha_alter_info->alter_info->key_list.elements
				* sizeof(dict_foreign_t*)));

		if (!innobase_get_foreign_key_info(
			ha_alter_info, table_share, prebuilt->table,
			add_fk, &n_add_fk, heap, prebuilt->trx)) {
err_exit:
			if (n_drop_index) {
				row_mysql_lock_data_dictionary(prebuilt->trx);

				/* Clear the to_be_dropped flags, which might
				have been set at this point. */
				for (ulint i = 0; i < n_drop_index; i++) {
					DBUG_ASSERT(*drop_index[i]->name
						    != TEMP_INDEX_PREFIX);
					drop_index[i]->to_be_dropped = 0;
				}

				row_mysql_unlock_data_dictionary(prebuilt->trx);
			}

			if (heap) {
				mem_heap_free(heap);
			}
			goto err_exit_no_heap;
		}
	}

	if (!(ha_alter_info->handler_flags & INNOBASE_INPLACE_CREATE)) {
		if (heap) {
			ha_alter_info->handler_ctx
				= new ha_innobase_inplace_ctx(
					prebuilt->trx, 0, 0, 0,
					drop_index, n_drop_index,
					drop_fk, n_drop_fk,
					add_fk, n_add_fk,
					ha_alter_info->online,
					heap, 0, indexed_table, 0,
					ULINT_UNDEFINED, 0, 0, 0);
		}

func_exit:
		DBUG_ASSERT(prebuilt->trx->dict_operation_lock_mode == 0);
		if (ha_alter_info->handler_flags & ~INNOBASE_INPLACE_IGNORE) {
			online_retry_drop_indexes(prebuilt->table, user_thd);
		}
		DBUG_RETURN(false);
	}

	/* If we are to build a full-text search index, check whether
	the table already has a DOC ID column.  If not, we will need to
	add a Doc ID hidden column and rebuild the primary index */
	if (innobase_fulltext_exist(altered_table->s)) {
		ulint	doc_col_no;

		if (!innobase_fts_check_doc_id_col(
			    prebuilt->table, altered_table, &fts_doc_col_no)) {
			fts_doc_col_no = altered_table->s->fields;
			add_fts_doc_id = true;
			add_fts_doc_id_idx = true;

			push_warning_printf(
				user_thd,
				Sql_condition::SL_WARNING,
				HA_ERR_WRONG_INDEX,
				"InnoDB rebuilding table to add column "
				FTS_DOC_ID_COL_NAME);
		} else if (fts_doc_col_no == ULINT_UNDEFINED) {
			goto err_exit;
		}

		switch (innobase_fts_check_doc_id_index(
				prebuilt->table, altered_table, &doc_col_no)) {
		case FTS_NOT_EXIST_DOC_ID_INDEX:
			add_fts_doc_id_idx = true;
			break;
		case FTS_INCORRECT_DOC_ID_INDEX:
			my_error(ER_INNODB_FT_WRONG_DOCID_INDEX, MYF(0),
				 FTS_DOC_ID_INDEX_NAME);
			goto err_exit;
		case FTS_EXIST_DOC_ID_INDEX:
			DBUG_ASSERT(doc_col_no == fts_doc_col_no
				    || doc_col_no == ULINT_UNDEFINED
				    || (ha_alter_info->handler_flags
					& (Alter_inplace_info::ALTER_COLUMN_ORDER
					   | Alter_inplace_info::DROP_COLUMN
					   | Alter_inplace_info::ADD_COLUMN)));
		}
	}

	/* See if an AUTO_INCREMENT column was added. */
	uint i = 0;
	List_iterator_fast<Create_field> cf_it(
		ha_alter_info->alter_info->create_list);
	while (const Create_field* new_field = cf_it++) {
		const Field*	field;

		DBUG_ASSERT(i < altered_table->s->fields);

		for (uint old_i = 0; table->field[old_i]; old_i++) {
			if (new_field->field == table->field[old_i]) {
				goto found_col;
			}
		}

		/* This is an added column. */
		DBUG_ASSERT(!new_field->field);
		DBUG_ASSERT(ha_alter_info->handler_flags
			    & Alter_inplace_info::ADD_COLUMN);

		field = altered_table->field[i];

		DBUG_ASSERT((MTYP_TYPENR(field->unireg_check)
			     == Field::NEXT_NUMBER)
			    == !!(field->flags & AUTO_INCREMENT_FLAG));

		if (field->flags & AUTO_INCREMENT_FLAG) {
			if (add_autoinc_col_no != ULINT_UNDEFINED) {
				/* This should have been blocked earlier. */
				ut_ad(0);
				my_error(ER_WRONG_AUTO_KEY, MYF(0));
				goto err_exit;
			}
			add_autoinc_col_no = i;

			autoinc_col_max_value = innobase_get_int_col_max_value(
				field);
		}
found_col:
		i++;
	}

	DBUG_ASSERT(user_thd == prebuilt->trx->mysql_thd);
	DBUG_RETURN(prepare_inplace_alter_table_dict(
			    ha_alter_info, altered_table, table,
			    prebuilt->table, prebuilt->trx,
			    table_share->table_name.str,
			    flags, flags2,
			    heap, drop_index, n_drop_index,
			    drop_fk, n_drop_fk, add_fk, n_add_fk,
			    fts_doc_col_no, add_autoinc_col_no,
			    autoinc_col_max_value, add_fts_doc_id,
			    add_fts_doc_id_idx));
}

/** Alter the table structure in-place with operations
specified using Alter_inplace_info.
The level of concurrency allowed during this operation depends
on the return value from check_if_supported_inplace_alter().

@param altered_table	TABLE object for new version of table.
@param ha_alter_info	Structure describing changes to be done
by ALTER TABLE and holding data used during in-place alter.

@retval true		Failure
@retval false		Success
*/
UNIV_INTERN
bool
ha_innobase::inplace_alter_table(
/*=============================*/
	TABLE*			altered_table,
	Alter_inplace_info*	ha_alter_info)
{
	dberr_t	error;

	DBUG_ENTER("inplace_alter_table");

	if (srv_read_only_mode) {
		DBUG_RETURN(false);
	}

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!rw_lock_own(&dict_operation_lock, RW_LOCK_EX));
	ut_ad(!rw_lock_own(&dict_operation_lock, RW_LOCK_SHARED));
#endif /* UNIV_SYNC_DEBUG */

	DEBUG_SYNC(user_thd, "innodb_inplace_alter_table_enter");

	if (!(ha_alter_info->handler_flags & INNOBASE_INPLACE_CREATE)) {
ok_exit:
		DEBUG_SYNC(user_thd, "innodb_after_inplace_alter_table");
		DBUG_RETURN(false);
	}

	if (ha_alter_info->handler_flags
	    == Alter_inplace_info::CHANGE_CREATE_OPTION
	    && !innobase_need_rebuild(ha_alter_info)) {
		goto ok_exit;
	}

	ha_innobase_inplace_ctx*	ctx
		= static_cast<ha_innobase_inplace_ctx*>
		(ha_alter_info->handler_ctx);

	DBUG_ASSERT(ctx);
	DBUG_ASSERT(ctx->trx);

	if (prebuilt->table->ibd_file_missing
	    || dict_table_is_discarded(prebuilt->table)) {
		goto all_done;
	}

	/* Read the clustered index of the table and build
	indexes based on this information using temporary
	files and merge sort. */
	DBUG_EXECUTE_IF("innodb_OOM_inplace_alter",
			error = DB_OUT_OF_MEMORY; goto oom;);
	error = row_merge_build_indexes(
		prebuilt->trx,
		prebuilt->table, ctx->indexed_table,
		ctx->online,
		ctx->add, ctx->add_key_numbers, ctx->num_to_add,
		altered_table, ctx->add_cols, ctx->col_map,
		ctx->add_autoinc, ctx->sequence);
#ifndef DBUG_OFF
oom:
#endif /* !DBUG_OFF */
	if (error == DB_SUCCESS && ctx->online
	    && ctx->indexed_table != prebuilt->table) {
		DEBUG_SYNC_C("row_log_table_apply1_before");
		error = row_log_table_apply(
			ctx->thr, prebuilt->table, altered_table);
	}

	DEBUG_SYNC_C("inplace_after_index_build");

	DBUG_EXECUTE_IF("create_index_fail",
			error = DB_DUPLICATE_KEY;);

	/* After an error, remove all those index definitions
	from the dictionary which were defined. */

	switch (error) {
		KEY*	dup_key;
	all_done:
	case DB_SUCCESS:
		ut_d(mutex_enter(&dict_sys->mutex));
		ut_d(dict_table_check_for_dup_indexes(
			     prebuilt->table, CHECK_PARTIAL_OK));
		ut_d(mutex_exit(&dict_sys->mutex));
		/* prebuilt->table->n_ref_count can be anything here,
		given that we hold at most a shared lock on the table. */
		goto ok_exit;
	case DB_DUPLICATE_KEY:
		if (prebuilt->trx->error_key_num == ULINT_UNDEFINED
		    || ha_alter_info->key_count == 0) {
			/* This should be the hidden index on
			FTS_DOC_ID, or there is no PRIMARY KEY in the
			table. Either way, we should be seeing and
			reporting a bogus duplicate key error. */
			dup_key = NULL;
		} else {
			DBUG_ASSERT(prebuilt->trx->error_key_num
				    < ha_alter_info->key_count);
			dup_key = &ha_alter_info->key_info_buffer[
				prebuilt->trx->error_key_num];
		}
		print_keydup_error(altered_table, dup_key, MYF(0));
		break;
	case DB_ONLINE_LOG_TOO_BIG:
		DBUG_ASSERT(ctx->online);
		my_error(ER_INNODB_ONLINE_LOG_TOO_BIG, MYF(0),
			 (prebuilt->trx->error_key_num == ULINT_UNDEFINED)
			 ? FTS_DOC_ID_INDEX_NAME
			 : ha_alter_info->key_info_buffer[
				 prebuilt->trx->error_key_num].name);
		break;
	case DB_INDEX_CORRUPT:
		my_error(ER_INDEX_CORRUPT, MYF(0),
			 (prebuilt->trx->error_key_num == ULINT_UNDEFINED)
			 ? FTS_DOC_ID_INDEX_NAME
			 : ha_alter_info->key_info_buffer[
				 prebuilt->trx->error_key_num].name);
		break;
	default:
		my_error_innodb(error,
				table_share->table_name.str,
				prebuilt->table->flags);
	}

	/* prebuilt->table->n_ref_count can be anything here, given
	that we hold at most a shared lock on the table. */
	prebuilt->trx->error_info = NULL;
	ctx->trx->error_state = DB_SUCCESS;

	DBUG_RETURN(true);
}

/** Free the modification log for online table rebuild.
@param table	table that was being rebuilt online */
static
void
innobase_online_rebuild_log_free(
/*=============================*/
	dict_table_t*	table)
{
	dict_index_t* clust_index = dict_table_get_first_index(table);

	ut_ad(mutex_own(&dict_sys->mutex));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&dict_operation_lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	rw_lock_x_lock(&clust_index->lock);

	if (clust_index->online_log) {
		ut_ad(dict_index_get_online_status(clust_index)
		      == ONLINE_INDEX_CREATION);
		clust_index->online_status = ONLINE_INDEX_COMPLETE;
		row_log_free(clust_index->online_log);
		DEBUG_SYNC_C("innodb_online_rebuild_log_free_aborted");
	}

	DBUG_ASSERT(dict_index_get_online_status(clust_index)
		    == ONLINE_INDEX_COMPLETE);
	rw_lock_x_unlock(&clust_index->lock);
}

/** Rollback a secondary index creation, drop the indexes with
temparary index prefix
@param prebuilt		the prebuilt struct
@param table_share	the TABLE_SHARE
@param trx		the transaction
*/
static
void
innobase_rollback_sec_index(
/*========================*/
	row_prebuilt_t*		prebuilt,
	const TABLE_SHARE*	table_share,
	trx_t*			trx)
{
	row_merge_drop_indexes(trx, prebuilt->table, FALSE);

	/* Free the table->fts only if there is no FTS_DOC_ID
	in the table */
	if (prebuilt->table->fts
	    && !DICT_TF2_FLAG_IS_SET(prebuilt->table,
				     DICT_TF2_FTS_HAS_DOC_ID)
	    && !innobase_fulltext_exist(table_share)) {
		fts_free(prebuilt->table);
	}
}

/** Roll back the changes made during prepare_inplace_alter_table()
and inplace_alter_table() inside the storage engine. Note that the
allowed level of concurrency during this operation will be the same as
for inplace_alter_table() and thus might be higher than during
prepare_inplace_alter_table(). (E.g concurrent writes were blocked
during prepare, but might not be during commit).

@param ha_alter_info	Data used during in-place alter.
@param table_share	the TABLE_SHARE
@param prebuilt		the prebuilt struct
@retval true		Failure
@retval false		Success
*/
inline
bool
rollback_inplace_alter_table(
/*=========================*/
	Alter_inplace_info*	ha_alter_info,
	const TABLE_SHARE*	table_share,
	row_prebuilt_t*		prebuilt)
{
	bool	fail	= false;

	ha_innobase_inplace_ctx*	ctx
		= static_cast<ha_innobase_inplace_ctx*>
		(ha_alter_info->handler_ctx);

	DBUG_ENTER("rollback_inplace_alter_table");

	if (!ctx || !ctx->trx) {
		/* If we have not started a transaction yet,
		(almost) nothing has been or needs to be done. */
		goto func_exit;
	}

	row_mysql_lock_data_dictionary(ctx->trx);

	if (prebuilt->table != ctx->indexed_table) {
		dberr_t	err;
		ulint	flags	= ctx->indexed_table->flags;

		/* DML threads can access ctx->indexed_table via the
		online rebuild log. Free it first. */
		innobase_online_rebuild_log_free(prebuilt->table);

		/* Since the FTS index specific auxiliary tables has
		not yet registered with "table->fts" by fts_add_index(),
		we will need explicitly delete them here */
		if (DICT_TF2_FLAG_IS_SET(ctx->indexed_table, DICT_TF2_FTS)) {

			err = innobase_drop_fts_index_table(
				ctx->indexed_table, ctx->trx);

			if (err != DB_SUCCESS) {
				my_error_innodb(
					err, table_share->table_name.str,
					flags);
				fail = true;
			}
		}

		/* Drop the table. */
		dict_table_close(ctx->indexed_table, TRUE, FALSE);

#ifdef UNIV_DDL_DEBUG
		/* Nobody should have initialized the stats of the
		newly created table yet. When this is the case, we
		know that it has not been added for background stats
		gathering. */
		ut_a(!ctx->indexed_table->stat_initialized);
#endif /* UNIV_DDL_DEBUG */

		err = row_merge_drop_table(ctx->trx, ctx->indexed_table);

		switch (err) {
		case DB_SUCCESS:
			break;
		default:
			my_error_innodb(err, table_share->table_name.str,
					flags);
			fail = true;
		}
	} else {
		DBUG_ASSERT(!(ha_alter_info->handler_flags
			      & Alter_inplace_info::ADD_PK_INDEX));

		trx_start_for_ddl(ctx->trx, TRX_DICT_OP_INDEX);

		innobase_rollback_sec_index(prebuilt, table_share, ctx->trx);
	}

	trx_commit_for_mysql(ctx->trx);
	row_mysql_unlock_data_dictionary(ctx->trx);
	trx_free_for_mysql(ctx->trx);


func_exit:
#ifndef DBUG_OFF
	dict_index_t* clust_index = dict_table_get_first_index(
		prebuilt->table);
	DBUG_ASSERT(!clust_index->online_log);
	DBUG_ASSERT(dict_index_get_online_status(clust_index)
		    == ONLINE_INDEX_COMPLETE);
#endif /* !DBUG_OFF */

	if (ctx) {
		if (ctx->num_to_add_fk) {
			for (ulint i = 0; i < ctx->num_to_add_fk; i++) {
				dict_foreign_free(ctx->add_fk[i]);
			}
		}

		if (ctx->num_to_drop) {
			row_mysql_lock_data_dictionary(prebuilt->trx);

			/* Clear the to_be_dropped flags
			in the data dictionary cache.
			The flags may already have been cleared,
			in case an error was detected in
			commit_inplace_alter_table(). */
			for (ulint i = 0; i < ctx->num_to_drop; i++) {
				dict_index_t*	index = ctx->drop[i];
				DBUG_ASSERT(*index->name != TEMP_INDEX_PREFIX);

				index->to_be_dropped = 0;
			}

			row_mysql_unlock_data_dictionary(prebuilt->trx);
		}
	}

	trx_commit_for_mysql(prebuilt->trx);
	srv_active_wake_master_thread();
	MONITOR_ATOMIC_DEC(MONITOR_PENDING_ALTER_TABLE);
	DBUG_RETURN(fail);
}

/** Drop a FOREIGN KEY constraint.
@param table_share	the TABLE_SHARE
@param trx		data dictionary transaction
@param foreign		the foreign key constraint, will be freed
@retval true		Failure
@retval false		Success */
static __attribute__((nonnull, warn_unused_result))
bool
innobase_drop_foreign(
/*==================*/
	const TABLE_SHARE*	table_share,
	trx_t*			trx,
	dict_foreign_t*		foreign)
{
	DBUG_ENTER("innobase_drop_foreign");

	DBUG_ASSERT(trx_get_dict_operation(trx) == TRX_DICT_OP_INDEX);
	ut_ad(trx->dict_operation_lock_mode == RW_X_LATCH);
	ut_ad(mutex_own(&dict_sys->mutex));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&dict_operation_lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	/* Drop the constraint from the data dictionary. */
	static const char sql[] =
		"PROCEDURE DROP_FOREIGN_PROC () IS\n"
		"BEGIN\n"
		"DELETE FROM SYS_FOREIGN WHERE ID=:id;\n"
		"DELETE FROM SYS_FOREIGN_COLS WHERE ID=:id;\n"
		"END;\n";

	dberr_t		error;
	pars_info_t*	info;

	info = pars_info_create();
	pars_info_add_str_literal(info, "id", foreign->id);

	trx->op_info = "dropping foreign key constraint from dictionary";
	error = que_eval_sql(info, sql, FALSE, trx);
	trx->op_info = "";

	DBUG_EXECUTE_IF("ib_drop_foreign_error",
			error = DB_OUT_OF_FILE_SPACE;);

	if (error != DB_SUCCESS) {
		my_error_innodb(error, table_share->table_name.str, 0);
		trx->error_state = DB_SUCCESS;
		DBUG_RETURN(true);
	}

	/* Drop the foreign key constraint from the data dictionary cache. */
	dict_foreign_remove_from_cache(foreign);
	DBUG_RETURN(false);
}

/** Rename a column.
@param table_share	the TABLE_SHARE
@param prebuilt		the prebuilt struct
@param trx		data dictionary transaction
@param nth_col		0-based index of the column
@param from		old column name
@param to		new column name
@param new_clustered	whether the table has been rebuilt
@retval true		Failure
@retval false		Success */
static __attribute__((nonnull, warn_unused_result))
bool
innobase_rename_column(
/*===================*/
	const TABLE_SHARE*	table_share,
	row_prebuilt_t*		prebuilt,
	trx_t*			trx,
	ulint			nth_col,
	const char*		from,
	const char*		to,
	bool			new_clustered)
{
	pars_info_t*	info;
	dberr_t		error;

	DBUG_ENTER("innobase_rename_column");

	DBUG_ASSERT(trx_get_dict_operation(trx)
		    == new_clustered ? TRX_DICT_OP_TABLE : TRX_DICT_OP_INDEX);
	ut_ad(trx->dict_operation_lock_mode == RW_X_LATCH);
	ut_ad(mutex_own(&dict_sys->mutex));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&dict_operation_lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	if (new_clustered) {
		goto rename_foreign;
	}

	info = pars_info_create();

	pars_info_add_ull_literal(info, "tableid", prebuilt->table->id);
	pars_info_add_int4_literal(info, "nth", nth_col);
	pars_info_add_str_literal(info, "old", from);
	pars_info_add_str_literal(info, "new", to);

	trx->op_info = "renaming column in SYS_COLUMNS";

	error = que_eval_sql(
		info,
		"PROCEDURE RENAME_SYS_COLUMNS_PROC () IS\n"
		"BEGIN\n"
		"UPDATE SYS_COLUMNS SET NAME=:new\n"
		"WHERE TABLE_ID=:tableid AND NAME=:old\n"
		"AND POS=:nth;\n"
		"END;\n",
		FALSE, trx);

	DBUG_EXECUTE_IF("ib_rename_column_error",
			error = DB_OUT_OF_FILE_SPACE;);

	if (error != DB_SUCCESS) {
err_exit:
		my_error_innodb(error, table_share->table_name.str, 0);
		trx->error_state = DB_SUCCESS;
		trx->op_info = "";
		DBUG_RETURN(true);
	}

	trx->op_info = "renaming column in SYS_FIELDS";

	for (dict_index_t* index = dict_table_get_first_index(prebuilt->table);
	     index != NULL;
	     index = dict_table_get_next_index(index)) {

		for (ulint i = 0; i < dict_index_get_n_fields(index); i++) {
			if (strcmp(dict_index_get_nth_field(index, i)->name,
				   from)) {
				continue;
			}

			info = pars_info_create();

			pars_info_add_ull_literal(info, "indexid", index->id);
			pars_info_add_int4_literal(info, "nth", i);
			pars_info_add_str_literal(info, "old", from);
			pars_info_add_str_literal(info, "new", to);

			error = que_eval_sql(
				info,
				"PROCEDURE RENAME_SYS_FIELDS_PROC () IS\n"
				"BEGIN\n"

				"UPDATE SYS_FIELDS SET COL_NAME=:new\n"
				"WHERE INDEX_ID=:indexid AND COL_NAME=:old\n"
				"AND POS=:nth;\n"

				/* Try again, in case there is a prefix_len
				encoded in SYS_FIELDS.POS */

				"UPDATE SYS_FIELDS SET COL_NAME=:new\n"
				"WHERE INDEX_ID=:indexid AND COL_NAME=:old\n"
				"AND POS>=65536*:nth AND POS<65536*(:nth+1);\n"

				"END;\n",
				FALSE, trx);

			if (error != DB_SUCCESS) {
				goto err_exit;
			}
		}
	}

rename_foreign:
	trx->op_info = "renaming column in SYS_FOREIGN_COLS";

	for (dict_foreign_t* foreign = UT_LIST_GET_FIRST(
		     prebuilt->table->foreign_list);
	     foreign != NULL;
	     foreign = UT_LIST_GET_NEXT(foreign_list, foreign)) {
		for (unsigned i = 0; i < foreign->n_fields; i++) {
			if (strcmp(foreign->foreign_col_names[i], from)) {
				continue;
			}

			info = pars_info_create();

			pars_info_add_str_literal(info, "id", foreign->id);
			pars_info_add_int4_literal(info, "nth", i);
			pars_info_add_str_literal(info, "old", from);
			pars_info_add_str_literal(info, "new", to);

			error = que_eval_sql(
				info,
				"PROCEDURE RENAME_SYS_FOREIGN_F_PROC () IS\n"
				"BEGIN\n"
				"UPDATE SYS_FOREIGN_COLS\n"
				"SET FOR_COL_NAME=:new\n"
				"WHERE ID=:id AND POS=:nth\n"
				"AND FOR_COL_NAME=:old;\n"
				"END;\n",
				FALSE, trx);

			if (error != DB_SUCCESS) {
				goto err_exit;
			}
		}
	}

	for (dict_foreign_t* foreign = UT_LIST_GET_FIRST(
		     prebuilt->table->referenced_list);
	     foreign != NULL;
	     foreign = UT_LIST_GET_NEXT(referenced_list, foreign)) {
		for (unsigned i = 0; i < foreign->n_fields; i++) {
			if (strcmp(foreign->referenced_col_names[i], from)) {
				continue;
			}

			info = pars_info_create();

			pars_info_add_str_literal(info, "id", foreign->id);
			pars_info_add_int4_literal(info, "nth", i);
			pars_info_add_str_literal(info, "old", from);
			pars_info_add_str_literal(info, "new", to);

			error = que_eval_sql(
				info,
				"PROCEDURE RENAME_SYS_FOREIGN_R_PROC () IS\n"
				"BEGIN\n"
				"UPDATE SYS_FOREIGN_COLS\n"
				"SET REF_COL_NAME=:new\n"
				"WHERE ID=:id AND POS=:nth\n"
				"AND REF_COL_NAME=:old;\n"
				"END;\n",
				FALSE, trx);

			if (error != DB_SUCCESS) {
				goto err_exit;
			}
		}
	}

	trx->op_info = "";
	if (!new_clustered) {
		/* Rename the column in the data dictionary cache. */
		dict_mem_table_col_rename(prebuilt->table, nth_col, from, to);
	}
	DBUG_RETURN(false);
}

/** Rename columns.
@param ha_alter_info	Data used during in-place alter.
@param new_clustered	whether the table has been rebuilt
@param table		the TABLE
@param table_share	the TABLE_SHARE
@param prebuilt		the prebuilt struct
@param trx		data dictionary transaction
@retval true		Failure
@retval false		Success */
static __attribute__((nonnull, warn_unused_result))
bool
innobase_rename_columns(
/*====================*/
	Alter_inplace_info*	ha_alter_info,
	bool			new_clustered,
	const TABLE*		table,
	const TABLE_SHARE*	table_share,
	row_prebuilt_t*		prebuilt,
	trx_t*			trx)
{
	List_iterator_fast<Create_field> cf_it(
		ha_alter_info->alter_info->create_list);
	uint i = 0;

	for (Field** fp = table->field; *fp; fp++, i++) {
		if (!((*fp)->flags & FIELD_IS_RENAMED)) {
			continue;
		}

		cf_it.rewind();
		while (Create_field* cf = cf_it++) {
			if (cf->field == *fp) {
				if (innobase_rename_column(
					    table_share,
					    prebuilt, trx, i,
					    cf->field->field_name,
					    cf->field_name, new_clustered)) {
					return(true);
				}
				goto processed_field;
			}
		}

		ut_error;
processed_field:
		continue;
	}

	return(false);
}

/** Undo the in-memory addition of foreign key on table->foreign_list
and table->referenced_list.
@param ctx		saved alter table context
@param table		the foreign table */
static __attribute__((nonnull))
void
innobase_undo_add_fk(
/*=================*/
	ha_innobase_inplace_ctx*	ctx,
	dict_table_t*			fk_table)
{
	for (ulint i = 0; i < ctx->num_to_add_fk; i++) {
		UT_LIST_REMOVE(
			foreign_list,
			fk_table->foreign_list,
			ctx->add_fk[i]);

		if (ctx->add_fk[i]->referenced_table) {
			UT_LIST_REMOVE(
				referenced_list,
				ctx->add_fk[i]->referenced_table
				->referenced_list,
				ctx->add_fk[i]);
		}
	}
}

/** Commit or rollback the changes made during
prepare_inplace_alter_table() and inplace_alter_table() inside
the storage engine. Note that the allowed level of concurrency
during this operation will be the same as for
inplace_alter_table() and thus might be higher than during
prepare_inplace_alter_table(). (E.g concurrent writes were
blocked during prepare, but might not be during commit).
@param altered_table	TABLE object for new version of table.
@param ha_alter_info	Structure describing changes to be done
by ALTER TABLE and holding data used during in-place alter.
@param commit		true => Commit, false => Rollback.
@retval true		Failure
@retval false		Success
*/
UNIV_INTERN
bool
ha_innobase::commit_inplace_alter_table(
/*====================================*/
	TABLE*			altered_table,
	Alter_inplace_info*	ha_alter_info,
	bool			commit)
{
	ha_innobase_inplace_ctx*	ctx
		= static_cast<ha_innobase_inplace_ctx*>
		(ha_alter_info->handler_ctx);
	trx_t*				trx;
	trx_t*				fk_trx = NULL;
	int				err	= 0;
	bool				new_clustered;
	dict_table_t*			fk_table = NULL;
	ulonglong			max_autoinc;

	ut_ad(!srv_read_only_mode);

	DBUG_ENTER("commit_inplace_alter_table");

	DEBUG_SYNC_C("innodb_commit_inplace_alter_table_enter");

	DEBUG_SYNC_C("innodb_commit_inplace_alter_table_wait");

	if (!commit) {
		/* A rollback is being requested. So far we may at
		most have created some indexes. If any indexes were to
		be dropped, they would actually be dropped in this
		method if commit=true. */
		DBUG_RETURN(rollback_inplace_alter_table(
				    ha_alter_info, table_share, prebuilt));
	}

	if (!altered_table->found_next_number_field) {
		/* There is no AUTO_INCREMENT column in the table
		after the ALTER operation. */
		max_autoinc = 0;
	} else if (ctx && ctx->add_autoinc != ULINT_UNDEFINED) {
		/* An AUTO_INCREMENT column was added. Get the last
		value from the sequence, which may be based on a
		supplied AUTO_INCREMENT value. */
		max_autoinc = ctx->sequence.last();
	} else if ((ha_alter_info->handler_flags
		    & Alter_inplace_info::CHANGE_CREATE_OPTION)
		   && (ha_alter_info->create_info->used_fields
		       & HA_CREATE_USED_AUTO)) {
		/* An AUTO_INCREMENT value was supplied, but the table
		was not rebuilt. Get the user-supplied value. */
		max_autoinc = ha_alter_info->create_info->auto_increment_value;
	} else {
		/* An AUTO_INCREMENT value was not specified.
		Read the old counter value from the table. */
		ut_ad(table->found_next_number_field);
		dict_table_autoinc_lock(prebuilt->table);
		max_autoinc = dict_table_autoinc_read(prebuilt->table);
		dict_table_autoinc_unlock(prebuilt->table);
	}

	if (!(ha_alter_info->handler_flags & ~INNOBASE_INPLACE_IGNORE)) {
		DBUG_ASSERT(!ctx);
		/* We may want to update table attributes. */
		goto func_exit;
	}

	trx_start_if_not_started_xa(prebuilt->trx);

	{
		/* Exclusively lock the table, to ensure that no other
		transaction is holding locks on the table while we
		change the table definition. The MySQL meta-data lock
		should normally guarantee that no conflicting locks
		exist. However, FOREIGN KEY constraints checks and any
		transactions collected during crash recovery could be
		holding InnoDB locks only, not MySQL locks. */
		dberr_t error = row_merge_lock_table(
			prebuilt->trx, prebuilt->table, LOCK_X);

		if (error != DB_SUCCESS) {
			my_error_innodb(error, table_share->table_name.str, 0);
			DBUG_RETURN(true);
		}

		DEBUG_SYNC(user_thd, "innodb_alter_commit_after_lock_table");
	}

	if (ctx) {
		if (ctx->indexed_table != prebuilt->table) {
			for (dict_index_t* index = dict_table_get_first_index(
				     ctx->indexed_table);
			     index;
			     index = dict_table_get_next_index(index)) {
				DBUG_ASSERT(dict_index_get_online_status(index)
					    == ONLINE_INDEX_COMPLETE);
				DBUG_ASSERT(*index->name != TEMP_INDEX_PREFIX);
				if (dict_index_is_corrupted(index)) {
					my_error(ER_INDEX_CORRUPT, MYF(0),
						 index->name);
					DBUG_RETURN(true);
				}
			}
		} else {
			for (ulint i = 0; i < ctx->num_to_add; i++) {
				dict_index_t*	index = ctx->add[i];
				DBUG_ASSERT(dict_index_get_online_status(index)
					    == ONLINE_INDEX_COMPLETE);
				DBUG_ASSERT(*index->name == TEMP_INDEX_PREFIX);
				if (dict_index_is_corrupted(index)) {
					/* Report a duplicate key
					error for the index that was
					flagged corrupted, most likely
					because a duplicate value was
					inserted (directly or by
					rollback) after
					ha_innobase::inplace_alter_table()
					completed. */
					my_error(ER_DUP_UNKNOWN_IN_INDEX,
						 MYF(0), index->name + 1);
					DBUG_RETURN(true);
				}
			}
		}
	}

	if (!ctx || !ctx->trx) {
		/* Create a background transaction for the operations on
		the data dictionary tables. */
		trx = innobase_trx_allocate(user_thd);

		trx_start_for_ddl(trx, TRX_DICT_OP_INDEX);

		new_clustered = false;
	} else {
		trx_dict_op_t	op;

		trx = ctx->trx;

		new_clustered = ctx->indexed_table != prebuilt->table;

		op = (new_clustered) ? TRX_DICT_OP_TABLE : TRX_DICT_OP_INDEX;

		trx_start_for_ddl(trx, op);
	}

	if (new_clustered) {
		if (prebuilt->table->fts) {
			ut_ad(!prebuilt->table->fts->add_wq);
			fts_optimize_remove_table(prebuilt->table);
		}

		if (ctx->indexed_table->fts) {
			ut_ad(!ctx->indexed_table->fts->add_wq);
			fts_optimize_remove_table(ctx->indexed_table);
		}
	}

	/* Latch the InnoDB data dictionary exclusively so that no deadlocks
	or lock waits can happen in it during the data dictionary operation. */
	row_mysql_lock_data_dictionary(trx);

	/* Wait for background stats processing to stop using the
	indexes that we are going to drop (if any). */
	if (ctx) {
		dict_stats_wait_bg_to_stop_using_tables(
			prebuilt->table, ctx->indexed_table, trx);
	}

	/* Final phase of add foreign key processing */
	if (ctx && ctx->num_to_add_fk > 0) {
		ulint		highest_id_so_far;
		dberr_t		error;

		/* If it runs concurrently with create index or table
		rebuild, we will need a separate trx to do the system
		table change, since in the case of failure to rebuild/create
		index, it will need to commit the trx that drops the newly
		created table/index, while for FK, it needs to rollback
		the metadata change */
		if (new_clustered || ctx->num_to_add) {
			fk_trx = innobase_trx_allocate(user_thd);

			trx_start_for_ddl(fk_trx, TRX_DICT_OP_INDEX);

			fk_trx->dict_operation_lock_mode =
				 trx->dict_operation_lock_mode;
		} else {
			fk_trx = trx;
		}

		ut_ad(ha_alter_info->handler_flags
		      & Alter_inplace_info::ADD_FOREIGN_KEY);

		highest_id_so_far = dict_table_get_highest_foreign_id(
			prebuilt->table);

		highest_id_so_far++;

		fk_table = ctx->indexed_table;

		for (ulint i = 0; i < ctx->num_to_add_fk; i++) {

			/* Get the new dict_table_t */
			if (new_clustered) {
				ctx->add_fk[i]->foreign_table
					= fk_table;
			}

			/* Add Foreign Key info to in-memory metadata */
			UT_LIST_ADD_LAST(foreign_list,
					 fk_table->foreign_list,
					 ctx->add_fk[i]);

			if (ctx->add_fk[i]->referenced_table) {
				UT_LIST_ADD_LAST(
					referenced_list,
					ctx->add_fk[i]->referenced_table->referenced_list,
					ctx->add_fk[i]);
			}

			if (!ctx->add_fk[i]->foreign_index) {
				ctx->add_fk[i]->foreign_index
					= dict_foreign_find_index(
					fk_table,
					ctx->add_fk[i]->foreign_col_names,
					ctx->add_fk[i]->n_fields, NULL,
					TRUE, FALSE);

				ut_ad(ctx->add_fk[i]->foreign_index);

				if (!innobase_check_fk_option(
					ctx->add_fk[i])) {
					my_error(ER_FK_INCORRECT_OPTION,
						 MYF(0),
						 table_share->table_name.str,
						 ctx->add_fk[i]->id);
					goto undo_add_fk;
				}
			}

			/* System table change */
			error = dict_create_add_foreign_to_dictionary(
				&highest_id_so_far, prebuilt->table,
				ctx->add_fk[i], fk_trx);

			DBUG_EXECUTE_IF(
				"innodb_test_cannot_add_fk_system",
				error = DB_ERROR;);

			if (error != DB_SUCCESS) {
				my_error(ER_FK_FAIL_ADD_SYSTEM, MYF(0),
					 ctx->add_fk[i]->id);
				goto undo_add_fk;
			}
		}

		/* Make sure the tables are moved to non-lru side of
		dictionary list */
		error = dict_load_foreigns(prebuilt->table->name, FALSE, TRUE);

		if (error != DB_SUCCESS) {
			my_error(ER_CANNOT_ADD_FOREIGN, MYF(0));

undo_add_fk:
			err = -1;

			if (new_clustered) {
				goto drop_new_clustered;
			} else if (ctx->num_to_add > 0) {
				ut_ad(trx != fk_trx);

				innobase_rollback_sec_index(
					prebuilt, table_share, trx);
				innobase_undo_add_fk(ctx, fk_table);
				trx_rollback_for_mysql(fk_trx);

				goto trx_commit;
			} else {
				goto trx_rollback;
			}
		}
	}

	if (new_clustered) {
		dberr_t	error;
		char*	tmp_name;

		/* Clear the to_be_dropped flag in the data dictionary. */
		for (ulint i = 0; i < ctx->num_to_drop; i++) {
			dict_index_t*	index = ctx->drop[i];
			DBUG_ASSERT(*index->name != TEMP_INDEX_PREFIX);
			DBUG_ASSERT(index->to_be_dropped);
			index->to_be_dropped = 0;
		}

		/* We copied the table. Any indexes that were
		requested to be dropped were not created in the copy
		of the table. Apply any last bit of the rebuild log
		and then rename the tables. */

		if (ctx->online) {
			DEBUG_SYNC_C("row_log_table_apply2_before");
			error = row_log_table_apply(
				ctx->thr, prebuilt->table, altered_table);

			switch (error) {
				KEY*	dup_key;
			case DB_SUCCESS:
				break;
			case DB_DUPLICATE_KEY:
				if (prebuilt->trx->error_key_num
				    == ULINT_UNDEFINED) {
					/* This should be the hidden index on
					FTS_DOC_ID. */
					dup_key = NULL;
				} else {
					DBUG_ASSERT(
						prebuilt->trx->error_key_num
						< ha_alter_info->key_count);
					dup_key = &ha_alter_info
						->key_info_buffer[
							prebuilt->trx
							->error_key_num];
				}
				print_keydup_error(altered_table, dup_key, MYF(0));
				break;
			case DB_ONLINE_LOG_TOO_BIG:
				my_error(ER_INNODB_ONLINE_LOG_TOO_BIG, MYF(0),
					 ha_alter_info->key_info_buffer[0]
					 .name);
				break;
			case DB_INDEX_CORRUPT:
				my_error(ER_INDEX_CORRUPT, MYF(0),
					 (prebuilt->trx->error_key_num
					  == ULINT_UNDEFINED)
					 ? FTS_DOC_ID_INDEX_NAME
					 : ha_alter_info->key_info_buffer[
						 prebuilt->trx->error_key_num]
					 .name);
				break;
			default:
				my_error_innodb(error,
						table_share->table_name.str,
						prebuilt->table->flags);
			}

			if (error != DB_SUCCESS) {
				err = -1;
				goto drop_new_clustered;
			}
		}

		if ((ha_alter_info->handler_flags
		     & Alter_inplace_info::ALTER_COLUMN_NAME)
		    && innobase_rename_columns(ha_alter_info, true, table,
					       table_share, prebuilt, trx)) {
			err = -1;
			goto drop_new_clustered;
		}

		/* A new clustered index was defined for the table
		and there was no error at this point. We can
		now rename the old table as a temporary table,
		rename the new temporary table as the old
		table and drop the old table. */
		tmp_name = dict_mem_create_temporary_tablename(
			ctx->heap, ctx->indexed_table->name,
			ctx->indexed_table->id);

		/* Rename table will reload and refresh the in-memory
		foreign key constraint metadata. This is a rename operation
		in preparing for dropping the old table. Set the table
		to_be_dropped bit here, so to make sure DML foreign key
		constraint check does not use the stale dict_foreign_t.
		This is done because WL#6049 (FK MDL) has not been
		implemented yet */
		prebuilt->table->to_be_dropped = true;

		DBUG_EXECUTE_IF("ib_ddl_crash_before_rename",
				DBUG_SUICIDE(););

		/* The new table must inherit the flag from the
		"parent" table. */
		if (dict_table_is_discarded(prebuilt->table)) {
			ctx->indexed_table->ibd_file_missing = true;
			ctx->indexed_table->flags2 |= DICT_TF2_DISCARDED;
		}

		error = row_merge_rename_tables(
			prebuilt->table, ctx->indexed_table,
			tmp_name, trx);

		DBUG_EXECUTE_IF("ib_ddl_crash_after_rename",
				DBUG_SUICIDE(););

		/* n_ref_count must be 1, because purge cannot
		be executing on this very table as we are
		holding dict_operation_lock X-latch. */
		ut_a(prebuilt->table->n_ref_count == 1);

		switch (error) {
			dict_table_t*	old_table;
		case DB_SUCCESS:
			old_table = prebuilt->table;

			DBUG_EXECUTE_IF("ib_ddl_crash_before_commit",
					DBUG_SUICIDE(););

			trx_commit_for_mysql(prebuilt->trx);

			DBUG_EXECUTE_IF("ib_ddl_crash_after_commit",
					DBUG_SUICIDE(););

			if (fk_trx) {
				ut_ad(fk_trx != trx);
				trx_commit_for_mysql(fk_trx);
			}

			row_prebuilt_free(prebuilt, TRUE);
			error = row_merge_drop_table(trx, old_table);
			prebuilt = row_create_prebuilt(
				ctx->indexed_table, table->s->reclength);
			err = 0;
			break;
		case DB_TABLESPACE_EXISTS:
			ut_a(ctx->indexed_table->n_ref_count == 1);
			my_error(ER_TABLESPACE_EXISTS, MYF(0), tmp_name);
			err = HA_ERR_TABLESPACE_EXISTS;
			goto drop_new_clustered;
		case DB_DUPLICATE_KEY:
			ut_a(ctx->indexed_table->n_ref_count == 1);
			my_error(ER_TABLE_EXISTS_ERROR, MYF(0), tmp_name);
			err = HA_ERR_TABLE_EXIST;
			goto drop_new_clustered;
		default:
			my_error_innodb(error,
					table_share->table_name.str,
					prebuilt->table->flags);
			err = -1;

drop_new_clustered:
			/* Reset the to_be_dropped bit for the old table,
			since we are aborting the operation and dropping
			the new table due to some error conditions */
			prebuilt->table->to_be_dropped = false;

			/* Need to drop the added foreign key first */
			if (fk_trx) {
				ut_ad(fk_trx != trx);
				innobase_undo_add_fk(ctx, fk_table);
				trx_rollback_for_mysql(fk_trx);
			}

			dict_table_close(ctx->indexed_table, TRUE, FALSE);

#ifdef UNIV_DDL_DEBUG
			/* Nobody should have initialized the stats of the
			newly created table yet. When this is the case, we
			know that it has not been added for background stats
			gathering. */
			ut_a(!ctx->indexed_table->stat_initialized);
#endif /* UNIV_DDL_DEBUG */

			row_merge_drop_table(trx, ctx->indexed_table);
			ctx->indexed_table = NULL;
			goto trx_commit;
		}
	} else if (ctx) {
		dberr_t	error;

		/* We altered the table in place. */
		/* Lose the TEMP_INDEX_PREFIX. */
		for (ulint i = 0; i < ctx->num_to_add; i++) {
			dict_index_t*	index = ctx->add[i];
			DBUG_ASSERT(dict_index_get_online_status(index)
				    == ONLINE_INDEX_COMPLETE);
			DBUG_ASSERT(*index->name
				    == TEMP_INDEX_PREFIX);
			index->name++;
			error = row_merge_rename_index_to_add(
				trx, prebuilt->table->id,
				index->id);
			if (error != DB_SUCCESS) {
				sql_print_error(
					"InnoDB: rename index to add: %lu\n",
					(ulong) error);
				DBUG_ASSERT(0);
			}
		}

		/* Drop any indexes that were requested to be dropped.
		Rename them to TEMP_INDEX_PREFIX in the data
		dictionary first. We do not bother to rename
		index->name in the dictionary cache, because the index
		is about to be freed after row_merge_drop_indexes_dict(). */

		for (ulint i = 0; i < ctx->num_to_drop; i++) {
			dict_index_t*	index = ctx->drop[i];
			DBUG_ASSERT(*index->name != TEMP_INDEX_PREFIX);
			DBUG_ASSERT(index->table == prebuilt->table);
			DBUG_ASSERT(index->to_be_dropped);

			error = row_merge_rename_index_to_drop(
				trx, index->table->id, index->id);
			if (error != DB_SUCCESS) {
				sql_print_error(
					"InnoDB: rename index to drop: %lu\n",
					(ulong) error);
				DBUG_ASSERT(0);
			}
		}
	}

	if (err == 0
	    && (ha_alter_info->handler_flags
		& Alter_inplace_info::DROP_FOREIGN_KEY)) {
		DBUG_ASSERT(ctx->num_to_drop_fk > 0);
		DBUG_ASSERT(ctx->num_to_drop_fk
			    == ha_alter_info->alter_info->drop_list.elements);
		for (ulint i = 0; i < ctx->num_to_drop_fk; i++) {
			DBUG_ASSERT(prebuilt->table
				    == ctx->drop_fk[i]->foreign_table);

			if (innobase_drop_foreign(
				    table_share, trx, ctx->drop_fk[i])) {
				err = -1;
			}
		}
	}

	if (err == 0 && !new_clustered
	    && (ha_alter_info->handler_flags
		& Alter_inplace_info::ALTER_COLUMN_NAME)
	    && innobase_rename_columns(ha_alter_info, false, table,
				       table_share, prebuilt, trx)) {
		err = -1;
	}

	if (err == 0) {
		if (fk_trx && fk_trx != trx) {
			/* This needs to be placed before "trx_commit" marker,
			since anyone called "goto trx_commit" has committed
			or rolled back fk_trx before jumping here */
			trx_commit_for_mysql(fk_trx);
		}
trx_commit:
		trx_commit_for_mysql(trx);
	} else {
trx_rollback:
		/* undo the addition of foreign key */
		if (fk_trx) {
			innobase_undo_add_fk(ctx, fk_table);

			if (fk_trx != trx) {
				trx_rollback_for_mysql(fk_trx);
			}
		}

		trx_rollback_for_mysql(trx);

		/* If there are newly added secondary indexes, above
		rollback will revert the rename operation and put the
		new indexes with the temp index prefix, we can drop
		them here */
		if (ctx && !new_clustered) {
			ulint	i;

			/* Need to drop the in-memory dict_index_t first
			to avoid dict_table_check_for_dup_indexes()
			assertion in row_merge_drop_indexes() in the case
			of add and drop the same index */
			for (i = 0; i < ctx->num_to_add; i++) {
				dict_index_t*   index = ctx->add[i];
				dict_index_remove_from_cache(
					prebuilt->table, index);
			}

			if (ctx->num_to_add) {
				trx_start_for_ddl(trx, TRX_DICT_OP_INDEX);
				row_merge_drop_indexes(trx, prebuilt->table,
						       FALSE);
				trx_commit_for_mysql(trx);
			}

			for (i = 0; i < ctx->num_to_drop; i++) {
				dict_index_t*	index = ctx->drop[i];
				index->to_be_dropped = false;
			}
		}
	}

	/* Flush the log to reduce probability that the .frm files and
	the InnoDB data dictionary get out-of-sync if the user runs
	with innodb_flush_log_at_trx_commit = 0 */

	log_buffer_flush_to_disk();

	if (new_clustered) {
		innobase_online_rebuild_log_free(prebuilt->table);
	}

	if (err == 0 && ctx) {
		/* The changes were successfully performed. */
		bool	add_fts	= false;

		/* Rebuild the index translation table.
		This should only be needed when !new_clustered. */
		share->idx_trans_tbl.index_count = 0;

		/* Publish the created fulltext index, if any.
		Note that a fulltext index can be created without
		creating the clustered index, if there already exists
		a suitable FTS_DOC_ID column. If not, one will be
		created, implying new_clustered */
		for (ulint i = 0; i < ctx->num_to_add; i++) {
			dict_index_t*	index = ctx->add[i];

			if (index->type & DICT_FTS) {
				DBUG_ASSERT(index->type == DICT_FTS);
				fts_add_index(index, prebuilt->table);
				add_fts = true;
			}
		}

		if (!new_clustered && ha_alter_info->index_drop_count) {

			/* Really drop the indexes that were dropped.
			The transaction had to be committed first
			(after renaming the indexes), so that in the
			event of a crash, crash recovery will drop the
			indexes, because it drops all indexes whose
			names start with TEMP_INDEX_PREFIX. Once we
			have started dropping an index tree, there is
			no way to roll it back. */

			trx_start_for_ddl(trx, TRX_DICT_OP_INDEX);

			for (ulint i = 0; i < ctx->num_to_drop; i++) {
				dict_index_t*	index = ctx->drop[i];
				DBUG_ASSERT(*index->name != TEMP_INDEX_PREFIX);
				DBUG_ASSERT(index->table == prebuilt->table);
				DBUG_ASSERT(index->to_be_dropped);

				/* Replace the indexes in foreign key
				constraints if needed. */

				dict_foreign_replace_index(
					prebuilt->table, index, prebuilt->trx);

				/* Mark the index dropped
				in the data dictionary cache. */
				rw_lock_x_lock(dict_index_get_lock(index));
				index->page = FIL_NULL;
				rw_lock_x_unlock(dict_index_get_lock(index));
			}

			row_merge_drop_indexes_dict(trx, prebuilt->table->id);

			for (ulint i = 0; i < ctx->num_to_drop; i++) {
				dict_index_t*	index = ctx->drop[i];
				DBUG_ASSERT(*index->name != TEMP_INDEX_PREFIX);
				DBUG_ASSERT(index->table == prebuilt->table);

				if (index->type & DICT_FTS) {
					DBUG_ASSERT(index->type == DICT_FTS
						    || (index->type
							& DICT_CORRUPT));
					DBUG_ASSERT(prebuilt->table->fts);
					fts_drop_index(
						prebuilt->table, index, trx);
				}

				dict_index_remove_from_cache(
					prebuilt->table, index);
			}

			trx_commit_for_mysql(trx);
		}

		ut_d(dict_table_check_for_dup_indexes(
			     prebuilt->table, CHECK_ALL_COMPLETE));
		DBUG_ASSERT(new_clustered == !prebuilt->trx);

		if (add_fts) {
			fts_optimize_add_table(prebuilt->table);
		}
	}

	if (!prebuilt->trx) {
		/* We created a new clustered index and committed the
		user transaction already, so that we were able to
		drop the old table. */
		update_thd();
		prebuilt->trx->will_lock++;

		DBUG_EXECUTE_IF("ib_ddl_crash_after_user_trx_commit",
				DBUG_SUICIDE(););

		trx_start_if_not_started_xa(prebuilt->trx);
	}

	ut_d(dict_table_check_for_dup_indexes(
		     prebuilt->table, CHECK_ABORTED_OK));
	ut_a(fts_check_cached_index(prebuilt->table));
	row_mysql_unlock_data_dictionary(trx);
	if (fk_trx && fk_trx != trx) {
		fk_trx->dict_operation_lock_mode = 0;
		trx_free_for_mysql(fk_trx);
	}
	trx_free_for_mysql(trx);

	if (ctx && trx == ctx->trx) {
		ctx->trx = NULL;
	}

	if (err == 0) {
		/* Delete corresponding rows from the stats table. We update
		the statistics in a separate transaction from trx, because
		lock waits are not allowed in a data dictionary transaction.
		(Lock waits are possible on the statistics table, because it
		is directly accessible by users, not covered by the
		dict_operation_lock.)

		Because the data dictionary changes were already committed,
		orphaned rows may be left in the statistics table if the
		system crashes. */

		for (uint i = 0; i < ha_alter_info->index_drop_count; i++) {
			const KEY*	key
				= ha_alter_info->index_drop_buffer[i];
			dberr_t		ret;
			char		errstr[1024];

			ret = dict_stats_drop_index(
				prebuilt->table->name, key->name,
				errstr, sizeof(errstr));

			if (ret != DB_SUCCESS) {
				push_warning(user_thd,
					     Sql_condition::SL_WARNING,
					     ER_LOCK_WAIT_TIMEOUT,
					     errstr);
			}
		}

		if (ctx && !dict_table_is_discarded(prebuilt->table)) {
			bool	stats_init_called = false;

			for (uint i = 0; i < ctx->num_to_add; i++) {
				dict_index_t*	index = ctx->add[i];

				if (!(index->type & DICT_FTS)) {

					if (!stats_init_called) {
						innobase_copy_frm_flags_from_table_share(
							index->table,
							altered_table->s);

						dict_stats_init(index->table);

						stats_init_called = true;
					}

					dict_stats_update_for_index(index);
				}
			}
		}
	}

	trx_commit_for_mysql(prebuilt->trx);

	/* Flush the log to reduce probability that the .frm files and
	the InnoDB data dictionary get out-of-sync if the user runs
	with innodb_flush_log_at_trx_commit = 0 */

	log_buffer_flush_to_disk();

	/* Tell the InnoDB server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

func_exit:

	if (err == 0 && altered_table->found_next_number_field != 0) {
		dict_table_autoinc_lock(prebuilt->table);
		dict_table_autoinc_initialize(prebuilt->table, max_autoinc);
		dict_table_autoinc_unlock(prebuilt->table);
	}

#ifndef DBUG_OFF
	dict_index_t* clust_index = dict_table_get_first_index(
		prebuilt->table);
	DBUG_ASSERT(!clust_index->online_log);
	DBUG_ASSERT(dict_index_get_online_status(clust_index)
		    == ONLINE_INDEX_COMPLETE);
#endif /* !DBUG_OFF */

#ifdef UNIV_DEBUG
	for (dict_index_t* index = dict_table_get_first_index(
		     prebuilt->table);
	     index;
	     index = dict_table_get_next_index(index)) {
		ut_ad(!index->to_be_dropped);
	}
#endif /* UNIV_DEBUG */

	if (err == 0) {
		MONITOR_ATOMIC_DEC(MONITOR_PENDING_ALTER_TABLE);

#ifdef UNIV_DDL_DEBUG
		/* Invoke CHECK TABLE atomically after a successful
		ALTER TABLE. */
		TABLE* old_table = table;
		table = altered_table;
		ut_a(check(user_thd, 0) == HA_ADMIN_OK);
		table = old_table;
#endif /* UNIV_DDL_DEBUG */
	}

	DBUG_RETURN(err != 0);
}

/**
@param thd - the session
@param start_value - the lower bound
@param max_value - the upper bound (inclusive) */
ib_sequence_t::ib_sequence_t(
	THD*		thd,
	ulonglong	start_value,
	ulonglong	max_value)
	:
	m_max_value(max_value),
	m_increment(0),
	m_offset(0),
	m_next_value(start_value),
	m_eof(false)
{
	if (thd != 0 && m_max_value > 0) {

		thd_get_autoinc(thd, &m_offset, &m_increment);

		if (m_increment > 1 || m_offset > 1) {

			/* If there is an offset or increment specified
			then we need to work out the exact next value. */

			m_next_value = innobase_next_autoinc(
				start_value, 1,
				m_increment, m_offset, m_max_value);

		} else if (start_value == 0) {
			/* The next value can never be 0. */
			m_next_value = 1;
		}
	} else {
		m_eof = true;
	}
}

/**
Postfix increment
@return the next value to insert */
ulonglong
ib_sequence_t::operator++(int) UNIV_NOTHROW
{
	ulonglong	current = m_next_value;

	ut_ad(!m_eof);
	ut_ad(m_max_value > 0);

	m_next_value = innobase_next_autoinc(
		current, 1, m_increment, m_offset, m_max_value);

	if (m_next_value == m_max_value && current == m_next_value) {
		m_eof = true;
	}

	return(current);
}
