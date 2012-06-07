/*****************************************************************************

Copyright (c) 1996, 2010, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/******************************************************************//**
@file dict/dict0mem.c
Data dictionary memory object creation

Created 1/8/1996 Heikki Tuuri
***********************************************************************/

#include "dict0mem.h"

#ifdef UNIV_NONINL
#include "dict0mem.ic"
#endif

#include "rem0rec.h"
#include "data0type.h"
#include "mach0data.h"
#include "dict0dict.h"
#ifndef UNIV_HOTBACKUP
# include "ha_prototypes.h" /* innobase_casedn_str()*/
# include "lock0lock.h"
#endif /* !UNIV_HOTBACKUP */
#ifdef UNIV_BLOB_DEBUG
# include "ut0rbt.h"
#endif /* UNIV_BLOB_DEBUG */

#define	DICT_HEAP_SIZE		100	/*!< initial memory heap size when
					creating a table or index object */

#ifdef UNIV_PFS_MUTEX
/* Key to register autoinc_mutex with performance schema */
UNIV_INTERN mysql_pfs_key_t	autoinc_mutex_key;
#endif /* UNIV_PFS_MUTEX */

/**********************************************************************//**
Creates a table memory object.
@return	own: table object */
UNIV_INTERN
dict_table_t*
dict_mem_table_create(
/*==================*/
	const char*	name,	/*!< in: table name */
	ulint		space,	/*!< in: space where the clustered index of
				the table is placed; this parameter is
				ignored if the table is made a member of
				a cluster */
	ulint		n_cols,	/*!< in: number of columns */
	ulint		flags)	/*!< in: table flags */
{
	dict_table_t*	table;
	mem_heap_t*	heap;

	ut_ad(name);
	ut_a(!(flags & (~0 << DICT_TF2_BITS)));

	heap = mem_heap_create(DICT_HEAP_SIZE);

	table = mem_heap_zalloc(heap, sizeof(dict_table_t));

	table->heap = heap;

	table->flags = (unsigned int) flags;
	table->name = ut_malloc(strlen(name) + 1);
	memcpy(table->name, name, strlen(name) + 1);
	table->space = (unsigned int) space;
	table->n_cols = (unsigned int) (n_cols + DATA_N_SYS_COLS);

	table->cols = mem_heap_alloc(heap, (n_cols + DATA_N_SYS_COLS)
				     * sizeof(dict_col_t));

#ifndef UNIV_HOTBACKUP
	table->autoinc_lock = mem_heap_alloc(heap, lock_get_size());

	mutex_create(autoinc_mutex_key,
		     &table->autoinc_mutex, SYNC_DICT_AUTOINC_MUTEX);

	table->autoinc = 0;

	/* The number of transactions that are either waiting on the
	AUTOINC lock or have been granted the lock. */
	table->n_waiting_or_granted_auto_inc_locks = 0;
#endif /* !UNIV_HOTBACKUP */

	ut_d(table->magic_n = DICT_TABLE_MAGIC_N);
	return(table);
}

/****************************************************************//**
Free a table memory object. */
UNIV_INTERN
void
dict_mem_table_free(
/*================*/
	dict_table_t*	table)		/*!< in: table */
{
	ut_ad(table);
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);
	ut_d(table->cached = FALSE);

#ifndef UNIV_HOTBACKUP
	mutex_free(&(table->autoinc_mutex));
#endif /* UNIV_HOTBACKUP */
	ut_free(table->name);
	mem_heap_free(table->heap);
}

/****************************************************************//**
Append 'name' to 'col_names'.  @see dict_table_t::col_names
@return	new column names array */
static
const char*
dict_add_col_name(
/*==============*/
	const char*	col_names,	/*!< in: existing column names, or
					NULL */
	ulint		cols,		/*!< in: number of existing columns */
	const char*	name,		/*!< in: new column name */
	mem_heap_t*	heap)		/*!< in: heap */
{
	ulint	old_len;
	ulint	new_len;
	ulint	total_len;
	char*	res;

	ut_ad(!cols == !col_names);

	/* Find out length of existing array. */
	if (col_names) {
		const char*	s = col_names;
		ulint		i;

		for (i = 0; i < cols; i++) {
			s += strlen(s) + 1;
		}

		old_len = s - col_names;
	} else {
		old_len = 0;
	}

	new_len = strlen(name) + 1;
	total_len = old_len + new_len;

	res = mem_heap_alloc(heap, total_len);

	if (old_len > 0) {
		memcpy(res, col_names, old_len);
	}

	memcpy(res + old_len, name, new_len);

	return(res);
}

/**********************************************************************//**
Adds a column definition to a table. */
UNIV_INTERN
void
dict_mem_table_add_col(
/*===================*/
	dict_table_t*	table,	/*!< in: table */
	mem_heap_t*	heap,	/*!< in: temporary memory heap, or NULL */
	const char*	name,	/*!< in: column name, or NULL */
	ulint		mtype,	/*!< in: main datatype */
	ulint		prtype,	/*!< in: precise type */
	ulint		len)	/*!< in: precision */
{
	dict_col_t*	col;
	ulint		i;

	ut_ad(table);
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);
	ut_ad(!heap == !name);

	i = table->n_def++;

	if (name) {
		if (UNIV_UNLIKELY(table->n_def == table->n_cols)) {
			heap = table->heap;
		}
		if (UNIV_LIKELY(i) && UNIV_UNLIKELY(!table->col_names)) {
			/* All preceding column names are empty. */
			char* s = mem_heap_zalloc(heap, table->n_def);
			table->col_names = s;
		}

		table->col_names = dict_add_col_name(table->col_names,
						     i, name, heap);
	}

	col = dict_table_get_nth_col(table, i);

	dict_mem_fill_column_struct(col, i, mtype, prtype, len);
}


/**********************************************************************//**
This function populates a dict_col_t memory structure with
supplied information. */
UNIV_INTERN
void
dict_mem_fill_column_struct(
/*========================*/
	dict_col_t*	column,		/*!< out: column struct to be
					filled */
	ulint		col_pos,	/*!< in: column position */
	ulint		mtype,		/*!< in: main data type */
	ulint		prtype,		/*!< in: precise type */
	ulint		col_len)	/*!< in: column length */
{
#ifndef UNIV_HOTBACKUP
	ulint	mbminlen;
	ulint	mbmaxlen;
#endif /* !UNIV_HOTBACKUP */

	column->ind = (unsigned int) col_pos;
	column->ord_part = 0;
	column->max_prefix = 0;
	column->mtype = (unsigned int) mtype;
	column->prtype = (unsigned int) prtype;
	column->len = (unsigned int) col_len;
#ifndef UNIV_HOTBACKUP
        dtype_get_mblen(mtype, prtype, &mbminlen, &mbmaxlen);
	dict_col_set_mbminmaxlen(column, mbminlen, mbmaxlen);
#endif /* !UNIV_HOTBACKUP */
}

/**********************************************************************//**
Creates an index memory object.
@return	own: index object */
UNIV_INTERN
dict_index_t*
dict_mem_index_create(
/*==================*/
	const char*	table_name,	/*!< in: table name */
	const char*	index_name,	/*!< in: index name */
	ulint		space,		/*!< in: space where the index tree is
					placed, ignored if the index is of
					the clustered type */
	ulint		type,		/*!< in: DICT_UNIQUE,
					DICT_CLUSTERED, ... ORed */
	ulint		n_fields)	/*!< in: number of fields */
{
	dict_index_t*	index;
	mem_heap_t*	heap;

	ut_ad(table_name && index_name);

	heap = mem_heap_create(DICT_HEAP_SIZE);
	index = mem_heap_zalloc(heap, sizeof(dict_index_t));

	dict_mem_fill_index_struct(index, heap, table_name, index_name,
				   space, type, n_fields);

	return(index);
}

#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Creates and initializes a foreign constraint memory object.
@return	own: foreign constraint struct */
UNIV_INTERN
dict_foreign_t*
dict_mem_foreign_create(void)
/*=========================*/
{
	dict_foreign_t*	foreign;
	mem_heap_t*	heap;

	heap = mem_heap_create(100);

	foreign = mem_heap_zalloc(heap, sizeof(dict_foreign_t));

	foreign->heap = heap;

	return(foreign);
}

/**********************************************************************//**
Sets the foreign_table_name_lookup pointer based on the value of
lower_case_table_names.  If that is 0 or 1, foreign_table_name_lookup
will point to foreign_table_name.  If 2, then another string is
allocated from foreign->heap and set to lower case. */
UNIV_INTERN
void
dict_mem_foreign_table_name_lookup_set(
/*===================================*/
	dict_foreign_t*	foreign,	/*!< in/out: foreign struct */
	ibool		do_alloc)	/*!< in: is an alloc needed */
{
	if (innobase_get_lower_case_table_names() == 2) {
		if (do_alloc) {
			foreign->foreign_table_name_lookup = mem_heap_alloc(
				foreign->heap,
				strlen(foreign->foreign_table_name) + 1);
		}
		strcpy(foreign->foreign_table_name_lookup,
		       foreign->foreign_table_name);
		innobase_casedn_str(foreign->foreign_table_name_lookup);
	} else {
		foreign->foreign_table_name_lookup
			= foreign->foreign_table_name;
	}
}

/**********************************************************************//**
Sets the referenced_table_name_lookup pointer based on the value of
lower_case_table_names.  If that is 0 or 1, referenced_table_name_lookup
will point to referenced_table_name.  If 2, then another string is
allocated from foreign->heap and set to lower case. */
UNIV_INTERN
void
dict_mem_referenced_table_name_lookup_set(
/*======================================*/
	dict_foreign_t*	foreign,	/*!< in/out: foreign struct */
	ibool		do_alloc)	/*!< in: is an alloc needed */
{
	if (innobase_get_lower_case_table_names() == 2) {
		if (do_alloc) {
			foreign->referenced_table_name_lookup = mem_heap_alloc(
				foreign->heap,
				strlen(foreign->referenced_table_name) + 1);
		}
		strcpy(foreign->referenced_table_name_lookup,
		       foreign->referenced_table_name);
		innobase_casedn_str(foreign->referenced_table_name_lookup);
	} else {
		foreign->referenced_table_name_lookup
			= foreign->referenced_table_name;
	}
}

#endif /* !UNIV_HOTBACKUP */
/**********************************************************************//**
Adds a field definition to an index. NOTE: does not take a copy
of the column name if the field is a column. The memory occupied
by the column name may be released only after publishing the index. */
UNIV_INTERN
void
dict_mem_index_add_field(
/*=====================*/
	dict_index_t*	index,		/*!< in: index */
	const char*	name,		/*!< in: column name */
	ulint		prefix_len)	/*!< in: 0 or the column prefix length
					in a MySQL index like
					INDEX (textcol(25)) */
{
	dict_field_t*	field;

	ut_ad(index);
	ut_ad(index->magic_n == DICT_INDEX_MAGIC_N);

	index->n_def++;

	field = dict_index_get_nth_field(index, index->n_def - 1);

	field->name = name;
	field->prefix_len = (unsigned int) prefix_len;
}

/**********************************************************************//**
Frees an index memory object. */
UNIV_INTERN
void
dict_mem_index_free(
/*================*/
	dict_index_t*	index)	/*!< in: index */
{
	ut_ad(index);
	ut_ad(index->magic_n == DICT_INDEX_MAGIC_N);
#ifdef UNIV_BLOB_DEBUG
	if (index->blobs) {
		mutex_free(&index->blobs_mutex);
		rbt_free(index->blobs);
	}
#endif /* UNIV_BLOB_DEBUG */

	mem_heap_free(index->heap);
}
