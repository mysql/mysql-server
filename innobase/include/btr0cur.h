/******************************************************
The index tree cursor

(c) 1994-1996 Innobase Oy

Created 10/16/1994 Heikki Tuuri
*******************************************************/

#ifndef btr0cur_h
#define btr0cur_h

#include "univ.i"
#include "dict0dict.h"
#include "data0data.h"
#include "page0cur.h"
#include "btr0types.h"
#include "que0types.h"
#include "row0types.h"
#include "ha0ha.h"

/* Mode flags for btr_cur operations; these can be ORed */
#define BTR_NO_UNDO_LOG_FLAG	1	/* do no undo logging */
#define BTR_NO_LOCKING_FLAG	2	/* do no record lock checking */
#define BTR_KEEP_SYS_FLAG	4	/* sys fields will be found from the
					update vector or inserted entry */

#define BTR_CUR_ADAPT
#define BTR_CUR_HASH_ADAPT

/*************************************************************
Returns the page cursor component of a tree cursor. */
UNIV_INLINE
page_cur_t*
btr_cur_get_page_cur(
/*=================*/
				/* out: pointer to page cursor component */
	btr_cur_t*	cursor);	/* in: tree cursor */
/*************************************************************
Returns the record pointer of a tree cursor. */
UNIV_INLINE
rec_t*
btr_cur_get_rec(
/*============*/
				/* out: pointer to record */
	btr_cur_t*	cursor);	/* in: tree cursor */
/*************************************************************
Invalidates a tree cursor by setting record pointer to NULL. */
UNIV_INLINE
void
btr_cur_invalidate(
/*===============*/
	btr_cur_t*	cursor);	/* in: tree cursor */
/*************************************************************
Returns the page of a tree cursor. */
UNIV_INLINE
page_t*
btr_cur_get_page(
/*=============*/
				/* out: pointer to page */
	btr_cur_t*	cursor);	/* in: tree cursor */
/*************************************************************
Returns the tree of a cursor. */
UNIV_INLINE
dict_tree_t*
btr_cur_get_tree(
/*=============*/
				/* out: tree */
	btr_cur_t*	cursor);	/* in: tree cursor */
/*************************************************************
Positions a tree cursor at a given record. */
UNIV_INLINE
void
btr_cur_position(
/*=============*/
	dict_index_t*	index, 	/* in: index */
	rec_t*		rec,	/* in: record in tree */
	btr_cur_t*	cursor);/* in: cursor */
/************************************************************************
Searches an index tree and positions a tree cursor on a given level.
NOTE: n_fields_cmp in tuple must be set so that it cannot be compared
to node pointer page number fields on the upper levels of the tree!
Note that if mode is PAGE_CUR_LE, which is used in inserts, then
cursor->up_match and cursor->low_match both will have sensible values.
If mode is PAGE_CUR_GE, then up_match will a have a sensible value. */

void
btr_cur_search_to_nth_level(
/*========================*/
	dict_index_t*	index,	/* in: index */
	ulint		level,	/* in: the tree level of search */
	dtuple_t*	tuple,	/* in: data tuple; NOTE: n_fields_cmp in
				tuple must be set so that it cannot get
				compared to the node ptr page number field! */
	ulint		mode,	/* in: PAGE_CUR_L, ...;
				NOTE that if the search is made using a unique
				prefix of a record, mode should be PAGE_CUR_LE,
				not PAGE_CUR_GE, as the latter may end up on
				the previous page of the record! Inserts
				should always be made using PAGE_CUR_LE to
				search the position! */
	ulint		latch_mode, /* in: BTR_SEARCH_LEAF, ..., ORed with
				BTR_INSERT and BTR_ESTIMATE;
				cursor->left_page is used to store a pointer
				to the left neighbor page, in the cases
				BTR_SEARCH_PREV and BTR_MODIFY_PREV;
				NOTE that if has_search_latch
				is != 0, we maybe do not have a latch set
				on the cursor page, we assume
				the caller uses his search latch
				to protect the record! */
	btr_cur_t*	cursor, /* in/out: tree cursor; the cursor page is
				s- or x-latched, but see also above! */
	ulint		has_search_latch,/* in: latch mode the caller
				currently has on btr_search_latch:
				RW_S_LATCH, or 0 */
	mtr_t*		mtr);	/* in: mtr */
/*********************************************************************
Opens a cursor at either end of an index. */

void
btr_cur_open_at_index_side(
/*=======================*/
	ibool		from_left,	/* in: TRUE if open to the low end,
					FALSE if to the high end */
	dict_index_t*	index,		/* in: index */
	ulint		latch_mode,	/* in: latch mode */
	btr_cur_t*	cursor,		/* in: cursor */
	mtr_t*		mtr);		/* in: mtr */
/**************************************************************************
Positions a cursor at a randomly chosen position within a B-tree. */

void
btr_cur_open_at_rnd_pos(
/*====================*/
	dict_index_t*	index,		/* in: index */
	ulint		latch_mode,	/* in: BTR_SEARCH_LEAF, ... */
	btr_cur_t*	cursor,		/* in/out: B-tree cursor */
	mtr_t*		mtr);		/* in: mtr */
/*****************************************************************
Tries to perform an insert to a page in an index tree, next to cursor.
It is assumed that mtr holds an x-latch on the page. The operation does
not succeed if there is too little space on the page. If there is just
one record on the page, the insert will always succeed; this is to
prevent trying to split a page with just one record. */

ulint
btr_cur_optimistic_insert(
/*======================*/
				/* out: DB_SUCCESS, DB_WAIT_LOCK,
				DB_FAIL, or error number */
	ulint		flags,	/* in: undo logging and locking flags: if not
				zero, the parameters index and thr should be
				specified */
	btr_cur_t*	cursor,	/* in: cursor on page after which to insert;
				cursor stays valid */
	dtuple_t*	entry,	/* in: entry to insert */
	rec_t**		rec,	/* out: pointer to inserted record if
				succeed */
	big_rec_t**	big_rec,/* out: big rec vector whose fields have to
				be stored externally by the caller, or
				NULL */
	que_thr_t*	thr,	/* in: query thread or NULL */
	mtr_t*		mtr);	/* in: mtr */
/*****************************************************************
Performs an insert on a page of an index tree. It is assumed that mtr
holds an x-latch on the tree and on the cursor page. If the insert is
made on the leaf level, to avoid deadlocks, mtr must also own x-latches
to brothers of page, if those brothers exist. */

ulint
btr_cur_pessimistic_insert(
/*=======================*/
				/* out: DB_SUCCESS or error number */
	ulint		flags,	/* in: undo logging and locking flags: if not
				zero, the parameter thr should be
				specified; if no undo logging is specified,
				then the caller must have reserved enough
				free extents in the file space so that the
				insertion will certainly succeed */
	btr_cur_t*	cursor,	/* in: cursor after which to insert;
				cursor stays valid */
	dtuple_t*	entry,	/* in: entry to insert */
	rec_t**		rec,	/* out: pointer to inserted record if
				succeed */
	big_rec_t**	big_rec,/* out: big rec vector whose fields have to
				be stored externally by the caller, or
				NULL */
	que_thr_t*	thr,	/* in: query thread or NULL */
	mtr_t*		mtr);	/* in: mtr */
/*****************************************************************
Updates a secondary index record when the update causes no size
changes in its fields. The only case when this function is currently
called is that in a char field characters change to others which
are identified in the collation order. */

ulint
btr_cur_update_sec_rec_in_place(
/*============================*/
				/* out: DB_SUCCESS or error number */
	btr_cur_t*	cursor,	/* in: cursor on the record to update;
				cursor stays valid and positioned on the
				same record */
	upd_t*		update,	/* in: update vector */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr);	/* in: mtr */
/*****************************************************************
Updates a record when the update causes no size changes in its fields. */

ulint
btr_cur_update_in_place(
/*====================*/
				/* out: DB_SUCCESS or error number */
	ulint		flags,	/* in: undo logging and locking flags */
	btr_cur_t*	cursor,	/* in: cursor on the record to update;
				cursor stays valid and positioned on the
				same record */
	upd_t*		update,	/* in: update vector */
	ulint		cmpl_info,/* in: compiler info on secondary index
				updates */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr);	/* in: mtr */
/*****************************************************************
Tries to update a record on a page in an index tree. It is assumed that mtr
holds an x-latch on the page. The operation does not succeed if there is too
little space on the page or if the update would result in too empty a page,
so that tree compression is recommended. */

ulint
btr_cur_optimistic_update(
/*======================*/
				/* out: DB_SUCCESS, or DB_OVERFLOW if the
				updated record does not fit, DB_UNDERFLOW
				if the page would become too empty */
	ulint		flags,	/* in: undo logging and locking flags */
	btr_cur_t*	cursor,	/* in: cursor on the record to update;
				cursor stays valid and positioned on the
				same record */
	upd_t*		update,	/* in: update vector; this must also
				contain trx id and roll ptr fields */
	ulint		cmpl_info,/* in: compiler info on secondary index
				updates */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr);	/* in: mtr */
/*****************************************************************
Performs an update of a record on a page of a tree. It is assumed
that mtr holds an x-latch on the tree and on the cursor page. If the
update is made on the leaf level, to avoid deadlocks, mtr must also
own x-latches to brothers of page, if those brothers exist. */

ulint
btr_cur_pessimistic_update(
/*=======================*/
				/* out: DB_SUCCESS or error code */
	ulint		flags,	/* in: undo logging, locking, and rollback
				flags */
	btr_cur_t*	cursor,	/* in: cursor on the record to update */
	big_rec_t**	big_rec,/* out: big rec vector whose fields have to
				be stored externally by the caller, or NULL */
	upd_t*		update,	/* in: update vector; this is allowed also
				contain trx id and roll ptr fields, but
				the values in update vector have no effect */
	ulint		cmpl_info,/* in: compiler info on secondary index
				updates */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr);	/* in: mtr */
/***************************************************************
Marks a clustered index record deleted. Writes an undo log record to
undo log on this delete marking. Writes in the trx id field the id
of the deleting transaction, and in the roll ptr field pointer to the
undo log record created. */

ulint
btr_cur_del_mark_set_clust_rec(
/*===========================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT, or error
				number */
	ulint		flags,	/* in: undo logging and locking flags */
	btr_cur_t*	cursor,	/* in: cursor */
	ibool		val,	/* in: value to set */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr);	/* in: mtr */
/***************************************************************
Sets a secondary index record delete mark to TRUE or FALSE. */

ulint
btr_cur_del_mark_set_sec_rec(
/*=========================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT, or error
				number */
	ulint		flags,	/* in: locking flag */
	btr_cur_t*	cursor,	/* in: cursor */
	ibool		val,	/* in: value to set */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr);	/* in: mtr */
/***************************************************************
Sets a secondary index record delete mark to FALSE. This function is
only used by the insert buffer insert merge mechanism. */

void
btr_cur_del_unmark_for_ibuf(
/*========================*/
	rec_t*	rec,	/* in: record to delete unmark */
	mtr_t*	mtr);	/* in: mtr */
/*****************************************************************
Tries to compress a page of the tree on the leaf level. It is assumed
that mtr holds an x-latch on the tree and on the cursor page. To avoid
deadlocks, mtr must also own x-latches to brothers of page, if those
brothers exist. NOTE: it is assumed that the caller has reserved enough
free extents so that the compression will always succeed if done! */

void
btr_cur_compress(
/*=============*/
	btr_cur_t*	cursor,	/* in: cursor on the page to compress;
				cursor does not stay valid */
	mtr_t*		mtr);	/* in: mtr */
/*****************************************************************
Tries to compress a page of the tree if it seems useful. It is assumed
that mtr holds an x-latch on the tree and on the cursor page. To avoid
deadlocks, mtr must also own x-latches to brothers of page, if those
brothers exist. NOTE: it is assumed that the caller has reserved enough
free extents so that the compression will always succeed if done! */

ibool
btr_cur_compress_if_useful(
/*=======================*/
				/* out: TRUE if compression occurred */
	btr_cur_t*	cursor,	/* in: cursor on the page to compress;
				cursor does not stay valid if compression
				occurs */
	mtr_t*		mtr);	/* in: mtr */
/***********************************************************
Removes the record on which the tree cursor is positioned. It is assumed
that the mtr has an x-latch on the page where the cursor is positioned,
but no latch on the whole tree. */

ibool
btr_cur_optimistic_delete(
/*======================*/
				/* out: TRUE if success, i.e., the page
				did not become too empty */
	btr_cur_t*	cursor,	/* in: cursor on the record to delete;
				cursor stays valid: if deletion succeeds,
				on function exit it points to the successor
				of the deleted record */
	mtr_t*		mtr);	/* in: mtr */
/*****************************************************************
Removes the record on which the tree cursor is positioned. Tries
to compress the page if its fillfactor drops below a threshold
or if it is the only page on the level. It is assumed that mtr holds
an x-latch on the tree and on the cursor page. To avoid deadlocks,
mtr must also own x-latches to brothers of page, if those brothers
exist. */

ibool
btr_cur_pessimistic_delete(
/*=======================*/
				/* out: TRUE if compression occurred */
	ulint*		err,	/* out: DB_SUCCESS or DB_OUT_OF_FILE_SPACE;
				the latter may occur because we may have
				to update node pointers on upper levels,
				and in the case of variable length keys
				these may actually grow in size */
	ibool		has_reserved_extents, /* in: TRUE if the
				caller has already reserved enough free
				extents so that he knows that the operation
				will succeed */
	btr_cur_t*	cursor,	/* in: cursor on the record to delete;
				if compression does not occur, the cursor
				stays valid: it points to successor of
				deleted record on function exit */
	ibool		in_rollback,/* in: TRUE if called in rollback */
	mtr_t*		mtr);	/* in: mtr */
/***************************************************************
Parses a redo log record of updating a record in-place. */

byte*
btr_cur_parse_update_in_place(
/*==========================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page);	/* in: page or NULL */
/***************************************************************
Parses a redo log record of updating a record, but not in-place. */

byte*
btr_cur_parse_opt_update(
/*=====================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page,	/* in: page or NULL */
	mtr_t*	mtr);	/* in: mtr or NULL */
/********************************************************************
Parses the redo log record for delete marking or unmarking of a clustered
index record. */

byte*
btr_cur_parse_del_mark_set_clust_rec(
/*=================================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page);	/* in: page or NULL */	
/********************************************************************
Parses the redo log record for delete marking or unmarking of a secondary
index record. */

byte*
btr_cur_parse_del_mark_set_sec_rec(
/*===============================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page);	/* in: page or NULL */	
/***********************************************************************
Estimates the number of rows in a given index range. */

ulint
btr_estimate_n_rows_in_range(
/*=========================*/
				/* out: estimated number of rows */
	dict_index_t*	index,	/* in: index */
	dtuple_t*	tuple1,	/* in: range start, may also be empty tuple */
	ulint		mode1,	/* in: search mode for range start */
	dtuple_t*	tuple2,	/* in: range end, may also be empty tuple */
	ulint		mode2);	/* in: search mode for range end */
/***********************************************************************
Estimates the number of different key values in a given index, for
each n-column prefix of the index where n <= dict_index_get_n_unique(index).
The estimates are stored in the array index->stat_n_diff_key_vals. */

void
btr_estimate_number_of_different_key_vals(
/*======================================*/
	dict_index_t*	index);	/* in: index */
/***********************************************************************
Marks not updated extern fields as not-owned by this record. The ownership
is transferred to the updated record which is inserted elsewhere in the
index tree. In purge only the owner of externally stored field is allowed
to free the field. */

void
btr_cur_mark_extern_inherited_fields(
/*=================================*/
	rec_t*	rec,	/* in: record in a clustered index */
	upd_t*	update,	/* in: update vector */
	mtr_t*	mtr);	/* in: mtr */
/***********************************************************************
The complement of the previous function: in an update entry may inherit
some externally stored fields from a record. We must mark them as inherited
in entry, so that they are not freed in a rollback. */

void
btr_cur_mark_dtuple_inherited_extern(
/*=================================*/
	dtuple_t*	entry,		/* in: updated entry to be inserted to
					clustered index */
	ulint*		ext_vec,	/* in: array of extern fields in the
					original record */
	ulint		n_ext_vec,	/* in: number of elements in ext_vec */
	upd_t*		update);	/* in: update vector */
/***********************************************************************
Marks all extern fields in a record as owned by the record. This function
should be called if the delete mark of a record is removed: a not delete
marked record always owns all its extern fields. */

void
btr_cur_unmark_extern_fields(
/*=========================*/
	rec_t*	rec,	/* in: record in a clustered index */
	mtr_t*	mtr);	/* in: mtr */
/***********************************************************************
Marks all extern fields in a dtuple as owned by the record. */

void
btr_cur_unmark_dtuple_extern_fields(
/*================================*/
	dtuple_t*	entry,		/* in: clustered index entry */
	ulint*		ext_vec,	/* in: array of numbers of fields
					which have been stored externally */
	ulint		n_ext_vec);	/* in: number of elements in ext_vec */
/***********************************************************************
Stores the fields in big_rec_vec to the tablespace and puts pointers to
them in rec. The fields are stored on pages allocated from leaf node
file segment of the index tree. */

ulint
btr_store_big_rec_extern_fields(
/*============================*/
					/* out: DB_SUCCESS or error */
	dict_index_t*	index,		/* in: index of rec; the index tree
					MUST be X-latched */
	rec_t*		rec,		/* in: record */
	big_rec_t*	big_rec_vec,	/* in: vector containing fields
					to be stored externally */
	mtr_t*		local_mtr);	/* in: mtr containing the latch to
					rec and to the tree */
/***********************************************************************
Frees the space in an externally stored field to the file space
management if the field in data is owned the externally stored field,
in a rollback we may have the additional condition that the field must
not be inherited. */

void
btr_free_externally_stored_field(
/*=============================*/
	dict_index_t*	index,		/* in: index of the data, the index
					tree MUST be X-latched */
	byte*		data,		/* in: internally stored data
					+ reference to the externally
					stored part */
	ulint		local_len,	/* in: length of data */
	ibool		do_not_free_inherited,/* in: TRUE if called in a
					rollback and we do not want to free
					inherited fields */
	mtr_t*		local_mtr);	/* in: mtr containing the latch to
					data an an X-latch to the index
					tree */
/***************************************************************
Frees the externally stored fields for a record. */

void
btr_rec_free_externally_stored_fields(
/*==================================*/
	dict_index_t*	index,	/* in: index of the data, the index
				tree MUST be X-latched */
	rec_t*		rec,	/* in: record */
	ibool		do_not_free_inherited,/* in: TRUE if called in a
				rollback and we do not want to free
				inherited fields */
	mtr_t*		mtr);	/* in: mini-transaction handle which contains
				an X-latch to record page and to the index
				tree */
/***********************************************************************
Copies an externally stored field of a record to mem heap. */

byte*
btr_rec_copy_externally_stored_field(
/*=================================*/
				/* out: the field copied to heap */
	rec_t*		rec,	/* in: record */
	ulint		no,	/* in: field number */
	ulint*		len,	/* out: length of the field */
	mem_heap_t*	heap);	/* in: mem heap */
/***********************************************************************
Copies an externally stored field of a record to mem heap. Parameter
data contains a pointer to 'internally' stored part of the field:
possibly some data, and the reference to the externally stored part in
the last 20 bytes of data. */

byte*
btr_copy_externally_stored_field(
/*=============================*/
				/* out: the whole field copied to heap */
	ulint*		len,	/* out: length of the whole field */
	byte*		data,	/* in: 'internally' stored part of the
				field containing also the reference to
				the external part */
	ulint		local_len,/* in: length of data */
	mem_heap_t*	heap);	/* in: mem heap */
/***********************************************************************
Stores the positions of the fields marked as extern storage in the update
vector, and also those fields who are marked as extern storage in rec
and not mentioned in updated fields. We use this function to remember
which fields we must mark as extern storage in a record inserted for an
update. */

ulint
btr_push_update_extern_fields(
/*==========================*/
				/* out: number of values stored in ext_vect */
	ulint*	ext_vect,	/* in: array of ulints, must be preallocated
				to have place for all fields in rec */
	rec_t*	rec,		/* in: record */
	upd_t*	update);	/* in: update vector */
	

/*######################################################################*/

/* In the pessimistic delete, if the page data size drops below this
limit, merging it to a neighbor is tried */

#define BTR_CUR_PAGE_COMPRESS_LIMIT	(UNIV_PAGE_SIZE / 2)

/* A slot in the path array. We store here info on a search path down the
tree. Each slot contains data on a single level of the tree. */

typedef struct btr_path_struct	btr_path_t;
struct btr_path_struct{
	ulint	nth_rec;	/* index of the record
				where the page cursor stopped on
				this level (index in alphabetical
				order); value ULINT_UNDEFINED
				denotes array end */
	ulint	n_recs;		/* number of records on the page */
};

#define BTR_PATH_ARRAY_N_SLOTS	250	/* size of path array (in slots) */

/* The tree cursor: the definition appears here only for the compiler
to know struct size! */

struct btr_cur_struct {
	dict_index_t*	index;		/* index where positioned */
	page_cur_t	page_cur;	/* page cursor */
	page_t*		left_page;	/* this field is used to store a pointer
					to the left neighbor page, in the cases
					BTR_SEARCH_PREV and BTR_MODIFY_PREV */
	/*------------------------------*/
	que_thr_t*	thr;		/* this field is only used when
					btr_cur_search_... is called for an
					index entry insertion: the calling
					query thread is passed here to be
					used in the insert buffer */
	/*------------------------------*/
	/* The following fields are used in btr_cur_search... to pass
	information: */
	ulint		flag;		/* BTR_CUR_HASH, BTR_CUR_HASH_FAIL,
					BTR_CUR_BINARY, or
					BTR_CUR_INSERT_TO_IBUF */
	ulint		tree_height;	/* Tree height if the search is done
					for a pessimistic insert or update
					operation */
	ulint		up_match;	/* If the search mode was PAGE_CUR_LE,
					the number of matched fields to the
					the first user record to the right of
					the cursor record after
					btr_cur_search_...;
					for the mode PAGE_CUR_GE, the matched
					fields to the first user record AT THE
					CURSOR or to the right of it;
					NOTE that the up_match and low_match
					values may exceed the correct values
					for comparison to the adjacent user
					record if that record is on a
					different leaf page! (See the note in
					row_ins_duplicate_key.) */
	ulint		up_bytes;	/* number of matched bytes to the
					right at the time cursor positioned;
					only used internally in searches: not
					defined after the search */
	ulint		low_match;	/* if search mode was PAGE_CUR_LE,
					the number of matched fields to the
					first user record AT THE CURSOR or
					to the left of it after
					btr_cur_search_...;
					NOT defined for PAGE_CUR_GE or any
					other search modes; see also the NOTE
					in up_match! */
	ulint		low_bytes;	/* number of matched bytes to the
					right at the time cursor positioned;
					only used internally in searches: not
					defined after the search */
	ulint		n_fields;	/* prefix length used in a hash
					search if hash_node != NULL */
	ulint		n_bytes;	/* hash prefix bytes if hash_node !=
					NULL */
	ulint		fold;		/* fold value used in the search if
					flag is BTR_CUR_HASH */
	/*------------------------------*/
	btr_path_t*	path_arr;	/* in estimating the number of
					rows in range, we store in this array
					information of the path through
					the tree */
};

/* Values for the flag documenting the used search method */
#define BTR_CUR_HASH		1	/* successful shortcut using the hash
					index */
#define BTR_CUR_HASH_FAIL	2	/* failure using hash, success using
					binary search: the misleading hash
					reference is stored in the field
					hash_node, and might be necessary to
					update */
#define BTR_CUR_BINARY		3	/* success using the binary search */
#define BTR_CUR_INSERT_TO_IBUF	4	/* performed the intended insert to
					the insert buffer */

/* If pessimistic delete fails because of lack of file space,
there is still a good change of success a little later: try this many times,
and sleep this many microseconds in between */
#define BTR_CUR_RETRY_DELETE_N_TIMES	100
#define BTR_CUR_RETRY_SLEEP_TIME	50000

/* The reference in a field of which data is stored on a different page */
/*--------------------------------------*/
#define BTR_EXTERN_SPACE_ID		0	/* space id where stored */
#define BTR_EXTERN_PAGE_NO		4	/* page no where stored */
#define BTR_EXTERN_OFFSET		8	/* offset of BLOB header
						on that page */
#define BTR_EXTERN_LEN			12	/* 8 bytes containing the
						length of the externally
						stored part of the BLOB.
						The 2 highest bits are
						reserved to the flags below. */
/*--------------------------------------*/
#define BTR_EXTERN_FIELD_REF_SIZE	20

/* The highest bit of BTR_EXTERN_LEN (i.e., the highest bit of the byte
at lowest address) is set to 1 if this field does not 'own' the externally
stored field; only the owner field is allowed to free the field in purge!
If the 2nd highest bit is 1 then it means that the externally stored field
was inherited from an earlier version of the row. In rollback we are not
allowed to free an inherited external field. */

#define BTR_EXTERN_OWNER_FLAG		128
#define BTR_EXTERN_INHERITED_FLAG	64

extern ulint	btr_cur_n_non_sea;

#ifndef UNIV_NONINL
#include "btr0cur.ic"
#endif
				
#endif
