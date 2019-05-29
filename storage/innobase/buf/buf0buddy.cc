/*****************************************************************************

Copyright (c) 2006, 2019, Oracle and/or its affiliates. All Rights Reserved.

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

/**************************************************//**
@file buf/buf0buddy.cc
Binary buddy allocator for compressed pages

Created December 2006 by Marko Makela
*******************************************************/

#include "buf0buddy.h"
#ifdef UNIV_NONINL
# include "buf0buddy.ic"
#endif
#include "buf0buf.h"
#include "buf0lru.h"
#include "buf0flu.h"
#include "page0zip.h"
#include "srv0start.h"

/** When freeing a buf we attempt to coalesce by looking at its buddy
and deciding whether it is free or not. To ascertain if the buddy is
free we look for BUF_BUDDY_STAMP_FREE at BUF_BUDDY_STAMP_OFFSET
within the buddy. The question is how we can be sure that it is
safe to look at BUF_BUDDY_STAMP_OFFSET.
The answer lies in following invariants:
* All blocks allocated by buddy allocator are used for compressed
page frame.
* A compressed table always have space_id < SRV_LOG_SPACE_FIRST_ID
* BUF_BUDDY_STAMP_OFFSET always points to the space_id field in
a frame.
  -- The above is true because we look at these fields when the
     corresponding buddy block is free which implies that:
     * The block we are looking at must have an address aligned at
       the same size that its free buddy has. For example, if we have
       a free block of 8K then its buddy's address must be aligned at
       8K as well.
     * It is possible that the block we are looking at may have been
       further divided into smaller sized blocks but its starting
       address must still remain the start of a page frame i.e.: it
       cannot be middle of a block. For example, if we have a free
       block of size 8K then its buddy may be divided into blocks
       of, say, 1K, 1K, 2K, 4K but the buddy's address will still be
       the starting address of first 1K compressed page.
     * What is important to note is that for any given block, the
       buddy's address cannot be in the middle of a larger block i.e.:
       in above example, our 8K block cannot have a buddy whose address
       is aligned on 8K but it is part of a larger 16K block.
*/

/** Offset within buf_buddy_free_t where free or non_free stamps
are written.*/
#define BUF_BUDDY_STAMP_OFFSET	FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID

/** Value that we stamp on all buffers that are currently on the zip_free
list. This value is stamped at BUF_BUDDY_STAMP_OFFSET offset */
#define BUF_BUDDY_STAMP_FREE	 SRV_LOG_SPACE_FIRST_ID

/** Stamp value for non-free buffers. Will be overwritten by a non-zero
value by the consumer of the block */
#define BUF_BUDDY_STAMP_NONFREE	0XFFFFFFFFUL

#if BUF_BUDDY_STAMP_FREE >= BUF_BUDDY_STAMP_NONFREE
# error "BUF_BUDDY_STAMP_FREE >= BUF_BUDDY_STAMP_NONFREE"
#endif

/** Return type of buf_buddy_is_free() */
enum buf_buddy_state_t {
	BUF_BUDDY_STATE_FREE,	/*!< If the buddy to completely free */
	BUF_BUDDY_STATE_USED,	/*!< Buddy currently in used */
	BUF_BUDDY_STATE_PARTIALLY_USED/*!< Some sub-blocks in the buddy
				are in use */
};

#ifdef UNIV_DEBUG_VALGRIND
/**********************************************************************//**
Invalidate memory area that we won't access while page is free */
UNIV_INLINE
void
buf_buddy_mem_invalid(
/*==================*/
	buf_buddy_free_t*	buf,	/*!< in: block to check */
	ulint			i)	/*!< in: index of zip_free[] */
{
	const size_t	size	= BUF_BUDDY_LOW << i;
	ut_ad(i <= BUF_BUDDY_SIZES);

	UNIV_MEM_ASSERT_W(buf, size);
	UNIV_MEM_INVALID(buf, size);
}
#else /* UNIV_DEBUG_VALGRIND */
# define buf_buddy_mem_invalid(buf, i) ut_ad((i) <= BUF_BUDDY_SIZES)
#endif /* UNIV_DEBUG_VALGRIND */

/**********************************************************************//**
Check if a buddy is stamped free.
@return whether the buddy is free */
UNIV_INLINE MY_ATTRIBUTE((warn_unused_result))
bool
buf_buddy_stamp_is_free(
/*====================*/
	const buf_buddy_free_t*	buf)	/*!< in: block to check */
{
	return(mach_read_from_4(buf->stamp.bytes + BUF_BUDDY_STAMP_OFFSET)
	       == BUF_BUDDY_STAMP_FREE);
}

/**********************************************************************//**
Stamps a buddy free. */
UNIV_INLINE
void
buf_buddy_stamp_free(
/*=================*/
	buf_buddy_free_t*	buf,	/*!< in/out: block to stamp */
	ulint			i)	/*!< in: block size */
{
	ut_d(memset(&buf->stamp, static_cast<int>(i), BUF_BUDDY_LOW << i));
	buf_buddy_mem_invalid(buf, i);
	mach_write_to_4(buf->stamp.bytes + BUF_BUDDY_STAMP_OFFSET,
			BUF_BUDDY_STAMP_FREE);
	buf->stamp.size = i;
}

/**********************************************************************//**
Stamps a buddy nonfree.
@param[in,out]	buf	block to stamp
@param[in]	i	block size */
#define buf_buddy_stamp_nonfree(buf, i) do {				\
	buf_buddy_mem_invalid(buf, i);					\
	memset(buf->stamp.bytes + BUF_BUDDY_STAMP_OFFSET, 0xff, 4);	\
} while (0)
#if BUF_BUDDY_STAMP_NONFREE != 0xffffffff
# error "BUF_BUDDY_STAMP_NONFREE != 0xffffffff"
#endif

/**********************************************************************//**
Get the offset of the buddy of a compressed page frame.
@return the buddy relative of page */
UNIV_INLINE
void*
buf_buddy_get(
/*==========*/
	byte*	page,	/*!< in: compressed page */
	ulint	size)	/*!< in: page size in bytes */
{
	ut_ad(ut_is_2pow(size));
	ut_ad(size >= BUF_BUDDY_LOW);
	ut_ad(BUF_BUDDY_LOW <= UNIV_ZIP_SIZE_MIN);
	ut_ad(size < BUF_BUDDY_HIGH);
	ut_ad(BUF_BUDDY_HIGH == UNIV_PAGE_SIZE);
	ut_ad(!ut_align_offset(page, size));

	if (((ulint) page) & size) {
		return(page - size);
	} else {
		return(page + size);
	}
}

#ifdef UNIV_DEBUG
/** Validate a given zip_free list. */
struct	CheckZipFree {
	CheckZipFree(ulint i) : m_i(i) {}

	void	operator()(const buf_buddy_free_t* elem) const
	{
		ut_a(buf_buddy_stamp_is_free(elem));
		ut_a(elem->stamp.size <= m_i);
	}

	ulint		m_i;
};

/** Validate a buddy list.
@param[in]	buf_pool	buffer pool instance
@param[in]	i		buddy size to validate */
static
void
buf_buddy_list_validate(
	const buf_pool_t*	buf_pool,
	ulint			i)
{
	CheckZipFree	check(i);
	ut_list_validate(buf_pool->zip_free[i], check);
}

/**********************************************************************//**
Debug function to validate that a buffer is indeed free i.e.: in the
zip_free[].
@return true if free */
UNIV_INLINE
bool
buf_buddy_check_free(
/*=================*/
	buf_pool_t*		buf_pool,/*!< in: buffer pool instance */
	const buf_buddy_free_t*	buf,	/*!< in: block to check */
	ulint			i)	/*!< in: index of buf_pool->zip_free[] */
{
	const ulint	size	= BUF_BUDDY_LOW << i;

	ut_ad(buf_pool_mutex_own(buf_pool));
	ut_ad(!ut_align_offset(buf, size));
	ut_ad(i >= buf_buddy_get_slot(UNIV_ZIP_SIZE_MIN));

	buf_buddy_free_t* itr;

	for (itr = UT_LIST_GET_FIRST(buf_pool->zip_free[i]);
	     itr && itr != buf;
	     itr = UT_LIST_GET_NEXT(list, itr)) {
	}

	return(itr == buf);
}
#endif /* UNIV_DEBUG */

/**********************************************************************//**
Checks if a buf is free i.e.: in the zip_free[].
@retval BUF_BUDDY_STATE_FREE if fully free
@retval BUF_BUDDY_STATE_USED if currently in use
@retval BUF_BUDDY_STATE_PARTIALLY_USED if partially in use. */
static  MY_ATTRIBUTE((warn_unused_result))
buf_buddy_state_t
buf_buddy_is_free(
/*==============*/
	buf_buddy_free_t*	buf,	/*!< in: block to check */
	ulint			i)	/*!< in: index of
					buf_pool->zip_free[] */
{
#ifdef UNIV_DEBUG
	const ulint	size	= BUF_BUDDY_LOW << i;
	ut_ad(!ut_align_offset(buf, size));
	ut_ad(i >= buf_buddy_get_slot(UNIV_ZIP_SIZE_MIN));
#endif /* UNIV_DEBUG */

	/* We assume that all memory from buf_buddy_alloc()
	is used for compressed page frames. */

	/* We look inside the allocated objects returned by
	buf_buddy_alloc() and assume that each block is a compressed
	page that contains one of the following in space_id.
	* BUF_BUDDY_STAMP_FREE if the block is in a zip_free list or
	* BUF_BUDDY_STAMP_NONFREE if the block has been allocated but
	not initialized yet or
	* A valid space_id of a compressed tablespace

	The call below attempts to read from free memory.  The memory
	is "owned" by the buddy allocator (and it has been allocated
	from the buffer pool), so there is nothing wrong about this. */
	if (!buf_buddy_stamp_is_free(buf)) {
		return(BUF_BUDDY_STATE_USED);
	}

	/* A block may be free but a fragment of it may still be in use.
	To guard against that we write the free block size in terms of
	zip_free index at start of stamped block. Note that we can
	safely rely on this value only if the buf is free. */
	ut_ad(buf->stamp.size <= i);
	return(buf->stamp.size == i
	       ? BUF_BUDDY_STATE_FREE
	       : BUF_BUDDY_STATE_PARTIALLY_USED);
}

/**********************************************************************//**
Add a block to the head of the appropriate buddy free list. */
UNIV_INLINE
void
buf_buddy_add_to_free(
/*==================*/
	buf_pool_t*		buf_pool,	/*!< in: buffer pool instance */
	buf_buddy_free_t*	buf,		/*!< in,own: block to be freed */
	ulint			i)		/*!< in: index of
						buf_pool->zip_free[] */
{
	ut_ad(buf_pool_mutex_own(buf_pool));
	ut_ad(buf_pool->zip_free[i].start != buf);

	buf_buddy_stamp_free(buf, i);
	UT_LIST_ADD_FIRST(buf_pool->zip_free[i], buf);
	ut_d(buf_buddy_list_validate(buf_pool, i));
}

/**********************************************************************//**
Remove a block from the appropriate buddy free list. */
UNIV_INLINE
void
buf_buddy_remove_from_free(
/*=======================*/
	buf_pool_t*		buf_pool,	/*!< in: buffer pool instance */
	buf_buddy_free_t*	buf,		/*!< in,own: block to be
						freed */
	ulint			i)		/*!< in: index of
						buf_pool->zip_free[] */
{
	ut_ad(buf_pool_mutex_own(buf_pool));
	ut_ad(buf_buddy_check_free(buf_pool, buf, i));

	UT_LIST_REMOVE(buf_pool->zip_free[i], buf);
	buf_buddy_stamp_nonfree(buf, i);
}

/**********************************************************************//**
Try to allocate a block from buf_pool->zip_free[].
@return allocated block, or NULL if buf_pool->zip_free[] was empty */
static
buf_buddy_free_t*
buf_buddy_alloc_zip(
/*================*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	ulint		i)		/*!< in: index of buf_pool->zip_free[] */
{
	buf_buddy_free_t*	buf;

	ut_ad(buf_pool_mutex_own(buf_pool));
	ut_a(i < BUF_BUDDY_SIZES);
	ut_a(i >= buf_buddy_get_slot(UNIV_ZIP_SIZE_MIN));

	ut_d(buf_buddy_list_validate(buf_pool, i));

	buf = UT_LIST_GET_FIRST(buf_pool->zip_free[i]);

	if (buf_pool->curr_size < buf_pool->old_size
	    && UT_LIST_GET_LEN(buf_pool->withdraw)
		< buf_pool->withdraw_target) {

		while (buf != NULL
		       && buf_frame_will_withdrawn(
			       buf_pool, reinterpret_cast<byte*>(buf))) {
			/* This should be withdrawn, not to be allocated */
			buf = UT_LIST_GET_NEXT(list, buf);
		}
	}

	if (buf) {
		buf_buddy_remove_from_free(buf_pool, buf, i);
	} else if (i + 1 < BUF_BUDDY_SIZES) {
		/* Attempt to split. */
		buf = buf_buddy_alloc_zip(buf_pool, i + 1);

		if (buf) {
			buf_buddy_free_t* buddy =
				reinterpret_cast<buf_buddy_free_t*>(
					buf->stamp.bytes
					+ (BUF_BUDDY_LOW << i));

			ut_ad(!buf_pool_contains_zip(buf_pool, buddy));
			buf_buddy_add_to_free(buf_pool, buddy, i);
		}
	}

	if (buf) {
		/* Trash the page other than the BUF_BUDDY_STAMP_NONFREE. */
		UNIV_MEM_TRASH(buf, ~i, BUF_BUDDY_STAMP_OFFSET);
		UNIV_MEM_TRASH(BUF_BUDDY_STAMP_OFFSET + 4
			       + buf->stamp.bytes, ~i,
			       (BUF_BUDDY_LOW << i)
			       - (BUF_BUDDY_STAMP_OFFSET + 4));
		ut_ad(mach_read_from_4(buf->stamp.bytes
				       + BUF_BUDDY_STAMP_OFFSET)
		      == BUF_BUDDY_STAMP_NONFREE);
	}

	return(buf);
}

/**********************************************************************//**
Deallocate a buffer frame of UNIV_PAGE_SIZE. */
static
void
buf_buddy_block_free(
/*=================*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	void*		buf)		/*!< in: buffer frame to deallocate */
{
	const ulint	fold	= BUF_POOL_ZIP_FOLD_PTR(buf);
	buf_page_t*	bpage;
	buf_block_t*	block;

	ut_ad(buf_pool_mutex_own(buf_pool));
	ut_ad(!mutex_own(&buf_pool->zip_mutex));
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
	buf_page_mutex_enter(block);
	buf_LRU_block_free_non_file_page(block);
	buf_page_mutex_exit(block);

	ut_ad(buf_pool->buddy_n_frames > 0);
	ut_d(buf_pool->buddy_n_frames--);
}

/**********************************************************************//**
Allocate a buffer block to the buddy allocator. */
static
void
buf_buddy_block_register(
/*=====================*/
	buf_block_t*	block)	/*!< in: buffer frame to allocate */
{
	buf_pool_t*	buf_pool = buf_pool_from_block(block);
	const ulint	fold = BUF_POOL_ZIP_FOLD(block);
	ut_ad(buf_pool_mutex_own(buf_pool));
	ut_ad(!mutex_own(&buf_pool->zip_mutex));
	ut_ad(buf_block_get_state(block) == BUF_BLOCK_READY_FOR_USE);

	buf_block_set_state(block, BUF_BLOCK_MEMORY);

	ut_a(block->frame);
	ut_a(!ut_align_offset(block->frame, UNIV_PAGE_SIZE));

	ut_ad(!block->page.in_page_hash);
	ut_ad(!block->page.in_zip_hash);
	ut_d(block->page.in_zip_hash = TRUE);
	HASH_INSERT(buf_page_t, hash, buf_pool->zip_hash, fold, &block->page);

	ut_d(buf_pool->buddy_n_frames++);
}

/**********************************************************************//**
Allocate a block from a bigger object.
@return allocated block */
static
void*
buf_buddy_alloc_from(
/*=================*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	void*		buf,		/*!< in: a block that is free to use */
	ulint		i,		/*!< in: index of
					buf_pool->zip_free[] */
	ulint		j)		/*!< in: size of buf as an index
					of buf_pool->zip_free[] */
{
	ulint	offs	= BUF_BUDDY_LOW << j;
	ut_ad(j <= BUF_BUDDY_SIZES);
	ut_ad(i >= buf_buddy_get_slot(UNIV_ZIP_SIZE_MIN));
	ut_ad(j >= i);
	ut_ad(!ut_align_offset(buf, offs));

	/* Add the unused parts of the block to the free lists. */
	while (j > i) {
		buf_buddy_free_t*	zip_buf;

		offs >>= 1;
		j--;

		zip_buf = reinterpret_cast<buf_buddy_free_t*>(
			reinterpret_cast<byte*>(buf) + offs);
		buf_buddy_add_to_free(buf_pool, zip_buf, j);
	}

	buf_buddy_stamp_nonfree(reinterpret_cast<buf_buddy_free_t*>(buf), i);
	return(buf);
}

/**********************************************************************//**
Allocate a block.  The thread calling this function must hold
buf_pool->mutex and must not hold buf_pool->zip_mutex or any block->mutex.
The buf_pool_mutex may be released and reacquired.
@return allocated block, never NULL */
void*
buf_buddy_alloc_low(
/*================*/
	buf_pool_t*	buf_pool,	/*!< in/out: buffer pool instance */
	ulint		i,		/*!< in: index of buf_pool->zip_free[],
					or BUF_BUDDY_SIZES */
	ibool*		lru)		/*!< in: pointer to a variable that
					will be assigned TRUE if storage was
					allocated from the LRU list and
					buf_pool->mutex was temporarily
					released */
{
	buf_block_t*	block;

	ut_ad(lru);
	ut_ad(buf_pool_mutex_own(buf_pool));
	ut_ad(!mutex_own(&buf_pool->zip_mutex));
	ut_ad(i >= buf_buddy_get_slot(UNIV_ZIP_SIZE_MIN));

	if (i < BUF_BUDDY_SIZES) {
		/* Try to allocate from the buddy system. */
		block = (buf_block_t*) buf_buddy_alloc_zip(buf_pool, i);

		if (block) {
			goto func_exit;
		}
	}

	/* Try allocating from the buf_pool->free list. */
	block = buf_LRU_get_free_only(buf_pool);

	if (block) {

		goto alloc_big;
	}

	/* Try replacing an uncompressed page in the buffer pool. */
	buf_pool_mutex_exit(buf_pool);
	block = buf_LRU_get_free_block(buf_pool);
	*lru = TRUE;
	buf_pool_mutex_enter(buf_pool);

alloc_big:
	buf_buddy_block_register(block);

	block = (buf_block_t*) buf_buddy_alloc_from(
		buf_pool, block->frame, i, BUF_BUDDY_SIZES);

func_exit:
	buf_pool->buddy_stat[i].used++;
	return(block);
}

/**********************************************************************//**
Try to relocate a block.
@return true if relocated */
static
bool
buf_buddy_relocate(
/*===============*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	void*		src,		/*!< in: block to relocate */
	void*		dst,		/*!< in: free block to relocate to */
	ulint		i,		/*!< in: index of
					buf_pool->zip_free[] */
	bool		force)		/*!< in: true if we must relocate
					always */
{
	buf_page_t*	bpage;
	const ulint	size = BUF_BUDDY_LOW << i;
	ulint		space;
	ulint		offset;

	ut_ad(buf_pool_mutex_own(buf_pool));
	ut_ad(!mutex_own(&buf_pool->zip_mutex));
	ut_ad(!ut_align_offset(src, size));
	ut_ad(!ut_align_offset(dst, size));
	ut_ad(i >= buf_buddy_get_slot(UNIV_ZIP_SIZE_MIN));
	UNIV_MEM_ASSERT_W(dst, size);

	space	= mach_read_from_4((const byte*) src
				   + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);
	offset	= mach_read_from_4((const byte*) src
				   + FIL_PAGE_OFFSET);

	/* Suppress Valgrind warnings about conditional jump
	on uninitialized value. */
	UNIV_MEM_VALID(&space, sizeof space);
	UNIV_MEM_VALID(&offset, sizeof offset);

	ut_ad(space != BUF_BUDDY_STAMP_FREE);

	const page_id_t	page_id(space, offset);

	/* If space,offset is bogus, then we know that the
	buf_page_hash_get_low() call below will return NULL. */
	if (!force && buf_pool != buf_pool_get(page_id)) {
		return(false);
	}

	rw_lock_t*	hash_lock = buf_page_hash_lock_get(buf_pool, page_id);

	rw_lock_x_lock(hash_lock);

	bpage = buf_page_hash_get_low(buf_pool, page_id);

	if (!bpage || bpage->zip.data != src) {
		/* The block has probably been freshly
		allocated by buf_LRU_get_free_block() but not
		added to buf_pool->page_hash yet.  Obviously,
		it cannot be relocated. */

		rw_lock_x_unlock(hash_lock);

		if (!force || space != 0 || offset != 0) {
			return(false);
		}

		/* It might be just uninitialized page.
		We should search from LRU list also. */

		bpage = UT_LIST_GET_FIRST(buf_pool->LRU);
		while (bpage != NULL) {
			if (bpage->zip.data == src) {
				hash_lock = buf_page_hash_lock_get(
					buf_pool, bpage->id);
				rw_lock_x_lock(hash_lock);
				break;
			}
			bpage = UT_LIST_GET_NEXT(LRU, bpage);
		}

		if (bpage == NULL) {
			return(false);
		}
	}

	if (page_zip_get_size(&bpage->zip) != size) {
		/* The block is of different size.  We would
		have to relocate all blocks covered by src.
		For the sake of simplicity, give up. */
		ut_ad(page_zip_get_size(&bpage->zip) < size);

		rw_lock_x_unlock(hash_lock);

		return(false);
	}

	/* The block must have been allocated, but it may
	contain uninitialized data. */
	UNIV_MEM_ASSERT_W(src, size);

	BPageMutex*	block_mutex = buf_page_get_mutex(bpage);

	mutex_enter(block_mutex);

	if (buf_page_can_relocate(bpage)) {
		/* Relocate the compressed page. */
		ib_time_monotonic_us_t usec = ut_time_monotonic_us();

		ut_a(bpage->zip.data == src);

		memcpy(dst, src, size);
		bpage->zip.data = reinterpret_cast<page_zip_t*>(dst);

		rw_lock_x_unlock(hash_lock);

		mutex_exit(block_mutex);

		buf_buddy_mem_invalid(
			reinterpret_cast<buf_buddy_free_t*>(src), i);

		buf_buddy_stat_t*	buddy_stat = &buf_pool->buddy_stat[i];
		buddy_stat->relocated++;
		buddy_stat->relocated_usec += ut_time_monotonic_us() - usec;
		return(true);
	}

	rw_lock_x_unlock(hash_lock);

	mutex_exit(block_mutex);
	return(false);
}

/**********************************************************************//**
Deallocate a block. */
void
buf_buddy_free_low(
/*===============*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	void*		buf,		/*!< in: block to be freed, must not be
					pointed to by the buffer pool */
	ulint		i)		/*!< in: index of buf_pool->zip_free[],
					or BUF_BUDDY_SIZES */
{
	buf_buddy_free_t*	buddy;

	ut_ad(buf_pool_mutex_own(buf_pool));
	ut_ad(!mutex_own(&buf_pool->zip_mutex));
	ut_ad(i <= BUF_BUDDY_SIZES);
	ut_ad(i >= buf_buddy_get_slot(UNIV_ZIP_SIZE_MIN));
	ut_ad(buf_pool->buddy_stat[i].used > 0);

	buf_pool->buddy_stat[i].used--;
recombine:
	UNIV_MEM_ASSERT_AND_ALLOC(buf, BUF_BUDDY_LOW << i);

	if (i == BUF_BUDDY_SIZES) {
		buf_buddy_block_free(buf_pool, buf);
		return;
	}

	ut_ad(i < BUF_BUDDY_SIZES);
	ut_ad(buf == ut_align_down(buf, BUF_BUDDY_LOW << i));
	ut_ad(!buf_pool_contains_zip(buf_pool, buf));

	/* Do not recombine blocks if there are few free blocks.
	We may waste up to 15360*max_len bytes to free blocks
	(1024 + 2048 + 4096 + 8192 = 15360) */
	if (UT_LIST_GET_LEN(buf_pool->zip_free[i]) < 16
	    && buf_pool->curr_size >= buf_pool->old_size) {
		goto func_exit;
	}

	/* Try to combine adjacent blocks. */
	buddy = reinterpret_cast<buf_buddy_free_t*>(
		buf_buddy_get(reinterpret_cast<byte*>(buf),
			      BUF_BUDDY_LOW << i));

	switch (buf_buddy_is_free(buddy, i)) {
	case BUF_BUDDY_STATE_FREE:
		/* The buddy is free: recombine */
		buf_buddy_remove_from_free(buf_pool, buddy, i);
buddy_is_free:
		ut_ad(!buf_pool_contains_zip(buf_pool, buddy));
		i++;
		buf = ut_align_down(buf, BUF_BUDDY_LOW << i);

		goto recombine;

	case BUF_BUDDY_STATE_USED:
		ut_d(buf_buddy_list_validate(buf_pool, i));

		/* The buddy is not free. Is there a free block of
		this size? */
		if (buf_buddy_free_t* zip_buf =
			UT_LIST_GET_FIRST(buf_pool->zip_free[i])) {

			/* Remove the block from the free list, because
			a successful buf_buddy_relocate() will overwrite
			zip_free->list. */
			buf_buddy_remove_from_free(buf_pool, zip_buf, i);

			/* Try to relocate the buddy of buf to the free
			block. */
			if (buf_buddy_relocate(buf_pool, buddy, zip_buf, i,
					       false)) {

				goto buddy_is_free;
			}

			buf_buddy_add_to_free(buf_pool, zip_buf, i);
		}

		break;
	case BUF_BUDDY_STATE_PARTIALLY_USED:
		/* Some sub-blocks in the buddy are still in use.
		Relocation will fail. No need to try. */
		break;
	}

func_exit:
	/* Free the block to the buddy list. */
	buf_buddy_add_to_free(buf_pool,
			      reinterpret_cast<buf_buddy_free_t*>(buf),
			      i);
}

/** Reallocate a block.
@param[in]	buf_pool	buffer pool instance
@param[in]	buf		block to be reallocated, must be pointed
to by the buffer pool
@param[in]	size		block size, up to UNIV_PAGE_SIZE
@retval false	if failed because of no free blocks. */
bool
buf_buddy_realloc(
	buf_pool_t*	buf_pool,
	void*		buf,
	ulint		size)
{
	buf_block_t*	block = NULL;
	ulint		i = buf_buddy_get_slot(size);

	ut_ad(buf_pool_mutex_own(buf_pool));
	ut_ad(!mutex_own(&buf_pool->zip_mutex));
	ut_ad(i <= BUF_BUDDY_SIZES);
	ut_ad(i >= buf_buddy_get_slot(UNIV_ZIP_SIZE_MIN));

	if (i < BUF_BUDDY_SIZES) {
		/* Try to allocate from the buddy system. */
		block = reinterpret_cast<buf_block_t*>(
			buf_buddy_alloc_zip(buf_pool, i));
	}

	if (block == NULL) {
		/* Try allocating from the buf_pool->free list. */
		block = buf_LRU_get_free_only(buf_pool);

		if (block == NULL) {
			return(false); /* free_list was not enough */
		}

		buf_buddy_block_register(block);

		block = reinterpret_cast<buf_block_t*>(
			buf_buddy_alloc_from(
				buf_pool, block->frame, i, BUF_BUDDY_SIZES));
	}

	buf_pool->buddy_stat[i].used++;

	/* Try to relocate the buddy of buf to the free block. */
	if (buf_buddy_relocate(buf_pool, buf, block, i, true)) {
		/* succeeded */
		buf_buddy_free_low(buf_pool, buf, i);
	} else {
		/* failed */
		buf_buddy_free_low(buf_pool, block, i);
	}

	return(true); /* free_list was enough */
}

/** Combine all pairs of free buddies.
@param[in]	buf_pool	buffer pool instance */
void
buf_buddy_condense_free(
	buf_pool_t*	buf_pool)
{
	ut_ad(buf_pool_mutex_own(buf_pool));
	ut_ad(buf_pool->curr_size < buf_pool->old_size);

	for (ulint i = 0; i < UT_ARR_SIZE(buf_pool->zip_free); ++i) {
		buf_buddy_free_t* buf =
			UT_LIST_GET_FIRST(buf_pool->zip_free[i]);

		/* seek to withdraw target */
		while (buf != NULL
		       && !buf_frame_will_withdrawn(
				buf_pool, reinterpret_cast<byte*>(buf))) {
			buf = UT_LIST_GET_NEXT(list, buf);
		}

		while (buf != NULL) {
			buf_buddy_free_t* next =
				UT_LIST_GET_NEXT(list, buf);

			buf_buddy_free_t* buddy =
				reinterpret_cast<buf_buddy_free_t*>(
					buf_buddy_get(
						reinterpret_cast<byte*>(buf),
						BUF_BUDDY_LOW << i));

			/* seek to the next withdraw target */
			while (true) {
				while (next != NULL
				       && !buf_frame_will_withdrawn(
						buf_pool,
						reinterpret_cast<byte*>(next))) {
					 next = UT_LIST_GET_NEXT(list, next);
				}

				if (buddy != next) {
					break;
				}

				next = UT_LIST_GET_NEXT(list, next);
			}

			if (buf_buddy_is_free(buddy, i)
			    == BUF_BUDDY_STATE_FREE) {
				/* Both buf and buddy are free.
				Try to combine them. */
				buf_buddy_remove_from_free(buf_pool, buf, i);
				buf_pool->buddy_stat[i].used++;

				buf_buddy_free_low(buf_pool, buf, i);
			}

			buf = next;
		}
	}
}
