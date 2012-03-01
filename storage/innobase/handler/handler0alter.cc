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
#include <sql_lex.h>                            // SQLCOM_CREATE_INDEX
#include <mysql/innodb_priv.h>

#include "dict0stats.h"
#include "log0log.h"
#include "row0merge.h"
#include "srv0srv.h"
#include "trx0trx.h"
#include "trx0roll.h"
#include "ha_prototypes.h"
#include "handler0alter.h"
#include "srv0mon.h"
#include "fts0priv.h"

#include "ha_innodb.h"

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
	      || (DICT_TF2_FLAG_IS_SET(index->table, DICT_TF2_FTS_HAS_DOC_ID)
		  && n_fields + 1 == dict_table_get_n_user_cols(index->table)));

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
static
int
innobase_check_index_keys(
/*======================*/
	const KEY*		key_info,	/*!< in: Indexes to be
						created */
	ulint			num_of_keys,	/*!< in: Number of
						indexes to be created */
	const dict_table_t*	table)		/*!< in: Existing indexes */
{
	ulint		key_num;

	ut_ad(key_info);
	ut_ad(num_of_keys);

	for (key_num = 0; key_num < num_of_keys; key_num++) {
		const KEY&	key = key_info[key_num];

		/* Check that the same index name does not appear
		twice in indexes to be created. */

		for (ulint i = 0; i < key_num; i++) {
			const KEY&	key2 = key_info[i];

			if (0 == strcmp(key.name, key2.name)) {
				my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0),
					 key.name);

				return(ER_WRONG_NAME_FOR_INDEX);
			}
		}

		/* Check that the same index name does not already exist. */

		for (const dict_index_t* index
			     = dict_table_get_first_index(table);
		     index; index = dict_table_get_next_index(index)) {

			if (0 == strcmp(key.name, index->name)) {
				my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0),
					 key.name);

				return(ER_WRONG_NAME_FOR_INDEX);
			}
		}

		/* Check that MySQL does not try to create a column
		prefix index field on an inappropriate data type and
		that the same column does not appear twice in the index. */

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

			for (ulint j = 0; j < i; j++) {
				const KEY_PART_INFO&	key_part2
					= key.key_part[j];

				if (strcmp(key_part1.field->field_name,
					   key_part2.field->field_name)) {
					continue;
				}

				my_error(ER_WRONG_KEY_COLUMN, MYF(0),
					 key_part1.field->field_name);
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
	KEY_PART_INFO*		key_part,	/*!< in: MySQL key definition */
	mem_heap_t*		heap,		/*!< in: memory heap */
	merge_index_field_t*	index_field)	/*!< out: index field
						definition for key_part */
{
	Field*		field;
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
	KEY*			key,		/*!< in: key definition */
	bool			new_primary,	/*!< in: TRUE=generating
						a new primary key
						on the table */
	bool			key_primary,	/*!< in: TRUE if this key
						is a primary key */
	merge_index_def_t*	index,		/*!< out: index definition */
	mem_heap_t*		heap)		/*!< in: heap where memory
						is allocated */
{
	ulint	i;
	ulint	len;
	ulint	n_fields = key->key_parts;
	char*	index_name;

	DBUG_ENTER("innobase_create_index_def");

	index->fields = (merge_index_field_t*) mem_heap_alloc(
		heap, n_fields * sizeof *index->fields);

	index->ind_type = 0;
	index->n_fields = n_fields;
	len = strlen(key->name) + 1;
	index->name = index_name = (char*) mem_heap_alloc(heap,
							  len + !new_primary);

	if (UNIV_LIKELY(!new_primary)) {
		*index_name++ = TEMP_INDEX_PREFIX;
	}

	memcpy(index_name, key->name, len);

	if (key->flags & HA_NOSAME) {
		index->ind_type |= DICT_UNIQUE;
	}

	if (key->flags & HA_FULLTEXT) {
		index->ind_type |= DICT_FTS;
	}

	if (key_primary) {
		index->ind_type |= DICT_CLUSTERED;
	}

	for (i = 0; i < n_fields; i++) {
		innobase_create_index_field_def(&key->key_part[i], heap,
						&index->fields[i]);
	}

	DBUG_VOID_RETURN;
}

/*******************************************************************//**
Copy index field definition */
static
void
innobase_copy_index_field_def(
/*==========================*/
	const dict_field_t*	field,		/*!< in: definition to copy */
	merge_index_field_t*	index_field)	/*!< out: copied definition */
{
	DBUG_ENTER("innobase_copy_index_field_def");
	DBUG_ASSERT(field != NULL);
	DBUG_ASSERT(index_field != NULL);

	index_field->field_name = field->name;
	index_field->prefix_len = field->prefix_len;

	DBUG_VOID_RETURN;
}

/*******************************************************************//**
Copy index definition for the index */
static
void
innobase_copy_index_def(
/*====================*/
	const dict_index_t*	index,	/*!< in: index definition to copy */
	merge_index_def_t*	new_index,/*!< out: Index definition */
	mem_heap_t*		heap)	/*!< in: heap where allocated */
{
	ulint	n_fields;
	ulint	i;

	DBUG_ENTER("innobase_copy_index_def");

	/* Note that we take only those fields that user defined to be
	in the index.  In the internal representation more colums were
	added and those colums are not copied .*/

	n_fields = index->n_user_defined_cols;

	new_index->fields = (merge_index_field_t*) mem_heap_alloc(
		heap, n_fields * sizeof *new_index->fields);

	/* When adding a PRIMARY KEY, we may convert a previous
	clustered index to a secondary index (UNIQUE NOT NULL). */
	new_index->ind_type = index->type & ~DICT_CLUSTERED;
	new_index->n_fields = n_fields;
	new_index->name = index->name;

	for (i = 0; i < n_fields; i++) {
		innobase_copy_index_field_def(&index->fields[i],
					      &new_index->fields[i]);
	}

	DBUG_VOID_RETURN;
}

/*******************************************************************//**
Check whether the table has the FTS_DOC_ID column
@return TRUE if there exists the FTS_DOC_ID column, if TRUE but fts_doc_col_no
        equal to ULINT_UNDEFINED then that means the column exists but is not
	of the right type. */
static
ibool
innobase_fts_check_doc_id_col(
/*==========================*/
	dict_table_t*	table,		/*!< in: table with FTS index */
	ulint*		fts_doc_col_no)	/*!< out: The column number for
					Doc ID */
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
@return	FTS_EXIST_DOC_ID_INDEX if there exists the FTS_DOC_ID index,
FTS_INCORRECT_DOC_ID_INDEX if the FTS_DOC_ID index is of wrong format */
UNIV_INTERN
enum fts_doc_id_index_enum
innobase_fts_check_doc_id_index(
/*============================*/
	dict_table_t*	table,		/*!< in: table definition */
	ulint*		fts_doc_col_no)	/*!< out: The column number for
					Doc ID */
{
	dict_index_t*	index;
	dict_field_t*	field;

	for (index = dict_table_get_first_index(table);
	     index; index = dict_table_get_next_index(index)) {

		/* Check if there exists a unique index with the name of
		FTS_DOC_ID_INDEX_NAME */
		if (innobase_strcasecmp(index->name, FTS_DOC_ID_INDEX_NAME)) {
			continue;
		}

		if (!dict_index_is_unique(index)
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
	KEY *		key_info)	/*!< in: Key definition */
{
	/* Check whether there is a "FTS_DOC_ID_INDEX" in the to be built index
	list */
	for (ulint j = 0; j < n_key; j++) {
		KEY*    key = &key_info[j];

		if (innobase_strcasecmp(key->name, FTS_DOC_ID_INDEX_NAME)) {
			continue;
		}

		/* Do a check on FTS DOC ID_INDEX, it must be unique,
		named as "FTS_DOC_ID_INDEX" and on column "FTS_DOC_ID" */
		if (!(key->flags & HA_NOSAME)
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
	2) Original secondary indexes
	3) New secondary indexes

ELSE

	1) All new indexes in the order they arrive from MySQL

ENDIF


@return	key definitions or NULL */
static
merge_index_def_t*
innobase_create_key_def(
/*====================*/
	trx_t*		trx,		/*!< in: trx */
	dict_table_t*	table,		/*!< in: table definition */
	mem_heap_t*	heap,		/*!< in: heap where space for key
					definitions are allocated */
	KEY*		key_info,	/*!< in: Indexes to be created */
	ulint&		n_keys,		/*!< in/out: Number of indexes to
					be created */
	ulint*		num_fts_index,	/*!< out: Number of FTS indexes */
	ibool*		add_fts_doc_id,	/*!< out: Whether we need to add
					new DOC ID column for FTS index */
	ibool*		add_fts_doc_id_idx)/*!< out: Whether we need to add
					new index on DOC ID column */
{
	ulint			i = 0;
	merge_index_def_t*	indexdef;
	merge_index_def_t*	indexdefs;
	bool			new_primary;

	DBUG_ENTER("innobase_create_key_def");

	indexdef = indexdefs = (merge_index_def_t*)
		mem_heap_alloc(heap, sizeof *indexdef
			       * (n_keys + UT_LIST_GET_LEN(table->indexes)));

	*add_fts_doc_id = FALSE;
	*add_fts_doc_id_idx = FALSE;

	/* If there is a primary key, it is always the first index
	defined for the table. */

	new_primary = !my_strcasecmp(system_charset_info,
				     key_info->name, "PRIMARY");

	/* If there is a UNIQUE INDEX consisting entirely of NOT NULL
	columns and if the index does not contain column prefix(es)
	(only prefix/part of the column is indexed), MySQL will treat the
	index as a PRIMARY KEY unless the table already has one. */

	if (!new_primary && (key_info->flags & HA_NOSAME)
	    && (!(key_info->flags & HA_KEY_HAS_PART_KEY_SEG))
	    && row_table_got_default_clust_index(table)) {
		uint	key_part = key_info->key_parts;

		new_primary = TRUE;

		while (key_part--) {
			if (key_info->key_part[key_part].key_type
			    & FIELDFLAG_MAYBE_NULL) {
				new_primary = FALSE;
				break;
			}
		}
	}

	/* Check whether any indexes in the create list are Full
	Text Indexes*/
	for (ulint j = 0; j < n_keys; j++) {
		if (key_info[j].flags & HA_FULLTEXT) {
			(*num_fts_index)++;
		}
	}

	/* Check whether there is a "FTS_DOC_ID_INDEX" in the to be built index
	list */
	if (innobase_fts_check_doc_id_index_in_def(n_keys, key_info)
	    == FTS_INCORRECT_DOC_ID_INDEX) {
		push_warning_printf((THD*) trx->mysql_thd,
				   Sql_condition::WARN_LEVEL_WARN,
				   ER_WRONG_NAME_FOR_INDEX,
				   " InnoDB: Index name %s is reserved"
				   " for the unique index on"
				   " FTS_DOC_ID column for FTS"
				   " document ID indexing"
				   " on table %s. Please check"
				   " the index definition to"
				   " make sure it is of correct"
				   " type\n",
				   FTS_DOC_ID_INDEX_NAME,
				   table->name);
	       DBUG_RETURN(NULL);
	}

	/* If we are to build an FTS index, check whether the table
	already has a DOC ID column, if not, we will need to add a
	Doc ID hidden column and rebuild the primary index */
	if (*num_fts_index) {
		enum fts_doc_id_index_enum	ret;
		ibool				exists;
		ulint				doc_col_no;
		ulint				fts_doc_col_no;

		exists = innobase_fts_check_doc_id_col(table, &fts_doc_col_no);

		if (exists) {

			if (fts_doc_col_no == ULINT_UNDEFINED) {

				push_warning_printf(
					(THD*) trx->mysql_thd,
					Sql_condition::WARN_LEVEL_WARN,
					ER_WRONG_COLUMN_NAME,
					" InnoDB: There exists a column %s "
					"in table %s, but it is the wrong "
					"type. Create of FTS index failed.\n",
					FTS_DOC_ID_COL_NAME, table->name);

				DBUG_RETURN(NULL);

			} else if (!table->fts) {
				table->fts = fts_create(table);
			}

			table->fts->doc_col = fts_doc_col_no;

		} else {
			*add_fts_doc_id = TRUE;
			*add_fts_doc_id_idx = TRUE;

			ut_print_timestamp(stderr);
			fprintf(stderr, " InnoDB: Rebuild table %s to add "
					"DOC_ID column\n", table->name);
		}

		ret = innobase_fts_check_doc_id_index(table, &doc_col_no);

		switch (ret) {
		case FTS_NOT_EXIST_DOC_ID_INDEX:
			*add_fts_doc_id_idx = TRUE;
			break;
		case FTS_INCORRECT_DOC_ID_INDEX:

			ut_print_timestamp(stderr);
			fprintf(stderr, " InnoDB: Index %s is used for FTS"
					" Doc ID indexing on table %s, it is"
					" now on the wrong column or of"
					" wrong format. Please drop it.\n",
					FTS_DOC_ID_INDEX_NAME, table->name);
			DBUG_RETURN(NULL);

		default:
			ut_ad(ret == FTS_EXIST_DOC_ID_INDEX);

			ut_ad(doc_col_no == fts_doc_col_no);
		}
	}

	/* If DICT_TF2_FTS_ADD_DOC_ID is set, we will need to rebuild
	the table to add the unique Doc ID column for FTS index. And
	thus the primary index would required to be rebuilt. Copy all
	the index definitions */
	if (new_primary || *add_fts_doc_id) {
		const dict_index_t*	index;

		if (new_primary) {
			/* Create the PRIMARY key index definition */
			innobase_create_index_def(&key_info[i++],
						  TRUE, TRUE,
						  indexdef++, heap);
		}

		row_mysql_lock_data_dictionary(trx);

		index = dict_table_get_first_index(table);

		/* Copy the index definitions of the old table.  Skip
		the old clustered index if it is a generated clustered
		index or a PRIMARY KEY.  If the clustered index is a
		UNIQUE INDEX, it must be converted to a secondary index. */

		if (new_primary
		    && (dict_index_get_nth_col(index, 0)->mtype
			== DATA_SYS
		        || !my_strcasecmp(system_charset_info,
					  index->name, "PRIMARY"))) {
			index = dict_table_get_next_index(index);
		}

		while (index) {
			innobase_copy_index_def(index, indexdef++, heap);

			if (new_primary && index->type & DICT_FTS) {
				(*num_fts_index)++;
			}

			index = dict_table_get_next_index(index);
		}

		/* The primary index would be rebuilt if a FTS Doc ID
		column is to be added, and the primary index definition
		is just copied from old table and stored in indexdefs[0] */
		if (*add_fts_doc_id) {
			indexdefs[0].ind_type |= DICT_CLUSTERED;
			DICT_TF2_FLAG_SET(table, DICT_TF2_FTS_ADD_DOC_ID);
		}

		row_mysql_unlock_data_dictionary(trx);
	}

	/* Create definitions for added secondary indexes. */

	while (i < n_keys) {
		innobase_create_index_def(&key_info[i++], new_primary, FALSE,
					  indexdef++, heap);
	}

	n_keys = indexdef - indexdefs;

	DBUG_RETURN(indexdefs);
}

/*******************************************************************//**
Check each index column size, make sure they do not exceed the max limit
@return	HA_ERR_INDEX_COL_TOO_LONG if index column size exceeds limit */
static
int
innobase_check_column_length(
/*=========================*/
	const dict_table_t*table,	/*!< in: table definition */
	const KEY*	key_info)	/*!< in: Indexes to be created */
{
	ulint	max_col_len = DICT_MAX_FIELD_LEN_BY_FORMAT(table);

	for (ulint key_part = 0; key_part < key_info->key_parts; key_part++) {
		if (key_info->key_part[key_part].length > max_col_len) {
			my_error(ER_INDEX_COLUMN_TOO_LONG, MYF(0), max_col_len);
			return(HA_ERR_INDEX_COL_TOO_LONG);
		}
	}
	return(0);
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

class ha_innobase_add_index : public handler_add_index
{
public:
	/** table where the indexes are being created */
	dict_table_t* indexed_table;
	ha_innobase_add_index(TABLE* table, KEY* key_info, uint num_of_keys,
			      dict_table_t* indexed_table_arg) :
		handler_add_index(table, key_info, num_of_keys),
		indexed_table (indexed_table_arg) {}
	~ha_innobase_add_index() {}
};

/*******************************************************************//**
This is to create FTS_DOC_ID_INDEX definition on the newly added Doc ID for
the FTS indexes table
@return	dict_index_t for the FTS_DOC_ID_INDEX */
dict_index_t*
innobase_create_fts_doc_id_idx(
/*===========================*/
	dict_table_t*	indexed_table,	/*!< in: Table where indexes are
					created */
	trx_t*		trx,		/*!< in: Transaction */
	mem_heap_t*     heap)		/*!< Heap for index definitions */
{
	dict_index_t*		index;
	merge_index_def_t	fts_index_def;
	char*			index_name;

	/* Create the temp index name for FTS_DOC_ID_INDEX */
	fts_index_def.name = index_name = (char*) mem_heap_alloc(
		heap, FTS_DOC_ID_INDEX_NAME_LEN + 2);
	*index_name++ = TEMP_INDEX_PREFIX;
	memcpy(index_name, FTS_DOC_ID_INDEX_NAME,
	       FTS_DOC_ID_INDEX_NAME_LEN);
	index_name[FTS_DOC_ID_INDEX_NAME_LEN] = 0;

	/* Only the Doc ID will be indexed */
	fts_index_def.n_fields = 1;
	fts_index_def.ind_type = DICT_UNIQUE;
	fts_index_def.fields = (merge_index_field_t*) mem_heap_alloc(
		heap, sizeof *fts_index_def.fields);
	fts_index_def.fields[0].prefix_len = 0;
	fts_index_def.fields[0].field_name = mem_heap_strdup(
		heap, FTS_DOC_ID_COL_NAME);

	index = row_merge_create_index(trx, indexed_table, &fts_index_def);
	return(index);
}

/*******************************************************************//**
Clean up on ha_innobase::add_index error. */
static
void
innobase_add_index_cleanup(
/*=======================*/
	row_prebuilt_t*	prebuilt,		/*!< in/out: prebuilt */
	trx_t*		trx,			/*!< in/out: transaction */
	dict_table_t*	table)			/*!< in/out: table on which
						the indexes were going to be
						created */
{
	trx_rollback_to_savepoint(trx, NULL);

	ut_a(trx != prebuilt->trx);

	trx_free_for_mysql(trx);

	trx_commit_for_mysql(prebuilt->trx);

	if (table != NULL) {

		rw_lock_x_lock(&dict_operation_lock);

		dict_mutex_enter_for_mysql();

		/* Note: This check excludes the system tables. However, we
		should be safe because users cannot add indexes to system
		tables. */

		if (UT_LIST_GET_LEN(table->foreign_list) == 0
		    && UT_LIST_GET_LEN(table->referenced_list) == 0
		    && !table->can_be_evicted) {

			dict_table_move_from_non_lru_to_lru(table);
		}

		dict_table_close(table, TRUE);

		dict_mutex_exit_for_mysql();

		rw_lock_x_unlock(&dict_operation_lock);
	}
}

/*******************************************************************//**
Create indexes.
@return	0 or error number */
UNIV_INTERN
int
ha_innobase::add_index(
/*===================*/
	TABLE*			in_table,	/*!< in: Table where indexes
						are created */
	KEY*			key_info,	/*!< in: Indexes
						to be created */
	uint			num_of_keys,	/*!< in: Number of indexes
						to be created */
	handler_add_index**	add)		/*!< out: context */
{
	dict_index_t**	index = NULL;	/*!< Index to be created */
	dict_index_t*	fts_index = NULL;/*!< FTS Index to be created */
	dict_table_t*	indexed_table;	/*!< Table where indexes are created */
	merge_index_def_t* index_defs;	/*!< Index definitions */
	mem_heap_t*     heap = NULL;	/*!< Heap for index definitions */
	trx_t*		trx;		/*!< Transaction */
	ulint		num_of_idx;
	ulint		num_created	= 0;
	ibool		dict_locked	= FALSE;
	ulint		new_primary	= 0;
	int		error;
	ulint		num_fts_index	= 0;
	ulint		num_idx_create	= 0;
	ibool		fts_add_doc_id	= FALSE;
	ibool		fts_add_doc_idx	= FALSE;

	DBUG_ENTER("ha_innobase::add_index");
	ut_a(table);
	ut_a(key_info);
	ut_a(num_of_keys);

	*add = NULL;

	if (srv_created_new_raw || srv_force_recovery) {
		DBUG_RETURN(HA_ERR_WRONG_COMMAND);
	}

	update_thd();

	/* In case MySQL calls this in the middle of a SELECT query, release
	possible adaptive hash latch to avoid deadlocks of threads. */
	trx_search_latch_release_if_reserved(prebuilt->trx);

	/* Check if the index name is reserved. */
	if (innobase_index_name_is_reserved(user_thd, key_info, num_of_keys)) {
		DBUG_RETURN(-1);
	}

	indexed_table = dict_table_open_on_name(prebuilt->table->name, FALSE);

	if (UNIV_UNLIKELY(!indexed_table)) {
		DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);
	}

	ut_a(indexed_table == prebuilt->table);

	/* Check that index keys are sensible */
	error = innobase_check_index_keys(key_info, num_of_keys, prebuilt->table);

	if (UNIV_UNLIKELY(error)) {
		dict_table_close(prebuilt->table, FALSE);
		DBUG_RETURN(error);
	}

	/* Check each index's column length to make sure they do not
	exceed limit */
	for (ulint i = 0; i < num_of_keys; i++) {
		if (key_info[i].flags & HA_FULLTEXT) {
			continue;
		}

		error = innobase_check_column_length(prebuilt->table,
						     &key_info[i]);

		if (error) {
			dict_table_close(prebuilt->table, FALSE);
			DBUG_RETURN(error);
		}
	}

	heap = mem_heap_create(1024);
	trx_start_if_not_started(prebuilt->trx);

	/* Create a background transaction for the operations on
	the data dictionary tables. */
	trx = innobase_trx_allocate(user_thd);
	trx_start_if_not_started(trx);

	/* We don't want this table to be evicted from the cache while we
	are building an index on it. Another issue is that while we are
	building the index this table could be referred to in a foreign
	key relationship. In innobase_add_index_cleanup() we check for
	that condition before moving it back to the LRU list. */

	row_mysql_lock_data_dictionary(trx);

	if (prebuilt->table->can_be_evicted) {
		dict_table_move_from_lru_to_non_lru(prebuilt->table);
	}

	row_mysql_unlock_data_dictionary(trx);

	/* Create table containing all indexes to be built in this
	alter table add index so that they are in the correct order
	in the table. */

	num_of_idx = num_of_keys;

	index_defs = innobase_create_key_def(
		trx, prebuilt->table, heap, key_info, num_of_idx,
		&num_fts_index, &fts_add_doc_id, &fts_add_doc_idx);

	if (!index_defs) {
		error = DB_UNSUPPORTED;
		goto error_handling;
	}

	/* Currently, support create one single FULLTEXT index in parallel at
	a time */
	if (num_fts_index > 1) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Only support create ONE Fulltext index"
			" at a time\n");
		error = DB_UNSUPPORTED;
		goto error_handling;
	}

	new_primary = DICT_CLUSTERED & index_defs[0].ind_type;

	/* If a new FTS Doc ID column is to be added, there will be
	one additional index to be built on the Doc ID column itself. */
	num_idx_create = (fts_add_doc_idx) ? num_of_idx + 1 : num_of_idx;

	/* Allocate memory for dictionary index definitions */
	index = (dict_index_t**) mem_heap_alloc(
		heap, num_idx_create * sizeof *index);

	/* Flag this transaction as a dictionary operation, so that
	the data dictionary will be locked in crash recovery. */
	trx_set_dict_operation(trx, TRX_DICT_OP_INDEX);

	/* Acquire a lock on the table before creating any indexes. */
	error = row_merge_lock_table(prebuilt->trx, prebuilt->table,
				     new_primary ? LOCK_X : LOCK_S);

	if (UNIV_UNLIKELY(error != DB_SUCCESS)) {

		goto error_handling;
	}

	/* Latch the InnoDB data dictionary exclusively so that no deadlocks
	or lock waits can happen in it during an index create operation. */

	row_mysql_lock_data_dictionary(trx);
	dict_locked = TRUE;

	ut_d(dict_table_check_for_dup_indexes(prebuilt->table, FALSE));

	/* If a new primary key is defined for the table we need
	to drop the original table and rebuild all indexes. */

	if (UNIV_UNLIKELY(new_primary)) {
		/* This transaction should be the only one
		operating on the table. The table get above
		would have incremented the ref count to 2. */
		ut_a(prebuilt->table->n_ref_count == 2);

		char*	new_table_name = innobase_create_temporary_tablename(
			heap, '1', prebuilt->table->name);

		/* Clone the table. */
		trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);
		indexed_table = row_merge_create_temporary_table(
			new_table_name, index_defs, prebuilt->table, trx);

		if (!indexed_table) {

			switch (trx->error_state) {
			case DB_TABLESPACE_ALREADY_EXISTS:
			case DB_DUPLICATE_KEY:
				innobase_convert_tablename(new_table_name);
				my_error(HA_ERR_TABLE_EXIST, MYF(0),
					 new_table_name);
				error = HA_ERR_TABLE_EXIST;
				break;
			default:
				error = convert_error_code_to_mysql(
					trx->error_state,
					prebuilt->table->flags,
					user_thd);
			}

			ut_d(dict_table_check_for_dup_indexes(prebuilt->table,
							      FALSE));
			row_mysql_unlock_data_dictionary(trx);
			mem_heap_free(heap);

			innobase_add_index_cleanup(
				prebuilt, trx, prebuilt->table);

			DBUG_RETURN(error);
		}

		trx->table_id = indexed_table->id;
	}

	/* Create the indexes in SYS_INDEXES and load into dictionary. */

	for (num_created = 0; num_created < num_of_idx; num_created++) {

		index[num_created] = row_merge_create_index(
			trx, indexed_table, &index_defs[num_created]);

		if (!index[num_created]) {
			error = trx->error_state;
			goto error_handling;
		}

		if (index[num_created]->type & DICT_FTS) {
			fts_index = index[num_created];
			fts_create_index_tables(trx, fts_index);

		}
	}

	/* create FTS_DOC_ID_INDEX on the Doc ID column on the table */
	if (fts_add_doc_idx) {
		index[num_of_idx] = innobase_create_fts_doc_id_idx(
					       indexed_table, trx, heap);
		/* FTS_DOC_ID_INDEX is internal defined new index */
		num_of_idx++;
		num_created++;
	}

	if (num_fts_index) {
		DICT_TF2_FLAG_SET(indexed_table, DICT_TF2_FTS);

		if (!indexed_table->fts
		    || ib_vector_size(indexed_table->fts->indexes) == 0) {
			fts_create_common_tables(trx, indexed_table,
						 prebuilt->table->name, TRUE);

			indexed_table->fts->fts_status |= TABLE_DICT_LOCKED;
			innobase_fts_load_stopword(
				indexed_table, trx, ha_thd());
			indexed_table->fts->fts_status &= ~TABLE_DICT_LOCKED;
		}

		if (new_primary && prebuilt->table->fts) {
			indexed_table->fts->doc_col = prebuilt->table->fts->doc_col;
		}
	}

	ut_ad(error == DB_SUCCESS);

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

	if (UNIV_UNLIKELY(new_primary)) {
		/* A primary key is to be built.  Acquire an exclusive
		table lock also on the table that is being created. */
		ut_ad(indexed_table != prebuilt->table);

		error = row_merge_lock_table(prebuilt->trx, indexed_table,
					     LOCK_X);

		if (UNIV_UNLIKELY(error != DB_SUCCESS)) {

			goto error_handling;
		}
	}

	/* Read the clustered index of the table and build indexes
	based on this information using temporary files and merge sort. */
	error = row_merge_build_indexes(prebuilt->trx,
					prebuilt->table, indexed_table,
					index, num_of_idx, table);

error_handling:

	/* After an error, remove all those index definitions from the
	dictionary which were defined. */

	if (!dict_locked) {
		row_mysql_lock_data_dictionary(trx);
		dict_locked = TRUE;
	}

	switch (error) {
	case DB_SUCCESS:
		ut_d(dict_table_check_for_dup_indexes(prebuilt->table, TRUE));

		*add = new ha_innobase_add_index(
			table, key_info, num_of_keys, indexed_table);

		dict_table_close(prebuilt->table, dict_locked);
		break;

	case DB_TOO_BIG_RECORD:
		my_error(HA_ERR_TO_BIG_ROW, MYF(0));
		goto error_exit;
	case DB_PRIMARY_KEY_IS_NULL:
		my_error(ER_PRIMARY_CANT_HAVE_NULL, MYF(0));
		/* fall through */
	case DB_DUPLICATE_KEY:
		if (fts_add_doc_idx
		    && prebuilt->trx->error_key_num == num_of_idx - 1) {
			prebuilt->trx->error_key_num = ULINT_UNDEFINED;
		}
error_exit:
		prebuilt->trx->error_info = NULL;
		/* fall through */
	default:
		dict_table_close(prebuilt->table, dict_locked);

		trx->error_state = DB_SUCCESS;

		if (new_primary) {
			if (indexed_table != prebuilt->table) {
				dict_table_close(indexed_table, dict_locked);
				row_merge_drop_table(trx, indexed_table);
			}
		} else {
			row_merge_drop_indexes(trx, indexed_table,
					       index, num_created);
		}
	}

	ut_ad(!new_primary || prebuilt->table->n_ref_count == 1);
	trx_commit_for_mysql(trx);
	ut_ad(dict_locked);
	row_mysql_unlock_data_dictionary(trx);
	trx_free_for_mysql(trx);
	mem_heap_free(heap);

	if (prebuilt->trx) {
		trx_commit_for_mysql(prebuilt->trx);
	}

	/* There might be work for utility threads.*/
	srv_active_wake_master_thread();

	DBUG_RETURN(convert_error_code_to_mysql(error, prebuilt->table->flags,
						user_thd));
}

/*******************************************************************//**
Finalize or undo add_index().
@return	0 or error number */
UNIV_INTERN
int
ha_innobase::final_add_index(
/*=========================*/
	handler_add_index*	add_arg,/*!< in: context from add_index() */
	bool			commit)	/*!< in: true=commit, false=rollback */
{
	ha_innobase_add_index*	add;
	trx_t*			trx;
	int			err	= 0;

	DBUG_ENTER("ha_innobase::final_add_index");

	ut_ad(add_arg);
	add = static_cast<class ha_innobase_add_index*>(add_arg);

	/* Create a background transaction for the operations on
	the data dictionary tables. */
	trx = innobase_trx_allocate(user_thd);
	trx_start_if_not_started(trx);

	/* Flag this transaction as a dictionary operation, so that
	the data dictionary will be locked in crash recovery. */
	trx_set_dict_operation(trx, TRX_DICT_OP_INDEX);

	/* Latch the InnoDB data dictionary exclusively so that no deadlocks
	or lock waits can happen in it during an index create operation. */
	row_mysql_lock_data_dictionary(trx);

	if (add->indexed_table != prebuilt->table) {
		ulint	error;

		/* We copied the table (new_primary). */
		if (commit) {
			mem_heap_t*	heap;
			char*		tmp_name;

			heap = mem_heap_create(1024);

			/* A new primary key was defined for the table
			and there was no error at this point. We can
			now rename the old table as a temporary table,
			rename the new temporary table as the old
			table and drop the old table. */
			tmp_name = innobase_create_temporary_tablename(
				heap, '2', prebuilt->table->name);

			error = row_merge_rename_tables(
				prebuilt->table, add->indexed_table,
				tmp_name, trx);

			ut_a(prebuilt->table->n_ref_count == 1);

			switch (error) {
			case DB_TABLESPACE_ALREADY_EXISTS:
			case DB_DUPLICATE_KEY:
				ut_a(add->indexed_table->n_ref_count == 0);
				innobase_convert_tablename(tmp_name);
				my_error(HA_ERR_TABLE_EXIST, MYF(0), tmp_name);
				err = HA_ERR_TABLE_EXIST;
				break;
			default:
				err = convert_error_code_to_mysql(
					error, prebuilt->table->flags,
					user_thd);
				break;
			}

			mem_heap_free(heap);
		}

		if (!commit || err) {
			dict_table_close(add->indexed_table, TRUE);
			error = row_merge_drop_table(trx, add->indexed_table);
			trx_commit_for_mysql(prebuilt->trx);
		} else {
			dict_table_t*	old_table = prebuilt->table;
			trx_commit_for_mysql(prebuilt->trx);
			row_prebuilt_free(prebuilt, TRUE);
			error = row_merge_drop_table(trx, old_table);
			prebuilt = row_create_prebuilt(add->indexed_table,
				0 /* XXX Do we know the mysql_row_len here?
				Before the addition of this parameter to
				row_create_prebuilt() the mysql_row_len
				member was left 0 (from zalloc) in the
				prebuilt object. */);
		}

		err = convert_error_code_to_mysql(
			error, prebuilt->table->flags, user_thd);
	}

	if (add->indexed_table == prebuilt->table
	    || DICT_TF2_FLAG_IS_SET(prebuilt->table, DICT_TF2_FTS_ADD_DOC_ID)) {
		/* We created secondary indexes (!new_primary) or create full
		text index and added a new Doc ID column, we will need to
		rename the secondary index on the Doc ID column to its
		official index name.. */

		if (commit) {
			err = convert_error_code_to_mysql(
				row_merge_rename_indexes(trx, prebuilt->table),
				prebuilt->table->flags, user_thd);
		}

		if (!commit || err) {
			dict_index_t*	index;
			dict_index_t*	next_index;

			for (index = dict_table_get_first_index(
				     prebuilt->table);
			     index; index = next_index) {

				next_index = dict_table_get_next_index(index);

				if (*index->name == TEMP_INDEX_PREFIX) {
					row_merge_drop_index(
						index, prebuilt->table, trx);
				}
			}
		}

		DICT_TF2_FLAG_UNSET(prebuilt->table, DICT_TF2_FTS_ADD_DOC_ID);
	}

	/* If index is successfully built, we will need to rebuild index
	translation table. Set valid index entry count in the translation
	table to zero. */
	if (err == 0 && commit) {
		ibool		new_primary;
		dict_index_t*	index;
		dict_index_t*	next_index;
		ibool		new_fts = FALSE;
		dict_index_t*	primary;

		new_primary = !my_strcasecmp(
			system_charset_info, add->key_info[0].name, "PRIMARY");

		primary = dict_table_get_first_index(add->indexed_table);

		if (!new_primary) {
			new_primary = !my_strcasecmp(
				system_charset_info, add->key_info[0].name,
				primary->name);
		}

		share->idx_trans_tbl.index_count = 0;

		if (new_primary) {
			for (index = primary; index; index = next_index) {

				next_index = dict_table_get_next_index(index);

				if (index->type & DICT_FTS) {
					fts_add_index(index,
						      add->indexed_table);
					new_fts = TRUE;
				}
			}
		} else {
			ulint		i;
			for (i = 0; i < add->num_of_keys; i++) {
				if (add->key_info[i].flags & HA_FULLTEXT) {
					dict_index_t*	fts_index;

					fts_index =
						dict_table_get_index_on_name(
							prebuilt->table,
							 add->key_info[i].name);

					ut_ad(fts_index);
					fts_add_index(fts_index,
						      prebuilt->table);
					new_fts = TRUE;
				}
			}
		}

		if (new_fts) {
			fts_optimize_add_table(prebuilt->table);
		}
	}

	trx_commit_for_mysql(trx);
	if (prebuilt->trx) {
		trx_commit_for_mysql(prebuilt->trx);
	}

	ut_d(dict_table_check_for_dup_indexes(prebuilt->table, FALSE));

	ut_a(fts_check_cached_index(prebuilt->table));

	row_mysql_unlock_data_dictionary(trx);

	trx_free_for_mysql(trx);

	/* There might be work for utility threads.*/
	srv_active_wake_master_thread();

	delete add;
	DBUG_RETURN(err);
}
/*******************************************************************//**
Prepare to drop some indexes of a table.
@return	0 or error number */
UNIV_INTERN
int
ha_innobase::prepare_drop_index(
/*============================*/
	TABLE*	in_table,	/*!< in: Table where indexes are dropped */
	uint*	key_num,	/*!< in: Key nums to be dropped */
	uint	num_of_keys)	/*!< in: Number of keys to be dropped */
{
	trx_t*		trx;
	int		err = 0;
	uint		n_key;

	DBUG_ENTER("ha_innobase::prepare_drop_index");
	ut_ad(table);
	ut_ad(key_num);
	ut_ad(num_of_keys);
	if (srv_created_new_raw || srv_force_recovery) {
		DBUG_RETURN(HA_ERR_WRONG_COMMAND);
	}

	update_thd();

	trx_search_latch_release_if_reserved(prebuilt->trx);
	trx = prebuilt->trx;

	/* Test and mark all the indexes to be dropped */

	row_mysql_lock_data_dictionary(trx);
	ut_d(dict_table_check_for_dup_indexes(prebuilt->table, FALSE));

	/* Check that none of the indexes have previously been flagged
	for deletion. */
	{
		const dict_index_t*	index
			= dict_table_get_first_index(prebuilt->table);
		do {
			ut_a(!index->to_be_dropped);
			index = dict_table_get_next_index(index);
		} while (index);
	}

	for (n_key = 0; n_key < num_of_keys; n_key++) {
		const KEY*	key;
		dict_index_t*	index;

		key = table->key_info + key_num[n_key];
		index = dict_table_get_index_on_name_and_min_id(
			prebuilt->table, key->name);

		if (!index) {
			sql_print_error("InnoDB could not find key n:o %u "
					"with name %s for table %s",
					key_num[n_key],
					key ? key->name : "NULL",
					prebuilt->table->name);

			err = HA_ERR_KEY_NOT_FOUND;
			goto func_exit;
		}

		/* Refuse to drop the clustered index.  It would be
		better to automatically generate a clustered index,
		but mysql_alter_table() will call this method only
		after ha_innobase::add_index(). */

		if (dict_index_is_clust(index)) {
			my_error(ER_REQUIRES_PRIMARY_KEY, MYF(0));
			err = -1;
			goto func_exit;
		}

		rw_lock_x_lock(dict_index_get_lock(index));
		index->to_be_dropped = TRUE;
		rw_lock_x_unlock(dict_index_get_lock(index));
	}

	/* If FOREIGN_KEY_CHECKS = 1 you may not drop an index defined
	for a foreign key constraint because InnoDB requires that both
	tables contain indexes for the constraint. Such index can
	be dropped only if FOREIGN_KEY_CHECKS is set to 0.
	Note that CREATE INDEX id ON table does a CREATE INDEX and
	DROP INDEX, and we can ignore here foreign keys because a
	new index for the foreign key has already been created.

	We check for the foreign key constraints after marking the
	candidate indexes for deletion, because when we check for an
	equivalent foreign index we don't want to select an index that
	is later deleted. */

	if (trx->check_foreigns
	    && thd_sql_command(user_thd) != SQLCOM_CREATE_INDEX) {
		dict_index_t*	index;

		for (index = dict_table_get_first_index(prebuilt->table);
		     index;
		     index = dict_table_get_next_index(index)) {
			dict_foreign_t*	foreign;

			if (!index->to_be_dropped) {

				continue;
			}

			/* Check if the index is referenced. */
			foreign = dict_table_get_referenced_constraint(
				prebuilt->table, index);

			if (foreign) {
index_needed:
				trx_set_detailed_error(
					trx,
					"Index needed in foreign key "
					"constraint");

				trx->error_info = index;

				err = HA_ERR_DROP_INDEX_FK;
				break;
			} else {
				/* Check if this index references some
				other table */
				foreign = dict_table_get_foreign_constraint(
					prebuilt->table, index);

				if (foreign) {
					ut_a(foreign->foreign_index == index);

					/* Search for an equivalent index that
					the foreign key constraint could use
					if this index were to be deleted. */
					if (!dict_foreign_find_equiv_index(
						foreign)) {

						goto index_needed;
					}
				}
			}
		}
	} else if (thd_sql_command(user_thd) == SQLCOM_CREATE_INDEX) {
		/* This is a drop of a foreign key constraint index that
		was created by MySQL when the constraint was added.  MySQL
		does this when the user creates an index explicitly which
		can be used in place of the automatically generated index. */

		dict_index_t*	index;

		for (index = dict_table_get_first_index(prebuilt->table);
		     index;
		     index = dict_table_get_next_index(index)) {
			dict_foreign_t*	foreign;

			if (!index->to_be_dropped) {

				continue;
			}

			/* Check if this index references some other table */
			foreign = dict_table_get_foreign_constraint(
				prebuilt->table, index);

			if (foreign == NULL) {

				continue;
			}

			ut_a(foreign->foreign_index == index);

			/* Search for an equivalent index that the
			foreign key constraint could use if this index
			were to be deleted. */

			if (!dict_foreign_find_equiv_index(foreign)) {
				trx_set_detailed_error(
					trx,
					"Index needed in foreign key "
					"constraint");

				trx->error_info = foreign->foreign_index;

				err = HA_ERR_DROP_INDEX_FK;
				break;
			}
		}
	}

func_exit:
	if (err) {
		/* Undo our changes since there was some sort of error. */
		dict_index_t*	index
			= dict_table_get_first_index(prebuilt->table);

		do {
			rw_lock_x_lock(dict_index_get_lock(index));
			index->to_be_dropped = FALSE;
			rw_lock_x_unlock(dict_index_get_lock(index));
			index = dict_table_get_next_index(index);
		} while (index);
	}

	ut_d(dict_table_check_for_dup_indexes(prebuilt->table, FALSE));
	row_mysql_unlock_data_dictionary(trx);

	DBUG_RETURN(err);
}

/*******************************************************************//**
Drop the indexes that were passed to a successful prepare_drop_index().
@return	0 or error number */
UNIV_INTERN
int
ha_innobase::final_drop_index(
/*==========================*/
	TABLE*	        iin_table)	/*!< in: Table where indexes
					are dropped */
{
	dict_index_t*	index;		/*!< Index to be dropped */
	trx_t*		trx;		/*!< Transaction */
	int		err;

	DBUG_ENTER("ha_innobase::final_drop_index");
	ut_ad(table);

	if (srv_created_new_raw || srv_force_recovery) {
		DBUG_RETURN(HA_ERR_WRONG_COMMAND);
	}

	update_thd();

	trx_search_latch_release_if_reserved(prebuilt->trx);
	trx_start_if_not_started_xa(prebuilt->trx);

	/* Create a background transaction for the operations on
	the data dictionary tables. */
	trx = innobase_trx_allocate(user_thd);
	trx_start_if_not_started_xa(trx);

	/* Flag this transaction as a dictionary operation, so that
	the data dictionary will be locked in crash recovery. */
	trx_set_dict_operation(trx, TRX_DICT_OP_INDEX);

	/* Lock the table exclusively, to ensure that no active
	transaction depends on an index that is being dropped. */
	err = convert_error_code_to_mysql(
		row_merge_lock_table(prebuilt->trx, prebuilt->table, LOCK_X),
		prebuilt->table->flags, user_thd);

	/* Delete corresponding rows from the stats table.
	Marko advises not to edit both user tables and SYS_* tables in one
	trx, thus we use prebuilt->trx instead of trx. Because of this the
	drop from SYS_* and from the stats table cannot happen in one
	transaction and eventually if a crash occurs below, between
	trx_commit_for_mysql(trx); which drops the indexes from SYS_* and
	trx_commit_for_mysql(prebuilt->trx);
	then an orphaned rows will be left in the stats table. */
	for (index = dict_table_get_first_index(prebuilt->table);
	     index != NULL;
	     index = dict_table_get_next_index(index)) {

		if (index->to_be_dropped) {

			enum db_err	ret;
			char		errstr[1024];

			ret = dict_stats_delete_index_stats(
				index, prebuilt->trx,
				errstr, sizeof(errstr));

			if (ret != DB_SUCCESS) {
				push_warning(user_thd,
					     Sql_condition::WARN_LEVEL_WARN,
					     ER_LOCK_WAIT_TIMEOUT,
					     errstr);
			}
		}
	}

	row_mysql_lock_data_dictionary(trx);
	ut_d(dict_table_check_for_dup_indexes(prebuilt->table, FALSE));

	if (UNIV_UNLIKELY(err)) {

		/* Unmark the indexes to be dropped. */
		for (index = dict_table_get_first_index(prebuilt->table);
		     index; index = dict_table_get_next_index(index)) {

			rw_lock_x_lock(dict_index_get_lock(index));
			index->to_be_dropped = FALSE;
			rw_lock_x_unlock(dict_index_get_lock(index));
		}

		goto func_exit;
	}

	/* Drop indexes marked to be dropped */

	index = dict_table_get_first_index(prebuilt->table);

	while (index) {
		dict_index_t*	next_index;

		next_index = dict_table_get_next_index(index);

		if (index->to_be_dropped) {
			row_merge_drop_index(index, prebuilt->table, trx);
		}

		index = next_index;
	}

	/* Check that all flagged indexes were dropped. */
	for (index = dict_table_get_first_index(prebuilt->table);
	     index; index = dict_table_get_next_index(index)) {
		ut_a(!index->to_be_dropped);
	}

	/* We will need to rebuild index translation table. Set
	valid index entry count in the translation table to zero */
	share->idx_trans_tbl.index_count = 0;

func_exit:
	ut_d(dict_table_check_for_dup_indexes(prebuilt->table, FALSE));

	ut_a(fts_check_cached_index(prebuilt->table));

	trx_commit_for_mysql(trx);
	trx_commit_for_mysql(prebuilt->trx);
	row_mysql_unlock_data_dictionary(trx);

	/* Flush the log to reduce probability that the .frm files and
	the InnoDB data dictionary get out-of-sync if the user runs
	with innodb_flush_log_at_trx_commit = 0 */

	log_buffer_flush_to_disk();

	trx_free_for_mysql(trx);

	/* Tell the InnoDB server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

	DBUG_RETURN(err);
}
