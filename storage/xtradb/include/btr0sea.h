/*****************************************************************************

Copyright (c) 1996, 2011, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 
51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

*****************************************************************************/

/********************************************************************//**
@file include/btr0sea.h
The index tree adaptive search

Created 2/17/1996 Heikki Tuuri
*************************************************************************/

#ifndef btr0sea_h
#define btr0sea_h

#include "univ.i"

#include "rem0rec.h"
#include "dict0dict.h"
#include "btr0types.h"
#include "mtr0mtr.h"
#include "ha0ha.h"

/*****************************************************************//**
Creates and initializes the adaptive search system at a database start. */
UNIV_INTERN
void
btr_search_sys_create(
/*==================*/
	ulint	hash_size);	/*!< in: hash index hash table size */
/*****************************************************************//**
Frees the adaptive search system at a database shutdown. */
UNIV_INTERN
void
btr_search_sys_free(void);
/*=====================*/

/********************************************************************//**
Disable the adaptive hash search system and empty the index. */
UNIV_INTERN
void
btr_search_disable(void);
/*====================*/
/********************************************************************//**
Enable the adaptive hash search system. */
UNIV_INTERN
void
btr_search_enable(void);
/*====================*/

/********************************************************************//**
Returns search info for an index.
@return	search info; search mutex reserved */
UNIV_INLINE
btr_search_t*
btr_search_get_info(
/*================*/
	dict_index_t*	index);	/*!< in: index */
/*****************************************************************//**
Creates and initializes a search info struct.
@return	own: search info struct */
UNIV_INTERN
btr_search_t*
btr_search_info_create(
/*===================*/
	mem_heap_t*	heap);	/*!< in: heap where created */
/*****************************************************************//**
Returns the value of ref_count. The value is protected by
the latch of the AHI partition corresponding to this index.
@return	ref_count value. */
UNIV_INTERN
ulint
btr_search_info_get_ref_count(
/*==========================*/
	btr_search_t*   info,	/*!< in: search info. */
	dict_index_t*	index); /*!< in: index */
/*********************************************************************//**
Updates the search info. */
UNIV_INLINE
void
btr_search_info_update(
/*===================*/
	dict_index_t*	index,	/*!< in: index of the cursor */
	btr_cur_t*	cursor);/*!< in: cursor which was just positioned */
/******************************************************************//**
Tries to guess the right search position based on the hash search info
of the index. Note that if mode is PAGE_CUR_LE, which is used in inserts,
and the function returns TRUE, then cursor->up_match and cursor->low_match
both have sensible values.
@return	TRUE if succeeded */
UNIV_INTERN
ibool
btr_search_guess_on_hash(
/*=====================*/
	dict_index_t*	index,		/*!< in: index */
	btr_search_t*	info,		/*!< in: index search info */
	const dtuple_t*	tuple,		/*!< in: logical record */
	ulint		mode,		/*!< in: PAGE_CUR_L, ... */
	ulint		latch_mode,	/*!< in: BTR_SEARCH_LEAF, ... */
	btr_cur_t*	cursor,		/*!< out: tree cursor */
	ulint		has_search_latch,/*!< in: latch mode the caller
					currently has on btr_search_latch:
					RW_S_LATCH, RW_X_LATCH, or 0 */
	mtr_t*		mtr);		/*!< in: mtr */
/********************************************************************//**
Moves or deletes hash entries for moved records. If new_page is already hashed,
then the hash index for page, if any, is dropped. If new_page is not hashed,
and page is hashed, then a new hash index is built to new_page with the same
parameters as page (this often happens when a page is split). */
UNIV_INTERN
void
btr_search_move_or_delete_hash_entries(
/*===================================*/
	buf_block_t*	new_block,	/*!< in: records are copied
					to this page */
	buf_block_t*	block,		/*!< in: index page from which
					records were copied, and the
					copied records will be deleted
					from this page */
	dict_index_t*	index);		/*!< in: record descriptor */
/********************************************************************//**
Drops a page hash index. */
UNIV_INTERN
void
btr_search_drop_page_hash_index(
/*============================*/
	buf_block_t*	block);	/*!< in: block containing index page,
				s- or x-latched, or an index page
				for which we know that
				block->buf_fix_count == 0 */
/************************************************************************
Drops a page hash index based on index */
UNIV_INTERN
void
btr_search_drop_page_hash_index_on_index(
/*=====================================*/
	dict_index_t*	index);		/* in: record descriptor */
/********************************************************************//**
Drops a possible page hash index when a page is evicted from the buffer pool
or freed in a file segment. */
UNIV_INTERN
void
btr_search_drop_page_hash_when_freed(
/*=================================*/
	ulint	space,		/*!< in: space id */
	ulint	zip_size,	/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	ulint	page_no);	/*!< in: page number */
/********************************************************************//**
Updates the page hash index when a single record is inserted on a page. */
UNIV_INTERN
void
btr_search_update_hash_node_on_insert(
/*==================================*/
	btr_cur_t*	cursor);/*!< in: cursor which was positioned to the
				place to insert using btr_cur_search_...,
				and the new record has been inserted next
				to the cursor */
/********************************************************************//**
Updates the page hash index when a single record is inserted on a page. */
UNIV_INTERN
void
btr_search_update_hash_on_insert(
/*=============================*/
	btr_cur_t*	cursor);/*!< in: cursor which was positioned to the
				place to insert using btr_cur_search_...,
				and the new record has been inserted next
				to the cursor */
/********************************************************************//**
Updates the page hash index when a single record is deleted from a page. */
UNIV_INTERN
void
btr_search_update_hash_on_delete(
/*=============================*/
	btr_cur_t*	cursor);/*!< in: cursor which was positioned on the
				record to delete using btr_cur_search_...,
				the record is not yet deleted */
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
/********************************************************************//**
Validates the search system.
@return	TRUE if ok */
UNIV_INTERN
ibool
btr_search_validate(void);
/*======================*/
#else
# define btr_search_validate()	TRUE
#endif /* defined UNIV_AHI_DEBUG || defined UNIV_DEBUG */

/********************************************************************//**
New functions to control split btr_search_index */
UNIV_INLINE
hash_table_t*
btr_search_get_hash_table(
/*======================*/
	const dict_index_t*	index)	/*!< in: index */
	__attribute__((nonnull,pure,warn_unused_result));

UNIV_INLINE
rw_lock_t*
btr_search_get_latch(
/*=================*/
	const dict_index_t*	index)	/*!< in: index */
	__attribute__((nonnull,pure,warn_unused_result));

/*********************************************************************//**
Returns the AHI partition number corresponding to a given index ID. */
UNIV_INLINE
ulint
btr_search_get_key(
/*===============*/
	index_id_t	index_id)	/*!< in: index ID */
	__attribute__((pure,warn_unused_result));

/*********************************************************************//**
Initializes AHI-related fields in a newly created index. */
UNIV_INLINE
void
btr_search_index_init(
/*===============*/
	dict_index_t*	index)	/*!< in: index */
	__attribute__((nonnull));

UNIV_INLINE
void
btr_search_x_lock_all(void);
/*========================*/

UNIV_INLINE
void
btr_search_x_unlock_all(void);
/*==========================*/

#ifdef UNIV_SYNC_DEBUG
/********************************************************************//**
Checks if the thread owns any adaptive hash latches in either S or X mode.
@return	TRUE if the thread owns at least one latch in any mode. */
UNIV_INLINE
ibool
btr_search_own_any(void)
/*=====================*/
	 __attribute__((warn_unused_result));
#endif

/** The search info struct in an index */
struct btr_search_struct{
	ulint	ref_count;	/*!< Number of blocks in this index tree
				that have search index built
				i.e. block->index points to this index.
				Protected by btr_search_latch except
				when during initialization in
				btr_search_info_create(). */

	/* @{ The following fields are not protected by any latch.
	Unfortunately, this means that they must be aligned to
	the machine word, i.e., they cannot be turned into bit-fields. */
	buf_block_t* root_guess;/*!< the root page frame when it was last time
				fetched, or NULL */
	ulint	hash_analysis;	/*!< when this exceeds
				BTR_SEARCH_HASH_ANALYSIS, the hash
				analysis starts; this is reset if no
				success noticed */
	ibool	last_hash_succ;	/*!< TRUE if the last search would have
				succeeded, or did succeed, using the hash
				index; NOTE that the value here is not exact:
				it is not calculated for every search, and the
				calculation itself is not always accurate! */
	ulint	n_hash_potential;
				/*!< number of consecutive searches
				which would have succeeded, or did succeed,
				using the hash index;
				the range is 0 .. BTR_SEARCH_BUILD_LIMIT + 5 */
	/* @} */
	/*---------------------- @{ */
	ulint	n_fields;	/*!< recommended prefix length for hash search:
				number of full fields */
	ulint	n_bytes;	/*!< recommended prefix: number of bytes in
				an incomplete field
				@see BTR_PAGE_MAX_REC_SIZE */
	ibool	left_side;	/*!< TRUE or FALSE, depending on whether
				the leftmost record of several records with
				the same prefix should be indexed in the
				hash index */
	/*---------------------- @} */
#ifdef UNIV_SEARCH_PERF_STAT
	ulint	n_hash_succ;	/*!< number of successful hash searches thus
				far */
	ulint	n_hash_fail;	/*!< number of failed hash searches */
	ulint	n_patt_succ;	/*!< number of successful pattern searches thus
				far */
	ulint	n_searches;	/*!< number of searches */
#endif /* UNIV_SEARCH_PERF_STAT */
#ifdef UNIV_DEBUG
	ulint	magic_n;	/*!< magic number @see BTR_SEARCH_MAGIC_N */
/** value of btr_search_struct::magic_n, used in assertions */
# define BTR_SEARCH_MAGIC_N	1112765
#endif /* UNIV_DEBUG */
};

/** The hash index system */
typedef struct btr_search_sys_struct	btr_search_sys_t;

/** The hash index system */
struct btr_search_sys_struct{
	hash_table_t**	hash_tables;	/*!< the array of adaptive hash index,
					tables mapping dtuple_fold values
					to rec_t pointers on index pages */
};

/** The adaptive hash index */
extern btr_search_sys_t*	btr_search_sys;

#ifdef UNIV_SEARCH_PERF_STAT
/** Number of successful adaptive hash index lookups */
extern ulint	btr_search_n_succ;
/** Number of failed adaptive hash index lookups */
extern ulint	btr_search_n_hash_fail;
#endif /* UNIV_SEARCH_PERF_STAT */

/** After change in n_fields or n_bytes in info, this many rounds are waited
before starting the hash analysis again: this is to save CPU time when there
is no hope in building a hash index. */
#define BTR_SEARCH_HASH_ANALYSIS	17

/** Limit of consecutive searches for trying a search shortcut on the search
pattern */
#define BTR_SEARCH_ON_PATTERN_LIMIT	3

/** Limit of consecutive searches for trying a search shortcut using
the hash index */
#define BTR_SEARCH_ON_HASH_LIMIT	3

/** We do this many searches before trying to keep the search latch
over calls from MySQL. If we notice someone waiting for the latch, we
again set this much timeout. This is to reduce contention. */
#define BTR_SEA_TIMEOUT			10000

#ifndef UNIV_NONINL
#include "btr0sea.ic"
#endif

#endif
