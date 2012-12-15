/*****************************************************************************

Copyright (c) 2010, 2012, Oracle and/or its affiliates. All Rights Reserved.
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

/**************************************************//**
@file srv/srv0mon.cc
Database monitor counter interfaces

Created 12/9/2009 Jimmy Yang
*******************************************************/

#ifndef UNIV_HOTBACKUP
#include "os0file.h"
#include "mach0data.h"
#include "srv0mon.h"
#include "srv0srv.h"
#include "buf0buf.h"
#include "trx0sys.h"
#include "trx0rseg.h"
#include "lock0lock.h"
#include "ibuf0ibuf.h"
#ifdef UNIV_NONINL
#include "srv0mon.ic"
#endif

/* Macro to standardize the counter names for counters in the
"monitor_buf_page" module as they have very structured defines */
#define	MONITOR_BUF_PAGE(name, description, code, op, op_code)	\
	{"buffer_page_"op"_"name, "buffer_page_io",		\
	 "Number of "description" Pages "op,			\
	 MONITOR_GROUP_MODULE, MONITOR_DEFAULT_START,		\
	 MONITOR_##code##_##op_code}

#define MONITOR_BUF_PAGE_READ(name, description, code)		\
	 MONITOR_BUF_PAGE(name, description, code, "read", PAGE_READ)

#define MONITOR_BUF_PAGE_WRITTEN(name, description, code)	\
	 MONITOR_BUF_PAGE(name, description, code, "written", PAGE_WRITTEN)


/** This array defines basic static information of monitor counters,
including each monitor's name, module it belongs to, a short
description and its property/type and corresponding monitor_id.
Please note: If you add a monitor here, please add its corresponding
monitor_id to "enum monitor_id_value" structure in srv0mon.h file. */

static monitor_info_t	innodb_counter_info[] =
{
	/* A dummy item to mark the module start, this is
	to accomodate the default value (0) set for the
	global variables with the control system. */
	{"module_start", "module_start", "module_start",
	MONITOR_MODULE,
	MONITOR_DEFAULT_START, MONITOR_DEFAULT_START},

	/* ========== Counters for Server Metadata ========== */
	{"module_metadata", "metadata", "Server Metadata",
	 MONITOR_MODULE,
	 MONITOR_DEFAULT_START, MONITOR_MODULE_METADATA},

	{"metadata_table_handles_opened", "metadata",
	 "Number of table handles opened",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_TABLE_OPEN},

	{"metadata_table_handles_closed", "metadata",
	 "Number of table handles closed",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_TABLE_CLOSE},

	{"metadata_table_reference_count", "metadata",
	 "Table reference counter",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_TABLE_REFERENCE},

	{"metadata_mem_pool_size", "metadata",
	 "Size of a memory pool InnoDB uses to store data dictionary"
	 " and internal data structures in bytes",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON | MONITOR_DISPLAY_CURRENT),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_META_MEM_POOL},

	/* ========== Counters for Lock Module ========== */
	{"module_lock", "lock", "Lock Module",
	 MONITOR_MODULE,
	 MONITOR_DEFAULT_START, MONITOR_MODULE_LOCK},

	{"lock_deadlocks", "lock", "Number of deadlocks",
	 MONITOR_DEFAULT_ON,
	 MONITOR_DEFAULT_START, MONITOR_DEADLOCK},

	{"lock_timeouts", "lock", "Number of lock timeouts",
	 MONITOR_DEFAULT_ON,
	 MONITOR_DEFAULT_START, MONITOR_TIMEOUT},

	{"lock_rec_lock_waits", "lock",
	 "Number of times enqueued into record lock wait queue",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_LOCKREC_WAIT},

	{"lock_table_lock_waits", "lock",
	 "Number of times enqueued into table lock wait queue",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_TABLELOCK_WAIT},

	{"lock_rec_lock_requests", "lock",
	 "Number of record locks requested",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_NUM_RECLOCK_REQ},

	{"lock_rec_lock_created", "lock", "Number of record locks created",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_RECLOCK_CREATED},

	{"lock_rec_lock_removed", "lock",
	 "Number of record locks removed from the lock queue",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_RECLOCK_REMOVED},

	{"lock_rec_locks", "lock",
	 "Current number of record locks on tables",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_NUM_RECLOCK},

	{"lock_table_lock_created", "lock", "Number of table locks created",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_TABLELOCK_CREATED},

	{"lock_table_lock_removed", "lock",
	 "Number of table locks removed from the lock queue",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_TABLELOCK_REMOVED},

	{"lock_table_locks", "lock",
	 "Current number of table locks on tables",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_NUM_TABLELOCK},

	{"lock_row_lock_current_waits", "lock",
	 "Number of row locks currently being waited for"
	 " (innodb_row_lock_current_waits)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_ROW_LOCK_CURRENT_WAIT},

	{"lock_row_lock_time", "lock",
	 "Time spent in acquiring row locks, in milliseconds"
	 " (innodb_row_lock_time)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_LOCK_WAIT_TIME},

	{"lock_row_lock_time_max", "lock",
	 "The maximum time to acquire a row lock, in milliseconds"
	 " (innodb_row_lock_time_max)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DISPLAY_CURRENT | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_LOCK_MAX_WAIT_TIME},

	{"lock_row_lock_waits", "lock",
	 "Number of times a row lock had to be waited for"
	 " (innodb_row_lock_waits)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_ROW_LOCK_WAIT},

	{"lock_row_lock_time_avg", "lock",
	 "The average time to acquire a row lock, in milliseconds"
	 " (innodb_row_lock_time_avg)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DISPLAY_CURRENT | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_LOCK_AVG_WAIT_TIME},

	/* ========== Counters for Buffer Manager and I/O ========== */
	{"module_buffer", "buffer", "Buffer Manager Module",
	 MONITOR_MODULE,
	 MONITOR_DEFAULT_START, MONITOR_MODULE_BUFFER},

	{"buffer_pool_size", "server",
	 "Server buffer pool size (all buffer pools) in bytes",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON | MONITOR_DISPLAY_CURRENT),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_BUFFER_POOL_SIZE},

	{"buffer_pool_reads", "buffer",
	 "Number of reads directly from disk (innodb_buffer_pool_reads)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_BUF_POOL_READS},

	{"buffer_pool_read_requests", "buffer",
	 "Number of logical read requests (innodb_buffer_pool_read_requests)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_BUF_POOL_READ_REQUESTS},

	{"buffer_pool_write_requests", "buffer",
	 "Number of write requests (innodb_buffer_pool_write_requests)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_BUF_POOL_WRITE_REQUEST},

	{"buffer_pool_wait_free", "buffer",
	 "Number of times waited for free buffer"
	 " (innodb_buffer_pool_wait_free)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_BUF_POOL_WAIT_FREE},

	{"buffer_pool_read_ahead", "buffer",
	 "Number of pages read as read ahead (innodb_buffer_pool_read_ahead)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_BUF_POOL_READ_AHEAD},

	{"buffer_pool_read_ahead_evicted", "buffer",
	 "Read-ahead pages evicted without being accessed"
	 " (innodb_buffer_pool_read_ahead_evicted)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_BUF_POOL_READ_AHEAD_EVICTED},

	{"buffer_pool_pages_total", "buffer",
	 "Total buffer pool size in pages (innodb_buffer_pool_pages_total)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DISPLAY_CURRENT | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_BUF_POOL_PAGE_TOTAL},

	{"buffer_pool_pages_misc", "buffer",
	 "Buffer pages for misc use such as row locks or the adaptive"
	 " hash index (innodb_buffer_pool_pages_misc)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DISPLAY_CURRENT | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_BUF_POOL_PAGE_MISC},

	{"buffer_pool_pages_data", "buffer",
	 "Buffer pages containing data (innodb_buffer_pool_pages_data)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DISPLAY_CURRENT | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_BUF_POOL_PAGES_DATA},

	{"buffer_pool_bytes_data", "buffer",
	 "Buffer bytes containing data (innodb_buffer_pool_bytes_data)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DISPLAY_CURRENT | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_BUF_POOL_BYTES_DATA},

	{"buffer_pool_pages_dirty", "buffer",
	 "Buffer pages currently dirty (innodb_buffer_pool_pages_dirty)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DISPLAY_CURRENT | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_BUF_POOL_PAGES_DIRTY},

	{"buffer_pool_bytes_dirty", "buffer",
         "Buffer bytes currently dirty (innodb_buffer_pool_bytes_dirty)",
         static_cast<monitor_type_t>(
         MONITOR_EXISTING | MONITOR_DISPLAY_CURRENT | MONITOR_DEFAULT_ON),
         MONITOR_DEFAULT_START, MONITOR_OVLD_BUF_POOL_BYTES_DIRTY},

	{"buffer_pool_pages_free", "buffer",
	 "Buffer pages currently free (innodb_buffer_pool_pages_free)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DISPLAY_CURRENT | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_BUF_POOL_PAGES_FREE},

	{"buffer_pages_created", "buffer",
	 "Number of pages created (innodb_pages_created)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_PAGE_CREATED},

	{"buffer_pages_written", "buffer",
	 "Number of pages written (innodb_pages_written)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_PAGES_WRITTEN},

	{"buffer_pages_read", "buffer",
	 "Number of pages read (innodb_pages_read)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_PAGES_READ},

	{"buffer_data_reads", "buffer",
	 "Amount of data read in bytes (innodb_data_reads)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_BYTE_READ},

	{"buffer_data_written", "buffer",
	 "Amount of data written in bytes (innodb_data_written)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_BYTE_WRITTEN},

	/* Cumulative counter for scanning in flush batches */
	{"buffer_flush_batch_scanned", "buffer",
	 "Total pages scanned as part of flush batch",
	 MONITOR_SET_OWNER,
	 MONITOR_FLUSH_BATCH_SCANNED_NUM_CALL,
	 MONITOR_FLUSH_BATCH_SCANNED},

	{"buffer_flush_batch_num_scan", "buffer",
	 "Number of times buffer flush list flush is called",
	 MONITOR_SET_MEMBER, MONITOR_FLUSH_BATCH_SCANNED,
	 MONITOR_FLUSH_BATCH_SCANNED_NUM_CALL},

	{"buffer_flush_batch_scanned_per_call", "buffer",
	 "Pages scanned per flush batch scan",
	 MONITOR_SET_MEMBER, MONITOR_FLUSH_BATCH_SCANNED,
	 MONITOR_FLUSH_BATCH_SCANNED_PER_CALL},

	/* Cumulative counter for pages flushed in flush batches */
	{"buffer_flush_batch_total_pages", "buffer",
	 "Total pages flushed as part of flush batch",
	 MONITOR_SET_OWNER, MONITOR_FLUSH_BATCH_COUNT,
	 MONITOR_FLUSH_BATCH_TOTAL_PAGE},

	{"buffer_flush_batches", "buffer",
	 "Number of flush batches",
	 MONITOR_SET_MEMBER, MONITOR_FLUSH_BATCH_TOTAL_PAGE,
	 MONITOR_FLUSH_BATCH_COUNT},

	{"buffer_flush_batch_pages", "buffer",
	 "Pages queued as a flush batch",
	 MONITOR_SET_MEMBER, MONITOR_FLUSH_BATCH_TOTAL_PAGE,
	 MONITOR_FLUSH_BATCH_PAGES},

	/* Cumulative counter for flush batches because of neighbor */
	{"buffer_flush_neighbor_total_pages", "buffer",
	 "Total neighbors flushed as part of neighbor flush",
	 MONITOR_SET_OWNER, MONITOR_FLUSH_NEIGHBOR_COUNT,
	 MONITOR_FLUSH_NEIGHBOR_TOTAL_PAGE},

	{"buffer_flush_neighbor", "buffer",
	 "Number of times neighbors flushing is invoked",
	 MONITOR_SET_MEMBER, MONITOR_FLUSH_NEIGHBOR_TOTAL_PAGE,
	 MONITOR_FLUSH_NEIGHBOR_COUNT},

	{"buffer_flush_neighbor_pages", "buffer",
	 "Pages queued as a neighbor batch",
	 MONITOR_SET_MEMBER, MONITOR_FLUSH_NEIGHBOR_TOTAL_PAGE,
	 MONITOR_FLUSH_NEIGHBOR_PAGES},

	{"buffer_flush_n_to_flush_requested", "buffer",
	 "Number of pages requested for flushing.",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_FLUSH_N_TO_FLUSH_REQUESTED},

	{"buffer_flush_avg_page_rate", "buffer",
	 "Average number of pages at which flushing is happening",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_FLUSH_AVG_PAGE_RATE},

	{"buffer_flush_lsn_avg_rate", "buffer",
	 "Average redo generation rate",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_FLUSH_LSN_AVG_RATE},

	{"buffer_flush_pct_for_dirty", "buffer",
	 "Percent of IO capacity used to avoid max dirty page limit",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_FLUSH_PCT_FOR_DIRTY},

	{"buffer_flush_pct_for_lsn", "buffer",
	 "Percent of IO capacity used to avoid reusable redo space limit",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_FLUSH_PCT_FOR_LSN},

	{"buffer_flush_sync_waits", "buffer",
	 "Number of times a wait happens due to sync flushing",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_FLUSH_SYNC_WAITS},


	/* Cumulative counter for flush batches for adaptive flushing  */
	{"buffer_flush_adaptive_total_pages", "buffer",
	 "Total pages flushed as part of adaptive flushing",
	 MONITOR_SET_OWNER, MONITOR_FLUSH_ADAPTIVE_COUNT,
	 MONITOR_FLUSH_ADAPTIVE_TOTAL_PAGE},

	{"buffer_flush_adaptive", "buffer",
	 "Number of adaptive batches",
	 MONITOR_SET_MEMBER, MONITOR_FLUSH_ADAPTIVE_TOTAL_PAGE,
	 MONITOR_FLUSH_ADAPTIVE_COUNT},

	{"buffer_flush_adaptive_pages", "buffer",
	 "Pages queued as an adaptive batch",
	 MONITOR_SET_MEMBER, MONITOR_FLUSH_ADAPTIVE_TOTAL_PAGE,
	 MONITOR_FLUSH_ADAPTIVE_PAGES},

	/* Cumulative counter for flush batches because of sync */
	{"buffer_flush_sync_total_pages", "buffer",
	 "Total pages flushed as part of sync batches",
	 MONITOR_SET_OWNER, MONITOR_FLUSH_SYNC_COUNT,
	 MONITOR_FLUSH_SYNC_TOTAL_PAGE},

	{"buffer_flush_sync", "buffer",
	 "Number of sync batches",
	 MONITOR_SET_MEMBER, MONITOR_FLUSH_SYNC_TOTAL_PAGE,
	 MONITOR_FLUSH_SYNC_COUNT},

	{"buffer_flush_sync_pages", "buffer",
	 "Pages queued as a sync batch",
	 MONITOR_SET_MEMBER, MONITOR_FLUSH_SYNC_TOTAL_PAGE,
	 MONITOR_FLUSH_SYNC_PAGES},

	/* Cumulative counter for flush batches because of background */
	{"buffer_flush_background_total_pages", "buffer",
	 "Total pages flushed as part of background batches",
	 MONITOR_SET_OWNER, MONITOR_FLUSH_BACKGROUND_COUNT,
	 MONITOR_FLUSH_BACKGROUND_TOTAL_PAGE},

	{"buffer_flush_background", "buffer",
	 "Number of background batches",
	 MONITOR_SET_MEMBER, MONITOR_FLUSH_BACKGROUND_TOTAL_PAGE,
	 MONITOR_FLUSH_BACKGROUND_COUNT},

	{"buffer_flush_background_pages", "buffer",
	 "Pages queued as a background batch",
	 MONITOR_SET_MEMBER, MONITOR_FLUSH_BACKGROUND_TOTAL_PAGE,
	 MONITOR_FLUSH_BACKGROUND_PAGES},

	/* Cumulative counter for LRU batch scan */
	{"buffer_LRU_batch_scanned", "buffer",
	 "Total pages scanned as part of LRU batch",
	 MONITOR_SET_OWNER, MONITOR_LRU_BATCH_SCANNED_NUM_CALL,
	 MONITOR_LRU_BATCH_SCANNED},

	{"buffer_LRU_batch_num_scan", "buffer",
	 "Number of times LRU batch is called",
	 MONITOR_SET_MEMBER, MONITOR_LRU_BATCH_SCANNED,
	 MONITOR_LRU_BATCH_SCANNED_NUM_CALL},

	{"buffer_LRU_batch_scanned_per_call", "buffer",
	 "Pages scanned per LRU batch call",
	 MONITOR_SET_MEMBER, MONITOR_LRU_BATCH_SCANNED,
	 MONITOR_LRU_BATCH_SCANNED_PER_CALL},

	/* Cumulative counter for LRU batch pages flushed */
	{"buffer_LRU_batch_total_pages", "buffer",
	 "Total pages flushed as part of LRU batches",
	 MONITOR_SET_OWNER, MONITOR_LRU_BATCH_COUNT,
	 MONITOR_LRU_BATCH_TOTAL_PAGE},

	{"buffer_LRU_batches", "buffer",
	 "Number of LRU batches",
	 MONITOR_SET_MEMBER, MONITOR_LRU_BATCH_TOTAL_PAGE,
	 MONITOR_LRU_BATCH_COUNT},

	{"buffer_LRU_batch_pages", "buffer",
	 "Pages queued as an LRU batch",
	 MONITOR_SET_MEMBER, MONITOR_LRU_BATCH_TOTAL_PAGE,
	 MONITOR_LRU_BATCH_PAGES},

	/* Cumulative counter for single page LRU scans */
	{"buffer_LRU_single_flush_scanned", "buffer",
	 "Total pages scanned as part of single page LRU flush",
	 MONITOR_SET_OWNER,
	 MONITOR_LRU_SINGLE_FLUSH_SCANNED_NUM_CALL,
	 MONITOR_LRU_SINGLE_FLUSH_SCANNED},

	{"buffer_LRU_single_flush_num_scan", "buffer",
	 "Number of times single page LRU flush is called",
	 MONITOR_SET_MEMBER, MONITOR_LRU_SINGLE_FLUSH_SCANNED,
	 MONITOR_LRU_SINGLE_FLUSH_SCANNED_NUM_CALL},

	{"buffer_LRU_single_flush_scanned_per_call", "buffer",
	 "Page scanned per single LRU flush",
	 MONITOR_SET_MEMBER, MONITOR_LRU_SINGLE_FLUSH_SCANNED,
	 MONITOR_LRU_SINGLE_FLUSH_SCANNED_PER_CALL},

	{"buffer_LRU_single_flush_failure_count", "Buffer",
	 "Number of times attempt to flush a single page from LRU failed",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_LRU_SINGLE_FLUSH_FAILURE_COUNT},

	{"buffer_LRU_get_free_search", "Buffer",
	 "Number of searches performed for a clean page",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_LRU_GET_FREE_SEARCH},

	/* Cumulative counter for LRU search scans */
	{"buffer_LRU_search_scanned", "buffer",
	 "Total pages scanned as part of LRU search",
	 MONITOR_SET_OWNER,
	 MONITOR_LRU_SEARCH_SCANNED_NUM_CALL,
	 MONITOR_LRU_SEARCH_SCANNED},

	{"buffer_LRU_search_num_scan", "buffer",
	 "Number of times LRU search is performed",
	 MONITOR_SET_MEMBER, MONITOR_LRU_SEARCH_SCANNED,
	 MONITOR_LRU_SEARCH_SCANNED_NUM_CALL},

	{"buffer_LRU_search_scanned_per_call", "buffer",
	 "Page scanned per single LRU search",
	 MONITOR_SET_MEMBER, MONITOR_LRU_SEARCH_SCANNED,
	 MONITOR_LRU_SEARCH_SCANNED_PER_CALL},

	/* Cumulative counter for LRU unzip search scans */
	{"buffer_LRU_unzip_search_scanned", "buffer",
	 "Total pages scanned as part of LRU unzip search",
	 MONITOR_SET_OWNER,
	 MONITOR_LRU_UNZIP_SEARCH_SCANNED_NUM_CALL,
	 MONITOR_LRU_UNZIP_SEARCH_SCANNED},

	{"buffer_LRU_unzip_search_num_scan", "buffer",
	 "Number of times LRU unzip search is performed",
	 MONITOR_SET_MEMBER, MONITOR_LRU_UNZIP_SEARCH_SCANNED,
	 MONITOR_LRU_UNZIP_SEARCH_SCANNED_NUM_CALL},

	{"buffer_LRU_unzip_search_scanned_per_call", "buffer",
	 "Page scanned per single LRU unzip search",
	 MONITOR_SET_MEMBER, MONITOR_LRU_UNZIP_SEARCH_SCANNED,
	 MONITOR_LRU_UNZIP_SEARCH_SCANNED_PER_CALL},

	/* ========== Counters for Buffer Page I/O ========== */
	{"module_buffer_page", "buffer_page_io", "Buffer Page I/O Module",
	 static_cast<monitor_type_t>(
	 MONITOR_MODULE | MONITOR_GROUP_MODULE),
	 MONITOR_DEFAULT_START, MONITOR_MODULE_BUF_PAGE},

	MONITOR_BUF_PAGE_READ("index_leaf","Index Leaf", INDEX_LEAF),

	MONITOR_BUF_PAGE_READ("index_non_leaf","Index Non-leaf",
			      INDEX_NON_LEAF),

	MONITOR_BUF_PAGE_READ("index_ibuf_leaf", "Insert Buffer Index Leaf",
			      INDEX_IBUF_LEAF),

	MONITOR_BUF_PAGE_READ("index_ibuf_non_leaf",
			      "Insert Buffer Index Non-Leaf",
			       INDEX_IBUF_NON_LEAF),

	MONITOR_BUF_PAGE_READ("undo_log", "Undo Log", UNDO_LOG),

	MONITOR_BUF_PAGE_READ("index_inode", "Index Inode", INODE),

	MONITOR_BUF_PAGE_READ("ibuf_free_list", "Insert Buffer Free List",
			      IBUF_FREELIST),

	MONITOR_BUF_PAGE_READ("ibuf_bitmap", "Insert Buffer Bitmap",
			      IBUF_BITMAP),

	MONITOR_BUF_PAGE_READ("system_page", "System", SYSTEM),

	MONITOR_BUF_PAGE_READ("trx_system", "Transaction System", TRX_SYSTEM),

	MONITOR_BUF_PAGE_READ("fsp_hdr", "File Space Header", FSP_HDR),

	MONITOR_BUF_PAGE_READ("xdes", "Extent Descriptor", XDES),

	MONITOR_BUF_PAGE_READ("blob", "Uncompressed BLOB", BLOB),

	MONITOR_BUF_PAGE_READ("zblob", "First Compressed BLOB", ZBLOB),

	MONITOR_BUF_PAGE_READ("zblob2", "Subsequent Compressed BLOB", ZBLOB2),

	MONITOR_BUF_PAGE_READ("other", "other/unknown (old version of InnoDB)",
			      OTHER),

	MONITOR_BUF_PAGE_WRITTEN("index_leaf","Index Leaf", INDEX_LEAF),

	MONITOR_BUF_PAGE_WRITTEN("index_non_leaf","Index Non-leaf",
				 INDEX_NON_LEAF),

	MONITOR_BUF_PAGE_WRITTEN("index_ibuf_leaf", "Insert Buffer Index Leaf",
				 INDEX_IBUF_LEAF),

	MONITOR_BUF_PAGE_WRITTEN("index_ibuf_non_leaf",
				 "Insert Buffer Index Non-Leaf",
				 INDEX_IBUF_NON_LEAF),

	MONITOR_BUF_PAGE_WRITTEN("undo_log", "Undo Log", UNDO_LOG),

	MONITOR_BUF_PAGE_WRITTEN("index_inode", "Index Inode", INODE),

	MONITOR_BUF_PAGE_WRITTEN("ibuf_free_list", "Insert Buffer Free List",
				 IBUF_FREELIST),

	MONITOR_BUF_PAGE_WRITTEN("ibuf_bitmap", "Insert Buffer Bitmap",
				 IBUF_BITMAP),

	MONITOR_BUF_PAGE_WRITTEN("system_page", "System", SYSTEM),

	MONITOR_BUF_PAGE_WRITTEN("trx_system", "Transaction System",
				 TRX_SYSTEM),

	MONITOR_BUF_PAGE_WRITTEN("fsp_hdr", "File Space Header", FSP_HDR),

	MONITOR_BUF_PAGE_WRITTEN("xdes", "Extent Descriptor", XDES),

	MONITOR_BUF_PAGE_WRITTEN("blob", "Uncompressed BLOB", BLOB),

	MONITOR_BUF_PAGE_WRITTEN("zblob", "First Compressed BLOB", ZBLOB),

	MONITOR_BUF_PAGE_WRITTEN("zblob2", "Subsequent Compressed BLOB",
				 ZBLOB2),

	MONITOR_BUF_PAGE_WRITTEN("other", "other/unknown (old version InnoDB)",
				 OTHER),

	/* ========== Counters for OS level operations ========== */
	{"module_os", "os", "OS Level Operation",
	 MONITOR_MODULE,
	 MONITOR_DEFAULT_START, MONITOR_MODULE_OS},

	{"os_data_reads", "os",
	 "Number of reads initiated (innodb_data_reads)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_OS_FILE_READ},

	{"os_data_writes", "os",
	 "Number of writes initiated (innodb_data_writes)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_OS_FILE_WRITE},

	{"os_data_fsyncs", "os",
	 "Number of fsync() calls (innodb_data_fsyncs)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_OS_FSYNC},

	{"os_pending_reads", "os", "Number of reads pending",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_OS_PENDING_READS},

	{"os_pending_writes", "os", "Number of writes pending",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_OS_PENDING_WRITES},

	{"os_log_bytes_written", "os",
	 "Bytes of log written (innodb_os_log_written)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_OS_LOG_WRITTEN},

	{"os_log_fsyncs", "os",
	 "Number of fsync log writes (innodb_os_log_fsyncs)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_OS_LOG_FSYNC},

	{"os_log_pending_fsyncs", "os",
	 "Number of pending fsync write (innodb_os_log_pending_fsyncs)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_OS_LOG_PENDING_FSYNC},

	{"os_log_pending_writes", "os",
	 "Number of pending log file writes (innodb_os_log_pending_writes)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_OS_LOG_PENDING_WRITES},

	/* ========== Counters for Transaction Module ========== */
	{"module_trx", "transaction", "Transaction Manager",
	 MONITOR_MODULE,
	 MONITOR_DEFAULT_START, MONITOR_MODULE_TRX},

	{"trx_rw_commits", "transaction", "Number of read-write transactions "
	  "committed",
	 MONITOR_NONE, MONITOR_DEFAULT_START, MONITOR_TRX_RW_COMMIT},

	{"trx_ro_commits", "transaction", "Number of read-only transactions "
	  "committed",
	 MONITOR_NONE, MONITOR_DEFAULT_START, MONITOR_TRX_RO_COMMIT},

	{"trx_nl_ro_commits", "transaction", "Number of non-locking "
	 "auto-commit read-only transactions committed",
	 MONITOR_NONE, MONITOR_DEFAULT_START, MONITOR_TRX_NL_RO_COMMIT},

	{"trx_commits_insert_update", "transaction",
	 "Number of transactions committed with inserts and updates",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_TRX_COMMIT_UNDO},

	{"trx_rollbacks", "transaction",
	 "Number of transactions rolled back",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_TRX_ROLLBACK},

	{"trx_rollbacks_savepoint", "transaction",
	 "Number of transactions rolled back to savepoint",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_TRX_ROLLBACK_SAVEPOINT},

	{"trx_rollback_active", "transaction",
	 "Number of resurrected active transactions rolled back",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_TRX_ROLLBACK_ACTIVE},

	{"trx_active_transactions", "transaction",
	 "Number of active transactions",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_TRX_ACTIVE},

	{"trx_rseg_history_len", "transaction",
	 "Length of the TRX_RSEG_HISTORY list",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DISPLAY_CURRENT | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_RSEG_HISTORY_LEN},

	{"trx_undo_slots_used", "transaction", "Number of undo slots used",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_NUM_UNDO_SLOT_USED},

	{"trx_undo_slots_cached", "transaction",
	 "Number of undo slots cached",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_NUM_UNDO_SLOT_CACHED},

	{"trx_rseg_current_size", "transaction",
	 "Current rollback segment size in pages",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DISPLAY_CURRENT),
	 MONITOR_DEFAULT_START, MONITOR_RSEG_CUR_SIZE},

	/* ========== Counters for Purge Module ========== */
	{"module_purge", "purge", "Purge Module",
	 MONITOR_MODULE,
	 MONITOR_DEFAULT_START, MONITOR_MODULE_PURGE},

	{"purge_del_mark_records", "purge",
	 "Number of delete-marked rows purged",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_N_DEL_ROW_PURGE},

	{"purge_upd_exist_or_extern_records", "purge",
	 "Number of purges on updates of existing records and "
	 " updates on delete marked record with externally stored field",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_N_UPD_EXIST_EXTERN},

	{"purge_invoked", "purge",
	 "Number of times purge was invoked",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_PURGE_INVOKED},

	{"purge_undo_log_pages", "purge",
	 "Number of undo log pages handled by the purge",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_PURGE_N_PAGE_HANDLED},

	{"purge_dml_delay_usec", "purge",
	 "Microseconds DML to be delayed due to purge lagging",
	 MONITOR_DISPLAY_CURRENT,
	 MONITOR_DEFAULT_START, MONITOR_DML_PURGE_DELAY},

	{"purge_stop_count", "purge",
	 "Number of times purge was stopped",
	 MONITOR_DISPLAY_CURRENT,
	 MONITOR_DEFAULT_START, MONITOR_PURGE_STOP_COUNT},

	{"purge_resume_count", "purge",
	 "Number of times purge was resumed",
	 MONITOR_DISPLAY_CURRENT,
	 MONITOR_DEFAULT_START, MONITOR_PURGE_RESUME_COUNT},

	/* ========== Counters for Recovery Module ========== */
	{"module_log", "recovery", "Recovery Module",
	 MONITOR_MODULE,
	 MONITOR_DEFAULT_START, MONITOR_MODULE_RECOVERY},

	{"log_checkpoints", "recovery", "Number of checkpoints",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_NUM_CHECKPOINT},

	{"log_lsn_last_flush", "recovery", "LSN of Last flush",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DISPLAY_CURRENT),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_LSN_FLUSHDISK},

	{"log_lsn_last_checkpoint", "recovery", "LSN at last checkpoint",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DISPLAY_CURRENT),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_LSN_CHECKPOINT},

	{"log_lsn_current", "recovery", "Current LSN value",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DISPLAY_CURRENT),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_LSN_CURRENT},

	{"log_lsn_checkpoint_age", "recovery",
	 "Current LSN value minus LSN at last checkpoint",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_LSN_CHECKPOINT_AGE},

	{"log_lsn_buf_pool_oldest", "recovery",
	 "The oldest modified block LSN in the buffer pool",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DISPLAY_CURRENT),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_BUF_OLDEST_LSN},

	{"log_max_modified_age_async", "recovery",
	 "Maximum LSN difference; when exceeded, start asynchronous preflush",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DISPLAY_CURRENT),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_MAX_AGE_ASYNC},

	{"log_max_modified_age_sync", "recovery",
	 "Maximum LSN difference; when exceeded, start synchronous preflush",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DISPLAY_CURRENT),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_MAX_AGE_SYNC},

	{"log_pending_log_writes", "recovery", "Pending log writes",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_PENDING_LOG_WRITE},

	{"log_pending_checkpoint_writes", "recovery", "Pending checkpoints",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_PENDING_CHECKPOINT_WRITE},

	{"log_num_log_io", "recovery", "Number of log I/Os",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_LOG_IO},

	{"log_waits", "recovery",
	 "Number of log waits due to small log buffer (innodb_log_waits)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_LOG_WAITS},

	{"log_write_requests", "recovery",
	 "Number of log write requests (innodb_log_write_requests)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_LOG_WRITE_REQUEST},

	{"log_writes", "recovery",
	 "Number of log writes (innodb_log_writes)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_LOG_WRITES},

	/* ========== Counters for Page Compression ========== */
	{"module_compress", "compression", "Page Compression Info",
	 MONITOR_MODULE,
	 MONITOR_DEFAULT_START, MONITOR_MODULE_PAGE},

	{"compress_pages_compressed", "compression",
	 "Number of pages compressed", MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_PAGE_COMPRESS},

	{"compress_pages_decompressed", "compression",
	 "Number of pages decompressed",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_PAGE_DECOMPRESS},

	{"compression_pad_increments", "compression",
	 "Number of times padding is incremented to avoid compression failures",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_PAD_INCREMENTS},

	{"compression_pad_decrements", "compression",
	 "Number of times padding is decremented due to good compressibility",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_PAD_DECREMENTS},

	/* ========== Counters for Index ========== */
	{"module_index", "index", "Index Manager",
	 MONITOR_MODULE,
	 MONITOR_DEFAULT_START, MONITOR_MODULE_INDEX},

	{"index_splits", "index", "Number of index splits",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_INDEX_SPLIT},

	{"index_merges", "index", "Number of index merges",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_INDEX_MERGE},

	/* ========== Counters for Adaptive Hash Index ========== */
	{"module_adaptive_hash", "adaptive_hash_index", "Adpative Hash Index",
	 MONITOR_MODULE,
	 MONITOR_DEFAULT_START, MONITOR_MODULE_ADAPTIVE_HASH},

	{"adaptive_hash_searches", "adaptive_hash_index",
	 "Number of successful searches using Adaptive Hash Index",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_ADAPTIVE_HASH_SEARCH},

	{"adaptive_hash_searches_btree", "adaptive_hash_index",
	 "Number of searches using B-tree on an index search",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_OVLD_ADAPTIVE_HASH_SEARCH_BTREE},

	{"adaptive_hash_pages_added", "adaptive_hash_index",
	 "Number of index pages on which the Adaptive Hash Index is built",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_ADAPTIVE_HASH_PAGE_ADDED},

	{"adaptive_hash_pages_removed", "adaptive_hash_index",
	 "Number of index pages whose corresponding Adaptive Hash Index"
	 " entries were removed",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_ADAPTIVE_HASH_PAGE_REMOVED},

	{"adaptive_hash_rows_added", "adaptive_hash_index",
	 "Number of Adaptive Hash Index rows added",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_ADAPTIVE_HASH_ROW_ADDED},

	{"adaptive_hash_rows_removed", "adaptive_hash_index",
	 "Number of Adaptive Hash Index rows removed",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_ADAPTIVE_HASH_ROW_REMOVED},

	{"adaptive_hash_rows_deleted_no_hash_entry", "adaptive_hash_index",
	 "Number of rows deleted that did not have corresponding Adaptive Hash"
	 " Index entries",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_ADAPTIVE_HASH_ROW_REMOVE_NOT_FOUND},

	{"adaptive_hash_rows_updated", "adaptive_hash_index",
	 "Number of Adaptive Hash Index rows updated",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_ADAPTIVE_HASH_ROW_UPDATED},

	/* ========== Counters for tablespace ========== */
	{"module_file", "file_system", "Tablespace and File System Manager",
	 MONITOR_MODULE,
	 MONITOR_DEFAULT_START, MONITOR_MODULE_FIL_SYSTEM},

	{"file_num_open_files", "file_system",
	 "Number of files currently open (innodb_num_open_files)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DISPLAY_CURRENT | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_N_FILE_OPENED},

	/* ========== Counters for Change Buffer ========== */
	{"module_ibuf_system", "change_buffer", "InnoDB Change Buffer",
	 MONITOR_MODULE,
	 MONITOR_DEFAULT_START, MONITOR_MODULE_IBUF_SYSTEM},

	{"ibuf_merges_insert", "change_buffer",
	 "Number of inserted records merged by change buffering",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_IBUF_MERGE_INSERT},

	{"ibuf_merges_delete_mark", "change_buffer",
	 "Number of deleted records merged by change buffering",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_IBUF_MERGE_DELETE},

	{"ibuf_merges_delete", "change_buffer",
	 "Number of purge records merged by change buffering",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_IBUF_MERGE_PURGE},

	{"ibuf_merges_discard_insert", "change_buffer",
	 "Number of insert merged operations discarded",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_IBUF_MERGE_DISCARD_INSERT},

	{"ibuf_merges_discard_delete_mark", "change_buffer",
	 "Number of deleted merged operations discarded",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_IBUF_MERGE_DISCARD_DELETE},

	{"ibuf_merges_discard_delete", "change_buffer",
	 "Number of purge merged  operations discarded",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_IBUF_MERGE_DISCARD_PURGE},

	{"ibuf_merges", "change_buffer", "Number of change buffer merges",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_IBUF_MERGES},

	{"ibuf_size", "change_buffer", "Change buffer size in pages",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_IBUF_SIZE},

	/* ========== Counters for server operations ========== */
	{"module_innodb", "innodb",
	 "Counter for general InnoDB server wide operations and properties",
	 MONITOR_MODULE,
	 MONITOR_DEFAULT_START, MONITOR_MODULE_SERVER},

	{"innodb_master_thread_sleeps", "server",
	 "Number of times (seconds) master thread sleeps",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_MASTER_THREAD_SLEEP},

	{"innodb_activity_count", "server", "Current server activity count",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_SERVER_ACTIVITY},

	{"innodb_master_active_loops", "server",
	 "Number of times master thread performs its tasks when"
	 " server is active",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_MASTER_ACTIVE_LOOPS},

	{"innodb_master_idle_loops", "server",
	 "Number of times master thread performs its tasks when server is idle",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_MASTER_IDLE_LOOPS},

	{"innodb_background_drop_table_usec", "server",
	 "Time (in microseconds) spent to process drop table list",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_SRV_BACKGROUND_DROP_TABLE_MICROSECOND},

	{"innodb_ibuf_merge_usec", "server",
	 "Time (in microseconds) spent to process change buffer merge",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_SRV_IBUF_MERGE_MICROSECOND},

	{"innodb_log_flush_usec", "server",
	 "Time (in microseconds) spent to flush log records",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_SRV_LOG_FLUSH_MICROSECOND},

	{"innodb_mem_validate_usec", "server",
	 "Time (in microseconds) spent to do memory validation",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_SRV_MEM_VALIDATE_MICROSECOND},

	{"innodb_master_purge_usec", "server",
	 "Time (in microseconds) spent by master thread to purge records",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_SRV_PURGE_MICROSECOND},

	{"innodb_dict_lru_usec", "server",
	 "Time (in microseconds) spent to process DICT LRU list",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_SRV_DICT_LRU_MICROSECOND},

	{"innodb_checkpoint_usec", "server",
	 "Time (in microseconds) spent by master thread to do checkpoint",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_SRV_CHECKPOINT_MICROSECOND},

	{"innodb_dblwr_writes", "server",
	 "Number of doublewrite operations that have been performed"
	 " (innodb_dblwr_writes)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_SRV_DBLWR_WRITES},

	{"innodb_dblwr_pages_written", "server",
	 "Number of pages that have been written for doublewrite operations"
	 " (innodb_dblwr_pages_written)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_SRV_DBLWR_PAGES_WRITTEN},

	{"innodb_page_size", "server",
	 "InnoDB page size in bytes (innodb_page_size)",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON | MONITOR_DISPLAY_CURRENT),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_SRV_PAGE_SIZE},

	{"innodb_rwlock_s_spin_waits", "server",
	 "Number of rwlock spin waits due to shared latch request",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_RWLOCK_S_SPIN_WAITS},

	{"innodb_rwlock_x_spin_waits", "server",
	 "Number of rwlock spin waits due to exclusive latch request",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_RWLOCK_X_SPIN_WAITS},

	{"innodb_rwlock_s_spin_rounds", "server",
	 "Number of rwlock spin loop rounds due to shared latch request",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_RWLOCK_S_SPIN_ROUNDS},

	{"innodb_rwlock_x_spin_rounds", "server",
	 "Number of rwlock spin loop rounds due to exclusive latch request",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_RWLOCK_X_SPIN_ROUNDS},

	{"innodb_rwlock_s_os_waits", "server",
	 "Number of OS waits due to shared latch request",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_RWLOCK_S_OS_WAITS},

	{"innodb_rwlock_x_os_waits", "server",
	 "Number of OS waits due to exclusive latch request",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OVLD_RWLOCK_X_OS_WAITS},

	/* ========== Counters for DML operations ========== */
	{"module_dml", "dml", "Statistics for DMLs",
	 MONITOR_MODULE,
	 MONITOR_DEFAULT_START, MONITOR_MODULE_DML_STATS},

	{"dml_reads", "dml", "Number of rows read",
	 static_cast<monitor_type_t>(MONITOR_EXISTING),
	 MONITOR_DEFAULT_START, MONITOR_OLVD_ROW_READ},

	{"dml_inserts", "dml", "Number of rows inserted",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OLVD_ROW_INSERTED},

	{"dml_deletes", "dml", "Number of rows deleted",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OLVD_ROW_DELETED},

	{"dml_updates", "dml", "Number of rows updated",
	 static_cast<monitor_type_t>(
	 MONITOR_EXISTING | MONITOR_DEFAULT_ON),
	 MONITOR_DEFAULT_START, MONITOR_OLVD_ROW_UPDTATED},

	/* ========== Counters for DDL operations ========== */
	{"module_ddl", "ddl", "Statistics for DDLs",
	 MONITOR_MODULE,
	 MONITOR_DEFAULT_START, MONITOR_MODULE_DDL_STATS},

	{"ddl_background_drop_indexes", "ddl",
	 "Number of indexes waiting to be dropped after failed index creation",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_BACKGROUND_DROP_INDEX},

	{"ddl_background_drop_tables", "ddl",
	 "Number of tables in background drop table list",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_BACKGROUND_DROP_TABLE},

	{"ddl_online_create_index", "ddl",
	 "Number of indexes being created online",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_ONLINE_CREATE_INDEX},

	{"ddl_pending_alter_table", "ddl",
	 "Number of ALTER TABLE, CREATE INDEX, DROP INDEX in progress",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_PENDING_ALTER_TABLE},

	/* ===== Counters for ICP (Index Condition Pushdown) Module ===== */
	{"module_icp", "icp", "Index Condition Pushdown",
	 MONITOR_MODULE,
	 MONITOR_DEFAULT_START, MONITOR_MODULE_ICP},

	{"icp_attempts", "icp",
	 "Number of attempts for index push-down condition checks",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_ICP_ATTEMPTS},

	{"icp_no_match", "icp", "Index push-down condition does not match",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_ICP_NO_MATCH},

	{"icp_out_of_range", "icp", "Index push-down condition out of range",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_ICP_OUT_OF_RANGE},

	{"icp_match", "icp", "Index push-down condition matches",
	 MONITOR_NONE,
	 MONITOR_DEFAULT_START, MONITOR_ICP_MATCH},

	/* ========== To turn on/off reset all counters ========== */
	{"all", "All Counters", "Turn on/off and reset all counters",
	 MONITOR_MODULE,
	 MONITOR_DEFAULT_START, MONITOR_ALL_COUNTER}
};

/* The "innodb_counter_value" array stores actual counter values */
UNIV_INTERN monitor_value_t	innodb_counter_value[NUM_MONITOR];

/* monitor_set_tbl is used to record and determine whether a monitor
has been turned on/off. */
UNIV_INTERN ulint		monitor_set_tbl[(NUM_MONITOR + NUM_BITS_ULINT
						- 1) / NUM_BITS_ULINT];

#ifndef HAVE_ATOMIC_BUILTINS_64
/** Mutex protecting atomic operations on platforms that lack
built-in operations for atomic memory access */
ib_mutex_t	monitor_mutex;

/** Key to register monitor_mutex with performance schema */
UNIV_INTERN mysql_pfs_key_t	monitor_mutex_key;

/****************************************************************//**
Initialize the monitor subsystem. */
UNIV_INTERN
void
srv_mon_create(void)
/*================*/
{
	mutex_create(monitor_mutex_key, &monitor_mutex, SYNC_ANY_LATCH);
}
/****************************************************************//**
Close the monitor subsystem. */
UNIV_INTERN
void
srv_mon_free(void)
/*==============*/
{
	mutex_free(&monitor_mutex);
}
#endif /* !HAVE_ATOMIC_BUILTINS_64 */

/****************************************************************//**
Get a monitor's "monitor_info" by its monitor id (index into the
innodb_counter_info array.
@return	Point to corresponding monitor_info_t, or NULL if no such
monitor */
UNIV_INTERN
monitor_info_t*
srv_mon_get_info(
/*=============*/
	monitor_id_t	monitor_id)	/*!< id indexing into the
					innodb_counter_info array */
{
	ut_a(monitor_id < NUM_MONITOR);

	return((monitor_id < NUM_MONITOR)
			? &innodb_counter_info[monitor_id]
			: NULL);
}

/****************************************************************//**
Get monitor's name by its monitor id (indexing into the
innodb_counter_info array.
@return	corresponding monitor name, or NULL if no such
monitor */
UNIV_INTERN
const char*
srv_mon_get_name(
/*=============*/
	monitor_id_t	monitor_id)	/*!< id index into the
					innodb_counter_info array */
{
	ut_a(monitor_id < NUM_MONITOR);

	return((monitor_id < NUM_MONITOR)
			? innodb_counter_info[monitor_id].monitor_name
			: NULL);
}

/****************************************************************//**
Turn on/off, reset monitor counters in a module. If module_id
is MONITOR_ALL_COUNTER then turn on all monitor counters.
turned on because it has already been turned on. */
UNIV_INTERN
void
srv_mon_set_module_control(
/*=======================*/
	monitor_id_t	module_id,	/*!< in: Module ID as in
					monitor_counter_id. If it is
					set to MONITOR_ALL_COUNTER, this means
					we shall turn on all the counters */
	mon_option_t	set_option)	/*!< in: Turn on/off reset the
					counter */
{
	ulint	ix;
	ulint	start_id;
	ibool	set_current_module = FALSE;

	ut_a(module_id <= NUM_MONITOR);
	ut_a(UT_ARR_SIZE(innodb_counter_info) == NUM_MONITOR);

	/* The module_id must be an ID of MONITOR_MODULE type */
	ut_a(innodb_counter_info[module_id].monitor_type & MONITOR_MODULE);

	/* start with the first monitor in the module. If module_id
	is MONITOR_ALL_COUNTER, this means we need to turn on all
	monitor counters. */
	if (module_id == MONITOR_ALL_COUNTER) {
		start_id = 1;
	} else if (innodb_counter_info[module_id].monitor_type
		   & MONITOR_GROUP_MODULE) {
		/* Counters in this module are set as a group together
		and cannot be turned on/off individually. Need to set
		the on/off bit in the module counter */
		start_id = module_id;
		set_current_module = TRUE;

	} else {
		start_id = module_id + 1;
	}

	for (ix = start_id; ix < NUM_MONITOR; ix++) {
		/* if we hit the next module counter, we will
		continue if we want to turn on all monitor counters,
		and break if just turn on the counters in the
		current module. */
		if (innodb_counter_info[ix].monitor_type & MONITOR_MODULE) {

			if (set_current_module) {
				/* Continue to set on/off bit on current
				module */
				set_current_module = FALSE;
			} else if (module_id == MONITOR_ALL_COUNTER) {
				continue;
			} else {
				/* Hitting the next module, stop */
				break;
			}
		}

		/* Cannot turn on a monitor already been turned on. User
		should be aware some counters are already on before
		turn them on again (which could reset counter value) */
		if (MONITOR_IS_ON(ix) && (set_option == MONITOR_TURN_ON)) {
			fprintf(stderr, "Monitor '%s' is already enabled.\n",
				srv_mon_get_name((monitor_id_t) ix));
			continue;
		}

		/* For some existing counters (server status variables),
		we will get its counter value at the start/stop time
		to calculate the actual value during the time. */
		if (innodb_counter_info[ix].monitor_type & MONITOR_EXISTING) {
			srv_mon_process_existing_counter(
				static_cast<monitor_id_t>(ix), set_option);
		}

		/* Currently support 4 operations on the monitor counters:
		turn on, turn off, reset and reset all operations. */
		switch (set_option) {
		case MONITOR_TURN_ON:
			MONITOR_ON(ix);
			MONITOR_INIT(ix);
			MONITOR_SET_START(ix);
			break;

		case MONITOR_TURN_OFF:
			MONITOR_OFF(ix);
			MONITOR_SET_OFF(ix);
			break;

		case MONITOR_RESET_VALUE:
			srv_mon_reset(static_cast<monitor_id_t>(ix));
			break;

		case MONITOR_RESET_ALL_VALUE:
			srv_mon_reset_all(static_cast<monitor_id_t>(ix));
			break;

		default:
			ut_error;
		}
	}
}

/****************************************************************//**
Get transaction system's rollback segment size in pages
@return size in pages */
static
ulint
srv_mon_get_rseg_size(void)
/*=======================*/
{
	ulint		i;
	ulint		value = 0;

	/* rseg_array is a static array, so we can go through it without
	mutex protection. In addition, we provide an estimate of the
	total rollback segment size and to avoid mutex contention we
	don't acquire the rseg->mutex" */
	for (i = 0; i < TRX_SYS_N_RSEGS; ++i) {
		const trx_rseg_t*	rseg = trx_sys->rseg_array[i];

		if (rseg != NULL) {
			value += rseg->curr_size;
		}
	}

	return(value);
}

/****************************************************************//**
This function consolidates some existing server counters used
by "system status variables". These existing system variables do not have
mechanism to start/stop and reset the counters, so we simulate these
controls by remembering the corresponding counter values when the
corresponding monitors are turned on/off/reset, and do appropriate
mathematics to deduct the actual value. Please also refer to
srv_export_innodb_status() for related global counters used by
the existing status variables.*/
UNIV_INTERN
void
srv_mon_process_existing_counter(
/*=============================*/
	monitor_id_t	monitor_id,	/*!< in: the monitor's ID as in
					monitor_counter_id */
	mon_option_t	set_option)	/*!< in: Turn on/off reset the
					counter */
{
	mon_type_t		value;
	monitor_info_t*		monitor_info;
	ibool			update_min = FALSE;
	buf_pool_stat_t		stat;
	buf_pools_list_size_t	buf_pools_list_size;
	ulint			LRU_len;
	ulint			free_len;
	ulint			flush_list_len;

	monitor_info = srv_mon_get_info(monitor_id);

	ut_a(monitor_info->monitor_type & MONITOR_EXISTING);
	ut_a(monitor_id < NUM_MONITOR);

	/* Get the value from corresponding global variable */
	switch (monitor_id) {
	case MONITOR_OVLD_META_MEM_POOL:
		value = srv_mem_pool_size;
		break;

	/* export_vars.innodb_buffer_pool_reads. Num Reads from
	disk (page not in buffer) */
	case MONITOR_OVLD_BUF_POOL_READS:
		value = srv_stats.buf_pool_reads;
		break;

	/* innodb_buffer_pool_read_requests, the number of logical
	read requests */
	case MONITOR_OVLD_BUF_POOL_READ_REQUESTS:
		buf_get_total_stat(&stat);
		value = stat.n_page_gets;
		break;

	/* innodb_buffer_pool_write_requests, the number of
	write request */
	case MONITOR_OVLD_BUF_POOL_WRITE_REQUEST:
		value = srv_stats.buf_pool_write_requests;
		break;

	/* innodb_buffer_pool_wait_free */
	case MONITOR_OVLD_BUF_POOL_WAIT_FREE:
		value = srv_stats.buf_pool_wait_free;
		break;

	/* innodb_buffer_pool_read_ahead */
	case MONITOR_OVLD_BUF_POOL_READ_AHEAD:
		buf_get_total_stat(&stat);
		value = stat.n_ra_pages_read;
		break;

	/* innodb_buffer_pool_read_ahead_evicted */
	case MONITOR_OVLD_BUF_POOL_READ_AHEAD_EVICTED:
		buf_get_total_stat(&stat);
		value = stat.n_ra_pages_evicted;
		break;

	/* innodb_buffer_pool_pages_total */
	case MONITOR_OVLD_BUF_POOL_PAGE_TOTAL:
		value = buf_pool_get_n_pages();
		break;

	/* innodb_buffer_pool_pages_misc */
	case MONITOR_OVLD_BUF_POOL_PAGE_MISC:
		buf_get_total_list_len(&LRU_len, &free_len, &flush_list_len);
		value = buf_pool_get_n_pages() - LRU_len - free_len;
		break;

	/* innodb_buffer_pool_pages_data */
	case MONITOR_OVLD_BUF_POOL_PAGES_DATA:
		buf_get_total_list_len(&LRU_len, &free_len, &flush_list_len);
		value = LRU_len;
		break;

	/* innodb_buffer_pool_bytes_data */
	case MONITOR_OVLD_BUF_POOL_BYTES_DATA:
		buf_get_total_list_size_in_bytes(&buf_pools_list_size);
		value = buf_pools_list_size.LRU_bytes
			+ buf_pools_list_size.unzip_LRU_bytes;
		break;

	/* innodb_buffer_pool_pages_dirty */
	case MONITOR_OVLD_BUF_POOL_PAGES_DIRTY:
		buf_get_total_list_len(&LRU_len, &free_len, &flush_list_len);
		value = flush_list_len;
		break;

	/* innodb_buffer_pool_bytes_dirty */
	case MONITOR_OVLD_BUF_POOL_BYTES_DIRTY:
		buf_get_total_list_size_in_bytes(&buf_pools_list_size);
		value = buf_pools_list_size.flush_list_bytes;
		break;

	/* innodb_buffer_pool_pages_free */
	case MONITOR_OVLD_BUF_POOL_PAGES_FREE:
		buf_get_total_list_len(&LRU_len, &free_len, &flush_list_len);
		value = free_len;
		break;

	/* innodb_pages_created, the number of pages created */
	case MONITOR_OVLD_PAGE_CREATED:
		buf_get_total_stat(&stat);
		value = stat.n_pages_created;
		break;

	/* innodb_pages_written, the number of page written */
	case MONITOR_OVLD_PAGES_WRITTEN:
		buf_get_total_stat(&stat);
		value = stat.n_pages_written;
		break;

	/* innodb_pages_read */
	case MONITOR_OVLD_PAGES_READ:
		buf_get_total_stat(&stat);
		value = stat.n_pages_read;
		break;

	/* innodb_data_reads, the total number of data reads */
	case MONITOR_OVLD_BYTE_READ:
		value = srv_stats.data_read;
		break;

	/* innodb_data_writes, the total number of data writes. */
	case MONITOR_OVLD_BYTE_WRITTEN:
		value = srv_stats.data_written;
		break;

	/* innodb_data_reads, the total number of data reads. */
	case MONITOR_OVLD_OS_FILE_READ:
		value = os_n_file_reads;
		break;

	/* innodb_data_writes, the total number of data writes*/
	case MONITOR_OVLD_OS_FILE_WRITE:
		value = os_n_file_writes;
		break;

	/* innodb_data_fsyncs, number of fsync() operations so far. */
	case MONITOR_OVLD_OS_FSYNC:
		value = os_n_fsyncs;
		break;

	/* innodb_os_log_written */
	case MONITOR_OVLD_OS_LOG_WRITTEN:
		value = (mon_type_t) srv_stats.os_log_written;
		break;

	/* innodb_os_log_fsyncs */
	case MONITOR_OVLD_OS_LOG_FSYNC:
		value = fil_n_log_flushes;
		break;

	/* innodb_os_log_pending_fsyncs */
	case MONITOR_OVLD_OS_LOG_PENDING_FSYNC:
		value = fil_n_pending_log_flushes;
		update_min = TRUE;
		break;

	/* innodb_os_log_pending_writes */
	case MONITOR_OVLD_OS_LOG_PENDING_WRITES:
		value = srv_stats.os_log_pending_writes;
		update_min = TRUE;
		break;

	/* innodb_log_waits */
	case MONITOR_OVLD_LOG_WAITS:
		value = srv_stats.log_waits;
		break;

	/* innodb_log_write_requests */
	case MONITOR_OVLD_LOG_WRITE_REQUEST:
		value = srv_stats.log_write_requests;
		break;

	/* innodb_log_writes */
	case MONITOR_OVLD_LOG_WRITES:
		value = srv_stats.log_writes;
		break;

	/* innodb_dblwr_writes */
	case MONITOR_OVLD_SRV_DBLWR_WRITES:
		value = srv_stats.dblwr_writes;
		break;

	/* innodb_dblwr_pages_written */
	case MONITOR_OVLD_SRV_DBLWR_PAGES_WRITTEN:
		value = srv_stats.dblwr_pages_written;
		break;

	/* innodb_page_size */
	case MONITOR_OVLD_SRV_PAGE_SIZE:
		value = UNIV_PAGE_SIZE;
		break;

	case MONITOR_OVLD_RWLOCK_S_SPIN_WAITS:
		value = rw_lock_stats.rw_s_spin_wait_count;
		break;

	case MONITOR_OVLD_RWLOCK_X_SPIN_WAITS:
		value = rw_lock_stats.rw_x_os_wait_count;
		break;

	case MONITOR_OVLD_RWLOCK_S_SPIN_ROUNDS:
		value = rw_lock_stats.rw_s_spin_round_count;
		break;

	case MONITOR_OVLD_RWLOCK_X_SPIN_ROUNDS:
		value = rw_lock_stats.rw_x_spin_round_count;
		break;

	case MONITOR_OVLD_RWLOCK_S_OS_WAITS:
		value = rw_lock_stats.rw_s_os_wait_count;
		break;

	case MONITOR_OVLD_RWLOCK_X_OS_WAITS:
		value = rw_lock_stats.rw_x_os_wait_count;
		break;

	case MONITOR_OVLD_BUFFER_POOL_SIZE:
		value = srv_buf_pool_size;
		break;

	/* innodb_rows_read */
	case MONITOR_OLVD_ROW_READ:
		value = srv_stats.n_rows_read;
		break;

	/* innodb_rows_inserted */
	case MONITOR_OLVD_ROW_INSERTED:
		value = srv_stats.n_rows_inserted;
		break;

	/* innodb_rows_deleted */
	case MONITOR_OLVD_ROW_DELETED:
		value = srv_stats.n_rows_deleted;
		break;

	/* innodb_rows_updated */
	case MONITOR_OLVD_ROW_UPDTATED:
		value = srv_stats.n_rows_updated;
		break;

	/* innodb_row_lock_current_waits */
	case MONITOR_OVLD_ROW_LOCK_CURRENT_WAIT:
		value = srv_stats.n_lock_wait_current_count;
		break;

	/* innodb_row_lock_time */
	case MONITOR_OVLD_LOCK_WAIT_TIME:
		value = srv_stats.n_lock_wait_time / 1000;
		break;

	/* innodb_row_lock_time_max */
	case MONITOR_OVLD_LOCK_MAX_WAIT_TIME:
		value = lock_sys->n_lock_max_wait_time / 1000;
		break;

	/* innodb_row_lock_time_avg */
	case MONITOR_OVLD_LOCK_AVG_WAIT_TIME:
		if (srv_stats.n_lock_wait_count > 0) {
			value = srv_stats.n_lock_wait_time / 1000
				/ srv_stats.n_lock_wait_count;
		} else {
			value = 0;
		}
		break;

	/* innodb_row_lock_waits */
	case MONITOR_OVLD_ROW_LOCK_WAIT:
		value = srv_stats.n_lock_wait_count;
		break;

	case MONITOR_RSEG_HISTORY_LEN:
		value = trx_sys->rseg_history_len;
		break;

	case MONITOR_RSEG_CUR_SIZE:
		value = srv_mon_get_rseg_size();
		break;

	case MONITOR_OVLD_N_FILE_OPENED:
		value = fil_n_file_opened;
		break;

	case MONITOR_OVLD_IBUF_MERGE_INSERT:
		value = ibuf->n_merged_ops[IBUF_OP_INSERT];
		break;

	case MONITOR_OVLD_IBUF_MERGE_DELETE:
		value = ibuf->n_merged_ops[IBUF_OP_DELETE_MARK];
		break;

	case MONITOR_OVLD_IBUF_MERGE_PURGE:
		value = ibuf->n_merged_ops[IBUF_OP_DELETE];
		break;

	case MONITOR_OVLD_IBUF_MERGE_DISCARD_INSERT:
		value = ibuf->n_discarded_ops[IBUF_OP_INSERT];
		break;

	case MONITOR_OVLD_IBUF_MERGE_DISCARD_DELETE:
		value = ibuf->n_discarded_ops[IBUF_OP_DELETE_MARK];
		break;

	case MONITOR_OVLD_IBUF_MERGE_DISCARD_PURGE:
		value = ibuf->n_discarded_ops[IBUF_OP_DELETE];
		break;

	case MONITOR_OVLD_IBUF_MERGES:
		value = ibuf->n_merges;
		break;

	case MONITOR_OVLD_IBUF_SIZE:
		value = ibuf->size;
		break;

	case MONITOR_OVLD_SERVER_ACTIVITY:
		value = srv_get_activity_count();
		break;

	case MONITOR_OVLD_LSN_FLUSHDISK:
		value = (mon_type_t) log_sys->flushed_to_disk_lsn;
		break;

	case MONITOR_OVLD_LSN_CURRENT:
		value = (mon_type_t) log_sys->lsn;
		break;

	case MONITOR_OVLD_BUF_OLDEST_LSN:
		value = (mon_type_t) buf_pool_get_oldest_modification();
		break;

	case MONITOR_OVLD_LSN_CHECKPOINT:
		value = (mon_type_t) log_sys->last_checkpoint_lsn;
		break;

	case MONITOR_OVLD_MAX_AGE_ASYNC:
		value = log_sys->max_modified_age_async;
		break;

	case MONITOR_OVLD_MAX_AGE_SYNC:
		value = log_sys->max_modified_age_sync;
		break;

	case MONITOR_OVLD_ADAPTIVE_HASH_SEARCH:
		value = btr_cur_n_sea;
		break;

	case MONITOR_OVLD_ADAPTIVE_HASH_SEARCH_BTREE:
		value = btr_cur_n_non_sea;
		break;

	default:
		ut_error;
	}

	switch (set_option) {
	case MONITOR_TURN_ON:
		/* Save the initial counter value in mon_start_value
		field */
		MONITOR_SAVE_START(monitor_id, value);
		return;

	case MONITOR_TURN_OFF:
		/* Save the counter value to mon_last_value when we
		turn off the monitor but not yet reset. Note the
		counter has not yet been set to off in the bitmap
		table for normal turn off. We need to check the
		count status (on/off) to avoid reset the value
		for an already off conte */
		if (MONITOR_IS_ON(monitor_id)) {
			srv_mon_process_existing_counter(monitor_id,
							 MONITOR_GET_VALUE);
			MONITOR_SAVE_LAST(monitor_id);
		}
		return;

	case MONITOR_GET_VALUE:
		if (MONITOR_IS_ON(monitor_id)) {

			/* If MONITOR_DISPLAY_CURRENT bit is on, we
			only record the current value, rather than
			incremental value over a period. Most of
`			this type of counters are resource related
			counters such as number of buffer pages etc. */
			if (monitor_info->monitor_type
			    & MONITOR_DISPLAY_CURRENT) {
				MONITOR_SET(monitor_id, value);
			} else {
				/* Most status counters are montonically
				increasing, no need to update their
				minimum values. Only do so
				if "update_min" set to TRUE */
				MONITOR_SET_DIFF(monitor_id, value);

				if (update_min
				    && (MONITOR_VALUE(monitor_id)
					< MONITOR_MIN_VALUE(monitor_id))) {
					MONITOR_MIN_VALUE(monitor_id) =
						MONITOR_VALUE(monitor_id);
				}
			}
		}
		return;

	case MONITOR_RESET_VALUE:
		if (!MONITOR_IS_ON(monitor_id)) {
			MONITOR_LAST_VALUE(monitor_id) = 0;
		}
		return;

	/* Nothing special for reset all operation for these existing
	counters */
	case MONITOR_RESET_ALL_VALUE:
		return;
	}
}

/*************************************************************//**
Reset a monitor, create a new base line with the current monitor
value. This baseline is recorded by MONITOR_VALUE_RESET(monitor) */
UNIV_INTERN
void
srv_mon_reset(
/*==========*/
	monitor_id_t	monitor)	/*!< in: monitor id */
{
	ibool	monitor_was_on;

	monitor_was_on = MONITOR_IS_ON(monitor);

	if (monitor_was_on) {
		/* Temporarily turn off the counter for the resetting
		operation */
		MONITOR_OFF(monitor);
	}

	/* Before resetting the current monitor value, first
	calculate and set the max/min value since monitor
	start */
	srv_mon_calc_max_since_start(monitor);
	srv_mon_calc_min_since_start(monitor);

	/* Monitors with MONITOR_DISPLAY_CURRENT bit
	are not incremental, no need to remember
	the reset value. */
	if (innodb_counter_info[monitor].monitor_type
	    & MONITOR_DISPLAY_CURRENT) {
		MONITOR_VALUE_RESET(monitor) = 0;
	} else {
		/* Remember the new baseline */
		MONITOR_VALUE_RESET(monitor) = MONITOR_VALUE_RESET(monitor)
					       + MONITOR_VALUE(monitor);
	}

	/* Reset the counter value */
	MONITOR_VALUE(monitor) = 0;
	MONITOR_MAX_VALUE(monitor) = MAX_RESERVED;
	MONITOR_MIN_VALUE(monitor) = MIN_RESERVED;

	MONITOR_FIELD((monitor), mon_reset_time) = time(NULL);

	if (monitor_was_on) {
		MONITOR_ON(monitor);
	}
}

/*************************************************************//**
Turn on monitor counters that are marked as default ON. */
UNIV_INTERN
void
srv_mon_default_on(void)
/*====================*/
{
	ulint   ix;

	for (ix = 0; ix < NUM_MONITOR; ix++) {
		if (innodb_counter_info[ix].monitor_type
		    & MONITOR_DEFAULT_ON) {
			/* Turn on monitor counters that are default on */
			MONITOR_ON(ix);
			MONITOR_INIT(ix);
			MONITOR_SET_START(ix);
		}
	}
}
#endif /* !UNIV_HOTBACKUP */
