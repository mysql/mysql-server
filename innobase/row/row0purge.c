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
#include "row0mysql.h"
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
	ulint		mode)	/* in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE */
{
	dict_index_t*	index;
	btr_pcur_t*	pcur;
	btr_cur_t*	btr_cur;
	ibool		success;
	ulint		err;
	mtr_t		mtr;

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
		btr_cur_pessimistic_delete(&err, FALSE, btr_cur, FALSE, &mtr);

		if (err == DB_SUCCESS) {
			success = TRUE;
		} else if (err == DB_OUT_OF_FILE_SPACE) {
			success = FALSE;
		} else {
			ut_error;
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
	purge_node_t*	node)	/* in: row purge node */
{
	ibool	success;
	ulint	n_tries	= 0;
	
/*	fputs("Purge: Removing clustered record\n", stderr); */

	success = row_purge_remove_clust_if_poss_low(node, BTR_MODIFY_LEAF);
	if (success) {

		return;
	}
retry:
	success = row_purge_remove_clust_if_poss_low(node, BTR_MODIFY_TREE);
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
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry,	/* in: index entry */
	ulint		mode)	/* in: latch mode BTR_MODIFY_LEAF or
				BTR_MODIFY_TREE */	
{
	btr_pcur_t	pcur;
	btr_cur_t*	btr_cur;
	ibool		success;
	ibool		old_has = 0; /* remove warning */
	ibool		found;
	ulint		err;
	mtr_t		mtr;
	mtr_t*		mtr_vers;
	
	log_free_check();
	mtr_start(&mtr);
	
	found = row_search_index_entry(index, entry, mode, &pcur, &mtr);

	if (!found) {
		/* Not found */

		/* fputs("PURGE:........sec entry not found\n", stderr); */
		/* dtuple_print(entry); */

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);

		return(TRUE);
	}

	btr_cur = btr_pcur_get_btr_cur(&pcur);
	
	/* We should remove the index record if no later version of the row,
	which cannot be purged yet, requires its existence. If some requires,
	we should do nothing. */

	mtr_vers = mem_alloc(sizeof(mtr_t));
	
	mtr_start(mtr_vers);

	success = row_purge_reposition_pcur(BTR_SEARCH_LEAF, node, mtr_vers);

	if (success) {		
		old_has = row_vers_old_has_index_entry(TRUE,
					btr_pcur_get_rec(&(node->pcur)),
					mtr_vers, index, entry);
	}

	btr_pcur_commit_specify_mtr(&(node->pcur), mtr_vers);

	mem_free(mtr_vers);
	
	if (!success || !old_has) {
		/* Remove the index record */

		if (mode == BTR_MODIFY_LEAF) {		
			success = btr_cur_optimistic_delete(btr_cur, &mtr);
		} else {
			ut_ad(mode == BTR_MODIFY_TREE);
			btr_cur_pessimistic_delete(&err, FALSE, btr_cur,
							FALSE, &mtr);
			if (err == DB_SUCCESS) {
				success = TRUE;
			} else if (err == DB_OUT_OF_FILE_SPACE) {
				success = FALSE;
			} else {
				ut_error;
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
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry)	/* in: index entry */
{
	ibool	success;
	ulint	n_tries		= 0;
	
/*	fputs("Purge: Removing secondary record\n", stderr); */

	success = row_purge_remove_sec_if_poss_low(node, index, entry,
							BTR_MODIFY_LEAF);
	if (success) {

		return;
	}
retry:
	success = row_purge_remove_sec_if_poss_low(node, index, entry,
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
	purge_node_t*	node)	/* in: row purge node */
{
	mem_heap_t*	heap;
	dtuple_t*	entry;
	dict_index_t*	index;
	
	ut_ad(node);

	heap = mem_heap_create(1024);

	while (node->index != NULL) {
		index = node->index;

		/* Build the index entry */
		entry = row_build_index_entry(node->row, index, heap);

		row_purge_remove_sec_if_poss(node, index, entry);

		node->index = dict_table_get_next_index(node->index);
	}

	mem_heap_free(heap);	

	row_purge_remove_clust_if_poss(node);
}
	
/***************************************************************
Purges an update of an existing record. Also purges an update of a delete
marked record if that record contained an externally stored field. */
static
void
row_purge_upd_exist_or_extern(
/*==========================*/
	purge_node_t*	node)	/* in: row purge node */
{
	mem_heap_t*	heap;
	dtuple_t*	entry;
	dict_index_t*	index;
	upd_field_t*	ufield;
	ibool		is_insert;
	ulint		rseg_id;
	ulint		page_no;
	ulint		offset;
	ulint		internal_offset;
	byte*		data_field;
	ulint		data_field_len;
	ulint		i;
	mtr_t		mtr;
	
	ut_ad(node);

	if (node->rec_type == TRX_UNDO_UPD_DEL_REC) {

		goto skip_secondaries;
	}

	heap = mem_heap_create(1024);

	while (node->index != NULL) {
		index = node->index;

		if (row_upd_changes_ord_field_binary(NULL, node->index,
							node->update)) {
			/* Build the older version of the index entry */
			entry = row_build_index_entry(node->row, index, heap);

			row_purge_remove_sec_if_poss(node, index, entry);
		}

		node->index = dict_table_get_next_index(node->index);
	}

	mem_heap_free(heap);	

skip_secondaries:
	/* Free possible externally stored fields */
	for (i = 0; i < upd_get_n_fields(node->update); i++) {

		ufield = upd_get_nth_field(node->update, i);

		if (ufield->extern_storage) {
			/* We use the fact that new_val points to
			node->undo_rec and get thus the offset of
			dfield data inside the unod record. Then we
			can calculate from node->roll_ptr the file
			address of the new_val data */

			internal_offset = ((byte*)ufield->new_val.data)
						- node->undo_rec;
						
			ut_a(internal_offset < UNIV_PAGE_SIZE);

			trx_undo_decode_roll_ptr(node->roll_ptr,
						&is_insert, &rseg_id,
						&page_no, &offset);
			mtr_start(&mtr);

			/* We have to acquire an X-latch to the clustered
			index tree */

			index = dict_table_get_first_index(node->table);

			mtr_x_lock(dict_tree_get_lock(index->tree), &mtr);

			/* NOTE: we must also acquire an X-latch to the
			root page of the tree. We will need it when we
			free pages from the tree. If the tree is of height 1,
			the tree X-latch does NOT protect the root page,
			because it is also a leaf page. Since we will have a
			latch on an undo log page, we would break the
			latching order if we would only later latch the
			root page of such a tree! */
			
			btr_root_get(index->tree, &mtr);

			/* We assume in purge of externally stored fields
			that the space id of the undo log record is 0! */

			data_field = buf_page_get(0, page_no, RW_X_LATCH, &mtr)
				     + offset + internal_offset;

#ifdef UNIV_SYNC_DEBUG
			buf_page_dbg_add_level(buf_frame_align(data_field),
						SYNC_TRX_UNDO_PAGE);
#endif /* UNIV_SYNC_DEBUG */
				     
			data_field_len = ufield->new_val.len;

			btr_free_externally_stored_field(index, data_field,
						data_field_len, FALSE, &mtr);
			mtr_commit(&mtr);
		}
	}
}

/***************************************************************
Parses the row reference and other info in a modify undo log record. */
static
ibool
row_purge_parse_undo_rec(
/*=====================*/
				/* out: TRUE if purge operation required:
				NOTE that then the CALLER must unfreeze
				data dictionary! */
	purge_node_t*	node,	/* in: row undo node */
	ibool*		updated_extern,
				/* out: TRUE if an externally stored field
				was updated */
	que_thr_t*	thr)	/* in: query thread */
{
	dict_index_t*	clust_index;
	byte*		ptr;
	trx_t*		trx;
	dulint		undo_no;
	dulint		table_id;
	dulint		trx_id;
	dulint		roll_ptr;
	ulint		info_bits;
	ulint		type;
	ulint		cmpl_info;
	
	ut_ad(node && thr);

	trx = thr_get_trx(thr);
	
	ptr = trx_undo_rec_get_pars(node->undo_rec, &type, &cmpl_info,
					updated_extern, &undo_no, &table_id);
	node->rec_type = type;

	if (type == TRX_UNDO_UPD_DEL_REC && !(*updated_extern)) {

		return(FALSE);
	}	    		

	ptr = trx_undo_update_rec_get_sys_cols(ptr, &trx_id, &roll_ptr,
								&info_bits);
	node->table = NULL;

	if (type == TRX_UNDO_UPD_EXIST_REC
	    && cmpl_info & UPD_NODE_NO_ORD_CHANGE && !(*updated_extern)) {

	    	/* Purge requires no changes to indexes: we may return */

	    	return(FALSE);
	}
	
	/* Prevent DROP TABLE etc. from running when we are doing the purge
	for this row */

	row_mysql_freeze_data_dictionary(trx);

	mutex_enter(&(dict_sys->mutex));

	node->table = dict_table_get_on_id_low(table_id, trx);
	
	mutex_exit(&(dict_sys->mutex));

	if (node->table == NULL) {
		/* The table has been dropped: no need to do purge */

		row_mysql_unfreeze_data_dictionary(trx);

		return(FALSE);
	}

	if (node->table->ibd_file_missing) {
		/* We skip purge of missing .ibd files */

		node->table = NULL;

		row_mysql_unfreeze_data_dictionary(trx);

		return(FALSE);
	}

	clust_index = dict_table_get_first_index(node->table);

	if (clust_index == NULL) {
		/* The table was corrupt in the data dictionary */

		row_mysql_unfreeze_data_dictionary(trx);

		return(FALSE);
	}

	ptr = trx_undo_rec_get_row_ref(ptr, clust_index, &(node->ref),
								node->heap);

	ptr = trx_undo_update_rec_get_update(ptr, clust_index, type, trx_id,
					roll_ptr, info_bits, trx,
					node->heap, &(node->update));

	/* Read to the partial row the fields that occur in indexes */

	if (!cmpl_info & UPD_NODE_NO_ORD_CHANGE) {
		ptr = trx_undo_rec_get_partial_row(ptr, clust_index,
						&(node->row), node->heap);
	}
	
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
	ibool	updated_extern;
	trx_t*	trx;
	
	ut_ad(node && thr);

	trx = thr_get_trx(thr);
	
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
		purge_needed = row_purge_parse_undo_rec(node, &updated_extern,
									thr);
		/* If purge_needed == TRUE, we must also remember to unfreeze
		data dictionary! */
	}

	if (purge_needed) {
		node->found_clust = FALSE;
	
		node->index = dict_table_get_next_index(
				dict_table_get_first_index(node->table));

		if (node->rec_type == TRX_UNDO_DEL_MARK_REC) {
			row_purge_del_mark(node);

		} else if (updated_extern
			    || node->rec_type == TRX_UNDO_UPD_EXIST_REC) {

			row_purge_upd_exist_or_extern(node);
		}

		if (node->found_clust) {
			btr_pcur_close(&(node->pcur));
		}

		row_mysql_unfreeze_data_dictionary(trx);
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
