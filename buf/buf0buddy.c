/******************************************************
Binary buddy allocator for compressed pages

(c) 2006 Innobase Oy

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

/**************************************************************************
Get the offset of the buddy of a compressed page frame. */
UNIV_INLINE
byte*
buf_buddy_get(
/*==========*/
			/* out: the buddy relative of page */
	byte*	page,	/* in: compressed page */
	ulint	size)	/* in: page size in bytes */
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

/**************************************************************************
Add a block to the head of the appropriate buddy free list. */
UNIV_INLINE
void
buf_buddy_add_to_free(
/*==================*/
	buf_page_t*	bpage,	/* in,own: block to be freed */
	ulint		i)	/* in: index of buf_pool->zip_free[] */
{
#ifdef UNIV_DEBUG_VALGRIND
	buf_page_t*	b  = UT_LIST_GET_FIRST(buf_pool->zip_free[i]);

	if (b) UNIV_MEM_VALID(b, BUF_BUDDY_LOW << i);
#endif /* UNIV_DEBUG_VALGRIND */

	ut_ad(buf_pool->zip_free[i].start != bpage);
	UT_LIST_ADD_FIRST(list, buf_pool->zip_free[i], bpage);

#ifdef UNIV_DEBUG_VALGRIND
	if (b) UNIV_MEM_FREE(b, BUF_BUDDY_LOW << i);
	UNIV_MEM_FREE(bpage, BUF_BUDDY_LOW << i);
#endif /* UNIV_DEBUG_VALGRIND */
}

/**************************************************************************
Remove a block from the appropriate buddy free list. */
UNIV_INLINE
void
buf_buddy_remove_from_free(
/*=======================*/
	buf_page_t*	bpage,	/* in: block to be removed */
	ulint		i)	/* in: index of buf_pool->zip_free[] */
{
#ifdef UNIV_DEBUG_VALGRIND
	buf_page_t*	prev = UT_LIST_GET_PREV(list, bpage);
	buf_page_t*	next = UT_LIST_GET_NEXT(list, bpage);

	if (prev) UNIV_MEM_VALID(prev, BUF_BUDDY_LOW << i);
	if (next) UNIV_MEM_VALID(next, BUF_BUDDY_LOW << i);

	ut_ad(!prev || buf_page_get_state(prev) == BUF_BLOCK_ZIP_FREE);
	ut_ad(!next || buf_page_get_state(next) == BUF_BLOCK_ZIP_FREE);
#endif /* UNIV_DEBUG_VALGRIND */

	ut_ad(buf_page_get_state(bpage) == BUF_BLOCK_ZIP_FREE);
	UT_LIST_REMOVE(list, buf_pool->zip_free[i], bpage);

#ifdef UNIV_DEBUG_VALGRIND
	if (prev) UNIV_MEM_FREE(prev, BUF_BUDDY_LOW << i);
	if (next) UNIV_MEM_FREE(next, BUF_BUDDY_LOW << i);
#endif /* UNIV_DEBUG_VALGRIND */
}

/**************************************************************************
Try to allocate a block from buf_pool->zip_free[]. */
static
void*
buf_buddy_alloc_zip(
/*================*/
			/* out: allocated block, or NULL
			if buf_pool->zip_free[] was empty */
	ulint	i)	/* in: index of buf_pool->zip_free[] */
{
	buf_page_t*	bpage;

#ifdef UNIV_SYNC_DEBUG
	ut_a(mutex_own(&buf_pool->mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_a(i < BUF_BUDDY_SIZES);

#if defined UNIV_DEBUG && !defined UNIV_DEBUG_VALGRIND
	/* Valgrind would complain about accessing free memory. */
	UT_LIST_VALIDATE(list, buf_page_t, buf_pool->zip_free[i]);
#endif /* UNIV_DEBUG && !UNIV_DEBUG_VALGRIND */
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

/**************************************************************************
Deallocate a buffer frame of UNIV_PAGE_SIZE. */
static
void
buf_buddy_block_free(
/*=================*/
	void*	buf)	/* in: buffer frame to deallocate */
{
	const ulint	fold	= BUF_POOL_ZIP_FOLD_PTR(buf);
	buf_page_t*	bpage;
	buf_block_t*	block;

#ifdef UNIV_SYNC_DEBUG
	ut_a(mutex_own(&buf_pool->mutex));
	ut_a(!mutex_own(&buf_pool->zip_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_a(!ut_align_offset(buf, UNIV_PAGE_SIZE));

	HASH_SEARCH(hash, buf_pool->zip_hash, fold, bpage,
		    ((buf_block_t*) bpage)->frame == buf);
	ut_a(bpage);
	ut_a(buf_page_get_state(bpage) == BUF_BLOCK_MEMORY);
	HASH_DELETE(buf_page_t, hash, buf_pool->zip_hash, fold, bpage);

	ut_d(memset(buf, 0, UNIV_PAGE_SIZE));
	UNIV_MEM_INVALID(buf, UNIV_PAGE_SIZE);

	block = (buf_block_t*) bpage;
	mutex_enter(&block->mutex);
	buf_LRU_block_free_non_file_page(block);
	mutex_exit(&block->mutex);
}

/**************************************************************************
Allocate a buffer block to the buddy allocator. */
static
void
buf_buddy_block_register(
/*=====================*/
	buf_block_t*	block)	/* in: buffer frame to allocate */
{
	const ulint	fold = BUF_POOL_ZIP_FOLD(block);
#ifdef UNIV_SYNC_DEBUG
	ut_a(mutex_own(&buf_pool->mutex));
	ut_a(!mutex_own(&buf_pool->zip_mutex));
#endif /* UNIV_SYNC_DEBUG */
	buf_block_set_state(block, BUF_BLOCK_MEMORY);

	ut_a(block->frame);
	ut_a(!ut_align_offset(block->frame, UNIV_PAGE_SIZE));

	HASH_INSERT(buf_page_t, hash, buf_pool->zip_hash, fold, &block->page);
}

/**************************************************************************
Allocate a block from a bigger object. */
static
void*
buf_buddy_alloc_from(
/*=================*/
				/* out: allocated block */
	void*		buf,	/* in: a block that is free to use */
	ulint		i,	/* in: index of buf_pool->zip_free[] */
	ulint		j)	/* in: size of buf as an index
				of buf_pool->zip_free[] */
{
	ulint	offs	= BUF_BUDDY_LOW << j;

	/* Add the unused parts of the block to the free lists. */
	while (j > i) {
		buf_page_t*	bpage;

		offs >>= 1;
		j--;

		bpage = (buf_page_t*) ((byte*) buf + offs);
		ut_d(memset(bpage, j, BUF_BUDDY_LOW << j));
		bpage->state = BUF_BLOCK_ZIP_FREE;
#if defined UNIV_DEBUG && !defined UNIV_DEBUG_VALGRIND
		/* Valgrind would complain about accessing free memory. */
		UT_LIST_VALIDATE(list, buf_page_t, buf_pool->zip_free[j]);
#endif /* UNIV_DEBUG && !UNIV_DEBUG_VALGRIND */
		buf_buddy_add_to_free(bpage, j);
	}

	return(buf);
}

/**************************************************************************
Try to allocate a block by freeing an unmodified page. */
static
void*
buf_buddy_alloc_clean(
/*==================*/
			/* out: allocated block, or NULL */
	ulint	i)	/* in: index of buf_pool->zip_free[] */
{
	buf_page_t*	bpage;

#ifdef UNIV_SYNC_DEBUG
	ut_a(mutex_own(&buf_pool->mutex));
	ut_a(!mutex_own(&buf_pool->zip_mutex));
#endif /* UNIV_SYNC_DEBUG */

	if (BUF_BUDDY_LOW << i >= PAGE_ZIP_MIN_SIZE
	    && i < BUF_BUDDY_SIZES) {
		/* Try to find a clean compressed-only page
		of the same size. */

		page_zip_des_t	dummy_zip;
		ulint		j;

		page_zip_set_size(&dummy_zip, BUF_BUDDY_LOW << i);

		j = ut_min(UT_LIST_GET_LEN(buf_pool->zip_clean), 100);
		bpage = UT_LIST_GET_FIRST(buf_pool->zip_clean);

		mutex_enter(&buf_pool->zip_mutex);

		for (; j--; bpage = UT_LIST_GET_NEXT(list, bpage)) {
			if (bpage->zip.ssize != dummy_zip.ssize
			    || !buf_LRU_free_block(bpage, FALSE)) {

				continue;
			}

			/* Reuse the block.  In case the block was
			recombined by buf_buddy_free(), we invoke the
			buddy allocator instead of using the block
			directly.  Yes, bpage points to freed memory
			here, but it cannot be used by other threads,
			because when invoked on compressed-only pages,
			buf_LRU_free_block() does not release
			buf_pool->mutex. */

			mutex_exit(&buf_pool->zip_mutex);
			bpage = buf_buddy_alloc_zip(i);
			ut_a(bpage);

			return(bpage);
		}

		mutex_exit(&buf_pool->zip_mutex);
	}

	/* Free blocks from the end of the LRU list until enough space
	is available. */

free_LRU:
	for (bpage = UT_LIST_GET_LAST(buf_pool->LRU); bpage;
	     bpage = UT_LIST_GET_PREV(LRU, bpage)) {

		void*		ret;
		mutex_t*	block_mutex = buf_page_get_mutex(bpage);

		if (!buf_page_in_file(bpage)) {

			/* This is most likely BUF_BLOCK_REMOVE_HASH,
			that is, the block is already being freed. */
			continue;
		}

		mutex_enter(block_mutex);

		/* Keep the compressed pages of uncompressed blocks. */
		if (!buf_LRU_free_block(bpage, FALSE)) {

			mutex_exit(block_mutex);
			continue;
		}

		mutex_exit(block_mutex);

		/* The block was successfully freed.
		Attempt to allocate memory. */

		if (i < BUF_BUDDY_SIZES) {

			ret = buf_buddy_alloc_zip(i);

			if (ret) {

				return(ret);
			}
		} else {
			buf_block_t*	block = buf_LRU_get_free_only();

			if (block) {
				buf_buddy_block_register(block);
				return(block->frame);
			}
		}

		/* A successful buf_LRU_free_block() may release and
		reacquire buf_pool->mutex, and thus bpage->LRU of
		an uncompressed page may point to garbage.  Furthermore,
		if bpage were a compressed page descriptor, it would
		have been deallocated by buf_LRU_free_block().

		Thus, we must restart the traversal of the LRU list. */

		goto free_LRU;
	}

	return(NULL);
}

/**************************************************************************
Allocate a block. */

void*
buf_buddy_alloc_low(
/*================*/
			/* out: allocated block, or NULL
			if buf_pool->zip_free[] was empty */
	ulint	i,	/* in: index of buf_pool->zip_free[],
			or BUF_BUDDY_SIZES */
	ibool	lru)	/* in: TRUE=allocate from the LRU list if needed */
{
	buf_block_t*	block;

#ifdef UNIV_SYNC_DEBUG
	ut_a(mutex_own(&buf_pool->mutex));
	ut_a(!mutex_own(&buf_pool->zip_mutex));
#endif /* UNIV_SYNC_DEBUG */

	if (i < BUF_BUDDY_SIZES) {
		/* Try to allocate from the buddy system. */
		block = buf_buddy_alloc_zip(i);

		if (block) {

			return(block);
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

	/* Try replacing a clean page in the buffer pool. */

	block = buf_buddy_alloc_clean(i);

	if (block) {

		return(block);
	}

	/* Try replacing an uncompressed page in the buffer pool. */
	mutex_exit(&buf_pool->mutex);
	block = buf_LRU_get_free_block(0);
	mutex_enter(&buf_pool->mutex);

alloc_big:
	buf_buddy_block_register(block);

	return(buf_buddy_alloc_from(block->frame, i, BUF_BUDDY_SIZES));
}

/**************************************************************************
Try to relocate a block. */
static
ibool
buf_buddy_relocate(
/*===============*/
				/* out: TRUE if relocated */
	const void*	src,	/* in: block to relocate */
	void*		dst,	/* in: free block to relocate to */
	ulint		i)	/* in: index of buf_pool->zip_free[] */
{
	buf_page_t*	bpage;
	const ulint	size	= BUF_BUDDY_LOW << i;

#ifdef UNIV_SYNC_DEBUG
	ut_a(mutex_own(&buf_pool->mutex));
	ut_a(!mutex_own(&buf_pool->zip_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(!ut_align_offset(src, size));
	ut_ad(!ut_align_offset(dst, size));
#ifdef UNIV_DEBUG_VALGRIND
	VALGRIND_CHECK_MEM_IS_ADDRESSABLE(dst, BUF_BUDDY_LOW << i);
#endif /* UNIV_DEBUG_VALGRIND */

	/* We assume that all memory from buf_buddy_alloc()
	is used for either compressed pages or buf_page_t
	objects covering compressed pages. */

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
			mach_read_from_4(src
					 + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID),
			mach_read_from_4(src
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

#ifdef UNIV_VALGRIND_DEBUG
		/* The block must have been allocated, but it may
		contain uninitialized data. */
		VALGRIND_CHECK_MEM_IS_ADDRESSABLE(src, size);
#endif /* UNIV_VALGRIND_DEBUG */

		mutex = buf_page_get_mutex(bpage);

		mutex_enter(mutex);

		if (buf_page_can_relocate(bpage)) {
			/* Relocate the compressed page. */
			ut_a(bpage->zip.data == src);
			memcpy(dst, src, size);
			UNIV_MEM_INVALID(src, size);
			bpage->zip.data = dst;
			mutex_exit(mutex);
			return(TRUE);
		}

		mutex_exit(mutex);
	} else if (i == buf_buddy_get_slot(sizeof(buf_page_t))) {
		/* This must be a buf_page_t object. */
		bpage = (buf_page_t*) src;
#ifdef UNIV_VALGRIND_DEBUG
		VALGRIND_CHECK_MEM_IS_DEFINED(src, size);
#endif /* UNIV_VALGRIND_DEBUG */

		switch (buf_page_get_state(bpage)) {
		case BUF_BLOCK_ZIP_FREE:
		case BUF_BLOCK_NOT_USED:
		case BUF_BLOCK_READY_FOR_USE:
		case BUF_BLOCK_FILE_PAGE:
		case BUF_BLOCK_MEMORY:
		case BUF_BLOCK_REMOVE_HASH:
			ut_error;
			break;

		case BUF_BLOCK_ZIP_DIRTY:
			/* Cannot relocate dirty pages. */
			break;

		case BUF_BLOCK_ZIP_PAGE:
			mutex_enter(&buf_pool->zip_mutex);

			if (buf_page_can_relocate(bpage)) {
				buf_page_t*	dpage	= (buf_page_t*) dst;
				buf_page_t*	b;

				buf_relocate(bpage, dpage);
				UNIV_MEM_INVALID(src, size);

				/* relocate buf_pool->zip_clean */
				b = UT_LIST_GET_PREV(list, dpage);
				UT_LIST_REMOVE(list, buf_pool->zip_clean,
					       dpage);

				if (b) {
					UT_LIST_INSERT_AFTER(
						list, buf_pool->zip_clean,
						b, dpage);
				} else {
					UT_LIST_ADD_FIRST(
						list, buf_pool->zip_clean,
						dpage);
				}
			}

			mutex_exit(&buf_pool->zip_mutex);
			return(TRUE);
		}
	}

	return(FALSE);
}

/**************************************************************************
Deallocate a block. */

void
buf_buddy_free_low(
/*===============*/
	void*	buf,	/* in: block to be freed, must not be
			pointed to by the buffer pool */
	ulint	i)	/* in: index of buf_pool->zip_free[] */
{
	buf_page_t*	bpage;
	buf_page_t*	buddy;
#ifdef UNIV_SYNC_DEBUG
	ut_a(mutex_own(&buf_pool->mutex));
	ut_a(!mutex_own(&buf_pool->zip_mutex));
#endif /* UNIV_SYNC_DEBUG */
recombine:
#ifdef UNIV_DEBUG_VALGRIND
	VALGRIND_CHECK_MEM_IS_ADDRESSABLE(buf, BUF_BUDDY_LOW << i);
	UNIV_MEM_INVALID(buf, BUF_BUDDY_LOW << i);
#endif /* UNIV_DEBUG_VALGRIND */
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
			UNIV_MEM_FREE(bpage, BUF_BUDDY_LOW << i);
			bpage = next;
		}
	}

#ifndef UNIV_DEBUG_VALGRIND
buddy_nonfree:
	/* Valgrind would complain about accessing free memory. */
	ut_d(UT_LIST_VALIDATE(list, buf_page_t, buf_pool->zip_free[i]));
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

#if defined UNIV_DEBUG && !defined UNIV_DEBUG_VALGRIND
		{
			const buf_page_t* b;

			/* The buddy must not be (completely) free, because
			we always recombine adjacent free blocks.
			(Parts of the buddy can be free in
			buf_pool->zip_free[j] with j < i.)*/
			for (b = UT_LIST_GET_FIRST(buf_pool->zip_free[i]);
			     b; b = UT_LIST_GET_NEXT(list, b)) {

				ut_a(b != buddy);
			}
		}
#endif /* UNIV_DEBUG && !UNIV_DEBUG_VALGRIND */

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
			     || b->state == BUF_BLOCK_ZIP_DIRTY) &&
			    b->space > 0 && b->space < 1000) {
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
