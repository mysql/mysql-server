/*****************************************************************************

Copyright (c) 1997, 2016, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************//**
@file include/row0undo.h
Row undo

Created 1/8/1997 Heikki Tuuri
*******************************************************/

#ifndef row0undo_h
#define row0undo_h

#include "univ.i"
#include "mtr0mtr.h"
#include "trx0sys.h"
#include "btr0types.h"
#include "btr0pcur.h"
#include "dict0types.h"
#include "trx0types.h"
#include "que0types.h"
#include "row0types.h"

/********************************************************************//**
Creates a row undo node to a query graph.
@return own: undo node */
undo_node_t*
row_undo_node_create(
/*=================*/
	trx_t*		trx,	/*!< in: transaction */
	que_thr_t*	parent,	/*!< in: parent node, i.e., a thr node */
	mem_heap_t*	heap);	/*!< in: memory heap where created */
/***********************************************************//**
Looks for the clustered index record when node has the row reference.
The pcur in node is used in the search. If found, stores the row to node,
and stores the position of pcur, and detaches it. The pcur must be closed
by the caller in any case.
@return true if found; NOTE the node->pcur must be closed by the
caller, regardless of the return value */
bool
row_undo_search_clust_to_pcur(
/*==========================*/
	undo_node_t*	node)	/*!< in/out: row undo node */
	MY_ATTRIBUTE((warn_unused_result));
/***********************************************************//**
Undoes a row operation in a table. This is a high-level function used
in SQL execution graphs.
@return query thread to run next or NULL */
que_thr_t*
row_undo_step(
/*==========*/
	que_thr_t*	thr);	/*!< in: query thread */

/* A single query thread will try to perform the undo for all successive
versions of a clustered index record, if the transaction has modified it
several times during the execution which is rolled back. It may happen
that the task is transferred to another query thread, if the other thread
is assigned to handle an undo log record in the chain of different versions
of the record, and the other thread happens to get the x-latch to the
clustered index record at the right time.
	If a query thread notices that the clustered index record it is looking
for is missing, or the roll ptr field in the record doed not point to the
undo log record the thread was assigned to handle, then it gives up the undo
task for that undo log record, and fetches the next. This situation can occur
just in the case where the transaction modified the same record several times
and another thread is currently doing the undo for successive versions of
that index record. */

/** Execution state of an undo node */
enum undo_exec {
	UNDO_NODE_FETCH_NEXT = 1,	/*!< we should fetch the next
					undo log record */
	UNDO_NODE_INSERT,		/*!< undo a fresh insert of a
					row to a table */
	UNDO_NODE_MODIFY		/*!< undo a modify operation
					(DELETE or UPDATE) on a row
					of a table */
};

/** Undo node structure */
struct undo_node_t{
	que_common_t	common;	/*!< node type: QUE_NODE_UNDO */
	enum undo_exec	state;	/*!< node execution state */
	trx_t*		trx;	/*!< trx for which undo is done */
	roll_ptr_t	roll_ptr;/*!< roll pointer to undo log record */
	trx_undo_rec_t*	undo_rec;/*!< undo log record */
	undo_no_t	undo_no;/*!< undo number of the record */
	ulint		rec_type;/*!< undo log record type: TRX_UNDO_INSERT_REC,
				... */
	trx_id_t	new_trx_id; /*!< trx id to restore to clustered index
				record */
	btr_pcur_t	pcur;	/*!< persistent cursor used in searching the
				clustered index record */
	dict_table_t*	table;	/*!< table where undo is done */
	ulint		cmpl_info;/*!< compiler analysis of an update */
	upd_t*		update;	/*!< update vector for a clustered index
				record */
	dtuple_t*	ref;	/*!< row reference to the next row to handle */
	dtuple_t*	row;	/*!< a copy (also fields copied to heap) of the
				row to handle */
	row_ext_t*	ext;	/*!< NULL, or prefixes of the externally
				stored columns of the row */
	dtuple_t*	undo_row;/*!< NULL, or the row after undo */
	row_ext_t*	undo_ext;/*!< NULL, or prefixes of the externally
				stored columns of undo_row */
	dict_index_t*	index;	/*!< the next index whose record should be
				handled */
	mem_heap_t*	heap;	/*!< memory heap used as auxiliary storage for
				row; this must be emptied after undo is tried
				on a row */
};

#include "row0undo.ic"

#endif
