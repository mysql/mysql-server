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
 * 2007-11-12	Paul McCullagh
 *
 * H&G2JCtL
 *
 * Restart and write data to the database.
 */

#ifndef __restart_xt_h__
#define __restart_xt_h__

#include "pthread_xt.h"
#include "filesys_xt.h"
#include "sortedlist_xt.h"
#include "util_xt.h"
#include "xactlog_xt.h"

struct XTThread;
struct XTOpenTable;
struct XTDatabase;
struct XTTable;

typedef struct XTWriterState {
	struct XTDatabase		*ws_db;
	xtBool					ws_in_recover;
	xtLogID					ws_ind_rec_log_id;
	xtLogOffset				ws_ind_rec_log_offset;
	XTXactSeqReadRec		ws_seqread;
	XTDataBufferRec			ws_databuf;
	XTInfoBufferRec			ws_rec_buf;
	xtTableID				ws_tab_gone;					/* Cache the ID of the last table that does not exist. */
	struct XTOpenTable		*ws_ot;
} XTWriterStateRec, *XTWriterStatePtr;

#define XT_CHECKPOINT_VERSION	1

typedef struct XTXlogCheckpoint {
	XTDiskValue2			xcp_checksum_2;					/* The checksum of the all checkpoint data. */
	XTDiskValue4			xcp_head_size_4;
	XTDiskValue2			xcp_version_2;					/* The version of the checkpoint record. */
	XTDiskValue6			xcp_chkpnt_no_6;				/* Incremented for each checkpoint. */
	XTDiskValue4			xcp_log_id_4;					/* The restart log ID. */
	XTDiskValue6			xcp_log_offs_6;					/* The restart log offset. */
	XTDiskValue4			xcp_tab_id_4;					/* The current high table ID. */
	XTDiskValue4			xcp_xact_id_4;					/* The current high transaction ID. */
	XTDiskValue4			xcp_ind_rec_log_id_4;			/* The index recovery log ID. */
	XTDiskValue6			xcp_ind_rec_log_offs_6;		/* The index recovery log offset. */
	XTDiskValue2			xcp_log_count_2;				/* Number of logs to be deleted in the area below. */
	XTDiskValue2			xcp_del_log[XT_VAR_LENGTH];
} XTXlogCheckpointDRec, *XTXlogCheckpointDPtr;

typedef struct XTXactRestart {
	struct XTDatabase		*xres_db;
	int						xres_next_res_no;				/* The next restart file to be written. */
	xtLogID					xres_cp_log_id;					/* Log number of the last checkpoint. */
	xtLogOffset				xres_cp_log_offset;				/* Log offset of the last checkpoint */
	xtBool					xres_cp_required;				/* Checkpoint required (startup and shutdown). */
	xtWord8					xres_cp_number;					/* The checkpoint number (used to decide which is the latest checkpoint). */

public:
	void					xres_init(struct XTThread *self, struct XTDatabase *db, xtLogID *log_id, xtLogOffset *log_offset, xtLogID	*max_log_id);
	void					xres_exit(struct XTThread *self);
	xtBool					xres_is_checkpoint_pending(xtLogID log_id, xtLogOffset log_offset);
	void					xres_checkpoint_pending(xtLogID log_id, xtLogOffset log_offset);
	xtBool					xres_checkpoint(struct XTThread *self);
	void					xres_name(size_t size, char *path, xtLogID log_id);

private:
	xtBool					xres_check_checksum(XTXlogCheckpointDPtr buffer, size_t size);
	void					xres_recover_progress(XTThreadPtr self, XTOpenFilePtr *of, int perc);
	xtBool					xres_restart(struct XTThread *self, xtLogID *log_id, xtLogOffset *log_offset, xtLogID ind_rec_log_id, off_t ind_rec_log_offset, xtLogID *max_log_id);
	off_t					xres_bytes_to_read(struct XTThread *self, struct XTDatabase *db, u_int *log_count, xtLogID *max_log_id);
} XTXactRestartRec, *XTXactRestartPtr;

typedef struct XTCheckPointState {
	xt_mutex_type			cp_state_lock;					/* Lock and the entire checkpoint state. */
	xtBool					cp_running;						/* TRUE if a checkpoint is running. */
	xtLogID					cp_log_id;
	xtLogOffset				cp_log_offset;
	xtLogID					cp_ind_rec_log_id;
	xtLogOffset				cp_ind_rec_log_offset;
	XTSortedListPtr			cp_table_ids;					/* List of tables to be flushed for the checkpoint. */
	u_int					cp_flush_count;					/* The number of tables flushed. */
	u_int					cp_next_to_flush;				/* The next table to be flushed. */
} XTCheckPointStateRec, *XTCheckPointStatePtr;

#define XT_CPT_NONE_FLUSHED			0
#define XT_CPT_REC_ROW_FLUSHED		1
#define XT_CPT_INDEX_FLUSHED		2
#define XT_CPT_ALL_FLUSHED			(XT_CPT_REC_ROW_FLUSHED | XT_CPT_INDEX_FLUSHED)

typedef struct XTCheckPointTable {
	u_int					cpt_flushed;
	xtTableID				cpt_tab_id;
} XTCheckPointTableRec, *XTCheckPointTablePtr;

void xt_xres_init(struct XTThread *self, struct XTDatabase *db);
void xt_xres_exit(struct XTThread *self, struct XTDatabase *db);

void xt_xres_init_tab(struct XTThread *self, struct XTTable *tab);
void xt_xres_exit_tab(struct XTThread *self, struct XTTable *tab);

void xt_xres_apply_in_order(struct XTThread *self, XTWriterStatePtr ws, xtLogID log_id, xtLogOffset log_offset, XTXactLogBufferDPtr record);

xtBool	xt_begin_checkpoint(struct XTDatabase *db, xtBool have_table_lock, struct XTThread *thread);
xtBool	xt_end_checkpoint(struct XTDatabase *db, struct XTThread *thread, xtBool *checkpoint_done);
void	xt_start_checkpointer(struct XTThread *self, struct XTDatabase *db);
void	xt_wait_for_checkpointer(struct XTThread *self, struct XTDatabase *db);
void	xt_stop_checkpointer(struct XTThread *self, struct XTDatabase *db);
void	xt_wake_checkpointer(struct XTThread *self, struct XTDatabase *db);
void	xt_free_writer_state(struct XTThread *self, XTWriterStatePtr ws);
xtWord8	xt_bytes_since_last_checkpoint(struct XTDatabase *db, xtLogID curr_log_id, xtLogOffset curr_log_offset);

void xt_print_log_record(xtLogID log, off_t offset, XTXactLogBufferDPtr record);
void xt_dump_xlogs(struct XTDatabase *db, xtLogID start_log);

xtPublic void xt_xres_start_database_recovery(XTThreadPtr self, const char *path);

#endif
