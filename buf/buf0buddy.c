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
