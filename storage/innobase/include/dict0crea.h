/*****************************************************************************

Copyright (c) 1996, 2012, Oracle and/or its affiliates. All Rights Reserved.

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

/*********************************************************************//**
Creates a table create graph.
@return	own: table create node */
UNIV_INTERN
tab_node_t*
tab_create_graph_create(
/*====================*/
	dict_table_t*	table,	/*!< in: table to create, built as a memory data
				structure */
	mem_heap_t*	heap,	/*!< in: heap where created */
	bool		commit);/*!< in: true if the commit node should be
				added to the query graph */
/*********************************************************************//**
Creates an index create graph.
@return	own: index create node */
UNIV_INTERN
ind_node_t*
ind_create_graph_create(
/*====================*/
	dict_index_t*	index,	/*!< in: index to create, built as a memory data
				structure */
	mem_heap_t*	heap,	/*!< in: heap where created */
	bool		commit);/*!< in: true if the commit node should be
				added to the query graph */
/***********************************************************//**
Creates a table. This is a high-level function used in SQL execution graphs.
@return	query thread to run next or NULL */
UNIV_INTERN
que_thr_t*
dict_create_table_step(
/*===================*/
	que_thr_t*	thr);	/*!< in: query thread */
/***************************************************************//**
Builds a tablespace, if configured.
@return	DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
dict_build_tablespace(
/*==================*/
	dict_table_t*	table,	/*!< in: table */
	trx_t*		trx);	/*!< in: InnoDB transaction handle */
/***********************************************************//**
Creates an index. This is a high-level function used in SQL execution
graphs.
@return	query thread to run next or NULL */
UNIV_INTERN
que_thr_t*
dict_create_index_step(
/*===================*/
	que_thr_t*	thr);	/*!< in: query thread */
/***************************************************************//**
Builds an index definition but don't update sys_table.
This interface is generally used for temp-tables for which we don't
update SYS_XXXX table during creation for performance.
@return	DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
dict_build_index_def(
/*=================*/
	dict_table_t*	table,	/*!< in: table */
	dict_index_t*	index,	/*!< in: index */
	trx_t*		trx);	/*!< in: InnoDB transaction handle */
/***************************************************************//**
Creates an index tree for the index if it is not a member of a cluster.
Don't update SYS_XXXX table.
@return	DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
UNIV_INTERN
dberr_t
dict_create_index_tree(
/*====================*/
	dict_index_t*	index,	/*!< in: index */
	trx_t*		trx,	/*!< in: InnoDB transaction handle */
	mem_heap_t*	heap);	/*!< in: memory heap from which the memory for
					the built tuple is allocated */
/*******************************************************************//**
Truncates the index tree associated with a row in SYS_INDEXES table.
@return	new root page number, or FIL_NULL on failure */
UNIV_INTERN
ulint
dict_truncate_index_tree_step(
/*==========================*/
	dict_table_t*	table,	/*!< in: the table the index belongs to */
	ulint		space,	/*!< in: 0=truncate,
				nonzero=create the index tree in the
				given tablespace */
	btr_pcur_t*	pcur,	/*!< in/out: persistent cursor pointing to
				record in the clustered index of
				SYS_INDEXES table. The cursor may be
				repositioned in this call. */
	mtr_t*		mtr);	/*!< in: mtr having the latch
				on the record page. The mtr may be
				committed and restarted in this call. */
/*******************************************************************//**
Truncates the index tree but don't update SYS_XXXX table.
This interface is generally used for temp-tables for which we don't
update SYS_XXXX table on creation.
@return	new root page number, or FIL_NULL on failure */
UNIV_INTERN
void
dict_truncate_index_tree(
/*=====================*/
	dict_index_t*	index,	/*!< in: index */
	ulint		space);	/*!< in: 0=truncate,
				nonzero=create the index tree in the
				given tablespace */
/*******************************************************************//**
Drops the index tree associated with a row in SYS_INDEXES table. */
UNIV_INTERN
void
dict_drop_index_tree_step(
/*======================*/
	rec_t*	rec,	/*!< in/out: record in the clustered index
			of SYS_INDEXES table */
	mtr_t*	mtr);	/*!< in: mtr having the latch on the record page */
/*******************************************************************//**
Drops the index tree but don't update SYS_INDEXES table.
This interface is generally used for temp-tables for which we don't
update SYS_XXXX table on creation. */
UNIV_INTERN
void
dict_drop_index_tree(
/*=================*/
	dict_index_t*   index,		/*!< in: index */
	ulint		page_no);	/*!< in: index page-no */
/****************************************************************//**
Creates the foreign key constraints system tables inside InnoDB
at server bootstrap or server start if they are not found or are
not of the right form.
@return	DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
dict_create_or_check_foreign_constraint_tables(void);
/*================================================*/
/********************************************************************//**
Adds foreign key definitions to data dictionary tables in the database. We
look at table->foreign_list, and also generate names to constraints that were
not named by the user. A generated constraint has a name of the format
databasename/tablename_ibfk_NUMBER, where the numbers start from 1, and are
given locally for this table, that is, the number is not global, as in the
old format constraints < 4.0.18 it used to be.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
dict_create_add_foreigns_to_dictionary(
/*===================================*/
	ulint		start_id,/*!< in: if we are actually doing ALTER TABLE
				ADD CONSTRAINT, we want to generate constraint
				numbers which are bigger than in the table so
				far; we number the constraints from
				start_id + 1 up; start_id should be set to 0 if
				we are creating a new table, or if the table
				so far has no constraints for which the name
				was generated here */
	dict_table_t*	table,	/*!< in: table */
	trx_t*		trx)	/*!< in: transaction */
	__attribute__((nonnull, warn_unused_result));
/****************************************************************//**
Creates the tablespaces and datafiles system tables inside InnoDB
at server bootstrap or server start if they are not found or are
not of the right form.
@return	DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
dict_create_or_check_sys_tablespace(void);
/*=====================================*/
/********************************************************************//**
Add a single tablespace definition to the data dictionary tables in the
database.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
dict_create_add_tablespace_to_dictionary(
/*=====================================*/
	ulint		space,		/*!< in: tablespace id */
	const char*	name,		/*!< in: tablespace name */
	ulint		flags,		/*!< in: tablespace flags */
	const char*	path,		/*!< in: tablespace path */
	trx_t*		trx,		/*!< in: transaction */
	bool		commit);	/*!< in: if true then commit the
					transaction */
/********************************************************************//**
Table create node structure */

/********************************************************************//**
Add a single foreign key definition to the data dictionary tables in the
database. We also generate names to constraints that were not named by the
user. A generated constraint has a name of the format
databasename/tablename_ibfk_NUMBER, where the numbers start from 1, and
are given locally for this table, that is, the number is not global, as in
the old format constraints < 4.0.18 it used to be.
@return error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
dict_create_add_foreign_to_dictionary(
/*==================================*/
	ulint*		id_nr,	/*!< in/out: number to use in id generation;
				incremented if used */
	dict_table_t*	table,	/*!< in: table */
	dict_foreign_t*	foreign,/*!< in: foreign */
	trx_t*		trx)	/*!< in/out: dictionary transaction */
	__attribute__((nonnull, warn_unused_result));

/* Table create node structure */
struct tab_node_t{
	que_common_t	common;	/*!< node type: QUE_NODE_TABLE_CREATE */
	dict_table_t*	table;	/*!< table to create, built as a memory data
				structure with dict_mem_... functions */
	ins_node_t*	tab_def; /* child node which does the insert of
				the table definition; the row to be inserted
				is built by the parent node  */
	ins_node_t*	col_def; /* child node which does the inserts of
				the column definitions; the row to be inserted
				is built by the parent node  */
	commit_node_t*	commit_node;
				/* child node which performs a commit after
				a successful table creation */
	/*----------------------*/
	/* Local storage for this graph node */
	ulint		state;	/*!< node execution state */
	ulint		col_no;	/*!< next column definition to insert */
	mem_heap_t*	heap;	/*!< memory heap used as auxiliary storage */
};

/* Table create node states */
#define	TABLE_BUILD_TABLE_DEF	1
#define	TABLE_BUILD_COL_DEF	2
#define	TABLE_COMMIT_WORK	3
#define	TABLE_ADD_TO_CACHE	4
#define	TABLE_COMPLETED		5

/* Index create node struct */

struct ind_node_t{
	que_common_t	common;	/*!< node type: QUE_NODE_INDEX_CREATE */
	dict_index_t*	index;	/*!< index to create, built as a memory data
				structure with dict_mem_... functions */
	ins_node_t*	ind_def; /* child node which does the insert of
				the index definition; the row to be inserted
				is built by the parent node  */
	ins_node_t*	field_def; /* child node which does the inserts of
				the field definitions; the row to be inserted
				is built by the parent node  */
	commit_node_t*	commit_node;
				/* child node which performs a commit after
				a successful index creation */
	/*----------------------*/
	/* Local storage for this graph node */
	ulint		state;	/*!< node execution state */
	ulint		page_no;/* root page number of the index */
	dict_table_t*	table;	/*!< table which owns the index */
	dtuple_t*	ind_row;/* index definition row built */
	ulint		field_no;/* next field definition to insert */
	mem_heap_t*	heap;	/*!< memory heap used as auxiliary storage */
};

/* Index create node states */
#define	INDEX_BUILD_INDEX_DEF	1
#define	INDEX_BUILD_FIELD_DEF	2
#define	INDEX_CREATE_INDEX_TREE	3
#define	INDEX_COMMIT_WORK	4
#define	INDEX_ADD_TO_CACHE	5

#ifndef UNIV_NONINL
#include "dict0crea.ic"
#endif

#endif
