/******************************************************
Row undo

(c) 1997 Innobase Oy

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

/************************************************************************
Creates a row undo node to a query graph. */

undo_node_t*
row_undo_node_create(
/*=================*/
				/* out, own: undo node */
	trx_t*		trx,	/* in: transaction */
	que_thr_t*	parent,	/* in: parent node, i.e., a thr node */
	mem_heap_t*	heap);	/* in: memory heap where created */
/***************************************************************
Looks for the clustered index record when node has the row reference.
The pcur in node is used in the search. If found, stores the row to node,
and stores the position of pcur, and detaches it. The pcur must be closed
by the caller in any case. */

ibool
row_undo_search_clust_to_pcur(
/*==========================*/
				/* out: TRUE if found; NOTE the node->pcur
				must be closed by the caller, regardless of
				the return value */
	undo_node_t*	node);	/* in: row undo node */
/***************************************************************
Undoes a row operation in a table. This is a high-level function used
in SQL execution graphs. */

que_thr_t*
row_undo_step(
/*==========*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr);	/* in: query thread */

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

/* Undo node structure */

struct undo_node_struct{
	que_common_t	common;	/* node type: QUE_NODE_UNDO */
	ulint		state;	/* node execution state */
	trx_t*		trx;	/* trx for which undo is done */
	dulint		roll_ptr;/* roll pointer to undo log record */
	trx_undo_rec_t*	undo_rec;/* undo log record */
	dulint		undo_no;/* undo number of the record */
	ulint		rec_type;/* undo log record type: TRX_UNDO_INSERT_REC,
				... */
	dulint		new_roll_ptr; /* roll ptr to restore to clustered index
				record */
	dulint		new_trx_id; /* trx id to restore to clustered index
				record */
	btr_pcur_t	pcur;	/* persistent cursor used in searching the
				clustered index record */
	dict_table_t*	table;	/* table where undo is done; NOTE that the
				table has to be released explicitly with
				dict_table_release */
	ulint		cmpl_info;/* compiler analysis of an update */
	upd_t*		update;	/* update vector for a clustered index record */
	dtuple_t*	ref;	/* row reference to the next row to handle */
	dtuple_t*	row;	/* a copy (also fields copied to heap) of the
				row to handle */
	dict_index_t*	index;	/* the next index whose record should be
				handled */
	mem_heap_t*	heap;	/* memory heap used as auxiliary storage for
				row; this must be emptied after undo is tried
				on a row */
};

/* Execution states for an undo node */
#define	UNDO_NODE_FETCH_NEXT	1	/* we should fetch the next undo log
					record */
#define	UNDO_NODE_PREV_VERS	2	/* the roll ptr to previous version of
					a row is stored in node, and undo
					should be done based on it */
#define UNDO_NODE_INSERT	3
#define UNDO_NODE_MODIFY	4


#ifndef UNIV_NONINL
#include "row0undo.ic"
#endif

#endif 
