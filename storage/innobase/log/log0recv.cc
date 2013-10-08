/*****************************************************************************

Copyright (c) 1997, 2013, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2012, Facebook Inc.

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

/**
@file log/log0recv.cc
Recovery

Created 9/20/1997 Heikki Tuuri
*******************************************************/

#include "log0recv.h"

#include "mem0mem.h"
#include "buf0buf.h"
#include "buf0flu.h"
#include "mtr0mtr.h"
#include "mtr0log.h"
#include "page0cur.h"
#include "page0zip.h"
#include "btr0btr.h"
#include "btr0cur.h"
#include "ibuf0ibuf.h"
#include "trx0undo.h"
#include "trx0rec.h"
#include "fil0fil.h"
#ifndef UNIV_HOTBACKUP
# include "buf0rea.h"
# include "srv0srv.h"
# include "srv0start.h"
# include "trx0roll.h"
# include "row0merge.h"
# include "sync0mutex.h"
#else /* !UNIV_HOTBACKUP */

/** This is set to false if the backup was originally taken with the
ibbackup --include regexp option: then we do not want to create tables in
directories which were not included */
bool		recv_replay_file_ops	= true;
#endif /* !UNIV_HOTBACKUP */

/** Size of the parsing buffer; it must accommodate RECV_SCAN_SIZE many
times! */
const ulint	redo_recover_t::s_parsing_buf_size = 2 * 1024 * 1024;

/** Log records are stored in the hash table in chunks at most of this size;
this must be less than UNIV_PAGE_SIZE as it is stored in the buffer pool */
#define RECV_DATA_BLOCK_SIZE	(MEM_MAX_ALLOC_IN_BUF - sizeof(recv_data_t))

/** Read-ahead area in applying log records to file pages */
static const ulint		RECV_READ_AHEAD_AREA = 32;

// Temporary hack
redo_recover_t	recover;
redo_recover_t*	recover_ptr = &recover;

#ifndef UNIV_HOTBACKUP
/** true when the redo log is being backed up */
# define recv_is_making_a_backup		false
/** true when recovering from a backed up redo log file */
# define recv_is_from_backup			false
#else /* !UNIV_HOTBACKUP */
# define buf_pool_get_curr_size() (5 * 1024 * 1024)
#endif /* !UNIV_HOTBACKUP */

#ifdef UNIV_PFS_THREAD
mysql_pfs_key_t	trx_rollback_clean_thread_key;
#endif /* UNIV_PFS_THREAD */

#ifndef UNIV_HOTBACKUP
# ifdef UNIV_PFS_THREAD
mysql_pfs_key_t	recv_writer_thread_key;
# endif /* UNIV_PFS_THREAD */

#endif /* !UNIV_HOTBACKUP */

/* prototypes */

/**
Creates the recovery system.
@param n_bytes		buffer pool memory available in bytes
@return DB_FAIL if recovery should be skipped */
dberr_t
redo_recover_t::create(ulint n_bytes)
{
	// FIXME: Make this an assertion
	if (!m_inited) {
		mutex_create("recv_sys", &m_mutex);

#ifndef UNIV_HOTBACKUP
		mutex_create("recv_writer", &m_writer_mutex);
#endif /* !UNIV_HOTBACKUP */

		m_inited = true;

		init(n_bytes);

		if (srv_force_recovery >= SRV_FORCE_NO_LOG_REDO) {

			return(DB_FAIL);
		}

		m_limit_lsn = LSN_MAX;
	}

	return(DB_SUCCESS);
}

/**
Release recovery system mutexes. */
void
redo_recover_t::destroy()
{
	// FIXME: Make this an assertion
	if (!m_inited) {
		return;
	}

	if (m_addr_hash != NULL) {
		hash_table_free(m_addr_hash);
	}

	if (m_heap != NULL) {
		mem_heap_free(m_heap);
	}

	if (m_buf != NULL) {
		ut_free(m_buf);
	}

#ifndef UNIV_HOTBACKUP
	ut_ad(!m_writer_thread_active);
	mutex_free(&m_writer_mutex);
#endif /* !UNIV_HOTBACKUP */

	mutex_free(&m_mutex);
}

/**
Frees the recovery system memory. */
void
redo_recover_t::release_resources()
{
	if (m_addr_hash != NULL) {
		hash_table_free(m_addr_hash);
	}

	if (m_heap != NULL) {
		mem_heap_free(m_heap);
	}

	if (m_buf != NULL) {
		ut_free(m_buf);
	}
}

#ifndef UNIV_HOTBACKUP
/**
Reset the state of the recovery system variables. */
void
redo_recover_t::var_init()
{
	m_lsn_checks_on = false;

	m_needed_recovery = false;

	m_log_scan_is_startup_type = false;

	m_max_page_lsn = 0;

	m_previous_parsed_rec_type = 999999;

	m_previous_parsed_rec_offset = 0;

	m_previous_parsed_rec_is_multi = 0;

	m_max_parsed_page_no = 0;

	m_writer_thread_active = false;
}

/**
redo_recover_t::writer thread tasked with flushing dirty pages from the buffer
pools.
@return a dummy parameter */
extern "C"
os_thread_ret_t
DECLARE_THREAD(recv_writer_thread)(
	void*	arg __attribute__((unused)))
			/*!< in: a dummy parameter required by
			os_thread_create */
{
	ut_ad(!srv_read_only_mode);

#ifdef UNIV_PFS_THREAD
	pfs_register_thread(recv_writer_thread_key);
#endif /* UNIV_PFS_THREAD */

#ifdef UNIV_DEBUG_THREAD_CREATION
	fprintf(stderr, "InnoDB: recv_writer thread running, id %lu\n",
		os_thread_pf(os_thread_get_curr_id()));
#endif /* UNIV_DEBUG_THREAD_CREATION */

	recover_ptr->writer_thread_started();

	while (srv_shutdown_state == SRV_SHUTDOWN_NONE) {

		os_thread_sleep(100000);

		mutex_enter(&recover_ptr->m_writer_mutex);

		if (!redo_log->is_recovery_on()) {
			mutex_exit(&recover_ptr->m_writer_mutex);
			break;
		}

		/* Flush pages from end of LRU if required */
		buf_flush_LRU_tail();

		mutex_exit(&recover_ptr->m_writer_mutex);
	}

	recover_ptr->writer_thread_exit();

	/* We count the number of threads in os_thread_exit().
	A created thread should always use that to exit and not
	use return() to exit. */
	os_thread_exit(NULL);

	OS_THREAD_DUMMY_RETURN;
}
#endif /* !UNIV_HOTBACKUP */

/**
Inits the recovery system for a recovery operation.
@param n_bytes		memory available in bytes*/
void
redo_recover_t::init(ulint n_bytes)
{
	ut_a(m_heap == NULL);

#ifndef UNIV_HOTBACKUP
	/* Initialize red-black tree for fast insertions into the
	flush_list during recovery process.
	As this initialization is done while holding the buffer pool
	mutex we perform it before acquiring m_mutex. */
	buf_flush_init_flush_rbt();

	mutex_enter(&m_mutex);

	m_heap = mem_heap_create_typed(256, MEM_HEAP_FOR_RECV_SYS);

#else /* !UNIV_HOTBACKUP */
	m_heap = mem_heap_create(256);
	m_is_from_backup = true;
#endif /* !UNIV_HOTBACKUP */

	m_buf = static_cast<byte*>(ut_malloc(s_parsing_buf_size));
	m_len = 0;
	m_recovered_offset = 0;

	m_addr_hash = hash_create(n_bytes / 512);
	m_n_addrs = 0;

	m_apply_log_recs = false;
	m_apply_batch_on = false;

	m_found_corrupt_log = false;

	m_max_page_lsn = 0;

	mutex_exit(&m_mutex);
}

/**
Empties the hash table when it has been fully processed. */
void
redo_recover_t::empty_hash()
{
	ut_ad(mutex_own(&m_mutex));

	if (m_n_addrs != 0) {
		ib_logf(IB_LOG_LEVEL_FATAL,
			"%lu pages with log records were left unprocessed! "
			"Maximum page number with log records on it is %lu",
			(ulong) m_n_addrs,
			(ulong) m_max_parsed_page_no);
	}

	hash_table_free(m_addr_hash);
	mem_heap_empty(m_heap);

	m_addr_hash = hash_create(buf_pool_get_curr_size() / 512);
}

#ifndef UNIV_HOTBACKUP
/**
Frees the recovery system. */
void
redo_recover_t::debug_free()
{
	mutex_enter(&m_mutex);

	hash_table_free(m_addr_hash);
	mem_heap_free(m_heap);
	ut_free(m_buf);

	m_buf = NULL;
	m_heap = NULL;
	m_addr_hash = NULL;

	mutex_exit(&m_mutex);

	/* Free up the flush_rbt. */
	buf_flush_free_flush_rbt();
}

#endif /* !UNIV_HOTBACKUP */

#ifdef UNIV_HOTBACKUP
/**
Reads the checkpoint info needed in hot backup.
@param hdr		buffer containing the log group header
@param lsn		checkpoint lsn
@param offse		checkpoint offset in the log group
@param cp_no,		checkpoint number
@param first_header_lsn lsn of of the start of the first log file
@return	true if success */
bool
redo_recover_t::read_checkpoint_info_for_backup(
	const byte*	hdr,
	lsn_t*		lsn,
	lsn_t*		offset,
	lsn_t*		cp_no,
	lsn_t*		first_header_lsn)
{
	ulint		max_cp		= 0;
	ib_uint64_t	max_cp_no	= 0;
	const byte*	cp_buf;

	cp_buf = hdr + LOG_CHECKPOINT_1;

	if (m_check_cp_is_consistent(cp_buf)) {
		max_cp_no = mach_read_from_8(cp_buf + LOG_CHECKPOINT_NO);
		max_cp = LOG_CHECKPOINT_1;
	}

	cp_buf = hdr + LOG_CHECKPOINT_2;

	if (check_cp_is_consistent(cp_buf)) {
		if (mach_read_from_8(cp_buf + LOG_CHECKPOINT_NO) > max_cp_no) {
			max_cp = LOG_CHECKPOINT_2;
		}
	}

	if (max_cp == 0) {
		return(false);
	}

	cp_buf = hdr + max_cp;

	*lsn = mach_read_from_8(cp_buf + LOG_CHECKPOINT_LSN);
	*offset = mach_read_from_4(
		cp_buf + LOG_CHECKPOINT_OFFSET_LOW32);
	*offset |= ((lsn_t) mach_read_from_4(
			    cp_buf + LOG_CHECKPOINT_OFFSET_HIGH32)) << 32;

	*cp_no = mach_read_from_8(cp_buf + LOG_CHECKPOINT_NO);

	*first_header_lsn = mach_read_from_8(hdr + LOG_FILE_START_LSN);

	return(true);
}
#endif /* !UNIV_HOTBACKUP */
/**
Tries to parse a single log record body and also applies it to a page if
specified. File ops are parsed, but not applied in this function.

@param type		type
@param ptr		pointer to a buffer
@param end_ptr		pointer to the buffer end
@param block		buffer block or NULL; if not NULL, then the
			log record is applied to the page, and the log
			record should be complete then
@param 	mtr		mtr or NULL; should be non-NULL if and only
			if block is non-NULL
@param space_id		tablespace id obtained by parsing initial log
			record
@param page_no		obtained by parsing initial log record.

@return	log record end, NULL if not a complete record */
byte*
redo_recover_t::parse_or_apply_log_rec_body(
	mlog_id_t	type,
	byte*		ptr,
	byte*		end_ptr,
	buf_block_t*	block,
	mtr_t*		mtr,
	ulint		space_id,
	ulint		page_no)
{
	page_t*		page;
	page_zip_des_t*	page_zip;
	dict_index_t*	index	= NULL;
#ifdef UNIV_DEBUG
	ulint		page_type;
#endif /* UNIV_DEBUG */

	ut_ad(!block == !mtr);

	if (block) {
		page = block->frame;
		page_zip = buf_block_get_page_zip(block);
		ut_d(page_type = fil_page_get_type(page));
	} else {
		page = NULL;
		page_zip = NULL;
		ut_d(page_type = FIL_PAGE_TYPE_ALLOCATED);
	}

	switch (type) {
#ifdef UNIV_LOG_LSN_DEBUG
	case MLOG_LSN:
		/* The LSN is checked in parse_log_rec(). */
		break;
#endif /* UNIV_LOG_LSN_DEBUG */
	case MLOG_1BYTE: case MLOG_2BYTES: case MLOG_4BYTES: case MLOG_8BYTES:
#ifdef UNIV_DEBUG
		if (page && page_type == FIL_PAGE_TYPE_ALLOCATED
		    && end_ptr >= ptr + 2) {
			/* It is OK to set FIL_PAGE_TYPE and certain
			list node fields on an empty page.  Any other
			write is not OK. */

			/* NOTE: There may be bogus assertion failures for
			dict_hdr_create(), trx_rseg_header_create(),
			trx_sys_create_doublewrite_buf(), and
			trx_sysf_create().
			These are only called during database creation. */
			ulint	offs = mach_read_from_2(ptr);

			switch (type) {
			default:
				ut_error;
			case MLOG_2BYTES:
				/* Note that this can fail when the
				redo log been written with something
				older than InnoDB Plugin 1.0.4. */
				ut_ad(offs == FIL_PAGE_TYPE
				      || offs == IBUF_TREE_SEG_HEADER
				      + IBUF_HEADER + FSEG_HDR_OFFSET
				      || offs == PAGE_BTR_IBUF_FREE_LIST
				      + PAGE_HEADER + FIL_ADDR_BYTE
				      || offs == PAGE_BTR_IBUF_FREE_LIST
				      + PAGE_HEADER + FIL_ADDR_BYTE
				      + FIL_ADDR_SIZE
				      || offs == PAGE_BTR_SEG_LEAF
				      + PAGE_HEADER + FSEG_HDR_OFFSET
				      || offs == PAGE_BTR_SEG_TOP
				      + PAGE_HEADER + FSEG_HDR_OFFSET
				      || offs == PAGE_BTR_IBUF_FREE_LIST_NODE
				      + PAGE_HEADER + FIL_ADDR_BYTE
				      + 0 /*FLST_PREV*/
				      || offs == PAGE_BTR_IBUF_FREE_LIST_NODE
				      + PAGE_HEADER + FIL_ADDR_BYTE
				      + FIL_ADDR_SIZE /*FLST_NEXT*/);
				break;
			case MLOG_4BYTES:
				/* Note that this can fail when the
				redo log been written with something
				older than InnoDB Plugin 1.0.4. */
				ut_ad(0
				      || offs == IBUF_TREE_SEG_HEADER
				      + IBUF_HEADER + FSEG_HDR_SPACE
				      || offs == IBUF_TREE_SEG_HEADER
				      + IBUF_HEADER + FSEG_HDR_PAGE_NO
				      || offs == PAGE_BTR_IBUF_FREE_LIST
				      + PAGE_HEADER/* flst_init */
				      || offs == PAGE_BTR_IBUF_FREE_LIST
				      + PAGE_HEADER + FIL_ADDR_PAGE
				      || offs == PAGE_BTR_IBUF_FREE_LIST
				      + PAGE_HEADER + FIL_ADDR_PAGE
				      + FIL_ADDR_SIZE
				      || offs == PAGE_BTR_SEG_LEAF
				      + PAGE_HEADER + FSEG_HDR_PAGE_NO
				      || offs == PAGE_BTR_SEG_LEAF
				      + PAGE_HEADER + FSEG_HDR_SPACE
				      || offs == PAGE_BTR_SEG_TOP
				      + PAGE_HEADER + FSEG_HDR_PAGE_NO
				      || offs == PAGE_BTR_SEG_TOP
				      + PAGE_HEADER + FSEG_HDR_SPACE
				      || offs == PAGE_BTR_IBUF_FREE_LIST_NODE
				      + PAGE_HEADER + FIL_ADDR_PAGE
				      + 0 /*FLST_PREV*/
				      || offs == PAGE_BTR_IBUF_FREE_LIST_NODE
				      + PAGE_HEADER + FIL_ADDR_PAGE
				      + FIL_ADDR_SIZE /*FLST_NEXT*/);
				break;
			}
		}
#endif /* UNIV_DEBUG */
		ptr = mlog_parse_nbytes(type, ptr, end_ptr, page, page_zip);
		break;
	case MLOG_REC_INSERT: case MLOG_COMP_REC_INSERT:
		ut_ad(!page || page_type == FIL_PAGE_INDEX);

		if (NULL != (ptr = mlog_parse_index(
				     ptr, end_ptr,
				     type == MLOG_COMP_REC_INSERT,
				     &index))) {
			ut_a(!page
			     || (bool)!!page_is_comp(page)
			     == dict_table_is_comp(index->table));

			ptr = page_cur_parse_insert_rec(
				false, ptr, end_ptr, block, index, mtr);
		}
		break;
	case MLOG_REC_CLUST_DELETE_MARK: case MLOG_COMP_REC_CLUST_DELETE_MARK:
		ut_ad(!page || page_type == FIL_PAGE_INDEX);

		if (NULL != (ptr = mlog_parse_index(
				     ptr, end_ptr,
				     type == MLOG_COMP_REC_CLUST_DELETE_MARK,
				     &index))) {
			ut_a(!page
			     || (bool)!!page_is_comp(page)
			     == dict_table_is_comp(index->table));
			ptr = btr_cur_parse_del_mark_set_clust_rec(
				ptr, end_ptr, page, page_zip, index);
		}
		break;
	case MLOG_COMP_REC_SEC_DELETE_MARK:
		ut_ad(!page || page_type == FIL_PAGE_INDEX);
		/* This log record type is obsolete, but we process it for
		backward compatibility with MySQL 5.0.3 and 5.0.4. */
		ut_a(!page || page_is_comp(page));
		ut_a(!page_zip);
		ptr = mlog_parse_index(ptr, end_ptr, true, &index);
		if (!ptr) {
			break;
		}
		/* Fall through */
	case MLOG_REC_SEC_DELETE_MARK:
		ut_ad(!page || page_type == FIL_PAGE_INDEX);
		ptr = btr_cur_parse_del_mark_set_sec_rec(ptr, end_ptr,
							 page, page_zip);
		break;
	case MLOG_REC_UPDATE_IN_PLACE: case MLOG_COMP_REC_UPDATE_IN_PLACE:
		ut_ad(!page || page_type == FIL_PAGE_INDEX);

		if (NULL != (ptr = mlog_parse_index(
				     ptr, end_ptr,
				     type == MLOG_COMP_REC_UPDATE_IN_PLACE,
				     &index))) {
			ut_a(!page
			     || (bool)!!page_is_comp(page)
			     == dict_table_is_comp(index->table));
			ptr = btr_cur_parse_update_in_place(ptr, end_ptr, page,
							    page_zip, index);
		}
		break;
	case MLOG_LIST_END_DELETE: case MLOG_COMP_LIST_END_DELETE:
	case MLOG_LIST_START_DELETE: case MLOG_COMP_LIST_START_DELETE:
		ut_ad(!page || page_type == FIL_PAGE_INDEX);

		if (NULL != (ptr = mlog_parse_index(
				     ptr, end_ptr,
				     type == MLOG_COMP_LIST_END_DELETE
				     || type == MLOG_COMP_LIST_START_DELETE,
				     &index))) {
			ut_a(!page
			     || (bool)!!page_is_comp(page)
			     == dict_table_is_comp(index->table));
			ptr = page_parse_delete_rec_list(type, ptr, end_ptr,
							 block, index, mtr);
		}
		break;
	case MLOG_LIST_END_COPY_CREATED: case MLOG_COMP_LIST_END_COPY_CREATED:
		ut_ad(!page || page_type == FIL_PAGE_INDEX);

		if (NULL != (ptr = mlog_parse_index(
				     ptr, end_ptr,
				     type == MLOG_COMP_LIST_END_COPY_CREATED,
				     &index))) {
			ut_a(!page
			     || (bool)!!page_is_comp(page)
			     == dict_table_is_comp(index->table));
			ptr = page_parse_copy_rec_list_to_created_page(
				ptr, end_ptr, block, index, mtr);
		}
		break;
	case MLOG_PAGE_REORGANIZE:
	case MLOG_COMP_PAGE_REORGANIZE:
	case MLOG_ZIP_PAGE_REORGANIZE:
		ut_ad(!page || page_type == FIL_PAGE_INDEX);

		if (NULL != (ptr = mlog_parse_index(
				     ptr, end_ptr,
				     type != MLOG_PAGE_REORGANIZE,
				     &index))) {
			ut_a(!page
			     || (bool)!!page_is_comp(page)
			     == dict_table_is_comp(index->table));
			ptr = btr_parse_page_reorganize(
				ptr, end_ptr, index,
				type == MLOG_ZIP_PAGE_REORGANIZE,
				block, mtr);
		}
		break;
	case MLOG_PAGE_CREATE: case MLOG_COMP_PAGE_CREATE:
		/* Allow anything in page_type when creating a page. */
		ut_a(!page_zip);
		ptr = page_parse_create(ptr, end_ptr,
					type == MLOG_COMP_PAGE_CREATE,
					block, mtr);
		break;
	case MLOG_UNDO_INSERT:
		ut_ad(!page || page_type == FIL_PAGE_UNDO_LOG);
		ptr = trx_undo_parse_add_undo_rec(ptr, end_ptr, page);
		break;
	case MLOG_UNDO_ERASE_END:
		ut_ad(!page || page_type == FIL_PAGE_UNDO_LOG);
		ptr = trx_undo_parse_erase_page_end(ptr, end_ptr, page, mtr);
		break;
	case MLOG_UNDO_INIT:
		/* Allow anything in page_type when creating a page. */
		ptr = trx_undo_parse_page_init(ptr, end_ptr, page, mtr);
		break;
	case MLOG_UNDO_HDR_DISCARD:
		ut_ad(!page || page_type == FIL_PAGE_UNDO_LOG);
		ptr = trx_undo_parse_discard_latest(ptr, end_ptr, page, mtr);
		break;
	case MLOG_UNDO_HDR_CREATE:
	case MLOG_UNDO_HDR_REUSE:
		ut_ad(!page || page_type == FIL_PAGE_UNDO_LOG);
		ptr = trx_undo_parse_page_header(type, ptr, end_ptr,
						 page, mtr);
		break;
	case MLOG_REC_MIN_MARK: case MLOG_COMP_REC_MIN_MARK:
		ut_ad(!page || page_type == FIL_PAGE_INDEX);
		/* On a compressed page, MLOG_COMP_REC_MIN_MARK
		will be followed by MLOG_COMP_REC_DELETE
		or MLOG_ZIP_WRITE_HEADER(FIL_PAGE_PREV, FIL_NULL)
		in the same mini-transaction. */
		ut_a(type == MLOG_COMP_REC_MIN_MARK || !page_zip);
		ptr = btr_parse_set_min_rec_mark(
			ptr, end_ptr, type == MLOG_COMP_REC_MIN_MARK,
			page, mtr);
		break;
	case MLOG_REC_DELETE: case MLOG_COMP_REC_DELETE:
		ut_ad(!page || page_type == FIL_PAGE_INDEX);

		if (NULL != (ptr = mlog_parse_index(
				     ptr, end_ptr,
				     type == MLOG_COMP_REC_DELETE,
				     &index))) {
			ut_a(!page
			     || (bool)!!page_is_comp(page)
			     == dict_table_is_comp(index->table));
			ptr = page_cur_parse_delete_rec(ptr, end_ptr,
							block, index, mtr);
		}
		break;
	case MLOG_IBUF_BITMAP_INIT:
		/* Allow anything in page_type when creating a page. */
		ptr = ibuf_parse_bitmap_init(ptr, end_ptr, block, mtr);
		break;
	case MLOG_INIT_FILE_PAGE:
		/* Allow anything in page_type when creating a page. */
		ptr = fsp_parse_init_file_page(ptr, end_ptr, block);
		break;
	case MLOG_WRITE_STRING:
		ut_ad(!page || page_type != FIL_PAGE_TYPE_ALLOCATED);
		ptr = mlog_parse_string(ptr, end_ptr, page, page_zip);
		break;
	case MLOG_FILE_RENAME:
		ptr = fil_op_log_parse_or_replay(
			ptr, end_ptr, type, space_id, page_no, 0, false);
		break;
	case MLOG_FILE_CREATE:
	case MLOG_FILE_DELETE:
	case MLOG_FILE_CREATE2:
		ptr = fil_op_log_parse_or_replay(
			ptr, end_ptr, type, ULINT_UNDEFINED, page_no, 0, true);
		break;
	case MLOG_ZIP_WRITE_NODE_PTR:
		ut_ad(!page || page_type == FIL_PAGE_INDEX);
		ptr = page_zip_parse_write_node_ptr(ptr, end_ptr,
						    page, page_zip);
		break;
	case MLOG_ZIP_WRITE_BLOB_PTR:
		ut_ad(!page || page_type == FIL_PAGE_INDEX);
		ptr = page_zip_parse_write_blob_ptr(ptr, end_ptr,
						    page, page_zip);
		break;
	case MLOG_ZIP_WRITE_HEADER:
		ut_ad(!page || page_type == FIL_PAGE_INDEX);
		ptr = page_zip_parse_write_header(ptr, end_ptr,
						  page, page_zip);
		break;
	case MLOG_ZIP_PAGE_COMPRESS:
		/* Allow anything in page_type when creating a page. */
		ptr = page_zip_parse_compress(ptr, end_ptr,
					      page, page_zip);
		break;
	case MLOG_ZIP_PAGE_COMPRESS_NO_DATA:
		if (NULL != (ptr = mlog_parse_index(
				ptr, end_ptr, true, &index))) {

			ut_a(!page || ((bool)!!page_is_comp(page)
				== dict_table_is_comp(index->table)));
			ptr = page_zip_parse_compress_no_data(
				ptr, end_ptr, page, page_zip, index);
		}
		break;

	case MLOG_SINGLE_REC_FLAG:
	case MLOG_MULTI_REC_END:
	case MLOG_DUMMY_RECORD:
		// FIXME: Why aren't these handled here?
		ptr = NULL;
		m_found_corrupt_log = true;
	}

	if (index) {
		dict_table_t*	table = index->table;

		dict_mem_index_free(index);
		dict_mem_table_free(table);
	}

	return(ptr);
}

/**
Gets the hashed file address struct for a page.
@param space		space id
@param page_no		page number
@return	file address struct, NULL if not found from the hash table */
recv_addr_t*
redo_recover_t::get_fil_addr_struct(ulint space, ulint	page_no)
{
	recv_addr_t*	recv_addr;

	for (recv_addr = static_cast<recv_addr_t*>(
			HASH_GET_FIRST(m_addr_hash,
				       hash(space, page_no)));
	     recv_addr != 0;
	     recv_addr = static_cast<recv_addr_t*>(
		     HASH_GET_NEXT(addr_hash, recv_addr))) {

		if (recv_addr->space == space
		    && recv_addr->page_no == page_no) {

			return(recv_addr);
		}
	}

	return(NULL);
}

/**
Adds a new log record to the hash table of log records.
@param type		log record type
@param space		space id
@param page_no		page number
@param body		log record body
@param rec_end		log record end
@param start_lsn	start lsn of the mtr
@param end_lsn		end lsn of the mtr */
void
redo_recover_t::add_to_hash_table(
	mlog_id_t	type,
	ulint		space,
	ulint		page_no,
	byte*		body,
	byte*		rec_end,
	lsn_t		start_lsn,
	lsn_t		end_lsn)
{
	recv_t*		recv;
	ulint		len;
	recv_data_t*	recv_data;
	recv_data_t**	prev_field;
	recv_addr_t*	recv_addr;

	if (fil_tablespace_deleted_or_being_deleted_in_mem(space, -1)) {
		/* The tablespace does not exist any more: do not store the
		log record */

		return;
	}

	len = rec_end - body;

	recv = static_cast<recv_t*>(
		mem_heap_alloc(m_heap, sizeof(*recv)));

	recv->type = type;
	recv->len = rec_end - body;
	recv->start_lsn = start_lsn;
	recv->end_lsn = end_lsn;

	recv_addr = get_fil_addr_struct(space, page_no);

	if (recv_addr == NULL) {
		recv_addr = static_cast<recv_addr_t*>(
			mem_heap_alloc(m_heap, sizeof(recv_addr_t)));

		recv_addr->space = space;
		recv_addr->page_no = page_no;
		recv_addr->state = RECV_NOT_PROCESSED;

		UT_LIST_INIT(recv_addr->rec_list, &recv_t::rec_list);

		HASH_INSERT(recv_addr_t, addr_hash, m_addr_hash,
			    fold(space, page_no), recv_addr);
		m_n_addrs++;
	}

	UT_LIST_ADD_LAST(recv_addr->rec_list, recv);

	prev_field = &recv->data;

	/* Store the log record body in chunks of less than UNIV_PAGE_SIZE:
	m_heap grows into the buffer pool, and bigger chunks could not
	be allocated */

	while (rec_end > body) {

		len = rec_end - body;

		if (len > RECV_DATA_BLOCK_SIZE) {
			len = RECV_DATA_BLOCK_SIZE;
		}

		recv_data = static_cast<recv_data_t*>(
			mem_heap_alloc(m_heap,
				       sizeof(recv_data_t) + len));

		*prev_field = recv_data;

		memcpy(recv_data + 1, body, len);

		prev_field = &(recv_data->next);

		body += len;
	}

	*prev_field = NULL;
}

/**
Copies the log record body from recv to buf.
@param buf	buffer of length at least recv->len
@param recv	log record */
void
redo_recover_t::data_copy_to_buf(byte* buf, recv_t* recv)
{
	recv_data_t*	recv_data;
	ulint		part_len;
	ulint		len;

	len = recv->len;
	recv_data = recv->data;

	while (len > 0) {
		if (len > RECV_DATA_BLOCK_SIZE) {
			part_len = RECV_DATA_BLOCK_SIZE;
		} else {
			part_len = len;
		}

		ut_memcpy(buf, ((byte*) recv_data) + sizeof(recv_data_t),
			  part_len);
		buf += part_len;
		len -= part_len;

		recv_data = recv_data->next;
	}
}

/**
Applies the hashed log records to the page, if the page lsn is
less than the lsn of a log record. This can be called when a
buffer page has just been read in, or also for a page already
in the buffer pool.

@param just_read_in	true if the i/o handler calls this for
			a freshly read page
@param block		buffer block */
void
redo_recover_t::recover_page(
#ifndef UNIV_HOTBACKUP
	bool		just_read_in,
#endif /* !UNIV_HOTBACKUP */
	buf_block_t*	block)
{
	page_t*		page;
	page_zip_des_t*	page_zip;
	recv_addr_t*	recv_addr;
	recv_t*		recv;
	byte*		buf;
	lsn_t		start_lsn;
	lsn_t		end_lsn;
	lsn_t		page_lsn;
	lsn_t		page_newest_lsn;
	bool		modification_to_page;
#ifndef UNIV_HOTBACKUP
	bool		success;
#endif /* !UNIV_HOTBACKUP */
	mtr_t		mtr;

	mutex_enter(&m_mutex);

	if (!m_apply_log_recs) {

		/* Log records should not be applied now */

		mutex_exit(&m_mutex);

		return;
	}

	recv_addr = get_fil_addr_struct(
		buf_block_get_space(block), buf_block_get_page_no(block));

	if ((recv_addr == NULL)
	    || (recv_addr->state == RECV_BEING_PROCESSED)
	    || (recv_addr->state == RECV_PROCESSED)) {

		mutex_exit(&m_mutex);

		return;
	}

	recv_addr->state = RECV_BEING_PROCESSED;

	mutex_exit(&m_mutex);

	mtr_start(&mtr);
	mtr_set_log_mode(&mtr, MTR_LOG_NONE);

	page = block->frame;
	page_zip = buf_block_get_page_zip(block);

#ifndef UNIV_HOTBACKUP
	if (just_read_in) {
		/* Move the ownership of the x-latch on the page to
		this OS thread, so that we can acquire a second
		x-latch on it.  This is needed for the operations to
		the page to pass the debug checks. */

		rw_lock_x_lock_move_ownership(&block->lock);
	}

	success = buf_page_get_known_nowait(
		RW_X_LATCH, block, BUF_KEEP_OLD,
		__FILE__, __LINE__, &mtr);

	ut_a(success);

	buf_block_dbg_add_level(block, SYNC_NO_ORDER_CHECK);
#endif /* !UNIV_HOTBACKUP */

	/* Read the newest modification lsn from the page */
	page_lsn = mach_read_from_8(page + FIL_PAGE_LSN);

#ifndef UNIV_HOTBACKUP
	/* It may be that the page has been modified in the buffer
	pool: read the newest modification lsn there */

	page_newest_lsn = buf_page_get_newest_modification(&block->page);

	if (page_newest_lsn) {

		page_lsn = page_newest_lsn;
	}
#else /* !UNIV_HOTBACKUP */
	/* In recovery from a backup we do not really use the buffer pool */
	page_newest_lsn = 0;
#endif /* !UNIV_HOTBACKUP */

	modification_to_page = false;
	start_lsn = end_lsn = 0;

	recv = UT_LIST_GET_FIRST(recv_addr->rec_list);

	while (recv) {
		end_lsn = recv->end_lsn;

		if (recv->len > RECV_DATA_BLOCK_SIZE) {
			/* We have to copy the record body to a separate
			buffer */

			buf = static_cast<byte*>(ut_malloc(recv->len));

			data_copy_to_buf(buf, recv);
		} else {
			buf = ((byte*)(recv->data)) + sizeof(recv_data_t);
		}

		if (recv->type == MLOG_INIT_FILE_PAGE) {
			page_lsn = page_newest_lsn;

			memset(FIL_PAGE_LSN + page, 0, 8);
			memset(UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM
			       + page, 0, 8);

			if (page_zip) {
				memset(FIL_PAGE_LSN + page_zip->data, 0, 8);
			}
		}

		/* Ignore applying the redo logs for tablespace that is
		truncated. Post recovery there is fixup action that will
		restore the tablespace back to normal state.
		Applying redo at this stage can result in error given that
		redo will have action recorded on page before tablespace
		was re-inited and that would lead to an error while applying
		such action. */
		if (recv->start_lsn >= page_lsn
		    && !srv_is_tablespace_truncated(recv_addr->space)) {

			lsn_t	end_lsn;

			if (!modification_to_page) {

				modification_to_page = true;
				start_lsn = recv->start_lsn;
			}

			DBUG_PRINT("ib_log",
				   ("apply " LSN_PF ": %u len %u "
				    "page %u:%u", recv->start_lsn,
				    (unsigned) recv->type,
				    (unsigned) recv->len,
				    (unsigned) recv_addr->space,
				    (unsigned) recv_addr->page_no));

			parse_or_apply_log_rec_body(
				recv->type, buf, buf + recv->len,
				block, &mtr,
				recv_addr->space,
				recv_addr->page_no);

			end_lsn = recv->start_lsn + recv->len;
			mach_write_to_8(FIL_PAGE_LSN + page, end_lsn);
			mach_write_to_8(UNIV_PAGE_SIZE
					- FIL_PAGE_END_LSN_OLD_CHKSUM
					+ page, end_lsn);

			if (page_zip) {
				mach_write_to_8(FIL_PAGE_LSN
						+ page_zip->data, end_lsn);
			}
		}

		if (recv->len > RECV_DATA_BLOCK_SIZE) {
			ut_free(buf);
		}

		recv = UT_LIST_GET_NEXT(rec_list, recv);
	}

#ifdef UNIV_ZIP_DEBUG
	if (fil_page_get_type(page) == FIL_PAGE_INDEX) {
		page_zip_des_t*	page_zip = buf_block_get_page_zip(block);

		ut_a(!page_zip
		     || page_zip_validate_low(page_zip, page, NULL, false));
	}
#endif /* UNIV_ZIP_DEBUG */

#ifndef UNIV_HOTBACKUP
	if (modification_to_page) {
		redo_log->flush_order_mutex_enter();

		buf_flush_recv_note_modification(block, start_lsn, end_lsn);

		redo_log->flush_order_mutex_exit();
	}
#endif /* !UNIV_HOTBACKUP */

	/* Make sure that committing mtr does not change the modification
	lsn values of page */

	mtr.discard_modifications();

	mtr.commit();

	mutex_enter(&m_mutex);

	if (m_max_page_lsn < page_lsn) {
		m_max_page_lsn = page_lsn;
	}

	recv_addr->state = RECV_PROCESSED;

	ut_a(m_n_addrs > 0);
	--m_n_addrs;

	mutex_exit(&m_mutex);
}

#ifndef UNIV_HOTBACKUP
/**
Reads in pages which have hashed log records, from an area around
a given page number.

@param space		space
@param zip_size		compressed page size in bytes, or 0
@param page_no		page number

@return	number of pages found */
ulint
redo_recover_t::read_in_area(ulint space, ulint zip_size, ulint page_no)
{
	recv_addr_t*	recv_addr;
	ulint		page_nos[RECV_READ_AHEAD_AREA];
	ulint		low_limit;
	low_limit = page_no - (page_no % RECV_READ_AHEAD_AREA);

	ulint		n = 0;

	for (page_no = low_limit; page_no < low_limit + RECV_READ_AHEAD_AREA;
	     page_no++) {
		recv_addr = get_fil_addr_struct(space, page_no);

		if (recv_addr && !buf_page_peek(space, page_no)) {

			mutex_enter(&m_mutex);

			if (recv_addr->state == RECV_NOT_PROCESSED) {
				recv_addr->state = RECV_BEING_READ;

				page_nos[n] = page_no;

				n++;
			}

			mutex_exit(&m_mutex);
		}
	}

	buf_read_recv_pages(false, space, zip_size, page_nos, n);

	return(n);
}

/**
Empties the hash table of stored log records, applying them
to appropriate pages

@param allow_ibuf	if true, also ibuf operations are
			allowed during the application; if FALSE,
			no ibuf operations are allowed, and after
			the application all file pages are flushed to
			disk and invalidated in buffer pool: this
			alternative means that no new log records
			can be generated during the application */
void
redo_recover_t::apply_hashed_log_recs(bool allow_ibuf)
{
	mtr_t		mtr;
	recv_addr_t*	recv_addr;
	bool		has_printed = false;

	for (;;) {
		mutex_enter(&m_mutex);

		if (m_apply_batch_on) {

			mutex_exit(&m_mutex);

			os_thread_sleep(500000);
		} else {
			break;
		}
	}

	ut_ad(!allow_ibuf == m_redo->is_mutex_owned());

	m_apply_log_recs = true;
	m_apply_batch_on = true;

	for (ulint i = 0; i < hash_get_n_cells(m_addr_hash); i++) {

		for (recv_addr = static_cast<recv_addr_t*>(
				HASH_GET_FIRST(m_addr_hash, i));
		     recv_addr != 0;
		     recv_addr = static_cast<recv_addr_t*>(
				HASH_GET_NEXT(addr_hash, recv_addr))) {

			ulint	space = recv_addr->space;
			ulint	zip_size = fil_space_get_zip_size(space);
			ulint	page_no = recv_addr->page_no;

			if (recv_addr->state == RECV_NOT_PROCESSED) {

				if (!has_printed) {
					ib_logf(IB_LOG_LEVEL_INFO,
						"Starting an apply batch"
						" of log records"
						" to the database...");
					ib_logf(IB_LOG_LEVEL_INFO,
						"Progress in percent: ");

					has_printed = true;
				}

				mutex_exit(&m_mutex);

				if (buf_page_peek(space, page_no)) {
					buf_block_t*	block;

					mtr_start(&mtr);

					block = buf_page_get(
						space, zip_size, page_no,
						RW_X_LATCH, &mtr);

					buf_block_dbg_add_level(
						block, SYNC_NO_ORDER_CHECK);

					recover_page(false, block);

					mtr_commit(&mtr);
				} else {
					read_in_area(space, zip_size, page_no);
				}

				mutex_enter(&m_mutex);
			}
		}

		if (has_printed
		    && (i * 100) / hash_get_n_cells(m_addr_hash)
		    != ((i + 1) * 100)
		    / hash_get_n_cells(m_addr_hash)) {

			fprintf(stderr, "%lu ", (ulong)
				((i * 100)
				 / hash_get_n_cells(m_addr_hash)));
		}
	}

	/* Wait until all the pages have been processed */

	while (m_n_addrs != 0) {

		mutex_exit(&m_mutex);

		os_thread_sleep(500000);

		mutex_enter(&m_mutex);
	}

	if (has_printed) {

		fprintf(stderr, "\n");
	}

	if (!allow_ibuf) {

		/* Flush all the file pages to disk and invalidate them in
		the buffer pool */

		ut_d(m_redo->disable_log_write());

		mutex_exit(&m_mutex);

		m_redo->mutex_release();

		/* Stop the recv_writer thread from issuing any LRU
		flush batches. */
		mutex_enter(&m_writer_mutex);

		/* Wait for any currently run batch to end. */
		buf_flush_wait_LRU_batch_end();

		buf_flush_sync_all_buf_pools();

		buf_pool_invalidate();

		/* Allow batches from recv_writer thread. */
		mutex_exit(&m_writer_mutex);

		m_redo->mutex_acquire();

		mutex_enter(&m_mutex);

		ut_d(m_redo->enable_log_write());
	}

	m_apply_log_recs = false;
	m_apply_batch_on = false;

	empty_hash();

	if (has_printed) {
		ib_logf(IB_LOG_LEVEL_INFO, "Apply batch completed");
	}

	mutex_exit(&m_mutex);
}

#else /* !UNIV_HOTBACKUP */
/**
Applies log records in the hash table to a backup. */
void
redo_recover_t::apply_log_recs_for_backup()
{
	recv_addr_t*	recv_addr;
	ulint		n_hash_cells;
	buf_block_t*	block;
	ulint		actual_size;
	bool		success;
	ulint		error;

	m_apply_log_recs = true;
	m_apply_batch_on = true;

	block = back_block1;

	ib_logf(IB_LOG_LEVEL_INFO,
		"Starting an apply batch of log records to the database...");

	fputs("InnoDB: Progress in percent: ", stderr);

	n_hash_cells = hash_get_n_cells(m_addr_hash);

	for (ulint i = 0; i < n_hash_cells; i++) {
		/* The address hash table is externally chained */

		recv_addr = hash_get_nth_cell(m_addr_hash, i)->node;

		while (recv_addr != NULL) {

			ulint	zip_size
				= fil_space_get_zip_size(recv_addr->space);

			if (zip_size == ULINT_UNDEFINED) {
				recv_addr->state = RECV_PROCESSED;

				ut_a(m_n_addrs);
				m_n_addrs--;

				goto skip_this_recv_addr;
			}

			/* We simulate a page read made by the buffer pool, to
			make sure the recovery apparatus works ok. We must init
			the block. */

			buf_page_init_for_backup_restore(
				recv_addr->space, recv_addr->page_no,
				zip_size, block);

			/* Extend the tablespace's last file if the page_no
			does not fall inside its bounds; we assume the last
			file is auto-extending, and ibbackup copied the file
			when it still was smaller */

			success = fil_extend_space_to_desired_size(
				&actual_size,
				recv_addr->space, recv_addr->page_no + 1);
			if (!success) {
				ib_logf(IB_LOG_LEVEL_FATAL,
					"Cannot extend tablespace %u"
					" to hold %u pages",
					recv_addr->space, recv_addr->page_no);
			}

			/* Read the page from the tablespace file using the
			fil0fil.cc routines */

			if (zip_size) {
				error = fil_io(OS_FILE_READ, true,
					       recv_addr->space, zip_size,
					       recv_addr->page_no, 0, zip_size,
					       block->page.zip.data, NULL);
				if (error == DB_SUCCESS
				    && !buf_zip_decompress(block, true)) {
					ut_error;
				}
			} else {
				error = fil_io(OS_FILE_READ, true,
					       recv_addr->space, 0,
					       recv_addr->page_no, 0,
					       UNIV_PAGE_SIZE,
					       block->frame, NULL);
			}

			if (error != DB_SUCCESS) {
				ib_logf(IB_LOG_LEVEL_FATAL,
					"Cannot read from tablespace"
					" %lu page number %lu",
					(ulong) recv_addr->space,
					(ulong) recv_addr->page_no);
			}

			/* Apply the log records to this page */
			recv_recover_page(false, block);

			/* Write the page back to the tablespace file using the
			fil0fil.cc routines */

			buf_flush_init_for_writing(
				block->frame, buf_block_get_page_zip(block),
				mach_read_from_8(block->frame + FIL_PAGE_LSN));

			if (zip_size) {
				error = fil_io(OS_FILE_WRITE, true,
					       recv_addr->space, zip_size,
					       recv_addr->page_no, 0,
					       zip_size,
					       block->page.zip.data, NULL);
			} else {
				error = fil_io(OS_FILE_WRITE, true,
					       recv_addr->space, 0,
					       recv_addr->page_no, 0,
					       UNIV_PAGE_SIZE,
					       block->frame, NULL);
			}
skip_this_recv_addr:
			recv_addr = HASH_GET_NEXT(addr_hash, recv_addr);
		}

		if ((100 * i) / n_hash_cells
		    != (100 * (i + 1)) / n_hash_cells) {
			fprintf(stderr, "%lu ",
				(ulong) ((100 * i) / n_hash_cells));
			fflush(stderr);
		}
	}

	empty_hash();
}
#endif /* !UNIV_HOTBACKUP */

/**
Tries to parse a single log record and returns its length.
@param ptr		pointer to a buffer
@param end_ptr		pointer to the buffer end
@param type		type
@param space		space id
@param page_no		page number
@param body		log record body start
@return	length of the record, or 0 if the record was not complete */
ulint
redo_recover_t::parse_log_rec(
	byte*		ptr,
	byte*		end_ptr,
	mlog_id_t*	type,
	ulint*		space,
	ulint*		page_no,
	byte**		body)
{
	*body = NULL;

	if (ptr == end_ptr) {

		return(0);
	}

	if (*ptr == MLOG_MULTI_REC_END) {

		*type = static_cast<mlog_id_t>(*ptr);

		return(1);
	} else if (*ptr == MLOG_DUMMY_RECORD) {

		*type = static_cast<mlog_id_t>(*ptr);

		/* For debugging */
		*space = ULINT_UNDEFINED - 1;

		return(1);
	}

	byte*	new_ptr;

	new_ptr = mlog_parse_initial_log_record(
		ptr, end_ptr, type, space, page_no);

	*body = new_ptr;

	if (!new_ptr) {

		return(0);
	}

#ifdef UNIV_LOG_LSN_DEBUG
	if (*type == MLOG_LSN) {
		lsn_t	lsn = (lsn_t) *space << 32 | *page_no;
		ut_a(lsn == m_recovered_lsn);
	}
#endif /* UNIV_LOG_LSN_DEBUG */

	new_ptr = parse_or_apply_log_rec_body(
		*type, new_ptr, end_ptr, NULL, NULL, *space, *page_no);

	if (new_ptr == NULL) {

		return(0);

	} else if (*page_no > m_max_parsed_page_no) {
		m_max_parsed_page_no = *page_no;
	}

	return(new_ptr - ptr);
}

/**
Calculates the new value for lsn when more data is added to the log.
@param lsn		old lsn
@param len		this many bytes of data is added, log block
			headers not included */
lsn_t
redo_recover_t::calc_lsn_on_data_add(lsn_t lsn, ib_uint64_t len)
{
	ulint		frag_len;
	ib_uint64_t	lsn_len;

	frag_len = (lsn % OS_FILE_LOG_BLOCK_SIZE) - RedoLog::BLOCK_HDR_SIZE;

	ut_ad(frag_len < OS_FILE_LOG_BLOCK_SIZE - RedoLog::BLOCK_HDR_SIZE
	      - RedoLog::TRAILER_SIZE);

	lsn_len = len;

	lsn_len += (lsn_len + frag_len)
		/ (OS_FILE_LOG_BLOCK_SIZE - RedoLog::BLOCK_HDR_SIZE
		   - RedoLog::TRAILER_SIZE)
		* (RedoLog::BLOCK_HDR_SIZE + RedoLog::TRAILER_SIZE);

	return(lsn + lsn_len);
}

/**
Prints diagnostic info of corrupt log.
@param ptr		pointer to corrupt log record
@param type,		type of the record
@param space		space id, this may also be garbage
@param page_no		page number, this may also be garbage */
void
redo_recover_t::report_corrupt_log(
	byte*		ptr,
	mlog_id_t	type,
	ulint		space,
	ulint		page_no)
{
	fprintf(stderr,
		"InnoDB: ############### CORRUPT LOG RECORD FOUND\n"
		"InnoDB: Log record type %lu, space id %lu, page number %lu\n"
		"InnoDB: Log parsing proceeded successfully up to " LSN_PF "\n"
		"InnoDB: Previous log record type %lu, is multi %lu\n"
		"InnoDB: Recv offset %lu, prev %lu\n",
		(ulong) type, (ulong) space, (ulong) page_no,
		m_recovered_lsn,
		(ulong) m_previous_parsed_rec_type,
		(ulong) m_previous_parsed_rec_is_multi,
		(ulong) (ptr - m_buf),
		(ulong) m_previous_parsed_rec_offset);

	if ((ulint)(ptr - m_buf + 100) > m_previous_parsed_rec_offset
	    && (ulint)(ptr - m_buf + 100 - m_previous_parsed_rec_offset)
	    < 200000) {
		fputs("InnoDB: Hex dump of corrupt log starting"
		      " 100 bytes before the start\n"
		      "InnoDB: of the previous log rec,\n"
		      "InnoDB: and ending 100 bytes after the start"
		      " of the corrupt rec:\n",
		      stderr);

		ut_print_buf(stderr,
			     m_buf
			     + m_previous_parsed_rec_offset - 100,
			     ptr - m_buf + 200
			     - m_previous_parsed_rec_offset);
		putc('\n', stderr);
	}

#ifndef UNIV_HOTBACKUP
	if (!srv_force_recovery) {
		ib_logf(IB_LOG_LEVEL_FATAL,
			"Set innodb_force_recovery to ignore this error.");
	}
#endif /* !UNIV_HOTBACKUP */

	ib_logf(IB_LOG_LEVEL_WARN,
		"The log file may have been corrupt and it is possible"
		" that the log scan did not proceed far enough in recovery!"
		" Please run CHECK TABLE on your InnoDB tables to check"
		" that they are ok! If mysqld crashes after this recovery,"
		" look at " REFMAN "forcing-innodb-recovery.html"
		" about forcing recovery.");

	fflush(stderr);
}

/**
Parses log records from a buffer and stores them to a hash table to wait
merging to file pages.

@param store_to_hash	true if the records should be stored to the
			hash table; this is set to false if just debug
			checking is needed

@return	currently always returns false */
bool
redo_recover_t::parse_log_recs(bool store_to_hash)
{
	byte*		ptr;
	byte*		end_ptr;
	ulint		single_rec;
	ulint		len;
	ulint		total_len;
	lsn_t		new_recovered_lsn;
	lsn_t		old_lsn;
	mlog_id_t	type;
	ulint		space;
	ulint		page_no;
	byte*		body;
	ulint		n_recs;

	ut_ad(m_redo->is_mutex_owned());
	ut_ad(m_parse_start_lsn != 0);
loop:
	ptr = m_buf + m_recovered_offset;

	end_ptr = m_buf + m_len;

	if (ptr == end_ptr) {

		return(false);
	}

	single_rec = (ulint)*ptr & MLOG_SINGLE_REC_FLAG;

	if (single_rec || *ptr == MLOG_DUMMY_RECORD) {
		/* The mtr only modified a single page, or this is a file op */

		old_lsn = m_recovered_lsn;

		/* Try to parse a log record, fetching its type, space id,
		page no, and a pointer to the body of the log record */

		len = parse_log_rec(
			ptr, end_ptr, &type, &space, &page_no, &body);

		if (len == 0 || m_found_corrupt_log) {
			if (m_found_corrupt_log) {

				report_corrupt_log(ptr, type, space, page_no);
			}

			return(false);
		}

		new_recovered_lsn = calc_lsn_on_data_add(old_lsn, len);

		if (new_recovered_lsn > m_scanned_lsn) {
			/* The log record filled a log block, and we require
			that also the next log block should have been scanned
			in */

			return(false);
		}

		m_previous_parsed_rec_type = (ulint) type;
		m_previous_parsed_rec_offset = m_recovered_offset;
		m_previous_parsed_rec_is_multi = 0;

		m_recovered_offset += len;
		m_recovered_lsn = new_recovered_lsn;

		DBUG_PRINT("ib_log",
			   ("scan " LSN_PF ": log rec %u len %u "
			    "page %u:%u", old_lsn,
			    (unsigned) type, (unsigned) len,
			    (unsigned) space, (unsigned) page_no));

		if (type == MLOG_DUMMY_RECORD) {
			/* Do nothing */

		} else if (!store_to_hash) {
			/* In debug checking, update a replicate page
			according to the log record, and check that it
			becomes identical with the original page */

		} else if (type == MLOG_FILE_CREATE
			   || type == MLOG_FILE_CREATE2
			   || type == MLOG_FILE_RENAME
			   || type == MLOG_FILE_DELETE) {
			ut_a(space);
#ifdef UNIV_HOTBACKUP
			if (recv_replay_file_ops) {

				/* In ibbackup --apply-log, replay an .ibd file
				operation, if possible; note that
				fil_path_to_mysql_datadir is set in ibbackup to
				point to the datadir we should use there */

				if (NULL == fil_op_log_parse_or_replay(
					    body, end_ptr, type,
					    space, page_no, 0, false)) {

					ib_logf(IB_LOG_LEVEL_FATAL,
						"File op log record of type"
						" %lu space %lu not complete"
						" in the replay phase."
						" Path %s",
						(ulint) type, space,
						(char*)(body + 2));
				}
			}
#endif /* UNIV_HOTBACKUP */
			/* In normal mysqld crash recovery we do not try to
			replay file operations */
#ifdef UNIV_LOG_LSN_DEBUG
		} else if (type == MLOG_LSN) {
			/* Do not add these records to the hash table.
			The page number and space id fields are misused
			for something else. */
#endif /* UNIV_LOG_LSN_DEBUG */
		} else {
			add_to_hash_table(
				type, space, page_no, body,
				ptr + len, old_lsn, m_recovered_lsn);
		}
	} else {
		/* Check that all the records associated with the single mtr
		are included within the buffer */

		total_len = 0;
		n_recs = 0;

		for (;;) {

			len = parse_log_rec(
				ptr, end_ptr, &type, &space, &page_no, &body);

			if (len == 0 || m_found_corrupt_log) {

				if (m_found_corrupt_log) {

					report_corrupt_log(
						ptr, type, space, page_no);
				}

				return(false);
			}

			m_previous_parsed_rec_type = (ulint) type;

			m_previous_parsed_rec_offset
				= m_recovered_offset + total_len;

			m_previous_parsed_rec_is_multi = 1;

			DBUG_PRINT("ib_log",
				   ("scan " LSN_PF ": multi-log rec %u "
				    "len %u page %u:%u",
				    m_recovered_lsn,
				    (unsigned) type, (unsigned) len,
				    (unsigned) space, (unsigned) page_no));

			total_len += len;
			n_recs++;

			ptr += len;

			if (type == MLOG_MULTI_REC_END) {

				/* Found the end mark for the records */

				break;
			}
		}

		new_recovered_lsn = calc_lsn_on_data_add(
			m_recovered_lsn, total_len);

		if (new_recovered_lsn > m_scanned_lsn) {
			/* The log record filled a log block, and we require
			that also the next log block should have been scanned
			in */

			return(false);
		}

		/* Add all the records to the hash table */

		ptr = m_buf + m_recovered_offset;

		for (;;) {
			old_lsn = m_recovered_lsn;

			len = parse_log_rec(
				ptr, end_ptr, &type, &space, &page_no, &body);

			if (m_found_corrupt_log) {

				report_corrupt_log(ptr, type, space, page_no);
			}

			ut_a(len != 0);
			ut_a(0 == ((ulint)*ptr & MLOG_SINGLE_REC_FLAG));

			m_recovered_offset += len;

			m_recovered_lsn
				= calc_lsn_on_data_add(old_lsn, len);

			if (type == MLOG_MULTI_REC_END) {

				/* Found the end mark for the records */

				break;
			}

			if (store_to_hash
#ifdef UNIV_LOG_LSN_DEBUG
			    && type != MLOG_LSN
#endif /* UNIV_LOG_LSN_DEBUG */
			    ) {
				add_to_hash_table(
					type, space, page_no,
					body, ptr + len, old_lsn,
					new_recovered_lsn);
			}

			ptr += len;
		}
	}

	goto loop;
}

/**
Adds data from a new log block to the parsing buffer if m_parse_start_lsn is
non-zero.

@param log_block	log block
@param scanned_lsn	lsn of how far we were able to find data in
			this log block

@return	true if more data added */
bool
redo_recover_t::add_to_parsing_buf(const byte* log_block, lsn_t scanned_lsn)
{
	ulint	more_len;
	ulint	data_len;
	ulint	start_offset;
	ulint	end_offset;

	ut_ad(scanned_lsn >= m_scanned_lsn);

	if (!m_parse_start_lsn) {
		/* Cannot start parsing yet because no start point for
		it found */

		return(false);
	}

	data_len = RedoLog::block_get_data_len(log_block);

	if (m_parse_start_lsn >= scanned_lsn) {

		return(false);

	} else if (m_scanned_lsn >= scanned_lsn) {

		return(false);

	} else if (m_parse_start_lsn > m_scanned_lsn) {

		more_len = (ulint) (scanned_lsn - m_parse_start_lsn);

	} else {
		more_len = (ulint) (scanned_lsn - m_scanned_lsn);
	}

	if (more_len == 0) {

		return(false);
	}

	ut_ad(data_len >= more_len);

	start_offset = data_len - more_len;

	if (start_offset < RedoLog::BLOCK_HDR_SIZE) {
		start_offset = RedoLog::BLOCK_HDR_SIZE;
	}

	end_offset = data_len;

	if (end_offset > OS_FILE_LOG_BLOCK_SIZE - RedoLog::TRAILER_SIZE) {
		end_offset = OS_FILE_LOG_BLOCK_SIZE - RedoLog::TRAILER_SIZE;
	}

	ut_ad(start_offset <= end_offset);

	if (start_offset < end_offset) {

		ut_memcpy(m_buf + m_len,
			  log_block + start_offset, end_offset - start_offset);

		m_len += end_offset - start_offset;

		ut_a(m_len <= s_parsing_buf_size);
	}

	return(true);
}

/**
Moves the parsing buffer data left to the buffer start. */
void
redo_recover_t::justify_left_parsing_buf()
{
	ut_memmove(m_buf,
		   m_buf + m_recovered_offset,
		   m_len - m_recovered_offset);

	m_len -= m_recovered_offset;

	m_recovered_offset = 0;
}

/**
Initialize crash recovery environment. Can be called iff
redo_recover_t::needed_recovery == false. */
void
redo_recover_t::init_crash_recovery()
{
	ut_ad(!srv_read_only_mode);
	ut_a(!m_needed_recovery);

	m_needed_recovery = true;

	ib_logf(IB_LOG_LEVEL_INFO, "Database was not shutdown normally!");
	ib_logf(IB_LOG_LEVEL_INFO, "Starting crash recovery.");

	ib_logf(IB_LOG_LEVEL_INFO,
		"Reading tablespace information from the .ibd files...");

	fil_load_single_table_tablespaces();

	/* If we are using the doublewrite method, we will
	check if there are half-written pages in data files,
	and restore them from the doublewrite buffer if
	possible */

	if (srv_force_recovery < SRV_FORCE_NO_LOG_REDO) {

		ib_logf(IB_LOG_LEVEL_INFO,
			"Restoring possible half-written data pages ");

		ib_logf(IB_LOG_LEVEL_INFO,
			"from the doublewrite buffer...");

		buf_dblwr_init_or_restore_pages(true);

		/* Spawn the background thread to flush dirty pages
		from the buffer pools. */
		os_thread_create(recv_writer_thread, 0, 0);
	}
}

#ifndef UNIV_HOTBACK
/**
Completes recovery from a checkpoint. */
void
redo_recover_t::complete()
{
	/* Apply the hashed log records to the respective file pages */

	if (srv_force_recovery < SRV_FORCE_NO_LOG_REDO) {

		apply_hashed_log_recs(true);
	}

	DBUG_PRINT("ib_log", ("apply completed"));

	if (m_needed_recovery) {
		trx_sys_print_mysql_master_log_pos();
		trx_sys_print_mysql_binlog_offset();
	}

	if (m_found_corrupt_log) {

		ib_logf(IB_LOG_LEVEL_WARN,
			"The log file may have been corrupt and it is possible "
			"that the log scan or parsing did not proceed far "
			"enough in recovery. Please run CHECK TABLE on your "
			"InnoDB tables to check that they are ok! It may be "
			"safest to recover your database from a backup!");
	}

	/* Make sure that the recv_writer thread is done. This is
	required because it grabs various mutexes and we want to
	ensure that when we enable sync_order_checks there is no
	mutex currently held by any thread. */
	mutex_enter(&m_writer_mutex);

	/* By acquring the mutex we ensure that the recv_writer thread
	won't trigger any more LRU batchtes. Now wait for currently
	in progress batches to finish. */
	buf_flush_wait_LRU_batch_end();

	mutex_exit(&m_writer_mutex);
}

/**
Called at end of recovery for the recovery manager to wrap up */
void
redo_recover_t::finish()
{
	ulint	count = 0;

	while (m_writer_thread_active) {

		++count;

		os_thread_sleep(100000);

		if (srv_print_verbose_log && count > 600) {

			ib_logf(IB_LOG_LEVEL_INFO,
				"Waiting for recv_writer to "
				"finish flushing of buffer pool");

			count = 0;
		}
	}

	debug_free();

	/* Roll back any recovered data dictionary transactions, so
	that the data dictionary tables will be free of any locks.
	The data dictionary latch should guarantee that there is at
	most one data dictionary transaction active at a time. */
	if (srv_force_recovery < SRV_FORCE_NO_TRX_UNDO) {
		trx_rollback_or_clean_recovered(false);
	}
}

/**
Initiates the rollback of active transactions. */
void
redo_recover_t::recovery_rollback_active()
{
	ut_ad(!m_writer_thread_active);

	/* We can't start any (DDL) transactions if UNDO logging
	has been disabled, additionally disable ROLLBACK of recovered
	user transactions. */
	if (srv_force_recovery < SRV_FORCE_NO_TRX_UNDO
	    && !srv_read_only_mode) {

		/* Drop partially created indexes. */
		row_merge_drop_temp_indexes();
		/* Drop temporary tables. */
		row_mysql_drop_temp_tables();

		/* Drop any auxiliary tables that were not dropped when the
		parent table was dropped. This can happen if the parent table
		was dropped but the server crashed before the auxiliary tables
		were dropped. */
		fts_drop_orphaned_tables();

		/* Rollback the uncommitted transactions which have no user
		session */

		os_thread_create(trx_rollback_or_clean_all_recovered, 0, 0);
	}
}

#else

/**
Creates new log files after a backup has been restored. */
void
redo_recover_t::reset_log_files_for_backup(
	const char*	log_dir,	/*!< in: log file directory path */
	ulint		n_log_files,	/*!< in: number of log files */
	lsn_t		log_file_size,	/*!< in: log file size */
	lsn_t		lsn)		/*!< in: new start lsn, must be
					divisible by OS_FILE_LOG_BLOCK_SIZE */
{
	os_file_t	log_file;
	bool		success;
	byte*		buf;
	ulint		log_dir_len;
	char		name[5000];
	static const char ib_logfile_basename[] = "ib_logfile";

	log_dir_len = strlen(log_dir);

	/* full path name of ib_logfile consists of log dir path + basename
	+ number. This must fit in the name buffer.  */
	ut_a(log_dir_len + strlen(ib_logfile_basename) + 11  < sizeof(name));

	buf = ut_malloc(LOG_FILE_HDR_SIZE + OS_FILE_LOG_BLOCK_SIZE);
	memset(buf, '\0', LOG_FILE_HDR_SIZE + OS_FILE_LOG_BLOCK_SIZE);

	for (ulint i = 0; i < n_log_files; ++i) {

		ut_snprintf(name, sizeof(name), "%s%s%lu", log_dir,
			    ib_logfile_basename, (ulong) i);

		log_file = os_file_create_simple(
			innodb_log_file_key,
						 name, OS_FILE_CREATE,
						 OS_FILE_READ_WRITE,
						 &success);
		if (!success) {
			ib_logf(IB_LOG_LEVEL_FATAL,
				"Cannot create %s. Check that"
				" the file does not exist yet.", name);
		}

		ib_logf(IB_LOG_LEVEL_INFO,
			"Setting log file size to %llu",
			log_file_size);

		success = os_file_set_size(name, log_file, log_file_size);

		if (!success) {
			ib_logf(IB_LOG_LEVEL_FATAL,
				"Cannot set %s size to %llu",
				name, log_file_size);
		}

		os_file_flush(log_file);
		os_file_close(log_file);
	}

	/* We pretend there is a checkpoint at
	lsn + RedoLog::BLOCK_HDR_SIZE */

	log_reset_first_header_and_checkpoint(buf, lsn);

	RedoLog::block_init_v1(buf + LOG_FILE_HDR_SIZE, lsn);

	RedoLog::block_set_first_rec_group(
		buf + LOG_FILE_HDR_SIZE, RedoLog::BLOCK_HDR_SIZE);

	sprintf(name, "%s%s%lu", log_dir, ib_logfile_basename, (ulong)0);

	log_file = os_file_create_simple(
		innodb_log_file_key, name, OS_FILE_OPEN,
		OS_FILE_READ_WRITE, &success);

	if (!success) {
		ib_logf(IB_LOG_LEVEL_FATAL, "Cannot open %s.", name);
	}

	os_file_write(
		name, log_file, buf, 0,
		LOG_FILE_HDR_SIZE + OS_FILE_LOG_BLOCK_SIZE);

	os_file_flush(log_file);
	os_file_close(log_file);

	ut_free(buf);
}
#endif /* !UNIV_HOTBACKUP */

