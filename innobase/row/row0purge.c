/******************************************************
Purge obsolete records

(c) 1997 Innobase Oy

Created 3/14/1997 Heikki Tuuri
*******************************************************/

#include "row0purge.h"

#ifdef UNIV_NONINL
#include "row0purge.ic"
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
#include "row0upd.h"
#include "row0vers.h"
#include "log0log.h"

/************************************************************************
Creates a purge node to a query graph. */

purge_node_t*
row_purge_node_create(
/*==================*/
				/* out, own: purge node */
	que_thr_t*	parent,	/* in: parent node, i.e., a thr node */
	mem_heap_t*	heap)	/* in: memory heap where created */
{
	purge_node_t*	node;

	ut_ad(parent && heap);

	node = mem_heap_alloc(heap, sizeof(purge_node_t));

	node->common.type = QUE_NODE_PURGE;
	node->common.parent = parent;

	node->heap = mem_heap_create(256);

	return(node);
}

/***************************************************************
Repositions the pcur in the purge node on the clustered index record,
if found. */
static
ibool
row_purge_reposition_pcur(
/*======================*/
				/* out: TRUE if the record was found */
	ulint		mode,	/* in: latching mode */
	purge_node_t*	node,	/* in: row purge node */
	mtr_t*		mtr)	/* in: mtr */
{
	ibool	found;

	if (node->found_clust) {
		found = btr_pcur_restore_position(mode, &(node->pcur), mtr);

		return(found);
	}

	found = row_search_on_row_ref(&(node->pcur), mode, node->table,
							node->ref, mtr);
	node->found_clust = found;

	if (found) {
		btr_pcur_store_position(&(node->pcur), mtr);
	}

	return(found);
}

/***************************************************************
Removes a delete marked clustered index record if possible. */
static
ibool
row_purge_remove_clust_if_poss_low(
/*===============================*/
				/* out: TRUE if success, or if not found, or
				if modified after the delete marking */
	purge_node_t*	node,	/* in: row purge node */
	que_thr_t*	thr,	/* in: query thread */
	ulint		mode)	/* in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE */
{
	dict_index_t*	index;
	btr_pcur_t*	pcur;
	btr_cur_t*	btr_cur;
	ibool		success;
	ulint		err;
	mtr_t		mtr;

	UT_NOT_USED(thr);

	index = dict_table_get_first_index(node->table);
	
	pcur = &(node->pcur);
	btr_cur = btr_pcur_get_btr_cur(pcur);

	mtr_start(&mtr);

	success = row_purge_reposition_pcur(mode, node, &mtr);

	if (!success) {
		/* The record is already removed */

		btr_pcur_commit_specify_mtr(pcur, &mtr);

		return(TRUE);
	}

	if (0 != ut_dulint_cmp(node->roll_ptr,
		row_get_rec_roll_ptr(btr_pcur_get_rec(pcur), index))) {
		
		/* Someone else has modified the record later: do not remove */
		btr_pcur_commit_specify_mtr(pcur, &mtr);

		return(TRUE);
	}

	if (mode == BTR_MODIFY_LEAF) {
		success = btr_cur_optimistic_delete(btr_cur, &mtr);
	} else {
		ut_ad(mode == BTR_MODIFY_TREE);
		btr_cur_pessimistic_delete(&err, FALSE, btr_cur, &mtr);

		if (err == DB_SUCCESS) {
			success = TRUE;
		} else if (err == DB_OUT_OF_FILE_SPACE) {
			success = FALSE;
		} else {
			ut_a(0);
		}
	}

	btr_pcur_commit_specify_mtr(pcur, &mtr);

	return(success);
}
		
/***************************************************************
Removes a clustered index record if it has not been modified after the delete
marking. */
static
void
row_purge_remove_clust_if_poss(
/*===========================*/
	purge_node_t*	node,	/* in: row purge node */
	que_thr_t*	thr)	/* in: query thread */
{
	ibool	success;
	ulint	n_tries	= 0;
	
/*	printf("Purge: Removing clustered record\n"); */

	success = row_purge_remove_clust_if_poss_low(node, thr,
							BTR_MODIFY_LEAF);
	if (success) {

		return;
	}
retry:
	success = row_purge_remove_clust_if_poss_low(node, thr,
							BTR_MODIFY_TREE);
	/* The delete operation may fail if we have little
	file space left: TODO: easiest to crash the database
	and restart with more file space */

	if (!success && n_tries < BTR_CUR_RETRY_DELETE_N_TIMES) {
		n_tries++;

		os_thread_sleep(BTR_CUR_RETRY_SLEEP_TIME);
			
		goto retry;
	}

	ut_a(success);
}
 						
/***************************************************************
Removes a secondary index entry if possible. */
static
ibool
row_purge_remove_sec_if_poss_low(
/*=============================*/
				/* out: TRUE if success or if not found */
	purge_node_t*	node,	/* in: row purge node */
	que_thr_t*	thr,	/* in: query thread */
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry,	/* in: index entry */
	ulint		mode)	/* in: latch mode BTR_MODIFY_LEAF or
				BTR_MODIFY_TREE */	
{
	btr_pcur_t	pcur;
	btr_cur_t*	btr_cur;
	ibool		success;
	ibool		old_has;
	ibool		found;
	ulint		err;
	mtr_t		mtr;
	mtr_t		mtr_vers;
	
	UT_NOT_USED(thr);

	log_free_check();
	mtr_start(&mtr);
	
	found = row_search_index_entry(index, entry, mode, &pcur, &mtr);

	if (!found) {
		/* Not found */

		/* FIXME: printf("PURGE:........sec entry not found\n"); */
		/* dtuple_print(entry); */

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);

		return(TRUE);
	}

	btr_cur = btr_pcur_get_btr_cur(&pcur);
	
	/* We should remove the index record if no later version of the row,
	which cannot be purged yet, requires its existence. If some requires,
	we should do nothing. */

	mtr_start(&mtr_vers);

	success = row_purge_reposition_pcur(BTR_SEARCH_LEAF, node, &mtr_vers);

	if (success) {		
		old_has = row_vers_old_has_index_entry(TRUE,
					btr_pcur_get_rec(&(node->pcur)),
					&mtr_vers, index, entry);
	}

	btr_pcur_commit_specify_mtr(&(node->pcur), &mtr_vers);
	
	if (!success || !old_has) {
		/* Remove the index record */

		if (mode == BTR_MODIFY_LEAF) {		
			success = btr_cur_optimistic_delete(btr_cur, &mtr);
		} else {
			ut_ad(mode == BTR_MODIFY_TREE);
			btr_cur_pessimistic_delete(&err, FALSE, btr_cur, &mtr);

			if (err == DB_SUCCESS) {
				success = TRUE;
			} else if (err == DB_OUT_OF_FILE_SPACE) {
				success = FALSE;
			} else {
				ut_a(0);
			}
		}
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	return(success);
}

/***************************************************************
Removes a secondary index entry if possible. */
UNIV_INLINE
void
row_purge_remove_sec_if_poss(
/*=========================*/
	purge_node_t*	node,	/* in: row purge node */
	que_thr_t*	thr,	/* in: query thread */
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry)	/* in: index entry */
{
	ibool	success;
	ulint	n_tries		= 0;
	
/*	printf("Purge: Removing secondary record\n"); */

	success = row_purge_remove_sec_if_poss_low(node, thr, index, entry,
							BTR_MODIFY_LEAF);
	if (success) {

		return;
	}
retry:
	success = row_purge_remove_sec_if_poss_low(node, thr, index, entry,
							BTR_MODIFY_TREE);
	/* The delete operation may fail if we have little
	file space left: TODO: easiest to crash the database
	and restart with more file space */

	if (!success && n_tries < BTR_CUR_RETRY_DELETE_N_TIMES) {
				
		n_tries++;

		os_thread_sleep(BTR_CUR_RETRY_SLEEP_TIME);
							
		goto retry;
	}

	ut_a(success);
}

/***************************************************************
Purges a delete marking of a record. */
static
void
row_purge_del_mark(
/*===============*/
	purge_node_t*	node,	/* in: row purge node */
	que_thr_t*	thr)	/* in: query thread */
{
	mem_heap_t*	heap;
	dtuple_t*	entry;
	dict_index_t*	index;
	
	ut_ad(node && thr);

	heap = mem_heap_create(1024);

	while (node->index != NULL) {
		index = node->index;

		/* Build the index entry */
		entry = row_build_index_entry(node->row, index, heap);

		row_purge_remove_sec_if_poss(node, thr, index, entry);

		node->index = dict_table_get_next_index(node->index);
	}

	mem_heap_free(heap);	

	row_purge_remove_clust_if_poss(node, thr);
}
	
/***************************************************************
Purges an update of an existing record. */
static
void
row_purge_upd_exist(
/*================*/
	purge_node_t*	node,	/* in: row purge node */
	que_thr_t*	thr)	/* in: query thread */
{
	mem_heap_t*	heap;
	dtuple_t*	entry;
	dict_index_t*	index;
	
	ut_ad(node && thr);

	heap = mem_heap_create(1024);

	while (node->index != NULL) {
		index = node->index;

		if (row_upd_changes_ord_field(NULL, node->index,
							node->update)) {
			/* Build the older version of the index entry */
			entry = row_build_index_entry(node->row, index, heap);

			row_purge_remove_sec_if_poss(node, thr, index, entry);
		}

		node->index = dict_table_get_next_index(node->index);
	}

	mem_heap_free(heap);	
}

/***************************************************************
Parses the row reference and other info in a modify undo log record. */
static
ibool
row_purge_parse_undo_rec(
/*=====================*/
				/* out: TRUE if purge operation required */
	purge_node_t*	node,	/* in: row undo node */
	que_thr_t*	thr)	/* in: query thread */
{
	dict_index_t*	clust_index;
	byte*		ptr;
	dulint		undo_no;
	dulint		table_id;
	dulint		trx_id;
	dulint		roll_ptr;
	ulint		info_bits;
	ulint		type;
	ulint		cmpl_info;
	
	ut_ad(node && thr);
	
	ptr = trx_undo_rec_get_pars(node->undo_rec, &type, &cmpl_info,
							&undo_no, &table_id);
	node->rec_type = type;

	if (type == TRX_UNDO_UPD_DEL_REC) {

		return(FALSE);
	}	    		

	ptr = trx_undo_update_rec_get_sys_cols(ptr, &trx_id, &roll_ptr,
								&info_bits);
	node->table = NULL;

	if (type == TRX_UNDO_UPD_EXIST_REC
				&& cmpl_info & UPD_NODE_NO_ORD_CHANGE) {

	    	/* Purge requires no changes to indexes: we may return */

	    	return(FALSE);
	}
	
	/* NOTE that the table has to be explicitly released later */

	/* TODO: currently nothing prevents dropping of table when purge
	is accessing it! */

 	mutex_enter(&(dict_sys->mutex));

	node->table = dict_table_get_on_id_low(table_id, thr_get_trx(thr));

	rw_lock_x_lock(&(purge_sys->purge_is_running));

 	mutex_exit(&(dict_sys->mutex));
	
	if (node->table == NULL) {
		/* The table has been dropped: no need to do purge */

		rw_lock_x_unlock(&(purge_sys->purge_is_running));

		return(FALSE);
	}

	clust_index = dict_table_get_first_index(node->table);

	ptr = trx_undo_rec_get_row_ref(ptr, clust_index, &(node->ref),
								node->heap);

	ptr = trx_undo_update_rec_get_update(ptr, clust_index, type, trx_id,
					roll_ptr, info_bits, node->heap,
					&(node->update));

	/* Read to the partial row the fields that occur in indexes */

	ptr = trx_undo_rec_get_partial_row(ptr, clust_index, &(node->row),
								node->heap);
	return(TRUE);
}

/***************************************************************
Fetches an undo log record and does the purge for the recorded operation.
If none left, or the current purge completed, returns the control to the
parent node, which is always a query thread node. */
static
ulint
row_purge(
/*======*/
				/* out: DB_SUCCESS if operation successfully
				completed, else error code */
	purge_node_t*	node,	/* in: row purge node */
	que_thr_t*	thr)	/* in: query thread */
{
	dulint	roll_ptr;
	ibool	purge_needed;
	
	ut_ad(node && thr);

	node->undo_rec = trx_purge_fetch_next_rec(&roll_ptr,
						&(node->reservation),
						node->heap);
	if (!node->undo_rec) {
		/* Purge completed for this query thread */

		thr->run_node = que_node_get_parent(node);

		return(DB_SUCCESS);
	}

	node->roll_ptr = roll_ptr;

	if (node->undo_rec == &trx_purge_dummy_rec) {
		purge_needed = FALSE;
	} else {
		purge_needed = row_purge_parse_undo_rec(node, thr);
	}

	if (purge_needed) {
		node->found_clust = FALSE;
	
		node->index = dict_table_get_next_index(
				dict_table_get_first_index(node->table));

		if (node->rec_type == TRX_UNDO_UPD_EXIST_REC) {
			row_purge_upd_exist(node, thr);
		} else {
			ut_ad(node->rec_type == TRX_UNDO_DEL_MARK_REC);
			row_purge_del_mark(node, thr);
		}

		if (node->found_clust) {
			btr_pcur_close(&(node->pcur));
		}

		rw_lock_x_unlock(&(purge_sys->purge_is_running));		
	}

	/* Do some cleanup */
	trx_purge_rec_release(node->reservation);
	mem_heap_empty(node->heap);
	
	thr->run_node = node;

	return(DB_SUCCESS);
}

/***************************************************************
Does the purge operation for a single undo log record. This is a high-level
function used in an SQL execution graph. */

que_thr_t*
row_purge_step(
/*===========*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr)	/* in: query thread */
{
	purge_node_t*	node;
	ulint		err;

	ut_ad(thr);
	
	node = thr->run_node;

	ut_ad(que_node_get_type(node) == QUE_NODE_PURGE);

	err = row_purge(node, thr);

	ut_ad(err == DB_SUCCESS);

	return(thr);
} 
