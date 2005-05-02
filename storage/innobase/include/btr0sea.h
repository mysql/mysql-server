/************************************************************************
The index tree adaptive search

(c) 1996 Innobase Oy

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

/*********************************************************************
Creates and initializes the adaptive search system at a database start. */

void
btr_search_sys_create(
/*==================*/
	ulint	hash_size);	/* in: hash index hash table size */
/************************************************************************
Returns search info for an index. */
UNIV_INLINE
btr_search_t*
btr_search_get_info(
/*================*/
				/* out: search info; search mutex reserved */
	dict_index_t*	index);	/* in: index */
/*********************************************************************
Creates and initializes a search info struct. */

btr_search_t*
btr_search_info_create(
/*===================*/
				/* out, own: search info struct */
	mem_heap_t*	heap);	/* in: heap where created */
/*************************************************************************
Updates the search info. */
UNIV_INLINE
void
btr_search_info_update(
/*===================*/
	dict_index_t*	index,	/* in: index of the cursor */
	btr_cur_t*	cursor);/* in: cursor which was just positioned */
/**********************************************************************
Tries to guess the right search position based on the hash search info
of the index. Note that if mode is PAGE_CUR_LE, which is used in inserts,
and the function returns TRUE, then cursor->up_match and cursor->low_match
both have sensible values. */

ibool
btr_search_guess_on_hash(
/*=====================*/
					/* out: TRUE if succeeded */	
	dict_index_t*	index,		/* in: index */
	btr_search_t*	info,		/* in: index search info */
	dtuple_t*	tuple,		/* in: logical record */
	ulint		mode,		/* in: PAGE_CUR_L, ... */
	ulint		latch_mode, 	/* in: BTR_SEARCH_LEAF, ... */
	btr_cur_t*	cursor, 	/* out: tree cursor */
	ulint		has_search_latch,/* in: latch mode the caller
					currently has on btr_search_latch:
					RW_S_LATCH, RW_X_LATCH, or 0 */
	mtr_t*		mtr);		/* in: mtr */
/************************************************************************
Moves or deletes hash entries for moved records. If new_page is already hashed,
then the hash index for page, if any, is dropped. If new_page is not hashed,
and page is hashed, then a new hash index is built to new_page with the same
parameters as page (this often happens when a page is split). */

void
btr_search_move_or_delete_hash_entries(
/*===================================*/
	page_t*		new_page,	/* in: records are copied
					to this page */
	page_t*		page,		/* in: index page */
	dict_index_t*	index);		/* in: record descriptor */
/************************************************************************
Drops a page hash index. */

void
btr_search_drop_page_hash_index(
/*============================*/
	page_t*	page);	/* in: index page, s- or x-latched */
/************************************************************************
Drops a page hash index when a page is freed from a fseg to the file system.
Drops possible hash index if the page happens to be in the buffer pool. */

void
btr_search_drop_page_hash_when_freed(
/*=================================*/
	ulint	space,		/* in: space id */
	ulint	page_no);	/* in: page number */
/************************************************************************
Updates the page hash index when a single record is inserted on a page. */

void
btr_search_update_hash_node_on_insert(
/*==================================*/
	btr_cur_t*	cursor);/* in: cursor which was positioned to the
				place to insert using btr_cur_search_...,
				and the new record has been inserted next
				to the cursor */
/************************************************************************
Updates the page hash index when a single record is inserted on a page. */

void
btr_search_update_hash_on_insert(
/*=============================*/
	btr_cur_t*	cursor);/* in: cursor which was positioned to the
				place to insert using btr_cur_search_...,
				and the new record has been inserted next
				to the cursor */
/************************************************************************
Updates the page hash index when a single record is deleted from a page. */

void
btr_search_update_hash_on_delete(
/*=============================*/
	btr_cur_t*	cursor);/* in: cursor which was positioned on the
				record to delete using btr_cur_search_...,
				the record is not yet deleted */
/************************************************************************
Validates the search system. */

ibool
btr_search_validate(void);
/*======================*/
				/* out: TRUE if ok */

/* Search info directions */
#define BTR_SEA_NO_DIRECTION	1
#define BTR_SEA_LEFT		2
#define BTR_SEA_RIGHT		3
#define BTR_SEA_SAME_REC	4

/* The search info struct in an index */

struct btr_search_struct{
	ulint	magic_n;	/* magic number */
	/* The following 4 fields are currently not used: */
	rec_t*	last_search;	/* pointer to the lower limit record of the
				previous search; NULL if not known */
	ulint	n_direction;	/* number of consecutive searches in the
				same direction */
	ulint	direction;	/* BTR_SEA_NO_DIRECTION, BTR_SEA_LEFT,
				BTR_SEA_RIGHT, BTR_SEA_SAME_REC,
				or BTR_SEA_SAME_PAGE */
	dulint	modify_clock;	/* value of modify clock at the time
				last_search was stored */
	/*----------------------*/
	/* The following 4 fields are not protected by any latch: */
	page_t*	root_guess;	/* the root page frame when it was last time
				fetched, or NULL */
	ulint	hash_analysis;	/* when this exceeds a certain value, the
				hash analysis starts; this is reset if no
				success noticed */
	ibool	last_hash_succ;	/* TRUE if the last search would have
				succeeded, or did succeed, using the hash
				index; NOTE that the value here is not exact:
				it is not calculated for every search, and the
				calculation itself is not always accurate! */
	ulint	n_hash_potential;/* number of consecutive searches which would
				have succeeded, or did succeed, using the hash
				index */
	/*----------------------*/			
	ulint	n_fields;	/* recommended prefix length for hash search:
				number of full fields */
	ulint	n_bytes;	/* recommended prefix: number of bytes in
				an incomplete field */
	ulint	side;		/* BTR_SEARCH_LEFT_SIDE or
				BTR_SEARCH_RIGHT_SIDE, depending on whether
				the leftmost record of several records with
				the same prefix should be indexed in the
				hash index */
	/*----------------------*/
	ulint	n_hash_succ;	/* number of successful hash searches thus
				far */
	ulint	n_hash_fail;	/* number of failed hash searches */
	ulint	n_patt_succ;	/* number of successful pattern searches thus
				far */
	ulint	n_searches;	/* number of searches */
};

#define BTR_SEARCH_MAGIC_N	1112765

/* The hash index system */

typedef struct btr_search_sys_struct	btr_search_sys_t;

struct btr_search_sys_struct{
	hash_table_t*	hash_index;
};

extern btr_search_sys_t*	btr_search_sys;

/* The latch protecting the adaptive search system: this latch protects the
(1) hash index;
(2) columns of a record to which we have a pointer in the hash index;

but does NOT protect:

(3) next record offset field in a record;
(4) next or previous records on the same page.

Bear in mind (3) and (4) when using the hash index.
*/

extern rw_lock_t*	btr_search_latch_temp;

#define btr_search_latch	(*btr_search_latch_temp)

#ifdef UNIV_SEARCH_PERF_STAT
extern ulint	btr_search_n_succ;
#endif /* UNIV_SEARCH_PERF_STAT */
extern ulint	btr_search_n_hash_fail;

/* After change in n_fields or n_bytes in info, this many rounds are waited
before starting the hash analysis again: this is to save CPU time when there
is no hope in building a hash index. */

#define BTR_SEARCH_HASH_ANALYSIS	17

#define BTR_SEARCH_LEFT_SIDE	1
#define BTR_SEARCH_RIGHT_SIDE	2

/* Limit of consecutive searches for trying a search shortcut on the search
pattern */

#define BTR_SEARCH_ON_PATTERN_LIMIT	3

/* Limit of consecutive searches for trying a search shortcut using the hash
index */

#define BTR_SEARCH_ON_HASH_LIMIT	3

/* We do this many searches before trying to keep the search latch over calls
from MySQL. If we notice someone waiting for the latch, we again set this
much timeout. This is to reduce contention. */

#define BTR_SEA_TIMEOUT			10000

#ifndef UNIV_NONINL
#include "btr0sea.ic"
#endif

#endif 
