/******************************************************
Insert into a table

(c) 1996 Innobase Oy

Created 4/20/1996 Heikki Tuuri
*******************************************************/

#include "row0ins.h"

#ifdef UNIV_NONINL
#include "row0ins.ic"
#endif

#include "dict0dict.h"
#include "dict0boot.h"
#include "trx0undo.h"
#include "btr0btr.h"
#include "btr0cur.h"
#include "mach0data.h"
#include "que0que.h"
#include "row0upd.h"
#include "row0sel.h"
#include "row0row.h"
#include "rem0cmp.h"
#include "lock0lock.h"
#include "log0log.h"
#include "eval0eval.h"
#include "data0data.h"
#include "usr0sess.h"

#define	ROW_INS_PREV	1
#define	ROW_INS_NEXT	2

/*************************************************************************
Creates an insert node struct. */

ins_node_t*
ins_node_create(
/*============*/
					/* out, own: insert node struct */
	ulint		ins_type,	/* in: INS_VALUES, ... */
	dict_table_t*	table, 		/* in: table where to insert */
	mem_heap_t*	heap)		/* in: mem heap where created */
{
	ins_node_t*	node;

	node = mem_heap_alloc(heap, sizeof(ins_node_t));

	node->common.type = QUE_NODE_INSERT;

	node->ins_type = ins_type;

	node->state = INS_NODE_SET_IX_LOCK;
	node->table = table;
	node->index = NULL;
	node->entry = NULL;

	node->select = NULL;
	
	node->trx_id = ut_dulint_zero;
	
	node->entry_sys_heap = mem_heap_create(128);

	node->magic_n = INS_NODE_MAGIC_N;	
	
	return(node);
}

/***************************************************************
Creates an entry template for each index of a table. */
static
void
ins_node_create_entry_list(
/*=======================*/
	ins_node_t*	node)	/* in: row insert node */
{
	dict_index_t*	index;
	dtuple_t*	entry;

	ut_ad(node->entry_sys_heap);

	UT_LIST_INIT(node->entry_list);

	index = dict_table_get_first_index(node->table);
	
	while (index != NULL) {
		entry = row_build_index_entry(node->row, index,
							node->entry_sys_heap);
		UT_LIST_ADD_LAST(tuple_list, node->entry_list, entry);

		index = dict_table_get_next_index(index);
	}
}

/*********************************************************************
Adds system field buffers to a row. */
static
void
row_ins_alloc_sys_fields(
/*=====================*/
	ins_node_t*	node)	/* in: insert node */
{
	dtuple_t*	row;
	dict_table_t*	table;
	mem_heap_t*	heap;
	dict_col_t*	col;
	dfield_t*	dfield;
	ulint		len;
	byte*		ptr;

	row = node->row;
	table = node->table;
	heap = node->entry_sys_heap;

	ut_ad(row && table && heap);
	ut_ad(dtuple_get_n_fields(row) == dict_table_get_n_cols(table));

	/* 1. Allocate buffer for row id */

	col = dict_table_get_sys_col(table, DATA_ROW_ID);
	
	dfield = dtuple_get_nth_field(row, dict_col_get_no(col));

	ptr = mem_heap_alloc(heap, DATA_ROW_ID_LEN);
				
	dfield_set_data(dfield, ptr, DATA_ROW_ID_LEN);

	node->row_id_buf = ptr;

	if (table->type == DICT_TABLE_CLUSTER_MEMBER) {

		/* 2. Fill in the dfield for mix id */

		col = dict_table_get_sys_col(table, DATA_MIX_ID);
	
		dfield = dtuple_get_nth_field(row, dict_col_get_no(col));

		len = mach_dulint_get_compressed_size(table->mix_id);
		ptr = mem_heap_alloc(heap, DATA_MIX_ID_LEN);
				
		mach_dulint_write_compressed(ptr, table->mix_id);
		dfield_set_data(dfield, ptr, len);
	}

	/* 3. Allocate buffer for trx id */

	col = dict_table_get_sys_col(table, DATA_TRX_ID);
	
	dfield = dtuple_get_nth_field(row, dict_col_get_no(col));
	ptr = mem_heap_alloc(heap, DATA_TRX_ID_LEN);
				
	dfield_set_data(dfield, ptr, DATA_TRX_ID_LEN);

	node->trx_id_buf = ptr;

	/* 4. Allocate buffer for roll ptr */

	col = dict_table_get_sys_col(table, DATA_ROLL_PTR);
	
	dfield = dtuple_get_nth_field(row, dict_col_get_no(col));
	ptr = mem_heap_alloc(heap, DATA_ROLL_PTR_LEN);
				
	dfield_set_data(dfield, ptr, DATA_ROLL_PTR_LEN);
}

/*************************************************************************
Sets a new row to insert for an INS_DIRECT node. This function is only used
if we have constructed the row separately, which is a rare case; this
function is quite slow. */

void
ins_node_set_new_row(
/*=================*/
	ins_node_t*	node,	/* in: insert node */
	dtuple_t*	row)	/* in: new row (or first row) for the node */
{
	node->state = INS_NODE_SET_IX_LOCK;
	node->index = NULL;
	node->entry = NULL;

	node->row = row;

	mem_heap_empty(node->entry_sys_heap);

	/* Create templates for index entries */
			
	ins_node_create_entry_list(node);

	/* Allocate from entry_sys_heap buffers for sys fields */

	row_ins_alloc_sys_fields(node);

	/* As we allocated a new trx id buf, the trx id should be written
	there again: */

	node->trx_id = ut_dulint_zero;
}

/***********************************************************************
Does an insert operation by updating a delete marked existing record
in the index. This situation can occur if the delete marked record is
kept in the index for consistent reads. */
static
ulint
row_ins_sec_index_entry_by_modify(
/*==============================*/
				/* out: DB_SUCCESS or error code */
	btr_cur_t*	cursor,	/* in: B-tree cursor */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr)	/* in: mtr */
{
	ulint	err;

	ut_ad(((cursor->index)->type & DICT_CLUSTERED) == 0);
	ut_ad(rec_get_deleted_flag(btr_cur_get_rec(cursor)));
	
	/* We just remove the delete mark from the secondary index record */
	err = btr_cur_del_mark_set_sec_rec(0, cursor, FALSE, thr, mtr);

	return(err);
}

/***********************************************************************
Does an insert operation by delete unmarking and updating a delete marked
existing record in the index. This situation can occur if the delete marked
record is kept in the index for consistent reads. */
static
ulint
row_ins_clust_index_entry_by_modify(
/*================================*/
				/* out: DB_SUCCESS, DB_FAIL, or error code */
	ulint		mode,	/* in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE,
				depending on whether mtr holds just a leaf
				latch or also a tree latch */
	btr_cur_t*	cursor,	/* in: B-tree cursor */
	big_rec_t**	big_rec,/* out: possible big rec vector of fields
				which have to be stored externally by the
				caller */
	dtuple_t*	entry,	/* in: index entry to insert */
	ulint*		ext_vec,/* in: array containing field numbers of
				externally stored fields in entry, or NULL */
	ulint		n_ext_vec,/* in: number of fields in ext_vec */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr)	/* in: mtr */
{
	mem_heap_t*	heap;
	rec_t*		rec;
	upd_t*		update;
	ulint		err;
	
	ut_ad(cursor->index->type & DICT_CLUSTERED);
	
	*big_rec = NULL;

	rec = btr_cur_get_rec(cursor);

	ut_ad(rec_get_deleted_flag(rec));	

	heap = mem_heap_create(1024);
	
	/* Build an update vector containing all the fields to be modified;
	NOTE that this vector may contain also system columns! */
	
	update = row_upd_build_difference(cursor->index, entry, ext_vec,
						n_ext_vec, rec, heap); 
	if (mode == BTR_MODIFY_LEAF) {
		/* Try optimistic updating of the record, keeping changes
		within the page */

		err = btr_cur_optimistic_update(0, cursor, update, 0, thr, mtr);

		if (err == DB_OVERFLOW || err == DB_UNDERFLOW) {
			err = DB_FAIL;
		}
	} else  {
		ut_a(mode == BTR_MODIFY_TREE);
		err = btr_cur_pessimistic_update(0, cursor, big_rec, update,
								0, thr, mtr);
	}
	
	mem_heap_free(heap);

	return(err);
}

/*******************************************************************
Checks if a unique key violation to rec would occur at the index entry
insert. */
static
ibool
row_ins_dupl_error_with_rec(
/*========================*/
				/* out: TRUE if error */
	rec_t*		rec,	/* in: user record; NOTE that we assume
				that the caller already has a record lock on
				the record! */
	dtuple_t*	entry,	/* in: entry to insert */
	dict_index_t*	index)	/* in: index */
{
	ulint	matched_fields;
	ulint	matched_bytes;
	ulint	n_unique;
	
	n_unique = dict_index_get_n_unique(index);

	matched_fields = 0;
	matched_bytes = 0;

	cmp_dtuple_rec_with_match(entry, rec, &matched_fields, &matched_bytes);

	if (matched_fields < n_unique) {

			return(FALSE);
	}

	if (!rec_get_deleted_flag(rec)) {

			return(TRUE);
	}

	return(FALSE);
}	

/*************************************************************************
Sets a shared lock on a record. Used in locking possible duplicate key
records. */
static
ulint
row_ins_set_shared_rec_lock(
/*========================*/
				/* out: DB_SUCCESS or error code */
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,	/* in: index */
	que_thr_t*	thr)	/* in: query thread */	
{
	ulint	err;

	if (index->type & DICT_CLUSTERED) {
		err = lock_clust_rec_read_check_and_lock(0, rec, index, LOCK_S,
									thr);
	} else {
		err = lock_sec_rec_read_check_and_lock(0, rec, index, LOCK_S,
									thr);
	}

	return(err);
}
	
/*******************************************************************
Scans a unique non-clustered index at a given index entry to determine
whether a uniqueness violation has occurred for the key value of the entry.
Set shared locks on possible duplicate records. */
static
ulint
row_ins_scan_sec_index_for_duplicate(
/*=================================*/
				/* out: DB_SUCCESS, DB_DUPLICATE_KEY, or
				DB_LOCK_WAIT */
	dict_index_t*	index,	/* in: non-clustered unique index */
	dtuple_t*	entry,	/* in: index entry */
	que_thr_t*	thr)	/* in: query thread */
{
	int		cmp;
	ulint		n_fields_cmp;
	rec_t*		rec;
	btr_pcur_t	pcur;
	trx_t*		trx		= thr_get_trx(thr);
	ulint		err		= DB_SUCCESS;
	ibool		moved;
	mtr_t		mtr;

	mtr_start(&mtr);

	/* Store old value on n_fields_cmp */

	n_fields_cmp = dtuple_get_n_fields_cmp(entry);

	dtuple_set_n_fields_cmp(entry, dict_index_get_n_unique(index));
	
	btr_pcur_open(index, entry, PAGE_CUR_GE, BTR_SEARCH_LEAF, &pcur, &mtr);

	/* Scan index records and check if there is a duplicate */

	for (;;) {
		rec = btr_pcur_get_rec(&pcur);

		if (rec == page_get_infimum_rec(buf_frame_align(rec))) {

			goto next_rec;
		}
				
		/* Try to place a lock on the index record */	

		err = row_ins_set_shared_rec_lock(rec, index, thr);

		if (err != DB_SUCCESS) {

			break;
		}

		if (rec == page_get_supremum_rec(buf_frame_align(rec))) {
		
			goto next_rec;
		}

		cmp = cmp_dtuple_rec(entry, rec);

		if (cmp == 0) {
			if (row_ins_dupl_error_with_rec(rec, entry, index)) {
				/* printf("Duplicate key in index %s\n",
				     				index->name);
				dtuple_print(entry); */

				err = DB_DUPLICATE_KEY;

				trx->error_info = index;

				break;
			}
		}

		if (cmp < 0) {
			break;
		}

		ut_a(cmp == 0);
next_rec:
		moved = btr_pcur_move_to_next(&pcur, &mtr);

		if (!moved) {
			break;
		}
	}

	mtr_commit(&mtr);

	/* Restore old value */
	dtuple_set_n_fields_cmp(entry, n_fields_cmp);

	return(err);
}

/*******************************************************************
Checks if a unique key violation error would occur at an index entry
insert. Sets shared locks on possible duplicate records. Works only
for a clustered index! */
static
ulint
row_ins_duplicate_error_in_clust(
/*=============================*/
				/* out: DB_SUCCESS if no error,
				DB_DUPLICATE_KEY if error, DB_LOCK_WAIT if we
				have to wait for a lock on a possible
				duplicate record */
	btr_cur_t*	cursor,	/* in: B-tree cursor */
	dtuple_t*	entry,	/* in: entry to insert */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr)	/* in: mtr */
{
	ulint	err;
	rec_t*	rec;
	page_t*	page;
	ulint	n_unique;
	trx_t*	trx	= thr_get_trx(thr);

	UT_NOT_USED(mtr);
	
	ut_a(cursor->index->type & DICT_CLUSTERED);
	ut_ad(cursor->index->type & DICT_UNIQUE);

	/* NOTE: For unique non-clustered indexes there may be any number
	of delete marked records with the same value for the non-clustered
	index key (remember multiversioning), and which differ only in
	the row refererence part of the index record, containing the
	clustered index key fields. For such a secondary index record,
	to avoid race condition, we must FIRST do the insertion and after
	that check that the uniqueness condition is not breached! */
	
	/* NOTE: A problem is that in the B-tree node pointers on an
	upper level may match more to the entry than the actual existing
	user records on the leaf level. So, even if low_match would suggest
	that a duplicate key violation may occur, this may not be the case. */

	n_unique = dict_index_get_n_unique(cursor->index);
	
	if (cursor->low_match >= n_unique) {
		
		rec = btr_cur_get_rec(cursor);
		page = buf_frame_align(rec);

		if (rec != page_get_infimum_rec(page)) {

			/* We set a lock on the possible duplicate: this
			is needed in logical logging of MySQL to make
			sure that in roll-forward we get the same duplicate
			errors as in original execution */
		
			err = row_ins_set_shared_rec_lock(rec, cursor->index,
									thr);
			if (err != DB_SUCCESS) {
					
				return(err);
			}

			if (row_ins_dupl_error_with_rec(rec, entry,
							cursor->index)) {
				trx->error_info = cursor->index;
				
				return(DB_DUPLICATE_KEY);
			}
		}
	}

	if (cursor->up_match >= n_unique) {

		rec = page_rec_get_next(btr_cur_get_rec(cursor));
		page = buf_frame_align(rec);

		if (rec != page_get_supremum_rec(page)) {

			err = row_ins_set_shared_rec_lock(rec, cursor->index,
									thr);
			if (err != DB_SUCCESS) {
					
				return(err);
			}

			if (row_ins_dupl_error_with_rec(rec, entry,
							cursor->index)) {
				trx->error_info = cursor->index;

				return(DB_DUPLICATE_KEY);
			}
		}

		ut_a(!(cursor->index->type & DICT_CLUSTERED));
						/* This should never happen */
	}

	return(DB_SUCCESS);
}

/*******************************************************************
Checks if an index entry has long enough common prefix with an existing
record so that the intended insert of the entry must be changed to a modify of
the existing record. In the case of a clustered index, the prefix must be
n_unique fields long, and in the case of a secondary index, all fields must be
equal. */
UNIV_INLINE
ulint
row_ins_must_modify(
/*================*/
				/* out: 0 if no update, ROW_INS_PREV if
				previous should be updated; currently we
				do the search so that only the low_match
				record can match enough to the search tuple,
				not the next record */
	btr_cur_t*	cursor)	/* in: B-tree cursor */
{
	ulint	enough_match;
	rec_t*	rec;
	page_t*	page;
	
	/* NOTE: (compare to the note in row_ins_duplicate_error) Because node
	pointers on upper levels of the B-tree may match more to entry than
	to actual user records on the leaf level, we have to check if the
	candidate record is actually a user record. In a clustered index
	node pointers contain index->n_unique first fields, and in the case
	of a secondary index, all fields of the index. */

	enough_match = dict_index_get_n_unique_in_tree(cursor->index);
	
	if (cursor->low_match >= enough_match) {

		rec = btr_cur_get_rec(cursor);
		page = buf_frame_align(rec);

		if (rec != page_get_infimum_rec(page)) {

			return(ROW_INS_PREV);
		}
	}

	return(0);
}

/*******************************************************************
Tries to insert an index entry to an index. If the index is clustered
and a record with the same unique key is found, the other record is
necessarily marked deleted by a committed transaction, or a unique key
violation error occurs. The delete marked record is then updated to an
existing record, and we must write an undo log record on the delete
marked record. If the index is secondary, and a record with exactly the
same fields is found, the other record is necessarily marked deleted.
It is then unmarked. Otherwise, the entry is just inserted to the index. */

ulint
row_ins_index_entry_low(
/*====================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT, DB_FAIL
				if pessimistic retry needed, or error code */
	ulint		mode,	/* in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE,
				depending on whether we wish optimistic or
				pessimistic descent down the index tree */
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry,	/* in: index entry to insert */
	ulint*		ext_vec,/* in: array containing field numbers of
				externally stored fields in entry, or NULL */
	ulint		n_ext_vec,/* in: number of fields in ext_vec */
	que_thr_t*	thr)	/* in: query thread */
{
	btr_cur_t	cursor;		
	ulint		modify;
	rec_t*		insert_rec;
	rec_t*		rec;
	ulint		err;
	ulint		n_unique;
	big_rec_t*	big_rec		= NULL;
	mtr_t		mtr;
	
	log_free_check();

	mtr_start(&mtr);

	cursor.thr = thr;

	/* Note that we use PAGE_CUR_LE as the search mode, because then
	the function will return in both low_match and up_match of the
	cursor sensible values */
	
	btr_cur_search_to_nth_level(index, 0, entry, PAGE_CUR_LE,
					mode | BTR_INSERT, &cursor, 0, &mtr);

	if (cursor.flag == BTR_CUR_INSERT_TO_IBUF) {
		/* The insertion was made to the insert buffer already during
		the search: we are done */

		err = DB_SUCCESS;

		goto function_exit;
	}	
					
	n_unique = dict_index_get_n_unique(index);

	if (index->type & DICT_UNIQUE && (cursor.up_match >= n_unique
					 || cursor.low_match >= n_unique)) {

		if (index->type & DICT_CLUSTERED) {			 
			/* Note that the following may return also
			DB_LOCK_WAIT */

			err = row_ins_duplicate_error_in_clust(&cursor,
							entry, thr, &mtr);
			if (err != DB_SUCCESS) {

				goto function_exit;
			}
		} else {
			mtr_commit(&mtr);
			err = row_ins_scan_sec_index_for_duplicate(index,
								entry, thr);
			mtr_start(&mtr);

			if (err != DB_SUCCESS) {

				goto function_exit;
			}

			/* We did not find a duplicate and we have now
			locked with s-locks the necessary records to
			prevent any insertion of a duplicate by another
			transaction. Let us now reposition the cursor and
			continue the insertion. */
			
			btr_cur_search_to_nth_level(index, 0, entry,
					PAGE_CUR_LE, mode | BTR_INSERT,
					&cursor, 0, &mtr);
		}		
	}

	modify = row_ins_must_modify(&cursor);

	if (modify != 0) {
		/* There is already an index entry with a long enough common
		prefix, we must convert the insert into a modify of an
		existing record */

		if (modify == ROW_INS_NEXT) {
			rec = page_rec_get_next(btr_cur_get_rec(&cursor));

			btr_cur_position(index, rec, &cursor);
		}

		if (index->type & DICT_CLUSTERED) {
			err = row_ins_clust_index_entry_by_modify(mode,
							&cursor, &big_rec,
							entry,
							ext_vec, n_ext_vec,
							thr, &mtr);
		} else {
			err = row_ins_sec_index_entry_by_modify(&cursor,
								thr, &mtr);
		}
		
	} else {
		if (mode == BTR_MODIFY_LEAF) {
			err = btr_cur_optimistic_insert(0, &cursor, entry,
					&insert_rec, &big_rec, thr, &mtr);
		} else {
			ut_a(mode == BTR_MODIFY_TREE);
			err = btr_cur_pessimistic_insert(0, &cursor, entry,
					&insert_rec, &big_rec, thr, &mtr);
		}

		if (err == DB_SUCCESS) {
			if (ext_vec) {
				rec_set_field_extern_bits(insert_rec,
						ext_vec, n_ext_vec, &mtr);
			}
		}
	}

function_exit:
	mtr_commit(&mtr);

	if (big_rec) {
		mtr_start(&mtr);
	
		btr_cur_search_to_nth_level(index, 0, entry, PAGE_CUR_LE,
					BTR_MODIFY_TREE, &cursor, 0, &mtr);

		err = btr_store_big_rec_extern_fields(index,
						btr_cur_get_rec(&cursor), 
						big_rec, &mtr);
		if (modify) {
			dtuple_big_rec_free(big_rec);
		} else {
			dtuple_convert_back_big_rec(index, entry, big_rec);
		}

		mtr_commit(&mtr);
	}

	return(err);
}

/*******************************************************************
Inserts an index entry to index. Tries first optimistic, then pessimistic
descent down the tree. If the entry matches enough to a delete marked record,
performs the insert by updating or delete unmarking the delete marked
record. */

ulint
row_ins_index_entry(
/*================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT,
				DB_DUPLICATE_KEY, or some other error code */
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry,	/* in: index entry to insert */
	ulint*		ext_vec,/* in: array containing field numbers of
				externally stored fields in entry, or NULL */
	ulint		n_ext_vec,/* in: number of fields in ext_vec */
	que_thr_t*	thr)	/* in: query thread */
{
	ulint	err;

	/* Try first optimistic descent to the B-tree */

	err = row_ins_index_entry_low(BTR_MODIFY_LEAF, index, entry,
						ext_vec, n_ext_vec, thr);	
	if (err != DB_FAIL) {

		return(err);
	}

	/* Try then pessimistic descent to the B-tree */

	err = row_ins_index_entry_low(BTR_MODIFY_TREE, index, entry,
						ext_vec, n_ext_vec, thr);
	return(err);
}

/***************************************************************
Sets the values of the dtuple fields in entry from the values of appropriate
columns in row. */
UNIV_INLINE
void
row_ins_index_entry_set_vals(
/*=========================*/
	dtuple_t*	entry,	/* in: index entry to make */
	dtuple_t*	row)	/* in: row */
{
	dfield_t*	field;
	dfield_t*	row_field;
	ulint		n_fields;
	ulint		i;

	ut_ad(entry && row);

	n_fields = dtuple_get_n_fields(entry);

	for (i = 0; i < n_fields; i++) {
		field = dtuple_get_nth_field(entry, i);

		row_field = dtuple_get_nth_field(row, field->col_no);

		field->data = row_field->data;
		field->len = row_field->len;
	}
}

/***************************************************************
Inserts a single index entry to the table. */
UNIV_INLINE
ulint
row_ins_index_entry_step(
/*=====================*/
				/* out: DB_SUCCESS if operation successfully
				completed, else error code or DB_LOCK_WAIT */
	ins_node_t*	node,	/* in: row insert node */
	que_thr_t*	thr)	/* in: query thread */
{
	ulint	err;

	ut_ad(dtuple_check_typed(node->row));
	
	row_ins_index_entry_set_vals(node->entry, node->row);
	
	ut_ad(dtuple_check_typed(node->entry));

	err = row_ins_index_entry(node->index, node->entry, NULL, 0, thr);

	return(err);
}

/***************************************************************
Allocates a row id for row and inits the node->index field. */
UNIV_INLINE
void
row_ins_alloc_row_id_step(
/*======================*/
	ins_node_t*	node)	/* in: row insert node */
{
	dulint	row_id;
	
	ut_ad(node->state == INS_NODE_ALLOC_ROW_ID);
	
	if (dict_table_get_first_index(node->table)->type & DICT_UNIQUE) {

		/* No row id is stored if the clustered index is unique */

		return;
	}
	
	/* Fill in row id value to row */

	row_id = dict_sys_get_new_row_id();

	dict_sys_write_row_id(node->row_id_buf, row_id);
}

/***************************************************************
Gets a row to insert from the values list. */
UNIV_INLINE
void
row_ins_get_row_from_values(
/*========================*/
	ins_node_t*	node)	/* in: row insert node */
{
	que_node_t*	list_node;
	dfield_t*	dfield;
	dtuple_t*	row;
	ulint		i;
	
	/* The field values are copied in the buffers of the select node and
	it is safe to use them until we fetch from select again: therefore
	we can just copy the pointers */

	row = node->row; 

	i = 0;
	list_node = node->values_list;

	while (list_node) {
		eval_exp(list_node);

		dfield = dtuple_get_nth_field(row, i);
		dfield_copy_data(dfield, que_node_get_val(list_node));

		i++;
		list_node = que_node_get_next(list_node);
	}
}

/***************************************************************
Gets a row to insert from the select list. */
UNIV_INLINE
void
row_ins_get_row_from_select(
/*========================*/
	ins_node_t*	node)	/* in: row insert node */
{
	que_node_t*	list_node;
	dfield_t*	dfield;
	dtuple_t*	row;
	ulint		i;

	/* The field values are copied in the buffers of the select node and
	it is safe to use them until we fetch from select again: therefore
	we can just copy the pointers */

	row = node->row; 

	i = 0;
	list_node = node->select->select_list;

	while (list_node) {
		dfield = dtuple_get_nth_field(row, i);
		dfield_copy_data(dfield, que_node_get_val(list_node));

		i++;
		list_node = que_node_get_next(list_node);
	}
}
	
/***************************************************************
Inserts a row to a table. */

ulint
row_ins(
/*====*/
				/* out: DB_SUCCESS if operation successfully
				completed, else error code or DB_LOCK_WAIT */
	ins_node_t*	node,	/* in: row insert node */
	que_thr_t*	thr)	/* in: query thread */
{
	ulint	err;
	
	ut_ad(node && thr);

	if (node->state == INS_NODE_ALLOC_ROW_ID) {

		row_ins_alloc_row_id_step(node);
	
		node->index = dict_table_get_first_index(node->table);
		node->entry = UT_LIST_GET_FIRST(node->entry_list);

		if (node->ins_type == INS_SEARCHED) {

			row_ins_get_row_from_select(node);

		} else if (node->ins_type == INS_VALUES) {

			row_ins_get_row_from_values(node);
		}

		node->state = INS_NODE_INSERT_ENTRIES;
	}

	ut_ad(node->state == INS_NODE_INSERT_ENTRIES);

	while (node->index != NULL) {
		err = row_ins_index_entry_step(node, thr);
		
		if (err != DB_SUCCESS) {

			return(err);
		}

		node->index = dict_table_get_next_index(node->index);
		node->entry = UT_LIST_GET_NEXT(tuple_list, node->entry);
	}

	ut_ad(node->entry == NULL);
	
	node->state = INS_NODE_ALLOC_ROW_ID;
	
	return(DB_SUCCESS);
}

/***************************************************************
Inserts a row to a table. This is a high-level function used in SQL execution
graphs. */

que_thr_t*
row_ins_step(
/*=========*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr)	/* in: query thread */
{
	ins_node_t*	node;
	que_node_t*	parent;
	sel_node_t*	sel_node;
	trx_t*		trx;
	ulint		err;

	ut_ad(thr);
	
	trx = thr_get_trx(thr);

	node = thr->run_node;

	ut_ad(que_node_get_type(node) == QUE_NODE_INSERT);

	parent = que_node_get_parent(node);
	sel_node = node->select;

	if (thr->prev_node == parent) {
		node->state = INS_NODE_SET_IX_LOCK;
	}

	/* If this is the first time this node is executed (or when
	execution resumes after wait for the table IX lock), set an
	IX lock on the table and reset the possible select node. */

	if (node->state == INS_NODE_SET_IX_LOCK) {

		/* It may be that the current session has not yet started
		its transaction, or it has been committed: */
		
		trx_start_if_not_started(trx);

		if (UT_DULINT_EQ(trx->id, node->trx_id)) {
			/* No need to do IX-locking or write trx id to buf */

			goto same_trx;
		}	

		trx_write_trx_id(node->trx_id_buf, trx->id);

		err = lock_table(0, node->table, LOCK_IX, thr);

		if (err != DB_SUCCESS) {

			goto error_handling;
		}

		node->trx_id = trx->id;
	same_trx:				
		node->state = INS_NODE_ALLOC_ROW_ID;

		if (node->ins_type == INS_SEARCHED) {
			/* Reset the cursor */
			sel_node->state = SEL_NODE_OPEN;
 		
			/* Fetch a row to insert */
		
			thr->run_node = sel_node;
	
			return(thr);
		}
	}

	if ((node->ins_type == INS_SEARCHED)
				&& (sel_node->state != SEL_NODE_FETCH)) {

		ut_ad(sel_node->state == SEL_NODE_NO_MORE_ROWS);

		/* No more rows to insert */
		thr->run_node = parent;
	
		return(thr);
	}

	/* DO THE CHECKS OF THE CONSISTENCY CONSTRAINTS HERE */

	err = row_ins(node, thr);

error_handling:
	trx->error_state = err;

	if (err == DB_SUCCESS) {
		/* Ok: do nothing */

	} else if (err == DB_LOCK_WAIT) {

		return(NULL);
	} else {
		/* SQL error detected */

		return(NULL);
	}

	/* DO THE TRIGGER ACTIONS HERE */

	if (node->ins_type == INS_SEARCHED) {
		/* Fetch a row to insert */
		
		thr->run_node = sel_node;
	} else {
		thr->run_node = que_node_get_parent(node);
	}

	return(thr);
} 
