/* Copyright (c) 2007 PrimeBase Technologies GmbH
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2007-10-31	Paul McCullagh
 *
 * H&G2JCtL
 *
 * The new table cache. Caches all non-index data. This includes the data
 * files and the row pointer files.
 */
#ifndef __tabcache_h__
#define __tabcache_h__

struct XTTable;
struct XTOpenTable;
struct XTTabCache;

#include "thread_xt.h"
#include "filesys_xt.h"
#include "lock_xt.h"

#ifdef DEBUG
//#define XT_USE_CACHE_DEBUG_SIZES
//#define XT_NOT_INLINE
#endif

#ifdef XT_USE_CACHE_DEBUG_SIZES

#define XT_TC_PAGE_SIZE				(4*1024)
#define XT_TC_SEGMENT_SHIFTS		1

#else

#define XT_TC_PAGE_SIZE				(32*1024)
#define XT_TC_SEGMENT_SHIFTS		3

#endif

#define XT_TIME_DIFF(start, now) (\
	((xtWord4) (now) < (xtWord4) (start)) ? \
	((xtWord4) 0XFFFFFFFF - ((xtWord4) (start) - (xtWord4) (now))) : \
	((xtWord4) (now) - (xtWord4) (start)))

#define XT_TC_SEGMENT_COUNT			((off_t) 1 << XT_TC_SEGMENT_SHIFTS)
#define XT_TC_SEGMENT_MASK			(XT_TC_SEGMENT_COUNT - 1)

typedef struct XTTabCachePage {
	xtWord1					tcp_dirty;						/* TRUE if the page is dirty. */
	xtWord1					tcp_seg;						/* Segement number of the page. */
	u_int					tcp_lock_count;					/* Number of read locks on this page. */
	u_int					tcp_hash_idx;					/* The hash index of the page. */
	u_int					tcp_page_idx;					/* The page address. */
	u_int					tcp_file_id;					/* The file id of the page. */
	xtDatabaseID			tcp_db_id;						/* The ID of the database. */
	xtTableID				tcp_tab_id;						/* The ID of the table of this cache page. */
	xtWord4					tcp_data_size;					/* Size of the data on this page. */
	xtOpSeqNo				tcp_op_seq;						/* The operation sequence number (dirty pages have a operations sequence) */
	xtWord4					tcp_ru_time;					/* If this is in the top 1/4 don't change position in MRU list. */
	struct XTTabCachePage	*tcp_next;						/* Pointer to next page on hash list, or next free page on free list. */
	struct XTTabCachePage	*tcp_mr_used;					/* More recently used pages. */
	struct XTTabCachePage	*tcp_lr_used;					/* Less recently used pages. */
	xtWord1					tcp_data[XT_TC_PAGE_SIZE];		/* This is actually tci_page_size! */
} XTTabCachePageRec, *XTTabCachePagePtr;

/*
 * Each table has a "table operation sequence". This sequence is incremented by
 * each operation on the table. Each operation in the log is tagged by a
 * sequence number.
 *
 * The writter threads re-order operations in the log, and write the operations
 * to the database in sequence.
 *
 * It is safe to free a cache page when the sequence number of the cache page,
 * is less than or equal to the written sequence number.
 */
typedef struct XTTableSeq {
	xtOpSeqNo				ts_next_seq;					/* The next sequence number for operations on the table. */
	xt_mutex_type			ts_ns_lock;						/* Lock for the next sequence number. */

	xtBool ts_log_no_op(XTThreadPtr thread, xtTableID tab_id, xtOpSeqNo op_seq);

	/* Return the next operation sequence number. */
#ifdef XT_NOT_INLINE
	xtOpSeqNo ts_set_op_seq(XTTabCachePagePtr page);

	xtOpSeqNo ts_get_op_seq();
#else
	xtOpSeqNo ts_set_op_seq(XTTabCachePagePtr page)
	{
		xtOpSeqNo seq;

		xt_lock_mutex_ns(&ts_ns_lock);
		page->tcp_op_seq = seq = ts_next_seq++;
		xt_unlock_mutex_ns(&ts_ns_lock);
		return seq;
	}

	xtOpSeqNo ts_get_op_seq()
	{
		xtOpSeqNo seq;

		xt_lock_mutex_ns(&ts_ns_lock);
		seq = ts_next_seq++;
		xt_unlock_mutex_ns(&ts_ns_lock);
		return seq;
	}
#endif

	void xt_op_seq_init(XTThreadPtr self) {
		xt_init_mutex_with_autoname(self, &ts_ns_lock);
	}

	void xt_op_seq_set(XTThreadPtr XT_UNUSED(self), xtOpSeqNo n) {
		ts_next_seq = n;
	}

	void xt_op_seq_exit(XTThreadPtr XT_UNUSED(self)) {
		xt_free_mutex(&ts_ns_lock);
	}

#ifdef XT_NOT_INLINE
	static xtBool xt_op_is_before(register xtOpSeqNo now, register xtOpSeqNo then);
#else
	static inline xtBool xt_op_is_before(register xtOpSeqNo now, register xtOpSeqNo then)
	{
		if (now >= then) {
			if ((now - then) > (xtOpSeqNo) 0xFFFFFFFF/2)
				return TRUE;
			return FALSE;
		}
		if ((then - now) > (xtOpSeqNo) 0xFFFFFFFF/2)
			return FALSE;
		return TRUE;
	}
#endif
} XTTableSeqRec, *XTTableSeqPtr;

#ifdef XT_NO_ATOMICS
#define TAB_CAC_USE_PTHREAD_RW
#else
//#define TAB_CAC_USE_RWMUTEX
//#define TAB_CAC_USE_PTHREAD_RW
//#define IDX_USE_SPINXSLOCK
#define TAB_CAC_USE_XSMUTEX
#endif

#ifdef TAB_CAC_USE_XSMUTEX
#define TAB_CAC_LOCK_TYPE				XTXSMutexRec
#define TAB_CAC_INIT_LOCK(s, i)			xt_xsmutex_init_with_autoname(s, i)
#define TAB_CAC_FREE_LOCK(s, i)			xt_xsmutex_free(s, i)	
#define TAB_CAC_READ_LOCK(i, o)			xt_xsmutex_slock(i, o)
#define TAB_CAC_WRITE_LOCK(i, o)		xt_xsmutex_xlock(i, o)
#define TAB_CAC_UNLOCK(i, o)			xt_xsmutex_unlock(i, o)
#elif defined(TAB_CAC_USE_PTHREAD_RW)
#define TAB_CAC_LOCK_TYPE				xt_rwlock_type
#define TAB_CAC_INIT_LOCK(s, i)			xt_init_rwlock(s, i)
#define TAB_CAC_FREE_LOCK(s, i)			xt_free_rwlock(i)	
#define TAB_CAC_READ_LOCK(i, o)			xt_slock_rwlock_ns(i)
#define TAB_CAC_WRITE_LOCK(i, o)		xt_xlock_rwlock_ns(i)
#define TAB_CAC_UNLOCK(i, o)			xt_unlock_rwlock_ns(i)
#elif defined(TAB_CAC_USE_RWMUTEX)
#define TAB_CAC_LOCK_TYPE				XTRWMutexRec
#define TAB_CAC_INIT_LOCK(s, i)			xt_rwmutex_init_with_autoname(s, i)
#define TAB_CAC_FREE_LOCK(s, i)			xt_rwmutex_free(s, i)	
#define TAB_CAC_READ_LOCK(i, o)			xt_rwmutex_slock(i, o)
#define TAB_CAC_WRITE_LOCK(i, o)		xt_rwmutex_xlock(i, o)
#define TAB_CAC_UNLOCK(i, o)			xt_rwmutex_unlock(i, o)
#elif defined(TAB_CAC_USE_SPINXSLOCK)
#define TAB_CAC_LOCK_TYPE				XTSpinXSLockRec
#define TAB_CAC_INIT_LOCK(s, i)			xt_spinxslock_init_with_autoname(s, i)
#define TAB_CAC_FREE_LOCK(s, i)			xt_spinxslock_free(s, i)	
#define TAB_CAC_READ_LOCK(i, o)			xt_spinxslock_slock(i, o)
#define TAB_CAC_WRITE_LOCK(i, o)		xt_spinxslock_xlock(i, o)
#define TAB_CAC_UNLOCK(i, o)			xt_spinxslock_unlock(i, o)
#endif

/* A disk cache segment. The cache is divided into a number of segments
 * to improve concurrency.
 */
typedef struct XTTabCacheSeg {
	TAB_CAC_LOCK_TYPE		tcs_lock;						/* The cache segment read/write lock. */
	XTTabCachePagePtr		*tcs_hash_table;
	size_t					tcs_cache_in_use;
} XTTabCacheSegRec, *XTTabCacheSegPtr;

/*
 * The free'er thread has a list of tables to be purged from the cache.
 * If a table is in the list then it is not allowed to fetch a cache page from
 * that table.
 * The free'er thread goes through all the cache, and removes
 * all cache pages for any table in the purge list.
 * When a table has been purged it signals any threads waiting for the
 * purge to complete (this is usually due to a drop table).
 */
typedef struct XTTabCachePurge {
	int						tcp_state;						/* The state of the purge. */
	XTTableSeqPtr			tcp_tab_seq;					/* Identifies the table to be purged from cache. */
} XTTabCachePurgeRec, *XTTabCachePurgePtr;

typedef struct XTTabCacheMem {
	xt_mutex_type			tcm_lock;						/* The public cache lock. */
	xt_cond_type			tcm_cond;						/* The public cache wait condition. */
	XTTabCacheSegRec		tcm_segment[XT_TC_SEGMENT_COUNT];
	XTTabCachePagePtr		tcm_lru_page;
	XTTabCachePagePtr		tcm_mru_page;
	xtWord4					tcm_ru_now;
	size_t					tcm_approx_page_count;
	size_t					tcm_hash_size;
	u_int					tcm_writer_thread_count;
	size_t					tcm_cache_size;
	size_t					tcm_cache_high;					/* The high water level of cache allocation. */
	size_t					tcm_low_level;					/* This is the level to which the freeer will free, once it starts working. */
	size_t					tcm_high_level;					/* This is the level at which the freeer will start to work (to avoid waiting)! */

	/* The free'er thread: */
	struct XTThread			*tcm_freeer_thread;				/* The freeer thread . */
	xt_mutex_type			tcm_freeer_lock;				/* The public cache lock. */
	xt_cond_type			tcm_freeer_cond;				/* The public cache wait condition. */
	u_int					tcm_purge_list_len;				/* The length of the purge list. */
	XTTabCachePurgePtr		tcm_purge_list;					/* Non-NULL if a table is to be purged. */
	u_int					tcm_threads_waiting;			/* Count of the number of threads waiting for the freeer. */
	xtBool					tcm_freeer_busy;
	u_int					tcm_free_try_count;
} XTTabCacheMemRec, *XTTabCacheMemPtr;

/*
 * This structure contains the information about a particular table
 * for the cache. Each table has its own page size, row size
 * and rows per page.
 * Tables also have 
 */
typedef struct XTTabCache {
	struct XTTable			*tci_table;
	size_t					tci_header_size;
	size_t					tci_page_size;
	size_t					tci_rec_size;
	size_t					tci_rows_per_page;

public:
	void					xt_tc_setup(struct XTTable *tab, size_t head_size, size_t row_size);
	xtBool					xt_tc_write(XT_ROW_REC_FILE_PTR file, xtRefID ref_id, size_t offset, size_t size, xtWord1 *data, xtOpSeqNo *op_seq, xtBool read, XTThreadPtr thread);
	xtBool					xt_tc_write_cond(XTThreadPtr self, XT_ROW_REC_FILE_PTR file, xtRefID ref_id, xtWord1 new_type, xtOpSeqNo *op_seq, xtXactID xn_id, xtRowID row_id, u_int stat_id, u_int rec_type);
	xtBool					xt_tc_read(XT_ROW_REC_FILE_PTR file, xtRefID ref_id, size_t size, xtWord1 *data, XTThreadPtr thread);
	xtBool					xt_tc_read_4(XT_ROW_REC_FILE_PTR file, xtRefID ref_id, xtWord4 *data, XTThreadPtr thread);
	xtBool					xt_tc_read_page(XT_ROW_REC_FILE_PTR file, xtRefID ref_id, xtWord1 *data, XTThreadPtr thread);
	xtBool					xt_tc_get_page(XT_ROW_REC_FILE_PTR file, xtRefID ref_id, xtBool load, XTTabCachePagePtr *page, size_t *offset, XTThreadPtr thread);
	void					xt_tc_release_page(XT_ROW_REC_FILE_PTR file, XTTabCachePagePtr page, XTThreadPtr thread);
	xtBool					tc_fetch(XT_ROW_REC_FILE_PTR file, xtRefID ref_id, XTTabCacheSegPtr *ret_seg, XTTabCachePagePtr *ret_page, size_t *offset, xtBool read, XTThreadPtr thread);

private:
	xtBool					tc_read_direct(XT_ROW_REC_FILE_PTR file, xtRefID ref_id, size_t size, xtWord1 *data, XTThreadPtr thread);
	xtBool					tc_fetch_direct(XT_ROW_REC_FILE_PTR file, xtRefID ref_id, XTTabCacheSegPtr *ret_seg, XTTabCachePagePtr *ret_page, size_t *offset, XTThreadPtr thread);
} XTTabCacheRec, *XTTabCachePtr;

extern XTTabCacheMemRec xt_tab_cache;

void	xt_tc_init(XTThreadPtr self, size_t cache_size);
void	xt_tc_exit(XTThreadPtr self);
void	xt_tc_set_cache_size(size_t cache_size);
xtInt8	xt_tc_get_usage();
xtInt8	xt_tc_get_size();
xtInt8	xt_tc_get_high();
void	xt_load_pages(XTThreadPtr self, struct XTOpenTable *ot);
#ifdef DEBUG
void	xt_check_table_cache(struct XTTable *tab);
#endif

void	xt_quit_freeer(XTThreadPtr self);
void	xt_stop_freeer(XTThreadPtr self);
void	xt_start_freeer(XTThreadPtr self);
void	xt_wr_wake_freeer(XTThreadPtr self);

#endif
