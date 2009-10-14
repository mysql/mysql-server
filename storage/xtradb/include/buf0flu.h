/*****************************************************************************

Copyright (c) 1995, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/******************************************************
The database buffer pool flush algorithm

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#ifndef buf0flu_h
#define buf0flu_h

#include "univ.i"
#include "buf0types.h"
#include "ut0byte.h"
#include "mtr0types.h"

/************************************************************************
Remove a block from the flush list of modified blocks. */
UNIV_INTERN
void
buf_flush_remove(
/*=============*/
	buf_page_t*	bpage);	/* in: pointer to the block in question */
/************************************************************************
Updates the flush system data structures when a write is completed. */
UNIV_INTERN
void
buf_flush_write_complete(
/*=====================*/
	buf_page_t*	bpage);	/* in: pointer to the block in question */
/*************************************************************************
Flushes pages from the end of the LRU list if there is too small
a margin of replaceable pages there. */
UNIV_INTERN
void
buf_flush_free_margin(
/*=======================*/
	ibool	wait);
/************************************************************************
Initializes a page for writing to the tablespace. */
UNIV_INTERN
void
buf_flush_init_for_writing(
/*=======================*/
	byte*		page,		/* in/out: page */
	void*		page_zip_,	/* in/out: compressed page, or NULL */
	ib_uint64_t	newest_lsn);	/* in: newest modification lsn
					to the page */
/***********************************************************************
This utility flushes dirty blocks from the end of the LRU list or flush_list.
NOTE 1: in the case of an LRU flush the calling thread may own latches to
pages: to avoid deadlocks, this function must be written so that it cannot
end up waiting for these latches! NOTE 2: in the case of a flush list flush,
the calling thread is not allowed to own any latches on pages! */
UNIV_INTERN
ulint
buf_flush_batch(
/*============*/
					/* out: number of blocks for which the
					write request was queued;
					ULINT_UNDEFINED if there was a flush
					of the same type already running */
	enum buf_flush	flush_type,	/* in: BUF_FLUSH_LRU or
					BUF_FLUSH_LIST; if BUF_FLUSH_LIST,
					then the caller must not own any
					latches on pages */
	ulint		min_n,		/* in: wished minimum mumber of blocks
					flushed (it is not guaranteed that the
					actual number is that big, though) */
	ib_uint64_t	lsn_limit);	/* in the case BUF_FLUSH_LIST all
					blocks whose oldest_modification is
					smaller than this should be flushed
					(if their number does not exceed
					min_n), otherwise ignored */
/**********************************************************************
Waits until a flush batch of the given type ends */
UNIV_INTERN
void
buf_flush_wait_batch_end(
/*=====================*/
	enum buf_flush	type);	/* in: BUF_FLUSH_LRU or BUF_FLUSH_LIST */
/************************************************************************
This function should be called at a mini-transaction commit, if a page was
modified in it. Puts the block to the list of modified blocks, if it not
already in it. */
UNIV_INLINE
void
buf_flush_note_modification(
/*========================*/
	buf_block_t*	block,	/* in: block which is modified */
	mtr_t*		mtr);	/* in: mtr */
/************************************************************************
This function should be called when recovery has modified a buffer page. */
UNIV_INLINE
void
buf_flush_recv_note_modification(
/*=============================*/
	buf_block_t*	block,		/* in: block which is modified */
	ib_uint64_t	start_lsn,	/* in: start lsn of the first mtr in a
					set of mtr's */
	ib_uint64_t	end_lsn);	/* in: end lsn of the last mtr in the
					set of mtr's */
/************************************************************************
Returns TRUE if the file page block is immediately suitable for replacement,
i.e., transition FILE_PAGE => NOT_USED allowed. */
UNIV_INTERN
ibool
buf_flush_ready_for_replace(
/*========================*/
				/* out: TRUE if can replace immediately */
	buf_page_t*	bpage);	/* in: buffer control block, must be
				buf_page_in_file(bpage) and in the LRU list */
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/**********************************************************************
Validates the flush list. */
UNIV_INTERN
ibool
buf_flush_validate(void);
/*====================*/
		/* out: TRUE if ok */
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

/* When buf_flush_free_margin is called, it tries to make this many blocks
available to replacement in the free list and at the end of the LRU list (to
make sure that a read-ahead batch can be read efficiently in a single
sweep). */

#define BUF_FLUSH_FREE_BLOCK_MARGIN	(5 + BUF_READ_AHEAD_AREA)
#define BUF_FLUSH_EXTRA_MARGIN		(BUF_FLUSH_FREE_BLOCK_MARGIN / 4 + 100)

#ifndef UNIV_NONINL
#include "buf0flu.ic"
#endif

#endif
