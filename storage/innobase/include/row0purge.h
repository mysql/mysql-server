/*****************************************************************************

Copyright (c) 1997, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/row0purge.h
Purge obsolete records

Created 3/14/1997 Heikki Tuuri
*******************************************************/

#ifndef row0purge_h
#define row0purge_h

#include "univ.i"
#include "data0data.h"
#include "btr0types.h"
#include "btr0pcur.h"
#include "dict0types.h"
#include "trx0types.h"
#include "que0types.h"
#include "row0types.h"
#include "row0purge.h"
#include "ut0vec.h"

/********************************************************************//**
Creates a purge node to a query graph.
@return	own: purge node */
UNIV_INTERN
purge_node_t*
row_purge_node_create(
/*==================*/
	que_thr_t*	parent,		/*!< in: parent node, i.e., a
					thr node */
	mem_heap_t*	heap)		/*!< in: memory heap where created */
	__attribute__((nonnull, warn_unused_result));
/***********************************************************//**
Determines if it is possible to remove a secondary index entry.
Removal is possible if the secondary index entry does not refer to any
not delete marked version of a clustered index record where DB_TRX_ID
is newer than the purge view.

NOTE: This function should only be called by the purge thread, only
while holding a latch on the leaf page of the secondary index entry
(or keeping the buffer pool watch on the page).  It is possible that
this function first returns true and then false, if a user transaction
inserts a record that the secondary index entry would refer to.
However, in that case, the user transaction would also re-insert the
secondary index entry after purge has removed it and released the leaf
page latch.
@return	true if the secondary index record can be purged */
UNIV_INTERN
bool
row_purge_poss_sec(
/*===============*/
	purge_node_t*	node,	/*!< in/out: row purge node */
	dict_index_t*	index,	/*!< in: secondary index */
	const dtuple_t*	entry)	/*!< in: secondary index entry */
	__attribute__((nonnull, warn_unused_result));
/***************************************************************
Does the purge operation for a single undo log record. This is a high-level
function used in an SQL execution graph.
@return	query thread to run next or NULL */
UNIV_INTERN
que_thr_t*
row_purge_step(
/*===========*/
	que_thr_t*	thr)	/*!< in: query thread */
	__attribute__((nonnull, warn_unused_result));

/* Purge node structure */

struct purge_node_t{
	que_common_t	common;	/*!< node type: QUE_NODE_PURGE */
	/*----------------------*/
	/* Local storage for this graph node */
	roll_ptr_t	roll_ptr;/* roll pointer to undo log record */
	ib_vector_t*    undo_recs;/*!< Undo recs to purge */

	undo_no_t	undo_no;/* undo number of the record */

	ulint		rec_type;/* undo log record type: TRX_UNDO_INSERT_REC,
				... */
	dict_table_t*	table;	/*!< table where purge is done */

	ulint		cmpl_info;/* compiler analysis info of an update */

	upd_t*		update;	/*!< update vector for a clustered index
				record */
	dtuple_t*	ref;	/*!< NULL, or row reference to the next row to
				handle */
	dtuple_t*	row;	/*!< NULL, or a copy (also fields copied to
				heap) of the indexed fields of the row to
				handle */
	dict_index_t*	index;	/*!< NULL, or the next index whose record should
				be handled */
	mem_heap_t*	heap;	/*!< memory heap used as auxiliary storage for
				row; this must be emptied after a successful
				purge of a row */
	ibool		found_clust;/* TRUE if the clustered index record
				determined by ref was found in the clustered
				index, and we were able to position pcur on
				it */
	btr_pcur_t	pcur;	/*!< persistent cursor used in searching the
				clustered index record */
	ibool		done;	/* Debug flag */

};

#ifndef UNIV_NONINL
#include "row0purge.ic"
#endif

#endif
