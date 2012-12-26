/*****************************************************************************

Copyright (c) 1994, 2012, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2012, Facebook Inc.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file include/btr0btr.h
The B-tree

Created 6/2/1994 Heikki Tuuri
*******************************************************/

#ifndef btr0btr_h
#define btr0btr_h

#include "univ.i"

#include "dict0dict.h"
#include "data0data.h"
#include "page0cur.h"
#include "mtr0mtr.h"
#include "btr0types.h"

#ifndef UNIV_HOTBACKUP
/** Maximum record size which can be stored on a page, without using the
special big record storage structure */
#define	BTR_PAGE_MAX_REC_SIZE	(UNIV_PAGE_SIZE / 2 - 200)

/** @brief Maximum depth of a B-tree in InnoDB.

Note that this isn't a maximum as such; none of the tree operations
avoid producing trees bigger than this. It is instead a "max depth
that other code must work with", useful for e.g.  fixed-size arrays
that must store some information about each level in a tree. In other
words: if a B-tree with bigger depth than this is encountered, it is
not acceptable for it to lead to mysterious memory corruption, but it
is acceptable for the program to die with a clear assert failure. */
#define BTR_MAX_LEVELS		100

/** Latching modes for btr_cur_search_to_nth_level(). */
enum btr_latch_mode {
	/** Search a record on a leaf page and S-latch it. */
	BTR_SEARCH_LEAF = RW_S_LATCH,
	/** (Prepare to) modify a record on a leaf page and X-latch it. */
	BTR_MODIFY_LEAF	= RW_X_LATCH,
	/** Obtain no latches. */
	BTR_NO_LATCHES = RW_NO_LATCH,
	/** Start modifying the entire B-tree. */
	BTR_MODIFY_TREE = 33,
	/** Continue modifying the entire B-tree. */
	BTR_CONT_MODIFY_TREE = 34,
	/** Search the previous record. */
	BTR_SEARCH_PREV = 35,
	/** Modify the previous record. */
	BTR_MODIFY_PREV = 36
};

/* BTR_INSERT, BTR_DELETE and BTR_DELETE_MARK are mutually exclusive. */

/** If this is ORed to btr_latch_mode, it means that the search tuple
will be inserted to the index, at the searched position.
When the record is not in the buffer pool, try to use the insert buffer. */
#define BTR_INSERT		512

/** This flag ORed to btr_latch_mode says that we do the search in query
optimization */
#define BTR_ESTIMATE		1024

/** This flag ORed to BTR_INSERT says that we can ignore possible
UNIQUE definition on secondary indexes when we decide if we can use
the insert buffer to speed up inserts */
#define BTR_IGNORE_SEC_UNIQUE	2048

/** Try to delete mark the record at the searched position using the
insert/delete buffer when the record is not in the buffer pool. */
#define BTR_DELETE_MARK		4096

/** Try to purge the record at the searched position using the insert/delete
buffer when the record is not in the buffer pool. */
#define BTR_DELETE		8192

/** In the case of BTR_SEARCH_LEAF or BTR_MODIFY_LEAF, the caller is
already holding an S latch on the index tree */
#define BTR_ALREADY_S_LATCHED	16384

#define BTR_LATCH_MODE_WITHOUT_FLAGS(latch_mode)	\
	((latch_mode) & ~(BTR_INSERT			\
			  | BTR_DELETE_MARK		\
			  | BTR_DELETE			\
			  | BTR_ESTIMATE		\
			  | BTR_IGNORE_SEC_UNIQUE	\
			  | BTR_ALREADY_S_LATCHED))
#endif /* UNIV_HOTBACKUP */

/**************************************************************//**
Report that an index page is corrupted. */
UNIV_INTERN
void
btr_corruption_report(
/*==================*/
	const buf_block_t*	block,	/*!< in: corrupted block */
	const dict_index_t*	index)	/*!< in: index tree */
	UNIV_COLD __attribute__((nonnull));

/** Assert that a B-tree page is not corrupted.
@param block buffer block containing a B-tree page
@param index the B-tree index */
#define btr_assert_not_corrupted(block, index)			\
	if ((ibool) !!page_is_comp(buf_block_get_frame(block))	\
	    != dict_table_is_comp((index)->table)) {		\
		btr_corruption_report(block, index);		\
		ut_error;					\
	}

#ifndef UNIV_HOTBACKUP
#ifdef UNIV_BLOB_DEBUG
# include "ut0rbt.h"
/** An index->blobs entry for keeping track of off-page column references */
struct btr_blob_dbg_t
{
	unsigned	blob_page_no:32;	/*!< first BLOB page number */
	unsigned	ref_page_no:32;		/*!< referring page number */
	unsigned	ref_heap_no:16;		/*!< referring heap number */
	unsigned	ref_field_no:10;	/*!< referring field number */
	unsigned	owner:1;		/*!< TRUE if BLOB owner */
	unsigned	always_owner:1;		/*!< TRUE if always
						has been the BLOB owner;
						reset to TRUE on B-tree
						page splits and merges */
	unsigned	del:1;			/*!< TRUE if currently
						delete-marked */
};

/**************************************************************//**
Add a reference to an off-page column to the index->blobs map. */
UNIV_INTERN
void
btr_blob_dbg_add_blob(
/*==================*/
	const rec_t*	rec,		/*!< in: clustered index record */
	ulint		field_no,	/*!< in: number of off-page column */
	ulint		page_no,	/*!< in: start page of the column */
	dict_index_t*	index,		/*!< in/out: index tree */
	const char*	ctx)		/*!< in: context (for logging) */
	__attribute__((nonnull));
/**************************************************************//**
Display the references to off-page columns.
This function is to be called from a debugger,
for example when a breakpoint on ut_dbg_assertion_failed is hit. */
UNIV_INTERN
void
btr_blob_dbg_print(
/*===============*/
	const dict_index_t*	index)	/*!< in: index tree */
	__attribute__((nonnull));
/**************************************************************//**
Check that there are no references to off-page columns from or to
the given page. Invoked when freeing or clearing a page.
@return TRUE when no orphan references exist */
UNIV_INTERN
ibool
btr_blob_dbg_is_empty(
/*==================*/
	dict_index_t*	index,		/*!< in: index */
	ulint		page_no)	/*!< in: page number */
	__attribute__((nonnull, warn_unused_result));

/**************************************************************//**
Modify the 'deleted' flag of a record. */
UNIV_INTERN
void
btr_blob_dbg_set_deleted_flag(
/*==========================*/
	const rec_t*		rec,	/*!< in: record */
	dict_index_t*		index,	/*!< in/out: index */
	const ulint*		offsets,/*!< in: rec_get_offs(rec, index) */
	ibool			del)	/*!< in: TRUE=deleted, FALSE=exists */
	__attribute__((nonnull));
/**************************************************************//**
Change the ownership of an off-page column. */
UNIV_INTERN
void
btr_blob_dbg_owner(
/*===============*/
	const rec_t*		rec,	/*!< in: record */
	dict_index_t*		index,	/*!< in/out: index */
	const ulint*		offsets,/*!< in: rec_get_offs(rec, index) */
	ulint			i,	/*!< in: ith field in rec */
	ibool			own)	/*!< in: TRUE=owned, FALSE=disowned */
	__attribute__((nonnull));
/** Assert that there are no BLOB references to or from the given page. */
# define btr_blob_dbg_assert_empty(index, page_no)	\
	ut_a(btr_blob_dbg_is_empty(index, page_no))
#else /* UNIV_BLOB_DEBUG */
# define btr_blob_dbg_add_blob(rec, field_no, page, index, ctx)	((void) 0)
# define btr_blob_dbg_set_deleted_flag(rec, index, offsets, del)((void) 0)
# define btr_blob_dbg_owner(rec, index, offsets, i, val)	((void) 0)
# define btr_blob_dbg_assert_empty(index, page_no)		((void) 0)
#endif /* UNIV_BLOB_DEBUG */

/**************************************************************//**
Gets the root node of a tree and x-latches it.
@return	root page, x-latched */
UNIV_INTERN
page_t*
btr_root_get(
/*=========*/
	const dict_index_t*	index,	/*!< in: index tree */
	mtr_t*			mtr)	/*!< in: mtr */
	__attribute__((nonnull));

/**************************************************************//**
Checks and adjusts the root node of a tree during IMPORT TABLESPACE.
@return error code, or DB_SUCCESS */
UNIV_INTERN
dberr_t
btr_root_adjust_on_import(
/*======================*/
	const dict_index_t*	index)	/*!< in: index tree */
	__attribute__((nonnull, warn_unused_result));

/**************************************************************//**
Gets the height of the B-tree (the level of the root, when the leaf
level is assumed to be 0). The caller must hold an S or X latch on
the index.
@return	tree height (level of the root) */
UNIV_INTERN
ulint
btr_height_get(
/*===========*/
	dict_index_t*	index,	/*!< in: index tree */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
	__attribute__((nonnull, warn_unused_result));
/**************************************************************//**
Gets a buffer page and declares its latching order level. */
UNIV_INLINE
buf_block_t*
btr_block_get_func(
/*===============*/
	ulint		space,		/*!< in: space id */
	ulint		zip_size,	/*!< in: compressed page size in bytes
					or 0 for uncompressed pages */
	ulint		page_no,	/*!< in: page number */
	ulint		mode,		/*!< in: latch mode */
	const char*	file,		/*!< in: file name */
	ulint		line,		/*!< in: line where called */
# ifdef UNIV_SYNC_DEBUG
	const dict_index_t*	index,	/*!< in: index tree, may be NULL
					if it is not an insert buffer tree */
# endif /* UNIV_SYNC_DEBUG */
	mtr_t*		mtr);		/*!< in/out: mini-transaction */
# ifdef UNIV_SYNC_DEBUG
/** Gets a buffer page and declares its latching order level.
@param space	tablespace identifier
@param zip_size	compressed page size in bytes or 0 for uncompressed pages
@param page_no	page number
@param mode	latch mode
@param index	index tree, may be NULL if not the insert buffer tree
@param mtr	mini-transaction handle
@return the block descriptor */
#  define btr_block_get(space,zip_size,page_no,mode,index,mtr)	\
	btr_block_get_func(space,zip_size,page_no,mode,		\
			   __FILE__,__LINE__,index,mtr)
# else /* UNIV_SYNC_DEBUG */
/** Gets a buffer page and declares its latching order level.
@param space	tablespace identifier
@param zip_size	compressed page size in bytes or 0 for uncompressed pages
@param page_no	page number
@param mode	latch mode
@param idx	index tree, may be NULL if not the insert buffer tree
@param mtr	mini-transaction handle
@return the block descriptor */
#  define btr_block_get(space,zip_size,page_no,mode,idx,mtr)		\
	btr_block_get_func(space,zip_size,page_no,mode,__FILE__,__LINE__,mtr)
# endif /* UNIV_SYNC_DEBUG */
/** Gets a buffer page and declares its latching order level.
@param space	tablespace identifier
@param zip_size	compressed page size in bytes or 0 for uncompressed pages
@param page_no	page number
@param mode	latch mode
@param idx	index tree, may be NULL if not the insert buffer tree
@param mtr	mini-transaction handle
@return the uncompressed page frame */
# define btr_page_get(space,zip_size,page_no,mode,idx,mtr)		\
	buf_block_get_frame(btr_block_get(space,zip_size,page_no,mode,idx,mtr))
#endif /* !UNIV_HOTBACKUP */
/**************************************************************//**
Gets the index id field of a page.
@return	index id */
UNIV_INLINE
index_id_t
btr_page_get_index_id(
/*==================*/
	const page_t*	page)	/*!< in: index page */
	__attribute__((nonnull, pure, warn_unused_result));
#ifndef UNIV_HOTBACKUP
/********************************************************//**
Gets the node level field in an index page.
@return	level, leaf level == 0 */
UNIV_INLINE
ulint
btr_page_get_level_low(
/*===================*/
	const page_t*	page)	/*!< in: index page */
	__attribute__((nonnull, pure, warn_unused_result));
#define btr_page_get_level(page, mtr) btr_page_get_level_low(page)
/********************************************************//**
Gets the next index page number.
@return	next page number */
UNIV_INLINE
ulint
btr_page_get_next(
/*==============*/
	const page_t*	page,	/*!< in: index page */
	mtr_t*		mtr)	/*!< in: mini-transaction handle */
	__attribute__((nonnull, warn_unused_result));
/********************************************************//**
Gets the previous index page number.
@return	prev page number */
UNIV_INLINE
ulint
btr_page_get_prev(
/*==============*/
	const page_t*	page,	/*!< in: index page */
	mtr_t*		mtr)	/*!< in: mini-transaction handle */
	__attribute__((nonnull, warn_unused_result));
/*************************************************************//**
Gets pointer to the previous user record in the tree. It is assumed
that the caller has appropriate latches on the page and its neighbor.
@return	previous user record, NULL if there is none */
UNIV_INTERN
rec_t*
btr_get_prev_user_rec(
/*==================*/
	rec_t*	rec,	/*!< in: record on leaf level */
	mtr_t*	mtr)	/*!< in: mtr holding a latch on the page, and if
			needed, also to the previous page */
	__attribute__((nonnull, warn_unused_result));
/*************************************************************//**
Gets pointer to the next user record in the tree. It is assumed
that the caller has appropriate latches on the page and its neighbor.
@return	next user record, NULL if there is none */
UNIV_INTERN
rec_t*
btr_get_next_user_rec(
/*==================*/
	rec_t*	rec,	/*!< in: record on leaf level */
	mtr_t*	mtr)	/*!< in: mtr holding a latch on the page, and if
			needed, also to the next page */
	__attribute__((nonnull, warn_unused_result));
/**************************************************************//**
Releases the latch on a leaf page and bufferunfixes it. */
UNIV_INLINE
void
btr_leaf_page_release(
/*==================*/
	buf_block_t*	block,		/*!< in: buffer block */
	ulint		latch_mode,	/*!< in: BTR_SEARCH_LEAF or
					BTR_MODIFY_LEAF */
	mtr_t*		mtr)		/*!< in: mtr */
	__attribute__((nonnull));
/**************************************************************//**
Gets the child node file address in a node pointer.
NOTE: the offsets array must contain all offsets for the record since
we read the last field according to offsets and assume that it contains
the child page number. In other words offsets must have been retrieved
with rec_get_offsets(n_fields=ULINT_UNDEFINED).
@return	child node address */
UNIV_INLINE
ulint
btr_node_ptr_get_child_page_no(
/*===========================*/
	const rec_t*	rec,	/*!< in: node pointer record */
	const ulint*	offsets)/*!< in: array returned by rec_get_offsets() */
	__attribute__((nonnull, pure, warn_unused_result));
/** The information is used for creating a new index tree when
applying MLOG_FILE_TRUNCATE redo record during recovery */
struct btr_create_t {
	ulint		format_flags;	/*!< page format */
	ulint		n_fields;	/*!< number of index fields */
	ulint		field_len;	/*!< the length of index field */
	const byte*	fields;		/*!< index field information */
};
/************************************************************//**
Creates the root node for a new index tree.
@return	page number of the created root, FIL_NULL if did not succeed */
UNIV_INTERN
ulint
btr_create(
/*=======*/
	ulint			type,		/*!< in: type of the index */
	ulint			space,		/*!< in: space where created */
	ulint			zip_size,	/*!< in: compressed page size
						in bytes or 0 for uncompressed
						pages */
	index_id_t		index_id,	/*!< in: index id */
	dict_index_t*		index,		/*!< in: index */
	const btr_create_t*	btr_create_info,/*!< in: used for applying
						MLOG_FILE_TRUNCATE redo record
						during recovery */
	mtr_t*			mtr);		/*!< in: mini-transaction handle */
/************************************************************//**
Frees a B-tree except the root page, which MUST be freed after this
by calling btr_free_root. */
UNIV_INTERN
void
btr_free_but_not_root(
/*==================*/
	ulint	space,		/*!< in: space where created */
	ulint	zip_size,	/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	ulint	root_page_no);	/*!< in: root page number */
/************************************************************//**
Frees the B-tree root page. Other tree MUST already have been freed. */
UNIV_INTERN
void
btr_free_root(
/*==========*/
	ulint	space,		/*!< in: space where created */
	ulint	zip_size,	/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	ulint	root_page_no,	/*!< in: root page number */
	mtr_t*	mtr)		/*!< in/out: mini-transaction */
	__attribute__((nonnull));
/*************************************************************//**
Makes tree one level higher by splitting the root, and inserts
the tuple. It is assumed that mtr contains an x-latch on the tree.
NOTE that the operation of this function must always succeed,
we cannot reverse it: therefore enough free disk space must be
guaranteed to be available before this function is called.
@return	inserted record */
UNIV_INTERN
rec_t*
btr_root_raise_and_insert(
/*======================*/
	ulint		flags,	/*!< in: undo logging and locking flags */
	btr_cur_t*	cursor,	/*!< in: cursor at which to insert: must be
				on the root page; when the function returns,
				the cursor is positioned on the predecessor
				of the inserted record */
	ulint**		offsets,/*!< out: offsets on inserted record */
	mem_heap_t**	heap,	/*!< in/out: pointer to memory heap
				that can be emptied, or NULL */
	const dtuple_t*	tuple,	/*!< in: tuple to insert */
	ulint		n_ext,	/*!< in: number of externally stored columns */
	mtr_t*		mtr)	/*!< in: mtr */
	__attribute__((nonnull, warn_unused_result));
/*************************************************************//**
Reorganizes an index page.
IMPORTANT: if btr_page_reorganize() is invoked on a compressed leaf
page of a non-clustered index, the caller must update the insert
buffer free bits in the same mini-transaction in such a way that the
modification will be redo-logged.
@return	TRUE on success, FALSE on failure */
UNIV_INTERN
ibool
btr_page_reorganize(
/*================*/
	buf_block_t*	block,	/*!< in: page to be reorganized */
	dict_index_t*	index,	/*!< in: record descriptor */
	mtr_t*		mtr)	/*!< in: mtr */
	__attribute__((nonnull));
/*************************************************************//**
Decides if the page should be split at the convergence point of
inserts converging to left.
@return	TRUE if split recommended */
UNIV_INTERN
ibool
btr_page_get_split_rec_to_left(
/*===========================*/
	btr_cur_t*	cursor,	/*!< in: cursor at which to insert */
	rec_t**		split_rec)/*!< out: if split recommended,
				the first record on upper half page,
				or NULL if tuple should be first */
	__attribute__((nonnull, warn_unused_result));
/*************************************************************//**
Decides if the page should be split at the convergence point of
inserts converging to right.
@return	TRUE if split recommended */
UNIV_INTERN
ibool
btr_page_get_split_rec_to_right(
/*============================*/
	btr_cur_t*	cursor,	/*!< in: cursor at which to insert */
	rec_t**		split_rec)/*!< out: if split recommended,
				the first record on upper half page,
				or NULL if tuple should be first */
	__attribute__((nonnull, warn_unused_result));
/*************************************************************//**
Splits an index page to halves and inserts the tuple. It is assumed
that mtr holds an x-latch to the index tree. NOTE: the tree x-latch is
released within this function! NOTE that the operation of this
function must always succeed, we cannot reverse it: therefore enough
free disk space (2 pages) must be guaranteed to be available before
this function is called.

@return inserted record */
UNIV_INTERN
rec_t*
btr_page_split_and_insert(
/*======================*/
	ulint		flags,	/*!< in: undo logging and locking flags */
	btr_cur_t*	cursor,	/*!< in: cursor at which to insert; when the
				function returns, the cursor is positioned
				on the predecessor of the inserted record */
	ulint**		offsets,/*!< out: offsets on inserted record */
	mem_heap_t**	heap,	/*!< in/out: pointer to memory heap
				that can be emptied, or NULL */
	const dtuple_t*	tuple,	/*!< in: tuple to insert */
	ulint		n_ext,	/*!< in: number of externally stored columns */
	mtr_t*		mtr)	/*!< in: mtr */
	__attribute__((nonnull, warn_unused_result));
/*******************************************************//**
Inserts a data tuple to a tree on a non-leaf level. It is assumed
that mtr holds an x-latch on the tree. */
UNIV_INTERN
void
btr_insert_on_non_leaf_level_func(
/*==============================*/
	ulint		flags,	/*!< in: undo logging and locking flags */
	dict_index_t*	index,	/*!< in: index */
	ulint		level,	/*!< in: level, must be > 0 */
	dtuple_t*	tuple,	/*!< in: the record to be inserted */
	const char*	file,	/*!< in: file name */
	ulint		line,	/*!< in: line where called */
	mtr_t*		mtr)	/*!< in: mtr */
	__attribute__((nonnull));
# define btr_insert_on_non_leaf_level(f,i,l,t,m)			\
	btr_insert_on_non_leaf_level_func(f,i,l,t,__FILE__,__LINE__,m)
#endif /* !UNIV_HOTBACKUP */
/****************************************************************//**
Sets a record as the predefined minimum record. */
UNIV_INTERN
void
btr_set_min_rec_mark(
/*=================*/
	rec_t*	rec,	/*!< in/out: record */
	mtr_t*	mtr)	/*!< in: mtr */
	__attribute__((nonnull));
#ifndef UNIV_HOTBACKUP
/*************************************************************//**
Deletes on the upper level the node pointer to a page. */
UNIV_INTERN
void
btr_node_ptr_delete(
/*================*/
	dict_index_t*	index,	/*!< in: index tree */
	buf_block_t*	block,	/*!< in: page whose node pointer is deleted */
	mtr_t*		mtr)	/*!< in: mtr */
	__attribute__((nonnull));
#ifdef UNIV_DEBUG
/************************************************************//**
Checks that the node pointer to a page is appropriate.
@return	TRUE */
UNIV_INTERN
ibool
btr_check_node_ptr(
/*===============*/
	dict_index_t*	index,	/*!< in: index tree */
	buf_block_t*	block,	/*!< in: index page */
	mtr_t*		mtr)	/*!< in: mtr */
	__attribute__((nonnull, warn_unused_result));
#endif /* UNIV_DEBUG */
/*************************************************************//**
Tries to merge the page first to the left immediate brother if such a
brother exists, and the node pointers to the current page and to the
brother reside on the same page. If the left brother does not satisfy these
conditions, looks at the right brother. If the page is the only one on that
level lifts the records of the page to the father page, thus reducing the
tree height. It is assumed that mtr holds an x-latch on the tree and on the
page. If cursor is on the leaf level, mtr must also hold x-latches to
the brothers, if they exist.
@return	TRUE on success */
UNIV_INTERN
ibool
btr_compress(
/*=========*/
	btr_cur_t*	cursor,	/*!< in/out: cursor on the page to merge
				or lift; the page must not be empty:
				when deleting records, use btr_discard_page()
				if the page would become empty */
	ibool		adjust,	/*!< in: TRUE if should adjust the
				cursor position even if compression occurs */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
	__attribute__((nonnull));
/*************************************************************//**
Discards a page from a B-tree. This is used to remove the last record from
a B-tree page: the whole page must be removed at the same time. This cannot
be used for the root page, which is allowed to be empty. */
UNIV_INTERN
void
btr_discard_page(
/*=============*/
	btr_cur_t*	cursor,	/*!< in: cursor on the page to discard: not on
				the root page */
	mtr_t*		mtr)	/*!< in: mtr */
	__attribute__((nonnull));
#endif /* !UNIV_HOTBACKUP */
/****************************************************************//**
Parses the redo log record for setting an index record as the predefined
minimum record.
@return	end of log record or NULL */
UNIV_INTERN
byte*
btr_parse_set_min_rec_mark(
/*=======================*/
	byte*	ptr,	/*!< in: buffer */
	byte*	end_ptr,/*!< in: buffer end */
	ulint	comp,	/*!< in: nonzero=compact page format */
	page_t*	page,	/*!< in: page or NULL */
	mtr_t*	mtr)	/*!< in: mtr or NULL */
	__attribute__((nonnull(1,2), warn_unused_result));
/***********************************************************//**
Parses a redo log record of reorganizing a page.
@return	end of log record or NULL */
UNIV_INTERN
byte*
btr_parse_page_reorganize(
/*======================*/
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr,/*!< in: buffer end */
	dict_index_t*	index,	/*!< in: record descriptor */
	bool		compressed,/*!< in: true if compressed page */
	buf_block_t*	block,	/*!< in: page to be reorganized, or NULL */
	mtr_t*		mtr)	/*!< in: mtr or NULL */
	__attribute__((nonnull(1,2,3), warn_unused_result));
#ifndef UNIV_HOTBACKUP
/**************************************************************//**
Gets the number of pages in a B-tree.
@return	number of pages, or ULINT_UNDEFINED if the index is unavailable */
UNIV_INTERN
ulint
btr_get_size(
/*=========*/
	dict_index_t*	index,	/*!< in: index */
	ulint		flag,	/*!< in: BTR_N_LEAF_PAGES or BTR_TOTAL_SIZE */
	mtr_t*		mtr)	/*!< in/out: mini-transaction where index
				is s-latched */
	__attribute__((nonnull, warn_unused_result));
/**************************************************************//**
Allocates a new file page to be used in an index tree. NOTE: we assume
that the caller has made the reservation for free extents!
@retval NULL if no page could be allocated
@retval block, rw_lock_x_lock_count(&block->lock) == 1 if allocation succeeded
(init_mtr == mtr, or the page was not previously freed in mtr)
@retval block (not allocated or initialized) otherwise */
UNIV_INTERN
buf_block_t*
btr_page_alloc(
/*===========*/
	dict_index_t*	index,		/*!< in: index tree */
	ulint		hint_page_no,	/*!< in: hint of a good page */
	byte		file_direction,	/*!< in: direction where a possible
					page split is made */
	ulint		level,		/*!< in: level where the page is placed
					in the tree */
	mtr_t*		mtr,		/*!< in/out: mini-transaction
					for the allocation */
	mtr_t*		init_mtr)	/*!< in/out: mini-transaction
					for x-latching and initializing
					the page */
	__attribute__((nonnull, warn_unused_result));
/**************************************************************//**
Frees a file page used in an index tree. NOTE: cannot free field external
storage pages because the page must contain info on its level. */
UNIV_INTERN
void
btr_page_free(
/*==========*/
	dict_index_t*	index,	/*!< in: index tree */
	buf_block_t*	block,	/*!< in: block to be freed, x-latched */
	mtr_t*		mtr)	/*!< in: mtr */
	__attribute__((nonnull));
/**************************************************************//**
Frees a file page used in an index tree. Can be used also to BLOB
external storage pages, because the page level 0 can be given as an
argument. */
UNIV_INTERN
void
btr_page_free_low(
/*==============*/
	dict_index_t*	index,	/*!< in: index tree */
	buf_block_t*	block,	/*!< in: block to be freed, x-latched */
	ulint		level,	/*!< in: page level */
	mtr_t*		mtr)	/*!< in: mtr */
	__attribute__((nonnull));
#ifdef UNIV_BTR_PRINT
/*************************************************************//**
Prints size info of a B-tree. */
UNIV_INTERN
void
btr_print_size(
/*===========*/
	dict_index_t*	index)	/*!< in: index tree */
	__attribute__((nonnull));
/**************************************************************//**
Prints directories and other info of all nodes in the index. */
UNIV_INTERN
void
btr_print_index(
/*============*/
	dict_index_t*	index,	/*!< in: index */
	ulint		width)	/*!< in: print this many entries from start
				and end */
	__attribute__((nonnull));
#endif /* UNIV_BTR_PRINT */
/************************************************************//**
Checks the size and number of fields in a record based on the definition of
the index.
@return	TRUE if ok */
UNIV_INTERN
ibool
btr_index_rec_validate(
/*===================*/
	const rec_t*		rec,		/*!< in: index record */
	const dict_index_t*	index,		/*!< in: index */
	ibool			dump_on_error)	/*!< in: TRUE if the function
						should print hex dump of record
						and page on error */
	__attribute__((nonnull, warn_unused_result));
/**************************************************************//**
Checks the consistency of an index tree.
@return	TRUE if ok */
UNIV_INTERN
bool
btr_validate_index(
/*===============*/
	dict_index_t*	index,			/*!< in: index */
	const trx_t*	trx)			/*!< in: transaction or 0 */
	__attribute__((nonnull(1), warn_unused_result));

#define BTR_N_LEAF_PAGES	1
#define BTR_TOTAL_SIZE		2
#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_NONINL
#include "btr0btr.ic"
#endif

#endif
