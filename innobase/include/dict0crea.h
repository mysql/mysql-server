/******************************************************
Database object creation

(c) 1996 Innobase Oy

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
					
/*************************************************************************
Creates the default clustered index for a table: the records are ordered
by row id. */

void
dict_create_default_index(
/*======================*/
	dict_table_t*	table,	/* in: table */
	trx_t*		trx);	/* in: transaction handle */
/*************************************************************************
Creates a table create graph. */

tab_node_t*
tab_create_graph_create(
/*====================*/
				/* out, own: table create node */
	dict_table_t*	table,	/* in: table to create, built as a memory data
				structure */
	mem_heap_t*	heap);	/* in: heap where created */
/*************************************************************************
Creates an index create graph. */

ind_node_t*
ind_create_graph_create(
/*====================*/
				/* out, own: index create node */
	dict_index_t*	index,	/* in: index to create, built as a memory data
				structure */
	mem_heap_t*	heap);	/* in: heap where created */
/***************************************************************
Creates a table. This is a high-level function used in SQL execution graphs. */

que_thr_t*
dict_create_table_step(
/*===================*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr);	/* in: query thread */
/***************************************************************
Creates an index. This is a high-level function used in SQL execution
graphs. */

que_thr_t*
dict_create_index_step(
/*===================*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr);	/* in: query thread */
/***********************************************************************
Drops the index tree associated with a row in SYS_INDEXES table. */

void
dict_drop_index_tree(
/*=================*/
	rec_t*	rec,	/* in: record in the clustered index of SYS_INDEXES
			table */
	mtr_t*	mtr);	/* in: mtr having the latch on the record page */


/* Table create node structure */

struct tab_node_struct{
	que_common_t	common;	/* node type: QUE_NODE_TABLE_CREATE */
	dict_table_t*	table;	/* table to create, built as a memory data
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
	ulint		state;	/* node execution state */
	ulint		col_no;	/* next column definition to insert */
	mem_heap_t*	heap;	/* memory heap used as auxiliary storage */
};

/* Table create node states */
#define	TABLE_BUILD_TABLE_DEF	1
#define	TABLE_BUILD_COL_DEF	2
#define	TABLE_COMMIT_WORK	3
#define	TABLE_ADD_TO_CACHE	4
#define	TABLE_COMPLETED		5

/* Index create node struct */

struct ind_node_struct{
	que_common_t	common;	/* node type: QUE_NODE_INDEX_CREATE */
	dict_index_t*	index;	/* index to create, built as a memory data
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
	ulint		state;	/* node execution state */
	dict_table_t*	table;	/* table which owns the index */
	dtuple_t*	ind_row;/* index definition row built */
	ulint		field_no;/* next field definition to insert */
	mem_heap_t*	heap;	/* memory heap used as auxiliary storage */
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
