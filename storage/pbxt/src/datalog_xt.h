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
 * 2005-01-24	Paul McCullagh
 *
 * H&G2JCtL
 */
#ifndef __xt_datalog_h__
#define __xt_datalog_h__

#include "pthread_xt.h"
#include "filesys_xt.h"
#include "sortedlist_xt.h"
#include "xactlog_xt.h"
#include "util_xt.h"

struct XTThread;
struct XTDatabase;
struct xXTDataLog;
struct XTTable;
struct XTOpenTable;

#define XT_SET_LOG_REF(d, l, o)			do { XT_SET_DISK_2((d)->re_log_id_2, l); \
											 XT_SET_DISK_6((d)->re_log_offs_6, o); \
										} while (0)
#define XT_GET_LOG_REF(l, o, s)			do { l = XT_GET_DISK_2((s)->re_log_id_2); \
											 o = XT_GET_DISK_6((s)->re_log_offs_6); \
										} while (0)

#ifdef DEBUG
//#define USE_DEBUG_SIZES
#endif

#ifdef USE_DEBUG_SIZES
#define XT_DL_MAX_LOG_ID				500
#define XT_DL_LOG_POOL_SIZE				10
#define XT_DL_HASH_TABLE_SIZE			5
#define XT_DL_SEGMENT_SHIFTS			1
#else
#define XT_DL_MAX_LOG_ID				0x7FFF
#define XT_DL_LOG_POOL_SIZE				1000
#define XT_DL_HASH_TABLE_SIZE			10000
#define XT_DL_SEGMENT_SHIFTS			3
#endif

#define XT_DL_SEG_HASH_TABLE_SIZE		(XT_DL_HASH_TABLE_SIZE / XT_DL_NO_OF_SEGMENTS)
#define XT_DL_NO_OF_SEGMENTS			(1 << XT_DL_SEGMENT_SHIFTS)
#define XT_DL_SEGMENT_MASK				(XT_DL_NO_OF_SEGMENTS - 1)

typedef struct XTOpenLogFile {
	xtLogID					olf_log_id;
	XTOpenFilePtr			odl_log_file;					/* The open file handle. */
	struct XTDataLogFile	*odl_data_log;

	xtBool					odl_in_use;
	struct XTOpenLogFile	*odl_next_free;					/* Pointer to the next on the free list. */
	struct XTOpenLogFile	*odl_prev_free;					/* Pointer to the previous on the free list. */

	xtWord4					odl_ru_time;					/* If this is in the top 1/4 don't change position in MRU list. */
	struct XTOpenLogFile	*odl_mr_used;					/* More recently used pages. */
	struct XTOpenLogFile	*odl_lr_used;					/* Less recently used pages. */
} XTOpenLogFileRec, *XTOpenLogFilePtr;

#define XT_DL_MAY_COMPACT	-1								/* This is an indication to set the state to XT_DL_TO_COMPACT. */
#define XT_DL_UNKNOWN		0
#define XT_DL_HAS_SPACE		1								/* The log is not yet full, and can be used for writing. */
#define XT_DL_READ_ONLY		2								/* The log is full, and can only be read now. */
#define XT_DL_TO_COMPACT	3								/* The log has too much garbage, and must be compacted. */
#define XT_DL_COMPACTED		4								/* The state after compaction. */
#define XT_DL_TO_DELETE		5								/* All references to this log have been removed, and it is to be deleted. */
#define XT_DL_DELETED		6								/* After deletion, logs are locked until the next checkpoint. */
#define XT_DL_EXCLUSIVE		7								/* The log is locked and being written by a thread. */

typedef struct XTDataLogFile {
	xtLogID					dlf_log_id;						/* The ID of the data log. */
	int						dlf_state;
	struct XTDataLogFile	*dlf_next_hash;					/* Pointer to the next on the hash list. */
	u_int					dlf_open_count;					/* Number of open log files. */
	XTOpenLogFilePtr		dlf_free_list;					/* The open file free list. */
	off_t					dlf_log_eof;
	off_t					dlf_start_offset;				/* Start offset for garbage collection. */
	off_t					dlf_garbage_count;				/* The amount of garbage in the log file. */
	XTOpenFilePtr			dlf_log_file;					/* The open file handle (if the log is in exclusive use!!). */

	off_t					dlf_space_avaliable();
	xtBool					dlf_to_much_garbage();
} XTDataLogFileRec, *XTDataLogFilePtr;

typedef struct XTDataLogSeg {
	xt_mutex_type			dls_lock;						/* The cache segment lock. */
	xt_cond_type			dls_cond;
	XTDataLogFilePtr		dls_hash_table[XT_DL_SEG_HASH_TABLE_SIZE];
} XTDataLogSegRec, *XTDataLogSegPtr;

typedef struct XTDataLogCache {
	struct XTDatabase		*dlc_db;

	xt_mutex_type			dlc_lock;						/* The public cache lock. */
	xt_cond_type			dlc_cond;						/* The public cache wait condition. */
	XTSortedListPtr			dlc_has_space;					/* List of logs with space for more data. */
	XTSortedListPtr			dlc_to_compact;					/* List of logs to be compacted. */
	XTSortedListPtr			dlc_to_delete;					/* List of logs to be deleted at next checkpoint. */
	XTSortedListPtr			dlc_deleted;					/* List of logs deleted at the previous checkpoint. */
	XTDataLogSegRec			dlc_segment[XT_DL_NO_OF_SEGMENTS];
	xtLogID					dlc_next_log_id;				/* The next log ID to be used to create a new log. */

	xt_mutex_type			dlc_mru_lock;					/* The lock for the LRU list. */
	xtWord4					dlc_ru_now;
	XTOpenLogFilePtr		dlc_lru_open_log;
	XTOpenLogFilePtr		dlc_mru_open_log;
	u_int					dlc_open_count;					/* The total open file count. */

	xt_mutex_type			dlc_head_lock;					/* The lock for changing the header of shared logs. */

	void					dls_remove_log(XTDataLogFilePtr data_log);
	int						dls_get_log_state(XTDataLogFilePtr data_log);
	xtBool					dls_set_log_state(XTDataLogFilePtr data_log, int state);
	void					dlc_init(struct XTThread *self, struct XTDatabase *db);
	void					dlc_exit(struct XTThread *self);
	void					dlc_name(size_t size, char *path, xtLogID log_id);
	xtBool					dlc_open_log(XTOpenFilePtr *fh, xtLogID log_id, int mode);
	xtBool					dlc_unlock_log(XTDataLogFilePtr data_log);
	XTDataLogFilePtr		dlc_get_log_for_writing(off_t space_required, struct XTThread *thread);
	xtBool					dlc_get_data_log(XTDataLogFilePtr *data_log, xtLogID log_id, xtBool create, XTDataLogSegPtr *ret_seg);
	xtBool					dlc_remove_data_log(xtLogID log_id, xtBool just_close);
	xtBool					dlc_get_open_log(XTOpenLogFilePtr *open_log, xtLogID log_id);
	void					dlc_release_open_log(XTOpenLogFilePtr open_log);
} XTDataLogCacheRec, *XTDataLogCachePtr;

/* The data log buffer, used by a thread to write a
 * data log file.
 */
typedef struct XTDataLogBuffer {
	struct XTDatabase		*dlb_db;
	XTDataLogFilePtr		dlb_data_log;						/* The data log file. */
	
	xtLogOffset				dlb_buffer_offset;					/* The offset into the log file. */
	size_t					dlb_buffer_size;					/* The size of the buffer. */
	size_t					dlb_buffer_len;						/* The amount of data in the buffer. */
	xtWord1					*dlb_log_buffer;
	xtBool					dlb_flush_required;
#ifdef DEBUG
	off_t					dlb_max_write_offset;
#endif

	void					dlb_init(struct XTDatabase *db, size_t buffer_size);
	void					dlb_exit(struct XTThread *self);
	xtBool					dlb_close_log(struct XTThread *thread);
	xtBool					dlb_get_log_offset(xtLogID *log_id, off_t *out_offset, size_t req_size, struct XTThread *thread);
	xtBool					dlb_flush_log(xtBool commit, struct XTThread *thread);
	xtBool					dlb_write_thru_log(xtLogID log_id, xtLogOffset log_offset, size_t size, xtWord1 *data, struct XTThread *thread);
	xtBool					dlb_append_log(xtLogID log_id, off_t out_offset, size_t size, xtWord1 *data, struct XTThread *thread);
	xtBool					dlb_read_log(xtLogID log_id, off_t offset, size_t size, xtWord1 *data, struct XTThread *thread);
	xtBool					dlb_delete_log(xtLogID log_id, off_t offset, size_t size, xtTableID tab_id, xtRecordID tab_offset, struct XTThread *thread);
} XTDataLogBufferRec, *XTDataLogBufferPtr;

typedef struct XTSeqLogRead {
	struct XTDatabase		*sl_db;

	virtual					~XTSeqLogRead() { }
	virtual xtBool			sl_seq_init(struct XTDatabase *db, size_t buffer_size) { (void) buffer_size; sl_db = db; return OK; };
	virtual void			sl_seq_exit() { };
	virtual XTOpenFilePtr	sl_seq_open_file() { return NULL; };
	virtual void			sl_seq_pos(xtLogID *log_id, xtLogOffset *log_offset) { (void) log_id; (void) log_offset; };
	virtual xtBool			sl_seq_start(xtLogID log_id, xtLogOffset log_offset, xtBool missing_ok) {
		(void) log_id; (void) log_offset; (void) missing_ok; return OK; 
	};
	virtual xtBool			sl_rnd_read(xtLogOffset log_offset, size_t size, xtWord1 *data, size_t *read, struct XTThread *thread) {
		(void) log_offset; (void) size; (void) data; (void) read; (void) thread; return OK;
	};
	virtual xtBool			sl_seq_next(XTXactLogBufferDPtr *entry, struct XTThread *thread) {
		(void) entry; (void) thread; return OK;
	};
	virtual void			sl_seq_skip(size_t size) { (void) size; }
} XTSeqLogReadRec, *XTSeqLogReadPtr;

typedef struct XTDataSeqRead : public XTSeqLogRead {
	XTOpenFilePtr			sl_log_file;
	xtLogID					sl_rec_log_id;		/* The current record log ID. */
	xtLogOffset				sl_rec_log_offset;	/* The current log read position. */
	size_t					sl_record_len;		/* The length of the current record. */
	xtLogOffset				sl_log_eof;
	xtLogOffset				sl_extra_garbage;	/* Garbage found during a scan. */

	size_t					sl_buffer_size;		/* Size of the buffer. */
	xtLogOffset				sl_buf_log_offset;	/* File offset of the buffer. */
	size_t					sl_buffer_len;		/* Amount of data in the buffer. */
	xtWord1					*sl_buffer;

	virtual					~XTDataSeqRead() { }
	virtual xtBool			sl_seq_init(struct XTDatabase *db, size_t buffer_size);
	virtual void			sl_seq_exit();
	virtual XTOpenFilePtr	sl_seq_open_file();
	virtual void			sl_seq_pos(xtLogID *log_id, xtLogOffset *log_offset);
	virtual xtBool			sl_seq_start(xtLogID log_id, xtLogOffset log_offset, xtBool missing_ok);
	virtual xtBool			sl_rnd_read(xtLogOffset log_offset, size_t size, xtWord1 *data, size_t *read, struct XTThread *thread);
	virtual xtBool			sl_seq_next(XTXactLogBufferDPtr *entry, struct XTThread *thread);
	virtual void			sl_seq_skip(size_t size);
	virtual void			sl_seq_skip_to(off_t offset);
} XTDataSeqReadRec, *XTDataSeqReadPtr;

void	xt_dl_delete_ext_data(struct XTThread *self, struct XTTable *tab, xtBool missing_ok, xtBool have_table_lock);

void	xt_start_compactor(struct XTThread *self, struct XTDatabase *db);
void	xt_stop_compactor(struct XTThread *self, struct XTDatabase *db);

void	xt_dl_init_db(struct XTThread *self, struct XTDatabase *db);
void	xt_dl_exit_db(struct XTThread *self, struct XTDatabase *db);
void	xt_dl_set_to_delete(struct XTThread *self, struct XTDatabase *db, xtLogID log_id);
void	xt_dl_log_status(struct XTThread *self, struct XTDatabase *db, XTStringBufferPtr strbuf);
void	xt_dl_delete_logs(struct XTThread *self, struct XTDatabase *db);

#endif

