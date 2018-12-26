/*****************************************************************************

Copyright (c) 1997, 2016, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/row0sel.h
Select

Created 12/19/1997 Heikki Tuuri
*******************************************************/

#ifndef row0sel_h
#define row0sel_h

#include "univ.i"
#include "data0data.h"
#include "que0types.h"
#include "dict0types.h"
#include "trx0types.h"
#include "read0types.h"
#include "row0types.h"
#include "que0types.h"
#include "pars0sym.h"
#include "btr0pcur.h"
#include "row0mysql.h"

/*********************************************************************//**
Creates a select node struct.
@return own: select node struct */
sel_node_t*
sel_node_create(
/*============*/
	mem_heap_t*	heap);	/*!< in: memory heap where created */
/*********************************************************************//**
Frees the memory private to a select node when a query graph is freed,
does not free the heap where the node was originally created. */
void
sel_node_free_private(
/*==================*/
	sel_node_t*	node);	/*!< in: select node struct */
/*********************************************************************//**
Frees a prefetch buffer for a column, including the dynamically allocated
memory for data stored there. */
void
sel_col_prefetch_buf_free(
/*======================*/
	sel_buf_t*	prefetch_buf);	/*!< in, own: prefetch buffer */
/*********************************************************************//**
Gets the plan node for the nth table in a join.
@return plan node */
UNIV_INLINE
plan_t*
sel_node_get_nth_plan(
/*==================*/
	sel_node_t*	node,	/*!< in: select node */
	ulint		i);	/*!< in: get ith plan node */
/**********************************************************************//**
Performs a select step. This is a high-level function used in SQL execution
graphs.
@return query thread to run next or NULL */
que_thr_t*
row_sel_step(
/*=========*/
	que_thr_t*	thr);	/*!< in: query thread */
/**********************************************************************//**
Performs an execution step of an open or close cursor statement node.
@return query thread to run next or NULL */
UNIV_INLINE
que_thr_t*
open_step(
/*======*/
	que_thr_t*	thr);	/*!< in: query thread */
/**********************************************************************//**
Performs a fetch for a cursor.
@return query thread to run next or NULL */
que_thr_t*
fetch_step(
/*=======*/
	que_thr_t*	thr);	/*!< in: query thread */
/****************************************************************//**
Sample callback function for fetch that prints each row.
@return always returns non-NULL */
void*
row_fetch_print(
/*============*/
	void*	row,		/*!< in:  sel_node_t* */
	void*	user_arg);	/*!< in:  not used */
/***********************************************************//**
Prints a row in a select result.
@return query thread to run next or NULL */
que_thr_t*
row_printf_step(
/*============*/
	que_thr_t*	thr);	/*!< in: query thread */

/** Copy used fields from cached row.
Copy cache record field by field, don't touch fields that
are not covered by current key.
@param[out]	buf		Where to copy the MySQL row.
@param[in]	cached_rec	What to copy (in MySQL row format).
@param[in]	prebuilt	prebuilt struct. */
void
row_sel_copy_cached_fields_for_mysql(
	byte*		buf,
	const byte*	cached_rec,
	row_prebuilt_t*	prebuilt);

/****************************************************************//**
Converts a key value stored in MySQL format to an Innobase dtuple. The last
field of the key value may be just a prefix of a fixed length field: hence
the parameter key_len. But currently we do not allow search keys where the
last field is only a prefix of the full key field len and print a warning if
such appears. */
void
row_sel_convert_mysql_key_to_innobase(
/*==================================*/
	dtuple_t*	tuple,		/*!< in/out: tuple where to build;
					NOTE: we assume that the type info
					in the tuple is already according
					to index! */
	byte*		buf,		/*!< in: buffer to use in field
					conversions; NOTE that dtuple->data
					may end up pointing inside buf so
					do not discard that buffer while
					the tuple is being used. See
					row_mysql_store_col_in_innobase_format()
					in the case of DATA_INT */
	ulint		buf_len,	/*!< in: buffer length */
	dict_index_t*	index,		/*!< in: index of the key value */
	const byte*	key_ptr,	/*!< in: MySQL key value */
	ulint		key_len,	/*!< in: MySQL key value length */
	trx_t*		trx);		/*!< in: transaction */


/** Searches for rows in the database. This is used in the interface to
MySQL. This function opens a cursor, and also implements fetch next
and fetch prev. NOTE that if we do a search with a full key value
from a unique index (ROW_SEL_EXACT), then we will not store the cursor
position and fetch next or fetch prev must not be tried to the cursor!

@param[out]	buf		buffer for the fetched row in MySQL format
@param[in]	mode		search mode PAGE_CUR_L
@param[in,out]	prebuilt	prebuilt struct for the table handler;
				this contains the info to search_tuple,
				index; if search tuple contains 0 field then
				we position the cursor at start or the end of
				index, depending on 'mode'
@param[in]	match_mode	0 or ROW_SEL_EXACT or ROW_SEL_EXACT_PREFIX
@param[in]	direction	0 or ROW_SEL_NEXT or ROW_SEL_PREV;
				Note: if this is != 0, then prebuilt must has a
				pcur with stored position! In opening of a
				cursor 'direction' should be 0.
@return DB_SUCCESS, DB_RECORD_NOT_FOUND, DB_END_OF_INDEX, DB_DEADLOCK,
DB_LOCK_TABLE_FULL, DB_CORRUPTION, or DB_TOO_BIG_RECORD */
UNIV_INLINE
dberr_t
row_search_for_mysql(
	byte*		buf,
	page_cur_mode_t	mode,
	row_prebuilt_t*	prebuilt,
	ulint		match_mode,
	ulint		direction)
	MY_ATTRIBUTE((warn_unused_result));

/** Searches for rows in the database using cursor.
function is meant for temporary table that are not shared accross connection
and so lot of complexity is reduced especially locking and transaction related.
The cursor is an iterator over the table/index.

@param[out]	buf		buffer for the fetched row in MySQL format
@param[in]	mode		search mode PAGE_CUR_L
@param[in,out]	prebuilt	prebuilt struct for the table handler;
				this contains the info to search_tuple,
				index; if search tuple contains 0 field then
				we position the cursor at start or the end of
				index, depending on 'mode'
@param[in]	match_mode	0 or ROW_SEL_EXACT or ROW_SEL_EXACT_PREFIX
@param[in]	direction	0 or ROW_SEL_NEXT or ROW_SEL_PREV;
				Note: if this is != 0, then prebuilt must has a
				pcur with stored position! In opening of a
				cursor 'direction' should be 0.
@return DB_SUCCESS or error code */
dberr_t
row_search_no_mvcc(
	byte*		buf,
	page_cur_mode_t	mode,
	row_prebuilt_t*	prebuilt,
	ulint		match_mode,
	ulint		direction)
	MY_ATTRIBUTE((warn_unused_result));

/** Searches for rows in the database using cursor.
Function is mainly used for tables that are shared accorss connection and
so it employs technique that can help re-construct the rows that
transaction is suppose to see.
It also has optimization such as pre-caching the rows, using AHI, etc.

@param[out]	buf		buffer for the fetched row in MySQL format
@param[in]	mode		search mode PAGE_CUR_L
@param[in,out]	prebuilt	prebuilt struct for the table handler;
				this contains the info to search_tuple,
				index; if search tuple contains 0 field then
				we position the cursor at start or the end of
				index, depending on 'mode'
@param[in]	match_mode	0 or ROW_SEL_EXACT or ROW_SEL_EXACT_PREFIX
@param[in]	direction	0 or ROW_SEL_NEXT or ROW_SEL_PREV;
				Note: if this is != 0, then prebuilt must has a
				pcur with stored position! In opening of a
				cursor 'direction' should be 0.
@param[in]	ins_sel_stmt	if true, then this statement is
				insert .... select statement. For normal table
				this can be detected by checking out locked
				tables using trx->mysql_n_tables_locked > 0
				condition. For intrinsic table
				external_lock is not invoked and so condition
				above will not stand valid instead this is
				traced using alternative condition
				at caller level.
@return DB_SUCCESS or error code */
dberr_t
row_search_mvcc(
	byte*		buf,
	page_cur_mode_t	mode,
	row_prebuilt_t*	prebuilt,
	ulint		match_mode,
	ulint		direction)
	MY_ATTRIBUTE((warn_unused_result));

/********************************************************************//**
Count rows in a R-Tree leaf level.
@return DB_SUCCESS if successful */
dberr_t
row_count_rtree_recs(
/*=================*/
	row_prebuilt_t*	prebuilt,	/*!< in: prebuilt struct for the
					table handle; this contains the info
					of search_tuple, index; if search
					tuple contains 0 fields then we
					position the cursor at the start or
					the end of the index, depending on
					'mode' */
	ulint*		n_rows);	/*!< out: number of entries
					seen in the consistent read */

/*******************************************************************//**
Checks if MySQL at the moment is allowed for this table to retrieve a
consistent read result, or store it to the query cache.
@return TRUE if storing or retrieving from the query cache is permitted */
ibool
row_search_check_if_query_cache_permitted(
/*======================================*/
	trx_t*		trx,		/*!< in: transaction object */
	const char*	norm_name);	/*!< in: concatenation of database name,
					'/' char, table name */
/*******************************************************************//**
Read the max AUTOINC value from an index.
@return DB_SUCCESS if all OK else error code */
dberr_t
row_search_max_autoinc(
/*===================*/
	dict_index_t*	index,		/*!< in: index to search */
	const char*	col_name,	/*!< in: autoinc column name */
	ib_uint64_t*	value)		/*!< out: AUTOINC value read */
	MY_ATTRIBUTE((nonnull, warn_unused_result));

/** A structure for caching column values for prefetched rows */
struct sel_buf_t{
	byte*		data;	/*!< data, or NULL; if not NULL, this field
				has allocated memory which must be explicitly
				freed; can be != NULL even when len is
				UNIV_SQL_NULL */
	ulint		len;	/*!< data length or UNIV_SQL_NULL */
	ulint		val_buf_size;
				/*!< size of memory buffer allocated for data:
				this can be more than len; this is defined
				when data != NULL */
};

/** Query plan */
struct plan_t{
	dict_table_t*	table;		/*!< table struct in the dictionary
					cache */
	dict_index_t*	index;		/*!< table index used in the search */
	btr_pcur_t	pcur;		/*!< persistent cursor used to search
					the index */
	ibool		asc;		/*!< TRUE if cursor traveling upwards */
	ibool		pcur_is_open;	/*!< TRUE if pcur has been positioned
					and we can try to fetch new rows */
	ibool		cursor_at_end;	/*!< TRUE if the cursor is open but
					we know that there are no more
					qualifying rows left to retrieve from
					the index tree; NOTE though, that
					there may still be unprocessed rows in
					the prefetch stack; always FALSE when
					pcur_is_open is FALSE */
	ibool		stored_cursor_rec_processed;
					/*!< TRUE if the pcur position has been
					stored and the record it is positioned
					on has already been processed */
	que_node_t**	tuple_exps;	/*!< array of expressions
					which are used to calculate
					the field values in the search
					tuple: there is one expression
					for each field in the search
					tuple */
	dtuple_t*	tuple;		/*!< search tuple */
	page_cur_mode_t	mode;		/*!< search mode: PAGE_CUR_G, ... */
	ulint		n_exact_match;	/*!< number of first fields in
					the search tuple which must be
					exactly matched */
	ibool		unique_search;	/*!< TRUE if we are searching an
					index record with a unique key */
	ulint		n_rows_fetched;	/*!< number of rows fetched using pcur
					after it was opened */
	ulint		n_rows_prefetched;/*!< number of prefetched rows cached
					for fetch: fetching several rows in
					the same mtr saves CPU time */
	ulint		first_prefetched;/*!< index of the first cached row in
					select buffer arrays for each column */
	ibool		no_prefetch;	/*!< no prefetch for this table */
	sym_node_list_t	columns;	/*!< symbol table nodes for the columns
					to retrieve from the table */
	UT_LIST_BASE_NODE_T(func_node_t)
			end_conds;	/*!< conditions which determine the
					fetch limit of the index segment we
					have to look at: when one of these
					fails, the result set has been
					exhausted for the cursor in this
					index; these conditions are normalized
					so that in a comparison the column
					for this table is the first argument */
	UT_LIST_BASE_NODE_T(func_node_t)
			other_conds;	/*!< the rest of search conditions we can
					test at this table in a join */
	ibool		must_get_clust;	/*!< TRUE if index is a non-clustered
					index and we must also fetch the
					clustered index record; this is the
					case if the non-clustered record does
					not contain all the needed columns, or
					if this is a single-table explicit
					cursor, or a searched update or
					delete */
	ulint*		clust_map;	/*!< map telling how clust_ref is built
					from the fields of a non-clustered
					record */
	dtuple_t*	clust_ref;	/*!< the reference to the clustered
					index entry is built here if index is
					a non-clustered index */
	btr_pcur_t	clust_pcur;	/*!< if index is non-clustered, we use
					this pcur to search the clustered
					index */
	mem_heap_t*	old_vers_heap;	/*!< memory heap used in building an old
					version of a row, or NULL */
};

/** Select node states */
enum sel_node_state {
	SEL_NODE_CLOSED,	/*!< it is a declared cursor which is not
				currently open */
	SEL_NODE_OPEN,		/*!< intention locks not yet set on tables */
	SEL_NODE_FETCH,		/*!< intention locks have been set */
	SEL_NODE_NO_MORE_ROWS	/*!< cursor has reached the result set end */
};

/** Select statement node */
struct sel_node_t{
	que_common_t	common;		/*!< node type: QUE_NODE_SELECT */
	enum sel_node_state
			state;	/*!< node state */
	que_node_t*	select_list;	/*!< select list */
	sym_node_t*	into_list;	/*!< variables list or NULL */
	sym_node_t*	table_list;	/*!< table list */
	ibool		asc;		/*!< TRUE if the rows should be fetched
					in an ascending order */
	ibool		set_x_locks;	/*!< TRUE if the cursor is for update or
					delete, which means that a row x-lock
					should be placed on the cursor row */
	ulint		row_lock_mode;	/*!< LOCK_X or LOCK_S */
	ulint		n_tables;	/*!< number of tables */
	ulint		fetch_table;	/*!< number of the next table to access
					in the join */
	plan_t*		plans;		/*!< array of n_tables many plan nodes
					containing the search plan and the
					search data structures */
	que_node_t*	search_cond;	/*!< search condition */
	ReadView*	read_view;	/*!< if the query is a non-locking
					consistent read, its read view is
					placed here, otherwise NULL */
	ibool		consistent_read;/*!< TRUE if the select is a consistent,
					non-locking read */
	order_node_t*	order_by;	/*!< order by column definition, or
					NULL */
	ibool		is_aggregate;	/*!< TRUE if the select list consists of
					aggregate functions */
	ibool		aggregate_already_fetched;
					/*!< TRUE if the aggregate row has
					already been fetched for the current
					cursor */
	ibool		can_get_updated;/*!< this is TRUE if the select
					is in a single-table explicit
					cursor which can get updated
					within the stored procedure,
					or in a searched update or
					delete; NOTE that to determine
					of an explicit cursor if it
					can get updated, the parser
					checks from a stored procedure
					if it contains positioned
					update or delete statements */
	sym_node_t*	explicit_cursor;/*!< not NULL if an explicit cursor */
	UT_LIST_BASE_NODE_T(sym_node_t)
			copy_variables; /*!< variables whose values we have to
					copy when an explicit cursor is opened,
					so that they do not change between
					fetches */
};

/** Fetch statement node */
struct fetch_node_t{
	que_common_t	common;		/*!< type: QUE_NODE_FETCH */
	sel_node_t*	cursor_def;	/*!< cursor definition */
	sym_node_t*	into_list;	/*!< variables to set */

	pars_user_func_t*
			func;		/*!< User callback function or NULL.
					The first argument to the function
					is a sel_node_t*, containing the
					results of the SELECT operation for
					one row. If the function returns
					NULL, it is not interested in
					further rows and the cursor is
					modified so (cursor % NOTFOUND) is
					true. If it returns not-NULL,
					continue normally. See
					row_fetch_print() for an example
					(and a useful debugging tool). */
};

/** Open or close cursor operation type */
enum open_node_op {
	ROW_SEL_OPEN_CURSOR,	/*!< open cursor */
	ROW_SEL_CLOSE_CURSOR	/*!< close cursor */
};

/** Open or close cursor statement node */
struct open_node_t{
	que_common_t	common;		/*!< type: QUE_NODE_OPEN */
	enum open_node_op
			op_type;	/*!< operation type: open or
					close cursor */
	sel_node_t*	cursor_def;	/*!< cursor definition */
};

/** Row printf statement node */
struct row_printf_node_t{
	que_common_t	common;		/*!< type: QUE_NODE_ROW_PRINTF */
	sel_node_t*	sel_node;	/*!< select */
};

/** Search direction for the MySQL interface */
enum row_sel_direction {
	ROW_SEL_NEXT = 1,	/*!< ascending direction */
	ROW_SEL_PREV = 2	/*!< descending direction */
};

/** Match mode for the MySQL interface */
enum row_sel_match_mode {
	ROW_SEL_EXACT = 1,	/*!< search using a complete key value */
	ROW_SEL_EXACT_PREFIX	/*!< search using a key prefix which
				must match rows: the prefix may
				contain an incomplete field (the last
				field in prefix may be just a prefix
				of a fixed length column) */
};

#ifdef UNIV_DEBUG
/** Convert a non-SQL-NULL field from Innobase format to MySQL format. */
# define row_sel_field_store_in_mysql_format(dest,templ,idx,field,src,len,sec) \
        row_sel_field_store_in_mysql_format_func(dest,templ,idx,field,src,len,sec)
#else /* UNIV_DEBUG */
/** Convert a non-SQL-NULL field from Innobase format to MySQL format. */
# define row_sel_field_store_in_mysql_format(dest,templ,idx,field,src,len,sec) \
        row_sel_field_store_in_mysql_format_func(dest,templ,src,len)
#endif /* UNIV_DEBUG */

/** Stores a non-SQL-NULL field in the MySQL format. The counterpart of this
function is row_mysql_store_col_in_innobase_format() in row0mysql.cc.
@param[in,out]	dest		buffer where to store; NOTE
				that BLOBs are not in themselves stored
				here: the caller must allocate and copy
				the BLOB into buffer before, and pass
				the pointer to the BLOB in 'data'
@param[in]	templ		MySQL column template. Its following fields
				are referenced: type, is_unsigned, mysql_col_len,
				mbminlen, mbmaxlen
@param[in]	index		InnoDB index
@param[in]	field_no	templ->rec_field_no or templ->clust_rec_field_no
				or templ->icp_rec_field_no
@param[in]	data		data to store
@param[in]	len		length of the data
@param[in]	sec_field	secondary index field no if the secondary index
				record but the prebuilt template is in
				clustered index format and used only for end
				range comparison. */
void
row_sel_field_store_in_mysql_format_func(
	byte*				dest,
	const mysql_row_templ_t*	templ,
#ifdef UNIV_DEBUG
	const dict_index_t*		index,
	ulint				field_no,
#endif /* UNIV_DEBUG */
	const byte*			data,
	ulint				len
#ifdef UNIV_DEBUG
	,ulint				sec_field
#endif /* UNIV_DEBUG */
	);

#ifndef UNIV_NONINL
#include "row0sel.ic"
#endif

#endif
