/*****************************************************************************

Copyright (c) 1996, 2016, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/dict0crea.h
Database object creation

Created 1/8/1996 Heikki Tuuri
*******************************************************/

#ifndef dict0crea_h
#define dict0crea_h

#include "univ.i"
#include "dict0types.h"
#include "dict0dict.h"
#include "que0types.h"
#include "row0types.h"
#include "mtr0mtr.h"
#include "fsp0space.h"

/*********************************************************************//**
Creates a table create graph.
@return own: table create node */
tab_node_t*
tab_create_graph_create(
/*====================*/
	dict_table_t*	table,		/*!< in: table to create, built as
					a memory data structure */
	mem_heap_t*	heap);		/*!< in: heap where created */
/** Creates an index create graph.
@param[in]	index	index to create, built as a memory data structure
@param[in,out]	heap	heap where created
@param[in]	add_v	new virtual columns added in the same clause with
			add index
@return own: index create node */
ind_node_t*
ind_create_graph_create(
	dict_index_t*		index,
	mem_heap_t*		heap,
	const dict_add_v_col_t*	add_v);

/***********************************************************//**
Creates a table. This is a high-level function used in SQL execution graphs.
@return query thread to run next or NULL */
que_thr_t*
dict_create_table_step(
/*===================*/
	que_thr_t*	thr);		/*!< in: query thread */

/** Builds a tablespace to store various objects.
@param[in,out]	tablespace	Tablespace object describing what to build.
@return DB_SUCCESS or error code. */
dberr_t
dict_build_tablespace(
	Tablespace*	tablespace);

/** Builds a tablespace to contain a table, using file-per-table=1.
@param[in,out]	table	Table to build in its own tablespace.
@return DB_SUCCESS or error code */
dberr_t
dict_build_tablespace_for_table(
	dict_table_t*	table);

/** Assign a new table ID and put it into the table cache and the transaction.
@param[in,out]	table	Table that needs an ID
@param[in,out]	trx	Transaction */
void
dict_table_assign_new_id(
	dict_table_t*	table,
	trx_t*		trx);

/***********************************************************//**
Creates an index. This is a high-level function used in SQL execution
graphs.
@return query thread to run next or NULL */
que_thr_t*
dict_create_index_step(
/*===================*/
	que_thr_t*	thr);		/*!< in: query thread */

/***************************************************************//**
Builds an index definition but doesn't update sys_table.
@return DB_SUCCESS or error code */
void
dict_build_index_def(
/*=================*/
	const dict_table_t*	table,	/*!< in: table */
	dict_index_t*		index,	/*!< in/out: index */
	trx_t*			trx);	/*!< in/out: InnoDB transaction
					handle */
/***************************************************************//**
Creates an index tree for the index if it is not a member of a cluster.
Don't update SYSTEM TABLES.
@return DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
dberr_t
dict_create_index_tree(
/*===================*/
	dict_index_t*	index,	/*!< in/out: index */
	const trx_t*	trx);	/*!< in: InnoDB transaction handle */

/*******************************************************************//**
Recreate the index tree associated with a row in SYS_INDEXES table.
@return	new root page number, or FIL_NULL on failure */
ulint
dict_recreate_index_tree(
/*======================*/
	const dict_table_t*	table,	/*!< in: the table the index
					belongs to */
	btr_pcur_t*		pcur,	/*!< in/out: persistent cursor pointing
					to record in the clustered index of
					SYS_INDEXES table. The cursor may be
					repositioned in this call. */
	mtr_t*			mtr);	/*!< in: mtr having the latch
					on the record page. The mtr may be
					committed and restarted in this call. */

/** Drop the index tree associated with a row in SYS_INDEXES table.
@param[in,out]	rec	SYS_INDEXES record
@param[in,out]	pcur	persistent cursor on rec
@param[in,out]	mtr	mini-transaction
@return	whether freeing the B-tree was attempted */
bool
dict_drop_index_tree(
	rec_t*		rec,
	btr_pcur_t*	pcur,
	mtr_t*		mtr);

/***************************************************************//**
Creates an index tree for the index if it is not a member of a cluster.
Don't update SYSTEM TABLES.
@return	DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
dberr_t
dict_create_index_tree_in_mem(
/*==========================*/
	dict_index_t*	index,		/*!< in/out: index */
	const trx_t*	trx);		/*!< in: InnoDB transaction handle */

/*******************************************************************//**
Truncates the index tree but don't update SYSTEM TABLES.
@return DB_SUCCESS or error */
dberr_t
dict_truncate_index_tree_in_mem(
/*============================*/
	dict_index_t*	index);		/*!< in/out: index */

/*******************************************************************//**
Drops the index tree but don't update SYS_INDEXES table. */
void
dict_drop_index_tree_in_mem(
/*========================*/
	const dict_index_t*	index,	/*!< in: index */
	ulint			page_no);/*!< in: index page-no */

/****************************************************************//**
Creates the foreign key constraints system tables inside InnoDB
at server bootstrap or server start if they are not found or are
not of the right form.
@return DB_SUCCESS or error code */
dberr_t
dict_create_or_check_foreign_constraint_tables(void);
/*================================================*/

/********************************************************************//**
Generate a foreign key constraint name when it was not named by the user.
A generated constraint has a name of the format dbname/tablename_ibfk_NUMBER,
where the numbers start from 1, and are given locally for this table, that is,
the number is not global, as it used to be before MySQL 4.0.18.  */
UNIV_INLINE
dberr_t
dict_create_add_foreign_id(
/*=======================*/
	ulint*		id_nr,		/*!< in/out: number to use in id
					generation; incremented if used */
	const char*	name,		/*!< in: table name */
	dict_foreign_t*	foreign);	/*!< in/out: foreign key */

/** Adds the given set of foreign key objects to the dictionary tables
in the database. This function does not modify the dictionary cache. The
caller must ensure that all foreign key objects contain a valid constraint
name in foreign->id.
@param[in]	local_fk_set	set of foreign key objects, to be added to
the dictionary tables
@param[in]	table		table to which the foreign key objects in
local_fk_set belong to
@param[in,out]	trx		transaction
@return error code or DB_SUCCESS */
dberr_t
dict_create_add_foreigns_to_dictionary(
/*===================================*/
	const dict_foreign_set&	local_fk_set,
	const dict_table_t*	table,
	trx_t*			trx)
	MY_ATTRIBUTE((nonnull, warn_unused_result));

/** Check if a foreign constraint is on columns server as base columns
of any stored column. This is to prevent creating SET NULL or CASCADE
constraint on such columns
@param[in]	local_fk_set	set of foreign key objects, to be added to
the dictionary tables
@param[in]	table		table to which the foreign key objects in
local_fk_set belong to
@return true if yes, otherwise, false */
bool
dict_foreigns_has_s_base_col(
	const dict_foreign_set&	local_fk_set,
	const dict_table_t*	table);

/****************************************************************//**
Creates the tablespaces and datafiles system tables inside InnoDB
at server bootstrap or server start if they are not found or are
not of the right form.
@return DB_SUCCESS or error code */
dberr_t
dict_create_or_check_sys_tablespace(void);
/*=====================================*/
/** Creates the virtual column system tables inside InnoDB
at server bootstrap or server start if they are not found or are
not of the right form.
@return DB_SUCCESS or error code */
dberr_t
dict_create_or_check_sys_virtual();

/** Put a tablespace definition into the data dictionary,
replacing what was there previously.
@param[in]	space	Tablespace id
@param[in]	name	Tablespace name
@param[in]	flags	Tablespace flags
@param[in]	path	Tablespace path
@param[in]	trx	Transaction
@param[in]	commit	If true, commit the transaction
@return error code or DB_SUCCESS */
dberr_t
dict_replace_tablespace_in_dictionary(
	ulint		space_id,
	const char*	name,
	ulint		flags,
	const char*	path,
	trx_t*		trx,
	bool		commit);

/** Delete records from SYS_TABLESPACES and SYS_DATAFILES associated
with a particular tablespace ID.
@param[in]	space	Tablespace ID
@param[in,out]	trx	Current transaction
@return DB_SUCCESS if OK, dberr_t if the operation failed */
dberr_t
dict_delete_tablespace_and_datafiles(
	ulint		space,
	trx_t*		trx);

/********************************************************************//**
Add a foreign key definition to the data dictionary tables.
@return error code or DB_SUCCESS */
dberr_t
dict_create_add_foreign_to_dictionary(
/*==================================*/
	const char*		name,	/*!< in: table name */
	const dict_foreign_t*	foreign,/*!< in: foreign key */
	trx_t*			trx)	/*!< in/out: dictionary transaction */
	MY_ATTRIBUTE((nonnull, warn_unused_result));

/* Table create node structure */
struct tab_node_t{
	que_common_t	common;		/*!< node type: QUE_NODE_TABLE_CREATE */
	dict_table_t*	table;		/*!< table to create, built as a
					memory data structure with
					dict_mem_... functions */
	ins_node_t*	tab_def;	/*!< child node which does the insert of
					the table definition; the row to be
					inserted is built by the parent node  */
	ins_node_t*	col_def;	/*!< child node which does the inserts
					of the column definitions; the row to
					be inserted is built by the parent
					node  */
	ins_node_t*	v_col_def;	/*!< child node which does the inserts
					of the sys_virtual row definitions;
					the row to be inserted is built by
					the parent node  */
	/*----------------------*/
	/* Local storage for this graph node */
	ulint		state;		/*!< node execution state */
	ulint		col_no;		/*!< next column definition to insert */
	ulint		base_col_no;	/*!< next base column to insert */
	mem_heap_t*	heap;		/*!< memory heap used as auxiliary
					storage */
};

/* Table create node states */
#define	TABLE_BUILD_TABLE_DEF	1
#define	TABLE_BUILD_COL_DEF	2
#define	TABLE_BUILD_V_COL_DEF	3
#define	TABLE_ADD_TO_CACHE	4
#define	TABLE_COMPLETED		5

/* Index create node struct */

struct ind_node_t{
	que_common_t	common;		/*!< node type: QUE_NODE_INDEX_CREATE */
	dict_index_t*	index;		/*!< index to create, built as a
					memory data structure with
					dict_mem_... functions */
	ins_node_t*	ind_def;	/*!< child node which does the insert of
					the index definition; the row to be
					inserted is built by the parent node  */
	ins_node_t*	field_def;	/*!< child node which does the inserts
					of the field definitions; the row to
					be inserted is built by the parent
					node  */
	/*----------------------*/
	/* Local storage for this graph node */
	ulint		state;		/*!< node execution state */
	ulint		page_no;	/* root page number of the index */
	dict_table_t*	table;		/*!< table which owns the index */
	dtuple_t*	ind_row;	/* index definition row built */
	ulint		field_no;	/* next field definition to insert */
	mem_heap_t*	heap;		/*!< memory heap used as auxiliary
					storage */
	const dict_add_v_col_t*
			add_v;		/*!< new virtual columns that being
					added along with an add index call */
};

/** Compose a column number for a virtual column, stored in the "POS" field
of Sys_columns. The column number includes both its virtual column sequence
(the "nth" virtual column) and its actual column position in original table
@param[in]	v_pos		virtual column sequence
@param[in]	col_pos		column position in original table definition
@return	composed column position number */
UNIV_INLINE
ulint
dict_create_v_col_pos(
	ulint	v_pos,
	ulint	col_pos);

/** Get the column number for a virtual column (the column position in
original table), stored in the "POS" field of Sys_columns
@param[in]      pos             virtual column position
@return column position in original table */
UNIV_INLINE
ulint
dict_get_v_col_mysql_pos(
        ulint   pos);

/** Get a virtual column sequence (the "nth" virtual column) for a
virtual column, stord in the "POS" field of Sys_columns
@param[in]      pos             virtual column position
@return virtual column sequence */
UNIV_INLINE
ulint
dict_get_v_col_pos(
        ulint   pos);

/* Index create node states */
#define	INDEX_BUILD_INDEX_DEF	1
#define	INDEX_BUILD_FIELD_DEF	2
#define	INDEX_CREATE_INDEX_TREE	3
#define	INDEX_ADD_TO_CACHE	4

#ifndef UNIV_NONINL
#include "dict0crea.ic"
#endif

#endif
