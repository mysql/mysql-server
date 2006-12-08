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
				memcpy(dst, src, size);
				/* TODO: relocate bpage->list, bpage->LRU */
			}

			mutex_exit(&buf_pool->zip_mutex);
			/* TODO: return(TRUE); */
			break;
		}
	}

	return(FALSE);
}

/**************************************************************************
Release a block to buf_pool->zip_free[]. */

void*
buf_buddy_free_low(
/*===============*/
			/* out: pointer to the beginning of a block of
			size BUF_BUDDY_HIGH that should be freed to
			the underlying allocator, or NULL if released
			to buf_pool->zip_free[] */
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
			return(buf);
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

	return(NULL);
}
