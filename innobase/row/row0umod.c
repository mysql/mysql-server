/******************************************************
Undo modify of a row

(c) 1997 Innobase Oy

Created 2/27/1997 Heikki Tuuri
*******************************************************/

#include "row0umod.h"

#ifdef UNIV_NONINL
#include "row0umod.ic"
#endif

#include "dict0dict.h"
#include "dict0boot.h"
#include "trx0undo.h"
#include "trx0roll.h"
#include "btr0btr.h"
#include "mach0data.h"
#include "row0undo.h"
#include "row0vers.h"
#include "trx0trx.h"
#include "trx0rec.h"
#include "row0row.h"
#include "row0upd.h"
#include "que0que.h"
#include "log0log.h"

/* Considerations on undoing a modify operation.
(1) Undoing a delete marking: all index records should be found. Some of
them may have delete mark already FALSE, if the delete mark operation was
stopped underway, or if the undo operation ended prematurely because of a
system crash.
(2) Undoing an update of a delete unmarked record: the newer version of
an updated secondary index entry should be removed if no prior version
of the clustered index record requires its existence. Otherwise, it should
be delete marked.
(3) Undoing an update of a delete marked record. In this kind of update a
delete marked clustered index record was delete unmarked and possibly also
some of its fields were changed. Now, it is possible that the delete marked
version has become obsolete at the time the undo is started. */

/***************************************************************
Checks if also the previous version of the clustered index record was
modified or inserted by the same transaction, and its undo number is such
that it should be undone in the same rollback. */
UNIV_INLINE
ibool
row_undo_mod_undo_also_prev_vers(
/*=============================*/
				/* out: TRUE if also previous modify or
				insert of this row should be undone */
 	undo_node_t*	node,	/* in: row undo node */
	que_thr_t*	thr,	/* in: query thread */
	dulint*		undo_no)/* out: the undo number */
{
	trx_undo_rec_t*	undo_rec;
	ibool		ret;
	trx_t*		trx;

	UT_NOT_USED(thr);

	trx = node->trx;
	
	if (0 != ut_dulint_cmp(node->new_trx_id, trx->id)) {

		return(FALSE);
	}

	undo_rec = trx_undo_get_undo_rec_low(node->new_roll_ptr, node->heap);

	*undo_no = trx_undo_rec_get_undo_no(undo_rec);

	if (ut_dulint_cmp(trx->roll_limit, *undo_no) <= 0) {
		ret = TRUE;
	} else {
		ret = FALSE;
	}
	
	return(ret);
}
	
/***************************************************************
Undoes a modify in a clustered index record. */
static
ulint
row_undo_mod_clust_low(
/*===================*/
				/* out: DB_SUCCESS, DB_FAIL, or error code:
				we may run out of file space */
	undo_node_t*	node,	/* in: row undo node */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr,	/* in: mtr */
	ulint		mode)	/* in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE */
{
	dict_index_t*	index;
	btr_pcur_t*	pcur;
	btr_cur_t*	btr_cur;
	ulint		err;
	ibool		success;
	ibool		do_remove;

	index = dict_table_get_first_index(node->table);
	
	pcur = &(node->pcur);
	btr_cur = btr_pcur_get_btr_cur(pcur);

	success = btr_pcur_restore_position(mode, pcur, mtr);

	ut_ad(success);

	/* Find out if we can remove the whole clustered index record */

	if (node->rec_type == TRX_UNDO_UPD_DEL_REC
	    && !row_vers_must_preserve_del_marked(node->new_trx_id, mtr)) {

		do_remove = TRUE;
	} else {
		do_remove = FALSE;
	}
	    
	if (mode == BTR_MODIFY_LEAF) {

		if (do_remove) {
			success = btr_cur_optimistic_delete(btr_cur, mtr);

			if (success) {
				err = DB_SUCCESS;
			} else {
				err = DB_FAIL;
			}
		} else {
			err = btr_cur_optimistic_update(BTR_NO_LOCKING_FLAG
					| BTR_NO_UNDO_LOG_FLAG
					| BTR_KEEP_SYS_FLAG,
					btr_cur, node->update,
					node->cmpl_info, thr, mtr);
		}
	} else {
		ut_ad(mode == BTR_MODIFY_TREE);

		if (do_remove) {
			btr_cur_pessimistic_delete(&err, FALSE, btr_cur, mtr);

			/* The delete operation may fail if we have little
			file space left: TODO: easiest to crash the database
			and restart with more file space */
		} else {
			err = btr_cur_pessimistic_update(BTR_NO_LOCKING_FLAG
					| BTR_NO_UNDO_LOG_FLAG
					| BTR_KEEP_SYS_FLAG,
					btr_cur, node->update,
					node->cmpl_info, thr, mtr);
		}
	}

	return(err);
}
		
/***************************************************************
Undoes a modify in a clustered index record. Sets also the node state for the
next round of undo. */
static
ulint
row_undo_mod_clust(
/*===============*/
				/* out: DB_SUCCESS or error code: we may run
				out of file space */
	undo_node_t*	node,	/* in: row undo node */
	que_thr_t*	thr)	/* in: query thread */
{
	btr_pcur_t*	pcur;
	mtr_t		mtr;
	ulint		err;
	ibool		success;
	ibool		more_vers;
	dulint		new_undo_no;
	
	ut_ad(node && thr);

	/* Check if also the previous version of the clustered index record
	should be undone in this same rollback operation */

	more_vers = row_undo_mod_undo_also_prev_vers(node, thr, &new_undo_no);

	pcur = &(node->pcur);

	mtr_start(&mtr);

	/* Try optimistic processing of the record, keeping changes within
	the index page */

	err = row_undo_mod_clust_low(node, thr, &mtr, BTR_MODIFY_LEAF);

	if (err != DB_SUCCESS) {
		btr_pcur_commit_specify_mtr(pcur, &mtr);

		/* We may have to modify tree structure: do a pessimistic
		descent down the index tree */

		mtr_start(&mtr);

		err = row_undo_mod_clust_low(node, thr, &mtr, BTR_MODIFY_TREE);
	}

	node->state = UNDO_NODE_FETCH_NEXT;

	btr_pcur_commit_specify_mtr(pcur, &mtr);

 	trx_undo_rec_release(node->trx, node->undo_no);

	if (more_vers && err == DB_SUCCESS) {

		/* Reserve the undo log record to the prior version after
		committing &mtr: this is necessary to comply with the latching
		order, as &mtr may contain the fsp latch which is lower in
		the latch hierarchy than trx->undo_mutex. */

		success = trx_undo_rec_reserve(node->trx, new_undo_no);

		if (success) {
			node->state = UNDO_NODE_PREV_VERS;
		}
	}

	return(err);
}

/***************************************************************
Delete marks or removes a secondary index entry if found. */
static
ulint
row_undo_mod_del_mark_or_remove_sec_low(
/*====================================*/
				/* out: DB_SUCCESS, DB_FAIL, or
				DB_OUT_OF_FILE_SPACE */
	undo_node_t*	node,	/* in: row undo node */
	que_thr_t*	thr,	/* in: query thread */
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry,	/* in: index entry */
	ulint		mode)	/* in: latch mode BTR_MODIFY_LEAF or
				BTR_MODIFY_TREE */	
{
	ibool		found;
	mtr_t		mtr;
	mtr_t		mtr_vers;
	btr_pcur_t	pcur;
	btr_cur_t*	btr_cur;
	ibool		success;
	ibool		old_has;
	ulint		err;
	
	log_free_check();
	mtr_start(&mtr);
	
	found = row_search_index_entry(index, entry, mode, &pcur, &mtr);

	btr_cur = btr_pcur_get_btr_cur(&pcur);

	if (!found) {
		/* Not found */

		/* FIXME: remove printfs in the final version */

		/* printf(
		"--UNDO MOD: Record not found from page %lu index %s\n",
			buf_frame_get_page_no(btr_cur_get_rec(btr_cur)),
			index->name); */

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);

		return(DB_SUCCESS);
	}

	/* We should remove the index record if no prior version of the row,
	which cannot be purged yet, requires its existence. If some requires,
	we should delete mark the record. */

	mtr_start(&mtr_vers);
		
	success = btr_pcur_restore_position(BTR_SEARCH_LEAF, &(node->pcur),
								&mtr_vers);
	ut_ad(success);
		
	old_has = row_vers_old_has_index_entry(FALSE,
					btr_pcur_get_rec(&(node->pcur)),
					&mtr_vers, index, entry);
	if (old_has) {
		err = btr_cur_del_mark_set_sec_rec(BTR_NO_LOCKING_FLAG,
						btr_cur, TRUE, thr, &mtr);
		ut_ad(err == DB_SUCCESS);
	} else {
		/* Remove the index record */

		if (mode == BTR_MODIFY_LEAF) {		
			success = btr_cur_optimistic_delete(btr_cur, &mtr);
			if (success) {
				err = DB_SUCCESS;
			} else {
				err = DB_FAIL;
			}
		} else {
			ut_ad(mode == BTR_MODIFY_TREE);

			btr_cur_pessimistic_delete(&err, FALSE, btr_cur, &mtr);

			/* The delete operation may fail if we have little
			file space left: TODO: easiest to crash the database
			and restart with more file space */
		}
	}

	btr_pcur_commit_specify_mtr(&(node->pcur), &mtr_vers);
	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	return(err);
}

/***************************************************************
Delete marks or removes a secondary index entry if found. */
UNIV_INLINE
ulint
row_undo_mod_del_mark_or_remove_sec(
/*================================*/
				/* out: DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
	undo_node_t*	node,	/* in: row undo node */
	que_thr_t*	thr,	/* in: query thread */
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry)	/* in: index entry */
{
	ulint	err;
	
	err = row_undo_mod_del_mark_or_remove_sec_low(node, thr, index,
 						entry, BTR_MODIFY_LEAF);
	if (err == DB_SUCCESS) {

		return(err);
	}

	err = row_undo_mod_del_mark_or_remove_sec_low(node, thr, index,
 						entry, BTR_MODIFY_TREE);
 	return(err);
}

/***************************************************************
Delete unmarks a secondary index entry which must be found. */
static
void
row_undo_mod_del_unmark_sec(
/*========================*/
	undo_node_t*	node,	/* in: row undo node */
	que_thr_t*	thr,	/* in: query thread */
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry)	/* in: index entry */
{
	mtr_t		mtr;
	btr_pcur_t	pcur;
	btr_cur_t*	btr_cur;
	ulint		err;
	ibool		found;

	UT_NOT_USED(node);
	
	log_free_check();
	mtr_start(&mtr);
	
	found = row_search_index_entry(index, entry, BTR_MODIFY_LEAF, &pcur,
									&mtr);
	ut_a(found);

	btr_cur = btr_pcur_get_btr_cur(&pcur);

	err = btr_cur_del_mark_set_sec_rec(BTR_NO_LOCKING_FLAG,
						btr_cur, FALSE, thr, &mtr);
	ut_ad(err == DB_SUCCESS);

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
}

/***************************************************************
Undoes a modify in secondary indexes when undo record type is UPD_DEL. */
static
ulint
row_undo_mod_upd_del_sec(
/*=====================*/
				/* out: DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
	undo_node_t*	node,	/* in: row undo node */
	que_thr_t*	thr)	/* in: query thread */
{
	mem_heap_t*	heap;
	dtuple_t*	entry;
	dict_index_t*	index;
	ulint		err;
	
	heap = mem_heap_create(1024);

	while (node->index != NULL) {
		index = node->index;

		entry = row_build_index_entry(node->row, index, heap);

		err = row_undo_mod_del_mark_or_remove_sec(node, thr, index,
								entry);
		if (err != DB_SUCCESS) {

			mem_heap_free(heap);

			return(err);
		}
									
		node->index = dict_table_get_next_index(node->index);
	}

	mem_heap_free(heap);

	return(DB_SUCCESS);
}

/***************************************************************
Undoes a modify in secondary indexes when undo record type is DEL_MARK. */
static
ulint
row_undo_mod_del_mark_sec(
/*======================*/
				/* out: DB_SUCCESS */
	undo_node_t*	node,	/* in: row undo node */
	que_thr_t*	thr)	/* in: query thread */
{
	mem_heap_t*	heap;
	dtuple_t*	entry;
	dict_index_t*	index;

	heap = mem_heap_create(1024);

	while (node->index != NULL) {
		index = node->index;

		entry = row_build_index_entry(node->row, index, heap);
		
		row_undo_mod_del_unmark_sec(node, thr, index, entry);

		node->index = dict_table_get_next_index(node->index);
	}

	mem_heap_free(heap);	

	return(DB_SUCCESS);
}

/***************************************************************
Undoes a modify in secondary indexes when undo record type is UPD_EXIST. */
static
ulint
row_undo_mod_upd_exist_sec(
/*=======================*/
				/* out: DB_SUCCESS or error code */
	undo_node_t*	node,	/* in: row undo node */
	que_thr_t*	thr)	/* in: query thread */
{
	mem_heap_t*	heap;
	dtuple_t*	entry;
	dict_index_t*	index;
	ulint		err;

	if (node->cmpl_info & UPD_NODE_NO_ORD_CHANGE) {
		/* No change in secondary indexes */
	
		return(DB_SUCCESS);
	}
	
	heap = mem_heap_create(1024);

	while (node->index != NULL) {
		index = node->index;

		if (row_upd_changes_ord_field(node->row, node->index,
							node->update)) {

			/* Build the newest version of the index entry */
			entry = row_build_index_entry(node->row, index, heap);

			err = row_undo_mod_del_mark_or_remove_sec(node, thr,
							index, entry);
			if (err != DB_SUCCESS) {
				mem_heap_free(heap);

				return(err);
			}
							
			/* We may have to update the delete mark in the
			secondary index record of the previous version of
			the row */

			row_upd_index_replace_new_col_vals(entry, index,
								node->update);

			row_undo_mod_del_unmark_sec(node, thr, index, entry);
		}

		node->index = dict_table_get_next_index(node->index);
	}

	mem_heap_free(heap);

	return(DB_SUCCESS);
}

/***************************************************************
Parses the row reference and other info in a modify undo log record. */
static
void
row_undo_mod_parse_undo_rec(
/*========================*/
	undo_node_t*	node,	/* in: row undo node */
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
	
	node->table = dict_table_get_on_id(table_id, thr_get_trx(thr));

	/* TODO: other fixes associated with DROP TABLE + rollback in the
	same table by another user */

	if (node->table == NULL) {
	        /* Table was dropped */
	        return;
	}

	clust_index = dict_table_get_first_index(node->table);

	ptr = trx_undo_update_rec_get_sys_cols(ptr, &trx_id, &roll_ptr,
								&info_bits);

	ptr = trx_undo_rec_get_row_ref(ptr, clust_index, &(node->ref),
								node->heap);

	trx_undo_update_rec_get_update(ptr, clust_index, type, trx_id,
					roll_ptr, info_bits, node->heap,
					&(node->update));
	node->new_roll_ptr = roll_ptr;
	node->new_trx_id = trx_id;
	node->cmpl_info = cmpl_info;
}
	
/***************************************************************
Undoes a modify operation on a row of a table. */

ulint
row_undo_mod(
/*=========*/
				/* out: DB_SUCCESS or error code */
	undo_node_t*	node,	/* in: row undo node */
	que_thr_t*	thr)	/* in: query thread */
{
	ibool	found;
	ulint	err;
	
	ut_ad(node && thr);
	ut_ad(node->state == UNDO_NODE_MODIFY);

	row_undo_mod_parse_undo_rec(node, thr);

	if (node->table == NULL) {
	  found = FALSE;
	} else {

	  found = row_undo_search_clust_to_pcur(node, thr);
	}

	if (!found) {
		/* It is already undone, or will be undone by another query
		thread, or table was dropped */
	
		node->state = UNDO_NODE_FETCH_NEXT;

		return(DB_SUCCESS);
	}

	node->index = dict_table_get_next_index(
				dict_table_get_first_index(node->table));

	if (node->rec_type == TRX_UNDO_UPD_EXIST_REC) {
		
		err = row_undo_mod_upd_exist_sec(node, thr);

	} else if (node->rec_type == TRX_UNDO_DEL_MARK_REC) {

		err = row_undo_mod_del_mark_sec(node, thr);
	} else {
		ut_ad(node->rec_type == TRX_UNDO_UPD_DEL_REC);
		err = row_undo_mod_upd_del_sec(node, thr);
	}

	if (err != DB_SUCCESS) {

		return(err);
	}
	
	err = row_undo_mod_clust(node, thr);
	
	return(err);
}
