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
#include "page0page.h"

/**************************************************************************
Try to allocate a block from buf_pool->zip_free[]. */

void*
buf_buddy_alloc_low(
/*================*/
			/* out: allocated block, or NULL
			if buf_pool->zip_free[] was empty */
	ulint	i,	/* in: index of buf_pool->zip_free[] */
	ibool	split)	/* in: TRUE=attempt splitting,
			FALSE=try to allocate exact size */
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
	} else if (split && i + 1 < BUF_BUDDY_SIZES) {
		bpage = buf_buddy_alloc_low(i + 1, split);

		if (bpage) {
			buf_page_t*	buddy = bpage + (BUF_BUDDY_LOW << i);

			UT_LIST_ADD_FIRST(list, buf_pool->zip_free[i], buddy);
		}
	}

	return(bpage);
}

/**************************************************************************
Deallocate a buffer frame of UNIV_PAGE_SIZE. */
static
void
buf_buddy_free_block(
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

	block = (buf_block_t*) bpage;
	mutex_enter(&block->mutex);
	buf_LRU_block_free_non_file_page(block);
	mutex_exit(&block->mutex);
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
	ut_ad(src == ut_align_down(src, size));
	ut_ad(dst == ut_align_down(dst, size));
	ut_ad((((ulint) src) ^ ((ulint) dst)) == size);

	/* We assume that all memory from buf_buddy_alloc()
	is used for either compressed pages or buf_page_t
	objects covering compressed pages. */

	if (size >= PAGE_ZIP_MIN_SIZE) {
		/* This is a compressed page. */
		mutex_t*	mutex;

		bpage = buf_page_hash_get(page_get_space_id(src),
					  page_get_page_no(src));
		ut_a(bpage);
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
				ulint		fold
					= buf_page_address_fold(bpage->space,
								bpage->offset);

				memcpy(dpage, bpage, size);

				/* relocate buf_pool->LRU */

				b = UT_LIST_GET_PREV(LRU, bpage);
				UT_LIST_REMOVE(LRU, buf_pool->LRU, bpage);

				if (b) {
					UT_LIST_INSERT_AFTER(LRU,
							     buf_pool->LRU,
							     b, dpage);
				} else {
					UT_LIST_ADD_FIRST(LRU, buf_pool->LRU,
							  dpage);
				}

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

				/* relocate buf_pool->page_hash */
				HASH_DELETE(buf_page_t, hash,
					    buf_pool->page_hash, fold, bpage);
				HASH_INSERT(buf_page_t, hash,
					    buf_pool->page_hash, fold, dpage);
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

	for (bpage = UT_LIST_GET_FIRST(buf_pool->zip_free[i]);
	     bpage; bpage = UT_LIST_GET_NEXT(list, bpage)) {

		ut_ad(bpage->in_LRU_list);
#ifdef UNIV_DEBUG
		switch (buf_page_get_state(bpage)) {
		case BUF_BLOCK_ZIP_PAGE:
		case BUF_BLOCK_ZIP_DIRTY:
			goto state_ok;
		case BUF_BLOCK_ZIP_FREE:
		case BUF_BLOCK_NOT_USED:
		case BUF_BLOCK_READY_FOR_USE:
		case BUF_BLOCK_FILE_PAGE:
		case BUF_BLOCK_MEMORY:
		case BUF_BLOCK_REMOVE_HASH:
			break;
		}
		ut_error;
state_ok:
#endif /* UNIV_DEBUG */

		if (bpage == buddy) {
buddy_free:
			/* The buddy is free: recombine */
			UT_LIST_REMOVE(list, buf_pool->zip_free[i], bpage);
			buf = ut_align_down(buf, BUF_BUDDY_LOW << i);

			if (++i < BUF_BUDDY_SIZES) {

				goto recombine;
			}

			/* The whole block is free. */
			buf_buddy_free_block(buf);
			return;
		}

		ut_a(bpage != buf);
	}

	/* The buddy is not free. Is there a free block of this size? */
	bpage = UT_LIST_GET_FIRST(buf_pool->zip_free[i]);

	if (bpage) {
		/* Try to relocate the buddy of buf to the free block. */
		if (buf_buddy_relocate(buddy, bpage, i)) {

			goto buddy_free;
		}

		/* Try to relocate the buddy of the free block to buf. */
		buddy = buf_buddy_get(bpage, BUF_BUDDY_LOW << i);

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

			goto buddy_free;
		}
	}

	/* Free the block to the buddy list. */
	bpage = buf;
	bpage->state = BUF_BLOCK_ZIP_FREE;
	UT_LIST_ADD_FIRST(list, buf_pool->zip_free[i], bpage);
}
