/*****************************************************************************

Copyright (c) 2014, 2016, Oracle and/or its affiliates. All Rights Reserved.

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

/******************************************************************//**
@file include gis0rtree.h
R-tree header file

Created 2013/03/27 Jimmy Yang and Allen Lai
***********************************************************************/

#ifndef gis0rtree_h
#define gis0rtree_h

#include "univ.i"

#include "data0type.h"
#include "data0types.h"
#include "dict0types.h"
#include "hash0hash.h"
#include "mem0mem.h"
#include "page0page.h"
#include "rem0types.h"
#include "row0types.h"
#include "trx0types.h"
#include "ut0vec.h"
#include "ut0wqueue.h"
#include "que0types.h"
#include "gis0geo.h"
#include "gis0type.h"
#include "btr0types.h"
#include "btr0cur.h"

/* Whether MBR 'a' contains 'b' */
#define	MBR_CONTAIN_CMP(a, b)					\
	((((b)->xmin >= (a)->xmin) && ((b)->xmax <= (a)->xmax)	\
	 && ((b)->ymin >= (a)->ymin) && ((b)->ymax <= (a)->ymax)))

/* Whether MBR 'a' equals to 'b' */
#define	MBR_EQUAL_CMP(a, b)					\
	((((b)->xmin == (a)->xmin) && ((b)->xmax == (a)->xmax))	\
	 && (((b)->ymin == (a)->ymin) && ((b)->ymax == (a)->ymax)))

/* Whether MBR 'a' intersects 'b' */
#define	MBR_INTERSECT_CMP(a, b)					\
	((((b)->xmin <= (a)->xmax) || ((b)->xmax >= (a)->xmin))	\
	 && (((b)->ymin <= (a)->ymax) || ((b)->ymax >= (a)->ymin)))

/* Whether MBR 'a' and 'b' disjoint */
#define	MBR_DISJOINT_CMP(a, b)	(!MBR_INTERSECT_CMP(a, b))

/* Whether MBR 'a' within 'b' */
#define	MBR_WITHIN_CMP(a, b)					\
	((((b)->xmin <= (a)->xmin) && ((b)->xmax >= (a)->xmax))	\
	 && (((b)->ymin <= (a)->ymin) && ((b)->ymax >= (a)->ymax)))

/* Define it for rtree search mode checking. */
#define RTREE_SEARCH_MODE(mode)					\
	(((mode) >= PAGE_CUR_CONTAIN) && ((mode <= PAGE_CUR_RTREE_GET_FATHER)))

/* Geometry data header */
#define	GEO_DATA_HEADER_SIZE	4
/**********************************************************************//**
Builds a Rtree node pointer out of a physical record and a page number.
@return own: node pointer */
dtuple_t*
rtr_index_build_node_ptr(
/*=====================*/
	const dict_index_t*	index,	/*!< in: index */
	const rtr_mbr_t*	mbr,	/*!< in: mbr of lower page */
	const rec_t*		rec,	/*!< in: record for which to build node
					pointer */
	ulint			page_no,/*!< in: page number to put in node
					pointer */
	mem_heap_t*		heap,	/*!< in: memory heap where pointer
					created */
	ulint			level);	/*!< in: level of rec in tree:
					0 means leaf level */

/*************************************************************//**
Splits an R-tree index page to halves and inserts the tuple. It is assumed
that mtr holds an x-latch to the index tree. NOTE: the tree x-latch is
released within this function! NOTE that the operation of this
function must always succeed, we cannot reverse it: therefore enough
free disk space (2 pages) must be guaranteed to be available before
this function is called.
@return inserted record */
rec_t*
rtr_page_split_and_insert(
/*======================*/
	ulint		flags,	/*!< in: undo logging and locking flags */
	btr_cur_t*	cursor,	/*!< in/out: cursor at which to insert; when the
				function returns, the cursor is positioned
				on the predecessor of the inserted record */
	ulint**		offsets,/*!< out: offsets on inserted record */
	mem_heap_t**	heap,	/*!< in/out: pointer to memory heap, or NULL */
	const dtuple_t*	tuple,	/*!< in: tuple to insert */
	ulint		n_ext,	/*!< in: number of externally stored columns */
	mtr_t*		mtr);	/*!< in: mtr */

/**************************************************************//**
Sets the child node mbr in a node pointer. */
UNIV_INLINE
void
rtr_page_cal_mbr(
/*=============*/
	const dict_index_t*	index,	/*!< in: index */
	const buf_block_t*	block,	/*!< in: buffer block */
	rtr_mbr_t*		mbr,	/*!< out: MBR encapsulates the page */
	mem_heap_t*		heap);	/*!< in: heap for the memory
					allocation */
/*************************************************************//**
Find the next matching record. This function will first exhaust
the copied record listed in the rtr_info->matches vector before
moving to next page
@return true if there is next qualified record found, otherwise(if
exhausted) false */
bool
rtr_pcur_move_to_next(
/*==================*/
	const dtuple_t*	tuple,	/*!< in: data tuple; NOTE: n_fields_cmp in
				tuple must be set so that it cannot get
				compared to the node ptr page number field! */
	page_cur_mode_t	mode,	/*!< in: cursor search mode */
	btr_pcur_t*	cursor, /*!< in: persistent cursor; NOTE that the
				function may release the page latch */
	ulint		cur_level,
				/*!< in: current level */
	mtr_t*		mtr);	/*!< in: mtr */

/**************************************************************//**
Restores the stored position of a persistent cursor bufferfixing the page */
bool
rtr_cur_restore_position_func(
/*==========================*/
	ulint		latch_mode,	/*!< in: BTR_SEARCH_LEAF, ... */
	btr_cur_t*	cursor,		/*!< in: detached persistent cursor */
	ulint		level,		/*!< in: index level */
	const char*	file,		/*!< in: file name */
	ulint		line,		/*!< in: line where called */
	mtr_t*		mtr);		/*!< in: mtr */
#define rtr_cur_restore_position(l,cur,level,mtr)		\
	rtr_cur_restore_position_func(l,cur,level,__FILE__,__LINE__,mtr)

/****************************************************************//**
Searches the right position in rtree for a page cursor. */
bool
rtr_cur_search_with_match(
/*======================*/
	const buf_block_t*	block,	/*!< in: buffer block */
	dict_index_t*		index,	/*!< in: index descriptor */
	const dtuple_t*		tuple,	/*!< in: data tuple */
	page_cur_mode_t		mode,	/*!< in: PAGE_CUR_L,
					PAGE_CUR_LE, PAGE_CUR_G, or
					PAGE_CUR_GE */
	page_cur_t*		cursor,	/*!< in/out: page cursor */
	rtr_info_t*		rtr_info);/*!< in/out: search stack */

/****************************************************************//**
Calculate the area increased for a new record
@return area increased */
double
rtr_rec_cal_increase(
/*=================*/
	const dtuple_t*	dtuple,	/*!< in: data tuple to insert, which
				cause area increase */
	const rec_t*	rec,	/*!< in: physical record which differs from
				dtuple in some of the common fields, or which
				has an equal number or more fields than
				dtuple */
	const ulint*	offsets,/*!< in: array returned by rec_get_offsets() */
	double*		area);	/*!< out: increased area */

/****************************************************************//**
Following the right link to find the proper block for insert.
@return the proper block.*/
dberr_t
rtr_ins_enlarge_mbr(
/*=================*/
	btr_cur_t*		cursor,	/*!< in: btr cursor */
	que_thr_t*		thr,	/*!< in: query thread */
	mtr_t*			mtr);	/*!< in: mtr */

/********************************************************************//**
*/
void
rtr_get_father_node(
/*================*/
	dict_index_t*	index,	/*!< in: index */
	ulint		level,	/*!< in: the tree level of search */
	const dtuple_t* tuple,	/*!< in: data tuple; NOTE: n_fields_cmp in
				tuple must be set so that it cannot get
				compared to the node ptr page number field! */
	btr_cur_t*	sea_cur,/*!< in: search cursor */
	btr_cur_t*	cursor,	/*!< in/out: tree cursor; the cursor page is
				s- or x-latched */
	ulint		page_no,/*!< in: current page no */
	mtr_t*		mtr);	/*!< in: mtr */

/**************************************************************//**
push a nonleaf index node to the search path */
UNIV_INLINE
void
rtr_non_leaf_stack_push(
/*====================*/
	rtr_node_path_t*	path,		/*!< in/out: search path */
	ulint			pageno,		/*!< in: pageno to insert */
	node_seq_t		seq_no,		/*!< in: Node sequence num */
	ulint			level,		/*!< in: index level */
	ulint			child_no,	/*!< in: child page no */
	btr_pcur_t*		cursor,		/*!< in: position cursor */
	double			mbr_inc);	/*!< in: MBR needs to be
						enlarged */

/**************************************************************//**
push a nonleaf index node to the search path for insertion */
void
rtr_non_leaf_insert_stack_push(
/*===========================*/
	dict_index_t*		index,		/*!< in: index descriptor */
	rtr_node_path_t*	path,		/*!< in/out: search path */
	ulint			level,		/*!< in: index level */
	const buf_block_t*	block,		/*!< in: block of the page */
	const rec_t*		rec,		/*!< in: positioned record */
	double			mbr_inc);	/*!< in: MBR needs to be
						enlarged */

/*****************************************************************//**
Allocates a new Split Sequence Number.
@return new SSN id */
UNIV_INLINE
node_seq_t
rtr_get_new_ssn_id(
/*===============*/
	dict_index_t*	index);		/*!< in: the index struct */

/*****************************************************************//**
Get the current Split Sequence Number.
@return current SSN id */
UNIV_INLINE
node_seq_t
rtr_get_current_ssn_id(
/*===================*/
	dict_index_t*	index);		/*!< in/out: the index struct */

/********************************************************************//**
Create a RTree search info structure */
rtr_info_t*
rtr_create_rtr_info(
/******************/
	bool		need_prdt,	/*!< in: Whether predicate lock is
					needed */
	bool		init_matches,	/*!< in: Whether to initiate the
					"matches" structure for collecting
					matched leaf records */
	btr_cur_t*	cursor,		/*!< in: tree search cursor */
	dict_index_t*	index);		/*!< in: index struct */

/********************************************************************//**
Update a btr_cur_t with rtr_info */
void
rtr_info_update_btr(
/******************/
	btr_cur_t*	cursor,		/*!< in/out: tree cursor */
	rtr_info_t*	rtr_info);	/*!< in: rtr_info to set to the
					cursor */

/********************************************************************//**
Update a btr_cur_t with rtr_info */
void
rtr_init_rtr_info(
/****************/
	rtr_info_t*	rtr_info,	/*!< in: rtr_info to set to the
					cursor */
	bool		need_prdt,	/*!< in: Whether predicate lock is
					needed */
	btr_cur_t*	cursor,		/*!< in: tree search cursor */
	dict_index_t*	index,		/*!< in: index structure */
	bool		reinit);	/*!< in: Whether this is a reinit */

/**************************************************************//**
Clean up Rtree cursor */
void
rtr_clean_rtr_info(
/*===============*/
	rtr_info_t*	rtr_info,	/*!< in: RTree search info */
	bool		free_all);	/*!< in: need to free rtr_info itself */

/****************************************************************//**
Get the bounding box content from an index record*/
void
rtr_get_mbr_from_rec(
/*=================*/
	const rec_t*	rec,	/*!< in: data tuple */
	const ulint*	offsets,/*!< in: offsets array */
	rtr_mbr_t*	mbr);	/*!< out MBR */

/****************************************************************//**
Get the bounding box content from a MBR data record */
void
rtr_get_mbr_from_tuple(
/*===================*/
	const dtuple_t*	dtuple,	/*!< in: data tuple */
	rtr_mbr*	mbr);	/*!< out: mbr to fill */

#define rtr_page_get_father_node_ptr(of,heap,sea,cur,mtr)		\
	rtr_page_get_father_node_ptr_func(of,heap,sea,cur,__FILE__,__LINE__,mtr)

/* Get the rtree page father.
@param[in]	offsets		work area for the return value
@param[in]	index		rtree index
@param[in]	block		child page in the index
@param[in]	mtr		mtr
@param[in]	sea_cur		search cursor, contains information
				about parent nodes in search
@param[in]	cursor		cursor on node pointer record,
				its page x-latched */
void
rtr_page_get_father(
	dict_index_t*	index,
	buf_block_t*	block,
	mtr_t*		mtr,
	btr_cur_t*	sea_cur,
	btr_cur_t*	cursor);

/************************************************************//**
Returns the upper level node pointer to a R-Tree page. It is assumed
that mtr holds an x-latch on the tree.
@return rec_get_offsets() of the node pointer record */
ulint*
rtr_page_get_father_node_ptr_func(
/*==============================*/
	ulint*		offsets,/*!< in: work area for the return value */
	mem_heap_t*	heap,	/*!< in: memory heap to use */
	btr_cur_t*	sea_cur,/*!< in: search cursor */
	btr_cur_t*	cursor,	/*!< in: cursor pointing to user record,
				out: cursor on node pointer record,
				its page x-latched */
	const char*	file,	/*!< in: file name */
	ulint		line,	/*!< in: line where called */
	mtr_t*		mtr);	/*!< in: mtr */


/************************************************************//**
Returns the father block to a page. It is assumed that mtr holds
an X or SX latch on the tree.
@return rec_get_offsets() of the node pointer record */
ulint*
rtr_page_get_father_block(
/*======================*/
	ulint*		offsets,/*!< in: work area for the return value */
	mem_heap_t*	heap,	/*!< in: memory heap to use */
	dict_index_t*	index,	/*!< in: b-tree index */
	buf_block_t*	block,	/*!< in: child page in the index */
	mtr_t*		mtr,	/*!< in: mtr */
	btr_cur_t*	sea_cur,/*!< in: search cursor, contains information
				about parent nodes in search */
	btr_cur_t*	cursor);/*!< out: cursor on node pointer record,
				its page x-latched */
/**************************************************************//**
Store the parent path cursor
@return number of cursor stored */
ulint
rtr_store_parent_path(
/*==================*/
	const buf_block_t*	block,	/*!< in: block of the page */
	btr_cur_t*		btr_cur,/*!< in/out: persistent cursor */
	ulint			latch_mode,
					/*!< in: latch_mode */
	ulint			level,	/*!< in: index level */
	mtr_t*			mtr);	/*!< in: mtr */

/**************************************************************//**
Initializes and opens a persistent cursor to an index tree. It should be
closed with btr_pcur_close. */
void
rtr_pcur_open_low(
/*==============*/
	dict_index_t*	index,	/*!< in: index */
	ulint		level,	/*!< in: level in the btree */
	const dtuple_t*	tuple,	/*!< in: tuple on which search done */
	page_cur_mode_t	mode,	/*!< in: PAGE_CUR_L, ...;
				NOTE that if the search is made using a unique
				prefix of a record, mode should be
				PAGE_CUR_LE, not PAGE_CUR_GE, as the latter
				may end up on the previous page from the
				record! */
	ulint		latch_mode,/*!< in: BTR_SEARCH_LEAF, ... */
	btr_pcur_t*	cursor,	/*!< in: memory buffer for persistent cursor */
	const char*	file,	/*!< in: file name */
	ulint		line,	/*!< in: line where called */
	mtr_t*		mtr);	/*!< in: mtr */

#define rtr_pcur_open(i,t,md,l,c,m)			\
	rtr_pcur_open_low(i,0,t,md,l,c,__FILE__,__LINE__,m)

struct btr_cur_t;

/*********************************************************//**
Returns the R-Tree node stored in the parent search path
@return pointer to R-Tree cursor component */
UNIV_INLINE
node_visit_t*
rtr_get_parent_node(
/*================*/
	btr_cur_t*	btr_cur,	/*!< in: persistent cursor */
	ulint		level,		/*!< in: index level of buffer page */
	ulint		is_insert);	/*!< in: whether it is insert */

/*********************************************************//**
Returns the R-Tree cursor stored in the parent search path
@return pointer to R-Tree cursor component */
UNIV_INLINE
btr_pcur_t*
rtr_get_parent_cursor(
/*==================*/
	btr_cur_t*	btr_cur,	/*!< in: persistent cursor */
	ulint		level,		/*!< in: index level of buffer page */
	ulint		is_insert);	/*!< in: whether insert operation */

/*************************************************************//**
Copy recs from a page to new_block of rtree. */
void
rtr_page_copy_rec_list_end_no_locks(
/*================================*/
	buf_block_t*	new_block,	/*!< in: index page to copy to */
	buf_block_t*	block,		/*!< in: index page of rec */
	rec_t*		rec,		/*!< in: record on page */
	dict_index_t*	index,		/*!< in: record descriptor */
	mem_heap_t*	heap,		/*!< in/out: heap memory */
	rtr_rec_move_t*	rec_move,	/*!< in: recording records moved */
	ulint		max_move,	/*!< in: num of rec to move */
	ulint*		num_moved,	/*!< out: num of rec to move */
	mtr_t*		mtr);		/*!< in: mtr */

/*************************************************************//**
Copy recs till a specified rec from a page to new_block of rtree. */
void
rtr_page_copy_rec_list_start_no_locks(
/*==================================*/
	buf_block_t*	new_block,	/*!< in: index page to copy to */
	buf_block_t*	block,		/*!< in: index page of rec */
	rec_t*		rec,		/*!< in: record on page */
	dict_index_t*	index,		/*!< in: record descriptor */
	mem_heap_t*	heap,		/*!< in/out: heap memory */
	rtr_rec_move_t*	rec_move,	/*!< in: recording records moved */
	ulint		max_move,	/*!< in: num of rec to move */
	ulint*		num_moved,	/*!< out: num of rec to move */
	mtr_t*		mtr);		/*!< in: mtr */

/****************************************************************//**
Merge 2 mbrs and update the the mbr that cursor is on. */
dberr_t
rtr_merge_and_update_mbr(
/*=====================*/
	btr_cur_t*		cursor,		/*!< in/out: cursor */
	btr_cur_t*		cursor2,	/*!< in: the other cursor */
	ulint*			offsets,	/*!< in: rec offsets */
	ulint*			offsets2,	/*!< in: rec offsets */
	page_t*			child_page,	/*!< in: the child page. */
	buf_block_t*		merge_block,	/*!< in: page to merge */
	buf_block_t*		block,		/*!< in: page be merged */
	dict_index_t*		index,		/*!< in: index */
	mtr_t*			mtr);		/*!< in: mtr */

/*************************************************************//**
Deletes on the upper level the node pointer to a page. */
void
rtr_node_ptr_delete(
/*================*/
	dict_index_t*	index,	/*!< in: index tree */
	btr_cur_t*	sea_cur,/*!< in: search cursor, contains information
				about parent nodes in search */
	buf_block_t*	block,	/*!< in: page whose node pointer is deleted */
	mtr_t*		mtr);	/*!< in: mtr */

/****************************************************************//**
Check two MBRs are identical or need to be merged */
bool
rtr_merge_mbr_changed(
/*==================*/
	btr_cur_t*	cursor,		/*!< in: cursor */
	btr_cur_t*	cursor2,	/*!< in: the other cursor */
	ulint*		offsets,	/*!< in: rec offsets */
	ulint*		offsets2,	/*!< in: rec offsets */
	rtr_mbr_t*	new_mbr,	/*!< out: MBR to update */
	buf_block_t*	merge_block,	/*!< in: page to merge */
	buf_block_t*	block,		/*!< in: page be merged */
	dict_index_t*	index);		/*!< in: index */


/**************************************************************//**
Update the mbr field of a spatial index row.
@return true if successful */
bool
rtr_update_mbr_field(
/*=================*/
	btr_cur_t*	cursor,		/*!< in: cursor pointed to rec.*/
	ulint*		offsets,	/*!< in: offsets on rec. */
	btr_cur_t*	cursor2,	/*!< in/out: cursor pointed to rec
					that should be deleted.
					this cursor is for btr_compress to
					delete the merged page's father rec.*/
	page_t*		child_page,	/*!< in: child page. */
	rtr_mbr_t*	new_mbr,	/*!< in: the new mbr. */
	rec_t*		new_rec,	/*!< in: rec to use */
	mtr_t*		mtr);		/*!< in: mtr */

/**************************************************************//**
Check whether a Rtree page is child of a parent page
@return true if there is child/parent relationship */
bool
rtr_check_same_block(
/*=================*/
	dict_index_t*	index,	/*!< in: index tree */
	btr_cur_t*	cur,	/*!< in/out: position at the parent entry
				pointing to the child if successful */
	buf_block_t*	parentb,/*!< in: parent page to check */
	buf_block_t*	childb, /*!< in: child Page */
	mem_heap_t*	heap);	/*!< in: memory heap */

/*********************************************************************//**
Sets pointer to the data and length in a field. */
UNIV_INLINE
void
rtr_write_mbr(
/*==========*/
	byte*			data,	/*!< out: data */
	const rtr_mbr_t*	mbr);	/*!< in: data */

/*********************************************************************//**
Sets pointer to the data and length in a field. */
UNIV_INLINE
void
rtr_read_mbr(
/*==========*/
	const byte*		data,	/*!< in: data */
	rtr_mbr_t*		mbr);	/*!< out: data */

/**************************************************************//**
Check whether a discarding page is in anyone's search path */
void
rtr_check_discard_page(
/*===================*/
	dict_index_t*	index,	/*!< in: index */
	btr_cur_t*	cursor,	/*!< in: cursor on the page to discard: not on
				the root page */
	buf_block_t*	block);	/*!< in: block of page to be discarded */

/********************************************************************//**
Reinitialize a RTree search info */
UNIV_INLINE
void
rtr_info_reinit_in_cursor(
/************************/
	btr_cur_t*	cursor,		/*!< in/out: tree cursor */
	dict_index_t*	index,		/*!< in: index struct */
	bool		need_prdt);	/*!< in: Whether predicate lock is
					needed */

/** Estimates the number of rows in a given area.
@param[in]	index	index
@param[in]	tuple	range tuple containing mbr, may also be empty tuple
@param[in]	mode	search mode
@return estimated number of rows */
int64_t
rtr_estimate_n_rows_in_range(
	dict_index_t*	index,
	const dtuple_t*	tuple,
	page_cur_mode_t	mode);

#ifndef UNIV_NONINL
#include "gis0rtree.ic"
#endif
#endif /*!< gis0rtree.h */
