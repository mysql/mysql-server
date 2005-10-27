/******************************************************
The B-tree

(c) 1994-1996 Innobase Oy

Created 6/2/1994 Heikki Tuuri
*******************************************************/

#ifndef btr0btr_h
#define btr0btr_h

#include "univ.i"

#include "dict0dict.h"
#include "data0data.h"
#include "page0cur.h"
#include "rem0rec.h"
#include "mtr0mtr.h"
#include "btr0types.h"

/* Maximum record size which can be stored on a page, without using the
special big record storage structure */

#define	BTR_PAGE_MAX_REC_SIZE	(UNIV_PAGE_SIZE / 2 - 200)

/* Latching modes for the search function (in btr0cur.*) */
#define BTR_SEARCH_LEAF		RW_S_LATCH
#define BTR_MODIFY_LEAF		RW_X_LATCH
#define BTR_NO_LATCHES		RW_NO_LATCH
#define	BTR_MODIFY_TREE		33
#define	BTR_CONT_MODIFY_TREE	34
#define	BTR_SEARCH_PREV		35
#define	BTR_MODIFY_PREV		36

/* If this is ORed to the latch mode, it means that the search tuple will be
inserted to the index, at the searched position */
#define BTR_INSERT		512

/* This flag ORed to latch mode says that we do the search in query
optimization */
#define BTR_ESTIMATE		1024

/* This flag ORed to latch mode says that we can ignore possible
UNIQUE definition on secondary indexes when we decide if we can use the
insert buffer to speed up inserts */
#define BTR_IGNORE_SEC_UNIQUE	2048	

/******************************************************************
Gets the root node of a tree and x-latches it. */

page_t*
btr_root_get(
/*=========*/
				/* out: root page, x-latched */
	dict_tree_t*	tree,	/* in: index tree */
	mtr_t*		mtr);	/* in: mtr */
/******************************************************************
Gets a buffer page and declares its latching order level. */
UNIV_INLINE
page_t*
btr_page_get(
/*=========*/
	ulint	space,		/* in: space id */
	ulint	page_no,	/* in: page number */
	ulint	mode,		/* in: latch mode */
	mtr_t*	mtr);		/* in: mtr */
/******************************************************************
Gets the index id field of a page. */
UNIV_INLINE
dulint
btr_page_get_index_id(
/*==================*/
				/* out: index id */
	page_t*		page);	/* in: index page */
/************************************************************
Gets the node level field in an index page. */
UNIV_INLINE
ulint
btr_page_get_level_low(
/*===================*/
			/* out: level, leaf level == 0 */
	page_t*	page);	/* in: index page */
/************************************************************
Gets the node level field in an index page. */
UNIV_INLINE
ulint
btr_page_get_level(
/*===============*/
			/* out: level, leaf level == 0 */
	page_t*	page,	/* in: index page */
	mtr_t*	mtr);	/* in: mini-transaction handle */
/************************************************************
Gets the next index page number. */
UNIV_INLINE
ulint
btr_page_get_next(
/*==============*/
			/* out: next page number */
	page_t*	page,	/* in: index page */
	mtr_t*	mtr);	/* in: mini-transaction handle */
/************************************************************
Gets the previous index page number. */
UNIV_INLINE
ulint
btr_page_get_prev(
/*==============*/
			/* out: prev page number */
	page_t*	page,	/* in: index page */
	mtr_t*	mtr);	/* in: mini-transaction handle */
/*****************************************************************
Gets pointer to the previous user record in the tree. It is assumed
that the caller has appropriate latches on the page and its neighbor. */

rec_t*
btr_get_prev_user_rec(
/*==================*/
			/* out: previous user record, NULL if there is none */
	rec_t*	rec,	/* in: record on leaf level */
	mtr_t*	mtr);	/* in: mtr holding a latch on the page, and if
			needed, also to the previous page */
/*****************************************************************
Gets pointer to the next user record in the tree. It is assumed
that the caller has appropriate latches on the page and its neighbor. */

rec_t*
btr_get_next_user_rec(
/*==================*/
			/* out: next user record, NULL if there is none */
	rec_t*	rec,	/* in: record on leaf level */
	mtr_t*	mtr);	/* in: mtr holding a latch on the page, and if
			needed, also to the next page */
/******************************************************************
Releases the latch on a leaf page and bufferunfixes it. */
UNIV_INLINE
void
btr_leaf_page_release(
/*==================*/
	page_t*	page,		/* in: page */
	ulint	latch_mode,	/* in: BTR_SEARCH_LEAF or BTR_MODIFY_LEAF */
	mtr_t*	mtr);		/* in: mtr */
/******************************************************************
Gets the child node file address in a node pointer. */
UNIV_INLINE
ulint
btr_node_ptr_get_child_page_no(
/*===========================*/
			   	/* out: child node address */
	rec_t*		rec,	/* in: node pointer record */
	const ulint*	offsets);/* in: array returned by rec_get_offsets() */
/****************************************************************
Creates the root node for a new index tree. */

ulint
btr_create(
/*=======*/
			/* out: page number of the created root, FIL_NULL if
			did not succeed */
	ulint	type,	/* in: type of the index */
	ulint	space,	/* in: space where created */
	dulint	index_id,/* in: index id */
	ulint	comp,	/* in: nonzero=compact page format */
	mtr_t*	mtr);	/* in: mini-transaction handle */
/****************************************************************
Frees a B-tree except the root page, which MUST be freed after this
by calling btr_free_root. */

void
btr_free_but_not_root(
/*==================*/
	ulint	space,		/* in: space where created */
	ulint	root_page_no);	/* in: root page number */
/****************************************************************
Frees the B-tree root page. Other tree MUST already have been freed. */

void
btr_free_root(
/*==========*/
	ulint	space,		/* in: space where created */
	ulint	root_page_no,	/* in: root page number */
	mtr_t*	mtr);		/* in: a mini-transaction which has already
				been started */
/*****************************************************************
Makes tree one level higher by splitting the root, and inserts
the tuple. It is assumed that mtr contains an x-latch on the tree.
NOTE that the operation of this function must always succeed,
we cannot reverse it: therefore enough free disk space must be
guaranteed to be available before this function is called. */

rec_t*
btr_root_raise_and_insert(
/*======================*/
				/* out: inserted record */
	btr_cur_t*	cursor,	/* in: cursor at which to insert: must be
				on the root page; when the function returns,
				the cursor is positioned on the predecessor
				of the inserted record */
	dtuple_t*	tuple,	/* in: tuple to insert */
	mtr_t*		mtr);	/* in: mtr */
/*****************************************************************
Reorganizes an index page. */

void
btr_page_reorganize(
/*================*/
	page_t*		page,	/* in: page to be reorganized */
	dict_index_t*	index,	/* in: record descriptor */
	mtr_t*		mtr);	/* in: mtr */
/*****************************************************************
Decides if the page should be split at the convergence point of
inserts converging to left. */

ibool
btr_page_get_split_rec_to_left(
/*===========================*/
				/* out: TRUE if split recommended */
	btr_cur_t*	cursor,	/* in: cursor at which to insert */
	rec_t**		split_rec);/* out: if split recommended,
				the first record on upper half page,
				or NULL if tuple should be first */
/*****************************************************************
Decides if the page should be split at the convergence point of
inserts converging to right. */

ibool
btr_page_get_split_rec_to_right(
/*============================*/
				/* out: TRUE if split recommended */
	btr_cur_t*	cursor,	/* in: cursor at which to insert */
	rec_t**		split_rec);/* out: if split recommended,
				the first record on upper half page,
				or NULL if tuple should be first */
/*****************************************************************
Splits an index page to halves and inserts the tuple. It is assumed
that mtr holds an x-latch to the index tree. NOTE: the tree x-latch
is released within this function! NOTE that the operation of this
function must always succeed, we cannot reverse it: therefore
enough free disk space must be guaranteed to be available before
this function is called. */

rec_t*
btr_page_split_and_insert(
/*======================*/
				/* out: inserted record; NOTE: the tree
				x-latch is released! NOTE: 2 free disk
				pages must be available! */
	btr_cur_t*	cursor,	/* in: cursor at which to insert; when the
				function returns, the cursor is positioned
				on the predecessor of the inserted record */
	dtuple_t*	tuple,	/* in: tuple to insert */
	mtr_t*		mtr);	/* in: mtr */
/***********************************************************
Inserts a data tuple to a tree on a non-leaf level. It is assumed
that mtr holds an x-latch on the tree. */

void
btr_insert_on_non_leaf_level(
/*=========================*/
	dict_tree_t*	tree,	/* in: tree */
	ulint		level,	/* in: level, must be > 0 */
	dtuple_t*	tuple,	/* in: the record to be inserted */
	mtr_t*		mtr);	/* in: mtr */
/********************************************************************
Sets a record as the predefined minimum record. */

void
btr_set_min_rec_mark(
/*=================*/
	rec_t*		rec,	/* in/out: record */
	page_zip_des_t*	page_zip,/* in/out: compressed page with
				at least 5 bytes available, or NULL */
	mtr_t*		mtr);	/* in: mtr */
/*****************************************************************
Deletes on the upper level the node pointer to a page. */

void
btr_node_ptr_delete(
/*================*/
	dict_tree_t*	tree,	/* in: index tree */
	page_t*		page,	/* in: page whose node pointer is deleted */
	mtr_t*		mtr);	/* in: mtr */
/****************************************************************
Checks that the node pointer to a page is appropriate. */

ibool
btr_check_node_ptr(
/*===============*/
				/* out: TRUE */
	dict_tree_t*	tree,	/* in: index tree */
	page_t*		page,	/* in: index page */
	mtr_t*		mtr);	/* in: mtr */
/*****************************************************************
Tries to merge the page first to the left immediate brother if such a
brother exists, and the node pointers to the current page and to the
brother reside on the same page. If the left brother does not satisfy these
conditions, looks at the right brother. If the page is the only one on that
level lifts the records of the page to the father page, thus reducing the
tree height. It is assumed that mtr holds an x-latch on the tree and on the
page. If cursor is on the leaf level, mtr must also hold x-latches to
the brothers, if they exist. */

ibool
btr_compress(
/*=========*/
				/* out: TRUE on success */
	btr_cur_t*	cursor,	/* in: cursor on the page to merge or lift;
				the page must not be empty: in record delete
				use btr_discard_page if the page would become
				empty */
	mtr_t*		mtr);	/* in: mtr */
/*****************************************************************
Discards a page from a B-tree. This is used to remove the last record from
a B-tree page: the whole page must be removed at the same time. This cannot
be used for the root page, which is allowed to be empty. */

void
btr_discard_page(
/*=============*/
	btr_cur_t*	cursor,	/* in: cursor on the page to discard: not on
				the root page */
	mtr_t*		mtr);	/* in: mtr */
/********************************************************************
Parses the redo log record for setting an index record as the predefined
minimum record. */

byte*
btr_parse_set_min_rec_mark(
/*=======================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	ulint	comp,	/* in: nonzero=compact page format */
	page_t*	page,	/* in: page or NULL */
	mtr_t*	mtr);	/* in: mtr or NULL */
/***************************************************************
Parses a redo log record of reorganizing a page. */

byte*
btr_parse_page_reorganize(
/*======================*/
				/* out: end of log record or NULL */
	byte*		ptr,	/* in: buffer */
	byte*		end_ptr,/* in: buffer end */
	dict_index_t*	index,	/* in: record descriptor */
	page_t*		page,	/* in: page or NULL */
	mtr_t*		mtr);	/* in: mtr or NULL */
/******************************************************************
Gets the number of pages in a B-tree. */

ulint
btr_get_size(
/*=========*/
				/* out: number of pages */
	dict_index_t*	index,	/* in: index */
	ulint		flag);	/* in: BTR_N_LEAF_PAGES or BTR_TOTAL_SIZE */
/******************************************************************
Allocates a new file page to be used in an index tree. NOTE: we assume
that the caller has made the reservation for free extents! */

page_t*
btr_page_alloc(
/*===========*/
					/* out: new allocated page, x-latched;
					NULL if out of space */
	dict_tree_t*	tree,		/* in: index tree */
	ulint		hint_page_no,	/* in: hint of a good page */
	byte		file_direction,	/* in: direction where a possible
					page split is made */
	ulint		level,		/* in: level where the page is placed
					in the tree */
	mtr_t*		mtr);		/* in: mtr */
/******************************************************************
Frees a file page used in an index tree. NOTE: cannot free field external
storage pages because the page must contain info on its level. */

void
btr_page_free(
/*==========*/
	dict_tree_t*	tree,	/* in: index tree */
	page_t*		page,	/* in: page to be freed, x-latched */	
	mtr_t*		mtr);	/* in: mtr */
/******************************************************************
Frees a file page used in an index tree. Can be used also to BLOB
external storage pages, because the page level 0 can be given as an
argument. */

void
btr_page_free_low(
/*==============*/
	dict_tree_t*	tree,	/* in: index tree */
	page_t*		page,	/* in: page to be freed, x-latched */	
	ulint		level,	/* in: page level */
	mtr_t*		mtr);	/* in: mtr */
#ifdef UNIV_BTR_PRINT
/*****************************************************************
Prints size info of a B-tree. */

void
btr_print_size(
/*===========*/
	dict_tree_t*	tree);	/* in: index tree */
/******************************************************************
Prints directories and other info of all nodes in the tree. */

void
btr_print_tree(
/*===========*/
	dict_tree_t*	tree,	/* in: tree */
	ulint		width);	/* in: print this many entries from start
				and end */
#endif /* UNIV_BTR_PRINT */
/****************************************************************
Checks the size and number of fields in a record based on the definition of
the index. */

ibool
btr_index_rec_validate(
/*====================*/
					/* out: TRUE if ok */
	rec_t*		rec,		/* in: index record */
	dict_index_t*	index,		/* in: index */
	ibool		dump_on_error);	/* in: TRUE if the function
					should print hex dump of record
					and page on error */
/******************************************************************
Checks the consistency of an index tree. */

ibool
btr_validate_tree(
/*==============*/
				/* out: TRUE if ok */
	dict_tree_t*	tree,	/* in: tree */
	trx_t*		trx);	/* in: transaction or NULL */

#define BTR_N_LEAF_PAGES 	1
#define BTR_TOTAL_SIZE		2

#ifndef UNIV_NONINL
#include "btr0btr.ic"
#endif

#endif 
