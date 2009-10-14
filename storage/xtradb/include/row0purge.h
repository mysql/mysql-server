/*****************************************************************************

Copyright (c) 1997, 2009, Innobase Oy. All Rights Reserved.

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

/******************************************************
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

/************************************************************************
Creates a purge node to a query graph. */
UNIV_INTERN
purge_node_t*
row_purge_node_create(
/*==================*/
				/* out, own: purge node */
	que_thr_t*	parent,	/* in: parent node, i.e., a thr node */
	mem_heap_t*	heap);	/* in: memory heap where created */
/***************************************************************
Does the purge operation for a single undo log record. This is a high-level
function used in an SQL execution graph. */
UNIV_INTERN
que_thr_t*
row_purge_step(
/*===========*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr);	/* in: query thread */

/* Purge node structure */

struct purge_node_struct{
	que_common_t	common;	/* node type: QUE_NODE_PURGE */
	/*----------------------*/
	/* Local storage for this graph node */
	dulint		roll_ptr;/* roll pointer to undo log record */
	trx_undo_rec_t*	undo_rec;/* undo log record */
	trx_undo_inf_t*	reservation;/* reservation for the undo log record in
				the purge array */
	dulint		undo_no;/* undo number of the record */
	ulint		rec_type;/* undo log record type: TRX_UNDO_INSERT_REC,
				... */
	btr_pcur_t	pcur;	/* persistent cursor used in searching the
				clustered index record */
	ibool		found_clust;/* TRUE if the clustered index record
				determined by ref was found in the clustered
				index, and we were able to position pcur on
				it */
	dict_table_t*	table;	/* table where purge is done */
	ulint		cmpl_info;/* compiler analysis info of an update */
	upd_t*		update;	/* update vector for a clustered index
				record */
	dtuple_t*	ref;	/* NULL, or row reference to the next row to
				handle */
	dtuple_t*	row;	/* NULL, or a copy (also fields copied to
				heap) of the indexed fields of the row to
				handle */
	dict_index_t*	index;	/* NULL, or the next index whose record should
				be handled */
	mem_heap_t*	heap;	/* memory heap used as auxiliary storage for
				row; this must be emptied after a successful
				purge of a row */
};

#ifndef UNIV_NONINL
#include "row0purge.ic"
#endif

#endif
