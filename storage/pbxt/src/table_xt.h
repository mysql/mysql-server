/* Copyright (c) 2005 PrimeBase Technologies GmbH
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
 * 2005-02-08	Paul McCullagh
 *
 * H&G2JCtL
 */
#ifndef __xt_table_h__
#define __xt_table_h__

#include <time.h>

#include "datalog_xt.h"
#include "filesys_xt.h"
#include "hashtab_xt.h"
#include "index_xt.h"
#include "cache_xt.h"
#include "util_xt.h"
#include "heap_xt.h"
#include "tabcache_xt.h"
#include "xactlog_xt.h"
#include "lock_xt.h"

struct XTDatabase;
struct XTThread;
struct XTCache;
struct XTOpenTable;
struct XTTablePath;

#define XT_TAB_INCOMPATIBLE_VERSION	4
#define XT_TAB_CURRENT_VERSION		5

/* This version of the index does not have lazy
 * delete. The new version is compatible with
 * this and maintains the old format.
 */
#define XT_IND_NO_LAZY_DELETE		3
#define XT_IND_LAZY_DELETE_OK		4
#ifdef XT_USE_LAZY_DELETE
#define XT_IND_CURRENT_VERSION		XT_IND_LAZY_DELETE_OK
#else
#define XT_IND_CURRENT_VERSION		XT_IND_NO_LAZY_DELETE
#endif

#define XT_HEAD_BUFFER_SIZE			1024

#ifdef DEBUG
//#define XT_TRACK_INDEX_UPDATES
//#define XT_TRACK_RETURNED_ROWS
#endif

/*
 * NOTE: Records may only be freed (placed on the free list), after
 * all currently running transactions have ended.
 * The reason is, running transactions may have references in memory
 * to these records (a sequential scan has a large buffer).
 * If the records are freed they may be re-used. This will
 * cause problems because the references will then refer to
 * new data.
 *
 * As a result, deleted records are first placed in the
 * REMOVED state. Later, when transactions have quit, they
 * are freed.
 */
#define XT_TAB_STATUS_FREED			0x00			/* On the free list. */
#define XT_TAB_STATUS_DELETE		0x01			/* A transactional delete record (an "update" that indicates a delete). */
#define XT_TAB_STATUS_FIXED			0x02
#define XT_TAB_STATUS_VARIABLE		0x03			/* Uses one block, but has the variable format. */
#define XT_TAB_STATUS_EXT_DLOG		0x04			/* Variable format, and the trailing part of the record in the data log. */
#define XT_TAB_STATUS_EXT_HDATA		0x05			/* Variable format, and the trailing part of the record in the handle data file. */
#define XT_TAB_STATUS_DATA			0x06			/* A block of data with a next pointer (5 bytes overhead). */
#define XT_TAB_STATUS_END_DATA		0x07			/* An block of data without an end pointer (1 byte overhead). */
#define XT_TAB_STATUS_MASK			0x0F

#define XT_TAB_STATUS_DEL_CLEAN		(XT_TAB_STATUS_DELETE | XT_TAB_STATUS_CLEANED_BIT)
#define XT_TAB_STATUS_FIX_CLEAN		(XT_TAB_STATUS_FIXED | XT_TAB_STATUS_CLEANED_BIT)
#define XT_TAB_STATUS_VAR_CLEAN		(XT_TAB_STATUS_VARIABLE | XT_TAB_STATUS_CLEANED_BIT)
#define XT_TAB_STATUS_EXT_CLEAN		(XT_TAB_STATUS_EXT_DLOG | XT_TAB_STATUS_CLEANED_BIT)

#define XT_TAB_STATUS_CLEANED_BIT	0x80			/* This bit is set when the record is cleaned and committed. */

#define XT_REC_IS_CLEAN(x)			((x) & XT_TAB_STATUS_CLEANED_BIT)
#define XT_REC_IS_FREE(x)			(((x) & XT_TAB_STATUS_MASK) == XT_TAB_STATUS_FREED)
#define XT_REC_IS_DELETE(x)			(((x) & XT_TAB_STATUS_MASK) == XT_TAB_STATUS_DELETE)
#define XT_REC_IS_FIXED(x)			(((x) & XT_TAB_STATUS_MASK) == XT_TAB_STATUS_FIXED)
#define XT_REC_IS_VARIABLE(x)		(((x) & XT_TAB_STATUS_MASK) == XT_TAB_STATUS_VARIABLE)
#define XT_REC_IS_EXT_DLOG(x)		(((x) & XT_TAB_STATUS_MASK) == XT_TAB_STATUS_EXT_DLOG)
#define XT_REC_IS_EXT_HDATA(x)		(((x) & XT_TAB_STATUS_MASK) == XT_TAB_STATUS_EXT_HDATA)
#define XT_REC_NOT_VALID(x)			(XT_REC_IS_FREE(x) || XT_REC_IS_DELETE(x))

/* Results for xt_use_table_by_id(): */
#define XT_TAB_OK					0
#define XT_TAB_NOT_FOUND			1
#define XT_TAB_NO_DICTIONARY		2
#define XT_TAB_POOL_CLOSED			3				/* Cannot open table at the moment, the pool is closed. */
#define XT_TAB_FAILED				4

#ifdef XT_NO_ATOMICS
#define XT_TAB_ROW_USE_PTHREAD_RW
#else
//#define XT_TAB_ROW_USE_RWMUTEX
//#define XT_TAB_ROW_USE_SPINXSLOCK
#define XT_TAB_ROW_USE_XSMUTEX
#endif

#ifdef XT_TAB_ROW_USE_XSMUTEX
#define XT_TAB_ROW_LOCK_TYPE			XTXSMutexRec
#define XT_TAB_ROW_INIT_LOCK(s, i)		xt_xsmutex_init_with_autoname(s, i)
#define XT_TAB_ROW_FREE_LOCK(s, i)		xt_xsmutex_free(s, i)	
#define XT_TAB_ROW_READ_LOCK(i, s)		xt_xsmutex_slock(i, (s)->t_id)
#define XT_TAB_ROW_WRITE_LOCK(i, s)		xt_xsmutex_xlock(i, (s)->t_id)
#define XT_TAB_ROW_UNLOCK(i, s)			xt_xsmutex_unlock(i, (s)->t_id)
#elif defined(XT_TAB_ROW_USE_PTHREAD_RW)
#define XT_TAB_ROW_LOCK_TYPE			xt_rwlock_type
#define XT_TAB_ROW_INIT_LOCK(s, i)		xt_init_rwlock(s, i)
#define XT_TAB_ROW_FREE_LOCK(s, i)		xt_free_rwlock(i)	
#define XT_TAB_ROW_READ_LOCK(i, s)		xt_slock_rwlock_ns(i)
#define XT_TAB_ROW_WRITE_LOCK(i, s)		xt_xlock_rwlock_ns(i)
#define XT_TAB_ROW_UNLOCK(i, s)			xt_unlock_rwlock_ns(i)
#elif defined(XT_TAB_ROW_USE_RWMUTEX)
#define XT_TAB_ROW_LOCK_TYPE			XTRWMutexRec
#define XT_TAB_ROW_INIT_LOCK(s, i)		xt_rwmutex_init_with_autoname(s, i)
#define XT_TAB_ROW_FREE_LOCK(s, i)		xt_rwmutex_free(s, i)	
#define XT_TAB_ROW_READ_LOCK(i, s)		xt_rwmutex_slock(i, (s)->t_id)
#define XT_TAB_ROW_WRITE_LOCK(i, s)		xt_rwmutex_xlock(i, (s)->t_id)
#define XT_TAB_ROW_UNLOCK(i, s)			xt_rwmutex_unlock(i, (s)->t_id)
#elif defined(XT_TAB_ROW_USE_SPINXSLOCK)
#define XT_TAB_ROW_LOCK_TYPE			XTSpinXSLockRec
#define XT_TAB_ROW_INIT_LOCK(s, i)		xt_spinxslock_init_with_autoname(s, i)
#define XT_TAB_ROW_FREE_LOCK(s, i)		xt_spinxslock_free(s, i)	
#define XT_TAB_ROW_READ_LOCK(i, s)		xt_spinxslock_slock(i, (s)->t_id)
#define XT_TAB_ROW_WRITE_LOCK(i, s)		xt_spinxslock_xlock(i, (s)->t_id)
#define XT_TAB_ROW_UNLOCK(i, s)			xt_spinxslock_unlock(i, (s)->t_id)
#else
#define XT_TAB_ROW_LOCK_TYPE			XTSpinLockRec
#define XT_TAB_ROW_INIT_LOCK(s, i)		xt_spinlock_init_with_autoname(s, i)
#define XT_TAB_ROW_FREE_LOCK(s, i)		xt_spinlock_free(s, i)	
#define XT_TAB_ROW_READ_LOCK(i, s)		xt_spinlock_lock(i)
#define XT_TAB_ROW_WRITE_LOCK(i, s)		xt_spinlock_lock(i)
#define XT_TAB_ROW_UNLOCK(i, s)			xt_spinlock_unlock(i)
#endif

/* ------- TABLE DATA FILE ------- */

#define XT_TAB_DATA_MAGIC		0x1234ABCD

#define XT_FORMAT_DEF_SPACE		512

#define XT_TAB_FLAGS_TEMP_TAB	1

/*
 * This header ensures that no record in the data file has the offset 0.
 */
typedef struct XTTableHead {
	XTDiskValue4			th_head_size_4;							/* The size of the table header. */
	XTDiskValue4			th_op_seq_4;
	XTDiskValue6			th_row_free_6;
	XTDiskValue6			th_row_eof_6;
	XTDiskValue6			th_row_fnum_6;
	XTDiskValue6			th_rec_free_6;
	XTDiskValue6			th_rec_eof_6;
	XTDiskValue6			th_rec_fnum_6;
} XTTableHeadDRec, *XTTableHeadDPtr;

typedef struct XTTableFormat {
	XTDiskValue4			tf_format_size_4;						/* The size of this structure (table format). */
	XTDiskValue4			tf_tab_head_size_4;						/* The offset of the first record in the data handle file. */
	XTDiskValue2			tf_tab_version_2;						/* The table version number. */
	XTDiskValue2			tf_tab_flags_2;							/* Table flags XT_TAB_FLAGS_* */
	XTDiskValue4			tf_rec_size_4;							/* The maximum size of records in the table. */
	XTDiskValue1			tf_rec_fixed_1;							/* Set to 1 if this table contains fixed length records. */
	XTDiskValue1			tf_reserved_1;							/* - */
	XTDiskValue8			tf_min_auto_inc_8;						/* This is the minimum auto-increment value. */
	xtWord1					tf_reserved[64];						/* Reserved, set to 0. */
	char					tf_definition[XT_VAR_LENGTH];			/* A cstring, currently it only contains the foreign key information. */
} XTTableFormatDRec, *XTTableFormatDPtr;

#define XT_STAT_ID_MASK(x)	((x) & (u_int) 0x000000FF)

/* A record that fits completely in the data file record */
typedef struct XTTabRecHead {
	xtWord1					tr_rec_type_1;
	xtWord1					tr_stat_id_1;
	xtDiskRecordID4			tr_prev_rec_id_4;		/* The previous variation of this record. */
	XTDiskValue4			tr_xact_id_4;			/* The transaction ID. */
	XTDiskValue4			tr_row_id_4;			/* The row ID of this record. */
} XTTabRecHeadDRec, *XTTabRecHeadDPtr;

typedef struct XTTabRecFix {
	xtWord1					tr_rec_type_1;			/* XT_TAB_STATUS_FREED, XT_TAB_STATUS_DELETE,
													 * XT_TAB_STATUS_FIXED, XT_TAB_STATUS_VARIABLE */
	xtWord1					tr_stat_id_1;
	xtDiskRecordID4			tr_prev_rec_id_4;		/* The previous variation of this record. */
	XTDiskValue4			tr_xact_id_4;			/* The transaction ID. */
	XTDiskValue4			tr_row_id_4;			/* The row ID of this record. */
	xtWord1					rf_data[XT_VAR_LENGTH];	/* NOTE: This data is in RAW MySQL format. */
} XTTabRecFixDRec, *XTTabRecFixDPtr;

/* An extended record that overflows into the log file: */
typedef struct XTTabRecExt {
	xtWord1					tr_rec_type_1;			/* XT_TAB_STATUS_EXT_DLOG */
	xtWord1					tr_stat_id_1;
	xtDiskRecordID4			tr_prev_rec_id_4;		/* The previous variation of this record. */
	XTDiskValue4			tr_xact_id_4;			/* The transaction ID. */
	XTDiskValue4			tr_row_id_4;			/* The row ID of this record. */
	XTDiskValue2			re_log_id_2;			/* Reference to overflow area, log ID */
	XTDiskValue6			re_log_offs_6;			/* Reference to the overflow area, log offset */
	XTDiskValue4			re_log_dat_siz_4;		/* Size of the overflow data. */
	xtWord1					re_data[XT_VAR_LENGTH];	/* This data is in packed PBXT format. */
} XTTabRecExtDRec, *XTTabRecExtDPtr;

typedef struct XTTabRecExtHdat {
	xtWord1					tr_rec_type_1;			/* XT_TAB_STATUS_EXT_HDATA */
	xtWord1					tr_stat_id_1;
	xtDiskRecordID4			tr_prev_rec_id_4;		/* The previous variation of this record. */
	XTDiskValue4			tr_xact_id_4;			/* The transaction ID. */
	XTDiskValue4			tr_row_id_4;			/* The row ID of this record. */
	XTDiskValue4			eh_blk_rec_id_4;		/* The record ID of the next block. */
	XTDiskValue2			eh_blk_siz_2;			/* The total size of the data in the trailing blocks */
	xtWord1					eh_data[XT_VAR_LENGTH];	/* This data is in packed PBXT format. */
} XTTabRecExtHdatDRec, *XTTabRecExtHdatDPtr;

typedef struct XTTabRecData {
	xtWord1					tr_rec_type_1;			/* XT_TAB_STATUS_DATA */
	XTDiskValue4			rd_blk_rec_id_4;		/* The record ID of the next block. */
	xtWord1					rd_data[XT_VAR_LENGTH];	/* This data is in packed PBXT format. */
} XTTabRecDataDRec, *XTTabRecDataDPtr;

typedef struct XTTabRecEndDat {
	xtWord1					tr_rec_type_1;			/* XT_TAB_STATUS_END_DATA */
	xtWord1					ed_data[XT_VAR_LENGTH];	/* This data is in packed PBXT format. */
} XTTabRecEndDatDRec, *XTTabRecEndDatDPtr;

#define XT_REC_FIX_HEADER_SIZE		sizeof(XTTabRecHeadDRec)
#define XT_REC_EXT_HEADER_SIZE		offsetof(XTTabRecExtDRec, re_data)
#define XT_REC_FIX_EXT_HEADER_DIFF	(XT_REC_EXT_HEADER_SIZE - XT_REC_FIX_HEADER_SIZE)

typedef struct XTTabRecFree {
	xtWord1					rf_rec_type_1;
	xtWord1					rf_not_used_1;
	xtDiskRecordID4			rf_next_rec_id_4;		/* The next block on the free list. */
} XTTabRecFreeDRec, *XTTabRecFreeDPtr;

typedef struct XTTabRecInfo {
	XTTabRecFixDPtr			ri_fix_rec_buf;			/* This references the start of the buffer (set for all types of records) */
	XTTabRecExtDPtr			ri_ext_rec;				/* This is only set for extended records. */
	xtWord4					ri_rec_buf_size;
	XTactExtRecEntryDPtr	ri_log_buf;
	xtWord4					ri_log_data_size;		/* This size of the data in the log record. */
	xtRecordID				ri_rec_id;				/* The record ID. */
} XTTabRecInfoRec, *XTTabRecInfoPtr;

/* ------- TABLE ROW FILE ------- */

#define XT_TAB_ROW_SHIFTS		2
#define XT_TAB_ROW_MAGIC		0x4567CDEF
//#define XT_TAB_ROW_FREE			0
//#define XT_TAB_ROW_IN_USE		1

/*
 * NOTE: The shift count assumes the size of a table row
 * reference is 8 bytes (XT_TAB_ROW_SHIFTS)
 */
typedef struct XTTabRowRef {
	XTDiskValue4			rr_ref_id_4;			/* 4-byte reference, could be a RowID or a RecordID
													 * If this row is free, then it is a RowID, which
													 * references the next free row.
													 * If it is in use, then it is a RecordID which
													 * points to the first record in the variation
													 * list for the row.
													 */
} XTTabRowRefDRec, *XTTabRowRefDPtr;

/*
 * This is the header for the row file. The size MUST be a
 * the same size as sizeof(XTTabRowRefDRec)
 */
typedef struct XTTabRowHead {
	XTDiskValue4			rh_magic_4;
} XTTabRowHeadDRec, *XTTabRowHeadDPtr;

/* ------- TABLE & OPEN TABLES & TABLE LISTING ------- */

/* {TEMP-TABLES}
 * Temporary tables do not need to be flused,
 * and they also do not need to be recovered!
 * Currently this is determined by the name of the
 * table!
 */
typedef struct XTTable : public XTHeap {
	struct XTDatabase		*tab_db;			/* Heap pointer */
	XTPathStrPtr			tab_name;
	xtBool					tab_free_locks;
	xtTableID				tab_id;

	xtWord8					tab_auto_inc;							/* The last value returned as an auto-increment value {PRE-INC}. */
	XTSpinLockRec			tab_ainc_lock;							/* Lock for the auto-increment counter. */

	size_t					tab_index_format_offset;
	size_t					tab_index_header_size;
	size_t					tab_index_page_size;
	u_int					tab_index_block_shifts;
	XTIndexHeadDPtr			tab_index_head;
	size_t					tab_table_format_offset;
	size_t					tab_table_head_size;
	XTDictionaryRec			tab_dic;
	xt_mutex_type			tab_dic_field_lock;						/* Lock for setting field->ptr!. */

	XTRowLocksRec			tab_locks;								/* The locks held on this table. */

	XTTableSeqRec			tab_seq;								/* The table operation sequence. */
	XTTabCacheRec			tab_rows;
	XTTabCacheRec			tab_recs;

	/* Used to apply operations to the database in order. */
	XTSortedListPtr			tab_op_list;							/* The operation list. Operations to be applied. */

	/* Values that belong in the header when flushed! */
	xtBool					tab_flush_pending;						/* TRUE if the table needs to be flushed */
	xtBool					tab_recovery_done;						/* TRUE if the table has been recovered */
	xtBool					tab_temporary;							/* TRUE if this is a temporary table {TEMP-TABLES}. */
	off_t					tab_bytes_to_flush;						/* Number of bytes of the record/row files to flush. */

	xtOpSeqNo				tab_head_op_seq;						/* The number of the operation last applied to the database. */
	xtRowID					tab_head_row_free_id;
	xtRowID					tab_head_row_eof_id;
	xtWord4					tab_head_row_fnum;
	xtRecordID				tab_head_rec_free_id;
	xtRecordID				tab_head_rec_eof_id;
	xtWord4					tab_head_rec_fnum;

	xtOpSeqNo				tab_co_op_seq;							/* The operation last applied by the compactor. */

	xtBool					tab_wr_wake_freeer;						/* Set to TRUE if the writer must wake the freeer. */
	xtOpSeqNo				tab_wake_freeer_op;						/* Set to the sequence number the freeer is waiting for. */

	XTFilePtr				tab_row_file;
	xtRowID					tab_row_eof_id;							/* Indicates the EOF of the table row file. */
	xtRowID					tab_row_free_id;						/* The start of the free list in the table row file. */
	xtWord4					tab_row_fnum;							/* The count of the number of free rows on the free list. */
	xt_mutex_type			tab_row_lock;							/* Lock for updating the EOF and free list. */
	XT_TAB_ROW_LOCK_TYPE	tab_row_rwlock[XT_ROW_RWLOCKS];			/* Used to lock a row during update. */

	xt_mutex_type			tab_rec_flush_lock;						/* Required while the record/row files are being flushed. */
	XTFilePtr				tab_rec_file;
	xtRecordID				tab_rec_eof_id;							/* This value can only grow. */
	xtRecordID				tab_rec_free_id;
	xtWord4					tab_rec_fnum;							/* The count of the number of free rows on the free list. */
	xt_mutex_type			tab_rec_lock;							/* Lock for the free list. */

	xt_mutex_type			tab_ind_flush_lock;						/* Required while the index file is being flushed. */
	xtLogID					tab_ind_rec_log_id;						/* The point before which index entries have been written. */
	xtLogOffset				tab_ind_rec_log_offset;					/* The log offset of the write point. */
	XTFilePtr				tab_ind_file;
	xtIndexNodeID			tab_ind_eof;							/* This value can only grow. */
	xtIndexNodeID			tab_ind_free;							/* The start of the free page list of the index. */
	XTIndFreeListPtr		tab_ind_free_list;						/* A cache of the free list (if exists, don't go to disk!) */
	xt_mutex_type			tab_ind_lock;							/* Lock for reading and writing the index free list. */
	xtWord2					tab_ind_flush_seq;
} XTTableHRec, *XTTableHPtr;		/* Heap pointer */

/* Used for an in-memory list of the tables, ordered by ID. */
typedef struct XTTableEntry {
	xtTableID				te_tab_id;
	char					*te_tab_name;
	struct XTTablePath		*te_tab_path;
	XTTableHPtr				te_table;
} XTTableEntryRec, *XTTableEntryPtr;

typedef struct XTOpenTable {
	struct XTThread			*ot_thread;								/* The thread currently using this open table. */
	XTTableHPtr				ot_table;								/* PBXT table information. */

	struct XTOpenTable		*ot_otp_next_free;						/* Next free open table in the open table pool. */
	struct XTOpenTable		*ot_otp_mr_used;
	struct XTOpenTable		*ot_otp_lr_used;
	time_t					ot_otp_free_time;						/* The time this table was place on the free list. */

	//struct XTOpenTable	*ot_pool_next;							/* Next pointer for open table pool. */

	XT_ROW_REC_FILE_PTR		ot_rec_file;
	XT_ROW_REC_FILE_PTR		ot_row_file;
	XTOpenFilePtr			ot_ind_file;
	u_int					ot_err_index_no;						/* The number of the index on which the last error occurred */

	xtBool					ot_rec_fixed;							/* Cached from table for quick access. */
	size_t					ot_rec_size;							/* Cached from table for quick access. */
	
	char					ot_error_key[XT_IDENTIFIER_NAME_SIZE];
	xtBool					ot_for_update;							/* True if reading FOR UPDATE. */
	xtBool					ot_is_modify;							/* True if UPDATE or DELETE. */
	xtRowID					ot_temp_row_lock;						/* The temporary row lock set on this table. */
	u_int					ot_cols_req;							/* The number of columns required from the table. */

	/* GOTCHA: Separate buffers for reading and writing rows because
	 * of blob references, to this buffer, as in this test:
	 *
	 * drop table if exists t1;
	 * CREATE TABLE t1 (id MEDIUMINT NOT NULL, b1 BIT(8), vc TEXT, 
	 *                  bc CHAR(255), d DECIMAL(10,4) DEFAULT 0, 
	 *                  f FLOAT DEFAULT 0, total BIGINT UNSIGNED, 
	 *                  y YEAR, t DATE)
	 *                  PARTITION BY RANGE (YEAR(t)) 
	 *                 (PARTITION p1 VALUES LESS THAN (2005), 
	 *                  PARTITION p2 VALUES LESS THAN MAXVALUE);
	 *                
	 * INSERT INTO t1 VALUES(412,1,'eTesting MySQL databases is a cool ',
	 *                       'EEEMust make it bug free for the customer',
	 *                        654321.4321,15.21,0,1965,"2005-11-14");
	 * 
	 * UPDATE t1 SET b1 = 0, t="2006-02-22" WHERE id = 412;
	 * 
	 */
	size_t					ot_row_rbuf_size;						/* The current size of the read row buffer (resized dynamically). */
	xtWord1					*ot_row_rbuffer;						/* The row buffer for reading rows. */
	size_t					ot_row_wbuf_size;						/* The current size of the write row buffer (resized dynamically). */
	xtWord1					*ot_row_wbuffer;						/* The row buffer for writing rows. */

	/* Details of the current record: */
	xtRecordID				ot_curr_rec_id;							/* The offset of the current record. */
	xtRowID					ot_curr_row_id;							/* The row ID of the current record. */
	xtBool					ot_curr_updated;						/* TRUE if the current record was updated by the current transaction. */

	XTIndBlockPtr			ot_ind_res_bufs;						/* A list of reserved index buffers. */
	u_int					ot_ind_res_count;						/* The number of reserved buffers. */
#ifdef XT_TRACK_INDEX_UPDATES
	u_int					ot_ind_changed;
	u_int					ot_ind_reserved;
	u_int					ot_ind_reads;
#endif
#ifdef XT_TRACK_RETURNED_ROWS
	u_int					ot_rows_ret_max;
	u_int					ot_rows_ret_curr;
	xtRecordID				*ot_rows_returned;
#endif
	/* GOTCHA: Separate buffers for reading and writing the index are required
	 * because MySQL sometimes scans and updates an index with the same
	 * table handler.
	 */
	XTIdxItemRec			ot_ind_state;							/* Decribes the state of the index buffer. */
	XTIndHandlePtr			ot_ind_rhandle;							/* This handle references a block which is being used in a sequential scan. */
	//XTIdxBranchDRec			ot_ind_rbuf;							/* The index read buffer. */
	XTIdxBranchDRec			ot_ind_wbuf;							/* Buffer for the current index node for writing. */
	xtWord1					ot_ind_wbuf2[XT_INDEX_PAGE_SIZE];		/* Overflow for the write buffer when a node is too big. */

	/* Note: the fields below ot_ind_rbuf are not zero'ed out on creation
	 * of this structure!
	 */
	xtRecordID				ot_seq_rec_id;							/* Current position of a sequential scan. */
	xtRecordID				ot_seq_eof_id;							/* The EOF at the start of the sequential scan. */
	XTTabCachePagePtr		ot_seq_page;							/* If ot_seq_buffer is non-NULL, then a page has been locked! */
	xtWord1					*ot_seq_data;							/* Non-NULL if the data references memory mapped memory, or if it was
																	 * allocated if no memory mapping is being used.
																	 */
	xtBool					ot_on_page;
	size_t					ot_seq_offset;							/* Offset on the current page. */
} XTOpenTableRec, *XTOpenTablePtr;

#define XT_DATABASE_NAME_SIZE		XT_IDENTIFIER_NAME_SIZE

typedef struct XTTableDesc {
	char					td_tab_name[XT_TABLE_NAME_SIZE+4];	// 4 extra for DEL# (tables being deleted)
	xtTableID				td_tab_id;
	char					*td_file_name;

	struct XTDatabase		*td_db;
	struct XTTablePath		*td_tab_path;						// The path of the table.
	u_int					td_path_idx;
	XTOpenDirPtr			td_open_dir;
} XTTableDescRec, *XTTableDescPtr;


typedef struct XTFilesOfTable {
	int						ft_state;
	XTPathStrPtr			ft_tab_name;
	xtTableID				ft_tab_id;
	char					ft_file_path[PATH_MAX];
} XTFilesOfTableRec, *XTFilesOfTablePtr;

typedef struct XTRestrictItem {
	xtTableID				ri_tab_id;
	xtRecordID				ri_rec_id;
} XTRestrictItemRec, *XTRestrictItemPtr;

int					xt_tab_compare_names(const char *n1, const char *n2);
int					xt_tab_compare_paths(char *n1, char *n2);
void				xt_tab_init_db(struct XTThread *self, struct XTDatabase *db);
void				xt_tab_exit_db(struct XTThread *self, struct XTDatabase *db);
void				xt_check_tables(struct XTThread *self);

char				*xt_tab_file_to_name(size_t size, char *tab_name, char *file_name);

void				xt_create_table(struct XTThread *self, XTPathStrPtr name, XTDictionaryPtr dic);
XTTableHPtr			xt_use_table(struct XTThread *self, XTPathStrPtr name, xtBool no_load, xtBool missing_ok, xtBool *opened);
void				xt_sync_flush_table(struct XTThread *self, XTOpenTablePtr ot);
xtBool				xt_flush_record_row(XTOpenTablePtr ot, off_t *bytes_flushed, xtBool have_table_loc);
void				xt_flush_table(struct XTThread *self, XTOpenTablePtr ot);
XTTableHPtr			xt_use_table_no_lock(XTThreadPtr self, struct XTDatabase *db, XTPathStrPtr name, xtBool no_load, xtBool missing_ok, XTDictionaryPtr dic, xtBool *opened);
int					xt_use_table_by_id(struct XTThread *self, XTTableHPtr *tab, struct XTDatabase *db, xtTableID tab_id);
XTOpenTablePtr		xt_open_table(XTTableHPtr tab);
void				xt_close_table(XTOpenTablePtr ot, xtBool flush, xtBool have_table_lock);
void				xt_drop_table(struct XTThread *self, XTPathStrPtr name, xtBool drop_db);
void				xt_check_table(XTThreadPtr self, XTOpenTablePtr tab);
void				xt_rename_table(struct XTThread *self, XTPathStrPtr old_name, XTPathStrPtr new_name);

void				xt_describe_tables_init(struct XTThread *self, struct XTDatabase *db, XTTableDescPtr td);
xtBool				xt_describe_tables_next(struct XTThread *self, XTTableDescPtr td);
void				xt_describe_tables_exit(struct XTThread *self, XTTableDescPtr td);

xtBool				xt_table_exists(struct XTDatabase *db);

void				xt_enum_tables_init(u_int *edx);
XTTableEntryPtr		xt_enum_tables_next(struct XTThread *self, struct XTDatabase *db, u_int *edx);

void				xt_enum_files_of_tables_init(struct XTDatabase *db, char *tab_name, xtTableID tab_id, XTFilesOfTablePtr ft);
xtBool				xt_enum_files_of_tables_next(XTFilesOfTablePtr ft);

xtBool				xt_tab_seq_init(XTOpenTablePtr ot);
void				xt_tab_seq_reset(XTOpenTablePtr ot);
void				xt_tab_seq_exit(XTOpenTablePtr ot);
xtBool				xt_tab_seq_next(XTOpenTablePtr ot, xtWord1 *buffer, xtBool *eof);

xtBool				xt_tab_new_record(XTOpenTablePtr ot, xtWord1 *buffer);
xtBool				xt_tab_delete_record(XTOpenTablePtr ot, xtWord1 *buffer);
xtBool				xt_tab_restrict_rows(XTBasicListPtr list, struct XTThread *thread);
xtBool				xt_tab_update_record(XTOpenTablePtr ot, xtWord1 *before_buf, xtWord1 *after_buf);
int					xt_tab_visible(XTOpenTablePtr ot);
int					xt_tab_read_record(register XTOpenTablePtr ot, xtWord1 *buffer);
int					xt_tab_dirty_read_record(register XTOpenTablePtr ot, xtWord1 *buffer);
void				xt_tab_load_row_pointers(XTThreadPtr self, XTOpenTablePtr ot);
void				xt_tab_load_table(struct XTThread *self, XTOpenTablePtr ot);
xtBool				xt_tab_load_record(register XTOpenTablePtr ot, xtRecordID rec_id, XTInfoBufferPtr rec_buf);
int					xt_tab_remove_record(XTOpenTablePtr ot, xtRecordID rec_id, xtWord1 *rec_data, xtRecordID *prev_var_rec_id, xtBool clean_delete, xtRowID row_id, xtXactID xn_id);
int					xt_tab_maybe_committed(XTOpenTablePtr ot, xtRecordID rec_id, xtXactID *xn_id, xtRowID *out_rowid, xtBool *out_updated);
xtBool				xt_tab_free_record(XTOpenTablePtr ot, u_int status, xtRecordID rec_id, xtBool clean_delete);
void				xt_tab_store_header(XTOpenTablePtr ot, XTTableHeadDPtr rec_head);
xtBool				xt_tab_write_header(XTOpenTablePtr ot, XTTableHeadDPtr rec_head, struct XTThread *thread);
xtBool				xt_tab_write_min_auto_inc(XTOpenTablePtr ot);

xtBool				xt_tab_get_row(register XTOpenTablePtr ot, xtRowID row_id, xtRecordID *var_rec_id);
xtBool				xt_tab_set_row(XTOpenTablePtr ot, u_int status, xtRowID row_id, xtRecordID var_rec_id);
xtBool				xt_tab_free_row(XTOpenTablePtr ot, XTTableHPtr tab, xtRowID row_id);

xtBool				xt_tab_load_ext_data(XTOpenTablePtr ot, xtRecordID load_rec_id, xtWord1 *buffer, u_int cols_req);
xtBool				xt_tab_put_rec_data(XTOpenTablePtr ot, xtRecordID rec_id, size_t size, xtWord1 *buffer, xtOpSeqNo *op_seq);
xtBool				xt_tab_put_eof_rec_data(XTOpenTablePtr ot, xtRecordID rec_id, size_t size, xtWord1 *buffer, xtOpSeqNo *op_seq);
xtBool				xt_tab_put_log_op_rec_data(XTOpenTablePtr ot, u_int status, xtRecordID free_rec_id, xtRecordID rec_id, size_t size, xtWord1 *buffer);
xtBool				xt_tab_put_log_rec_data(XTOpenTablePtr ot, u_int status, xtRecordID free_rec_id, xtRecordID rec_id, size_t size, xtWord1 *buffer, xtOpSeqNo *op_seq);
xtBool				xt_tab_get_rec_data(register XTOpenTablePtr ot, xtRecordID rec_id, size_t size, xtWord1 *buffer);
void				xt_tab_set_index_error(XTTableHPtr tab);

inline off_t		xt_row_id_to_row_offset(register XTTableHPtr tab, xtRowID row_id)
{
	return (off_t) tab->tab_rows.tci_header_size + (off_t) (row_id - 1) * (off_t) tab->tab_rows.tci_rec_size;
}

inline  xtRowID		xt_row_offset_row_id(register XTTableHPtr tab, off_t rec_offs)
{
#ifdef DEBUG
	if (((rec_offs - (off_t) tab->tab_rows.tci_header_size) % (off_t) tab->tab_rows.tci_rec_size) != 0) {
		printf("ERROR! Not a valid record offset!\n");
	}
#endif
	return (xtRowID) ((rec_offs - (off_t) tab->tab_rows.tci_header_size) / (off_t) tab->tab_rows.tci_rec_size) + 1;
}

inline off_t		xt_rec_id_to_rec_offset(register XTTableHPtr tab, xtRefID ref_id)
{
	if (!ref_id)
		return (off_t) 0;
	return (off_t) tab->tab_recs.tci_header_size + (off_t) (ref_id-1) * (off_t) tab->tab_recs.tci_rec_size;
}

inline  xtRefID		xt_rec_offset_rec_id(register XTTableHPtr tab, off_t ref_offs)
{
	if (!ref_offs)
		return (xtRefID) 0;
#ifdef DEBUG
	if (((ref_offs - (off_t) tab->tab_recs.tci_header_size) % (off_t) tab->tab_recs.tci_rec_size) != 0) {
		printf("ERROR! Not a valid record offset!\n");
	}
#endif
		
	return (xtRefID) ((ref_offs - (off_t) tab->tab_recs.tci_header_size) / (off_t) tab->tab_recs.tci_rec_size)+1;
}

inline off_t		xt_ind_node_to_offset(register XTTableHPtr tab, xtIndexNodeID node_id)
{
	if (!XT_NODE_ID(node_id))
		return (off_t) 0;
	return (off_t) tab->tab_index_header_size + (off_t) (XT_NODE_ID(node_id)-1) * (off_t) tab->tab_index_page_size;
}

inline xtIndexNodeID xt_ind_offset_to_node(register XTTableHPtr tab, off_t ind_offs)
{
	XT_NODE_TEMP;

	if (!ind_offs)
		return XT_RET_NODE_ID(0);
#ifdef DEBUG
	if (((ind_offs - (off_t) tab->tab_index_header_size) % (off_t) tab->tab_index_page_size) != 0) {
		printf("ERROR! Not a valid index offset!\n");
	}
#endif
		
	return XT_RET_NODE_ID(((ind_offs - (off_t) tab->tab_index_header_size) / (off_t) tab->tab_index_page_size)+1);
}

#define XT_RESIZE_ROW_BUFFER(thr, rb, size) \
	do { \
		if (rb->rb_size < size) { \
			xt_realloc(thr, (void **) &rb->x.rb_buffer, size); \
			rb->rb_size = size; \
		} \
	} \
	while (0)

#endif
