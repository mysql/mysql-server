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
#include <mysql/innodb_priv.h>

#include "dict0stats.h"
#include "log0log.h"
#include "row0log.h"
#include "row0merge.h"
#include "srv0srv.h"
#include "trx0trx.h"
#include "trx0roll.h"
#include "ha_prototypes.h"
#include "handler0alter.h"
#include "srv0mon.h"
#include "fts0priv.h"

#include "ha_innodb.h"

/** Operations for creating an index in place */
static const Alter_inplace_info::HA_ALTER_FLAGS INNOBASE_INPLACE_CREATE
	= Alter_inplace_info::ADD_INDEX
	| Alter_inplace_info::ADD_UNIQUE_INDEX
	| Alter_inplace_info::ADD_PK_INDEX;// not online

/** Operations for altering a table that InnoDB does not care about */
static const Alter_inplace_info::HA_ALTER_FLAGS INNOBASE_INPLACE_IGNORE
	= Alter_inplace_info::ALTER_COLUMN_DEFAULT
	| Alter_inplace_info::CHANGE_CREATE_OPTION;

/** Operations that InnoDB can perform online */
static const Alter_inplace_info::HA_ALTER_FLAGS INNOBASE_ONLINE_OPERATIONS
	= INNOBASE_INPLACE_IGNORE
	| Alter_inplace_info::ADD_INDEX
	| Alter_inplace_info::DROP_INDEX
	| Alter_inplace_info::ADD_UNIQUE_INDEX
	| Alter_inplace_info::DROP_UNIQUE_INDEX
	| Alter_inplace_info::DROP_INDEX;

/* Report an InnoDB error to the client by invoking my_error(). */
static UNIV_COLD __attribute__((nonnull))
void
my_error_innodb(
/*============*/
	int		error,	/*!< in: InnoDB error code */
	const char*	table,	/*!< in: table name */
	ulint		flags)	/*!< in: table flags */
{
	switch (static_cast<enum db_err>(error)) {
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
	case DB_PRIMARY_KEY_IS_NULL:
		my_error(ER_PRIMARY_CANT_HAVE_NULL, MYF(0));
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
#ifdef UNIV_DEBUG
	case DB_SUCCESS:
	case DB_DUPLICATE_KEY:
	case DB_TABLESPACE_ALREADY_EXISTS:
	case DB_ONLINE_LOG_TOO_BIG:
		/* These codes should not be passed here. */
		ut_error;
#endif /* UNIV_DEBUG */
	default:
		my_error(ER_GET_ERRNO, MYF(0), error);
		break;
	}
}

/** Check if InnoDB supports a particular alter table in-place
@param altered_table	TABLE object for new version of table.
@param ha_alter_info	Structure describing changes to be done
by ALTER TABLE and holding data used during in-place alter.

@retval HA_ALTER_INPLACE_NOT_SUPPORTED	Not supported
@retval HA_ALTER_INPLACE_EXCLUSIVE_LOCK	Supported, but requires X-lock
@retval HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE	Supported, prepare phase
(any transactions that have modified the table must commit or roll back
first, and no transactions can start modifying the table while
prepare_inplace_alter_table() is executing)
*/
UNIV_INTERN
enum_alter_inplace_result
ha_innobase::check_if_supported_inplace_alter(
/*==========================================*/
	TABLE*			altered_table,
	Alter_inplace_info*	ha_alter_info)
{
	DBUG_ENTER("check_if_supported_inplace_alter");

	HA_CREATE_INFO* create_info = ha_alter_info->create_info;

	if (ha_alter_info->handler_flags
	    & ~(INNOBASE_ONLINE_OPERATIONS
		| Alter_inplace_info::ADD_PK_INDEX)) {
		DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
	}

	if (ha_alter_info->handler_flags
	    & Alter_inplace_info::CHANGE_CREATE_OPTION) {
		/* Changing ROW_FORMAT or KEY_BLOCK_SIZE should
		rebuild the table. TODO: copy index-by-index instead
		of row-by-row. */

		if (create_info->used_fields & HA_CREATE_USED_ROW_FORMAT
		    && create_info->row_type != get_row_type()) {
			DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
		}

		if (create_info->used_fields & HA_CREATE_USED_KEY_BLOCK_SIZE) {
			DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
		}
	}

	/* InnoDB cannot IGNORE when creating unique indexes. IGNORE
	should silently delete some duplicate rows. Our inplace_alter
	code will not delete anything from existing indexes. */
	if (ha_alter_info->ignore
	    && (ha_alter_info->handler_flags
		& (Alter_inplace_info::ADD_PK_INDEX
		   | Alter_inplace_info::ADD_UNIQUE_INDEX))) {
		DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
	}

	update_thd();

	/* Fix the key parts. */
	for (KEY* new_key = ha_alter_info->key_info_buffer;
	     new_key < ha_alter_info->key_info_buffer
		     + ha_alter_info->key_count;
	     new_key++) {
		for (KEY_PART_INFO* key_part = new_key->key_part;
		     key_part < new_key->key_part + new_key->key_parts;
		     key_part++) {
			key_part->field = table->field[key_part->fieldnr];
			/* TODO: Use altered_table for ADD_COLUMN.
			What to do with ADD_COLUMN|DROP_COLUMN? */

			if (dict_table_get_n_user_cols(prebuilt->table)
			    <= key_part->fieldnr) {
				/* This should never occur, unless
				the .frm file gets out of sync with
				the InnoDB data dictionary. */
				sql_print_warning(
					"InnoDB table '%s' has %u columns, "
					"MySQL table '%s' has %u.",
					prebuilt->table->name,
					(unsigned) dict_table_get_n_user_cols(
						prebuilt->table),
					table->s->table_name.str,
					table->s->fields);
				ut_ad(0);
				DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
			}

			dict_col_t* col = dict_table_get_nth_col(
				prebuilt->table, key_part->fieldnr);

			if (!(col->prtype & DATA_NOT_NULL)
			    != !!key_part->field->null_ptr) {
				sql_print_warning(
					"InnoDB table '%s' column '%s' "
					"attributes differ from "
					"MySQL '%s'.'%s'.",
					prebuilt->table->name,
					dict_table_get_col_name(
						prebuilt->table,
						key_part->fieldnr),
					table->s->table_name.str,
					key_part->field->field_name);
				ut_ad(0);
				DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
			}
		}
	}

	/* TODO: Reject if creating a fulltext index and there is an
	incompatible FTS_DOC_ID or FTS_DOC_ID_INDEX, either in the table
	or in the ha_alter_info. We can and should reject this before
	locking the data dictionary. */

	/* Creating the primary key requires an exclusive lock on the
	table during the whole copying operation. */
	if (ha_alter_info->handler_flags & Alter_inplace_info::ADD_PK_INDEX) {
		DBUG_RETURN(HA_ALTER_INPLACE_EXCLUSIVE_LOCK);
	}

	if (ha_alter_info->handler_flags
	    & (Alter_inplace_info::ADD_INDEX
	       | Alter_inplace_info::ADD_UNIQUE_INDEX)) {
		/* Building a full-text index requires an exclusive lock. */

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
				DBUG_RETURN(HA_ALTER_INPLACE_EXCLUSIVE_LOCK);
			}
		}
	}

	/* All other operations (create index, drop index, etc.) can
	be perfomed without blocking others in inplace_alter_table(). */
	DBUG_RETURN(HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE);
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
		/* Store a pointer to the BLOB buffer to dest: the BLOB was
		already copied to the buffer in row_sel_store_mysql_rec */

		row_mysql_store_blob_ref(dest, flen, data, len);
		break;

#ifdef UNIV_DEBUG
	case DATA_MYSQL:
		ut_ad(flen >= len);
		ut_ad(DATA_MBMAXLEN(col->mbminmaxlen)
		      >= DATA_MBMINLEN(col->mbminmaxlen));
		ut_ad(DATA_MBMAXLEN(col->mbminmaxlen)
		      > DATA_MBMINLEN(col->mbminmaxlen) || flen == len);
		memcpy(dest, data, len);
		break;

	default:
	case DATA_SYS_CHILD:
	case DATA_SYS:
		/* These column types should never be shipped to MySQL. */
		ut_ad(0);

	case DATA_CHAR:
	case DATA_FIXBINARY:
	case DATA_FLOAT:
	case DATA_DOUBLE:
	case DATA_DECIMAL:
		/* Above are the valid column types for MySQL data. */
		ut_ad(flen == len);
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
	TABLE*			table,		/*!< in/out: MySQL table */
	const rec_t*		rec,		/*!< in: record */
	const dict_index_t*	index,		/*!< in: index */
	const ulint*		offsets)	/*!< in: rec_get_offsets(
						rec, index, ...) */
{
	uint	n_fields	= table->s->fields;
	uint	i;

	ut_ad(n_fields == dict_table_get_n_user_cols(index->table)
	      - !!(DICT_TF2_FLAG_IS_SET(index->table,
					DICT_TF2_FTS_HAS_DOC_ID)));

	for (i = 0; i < n_fields; i++) {
		Field*		field	= table->field[i];
		ulint		ipos;
		ulint		ilen;
		const uchar*	ifield;

		field->reset();

		ipos = dict_index_get_nth_col_pos(index, i);

		if (UNIV_UNLIKELY(ipos == ULINT_UNDEFINED)) {
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

/******************************************************************//**
Removes the filename encoding of a database and table name. */
static
void
innobase_convert_tablename(
/*=======================*/
	char*	s)	/*!< in: identifier; out: decoded identifier */
{
	uint	errors;

	char*	slash = strchr(s, '/');

	if (slash) {
		char*	t;
		/* Temporarily replace the '/' with NUL. */
		*slash = 0;
		/* Convert the database name. */
		strconvert(&my_charset_filename, s, system_charset_info,
			   s, slash - s + 1, &errors);

		t = s + strlen(s);
		ut_ad(slash >= t);
		/* Append a  '.' after the database name. */
		*t++ = '.';
		slash++;
		/* Convert the table name. */
		strconvert(&my_charset_filename, slash, system_charset_info,
			   t, slash - t + strlen(slash), &errors);
	} else {
		strconvert(&my_charset_filename, s,
			   system_charset_info, s, strlen(s), &errors);
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
		for (ulint i = 0; i < key.key_parts; i++) {
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
static
void
innobase_create_index_field_def(
/*============================*/
	const KEY_PART_INFO*	key_part,	/*!< in: MySQL key definition */
	mem_heap_t*		heap,		/*!< in: memory heap */
	merge_index_field_t*	index_field)	/*!< out: index field
						definition for key_part */
{
	const Field*	field;
	ibool		is_unsigned;
	ulint		col_type;

	DBUG_ENTER("innobase_create_index_field_def");

	ut_ad(key_part);
	ut_ad(index_field);

	field = key_part->field;
	ut_a(field);

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

	index_field->field_name = mem_heap_strdup(heap, field->field_name);

	DBUG_VOID_RETURN;
}

/*******************************************************************//**
Create index definition for key */
static
void
innobase_create_index_def(
/*======================*/
	const KEY*		keys,		/*!< in: key definitions */
	ulint			key_number,	/*!< in: MySQL key number */
	bool			new_clustered,	/*!< in: true if generating
						a new clustered index
						on the table */
	bool			key_clustered,	/*!< in: true if this is
						the new clustered index */
	merge_index_def_t*	index,		/*!< out: index definition */
	mem_heap_t*		heap)		/*!< in: heap where memory
						is allocated */
{
	const KEY*	key = &keys[key_number];
	ulint		i;
	ulint		len;
	ulint		n_fields = key->key_parts;
	char*		index_name;

	DBUG_ENTER("innobase_create_index_def");
	DBUG_ASSERT(!key_clustered || new_clustered);

	index->fields = static_cast<merge_index_field_t*>(
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
			      & ~(HA_FULLTEXT | HA_BINARY_PACK_KEY)));
		DBUG_ASSERT(!(key->flags & HA_NOSAME));
		DBUG_ASSERT(!index->ind_type);
		index->ind_type |= DICT_FTS;
	}

	for (i = 0; i < n_fields; i++) {
		innobase_create_index_field_def(
			&key->key_part[i], heap, &index->fields[i]);
	}

	DBUG_VOID_RETURN;
}

/*******************************************************************//**
Check whether the table has the FTS_DOC_ID column
@return TRUE if there exists the FTS_DOC_ID column */
static
ibool
innobase_fts_check_doc_id_col(
/*==========================*/
	const dict_table_t*	table,	/*!< in: table with fulltext index */
	ulint*			fts_doc_col_no)
					/*!< out: The column number for
					Doc ID, or ULINT_UNDEFINED
					if it is of wrong type */
{
	*fts_doc_col_no = ULINT_UNDEFINED;

	for (ulint i = 0; i + DATA_N_SYS_COLS < (ulint) table->n_cols; i++) {
		const char*     name = dict_table_get_col_name(table, i);

		if (strcmp(name, FTS_DOC_ID_COL_NAME) == 0) {
			const dict_col_t*       col;

			col = dict_table_get_nth_col(table, i);

			if (col->mtype != DATA_INT || col->len != 8) {
				fprintf(stderr,
					" InnoDB: %s column in table %s"
					" must be of the BIGINT datatype\n",
					FTS_DOC_ID_COL_NAME, table->name);
			} else if (!(col->prtype & DATA_NOT_NULL)) {
				fprintf(stderr,
					" InnoDB: %s column in table %s"
					" must be NOT NULL\n",
					FTS_DOC_ID_COL_NAME, table->name);

			} else if (!(col->prtype & DATA_UNSIGNED)) {
				fprintf(stderr,
					" InnoDB: %s column in table %s"
					" must be UNSIGNED\n",
					FTS_DOC_ID_COL_NAME, table->name);
			} else {
				*fts_doc_col_no = i;
			}

			return(TRUE);
		}
	}

	return(FALSE);
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
	const Alter_inplace_info*ha_alter_info,	/*!< in: alter operation */
	ulint*			fts_doc_col_no)	/*!< out: The column number for
						Doc ID, or ULINT_UNDEFINED
						if it is being created in
						ha_alter_info */
{
	const dict_index_t*	index;
	const dict_field_t*	field;

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

	if (ha_alter_info) {
		/* Check if a unique index with the name of
		FTS_DOC_ID_INDEX_NAME is being created. */
		for (uint i = 0; i < ha_alter_info->index_add_count; i++) {
			const KEY& key = ha_alter_info->key_info_buffer[
				ha_alter_info->index_add_buffer[i]];

			if (innobase_strcasecmp(
				    key.name, FTS_DOC_ID_INDEX_NAME)) {
				continue;
			}

			if ((key.flags & HA_NOSAME)
			    && key.key_parts == 1
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
		    || key->key_parts != 1
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
merge_index_def_t*
innobase_create_key_defs(
/*=====================*/
	mem_heap_t*			heap,
			/*!< in/out: memory heap where space for key
			definitions are allocated */
	const Alter_inplace_info*	ha_alter_info,
			/*!< in: alter operation */
	ulint&				n_add,
			/*!< in/out: number of indexes to be created */
	bool				got_default_clust,
			/*!< in: whether the table lacks a primary key */
	bool				add_fts_doc_id,
			/*!< in: whether we need to add new DOC ID
			column for FTS index */
	bool				add_fts_doc_idx)
			/*!< in: whether we need to add new DOC ID
			index for FTS index */
{
	merge_index_def_t*	indexdef;
	merge_index_def_t*	indexdefs;
	bool			new_primary;
	const uint*const	add
		= ha_alter_info->index_add_buffer;
	const KEY*const		key_info
		= ha_alter_info->key_info_buffer;

	DBUG_ENTER("innobase_create_key_defs");
	DBUG_ASSERT(!add_fts_doc_id || add_fts_doc_idx);
	DBUG_ASSERT(ha_alter_info->index_add_count == n_add);

	indexdef = indexdefs = static_cast<merge_index_def_t*>(
		mem_heap_alloc(
			heap, sizeof *indexdef
			* (ha_alter_info->key_count
			   + add_fts_doc_idx + got_default_clust)));

	/* If there is a primary key, it is always the first index
	defined for the innodb_table. */

	new_primary = !my_strcasecmp(system_charset_info,
				     key_info[*add].name, "PRIMARY");

	/* If there is a UNIQUE INDEX consisting entirely of NOT NULL
	columns and if the index does not contain column prefix(es)
	(only prefix/part of the column is indexed), MySQL will treat the
	index as a PRIMARY KEY unless the table already has one. */

	if (!new_primary && got_default_clust
	    && (key_info[*add].flags & HA_NOSAME)
	    && !(key_info[*add].flags & HA_KEY_HAS_PART_KEY_SEG)) {
		uint	key_part = key_info[*add].key_parts;

		new_primary = TRUE;

		while (key_part--) {
			const uint	maybe_null
				= key_info[*add].key_part[key_part].key_type
				& FIELDFLAG_MAYBE_NULL;
			DBUG_ASSERT(!maybe_null
				    == !key_info[*add].key_part[key_part].
				    field->null_ptr);

			if (maybe_null) {
				new_primary = FALSE;
				break;
			}
		}
	}

	if (new_primary || add_fts_doc_id) {
		ulint	primary_key_number;

		if (new_primary) {
			primary_key_number = *add;
		} else if (add_fts_doc_id) {
			/* If DICT_TF2_FTS_ADD_DOC_ID is set, we will need to
			rebuild the table to add the unique Doc ID column for
			FTS index. And thus the clustered index would required
			to be rebuilt. */

			if (got_default_clust) {
				/* Create the GEN_CLUST_INDEX */
				merge_index_def_t*	index = indexdef++;

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
		}

		/* Create the PRIMARY key index definition */
		innobase_create_index_def(
			key_info, primary_key_number, TRUE, TRUE,
			indexdef++, heap);

created_clustered:
		n_add = 1;

		for (ulint i = 0; i < ha_alter_info->key_count; i++) {
			if (i == primary_key_number) {
				continue;
			}
			/* Copy the index definitions. */
			innobase_create_index_def(
				key_info, i, TRUE, FALSE,
				indexdef++, heap);
			n_add++;
		}
	} else {
		/* Create definitions for added secondary indexes. */

		for (ulint i = 0; i < n_add; i++) {
			innobase_create_index_def(
				key_info, add[i], FALSE, FALSE,
				indexdef++, heap);
		}
	}

	DBUG_ASSERT(indexdefs + n_add == indexdef);

	if (add_fts_doc_idx) {
		merge_index_def_t*	index = indexdef++;

		index->fields = static_cast<merge_index_field_t*>(
			mem_heap_alloc(heap, sizeof *index->fields));
		index->n_fields = 1;
		index->fields->prefix_len = 0;
		index->fields->field_name = mem_heap_strdup(
			heap, FTS_DOC_ID_COL_NAME);
		index->ind_type = DICT_UNIQUE;
		if (new_primary || add_fts_doc_id) {
			index->name = mem_heap_strdup(
				heap, FTS_DOC_ID_INDEX_NAME);
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
static
bool
innobase_check_column_length(
/*=========================*/
	const dict_table_t*table,	/*!< in: table definition */
	const KEY*	key_info)	/*!< in: Indexes to be created */
{
	ulint	max_col_len = DICT_MAX_FIELD_LEN_BY_FORMAT(table);

	for (ulint key_part = 0; key_part < key_info->key_parts; key_part++) {
		if (key_info->key_part[key_part].length > max_col_len) {
			return(true);
		}
	}
	return(false);
}

/*******************************************************************//**
Create a temporary tablename using query id, thread id, and id
@return	temporary tablename */
static
char*
innobase_create_temporary_tablename(
/*================================*/
	mem_heap_t*	heap,		/*!< in: memory heap */
	char		id,		/*!< in: identifier [0-9a-zA-Z] */
	const char*     table_name)	/*!< in: table name */
{
	char*			name;
	ulint			len;
	static const char	suffix[] = "@0023 "; /* "# " */

	len = strlen(table_name);

	name = (char*) mem_heap_alloc(heap, len + sizeof suffix);
	memcpy(name, table_name, len);
	memcpy(name + len, suffix, sizeof suffix);
	name[len + (sizeof suffix - 2)] = id;

	return(name);
}

class ha_innobase_inplace_ctx : public inplace_alter_handler_ctx
{
public:
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
	/** whether to create the indexes online */
	bool		online;
	/** memory heap */
	mem_heap_t*	heap;
	/** dictionary transaction */
	trx_t*		trx;
	/** table where the indexes are being created or dropped */
	dict_table_t*	indexed_table;
	ha_innobase_inplace_ctx(dict_index_t** add_arg,
				const ulint* add_key_numbers_arg,
				ulint num_to_add_arg,
				dict_index_t** drop_arg,
				ulint num_to_drop_arg,
				bool online_arg,
				mem_heap_t* heap_arg,
				trx_t* trx_arg,
				dict_table_t* indexed_table_arg) :
		inplace_alter_handler_ctx(),
		add (add_arg), add_key_numbers (add_key_numbers_arg),
		num_to_add (num_to_add_arg),
		drop (drop_arg), num_to_drop (num_to_drop_arg),
		online (online_arg), heap (heap_arg), trx (trx_arg),
		indexed_table (indexed_table_arg) {}
	~ha_innobase_inplace_ctx() {
		mem_heap_free(heap);
	}
};

#if 1 /* TODO: enable this in WL#6049 (MDL for FK lookups) */
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

		if (key->key_parts < n_cols) {
no_match:
			continue;
		}

		for (uint j = 0; j < n_cols; j++) {
			const KEY_PART_INFO&	key_part = key->key_part[j];
			if (key_part.length < key_part.field->pack_length()) {

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
#endif

/********************************************************************//**
This is to create FTS_DOC_ID_INDEX definition on the newly added Doc ID for
the FTS indexes table
@return	dict_index_t for the FTS_DOC_ID_INDEX */
dict_index_t*
innobase_create_fts_doc_id_idx(
/*===========================*/
	dict_table_t*	indexed_table,	/*!< in: Table where indexes are
					created */
	trx_t*		trx,		/*!< in: Transaction */
	bool		new_clustered,	/*!< in: true if creating a
					clustered index */
	mem_heap_t*     heap)		/*!< Heap for index definitions */
{
	dict_index_t*		index;
	merge_index_def_t	fts_index_def;
	char*			index_name;

	/* Create the temp index name for FTS_DOC_ID_INDEX */
	fts_index_def.name = index_name = static_cast<char*>(
		mem_heap_alloc(
			heap, FTS_DOC_ID_INDEX_NAME_LEN + 1 + !new_clustered));
	if (!new_clustered) {
		*index_name++ = TEMP_INDEX_PREFIX;
	}
	memcpy(index_name, FTS_DOC_ID_INDEX_NAME,
	       FTS_DOC_ID_INDEX_NAME_LEN);
	index_name[FTS_DOC_ID_INDEX_NAME_LEN] = 0;

	/* Only the Doc ID will be indexed */
	fts_index_def.n_fields = 1;
	fts_index_def.ind_type = DICT_UNIQUE;
	fts_index_def.fields = static_cast<merge_index_field_t*>(
		mem_heap_alloc(heap, sizeof *fts_index_def.fields));
	fts_index_def.fields[0].prefix_len = 0;
	fts_index_def.fields[0].field_name = mem_heap_strdup(
		heap, FTS_DOC_ID_COL_NAME);

	index = row_merge_create_index(trx, indexed_table, &fts_index_def);
	return(index);
}

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
		trx_t*	trx	= innobase_trx_allocate(user_thd);
		trx_start_if_not_started(trx);

		row_mysql_lock_data_dictionary(trx);
		/* Flag this transaction as a dictionary operation, so that
		the data dictionary will be locked in crash recovery. */
		trx_set_dict_operation(trx, TRX_DICT_OP_INDEX);
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
		/* Re-use the dictionary transaction object
		to avoid some memory allocation overhead. */
		ut_ad(trx_get_dict_operation(trx) == TRX_DICT_OP_TABLE);
		trx->dict_operation = TRX_DICT_OP_INDEX;
		trx->table_id = 0;
		trx_start_if_not_started(trx);
		online_retry_drop_indexes_low(table, trx);
		trx_commit_for_mysql(trx);
	}
}

/** Update internal structures with concurrent writes blocked,
while preparing ALTER TABLE.

@param ha_alter_info	Data used during in-place alter
@param altered_table	MySQL table that is being altered
@param user_table	InnoDB table that is being altered
@param user_trx		User transaction, for locking the table
@param table_name	Table name in MySQL
@param heap		Memory heap, or NULL
@param drop_index	Indexes to be dropped, or NULL
@param n_drop_index	Number of indexes to drop
@param num_fts_index	Number of full-text indexes to create
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
	dict_table_t*		user_table,
	trx_t*			user_trx,
	const char*		table_name,
	mem_heap_t*		heap,
	dict_index_t**		drop_index,
	ulint			n_drop_index,
	unsigned		num_fts_index,
	bool			add_fts_doc_id,
	bool			add_fts_doc_id_idx)
{
	trx_t*			trx;
	ibool			dict_locked	= FALSE;
	dict_index_t**		add_index;	/* indexes to be created */
	ulint*			add_key_nums;	/* MySQL key numbers */
	ulint			n_add_index;
	merge_index_def_t*	index_defs;	/* index definitions */
	dict_index_t*		fts_index	= NULL;
	dict_table_t*		indexed_table	= user_table;
	ulint			new_clustered	= 0;
	int			error;
	THD*			user_thd	= static_cast<THD*>(
		user_trx->mysql_thd);

	DBUG_ENTER("prepare_inplace_alter_table_dict");
	DBUG_ASSERT(!n_drop_index == !drop_index);
	DBUG_ASSERT(num_fts_index <= 1);
	DBUG_ASSERT(!add_fts_doc_id || add_fts_doc_id_idx);

	trx_start_if_not_started_xa(user_trx);

	/* Create a background transaction for the operations on
	the data dictionary tables. */
	trx = innobase_trx_allocate(user_thd);
	trx_start_if_not_started(trx);
	if (!heap) {
		heap = mem_heap_create(1024);
	}

	/* Create table containing all indexes to be built in this
	ALTER TABLE ADD INDEX so that they are in the correct order
	in the table. */

	n_add_index = ha_alter_info->index_add_count;

	index_defs = innobase_create_key_defs(
		heap, ha_alter_info, n_add_index,
		row_table_got_default_clust_index(indexed_table),
		add_fts_doc_id, add_fts_doc_id_idx);

	new_clustered = DICT_CLUSTERED & index_defs[0].ind_type;

	/* The primary index would be rebuilt if a FTS Doc ID
	column is to be added, and the primary index definition
	is just copied from old table and stored in indexdefs[0] */
	DBUG_ASSERT(!add_fts_doc_id || new_clustered);

	/* A primary key index cannot be created online. The table
	should be exclusively locked in this case. It should also
	be exclusively locked when a full-text index is being created. */
	DBUG_ASSERT(!!new_clustered
		    == ((ha_alter_info->handler_flags
			 & Alter_inplace_info::ADD_PK_INDEX)
			|| add_fts_doc_id));

	/* Allocate memory for dictionary index definitions */

	add_index = (dict_index_t**) mem_heap_alloc(
		heap, n_add_index * sizeof *add_index);
	add_key_nums = (ulint*) mem_heap_alloc(
		heap, n_add_index * sizeof *add_key_nums);

	/* Flag this transaction as a dictionary operation, so that
	the data dictionary will be locked in crash recovery. */
	trx_set_dict_operation(trx, TRX_DICT_OP_INDEX);

	/* Acquire a lock on the table before creating any indexes. */
	if (new_clustered) {
		error = row_merge_lock_table(
			user_trx, indexed_table, LOCK_X);

		if (error != DB_SUCCESS) {

			goto error_handling;
		}
	} else {
		error = DB_SUCCESS;
	}

	/* Latch the InnoDB data dictionary exclusively so that no deadlocks
	or lock waits can happen in it during an index create operation. */

	row_mysql_lock_data_dictionary(trx);
	dict_locked = TRUE;

	online_retry_drop_indexes_low(indexed_table, trx);

	ut_d(dict_table_check_for_dup_indexes(
		     indexed_table, CHECK_ABORTED_OK));

	/* If a new clustered index is defined for the table we need
	to drop the original table and rebuild all indexes. */

	if (new_clustered) {
		char*	new_table_name = innobase_create_temporary_tablename(
			heap, '1', indexed_table->name);
		ulint	flags;
		ulint	flags2;
		ulint	n_cols;

		if (!innobase_table_flags(table_name, altered_table,
					  ha_alter_info->create_info,
					  static_cast<THD*>(trx->mysql_thd),
					  srv_file_per_table,
					  &flags, &flags2)) {
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

		if (add_fts_doc_id_idx) {
			DBUG_ASSERT(flags2 & DICT_TF2_FTS);
			DBUG_ASSERT(flags2 & DICT_TF2_FTS_HAS_DOC_ID);
		}

		/* Create the table. */
		trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);
		/* The initial space id 0 may be overridden later. */
		indexed_table = dict_mem_table_create(
			new_table_name, 0, n_cols, flags, flags2);

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

			if (!col_type) {
col_fail:
				my_error(ER_WRONG_KEY_COLUMN, MYF(0),
					 field->field_name);
				goto new_clustered_failed;
			}

			/* we assume in dtype_form_prtype() that this
			fits in two bytes */
			ut_a(field_type <= MAX_CHAR_COLL_NUM);

			if (!field->null_ptr) {
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
					goto col_fail;
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
			indexed_table->fts->doc_col = altered_table->s->fields;
		}

		error = row_create_table_for_mysql(indexed_table, trx);

		switch (error) {
			dict_table_t*	temp_table;
		case DB_SUCCESS:
			/* We need to bump up the table ref count and
			before we can use it we need to open the
			table. The new_table must be in the data
			dictionary cache, because we are still holding
			the dict_sys->mutex. */
			ut_ad(mutex_own(&dict_sys->mutex));
			temp_table = dict_table_open_on_name_no_stats(
				indexed_table->name, TRUE, FALSE,
				DICT_ERR_IGNORE_NONE);
			ut_a(indexed_table == temp_table);
			/* n_ref_count must be 1, because purge cannot
			be executing on this very table as we are
			holding dict_operation_lock X-latch. */
			DBUG_ASSERT(indexed_table->n_ref_count == 1);
			break;
		case DB_TABLESPACE_ALREADY_EXISTS:
		case DB_DUPLICATE_KEY:
			innobase_convert_tablename(new_table_name);
			my_error(HA_ERR_TABLE_EXIST, MYF(0),
				 new_table_name);
			error = HA_ERR_TABLE_EXIST;
			goto new_clustered_failed;
		default:
			my_error_innodb(
				trx->error_state, table_name, flags);
			error = -1;
		new_clustered_failed:
			DBUG_ASSERT(trx != user_trx);
			trx_rollback_to_savepoint(trx, NULL);

			ut_ad(user_table->n_ref_count == 1);

			online_retry_drop_indexes_with_trx(user_table, trx);

			row_mysql_unlock_data_dictionary(trx);
			mem_heap_free(heap);

			trx_free_for_mysql(trx);
			trx_commit_for_mysql(user_trx);
			DBUG_RETURN(true);
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
		will be exclusively locked anyway, the modification
		log is unnecessary. */
		if (!num_fts_index
		    && !(ha_alter_info->handler_flags
			 & ~INNOBASE_ONLINE_OPERATIONS)) {
			DBUG_EXECUTE_IF("innodb_OOM_prepare_inplace_alter",
					error = DB_OUT_OF_MEMORY;
					goto error_handling;);
			rw_lock_x_lock(&add_index[num_created]->lock);
			bool ok = row_log_allocate(add_index[num_created]);
			rw_lock_x_unlock(&add_index[num_created]->lock);

			if (!ok) {
				error = DB_OUT_OF_MEMORY;
				goto error_handling;
			}
		}
	}

	if (fts_index) {
#ifdef UNIV_DEBUG
		/* Ensure that the dictionary operation mode will
		not change while creating the auxiliary tables. */
		enum trx_dict_op	op = trx_get_dict_operation(trx);

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

		error = fts_create_index_tables(trx, fts_index);

		if (error != DB_SUCCESS) {
			goto error_handling;
		}

		if (!indexed_table->fts
		    || ib_vector_size(indexed_table->fts->indexes) == 0) {
			error = fts_create_common_tables(
				trx, indexed_table, user_table->name, TRUE);
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

		if (indexed_table != user_table && user_table->fts) {
			indexed_table->fts->doc_col
				= user_table->fts->doc_col;
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
	dict_locked = FALSE;

	ut_a(trx->lock.n_active_thrs == 0);

	if (new_clustered) {
		/* A clustered index is to be built.  Acquire an exclusive
		table lock also on the table that is being created. */
		DBUG_ASSERT(indexed_table != user_table);

		error = row_merge_lock_table(user_trx, indexed_table, LOCK_X);
	}

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
			add_index, add_key_nums, n_add_index,
			drop_index, n_drop_index,
			!new_clustered && !num_fts_index,
			heap, trx, indexed_table);
		DBUG_RETURN(false);
	case DB_TABLESPACE_ALREADY_EXISTS:
		my_error(ER_TABLE_EXISTS_ERROR, MYF(0), "(unknown)");
		break;
	case DB_DUPLICATE_KEY:
		my_error(ER_DUP_KEY, MYF(0), "SYS_INDEXES");
		break;
	default:
		my_error_innodb(error, table_name, user_table->flags);
	}

	user_trx->error_info = NULL;
	trx->error_state = DB_SUCCESS;

	if (!dict_locked) {
		row_mysql_lock_data_dictionary(trx);
	}

	if (new_clustered) {
		if (indexed_table != user_table) {
			dict_table_close(indexed_table, TRUE, FALSE);
			row_merge_drop_table(trx, indexed_table);
		}

		trx_commit_for_mysql(trx);
		/* n_ref_count must be 1, because purge cannot
		be executing on this very table as we are
		holding dict_operation_lock X-latch. */
		DBUG_ASSERT(user_table->n_ref_count == 1);

		online_retry_drop_indexes_with_trx(user_table, trx);
	} else {
		ut_ad(indexed_table == user_table);
		row_merge_drop_indexes(trx, user_table, TRUE);
		trx_commit_for_mysql(trx);
	}

	ut_d(dict_table_check_for_dup_indexes(user_table, CHECK_ALL_COMPLETE));
	ut_ad(!user_table->drop_aborted);
	row_mysql_unlock_data_dictionary(trx);

	trx_free_for_mysql(trx);
	mem_heap_free(heap);

	/* There might be work for utility threads.*/
	srv_active_wake_master_thread();

	DBUG_RETURN(true);
}

/** Allows InnoDB to update internal structures with concurrent
writes blocked. Invoked before inplace_alter_table().

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
	dict_table_t*	indexed_table;	/*!< Table where indexes are created */
	mem_heap_t*     heap;
	int		error;
	ulint		num_fts_index;
	bool		add_fts_doc_id		= false;
	bool		add_fts_doc_id_idx	= false;

	DBUG_ENTER("prepare_inplace_alter_table");
	DBUG_ASSERT(!ha_alter_info->handler_ctx);
	DBUG_ASSERT(ha_alter_info->create_info);

	MONITOR_ATOMIC_INC(MONITOR_PENDING_ALTER_TABLE);

	if (!(ha_alter_info->handler_flags & ~INNOBASE_INPLACE_IGNORE)) {
		/* Nothing to do */
		goto func_exit;
	}

	if (srv_created_new_raw || srv_force_recovery) {
		my_error(ER_OPEN_AS_READONLY, MYF(0),
			 table->s->table_name.str);
		DBUG_RETURN(true);
	}

	ut_d(mutex_enter(&dict_sys->mutex));
	ut_d(dict_table_check_for_dup_indexes(
		     prebuilt->table, CHECK_ABORTED_OK));
	ut_d(mutex_exit(&dict_sys->mutex));

	update_thd();

	/* In case MySQL calls this in the middle of a SELECT query, release
	possible adaptive hash latch to avoid deadlocks of threads. */
	trx_search_latch_release_if_reserved(prebuilt->trx);

	/* Check if any index name is reserved. */
	if (innobase_index_name_is_reserved(
		    user_thd,
		    ha_alter_info->key_info_buffer,
		    ha_alter_info->key_count)) {
err_exit_no_heap:
		online_retry_drop_indexes(prebuilt->table, user_thd);
		DBUG_RETURN(true);
	}

	indexed_table = prebuilt->table;

	/* Check that index keys are sensible */
	error = innobase_check_index_keys(ha_alter_info, indexed_table);

	if (error) {
		goto err_exit_no_heap;
	}

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
				      & ~(HA_FULLTEXT | HA_BINARY_PACK_KEY)));
			continue;
		}

		if (innobase_check_column_length(indexed_table, key)) {
			my_error(ER_INDEX_COLUMN_TOO_LONG, MYF(0),
				 DICT_MAX_FIELD_LEN_BY_FORMAT_FLAG(
					 indexed_table->flags));
			goto err_exit_no_heap;
		}
	}

	n_drop_index = 0;

	if (ha_alter_info->index_drop_count) {
		DBUG_ASSERT(ha_alter_info->handler_flags
			    & (Alter_inplace_info::DROP_INDEX
			       | Alter_inplace_info::DROP_UNIQUE_INDEX));
		/* check which indexes to drop */
		heap = mem_heap_create(1024);
		drop_index = (dict_index_t**) mem_heap_alloc(
			heap, ha_alter_info->index_drop_count
			* sizeof *drop_index);
		for (uint i = 0; i < ha_alter_info->index_drop_count; i++) {
			const KEY*	key
				= ha_alter_info->index_drop_buffer[i];
			dict_index_t*	index
				= dict_table_get_index_on_name_and_min_id(
					indexed_table, key->name);

			if (!index) {
				push_warning_printf(
					user_thd,
					Sql_condition::WARN_LEVEL_WARN,
					HA_ERR_WRONG_INDEX,
					"InnoDB could not find key "
					"with name %s", key->name);
			} else if (dict_index_is_clust(index)) {
				my_error(ER_REQUIRES_PRIMARY_KEY, MYF(0));
				goto err_exit;
			} else {
				drop_index[n_drop_index++] = index;
			}
		}

		/* Check if the indexes can be dropped */

		if (prebuilt->trx->check_foreigns) {
			for (uint i = 0; i < n_drop_index; i++) {
				dict_index_t*	index = drop_index[i];
				dict_foreign_t*	foreign;

				/* TODO: skip foreign keys that are to
				be dropped */

				/* Check if the index is referenced. */
				foreign = dict_table_get_referenced_constraint(
					indexed_table, index);

				ut_ad(!foreign || indexed_table
				      == foreign->referenced_table);

				if (foreign
#if 1 /* TODO: enable this in WL#6049 (MDL for FK lookups) */
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
#endif
				    ) {
index_needed:
					prebuilt->trx->error_info = index;
					print_error(HA_ERR_DROP_INDEX_FK,
						    MYF(0));
					goto err_exit;
				}

				/* Check if this index references some
				other table */
				foreign = dict_table_get_foreign_constraint(
					indexed_table, index);

				ut_ad(!foreign || indexed_table
				      == foreign->foreign_table);

				if (foreign
#if 1 /* TODO: enable this in WL#6049 (MDL for FK lookups) */
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
#endif
				    ) {
					goto index_needed;
				}
			}
		}
	} else {
		drop_index = NULL;
		heap = NULL;
	}

	if (!(ha_alter_info->handler_flags & INNOBASE_INPLACE_CREATE)) {
		if (heap) {
			ha_alter_info->handler_ctx
				= new ha_innobase_inplace_ctx(
					NULL, NULL, 0,
					drop_index, n_drop_index, true,
					heap, NULL, indexed_table);
		}

func_exit:
		online_retry_drop_indexes(prebuilt->table, user_thd);
		DBUG_RETURN(false);
	}

	/* See if we are creating any full-text indexes. */
	num_fts_index = 0;

	for (ulint i = 0; i < ha_alter_info->index_add_count; i++) {
		const KEY* key = &ha_alter_info->key_info_buffer[
			ha_alter_info->index_add_buffer[i]];

		if (key->flags & HA_FULLTEXT) {
			DBUG_ASSERT(!(key->flags & HA_KEYFLAG_MASK
				      & ~(HA_FULLTEXT | HA_BINARY_PACK_KEY)));
			num_fts_index++;
		}
	}

	if (num_fts_index > 1) {
		my_error(ER_INNODB_FT_LIMIT, MYF(0));
err_exit:
		if (heap) {
			mem_heap_free(heap);
		}
		goto err_exit_no_heap;
	}

	/* If we are to build a full-text search index, check whether
	the table already has a DOC ID column.  If not, we will need to
	add a Doc ID hidden column and rebuild the primary index */
	if (num_fts_index) {
		ulint	doc_col_no;
		ulint	fts_doc_col_no;

		if (!innobase_fts_check_doc_id_col(
			    prebuilt->table, &fts_doc_col_no)) {
			add_fts_doc_id = true;
			add_fts_doc_id_idx = true;

			push_warning_printf(
				user_thd,
				Sql_condition::WARN_LEVEL_WARN,
				HA_ERR_WRONG_INDEX,
				"InnoDB rebuilding table to add column "
				FTS_DOC_ID_COL_NAME);
		} else if (fts_doc_col_no == ULINT_UNDEFINED) {
			my_error(ER_INNODB_FT_WRONG_DOCID_COLUMN, MYF(0),
				 FTS_DOC_ID_COL_NAME);
			goto err_exit;
		} else if (!prebuilt->table->fts) {
			prebuilt->table->fts = fts_create(prebuilt->table);
			prebuilt->table->fts->doc_col = fts_doc_col_no;
		}

		switch (innobase_fts_check_doc_id_index(
				prebuilt->table, ha_alter_info, &doc_col_no)) {
		case FTS_NOT_EXIST_DOC_ID_INDEX:
			add_fts_doc_id_idx = true;
			break;
		case FTS_INCORRECT_DOC_ID_INDEX:
			my_error(ER_INNODB_FT_WRONG_DOCID_INDEX, MYF(0),
				 FTS_DOC_ID_INDEX_NAME);
			if (prebuilt->table->fts) {
				fts_free(prebuilt->table);
			}
			goto err_exit;
		case FTS_EXIST_DOC_ID_INDEX:
			DBUG_ASSERT(doc_col_no == fts_doc_col_no
				    || doc_col_no == ULINT_UNDEFINED);
		}
	}

	DBUG_ASSERT(user_thd == prebuilt->trx->mysql_thd);
	DBUG_RETURN(prepare_inplace_alter_table_dict(
			    ha_alter_info, altered_table, prebuilt->table,
			    prebuilt->trx, table_share->table_name.str,
			    heap, drop_index, n_drop_index, num_fts_index,
			    add_fts_doc_id, add_fts_doc_id_idx));
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
	ulint	error;

	DBUG_ENTER("inplace_alter_table");
#ifdef UNIV_SYNC_DEBUG
	ut_ad(!rw_lock_own(&dict_operation_lock, RW_LOCK_EX));
	ut_ad(!rw_lock_own(&dict_operation_lock, RW_LOCK_SHARED));
#endif /* UNIV_SYNC_DEBUG */

	if (!(ha_alter_info->handler_flags & INNOBASE_INPLACE_CREATE)) {
		DBUG_RETURN(false);
	}

	update_thd();
	trx_search_latch_release_if_reserved(prebuilt->trx);

	class ha_innobase_inplace_ctx*	ctx
		= static_cast<class ha_innobase_inplace_ctx*>
		(ha_alter_info->handler_ctx);

	DBUG_ASSERT(ctx);
	DBUG_ASSERT(ctx->trx);

	/* Read the clustered index of the table and build
	indexes based on this information using temporary
	files and merge sort. */
	DBUG_EXECUTE_IF("innodb_OOM_inplace_alter",
			error = DB_OUT_OF_MEMORY; goto oom;);
	error = row_merge_build_indexes(
		prebuilt->trx,
		prebuilt->table, ctx->indexed_table,
		ctx->online,
		ctx->add, ctx->add_key_numbers, ctx->num_to_add, table);
#ifndef DBUG_OFF
oom:
#endif /* !DBUG_OFF */

	/* After an error, remove all those index definitions
	from the dictionary which were defined. */

	switch (error) {
		KEY*	dup_key;
	case DB_SUCCESS:
		ut_d(mutex_enter(&dict_sys->mutex));
		ut_d(dict_table_check_for_dup_indexes(
			     prebuilt->table, CHECK_PARTIAL_OK));
		ut_d(mutex_exit(&dict_sys->mutex));
		/* n_ref_count must be 1, or 2 when purge
		happens to be executing on this very table. */
		DBUG_ASSERT(ctx->indexed_table == prebuilt->table
			    || prebuilt->table->n_ref_count - 1 <= 1);
		DBUG_RETURN(false);
	case DB_DUPLICATE_KEY:
		if (prebuilt->trx->error_key_num == ULINT_UNDEFINED) {
			/* This should be the hidden index on FTS_DOC_ID. */
			dup_key = NULL;
		} else {
			DBUG_ASSERT(prebuilt->trx->error_key_num
				    < ha_alter_info->key_count);
			dup_key = &ha_alter_info->key_info_buffer[
				prebuilt->trx->error_key_num];
		}
		print_keydup_error(dup_key);
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

	/* n_ref_count must be 1, or 2 when purge
	happens to be executing on this very table. */
	DBUG_ASSERT(ctx->indexed_table == prebuilt->table
		    || prebuilt->table->n_ref_count - 1 <= 1);
	prebuilt->trx->error_info = NULL;
	ctx->trx->error_state = DB_SUCCESS;

	DBUG_RETURN(true);
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
	TABLE_SHARE*		table_share,
	row_prebuilt_t*		prebuilt)
{
	bool	fail	= false;

	class ha_innobase_inplace_ctx*	ctx
		= static_cast<class ha_innobase_inplace_ctx*>
		(ha_alter_info->handler_ctx);

	DBUG_ENTER("rollback_inplace_alter_table");

	if (!ctx || !ctx->trx) {
		/* If we have not started a transaction yet,
		nothing has been or needs to be done. */
		goto func_exit;
	}

	/* Roll back index creation. */
	DBUG_ASSERT(ha_alter_info->handler_flags & INNOBASE_INPLACE_CREATE);

	row_mysql_lock_data_dictionary(ctx->trx);

	if (prebuilt->table != ctx->indexed_table) {
		/* Drop the table. */
		dict_table_close(ctx->indexed_table, TRUE, FALSE);
		switch (row_merge_drop_table(ctx->trx, ctx->indexed_table)) {
		case DB_SUCCESS:
			break;
		default:
			my_error_innodb(ctx->trx->error_state,
					table_share->table_name.str,
					prebuilt->table->flags);
			fail = true;
		}
	} else {
		DBUG_ASSERT(!(ha_alter_info->handler_flags
			      & Alter_inplace_info::ADD_PK_INDEX));
		row_merge_drop_indexes(ctx->trx, prebuilt->table, FALSE);
	}

	trx_commit_for_mysql(ctx->trx);
	row_mysql_unlock_data_dictionary(ctx->trx);
	trx_free_for_mysql(ctx->trx);

func_exit:
	trx_commit_for_mysql(prebuilt->trx);
	srv_active_wake_master_thread();
	MONITOR_ATOMIC_DEC(MONITOR_PENDING_ALTER_TABLE);
	DBUG_RETURN(fail);
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
	class ha_innobase_inplace_ctx*	ctx
		= static_cast<class ha_innobase_inplace_ctx*>
		(ha_alter_info->handler_ctx);
	trx_t*				trx;
	int				err	= 0;
	bool				new_clustered;

	DBUG_ENTER("commit_inplace_alter_table");

	if (!(ha_alter_info->handler_flags & ~INNOBASE_INPLACE_IGNORE)) {
		/* Nothing to do */
		if (!commit) {
			goto ret;
		}
		/* We may want to update table attributes. */
		goto func_exit;
	}

	if (!commit) {
		/* A rollback is being requested. So far we may at
		most have created some indexes. If any indexes were to
		be dropped, they would actually be dropped in this
		method if commit=true. */
		DBUG_RETURN(rollback_inplace_alter_table(
				    ha_alter_info, table_share, prebuilt));
	}

	trx_start_if_not_started_xa(prebuilt->trx);

	if (!ctx || !ctx->trx) {
		/* Create a background transaction for the operations on
		the data dictionary tables. */
		trx = innobase_trx_allocate(user_thd);
		trx_start_if_not_started(trx);
		/* Flag this transaction as a dictionary operation, so that
		the data dictionary will be locked in crash recovery. */
		trx_set_dict_operation(trx, TRX_DICT_OP_INDEX);
		new_clustered = false;
	} else {
		trx = ctx->trx;
		new_clustered = ctx->indexed_table != prebuilt->table;
	}

	/* Latch the InnoDB data dictionary exclusively so that no deadlocks
	or lock waits can happen in it during the data dictionary operation. */
	row_mysql_lock_data_dictionary(trx);

	if (new_clustered) {
		ulint	error;
		char*	tmp_name;

		/* We copied the table. Any indexes that were
		requested to be dropped were not created in the copy
		of the table. */

		/* A new clustered index was defined for the table
		and there was no error at this point. We can
		now rename the old table as a temporary table,
		rename the new temporary table as the old
		table and drop the old table. */
		tmp_name = innobase_create_temporary_tablename(
			ctx->heap, '2', prebuilt->table->name);

		error = row_merge_rename_tables(
			prebuilt->table, ctx->indexed_table,
			tmp_name, trx);

		/* n_ref_count must be 1, because purge cannot
		be executing on this very table as we are
		holding dict_operation_lock X-latch. */
		ut_a(prebuilt->table->n_ref_count == 1);

		if (error == DB_SUCCESS) {
			dict_table_t*	old_table = prebuilt->table;
			trx_commit_for_mysql(prebuilt->trx);
			row_prebuilt_free(prebuilt, TRUE);
			error = row_merge_drop_table(trx, old_table);
			prebuilt = row_create_prebuilt(
				ctx->indexed_table, table->s->reclength);
		}

		switch (error) {
		case DB_SUCCESS:
			err = 0;
			break;
		case DB_TABLESPACE_ALREADY_EXISTS:
		case DB_DUPLICATE_KEY:
			ut_a(ctx->indexed_table->n_ref_count == 0);
			innobase_convert_tablename(tmp_name);
			my_error(ER_TABLE_EXISTS_ERROR, MYF(0), tmp_name);
			err = HA_ERR_TABLE_EXIST;
			goto drop_new_clustered;
		default:
			my_error_innodb(error,
					table_share->table_name.str,
					prebuilt->table->flags);
			err = -1;
		drop_new_clustered:
			dict_table_close(ctx->indexed_table, TRUE, FALSE);
			row_merge_drop_table(trx, ctx->indexed_table);
		}
	} else if (ctx) {
		ulint	error;
		/* We altered the table in place. */
		ulint	i;
		/* Lose the TEMP_INDEX_PREFIX. */
		for (i = 0; i < ctx->num_to_add; i++) {
			dict_index_t*	index = ctx->add[i];
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

		for (i = 0; i < ctx->num_to_drop; i++) {
			dict_index_t*	index = ctx->drop[i];
			DBUG_ASSERT(*index->name != TEMP_INDEX_PREFIX);
			DBUG_ASSERT(index->table == prebuilt->table);

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

	/* TODO: implement this */
	DBUG_ASSERT(!(ha_alter_info->handler_flags
		      & Alter_inplace_info::DROP_FOREIGN_KEY));

	trx_commit_for_mysql(trx);

	if (err == 0) {
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

			trx_start_if_not_started(trx);
			DBUG_ASSERT(trx_get_dict_operation(trx)
				    == TRX_DICT_OP_INDEX);

			for (ulint i = 0; i < ctx->num_to_drop; i++) {
				dict_index_t*	index = ctx->drop[i];
				DBUG_ASSERT(*index->name != TEMP_INDEX_PREFIX);
				DBUG_ASSERT(index->table == prebuilt->table);

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
					DBUG_ASSERT(index->type == DICT_FTS);
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

		if (!prebuilt->trx) {
			/* We created a new clustered index and committed the
			user transaction already, so that we were able to
			drop the old table. */
			update_thd();
			trx_start_if_not_started_xa(prebuilt->trx);
		}
	}

	ut_d(dict_table_check_for_dup_indexes(
		     prebuilt->table, CHECK_ABORTED_OK));
	ut_a(fts_check_cached_index(prebuilt->table));
	row_mysql_unlock_data_dictionary(trx);
	trx_free_for_mysql(trx);

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
			enum db_err	ret;
			char		errstr[1024];

			ret = dict_stats_delete_index_stats(
				prebuilt->table->name, key->name,
				prebuilt->trx,
				errstr, sizeof(errstr));

			if (ret != DB_SUCCESS) {
				push_warning(user_thd,
					     Sql_condition::WARN_LEVEL_WARN,
					     ER_LOCK_WAIT_TIMEOUT,
					     errstr);
			}
		}

		if (ctx) {
			for (uint i = 0; i < ctx->num_to_add; i++) {
				dict_index_t*	index = ctx->add[i];

				if (!(index->type & DICT_FTS)) {
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
	if (err == 0 && (ha_alter_info->create_info->used_fields
			 & HA_CREATE_USED_AUTO)) {
		dict_table_autoinc_lock(prebuilt->table);
		dict_table_autoinc_initialize(
			prebuilt->table,
			ha_alter_info->create_info->auto_increment_value);
		dict_table_autoinc_unlock(prebuilt->table);
	}

ret:
	MONITOR_ATOMIC_DEC(MONITOR_PENDING_ALTER_TABLE);
	DBUG_RETURN(err != 0);
}
