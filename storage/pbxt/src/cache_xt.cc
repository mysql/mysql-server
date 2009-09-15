/* Copyright (c) 2005 PrimeBase Technologies GmbH, Germany
 *
 * PrimeBase XT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2005-05-24	Paul McCullagh
 *
 * H&G2JCtL
 */

#include "xt_config.h"

#ifdef DRIZZLED
#include <bitset>
#endif

#ifndef XT_WIN
#include <unistd.h>
#endif

#include <stdio.h>
#include <time.h>

#include "pthread_xt.h"
#include "thread_xt.h"
#include "filesys_xt.h"
#include "cache_xt.h"
#include "table_xt.h"
#include "trace_xt.h"
#include "util_xt.h"

#define XT_TIME_DIFF(start, now) (\
	((xtWord4) (now) < (xtWord4) (start)) ? \
	((xtWord4) 0XFFFFFFFF - ((xtWord4) (start) - (xtWord4) (now))) : \
	((xtWord4) (now) - (xtWord4) (start)))

/*
 * -----------------------------------------------------------------------
 * D I S K   C A C H E
 */

#define IDX_CAC_SEGMENT_COUNT		((off_t) 1 << XT_INDEX_CACHE_SEGMENT_SHIFTS)
#define IDX_CAC_SEGMENT_MASK		(IDX_CAC_SEGMENT_COUNT - 1)

#ifdef XT_NO_ATOMICS
#define IDX_CAC_USE_PTHREAD_RW
#else
//#define IDX_CAC_USE_RWMUTEX
//#define IDX_CAC_USE_PTHREAD_RW
//#define IDX_USE_SPINXSLOCK
#define IDX_CAC_USE_XSMUTEX
#endif

#ifdef IDX_CAC_USE_XSMUTEX
#define IDX_CAC_LOCK_TYPE				XTXSMutexRec
#define IDX_CAC_INIT_LOCK(s, i)			xt_xsmutex_init_with_autoname(s, &(i)->cs_lock)
#define IDX_CAC_FREE_LOCK(s, i)			xt_xsmutex_free(s, &(i)->cs_lock)	
#define IDX_CAC_READ_LOCK(i, o)			xt_xsmutex_slock(&(i)->cs_lock, (o)->t_id)
#define IDX_CAC_WRITE_LOCK(i, o)		xt_xsmutex_xlock(&(i)->cs_lock, (o)->t_id)
#define IDX_CAC_UNLOCK(i, o)			xt_xsmutex_unlock(&(i)->cs_lock, (o)->t_id)
#elif defined(IDX_CAC_USE_PTHREAD_RW)
#define IDX_CAC_LOCK_TYPE				xt_rwlock_type
#define IDX_CAC_INIT_LOCK(s, i)			xt_init_rwlock(s, &(i)->cs_lock)
#define IDX_CAC_FREE_LOCK(s, i)			xt_free_rwlock(&(i)->cs_lock)	
#define IDX_CAC_READ_LOCK(i, o)			xt_slock_rwlock_ns(&(i)->cs_lock)
#define IDX_CAC_WRITE_LOCK(i, o)		xt_xlock_rwlock_ns(&(i)->cs_lock)
#define IDX_CAC_UNLOCK(i, o)			xt_unlock_rwlock_ns(&(i)->cs_lock)
#elif defined(IDX_CAC_USE_RWMUTEX)
#define IDX_CAC_LOCK_TYPE				XTRWMutexRec
#define IDX_CAC_INIT_LOCK(s, i)			xt_rwmutex_init_with_autoname(s, &(i)->cs_lock)
#define IDX_CAC_FREE_LOCK(s, i)			xt_rwmutex_free(s, &(i)->cs_lock)	
#define IDX_CAC_READ_LOCK(i, o)			xt_rwmutex_slock(&(i)->cs_lock, (o)->t_id)
#define IDX_CAC_WRITE_LOCK(i, o)		xt_rwmutex_xlock(&(i)->cs_lock, (o)->t_id)
#define IDX_CAC_UNLOCK(i, o)			xt_rwmutex_unlock(&(i)->cs_lock, (o)->t_id)
#elif defined(IDX_CAC_USE_SPINXSLOCK)
#define IDX_CAC_LOCK_TYPE				XTSpinXSLockRec
#define IDX_CAC_INIT_LOCK(s, i)			xt_spinxslock_init_with_autoname(s, &(i)->cs_lock)
#define IDX_CAC_FREE_LOCK(s, i)			xt_spinxslock_free(s, &(i)->cs_lock)	
#define IDX_CAC_READ_LOCK(i, s)			xt_spinxslock_slock(&(i)->cs_lock, (s)->t_id)
#define IDX_CAC_WRITE_LOCK(i, s)		xt_spinxslock_xlock(&(i)->cs_lock, (s)->t_id)
#define IDX_CAC_UNLOCK(i, s)			xt_spinxslock_unlock(&(i)->cs_lock, (s)->t_id)
#endif

#define ID_HANDLE_USE_SPINLOCK
//#define ID_HANDLE_USE_PTHREAD_RW

#if defined(ID_HANDLE_USE_PTHREAD_RW)
#define ID_HANDLE_LOCK_TYPE				xt_mutex_type
#define ID_HANDLE_INIT_LOCK(s, i)		xt_init_mutex_with_autoname(s, i)
#define ID_HANDLE_FREE_LOCK(s, i)		xt_free_mutex(i)	
#define ID_HANDLE_LOCK(i)				xt_lock_mutex_ns(i)
#define ID_HANDLE_UNLOCK(i)				xt_unlock_mutex_ns(i)
#elif defined(ID_HANDLE_USE_SPINLOCK)
#define ID_HANDLE_LOCK_TYPE				XTSpinLockRec
#define ID_HANDLE_INIT_LOCK(s, i)		xt_spinlock_init_with_autoname(s, i)
#define ID_HANDLE_FREE_LOCK(s, i)		xt_spinlock_free(s, i)	
#define ID_HANDLE_LOCK(i)				xt_spinlock_lock(i)
#define ID_HANDLE_UNLOCK(i)				xt_spinlock_unlock(i)
#endif

#define XT_HANDLE_SLOTS					37

/*
#ifdef DEBUG
#define XT_INIT_HANDLE_COUNT			0
#define XT_INIT_HANDLE_BLOCKS			0
#else
#define XT_INIT_HANDLE_COUNT			40
#define XT_INIT_HANDLE_BLOCKS			10
#endif
*/

/* A disk cache segment. The cache is divided into a number of segments
 * to improve concurrency.
 */
typedef struct DcSegment {
	IDX_CAC_LOCK_TYPE	cs_lock;						/* The cache segment lock. */
	XTIndBlockPtr		*cs_hash_table;
} DcSegmentRec, *DcSegmentPtr;

typedef struct DcHandleSlot {
	ID_HANDLE_LOCK_TYPE	hs_handles_lock;
	XTIndHandleBlockPtr	hs_free_blocks;
	XTIndHandlePtr		hs_free_handles;
	XTIndHandlePtr		hs_used_handles;
} DcHandleSlotRec, *DcHandleSlotPtr;

typedef struct DcGlobals {
	xt_mutex_type		cg_lock;						/* The public cache lock. */
	DcSegmentRec		cg_segment[IDX_CAC_SEGMENT_COUNT];
	XTIndBlockPtr		cg_blocks;
#ifdef XT_USE_DIRECT_IO_ON_INDEX
	xtWord1				*cg_buffer;
#endif
	XTIndBlockPtr		cg_free_list;
	xtWord4				cg_free_count;
	xtWord4				cg_ru_now;						/* A counter as described by Jim Starkey (my thanks) */
	XTIndBlockPtr		cg_lru_block;
	XTIndBlockPtr		cg_mru_block;
	xtWord4				cg_hash_size;
	xtWord4				cg_block_count;
	xtWord4				cg_max_free;
#ifdef DEBUG_CHECK_IND_CACHE
	u_int				cg_reserved_by_ots;				/* Number of blocks reserved by open tables. */
	u_int				cg_read_count;					/* Number of blocks being read. */
#endif

	/* Index cache handles: */
	DcHandleSlotRec		cg_handle_slot[XT_HANDLE_SLOTS];
} DcGlobalsRec;

static DcGlobalsRec	ind_cac_globals;

#ifdef XT_USE_MYSYS
#ifdef xtPublic
#undef xtPublic
#endif
#include "my_global.h"
#include "my_sys.h"
#include "keycache.h"
KEY_CACHE my_cache;
#undef	pthread_rwlock_rdlock
#undef	pthread_rwlock_wrlock
#undef	pthread_rwlock_unlock
#undef	pthread_mutex_lock
#undef	pthread_mutex_unlock
#undef	pthread_cond_wait
#undef	pthread_cond_broadcast
#undef	xt_mutex_type
#define xtPublic
#endif

/*
 * -----------------------------------------------------------------------
 * INDEX CACHE HANDLES
 */

static XTIndHandlePtr ind_alloc_handle()
{
	XTIndHandlePtr handle;

	if (!(handle = (XTIndHandlePtr) xt_calloc_ns(sizeof(XTIndHandleRec))))
		return NULL;
	xt_spinlock_init_with_autoname(NULL, &handle->ih_lock);
	return handle;
}

static void ind_free_handle(XTIndHandlePtr handle)
{
	xt_spinlock_free(NULL, &handle->ih_lock);
	xt_free_ns(handle);
}

static void ind_handle_exit(XTThreadPtr self)
{
	DcHandleSlotPtr		hs;
	XTIndHandlePtr		handle;
	XTIndHandleBlockPtr	hptr;

	for (int i=0; i<XT_HANDLE_SLOTS; i++) {
		hs = &ind_cac_globals.cg_handle_slot[i];

		while (hs->hs_used_handles) {
			handle = hs->hs_used_handles;
			xt_ind_release_handle(handle, FALSE, self);
		}

		while (hs->hs_free_blocks) {
			hptr = hs->hs_free_blocks;
			hs->hs_free_blocks = hptr->hb_next;
			xt_free(self, hptr);
		}

		while (hs->hs_free_handles) {
			handle = hs->hs_free_handles;
			hs->hs_free_handles = handle->ih_next;
			ind_free_handle(handle);
		}

		ID_HANDLE_FREE_LOCK(self, &hs->hs_handles_lock);
	}
}

static void ind_handle_init(XTThreadPtr self)
{
	DcHandleSlotPtr		hs;

	for (int i=0; i<XT_HANDLE_SLOTS; i++) {
		hs = &ind_cac_globals.cg_handle_slot[i];
		memset(hs, 0, sizeof(DcHandleSlotRec));
		ID_HANDLE_INIT_LOCK(self, &hs->hs_handles_lock);
	}
}

//#define CHECK_HANDLE_STRUCTS

#ifdef CHECK_HANDLE_STRUCTS
static int gdummy = 0;

static void ic_stop_here()
{
	gdummy = gdummy + 1;
	printf("Nooo %d!\n", gdummy);
}

static void ic_check_handle_structs()
{
	XTIndHandlePtr		handle, phandle;
	XTIndHandleBlockPtr	hptr, phptr;
	int					count = 0;
	int					ctest;

	phandle = NULL;
	handle = ind_cac_globals.cg_used_handles;
	while (handle) {
		if (handle == phandle)
			ic_stop_here();
		if (handle->ih_prev != phandle)
			ic_stop_here();
		if (handle->ih_cache_reference) {
			ctest = handle->x.ih_cache_block->cb_handle_count;
			if (ctest == 0 || ctest > 100)
				ic_stop_here();
		}
		else {
			ctest = handle->x.ih_handle_block->hb_ref_count;
			if (ctest == 0 || ctest > 100)
				ic_stop_here();
		}
		phandle = handle;
		handle = handle->ih_next;
		count++;
		if (count > 1000)
			ic_stop_here();
	}

	count = 0;
	hptr = ind_cac_globals.cg_free_blocks;
	while (hptr) {
		if (hptr == phptr)
			ic_stop_here();
		phptr = hptr;
		hptr = hptr->hb_next;
		count++;
		if (count > 1000)
			ic_stop_here();
	}

	count = 0;
	handle = ind_cac_globals.cg_free_handles;
	while (handle) {
		if (handle == phandle)
			ic_stop_here();
		phandle = handle;
		handle = handle->ih_next;
		count++;
		if (count > 1000)
			ic_stop_here();
	}
}
#endif

/*
 * Get a handle to the index block.
 * This function is called by index scanners (readers).
 */
xtPublic XTIndHandlePtr xt_ind_get_handle(XTOpenTablePtr ot, XTIndexPtr ind, XTIndReferencePtr iref)
{
	DcHandleSlotPtr	hs;
	XTIndHandlePtr	handle;

	hs = &ind_cac_globals.cg_handle_slot[iref->ir_block->cb_address % XT_HANDLE_SLOTS];

	ASSERT_NS(iref->ir_xlock == FALSE);
	ASSERT_NS(iref->ir_updated == FALSE);
	ID_HANDLE_LOCK(&hs->hs_handles_lock);
#ifdef CHECK_HANDLE_STRUCTS
	ic_check_handle_structs();
#endif
	if ((handle = hs->hs_free_handles))
		hs->hs_free_handles = handle->ih_next;
	else {
		if (!(handle = ind_alloc_handle())) {
			ID_HANDLE_UNLOCK(&hs->hs_handles_lock);
			xt_ind_release(ot, ind, XT_UNLOCK_READ, iref);
			return NULL;
		}
	}
	if (hs->hs_used_handles)
		hs->hs_used_handles->ih_prev = handle;
	handle->ih_next = hs->hs_used_handles;
	handle->ih_prev = NULL;
	handle->ih_address = iref->ir_block->cb_address;
	handle->ih_cache_reference = TRUE;
	handle->x.ih_cache_block = iref->ir_block;
	handle->ih_branch = iref->ir_branch;
	/* {HANDLE-COUNT-USAGE}
	 * This is safe because:
	 *
	 * I have an Slock on the cache block, and I have
	 * at least an Slock on the index.
	 * So this excludes anyone who is reading 
	 * cb_handle_count in the index.
	 * (all cache block writers, and the freeer).
	 *
	 * The increment is safe because I have the list
	 * lock (hs_handles_lock), which is required by anyone else
	 * who increments or decrements this value.
	 */
	iref->ir_block->cb_handle_count++;
	hs->hs_used_handles = handle;
#ifdef CHECK_HANDLE_STRUCTS
	ic_check_handle_structs();
#endif
	ID_HANDLE_UNLOCK(&hs->hs_handles_lock);
	xt_ind_release(ot, ind, XT_UNLOCK_READ, iref);
	return handle;
}

xtPublic void xt_ind_release_handle(XTIndHandlePtr handle, xtBool have_lock, XTThreadPtr thread)
{
	DcHandleSlotPtr	hs;
	XTIndBlockPtr	block = NULL;
	u_int		hash_idx = NULL;
	DcSegmentPtr	seg = NULL;
	XTIndBlockPtr	xblock;

	/* The lock order is:
	 * 1. Cache segment (cs_lock) - This is only by ind_free_block()!
	 * 1. S/Slock cache block (cb_lock)
	 * 2. List lock (cg_handles_lock).
	 * 3. Handle lock (ih_lock)
	 */
	if (!have_lock)
		xt_spinlock_lock(&handle->ih_lock);

	/* Get the lock on the cache page if required: */
	if (handle->ih_cache_reference) {
		u_int			file_id;
		xtIndexNodeID	address;

		block = handle->x.ih_cache_block;

		file_id = block->cb_file_id;
		address = block->cb_address;
		hash_idx = XT_NODE_ID(address) + (file_id * 223);
		seg = &ind_cac_globals.cg_segment[hash_idx & IDX_CAC_SEGMENT_MASK];
		hash_idx = (hash_idx >> XT_INDEX_CACHE_SEGMENT_SHIFTS) % ind_cac_globals.cg_hash_size;
	}

	xt_spinlock_unlock(&handle->ih_lock);

	/* Because of the lock order, I have to release the
	 * handle before I get a lock on the cache block.
	 *
	 * But, by doing this, thie cache block may be gone!
	 */
	if (block) {
		IDX_CAC_READ_LOCK(seg, thread);
		xblock = seg->cs_hash_table[hash_idx];
		while (xblock) {
			if (block == xblock) {
				/* Found the block... 
				 * {HANDLE-COUNT-SLOCK}
				 * 04.05.2009, changed to slock.
				 */
				XT_IPAGE_READ_LOCK(&block->cb_lock);
				goto block_found;
			}
			xblock = xblock->cb_next;
		}
		block = NULL;
		block_found:
		IDX_CAC_UNLOCK(seg, thread);
	}

	hs = &ind_cac_globals.cg_handle_slot[handle->ih_address % XT_HANDLE_SLOTS];

	ID_HANDLE_LOCK(&hs->hs_handles_lock);
#ifdef CHECK_HANDLE_STRUCTS
	ic_check_handle_structs();
#endif

	/* I don't need to lock the handle because I have locked
	 * the list, and no other thread can change the
	 * handle without first getting a lock on the list.
	 *
	 * In addition, the caller is the only owner of the
	 * handle, and the only thread with an independent
	 * reference to the handle.
	 * All other access occur over the list.
	 */

	/* Remove the reference to the cache or a handle block: */
	if (handle->ih_cache_reference) {
		ASSERT_NS(block == handle->x.ih_cache_block);
		ASSERT_NS(block && block->cb_handle_count > 0);
		/* {HANDLE-COUNT-USAGE}
		 * This is safe here because I have excluded
		 * all readers by taking an Xlock on the
		 * cache block (CHANGED - see below).
		 *
		 * {HANDLE-COUNT-SLOCK}
		 * 04.05.2009, changed to slock.
		 * Should be OK, because:
		 * A have a lock on the list lock (hs_handles_lock),
		 * which prevents concurrent updates to cb_handle_count.
		 *
		 * I have also have a read lock on the cache block
		 * but not a lock on the index. As a result, we cannot
		 * excluded all index writers (and readers of 
		 * cb_handle_count.
		 */
		block->cb_handle_count--;
	}
	else {
		XTIndHandleBlockPtr	hptr = handle->x.ih_handle_block;

		ASSERT_NS(!handle->ih_cache_reference);
		ASSERT_NS(hptr->hb_ref_count > 0);
		hptr->hb_ref_count--;
		if (!hptr->hb_ref_count) {
			/* Put it back on the free list: */
			hptr->hb_next = hs->hs_free_blocks;
			hs->hs_free_blocks = hptr;
		}
	}

	/* Unlink the handle: */
	if (handle->ih_next)
		handle->ih_next->ih_prev = handle->ih_prev;
	if (handle->ih_prev)
		handle->ih_prev->ih_next = handle->ih_next;
	if (hs->hs_used_handles == handle)
		hs->hs_used_handles = handle->ih_next;

	/* Put it on the free list: */
	handle->ih_next = hs->hs_free_handles;
	hs->hs_free_handles = handle;

#ifdef CHECK_HANDLE_STRUCTS
	ic_check_handle_structs();
#endif
	ID_HANDLE_UNLOCK(&hs->hs_handles_lock);

	if (block)
		XT_IPAGE_UNLOCK(&block->cb_lock, FALSE);
}

/* Call this function before a referenced cache block is modified!
 * This function is called by index updaters.
 */
xtPublic xtBool xt_ind_copy_on_write(XTIndReferencePtr iref)
{
	DcHandleSlotPtr		hs;
	XTIndHandleBlockPtr	hptr;
	u_int				branch_size;
	XTIndHandlePtr		handle;
	u_int				i = 0;

	hs = &ind_cac_globals.cg_handle_slot[iref->ir_block->cb_address % XT_HANDLE_SLOTS];

	ID_HANDLE_LOCK(&hs->hs_handles_lock);

	/* {HANDLE-COUNT-USAGE}
	 * This is only called by updaters of this index block, or
	 * the free which holds an Xlock on the index block.
	 * These are all mutually exclusive for the index block.
	 *
	 * {HANDLE-COUNT-SLOCK}
	 * Do this check again, after we have the list lock (hs_handles_lock).
	 * There is a small chance that the count has changed, since we last
	 * checked because xt_ind_release_handle() only holds
	 * an slock on the index page.
	 *
	 * An updater can sometimes have a XLOCK on the index and an slock
	 * on the cache block. In this case xt_ind_release_handle()
	 * could have run through.
	 */
	if (!iref->ir_block->cb_handle_count) {
		ID_HANDLE_UNLOCK(&hs->hs_handles_lock);
		return OK;
	}

#ifdef CHECK_HANDLE_STRUCTS
	ic_check_handle_structs();
#endif
	if ((hptr = hs->hs_free_blocks))
		hs->hs_free_blocks = hptr->hb_next;
	else {
		if (!(hptr = (XTIndHandleBlockPtr) xt_malloc_ns(sizeof(XTIndHandleBlockRec)))) {
			ID_HANDLE_UNLOCK(&hs->hs_handles_lock);
			return FAILED;
		}
	}

	branch_size = XT_GET_INDEX_BLOCK_LEN(XT_GET_DISK_2(iref->ir_branch->tb_size_2));
	memcpy(&hptr->hb_branch, iref->ir_branch, branch_size);
	hptr->hb_ref_count = iref->ir_block->cb_handle_count;

	handle = hs->hs_used_handles;
	while (handle) {
		if (handle->ih_branch == iref->ir_branch) {
			i++;
			xt_spinlock_lock(&handle->ih_lock);
			ASSERT_NS(handle->ih_cache_reference);
			handle->ih_cache_reference = FALSE;
			handle->x.ih_handle_block = hptr;
			handle->ih_branch = &hptr->hb_branch;
			xt_spinlock_unlock(&handle->ih_lock);
#ifndef DEBUG
			if (i == hptr->hb_ref_count)
				break;
#endif
		}
		handle = handle->ih_next;
	}
#ifdef DEBUG
	ASSERT_NS(hptr->hb_ref_count == i);
#endif
	/* {HANDLE-COUNT-USAGE}
	 * It is safe to modify cb_handle_count when I have the
	 * list lock, and I have excluded all readers!
	 */
	iref->ir_block->cb_handle_count = 0;
#ifdef CHECK_HANDLE_STRUCTS
	ic_check_handle_structs();
#endif
	ID_HANDLE_UNLOCK(&hs->hs_handles_lock);

	return OK;
}

xtPublic void xt_ind_lock_handle(XTIndHandlePtr handle)
{
	xt_spinlock_lock(&handle->ih_lock);
}

xtPublic void xt_ind_unlock_handle(XTIndHandlePtr handle)
{
	xt_spinlock_unlock(&handle->ih_lock);
}

/*
 * -----------------------------------------------------------------------
 * INIT/EXIT
 */

/*
 * Initialize the disk cache.
 */
xtPublic void xt_ind_init(XTThreadPtr self, size_t cache_size)
{
	XTIndBlockPtr	block;

#ifdef XT_USE_MYSYS
	init_key_cache(&my_cache, 1024, cache_size, 100, 300);
#endif
	/* Memory is devoted to the page data alone, I no longer count the size of the directory,
	 * or the page overhead: */
	ind_cac_globals.cg_block_count = cache_size / XT_INDEX_PAGE_SIZE;
	ind_cac_globals.cg_hash_size = ind_cac_globals.cg_block_count / (IDX_CAC_SEGMENT_COUNT >> 1);
	ind_cac_globals.cg_max_free = ind_cac_globals.cg_block_count / 10;
	if (ind_cac_globals.cg_max_free < 8)
		ind_cac_globals.cg_max_free = 8;
	if (ind_cac_globals.cg_max_free > 128)
		ind_cac_globals.cg_max_free = 128;

	try_(a) {
		for (u_int i=0; i<IDX_CAC_SEGMENT_COUNT; i++) {
			ind_cac_globals.cg_segment[i].cs_hash_table = (XTIndBlockPtr *) xt_calloc(self, ind_cac_globals.cg_hash_size * sizeof(XTIndBlockPtr));
			IDX_CAC_INIT_LOCK(self, &ind_cac_globals.cg_segment[i]);
		}

		block = (XTIndBlockPtr) xt_malloc(self, ind_cac_globals.cg_block_count * sizeof(XTIndBlockRec));
		ind_cac_globals.cg_blocks = block;
		xt_init_mutex_with_autoname(self, &ind_cac_globals.cg_lock);
#ifdef XT_USE_DIRECT_IO_ON_INDEX
		xtWord1 *buffer;
#ifdef XT_WIN
		size_t	psize = 512;
#else
		size_t	psize = getpagesize();
#endif
		size_t	diff;

		buffer = (xtWord1 *) xt_malloc(self, (ind_cac_globals.cg_block_count * XT_INDEX_PAGE_SIZE));
		diff = (size_t) buffer % psize;
		if (diff != 0) {
			xt_free(self, buffer);
			buffer = (xtWord1 *) xt_malloc(self, (ind_cac_globals.cg_block_count * XT_INDEX_PAGE_SIZE) + psize);
			diff = (size_t) buffer % psize;
			if (diff != 0)
				diff = psize - diff;
		}
		ind_cac_globals.cg_buffer = buffer;
		buffer += diff;
#endif

		for (u_int i=0; i<ind_cac_globals.cg_block_count; i++) {
			XT_IPAGE_INIT_LOCK(self, &block->cb_lock);
			block->cb_state = IDX_CAC_BLOCK_FREE;
			block->cb_next = ind_cac_globals.cg_free_list;
#ifdef XT_USE_DIRECT_IO_ON_INDEX
			block->cb_data = buffer;
			buffer += XT_INDEX_PAGE_SIZE;
#endif
			ind_cac_globals.cg_free_list = block;
			block++;
		}
		ind_cac_globals.cg_free_count = ind_cac_globals.cg_block_count;
#ifdef DEBUG_CHECK_IND_CACHE
		ind_cac_globals.cg_reserved_by_ots = 0;
#endif
		ind_handle_init(self);
	}
	catch_(a) {
		xt_ind_exit(self);
		throw_();
	}
	cont_(a);
}

xtPublic void xt_ind_exit(XTThreadPtr self)
{
#ifdef XT_USE_MYSYS
	end_key_cache(&my_cache, 1);
#endif
	for (u_int i=0; i<IDX_CAC_SEGMENT_COUNT; i++) {
		if (ind_cac_globals.cg_segment[i].cs_hash_table) {
			xt_free(self, ind_cac_globals.cg_segment[i].cs_hash_table);
			ind_cac_globals.cg_segment[i].cs_hash_table = NULL;
			IDX_CAC_FREE_LOCK(self, &ind_cac_globals.cg_segment[i]);
		}
	}

	if (ind_cac_globals.cg_blocks) {
		xt_free(self, ind_cac_globals.cg_blocks);
		ind_cac_globals.cg_blocks = NULL;
		xt_free_mutex(&ind_cac_globals.cg_lock);
	}
#ifdef XT_USE_DIRECT_IO_ON_INDEX
	if (ind_cac_globals.cg_buffer) {
		xt_free(self, ind_cac_globals.cg_buffer);
		ind_cac_globals.cg_buffer = NULL;
	}
#endif
	ind_handle_exit(self);

	memset(&ind_cac_globals, 0, sizeof(ind_cac_globals));
}

xtPublic xtInt8 xt_ind_get_usage()
{
	xtInt8 size = 0;

	size = (xtInt8) (ind_cac_globals.cg_block_count - ind_cac_globals.cg_free_count) * (xtInt8) XT_INDEX_PAGE_SIZE;
	return size;
}

xtPublic xtInt8 xt_ind_get_size()
{
	xtInt8 size = 0;

	size = (xtInt8) ind_cac_globals.cg_block_count * (xtInt8) XT_INDEX_PAGE_SIZE;
	return size;
}

/*
 * -----------------------------------------------------------------------
 * INDEX CHECKING
 */

xtPublic void xt_ind_check_cache(XTIndexPtr ind)
{
	XTIndBlockPtr	block;
	u_int			free_count, inuse_count, clean_count;
	xtBool			check_count = FALSE;

	if (ind == (XTIndex *) 1) {
		ind = NULL;
		check_count = TRUE;
	}

	// Check the dirty list:
	if (ind) {
		u_int cnt = 0;

		block = ind->mi_dirty_list;
		while (block) {
			cnt++;
			ASSERT_NS(block->cb_state == IDX_CAC_BLOCK_DIRTY);
			block = block->cb_dirty_next;
		}
		ASSERT_NS(ind->mi_dirty_blocks == cnt);
	}

	xt_lock_mutex_ns(&ind_cac_globals.cg_lock);

	// Check the free list:
	free_count = 0;
	block = ind_cac_globals.cg_free_list;
	while (block) {
		free_count++;
		ASSERT_NS(block->cb_state == IDX_CAC_BLOCK_FREE);
		block = block->cb_next;
	}
	ASSERT_NS(ind_cac_globals.cg_free_count == free_count);

	/* Check the LRU list: */
	XTIndBlockPtr list_block, plist_block;
	
	plist_block = NULL;
	list_block = ind_cac_globals.cg_lru_block;
	if (list_block) {
		ASSERT_NS(ind_cac_globals.cg_mru_block != NULL);
		ASSERT_NS(ind_cac_globals.cg_mru_block->cb_mr_used == NULL);
		ASSERT_NS(list_block->cb_lr_used == NULL);
		inuse_count = 0;
		clean_count = 0;
		while (list_block) {
			inuse_count++;
			ASSERT_NS(list_block->cb_state == IDX_CAC_BLOCK_DIRTY || list_block->cb_state == IDX_CAC_BLOCK_CLEAN);
			if (list_block->cb_state == IDX_CAC_BLOCK_CLEAN)
				clean_count++;
			ASSERT_NS(block != list_block);
			ASSERT_NS(list_block->cb_lr_used == plist_block);
			plist_block = list_block;
			list_block = list_block->cb_mr_used;
		}
		ASSERT_NS(ind_cac_globals.cg_mru_block == plist_block);
	}
	else {
		inuse_count = 0;
		clean_count = 0;
		ASSERT_NS(ind_cac_globals.cg_mru_block == NULL);
	}

#ifdef DEBUG_CHECK_IND_CACHE
	ASSERT_NS(free_count + inuse_count + ind_cac_globals.cg_reserved_by_ots + ind_cac_globals.cg_read_count == ind_cac_globals.cg_block_count);
#endif
	xt_unlock_mutex_ns(&ind_cac_globals.cg_lock);
	if (check_count) {
		/* We have just flushed, check how much is now free/clean. */
		if (free_count + clean_count < 10) {
			/* This could be a problem: */
			printf("Cache very low!\n");
		}
	}
}

#ifdef XXXXDEBUG
static void ind_cac_check_on_dirty_list(DcSegmentPtr seg, XTIndBlockPtr block)
{
	XTIndBlockPtr	list_block, plist_block;
	xtBool		found = FALSE;
	
	plist_block = NULL;
	list_block = seg->cs_dirty_list[block->cb_file_id % XT_INDEX_CACHE_FILE_SLOTS];
	while (list_block) {
		ASSERT_NS(list_block->cb_state == IDX_CAC_BLOCK_DIRTY);
		ASSERT_NS(list_block->cb_dirty_prev == plist_block);
		if (list_block == block)
			found = TRUE;
		plist_block = list_block;
		list_block = list_block->cb_dirty_next;
	}
	ASSERT_NS(found);
}

static void ind_cac_check_dirty_list(DcSegmentPtr seg, XTIndBlockPtr block)
{
	XTIndBlockPtr list_block, plist_block;
	
	for (u_int j=0; j<XT_INDEX_CACHE_FILE_SLOTS; j++) {
		plist_block = NULL;
		list_block = seg->cs_dirty_list[j];
		while (list_block) {
			ASSERT_NS(list_block->cb_state == IDX_CAC_BLOCK_DIRTY);
			ASSERT_NS(block != list_block);
			ASSERT_NS(list_block->cb_dirty_prev == plist_block);
			plist_block = list_block;
			list_block = list_block->cb_dirty_next;
		}
	}
}

#endif

/*
 * -----------------------------------------------------------------------
 * FREEING INDEX CACHE
 */

/*
 * This function return TRUE if the block is freed. 
 * This function returns FALSE if the block cannot be found, or the
 * block is not clean.
 *
 * We also return FALSE if we cannot copy the block to the handle
 * (if this is required). This will be due to out-of-memory!
 */
static xtBool ind_free_block(XTOpenTablePtr ot, XTIndBlockPtr block)
{
	XTIndBlockPtr	xblock, pxblock;
	u_int			hash_idx;
	u_int			file_id;
	xtIndexNodeID	address;
	DcSegmentPtr	seg;

#ifdef DEBUG_CHECK_IND_CACHE
	xt_ind_check_cache(NULL);
#endif
	file_id = block->cb_file_id;
	address = block->cb_address;

	hash_idx = XT_NODE_ID(address) + (file_id * 223);
	seg = &ind_cac_globals.cg_segment[hash_idx & IDX_CAC_SEGMENT_MASK];
	hash_idx = (hash_idx >> XT_INDEX_CACHE_SEGMENT_SHIFTS) % ind_cac_globals.cg_hash_size;

	IDX_CAC_WRITE_LOCK(seg, ot->ot_thread);

	pxblock = NULL;
	xblock = seg->cs_hash_table[hash_idx];
	while (xblock) {
		if (block == xblock) {
			/* Found the block... */
			XT_IPAGE_WRITE_LOCK(&block->cb_lock, ot->ot_thread->t_id);
			if (block->cb_state != IDX_CAC_BLOCK_CLEAN) {
				/* This block cannot be freeed: */
				XT_IPAGE_UNLOCK(&block->cb_lock, TRUE);
				IDX_CAC_UNLOCK(seg, ot->ot_thread);
#ifdef DEBUG_CHECK_IND_CACHE
				xt_ind_check_cache(NULL);
#endif
				return FALSE;
			}
			
			goto free_the_block;
		}
		pxblock = xblock;
		xblock = xblock->cb_next;
	}

	IDX_CAC_UNLOCK(seg, ot->ot_thread);

	/* Not found (this can happen, if block was freed by another thread) */
#ifdef DEBUG_CHECK_IND_CACHE
	xt_ind_check_cache(NULL);
#endif
	return FALSE;

	free_the_block:

	/* If the block is reference by a handle, then we
	 * have to copy the data to the handle before we
	 * free the page:
	 */
	/* {HANDLE-COUNT-USAGE}
	 * This access is safe because:
	 *
	 * We have an Xlock on the cache block, which excludes
	 * all other writers that want to change the cache block
	 * and also all readers of the cache block, because
	 * they all have at least an Slock on the cache block.
	 */
	if (block->cb_handle_count) {
		XTIndReferenceRec	iref;
		
		iref.ir_xlock = TRUE;
		iref.ir_updated = FALSE;
		iref.ir_block = block;
		iref.ir_branch = (XTIdxBranchDPtr) block->cb_data;
		if (!xt_ind_copy_on_write(&iref)) {
			XT_IPAGE_UNLOCK(&block->cb_lock, TRUE);
			return FALSE;
		}
	}

	/* Block is clean, remove from the hash table: */
	if (pxblock)
		pxblock->cb_next = block->cb_next;
	else
		seg->cs_hash_table[hash_idx] = block->cb_next;

	xt_lock_mutex_ns(&ind_cac_globals.cg_lock);

	/* Remove from the MRU list: */
	if (ind_cac_globals.cg_lru_block == block)
		ind_cac_globals.cg_lru_block = block->cb_mr_used;
	if (ind_cac_globals.cg_mru_block == block)
		ind_cac_globals.cg_mru_block = block->cb_lr_used;
	
	/* Note, I am updating blocks for which I have no lock
	 * here. But I think this is OK because I have a lock
	 * for the MRU list.
	 */
	if (block->cb_lr_used)
		block->cb_lr_used->cb_mr_used = block->cb_mr_used;
	if (block->cb_mr_used)
		block->cb_mr_used->cb_lr_used = block->cb_lr_used;

	/* The block is now free: */
	block->cb_next = ind_cac_globals.cg_free_list;
	ind_cac_globals.cg_free_list = block;
	ind_cac_globals.cg_free_count++;
	block->cb_state = IDX_CAC_BLOCK_FREE;
	IDX_TRACE("%d- f%x\n", (int) XT_NODE_ID(address), (int) XT_GET_DISK_2(block->cb_data));

	/* Unlock BEFORE the block is reused! */
	XT_IPAGE_UNLOCK(&block->cb_lock, TRUE);

	xt_unlock_mutex_ns(&ind_cac_globals.cg_lock);

	IDX_CAC_UNLOCK(seg, ot->ot_thread);

#ifdef DEBUG_CHECK_IND_CACHE
	xt_ind_check_cache(NULL);
#endif
	return TRUE;
}

#define IND_CACHE_MAX_BLOCKS_TO_FREE		100

/*
 * Return the number of blocks freed.
 *
 * The idea is to grab a list of blocks to free.
 * The list consists of the LRU blocks that are
 * clean.
 *
 * Free as many as possible (up to max of blocks_required)
 * from the list, even if LRU position has changed
 * (or we have a race if there are too few blocks).
 * However, if the block cannot be found, or is dirty
 * we must skip it.
 *
 * Repeat until we find no blocks for the list, or
 * we have freed 'blocks_required'.
 *
 * 'not_this' is a block that must not be freed because
 * it is locked by the calling thread!
 */
static u_int ind_cac_free_lru_blocks(XTOpenTablePtr ot, u_int blocks_required, XTIdxBranchDPtr not_this)
{
	register DcGlobalsRec	*dcg = &ind_cac_globals;
	XTIndBlockPtr			to_free[IND_CACHE_MAX_BLOCKS_TO_FREE];
	int						count;
	XTIndBlockPtr			block;
	u_int					blocks_freed = 0;
	XTIndBlockPtr			locked_block;

#ifdef XT_USE_DIRECT_IO_ON_INDEX
#error This will not work!
#endif
	locked_block = (XTIndBlockPtr) ((xtWord1 *) not_this - offsetof(XTIndBlockRec, cb_data));

	retry:
	xt_lock_mutex_ns(&ind_cac_globals.cg_lock);
	block = dcg->cg_lru_block;
	count = 0;
	while (block && count < IND_CACHE_MAX_BLOCKS_TO_FREE) {
		if (block != locked_block && block->cb_state == IDX_CAC_BLOCK_CLEAN) {
			to_free[count] = block;
			count++;
		}
		block = block->cb_mr_used;
	}
	xt_unlock_mutex_ns(&ind_cac_globals.cg_lock);

	if (!count)
		return blocks_freed;

	for (int i=0; i<count; i++) {
		if (ind_free_block(ot, to_free[i]))
			blocks_freed++;
		if (blocks_freed >= blocks_required &&
			ind_cac_globals.cg_free_count >= ind_cac_globals.cg_max_free + blocks_required)
		return blocks_freed;
	}

	goto retry;
}

/*
 * -----------------------------------------------------------------------
 * MAIN CACHE FUNCTIONS
 */

/*
 * Fetch the block. Note, if we are about to write the block
 * then there is no need to read it from disk!
 */
static XTIndBlockPtr ind_cac_fetch(XTOpenTablePtr ot, XTIndexPtr ind, xtIndexNodeID address, DcSegmentPtr *ret_seg, xtBool read_data)
{
	register XTOpenFilePtr	file = ot->ot_ind_file;
	register XTIndBlockPtr	block, new_block;
	register DcSegmentPtr	seg;
	register u_int			hash_idx;
	register DcGlobalsRec	*dcg = &ind_cac_globals;
	size_t					red_size;

#ifdef DEBUG_CHECK_IND_CACHE
	xt_ind_check_cache(NULL);
#endif
	/* Address, plus file ID multiplied by my favorite prime number! */
	hash_idx = XT_NODE_ID(address) + (file->fr_id * 223);
	seg = &dcg->cg_segment[hash_idx & IDX_CAC_SEGMENT_MASK];
	hash_idx = (hash_idx >> XT_INDEX_CACHE_SEGMENT_SHIFTS) % dcg->cg_hash_size;

	IDX_CAC_READ_LOCK(seg, ot->ot_thread);
	block = seg->cs_hash_table[hash_idx];
	while (block) {
		if (XT_NODE_ID(block->cb_address) == XT_NODE_ID(address) && block->cb_file_id == file->fr_id) {
			ASSERT_NS(block->cb_state != IDX_CAC_BLOCK_FREE);

			/* Check how recently this page has been used: */
			if (XT_TIME_DIFF(block->cb_ru_time, dcg->cg_ru_now) > (dcg->cg_block_count >> 1)) {
				xt_lock_mutex_ns(&dcg->cg_lock);

				/* Move to the front of the MRU list: */
				block->cb_ru_time = ++dcg->cg_ru_now;
				if (dcg->cg_mru_block != block) {
					/* Remove from the MRU list: */
					if (dcg->cg_lru_block == block)
						dcg->cg_lru_block = block->cb_mr_used;
					if (block->cb_lr_used)
						block->cb_lr_used->cb_mr_used = block->cb_mr_used;
					if (block->cb_mr_used)
						block->cb_mr_used->cb_lr_used = block->cb_lr_used;

					/* Make the block the most recently used: */
					if ((block->cb_lr_used = dcg->cg_mru_block))
						dcg->cg_mru_block->cb_mr_used = block;
					block->cb_mr_used = NULL;
					dcg->cg_mru_block = block;
					if (!dcg->cg_lru_block)
						dcg->cg_lru_block = block;
				}

				xt_unlock_mutex_ns(&dcg->cg_lock);
			}
		
			*ret_seg = seg;
#ifdef DEBUG_CHECK_IND_CACHE
			xt_ind_check_cache(NULL);
#endif
			ot->ot_thread->st_statistics.st_ind_cache_hit++;
			return block;
		}
		block = block->cb_next;
	}
	
	/* Block not found... */
	IDX_CAC_UNLOCK(seg, ot->ot_thread);

	/* Check the open table reserve list first: */
	if ((new_block = ot->ot_ind_res_bufs)) {
		ot->ot_ind_res_bufs = new_block->cb_next;
		ot->ot_ind_res_count--;
#ifdef DEBUG_CHECK_IND_CACHE
		xt_lock_mutex_ns(&dcg->cg_lock);
		dcg->cg_reserved_by_ots--;
		dcg->cg_read_count++;
		xt_unlock_mutex_ns(&dcg->cg_lock);
#endif
		goto use_free_block;
	}

	free_some_blocks:
	if (!dcg->cg_free_list) {
		if (!ind_cac_free_lru_blocks(ot, 1, NULL)) {
			if (!dcg->cg_free_list) {
				xt_register_xterr(XT_REG_CONTEXT, XT_ERR_NO_INDEX_CACHE);
#ifdef DEBUG_CHECK_IND_CACHE
				xt_ind_check_cache(NULL);
#endif
				return NULL;
			}
		}
	}

	/* Get a free block: */
	xt_lock_mutex_ns(&dcg->cg_lock);
	if (!(new_block = dcg->cg_free_list)) {
		xt_unlock_mutex_ns(&dcg->cg_lock);
		goto free_some_blocks;
	}
	ASSERT_NS(new_block->cb_state == IDX_CAC_BLOCK_FREE);
	dcg->cg_free_list = new_block->cb_next;
	dcg->cg_free_count--;
#ifdef DEBUG_CHECK_IND_CACHE
	dcg->cg_read_count++;
#endif
	xt_unlock_mutex_ns(&dcg->cg_lock);

	use_free_block:
	new_block->cb_address = address;
	new_block->cb_file_id = file->fr_id;
	new_block->cb_state = IDX_CAC_BLOCK_CLEAN;
	new_block->cb_handle_count = 0;
	new_block->cp_flush_seq = 0;
	new_block->cp_del_count = 0;
	new_block->cb_dirty_next = NULL;
	new_block->cb_dirty_prev = NULL;

	if (read_data) {
		if (!xt_pread_file(file, xt_ind_node_to_offset(ot->ot_table, address), XT_INDEX_PAGE_SIZE, 0, new_block->cb_data, &red_size, &ot->ot_thread->st_statistics.st_ind, ot->ot_thread)) {
			xt_lock_mutex_ns(&dcg->cg_lock);
			new_block->cb_next = dcg->cg_free_list;
			dcg->cg_free_list = new_block;
			dcg->cg_free_count++;
#ifdef DEBUG_CHECK_IND_CACHE
			dcg->cg_read_count--;
#endif
			new_block->cb_state = IDX_CAC_BLOCK_FREE;
			IDX_TRACE("%d- F%x\n", (int) XT_NODE_ID(address), (int) XT_GET_DISK_2(new_block->cb_data));
			xt_unlock_mutex_ns(&dcg->cg_lock);
#ifdef DEBUG_CHECK_IND_CACHE
			xt_ind_check_cache(NULL);
#endif
			return NULL;
		}
		IDX_TRACE("%d- R%x\n", (int) XT_NODE_ID(address), (int) XT_GET_DISK_2(new_block->cb_data));
		ot->ot_thread->st_statistics.st_ind_cache_miss++;
	}
	else
		red_size = 0;
	// PMC - I don't think this is required! memset(new_block->cb_data + red_size, 0, XT_INDEX_PAGE_SIZE - red_size);

	IDX_CAC_WRITE_LOCK(seg, ot->ot_thread);
	block = seg->cs_hash_table[hash_idx];
	while (block) {
		if (XT_NODE_ID(block->cb_address) == XT_NODE_ID(address) && block->cb_file_id == file->fr_id) {
			/* Oops, someone else was faster! */
			xt_lock_mutex_ns(&dcg->cg_lock);
			new_block->cb_next = dcg->cg_free_list;
			dcg->cg_free_list = new_block;
			dcg->cg_free_count++;
#ifdef DEBUG_CHECK_IND_CACHE
			dcg->cg_read_count--;
#endif
			new_block->cb_state = IDX_CAC_BLOCK_FREE;
			IDX_TRACE("%d- F%x\n", (int) XT_NODE_ID(address), (int) XT_GET_DISK_2(new_block->cb_data));
			xt_unlock_mutex_ns(&dcg->cg_lock);
			goto done_ok;
		}
		block = block->cb_next;
	}
	block = new_block;

	/* Make the block the most recently used: */
	xt_lock_mutex_ns(&dcg->cg_lock);
	block->cb_ru_time = ++dcg->cg_ru_now;
	if ((block->cb_lr_used = dcg->cg_mru_block))
		dcg->cg_mru_block->cb_mr_used = block;
	block->cb_mr_used = NULL;
	dcg->cg_mru_block = block;
	if (!dcg->cg_lru_block)
		dcg->cg_lru_block = block;
#ifdef DEBUG_CHECK_IND_CACHE
	dcg->cg_read_count--;
#endif
	xt_unlock_mutex_ns(&dcg->cg_lock);

	/* {LAZY-DEL-INDEX-ITEMS}
	 * Conditionally count the number of deleted entries in the index:
	 * We do this before other threads can read the block.
	 */
	if (ind->mi_lazy_delete && read_data)
		xt_ind_count_deleted_items(ot->ot_table, ind, block);

	/* Add to the hash table: */
	block->cb_next = seg->cs_hash_table[hash_idx];
	seg->cs_hash_table[hash_idx] = block;

	done_ok:
	*ret_seg = seg;
#ifdef DEBUG_CHECK_IND_CACHE
	xt_ind_check_cache(NULL);
#endif
	return block;
}

static xtBool ind_cac_get(XTOpenTablePtr ot, xtIndexNodeID address, DcSegmentPtr *ret_seg, XTIndBlockPtr *ret_block)
{
	register XTOpenFilePtr	file = ot->ot_ind_file;
	register XTIndBlockPtr	block;
	register DcSegmentPtr	seg;
	register u_int			hash_idx;
	register DcGlobalsRec	*dcg = &ind_cac_globals;

	hash_idx = XT_NODE_ID(address) + (file->fr_id * 223);
	seg = &dcg->cg_segment[hash_idx & IDX_CAC_SEGMENT_MASK];
	hash_idx = (hash_idx >> XT_INDEX_CACHE_SEGMENT_SHIFTS) % dcg->cg_hash_size;

	IDX_CAC_READ_LOCK(seg, ot->ot_thread);
	block = seg->cs_hash_table[hash_idx];
	while (block) {
		if (XT_NODE_ID(block->cb_address) == XT_NODE_ID(address) && block->cb_file_id == file->fr_id) {
			ASSERT_NS(block->cb_state != IDX_CAC_BLOCK_FREE);

			*ret_seg = seg;
			*ret_block = block;
			return OK;
		}
		block = block->cb_next;
	}
	IDX_CAC_UNLOCK(seg, ot->ot_thread);
	
	/* Block not found: */
	*ret_seg = NULL;
	*ret_block = NULL;
	return OK;
}

xtPublic xtBool xt_ind_write(XTOpenTablePtr ot, XTIndexPtr ind, xtIndexNodeID address, size_t size, xtWord1 *data)
{
	XTIndBlockPtr	block;
	DcSegmentPtr	seg;

	if (!(block = ind_cac_fetch(ot, ind, address, &seg, FALSE)))
		return FAILED;

	XT_IPAGE_WRITE_LOCK(&block->cb_lock, ot->ot_thread->t_id);
	ASSERT_NS(block->cb_state == IDX_CAC_BLOCK_CLEAN || block->cb_state == IDX_CAC_BLOCK_DIRTY);
	memcpy(block->cb_data, data, size);
	block->cp_flush_seq = ot->ot_table->tab_ind_flush_seq;
	if (block->cb_state != IDX_CAC_BLOCK_DIRTY) {
		TRACK_BLOCK_WRITE(offset);
		xt_spinlock_lock(&ind->mi_dirty_lock);
		if ((block->cb_dirty_next = ind->mi_dirty_list))
			ind->mi_dirty_list->cb_dirty_prev = block;
		block->cb_dirty_prev = NULL;
		ind->mi_dirty_list = block;
		ind->mi_dirty_blocks++;
		xt_spinlock_unlock(&ind->mi_dirty_lock);
		block->cb_state = IDX_CAC_BLOCK_DIRTY;
	}
	XT_IPAGE_UNLOCK(&block->cb_lock, TRUE);
	IDX_CAC_UNLOCK(seg, ot->ot_thread);
#ifdef XT_TRACK_INDEX_UPDATES
	ot->ot_ind_changed++;
#endif
	return OK;
}

/*
 * Update the cache, if in RAM.
 */
xtPublic xtBool xt_ind_write_cache(XTOpenTablePtr ot, xtIndexNodeID address, size_t size, xtWord1 *data)
{
	XTIndBlockPtr	block;
	DcSegmentPtr	seg;

	if (!ind_cac_get(ot, address, &seg, &block))
		return FAILED;

	if (block) {
		XT_IPAGE_WRITE_LOCK(&block->cb_lock, ot->ot_thread->t_id);
		ASSERT_NS(block->cb_state == IDX_CAC_BLOCK_CLEAN || block->cb_state == IDX_CAC_BLOCK_DIRTY);
		memcpy(block->cb_data, data, size);
		XT_IPAGE_UNLOCK(&block->cb_lock, TRUE);
		IDX_CAC_UNLOCK(seg, ot->ot_thread);
	}

	return OK;
}

xtPublic xtBool xt_ind_clean(XTOpenTablePtr ot, XTIndexPtr ind, xtIndexNodeID address)
{
	XTIndBlockPtr	block;
	DcSegmentPtr	seg;

	if (!ind_cac_get(ot, address, &seg, &block))
		return FAILED;
	if (block) {
		XT_IPAGE_WRITE_LOCK(&block->cb_lock, ot->ot_thread->t_id);
		ASSERT_NS(block->cb_state == IDX_CAC_BLOCK_CLEAN || block->cb_state == IDX_CAC_BLOCK_DIRTY);

		if (block->cb_state == IDX_CAC_BLOCK_DIRTY) {
			/* Take the block off the dirty list: */
			xt_spinlock_lock(&ind->mi_dirty_lock);
			if (block->cb_dirty_next)
				block->cb_dirty_next->cb_dirty_prev = block->cb_dirty_prev;
			if (block->cb_dirty_prev)
				block->cb_dirty_prev->cb_dirty_next = block->cb_dirty_next;
			if (ind->mi_dirty_list == block)
				ind->mi_dirty_list = block->cb_dirty_next;
			ind->mi_dirty_blocks--;
			xt_spinlock_unlock(&ind->mi_dirty_lock);
			block->cb_state = IDX_CAC_BLOCK_CLEAN;
		}
		XT_IPAGE_UNLOCK(&block->cb_lock, TRUE);

		IDX_CAC_UNLOCK(seg, ot->ot_thread);
	}

	return OK;
}

xtPublic xtBool xt_ind_read_bytes(XTOpenTablePtr ot, XTIndexPtr ind, xtIndexNodeID address, size_t size, xtWord1 *data)
{
	XTIndBlockPtr	block;
	DcSegmentPtr	seg;

	if (!(block = ind_cac_fetch(ot, ind, address, &seg, TRUE)))
		return FAILED;

	XT_IPAGE_READ_LOCK(&block->cb_lock);
	memcpy(data, block->cb_data, size);
	XT_IPAGE_UNLOCK(&block->cb_lock, FALSE);
	IDX_CAC_UNLOCK(seg, ot->ot_thread);
	return OK;
}

xtPublic xtBool xt_ind_fetch(XTOpenTablePtr ot, XTIndexPtr ind, xtIndexNodeID address, XTPageLockType ltype, XTIndReferencePtr iref)
{
	register XTIndBlockPtr	block;
	DcSegmentPtr			seg;
	xtWord2					branch_size;
	xtBool					xlock = FALSE;

#ifdef DEBUG
	ASSERT_NS(iref->ir_xlock == 2);
	ASSERT_NS(iref->ir_xlock == 2);
#endif
	if (!(block = ind_cac_fetch(ot, ind, address, &seg, TRUE)))
		return NULL;

	branch_size = XT_GET_DISK_2(((XTIdxBranchDPtr) block->cb_data)->tb_size_2);
	if (XT_GET_INDEX_BLOCK_LEN(branch_size) < 2 || XT_GET_INDEX_BLOCK_LEN(branch_size) > XT_INDEX_PAGE_SIZE) {
		IDX_CAC_UNLOCK(seg, ot->ot_thread);
		xt_register_taberr(XT_REG_CONTEXT, XT_ERR_INDEX_CORRUPTED, ot->ot_table->tab_name);
		return FAILED;
	}

	switch (ltype) {
		case XT_LOCK_READ:
			break;
		case XT_LOCK_WRITE:
			xlock = TRUE;
			break;
		case XT_XLOCK_LEAF:
			if (!XT_IS_NODE(branch_size))
				xlock = TRUE;
			break;
		case XT_XLOCK_DEL_LEAF:
			if (!XT_IS_NODE(branch_size)) {
				if (ot->ot_table->tab_dic.dic_no_lazy_delete)
					xlock = TRUE;
				else {
					/*
					 * {LAZY-DEL-INDEX-ITEMS}
					 *
					 * We are fetch a page for delete purpose.
					 * we decide here if we plan to do a lazy delete,
					 * Or if we plan to compact the node.
					 *
					 * A lazy delete just requires a shared lock.
					 *
					 */
					if (ind->mi_lazy_delete) {
						/* If the number of deleted items is greater than
						 * half of the number of times that can fit in the
						 * page, then we will compact the node.
						 */
						if (!xt_idx_lazy_delete_on_leaf(ind, block, XT_GET_INDEX_BLOCK_LEN(branch_size)))
							xlock = TRUE;
					}
					else
						xlock = TRUE;
				}
			}
			break;
	}

	if ((iref->ir_xlock = xlock))
		XT_IPAGE_WRITE_LOCK(&block->cb_lock, ot->ot_thread->t_id);
	else
		XT_IPAGE_READ_LOCK(&block->cb_lock);

	IDX_CAC_UNLOCK(seg, ot->ot_thread);

	/* {DIRECT-IO}
	 * Direct I/O requires that the buffer is 512 byte aligned.
	 * To do this, cb_data is turned into a pointer, instead
	 * of an array.
	 * As a result, we need to pass a pointer to both the
	 * cache block and the cache block data:
	 */
	iref->ir_updated = FALSE;
	iref->ir_block = block;
	iref->ir_branch = (XTIdxBranchDPtr) block->cb_data;
	return OK;
}

xtPublic xtBool xt_ind_release(XTOpenTablePtr ot, XTIndexPtr ind, XTPageUnlockType XT_NDEBUG_UNUSED(utype), XTIndReferencePtr iref)
{
	register XTIndBlockPtr	block;

	block = iref->ir_block;

#ifdef DEBUG
	ASSERT_NS(iref->ir_xlock != 2);
	ASSERT_NS(iref->ir_updated != 2);
	if (iref->ir_updated)
		ASSERT_NS(utype == XT_UNLOCK_R_UPDATE || utype == XT_UNLOCK_W_UPDATE);
	else
		ASSERT_NS(utype == XT_UNLOCK_READ || utype == XT_UNLOCK_WRITE);
	if (iref->ir_xlock)
		ASSERT_NS(utype == XT_UNLOCK_WRITE || utype == XT_UNLOCK_W_UPDATE);
	else
		ASSERT_NS(utype == XT_UNLOCK_READ || utype == XT_UNLOCK_R_UPDATE);
#endif
	if (iref->ir_updated) {
		/* The page was update: */
		ASSERT_NS(block->cb_state == IDX_CAC_BLOCK_CLEAN || block->cb_state == IDX_CAC_BLOCK_DIRTY);
		block->cp_flush_seq = ot->ot_table->tab_ind_flush_seq;
		if (block->cb_state != IDX_CAC_BLOCK_DIRTY) {
			TRACK_BLOCK_WRITE(offset);
			xt_spinlock_lock(&ind->mi_dirty_lock);
			if ((block->cb_dirty_next = ind->mi_dirty_list))
				ind->mi_dirty_list->cb_dirty_prev = block;
			block->cb_dirty_prev = NULL;
			ind->mi_dirty_list = block;
			ind->mi_dirty_blocks++;
			xt_spinlock_unlock(&ind->mi_dirty_lock);
			block->cb_state = IDX_CAC_BLOCK_DIRTY;
		}
	}

	XT_IPAGE_UNLOCK(&block->cb_lock, iref->ir_xlock);
#ifdef DEBUG
	iref->ir_xlock = 2;
	iref->ir_updated = 2;
#endif
	return OK;
}

xtPublic xtBool xt_ind_reserve(XTOpenTablePtr ot, u_int count, XTIdxBranchDPtr not_this)
{
	register XTIndBlockPtr	block;
	register DcGlobalsRec	*dcg = &ind_cac_globals;

#ifdef XT_TRACK_INDEX_UPDATES
	ot->ot_ind_reserved = count;
	ot->ot_ind_reads = 0;
#endif
#ifdef DEBUG_CHECK_IND_CACHE
	xt_ind_check_cache(NULL);
#endif
	while (ot->ot_ind_res_count < count) {
		if (!dcg->cg_free_list) {
			if (!ind_cac_free_lru_blocks(ot, count - ot->ot_ind_res_count, not_this)) {
				if (!dcg->cg_free_list) {
					xt_ind_free_reserved(ot);
					xt_register_xterr(XT_REG_CONTEXT, XT_ERR_NO_INDEX_CACHE);
#ifdef DEBUG_CHECK_IND_CACHE
					xt_ind_check_cache(NULL);
#endif
					return FAILED;
				}
			}
		}

		/* Get a free block: */
		xt_lock_mutex_ns(&dcg->cg_lock);
		while (ot->ot_ind_res_count < count && (block = dcg->cg_free_list)) {
			ASSERT_NS(block->cb_state == IDX_CAC_BLOCK_FREE);
			dcg->cg_free_list = block->cb_next;
			dcg->cg_free_count--;
			block->cb_next = ot->ot_ind_res_bufs;
			ot->ot_ind_res_bufs = block;
			ot->ot_ind_res_count++;
#ifdef DEBUG_CHECK_IND_CACHE
			dcg->cg_reserved_by_ots++;
#endif
		}
		xt_unlock_mutex_ns(&dcg->cg_lock);
	}
#ifdef DEBUG_CHECK_IND_CACHE
	xt_ind_check_cache(NULL);
#endif
	return OK;
}

xtPublic void xt_ind_free_reserved(XTOpenTablePtr ot)
{
#ifdef DEBUG_CHECK_IND_CACHE
	xt_ind_check_cache(NULL);
#endif
	if (ot->ot_ind_res_bufs) {
		register XTIndBlockPtr	block, fblock;
		register DcGlobalsRec	*dcg = &ind_cac_globals;

		xt_lock_mutex_ns(&dcg->cg_lock);
		block = ot->ot_ind_res_bufs;
		while (block) {
			fblock = block;
			block = block->cb_next;

			fblock->cb_next = dcg->cg_free_list;
			dcg->cg_free_list = fblock;
#ifdef DEBUG_CHECK_IND_CACHE
			dcg->cg_reserved_by_ots--;
#endif
			dcg->cg_free_count++;
		}
		xt_unlock_mutex_ns(&dcg->cg_lock);
		ot->ot_ind_res_bufs = NULL;
		ot->ot_ind_res_count = 0;
	}
#ifdef DEBUG_CHECK_IND_CACHE
	xt_ind_check_cache(NULL);
#endif
}

xtPublic void xt_ind_unreserve(XTOpenTablePtr ot)
{
	if (!ind_cac_globals.cg_free_list)
		xt_ind_free_reserved(ot);
}

