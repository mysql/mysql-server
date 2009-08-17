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

#ifndef __xactlog_xt_h__
#define __xactlog_xt_h__

#include "pthread_xt.h"
#include "filesys_xt.h"
#include "sortedlist_xt.h"

struct XTThread;
struct XTOpenTable;
struct XTDatabase;

#ifdef DEBUG
//#define XT_USE_CACHE_DEBUG_SIZES
#endif

#ifdef XT_USE_CACHE_DEBUG_SIZES
#define XT_XLC_BLOCK_SHIFTS			5
#define XT_XLC_FILE_SLOTS			7
#define XT_XLC_SEGMENT_SHIFTS		1
#define XT_XLC_MAX_FLUSH_SEG_COUNT	10
#define XT_XLC_MAX_FREE_COUNT		10
#else
/* Block size is determined by the number of shifts 1 << 15 = 32K */
#define XT_XLC_BLOCK_SHIFTS			15
#define XT_XLC_FILE_SLOTS			71
/* The number of segments are determined by the segment shifts 1 << 3 = 8 */
#define XT_XLC_SEGMENT_SHIFTS		3
#define XT_XLC_MAX_FLUSH_SEG_COUNT	250
#define XT_XLC_MAX_FREE_COUNT		100
#endif

#define XT_XLC_BLOCK_SIZE			(1 << XT_XLC_BLOCK_SHIFTS)
#define XT_XLC_BLOCK_MASK			(XT_XLC_BLOCK_SIZE - 1)

#define XT_TIME_DIFF(start, now) (\
	((xtWord4) (now) < (xtWord4) (start)) ? \
	((xtWord4) 0XFFFFFFFF - ((xtWord4) (start) - (xtWord4) (now))) : \
	((xtWord4) (now) - (xtWord4) (start)))

#define XLC_SEGMENT_COUNT			((off_t) 1 << XT_XLC_SEGMENT_SHIFTS)
#define XLC_SEGMENT_MASK			(XLC_SEGMENT_COUNT - 1)
#define XLC_MAX_FLUSH_COUNT			(XT_XLC_MAX_FLUSH_SEG_COUNT * XLC_SEGMENT_COUNT)

#define XLC_BLOCK_FREE				0
#define XLC_BLOCK_READING			1
#define XLC_BLOCK_CLEAN				2

#define XT_RECYCLE_LOGS				0
#define XT_DELETE_LOGS				1
#define XT_KEEP_LOGS				2

/* LOG CACHE ---------------------------------------------------- */

typedef struct XTXLogBlock {
	off_t					xlb_address;					/* The block address. */
	xtLogID					xlb_log_id;						/* The log id of the block. */
	xtWord4					xlb_state;						/* Block status. */
	struct XTXLogBlock		*xlb_next;						/* Pointer to next block on hash list, or next free block on free list. */
	xtWord1					xlb_data[XT_XLC_BLOCK_SIZE];
} XTXLogBlockRec, *XTXLogBlockPtr;

/* A disk cache segment. The cache is divided into a number of segments
 * to improve concurrency.
 */
typedef struct XTXLogCacheSeg {
	xt_mutex_type			lcs_lock;						/* The cache segment lock. */
	xt_cond_type			lcs_cond;
	XTXLogBlockPtr			*lcs_hash_table;
} XTXLogCacheSegRec, *XTXLogCacheSegPtr;

typedef struct XTXLogCache {
	xt_mutex_type			xlc_lock;						/* The public cache lock. */
	xt_cond_type			xlc_cond;						/* The public cache wait condition. */
	XTXLogCacheSegRec		xlc_segment[XLC_SEGMENT_COUNT];
	XTXLogBlockPtr			xlc_blocks;
	XTXLogBlockPtr			xlc_blocks_end;
	XTXLogBlockPtr			xlc_next_to_free;
	xtWord4					xlc_free_count;
	xtWord4					xlc_hash_size;
	xtWord4					xlc_block_count;
	xtWord8					xlc_upper_limit;
} XTXLogCacheRec;

/* LOG ENTRIES ---------------------------------------------------- */

#define XT_LOG_ENT_EOF				0
#define XT_LOG_ENT_HEADER			1
#define XT_LOG_ENT_NEW_LOG			2					/* Move to the next log! NOTE!! May not appear in a group!! */
#define XT_LOG_ENT_DEL_LOG			3					/* Delete the given transaction/data log. */
#define XT_LOG_ENT_NEW_TAB			4					/* This record indicates a new table was created. */

#define XT_LOG_ENT_COMMIT			5					/* Transaction was committed. */
#define XT_LOG_ENT_ABORT			6					/* Transaction was aborted. */
#define XT_LOG_ENT_CLEANUP			7					/* Written after a cleanup. */

#define XT_LOG_ENT_REC_MODIFIED		8					/* This records has been modified by the transaction. */
#define XT_LOG_ENT_UPDATE			9
#define XT_LOG_ENT_UPDATE_BG		10
#define XT_LOG_ENT_UPDATE_FL		11
#define XT_LOG_ENT_UPDATE_FL_BG		12
#define XT_LOG_ENT_INSERT			13
#define XT_LOG_ENT_INSERT_BG		14
#define XT_LOG_ENT_INSERT_FL		15
#define XT_LOG_ENT_INSERT_FL_BG		16
#define XT_LOG_ENT_DELETE			17
#define XT_LOG_ENT_DELETE_BG		18
#define XT_LOG_ENT_DELETE_FL		19
#define XT_LOG_ENT_DELETE_FL_BG		20

#define XT_LOG_ENT_REC_FREED		21					/* This record has been placed in the free list. */
#define XT_LOG_ENT_REC_REMOVED		22					/* Free record and dependecies: index references, blob references. */
#define XT_LOG_ENT_REC_REMOVED_EXT	23					/* Free record and dependecies: index references, extended data, blob references. */
#define XT_LOG_ENT_REC_REMOVED_BI	38					/* Free record and dependecies: includes before image of record, for freeing index, etc. */

#define XT_LOG_ENT_REC_MOVED		24					/* The record has been moved by the compactor. */
#define XT_LOG_ENT_REC_CLEANED		25					/* This record has been cleaned by the sweeper. */
#define XT_LOG_ENT_REC_CLEANED_1	26					/* This record has been cleaned by the sweeper (short form). */
#define XT_LOG_ENT_REC_UNLINKED		27					/* The record after this record is unlinked from the variation list. */

#define XT_LOG_ENT_ROW_NEW			28					/* Row allocated from the EOF. */
#define XT_LOG_ENT_ROW_NEW_FL		29					/* Row allocated from the free list. */
#define XT_LOG_ENT_ROW_ADD_REC		30					/* Record added to the row. */
#define XT_LOG_ENT_ROW_SET			31
#define XT_LOG_ENT_ROW_FREED		32

#define XT_LOG_ENT_OP_SYNC			33					/* Operations syncronised. */
#define XT_LOG_ENT_EXT_REC_OK		34					/* An extended record */
#define XT_LOG_ENT_EXT_REC_DEL		35					/* A deleted extended record */

#define XT_LOG_ENT_NO_OP			36					/* If write to the database fails, we still try to log the
														 * op code, in an attempt to continue, if writting to log
														 * still works.
														 */
#define XT_LOG_ENT_END_OF_LOG		37					/* This is a record that indicates the end of the log, and
														 * fills to the end of a 512 byte block.
														 */

#define XT_LOG_FILE_MAGIC			0xAE88FE12
#define XT_LOG_VERSION_NO			1

typedef struct XTXactLogHeader {
	xtWord1					xh_status_1;		/* XT_LOG_ENT_HEADER */
	xtWord1					xh_checksum_1;		
	XTDiskValue4			xh_size_4;			/* Must be set to sizeof(XTXactLogHeaderDRec). */
	XTDiskValue8			xh_free_space_8;	/* The accumulated free space in this file. */
	XTDiskValue8			xh_file_len_8;		/* The last confirmed correct file length (always set on close). */
	XTDiskValue8			xh_comp_pos_8;		/* Compaction position (XT_DL_STATUS_CO_SOURCE only). */
	xtWord1					xh_comp_stat_1;		/* The compaction status XT_DL_STATUS_CO_SOURCE/XT_DL_STATUS_CO_TARGET */
	XTDiskValue4			xh_log_id_4;
	XTDiskValue4			xh_version_2;		/* XT_LOG_VERSION_NO */
	XTDiskValue4			xh_magic_4;			/* MUST always be at the end of the structure!! */
} XTXactLogHeaderDRec, *XTXactLogHeaderDPtr;

/* This is the original log head size (don't change): */
#define XT_MIN_LOG_HEAD_SIZE		(offsetof(XTXactLogHeaderDRec, xh_log_id_4) + 4)
#define XT_LOG_HEAD_MAGIC(b, l)		XT_GET_DISK_4(((xtWord1 *) (b)) + (l) - 4)

typedef struct XTXactNewLogEntry {
	xtWord1					xl_status_1;		/* XT_LOG_ENT_NEW_LOG, XT_LOG_ENT_DEL_LOG */
	xtWord1					xl_checksum_1;		
	XTDiskValue4			xl_log_id_4;		/* Store the current table ID. */
} XTXactNewLogEntryDRec, *XTXactNewLogEntryDPtr;

typedef struct XTXactNewTabEntry {
	xtWord1					xt_status_1;		/* XT_LOG_ENT_NEW_TAB */
	xtWord1					xt_checksum_1;		
	XTDiskValue4			xt_tab_id_4;		/* Store the current table ID. */
} XTXactNewTabEntryDRec, *XTXactNewTabEntryDPtr;

/* This record must appear in a transaction group, and therefore has no transaction ID: */
typedef struct XTXactEndEntry {
	xtWord1					xe_status_1;		/* XT_LOG_ENT_COMMIT, XT_LOG_ENT_ABORT */
	xtWord1					xe_checksum_1;		
	XTDiskValue4			xe_xact_id_4;		/* The transaction. */
	XTDiskValue4			xe_not_used_4;		/* Was the end sequence number (no longer used - v1.0.04+), set to zero). */
} XTXactEndEntryDRec, *XTXactEndEntryDPtr;

typedef struct XTXactCleanupEntry {
	xtWord1					xc_status_1;		/* XT_LOG_ENT_CLEANUP */
	xtWord1					xc_checksum_1;		
	XTDiskValue4			xc_xact_id_4;		/* The transaction that was cleaned up. */
} XTXactCleanupEntryDRec, *XTXactCleanupEntryDPtr;

typedef struct XTactUpdateEntry {
	xtWord1					xu_status_1;		/* XT_LOG_ENT_REC_MODIFIED, XT_LOG_ENT_UPDATE, XT_LOG_ENT_INSERT, XT_LOG_ENT_DELETE */
												/* XT_LOG_ENT_UPDATE_BG, XT_LOG_ENT_INSERT_BG, XT_LOG_ENT_DELETE_BG */
	XTDiskValue2			xu_checksum_2;		
	XTDiskValue4			xu_op_seq_4;		/* Operation sequence number. */
	XTDiskValue4			xu_tab_id_4;		/* Table ID of the record. */
	xtDiskRecordID4			xu_rec_id_4;		/* Offset of the new updated record. */
	XTDiskValue2			xu_size_2;			/* Size of the record data. */
	/* This is the start of the actual record data: */
	xtWord1					xu_rec_type_1;		/* Type of the record. */
	xtWord1					xu_stat_id_1;
	xtDiskRecordID4			xu_prev_rec_id_4;		/* The previous variation of this record. */
	XTDiskValue4			xu_xact_id_4;		/* The transaction ID. */
	XTDiskValue4			xu_row_id_4;		/* The row ID of this record. */
} XTactUpdateEntryDRec, *XTactUpdateEntryDPtr;

typedef struct XTactUpdateFLEntry {
	xtWord1					xf_status_1;		/* XT_LOG_ENT_UPDATE_FL, XT_LOG_ENT_INSERT_FL, XT_LOG_ENT_DELETE_FL */
												/* XT_LOG_ENT_UPDATE_FL_BG, XT_LOG_ENT_INSERT_FL_BG, XT_LOG_ENT_DELETE_FL_BG */
	XTDiskValue2			xf_checksum_2;		
	XTDiskValue4			xf_op_seq_4;		/* Operation sequence number. */
	XTDiskValue4			xf_tab_id_4;		/* Table ID of the record. */
	xtDiskRecordID4			xf_rec_id_4;		/* Offset of the new updated record. */
	XTDiskValue2			xf_size_2;			/* Size of the record data. */
	xtDiskRecordID4			xf_free_rec_id_4;	/* Update to the free list. */
	/* This is the start of the actual record data: */
	xtWord1					xf_rec_type_1;		/* Type of the record. */
	xtWord1					xf_stat_id_1;
	xtDiskRecordID4			xf_prev_rec_id_4;	/* The previous variation of this record. */
	XTDiskValue4			xf_xact_id_4;		/* The transaction ID. */
	XTDiskValue4			xf_row_id_4;		/* The row ID of this record. */
} XTactUpdateFLEntryDRec, *XTactUpdateFLEntryDPtr;

typedef struct XTactFreeRecEntry {
	xtWord1					fr_status_1;		/* XT_LOG_ENT_REC_REMOVED, XT_LOG_ENT_REC_REMOVED_EXT, XT_LOG_ENT_REC_FREED */
	xtWord1					fr_checksum_1;		
	XTDiskValue4			fr_op_seq_4;		/* Operation sequence number. */
	XTDiskValue4			fr_tab_id_4;		/* Table ID of the record. */
	xtDiskRecordID4			fr_rec_id_4;		/* Offset of the new written record. */
	/* This data confirms the record state for release of
	 * attached resources (extended records, indexes and blobs)
	 */
	xtWord1					fr_stat_id_1;		/* The statement ID of the record. */
	XTDiskValue4			fr_xact_id_4;		/* The transaction ID of the record. */
	/* This is the start of the actual record data: */
	xtWord1					fr_rec_type_1;
	xtWord1					fr_not_used_1;
	xtDiskRecordID4			fr_next_rec_id_4;	/* The next block on the free list. */
} XTactFreeRecEntryDRec, *XTactFreeRecEntryDPtr;

typedef struct XTactRemoveBIEntry {
	xtWord1					rb_status_1;		/* XT_LOG_ENT_REC_REMOVED_BI */
	XTDiskValue2			rb_checksum_2;		
	XTDiskValue4			rb_op_seq_4;		/* Operation sequence number. */
	XTDiskValue4			rb_tab_id_4;		/* Table ID of the record. */
	xtDiskRecordID4			rb_rec_id_4;		/* Offset of the new written record. */
	XTDiskValue2			rb_size_2;			/* Size of the record data. */

	xtWord1					rb_new_rec_type_1;	/* New type of the record (needed for below). */

	/* This is the start of the record data, with some fields overwritten for the free: */
	xtWord1					rb_rec_type_1;		/* Type of the record. */
	xtWord1					rb_stat_id_1;
	xtDiskRecordID4			rb_next_rec_id_4;	/* The next block on the free list (overwritten). */
	XTDiskValue4			rb_xact_id_4;		/* The transaction ID. */
	XTDiskValue4			rb_row_id_4;		/* The row ID of this record. */
} XTactRemoveBIEntryDRec, *XTactRemoveBIEntryDPtr;

typedef struct XTactWriteRecEntry {
	xtWord1					xw_status_1;		/* XT_LOG_ENT_REC_MOVED, XT_LOG_ENT_REC_CLEANED, XT_LOG_ENT_REC_CLEANED_1,
												 * XT_LOG_ENT_REC_UNLINKED */
	xtWord1					xw_checksum_1;		
	XTDiskValue4			xw_op_seq_4;		/* Operation sequence number. */
	XTDiskValue4			xw_tab_id_4;		/* Table ID of the record. */
	xtDiskRecordID4			xw_rec_id_4;		/* Offset of the new written record. */
	/* This is the start of the actual record data: */
	xtWord1					xw_rec_type_1;
	xtWord1					xw_stat_id_1;
	xtDiskRecordID4			xw_next_rec_id_4;	/* The next block on the free list. */
} XTactWriteRecEntryDRec, *XTactWriteRecEntryDPtr;

typedef struct XTactRowAddedEntry {
	xtWord1					xa_status_1;		/* XT_LOG_ENT_ROW_NEW or XT_LOG_ENT_ROW_NEW_FL */
	xtWord1					xa_checksum_1;		
	XTDiskValue4			xa_op_seq_4;		/* Operation sequence number. */
	XTDiskValue4			xa_tab_id_4;		/* Table ID of the record. */
	XTDiskValue4			xa_row_id_4;		/* The row ID of the row allocated. */
	XTDiskValue4			xa_free_list_4;		/* Change to the free list (ONLY for XT_LOG_ENT_ROW_NEW_FL). */
} XTactRowAddedEntryDRec, *XTactRowAddedEntryDPtr;

typedef struct XTactWriteRowEntry {
	xtWord1					wr_status_1;		/* XT_LOG_ENT_ROW_ADD_REC, XT_LOG_ENT_ROW_SET, XT_LOG_ENT_ROW_FREED */
	xtWord1					wr_checksum_1;		
	XTDiskValue4			wr_op_seq_4;		/* Operation sequence number. */
	XTDiskValue4			wr_tab_id_4;		/* Table ID of the record. */
	XTDiskValue4			wr_row_id_4;		/* Row ID of the row that was modified. */
	/* This is the start of the actual record data: */
	XTDiskValue4			wr_ref_id_4;		/* The row reference data. */
} XTactWriteRowEntryDRec, *XTactWriteRowEntryDPtr;

typedef struct XTactOpSyncEntry {
	xtWord1					os_status_1;		/* XT_LOG_ENT_OP_SYNC  */
	xtWord1					os_checksum_1;		
	XTDiskValue4			os_time_4;			/* Time of the restart. */
} XTactOpSyncEntryDRec, *XTactOpSyncEntryDPtr;

typedef struct XTactNoOpEntry {
	xtWord1					no_status_1;		/* XT_LOG_ENT_NO_OP */
	xtWord1					no_checksum_1;		
	XTDiskValue4			no_op_seq_4;		/* Operation sequence number. */
	XTDiskValue4			no_tab_id_4;		/* Table ID of the record. */
} XTactNoOpEntryDRec, *XTactNoOpEntryDPtr;

typedef struct XTactExtRecEntry {
	xtWord1					er_status_1;		/* XT_LOG_ENT_EXT_REC_OK, XT_LOG_ENT_EXT_REC_DEL */
	XTDiskValue4			er_data_size_4;		/* Size of this record data area only. */
	XTDiskValue4			er_tab_id_4;		/* The table referencing this extended record. */
	xtDiskRecordID4			er_rec_id_4;		/* The ID of the reference record. */
	xtWord1					er_data[XT_VAR_LENGTH];
} XTactExtRecEntryDRec, *XTactExtRecEntryDPtr;

typedef union XTXactLogBuffer {
	XTXactLogHeaderDRec		xh;
	XTXactNewLogEntryDRec	xl;
	XTXactNewTabEntryDRec	xt;
	XTXactEndEntryDRec		xe;
	XTXactCleanupEntryDRec	xc;
	XTactUpdateEntryDRec	xu;
	XTactUpdateFLEntryDRec	xf;
	XTactFreeRecEntryDRec	fr;
	XTactRemoveBIEntryDRec	rb;
	XTactWriteRecEntryDRec	xw;
	XTactRowAddedEntryDRec	xa;
	XTactWriteRowEntryDRec	wr;
	XTactOpSyncEntryDRec	os;
	XTactExtRecEntryDRec	er;
	XTactNoOpEntryDRec		no;
} XTXactLogBufferDRec, *XTXactLogBufferDPtr;

/* ---------------------------------------- */

typedef struct XTXactSeqRead {
	size_t					xseq_buffer_size;		/* Size of the buffer. */
	xtBool					xseq_load_cache;		/* TRUE if reads should load the cache! */

	xtLogID					xseq_log_id;
	XTOpenFilePtr			xseq_log_file;
	off_t					xseq_log_eof;

	xtLogOffset				xseq_buf_log_offset;	/* File offset of the buffer. */
	size_t					xseq_buffer_len;		/* Amount of data in the buffer. */
	xtWord1					*xseq_buffer;

	xtLogID					xseq_rec_log_id;		/* The current record log ID. */
	xtLogOffset				xseq_rec_log_offset;	/* The current log read position. */
	size_t					xseq_record_len;		/* The length of the current record. */
} XTXactSeqReadRec, *XTXactSeqReadPtr;

typedef struct XTXactLogFile {
	xtLogID					lf_log_id;
	off_t					lr_file_len;					/* The log file size (0 means this is the last log) */
} XTXactLogFileRec, *XTXactLogFilePtr;

/*
 * The transaction log. Each database has one.
 */
 
/* Does not seem to make much difference... */
#ifndef XT_NO_ATOMICS
/* This function uses atomic ops: */
//#define XT_XLOG_WAIT_SPINS
#endif

typedef struct XTDatabaseLog {
	struct XTDatabase		*xl_db;

	off_t					xl_log_file_threshold;
	u_int					xl_log_file_count;				/* Number of logs to use (>= 1). */
	u_int					xt_log_file_dyn_count;			/* A dynamic value to add to log file count. */
	u_int					xt_log_file_dyn_dec;			/* Used to descide when to decrement the dynamic count. */
	size_t					xl_size_of_buffers;				/* The size of both log buffers. */
	xtWord8					xl_log_bytes_written;			/* The total number of bytes written to the log, after recovery. */
	xtWord8					xl_log_bytes_flushed;			/* The total number of bytes flushed to the log, after recovery. */
	xtWord8					xl_log_bytes_read;				/* The total number of log bytes read, after recovery. */

	u_int					xl_last_flush_time;				/* Last flush time in micro-seconds. */

	/* The writer log buffer: */
	xt_mutex_type			xl_write_lock;
	xt_cond_type			xl_write_cond;
#ifdef XT_XLOG_WAIT_SPINS
	xtWord4					xt_writing;						/* 1 if a thread is writing. */
	xtWord4					xt_waiting;						/* Count of the threads waiting on the xl_write_cond. */
#else
	xtBool					xt_writing;						/* TRUE if a thread is writing. */
#endif
	xtLogID					xl_log_id;						/* The number of the write log. */
	XTOpenFilePtr			xl_log_file;					/* The open write log. */

	XTSpinLockRec			xl_buffer_lock;					/* This locks both the write and the append log buffers. */

	xtLogID					xl_max_log_id;					/* The ID of the highest log on disk. */

	xtLogID					xl_write_log_id;				/* This is the log ID were the write data will go. */
	xtLogOffset				xl_write_log_offset;			/* The file offset of the write log. */
	size_t					xl_write_buf_pos;
	size_t					xl_write_buf_pos_start;
	xtWord1					*xl_write_buffer;
	xtBool					xl_write_done;					/* TRUE if the write buffer has been written! */

	xtLogID					xl_append_log_id;				/* This is the log ID were the append data will go. */
	xtLogOffset				xl_append_log_offset;			/* The file offset in the log were the append data will go. */
	size_t					xl_append_buf_pos;				/* The amount of data in the append buffer. */
	size_t					xl_append_buf_pos_start;		/* The amount of data in the append buffer already written. */
	xtWord1					*xl_append_buffer;

	xtLogID					xl_flush_log_id;				/* The last log flushed. */
	xtLogOffset				xl_flush_log_offset;			/* The position in the log flushed. */

	void					xlog_setup(struct XTThread *self, struct XTDatabase *db, off_t log_file_size, size_t transaction_buffer_size, int log_count);
	xtBool					xlog_set_write_offset(xtLogID log_id, xtLogOffset log_offset, xtLogID max_log_id, struct XTThread *thread);
	void					xlog_close(struct XTThread *self);
	void					xlog_exit(struct XTThread *self);
	void					xlog_name(size_t size, char *path, xtLogID log_id);
	int						xlog_delete_log(xtLogID del_log_id, struct XTThread *thread);

	xtBool					xlog_append(struct XTThread *thread, size_t size1, xtWord1 *data1, size_t size2, xtWord1 *data2, xtBool commit, xtLogID *log_id, xtLogOffset *log_offset);
	xtBool					xlog_flush(struct XTThread *thread);
	xtBool					xlog_flush_pending();

	xtBool					xlog_seq_init(XTXactSeqReadPtr seq, size_t buffer_size, xtBool load_cache);
	void					xlog_seq_exit(XTXactSeqReadPtr seq);
	void					xlog_seq_close(XTXactSeqReadPtr seq);
	xtBool					xlog_seq_start(XTXactSeqReadPtr seq, xtLogID log_id, xtLogOffset log_offset, xtBool missing_ok);
	xtBool					xlog_rnd_read(XTXactSeqReadPtr seq, xtLogID log_id, xtLogOffset log_offset, size_t size, xtWord1 *data, size_t *read, struct XTThread *thread);
	size_t					xlog_bytes_to_write();
	xtBool					xlog_read_from_cache(XTXactSeqReadPtr seq, xtLogID log_id, xtLogOffset log_offset, size_t size, off_t eof, xtWord1 *buffer, size_t *data_read, struct XTThread *thread);
	xtBool					xlog_write_thru(XTXactSeqReadPtr seq, size_t size, xtWord1 *data, struct XTThread *thread);
	xtBool					xlog_verify(XTXactLogBufferDPtr record, size_t rec_size, xtLogID log_id);
	xtBool					xlog_seq_next(XTXactSeqReadPtr seq, XTXactLogBufferDPtr *entry, xtBool verify, struct XTThread *thread);
	void					xlog_seq_skip(XTXactSeqReadPtr seq, size_t size);

private:
	xtBool					xlog_open_log(xtLogID log_id, off_t curr_eof, struct XTThread *thread);
} XTDatabaseLogRec, *XTDatabaseLogPtr;

xtBool			xt_xlog_flush_log(struct XTThread *thread);
xtBool			xt_xlog_log_data(struct XTThread *thread, size_t len, XTXactLogBufferDPtr log_entry, xtBool commit);
xtBool			xt_xlog_modify_table(struct XTOpenTable *ot, u_int status, xtOpSeqNo op_seq, xtRecordID free_list, xtRecordID address, size_t size, xtWord1 *data);

void			xt_xlog_init(struct XTThread *self, size_t cache_size);
void			xt_xlog_exit(struct XTThread *self);
xtInt8			xt_xlog_get_usage();
xtInt8			xt_xlog_get_size();
xtLogID			xt_xlog_get_min_log(struct XTThread *self, struct XTDatabase *db);
void			xt_xlog_delete_logs(struct XTThread *self, struct XTDatabase *db);

void			xt_start_writer(struct XTThread *self, struct XTDatabase *db);
void			xt_wait_for_writer(struct XTThread *self, struct XTDatabase *db);
void			xt_stop_writer(struct XTThread *self, struct XTDatabase *db);

#endif

