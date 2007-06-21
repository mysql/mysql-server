/******************************************************
Row undo

(c) 1997 Innobase Oy

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
#include "row0mysql.h"
#include "srv0srv.h"
#include "row0merge.h"

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

/************************************************************************
Creates a row undo node to a query graph. */

undo_node_t*
row_undo_node_create(
/*=================*/
				/* out, own: undo node */
	trx_t*		trx,	/* in: transaction */
	que_thr_t*	parent,	/* in: parent node, i.e., a thr node */
	mem_heap_t*	heap)	/* in: memory heap where created */
{
	undo_node_t*	undo;

	ut_ad(trx && parent && heap);

	undo = mem_heap_alloc(heap, sizeof(undo_node_t));

	undo->common.type = QUE_NODE_UNDO;
	undo->common.parent = parent;

	undo->state = UNDO_NODE_FETCH_NEXT;
	undo->trx = trx;
	undo->rec_sub_type = TRX_UNDO_NULL_REC;

	btr_pcur_init(&(undo->pcur));

	undo->heap = mem_heap_create(256);

	return(undo);
}

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
	undo_node_t*	node)	/* in: row undo node */
{
	dict_index_t*	clust_index;
	ibool		found;
	mtr_t		mtr;
	ibool		ret;
	rec_t*		rec;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	*offsets_ = (sizeof offsets_) / sizeof *offsets_;

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
		node->row = row_build(ROW_COPY_DATA, clust_index, rec,
				      offsets, &node->ext, node->heap);
		btr_pcur_store_position(&(node->pcur), &mtr);

		ret = TRUE;
	}

	btr_pcur_commit_specify_mtr(&(node->pcur), &mtr);

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
	return(ret);
}

/***************************************************************
Fetches an undo log record and does the undo for the recorded operation.
If none left, or a partial rollback completed, returns control to the
parent node, which is always a query thread node. */
static
ulint
row_undo(
/*=====*/
				/* out: DB_SUCCESS if operation successfully
				completed, else error code */
	undo_node_t*	node,	/* in: row undo node */
	que_thr_t*	thr)	/* in: query thread */
{
	ulint	err;
	trx_t*	trx;
	dulint	roll_ptr;
	ibool	locked_data_dict;

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

	} else if (node->state == UNDO_NODE_PREV_VERS) {

		/* Undo should be done to the same clustered index record
		again in this same rollback, restoring the previous version */

		roll_ptr = node->new_roll_ptr;

		node->undo_rec = trx_undo_get_undo_rec_low(roll_ptr,
							   node->heap);
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

		row_mysql_lock_data_dictionary(trx);
	}

	if (node->state == UNDO_NODE_INSERT) {

		err = row_undo_ins(node);

		node->state = UNDO_NODE_FETCH_NEXT;
	} else {
		ut_ad(node->state == UNDO_NODE_MODIFY);
		err = row_undo_mod(node, thr);
	}

	if (locked_data_dict) {

		row_mysql_unlock_data_dictionary(trx);
	}

	/* Do some cleanup */
	btr_pcur_close(&(node->pcur));

	mem_heap_empty(node->heap);

	thr->run_node = node;

	return(err);
}

/***************************************************************
Undoes a row operation in a table. This is a high-level function used
in SQL execution graphs. */

que_thr_t*
row_undo_step(
/*==========*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr)	/* in: query thread */
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

/***************************************************************
Parses the info in a fresh insert undo record containing a
dictionary change. */
static
ulint
row_undo_dictionary_parse_undo_rec(
/*===============================*/
				/* out: DB_SUCCESS or DB_ERROR */
	undo_node_t*	node)	/* in: row undo node */
{
	byte*		ptr;
	dulint		table_id;
	dulint		index_id;
	ulint		len;

	ut_ad(node);

	node->rec_type = trx_undo_rec_get_type(node->undo_rec);
	node->undo_no = trx_undo_rec_get_undo_no(node->undo_rec);

	ptr = trx_undo_rec_get_ptr(node->undo_rec, node->undo_no);

	ut_a(node->rec_type == TRX_UNDO_DICTIONARY_REC);

	/* Read dictionary rec sub type */
	node->rec_sub_type = mach_read_from_1(ptr);
	ptr++;

	/* Parse subtype parameters */

	switch (node->rec_sub_type) {

	case TRX_UNDO_INDEX_CREATE_REC:

		table_id = mach_dulint_read_much_compressed(ptr);
		len = mach_dulint_get_much_compressed_size(table_id);
		ptr += len;

		index_id = mach_dulint_read_much_compressed(ptr);
		len = mach_dulint_get_much_compressed_size(index_id);
		ptr += len;

		node->table = dict_table_get_on_id(table_id, node->trx);
		node->index = NULL;

		if (!node->table) {
			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: [Error]: Table %lu %lu not found "
				"in index create undo rec\n",
				(ulong) ut_dulint_get_high(table_id),
				(ulong) ut_dulint_get_low(table_id));
			goto err_exit;
		} else if (ut_dulint_is_zero(index_id)) {
			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: [Error]: Index id missing from "
				"index create undo rec\n");
err_exit:
			mutex_enter(&kernel_mutex);
			trx_print(stderr, node->trx, 1024);
			mutex_exit(&kernel_mutex);

			return(DB_ERROR);
		} else {
			node->index = dict_index_get_on_id_low(
				node->table, index_id);
		}

		if (node->table->ibd_file_missing || !node->index) {
			/* We skip undo operations to missing .ibd files
			and missing indexes */
			node->table = NULL;
			node->index = NULL;

			return(DB_SUCCESS);
		}

		break;

	case TRX_UNDO_TABLE_CREATE_REC:
	case TRX_UNDO_TABLE_DROP_REC:
		len = strlen((char *)ptr) + 1;

		node->new_table_name = mem_heap_strdup(node->heap, (char *)ptr);
		ptr += len;

		ut_ad(*node->new_table_name == TEMP_TABLE_PREFIX);
		break;

	case TRX_UNDO_TABLE_RENAME_REC:
		len = strlen((char *)ptr) + 1;

		node->new_table_name = mem_heap_strdup(node->heap, (char *)ptr);
		ptr += len;

		ut_ad(*node->new_table_name == TEMP_TABLE_PREFIX);

		len = strlen((char *)ptr) + 1;

		node->old_table_name = mem_heap_strdup(node->heap, (char *)ptr);
		ptr += len;

		len = strlen((char *)ptr) + 1;

		node->tmp_table_name = mem_heap_strdup(node->heap, (char *)ptr);
		ptr += len;

		ut_ad(*node->tmp_table_name == TEMP_TABLE_PREFIX);
		break;

	default:
		ut_print_timestamp(stderr);

		fprintf(stderr,
			"  InnoDB: [Error]: Undefined rec_sub_type = %lu at "
			"row_undo_dictionary_parse_undo_rec\n", 
			(ulong)node->rec_sub_type);

		mutex_enter(&kernel_mutex);
		trx_print(stderr, node->trx, 1024);
		mutex_exit(&kernel_mutex);

		return(DB_ERROR);
	}

	return(DB_SUCCESS);
}

/***************************************************************
Currently we gather all the information that is required to do the
UNDO.  The actual UNDO is done later in row_undo_dictionary(). */

ulint
row_undo_build_dict_undo_list(
/*==========================*/
				/* out: DB_SUCCESS or error code */
	undo_node_t*	node)	/* in: row undo node */
{
	trx_t*		trx;
	dict_undo_t*	dict_undo;
	ulint		err = DB_SUCCESS;
	ibool		locked_dictionary = FALSE;
	ibool		thawed_dictionary = FALSE;

	ut_ad(node);
	ut_ad(node->state == UNDO_NODE_INSERT);
	ut_a(node->trx->dict_operation);

	err = row_undo_dictionary_parse_undo_rec(node);

	if (err != DB_SUCCESS) {

		goto func_exit;
	}

	trx = node->trx;

	if (trx->dict_operation_lock_mode == RW_S_LATCH) {
		row_mysql_unfreeze_data_dictionary(trx);

		thawed_dictionary = TRUE;
	}

	if (trx->dict_operation_lock_mode == 0
	    || trx->dict_operation_lock_mode != RW_X_LATCH) {

		row_mysql_lock_data_dictionary(trx);

		locked_dictionary = TRUE;
	}

	/* We will do our own deletes */
	trx->table_id = ut_dulint_zero;

	if (trx->dict_undo_list == NULL) {
		dict_undo_create_list(trx);
	}

	/* Create an element and append to the list */
	dict_undo = dict_undo_create_element(trx);

	dict_undo->op_type = node->rec_sub_type;

	switch (node->rec_sub_type) {

	case TRX_UNDO_INDEX_CREATE_REC:

		if (node->table && node->index) {
			ut_a(node->index->table == node->table);

			dict_undo->data.index = node->index;
		} else {
			dict_undo->data.index = NULL;
		}

		break;

	case TRX_UNDO_TABLE_DROP_REC:
	case TRX_UNDO_TABLE_CREATE_REC:

		dict_undo->data.table.old_table = dict_table_get_low(
			node->new_table_name);

		break;

	case TRX_UNDO_TABLE_RENAME_REC:

		dict_undo->data.table.old_table = dict_table_get_low(
			node->old_table_name);

		dict_undo->data.table.tmp_table = dict_table_get_low(
			node->tmp_table_name);

		dict_undo->data.table.new_table = dict_table_get_low(
			node->new_table_name);

		if (dict_undo->data.table.tmp_table
		      && dict_undo->data.table.old_table
		      && dict_undo->data.table.new_table) {

			/* This can't happen */
			ut_error;
		}

		break;

	default:

		ut_error;

		break;
	}

	if (locked_dictionary) {
		row_mysql_unlock_data_dictionary(trx);
	}

	if (thawed_dictionary) {
		row_mysql_freeze_data_dictionary(trx);
	}

func_exit:
	trx_undo_rec_release(node->trx, node->undo_no);

	return(err);
}

/***************************************************************
Undo or redo a dictionary change. */

void
row_undo_dictionary(
/*================*/
	trx_t*		trx,		/* in: transaction */
	dict_undo_t*	dict_undo)	/* in: dict undo info */
{
	ulint		err;

	switch (dict_undo->op_type) {
	case TRX_UNDO_INDEX_CREATE_REC:
		row_merge_drop_index(dict_undo->data.index,
				     dict_undo->data.index->table, trx);
		break;

	/* TODO: We are REDOing the DROP ? */
	case TRX_UNDO_TABLE_DROP_REC:
	case TRX_UNDO_TABLE_CREATE_REC:

		if (dict_undo->data.table.old_table) {

			err = row_drop_table_for_mysql_no_commit(
				dict_undo->data.table.old_table->name,
				trx, FALSE);
			ut_a(err == DB_SUCCESS);
		}

		break;

	case TRX_UNDO_TABLE_RENAME_REC:
		if (!dict_undo->data.table.new_table) {

			/* Old name to tmp name succeeded and new name to old
			name succeeded too. We have to be very careful here as
			the user could loose the entire table if not done
			carefully.*/
			ut_ad(dict_undo->data.table.old_table);

			err = row_rename_table_for_mysql(
				dict_undo->data.table.old_table->name,
				dict_undo->data.table.new_table->name,
				trx, FALSE);

			ut_a(err == DB_SUCCESS);

			err = row_rename_table_for_mysql(
				dict_undo->data.table.tmp_table->name,
				dict_undo->data.table.old_table->name,
				trx, FALSE);

			ut_a(err == DB_SUCCESS);

			err = row_drop_table_for_mysql_no_commit(
				dict_undo->data.table.new_table->name,
				trx, FALSE);

			ut_a(err == DB_SUCCESS);

		} else if (dict_undo->data.table.old_table) {
			/* Rename to tmp failed.*/

			ut_a(!dict_undo->data.table.tmp_table);

			if (dict_undo->data.table.new_table) {

				err = row_drop_table_for_mysql_no_commit(
					dict_undo->data.table.new_table->name,
					trx, FALSE);

				ut_a(err == DB_SUCCESS);
			}

		} else if (dict_undo->data.table.tmp_table) {
			/* Rename to tmp was OK. We need to UNDO it.*/

			ut_ad(!dict_undo->data.table.old_table);

			err = row_rename_table_for_mysql(
				dict_undo->data.table.tmp_table->name,
				dict_undo->data.table.old_table->name,
				trx, FALSE);

			ut_a(err == DB_SUCCESS);

			if (dict_undo->data.table.new_table) {

				err = row_drop_table_for_mysql_no_commit(
					dict_undo->data.table.new_table->name,
					trx, FALSE);

				ut_a(err == DB_SUCCESS);
			}

		} else {
			/* Shouldn't happen */
			ut_error;
		}

	default:
		ut_error;
	}
}

