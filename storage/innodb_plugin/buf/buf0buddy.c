/*****************************************************************************

Copyright (c) 2006, 2010, Innobase Oy. All Rights Reserved.

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

/**************************************************//**
@file buf/buf0buddy.c
Binary buddy allocator for compressed pages

Created December 2006 by Marko Makela
*******************************************************/

#define THIS_MODULE
#include "buf0buddy.h"
#ifdef UNIV_NONINL
# include "buf0buddy.ic"
#endif
#undef THIS_MODULE
#include "buf0buf.h"
#include "buf0lru.h"
#include "buf0flu.h"
#include "page0zip.h"

/* Statistic counters */

#ifdef UNIV_DEBUG
/** Number of frames allocated from the buffer pool to the buddy system.
Protected by buf_pool_mutex. */
static ulint buf_buddy_n_frames;
#endif /* UNIV_DEBUG */
/** Statistics of the buddy system, indexed by block size.
Protected by buf_pool_mutex. */
UNIV_INTERN buf_buddy_stat_t buf_buddy_stat[BUF_BUDDY_SIZES + 1];

/**********************************************************************//**
Get the offset of the buddy of a compressed page frame.
@return	the buddy relative of page */
UNIV_INLINE
byte*
buf_buddy_get(
/*==========*/
	byte*	page,	/*!< in: compressed page */
	ulint	size)	/*!< in: page size in bytes */
{
	ut_ad(ut_is_2pow(size));
	ut_ad(size >= BUF_BUDDY_LOW);
	ut_ad(size < BUF_BUDDY_HIGH);
	ut_ad(!ut_align_offset(page, size));

	if (((ulint) page) & size) {
		return(page - size);
	} else {
		return(page + size);
	}
}

/**********************************************************************//**
Add a block to the head of the appropriate buddy free list. */
UNIV_INLINE
void
buf_buddy_add_to_free(
/*==================*/
	buf_page_t*	bpage,	/*!< in,own: block to be freed */
	ulint		i)	/*!< in: index of buf_pool->zip_free[] */
{
#ifdef UNIV_DEBUG_VALGRIND
	buf_page_t*	b  = UT_LIST_GET_FIRST(buf_pool->zip_free[i]);

	if (b) UNIV_MEM_VALID(b, BUF_BUDDY_LOW << i);
#endif /* UNIV_DEBUG_VALGRIND */

	ut_ad(buf_pool_mutex_own());
	ut_ad(buf_page_get_state(bpage) == BUF_BLOCK_ZIP_FREE);
	ut_ad(buf_pool->zip_free[i].start != bpage);
	UT_LIST_ADD_FIRST(list, buf_pool->zip_free[i], bpage);

#ifdef UNIV_DEBUG_VALGRIND
	if (b) UNIV_MEM_FREE(b, BUF_BUDDY_LOW << i);
	UNIV_MEM_ASSERT_AND_FREE(bpage, BUF_BUDDY_LOW << i);
#endif /* UNIV_DEBUG_VALGRIND */
}

/**********************************************************************//**
Remove a block from the appropriate buddy free list. */
UNIV_INLINE
void
buf_buddy_remove_from_free(
/*=======================*/
	buf_page_t*	bpage,	/*!< in: block to be removed */
	ulint		i)	/*!< in: index of buf_pool->zip_free[] */
{
#ifdef UNIV_DEBUG_VALGRIND
	buf_page_t*	prev = UT_LIST_GET_PREV(list, bpage);
	buf_page_t*	next = UT_LIST_GET_NEXT(list, bpage);

	if (prev) UNIV_MEM_VALID(prev, BUF_BUDDY_LOW << i);
	if (next) UNIV_MEM_VALID(next, BUF_BUDDY_LOW << i);

	ut_ad(!prev || buf_page_get_state(prev) == BUF_BLOCK_ZIP_FREE);
	ut_ad(!next || buf_page_get_state(next) == BUF_BLOCK_ZIP_FREE);
#endif /* UNIV_DEBUG_VALGRIND */

	ut_ad(buf_pool_mutex_own());
	ut_ad(buf_page_get_state(bpage) == BUF_BLOCK_ZIP_FREE);
	UT_LIST_REMOVE(list, buf_pool->zip_free[i], bpage);

#ifdef UNIV_DEBUG_VALGRIND
	if (prev) UNIV_MEM_FREE(prev, BUF_BUDDY_LOW << i);
	if (next) UNIV_MEM_FREE(next, BUF_BUDDY_LOW << i);
#endif /* UNIV_DEBUG_VALGRIND */
}

/**********************************************************************//**
Try to allocate a block from buf_pool->zip_free[].
@return	allocated block, or NULL if buf_pool->zip_free[] was empty */
static
void*
buf_buddy_alloc_zip(
/*================*/
	ulint	i)	/*!< in: index of buf_pool->zip_free[] */
{
	buf_page_t*	bpage;

	ut_ad(buf_pool_mutex_own());
	ut_a(i < BUF_BUDDY_SIZES);

#ifndef UNIV_DEBUG_VALGRIND
	/* Valgrind would complain about accessing free memory. */
	ut_d(UT_LIST_VALIDATE(list, buf_page_t, buf_pool->zip_free[i],
			      ut_ad(buf_page_get_state(ut_list_node_313)
				    == BUF_BLOCK_ZIP_FREE)));
#endif /* !UNIV_DEBUG_VALGRIND */
	bpage = UT_LIST_GET_FIRST(buf_pool->zip_free[i]);

	if (bpage) {
		UNIV_MEM_VALID(bpage, BUF_BUDDY_LOW << i);
		ut_a(buf_page_get_state(bpage) == BUF_BLOCK_ZIP_FREE);

		buf_buddy_remove_from_free(bpage, i);
	} else if (i + 1 < BUF_BUDDY_SIZES) {
		/* Attempt to split. */
		bpage = buf_buddy_alloc_zip(i + 1);

		if (bpage) {
			buf_page_t*	buddy = (buf_page_t*)
				(((char*) bpage) + (BUF_BUDDY_LOW << i));

			ut_ad(!buf_pool_contains_zip(buddy));
			ut_d(memset(buddy, i, BUF_BUDDY_LOW << i));
			buddy->state = BUF_BLOCK_ZIP_FREE;
			buf_buddy_add_to_free(buddy, i);
		}
	}

#ifdef UNIV_DEBUG
	if (bpage) {
		memset(bpage, ~i, BUF_BUDDY_LOW << i);
	}
#endif /* UNIV_DEBUG */

	UNIV_MEM_ALLOC(bpage, BUF_BUDDY_SIZES << i);

	return(bpage);
}

/**********************************************************************//**
Deallocate a buffer frame of UNIV_PAGE_SIZE. */
static
void
buf_buddy_block_free(
/*=================*/
	void*	buf)	/*!< in: buffer frame to deallocate */
{
	const ulint	fold	= BUF_POOL_ZIP_FOLD_PTR(buf);
	buf_page_t*	bpage;
	buf_block_t*	block;

	ut_ad(buf_pool_mutex_own());
	ut_ad(!mutex_own(&buf_pool_zip_mutex));
	ut_a(!ut_align_offset(buf, UNIV_PAGE_SIZE));

	HASH_SEARCH(hash, buf_pool->zip_hash, fold, buf_page_t*, bpage,
		    ut_ad(buf_page_get_state(bpage) == BUF_BLOCK_MEMORY
			  && bpage->in_zip_hash && !bpage->in_page_hash),
		    ((buf_block_t*) bpage)->frame == buf);
	ut_a(bpage);
	ut_a(buf_page_get_state(bpage) == BUF_BLOCK_MEMORY);
	ut_ad(!bpage->in_page_hash);
	ut_ad(bpage->in_zip_hash);
	ut_d(bpage->in_zip_hash = FALSE);
	HASH_DELETE(buf_page_t, hash, buf_pool->zip_hash, fold, bpage);

	ut_d(memset(buf, 0, UNIV_PAGE_SIZE));
	UNIV_MEM_INVALID(buf, UNIV_PAGE_SIZE);

	block = (buf_block_t*) bpage;
	mutex_enter(&block->mutex);
	buf_LRU_block_free_non_file_page(block);
	mutex_exit(&block->mutex);

	ut_ad(buf_buddy_n_frames > 0);
	ut_d(buf_buddy_n_frames--);
}

/**********************************************************************//**
Allocate a buffer block to the buddy allocator. */
static
void
buf_buddy_block_register(
/*=====================*/
	buf_block_t*	block)	/*!< in: buffer frame to allocate */
{
	const ulint	fold = BUF_POOL_ZIP_FOLD(block);
	ut_ad(buf_pool_mutex_own());
	ut_ad(!mutex_own(&buf_pool_zip_mutex));
	ut_ad(buf_block_get_state(block) == BUF_BLOCK_READY_FOR_USE);

	buf_block_set_state(block, BUF_BLOCK_MEMORY);

	ut_a(block->frame);
	ut_a(!ut_align_offset(block->frame, UNIV_PAGE_SIZE));

	ut_ad(!block->page.in_page_hash);
	ut_ad(!block->page.in_zip_hash);
	ut_d(block->page.in_zip_hash = TRUE);
	HASH_INSERT(buf_page_t, hash, buf_pool->zip_hash, fold, &block->page);

	ut_d(buf_buddy_n_frames++);
}

/**********************************************************************//**
Allocate a block from a bigger object.
@return	allocated block */
static
void*
buf_buddy_alloc_from(
/*=================*/
	void*		buf,	/*!< in: a block that is free to use */
	ulint		i,	/*!< in: index of buf_pool->zip_free[] */
	ulint		j)	/*!< in: size of buf as an index
				of buf_pool->zip_free[] */
{
	ulint	offs	= BUF_BUDDY_LOW << j;
	ut_ad(j <= BUF_BUDDY_SIZES);
	ut_ad(j >= i);
	ut_ad(!ut_align_offset(buf, offs));

	/* Add the unused parts of the block to the free lists. */
	while (j > i) {
		buf_page_t*	bpage;

		offs >>= 1;
		j--;

		bpage = (buf_page_t*) ((byte*) buf + offs);
		ut_d(memset(bpage, j, BUF_BUDDY_LOW << j));
		bpage->state = BUF_BLOCK_ZIP_FREE;
#ifndef UNIV_DEBUG_VALGRIND
		/* Valgrind would complain about accessing free memory. */
		ut_d(UT_LIST_VALIDATE(list, buf_page_t, buf_pool->zip_free[i],
				      ut_ad(buf_page_get_state(
						    ut_list_node_313)
					    == BUF_BLOCK_ZIP_FREE)));
#endif /* !UNIV_DEBUG_VALGRIND */
		buf_buddy_add_to_free(bpage, j);
	}

	return(buf);
}

/**********************************************************************//**
Allocate a block.  The thread calling this function must hold
buf_pool_mutex and must not hold buf_pool_zip_mutex or any block->mutex.
The buf_pool_mutex may only be released and reacquired if lru != NULL.
@return	allocated block, possibly NULL if lru==NULL */
UNIV_INTERN
void*
buf_buddy_alloc_low(
/*================*/
	ulint	i,	/*!< in: index of buf_pool->zip_free[],
			or BUF_BUDDY_SIZES */
	ibool*	lru)	/*!< in: pointer to a variable that will be assigned
			TRUE if storage was allocated from the LRU list
			and buf_pool_mutex was temporarily released,
			or NULL if the LRU list should not be used */
{
	buf_block_t*	block;

	ut_ad(buf_pool_mutex_own());
	ut_ad(!mutex_own(&buf_pool_zip_mutex));

	if (i < BUF_BUDDY_SIZES) {
		/* Try to allocate from the buddy system. */
		block = buf_buddy_alloc_zip(i);

		if (block) {

			goto func_exit;
		}
	}

	/* Try allocating from the buf_pool->free list. */
	block = buf_LRU_get_free_only();

	if (block) {

		goto alloc_big;
	}

	if (!lru) {

		return(NULL);
	}

	/* Try replacing an uncompressed page in the buffer pool. */
	buf_pool_mutex_exit();
	block = buf_LRU_get_free_block(0);
	*lru = TRUE;
	buf_pool_mutex_enter();

alloc_big:
	buf_buddy_block_register(block);

	block = buf_buddy_alloc_from(block->frame, i, BUF_BUDDY_SIZES);

func_exit:
	buf_buddy_stat[i].used++;
	return(block);
}

/**********************************************************************//**
Try to relocate the control block of a compressed page.
@return	TRUE if relocated */
static
ibool
buf_buddy_relocate_block(
/*=====================*/
	buf_page_t*	bpage,	/*!< in: block to relocate */
	buf_page_t*	dpage)	/*!< in: free block to relocate to */
{
	buf_page_t*	b;

	ut_ad(buf_pool_mutex_own());

	switch (buf_page_get_state(bpage)) {
	case BUF_BLOCK_ZIP_FREE:
	case BUF_BLOCK_NOT_USED:
	case BUF_BLOCK_READY_FOR_USE:
	case BUF_BLOCK_FILE_PAGE:
	case BUF_BLOCK_MEMORY:
	case BUF_BLOCK_REMOVE_HASH:
		ut_error;
	case BUF_BLOCK_ZIP_DIRTY:
		/* Cannot relocate dirty pages. */
		return(FALSE);

	case BUF_BLOCK_ZIP_PAGE:
		break;
	}

	mutex_enter(&buf_pool_zip_mutex);

	if (!buf_page_can_relocate(bpage)) {
		mutex_exit(&buf_pool_zip_mutex);
		return(FALSE);
	}

	buf_relocate(bpage, dpage);
	ut_d(bpage->state = BUF_BLOCK_ZIP_FREE);

	/* relocate buf_pool->zip_clean */
	b = UT_LIST_GET_PREV(list, dpage);
	UT_LIST_REMOVE(list, buf_pool->zip_clean, dpage);

	if (b) {
		UT_LIST_INSERT_AFTER(list, buf_pool->zip_clean, b, dpage);
	} else {
		UT_LIST_ADD_FIRST(list, buf_pool->zip_clean, dpage);
	}

	UNIV_MEM_INVALID(bpage, sizeof *bpage);

	mutex_exit(&buf_pool_zip_mutex);
	return(TRUE);
}

/**********************************************************************//**
Try to relocate a block.
@return	TRUE if relocated */
static
ibool
buf_buddy_relocate(
/*===============*/
	void*	src,	/*!< in: block to relocate */
	void*	dst,	/*!< in: free block to relocate to */
	ulint	i)	/*!< in: index of buf_pool->zip_free[] */
{
	buf_page_t*	bpage;
	const ulint	size	= BUF_BUDDY_LOW << i;
	ullint		usec	= ut_time_us(NULL);

	ut_ad(buf_pool_mutex_own());
	ut_ad(!mutex_own(&buf_pool_zip_mutex));
	ut_ad(!ut_align_offset(src, size));
	ut_ad(!ut_align_offset(dst, size));
	UNIV_MEM_ASSERT_W(dst, size);

	/* We assume that all memory from buf_buddy_alloc()
	is used for either compressed pages or buf_page_t
	objects covering compressed pages. */

	/* We look inside the allocated objects returned by
	buf_buddy_alloc() and assume that anything of
	PAGE_ZIP_MIN_SIZE or larger is a compressed page that contains
	a valid space_id and page_no in the page header.  Should the
	fields be invalid, we will be unable to relocate the block.
	We also assume that anything that fits sizeof(buf_page_t)
	actually is a properly initialized buf_page_t object. */

	if (size >= PAGE_ZIP_MIN_SIZE) {
		/* This is a compressed page. */
		mutex_t*	mutex;

		/* The src block may be split into smaller blocks,
		some of which may be free.  Thus, the
		mach_read_from_4() calls below may attempt to read
		from free memory.  The memory is "owned" by the buddy
		allocator (and it has been allocated from the buffer
		pool), so there is nothing wrong about this.  The
		mach_read_from_4() calls here will only trigger bogus
		Valgrind memcheck warnings in UNIV_DEBUG_VALGRIND builds. */
		bpage = buf_page_hash_get(
			mach_read_from_4((const byte*) src
					 + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID),
			mach_read_from_4((const byte*) src
					 + FIL_PAGE_OFFSET));

		if (!bpage || bpage->zip.data != src) {
			/* The block has probably been freshly
			allocated by buf_LRU_get_free_block() but not
			added to buf_pool->page_hash yet.  Obviously,
			it cannot be relocated. */

			return(FALSE);
		}

		if (page_zip_get_size(&bpage->zip) != size) {
			/* The block is of different size.  We would
			have to relocate all blocks covered by src.
			For the sake of simplicity, give up. */
			ut_ad(page_zip_get_size(&bpage->zip) < size);

			return(FALSE);
		}

		/* The block must have been allocated, but it may
		contain uninitialized data. */
		UNIV_MEM_ASSERT_W(src, size);

		mutex = buf_page_get_mutex(bpage);

		mutex_enter(mutex);

		if (buf_page_can_relocate(bpage)) {
			/* Relocate the compressed page. */
			ut_a(bpage->zip.data == src);
			memcpy(dst, src, size);
			bpage->zip.data = dst;
			mutex_exit(mutex);
success:
			UNIV_MEM_INVALID(src, size);
			{
				buf_buddy_stat_t*	buddy_stat
					= &buf_buddy_stat[i];
				buddy_stat->relocated++;
				buddy_stat->relocated_usec
					+= ut_time_us(NULL) - usec;
			}
			return(TRUE);
		}

		mutex_exit(mutex);
	} else if (i == buf_buddy_get_slot(sizeof(buf_page_t))) {
		/* This must be a buf_page_t object. */
		UNIV_MEM_ASSERT_RW(src, size);
		if (buf_buddy_relocate_block(src, dst)) {

			goto success;
		}
	}

	return(FALSE);
}

/**********************************************************************//**
Deallocate a block. */
UNIV_INTERN
void
buf_buddy_free_low(
/*===============*/
	void*	buf,	/*!< in: block to be freed, must not be
			pointed to by the buffer pool */
	ulint	i)	/*!< in: index of buf_pool->zip_free[],
			or BUF_BUDDY_SIZES */
{
	buf_page_t*	bpage;
	buf_page_t*	buddy;

	ut_ad(buf_pool_mutex_own());
	ut_ad(!mutex_own(&buf_pool_zip_mutex));
	ut_ad(i <= BUF_BUDDY_SIZES);
	ut_ad(buf_buddy_stat[i].used > 0);

	buf_buddy_stat[i].used--;
recombine:
	UNIV_MEM_ASSERT_AND_ALLOC(buf, BUF_BUDDY_LOW << i);
	ut_d(((buf_page_t*) buf)->state = BUF_BLOCK_ZIP_FREE);

	if (i == BUF_BUDDY_SIZES) {
		buf_buddy_block_free(buf);
		return;
	}

	ut_ad(i < BUF_BUDDY_SIZES);
	ut_ad(buf == ut_align_down(buf, BUF_BUDDY_LOW << i));
	ut_ad(!buf_pool_contains_zip(buf));

	/* Try to combine adjacent blocks. */

	buddy = (buf_page_t*) buf_buddy_get(((byte*) buf), BUF_BUDDY_LOW << i);

#ifndef UNIV_DEBUG_VALGRIND
	/* Valgrind would complain about accessing free memory. */

	if (buddy->state != BUF_BLOCK_ZIP_FREE) {

		goto buddy_nonfree;
	}

	/* The field buddy->state can only be trusted for free blocks.
	If buddy->state == BUF_BLOCK_ZIP_FREE, the block is free if
	it is in the free list. */
#endif /* !UNIV_DEBUG_VALGRIND */

	for (bpage = UT_LIST_GET_FIRST(buf_pool->zip_free[i]); bpage; ) {
		UNIV_MEM_VALID(bpage, BUF_BUDDY_LOW << i);
		ut_ad(buf_page_get_state(bpage) == BUF_BLOCK_ZIP_FREE);

		if (bpage == buddy) {
buddy_free:
			/* The buddy is free: recombine */
			buf_buddy_remove_from_free(bpage, i);
buddy_free2:
			ut_ad(buf_page_get_state(buddy) == BUF_BLOCK_ZIP_FREE);
			ut_ad(!buf_pool_contains_zip(buddy));
			i++;
			buf = ut_align_down(buf, BUF_BUDDY_LOW << i);

			goto recombine;
		}

		ut_a(bpage != buf);

		{
			buf_page_t*	next = UT_LIST_GET_NEXT(list, bpage);
			UNIV_MEM_ASSERT_AND_FREE(bpage, BUF_BUDDY_LOW << i);
			bpage = next;
		}
	}

#ifndef UNIV_DEBUG_VALGRIND
buddy_nonfree:
	/* Valgrind would complain about accessing free memory. */
	ut_d(UT_LIST_VALIDATE(list, buf_page_t, buf_pool->zip_free[i],
			      ut_ad(buf_page_get_state(ut_list_node_313)
				    == BUF_BLOCK_ZIP_FREE)));
#endif /* UNIV_DEBUG_VALGRIND */

	/* The buddy is not free. Is there a free block of this size? */
	bpage = UT_LIST_GET_FIRST(buf_pool->zip_free[i]);

	if (bpage) {
		/* Remove the block from the free list, because a successful
		buf_buddy_relocate() will overwrite bpage->list. */

		UNIV_MEM_VALID(bpage, BUF_BUDDY_LOW << i);
		buf_buddy_remove_from_free(bpage, i);

		/* Try to relocate the buddy of buf to the free block. */
		if (buf_buddy_relocate(buddy, bpage, i)) {

			ut_d(buddy->state = BUF_BLOCK_ZIP_FREE);
			goto buddy_free2;
		}

		buf_buddy_add_to_free(bpage, i);

		/* Try to relocate the buddy of the free block to buf. */
		buddy = (buf_page_t*) buf_buddy_get(((byte*) bpage),
						    BUF_BUDDY_LOW << i);

#ifndef UNIV_DEBUG_VALGRIND
		/* Valgrind would complain about accessing free memory. */

		/* The buddy must not be (completely) free, because we
		always recombine adjacent free blocks.

		(Parts of the buddy can be free in
		buf_pool->zip_free[j] with j < i.) */
		ut_d(UT_LIST_VALIDATE(list, buf_page_t, buf_pool->zip_free[i],
				      ut_ad(buf_page_get_state(
						    ut_list_node_313)
					    == BUF_BLOCK_ZIP_FREE
					    && ut_list_node_313 != buddy)));
#endif /* !UNIV_DEBUG_VALGRIND */

		if (buf_buddy_relocate(buddy, buf, i)) {

			buf = bpage;
			UNIV_MEM_VALID(bpage, BUF_BUDDY_LOW << i);
			ut_d(buddy->state = BUF_BLOCK_ZIP_FREE);
			goto buddy_free;
		}
	}

	/* Free the block to the buddy list. */
	bpage = buf;
#ifdef UNIV_DEBUG
	if (i < buf_buddy_get_slot(PAGE_ZIP_MIN_SIZE)) {
		/* This area has most likely been allocated for at
		least one compressed-only block descriptor.  Check
		that there are no live objects in the area.  This is
		not a complete check: it may yield false positives as
		well as false negatives.  Also, due to buddy blocks
		being recombined, it is possible (although unlikely)
		that this branch is never reached. */

		char* c;

# ifndef UNIV_DEBUG_VALGRIND
		/* Valgrind would complain about accessing
		uninitialized memory.  Besides, Valgrind performs a
		more exhaustive check, at every memory access. */
		const buf_page_t* b = buf;
		const buf_page_t* const b_end = (buf_page_t*)
			((char*) b + (BUF_BUDDY_LOW << i));

		for (; b < b_end; b++) {
			/* Avoid false positives (and cause false
			negatives) by checking for b->space < 1000. */

			if ((b->state == BUF_BLOCK_ZIP_PAGE
			     || b->state == BUF_BLOCK_ZIP_DIRTY)
			    && b->space > 0 && b->space < 1000) {
				fprintf(stderr,
					"buddy dirty %p %u (%u,%u) %p,%lu\n",
					(void*) b,
					b->state, b->space, b->offset,
					buf, i);
			}
		}
# endif /* !UNIV_DEBUG_VALGRIND */

		/* Scramble the block.  This should make any pointers
		invalid and trigger a segmentation violation.  Because
		the scrambling can be reversed, it may be possible to
		track down the object pointing to the freed data by
		dereferencing the unscrambled bpage->LRU or
		bpage->list pointers. */
		for (c = (char*) buf + (BUF_BUDDY_LOW << i);
		     c-- > (char*) buf; ) {
			*c = ~*c ^ i;
		}
	} else {
		/* Fill large blocks with a constant pattern. */
		memset(bpage, i, BUF_BUDDY_LOW << i);
	}
#endif /* UNIV_DEBUG */
	bpage->state = BUF_BLOCK_ZIP_FREE;
	buf_buddy_add_to_free(bpage, i);
}
