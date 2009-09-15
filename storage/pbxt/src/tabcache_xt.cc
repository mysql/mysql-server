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
 * 2007-10-30	Paul McCullagh
 *
 * H&G2JCtL
 *
 * The new table cache. Caches all non-index data. This includes the data
 * files and the row pointer files.
 */

#include "xt_config.h"

#ifdef DRIZZLED
#include <bitset>
#endif

#include <signal.h>

#include "pthread_xt.h"
#include "tabcache_xt.h"
#include "table_xt.h"
#include "database_xt.h"
#include "trace_xt.h"
#include "myxt_xt.h"

xtPublic XTTabCacheMemRec	xt_tab_cache;

static void tabc_fr_wait_for_cache(XTThreadPtr self, u_int msecs);

xtPublic void xt_tc_set_cache_size(size_t cache_size)
{
	xt_tab_cache.tcm_cache_size = cache_size;
	xt_tab_cache.tcm_low_level = cache_size / 4 * 3;		// Current 75%
	xt_tab_cache.tcm_high_level = cache_size / 100 * 95;	// Current 95%
}

/*
 * Initialize the disk cache.
 */
xtPublic void xt_tc_init(XTThreadPtr self, size_t cache_size)
{
	xt_tc_set_cache_size(cache_size);

	xt_tab_cache.tcm_approx_page_count = cache_size / sizeof(XTTabCachePageRec);
	/* Determine the size of the hash table.
	 * The size is set to 2* the number of pages!
	 */
	xt_tab_cache.tcm_hash_size = (xt_tab_cache.tcm_approx_page_count * 2) / XT_TC_SEGMENT_COUNT;

	try_(a) {
		for (u_int i=0; i<XT_TC_SEGMENT_COUNT; i++) {
			xt_tab_cache.tcm_segment[i].tcs_cache_in_use = 0;
			xt_tab_cache.tcm_segment[i].tcs_hash_table = (XTTabCachePagePtr *) xt_calloc(self, xt_tab_cache.tcm_hash_size * sizeof(XTTabCachePagePtr));
			TAB_CAC_INIT_LOCK(self, &xt_tab_cache.tcm_segment[i].tcs_lock);
		}

		xt_init_mutex_with_autoname(self, &xt_tab_cache.tcm_lock);
		xt_init_cond(self, &xt_tab_cache.tcm_cond);
		xt_init_mutex_with_autoname(self, &xt_tab_cache.tcm_freeer_lock);
		xt_init_cond(self, &xt_tab_cache.tcm_freeer_cond);
	}
	catch_(a) {
		xt_tc_exit(self);
		throw_();
	}
	cont_(a);
}

xtPublic void xt_tc_exit(XTThreadPtr self)
{
	for (u_int i=0; i<XT_TC_SEGMENT_COUNT; i++) {
		if (xt_tab_cache.tcm_segment[i].tcs_hash_table) {
			if (xt_tab_cache.tcm_segment[i].tcs_cache_in_use) {
				XTTabCachePagePtr page, tmp_page;

				for (size_t j=0; j<xt_tab_cache.tcm_hash_size; j++) {
					page = xt_tab_cache.tcm_segment[i].tcs_hash_table[j];
					while (page) {
						tmp_page = page;
						page = page->tcp_next;
						xt_free(self, tmp_page);
					}
				}
			}

			xt_free(self, xt_tab_cache.tcm_segment[i].tcs_hash_table);
			xt_tab_cache.tcm_segment[i].tcs_hash_table = NULL;
			TAB_CAC_FREE_LOCK(self, &xt_tab_cache.tcm_segment[i].tcs_lock);
		}
	}

	xt_free_mutex(&xt_tab_cache.tcm_lock);
	xt_free_cond(&xt_tab_cache.tcm_cond);
	xt_free_mutex(&xt_tab_cache.tcm_freeer_lock);
	xt_free_cond(&xt_tab_cache.tcm_freeer_cond);
}

xtPublic xtInt8 xt_tc_get_usage()
{
	xtInt8 size = 0;

	for (u_int i=0; i<XT_TC_SEGMENT_COUNT; i++) {
		size += xt_tab_cache.tcm_segment[i].tcs_cache_in_use;
	}
	return size;
}

xtPublic xtInt8 xt_tc_get_size()
{
	return (xtInt8) xt_tab_cache.tcm_cache_size;
}

xtPublic xtInt8 xt_tc_get_high()
{
	return (xtInt8) xt_tab_cache.tcm_cache_high;
}

#ifdef DEBUG
xtPublic void xt_check_table_cache(XTTableHPtr tab)
{
	XTTabCachePagePtr page, ppage;

	xt_lock_mutex_ns(&xt_tab_cache.tcm_lock);
	ppage = NULL;
	page = xt_tab_cache.tcm_lru_page;
	while (page) {
		if (tab) {
			if (page->tcp_db_id == tab->tab_db->db_id && page->tcp_tab_id == tab->tab_id) {
				ASSERT_NS(!XTTableSeq::xt_op_is_before(tab->tab_seq.ts_next_seq, page->tcp_op_seq));
			}
		}
		ASSERT_NS(page->tcp_lr_used == ppage);
		ppage = page;
		page = page->tcp_mr_used;
	}
	ASSERT_NS(xt_tab_cache.tcm_mru_page == ppage);
	xt_unlock_mutex_ns(&xt_tab_cache.tcm_lock);
}
#endif

void XTTabCache::xt_tc_setup(XTTableHPtr tab, size_t head_size, size_t rec_size)
{
	tci_table = tab;
	tci_header_size = head_size;
	tci_rec_size = rec_size;
	tci_rows_per_page = (XT_TC_PAGE_SIZE / rec_size) + 1;
	if (tci_rows_per_page < 2)
		tci_rows_per_page = 2;
	tci_page_size = tci_rows_per_page * rec_size;
}

/*
 * This function assumes that we never write past the boundary of a page.
 * This should be the case, because we should never write more than
 * a row, and there are only whole rows on a page.
 */
xtBool XTTabCache::xt_tc_write(XT_ROW_REC_FILE_PTR file, xtRefID ref_id, size_t inc, size_t size, xtWord1 *data, xtOpSeqNo *op_seq, xtBool read, XTThreadPtr thread)
{
	size_t				offset;
	XTTabCachePagePtr	page;
	XTTabCacheSegPtr	seg;

	/*
	retry:
	*/
	if (!tc_fetch(file, ref_id, &seg, &page, &offset, read, thread))
		return FAILED;
	/* Don't write while there is a read lock on the page,
	 * which can happen during a sequential scan...
	 *
	 * This will have to be OK.
	 * I cannot wait for the lock because a thread locks
	 * itself out when updating during a sequential scan.
	 *
	 * However, I don't think this is a problem, because
	 * the only records that are changed, are records
	 * containing uncommitted data. Such records should
	 * be ignored by a sequential scan. As long as
	 * we don't crash due to reading half written
	 * data!
	 *
	if (page->tcp_lock_count) {
		if (!xt_timed_wait_cond_ns(&seg->tcs_cond, &seg->tcs_lock, 100)) {
			xt_rwmutex_unlock(&seg->tcs_lock, thread->t_id);
			return FAILED;
		}
		xt_rwmutex_unlock(&seg->tcs_lock, thread->t_id);
		// The page may have dissappeared from the cache, while we were sleeping!
		goto retry;
	}
	*/
	
	ASSERT_NS(offset + inc + 4 <= tci_page_size);
	memcpy(page->tcp_data + offset + inc, data, size);
	/* GOTCHA, this was "op_seq > page->tcp_op_seq", however
	 * this does not handle overflow!
	if (XTTableSeq::xt_op_is_before(page->tcp_op_seq, op_seq))
		page->tcp_op_seq = op_seq;
	 */

	page->tcp_dirty = TRUE;
	ASSERT_NS(page->tcp_db_id == tci_table->tab_db->db_id && page->tcp_tab_id == tci_table->tab_id);
	*op_seq = tci_table->tab_seq.ts_set_op_seq(page);
	TAB_CAC_UNLOCK(&seg->tcs_lock, thread->t_id);
	return OK;
}

/*
 * This is a special version of write which is used to set the "clean" bit.
 * The alternative would be to read the record first, but this
 * is much quicker!
 *
 * This function also checks if xn_id, row_id and other data match (the checks 
 * are similar to xn_sw_cleanup_done) before modifying the record, otherwise it 
 * assumes that the record was already updated earlier and we must not set it to 
 * clean.
 *
 * If the record was not modified the function returns FALSE.
 *
 * The function has a self pointer and can throw an exception.
 */
xtBool XTTabCache::xt_tc_write_cond(XTThreadPtr self, XT_ROW_REC_FILE_PTR file, xtRefID ref_id, xtWord1 new_type, xtOpSeqNo *op_seq, 
	xtXactID xn_id, xtRowID row_id, u_int stat_id, u_int rec_type)
{
	size_t				offset;
	XTTabCachePagePtr	page;
	XTTabCacheSegPtr	seg;
	XTTabRecHeadDPtr	rec_head;

	if (!tc_fetch(file, ref_id, &seg, &page, &offset, TRUE, self))
		xt_throw(self);

	ASSERT(offset + 1 <= tci_page_size);

	rec_head = (XTTabRecHeadDPtr)(page->tcp_data + offset);

	/* Transaction must match: */
	if (XT_GET_DISK_4(rec_head->tr_xact_id_4) != xn_id)
		goto no_change;

	/* Record header must match expected value from
	 * log or clean has been done, or is not required.
	 *
	 * For example, it is not required if a record
	 * has been overwritten in a transaction.
	 */
	if (rec_head->tr_rec_type_1 != rec_type ||
		rec_head->tr_stat_id_1 != stat_id)
		goto no_change;

	/* Row must match: */
	if (XT_GET_DISK_4(rec_head->tr_row_id_4) != row_id)
		goto no_change;

	*(page->tcp_data + offset) = new_type;

	page->tcp_dirty = TRUE;
	ASSERT(page->tcp_db_id == tci_table->tab_db->db_id && page->tcp_tab_id == tci_table->tab_id);
	*op_seq = tci_table->tab_seq.ts_set_op_seq(page);
	TAB_CAC_UNLOCK(&seg->tcs_lock, self->t_id);
	return TRUE;

	no_change:
	TAB_CAC_UNLOCK(&seg->tcs_lock, self->t_id);
	return FALSE;
}

xtBool XTTabCache::xt_tc_read(XT_ROW_REC_FILE_PTR file, xtRefID ref_id, size_t size, xtWord1 *data, XTThreadPtr thread)
{
#ifdef XT_USE_ROW_REC_MMAP_FILES
	return tc_read_direct(file, ref_id, size, data, thread);
#else
	size_t				offset;
	XTTabCachePagePtr	page;
	XTTabCacheSegPtr	seg;

	if (!tc_fetch(file, ref_id, &seg, &page, &offset, TRUE, thread))
		return FAILED;
	/* A read must be completely on a page: */
	ASSERT_NS(offset + size <= tci_page_size);
	memcpy(data, page->tcp_data + offset, size);
	TAB_CAC_UNLOCK(&seg->tcs_lock, thread->t_id);
	return OK;
#endif
}

xtBool XTTabCache::xt_tc_read_4(XT_ROW_REC_FILE_PTR file, xtRefID ref_id, xtWord4 *value, XTThreadPtr thread)
{
#ifdef XT_USE_ROW_REC_MMAP_FILES
	register u_int				page_idx;
	register XTTabCachePagePtr	page;
	register XTTabCacheSegPtr	seg;
	register u_int				hash_idx;
	register XTTabCacheMemPtr	dcg = &xt_tab_cache;
	off_t						address;

	ASSERT_NS(ref_id);
	ref_id--;
	page_idx = ref_id / this->tci_rows_per_page;
	address = (off_t) ref_id * (off_t) this->tci_rec_size + (off_t) this->tci_header_size;

	hash_idx = page_idx + (file->fr_id * 223);
	seg = &dcg->tcm_segment[hash_idx & XT_TC_SEGMENT_MASK];
	hash_idx = (hash_idx >> XT_TC_SEGMENT_SHIFTS) % dcg->tcm_hash_size;

	TAB_CAC_READ_LOCK(&seg->tcs_lock, thread->t_id);
	page = seg->tcs_hash_table[hash_idx];
	while (page) {
		if (page->tcp_page_idx == page_idx && page->tcp_file_id == file->fr_id) {
			size_t	offset;
			xtWord1	*buffer;

			offset = (ref_id % this->tci_rows_per_page) * this->tci_rec_size;
			ASSERT_NS(offset + 4 <= this->tci_page_size);
			buffer = page->tcp_data + offset;
			*value = XT_GET_DISK_4(buffer);
			TAB_CAC_UNLOCK(&seg->tcs_lock, thread->t_id);
			return OK;
		}
		page = page->tcp_next;
	}
	TAB_CAC_UNLOCK(&seg->tcs_lock, thread->t_id);

	return xt_pread_fmap_4(file, address, value, &thread->st_statistics.st_rec, thread);
#else
	size_t				offset;
	XTTabCachePagePtr	page;
	XTTabCacheSegPtr	seg;
	xtWord1				*data;

	if (!tc_fetch(file, ref_id, &seg, &page, &offset, TRUE, thread))
		return FAILED;
	/* A read must be completely on a page: */
	ASSERT_NS(offset + 4 <= tci_page_size);
	data = page->tcp_data + offset;
	*value = XT_GET_DISK_4(data);
	TAB_CAC_UNLOCK(&seg->tcs_lock, thread->t_id);
	return OK;
#endif
}

xtBool XTTabCache::xt_tc_get_page(XT_ROW_REC_FILE_PTR file, xtRefID ref_id, xtBool load, XTTabCachePagePtr *ret_page, size_t *offset, XTThreadPtr thread)
{
	XTTabCachePagePtr	page;
	XTTabCacheSegPtr	seg;

	if (load) {
		if (!tc_fetch(file, ref_id, &seg, &page, offset, TRUE, thread))
			return FAILED;
	}
	else {
		if (!tc_fetch_direct(file, ref_id, &seg, &page, offset, thread))
			return FAILED;
		if (!seg) {
			*ret_page = NULL;
			return OK;
		}
	}
	page->tcp_lock_count++;
	TAB_CAC_UNLOCK(&seg->tcs_lock, thread->t_id);
	*ret_page = page;
	return OK;
}

void XTTabCache::xt_tc_release_page(XT_ROW_REC_FILE_PTR XT_UNUSED(file), XTTabCachePagePtr page, XTThreadPtr thread)
{
	XTTabCacheSegPtr	seg;

	seg = &xt_tab_cache.tcm_segment[page->tcp_seg];
	TAB_CAC_WRITE_LOCK(&seg->tcs_lock, thread->t_id);

#ifdef DEBUG
	XTTabCachePagePtr lpage, ppage;

	ppage = NULL;
	lpage = seg->tcs_hash_table[page->tcp_hash_idx];
	while (lpage) {
		if (lpage->tcp_page_idx == page->tcp_page_idx &&
			lpage->tcp_file_id == page->tcp_file_id)
			break;
		ppage = lpage;
		lpage = lpage->tcp_next;
	}

	ASSERT_NS(page == lpage);
	ASSERT_NS(page->tcp_lock_count > 0);
#endif

	if (page->tcp_lock_count > 0)
		page->tcp_lock_count--;

	TAB_CAC_UNLOCK(&seg->tcs_lock, thread->t_id);
}

xtBool XTTabCache::xt_tc_read_page(XT_ROW_REC_FILE_PTR file, xtRefID ref_id, xtWord1 *data, XTThreadPtr thread)
{
	return tc_read_direct(file, ref_id, this->tci_page_size, data, thread);
}

/* Read row and record files directly.
 * This by-passed the cache when reading, which mean
 * we rely in the OS for caching.
 * This probably only makes sense when these files
 * are memory mapped.
 */
xtBool XTTabCache::tc_read_direct(XT_ROW_REC_FILE_PTR file, xtRefID ref_id, size_t size, xtWord1 *data, XTThreadPtr thread)
{
	register u_int				page_idx;
	register XTTabCachePagePtr	page;
	register XTTabCacheSegPtr	seg;
	register u_int				hash_idx;
	register XTTabCacheMemPtr	dcg = &xt_tab_cache;
	size_t						red_size;
	off_t						address;

	ASSERT_NS(ref_id);
	ref_id--;
	page_idx = ref_id / this->tci_rows_per_page;
	address = (off_t) ref_id * (off_t) this->tci_rec_size + (off_t) this->tci_header_size;

	hash_idx = page_idx + (file->fr_id * 223);
	seg = &dcg->tcm_segment[hash_idx & XT_TC_SEGMENT_MASK];
	hash_idx = (hash_idx >> XT_TC_SEGMENT_SHIFTS) % dcg->tcm_hash_size;

	TAB_CAC_READ_LOCK(&seg->tcs_lock, thread->t_id);
	page = seg->tcs_hash_table[hash_idx];
	while (page) {
		if (page->tcp_page_idx == page_idx && page->tcp_file_id == file->fr_id) {
			size_t offset;

			offset = (ref_id % this->tci_rows_per_page) * this->tci_rec_size;
			ASSERT_NS(offset + size <= this->tci_page_size);
			memcpy(data, page->tcp_data + offset, size);
			TAB_CAC_UNLOCK(&seg->tcs_lock, thread->t_id);
			return OK;
		}
		page = page->tcp_next;
	}
	TAB_CAC_UNLOCK(&seg->tcs_lock, thread->t_id);
	if (!XT_PREAD_RR_FILE(file, address, size, 0, data, &red_size, &thread->st_statistics.st_rec, thread))
		return FAILED;
	memset(data + red_size, 0, size - red_size);
	return OK;
}

xtBool XTTabCache::tc_fetch_direct(XT_ROW_REC_FILE_PTR file, xtRefID ref_id, XTTabCacheSegPtr *ret_seg, XTTabCachePagePtr *ret_page, size_t *offset, XTThreadPtr thread)
{
	register u_int				page_idx;
	register XTTabCachePagePtr	page;
	register XTTabCacheSegPtr	seg;
	register u_int				hash_idx;
	register XTTabCacheMemPtr	dcg = &xt_tab_cache;

	ASSERT_NS(ref_id);
	ref_id--;
	page_idx = ref_id / this->tci_rows_per_page;
	*offset = (ref_id % this->tci_rows_per_page) * this->tci_rec_size;

	hash_idx = page_idx + (file->fr_id * 223);
	seg = &dcg->tcm_segment[hash_idx & XT_TC_SEGMENT_MASK];
	hash_idx = (hash_idx >> XT_TC_SEGMENT_SHIFTS) % dcg->tcm_hash_size;

	TAB_CAC_WRITE_LOCK(&seg->tcs_lock, thread->t_id);
	page = seg->tcs_hash_table[hash_idx];
	while (page) {
		if (page->tcp_page_idx == page_idx && page->tcp_file_id == file->fr_id) {
			*ret_seg = seg;
			*ret_page = page;
			return OK;
		}
		page = page->tcp_next;
	}
	TAB_CAC_UNLOCK(&seg->tcs_lock, thread->t_id);
	*ret_seg = NULL;
	*ret_page = NULL;
	return OK;
}

/*
 * Note, this function may return an exclusive, or a shared lock.
 * If the page is in cache it will return a shared lock of the segment.
 * If the page was just added to the cache it will return an
 * exclusive lock.
 */
xtBool XTTabCache::tc_fetch(XT_ROW_REC_FILE_PTR file, xtRefID ref_id, XTTabCacheSegPtr *ret_seg, XTTabCachePagePtr *ret_page, size_t *offset, xtBool read, XTThreadPtr thread)
{
	register u_int				page_idx;
	register XTTabCachePagePtr	page, new_page;
	register XTTabCacheSegPtr	seg;
	register u_int				hash_idx;
	register XTTabCacheMemPtr	dcg = &xt_tab_cache;
	size_t						red_size;
	off_t						address;

	ASSERT_NS(ref_id);
	ref_id--;
	page_idx = ref_id / this->tci_rows_per_page;
	address = (off_t) page_idx * (off_t) this->tci_page_size + (off_t) this->tci_header_size;
	*offset = (ref_id % this->tci_rows_per_page) * this->tci_rec_size;

	hash_idx = page_idx + (file->fr_id * 223);
	seg = &dcg->tcm_segment[hash_idx & XT_TC_SEGMENT_MASK];
	hash_idx = (hash_idx >> XT_TC_SEGMENT_SHIFTS) % dcg->tcm_hash_size;

	TAB_CAC_READ_LOCK(&seg->tcs_lock, thread->t_id);
	page = seg->tcs_hash_table[hash_idx];
	while (page) {
		if (page->tcp_page_idx == page_idx && page->tcp_file_id == file->fr_id) {
			/* This page has been most recently used: */
			if (XT_TIME_DIFF(page->tcp_ru_time, dcg->tcm_ru_now) > (dcg->tcm_approx_page_count >> 1)) {
				/* Move to the front of the MRU list: */
				xt_lock_mutex_ns(&dcg->tcm_lock);

				page->tcp_ru_time = ++dcg->tcm_ru_now;
				if (dcg->tcm_mru_page != page) {
					/* Remove from the MRU list: */
					if (dcg->tcm_lru_page == page)
						dcg->tcm_lru_page = page->tcp_mr_used;
					if (page->tcp_lr_used)
						page->tcp_lr_used->tcp_mr_used = page->tcp_mr_used;
					if (page->tcp_mr_used)
						page->tcp_mr_used->tcp_lr_used = page->tcp_lr_used;
	
					/* Make the page the most recently used: */
					if ((page->tcp_lr_used = dcg->tcm_mru_page))
						dcg->tcm_mru_page->tcp_mr_used = page;
					page->tcp_mr_used = NULL;
					dcg->tcm_mru_page = page;
					if (!dcg->tcm_lru_page)
						dcg->tcm_lru_page = page;
				}
				xt_unlock_mutex_ns(&dcg->tcm_lock);
			}
			*ret_seg = seg;
			*ret_page = page;
			thread->st_statistics.st_rec_cache_hit++;
			return OK;
		}
		page = page->tcp_next;
	}
	TAB_CAC_UNLOCK(&seg->tcs_lock, thread->t_id);
	
	/* Page not found, allocate a new page: */
	size_t page_size = offsetof(XTTabCachePageRec, tcp_data) + this->tci_page_size;
	if (!(new_page = (XTTabCachePagePtr) xt_malloc_ns(page_size)))
		return FAILED;
	/* Increment cache used. */
	seg->tcs_cache_in_use += page_size;

	/* Check the level of the cache: */
	size_t cache_used = 0;
	for (int i=0; i<XT_TC_SEGMENT_COUNT; i++)
		cache_used += dcg->tcm_segment[i].tcs_cache_in_use;

	if (cache_used > dcg->tcm_cache_high)
		dcg->tcm_cache_high = cache_used;

	if (cache_used > dcg->tcm_cache_size) {
		XTThreadPtr self;
		time_t		now;

		/* Wait for the cache level to go down.
		 * If this happens, then the freeer is not working fast
		 * enough!
		 */

		/* But before I do this, I must flush my own log because:
		 * - The freeer might be waiting for a page to be cleaned.
		 * - The page can only be cleaned once it has been written to
		 *   the database.
		 * - The writer cannot write the page data until it has been
		 *   flushed to the log.
		 * - The log won't be flushed, unless this thread does it.
		 * So there could be a deadlock if I don't flush the log!
		 */
		if ((self = xt_get_self())) {
			if (!xt_xlog_flush_log(self))
				goto failed;
		}

		/* Wait for the free'er thread: */
		xt_lock_mutex_ns(&dcg->tcm_freeer_lock);
		now = time(NULL);
		do {
			/* I have set the timeout to 2 here because of the following situation:
			 * 1. Transaction allocates an op seq
			 * 2. Transaction goes to update cache, but must wait for
			 *    cache to be freed (after this, the op would be written to
			 *    the log).
			 * 3. The free'er wants to free cache, but is waiting for the writter.
			 * 4. The writer cannot continue because an op seq is missing!
			 *    So the writer is waiting for the transaction thread to write
			 *    the op seq.
			 * - So we have a deadlock situation.
			 * - However, this situation can only occur if there is not enougn
			 *   cache.
			 * The timeout helps, but will not solve the problem, unless we
			 * ignore cache level here, after a while, and just continue.
			 */

			/* Wake freeer before we go to sleep: */
			if (!dcg->tcm_freeer_busy) {
				if (!xt_broadcast_cond_ns(&dcg->tcm_freeer_cond))
					xt_log_and_clear_exception_ns();
			}

			dcg->tcm_threads_waiting++;
#ifdef DEBUG
			if (!xt_timed_wait_cond_ns(&dcg->tcm_freeer_cond, &dcg->tcm_freeer_lock, 30000)) {
				dcg->tcm_threads_waiting--;
				break;
			}
#else
			if (!xt_timed_wait_cond_ns(&dcg->tcm_freeer_cond, &dcg->tcm_freeer_lock, 1000)) {
				dcg->tcm_threads_waiting--;
				break;
			}
#endif
			dcg->tcm_threads_waiting--;

			cache_used = 0;
			for (int i=0; i<XT_TC_SEGMENT_COUNT; i++)
				cache_used += dcg->tcm_segment[i].tcs_cache_in_use;

			if (cache_used <= dcg->tcm_high_level)
				break;
			/*
			 * If there is too little cache we can get stuck here.
			 * The problem is that seg numbers are allocated before fetching a
			 * record to be updated.
			 *
			 * It can happen that we end up waiting for that seq number
			 * to be written to the log before we can continue here.
			 *
			 * This happens as follows:
			 * 1. This thread waits for the freeer.
			 * 2. The freeer cannot free a page because it has not been
			 *    written by the writter.
			 * 3. The writter cannot continue because it is waiting
			 *    for a missing sequence number.
			 * 4. The missing sequence number is the one allocated
			 *    before we entered this function!
			 * 
			 * So don't wait for more than 5 seconds here!
			 */
		}
		while (time(NULL) < now + 5);
		xt_unlock_mutex_ns(&dcg->tcm_freeer_lock);
	}
	else if (cache_used > dcg->tcm_high_level) {
		/* Wake up the freeer because the cache level,
		 * is higher than the high level.
		 */
		if (!dcg->tcm_freeer_busy) {
			xt_lock_mutex_ns(&xt_tab_cache.tcm_freeer_lock);
			if (!xt_broadcast_cond_ns(&xt_tab_cache.tcm_freeer_cond))
				xt_log_and_clear_exception_ns();
			xt_unlock_mutex_ns(&xt_tab_cache.tcm_freeer_lock);
		}
	}

	/* Read the page into memory.... */
	new_page->tcp_dirty = FALSE;
	new_page->tcp_seg = (xtWord1) ((page_idx + (file->fr_id * 223)) & XT_TC_SEGMENT_MASK);
	new_page->tcp_lock_count = 0;
	new_page->tcp_hash_idx = hash_idx;
	new_page->tcp_page_idx = page_idx;
	new_page->tcp_file_id = file->fr_id;
	new_page->tcp_db_id = this->tci_table->tab_db->db_id;
	new_page->tcp_tab_id = this->tci_table->tab_id;
	new_page->tcp_data_size = this->tci_page_size;
	new_page->tcp_op_seq = 0; // Value not used because not dirty

	if (read) {
		if (!XT_PREAD_RR_FILE(file, address, this->tci_page_size, 0, new_page->tcp_data, &red_size, &thread->st_statistics.st_rec, thread))
			goto failed;
	}
	
#ifdef XT_MEMSET_UNUSED_SPACE
	/* Removing this is an optimization. It should not be required
	 * to clear the unused space in the page.
	 */
	memset(new_page->tcp_data + red_size, 0, this->tci_page_size - red_size);
#endif

	/* Add the page to the cache! */
	TAB_CAC_WRITE_LOCK(&seg->tcs_lock, thread->t_id);
	page = seg->tcs_hash_table[hash_idx];
	while (page) {
		if (page->tcp_page_idx == page_idx && page->tcp_file_id == file->fr_id) {
			/* Oops, someone else was faster! */
			xt_free_ns(new_page);
			goto done_ok;
		}
		page = page->tcp_next;
	}
	page = new_page;

	/* Make the page the most recently used: */
	xt_lock_mutex_ns(&dcg->tcm_lock);
	page->tcp_ru_time = ++dcg->tcm_ru_now;
	if ((page->tcp_lr_used = dcg->tcm_mru_page))
		dcg->tcm_mru_page->tcp_mr_used = page;
	page->tcp_mr_used = NULL;
	dcg->tcm_mru_page = page;
	if (!dcg->tcm_lru_page)
		dcg->tcm_lru_page = page;
	xt_unlock_mutex_ns(&dcg->tcm_lock);

	/* Add the page to the hash table: */
	page->tcp_next = seg->tcs_hash_table[hash_idx];
	seg->tcs_hash_table[hash_idx] = page;

	done_ok:
	*ret_seg = seg;
	*ret_page = page;
#ifdef DEBUG_CHECK_CACHE
	//XT_TC_check_cache();
#endif
	thread->st_statistics.st_rec_cache_miss++;
	return OK;

	failed:
	xt_free_ns(new_page);
	return FAILED;
}


/* ----------------------------------------------------------------------
 * OPERATION SEQUENCE
 */

xtBool XTTableSeq::ts_log_no_op(XTThreadPtr thread, xtTableID tab_id, xtOpSeqNo op_seq)
{
	XTactNoOpEntryDRec	ent_rec;
	xtWord4				sum = (xtWord4) tab_id ^ (xtWord4) op_seq;

	ent_rec.no_status_1 = XT_LOG_ENT_NO_OP;
	ent_rec.no_checksum_1 = XT_CHECKSUM_1(sum);
	XT_SET_DISK_4(ent_rec.no_tab_id_4, tab_id);
	XT_SET_DISK_4(ent_rec.no_op_seq_4, op_seq);
	/* TODO - If this also fails we have a problem.
	 * From this point on we should actually not generate
	 * any more op IDs. The problem is that the
	 * some will be missing, so the writer will not
	 * be able to contniue.
	 */
	return xt_xlog_log_data(thread, sizeof(XTactNoOpEntryDRec), (XTXactLogBufferDPtr) &ent_rec, FALSE);
}

#ifdef XT_NOT_INLINE
xtOpSeqNo XTTableSeq::ts_set_op_seq(XTTabCachePagePtr page)
{
	xtOpSeqNo seq;

	xt_lock_mutex_ns(&ts_ns_lock);
	page->tcp_op_seq = seq = ts_next_seq++;
	xt_unlock_mutex_ns(&ts_ns_lock);
	return seq;
}

xtOpSeqNo XTTableSeq::ts_get_op_seq()
{
	xtOpSeqNo seq;

	xt_lock_mutex_ns(&ts_ns_lock);
	seq = ts_next_seq++;
	xt_unlock_mutex_ns(&ts_ns_lock);
	return seq;
}
#endif

#ifdef XT_NOT_INLINE
/*
 * Return TRUE if the current sequence is before the
 * target (then) sequence number. This function
 * takes into account overflow. Overflow is detected
 * by checking the difference between the 2 values.
 * If the difference is very large, then we
 * assume overflow.
 */
xtBool XTTableSeq::xt_op_is_before(register xtOpSeqNo now, register xtOpSeqNo then)
{
	ASSERT_NS(sizeof(xtOpSeqNo) == 4);
	/* The now time is being incremented.
	 * If it is after the then time (which is static, then
	 * it is not before!
	 */
	if (now >= then) {
		if ((now - then) > (xtOpSeqNo) 0xFFFFFFFF/2)
			return TRUE;
		return FALSE;
	}

	/* If it appears to be before, we still have to check
	 * for overflow. If the gap is bigger then half of
	 * the MAX value, then we can assume it has wrapped around
	 * because we know that no then can be so far in the
	 * future!
	 */
	if ((then - now) > (xtOpSeqNo) 0xFFFFFFFF/2)
		return FALSE;
	return TRUE;
}
#endif


/* ----------------------------------------------------------------------
 * F R E E E R    P R O C E S S
 */

/*
 * Used by the writer to wake the freeer.
 */
xtPublic void xt_wr_wake_freeer(XTThreadPtr self)
{
	xt_lock_mutex(self, &xt_tab_cache.tcm_freeer_lock);
	pushr_(xt_unlock_mutex, &xt_tab_cache.tcm_freeer_lock);
	if (!xt_broadcast_cond_ns(&xt_tab_cache.tcm_freeer_cond))
		xt_log_and_clear_exception_ns();
	freer_(); // xt_unlock_mutex(&xt_tab_cache.tcm_freeer_lock)
}

/* Wait for a transaction to quit: */
static void tabc_fr_wait_for_cache(XTThreadPtr self, u_int msecs)
{
	if (!self->t_quit)
		xt_timed_wait_cond(NULL, &xt_tab_cache.tcm_freeer_cond, &xt_tab_cache.tcm_freeer_lock, msecs);
}

typedef struct TCResource {
	XTOpenTablePtr		tc_ot;
} TCResourceRec, *TCResourcePtr;

static void tabc_free_fr_resources(XTThreadPtr self, TCResourcePtr tc)
{
	if (tc->tc_ot) {
		xt_db_return_table_to_pool(self, tc->tc_ot);
		tc->tc_ot = NULL;
	}
}

static XTTableHPtr tabc_get_table(XTThreadPtr self, TCResourcePtr tc, xtDatabaseID db_id, xtTableID tab_id)
{
	XTTableHPtr	tab;
	XTDatabaseHPtr	db;

	if (tc->tc_ot) {
		tab = tc->tc_ot->ot_table;
		if (tab->tab_id == tab_id && tab->tab_db->db_id == db_id)
			return tab;

		xt_db_return_table_to_pool(self, tc->tc_ot);
		tc->tc_ot = NULL;
	}

	if (!tc->tc_ot) {
		if (!(db = xt_get_database_by_id(self, db_id)))
			return NULL;

		pushr_(xt_heap_release, db);
		tc->tc_ot = xt_db_open_pool_table(self, db, tab_id, NULL, TRUE);
		freer_(); // xt_heap_release(db);
		if (!tc->tc_ot)
			return NULL;
	}

	return tc->tc_ot->ot_table;
}

/*
 * Free the given page, or the least recently used page.
 * Return the amount of bytes freed.
 */
static size_t tabc_free_page(XTThreadPtr self, TCResourcePtr tc)
{
	register XTTabCacheMemPtr	dcg = &xt_tab_cache;
	XTTableHPtr					tab = NULL;
	XTTabCachePagePtr			page, lpage, ppage;
	XTTabCacheSegPtr			seg;
	u_int						page_cnt;
	xtBool						was_dirty;

#ifdef DEBUG_CHECK_CACHE
	//XT_TC_check_cache();
#endif
	dcg->tcm_free_try_count = 0;

	retry:
	/* Note, handling the page is safe because
	 * there is only one free'er thread which
	 * can remove pages from the cache!
	 */
	page_cnt = 0;
	if (!(page = dcg->tcm_lru_page)) {
		dcg->tcm_free_try_count = 0;
		return 0;
	}

	retry_2:
	if ((was_dirty = page->tcp_dirty)) {
		/* Do all this stuff without a lock, because to
		 * have a lock while doing this is too expensive!
		 */
	
		/* Wait for the page to be cleaned. */
		tab = tabc_get_table(self, tc, page->tcp_db_id, page->tcp_tab_id);
	}

	seg = &dcg->tcm_segment[page->tcp_seg];
	TAB_CAC_WRITE_LOCK(&seg->tcs_lock, self->t_id);

	if (page->tcp_dirty) {
		if (!was_dirty) {
			TAB_CAC_UNLOCK(&seg->tcs_lock, self->t_id);
			goto retry_2;
		}

		if (tab) {
			ASSERT(!XTTableSeq::xt_op_is_before(tab->tab_seq.ts_next_seq, page->tcp_op_seq+1));
			/* This should never happen. However, is has been occuring,
			 * during multi_update test on Windows.
			 * In particular it occurs after rename of a table, during ALTER.
			 * As if the table was not flushed before the rename!?
			 * To guard against an infinite loop below, I will just continue here.
			 */
			if (XTTableSeq::xt_op_is_before(tab->tab_seq.ts_next_seq, page->tcp_op_seq+1))
				goto go_on;
			/* OK, we have the table, now we check where the current
			 * sequence number is.
			 */
			if (XTTableSeq::xt_op_is_before(tab->tab_head_op_seq, page->tcp_op_seq)) {
				XTDatabaseHPtr db = tab->tab_db;

				rewait:
				TAB_CAC_UNLOCK(&seg->tcs_lock, self->t_id);

				/* Flush the log, in case this is holding up the
				 * writer!
				 */
				if (!db->db_xlog.xlog_flush(self)) {
					dcg->tcm_free_try_count = 0;
					xt_throw(self);
				}

				xt_lock_mutex(self, &db->db_wr_lock);
				pushr_(xt_unlock_mutex, &db->db_wr_lock);

				/* The freeer is now waiting: */
				db->db_wr_freeer_waiting = TRUE;

				/* If the writer is idle, wake it up. 
				 * The writer will commit the changes to the database
				 * which will allow the freeer to free up the cache.
				 */
				if (db->db_wr_idle) {
					if (!xt_broadcast_cond_ns(&db->db_wr_cond))
						xt_log_and_clear_exception_ns();
				}

				/* Go to sleep on the writer's condition.
				 * The writer will wake the free'er before it goes to
				 * sleep!
				 */
				tab->tab_wake_freeer_op = page->tcp_op_seq;
				tab->tab_wr_wake_freeer = TRUE;
				if (!xt_timed_wait_cond_ns(&db->db_wr_cond, &db->db_wr_lock, 30000)) {
					tab->tab_wr_wake_freeer = FALSE;
					db->db_wr_freeer_waiting = FALSE;
					xt_throw(self);
				}
				tab->tab_wr_wake_freeer = FALSE;
				db->db_wr_freeer_waiting = FALSE;
				freer_(); // xt_unlock_mutex(&db->db_wr_lock)

				TAB_CAC_WRITE_LOCK(&seg->tcs_lock, self->t_id);
				if (XTTableSeq::xt_op_is_before(tab->tab_head_op_seq, page->tcp_op_seq))
					goto rewait;
			}
			go_on:;
		}
	}

	/* Wait if the page is being read or locked. */
	if (page->tcp_lock_count) {
		/* (1) If the page is being read, then we should not free
		 *     it immediately.
		 * (2) If a page is locked, the locker may be waiting
		 *     for the freeer to free some cache - this
		 *     causes a deadlock.
		 *
		 * Therefore, we move on, and try to free another page...
		 */
		if (page_cnt < (dcg->tcm_approx_page_count >> 1)) {
			/* Page has not changed MRU position, and we
			 * have looked at less than half of the pages.
			 * Go to the next page...
			 */
			if ((page = page->tcp_mr_used)) {
				page_cnt++;
				TAB_CAC_UNLOCK(&seg->tcs_lock, self->t_id);
				goto retry_2;
			}
		}
		TAB_CAC_UNLOCK(&seg->tcs_lock, self->t_id);
		dcg->tcm_free_try_count++;				

		/* Starting to spin, free the threads: */
		if (dcg->tcm_threads_waiting) {
			if (!xt_broadcast_cond_ns(&dcg->tcm_freeer_cond))
				xt_log_and_clear_exception_ns();
		}
		goto retry;
	}

	/* Page is clean, remove from the hash table: */

	/* Find the page on the list: */
	u_int page_idx = page->tcp_page_idx;
	u_int file_id = page->tcp_file_id;

	ppage = NULL;
	lpage = seg->tcs_hash_table[page->tcp_hash_idx];
	while (lpage) {
		if (lpage->tcp_page_idx == page_idx && lpage->tcp_file_id == file_id)
			break;
		ppage = lpage;
		lpage = lpage->tcp_next;
	}

	if (page == lpage) {
		/* Should be the case! */
		if (ppage)
			ppage->tcp_next = page->tcp_next;
		else
			seg->tcs_hash_table[page->tcp_hash_idx] = page->tcp_next;
	}
#ifdef DEBUG
	else
		ASSERT_NS(FALSE);
#endif

	/* Remove from the MRU list: */
	xt_lock_mutex_ns(&dcg->tcm_lock);
	if (dcg->tcm_lru_page == page)
		dcg->tcm_lru_page = page->tcp_mr_used;
	if (dcg->tcm_mru_page == page)
		dcg->tcm_mru_page = page->tcp_lr_used;
	if (page->tcp_lr_used)
		page->tcp_lr_used->tcp_mr_used = page->tcp_mr_used;
	if (page->tcp_mr_used)
		page->tcp_mr_used->tcp_lr_used = page->tcp_lr_used;
	xt_unlock_mutex_ns(&dcg->tcm_lock);

	/* Free the page: */
	size_t freed_space = offsetof(XTTabCachePageRec, tcp_data) + page->tcp_data_size;
	seg->tcs_cache_in_use -= freed_space;
	xt_free_ns(page);

	TAB_CAC_UNLOCK(&seg->tcs_lock, self->t_id);
	self->st_statistics.st_rec_cache_frees++;
	dcg->tcm_free_try_count = 0;
	return freed_space;
}

static void tabc_fr_main(XTThreadPtr self)
{
	register XTTabCacheMemPtr	dcg = &xt_tab_cache;
	TCResourceRec				tc = { 0 };

	xt_set_low_priority(self);
	dcg->tcm_freeer_busy = TRUE;

	while (!self->t_quit) {		
		size_t cache_used, freed;

		pushr_(tabc_free_fr_resources, &tc);

		while (!self->t_quit) {
			/* Total up the cache memory used: */
			cache_used = 0;
			for (int i=0; i<XT_TC_SEGMENT_COUNT; i++)
				cache_used += dcg->tcm_segment[i].tcs_cache_in_use;
			if (cache_used > dcg->tcm_cache_high) {
				dcg->tcm_cache_high = cache_used;
			}

			/* Check if the cache usage is over 95%: */
			if (self->t_quit || cache_used < dcg->tcm_high_level)
				break;

			/* Reduce cache to the 75% level: */
			while (!self->t_quit && cache_used > dcg->tcm_low_level) {
				freed = tabc_free_page(self, &tc);
				cache_used -= freed;
				if (cache_used <= dcg->tcm_high_level) {
					/* Wakeup any threads that are waiting for some cache to be
					 * freed.
					 */
					if (dcg->tcm_threads_waiting) {
						if (!xt_broadcast_cond_ns(&dcg->tcm_freeer_cond))
							xt_log_and_clear_exception_ns();
					}
				}
			}
		}

		freer_(); // tabc_free_fr_resources(&tc)

		xt_lock_mutex(self, &dcg->tcm_freeer_lock);
		pushr_(xt_unlock_mutex, &dcg->tcm_freeer_lock);

		if (dcg->tcm_threads_waiting) {
			/* Wake threads before we go to sleep: */
			if (!xt_broadcast_cond_ns(&dcg->tcm_freeer_cond))
				xt_log_and_clear_exception_ns();
		}
			
		/* Wait for a thread that allocates data to signal
		 * that the cache level has exceeeded the upper limit:
		 */
		xt_db_approximate_time = time(NULL);
		dcg->tcm_freeer_busy = FALSE;
		tabc_fr_wait_for_cache(self, 500);
		//tabc_fr_wait_for_cache(self, 30*1000);
		dcg->tcm_freeer_busy = TRUE;
		xt_db_approximate_time = time(NULL);
		freer_(); // xt_unlock_mutex(&dcg->tcm_freeer_lock)
	}
}

static void *tabc_fr_run_thread(XTThreadPtr self)
{
	int		count;
	void	*mysql_thread;

	mysql_thread = myxt_create_thread();

	while (!self->t_quit) {
		try_(a) {
			tabc_fr_main(self);
		}
		catch_(a) {
			/* This error is "normal"! */
			if (!(self->t_exception.e_xt_err == XT_SIGNAL_CAUGHT &&
				self->t_exception.e_sys_err == SIGTERM))
				xt_log_and_clear_exception(self);
		}
		cont_(a);

		/* After an exception, pause before trying again... */
		/* Number of seconds */
#ifdef DEBUG
		count = 10;
#else
		count = 2*60;
#endif
		while (!self->t_quit && count > 0) {
			xt_db_approximate_time = xt_trace_clock();
			sleep(1);
			count--;
		}
	}

   /*
	* {MYSQL-THREAD-KILL}
	myxt_destroy_thread(mysql_thread, TRUE);
	*/
	return NULL;
}

static void tabc_fr_free_thread(XTThreadPtr self, void *XT_UNUSED(data))
{
	if (xt_tab_cache.tcm_freeer_thread) {
		xt_lock_mutex(self, &xt_tab_cache.tcm_freeer_lock);
		pushr_(xt_unlock_mutex, &xt_tab_cache.tcm_freeer_lock);
		xt_tab_cache.tcm_freeer_thread = NULL;
		freer_(); // xt_unlock_mutex(&xt_tab_cache.tcm_freeer_lock)
	}
}

xtPublic void xt_start_freeer(XTThreadPtr self)
{
	xt_tab_cache.tcm_freeer_thread = xt_create_daemon(self, "free-er");
	xt_set_thread_data(xt_tab_cache.tcm_freeer_thread, NULL, tabc_fr_free_thread);
	xt_run_thread(self, xt_tab_cache.tcm_freeer_thread, tabc_fr_run_thread);
}

xtPublic void xt_quit_freeer(XTThreadPtr self)
{
	if (xt_tab_cache.tcm_freeer_thread) {
		xt_lock_mutex(self, &xt_tab_cache.tcm_freeer_lock);
		pushr_(xt_unlock_mutex, &xt_tab_cache.tcm_freeer_lock);
		xt_terminate_thread(self, xt_tab_cache.tcm_freeer_thread);
		freer_(); // xt_unlock_mutex(&xt_tab_cache.tcm_freeer_lock)
	}
}

xtPublic void xt_stop_freeer(XTThreadPtr self)
{
	XTThreadPtr thr_fr;

	if (xt_tab_cache.tcm_freeer_thread) {
		xt_lock_mutex(self, &xt_tab_cache.tcm_freeer_lock);
		pushr_(xt_unlock_mutex, &xt_tab_cache.tcm_freeer_lock);

		/* This pointer is safe as long as you have the transaction lock. */
		if ((thr_fr = xt_tab_cache.tcm_freeer_thread)) {
			xtThreadID tid = thr_fr->t_id;

			/* Make sure the thread quits when woken up. */
			xt_terminate_thread(self, thr_fr);

			/* Wake the freeer to get it to quit: */
			if (!xt_broadcast_cond_ns(&xt_tab_cache.tcm_freeer_cond))
				xt_log_and_clear_exception_ns();
	
			freer_(); // xt_unlock_mutex(&xt_tab_cache.tcm_freeer_lock)

			/*
			 * GOTCHA: This is a wierd thing but the SIGTERM directed
			 * at a particular thread (in this case the sweeper) was
			 * being caught by a different thread and killing the server
			 * sometimes. Disconcerting.
			 * (this may only be a problem on Mac OS X)
			xt_kill_thread(thread);
			 */
			xt_wait_for_thread(tid, FALSE);
	
			/* PMC - This should not be necessary to set the signal here, but in the
			 * debugger the handler is not called!!?
			thr_fr->t_delayed_signal = SIGTERM;
			xt_kill_thread(thread);
			 */
			xt_tab_cache.tcm_freeer_thread = NULL;
		}
		else
			freer_(); // xt_unlock_mutex(&xt_tab_cache.tcm_freeer_lock)
	}
}

xtPublic void xt_load_pages(XTThreadPtr self, XTOpenTablePtr ot)
{
	XTTableHPtr			tab = ot->ot_table;
	xtRecordID			rec_id;
	XTTabCachePagePtr	page;
	XTTabCacheSegPtr	seg;
	size_t				poffset;

	rec_id = 1;
	while (rec_id<tab->tab_row_eof_id) {
		if (!tab->tab_rows.tc_fetch(ot->ot_row_file, rec_id, &seg, &page, &poffset, TRUE, self))
			xt_throw(self);
		TAB_CAC_UNLOCK(&seg->tcs_lock, self->t_id);
		rec_id += tab->tab_rows.tci_rows_per_page;
	}

	rec_id = 1;
	while (rec_id<tab->tab_rec_eof_id) {
		if (!tab->tab_recs.tc_fetch(ot->ot_rec_file, rec_id, &seg, &page, &poffset, TRUE, self))
			xt_throw(self);
		TAB_CAC_UNLOCK(&seg->tcs_lock, self->t_id);
		rec_id += tab->tab_recs.tci_rows_per_page;
	}
}


