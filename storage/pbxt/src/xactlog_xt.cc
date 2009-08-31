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
 * The transaction log contains all operations on the data handle
 * and row pointer files of a table.
 *
 * The transaction log does not contain operations on index data.
 */

#include "xt_config.h"

#ifdef DRIZZLED
#include <bitset>
#endif

#include <signal.h>

#include "xactlog_xt.h"
#include "database_xt.h"
#include "util_xt.h"
#include "strutil_xt.h"
#include "filesys_xt.h"
#include "myxt_xt.h"
#include "trace_xt.h"

#ifdef DEBUG
//#define PRINT_TABLE_MODIFICATIONS
//#define TRACE_WRITER_ACTIVITY
#endif
#ifndef XT_WIN
#ifndef XT_MAC
#define PREWRITE_LOG_COMPLETELY
#endif
#endif

static void xlog_wr_log_written(XTDatabaseHPtr db);

/*
 * -----------------------------------------------------------------------
 * T R A N S A C T I O   L O G   C A C H E
 */

static XTXLogCacheRec	xt_xlog_cache;

/*
 * Initialize the disk cache.
 */
xtPublic void xt_xlog_init(XTThreadPtr self, size_t cache_size)
{
	XTXLogBlockPtr	block;

	/*
	 * This is required to ensure that the block
	 * works!
	 */

	/* Determine the number of block that will fit into the given memory: */
	/*
	xt_xlog_cache.xlc_hash_size = (cache_size / (XLC_SEGMENT_COUNT * sizeof(XTXLogBlockPtr) + sizeof(XTXLogBlockRec))) / (XLC_SEGMENT_COUNT >> 1);
	xt_xlog_cache.xlc_block_count = (cache_size - (XLC_SEGMENT_COUNT * xt_xlog_cache.xlc_hash_size * sizeof(XTXLogBlockPtr))) / sizeof(XTXLogBlockRec);
	*/
	/* Do not count the size of the cache directory towards the cache size: */
	xt_xlog_cache.xlc_block_count = cache_size / sizeof(XTXLogBlockRec);
	xt_xlog_cache.xlc_upper_limit = ((xtWord8) xt_xlog_cache.xlc_block_count * (xtWord8) XT_XLC_BLOCK_SIZE * (xtWord8) 3) / (xtWord8) 4;
	xt_xlog_cache.xlc_hash_size = xt_xlog_cache.xlc_block_count / (XLC_SEGMENT_COUNT >> 1);
	if (!xt_xlog_cache.xlc_hash_size)
		xt_xlog_cache.xlc_hash_size = 1;

	try_(a) {
		for (u_int i=0; i<XLC_SEGMENT_COUNT; i++) {
			xt_xlog_cache.xlc_segment[i].lcs_hash_table = (XTXLogBlockPtr *) xt_calloc(self, xt_xlog_cache.xlc_hash_size * sizeof(XTXLogBlockPtr));
			xt_init_mutex_with_autoname(self, &xt_xlog_cache.xlc_segment[i].lcs_lock);
			xt_init_cond(self, &xt_xlog_cache.xlc_segment[i].lcs_cond);
		}

		block = (XTXLogBlockPtr) xt_malloc(self, xt_xlog_cache.xlc_block_count * sizeof(XTXLogBlockRec));
		xt_xlog_cache.xlc_blocks = block; 
		xt_xlog_cache.xlc_blocks_end = (XTXLogBlockPtr) ((char *) block + (xt_xlog_cache.xlc_block_count * sizeof(XTXLogBlockRec))); 
		xt_xlog_cache.xlc_next_to_free = block; 
		xt_init_mutex_with_autoname(self, &xt_xlog_cache.xlc_lock);
		xt_init_cond(self, &xt_xlog_cache.xlc_cond);

		for (u_int i=0; i<xt_xlog_cache.xlc_block_count; i++) {
			block->xlb_address = 0;
			block->xlb_log_id = 0;
			block->xlb_state = XLC_BLOCK_FREE;
			block++;
		}
		xt_xlog_cache.xlc_free_count = xt_xlog_cache.xlc_block_count;
	}
	catch_(a) {
		xt_xlog_exit(self);
		throw_();
	}
	cont_(a);
}

xtPublic void xt_xlog_exit(XTThreadPtr self)
{
	for (u_int i=0; i<XLC_SEGMENT_COUNT; i++) {
		if (xt_xlog_cache.xlc_segment[i].lcs_hash_table) {
			xt_free(self, xt_xlog_cache.xlc_segment[i].lcs_hash_table);
			xt_xlog_cache.xlc_segment[i].lcs_hash_table = NULL;
			xt_free_mutex(&xt_xlog_cache.xlc_segment[i].lcs_lock);
			xt_free_cond(&xt_xlog_cache.xlc_segment[i].lcs_cond);
		}
	}

	if (xt_xlog_cache.xlc_blocks) {
		xt_free(self, xt_xlog_cache.xlc_blocks);
		xt_xlog_cache.xlc_blocks = NULL;
		xt_free_mutex(&xt_xlog_cache.xlc_lock);
		xt_free_cond(&xt_xlog_cache.xlc_cond);
	}
	memset(&xt_xlog_cache, 0, sizeof(xt_xlog_cache));
}

xtPublic xtInt8 xt_xlog_get_usage()
{
	xtInt8 size;

	size = (xtInt8) (xt_xlog_cache.xlc_block_count - xt_xlog_cache.xlc_free_count) * sizeof(XTXLogBlockRec);
	return size;
}

xtPublic xtInt8 xt_xlog_get_size()
{
	xtInt8 size;

	size = (xtInt8) xt_xlog_cache.xlc_block_count * sizeof(XTXLogBlockRec);
	return size;
}

xtPublic xtLogID xt_xlog_get_min_log(XTThreadPtr self, XTDatabaseHPtr db)
{
	char			path[PATH_MAX];
	XTOpenDirPtr	od;
	char			*file;
	xtLogID			log_id, min_log = 0;

	xt_strcpy(PATH_MAX, path, db->db_main_path);
	xt_add_system_dir(PATH_MAX, path);
	if (xt_fs_exists(path)) {
		pushsr_(od, xt_dir_close, xt_dir_open(self, path, NULL));
		while (xt_dir_next(self, od)) {
			file = xt_dir_name(self, od);
			if (xt_starts_with(file, "xlog")) {
				if ((log_id = (xtLogID) xt_file_name_to_id(file))) {
					if (!min_log || log_id < min_log)
						min_log = log_id;
				}
			}
		}
		freer_(); // xt_dir_close(od)
	}
	if (!min_log)
		return 1;
	return min_log;
}

xtPublic void xt_xlog_delete_logs(XTThreadPtr self, XTDatabaseHPtr db)
{
	char			path[PATH_MAX];
	XTOpenDirPtr	od;
	char			*file;

	/* Close all the index logs before we delete them: */
	db->db_indlogs.ilp_close(self, TRUE);

	/* Close the transaction logs too: */
	db->db_xlog.xlog_close(self);

	xt_strcpy(PATH_MAX, path, db->db_main_path);
	xt_add_system_dir(PATH_MAX, path);
	if (!xt_fs_exists(path))
		return;
	pushsr_(od, xt_dir_close, xt_dir_open(self, path, NULL));
	while (xt_dir_next(self, od)) {
		file = xt_dir_name(self, od);
		if (xt_ends_with(file, ".xt")) {
			xt_add_dir_char(PATH_MAX, path);
			xt_strcat(PATH_MAX, path, file);
			xt_fs_delete(self, path);
			xt_remove_last_name_of_path(path);
		}
	}
	freer_(); // xt_dir_close(od)

	/* I no longer attach the condition: !db->db_multi_path
	 * to removing this directory. This is because
	 * the pbxt directory must now be removed explicitly
	 * by drop database, or by delete all the PBXT
	 * system tables.
	 */
	if (!xt_fs_rmdir(NULL, path))
		xt_log_and_clear_exception(self);
}

#ifdef DEBUG_CHECK_CACHE
static void xt_xlog_check_cache(void)
{
	XTXLogBlockPtr	block, pblock;
	u_int			used_count;
	u_int			free_count;

	// Check the LRU list:
	used_count = 0;
	pblock = NULL;
	block = xt_xlog_cache.xlc_lru_block;
	while (block) {
		used_count++;
		ASSERT_NS(block->xlb_state != XLC_BLOCK_FREE);
		ASSERT_NS(block->xlb_lr_used == pblock);
		pblock = block;
		block = block->xlb_mr_used;
	}
	ASSERT_NS(xt_xlog_cache.xlc_mru_block == pblock);
	ASSERT_NS(xt_xlog_cache.xlc_free_count + used_count == xt_xlog_cache.xlc_block_count);

	// Check the free list:
	free_count = 0;
	block = xt_xlog_cache.xlc_free_list;
	while (block) {
		free_count++;
		ASSERT_NS(block->xlb_state == XLC_BLOCK_FREE);
		block = block->xlb_next;
	}
	ASSERT_NS(xt_xlog_cache.xlc_free_count == free_count);
}
#endif

#ifdef FOR_DEBUG
static void xlog_check_lru_list(XTXLogBlockPtr block)
{
	XTXLogBlockPtr list_block, plist_block;
	
	plist_block = NULL;
	list_block = xt_xlog_cache.xlc_lru_block;
	while (list_block) {
		ASSERT_NS(block != list_block);
		ASSERT_NS(list_block->xlb_lr_used == plist_block);
		plist_block = list_block;
		list_block = list_block->xlb_mr_used;
	}
	ASSERT_NS(xt_xlog_cache.xlc_mru_block == plist_block);
}
#endif

/*
 * Log cache blocks are used and freed on a round-robin basis.
 * In addition, only data read by restart, and data transfered
 * from the transaction log are stored in the transaction log.
 *
 * This ensures that the transaction log contains the most
 * recently written log data.
 *
 * If the sweeper gets behind due to a long running transacation
 * then it falls out of the log cache, and must read from
 * the log files directly.
 *
 * This data read is no longer cached as it was previously.
 * This has the advantage that it does not disturn the writter
 * thread which would otherwise hit the cache.
 *
 * If transactions are not too long, it should be possible
 * to keep the sweeper in the log cache.
 */
static xtBool xlog_free_block(XTXLogBlockPtr to_free)
{
	XTXLogBlockPtr		block, pblock;
	xtLogID				log_id;
	off_t				address;
	XTXLogCacheSegPtr	seg;
	u_int				hash_idx;

	retry:
	log_id = to_free->xlb_log_id;
	address = to_free->xlb_address;

	seg = &xt_xlog_cache.xlc_segment[((u_int) address >> XT_XLC_BLOCK_SHIFTS) & XLC_SEGMENT_MASK];
	hash_idx = (((u_int) (address >> (XT_XLC_SEGMENT_SHIFTS + XT_XLC_BLOCK_SHIFTS))) ^ (log_id << 16)) % xt_xlog_cache.xlc_hash_size;

	xt_lock_mutex_ns(&seg->lcs_lock);
	if (to_free->xlb_state == XLC_BLOCK_FREE)
		goto done_ok;
	if (to_free->xlb_log_id != log_id || to_free->xlb_address != address) {
		xt_unlock_mutex_ns(&seg->lcs_lock);
		goto retry;
	}

	pblock = NULL;
	block = seg->lcs_hash_table[hash_idx];
	while (block) {
		if (block->xlb_address == address && block->xlb_log_id == log_id) {
			ASSERT_NS(block == to_free);
			ASSERT_NS(block->xlb_state != XLC_BLOCK_FREE);
			
			/* Wait if the block is being read: */
			if (block->xlb_state == XLC_BLOCK_READING) {
				/* Wait for the block to be read, then try again. */
				if (!xt_timed_wait_cond_ns(&seg->lcs_cond, &seg->lcs_lock, 100))
					goto failed;
				xt_unlock_mutex_ns(&seg->lcs_lock);
				goto retry;
			}
			
			goto free_the_block;
		}
		pblock = block;
		block = block->xlb_next;
	}

	/* We did not find the block, someone else freed it... */
	xt_unlock_mutex_ns(&seg->lcs_lock);
	goto retry;

	free_the_block:
	ASSERT_NS(block->xlb_state == XLC_BLOCK_CLEAN);

	/* Remove from the hash table: */
	if (pblock)
		pblock->xlb_next = block->xlb_next;
	else
		seg->lcs_hash_table[hash_idx] = block->xlb_next;

	/* Free the block: */
	xt_xlog_cache.xlc_free_count++;
	block->xlb_state = XLC_BLOCK_FREE;

	done_ok:
	xt_unlock_mutex_ns(&seg->lcs_lock);
	return OK;
	
	failed:
	xt_unlock_mutex_ns(&seg->lcs_lock);
	return FAILED;
}

#define XT_FETCH_READ		0
#define XT_FETCH_BLANK		1
#define XT_FETCH_TEST		2

static xtBool xlog_fetch_block(XTXLogBlockPtr *ret_block, XTOpenFilePtr file, xtLogID log_id, off_t address, XTXLogCacheSegPtr *ret_seg, int fetch_type, XTThreadPtr thread)
{
	register XTXLogBlockPtr		block;
	register XTXLogCacheSegPtr	seg;
	register u_int				hash_idx;
	register XTXLogCacheRec		*dcg = &xt_xlog_cache;
	size_t						red_size;

	/* Make sure we have a free block ready (to avoid unlock below): */
	if (fetch_type != XT_FETCH_TEST && dcg->xlc_next_to_free->xlb_state != XLC_BLOCK_FREE) {
		if (!xlog_free_block(dcg->xlc_next_to_free))
			return FAILED;
	}

	seg = &dcg->xlc_segment[((u_int) address >> XT_XLC_BLOCK_SHIFTS) & XLC_SEGMENT_MASK];
	hash_idx = (((u_int) (address >> (XT_XLC_SEGMENT_SHIFTS + XT_XLC_BLOCK_SHIFTS))) ^ (log_id << 16)) % dcg->xlc_hash_size;

	xt_lock_mutex_ns(&seg->lcs_lock);
	retry:
	block = seg->lcs_hash_table[hash_idx];
	while (block) {
		if (block->xlb_address == address && block->xlb_log_id == log_id) {
			ASSERT_NS(block->xlb_state != XLC_BLOCK_FREE);

			/*
			 * Wait if the block is being read.
			 */
			if (block->xlb_state == XLC_BLOCK_READING) {
				if (!xt_timed_wait_cond_ns(&seg->lcs_cond, &seg->lcs_lock, 100)) {
					xt_unlock_mutex_ns(&seg->lcs_lock);
					return FAILED;
				}
				goto retry;
			}

			*ret_seg = seg;
			*ret_block = block;
			thread->st_statistics.st_xlog_cache_hit++;
			return OK;
		}
		block = block->xlb_next;
	}

	if (fetch_type == XT_FETCH_TEST) {
		xt_unlock_mutex_ns(&seg->lcs_lock);
		*ret_seg = NULL;
		*ret_block = NULL;
		thread->st_statistics.st_xlog_cache_miss++;
		return OK;
	}

	/* Block not found: */
	get_free_block:
	if (dcg->xlc_next_to_free->xlb_state != XLC_BLOCK_FREE) {
		xt_unlock_mutex_ns(&seg->lcs_lock);
		if (!xlog_free_block(dcg->xlc_next_to_free))
			return FAILED;
		xt_lock_mutex_ns(&seg->lcs_lock);
	}

	xt_lock_mutex_ns(&dcg->xlc_lock);
	block = dcg->xlc_next_to_free;
	if (block->xlb_state != XLC_BLOCK_FREE) {
		xt_unlock_mutex_ns(&dcg->xlc_lock);
		goto get_free_block;
	}
	dcg->xlc_next_to_free++;
	if (dcg->xlc_next_to_free == dcg->xlc_blocks_end)
		dcg->xlc_next_to_free = dcg->xlc_blocks;
	dcg->xlc_free_count--;

	if (fetch_type == XT_FETCH_READ) {
		block->xlb_address = address;
		block->xlb_log_id = log_id;
		block->xlb_state = XLC_BLOCK_READING;

		xt_unlock_mutex_ns(&dcg->xlc_lock);

		/* Add the block to the hash table: */
		block->xlb_next = seg->lcs_hash_table[hash_idx];
		seg->lcs_hash_table[hash_idx] = block;

		/* Read the block into memory: */
		xt_unlock_mutex_ns(&seg->lcs_lock);

		if (!xt_pread_file(file, address, XT_XLC_BLOCK_SIZE, 0, block->xlb_data, &red_size, &thread->st_statistics.st_xlog, thread))
			return FAILED;
		memset(block->xlb_data + red_size, 0, XT_XLC_BLOCK_SIZE - red_size);
		thread->st_statistics.st_xlog_cache_miss++;

		xt_lock_mutex_ns(&seg->lcs_lock);
		block->xlb_state = XLC_BLOCK_CLEAN;
		xt_cond_wakeall(&seg->lcs_cond);
	}
	else {
		block->xlb_address = address;
		block->xlb_log_id = log_id;
		block->xlb_state = XLC_BLOCK_CLEAN;
		memset(block->xlb_data, 0, XT_XLC_BLOCK_SIZE);

		xt_unlock_mutex_ns(&dcg->xlc_lock);

		/* Add the block to the hash table: */
		block->xlb_next = seg->lcs_hash_table[hash_idx];
		seg->lcs_hash_table[hash_idx] = block;
	}

	*ret_seg = seg;
	*ret_block = block;
#ifdef DEBUG_CHECK_CACHE
	//xt_xlog_check_cache();
#endif
	return OK;
}

static xtBool xlog_transfer_to_cache(XTOpenFilePtr file, xtLogID log_id, off_t offset, size_t size, xtWord1 *data, XTThreadPtr thread)
{
	off_t				address;
	XTXLogBlockPtr		block;
	XTXLogCacheSegPtr	seg;
	size_t				boff;
	size_t				tfer;
	xtBool				read_block = FALSE;

#ifdef DEBUG_CHECK_CACHE
	//xt_xlog_check_cache();
#endif
	/* We have to read the first block, if we are
	 * not at the begining of the file:
	 */
	if (offset)
		read_block = TRUE;
	address = offset & ~XT_XLC_BLOCK_MASK;

	boff = (size_t) (offset - address);
	tfer = XT_XLC_BLOCK_SIZE - boff;
	if (tfer > size)
		tfer = size;
	while (size > 0) {
		if (!xlog_fetch_block(&block, file, log_id, address, &seg, read_block ? XT_FETCH_READ : XT_FETCH_BLANK, thread)) {
#ifdef DEBUG_CHECK_CACHE
			//xt_xlog_check_cache();
#endif
			return FAILED;
		}
		ASSERT_NS(block && block->xlb_state == XLC_BLOCK_CLEAN);
		memcpy(block->xlb_data + boff, data, tfer);
		xt_unlock_mutex_ns(&seg->lcs_lock);
		size -= tfer;
		data += tfer;

		/* Following block need not be read
		 * because we always transfer to the
		 * end of the file!
		 */
		read_block = FALSE;
		address += XT_XLC_BLOCK_SIZE;

		boff = 0;
		tfer = size;
		if (tfer > XT_XLC_BLOCK_SIZE)
			tfer = XT_XLC_BLOCK_SIZE;
	}
#ifdef DEBUG_CHECK_CACHE
	//xt_xlog_check_cache();
#endif
	return OK;
}

static xtBool xt_xlog_read(XTOpenFilePtr file, xtLogID log_id, off_t offset, size_t size, xtWord1 *data, xtBool load_cache, XTThreadPtr thread)
{
	off_t				address;
	XTXLogBlockPtr		block;
	XTXLogCacheSegPtr	seg;
	size_t				boff;
	size_t				tfer;

#ifdef DEBUG_CHECK_CACHE
	//xt_xlog_check_cache();
#endif
	address = offset & ~XT_XLC_BLOCK_MASK;
	boff = (size_t) (offset - address);
	tfer = XT_XLC_BLOCK_SIZE - boff;
	if (tfer > size)
		tfer = size;
	while (size > 0) {
		if (!xlog_fetch_block(&block, file, log_id, address, &seg, load_cache ? XT_FETCH_READ : XT_FETCH_TEST, thread))
			return FAILED;
		if (!block) {
			size_t red_size;

			if (!xt_pread_file(file, address + boff, size, 0, data, &red_size, &thread->st_statistics.st_xlog, thread))
				return FAILED;
			memset(data + red_size, 0, size - red_size);
			return OK;
		}
		memcpy(data, block->xlb_data + boff, tfer);
		xt_unlock_mutex_ns(&seg->lcs_lock);
		size -= tfer;
		data += tfer;
		address += XT_XLC_BLOCK_SIZE;
		boff = 0;
		tfer = size;
		if (tfer > XT_XLC_BLOCK_SIZE)
			tfer = XT_XLC_BLOCK_SIZE;
	}
#ifdef DEBUG_CHECK_CACHE
	//xt_xlog_check_cache();
#endif
	return OK;
}

static xtBool xt_xlog_write(XTOpenFilePtr file, xtLogID log_id, off_t offset, size_t size, xtWord1 *data, XTThreadPtr thread)
{
	if (!xt_pwrite_file(file, offset, size, data, &thread->st_statistics.st_xlog, thread))
		return FAILED;
	return xlog_transfer_to_cache(file, log_id, offset, size, data, thread);
}

/*
 * -----------------------------------------------------------------------
 * D A T A B A S E   T R A N S A C T I O N   L O G S
 */

void XTDatabaseLog::xlog_setup(XTThreadPtr self, XTDatabaseHPtr db, off_t inp_log_file_size, size_t transaction_buffer_size, int log_count)
{
	volatile off_t	log_file_size = inp_log_file_size;
	size_t			log_size;

	try_(a) {
		memset(this, 0, sizeof(XTDatabaseLogRec));

		if (log_count <= 1)
			log_count = 1;
		else if (log_count > 1000000)
			log_count = 1000000;

		xl_db = db;

		xl_log_file_threshold = xt_align_offset(log_file_size, 1024);
		xl_log_file_count = log_count;
		xl_size_of_buffers = transaction_buffer_size;
	
		xt_init_mutex_with_autoname(self, &xl_write_lock);
		xt_init_cond(self, &xl_write_cond);
#ifdef XT_XLOG_WAIT_SPINS
		xt_writing = 0;
		xt_waiting = 0;
#else
		xt_writing = FALSE;
#endif
		xl_log_id = 0;
		xl_log_file = 0;
	
		xt_spinlock_init_with_autoname(self, &xl_buffer_lock);

		/* Note that we allocate a little bit more for each buffer
		 * in order to make sure that we can write a trailing record
		 * to the log buffer.
		 */
		log_size = transaction_buffer_size + sizeof(XTXactNewLogEntryDRec);
		
		/* Add in order to round the buffer to an integral of 512 */
		if (log_size % 512)
			log_size += (512 - (log_size % 512));

		xl_write_log_id = 0;
		xl_write_log_offset = 0;
		xl_write_buf_pos = 0;
		xl_write_buf_pos_start = 0;
		xl_write_buffer = (xtWord1 *) xt_malloc(self, log_size);
		xl_write_done = TRUE;

		xl_append_log_id = 0;
		xl_append_log_offset = 0;
		xl_append_buf_pos = 0;
		xl_append_buf_pos_start = 0;
		xl_append_buffer = (xtWord1 *) xt_malloc(self, log_size);

		xl_last_flush_time = 10;
		xl_flush_log_id = 0;
		xl_flush_log_offset = 0;
	}
	catch_(a) {
		xlog_exit(self);
		throw_();
	}
	cont_(a);
}

xtBool XTDatabaseLog::xlog_set_write_offset(xtLogID log_id, xtLogOffset log_offset, xtLogID max_log_id, XTThreadPtr thread)
{
	xl_max_log_id = max_log_id;

	xl_write_log_id = log_id;
	xl_write_log_offset = log_offset;
	xl_write_buf_pos = 0;
	xl_write_buf_pos_start = 0;
	xl_write_done = TRUE;

	xl_append_log_id = log_id;
	xl_append_log_offset = log_offset;
	if (log_offset == 0) {
		XTXactLogHeaderDPtr log_head;

		log_head = (XTXactLogHeaderDPtr) xl_append_buffer;
		memset(log_head, 0, sizeof(XTXactLogHeaderDRec));
		log_head->xh_status_1 = XT_LOG_ENT_HEADER;
		log_head->xh_checksum_1 = XT_CHECKSUM_1(log_id);
		XT_SET_DISK_4(log_head->xh_size_4, sizeof(XTXactLogHeaderDRec));
		XT_SET_DISK_4(log_head->xh_log_id_4, log_id);
		XT_SET_DISK_2(log_head->xh_version_2, XT_LOG_VERSION_NO);
		XT_SET_DISK_4(log_head->xh_magic_4, XT_LOG_FILE_MAGIC);
		xl_append_buf_pos = sizeof(XTXactLogHeaderDRec);
		xl_append_buf_pos_start = 0;
	}
	else {
		/* Start the log buffer at a block boundary: */
		size_t buf_pos = (size_t) (log_offset % 512);

		xl_append_buf_pos = buf_pos;
		xl_append_buf_pos_start = buf_pos;
		xl_append_log_offset = log_offset - buf_pos;

		if (!xlog_open_log(log_id, log_offset, thread))
			return FAILED;

		if (!xt_pread_file(xl_log_file, xl_append_log_offset, buf_pos, buf_pos, xl_append_buffer, NULL, &thread->st_statistics.st_xlog, thread))
			return FAILED;
	}

	xl_flush_log_id = log_id;
	xl_flush_log_offset = log_offset;
	return OK;
}

void XTDatabaseLog::xlog_close(XTThreadPtr self)
{
	if (xl_log_file) {
		xt_close_file(self, xl_log_file);
		xl_log_file = NULL;
	}
}

void XTDatabaseLog::xlog_exit(XTThreadPtr self)
{
	xt_spinlock_free(self, &xl_buffer_lock);
	xt_free_mutex(&xl_write_lock);
	xt_free_cond(&xl_write_cond);
	xlog_close(self);
	if (xl_write_buffer) {
		xt_free(self, xl_write_buffer);
		xl_write_buffer = NULL;
	}
	if (xl_append_buffer) {
		xt_free(self, xl_append_buffer);
		xl_append_buffer = NULL;
	}
}

#define WR_NO_SPACE		1
#define WR_FLUSH		2

xtBool XTDatabaseLog::xlog_flush(XTThreadPtr thread)
{
	if (!xlog_flush_pending())
		return OK;
	return xlog_append(thread, 0, NULL, 0, NULL, TRUE, NULL, NULL);
}

xtBool XTDatabaseLog::xlog_flush_pending()
{
	xtLogID		req_flush_log_id;
	xtLogOffset	req_flush_log_offset;

	xt_lck_slock(&xl_buffer_lock);
	req_flush_log_id = xl_append_log_id;
	req_flush_log_offset = xl_append_log_offset + xl_append_buf_pos;
	if (xt_comp_log_pos(req_flush_log_id, req_flush_log_offset, xl_flush_log_id, xl_flush_log_offset) <= 0) {
		xt_spinlock_unlock(&xl_buffer_lock);
		return FALSE;
	}
	xt_spinlock_unlock(&xl_buffer_lock);
	return TRUE;
}

/*
 * Write data to the end of the log buffer.
 *
 * commit is set to true if the caller also requires
 * the log to be flushed, after writing the data.
 *
 * This function returns the log ID and offset of
 * the data write position.
 */
xtBool XTDatabaseLog::xlog_append(XTThreadPtr thread, size_t size1, xtWord1 *data1, size_t size2, xtWord1 *data2, xtBool commit, xtLogID *log_id, xtLogOffset *log_offset)
{
	int			write_reason = 0;
	xtLogID		req_flush_log_id;
	xtLogOffset	req_flush_log_offset;
	size_t		part_size;
	xtWord8		flush_time;
	xtWord2		sum;

	if (!size1) {
		/* Just flush the buffer... */
		xt_lck_slock(&xl_buffer_lock);
		write_reason = WR_FLUSH;
		req_flush_log_id = xl_append_log_id;
		req_flush_log_offset = xl_append_log_offset + xl_append_buf_pos;
		xt_spinlock_unlock(&xl_buffer_lock);
		goto write_log_to_file;
	}
	else {
		req_flush_log_id = 0;
		req_flush_log_offset = 0;
	}

	/*
	 * This is a dirty read, which will send us to the
	 * best starting position:
	 *
	 * If there is space, now, then there is probably
	 * still enough space, after we have locked the
	 * buffer for writting.
	 */
	if (xl_append_buf_pos + size1 + size2 <= xl_size_of_buffers)
		goto copy_to_log_buffer;

	/*
	 * There is not enough space in the append buffer.
	 * So we need to write the log, until there is space.
	 */
	write_reason = WR_NO_SPACE;

	write_log_to_file:
	if (write_reason) {
		/* We need to write for one of 2 reasons: not
		 * enough space in the buffer, or a flush
		 * is required.
		 */
		xtWord8	then;
		 
		/*
		 * The objective of the following code is to
		 * pick one writer, out of all threads.
		 * The rest will wait for the writer.
		 */

		if (write_reason == WR_FLUSH) {
			/* Before we flush, check if we should wait for running
			 * transactions that may commit shortly.
			 */
			if (xl_db->db_xn_writer_count - xl_db->db_xn_writer_wait_count - xl_db->db_xn_long_running_count > 0 && xl_last_flush_time) {
				/* Wait for about as long as the last flush took,
				 * the idea is to saturate the disk with flushing...: */
				then = xt_trace_clock() + (xtWord8) xl_last_flush_time;
				for (;;) {
					xt_critical_wait();
					/* If a thread leaves this loop because times up, or
					 * a thread manages to flush so fast that this thread
					 * sleeps during this time, then it could be that
					 * the required flush occurs before other conditions
					 * of this loop are met!
					 *
					 * So we check here to make sure that the log has not been
					 * flushed as we require:
					 */
					if (xt_comp_log_pos(req_flush_log_id, req_flush_log_offset, xl_flush_log_id, xl_flush_log_offset) <= 0) {
						ASSERT_NS(xt_comp_log_pos(xl_write_log_id, xl_write_log_offset, xl_append_log_id, xl_append_log_offset) <= 0);
						return OK;
					}

					if (xl_db->db_xn_writer_count - xl_db->db_xn_writer_wait_count - xl_db->db_xn_long_running_count > 0)
						break;
					if (xt_trace_clock() >= then)
						break;
				}
			}
		}

#ifdef XT_XLOG_WAIT_SPINS
		/* Spin for 1/1000s: */
		then = xt_trace_clock() + (xtWord8) 1000;
		for (;;) {
			if (!xt_atomic_tas4(&xt_writing, 1))
				break;

			/* If I am not the writer, then I just waited for the
			 * writer. So it may be that my requirements have now
			 * been met!
			 */
			if (write_reason == WR_FLUSH) {
				/* If the reason was to flush, then
				 * check the last flush sequence, maybe it is passed
				 * our required sequence.
				 */
				if (xt_comp_log_pos(req_flush_log_id, req_flush_log_offset, xl_flush_log_id, xl_flush_log_offset) <= 0) {
					/* The required flush position of the log is before
					 * or equal to the actual flush position. This means the condition
					 * for this thread have been satified (via group commit).
					 * Nothing more to do!
					 */
					ASSERT_NS(xt_comp_log_pos(xl_write_log_id, xl_write_log_offset, xl_append_log_id, xl_append_log_offset) <= 0);
					return OK;
				}
			}
			else {
				/* It may be that there is now space in the append buffer: */
				if (xl_append_buf_pos + size1 + size2 <= xl_size_of_buffers)
					goto copy_to_log_buffer;
			}

			if (xt_trace_clock() >= then) {
				xt_lock_mutex_ns(&xl_write_lock);
				xt_waiting++;
				if (!xt_timed_wait_cond_ns(&xl_write_cond, &xl_write_lock, 500)) {
					xt_waiting--;
					xt_unlock_mutex_ns(&xl_write_lock);
					return FALSE;
				}
				xt_waiting--;
				xt_unlock_mutex_ns(&xl_write_lock);
			}
			else
				xt_critical_wait();
		}
#else
		xtBool i_am_writer;

		i_am_writer = FALSE;
		xt_lock_mutex_ns(&xl_write_lock);
		if (xt_writing) {
			if (!xt_timed_wait_cond_ns(&xl_write_cond, &xl_write_lock, 500)) {
				xt_unlock_mutex_ns(&xl_write_lock);
				return FALSE;
			}
		}
		else {
			xt_writing = TRUE;
			i_am_writer = TRUE;
		}
		xt_unlock_mutex_ns(&xl_write_lock);

		if (!i_am_writer) {
			/* If I am not the writer, then I just waited for the
			 * writer. So it may be that my requirements have now
			 * been met!
			 */
			if (write_reason == WR_FLUSH) {
				/* If the reason was to flush, then
				 * check the last flush sequence, maybe it is passed
				 * our required sequence.
				 */
				if (xt_comp_log_pos(req_flush_log_id, req_flush_log_offset, xl_flush_log_id, xl_flush_log_offset) <= 0) {
					/* The required flush position of the log is before
					 * or equal to the actual flush position. This means the condition
					 * for this thread have been satified (via group commit).
					 * Nothing more to do!
					 */
					ASSERT_NS(xt_comp_log_pos(xl_write_log_id, xl_write_log_offset, xl_append_log_id, xl_append_log_offset) <= 0);
					return OK;
				}
				goto write_log_to_file;
			}

			/* It may be that there is now space in the append buffer: */
			if (xl_append_buf_pos + size1 + size2 <= xl_size_of_buffers)
				goto copy_to_log_buffer;
				
			goto write_log_to_file;
		}
#endif

		/* I am the writer, check the conditions, again: */
		if (write_reason == WR_FLUSH) {
			/* The writer wants the log to be flushed to a particular point: */
			if (xt_comp_log_pos(req_flush_log_id, req_flush_log_offset, xl_flush_log_id, xl_flush_log_offset) <= 0) {
				/* The writers required flush position is before or equal
				 * to the actual position, so the writer is done...
				 */
#ifdef XT_XLOG_WAIT_SPINS
				xt_writing = 0;
				if (xt_waiting)
					xt_cond_wakeall(&xl_write_cond);
#else
				xt_writing = FALSE;
				xt_cond_wakeall(&xl_write_cond);
#endif
				ASSERT_NS(xt_comp_log_pos(xl_write_log_id, xl_write_log_offset, xl_append_log_id, xl_append_log_offset) <= 0);
				return OK;
			}
			/* Not flushed, but what about written? */
			if (xt_comp_log_pos(req_flush_log_id, req_flush_log_offset, xl_write_log_id, xl_write_log_offset + (xl_write_done ? xl_write_buf_pos : 0)) <= 0) {
				/* The write position is after or equal to the required flush
				 * position. This means that all we have to do is flush
				 * to satisfy the writers condition.
				 */
				xtBool ok = TRUE;

				if (xl_log_id != xl_write_log_id)
					ok = xlog_open_log(xl_write_log_id, xl_write_log_offset + (xl_write_done ? xl_write_buf_pos : 0), thread);

				if (ok) {
					if (xl_db->db_co_busy) {
						/* [(8)] Flush the compactor log. */
						xt_lock_mutex_ns(&xl_db->db_co_dlog_lock);
						ok = xl_db->db_co_thread->st_dlog_buf.dlb_flush_log(TRUE, thread);
						xt_unlock_mutex_ns(&xl_db->db_co_dlog_lock);
					}
				}

				if (ok) {
					flush_time = thread->st_statistics.st_xlog.ts_flush_time;
					if ((ok = xt_flush_file(xl_log_file, &thread->st_statistics.st_xlog, thread))) {
						xl_last_flush_time = (u_int) (thread->st_statistics.st_xlog.ts_flush_time - flush_time);
						xl_log_bytes_flushed = xl_log_bytes_written;

						xt_lock_mutex_ns(&xl_db->db_wr_lock);
						xl_flush_log_id = xl_write_log_id;
						xl_flush_log_offset = xl_write_log_offset + (xl_write_done ? xl_write_buf_pos : 0);
						/*
						 * We have written data to the log, wake the writer to commit
						* the data to the database.
						*/
						xlog_wr_log_written(xl_db);
						xt_unlock_mutex_ns(&xl_db->db_wr_lock);
					}
				}
#ifdef XT_XLOG_WAIT_SPINS
				xt_writing = 0;
				if (xt_waiting)
					xt_cond_wakeall(&xl_write_cond);
#else
				xt_writing = FALSE;
				xt_cond_wakeall(&xl_write_cond);
#endif
				ASSERT_NS(xt_comp_log_pos(xl_write_log_id, xl_write_log_offset, xl_append_log_id, xl_append_log_offset) <= 0);
				return ok;
			}
		}
		else {
			/* If there is space in the buffer, then we can go on
			 * to copy our data into the buffer:
			 */
			if (xl_append_buf_pos + size1 + size2 <= xl_size_of_buffers) {
#ifdef XT_XLOG_WAIT_SPINS
				xt_writing = 0;
				if (xt_waiting)
					xt_cond_wakeall(&xl_write_cond);
#else
				xt_writing = FALSE;
				xt_cond_wakeall(&xl_write_cond);
#endif
				goto copy_to_log_buffer;
			}
		}

		rewrite:
		/* If the current write buffer has been written, then
		 * switch the logs. Otherwise we must try to existing
		 * write buffer.
		 */
		if (xl_write_done) {
			/* This means that the current write buffer has been writen,
			 * i.e. it is empty!
			 */
			xt_spinlock_lock(&xl_buffer_lock);
			xtWord1	*tmp_buffer = xl_write_buffer;

			/* The write position is now the append position: */
			xl_write_log_id = xl_append_log_id;
			xl_write_log_offset = xl_append_log_offset;
			xl_write_buf_pos = xl_append_buf_pos;
			xl_write_buf_pos_start = xl_append_buf_pos_start;
			xl_write_buffer = xl_append_buffer;
			xl_write_done = FALSE;

			/* We have to maintain 512 byte alignment: */
			ASSERT_NS((xl_write_log_offset % 512) == 0);
			part_size = xl_write_buf_pos % 512;
			if (part_size != 0)
				memcpy(tmp_buffer, xl_write_buffer + xl_write_buf_pos - part_size, part_size);

			/* The new append position will be after the
			 * current append position:
			 */
			xl_append_log_offset += xl_append_buf_pos - part_size;
			xl_append_buf_pos = part_size;
			xl_append_buf_pos_start = part_size;
			xl_append_buffer = tmp_buffer; // The old write buffer (which is empty)

			/*
			 * If the append offset exceeds the log threshhold, then
			 * we set the append buffer to a new log file:
			 *
			 * NOTE: This algorithm will cause the log to be overwriten by a maximum
			 * of the log buffer size!
			 */
			if (xl_append_log_offset >= xl_log_file_threshold) {
				XTXactNewLogEntryDPtr	log_tail;
				XTXactLogHeaderDPtr		log_head;

				xl_append_log_id++;

				/* Write the final record to the old log.
				 * There is enough space for this because we allocate the
				 * buffer a little bigger than required.
				 */
				log_tail = (XTXactNewLogEntryDPtr) (xl_write_buffer + xl_write_buf_pos);
				log_tail->xl_status_1 = XT_LOG_ENT_NEW_LOG;
				log_tail->xl_checksum_1 = XT_CHECKSUM_1(xl_append_log_id) ^ XT_CHECKSUM_1(xl_write_log_id);
				XT_SET_DISK_4(log_tail->xl_log_id_4, xl_append_log_id);
				xl_write_buf_pos += sizeof(XTXactNewLogEntryDRec);

				/* We add the header to the next log. */
				log_head = (XTXactLogHeaderDPtr) xl_append_buffer;
				memset(log_head, 0, sizeof(XTXactLogHeaderDRec));
				log_head->xh_status_1 = XT_LOG_ENT_HEADER;
				log_head->xh_checksum_1 = XT_CHECKSUM_1(xl_append_log_id);
				XT_SET_DISK_4(log_head->xh_size_4, sizeof(XTXactLogHeaderDRec));
				XT_SET_DISK_4(log_head->xh_log_id_4, xl_append_log_id);
				XT_SET_DISK_2(log_head->xh_version_2, XT_LOG_VERSION_NO);
				XT_SET_DISK_4(log_head->xh_magic_4, XT_LOG_FILE_MAGIC);

				xl_append_log_offset = 0;
				xl_append_buf_pos = sizeof(XTXactLogHeaderDRec);
				xl_append_buf_pos_start = 0;
			}
			xt_spinlock_unlock(&xl_buffer_lock);
			/* We have completed the switch. The append buffer is empty, and
			 * other threads can begin to write to it.
			 *
			 * Meanwhile, this thread will write the write buffer...
			 */
		}

		/* Make sure we have the correct log open: */
		if (xl_log_id != xl_write_log_id) {
			if (!xlog_open_log(xl_write_log_id, xl_write_log_offset, thread))
				goto write_failed;
		}

		/* Write the buffer. */
		/* Always write an integral number of 512 byte blocks: */
		ASSERT_NS((xl_write_log_offset % 512) == 0);
		if ((part_size = xl_write_buf_pos % 512)) {
			part_size = 512 - part_size;
			xl_write_buffer[xl_write_buf_pos] = XT_LOG_ENT_END_OF_LOG;
			if (!xt_pwrite_file(xl_log_file, xl_write_log_offset, xl_write_buf_pos+part_size, xl_write_buffer, &thread->st_statistics.st_xlog, thread))
				goto write_failed;			
		}
		else {
			if (!xt_pwrite_file(xl_log_file, xl_write_log_offset, xl_write_buf_pos, xl_write_buffer, &thread->st_statistics.st_xlog, thread))
				goto write_failed;
		}

		/* This part has not been written: */
		part_size = xl_write_buf_pos - xl_write_buf_pos_start;

		/* We have written the data to the log, transfer
		 * the buffer data into the cache. */
		if (!xlog_transfer_to_cache(xl_log_file, xl_log_id, xl_write_log_offset+xl_write_buf_pos_start, part_size, xl_write_buffer+xl_write_buf_pos_start, thread))
			goto write_failed;

		xl_write_done = TRUE;
		xl_log_bytes_written += part_size;

		if (write_reason == WR_FLUSH) {
			if (xl_db->db_co_busy) {
				/* [(8)] Flush the compactor log. */
				xt_lock_mutex_ns(&xl_db->db_co_dlog_lock);
				if (!xl_db->db_co_thread->st_dlog_buf.dlb_flush_log(TRUE, thread)) {
					xl_log_bytes_written -= part_size;
					xt_unlock_mutex_ns(&xl_db->db_co_dlog_lock);
					goto write_failed;
				}
				xt_unlock_mutex_ns(&xl_db->db_co_dlog_lock);
			}

			/* And flush if required: */
			flush_time = thread->st_statistics.st_xlog.ts_flush_time;
			if (!xt_flush_file(xl_log_file, &thread->st_statistics.st_xlog, thread)) {
				xl_log_bytes_written -= part_size;
				goto write_failed;
			}
			xl_last_flush_time = (u_int) (thread->st_statistics.st_xlog.ts_flush_time - flush_time);

			xl_log_bytes_flushed = xl_log_bytes_written;

			xt_lock_mutex_ns(&xl_db->db_wr_lock);
			xl_flush_log_id = xl_write_log_id;
			xl_flush_log_offset = xl_write_log_offset + xl_write_buf_pos;
			/*
			 * We have written data to the log, wake the writer to commit
			 * the data to the database.
			 */
			xlog_wr_log_written(xl_db);
			xt_unlock_mutex_ns(&xl_db->db_wr_lock);

			/* Check that the require flush condition has arrived. */
			if (xt_comp_log_pos(req_flush_log_id, req_flush_log_offset, xl_flush_log_id, xl_flush_log_offset) > 0)
				/* The required position is still after the current flush
				 * position, continue writing: */
				goto rewrite;

#ifdef XT_XLOG_WAIT_SPINS
			xt_writing = 0;
			if (xt_waiting)
				xt_cond_wakeall(&xl_write_cond);
#else
			xt_writing = FALSE;
			xt_cond_wakeall(&xl_write_cond);
#endif
			ASSERT_NS(xt_comp_log_pos(xl_write_log_id, xl_write_log_offset, xl_append_log_id, xl_append_log_offset) <= 0);
			return OK;
		}
		else
			xlog_wr_log_written(xl_db);

		/*
		 * Check that the buffer is now available, otherwise,
		 * switch and write again!
		 */
		if (xl_append_buf_pos + size1 + size2 > xl_size_of_buffers)
			goto rewrite;

#ifdef XT_XLOG_WAIT_SPINS
		xt_writing = 0;
		if (xt_waiting)
			xt_cond_wakeall(&xl_write_cond);
#else
		xt_writing = FALSE;
		xt_cond_wakeall(&xl_write_cond);
#endif
	}

	copy_to_log_buffer:
	xt_spinlock_lock(&xl_buffer_lock);
	/* Now we have to check again. The check above was a dirty read!
	 */
	if (xl_append_buf_pos + size1 + size2 > xl_size_of_buffers) {
		xt_spinlock_unlock(&xl_buffer_lock);
		/* Not enough space, write the buffer, and return here. */
		write_reason = WR_NO_SPACE;
		goto write_log_to_file;
	}

	memcpy(xl_append_buffer + xl_append_buf_pos, data1, size1);
	if (size2)
		memcpy(xl_append_buffer + xl_append_buf_pos + size1, data2, size2);
	/* Add the log ID to the checksum!
	 * This is required because log files are re-used, and we don't
	 * want the records to be valid when the log is re-used.
	 */
	register XTXactLogBufferDPtr record;

	/*
	 * Adjust db_xn_writer_count here. It is protected by
	 * xl_buffer_lock.
	 */
	record = (XTXactLogBufferDPtr) (xl_append_buffer + xl_append_buf_pos);
	switch (record->xh.xh_status_1) {
		case XT_LOG_ENT_HEADER:
		case XT_LOG_ENT_END_OF_LOG:
			break;
		case XT_LOG_ENT_REC_MODIFIED:
		case XT_LOG_ENT_UPDATE:
		case XT_LOG_ENT_UPDATE_BG:
		case XT_LOG_ENT_UPDATE_FL:
		case XT_LOG_ENT_UPDATE_FL_BG:
		case XT_LOG_ENT_INSERT:
		case XT_LOG_ENT_INSERT_BG:
		case XT_LOG_ENT_INSERT_FL:
		case XT_LOG_ENT_INSERT_FL_BG:
		case XT_LOG_ENT_DELETE:
		case XT_LOG_ENT_DELETE_BG:
		case XT_LOG_ENT_DELETE_FL:
		case XT_LOG_ENT_DELETE_FL_BG:
			sum = XT_GET_DISK_2(record->xu.xu_checksum_2) ^ XT_CHECKSUM_2(xl_append_log_id);
			XT_SET_DISK_2(record->xu.xu_checksum_2, sum);

			if (!thread->st_xact_writer) {
				thread->st_xact_writer = TRUE;
				thread->st_xact_write_time = xt_db_approximate_time;
				xl_db->db_xn_writer_count++;
				xl_db->db_xn_total_writer_count++;
			}
			break;
		case XT_LOG_ENT_REC_REMOVED_BI:
			sum = XT_GET_DISK_2(record->xu.xu_checksum_2) ^ XT_CHECKSUM_2(xl_append_log_id);
			XT_SET_DISK_2(record->xu.xu_checksum_2, sum);
			break;
		case XT_LOG_ENT_ROW_NEW:
		case XT_LOG_ENT_ROW_NEW_FL:
			record->xl.xl_checksum_1 ^= XT_CHECKSUM_1(xl_append_log_id);

			if (!thread->st_xact_writer) {
				thread->st_xact_writer = TRUE;
				thread->st_xact_write_time = xt_db_approximate_time;
				xl_db->db_xn_writer_count++;
				xl_db->db_xn_total_writer_count++;
			}
			break;
		case XT_LOG_ENT_COMMIT:
		case XT_LOG_ENT_ABORT:
			ASSERT_NS(thread->st_xact_writer);
			ASSERT_NS(xl_db->db_xn_writer_count > 0);
			if (thread->st_xact_writer) {
				xl_db->db_xn_writer_count--;
				thread->st_xact_writer = FALSE;
				if (thread->st_xact_long_running) {
					xl_db->db_xn_long_running_count--;
					thread->st_xact_long_running = FALSE;
				}
			}
			/* No break required! */
		default:
			record->xl.xl_checksum_1 ^= XT_CHECKSUM_1(xl_append_log_id);
			break;
	}
#ifdef DEBUG
	ASSERT_NS(xlog_verify(record, size1 + size2, xl_append_log_id));
#endif
	if (log_id)
		*log_id = xl_append_log_id;
	if (log_offset)
		*log_offset = xl_append_log_offset + xl_append_buf_pos;
	xl_append_buf_pos += size1 + size2;
	if (commit) {
		write_reason = WR_FLUSH;
		req_flush_log_id = xl_append_log_id;
		req_flush_log_offset = xl_append_log_offset + xl_append_buf_pos;
		xt_spinlock_unlock(&xl_buffer_lock);
		goto write_log_to_file;
	}

	// Failed sometime when outside the spinlock!
	ASSERT_NS(xt_comp_log_pos(xl_write_log_id, xl_write_log_offset, xl_append_log_id, xl_append_log_offset + xl_append_buf_pos) <= 0); 
	xt_spinlock_unlock(&xl_buffer_lock);

	return OK;

	write_failed:
#ifdef XT_XLOG_WAIT_SPINS
	xt_writing = 0;
	if (xt_waiting)
		xt_cond_wakeall(&xl_write_cond);
#else
	xt_writing = FALSE;
	xt_cond_wakeall(&xl_write_cond);
#endif
	return FAILED;
}

/*
 * This function does not always delete the log. It may just rename a
 * log to a new log which it will need.
 * This speeds things up:
 *
 * - No need to pre-allocate the new log.
 * - Log data is already flushed (i.e. disk blocks allocated)
 * - Log is already in OS cache.
 *
 * However, it means that I need to checksum things differently
 * on each log to make sure I do not treat an old record
 * as valid!
 *
 * Return OK, FAILED or XT_ERR
 */ 
int XTDatabaseLog::xlog_delete_log(xtLogID del_log_id, XTThreadPtr thread)
{
	char	path[PATH_MAX];

	if (xl_max_log_id < xl_write_log_id)
		xl_max_log_id = xl_write_log_id;

	xlog_name(PATH_MAX, path, del_log_id);

	if (xt_db_offline_log_function == XT_RECYCLE_LOGS) {
		char	new_path[PATH_MAX];
		xtLogID	new_log_id;
		xtBool	ok;

		/* Make sure that the total logs is less than or equal to the log file count
		 * (plus dynamic component).
		 */
		while (xl_max_log_id - del_log_id + 1 <= (xl_log_file_count + xt_log_file_dyn_count) &&
			/* And the number of logs after the current log (including the current log)
			 * must be less or equal to the log file count. */
			xl_max_log_id - xl_write_log_id + 1 <= xl_log_file_count) {
			new_log_id = xl_max_log_id+1;
			xlog_name(PATH_MAX, new_path, new_log_id);
			ok = xt_fs_rename(NULL, path, new_path);
			if (ok) {
				xl_max_log_id = new_log_id;
				goto done;
			}
			if (!xt_fs_exists(new_path)) {
				/* Try again later: */
				if (thread->t_exception.e_xt_err == XT_SYSTEM_ERROR &&
					XT_FILE_IN_USE(thread->t_exception.e_sys_err))
					return FAILED;

				return XT_ERR;
			}
			xl_max_log_id = new_log_id;
		}
	}

	if (xt_db_offline_log_function != XT_KEEP_LOGS) {
		if (!xt_fs_delete(NULL, path)) {
			if (thread->t_exception.e_xt_err == XT_SYSTEM_ERROR &&
				XT_FILE_IN_USE(thread->t_exception.e_sys_err))
				return FAILED;

			return XT_ERR;
		}
	}

	done:
	return OK;
}

/* PRIVATE FUNCTIONS */
xtBool XTDatabaseLog::xlog_open_log(xtLogID log_id, off_t curr_write_pos, XTThreadPtr thread)
{
	char	log_path[PATH_MAX];
	off_t	eof;

	if (xl_log_id == log_id)
		return OK;

	if (xl_log_file) {
		if (!xt_flush_file(xl_log_file, &thread->st_statistics.st_xlog, thread))
			return FAILED;
		xt_close_file_ns(xl_log_file);
		xl_log_file = NULL;
		xl_log_id = 0;
	}

	xlog_name(PATH_MAX, log_path, log_id);
	if (!(xl_log_file = xt_open_file_ns(log_path, XT_FS_CREATE | XT_FS_MAKE_PATH)))
		return FAILED;
	/* Allocate space until the required size: */
	if (curr_write_pos <  xl_log_file_threshold) {
		eof = xt_seek_eof_file(NULL, xl_log_file);
		if (eof == 0) {
			/* A new file (bad), we need a greater file count: */
			xt_log_file_dyn_count++;
			xt_log_file_dyn_dec = 4;
		}
		else {
			/* An existing file (good): */
			if (xt_log_file_dyn_count > 0) {
				if (xt_log_file_dyn_dec > 0)
					xt_log_file_dyn_dec--;
				else
					xt_log_file_dyn_count--;
			}
		}
		if (eof < xl_log_file_threshold) {
			char	buffer[2048];
			size_t	tfer;

			memset(buffer, 0, 2048);

			curr_write_pos = xt_align_offset(curr_write_pos, 512);
#ifdef PREWRITE_LOG_COMPLETELY
			while (curr_write_pos < xl_log_file_threshold) {
				tfer = 2048;
				if ((off_t) tfer > xl_log_file_threshold - curr_write_pos)
					tfer = (size_t) (xl_log_file_threshold - curr_write_pos);
				if (curr_write_pos == 0)
					*buffer = XT_LOG_ENT_END_OF_LOG;
				if (!xt_pwrite_file(xl_log_file, curr_write_pos, tfer, buffer, &thread->st_statistics.st_xlog, thread))
					return FAILED;
				*buffer = 0;
				curr_write_pos += tfer;
			}
#else
			if (curr_write_pos < xl_log_file_threshold) {
				tfer = 2048;
				
				if (curr_write_pos < xl_log_file_threshold - 2048)
					curr_write_pos = xl_log_file_threshold - 2048;
				if ((off_t) tfer > xl_log_file_threshold - curr_write_pos)
					tfer = (size_t) (xl_log_file_threshold - curr_write_pos);
				if (!xt_pwrite_file(xl_log_file, curr_write_pos, tfer, buffer, &thread->st_statistics.st_xlog, thread))
					return FAILED;
			}
#endif
		}
		else if (eof > xl_log_file_threshold + (128 * 1024 * 1024)) {
			if (!xt_set_eof_file(NULL, xl_log_file, xl_log_file_threshold))
				return FAILED;
		}
	}
	xl_log_id = log_id;
	return OK;
}

void XTDatabaseLog::xlog_name(size_t size, char *path, xtLogID log_id)
{
	char name[50];

	sprintf(name, "xlog-%lu.xt", (u_long) log_id);
	xt_strcpy(size, path, xl_db->db_main_path);
	xt_add_system_dir(size, path);
	xt_add_dir_char(size, path);
	xt_strcat(size, path, name);
}

/*
 * -----------------------------------------------------------------------
 * T H R E A D   T R A N S A C T I O N   B U F F E R
 */

xtPublic xtBool xt_xlog_flush_log(XTThreadPtr thread)
{
	return thread->st_database->db_xlog.xlog_flush(thread);
}

xtPublic xtBool xt_xlog_log_data(XTThreadPtr thread, size_t size, XTXactLogBufferDPtr log_entry, xtBool commit)
{
	return thread->st_database->db_xlog.xlog_append(thread, size, (xtWord1 *) log_entry, 0, NULL, commit, NULL, NULL);
}

/* Allocate a record from the free list. */
xtPublic xtBool xt_xlog_modify_table(struct XTOpenTable *ot, u_int status, xtOpSeqNo op_seq, xtRecordID free_rec_id, xtRecordID rec_id, size_t size, xtWord1 *data)
{
	XTXactLogBufferDRec	log_entry;
	XTThreadPtr			thread = ot->ot_thread;
	XTTableHPtr			tab = ot->ot_table;
	size_t				len;
	xtWord4				sum = 0;
	int					check_size = 1;
	XTXactDataPtr		xact = NULL;

	switch (status) {
		case XT_LOG_ENT_REC_MODIFIED:
		case XT_LOG_ENT_UPDATE:
		case XT_LOG_ENT_INSERT:
		case XT_LOG_ENT_DELETE:
			check_size = 2;
			XT_SET_DISK_4(log_entry.xu.xu_op_seq_4, op_seq);
			XT_SET_DISK_4(log_entry.xu.xu_tab_id_4, tab->tab_id);
			XT_SET_DISK_4(log_entry.xu.xu_rec_id_4, rec_id);
			XT_SET_DISK_2(log_entry.xu.xu_size_2, size);
			len = offsetof(XTactUpdateEntryDRec, xu_rec_type_1);
			if (!(thread->st_xact_data->xd_flags & XT_XN_XAC_LOGGED)) {
				/* Add _BG: */
				status++;
				xact = thread->st_xact_data;
				xact->xd_flags |= XT_XN_XAC_LOGGED;
			}
			break;
		case XT_LOG_ENT_UPDATE_FL:
		case XT_LOG_ENT_INSERT_FL:
		case XT_LOG_ENT_DELETE_FL:
			check_size = 2;
			XT_SET_DISK_4(log_entry.xf.xf_op_seq_4, op_seq);
			XT_SET_DISK_4(log_entry.xf.xf_tab_id_4, tab->tab_id);
			XT_SET_DISK_4(log_entry.xf.xf_rec_id_4, rec_id);
			XT_SET_DISK_2(log_entry.xf.xf_size_2, size);
			XT_SET_DISK_4(log_entry.xf.xf_free_rec_id_4, free_rec_id);
			sum ^= XT_CHECKSUM4_REC(free_rec_id);
			len = offsetof(XTactUpdateFLEntryDRec, xf_rec_type_1);
			if (!(thread->st_xact_data->xd_flags & XT_XN_XAC_LOGGED)) {
				/* Add _BG: */
				status++;
				xact = thread->st_xact_data;
				xact->xd_flags |= XT_XN_XAC_LOGGED;
			}
			break;
		case XT_LOG_ENT_REC_FREED:
		case XT_LOG_ENT_REC_REMOVED:
		case XT_LOG_ENT_REC_REMOVED_EXT:
			ASSERT_NS(size == 1 + XT_XACT_ID_SIZE + sizeof(XTTabRecFreeDRec));
			XT_SET_DISK_4(log_entry.fr.fr_op_seq_4, op_seq);
			XT_SET_DISK_4(log_entry.fr.fr_tab_id_4, tab->tab_id);
			XT_SET_DISK_4(log_entry.fr.fr_rec_id_4, rec_id);
			len = offsetof(XTactFreeRecEntryDRec, fr_stat_id_1);
			break;
		case XT_LOG_ENT_REC_REMOVED_BI:
			check_size = 2;
			XT_SET_DISK_4(log_entry.rb.rb_op_seq_4, op_seq);
			XT_SET_DISK_4(log_entry.rb.rb_tab_id_4, tab->tab_id);
			XT_SET_DISK_4(log_entry.rb.rb_rec_id_4, rec_id);
			XT_SET_DISK_2(log_entry.rb.rb_size_2, size);
			log_entry.rb.rb_new_rec_type_1 = (xtWord1) free_rec_id;
			sum ^= XT_CHECKSUM4_REC(free_rec_id);
			len = offsetof(XTactRemoveBIEntryDRec, rb_rec_type_1);
			break;
		case XT_LOG_ENT_REC_MOVED:
			ASSERT_NS(size == 8);
			XT_SET_DISK_4(log_entry.xw.xw_op_seq_4, op_seq);
			XT_SET_DISK_4(log_entry.xw.xw_tab_id_4, tab->tab_id);
			XT_SET_DISK_4(log_entry.xw.xw_rec_id_4, rec_id);
			len = offsetof(XTactWriteRecEntryDRec, xw_rec_type_1);
			break;
		case XT_LOG_ENT_REC_CLEANED:
			ASSERT_NS(size == offsetof(XTTabRecHeadDRec, tr_prev_rec_id_4) + XT_RECORD_ID_SIZE);
			XT_SET_DISK_4(log_entry.xw.xw_op_seq_4, op_seq);
			XT_SET_DISK_4(log_entry.xw.xw_tab_id_4, tab->tab_id);
			XT_SET_DISK_4(log_entry.xw.xw_rec_id_4, rec_id);
			len = offsetof(XTactWriteRecEntryDRec, xw_rec_type_1);
			break;
		case XT_LOG_ENT_REC_CLEANED_1:
			ASSERT_NS(size == 1);
			XT_SET_DISK_4(log_entry.xw.xw_op_seq_4, op_seq);
			XT_SET_DISK_4(log_entry.xw.xw_tab_id_4, tab->tab_id);
			XT_SET_DISK_4(log_entry.xw.xw_rec_id_4, rec_id);
			len = offsetof(XTactWriteRecEntryDRec, xw_rec_type_1);
			break;
		case XT_LOG_ENT_REC_UNLINKED:
			ASSERT_NS(size == offsetof(XTTabRecHeadDRec, tr_prev_rec_id_4) + XT_RECORD_ID_SIZE);
			XT_SET_DISK_4(log_entry.xw.xw_op_seq_4, op_seq);
			XT_SET_DISK_4(log_entry.xw.xw_tab_id_4, tab->tab_id);
			XT_SET_DISK_4(log_entry.xw.xw_rec_id_4, rec_id);
			len = offsetof(XTactWriteRecEntryDRec, xw_rec_type_1);
			break;
		case XT_LOG_ENT_ROW_NEW:
			ASSERT_NS(size == 0);
			XT_SET_DISK_4(log_entry.xa.xa_op_seq_4, op_seq);
			XT_SET_DISK_4(log_entry.xa.xa_tab_id_4, tab->tab_id);
			XT_SET_DISK_4(log_entry.xa.xa_row_id_4, rec_id);
			len = offsetof(XTactRowAddedEntryDRec, xa_row_id_4) + XT_ROW_ID_SIZE;
			break;
		case XT_LOG_ENT_ROW_NEW_FL:
			ASSERT_NS(size == 0);
			XT_SET_DISK_4(log_entry.xa.xa_op_seq_4, op_seq);
			XT_SET_DISK_4(log_entry.xa.xa_tab_id_4, tab->tab_id);
			XT_SET_DISK_4(log_entry.xa.xa_row_id_4, rec_id);
			XT_SET_DISK_4(log_entry.xa.xa_free_list_4, free_rec_id);
			sum ^= XT_CHECKSUM4_REC(free_rec_id);
			len = offsetof(XTactRowAddedEntryDRec, xa_free_list_4) + XT_ROW_ID_SIZE;
			break;
		case XT_LOG_ENT_ROW_ADD_REC:
		case XT_LOG_ENT_ROW_SET:
		case XT_LOG_ENT_ROW_FREED:
			ASSERT_NS(size == sizeof(XTTabRowRefDRec));
			XT_SET_DISK_4(log_entry.wr.wr_op_seq_4, op_seq);
			XT_SET_DISK_4(log_entry.wr.wr_tab_id_4, tab->tab_id);
			XT_SET_DISK_4(log_entry.wr.wr_row_id_4, rec_id);
			len = offsetof(XTactWriteRowEntryDRec, wr_ref_id_4);
			break;
		default:
			ASSERT_NS(FALSE);
			len = 0;
			break;
	}

	xtWord1	*dptr = data;
	xtWord4	g;

	sum ^= op_seq ^ (tab->tab_id << 8) ^ XT_CHECKSUM4_REC(rec_id);
	if ((g = sum & 0xF0000000)) {
		sum = sum ^ (g >> 24);
		sum = sum ^ g;
	}
	for (u_int i=0; i<(u_int) size; i++) {
		sum = (sum << 4) + *dptr;
		if ((g = sum & 0xF0000000)) {
			sum = sum ^ (g >> 24);
			sum = sum ^ g;
		}
		dptr++;
	}

	log_entry.xh.xh_status_1 = status;
	if (check_size == 1) {
		log_entry.xh.xh_checksum_1 = XT_CHECKSUM_1(sum);
	}
	else {
		xtWord2 c;
		
		c = XT_CHECKSUM_2(sum);
		XT_SET_DISK_2(log_entry.xu.xu_checksum_2, c);
	}
#ifdef PRINT_TABLE_MODIFICATIONS
	xt_print_log_record(0, 0, &log_entry);
#endif
	if (xact)
		return thread->st_database->db_xlog.xlog_append(thread, len, (xtWord1 *) &log_entry, size, data, FALSE, &xact->xd_begin_log, &xact->xd_begin_offset);

	return thread->st_database->db_xlog.xlog_append(thread, len, (xtWord1 *) &log_entry, size, data, FALSE, NULL, NULL);
}

/*
 * -----------------------------------------------------------------------
 * S E Q U E N T I A L   L O G   R E A  D I N G
 */

/*
 * Use the log buffer for sequential reading the log.
 */
xtBool XTDatabaseLog::xlog_seq_init(XTXactSeqReadPtr seq, size_t buffer_size, xtBool load_cache)
{
	seq->xseq_buffer_size = buffer_size;
	seq->xseq_load_cache = load_cache;

	seq->xseq_log_id = 0;
	seq->xseq_log_file = NULL;
	seq->xseq_log_eof = 0;

	seq->xseq_buf_log_offset = 0;
	seq->xseq_buffer_len = 0;
	seq->xseq_buffer = (xtWord1 *) xt_malloc_ns(buffer_size);

	seq->xseq_rec_log_id = 0;
	seq->xseq_rec_log_offset = 0;
	seq->xseq_record_len = 0;

	return seq->xseq_buffer != NULL;
}

void XTDatabaseLog::xlog_seq_exit(XTXactSeqReadPtr seq)
{
	xlog_seq_close(seq);
	if (seq->xseq_buffer) {
		xt_free_ns(seq->xseq_buffer);
		seq->xseq_buffer = NULL;
	}
}

void XTDatabaseLog::xlog_seq_close(XTXactSeqReadPtr seq)
{
	if (seq->xseq_log_file) {
		xt_close_file_ns(seq->xseq_log_file);
		seq->xseq_log_file = NULL;
	}
	seq->xseq_log_id = 0;
	seq->xseq_log_eof = 0;
}

xtBool XTDatabaseLog::xlog_seq_start(XTXactSeqReadPtr seq, xtLogID log_id, xtLogOffset log_offset, xtBool XT_UNUSED(missing_ok))
{
	if (seq->xseq_rec_log_id != log_id) {
		seq->xseq_rec_log_id = log_id;
		seq->xseq_buf_log_offset = seq->xseq_rec_log_offset;
		seq->xseq_buffer_len = 0;
	}

	/* Windows version: this will help to switch
	 * to the new log file.
	 * Due to reading from the log buffers, this was
	 * not always done!
	 */
	if (seq->xseq_log_id != log_id) {
		if (seq->xseq_log_file) {
			xt_close_file_ns(seq->xseq_log_file);
			seq->xseq_log_file = NULL;
		}
	}
	seq->xseq_rec_log_offset = log_offset;
	seq->xseq_record_len = 0;
	return OK;
}

size_t XTDatabaseLog::xlog_bytes_to_write()
{
	xtLogID					log_id;
	xtLogOffset				log_offset;
	xtLogID					to_log_id;
	xtLogOffset				to_log_offset;
	size_t					byte_count = 0;

	log_id = xl_db->db_wr_log_id;
	log_offset = xl_db->db_wr_log_offset;
	to_log_id = xl_db->db_xlog.xl_flush_log_id;
	to_log_offset = xl_db->db_xlog.xl_flush_log_offset;

	/* Assume the logs have the threshold: */
	if (log_id < to_log_id) {
		if (log_offset < xt_db_log_file_threshold)
			byte_count = (size_t) (xt_db_log_file_threshold - log_offset);
		log_offset = 0;
		log_id++;
	}
	while (log_id < to_log_id) {
		byte_count += (size_t) xt_db_log_file_threshold;
		log_id++;
	}
	if (log_offset < to_log_offset)
		byte_count += (size_t) (to_log_offset - log_offset);

	return byte_count;
}

xtBool XTDatabaseLog::xlog_read_from_cache(XTXactSeqReadPtr seq, xtLogID log_id, xtLogOffset log_offset, size_t size, off_t eof, xtWord1 *buffer, size_t *data_read, XTThreadPtr thread)
{
	/* xseq_log_file could be NULL because xseq_log_id is not set
	 * to zero when xseq_log_file is set to NULL!
	 * This bug caused a crash in TeamDrive.
	 */
	if (seq->xseq_log_id != log_id || !seq->xseq_log_file) {
		char path[PATH_MAX];

		if (seq->xseq_log_file) {
			xt_close_file_ns(seq->xseq_log_file);
			seq->xseq_log_file = NULL;
		}

		xlog_name(PATH_MAX, path, log_id);
		if (!xt_open_file_ns(&seq->xseq_log_file, path, XT_FS_MISSING_OK))
			return FAILED;
		if (!seq->xseq_log_file) {
			if (data_read)
				*data_read = 0;
			return OK;
		}
		seq->xseq_log_id = log_id;
		seq->xseq_log_eof = 0;
	}

	if (!eof) {
		if (!seq->xseq_log_eof)
			seq->xseq_log_eof = xt_seek_eof_file(NULL, seq->xseq_log_file);
		eof = seq->xseq_log_eof;
	}

	if (log_offset >= eof) {
		if (data_read)
			*data_read = 0;
		return OK;
	}

	if ((off_t) size > eof - log_offset)
		size = (size_t) (eof - log_offset);

	if (data_read)
		*data_read = size;
	return xt_xlog_read(seq->xseq_log_file, seq->xseq_log_id, log_offset, size, buffer, seq->xseq_load_cache, thread);
}

xtBool XTDatabaseLog::xlog_rnd_read(XTXactSeqReadPtr seq, xtLogID log_id, xtLogOffset log_offset, size_t size, xtWord1 *buffer, size_t *data_read, XTThreadPtr thread)
{
	/* Fast track to reading from cache: */
	if (log_id < xl_write_log_id)
		return xlog_read_from_cache(seq, log_id, log_offset, size, 0, buffer, data_read, thread);
	
	if (log_id == xl_write_log_id && log_offset + (xtLogOffset) size <= xl_write_log_offset)
		return xlog_read_from_cache(seq, log_id, log_offset, size, xl_write_log_offset, buffer, data_read, thread);

	/* May be in the log write or append buffer: */
	xt_lck_slock(&xl_buffer_lock);

	if (log_id < xl_write_log_id) {
		xt_spinlock_unlock(&xl_buffer_lock);
		return xlog_read_from_cache(seq, log_id, log_offset, size, 0, buffer, data_read, thread);
	}

	/* Check the write buffer: */
	if (log_id == xl_write_log_id) {
		if (log_offset + (xtLogOffset) size <= xl_write_log_offset) {
			xt_spinlock_unlock(&xl_buffer_lock);
			return xlog_read_from_cache(seq, log_id, log_offset, size, xl_write_log_offset, buffer, data_read, thread);
		}

		if (log_offset < xl_write_log_offset + (xtLogOffset) xl_write_buf_pos) {
			/* Reading partially from the write buffer: */
			if (log_offset >= xl_write_log_offset) {
				/* Completely in the buffer. */
				off_t offset = log_offset - xl_write_log_offset;
				
				if (size > xl_write_buf_pos - offset)
					size = (size_t) (xl_write_buf_pos - offset);
				
				memcpy(buffer, xl_write_buffer + offset, size);
				if (data_read)
					*data_read = size;
				goto unlock_and_return;
			}

			/* End part in the buffer: */
			size_t tfer;
			
			/* The amount that will be taken from the cache: */
			tfer = (size_t) (xl_write_log_offset - log_offset);
			
			size -= tfer;
			if (size > xl_write_buf_pos)
				size = xl_write_buf_pos;
			
			memcpy(buffer + tfer, xl_write_buffer, size);

			xt_spinlock_unlock(&xl_buffer_lock);
			
			/* Read the first part from the cache: */
			if (data_read)
				*data_read = tfer + size;			
			return xlog_read_from_cache(seq, log_id, log_offset, tfer, log_offset + tfer, buffer, NULL, thread);
		}
	}

	/* Check the append buffer: */
	if (log_id == xl_append_log_id) {
		if (log_offset >= xl_append_log_offset && log_offset < xl_append_log_offset + (xtLogOffset) xl_append_buf_pos) {
			/* It is in the append buffer: */
			size_t offset = (size_t) (log_offset - xl_append_log_offset);
			
			if (size > xl_append_buf_pos - offset)
				size = xl_append_buf_pos - offset;
			
			memcpy(buffer, xl_append_buffer + offset, size);
			if (data_read)
				*data_read = size;
			goto unlock_and_return;
		}
	}

	if (xl_append_log_id == 0) {
		/* This catches the case that
		 * the log has not yet been initialized
		 * for writing.
		 */
		xt_spinlock_unlock(&xl_buffer_lock);
		return xlog_read_from_cache(seq, log_id, log_offset, size, 0, buffer, data_read, thread);
	}

	if (data_read)
		*data_read = 0;

	unlock_and_return:
	xt_spinlock_unlock(&xl_buffer_lock);
	return OK;
}

xtBool XTDatabaseLog::xlog_write_thru(XTXactSeqReadPtr seq, size_t size, xtWord1 *data, XTThreadPtr thread)
{
	if (!xt_xlog_write(seq->xseq_log_file, seq->xseq_log_id, seq->xseq_rec_log_offset, size, data, thread))
		return FALSE;
	xl_log_bytes_written += size;
	seq->xseq_rec_log_offset += size;
	return TRUE;
}

xtBool XTDatabaseLog::xlog_verify(XTXactLogBufferDPtr record, size_t rec_size, xtLogID log_id)
{
	xtWord4		sum = 0;
	xtOpSeqNo	op_seq;
	xtTableID	tab_id;
	xtRecordID	rec_id, free_rec_id;
	int			check_size = 1;
	xtWord1		*dptr;

	switch (record->xh.xh_status_1) {
		case XT_LOG_ENT_HEADER:
			if (record->xh.xh_checksum_1 != XT_CHECKSUM_1(log_id))
				return FALSE;
			if (XT_LOG_HEAD_MAGIC(record, rec_size) != XT_LOG_FILE_MAGIC)
				return FALSE;
			if (rec_size >= offsetof(XTXactLogHeaderDRec, xh_log_id_4) + 4) {
				if (XT_GET_DISK_4(record->xh.xh_log_id_4) != log_id)
					return FALSE;
			}
			return TRUE;
		case XT_LOG_ENT_NEW_LOG:
		case XT_LOG_ENT_DEL_LOG:
			return record->xl.xl_checksum_1 == (XT_CHECKSUM_1(XT_GET_DISK_4(record->xl.xl_log_id_4)) ^ XT_CHECKSUM_1(log_id));
		case XT_LOG_ENT_NEW_TAB:
			return record->xl.xl_checksum_1 == (XT_CHECKSUM_1(XT_GET_DISK_4(record->xt.xt_tab_id_4)) ^ XT_CHECKSUM_1(log_id));
		case XT_LOG_ENT_COMMIT:
		case XT_LOG_ENT_ABORT:
			sum = XT_CHECKSUM4_XACT(XT_GET_DISK_4(record->xe.xe_xact_id_4)) ^ XT_CHECKSUM4_XACT(XT_GET_DISK_4(record->xe.xe_not_used_4));
			return record->xe.xe_checksum_1 == (XT_CHECKSUM_1(sum) ^ XT_CHECKSUM_1(log_id));
		case XT_LOG_ENT_CLEANUP:
			sum = XT_CHECKSUM4_XACT(XT_GET_DISK_4(record->xc.xc_xact_id_4));
			return record->xc.xc_checksum_1 == (XT_CHECKSUM_1(sum) ^ XT_CHECKSUM_1(log_id));
		case XT_LOG_ENT_REC_MODIFIED:
		case XT_LOG_ENT_UPDATE:
		case XT_LOG_ENT_INSERT:
		case XT_LOG_ENT_DELETE:
		case XT_LOG_ENT_UPDATE_BG:
		case XT_LOG_ENT_INSERT_BG:
		case XT_LOG_ENT_DELETE_BG:
			check_size = 2;
			op_seq = XT_GET_DISK_4(record->xu.xu_op_seq_4);
			tab_id = XT_GET_DISK_4(record->xu.xu_tab_id_4);
			rec_id = XT_GET_DISK_4(record->xu.xu_rec_id_4);
			dptr = &record->xu.xu_rec_type_1;
			rec_size -= offsetof(XTactUpdateEntryDRec, xu_rec_type_1);
			break;
		case XT_LOG_ENT_UPDATE_FL:
		case XT_LOG_ENT_INSERT_FL:
		case XT_LOG_ENT_DELETE_FL:
		case XT_LOG_ENT_UPDATE_FL_BG:
		case XT_LOG_ENT_INSERT_FL_BG:
		case XT_LOG_ENT_DELETE_FL_BG:
			check_size = 2;
			op_seq = XT_GET_DISK_4(record->xf.xf_op_seq_4);
			tab_id = XT_GET_DISK_4(record->xf.xf_tab_id_4);
			rec_id = XT_GET_DISK_4(record->xf.xf_rec_id_4);
			free_rec_id = XT_GET_DISK_4(record->xf.xf_free_rec_id_4);
			sum ^= XT_CHECKSUM4_REC(free_rec_id);
			dptr = &record->xf.xf_rec_type_1;
			rec_size -= offsetof(XTactUpdateFLEntryDRec, xf_rec_type_1);
			break;
		case XT_LOG_ENT_REC_FREED:
		case XT_LOG_ENT_REC_REMOVED:
		case XT_LOG_ENT_REC_REMOVED_EXT:
			op_seq = XT_GET_DISK_4(record->fr.fr_op_seq_4);
			tab_id = XT_GET_DISK_4(record->fr.fr_tab_id_4);
			rec_id = XT_GET_DISK_4(record->fr.fr_rec_id_4);
			dptr = &record->fr.fr_stat_id_1;
			rec_size -= offsetof(XTactFreeRecEntryDRec, fr_stat_id_1);
			break;
		case XT_LOG_ENT_REC_REMOVED_BI:
			check_size = 2;
			op_seq = XT_GET_DISK_4(record->rb.rb_op_seq_4);
			tab_id = XT_GET_DISK_4(record->rb.rb_tab_id_4);
			rec_id = XT_GET_DISK_4(record->rb.rb_rec_id_4);
			free_rec_id = (xtWord4) record->rb.rb_new_rec_type_1;
			sum ^= XT_CHECKSUM4_REC(free_rec_id);
			dptr = &record->rb.rb_rec_type_1;
			rec_size -= offsetof(XTactRemoveBIEntryDRec, rb_rec_type_1);
			break;
		case XT_LOG_ENT_REC_MOVED:
		case XT_LOG_ENT_REC_CLEANED:
		case XT_LOG_ENT_REC_CLEANED_1:
		case XT_LOG_ENT_REC_UNLINKED:
			op_seq = XT_GET_DISK_4(record->xw.xw_op_seq_4);
			tab_id = XT_GET_DISK_4(record->xw.xw_tab_id_4);
			rec_id = XT_GET_DISK_4(record->xw.xw_rec_id_4);
			dptr = &record->xw.xw_rec_type_1;
			rec_size -= offsetof(XTactWriteRecEntryDRec, xw_rec_type_1);
			break;
		case XT_LOG_ENT_ROW_NEW:
		case XT_LOG_ENT_ROW_NEW_FL:
			op_seq = XT_GET_DISK_4(record->xa.xa_op_seq_4);
			tab_id = XT_GET_DISK_4(record->xa.xa_tab_id_4);
			rec_id = XT_GET_DISK_4(record->xa.xa_row_id_4);
			if (record->xh.xh_status_1 == XT_LOG_ENT_ROW_NEW) {
				dptr = (xtWord1 *) record + offsetof(XTactRowAddedEntryDRec, xa_free_list_4);
				rec_size -= offsetof(XTactRowAddedEntryDRec, xa_free_list_4);
			}
			else {
				free_rec_id = XT_GET_DISK_4(record->xa.xa_free_list_4);
				sum ^= XT_CHECKSUM4_REC(free_rec_id);
				dptr = (xtWord1 *) record + sizeof(XTactRowAddedEntryDRec);
				rec_size -= sizeof(XTactRowAddedEntryDRec);
			}
			break;
		case XT_LOG_ENT_ROW_ADD_REC:
		case XT_LOG_ENT_ROW_SET:
		case XT_LOG_ENT_ROW_FREED:
			op_seq = XT_GET_DISK_4(record->wr.wr_op_seq_4);
			tab_id = XT_GET_DISK_4(record->wr.wr_tab_id_4);
			rec_id = XT_GET_DISK_4(record->wr.wr_row_id_4);
			dptr = (xtWord1 *) &record->wr.wr_ref_id_4;
			rec_size -= offsetof(XTactWriteRowEntryDRec, wr_ref_id_4);
			break;
		case XT_LOG_ENT_OP_SYNC:
			return record->xl.xl_checksum_1 == (XT_CHECKSUM_1(XT_GET_DISK_4(record->os.os_time_4)) ^ XT_CHECKSUM_1(log_id));
		case XT_LOG_ENT_NO_OP:
			sum = XT_GET_DISK_4(record->no.no_tab_id_4) ^ XT_GET_DISK_4(record->no.no_op_seq_4);
			return record->xe.xe_checksum_1 == (XT_CHECKSUM_1(sum) ^ XT_CHECKSUM_1(log_id));
		case XT_LOG_ENT_END_OF_LOG:
			return FALSE;
		default:
			ASSERT_NS(FALSE);
			return FALSE;
	}

	xtWord4	g;

	sum ^= (xtWord4) op_seq ^ ((xtWord4) tab_id << 8) ^ XT_CHECKSUM4_REC(rec_id);

	if ((g = sum & 0xF0000000)) {
		sum = sum ^ (g >> 24);
		sum = sum ^ g;
	}
	for (u_int i=0; i<(u_int) rec_size; i++) {
		sum = (sum << 4) + *dptr;
		if ((g = sum & 0xF0000000)) {
			sum = sum ^ (g >> 24);
			sum = sum ^ g;
		}
		dptr++;
	}

	if (check_size == 1) {
		if (record->xh.xh_checksum_1 != (XT_CHECKSUM_1(sum) ^ XT_CHECKSUM_1(log_id))) {
			return FAILED;
		}
	}
	else {
		if (XT_GET_DISK_2(record->xu.xu_checksum_2) != (XT_CHECKSUM_2(sum) ^ XT_CHECKSUM_2(log_id))) {
			return FAILED;
		}
	}
	return TRUE;
}

xtBool XTDatabaseLog::xlog_seq_next(XTXactSeqReadPtr seq, XTXactLogBufferDPtr *ret_entry, xtBool verify, XTThreadPtr thread)
{
	XTXactLogBufferDPtr	record;
	size_t				tfer;
	size_t				len;
	size_t				rec_offset;
	size_t				max_rec_len;
	size_t				size;
	u_int				check_size = 1;

	/* Go to the next record (xseq_record_len must be initialized
	 * to 0 for this to work.
	 */
	seq->xseq_rec_log_offset += seq->xseq_record_len;
	seq->xseq_record_len = 0;

	if (seq->xseq_rec_log_offset < seq->xseq_buf_log_offset ||
		seq->xseq_rec_log_offset >= seq->xseq_buf_log_offset + (xtLogOffset) seq->xseq_buffer_len) {
		/* The current position is nowhere near the buffer, read data into the
		 * buffer:
		 */
		tfer = seq->xseq_buffer_size;
		if (!xlog_rnd_read(seq, seq->xseq_rec_log_id, seq->xseq_rec_log_offset, tfer, seq->xseq_buffer, &tfer, thread))
			return FAILED;
		seq->xseq_buf_log_offset = seq->xseq_rec_log_offset;
		seq->xseq_buffer_len = tfer;

		/* Should we go to the next log? */
		if (!tfer) {
			goto return_empty;
		}
	}

	/* The start of the record is in the buffer: */
	read_from_buffer:
	rec_offset = (size_t) (seq->xseq_rec_log_offset - seq->xseq_buf_log_offset);
	max_rec_len = seq->xseq_buffer_len - rec_offset;
	size = 0;

	/* Check the type of record: */
	record = (XTXactLogBufferDPtr) (seq->xseq_buffer + rec_offset);
	switch (record->xh.xh_status_1) {
		case XT_LOG_ENT_HEADER:
			len = sizeof(XTXactLogHeaderDRec);
			break;
		case XT_LOG_ENT_NEW_LOG:
		case XT_LOG_ENT_DEL_LOG:
			len = sizeof(XTXactNewLogEntryDRec);
			break;
		case XT_LOG_ENT_NEW_TAB:
			len = sizeof(XTXactNewTabEntryDRec);
			break;
		case XT_LOG_ENT_COMMIT:
		case XT_LOG_ENT_ABORT:
			len = sizeof(XTXactEndEntryDRec);
			break;
		case XT_LOG_ENT_CLEANUP:
			len = sizeof(XTXactCleanupEntryDRec);
			break;
		case XT_LOG_ENT_REC_MODIFIED:
		case XT_LOG_ENT_UPDATE:
		case XT_LOG_ENT_INSERT:
		case XT_LOG_ENT_DELETE:
		case XT_LOG_ENT_UPDATE_BG:
		case XT_LOG_ENT_INSERT_BG:
		case XT_LOG_ENT_DELETE_BG:
			check_size = 2;
			len = offsetof(XTactUpdateEntryDRec, xu_rec_type_1);
			if (len > max_rec_len)
				/* The size is not in the buffer: */
				goto read_more;
			len += (size_t) XT_GET_DISK_2(record->xu.xu_size_2);
			break;
		case XT_LOG_ENT_UPDATE_FL:
		case XT_LOG_ENT_INSERT_FL:
		case XT_LOG_ENT_DELETE_FL:
		case XT_LOG_ENT_UPDATE_FL_BG:
		case XT_LOG_ENT_INSERT_FL_BG:
		case XT_LOG_ENT_DELETE_FL_BG:
			check_size = 2;
			len = offsetof(XTactUpdateFLEntryDRec, xf_rec_type_1);
			if (len > max_rec_len)
				/* The size is not in the buffer: */
				goto read_more;
			len += (size_t) XT_GET_DISK_2(record->xf.xf_size_2);
			break;
		case XT_LOG_ENT_REC_FREED:
		case XT_LOG_ENT_REC_REMOVED:
		case XT_LOG_ENT_REC_REMOVED_EXT:
			/* [(7)] REMOVE is now a extended version of FREE! */
			len = offsetof(XTactFreeRecEntryDRec, fr_rec_type_1) + sizeof(XTTabRecFreeDRec);
			break;
		case XT_LOG_ENT_REC_REMOVED_BI:
			check_size = 2;
			len = offsetof(XTactRemoveBIEntryDRec, rb_rec_type_1);
			if (len > max_rec_len)
				/* The size is not in the buffer: */
				goto read_more;
			len += (size_t) XT_GET_DISK_2(record->rb.rb_size_2);
			break;
		case XT_LOG_ENT_REC_MOVED:
			len = offsetof(XTactWriteRecEntryDRec, xw_rec_type_1) + 8;
			break;
		case XT_LOG_ENT_REC_CLEANED:
			len = offsetof(XTactWriteRecEntryDRec, xw_rec_type_1) + offsetof(XTTabRecHeadDRec, tr_prev_rec_id_4) + XT_RECORD_ID_SIZE;
			break;
		case XT_LOG_ENT_REC_CLEANED_1:
			len = offsetof(XTactWriteRecEntryDRec, xw_rec_type_1) + 1;
			break;
		case XT_LOG_ENT_REC_UNLINKED:
			len = offsetof(XTactWriteRecEntryDRec, xw_rec_type_1) + offsetof(XTTabRecHeadDRec, tr_prev_rec_id_4) + XT_RECORD_ID_SIZE;
			break;
		case XT_LOG_ENT_ROW_NEW:
			len = offsetof(XTactRowAddedEntryDRec, xa_row_id_4) + XT_ROW_ID_SIZE;
			break;
		case XT_LOG_ENT_ROW_NEW_FL:
			len = offsetof(XTactRowAddedEntryDRec, xa_free_list_4) + XT_ROW_ID_SIZE;
			break;
		case XT_LOG_ENT_ROW_ADD_REC:
		case XT_LOG_ENT_ROW_SET:
		case XT_LOG_ENT_ROW_FREED:
			len = offsetof(XTactWriteRowEntryDRec, wr_ref_id_4) + XT_REF_ID_SIZE;
			break;
		case XT_LOG_ENT_OP_SYNC:
			len = sizeof(XTactOpSyncEntryDRec);
			break;
		case XT_LOG_ENT_NO_OP:
			len = sizeof(XTactNoOpEntryDRec);
			break;
		case XT_LOG_ENT_END_OF_LOG: {
			off_t eof = seq->xseq_log_eof, adjust;
			
			if (eof > seq->xseq_rec_log_offset) {
				adjust = eof - seq->xseq_rec_log_offset;

				seq->xseq_record_len = (size_t) adjust;
			}
			goto return_empty;
		}
		default:
			/* It is possible to land here after a crash, if the
			 * log was not completely written.
			 */
			seq->xseq_record_len = 0;
			goto return_empty;
	}

	ASSERT_NS(len <= seq->xseq_buffer_size);
	if (len <= max_rec_len) {
		if (verify) {
			if (!xlog_verify(record, len, seq->xseq_rec_log_id)) {
				goto return_empty;
			}
		}

		/* The record is completely in the buffer: */
		seq->xseq_record_len = len;
		*ret_entry = record;
		return OK;
	}
	
	/* The record is partially in the buffer. */
	memmove(seq->xseq_buffer, seq->xseq_buffer + rec_offset, max_rec_len);
	seq->xseq_buf_log_offset += rec_offset;
	seq->xseq_buffer_len = max_rec_len;

	/* Read the rest, as far as possible: */
	tfer = seq->xseq_buffer_size - max_rec_len;
	if (!xlog_rnd_read(seq, seq->xseq_rec_log_id, seq->xseq_buf_log_offset + max_rec_len, tfer, seq->xseq_buffer + max_rec_len, &tfer, thread))
		return FAILED;
	seq->xseq_buffer_len += tfer;

	if (seq->xseq_buffer_len < len) {
		/* A partial record is in the log, must be the end of the log: */
		goto return_empty;
	}

	/* The record is not completely in the buffer: */
	seq->xseq_record_len = len;
	*ret_entry = (XTXactLogBufferDPtr) seq->xseq_buffer;
	return OK;

	read_more:
	ASSERT_NS(len <= seq->xseq_buffer_size);
	memmove(seq->xseq_buffer, seq->xseq_buffer + rec_offset, max_rec_len);
	seq->xseq_buf_log_offset += rec_offset;
	seq->xseq_buffer_len = max_rec_len;

	/* Read the rest, as far as possible: */
	tfer = seq->xseq_buffer_size - max_rec_len;
	if (!xlog_rnd_read(seq, seq->xseq_rec_log_id, seq->xseq_buf_log_offset + max_rec_len, tfer, seq->xseq_buffer + max_rec_len, &tfer, thread))
		return FAILED;
	seq->xseq_buffer_len += tfer;

	if (seq->xseq_buffer_len < len + size) {
		/* We did not get as much as we need, return an empty record: */
		goto return_empty;
	}

	goto read_from_buffer;

	return_empty:
	*ret_entry = NULL;
	return OK;
}

void XTDatabaseLog::xlog_seq_skip(XTXactSeqReadPtr seq, size_t size)
{
	seq->xseq_record_len += size;
}

/* ----------------------------------------------------------------------
 * W R I T E R    P R O C E S S
 */

/*
 * The log has been written. Wake the writer to commit the
 * data to disk, if the transaction log cache is full.
 *
 * Data may not be written to the database until it has been
 * flushed to the log.
 *
 * This is because there is no way to undo changes to the
 * database.
 *
 * However, I have dicovered that writing constantly in the
 * background can disturb the I/O in the foreground.
 *
 * So we can delay the writing of the database. But we should
 * not delay it longer than we have transaction log cache.
 *
 * If so, the data that we need will fall out of the cache
 * and we will have to read it again.
 */
static void xlog_wr_log_written(XTDatabaseHPtr db)
{
	if (db->db_wr_idle) {
		xtWord8 cached_bytes;

		/* Determine if the cached log data is about to fall out of the cache. */
		cached_bytes = db->db_xlog.xl_log_bytes_written - db->db_xlog.xl_log_bytes_read;
		/* The limit is 75%: */
		if (cached_bytes >= xt_xlog_cache.xlc_upper_limit) {
			if (!xt_broadcast_cond_ns(&db->db_wr_cond))
				xt_log_and_clear_exception_ns();
		}
	}
}

#define XT_MORE_TO_WRITE		1
#define XT_FREER_WAITING		2
#define XT_NO_ACTIVITY			3
#define XT_LOG_CACHE_FULL		4
#define XT_CHECKPOINT_REQ		5
#define XT_THREAD_WAITING		6
#define XT_TIME_TO_WRITE		7

/*
 * Wait for a transaction to quit, i.e. the log to be flushed.
 */
static void xlog_wr_wait_for_log_flush(XTThreadPtr self, XTDatabaseHPtr db)
{
	xtXactID	last_xn_id;
	xtWord8		cached_bytes;
	int			reason = XT_MORE_TO_WRITE;

#ifdef TRACE_WRITER_ACTIVITY
	printf("WRITER --- DONE\n");
#endif

	xt_lock_mutex(self, &db->db_wr_lock);
	pushr_(xt_unlock_mutex, &db->db_wr_lock);

	/*
	 * Wake the freeer if it is waiting for this writer, before
	 * we go to sleep!
	 */
	if (db->db_wr_freeer_waiting) {
		if (!xt_broadcast_cond_ns(&db->db_wr_cond))
			xt_log_and_clear_exception_ns();
	}

	if (db->db_wr_flush_point_log_id == db->db_xlog.xl_flush_log_id &&
		db->db_wr_flush_point_log_offset == db->db_xlog.xl_flush_log_offset) {
		/* Wake the checkpointer to flush the indexes:
		 * PMC 15.05.2008 - Not doing this anymore!
		xt_wake_checkpointer(self, db);
		*/

		/* Sleep as long as the flush point has not changed, from the last
		 * target flush point.
		 */
		while (!self->t_quit &&
			db->db_wr_flush_point_log_id == db->db_xlog.xl_flush_log_id &&
			db->db_wr_flush_point_log_offset == db->db_xlog.xl_flush_log_offset &&
			reason != XT_LOG_CACHE_FULL &&
			reason != XT_TIME_TO_WRITE &&
			reason != XT_CHECKPOINT_REQ) {

			/*
			 * Sleep as long as there is no reason to write any more...
			 */
			while (!self->t_quit) {
				last_xn_id = db->db_xn_curr_id;
				db->db_wr_idle = XT_THREAD_IDLE;
				xt_timed_wait_cond(self, &db->db_wr_cond, &db->db_wr_lock, 500);
				db->db_wr_idle = XT_THREAD_BUSY;
				/* These are the reasons for doing work: */
				/* The free'er thread is waiting for the writer: */
				if (db->db_wr_freeer_waiting) {
					reason = XT_FREER_WAITING;
					break;
				}
				/* Some thread is waiting for the writer: */
				if (db->db_wr_thread_waiting) {
					reason = XT_THREAD_WAITING;
					break;
				}
				/* Check if the cache will soon overflow... */
				ASSERT(db->db_xlog.xl_log_bytes_written >= db->db_xlog.xl_log_bytes_read);
				ASSERT(db->db_xlog.xl_log_bytes_written >= db->db_xlog.xl_log_bytes_flushed);
				/* Sanity check: */
				ASSERT(db->db_xlog.xl_log_bytes_written < db->db_xlog.xl_log_bytes_read + 500000000);
				/* This is the amount of data still to be written: */
				cached_bytes = db->db_xlog.xl_log_bytes_written - db->db_xlog.xl_log_bytes_read;
				/* The limit is 75%: */
				if (cached_bytes >= xt_xlog_cache.xlc_upper_limit) {
					reason = XT_LOG_CACHE_FULL;
					break;
				}
				
				/* TODO: Create a system variable which specifies the write frequency. *//*
				if (cached_bytes >= (12 * 1024 * 1024)) {
					reason = XT_TIME_TO_WRITE;
					break;
				}
				*/
				
				/* Check if we are holding up a checkpoint: */
				if (db->db_restart.xres_cp_required ||
					db->db_restart.xres_is_checkpoint_pending(db->db_xlog.xl_write_log_id, db->db_xlog.xl_write_log_offset)) {
					/* Enough data has been flushed for a checkpoint: */
					if (!db->db_restart.xres_is_checkpoint_pending(db->db_wr_log_id, db->db_wr_log_offset)) {
						/* But not enough data has been written for a checkpoint: */
						reason = XT_CHECKPOINT_REQ;
						break;
					}
				}
				/* There is no activity, if the current ID has not changed during
				 * the wait, and the sweeper has nothing to do, and the checkpointer.
				 */
				if (db->db_xn_curr_id == last_xn_id &&
					/* Changed xt_xn_get_curr_id(db) to db->db_xn_curr_id,
					 * This should work because we are not concerned about the difference
					 * between xt_xn_get_curr_id(db) and db->db_xn_curr_id,
					 * Which is just a matter of when transactions we can expect ot find
					 * in memory (see {GAP-INC-ADD-XACT})
					 */
					xt_xn_is_before(db->db_xn_curr_id, db->db_xn_to_clean_id) && // db->db_xn_curr_id < db->db_xn_to_clean_id
					!db->db_restart.xres_is_checkpoint_pending(db->db_xlog.xl_write_log_id, db->db_xlog.xl_write_log_offset)) {
					/* There seems to be no activity at the moment.
					 * this might be a good time to write the log data.
					 */
					reason = XT_NO_ACTIVITY;
					break;
				}
			}
		}
	}
	freer_(); // xt_unlock_mutex(&db->db_wr_lock)

	if (reason == XT_LOG_CACHE_FULL || reason == XT_TIME_TO_WRITE || reason == XT_CHECKPOINT_REQ) {
		/* Make sure that we have something to write: */
		if (db->db_xlog.xlog_bytes_to_write() < 2 * 1204 * 1024)
			xt_xlog_flush_log(self);
	}

#ifdef TRACE_WRITER_ACTIVITY
	switch (reason) {
		case XT_MORE_TO_WRITE:	printf("WRITER --- still more to write...\n"); break;
		case XT_FREER_WAITING:	printf("WRITER --- free'er waiting for writer...\n"); break;
		case XT_NO_ACTIVITY:	printf("WRITER --- no activity...\n"); break;
		case XT_LOG_CACHE_FULL:	printf("WRITER --- running out of log cache...\n"); break;
		case XT_CHECKPOINT_REQ:	printf("WRITER --- enough flushed for a checkpoint...\n"); break;
		case XT_THREAD_WAITING: printf("WRITER --- thread waiting for writer...\n"); break;
		case XT_TIME_TO_WRITE:	printf("WRITER --- limit of 12MB reached, time to write...\n"); break;
	}
#endif
}

static void xlog_wr_could_go_faster(XTThreadPtr self, XTDatabaseHPtr db)
{
	if (db->db_wr_faster) {
		if (!db->db_wr_fast) {
			xt_set_normal_priority(self);
			db->db_wr_fast = TRUE;
		}
		db->db_wr_faster = FALSE;
	}
}

static void xlog_wr_could_go_slower(XTThreadPtr self, XTDatabaseHPtr db)
{
	if (db->db_wr_fast && !db->db_wr_faster) {
		xt_set_low_priority(self);
		db->db_wr_fast = FALSE;
	}
}

static void xlog_wr_main(XTThreadPtr self)
{
	XTDatabaseHPtr		db = self->st_database;
	XTWriterStatePtr	ws;
	XTXactLogBufferDPtr	record;

	xt_set_low_priority(self);

	alloczr_(ws, xt_free_writer_state, sizeof(XTWriterStateRec), XTWriterStatePtr);
	ws->ws_db = db;
	ws->ws_in_recover = FALSE;

	if (!db->db_xlog.xlog_seq_init(&ws->ws_seqread, xt_db_log_buffer_size, FALSE))
		xt_throw(self);

	if (!db->db_xlog.xlog_seq_start(&ws->ws_seqread, db->db_wr_log_id, db->db_wr_log_offset, FALSE))
		xt_throw(self);

	while (!self->t_quit) {
		while (!self->t_quit) {
			/* Determine the point to which we can write.
			 * This is the current log flush point!
			 */
			xt_lock_mutex_ns(&db->db_wr_lock);
			db->db_wr_flush_point_log_id = db->db_xlog.xl_flush_log_id;
			db->db_wr_flush_point_log_offset = db->db_xlog.xl_flush_log_offset;
			xt_unlock_mutex_ns(&db->db_wr_lock);

			if (xt_comp_log_pos(db->db_wr_log_id, db->db_wr_log_offset, db->db_wr_flush_point_log_id, db->db_wr_flush_point_log_offset) >= 0) {
				break;
			}

			while (!self->t_quit) {
				xlog_wr_could_go_faster(self, db);

				/* This is the restart position: */
				xt_lock_mutex(self, &db->db_wr_lock);
				pushr_(xt_unlock_mutex, &db->db_wr_lock);
				db->db_wr_log_id = ws->ws_seqread.xseq_rec_log_id;
				db->db_wr_log_offset = ws->ws_seqread.xseq_rec_log_offset +  ws->ws_seqread.xseq_record_len;
				freer_(); // xt_unlock_mutex(&db->db_wr_lock)

				if (xt_comp_log_pos(db->db_wr_log_id, db->db_wr_log_offset, db->db_wr_flush_point_log_id, db->db_wr_flush_point_log_offset) >= 0) {
					break;
				}

				/* Apply all changes that have been flushed to the log, to the
				 * database.
				 */
				if (!db->db_xlog.xlog_seq_next(&ws->ws_seqread, &record, FALSE, self))
					xt_throw(self);
				if (!record) {
					break;
				}
				switch (record->xl.xl_status_1) {
					case XT_LOG_ENT_HEADER:
						break;
					case XT_LOG_ENT_NEW_LOG:
						if (!db->db_xlog.xlog_seq_start(&ws->ws_seqread, XT_GET_DISK_4(record->xl.xl_log_id_4), 0, TRUE))
							xt_throw(self);
						break;
					case XT_LOG_ENT_NEW_TAB:
					case XT_LOG_ENT_COMMIT:
					case XT_LOG_ENT_ABORT:
					case XT_LOG_ENT_CLEANUP:
					case XT_LOG_ENT_OP_SYNC:
						break;
					case XT_LOG_ENT_DEL_LOG:
						xtLogID log_id;

						log_id = XT_GET_DISK_4(record->xl.xl_log_id_4);
						xt_dl_set_to_delete(self, db, log_id);
						break;
					default:
						xt_xres_apply_in_order(self, ws, ws->ws_seqread.xseq_rec_log_id, ws->ws_seqread.xseq_rec_log_offset, record);
						break;
				}
				/* Count the number of bytes read from the log: */
				db->db_xlog.xl_log_bytes_read += ws->ws_seqread.xseq_record_len;
			}
		}

		if (ws->ws_ot) {
			xt_db_return_table_to_pool(self, ws->ws_ot);
			ws->ws_ot = NULL;
		}

		xlog_wr_could_go_slower(self, db);

		/* Note, we delay writing the database for a maximum of
		 * 2 seconds.
		 */
		xlog_wr_wait_for_log_flush(self, db);
	}

	freer_(); // xt_free_writer_state(ss)
}

static void *xlog_wr_run_thread(XTThreadPtr self)
{
	XTDatabaseHPtr	db = (XTDatabaseHPtr) self->t_data;
	int				count;
	void			*mysql_thread;

	mysql_thread = myxt_create_thread();

	while (!self->t_quit) {
		try_(a) {
			/*
			 * The garbage collector requires that the database
			 * is in use because.
			 */
			xt_use_database(self, db, XT_FOR_WRITER);

			/* This action is both safe and required (see details elsewhere) */
			xt_heap_release(self, self->st_database);

			xlog_wr_main(self);
		}
		catch_(a) {
			/* This error is "normal"! */
			if (self->t_exception.e_xt_err != XT_ERR_NO_DICTIONARY &&
				!(self->t_exception.e_xt_err == XT_SIGNAL_CAUGHT &&
				self->t_exception.e_sys_err == SIGTERM))
				xt_log_and_clear_exception(self);
		}
		cont_(a);

		/* Avoid releasing the database (done above) */
		self->st_database = NULL;
		xt_unuse_database(self, self);

		/* After an exception, pause before trying again... */
		/* Number of seconds */
#ifdef DEBUG
		count = 10;
#else
		count = 2*60;
#endif
		db->db_wr_idle = XT_THREAD_INERR;
		while (!self->t_quit && count > 0) {
			sleep(1);
			count--;
		}
		db->db_wr_idle = XT_THREAD_BUSY;
	}

	myxt_destroy_thread(mysql_thread, TRUE);
	return NULL;
}

static void xlog_wr_free_thread(XTThreadPtr self, void *data)
{
	XTDatabaseHPtr db = (XTDatabaseHPtr) data;

	if (db->db_wr_thread) {
		xt_lock_mutex(self, &db->db_wr_lock);
		pushr_(xt_unlock_mutex, &db->db_wr_lock);
		db->db_wr_thread = NULL;
		freer_(); // xt_unlock_mutex(&db->db_wr_lock)
	}
}

xtPublic void xt_start_writer(XTThreadPtr self, XTDatabaseHPtr db)
{
	char name[PATH_MAX];

	sprintf(name, "WR-%s", xt_last_directory_of_path(db->db_main_path));
	xt_remove_dir_char(name);
	db->db_wr_thread = xt_create_daemon(self, name);
	xt_set_thread_data(db->db_wr_thread, db, xlog_wr_free_thread);
	xt_run_thread(self, db->db_wr_thread, xlog_wr_run_thread);
}

/*
 * This function is called on database shutdown.
 * We will wait a certain amounnt of time for the writer to
 * complete its work.
 * If it takes to long we will abort!
 */
xtPublic void xt_wait_for_writer(XTThreadPtr self, XTDatabaseHPtr db)
{
	time_t	then, now;
	xtBool	message = FALSE;

	if (db->db_wr_thread) {
		then = time(NULL);
		while (xt_comp_log_pos(db->db_wr_log_id, db->db_wr_log_offset, db->db_wr_flush_point_log_id, db->db_wr_flush_point_log_offset) < 0) {

			xt_lock_mutex(self, &db->db_wr_lock);
			pushr_(xt_unlock_mutex, &db->db_wr_lock);
			db->db_wr_thread_waiting++;
			/* Wake the writer so that it con complete its work. */
			if (db->db_wr_idle) {
				if (!xt_broadcast_cond_ns(&db->db_wr_cond))
					xt_log_and_clear_exception_ns();
			}
			freer_(); // xt_unlock_mutex(&db->db_wr_lock)

			xt_sleep_milli_second(10);

			xt_lock_mutex(self, &db->db_wr_lock);
			pushr_(xt_unlock_mutex, &db->db_wr_lock);
			db->db_wr_thread_waiting--;
			freer_(); // xt_unlock_mutex(&db->db_wr_lock)

			now = time(NULL);
			if (now >= then + 16) {
				xt_logf(XT_NT_INFO, "Aborting wait for '%s' writer\n", db->db_name);
				message = FALSE;
				break;
			}
			if (now >= then + 2) {
				if (!message) {
					message = TRUE;
					xt_logf(XT_NT_INFO, "Waiting for '%s' writer...\n", db->db_name);
				}
			}
		}
		
		if (message)
			xt_logf(XT_NT_INFO, "Writer '%s' done.\n", db->db_name);
	}
}

xtPublic void xt_stop_writer(XTThreadPtr self, XTDatabaseHPtr db)
{
	XTThreadPtr thr_wr;

	if (db->db_wr_thread) {
		xt_lock_mutex(self, &db->db_wr_lock);
		pushr_(xt_unlock_mutex, &db->db_wr_lock);

		/* This pointer is safe as long as you have the transaction lock. */
		if ((thr_wr = db->db_wr_thread)) {
			xtThreadID tid = thr_wr->t_id;

			/* Make sure the thread quits when woken up. */
			xt_terminate_thread(self, thr_wr);

			/* Wake the writer thread so that it will quit: */
			xt_broadcast_cond(self, &db->db_wr_cond);
	
			freer_(); // xt_unlock_mutex(&db->db_wr_lock)

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
			thr_wr->t_delayed_signal = SIGTERM;
			xt_kill_thread(thread);
			 */
			db->db_wr_thread = NULL;
		}
		else
			freer_(); // xt_unlock_mutex(&db->db_wr_lock)
	}
}

#ifdef NOT_USED
static void xlog_add_to_flush_buffer(u_int flush_count, XTXLogBlockPtr *flush_buffer, XTXLogBlockPtr block)
{
	register u_int		count = flush_count;
	register u_int		i;
	register u_int		guess;
	register xtInt8		r;

	i = 0;
	while (i < count) {
		guess = (i + count - 1) >> 1;
		r = (xtInt8) block->xlb_address - (xtInt8) flush_buffer[guess]->xlb_address;
		if (r == 0) {
			// Should not happen...
			ASSERT_NS(FALSE);
			return;
		}
		if (r < (xtInt8) 0)
			count = guess;
		else
			i = guess + 1;
	}

	/* Insert at position i */
	memmove(flush_buffer + i + 1, flush_buffer + i, (flush_count - i) * sizeof(XTXLogBlockPtr));
	flush_buffer[i] = block;
}

static XTXLogBlockPtr xlog_find_block(XTOpenFilePtr file, xtLogID log_id, off_t address, XTXLogCacheSegPtr *ret_seg)
{
	register XTXLogCacheSegPtr	seg;
	register XTXLogBlockPtr		block;
	register u_int				hash_idx;
	register XTXLogCacheRec		*dcg = &xt_xlog_cache;

	seg = &dcg->xlc_segment[((u_int) address >> XT_XLC_BLOCK_SHIFTS) & XLC_SEGMENT_MASK];
	hash_idx = (((u_int) (address >> (XT_XLC_SEGMENT_SHIFTS + XT_XLC_BLOCK_SHIFTS))) ^ (log_id << 16)) % dcg->xlc_hash_size;

	xt_lock_mutex_ns(&seg->lcs_lock);
	retry:
	block = seg->lcs_hash_table[hash_idx];
	while (block) {
		if (block->xlb_address == address && block->xlb_log_id == log_id) {
			ASSERT_NS(block->xlb_state != XLC_BLOCK_FREE);

			/* Wait if the block is being read or written.
			 * If we will just read the data, then we don't care
			 * if the buffer is being written.
			 */
			if (block->xlb_state == XLC_BLOCK_READING) {
				if (!xt_timed_wait_cond_ns(&seg->lcs_cond, &seg->lcs_lock, 100))
					break;
				goto retry;
			}

			*ret_seg = seg;
			return block;
		}
		block = block->xlb_next;
	}
	
	/* Block not found: */
	xt_unlock_mutex_ns(&seg->lcs_lock);
	return NULL;
}

static int xlog_cmp_log_files(struct XTThread *self, register const void *thunk, register const void *a, register const void *b)
{
#pragma unused(self, thunk)
	xtLogID				lf_id = *((xtLogID *) a);
	XTXactLogFilePtr	lf_ptr = (XTXactLogFilePtr) b;

	if (lf_id < lf_ptr->lf_log_id)
		return -1;
	if (lf_id == lf_ptr->lf_log_id)
		return 0;
	return 1;
}

#endif


#ifdef OLD_CODE
static xtBool xlog_free_lru_blocks()
{
	XTXLogBlockPtr		block, pblock;
	xtWord4				ru_time;
	xtLogID				log_id;
	off_t				address;
	//off_t				hash;
	XTXLogCacheSegPtr	seg;
	u_int				hash_idx;
	xtBool				have_global_lock = FALSE;

#ifdef DEBUG_CHECK_CACHE
	//xt_xlog_check_cache();
#endif
	retry:
	if (!(block = xt_xlog_cache.xlc_lru_block))
		return OK;

	ru_time = block->xlb_ru_time;
	log_id = block->xlb_log_id;
	address = block->xlb_address;

	/*
	hash = (address >> XT_XLC_BLOCK_SHIFTS) ^ ((off_t) log_id << 28);
	seg = &xt_xlog_cache.xlc_segment[hash & XLC_SEGMENT_MASK];
	hash_idx = (hash >> XT_XLC_SEGMENT_SHIFTS) % xt_xlog_cache.xlc_hash_size;
	*/
	seg = &xt_xlog_cache.xlc_segment[((u_int) address >> XT_XLC_BLOCK_SHIFTS) & XLC_SEGMENT_MASK];
	hash_idx = (((u_int) (address >> (XT_XLC_SEGMENT_SHIFTS + XT_XLC_BLOCK_SHIFTS))) ^ (log_id << 16)) % xt_xlog_cache.xlc_hash_size;

	xt_lock_mutex_ns(&seg->lcs_lock);

	free_more:
	pblock = NULL;
	block = seg->lcs_hash_table[hash_idx];
	while (block) {
		if (block->xlb_address == address && block->xlb_log_id == log_id) {
			ASSERT_NS(block->xlb_state != XLC_BLOCK_FREE);
			
			/* Try again if the block has been used in the meantime: */
			if (ru_time != block->xlb_ru_time) {
				if (have_global_lock)
					/* Having this lock means we have already freed at least one block so
					 * don't bother to free more if we are having trouble.
					 */
					goto done_ok;

				/* If the recently used time has changed, then the
				 * block is probably no longer the LR used.
				 */
				xt_unlock_mutex_ns(&seg->lcs_lock);
				goto retry;
			}

			/* Wait if the block is being read: */
			if (block->xlb_state == XLC_BLOCK_READING) {
				if (have_global_lock)
					goto done_ok;

				/* Wait for the block to be read, then try again. */
				if (!xt_timed_wait_cond_ns(&seg->lcs_cond, &seg->lcs_lock, 100))
					goto failed;
				xt_unlock_mutex_ns(&seg->lcs_lock);
				goto retry;
			}
			
			goto free_the_block;
		}
		pblock = block;
		block = block->xlb_next;
	}

	if (have_global_lock) {
		xt_unlock_mutex_ns(&xt_xlog_cache.xlc_lock);
		have_global_lock = FALSE;
	}

	/* We did not find the block, someone else freed it... */
	xt_unlock_mutex_ns(&seg->lcs_lock);
	goto retry;

	free_the_block:
	ASSERT_NS(block->xlb_state == XLC_BLOCK_CLEAN);

	/* Remove from the hash table: */
	if (pblock)
		pblock->xlb_next = block->xlb_next;
	else
		seg->lcs_hash_table[hash_idx] = block->xlb_next;

	/* Now free the block */
	if (!have_global_lock) {
		xt_lock_mutex_ns(&xt_xlog_cache.xlc_lock);
		have_global_lock = TRUE;
	}

	/* Remove from the MRU list: */
	if (xt_xlog_cache.xlc_lru_block == block)
		xt_xlog_cache.xlc_lru_block = block->xlb_mr_used;
	if (xt_xlog_cache.xlc_mru_block == block)
		xt_xlog_cache.xlc_mru_block = block->xlb_lr_used;
	if (block->xlb_lr_used)
		block->xlb_lr_used->xlb_mr_used = block->xlb_mr_used;
	if (block->xlb_mr_used)
		block->xlb_mr_used->xlb_lr_used = block->xlb_lr_used;

	/* Put the block on the free list: */
	block->xlb_next = xt_xlog_cache.xlc_free_list;
	xt_xlog_cache.xlc_free_list = block;
	xt_xlog_cache.xlc_free_count++;
	block->xlb_state = XLC_BLOCK_FREE;

	if (xt_xlog_cache.xlc_free_count < XT_XLC_MAX_FREE_COUNT) {
		/* Now that we have all the locks, try to free some more in this segment: */
		block = block->xlb_mr_used;
		for (u_int i=0; block && i<XLC_SEGMENT_COUNT; i++) {
			ru_time = block->xlb_ru_time;
			log_id = block->xlb_log_id;
			address = block->xlb_address;

			if (seg == &xt_xlog_cache.xlc_segment[((u_int) address >> XT_XLC_BLOCK_SHIFTS) & XLC_SEGMENT_MASK]) {
				hash_idx = (((u_int) (address >> (XT_XLC_SEGMENT_SHIFTS + XT_XLC_BLOCK_SHIFTS))) ^ (log_id << 16)) % xt_xlog_cache.xlc_hash_size;
				goto free_more;
			}
			block = block->xlb_mr_used;
		}
	}

	done_ok:
	xt_unlock_mutex_ns(&xt_xlog_cache.xlc_lock);
	xt_unlock_mutex_ns(&seg->lcs_lock);
#ifdef DEBUG_CHECK_CACHE
	//xt_xlog_check_cache();
#endif
	return OK;
	
	failed:
	xt_unlock_mutex_ns(&seg->lcs_lock);
#ifdef DEBUG_CHECK_CACHE
	//xt_xlog_check_cache();
#endif
	return FAILED;
}

#endif
