/*****************************************************************************

Copyright (c) 2008, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file api/api0api.cc
InnoDB Native API

2008-08-01 Created Sunny Bains
3/20/2011 Jimmy Yang extracted from Embedded InnoDB
*******************************************************/

#include "univ.i"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "api0api.h"
#include "api0misc.h"
#include "srv0start.h"
#include "dict0dict.h"
#include "btr0pcur.h"
#include "row0ins.h"
#include "row0upd.h"
#include "row0vers.h"
#include "trx0roll.h"
#include "dict0crea.h"
#include "row0merge.h"
#include "pars0pars.h"
#include "lock0types.h"
#include "row0sel.h"
#include "lock0lock.h"
#include "rem0cmp.h"
#include "ut0dbg.h"
#include "dict0priv.h"
#include "ut0ut.h"
#include "ha_prototypes.h"
#include "trx0roll.h"

/** configure variable for binlog option with InnoDB APIs */
my_bool ib_binlog_enabled = FALSE;

/** configure variable for MDL option with InnoDB APIs */
my_bool ib_mdl_enabled = FALSE;

/** configure variable for disable rowlock with InnoDB APIs */
my_bool ib_disable_row_lock = FALSE;

/** configure variable for Transaction isolation levels */
ulong ib_trx_level_setting = IB_TRX_READ_UNCOMMITTED;

/** configure variable for background commit interval in seconds */
ulong ib_bk_commit_interval = 0;

/** InnoDB tuple types. */
enum ib_tuple_type_t{
	TPL_TYPE_ROW,			/*!< Data row tuple */
	TPL_TYPE_KEY			/*!< Index key tuple */
};

/** Query types supported. */
enum ib_qry_type_t{
	QRY_NON,			/*!< None/Sentinel */
	QRY_INS,			/*!< Insert operation */
	QRY_UPD,			/*!< Update operation */
	QRY_SEL				/*!< Select operation */
};

/** Query graph types. */
struct ib_qry_grph_t {
	que_fork_t*	ins;		/*!< Innobase SQL query graph used
					in inserts */
	que_fork_t*	upd;		/*!< Innobase SQL query graph used
					in updates or deletes */
	que_fork_t*	sel;		/*!< dummy query graph used in
					selects */
};

/** Query node types. */
struct ib_qry_node_t {
	ins_node_t*	ins;		/*!< Innobase SQL insert node
					used to perform inserts to the table */
	upd_node_t*	upd;		/*!< Innobase SQL update node
					used to perform updates and deletes */
	sel_node_t*	sel;		/*!< Innobase SQL select node
					used to perform selects on the table */
};

/** Query processing fields. */
struct ib_qry_proc_t {

	ib_qry_node_t	node;		/*!< Query node*/

	ib_qry_grph_t	grph;		/*!< Query graph */
};

/** Cursor instance for traversing tables/indexes. This will eventually
become row_prebuilt_t. */
struct ib_cursor_t {
	mem_heap_t*	heap;		/*!< Instance heap */

	mem_heap_t*	query_heap;	/*!< Heap to use for query graphs */

	ib_qry_proc_t	q_proc;		/*!< Query processing info */

	ib_match_mode_t	match_mode;	/*!< ib_cursor_moveto match mode */

	row_prebuilt_t*	prebuilt;	/*!< For reading rows */

	bool		valid_trx;	/*!< Valid transaction attached */
};

/** InnoDB table columns used during table and index schema creation. */
struct ib_col_t {
	const char*	name;		/*!< Name of column */

	ib_col_type_t	ib_col_type;	/*!< Main type of the column */

	ulint		len;		/*!< Length of the column */

	ib_col_attr_t	ib_col_attr;	/*!< Column attributes */

};

/** InnoDB index columns used during index and index schema creation. */
struct ib_key_col_t {
	const char*	name;		/*!< Name of column */

	ulint		prefix_len;	/*!< Column index prefix len or 0 */
};

struct ib_table_def_t;

/** InnoDB index schema used during index creation */
struct ib_index_def_t {
	mem_heap_t*	heap;		/*!< Heap used to build this and all
					its columns in the list */

	const char*	name;		/*!< Index name */

	dict_table_t*	table;		/*!< Parent InnoDB table */

	ib_table_def_t*	schema;		/*!< Parent table schema that owns
					this instance */

	ibool		clustered;	/*!< True if clustered index */

	ibool		unique;		/*!< True if unique index */

	ib_vector_t*	cols;		/*!< Vector of columns */

	trx_t*		usr_trx;	/*!< User transacton covering the
					DDL operations */
};

/** InnoDB table schema used during table creation */
struct ib_table_def_t {
	mem_heap_t*	heap;		/*!< Heap used to build this and all
					its columns in the list */
	const char*	name;		/*!< Table name */

	ib_tbl_fmt_t	ib_tbl_fmt;	/*!< Row format */

	ulint		page_size;	/*!< Page size */

	ib_vector_t*	cols;		/*!< Vector of columns */

	ib_vector_t*	indexes;	/*!< Vector of indexes */

	dict_table_t*	table;		/* Table read from or NULL */
};

/** InnoDB tuple used for key operations. */
struct ib_tuple_t {
	mem_heap_t*		heap;	/*!< Heap used to build
					this and for copying
					the column values. */

	ib_tuple_type_t		type;	/*!< Tuple discriminitor. */

	const dict_index_t*	index;	/*!< Index for tuple can be either
					secondary or cluster index. */

	dtuple_t*		ptr;	/*!< The internal tuple
					instance */
};

/** The following counter is used to convey information to InnoDB
about server activity: in selects it is not sensible to call
srv_active_wake_master_thread after each fetch or search, we only do
it every INNOBASE_WAKE_INTERVAL'th step. */

#define INNOBASE_WAKE_INTERVAL	32

/*****************************************************************//**
Check whether the Innodb persistent cursor is positioned.
@return	IB_TRUE if positioned */
UNIV_INLINE
ib_bool_t
ib_btr_cursor_is_positioned(
/*========================*/
	btr_pcur_t*	pcur)		/*!< in: InnoDB persistent cursor */
{
	return(pcur->old_stored == BTR_PCUR_OLD_STORED
	       && (pcur->pos_state == BTR_PCUR_IS_POSITIONED
	           || pcur->pos_state == BTR_PCUR_WAS_POSITIONED));
}


/********************************************************************//**
Open a table using the table id, if found then increment table ref count.
@return	table instance if found */
static
dict_table_t*
ib_open_table_by_id(
/*================*/
	ib_id_u64_t	tid,		/*!< in: table id to lookup */
	ib_bool_t	locked)		/*!< in: TRUE if own dict mutex */
{
	dict_table_t*	table;
	table_id_t	table_id;

	table_id = tid;

	if (!locked) {
		dict_mutex_enter_for_mysql();
	}

	table = dict_table_open_on_id(table_id, FALSE, FALSE);

	if (table != NULL && table->ibd_file_missing) {
		table = NULL;
	}

	if (!locked) {
		dict_mutex_exit_for_mysql();
	}

	return(table);
}

/********************************************************************//**
Open a table using the table name, if found then increment table ref count.
@return	table instance if found */
UNIV_INTERN
void*
ib_open_table_by_name(
/*==================*/
	const char*	name)		/*!< in: table name to lookup */
{
	dict_table_t*	table;

	table = dict_table_open_on_name(name, FALSE, FALSE,
					DICT_ERR_IGNORE_NONE);

	if (table != NULL && table->ibd_file_missing) {
		table = NULL;
	}

	return(table);
}

/********************************************************************//**
Find table using table name.
@return	table instance if found */
static
dict_table_t*
ib_lookup_table_by_name(
/*====================*/
	const char*	name)		/*!< in: table name to lookup */
{
	dict_table_t*	table;

	table = dict_table_get_low(name);

	if (table != NULL && table->ibd_file_missing) {
		table = NULL;
	}

	return(table);
}

/********************************************************************//**
Increments innobase_active_counter and every INNOBASE_WAKE_INTERVALth
time calls srv_active_wake_master_thread. This function should be used
when a single database operation may introduce a small need for
server utility activity, like checkpointing. */
UNIV_INLINE
void
ib_wake_master_thread(void)
/*=======================*/
{
        static ulint    ib_signal_counter = 0;

        ++ib_signal_counter;

        if ((ib_signal_counter % INNOBASE_WAKE_INTERVAL) == 0) {
                srv_active_wake_master_thread();
        }
}

/*********************************************************************//**
Calculate the max row size of the columns in a cluster index.
@return	max row length */
UNIV_INLINE
ulint
ib_get_max_row_len(
/*===============*/
	dict_index_t*	cluster)		/*!< in: cluster index */
{
	ulint		i;
	ulint		max_len = 0;
	ulint		n_fields = cluster->n_fields;

	/* Add the size of the ordering columns in the
	clustered index. */
	for (i = 0; i < n_fields; ++i) {
		const dict_col_t*	col;

		col = dict_index_get_nth_col(cluster, i);

		/* Use the maximum output size of
		mach_write_compressed(), although the encoded
		length should always fit in 2 bytes. */
		max_len += dict_col_get_max_size(col);
	}

	return(max_len);
}

/*****************************************************************//**
Read the columns from a rec into a tuple. */
static
void
ib_read_tuple(
/*==========*/
	const rec_t*	rec,		/*!< in: Record to read */
	ib_bool_t	page_format,	/*!< in: IB_TRUE if compressed format */
	ib_tuple_t*	tuple)		/*!< in: tuple to read into */
{
	ulint		i;
	void*		ptr;
	rec_t*		copy;
	ulint		rec_meta_data;
	ulint		n_index_fields;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets	= offsets_;
	dtuple_t*	dtuple = tuple->ptr;
	const dict_index_t* index = tuple->index;

	rec_offs_init(offsets_);

	offsets = rec_get_offsets(
		rec, index, offsets, ULINT_UNDEFINED, &tuple->heap);

	rec_meta_data = rec_get_info_bits(rec, page_format);
	dtuple_set_info_bits(dtuple, rec_meta_data);

	/* Make a copy of the rec. */
	ptr = mem_heap_alloc(tuple->heap, rec_offs_size(offsets));
	copy = rec_copy(ptr, rec, offsets);

	n_index_fields = ut_min(
		rec_offs_n_fields(offsets), dtuple_get_n_fields(dtuple));

	for (i = 0; i < n_index_fields; ++i) {
		ulint		len;
		const byte*	data;
		dfield_t*	dfield;

		if (tuple->type == TPL_TYPE_ROW) {
			const dict_col_t*	col;
			ulint			col_no;
			const dict_field_t*	index_field;

			index_field = dict_index_get_nth_field(index, i);
			col = dict_field_get_col(index_field);
			col_no = dict_col_get_no(col);

			dfield = dtuple_get_nth_field(dtuple, col_no);
		} else {
			dfield = dtuple_get_nth_field(dtuple, i);
		}

		data = rec_get_nth_field(copy, offsets, i, &len);

		/* Fetch and copy any externally stored column. */
		if (rec_offs_nth_extern(offsets, i)) {

			ulint	zip_size;

			zip_size = dict_table_zip_size(index->table);

			data = btr_rec_copy_externally_stored_field(
				copy, offsets, zip_size, i, &len,
				tuple->heap);

			ut_a(len != UNIV_SQL_NULL);
		}

		dfield_set_data(dfield, data, len);
	}
}

/*****************************************************************//**
Create an InnoDB key tuple.
@return	tuple instance created, or NULL */
static
ib_tpl_t
ib_key_tuple_new_low(
/*=================*/
	const dict_index_t*	index,	/*!< in: index for which tuple
					required */
	ulint			n_cols,	/*!< in: no. of user defined cols */
	mem_heap_t*		heap)	/*!< in: memory heap */
{
	ib_tuple_t*	tuple;
	ulint		i;
	ulint		n_cmp_cols;

	tuple = static_cast<ib_tuple_t*>(
			mem_heap_alloc(heap, sizeof(*tuple)));

	if (tuple == NULL) {
		mem_heap_free(heap);
		return(NULL);
	}

	tuple->heap  = heap;
	tuple->index = index;
	tuple->type  = TPL_TYPE_KEY;

	/* Is it a generated clustered index ? */
	if (n_cols == 0) {
		++n_cols;
	}

	tuple->ptr = dtuple_create(heap, n_cols);

	/* Copy types and set to SQL_NULL. */
	dict_index_copy_types(tuple->ptr, index, n_cols);

	for (i = 0; i < n_cols; i++) {

		dfield_t*	dfield;

		dfield	= dtuple_get_nth_field(tuple->ptr, i);
		dfield_set_null(dfield);
	}

	n_cmp_cols = dict_index_get_n_ordering_defined_by_user(index);

	dtuple_set_n_fields_cmp(tuple->ptr, n_cmp_cols);

	return((ib_tpl_t) tuple);
}

/*****************************************************************//**
Create an InnoDB key tuple.
@return	tuple instance created, or NULL */
static
ib_tpl_t
ib_key_tuple_new(
/*=============*/
	const dict_index_t*	index,	/*!< in: index of tuple */
	ulint			n_cols)	/*!< in: no. of user defined cols */
{
	mem_heap_t*	heap;

	heap = mem_heap_create(64);

	if (heap == NULL) {
		return(NULL);
	}

	return(ib_key_tuple_new_low(index, n_cols, heap));
}

/*****************************************************************//**
Create an InnoDB row tuple.
@return	tuple instance, or NULL */
static
ib_tpl_t
ib_row_tuple_new_low(
/*=================*/
	const dict_index_t*	index,	/*!< in: index of tuple */
	ulint			n_cols,	/*!< in: no. of cols in tuple */
	mem_heap_t*		heap)	/*!< in: memory heap */
{
	ib_tuple_t*	tuple;

	tuple = static_cast<ib_tuple_t*>(mem_heap_alloc(heap, sizeof(*tuple)));

	if (tuple == NULL) {
		mem_heap_free(heap);
		return(NULL);
	}

	tuple->heap  = heap;
	tuple->index = index;
	tuple->type  = TPL_TYPE_ROW;

	tuple->ptr = dtuple_create(heap, n_cols);

	/* Copy types and set to SQL_NULL. */
	dict_table_copy_types(tuple->ptr, index->table);

	return((ib_tpl_t) tuple);
}

/*****************************************************************//**
Create an InnoDB row tuple.
@return	tuple instance, or NULL */
static
ib_tpl_t
ib_row_tuple_new(
/*=============*/
	const dict_index_t*	index,	/*!< in: index of tuple */
	ulint			n_cols)	/*!< in: no. of cols in tuple */
{
	mem_heap_t*	heap;

	heap = mem_heap_create(64);

	if (heap == NULL) {
		return(NULL);
	}

	return(ib_row_tuple_new_low(index, n_cols, heap));
}

/*****************************************************************//**
Begin a transaction.
@return	innobase txn handle */
UNIV_INTERN
ib_err_t
ib_trx_start(
/*=========*/
	ib_trx_t	ib_trx,		/*!< in: transaction to restart */
	ib_trx_level_t	ib_trx_level,	/*!< in: trx isolation level */
	void*		thd)		/*!< in: THD */
{
	ib_err_t	err = DB_SUCCESS;
	trx_t*		trx = (trx_t*) ib_trx;

	ut_a(ib_trx_level <= IB_TRX_SERIALIZABLE);

	trx_start_if_not_started(trx);

	trx->isolation_level = ib_trx_level;

	/* FIXME: This is a place holder, we should add an arg that comes
	from the client. */
	trx->mysql_thd = static_cast<THD*>(thd);

	return(err);
}

/*****************************************************************//**
Begin a transaction. This will allocate a new transaction handle.
put the transaction in the active state.
@return	innobase txn handle */
UNIV_INTERN
ib_trx_t
ib_trx_begin(
/*=========*/
	ib_trx_level_t	ib_trx_level)	/*!< in: trx isolation level */
{
	trx_t*		trx;
	ib_bool_t	started;

	trx = trx_allocate_for_mysql();
	started = ib_trx_start((ib_trx_t) trx, ib_trx_level, NULL);
	ut_a(started);

	return((ib_trx_t) trx);
}

/*****************************************************************//**
Get the transaction's state.
@return	transaction state */
UNIV_INTERN
ib_trx_state_t
ib_trx_state(
/*=========*/
	ib_trx_t	ib_trx)		/*!< in: trx handle */
{
	trx_t*		trx = (trx_t*) ib_trx;

	return((ib_trx_state_t) trx->state);
}

/*****************************************************************//**
Get a trx start time.
@return	trx start_time */
UNIV_INTERN
ib_u64_t
ib_trx_get_start_time(
/*==================*/
	ib_trx_t	ib_trx)		/*!< in: transaction */
{
	trx_t*		trx = (trx_t*) ib_trx;
	return(static_cast<ib_u64_t>(trx->start_time));
}
/*****************************************************************//**
Release the resources of the transaction.
@return	DB_SUCCESS or err code */
UNIV_INTERN
ib_err_t
ib_trx_release(
/*===========*/
	ib_trx_t	ib_trx)		/*!< in: trx handle */
{
	trx_t*		trx = (trx_t*) ib_trx;

	ut_ad(trx != NULL);
	trx_free_for_mysql(trx);

	return(DB_SUCCESS);
}

/*****************************************************************//**
Commit a transaction. This function will also release the schema
latches too.
@return	DB_SUCCESS or err code */

ib_err_t
ib_trx_commit(
/*==========*/
	ib_trx_t	ib_trx)		/*!< in: trx handle */
{
	ib_err_t	err = DB_SUCCESS;
	trx_t*		trx = (trx_t*) ib_trx;

	if (trx->state == TRX_STATE_NOT_STARTED) {
		err = ib_trx_release(ib_trx);
		return(err);
	}

	trx_commit(trx);

	err = ib_trx_release(ib_trx);
	ut_a(err == DB_SUCCESS);

	return(DB_SUCCESS);
}

/*****************************************************************//**
Rollback a transaction. This function will also release the schema
latches too.
@return	DB_SUCCESS or err code */
UNIV_INTERN
ib_err_t
ib_trx_rollback(
/*============*/
	ib_trx_t	ib_trx)		/*!< in: trx handle */
{
	ib_err_t	err;
	trx_t*		trx = (trx_t*) ib_trx;

	err = static_cast<ib_err_t>(trx_rollback_for_mysql(trx));

        /* It should always succeed */
        ut_a(err == DB_SUCCESS);

	err = ib_trx_release(ib_trx);
	ut_a(err == DB_SUCCESS);

	ib_wake_master_thread();

	return(err);
}

/*****************************************************************//**
Find an index definition from the index vector using index name.
@return	index def. if found else NULL */
UNIV_INLINE
const ib_index_def_t*
ib_table_find_index(
/*================*/
	ib_vector_t*	indexes,	/*!< in: vector of indexes */
	const char*	name)		/*!< in: index name */
{
	ulint		i;

	for (i = 0; i < ib_vector_size(indexes); ++i) {
		const ib_index_def_t*	index_def;

		index_def = (ib_index_def_t*) ib_vector_get(indexes, i);

		if (innobase_strcasecmp(name, index_def->name) == 0) {
			return(index_def);
		}
	}

	return(NULL);
}

/*****************************************************************//**
Get the InnoDB internal precise type from the schema column definition.
@return	precise type in api format */
UNIV_INLINE
ulint
ib_col_get_prtype(
/*==============*/
	const ib_col_t*	ib_col)		/*!< in: column definition */
{
	ulint		prtype = 0;

	if (ib_col->ib_col_attr & IB_COL_UNSIGNED) {
		prtype |= DATA_UNSIGNED;

		ut_a(ib_col->ib_col_type == IB_INT);
	}

	if (ib_col->ib_col_attr & IB_COL_NOT_NULL) {
		prtype |= DATA_NOT_NULL;
	}

	return(prtype);
}

/*****************************************************************//**
Get the InnoDB internal main type from the schema column definition.
@return	column main type */
UNIV_INLINE
ulint
ib_col_get_mtype(
/*==============*/
	const ib_col_t*	ib_col)		/*!< in: column definition */
{
	/* Note: The api0api.h types should map directly to
	the internal numeric codes. */
	return(ib_col->ib_col_type);
}

/*****************************************************************//**
Find a column in the the column vector with the same name.
@return	col. def. if found else NULL */
UNIV_INLINE
const ib_col_t*
ib_table_find_col(
/*==============*/
	const ib_vector_t*	cols,	/*!< in: column list head */
	const char*	name)		/*!< in: column name to find */
{
	ulint		i;

	for (i = 0; i < ib_vector_size(cols); ++i) {
		const ib_col_t*	ib_col;

		ib_col =  static_cast<const ib_col_t*>(
			ib_vector_get((ib_vector_t*) cols, i));

		if (innobase_strcasecmp(ib_col->name, name) == 0) {
			return(ib_col);
		}
	}

	return(NULL);
}

/*****************************************************************//**
Find a column in the the column list with the same name.
@return	col. def. if found else NULL */
UNIV_INLINE
const ib_key_col_t*
ib_index_find_col(
/*==============*/
	ib_vector_t*	cols,		/*!< in: column list head */
	const char*	name)		/*!< in: column name to find */
{
	ulint		i;

	for (i = 0; i < ib_vector_size(cols); ++i) {
		const ib_key_col_t*	ib_col;

		ib_col = static_cast<ib_key_col_t*>(ib_vector_get(cols, i));

		if (innobase_strcasecmp(ib_col->name, name) == 0) {
			return(ib_col);
		}
	}

	return(NULL);
}

#ifdef __WIN__
/*****************************************************************//**
Convert a string to lower case. */
static
void
ib_to_lower_case(
/*=============*/
	char*		ptr)		/*!< string to convert to lower case */
{
	while (*ptr) {
		*ptr = tolower(*ptr);
		++ptr;
	}
}
#endif /* __WIN__ */

/*****************************************************************//**
Normalizes a table name string. A normalized name consists of the
database name catenated to '/' and table name. An example:
test/mytable. On Windows normalization puts both the database name and the
table name always to lower case. This function can be called for system
tables and they don't have a database component. For tables that don't have
a database component, we don't normalize them to lower case on Windows.
The assumption is that they are system tables that reside in the system
table space. */
static
void
ib_normalize_table_name(
/*====================*/
	char*		norm_name,	/*!< out: normalized name as a
					null-terminated string */
	const char*	name)		/*!< in: table name string */
{
	const char*	ptr = name;

	/* Scan name from the end */

	ptr += ut_strlen(name) - 1;

	/* Find the start of the table name. */
	while (ptr >= name && *ptr != '\\' && *ptr != '/' && ptr > name) {
		--ptr;
	}


	/* For system tables there is no '/' or dbname. */
	ut_a(ptr >= name);

	if (ptr > name) {
		const char*	db_name;
		const char*	table_name;

		table_name = ptr + 1;

		--ptr;

		while (ptr >= name && *ptr != '\\' && *ptr != '/') {
			ptr--;
		}

		db_name = ptr + 1;

		memcpy(norm_name, db_name,
			ut_strlen(name) + 1 - (db_name - name));

		norm_name[table_name - db_name - 1] = '/';
#ifdef __WIN__
		ib_to_lower_case(norm_name);
#endif
	} else {
		ut_strcpy(norm_name, name);
	}
}

/*****************************************************************//**
Check whether the table name conforms to our requirements. Currently
we only do a simple check for the presence of a '/'.
@return	DB_SUCCESS or err code */
UNIV_INTERN
ib_err_t
ib_table_name_check(
/*================*/
	const char*	name)		/*!< in: table name to check */
{
	const char*	slash = NULL;
	ulint		len = ut_strlen(name);

	if (len < 2
	    || *name == '/'
	    || name[len - 1] == '/'
	    || (name[0] == '.' && name[1] == '/')
	    || (name[0] == '.' && name[1] == '.' && name[2] == '/')) {

		return(DB_DATA_MISMATCH);
	}

	for ( ; *name; ++name) {
#ifdef __WIN__
		/* Check for reserved characters in DOS filenames. */
		switch (*name) {
		case ':':
		case '|':
		case '"':
		case '*':
		case '<':
		case '>':
			return(DB_DATA_MISMATCH);
		}
#endif /* __WIN__ */
		if (*name == '/') {
			if (slash) {
				return(DB_DATA_MISMATCH);
			}
			slash = name;
		}
	}

	return(slash ? DB_SUCCESS : DB_DATA_MISMATCH);
}



/*****************************************************************//**
Get an index definition that is tagged as a clustered index.
@return	cluster index schema */
UNIV_INLINE
ib_index_def_t*
ib_find_clustered_index(
/*====================*/
	ib_vector_t*	indexes)	/*!< in: index defs. to search */
{
	ulint		i;
	ulint		n_indexes;

	n_indexes = ib_vector_size(indexes);

	for (i = 0; i < n_indexes; ++i) {
		ib_index_def_t*	ib_index_def;

		ib_index_def = static_cast<ib_index_def_t*>(
			ib_vector_get(indexes, i));

		if (ib_index_def->clustered) {
			return(ib_index_def);
		}
	}

	return(NULL);
}

/*****************************************************************//**
Get a table id. The caller must have acquired the dictionary mutex.
@return	DB_SUCCESS if found */
static
ib_err_t
ib_table_get_id_low(
/*================*/
	const char*	table_name,	/*!< in: table to find */
	ib_id_u64_t*	table_id)	/*!< out: table id if found */
{
	dict_table_t*	table;
	ib_err_t	err = DB_TABLE_NOT_FOUND;

	*table_id = 0;

	table = ib_lookup_table_by_name(table_name);

	if (table != NULL) {
		*table_id = (table->id);

		err = DB_SUCCESS;
	}

	return(err);
}

/*****************************************************************//**
Create an internal cursor instance.
@return	DB_SUCCESS or err code */
static
ib_err_t
ib_create_cursor(
/*=============*/
	ib_crsr_t*	ib_crsr,	/*!< out: InnoDB cursor */
	dict_table_t*	table,		/*!< in: table instance */
	dict_index_t*	index,		/*!< in: index to use */
	trx_t*		trx)		/*!< in: transaction */
{
	mem_heap_t*	heap;
	ib_cursor_t*	cursor;
	ib_err_t	err = DB_SUCCESS;

	heap = mem_heap_create(sizeof(*cursor) * 2);

	if (heap != NULL) {
		row_prebuilt_t*	prebuilt;

		cursor = static_cast<ib_cursor_t*>(
			 mem_heap_zalloc(heap, sizeof(*cursor)));

		cursor->heap = heap;

		cursor->query_heap = mem_heap_create(64);

		if (cursor->query_heap == NULL) {
			mem_heap_free(heap);

			return(DB_OUT_OF_MEMORY);
		}

		cursor->prebuilt = row_create_prebuilt(table, 0);

		prebuilt = cursor->prebuilt;

		prebuilt->trx = trx;

		cursor->valid_trx = TRUE;

		prebuilt->table = table;
		prebuilt->select_lock_type = LOCK_NONE;
		prebuilt->innodb_api = TRUE;

		prebuilt->index = index;

		ut_a(prebuilt->index != NULL);

		if (prebuilt->trx != NULL) {
			++prebuilt->trx->n_mysql_tables_in_use;

			 prebuilt->index_usable =
				row_merge_is_index_usable(
					prebuilt->trx, prebuilt->index);

			/* Assign a read view if the transaction does
			not have it yet */

			trx_assign_read_view(prebuilt->trx);
		}

		*ib_crsr = (ib_crsr_t) cursor;
	} else {
		err = DB_OUT_OF_MEMORY;
	}

	return(err);
}

/*****************************************************************//**
Create an internal cursor instance, and set prebuilt->index to index
with supplied index_id.
@return	DB_SUCCESS or err code */
static
ib_err_t
ib_create_cursor_with_index_id(
/*===========================*/
	ib_crsr_t*	ib_crsr,	/*!< out: InnoDB cursor */
	dict_table_t*	table,		/*!< in: table instance */
	ib_id_u64_t	index_id,	/*!< in: index id or 0 */
	trx_t*		trx)		/*!< in: transaction */
{
	dict_index_t*	index;

	if (index_id != 0) {
		mutex_enter(&dict_sys->mutex);
		index = dict_index_find_on_id_low(index_id);
		mutex_exit(&dict_sys->mutex);
	} else {
		index = dict_table_get_first_index(table);
	}

	return(ib_create_cursor(ib_crsr, table, index, trx));
}

/*****************************************************************//**
Open an InnoDB table and return a cursor handle to it.
@return	DB_SUCCESS or err code */
UNIV_INTERN
ib_err_t
ib_cursor_open_table_using_id(
/*==========================*/
	ib_id_u64_t	table_id,	/*!< in: table id of table to open */
	ib_trx_t	ib_trx,		/*!< in: Current transaction handle
					can be NULL */
	ib_crsr_t*	ib_crsr)	/*!< out,own: InnoDB cursor */
{
	ib_err_t	err;
	dict_table_t*	table;

	if (ib_trx == NULL || !ib_schema_lock_is_exclusive(ib_trx)) {
		table = ib_open_table_by_id(table_id, FALSE);
	} else {
		table = ib_open_table_by_id(table_id, TRUE);
	}

	if (table == NULL) {

		return(DB_TABLE_NOT_FOUND);
	}

	err = ib_create_cursor_with_index_id(ib_crsr, table, 0,
					     (trx_t*) ib_trx);

	return(err);
}

/*****************************************************************//**
Open an InnoDB index and return a cursor handle to it.
@return	DB_SUCCESS or err code */
UNIV_INTERN
ib_err_t
ib_cursor_open_index_using_id(
/*==========================*/
	ib_id_u64_t	index_id,	/*!< in: index id of index to open */
	ib_trx_t	ib_trx,		/*!< in: Current transaction handle
					can be NULL */
	ib_crsr_t*	ib_crsr)	/*!< out: InnoDB cursor */
{
	ib_err_t	err;
	dict_table_t*	table;
	ulint		table_id = (ulint)( index_id >> 32);

	if (ib_trx == NULL || !ib_schema_lock_is_exclusive(ib_trx)) {
		table = ib_open_table_by_id(table_id, FALSE);
	} else {
		table = ib_open_table_by_id(table_id, TRUE);
	}

	if (table == NULL) {

		return(DB_TABLE_NOT_FOUND);
	}

	/* We only return the lower 32 bits of the dulint. */
	err = ib_create_cursor_with_index_id(
		ib_crsr, table, index_id, (trx_t*) ib_trx);

	if (ib_crsr != NULL) {
		const ib_cursor_t*	cursor;

		cursor = *(ib_cursor_t**) ib_crsr;

		if (cursor->prebuilt->index == NULL) {
			ib_err_t	crsr_err;

			crsr_err = ib_cursor_close(*ib_crsr);
			ut_a(crsr_err == DB_SUCCESS);

			*ib_crsr = NULL;
		}
	}

	return(err);
}

/*****************************************************************//**
Open an InnoDB secondary index cursor and return a cursor handle to it.
@return	DB_SUCCESS or err code */
UNIV_INTERN
ib_err_t
ib_cursor_open_index_using_name(
/*============================*/
	ib_crsr_t	ib_open_crsr,	/*!< in: open/active cursor */
	const char*	index_name,	/*!< in: secondary index name */
	ib_crsr_t*	ib_crsr,	/*!< out,own: InnoDB index cursor */
	int*		idx_type,	/*!< out: index is cluster index */
	ib_id_u64_t*	idx_id)		/*!< out: index id */
{
	dict_table_t*	table;
	dict_index_t*	index;
	index_id_t	index_id = 0;
	ib_err_t	err = DB_TABLE_NOT_FOUND;
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_open_crsr;

	*idx_type = 0;
	*idx_id = 0;
	*ib_crsr = NULL;

	/* We want to increment the ref count, so we do a redundant search. */
	table = dict_table_open_on_id(cursor->prebuilt->table->id,
				      FALSE, FALSE);
	ut_a(table != NULL);

	/* The first index is always the cluster index. */
	index = dict_table_get_first_index(table);

	/* Traverse the user defined indexes. */
	while (index != NULL) {
		if (innobase_strcasecmp(index->name, index_name) == 0) {
			index_id = index->id;
			*idx_type = index->type;
			*idx_id = index_id;
			break;
		}
		index = UT_LIST_GET_NEXT(indexes, index);
	}

	if (!index_id) {
		dict_table_close(table, FALSE, FALSE);
		return(DB_ERROR);
	}

	if (index_id > 0) {
		ut_ad(index->id == index_id);
		err = ib_create_cursor(
			ib_crsr, table, index, cursor->prebuilt->trx);
	}

	if (*ib_crsr != NULL) {
		const ib_cursor_t*	cursor;

		cursor = *(ib_cursor_t**) ib_crsr;

		if (cursor->prebuilt->index == NULL) {
			err = ib_cursor_close(*ib_crsr);
			ut_a(err == DB_SUCCESS);
			*ib_crsr = NULL;
		}
	}

	return(err);
}

/*****************************************************************//**
Open an InnoDB table and return a cursor handle to it.
@return	DB_SUCCESS or err code */
UNIV_INTERN
ib_err_t
ib_cursor_open_table(
/*=================*/
	const char*	name,		/*!< in: table name */
	ib_trx_t	ib_trx,		/*!< in: Current transaction handle
					can be NULL */
	ib_crsr_t*	ib_crsr)	/*!< out,own: InnoDB cursor */
{
	ib_err_t	err;
	dict_table_t*	table;
	char*		normalized_name;

	normalized_name = static_cast<char*>(mem_alloc(ut_strlen(name) + 1));
	ib_normalize_table_name(normalized_name, name);

	if (ib_trx != NULL) {
	       if (!ib_schema_lock_is_exclusive(ib_trx)) {
			table = (dict_table_t*)ib_open_table_by_name(
				normalized_name);
		} else {
			/* NOTE: We do not acquire MySQL metadata lock */
			table = ib_lookup_table_by_name(normalized_name);
		}
	} else {
		table = (dict_table_t*)ib_open_table_by_name(normalized_name);
	}

	mem_free(normalized_name);
	normalized_name = NULL;

	/* It can happen that another thread has created the table but
	not the cluster index or it's a broken table definition. Refuse to
	open if that's the case. */
	if (table != NULL && dict_table_get_first_index(table) == NULL) {
		table = NULL;
	}

	if (table != NULL) {
		err = ib_create_cursor_with_index_id(ib_crsr, table, 0,
						     (trx_t*) ib_trx);
	} else {
		err = DB_TABLE_NOT_FOUND;
	}

	return(err);
}

/********************************************************************//**
Free a context struct for a table handle. */
static
void
ib_qry_proc_free(
/*=============*/
	ib_qry_proc_t*	q_proc)		/*!< in, own: qproc struct */
{
	que_graph_free_recursive(q_proc->grph.ins);
	que_graph_free_recursive(q_proc->grph.upd);
	que_graph_free_recursive(q_proc->grph.sel);

	memset(q_proc, 0x0, sizeof(*q_proc));
}

/*****************************************************************//**
set a cursor trx to NULL */
UNIV_INTERN
void
ib_cursor_clear_trx(
/*================*/
	ib_crsr_t	ib_crsr)	/*!< in/out: InnoDB cursor */
{
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_crsr;

	cursor->prebuilt->trx = NULL;
}

/*****************************************************************//**
Reset the cursor.
@return	DB_SUCCESS or err code */
UNIV_INTERN
ib_err_t
ib_cursor_reset(
/*============*/
	ib_crsr_t	ib_crsr)	/*!< in/out: InnoDB cursor */
{
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_crsr;
	row_prebuilt_t*	prebuilt = cursor->prebuilt;

	if (cursor->valid_trx && prebuilt->trx != NULL
	    && prebuilt->trx->n_mysql_tables_in_use > 0) {

		--prebuilt->trx->n_mysql_tables_in_use;
	}

	/* The fields in this data structure are allocated from
	the query heap and so need to be reset too. */
	ib_qry_proc_free(&cursor->q_proc);

	mem_heap_empty(cursor->query_heap);

	return(DB_SUCCESS);
}

/*****************************************************************//**
update the cursor with new transactions and also reset the cursor
@return	DB_SUCCESS or err code */
ib_err_t
ib_cursor_new_trx(
/*==============*/
	ib_crsr_t	ib_crsr,	/*!< in/out: InnoDB cursor */
	ib_trx_t	ib_trx)		/*!< in: transaction */
{
	ib_err_t        err = DB_SUCCESS;
	ib_cursor_t*    cursor = (ib_cursor_t*) ib_crsr;
	trx_t*          trx = (trx_t*) ib_trx;

	row_prebuilt_t*	prebuilt = cursor->prebuilt;

	row_update_prebuilt_trx(prebuilt, trx);

	cursor->valid_trx = TRUE;

	trx_assign_read_view(prebuilt->trx);

        ib_qry_proc_free(&cursor->q_proc);

        mem_heap_empty(cursor->query_heap);

	return(err);
}

/*****************************************************************//**
Commit the transaction in a cursor
@return	DB_SUCCESS or err code */
ib_err_t
ib_cursor_commit_trx(
/*=================*/
	ib_crsr_t	ib_crsr,	/*!< in/out: InnoDB cursor */
	ib_trx_t	ib_trx)		/*!< in: transaction */
{
	ib_err_t        err = DB_SUCCESS;
	ib_cursor_t*    cursor = (ib_cursor_t*) ib_crsr;
	row_prebuilt_t*	prebuilt = cursor->prebuilt;

	ut_ad(prebuilt->trx == (trx_t*) ib_trx);
	err = ib_trx_commit(ib_trx);
	prebuilt->trx = NULL;
	cursor->valid_trx = FALSE;
	return(err);
}

/*****************************************************************//**
Close an InnoDB table and free the cursor.
@return	DB_SUCCESS or err code */
UNIV_INTERN
ib_err_t
ib_cursor_close(
/*============*/
	ib_crsr_t	ib_crsr)	/*!< in,own: InnoDB cursor */
{
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_crsr;
	row_prebuilt_t*	prebuilt;
	trx_t*		trx;

	if (!cursor) {
		return(DB_SUCCESS);
	}

	prebuilt = cursor->prebuilt;
	trx = prebuilt->trx;

	ib_qry_proc_free(&cursor->q_proc);

	/* The transaction could have been detached from the cursor. */
	if (cursor->valid_trx && trx != NULL
	    && trx->n_mysql_tables_in_use > 0) {
		--trx->n_mysql_tables_in_use;
	}

	row_prebuilt_free(prebuilt, FALSE);
	cursor->prebuilt = NULL;

	mem_heap_free(cursor->query_heap);
	mem_heap_free(cursor->heap);
	cursor = NULL;

	return(DB_SUCCESS);
}

/*****************************************************************//**
Close the table, decrement n_ref_count count.
@return	DB_SUCCESS or err code */
UNIV_INTERN
ib_err_t
ib_cursor_close_table(
/*==================*/
	ib_crsr_t	ib_crsr)	/*!< in,own: InnoDB cursor */
{
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_crsr;
	row_prebuilt_t*	prebuilt = cursor->prebuilt;

	if (prebuilt && prebuilt->table) {
		dict_table_close(prebuilt->table, FALSE, FALSE);
	}

	return(DB_SUCCESS);
}
/**********************************************************************//**
Run the insert query and do error handling.
@return	DB_SUCCESS or error code */
UNIV_INLINE
ib_err_t
ib_insert_row_with_lock_retry(
/*==========================*/
	que_thr_t*	thr,		/*!< in: insert query graph */
	ins_node_t*	node,		/*!< in: insert node for the query */
	trx_savept_t*	savept)		/*!< in: savepoint to rollback to
					in case of an error */
{
	trx_t*		trx;
	ib_err_t	err;
	ib_bool_t	lock_wait;

	trx = thr_get_trx(thr);

	do {
		thr->run_node = node;
		thr->prev_node = node;

		row_ins_step(thr);

		err = trx->error_state;

		if (err != DB_SUCCESS) {
			que_thr_stop_for_mysql(thr);

			thr->lock_state = QUE_THR_LOCK_ROW;
			lock_wait = ib_handle_errors(&err, trx, thr, savept);
			thr->lock_state = QUE_THR_LOCK_NOLOCK;
		} else {
			lock_wait = FALSE;
		}
	} while (lock_wait);

	return(err);
}

/*****************************************************************//**
Write a row.
@return	DB_SUCCESS or err code */
static
ib_err_t
ib_execute_insert_query_graph(
/*==========================*/
	dict_table_t*	table,		/*!< in: table where to insert */
	que_fork_t*	ins_graph,	/*!< in: query graph */
	ins_node_t*	node)		/*!< in: insert node */
{
	trx_t*		trx;
	que_thr_t*	thr;
	trx_savept_t	savept;
	ib_err_t	err = DB_SUCCESS;

	trx = ins_graph->trx;

	savept = trx_savept_take(trx);

	thr = que_fork_get_first_thr(ins_graph);

	que_thr_move_to_run_state_for_mysql(thr, trx);

	err = ib_insert_row_with_lock_retry(thr, node, &savept);

	if (err == DB_SUCCESS) {
		que_thr_stop_for_mysql_no_error(thr, trx);

		dict_table_n_rows_inc(table);

		srv_stats.n_rows_inserted.inc();
	}

	trx->op_info = "";

	return(err);
}

/*****************************************************************//**
Create an insert query graph node. */
static
void
ib_insert_query_graph_create(
/*==========================*/
	ib_cursor_t*	cursor)		/*!< in: Cursor instance */
{
	ib_qry_proc_t*	q_proc = &cursor->q_proc;
	ib_qry_node_t*	node = &q_proc->node;
	trx_t*		trx = cursor->prebuilt->trx;

	ut_a(trx->state != TRX_STATE_NOT_STARTED);

	if (node->ins == NULL) {
		dtuple_t*	row;
		ib_qry_grph_t*	grph = &q_proc->grph;
		mem_heap_t*	heap = cursor->query_heap;
		dict_table_t*	table = cursor->prebuilt->table;

		node->ins = ins_node_create(INS_DIRECT, table, heap);

		node->ins->select = NULL;
		node->ins->values_list = NULL;

		row = dtuple_create(heap, dict_table_get_n_cols(table));
		dict_table_copy_types(row, table);

		ins_node_set_new_row(node->ins, row);

		grph->ins = static_cast<que_fork_t*>(
			que_node_get_parent(
				pars_complete_graph_for_exec(node->ins, trx,
							     heap)));

		grph->ins->state = QUE_FORK_ACTIVE;
	}
}

/*****************************************************************//**
Insert a row to a table.
@return	DB_SUCCESS or err code */
UNIV_INTERN
ib_err_t
ib_cursor_insert_row(
/*=================*/
	ib_crsr_t	ib_crsr,	/*!< in/out: InnoDB cursor instance */
	const ib_tpl_t	ib_tpl)		/*!< in: tuple to insert */
{
	ib_ulint_t	i;
	ib_qry_node_t*	node;
	ib_qry_proc_t*	q_proc;
	ulint		n_fields;
	dtuple_t*	dst_dtuple;
	ib_err_t	err = DB_SUCCESS;
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_crsr;
	const ib_tuple_t* src_tuple = (const ib_tuple_t*) ib_tpl;

	ib_insert_query_graph_create(cursor);

	ut_ad(src_tuple->type == TPL_TYPE_ROW);

	q_proc = &cursor->q_proc;
	node = &q_proc->node;

	node->ins->state = INS_NODE_ALLOC_ROW_ID;
	dst_dtuple = node->ins->row;

	n_fields = dtuple_get_n_fields(src_tuple->ptr);
	ut_ad(n_fields == dtuple_get_n_fields(dst_dtuple));

	/* Do a shallow copy of the data fields and check for NULL
	constraints on columns. */
	for (i = 0; i < n_fields; i++) {
		ulint		mtype;
		dfield_t*	src_field;
		dfield_t*	dst_field;

		src_field = dtuple_get_nth_field(src_tuple->ptr, i);

		mtype = dtype_get_mtype(dfield_get_type(src_field));

		/* Don't touch the system columns. */
		if (mtype != DATA_SYS) {
			ulint	prtype;

			prtype = dtype_get_prtype(dfield_get_type(src_field));

			if ((prtype & DATA_NOT_NULL)
			    && dfield_is_null(src_field)) {

				err = DB_DATA_MISMATCH;
				break;
			}

			dst_field = dtuple_get_nth_field(dst_dtuple, i);
			ut_ad(mtype
			      == dtype_get_mtype(dfield_get_type(dst_field)));

			/* Do a shallow copy. */
			dfield_set_data(
				dst_field, src_field->data, src_field->len);

			if (dst_field->len != IB_SQL_NULL) {
				UNIV_MEM_ASSERT_RW(dst_field->data,
						   dst_field->len);
			}
		}
	}

	if (err == DB_SUCCESS) {
		err = ib_execute_insert_query_graph(
			src_tuple->index->table, q_proc->grph.ins, node->ins);
	}

	return(err);
}

/*********************************************************************//**
Gets pointer to a prebuilt update vector used in updates.
@return	update vector */
UNIV_INLINE
upd_t*
ib_update_vector_create(
/*====================*/
	ib_cursor_t*	cursor)		/*!< in: current cursor */
{
	trx_t*		trx = cursor->prebuilt->trx;
	mem_heap_t*	heap = cursor->query_heap;
	dict_table_t*	table = cursor->prebuilt->table;
	ib_qry_proc_t*	q_proc = &cursor->q_proc;
	ib_qry_grph_t*	grph = &q_proc->grph;
	ib_qry_node_t*	node = &q_proc->node;

	ut_a(trx->state != TRX_STATE_NOT_STARTED);

	if (node->upd == NULL) {
		node->upd = static_cast<upd_node_t*>(
			row_create_update_node_for_mysql(table, heap));
	}

	grph->upd = static_cast<que_fork_t*>(
		que_node_get_parent(
			pars_complete_graph_for_exec(node->upd, trx, heap)));

	grph->upd->state = QUE_FORK_ACTIVE;

	return(node->upd->update);
}

/**********************************************************************//**
Note that a column has changed. */
static
void
ib_update_col(
/*==========*/

	ib_cursor_t*	cursor,		/*!< in: current cursor */
	upd_field_t*	upd_field,	/*!< in/out: update field */
	ulint		col_no,		/*!< in: column number */
	dfield_t*	dfield)		/*!< in: updated dfield */
{
	ulint		data_len;
	dict_table_t*	table = cursor->prebuilt->table;
	dict_index_t*	index = dict_table_get_first_index(table);

	data_len = dfield_get_len(dfield);

	if (data_len == UNIV_SQL_NULL) {
		dfield_set_null(&upd_field->new_val);
	} else {
		dfield_copy_data(&upd_field->new_val, dfield);
	}

	upd_field->exp = NULL;

	upd_field->orig_len = 0;

	upd_field->field_no = dict_col_get_clust_pos(
		&table->cols[col_no], index);
}

/**********************************************************************//**
Checks which fields have changed in a row and stores the new data
to an update vector.
@return	DB_SUCCESS or err code */
static
ib_err_t
ib_calc_diff(
/*=========*/
	ib_cursor_t*	cursor,		/*!< in: current cursor */
	upd_t*		upd,		/*!< in/out: update vector */
	const ib_tuple_t*old_tuple,	/*!< in: Old tuple in table */
	const ib_tuple_t*new_tuple)	/*!< in: New tuple to update */
{
	ulint		i;
	ulint		n_changed = 0;
	ib_err_t	err = DB_SUCCESS;
	ulint		n_fields = dtuple_get_n_fields(new_tuple->ptr);

	ut_a(old_tuple->type == TPL_TYPE_ROW);
	ut_a(new_tuple->type == TPL_TYPE_ROW);
	ut_a(old_tuple->index->table == new_tuple->index->table);

	for (i = 0; i < n_fields; ++i) {
		ulint		mtype;
		ulint		prtype;
		upd_field_t*	upd_field;
		dfield_t*	new_dfield;
		dfield_t*	old_dfield;

		new_dfield = dtuple_get_nth_field(new_tuple->ptr, i);
		old_dfield = dtuple_get_nth_field(old_tuple->ptr, i);

		mtype = dtype_get_mtype(dfield_get_type(old_dfield));
		prtype = dtype_get_prtype(dfield_get_type(old_dfield));

		/* Skip the system columns */
		if (mtype == DATA_SYS) {
			continue;

		} else if ((prtype & DATA_NOT_NULL)
			   && dfield_is_null(new_dfield)) {

			err = DB_DATA_MISMATCH;
			break;
		}

		if (dfield_get_len(new_dfield) != dfield_get_len(old_dfield)
		    || (!dfield_is_null(old_dfield)
		        && memcmp(dfield_get_data(new_dfield),
			      dfield_get_data(old_dfield),
			      dfield_get_len(old_dfield)) != 0)) {

			upd_field = &upd->fields[n_changed];

			ib_update_col(cursor, upd_field, i, new_dfield);

			++n_changed;
		}
	}

	if (err == DB_SUCCESS) {
		upd->info_bits = 0;
		upd->n_fields = n_changed;
	}

	return(err);
}

/**********************************************************************//**
Run the update query and do error handling.
@return	DB_SUCCESS or error code */
UNIV_INLINE
ib_err_t
ib_update_row_with_lock_retry(
/*==========================*/
	que_thr_t*	thr,		/*!< in: Update query graph */
	upd_node_t*	node,		/*!< in: Update node for the query */
	trx_savept_t*	savept)		/*!< in: savepoint to rollback to
					in case of an error */

{
	trx_t*		trx;
	ib_err_t	err;
	ib_bool_t	lock_wait;

	trx = thr_get_trx(thr);

	do {
		thr->run_node = node;
		thr->prev_node = node;

		row_upd_step(thr);

		err = trx->error_state;

		if (err != DB_SUCCESS) {
			que_thr_stop_for_mysql(thr);

			if (err != DB_RECORD_NOT_FOUND) {
				thr->lock_state = QUE_THR_LOCK_ROW;

				lock_wait = ib_handle_errors(
					&err, trx, thr, savept);

				thr->lock_state = QUE_THR_LOCK_NOLOCK;
			} else {
				lock_wait = FALSE;
			}
		} else {
			lock_wait = FALSE;
		}
	} while (lock_wait);

	return(err);
}

/*********************************************************************//**
Does an update or delete of a row.
@return	DB_SUCCESS or err code */
UNIV_INLINE
ib_err_t
ib_execute_update_query_graph(
/*==========================*/
	ib_cursor_t*	cursor,		/*!< in: Cursor instance */
	btr_pcur_t*	pcur)		/*!< in: Btree persistent cursor */
{
	ib_err_t	err;
	que_thr_t*	thr;
	upd_node_t*	node;
	trx_savept_t	savept;
	trx_t*		trx = cursor->prebuilt->trx;
	dict_table_t*	table = cursor->prebuilt->table;
	ib_qry_proc_t*	q_proc = &cursor->q_proc;

	/* The transaction must be running. */
	ut_a(trx->state != TRX_STATE_NOT_STARTED);

	node = q_proc->node.upd;

	ut_a(dict_index_is_clust(pcur->btr_cur.index));
	btr_pcur_copy_stored_position(node->pcur, pcur);

	ut_a(node->pcur->rel_pos == BTR_PCUR_ON);

	savept = trx_savept_take(trx);

	thr = que_fork_get_first_thr(q_proc->grph.upd);

	node->state = UPD_NODE_UPDATE_CLUSTERED;

	que_thr_move_to_run_state_for_mysql(thr, trx);

	err = ib_update_row_with_lock_retry(thr, node, &savept);

	if (err == DB_SUCCESS) {

		que_thr_stop_for_mysql_no_error(thr, trx);

		if (node->is_delete) {

			dict_table_n_rows_dec(table);

			srv_stats.n_rows_deleted.inc();
		} else {
			srv_stats.n_rows_updated.inc();
		}

	} else if (err == DB_RECORD_NOT_FOUND) {
		trx->error_state = DB_SUCCESS;
	}

	trx->op_info = "";

	return(err);
}

/*****************************************************************//**
Update a row in a table.
@return	DB_SUCCESS or err code */
UNIV_INTERN
ib_err_t
ib_cursor_update_row(
/*=================*/
	ib_crsr_t	ib_crsr,	/*!< in: InnoDB cursor instance */
	const ib_tpl_t	ib_old_tpl,	/*!< in: Old tuple in table */
	const ib_tpl_t	ib_new_tpl)	/*!< in: New tuple to update */
{
	upd_t*		upd;
	ib_err_t	err;
	btr_pcur_t*	pcur;
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_crsr;
	row_prebuilt_t*	prebuilt = cursor->prebuilt;
	const ib_tuple_t*old_tuple = (const ib_tuple_t*) ib_old_tpl;
	const ib_tuple_t*new_tuple = (const ib_tuple_t*) ib_new_tpl;

	if (dict_index_is_clust(prebuilt->index)) {
		pcur = &cursor->prebuilt->pcur;
	} else if (prebuilt->need_to_access_clustered) {
		pcur = &cursor->prebuilt->clust_pcur;
	} else {
		return(DB_ERROR);
	}

	ut_a(old_tuple->type == TPL_TYPE_ROW);
	ut_a(new_tuple->type == TPL_TYPE_ROW);

	upd = ib_update_vector_create(cursor);

	err = ib_calc_diff(cursor, upd, old_tuple, new_tuple);

	if (err == DB_SUCCESS) {
		/* Note that this is not a delete. */
		cursor->q_proc.node.upd->is_delete = FALSE;

		err = ib_execute_update_query_graph(cursor, pcur);
	}

	return(err);
}

/**********************************************************************//**
Build the update query graph to delete a row from an index.
@return	DB_SUCCESS or err code */
static
ib_err_t
ib_delete_row(
/*==========*/
	ib_cursor_t*	cursor,		/*!< in: current cursor */
	btr_pcur_t*	pcur,		/*!< in: Btree persistent cursor */
	const rec_t*	rec)		/*!< in: record to delete */
{
	ulint		i;
	upd_t*		upd;
	ib_err_t	err;
	ib_tuple_t*	tuple;
	ib_tpl_t	ib_tpl;
	ulint		n_cols;
	upd_field_t*	upd_field;
	ib_bool_t	page_format;
	dict_table_t*	table = cursor->prebuilt->table;
	dict_index_t*	index = dict_table_get_first_index(table);

	n_cols = dict_index_get_n_ordering_defined_by_user(index);
	ib_tpl = ib_key_tuple_new(index, n_cols);

	if (!ib_tpl) {
		return(DB_OUT_OF_MEMORY);
	}

	tuple = (ib_tuple_t*) ib_tpl;

	upd = ib_update_vector_create(cursor);

	page_format = dict_table_is_comp(index->table);
	ib_read_tuple(rec, page_format, tuple);

	upd->n_fields = ib_tuple_get_n_cols(ib_tpl);

	for (i = 0; i < upd->n_fields; ++i) {
		dfield_t*	dfield;

		upd_field = &upd->fields[i];
		dfield = dtuple_get_nth_field(tuple->ptr, i);

		dfield_copy_data(&upd_field->new_val, dfield);

		upd_field->exp = NULL;

		upd_field->orig_len = 0;

		upd->info_bits = 0;

		upd_field->field_no = dict_col_get_clust_pos(
			&table->cols[i], index);
	}

	/* Note that this is a delete. */
	cursor->q_proc.node.upd->is_delete = TRUE;

	err = ib_execute_update_query_graph(cursor, pcur);

	ib_tuple_delete(ib_tpl);

	return(err);
}

/*****************************************************************//**
Delete a row in a table.
@return	DB_SUCCESS or err code */
UNIV_INTERN
ib_err_t
ib_cursor_delete_row(
/*=================*/
	ib_crsr_t	ib_crsr)	/*!< in: InnoDB cursor instance */
{
	ib_err_t	err;
	btr_pcur_t*	pcur;
	dict_index_t*	index;
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_crsr;
	row_prebuilt_t*	prebuilt = cursor->prebuilt;

	index = dict_table_get_first_index(prebuilt->index->table);

	/* Check whether this is a secondary index cursor */
	if (index != prebuilt->index) {
		if (prebuilt->need_to_access_clustered) {
			pcur = &prebuilt->clust_pcur;
		} else {
			return(DB_ERROR);
		}
	} else {
		pcur = &prebuilt->pcur;
	}

	if (ib_btr_cursor_is_positioned(pcur)) {
		const rec_t*	rec;
		ib_bool_t	page_format;
		mtr_t		mtr;

		page_format = dict_table_is_comp(index->table);

		mtr_start(&mtr);

		if (btr_pcur_restore_position(
			BTR_SEARCH_LEAF, pcur, &mtr)) {

			rec = btr_pcur_get_rec(pcur);
		} else {
			rec = NULL;
		}

		mtr_commit(&mtr);

		if (rec && !rec_get_deleted_flag(rec, page_format)) {
			err = ib_delete_row(cursor, pcur, rec);
		} else {
			err = DB_RECORD_NOT_FOUND;
		}
	} else {
		err = DB_RECORD_NOT_FOUND;
	}

	return(err);
}

/*****************************************************************//**
Read current row.
@return	DB_SUCCESS or err code */
UNIV_INTERN
ib_err_t
ib_cursor_read_row(
/*===============*/
	ib_crsr_t	ib_crsr,	/*!< in: InnoDB cursor instance */
	ib_tpl_t	ib_tpl)		/*!< out: read cols into this tuple */
{
	ib_err_t	err;
	ib_tuple_t*	tuple = (ib_tuple_t*) ib_tpl;
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_crsr;

	ut_a(cursor->prebuilt->trx->state != TRX_STATE_NOT_STARTED);

	/* When searching with IB_EXACT_MATCH set, row_search_for_mysql()
	will not position the persistent cursor but will copy the record
	found into the row cache. It should be the only entry. */
	if (!ib_cursor_is_positioned(ib_crsr) ) {
		err = DB_RECORD_NOT_FOUND;
	} else {
		mtr_t		mtr;
		btr_pcur_t*	pcur;
		row_prebuilt_t*	prebuilt = cursor->prebuilt;

		if (prebuilt->need_to_access_clustered
		    && tuple->type == TPL_TYPE_ROW) {
			pcur = &prebuilt->clust_pcur;
		} else {
			pcur = &prebuilt->pcur;
		}

		if (pcur == NULL) {
			return(DB_ERROR);
		}

		mtr_start(&mtr);

		if (btr_pcur_restore_position(BTR_SEARCH_LEAF, pcur, &mtr)) {
			const rec_t*	rec;
			ib_bool_t	page_format;

			page_format = dict_table_is_comp(tuple->index->table);
			rec = btr_pcur_get_rec(pcur);

			if (prebuilt->innodb_api_rec &&
			    prebuilt->innodb_api_rec != rec) {
				rec = prebuilt->innodb_api_rec;
			}

			if (!rec_get_deleted_flag(rec, page_format)) {
				ib_read_tuple(rec, page_format, tuple);
				err = DB_SUCCESS;
			} else{
				err = DB_RECORD_NOT_FOUND;
			}

		} else {
			err = DB_RECORD_NOT_FOUND;
		}

		mtr_commit(&mtr);
	}

	return(err);
}

/*****************************************************************//**
Move cursor to the first record in the table.
@return	DB_SUCCESS or err code */
UNIV_INLINE
ib_err_t
ib_cursor_position(
/*===============*/
	ib_cursor_t*	cursor,		/*!< in: InnoDB cursor instance */
	ib_srch_mode_t	mode)		/*!< in: Search mode */
{
	ib_err_t	err;
	row_prebuilt_t*	prebuilt = cursor->prebuilt;
	unsigned char*	buf;

	buf = static_cast<unsigned char*>(mem_alloc(UNIV_PAGE_SIZE));

	/* We want to position at one of the ends, row_search_for_mysql()
	uses the search_tuple fields to work out what to do. */
	dtuple_set_n_fields(prebuilt->search_tuple, 0);

	err = static_cast<ib_err_t>(row_search_for_mysql(
		buf, mode, prebuilt, 0, 0));

	mem_free(buf);

	return(err);
}

/*****************************************************************//**
Move cursor to the first record in the table.
@return	DB_SUCCESS or err code */
UNIV_INTERN
ib_err_t
ib_cursor_first(
/*============*/
	ib_crsr_t	ib_crsr)	/*!< in: InnoDB cursor instance */
{
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_crsr;

	return(ib_cursor_position(cursor, IB_CUR_G));
}

/*****************************************************************//**
Move cursor to the last record in the table.
@return	DB_SUCCESS or err code */
UNIV_INTERN
ib_err_t
ib_cursor_last(
/*===========*/
	ib_crsr_t	ib_crsr)	/*!< in: InnoDB cursor instance */
{
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_crsr;

	return(ib_cursor_position(cursor, IB_CUR_L));
}

/*****************************************************************//**
Move cursor to the next user record in the table.
@return DB_SUCCESS or err code */
UNIV_INTERN
ib_err_t
ib_cursor_next(
/*===========*/
        ib_crsr_t       ib_crsr)        /*!< in: InnoDB cursor instance */
{
        ib_err_t	err;
        ib_cursor_t*    cursor = (ib_cursor_t*) ib_crsr;
        row_prebuilt_t* prebuilt = cursor->prebuilt;
	byte		buf[UNIV_PAGE_SIZE_MAX];

        /* We want to move to the next record */
        dtuple_set_n_fields(prebuilt->search_tuple, 0);

        err = static_cast<ib_err_t>(row_search_for_mysql(
		buf, PAGE_CUR_G, prebuilt, 0, ROW_SEL_NEXT));

        return(err);
}

/*****************************************************************//**
Search for key.
@return	DB_SUCCESS or err code */
UNIV_INTERN
ib_err_t
ib_cursor_moveto(
/*=============*/
	ib_crsr_t	ib_crsr,	/*!< in: InnoDB cursor instance */
	ib_tpl_t	ib_tpl,		/*!< in: Key to search for */
	ib_srch_mode_t	ib_srch_mode)	/*!< in: search mode */
{
	ulint		i;
	ulint		n_fields;
	ib_err_t	err = DB_SUCCESS;
	ib_tuple_t*	tuple = (ib_tuple_t*) ib_tpl;
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_crsr;
	row_prebuilt_t*	prebuilt = cursor->prebuilt;
	dtuple_t*	search_tuple = prebuilt->search_tuple;
	unsigned char*	buf;

	ut_a(tuple->type == TPL_TYPE_KEY);

	n_fields = dict_index_get_n_ordering_defined_by_user(prebuilt->index);

	dtuple_set_n_fields(search_tuple, n_fields);
	dtuple_set_n_fields_cmp(search_tuple, n_fields);

	/* Do a shallow copy */
	for (i = 0; i < n_fields; ++i) {
		dfield_copy(dtuple_get_nth_field(search_tuple, i),
			    dtuple_get_nth_field(tuple->ptr, i));
	}

	ut_a(prebuilt->select_lock_type <= LOCK_NUM);

	prebuilt->innodb_api_rec = NULL;

	buf = static_cast<unsigned char*>(mem_alloc(UNIV_PAGE_SIZE));

	err = static_cast<ib_err_t>(row_search_for_mysql(
		buf, ib_srch_mode, prebuilt, cursor->match_mode, 0));

	mem_free(buf);

	return(err);
}

/*****************************************************************//**
Set the cursor search mode. */
UNIV_INTERN
void
ib_cursor_set_match_mode(
/*=====================*/
	ib_crsr_t	ib_crsr,	/*!< in: Cursor instance */
	ib_match_mode_t	match_mode)	/*!< in: ib_cursor_moveto match mode */
{
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_crsr;

	cursor->match_mode = match_mode;
}

/*****************************************************************//**
Get the dfield instance for the column in the tuple.
@return	dfield instance in tuple */
UNIV_INLINE
dfield_t*
ib_col_get_dfield(
/*==============*/
	ib_tuple_t*	tuple,		/*!< in: tuple instance */
	ulint		col_no)		/*!< in: col no. in tuple */
{
	dfield_t*	dfield;

	dfield = dtuple_get_nth_field(tuple->ptr, col_no);

	return(dfield);
}

/*****************************************************************//**
Predicate to check whether a column type contains variable length data.
@return	DB_SUCCESS or error code */
UNIV_INLINE
ib_err_t
ib_col_is_capped(
/*==============*/
	const dtype_t*  dtype)		/*!< in: column type */
{
	return(static_cast<ib_err_t>(
		(dtype_get_mtype(dtype) == DATA_VARCHAR
		|| dtype_get_mtype(dtype) == DATA_CHAR
		|| dtype_get_mtype(dtype) == DATA_MYSQL
		|| dtype_get_mtype(dtype) == DATA_VARMYSQL
		|| dtype_get_mtype(dtype) == DATA_FIXBINARY
		|| dtype_get_mtype(dtype) == DATA_BINARY)
	       && dtype_get_len(dtype) > 0));
}

/*****************************************************************//**
Set a column of the tuple. Make a copy using the tuple's heap.
@return	DB_SUCCESS or error code */
UNIV_INTERN
ib_err_t
ib_col_set_value(
/*=============*/
	ib_tpl_t	ib_tpl,		/*!< in: tuple instance */
	ib_ulint_t	col_no,		/*!< in: column index in tuple */
	const void*	src,		/*!< in: data value */
	ib_ulint_t	len)		/*!< in: data value len */
{
	const dtype_t*  dtype;
	dfield_t*	dfield;
	void*		dst = NULL;
	ib_tuple_t*	tuple = (ib_tuple_t*) ib_tpl;

	dfield = ib_col_get_dfield(tuple, col_no);

	/* User wants to set the column to NULL. */
	if (len == IB_SQL_NULL) {
		dfield_set_null(dfield);
		return(DB_SUCCESS);
	}

	dtype = dfield_get_type(dfield);

	/* Not allowed to update system columns. */
	if (dtype_get_mtype(dtype) == DATA_SYS) {
		return(DB_DATA_MISMATCH);
	}

	dst = dfield_get_data(dfield);

	/* Since TEXT/CLOB also map to DATA_VARCHAR we need to make an
	exception. Perhaps we need to set the precise type and check
	for that. */
	if (ib_col_is_capped(dtype)) {

		len = ut_min(len, dtype_get_len(dtype));

		if (dst == NULL || len > dfield_get_len(dfield)) {
			dst = mem_heap_alloc(tuple->heap, dtype_get_len(dtype));
			ut_a(dst != NULL);
		}
	} else if (dst == NULL || len > dfield_get_len(dfield)) {
		dst = mem_heap_alloc(tuple->heap, len);
	}

	if (dst == NULL) {
		return(DB_OUT_OF_MEMORY);
	}

	switch (dtype_get_mtype(dtype)) {
	case DATA_INT: {

		if (dtype_get_len(dtype) == len) {
			ibool		usign;

			usign = dtype_get_prtype(dtype) & DATA_UNSIGNED;
			mach_write_int_type(static_cast<byte*>(dst),
					    static_cast<const byte*>(src),
					    len, usign);

		} else {
			return(DB_DATA_MISMATCH);
		}
		break;
	}

	case DATA_FLOAT:
		if (len == sizeof(float)) {
			mach_float_write(static_cast<byte*>(dst), *(float*)src);
		} else {
			return(DB_DATA_MISMATCH);
		}
		break;

	case DATA_DOUBLE:
		if (len == sizeof(double)) {
			mach_double_write(static_cast<byte*>(dst),
					  *(double*)src);
		} else {
			return(DB_DATA_MISMATCH);
		}
		break;

	case DATA_SYS:
		ut_error;
		break;

	case DATA_CHAR: {
		ulint	pad_char = ULINT_UNDEFINED;

		pad_char = dtype_get_pad_char(
			dtype_get_mtype(dtype),	dtype_get_prtype(dtype));

		ut_a(pad_char != ULINT_UNDEFINED);

		memset((byte*) dst + len,
		       pad_char,
		       dtype_get_len(dtype) - len);

		memcpy(dst, src, len);

		len = dtype_get_len(dtype);
		break;
	}
	case DATA_BLOB:
	case DATA_BINARY:
	case DATA_MYSQL:
	case DATA_DECIMAL:
	case DATA_VARCHAR:
	case DATA_VARMYSQL:
	case DATA_FIXBINARY:
		memcpy(dst, src, len);
		break;

	default:
		ut_error;
	}

	if (dst != dfield_get_data(dfield)) {
		dfield_set_data(dfield, dst, len);
	} else {
		dfield_set_len(dfield, len);
	}

	return(DB_SUCCESS);
}

/*****************************************************************//**
Get the size of the data available in a column of the tuple.
@return	bytes avail or IB_SQL_NULL */
UNIV_INTERN
ib_ulint_t
ib_col_get_len(
/*===========*/
	ib_tpl_t	ib_tpl,		/*!< in: tuple instance */
	ib_ulint_t	i)		/*!< in: column index in tuple */
{
	const dfield_t*		dfield;
	ulint			data_len;
	ib_tuple_t*		tuple = (ib_tuple_t*) ib_tpl;

	dfield = ib_col_get_dfield(tuple, i);

	data_len = dfield_get_len(dfield);

	return(data_len == UNIV_SQL_NULL ? IB_SQL_NULL : data_len);
}

/*****************************************************************//**
Copy a column value from the tuple.
@return	bytes copied or IB_SQL_NULL */
UNIV_INLINE
ib_ulint_t
ib_col_copy_value_low(
/*==================*/
	ib_tpl_t	ib_tpl,		/*!< in: tuple instance */
	ib_ulint_t	i,		/*!< in: column index in tuple */
	void*		dst,		/*!< out: copied data value */
	ib_ulint_t	len)		/*!< in: max data value len to copy */
{
	const void*	data;
	const dfield_t*	dfield;
	ulint		data_len;
	ib_tuple_t*	tuple = (ib_tuple_t*) ib_tpl;

	dfield = ib_col_get_dfield(tuple, i);

	data = dfield_get_data(dfield);
	data_len = dfield_get_len(dfield);

	if (data_len != UNIV_SQL_NULL) {

		const dtype_t*  dtype = dfield_get_type(dfield);

		switch (dtype_get_mtype(dfield_get_type(dfield))) {
		case DATA_INT: {
			ibool		usign;
			ullint		ret;

			ut_a(data_len == len);

			usign = dtype_get_prtype(dtype) & DATA_UNSIGNED;
			ret = mach_read_int_type(static_cast<const byte*>(data),
						 data_len, usign);

			if (usign) {
				if (len == 2) {
					*(ib_i16_t*)dst = (ib_i16_t)ret;
				} else if (len == 4) {
					*(ib_i32_t*)dst = (ib_i32_t)ret;
				} else {
					*(ib_i64_t*)dst = (ib_i64_t)ret;
				}
			} else {
				if (len == 2) {
					*(ib_u16_t*)dst = (ib_i16_t)ret;
				} else if (len == 4) {
					*(ib_u32_t*)dst = (ib_i32_t)ret;
				} else {
					*(ib_u64_t*)dst = (ib_i64_t)ret;
				}
			}

			break;
		}
		case DATA_FLOAT:
			if (len == data_len) {
				float	f;

				ut_a(data_len == sizeof(f));
				f = mach_float_read(static_cast<const byte*>(
					data));
				memcpy(dst, &f, sizeof(f));
			} else {
				data_len = 0;
			}
			break;
		case DATA_DOUBLE:
			if (len == data_len) {
				double	d;

				ut_a(data_len == sizeof(d));
				d = mach_double_read(static_cast<const byte*>(
					data));
				memcpy(dst, &d, sizeof(d));
			} else {
				data_len = 0;
			}
			break;
		default:
			data_len = ut_min(data_len, len);
			memcpy(dst, data, data_len);
		}
	} else {
		data_len = IB_SQL_NULL;
	}

	return(data_len);
}

/*****************************************************************//**
Copy a column value from the tuple.
@return	bytes copied or IB_SQL_NULL */
UNIV_INTERN
ib_ulint_t
ib_col_copy_value(
/*==============*/
	ib_tpl_t	ib_tpl,		/*!< in: tuple instance */
	ib_ulint_t	i,		/*!< in: column index in tuple */
	void*		dst,		/*!< out: copied data value */
	ib_ulint_t	len)		/*!< in: max data value len to copy */
{
	return(ib_col_copy_value_low(ib_tpl, i, dst, len));
}

/*****************************************************************//**
Get the InnoDB column attribute from the internal column precise type.
@return	precise type in api format */
UNIV_INLINE
ib_col_attr_t
ib_col_get_attr(
/*============*/
	ulint		prtype)		/*!< in: column definition */
{
	ib_col_attr_t	attr = IB_COL_NONE;

	if (prtype & DATA_UNSIGNED) {
		attr = static_cast<ib_col_attr_t>(attr | IB_COL_UNSIGNED);
	}

	if (prtype & DATA_NOT_NULL) {
		attr = static_cast<ib_col_attr_t>(attr | IB_COL_NOT_NULL);
	}

	return(attr);
}

/*****************************************************************//**
Get a column name from the tuple.
@return	name of the column */
UNIV_INTERN
const char*
ib_col_get_name(
/*============*/
	ib_crsr_t       ib_crsr,        /*!< in: InnoDB cursor instance */
	ib_ulint_t	i)		/*!< in: column index in tuple */
{
	const char*	name;
	ib_cursor_t*    cursor = (ib_cursor_t*) ib_crsr;
	dict_table_t*	table = cursor->prebuilt->table;
	dict_col_t*     col = dict_table_get_nth_col(table, i);
	ulint           col_no = dict_col_get_no(col);

	name = dict_table_get_col_name(table, col_no);

	return(name);
}

/*****************************************************************//**
Get an index field name from the cursor.
@return	name of the field */
UNIV_INTERN
const char*
ib_get_idx_field_name(
/*==================*/
	ib_crsr_t       ib_crsr,        /*!< in: InnoDB cursor instance */
	ib_ulint_t	i)		/*!< in: column index in tuple */
{
	ib_cursor_t*    cursor = (ib_cursor_t*) ib_crsr;
	dict_index_t*	index = cursor->prebuilt->index;
	dict_field_t* 	field;

	if (index) {
		field = dict_index_get_nth_field(cursor->prebuilt->index, i);

		if (field) {
			return(field->name);
		}
	}

	return(NULL);
}

/*****************************************************************//**
Get a column type, length and attributes from the tuple.
@return	len of column data */
UNIV_INLINE
ib_ulint_t
ib_col_get_meta_low(
/*================*/
	ib_tpl_t	ib_tpl,		/*!< in: tuple instance */
	ib_ulint_t	i,		/*!< in: column index in tuple */
	ib_col_meta_t*	ib_col_meta)	/*!< out: column meta data */
{
	ib_u16_t	prtype;
	const dfield_t*	dfield;
	ulint		data_len;
	ib_tuple_t*	tuple = (ib_tuple_t*) ib_tpl;

	dfield = ib_col_get_dfield(tuple, i);

	data_len = dfield_get_len(dfield);

	/* We assume 1-1 mapping between the ENUM and internal type codes. */
	ib_col_meta->type = static_cast<ib_col_type_t>(
		dtype_get_mtype(dfield_get_type(dfield)));

	ib_col_meta->type_len = dtype_get_len(dfield_get_type(dfield));

	prtype = (ib_u16_t) dtype_get_prtype(dfield_get_type(dfield));

	ib_col_meta->attr = ib_col_get_attr(prtype);
	ib_col_meta->client_type = prtype & DATA_MYSQL_TYPE_MASK;

	return(data_len);
}

/*************************************************************//**
Read a signed int 8 bit column from an InnoDB tuple. */
UNIV_INLINE
ib_err_t
ib_tuple_check_int(
/*===============*/
	ib_tpl_t		ib_tpl,	/*!< in: InnoDB tuple */
	ib_ulint_t		i,	/*!< in: column number */
	ib_bool_t		usign,	/*!< in: true if unsigned */
	ulint			size)	/*!< in: size of integer */
{
	ib_col_meta_t		ib_col_meta;

	ib_col_get_meta_low(ib_tpl, i, &ib_col_meta);

	if (ib_col_meta.type != IB_INT) {
		return(DB_DATA_MISMATCH);
	} else if (ib_col_meta.type_len == IB_SQL_NULL) {
		return(DB_UNDERFLOW);
	} else if (ib_col_meta.type_len != size) {
		return(DB_DATA_MISMATCH);
	} else if ((ib_col_meta.attr & IB_COL_UNSIGNED) && !usign) {
		return(DB_DATA_MISMATCH);
	}

	return(DB_SUCCESS);
}

/*************************************************************//**
Read a signed int 8 bit column from an InnoDB tuple.
@return	DB_SUCCESS or error */
UNIV_INTERN
ib_err_t
ib_tuple_read_i8(
/*=============*/
	ib_tpl_t		ib_tpl,	/*!< in: InnoDB tuple */
	ib_ulint_t		i,	/*!< in: column number */
	ib_i8_t*		ival)	/*!< out: integer value */
{
	ib_err_t		err;

	err = ib_tuple_check_int(ib_tpl, i, IB_FALSE, sizeof(*ival));

	if (err == DB_SUCCESS) {
		ib_col_copy_value_low(ib_tpl, i, ival, sizeof(*ival));
	}

	return(err);
}

/*************************************************************//**
Read an unsigned int 8 bit column from an InnoDB tuple.
@return	DB_SUCCESS or error */
UNIV_INTERN
ib_err_t
ib_tuple_read_u8(
/*=============*/
	ib_tpl_t		ib_tpl,	/*!< in: InnoDB tuple */
	ib_ulint_t		i,	/*!< in: column number */
	ib_u8_t*		ival)	/*!< out: integer value */
{
	ib_err_t		err;

	err = ib_tuple_check_int(ib_tpl, i, IB_TRUE, sizeof(*ival));

	if (err == DB_SUCCESS) {
		ib_col_copy_value_low(ib_tpl, i, ival, sizeof(*ival));
	}

	return(err);
}

/*************************************************************//**
Read a signed int 16 bit column from an InnoDB tuple.
@return	DB_SUCCESS or error */
UNIV_INTERN
ib_err_t
ib_tuple_read_i16(
/*==============*/
	ib_tpl_t		ib_tpl,	/*!< in: InnoDB tuple */
	ib_ulint_t		i,	/*!< in: column number */
	ib_i16_t*		ival)	/*!< out: integer value */
{
	ib_err_t		err;

	err = ib_tuple_check_int(ib_tpl, i, FALSE, sizeof(*ival));

	if (err == DB_SUCCESS) {
		ib_col_copy_value_low(ib_tpl, i, ival, sizeof(*ival));
	}

	return(err);
}

/*************************************************************//**
Read an unsigned int 16 bit column from an InnoDB tuple.
@return	DB_SUCCESS or error */
UNIV_INTERN
ib_err_t
ib_tuple_read_u16(
/*==============*/
	ib_tpl_t		ib_tpl,	/*!< in: InnoDB tuple */
	ib_ulint_t		i,	/*!< in: column number */
	ib_u16_t*		ival)	/*!< out: integer value */
{
	ib_err_t		err;

	err = ib_tuple_check_int(ib_tpl, i, IB_TRUE, sizeof(*ival));

	if (err == DB_SUCCESS) {
		ib_col_copy_value_low(ib_tpl, i, ival, sizeof(*ival));
	}

	return(err);
}

/*************************************************************//**
Read a signed int 32 bit column from an InnoDB tuple.
@return	DB_SUCCESS or error */
UNIV_INTERN
ib_err_t
ib_tuple_read_i32(
/*==============*/
	ib_tpl_t		ib_tpl,	/*!< in: InnoDB tuple */
	ib_ulint_t		i,	/*!< in: column number */
	ib_i32_t*		ival)	/*!< out: integer value */
{
	ib_err_t		err;

	err = ib_tuple_check_int(ib_tpl, i, FALSE, sizeof(*ival));

	if (err == DB_SUCCESS) {
		ib_col_copy_value_low(ib_tpl, i, ival, sizeof(*ival));
	}

	return(err);
}

/*************************************************************//**
Read an unsigned int 32 bit column from an InnoDB tuple.
@return	DB_SUCCESS or error */
UNIV_INTERN
ib_err_t
ib_tuple_read_u32(
/*==============*/
	ib_tpl_t		ib_tpl,	/*!< in: InnoDB tuple */
	ib_ulint_t		i,	/*!< in: column number */
	ib_u32_t*		ival)	/*!< out: integer value */
{
	ib_err_t		err;

	err = ib_tuple_check_int(ib_tpl, i, IB_TRUE, sizeof(*ival));

	if (err == DB_SUCCESS) {
		ib_col_copy_value_low(ib_tpl, i, ival, sizeof(*ival));
	}

	return(err);
}

/*************************************************************//**
Read a signed int 64 bit column from an InnoDB tuple.
@return	DB_SUCCESS or error */
UNIV_INTERN
ib_err_t
ib_tuple_read_i64(
/*==============*/
	ib_tpl_t		ib_tpl,	/*!< in: InnoDB tuple */
	ib_ulint_t		i,	/*!< in: column number */
	ib_i64_t*		ival)	/*!< out: integer value */
{
	ib_err_t		err;

	err = ib_tuple_check_int(ib_tpl, i, FALSE, sizeof(*ival));

	if (err == DB_SUCCESS) {
		ib_col_copy_value_low(ib_tpl, i, ival, sizeof(*ival));
	}

	return(err);
}

/*************************************************************//**
Read an unsigned int 64 bit column from an InnoDB tuple.
@return	DB_SUCCESS or error */
UNIV_INTERN
ib_err_t
ib_tuple_read_u64(
/*==============*/
	ib_tpl_t		ib_tpl,	/*!< in: InnoDB tuple */
	ib_ulint_t		i,	/*!< in: column number */
	ib_u64_t*		ival)	/*!< out: integer value */
{
	ib_err_t		err;

	err = ib_tuple_check_int(ib_tpl, i, IB_TRUE, sizeof(*ival));

	if (err == DB_SUCCESS) {
		ib_col_copy_value_low(ib_tpl, i, ival, sizeof(*ival));
	}

	return(err);
}

/*****************************************************************//**
Get a column value pointer from the tuple.
@return	NULL or pointer to buffer */
UNIV_INTERN
const void*
ib_col_get_value(
/*=============*/
	ib_tpl_t	ib_tpl,		/*!< in: tuple instance */
	ib_ulint_t	i)		/*!< in: column index in tuple */
{
	const void*	data;
	const dfield_t*	dfield;
	ulint		data_len;
	ib_tuple_t*	tuple = (ib_tuple_t*) ib_tpl;

	dfield = ib_col_get_dfield(tuple, i);

	data = dfield_get_data(dfield);
	data_len = dfield_get_len(dfield);

	return(data_len != UNIV_SQL_NULL ? data : NULL);
}

/*****************************************************************//**
Get a column type, length and attributes from the tuple.
@return	len of column data */
UNIV_INTERN
ib_ulint_t
ib_col_get_meta(
/*============*/
	ib_tpl_t	ib_tpl,		/*!< in: tuple instance */
	ib_ulint_t	i,		/*!< in: column index in tuple */
	ib_col_meta_t*	ib_col_meta)	/*!< out: column meta data */
{
	return(ib_col_get_meta_low(ib_tpl, i, ib_col_meta));
}

/*****************************************************************//**
"Clear" or reset an InnoDB tuple. We free the heap and recreate the tuple.
@return	new tuple, or NULL */
UNIV_INTERN
ib_tpl_t
ib_tuple_clear(
/*============*/
	ib_tpl_t	ib_tpl)		/*!< in,own: tuple (will be freed) */
{
	const dict_index_t*	index;
	ulint			n_cols;
	ib_tuple_t*		tuple	= (ib_tuple_t*) ib_tpl;
	ib_tuple_type_t		type	= tuple->type;
	mem_heap_t*		heap	= tuple->heap;

	index = tuple->index;
	n_cols = dtuple_get_n_fields(tuple->ptr);

	mem_heap_empty(heap);

	if (type == TPL_TYPE_ROW) {
		return(ib_row_tuple_new_low(index, n_cols, heap));
	} else {
		return(ib_key_tuple_new_low(index, n_cols, heap));
	}
}

/*****************************************************************//**
Create a new cluster key search tuple and copy the contents of  the
secondary index key tuple columns that refer to the cluster index record
to the cluster key. It does a deep copy of the column data.
@return	DB_SUCCESS or error code */
UNIV_INTERN
ib_err_t
ib_tuple_get_cluster_key(
/*=====================*/
	ib_crsr_t	ib_crsr,	/*!< in: secondary index cursor */
	ib_tpl_t*	ib_dst_tpl,	/*!< out,own: destination tuple */
	const ib_tpl_t	ib_src_tpl)	/*!< in: source tuple */
{
	ulint		i;
	ulint		n_fields;
	ib_err_t	err = DB_SUCCESS;
	ib_tuple_t*	dst_tuple = NULL;
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_crsr;
	ib_tuple_t*	src_tuple = (ib_tuple_t*) ib_src_tpl;
	dict_index_t*	clust_index;

	clust_index = dict_table_get_first_index(cursor->prebuilt->table);

	/* We need to ensure that the src tuple belongs to the same table
	as the open cursor and that it's not a tuple for a cluster index. */
	if (src_tuple->type != TPL_TYPE_KEY) {
		return(DB_ERROR);
	} else if (src_tuple->index->table != cursor->prebuilt->table) {
		return(DB_DATA_MISMATCH);
	} else if (src_tuple->index == clust_index) {
		return(DB_ERROR);
	}

	/* Create the cluster index key search tuple. */
	*ib_dst_tpl = ib_clust_search_tuple_create(ib_crsr);

	if (!*ib_dst_tpl) {
		return(DB_OUT_OF_MEMORY);
	}

	dst_tuple = (ib_tuple_t*) *ib_dst_tpl;
	ut_a(dst_tuple->index == clust_index);

	n_fields = dict_index_get_n_unique(dst_tuple->index);

	/* Do a deep copy of the data fields. */
	for (i = 0; i < n_fields; i++) {
		ulint		pos;
		dfield_t*	src_field;
		dfield_t*	dst_field;

		pos = dict_index_get_nth_field_pos(
			src_tuple->index, dst_tuple->index, i);

		ut_a(pos != ULINT_UNDEFINED);

		src_field = dtuple_get_nth_field(src_tuple->ptr, pos);
		dst_field = dtuple_get_nth_field(dst_tuple->ptr, i);

		if (!dfield_is_null(src_field)) {
			UNIV_MEM_ASSERT_RW(src_field->data, src_field->len);

			dst_field->data = mem_heap_dup(
				dst_tuple->heap,
				src_field->data,
				src_field->len);

			dst_field->len = src_field->len;
		} else {
			dfield_set_null(dst_field);
		}
	}

	return(err);
}

/*****************************************************************//**
Copy the contents of  source tuple to destination tuple. The tuples
must be of the same type and belong to the same table/index.
@return	DB_SUCCESS or error code */
UNIV_INTERN
ib_err_t
ib_tuple_copy(
/*==========*/
	ib_tpl_t	ib_dst_tpl,	/*!< in: destination tuple */
	const ib_tpl_t	ib_src_tpl)	/*!< in: source tuple */
{
	ulint		i;
	ulint		n_fields;
	ib_err_t	err = DB_SUCCESS;
	const ib_tuple_t*src_tuple = (const ib_tuple_t*) ib_src_tpl;
	ib_tuple_t*	dst_tuple = (ib_tuple_t*) ib_dst_tpl;

	/* Make sure src and dst are not the same. */
	ut_a(src_tuple != dst_tuple);

	/* Make sure they are the same type and refer to the same index. */
	if (src_tuple->type != dst_tuple->type
	   || src_tuple->index != dst_tuple->index) {

		return(DB_DATA_MISMATCH);
	}

	n_fields = dtuple_get_n_fields(src_tuple->ptr);
	ut_ad(n_fields == dtuple_get_n_fields(dst_tuple->ptr));

	/* Do a deep copy of the data fields. */
	for (i = 0; i < n_fields; ++i) {
		dfield_t*	src_field;
		dfield_t*	dst_field;

		src_field = dtuple_get_nth_field(src_tuple->ptr, i);
		dst_field = dtuple_get_nth_field(dst_tuple->ptr, i);

		if (!dfield_is_null(src_field)) {
			UNIV_MEM_ASSERT_RW(src_field->data, src_field->len);

			dst_field->data = mem_heap_dup(
				dst_tuple->heap,
				src_field->data,
				src_field->len);

			dst_field->len = src_field->len;
		} else {
			dfield_set_null(dst_field);
		}
	}

	return(err);
}

/*****************************************************************//**
Create an InnoDB tuple used for index/table search.
@return	own: Tuple for current index */
UNIV_INTERN
ib_tpl_t
ib_sec_search_tuple_create(
/*=======================*/
	ib_crsr_t	ib_crsr)	/*!< in: Cursor instance */
{
	ulint		n_cols;
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_crsr;
	dict_index_t*	index = cursor->prebuilt->index;

	n_cols = dict_index_get_n_unique_in_tree(index);
	return(ib_key_tuple_new(index, n_cols));
}

/*****************************************************************//**
Create an InnoDB tuple used for index/table search.
@return	own: Tuple for current index */
UNIV_INTERN
ib_tpl_t
ib_sec_read_tuple_create(
/*=====================*/
	ib_crsr_t	ib_crsr)	/*!< in: Cursor instance */
{
	ulint		n_cols;
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_crsr;
	dict_index_t*	index = cursor->prebuilt->index;

	n_cols = dict_index_get_n_fields(index);
	return(ib_row_tuple_new(index, n_cols));
}

/*****************************************************************//**
Create an InnoDB tuple used for table key operations.
@return	own: Tuple for current table */
UNIV_INTERN
ib_tpl_t
ib_clust_search_tuple_create(
/*=========================*/
	ib_crsr_t	ib_crsr)	/*!< in: Cursor instance */
{
	ulint		n_cols;
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_crsr;
	dict_index_t*	index;

	index = dict_table_get_first_index(cursor->prebuilt->table);

	n_cols = dict_index_get_n_ordering_defined_by_user(index);
	return(ib_key_tuple_new(index, n_cols));
}

/*****************************************************************//**
Create an InnoDB tuple for table row operations.
@return	own: Tuple for current table */
UNIV_INTERN
ib_tpl_t
ib_clust_read_tuple_create(
/*=======================*/
	ib_crsr_t	ib_crsr)	/*!< in: Cursor instance */
{
	ulint		n_cols;
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_crsr;
	dict_index_t*	index;

	index = dict_table_get_first_index(cursor->prebuilt->table);

	n_cols = dict_table_get_n_cols(cursor->prebuilt->table);
	return(ib_row_tuple_new(index, n_cols));
}

/*****************************************************************//**
Return the number of user columns in the tuple definition.
@return	number of user columns */
UNIV_INTERN
ib_ulint_t
ib_tuple_get_n_user_cols(
/*=====================*/
	const ib_tpl_t	ib_tpl)		/*!< in: Tuple for current table */
{
	const ib_tuple_t*	tuple = (const ib_tuple_t*) ib_tpl;

	if (tuple->type == TPL_TYPE_ROW) {
		return(dict_table_get_n_user_cols(tuple->index->table));
	}

	return(dict_index_get_n_ordering_defined_by_user(tuple->index));
}

/*****************************************************************//**
Return the number of columns in the tuple definition.
@return	number of columns */
UNIV_INTERN
ib_ulint_t
ib_tuple_get_n_cols(
/*================*/
	const ib_tpl_t	ib_tpl)		/*!< in: Tuple for table/index */
{
	const ib_tuple_t*	tuple = (const ib_tuple_t*) ib_tpl;

	return(dtuple_get_n_fields(tuple->ptr));
}

/*****************************************************************//**
Destroy an InnoDB tuple. */
UNIV_INTERN
void
ib_tuple_delete(
/*============*/
	ib_tpl_t	ib_tpl)		/*!< in,own: Tuple instance to delete */
{
	ib_tuple_t*	tuple = (ib_tuple_t*) ib_tpl;

	if (!ib_tpl) {
		return;
	}

	mem_heap_free(tuple->heap);
}

/*****************************************************************//**
Get a table id. This function will acquire the dictionary mutex.
@return	DB_SUCCESS if found */
UNIV_INTERN
ib_err_t
ib_table_get_id(
/*============*/
	const char*	table_name,	/*!< in: table to find */
	ib_id_u64_t*	table_id)	/*!< out: table id if found */
{
	ib_err_t	err;

	dict_mutex_enter_for_mysql();

	err = ib_table_get_id_low(table_name, table_id);

	dict_mutex_exit_for_mysql();

	return(err);
}

/*****************************************************************//**
Get an index id.
@return	DB_SUCCESS if found */
UNIV_INTERN
ib_err_t
ib_index_get_id(
/*============*/
	const char*	table_name,	/*!< in: find index for this table */
	const char*	index_name,	/*!< in: index to find */
	ib_id_u64_t*	index_id)	/*!< out: index id if found */
{
	dict_table_t*	table;
	char*		normalized_name;
	ib_err_t	err = DB_TABLE_NOT_FOUND;

	*index_id = 0;

	normalized_name = static_cast<char*>(
		mem_alloc(ut_strlen(table_name) + 1));
	ib_normalize_table_name(normalized_name, table_name);

	table = ib_lookup_table_by_name(normalized_name);

	mem_free(normalized_name);
	normalized_name = NULL;

	if (table != NULL) {
		dict_index_t*	index;

		index = dict_table_get_index_on_name(table, index_name);

		if (index != NULL) {
			/* We only support 32 bit table and index ids. Because
			we need to pack the table id into the index id. */

			*index_id = (table->id);
			*index_id <<= 32;
			*index_id |= (index->id);

			err = DB_SUCCESS;
		}
	}

	return(err);
}

#ifdef __WIN__
#define SRV_PATH_SEPARATOR      '\\'
#else
#define SRV_PATH_SEPARATOR      '/'
#endif


/*****************************************************************//**
Check if cursor is positioned.
@return	IB_TRUE if positioned */
UNIV_INTERN
ib_bool_t
ib_cursor_is_positioned(
/*====================*/
	const ib_crsr_t	ib_crsr)	/*!< in: InnoDB cursor instance */
{
	const ib_cursor_t*	cursor = (const ib_cursor_t*) ib_crsr;
	row_prebuilt_t*		prebuilt = cursor->prebuilt;

	return(ib_btr_cursor_is_positioned(&prebuilt->pcur));
}


/*****************************************************************//**
Checks if the data dictionary is latched in exclusive mode.
@return	TRUE if exclusive latch */
UNIV_INTERN
ib_bool_t
ib_schema_lock_is_exclusive(
/*========================*/
	const ib_trx_t	ib_trx)		/*!< in: transaction */
{
	const trx_t*	trx = (const trx_t*) ib_trx;

	return(trx->dict_operation_lock_mode == RW_X_LATCH);
}

/*****************************************************************//**
Checks if the data dictionary is latched in shared mode.
@return	TRUE if shared latch */
UNIV_INTERN
ib_bool_t
ib_schema_lock_is_shared(
/*=====================*/
	const ib_trx_t	ib_trx)		/*!< in: transaction */
{
	const trx_t*	trx = (const trx_t*) ib_trx;

	return(trx->dict_operation_lock_mode == RW_S_LATCH);
}

/*****************************************************************//**
Set the Lock an InnoDB cursor/table.
@return	DB_SUCCESS or error code */
UNIV_INTERN
ib_err_t
ib_cursor_lock(
/*===========*/
	ib_crsr_t	ib_crsr,	/*!< in/out: InnoDB cursor */
	ib_lck_mode_t	ib_lck_mode)	/*!< in: InnoDB lock mode */
{
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_crsr;
	row_prebuilt_t*	prebuilt = cursor->prebuilt;
	trx_t*		trx = prebuilt->trx;
	dict_table_t*	table = prebuilt->table;

	return(ib_trx_lock_table_with_retry(
		trx, table, (enum lock_mode) ib_lck_mode));
}

/*****************************************************************//**
Set the Lock an InnoDB table using the table id.
@return	DB_SUCCESS or error code */
UNIV_INTERN
ib_err_t
ib_table_lock(
/*==========*/
	ib_trx_t	ib_trx,		/*!< in/out: transaction */
	ib_id_u64_t	table_id,	/*!< in: table id */
	ib_lck_mode_t	ib_lck_mode)	/*!< in: InnoDB lock mode */
{
	ib_err_t	err;
	que_thr_t*	thr;
	mem_heap_t*	heap;
	dict_table_t*	table;
	ib_qry_proc_t	q_proc;
	trx_t*		trx = (trx_t*) ib_trx;

	ut_a(trx->state != TRX_STATE_NOT_STARTED);

	table = ib_open_table_by_id(table_id, FALSE);

	if (table == NULL) {
		return(DB_TABLE_NOT_FOUND);
	}

	ut_a(ib_lck_mode <= static_cast<ib_lck_mode_t>(LOCK_NUM));

	heap = mem_heap_create(128);

	q_proc.node.sel = sel_node_create(heap);

	thr = pars_complete_graph_for_exec(q_proc.node.sel, trx, heap);

	q_proc.grph.sel = static_cast<que_fork_t*>(que_node_get_parent(thr));
	q_proc.grph.sel->state = QUE_FORK_ACTIVE;

	trx->op_info = "setting table lock";

	ut_a(ib_lck_mode == IB_LOCK_IS || ib_lck_mode == IB_LOCK_IX);
	err = static_cast<ib_err_t>(
		lock_table(0, table, (enum lock_mode) ib_lck_mode, thr));

	trx->error_state = err;

	mem_heap_free(heap);

	return(err);
}

/*****************************************************************//**
Unlock an InnoDB table.
@return	DB_SUCCESS or error code */
UNIV_INTERN
ib_err_t
ib_cursor_unlock(
/*=============*/
	ib_crsr_t	ib_crsr)	/*!< in/out: InnoDB cursor */
{
	ib_err_t	err = DB_SUCCESS;
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_crsr;
	row_prebuilt_t*	prebuilt = cursor->prebuilt;

	if (prebuilt->trx->mysql_n_tables_locked > 0) {
		--prebuilt->trx->mysql_n_tables_locked;
	} else {
		err = DB_ERROR;
	}

	return(err);
}

/*****************************************************************//**
Set the Lock mode of the cursor.
@return	DB_SUCCESS or error code */
UNIV_INTERN
ib_err_t
ib_cursor_set_lock_mode(
/*====================*/
	ib_crsr_t	ib_crsr,	/*!< in/out: InnoDB cursor */
	ib_lck_mode_t	ib_lck_mode)	/*!< in: InnoDB lock mode */
{
	ib_err_t	err = DB_SUCCESS;
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_crsr;
	row_prebuilt_t*	prebuilt = cursor->prebuilt;

	ut_a(ib_lck_mode <= static_cast<ib_lck_mode_t>(LOCK_NUM));

	if (ib_lck_mode == IB_LOCK_X) {
		err = ib_cursor_lock(ib_crsr, IB_LOCK_IX);
	} else if (ib_lck_mode == IB_LOCK_S) {
		err = ib_cursor_lock(ib_crsr, IB_LOCK_IS);
	}

	if (err == DB_SUCCESS) {
		prebuilt->select_lock_type = (enum lock_mode) ib_lck_mode;
		ut_a(prebuilt->trx->state != TRX_STATE_NOT_STARTED);
	}

	return(err);
}

/*****************************************************************//**
Set need to access clustered index record. */
UNIV_INTERN
void
ib_cursor_set_cluster_access(
/*=========================*/
	ib_crsr_t	ib_crsr)	/*!< in/out: InnoDB cursor */
{
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_crsr;
	row_prebuilt_t*	prebuilt = cursor->prebuilt;

	prebuilt->need_to_access_clustered = TRUE;
}

/*************************************************************//**
Convert and write an INT column value to an InnoDB tuple.
@return	DB_SUCCESS or error */
UNIV_INLINE
ib_err_t
ib_tuple_write_int(
/*===============*/
	ib_tpl_t	ib_tpl,		/*!< in/out: tuple to write to */
	ulint		col_no,		/*!< in: column number */
	const void*	value,		/*!< in: integer value */
	ulint		value_len)	/*!< in: sizeof value type */
{
	const dfield_t*	dfield;
	ulint		data_len;
	ulint		type_len;
	ib_tuple_t*	tuple = (ib_tuple_t*) ib_tpl;

	ut_a(col_no < ib_tuple_get_n_cols(ib_tpl));

	dfield = ib_col_get_dfield(tuple, col_no);

	data_len = dfield_get_len(dfield);
	type_len = dtype_get_len(dfield_get_type(dfield));

	if (dtype_get_mtype(dfield_get_type(dfield)) != DATA_INT
	    || value_len != data_len) {

		return(DB_DATA_MISMATCH);
	}

	return(ib_col_set_value(ib_tpl, col_no, value, type_len));
}

/*****************************************************************//**
Write an integer value to a column. Integers are stored in big-endian
format and will need to be converted from the host format.
@return	DB_SUCESS or error */
UNIV_INTERN
ib_err_t
ib_tuple_write_i8(
/*==============*/
	ib_tpl_t	ib_tpl,		/*!< in/out: tuple to write to */
	int		col_no,		/*!< in: column number */
	ib_i8_t		val)		/*!< in: value to write */
{
	return(ib_col_set_value(ib_tpl, col_no, &val, sizeof(val)));
}

/*****************************************************************//**
Write an integer value to a column. Integers are stored in big-endian
format and will need to be converted from the host format.
@return	DB_SUCESS or error */
UNIV_INTERN
ib_err_t
ib_tuple_write_i16(
/*===============*/
	ib_tpl_t	ib_tpl,		/*!< in/out: tuple to write to */
	int		col_no,		/*!< in: column number */
	ib_i16_t	val)		/*!< in: value to write */
{
	return(ib_col_set_value(ib_tpl, col_no, &val, sizeof(val)));
}

/*****************************************************************//**
Write an integer value to a column. Integers are stored in big-endian
format and will need to be converted from the host format.
@return	DB_SUCCESS or error */
UNIV_INTERN
ib_err_t
ib_tuple_write_i32(
/*===============*/
	ib_tpl_t	ib_tpl,		/*!< in/out: tuple to write to */
	int		col_no,		/*!< in: column number */
	ib_i32_t	val)		/*!< in: value to write */
{
	return(ib_col_set_value(ib_tpl, col_no, &val, sizeof(val)));
}

/*****************************************************************//**
Write an integer value to a column. Integers are stored in big-endian
format and will need to be converted from the host format.
@return	DB_SUCCESS or error */
UNIV_INTERN
ib_err_t
ib_tuple_write_i64(
/*===============*/
	ib_tpl_t	ib_tpl,		/*!< in/out: tuple to write to */
	int		col_no,		/*!< in: column number */
	ib_i64_t	val)		/*!< in: value to write */
{
	return(ib_col_set_value(ib_tpl, col_no, &val, sizeof(val)));
}

/*****************************************************************//**
Write an integer value to a column. Integers are stored in big-endian
format and will need to be converted from the host format.
@return	DB_SUCCESS or error */
UNIV_INTERN
ib_err_t
ib_tuple_write_u8(
/*==============*/
	ib_tpl_t	ib_tpl,		/*!< in/out: tuple to write to */
	int		col_no,		/*!< in: column number */
	ib_u8_t		val)		/*!< in: value to write */
{
	return(ib_col_set_value(ib_tpl, col_no, &val, sizeof(val)));
}

/*****************************************************************//**
Write an integer value to a column. Integers are stored in big-endian
format and will need to be converted from the host format.
@return	DB_SUCCESS or error */
UNIV_INTERN
ib_err_t
ib_tuple_write_u16(
/*===============*/
	ib_tpl_t	ib_tpl,		/*!< in/out: tupe to write to */
	int		col_no,		/*!< in: column number */
	ib_u16_t	val)		/*!< in: value to write */
{
	return(ib_col_set_value(ib_tpl, col_no, &val, sizeof(val)));
}

/*****************************************************************//**
Write an integer value to a column. Integers are stored in big-endian
format and will need to be converted from the host format.
@return	DB_SUCCESS or error */
UNIV_INTERN
ib_err_t
ib_tuple_write_u32(
/*===============*/
	ib_tpl_t	ib_tpl,		/*!< in/out: tuple to write to */
	int		col_no,		/*!< in: column number */
	ib_u32_t	val)		/*!< in: value to write */
{
	return(ib_col_set_value(ib_tpl, col_no, &val, sizeof(val)));
}

/*****************************************************************//**
Write an integer value to a column. Integers are stored in big-endian
format and will need to be converted from the host format.
@return	DB_SUCCESS or error */
UNIV_INTERN
ib_err_t
ib_tuple_write_u64(
/*===============*/
	ib_tpl_t	ib_tpl,		/*!< in/out: tuple to write to */
	int		col_no,		/*!< in: column number */
	ib_u64_t	val)		/*!< in: value to write */
{
	return(ib_col_set_value(ib_tpl, col_no, &val, sizeof(val)));
}

/*****************************************************************//**
Inform the cursor that it's the start of an SQL statement. */
UNIV_INTERN
void
ib_cursor_stmt_begin(
/*=================*/
	ib_crsr_t	ib_crsr)	/*!< in: cursor */
{
	ib_cursor_t*	cursor = (ib_cursor_t*) ib_crsr;

	cursor->prebuilt->sql_stat_start = TRUE;
}

/*****************************************************************//**
Write a double value to a column.
@return	DB_SUCCESS or error */
UNIV_INTERN
ib_err_t
ib_tuple_write_double(
/*==================*/
	ib_tpl_t	ib_tpl,		/*!< in/out: tuple to write to */
	int		col_no,		/*!< in: column number */
	double		val)		/*!< in: value to write */
{
	const dfield_t*	dfield;
	ib_tuple_t*	tuple = (ib_tuple_t*) ib_tpl;

	dfield = ib_col_get_dfield(tuple, col_no);

	if (dtype_get_mtype(dfield_get_type(dfield)) == DATA_DOUBLE) {
		return(ib_col_set_value(ib_tpl, col_no, &val, sizeof(val)));
	} else {
		return(DB_DATA_MISMATCH);
	}
}

/*************************************************************//**
Read a double column value from an InnoDB tuple.
@return	DB_SUCCESS or error */
UNIV_INTERN
ib_err_t
ib_tuple_read_double(
/*=================*/
	ib_tpl_t	ib_tpl,		/*!< in: InnoDB tuple */
	ib_ulint_t	col_no,		/*!< in: column number */
	double*		dval)		/*!< out: double value */
{
	ib_err_t	err;
	const dfield_t*	dfield;
	ib_tuple_t*	tuple = (ib_tuple_t*) ib_tpl;

	dfield = ib_col_get_dfield(tuple, col_no);

	if (dtype_get_mtype(dfield_get_type(dfield)) == DATA_DOUBLE) {
		ib_col_copy_value_low(ib_tpl, col_no, dval, sizeof(*dval));
		err = DB_SUCCESS;
	} else {
		err = DB_DATA_MISMATCH;
	}

	return(err);
}

/*****************************************************************//**
Write a float value to a column.
@return	DB_SUCCESS or error */
UNIV_INTERN
ib_err_t
ib_tuple_write_float(
/*=================*/
	ib_tpl_t	ib_tpl,		/*!< in/out: tuple to write to */
	int		col_no,		/*!< in: column number */
	float		val)		/*!< in: value to write */
{
	const dfield_t*	dfield;
	ib_tuple_t*	tuple = (ib_tuple_t*) ib_tpl;

	dfield = ib_col_get_dfield(tuple, col_no);

	if (dtype_get_mtype(dfield_get_type(dfield)) == DATA_FLOAT) {
		return(ib_col_set_value(ib_tpl, col_no, &val, sizeof(val)));
	} else {
		return(DB_DATA_MISMATCH);
	}
}

/*************************************************************//**
Read a float value from an InnoDB tuple.
@return	DB_SUCCESS or error */
UNIV_INTERN
ib_err_t
ib_tuple_read_float(
/*================*/
	ib_tpl_t	ib_tpl,		/*!< in: InnoDB tuple */
	ib_ulint_t	col_no,		/*!< in: column number */
	float*		fval)		/*!< out: float value */
{
	ib_err_t	err;
	const dfield_t*	dfield;
	ib_tuple_t*	tuple = (ib_tuple_t*) ib_tpl;

	dfield = ib_col_get_dfield(tuple, col_no);

	if (dtype_get_mtype(dfield_get_type(dfield)) == DATA_FLOAT) {
		ib_col_copy_value_low(ib_tpl, col_no, fval, sizeof(*fval));
		err = DB_SUCCESS;
	} else {
		err = DB_DATA_MISMATCH;
	}

	return(err);
}

/*****************************************************************//**
Truncate a table. The cursor handle will be closed and set to NULL
on success.
@return DB_SUCCESS or error code */
UNIV_INTERN
ib_err_t
ib_cursor_truncate(
/*===============*/
	ib_crsr_t*	ib_crsr,	/*!< in/out: cursor for table
					to truncate */
	ib_id_u64_t*	table_id)	/*!< out: new table id */
{
	ib_err_t        err;
	ib_cursor_t*    cursor = *(ib_cursor_t**) ib_crsr;
	row_prebuilt_t* prebuilt = cursor->prebuilt;

	*table_id = 0;

	err = ib_cursor_lock(*ib_crsr, IB_LOCK_X);

	if (err == DB_SUCCESS) {
		trx_t*          trx;
		dict_table_t*   table = prebuilt->table;

		/* We are going to free the cursor and the prebuilt. Store
		the transaction handle locally. */
		trx = prebuilt->trx;
		err = ib_cursor_close(*ib_crsr);
		ut_a(err == DB_SUCCESS);

		*ib_crsr = NULL;

		/* A temp go around for assertion in trx_start_for_ddl_low
		we already start the trx */
		if (trx->state == TRX_STATE_ACTIVE) {
#ifdef UNIV_DEBUG
			trx->start_file = 0;
#endif /* UNIV_DEBUG */
			trx->dict_operation = TRX_DICT_OP_TABLE;
		}

		/* This function currently commits the transaction
		on success. */
		err = static_cast<ib_err_t>(
			row_truncate_table_for_mysql(table, trx));

		if (err == DB_SUCCESS) {
			*table_id = (table->id);
		}
	}

        return(err);
}

/*****************************************************************//**
Truncate a table.
@return DB_SUCCESS or error code */
UNIV_INTERN
ib_err_t
ib_table_truncate(
/*==============*/
	const char*	table_name,	/*!< in: table name */
	ib_id_u64_t*	table_id)	/*!< out: new table id */
{
	ib_err_t        err;
	dict_table_t*   table;
	ib_err_t        trunc_err;
	ib_trx_t        ib_trx = NULL;
	ib_crsr_t       ib_crsr = NULL;

	ib_trx = ib_trx_begin(IB_TRX_SERIALIZABLE);

	dict_mutex_enter_for_mysql();

	table = dict_table_open_on_name(table_name, TRUE, FALSE,
					DICT_ERR_IGNORE_NONE);

	if (table != NULL && dict_table_get_first_index(table)) {
		err = ib_create_cursor_with_index_id(&ib_crsr, table, 0,
						     (trx_t*) ib_trx);
	} else {
		err = DB_TABLE_NOT_FOUND;
	}

	dict_mutex_exit_for_mysql();

	if (err == DB_SUCCESS) {
		trunc_err = ib_cursor_truncate(&ib_crsr, table_id);
		ut_a(err == DB_SUCCESS);
	} else {
		trunc_err = err;
	}

	if (ib_crsr != NULL) {
		err = ib_cursor_close(ib_crsr);
		ut_a(err == DB_SUCCESS);
	}

	if (trunc_err == DB_SUCCESS) {
		ut_a(ib_trx_state(ib_trx) == static_cast<ib_trx_state_t>(
			TRX_STATE_NOT_STARTED));

		err = ib_trx_release(ib_trx);
		ut_a(err == DB_SUCCESS);
	} else {
		err = ib_trx_rollback(ib_trx);
		ut_a(err == DB_SUCCESS);
	}

        return(trunc_err);
}

/*****************************************************************//**
Frees a possible InnoDB trx object associated with the current THD.
@return 0 or error number */
UNIV_INTERN
ib_err_t
ib_close_thd(
/*=========*/
	void*		thd)	/*!< in: handle to the MySQL thread of the user
				whose resources should be free'd */
{
	innobase_close_thd(static_cast<THD*>(thd));

	return(DB_SUCCESS);
}

/*****************************************************************//**
Return isolation configuration set by "innodb_api_trx_level"
@return trx isolation level*/
UNIV_INTERN
ib_trx_state_t
ib_cfg_trx_level()
/*==============*/
{
	return(static_cast<ib_trx_state_t>(ib_trx_level_setting));
}

/*****************************************************************//**
Return configure value for background commit interval (in seconds)
@return background commit interval (in seconds) */
UNIV_INTERN
ib_ulint_t
ib_cfg_bk_commit_interval()
/*=======================*/
{
	return(static_cast<ib_ulint_t>(ib_bk_commit_interval));
}

/*****************************************************************//**
Get generic configure status
@return configure status*/
UNIV_INTERN
int
ib_cfg_get_cfg()
/*============*/
{
	int	cfg_status;

	cfg_status = (ib_binlog_enabled) ? IB_CFG_BINLOG_ENABLED : 0;

	if (ib_mdl_enabled) {
		cfg_status |= IB_CFG_MDL_ENABLED;
	}

	if (ib_disable_row_lock) {
		cfg_status |= IB_CFG_DISABLE_ROWLOCK;
	}

	return(cfg_status);
}
