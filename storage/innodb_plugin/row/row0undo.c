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

/**************************************************//**
@file row/row0undo.c
Row undo

Created 1/8/1997 Heikki Tuuri
*******************************************************/

#include "row0undo.h"

#ifdef UNIV_NONINL
#include "row0undo.ic"
#endif

#include "fsp0fsp.h"
#include "mach0data.h"
#include "trx0rseg.h"
#include "trx0trx.h"
#include "trx0roll.h"
#include "trx0undo.h"
#include "trx0purge.h"
#include "trx0rec.h"
#include "que0que.h"
#include "row0row.h"
#include "row0uins.h"
#include "row0umod.h"
#include "row0upd.h"
#include "row0mysql.h"
#include "srv0srv.h"

/* How to undo row operations?
(1) For an insert, we have stored a prefix of the clustered index record
in the undo log. Using it, we look for the clustered record, and using
that we look for the records in the secondary indexes. The insert operation
may have been left incomplete, if the database crashed, for example.
We may have look at the trx id and roll ptr to make sure the record in the
clustered index is really the one for which the undo log record was
written. We can use the framework we get from the original insert op.
(2) Delete marking: We can use the framework we get from the original
delete mark op. We only have to check the trx id.
(3) Update: This may be the most complicated. We have to use the framework
we get from the original update op.

What if the same trx repeatedly deletes and inserts an identical row.
Then the row id changes and also roll ptr. What if the row id was not
part of the ordering fields in the clustered index? Maybe we have to write
it to undo log. Well, maybe not, because if we order the row id and trx id
in descending order, then the only undeleted copy is the first in the
index. Our searches in row operations always position the cursor before
the first record in the result set. But, if there is no key defined for
a table, then it would be desirable that row id is in ascending order.
So, lets store row id in descending order only if it is not an ordering
field in the clustered index.

NOTE: Deletes and inserts may lead to situation where there are identical
records in a secondary index. Is that a problem in the B-tree? Yes.
Also updates can lead to this, unless trx id and roll ptr are included in
ord fields.
(1) Fix in clustered indexes: include row id, trx id, and roll ptr
in node pointers of B-tree.
(2) Fix in secondary indexes: include all fields in node pointers, and
if an entry is inserted, check if it is equal to the right neighbor,
in which case update the right neighbor: the neighbor must be delete
marked, set it unmarked and write the trx id of the current transaction.

What if the same trx repeatedly updates the same row, updating a secondary
index field or not? Updating a clustered index ordering field?

(1) If it does not update the secondary index and not the clustered index
ord field. Then the secondary index record stays unchanged, but the
trx id in the secondary index record may be smaller than in the clustered
index record. This is no problem?
(2) If it updates secondary index ord field but not clustered: then in
secondary index there are delete marked records, which differ in an
ord field. No problem.
(3) Updates clustered ord field but not secondary, and secondary index
is unique. Then the record in secondary index is just updated at the
clustered ord field.
(4)

Problem with duplicate records:
Fix 1: Add a trx op no field to all indexes. A problem: if a trx with a
bigger trx id has inserted and delete marked a similar row, our trx inserts
again a similar row, and a trx with an even bigger id delete marks it. Then
the position of the row should change in the index if the trx id affects
the alphabetical ordering.

Fix 2: If an insert encounters a similar row marked deleted, we turn the
insert into an 'update' of the row marked deleted. Then we must write undo
info on the update. A problem: what if a purge operation tries to remove
the delete marked row?

We can think of the database row versions as a linked list which starts
from the record in the clustered index, and is linked by roll ptrs
through undo logs. The secondary index records are references which tell
what kinds of records can be found in this linked list for a record
in the clustered index.

How to do the purge? A record can be removed from the clustered index
if its linked list becomes empty, i.e., the row has been marked deleted
and its roll ptr points to the record in the undo log we are going through,
doing the purge. Similarly, during a rollback, a record can be removed
if the stored roll ptr in the undo log points to a trx already (being) purged,
or if the roll ptr is NULL, i.e., it was a fresh insert. */

/********************************************************************//**
Creates a row undo node to a query graph.
@return	own: undo node */
UNIV_INTERN
undo_node_t*
row_undo_node_create(
/*=================*/
	trx_t*		trx,	/*!< in: transaction */
	que_thr_t*	parent,	/*!< in: parent node, i.e., a thr node */
	mem_heap_t*	heap)	/*!< in: memory heap where created */
{
	undo_node_t*	undo;

	ut_ad(trx && parent && heap);

	undo = mem_heap_alloc(heap, sizeof(undo_node_t));

	undo->common.type = QUE_NODE_UNDO;
	undo->common.parent = parent;

	undo->state = UNDO_NODE_FETCH_NEXT;
	undo->trx = trx;

	btr_pcur_init(&(undo->pcur));

	undo->heap = mem_heap_create(256);

	return(undo);
}

/***********************************************************//**
Looks for the clustered index record when node has the row reference.
The pcur in node is used in the search. If found, stores the row to node,
and stores the position of pcur, and detaches it. The pcur must be closed
by the caller in any case.
@return TRUE if found; NOTE the node->pcur must be closed by the
caller, regardless of the return value */
UNIV_INTERN
ibool
row_undo_search_clust_to_pcur(
/*==========================*/
	undo_node_t*	node)	/*!< in: row undo node */
{
	dict_index_t*	clust_index;
	ibool		found;
	mtr_t		mtr;
	ibool		ret;
	rec_t*		rec;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	rec_offs_init(offsets_);

	mtr_start(&mtr);

	clust_index = dict_table_get_first_index(node->table);

	found = row_search_on_row_ref(&(node->pcur), BTR_MODIFY_LEAF,
				      node->table, node->ref, &mtr);

	rec = btr_pcur_get_rec(&(node->pcur));

	offsets = rec_get_offsets(rec, clust_index, offsets,
				  ULINT_UNDEFINED, &heap);

	if (!found || 0 != ut_dulint_cmp(node->roll_ptr,
					 row_get_rec_roll_ptr(rec, clust_index,
							      offsets))) {

		/* We must remove the reservation on the undo log record
		BEFORE releasing the latch on the clustered index page: this
		is to make sure that some thread will eventually undo the
		modification corresponding to node->roll_ptr. */

		/* fputs("--------------------undoing a previous version\n",
		stderr); */

		ret = FALSE;
	} else {
		row_ext_t**	ext;

		if (dict_table_get_format(node->table) >= DICT_TF_FORMAT_ZIP) {
			/* In DYNAMIC or COMPRESSED format, there is
			no prefix of externally stored columns in the
			clustered index record. Build a cache of
			column prefixes. */
			ext = &node->ext;
		} else {
			/* REDUNDANT and COMPACT formats store a local
			768-byte prefix of each externally stored
			column. No cache is needed. */
			ext = NULL;
			node->ext = NULL;
		}

		node->row = row_build(ROW_COPY_DATA, clust_index, rec,
				      offsets, NULL, ext, node->heap);
		if (node->update) {
			node->undo_row = dtuple_copy(node->row, node->heap);
			row_upd_replace(node->undo_row, &node->undo_ext,
					clust_index, node->update, node->heap);
		} else {
			node->undo_row = NULL;
			node->undo_ext = NULL;
		}

		btr_pcur_store_position(&(node->pcur), &mtr);

		ret = TRUE;
	}

	btr_pcur_commit_specify_mtr(&(node->pcur), &mtr);

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
	return(ret);
}

/***********************************************************//**
Fetches an undo log record and does the undo for the recorded operation.
If none left, or a partial rollback completed, returns control to the
parent node, which is always a query thread node.
@return	DB_SUCCESS if operation successfully completed, else error code */
static
ulint
row_undo(
/*=====*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	ulint		err;
	trx_t*		trx;
	roll_ptr_t	roll_ptr;
	ibool		locked_data_dict;

	ut_ad(node && thr);

	trx = node->trx;

	if (node->state == UNDO_NODE_FETCH_NEXT) {

		node->undo_rec = trx_roll_pop_top_rec_of_trx(trx,
							     trx->roll_limit,
							     &roll_ptr,
							     node->heap);
		if (!node->undo_rec) {
			/* Rollback completed for this query thread */

			thr->run_node = que_node_get_parent(node);

			return(DB_SUCCESS);
		}

		node->roll_ptr = roll_ptr;
		node->undo_no = trx_undo_rec_get_undo_no(node->undo_rec);

		if (trx_undo_roll_ptr_is_insert(roll_ptr)) {

			node->state = UNDO_NODE_INSERT;
		} else {
			node->state = UNDO_NODE_MODIFY;
		}
	}

	/* Prevent DROP TABLE etc. while we are rolling back this row.
	If we are doing a TABLE CREATE or some other dictionary operation,
	then we already have dict_operation_lock locked in x-mode. Do not
	try to lock again, because that would cause a hang. */

	locked_data_dict = (trx->dict_operation_lock_mode == 0);

	if (locked_data_dict) {

		row_mysql_freeze_data_dictionary(trx);
	}

	if (node->state == UNDO_NODE_INSERT) {

		err = row_undo_ins(node);

		node->state = UNDO_NODE_FETCH_NEXT;
	} else {
		ut_ad(node->state == UNDO_NODE_MODIFY);
		err = row_undo_mod(node, thr);
	}

	if (locked_data_dict) {

		row_mysql_unfreeze_data_dictionary(trx);
	}

	/* Do some cleanup */
	btr_pcur_close(&(node->pcur));

	mem_heap_empty(node->heap);

	thr->run_node = node;

	return(err);
}

/***********************************************************//**
Undoes a row operation in a table. This is a high-level function used
in SQL execution graphs.
@return	query thread to run next or NULL */
UNIV_INTERN
que_thr_t*
row_undo_step(
/*==========*/
	que_thr_t*	thr)	/*!< in: query thread */
{
	ulint		err;
	undo_node_t*	node;
	trx_t*		trx;

	ut_ad(thr);

	srv_activity_count++;

	trx = thr_get_trx(thr);

	node = thr->run_node;

	ut_ad(que_node_get_type(node) == QUE_NODE_UNDO);

	err = row_undo(node, thr);

	trx->error_state = err;

	if (err != DB_SUCCESS) {
		/* SQL error detected */

		fprintf(stderr, "InnoDB: Fatal error %lu in rollback.\n",
			(ulong) err);

		if (err == DB_OUT_OF_FILE_SPACE) {
			fprintf(stderr,
				"InnoDB: Error 13 means out of tablespace.\n"
				"InnoDB: Consider increasing"
				" your tablespace.\n");

			exit(1);
		}

		ut_error;

		return(NULL);
	}

	return(thr);
}
