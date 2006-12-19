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

	bpage = UT_LIST_GET_FIRST(buf_pool->zip_free[i]);

	if (bpage) {
		ut_a(buf_page_get_state(bpage) == BUF_BLOCK_ZIP_FREE);

		UT_LIST_REMOVE(list, buf_pool->zip_free[i], bpage);
	} else if (i + 1 < BUF_BUDDY_SIZES) {
		/* Attempt to split. */
		bpage = buf_buddy_alloc_zip(i + 1);

		if (bpage) {
			buf_page_t*	buddy = (buf_page_t*)
				(((char*) bpage) + (BUF_BUDDY_LOW << i));

			ut_d(memset(buddy, i, BUF_BUDDY_LOW << i));
			buddy->state = BUF_BLOCK_ZIP_FREE;
			UT_LIST_ADD_FIRST(list, buf_pool->zip_free[i], buddy);
		}
	}

#ifdef UNIV_DEBUG
	if (bpage) {
		memset(bpage, ~i, BUF_BUDDY_LOW << i);
	}
#endif /* UNIV_DEBUG */

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
	ulint		fold	= (ulint) buf / UNIV_PAGE_SIZE;
	buf_page_t*	bpage;
	buf_block_t*	block;

#ifdef UNIV_SYNC_DEBUG
	ut_a(mutex_own(&buf_pool->mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_a(buf == ut_align_down(buf, UNIV_PAGE_SIZE));

	HASH_SEARCH(hash, buf_pool->zip_hash, fold, bpage,
		    ((buf_block_t*) bpage)->frame == buf);
	ut_a(bpage);
	ut_a(buf_page_get_state(bpage) == BUF_BLOCK_MEMORY);
	ut_d(memset(buf, 0, UNIV_PAGE_SIZE));

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
	ulint		fold;
#ifdef UNIV_SYNC_DEBUG
	ut_a(mutex_own(&buf_pool->mutex));
#endif /* UNIV_SYNC_DEBUG */
	buf_block_set_state(block, BUF_BLOCK_MEMORY);

	ut_a(block->frame);
	ut_a(block->frame == ut_align_down(block->frame, UNIV_PAGE_SIZE));

	fold = (ulint) block->frame / UNIV_PAGE_SIZE;

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
		UT_LIST_ADD_FIRST(list, buf_pool->zip_free[j], bpage);
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
#endif /* UNIV_SYNC_DEBUG */

	if (BUF_BUDDY_LOW << i >= PAGE_ZIP_MIN_SIZE) {
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

	for (bpage = UT_LIST_GET_LAST(buf_pool->LRU); bpage;
	     bpage = UT_LIST_GET_PREV(LRU, bpage)) {

		void*		ret;
		mutex_t*	block_mutex = buf_page_get_mutex(bpage);

		mutex_enter(block_mutex);

		/* Keep the compressed pages of uncompressed blocks. */
		if (!buf_LRU_free_block(bpage, FALSE)) {

			mutex_exit(block_mutex);
			continue;
		}

		mutex_exit(block_mutex);

		ret = buf_buddy_alloc_zip(i);

		if (ret) {

			return(ret);
		}
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
	ulint	i,	/* in: index of buf_pool->zip_free[] */
	ibool	lru)	/* in: TRUE=allocate from the LRU list if needed */
{
	buf_block_t*	block;

#ifdef UNIV_SYNC_DEBUG
	ut_a(mutex_own(&buf_pool->mutex));
#endif /* UNIV_SYNC_DEBUG */

	/* Try to allocate from the buddy system. */
	block = buf_buddy_alloc_zip(i);

	if (block) {

		return(block);
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
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(!ut_align_offset(src, size));
	ut_ad(!ut_align_offset(dst, size));

	/* We assume that all memory from buf_buddy_alloc()
	is used for either compressed pages or buf_page_t
	objects covering compressed pages. */

	if (size >= PAGE_ZIP_MIN_SIZE) {
		/* This is a compressed page. */
		mutex_t*	mutex;

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

		mutex = buf_page_get_mutex(bpage);

		mutex_enter(mutex);

		if (buf_flush_ready_for_replace(bpage)) {
			switch (buf_page_get_state(bpage)) {
			case BUF_BLOCK_ZIP_FREE:
			case BUF_BLOCK_NOT_USED:
			case BUF_BLOCK_READY_FOR_USE:
			case BUF_BLOCK_MEMORY:
			case BUF_BLOCK_REMOVE_HASH:
				ut_error;
				break;

			case BUF_BLOCK_ZIP_PAGE:
			case BUF_BLOCK_ZIP_DIRTY:
			case BUF_BLOCK_FILE_PAGE:
				/* Relocate the compressed page. */
				ut_a(bpage->zip.data == src);
				memcpy(dst, src, size);
				bpage->zip.data = dst;
				mutex_exit(mutex);
				return(TRUE);
			}
		}

		mutex_exit(mutex);
	} else {
		/* This must be a buf_page_t object. */
		bpage = (buf_page_t*) src;

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

			if (buf_flush_ready_for_replace(bpage)) {
				buf_page_t*	dpage	= (buf_page_t*) dst;
				buf_page_t*	b;

				memcpy(dpage, bpage, size);
				buf_relocate(bpage, dpage);

				/* relocate buf_pool->zip_clean */
				b = UT_LIST_GET_PREV(list, bpage);
				UT_LIST_REMOVE(list, buf_pool->zip_clean,
					       bpage);

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
	void*	buf,	/* in: block to free */
	ulint	i)	/* in: index of buf_pool->zip_free[] */
{
	buf_page_t*	bpage;
	buf_page_t*	buddy;
#ifdef UNIV_SYNC_DEBUG
	ut_a(mutex_own(&buf_pool->mutex));
#endif /* UNIV_SYNC_DEBUG */
recombine:
	ut_ad(i < BUF_BUDDY_SIZES);
	ut_ad(buf == ut_align_down(buf, BUF_BUDDY_LOW << i));

	/* Try to combine adjacent blocks. */

	buddy = (buf_page_t*) buf_buddy_get(((byte*) buf), BUF_BUDDY_LOW << i);

	if (buddy->state != BUF_BLOCK_ZIP_FREE) {

		goto buddy_nonfree;
	}

	/* The field buddy->state can only be trusted for free blocks.
	If buddy->state == BUF_BLOCK_ZIP_FREE, the block is free if
	it is in the free list. */

	for (bpage = UT_LIST_GET_FIRST(buf_pool->zip_free[i]);
	     bpage; bpage = UT_LIST_GET_NEXT(list, bpage)) {
		ut_ad(buf_page_get_state(bpage) == BUF_BLOCK_ZIP_FREE);

		if (bpage == buddy) {
buddy_free:
			/* The buddy is free: recombine */
			UT_LIST_REMOVE(list, buf_pool->zip_free[i], bpage);
			buf = ut_align_down(buf, BUF_BUDDY_LOW << (i + 1));

			if (++i < BUF_BUDDY_SIZES) {

				goto recombine;
			}

			/* The whole block is free. */
			buf_buddy_block_free(buf);
			return;
		}

		ut_a(bpage != buf);
	}

buddy_nonfree:
	/* The buddy is not free. Is there a free block of this size? */
	bpage = UT_LIST_GET_FIRST(buf_pool->zip_free[i]);

	if (bpage) {
		/* Try to relocate the buddy of buf to the free block. */
		if (buf_buddy_relocate(buddy, bpage, i)) {

			goto buddy_free;
		}

		/* Try to relocate the buddy of the free block to buf. */
		buddy = (buf_page_t*) buf_buddy_get(((byte*) bpage),
						    BUF_BUDDY_LOW << i);

#ifdef UNIV_DEBUG
		{
			const buf_page_t* b;

			/* The buddy must not be free, because we always
			recombine adjacent free blocks. */
			for (b = UT_LIST_GET_FIRST(buf_pool->zip_free[i]);
			     b; b = UT_LIST_GET_NEXT(list, b)) {

				ut_a(b != buddy);
			}
		}
#endif /* UNIV_DEBUG */

		if (buf_buddy_relocate(buddy, buf, i)) {

			buf = bpage;
			goto buddy_free;
		}
	}

	/* Free the block to the buddy list. */
	bpage = buf;
	ut_d(memset(bpage, i, BUF_BUDDY_LOW << i));
	bpage->state = BUF_BLOCK_ZIP_FREE;
	UT_LIST_ADD_FIRST(list, buf_pool->zip_free[i], bpage);
}
