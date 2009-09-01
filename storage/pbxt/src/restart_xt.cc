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

#include "xt_config.h"

#include <signal.h>
#include <time.h>

#ifndef DRIZZLED
#include "mysql_priv.h"
#endif

#include "ha_pbxt.h"

#include "xactlog_xt.h"
#include "database_xt.h"
#include "util_xt.h"
#include "strutil_xt.h"
#include "filesys_xt.h"
#include "restart_xt.h"
#include "myxt_xt.h"
#include "trace_xt.h"

#ifdef DEBUG
//#define DEBUG_PRINT
//#define DEBUG_KEEP_LOGS
//#define PRINT_LOG_ON_RECOVERY
//#define TRACE_RECORD_DATA
//#define SKIP_STARTUP_CHECKPOINT
//#define NEVER_CHECKPOINT
//#define TRACE_CHECKPOINT
#endif

#define PRINTF		printf
//#define PRINTF		xt_ftracef
//#define PRINTF		xt_trace

void xt_print_bytes(xtWord1 *buf, u_int len)
{
	for (u_int i=0; i<len; i++) {
		PRINTF("%02x ", (u_int) *buf);
		buf++;
	}
}

void xt_print_log_record(xtLogID log, xtLogOffset offset, XTXactLogBufferDPtr record)
{
	const char		*type = NULL;
	const char		*rec_type = NULL;
	xtOpSeqNo		op_no = 0;
	xtTableID		tab_id = 0;
	xtRowID			row_id = 0;
	xtRecordID		rec_id = 0;
	xtBool			xn_set = FALSE;
	xtXactID		xn_id = 0;
	char			buffer[200];
	XTTabRecExtDPtr	rec_buf;
	XTTabRecExtDPtr	ext_rec;
	XTTabRecFixDPtr	fix_rec;
	u_int			rec_len;
	xtLogID			log_id = 0;
	xtLogOffset		log_offset = 0;

	rec_buf = NULL;
	ext_rec = NULL;
	fix_rec = NULL;
	rec_len = 0;
	switch (record->xl.xl_status_1) {
		case XT_LOG_ENT_REC_MODIFIED:
		case XT_LOG_ENT_UPDATE:
		case XT_LOG_ENT_INSERT:
		case XT_LOG_ENT_DELETE:
		case XT_LOG_ENT_UPDATE_BG:
		case XT_LOG_ENT_INSERT_BG:
		case XT_LOG_ENT_DELETE_BG:
			op_no = XT_GET_DISK_4(record->xu.xu_op_seq_4);
			tab_id = XT_GET_DISK_4(record->xu.xu_tab_id_4);
			rec_id = XT_GET_DISK_4(record->xu.xu_rec_id_4);
			xn_id = XT_GET_DISK_4(record->xu.xu_xact_id_4);
			row_id = XT_GET_DISK_4(record->xu.xu_row_id_4);
			rec_len = XT_GET_DISK_2(record->xu.xu_size_2);
			xn_set = TRUE;
			type="rec";
			rec_buf = (XTTabRecExtDPtr) &record->xu.xu_rec_type_1;
			ext_rec = (XTTabRecExtDPtr) &record->xu.xu_rec_type_1;
			if (XT_REC_IS_EXT_DLOG(ext_rec->tr_rec_type_1)) {
				log_id = XT_GET_DISK_2(ext_rec->re_log_id_2);
				log_offset = XT_GET_DISK_6(ext_rec->re_log_offs_6);
			}
			else {
				ext_rec = NULL;
				fix_rec = (XTTabRecFixDPtr) &record->xu.xu_rec_type_1;
			}
			break;
		case XT_LOG_ENT_UPDATE_FL:
		case XT_LOG_ENT_INSERT_FL:
		case XT_LOG_ENT_DELETE_FL:
		case XT_LOG_ENT_UPDATE_FL_BG:
		case XT_LOG_ENT_INSERT_FL_BG:
		case XT_LOG_ENT_DELETE_FL_BG:
			op_no = XT_GET_DISK_4(record->xf.xf_op_seq_4);
			tab_id = XT_GET_DISK_4(record->xf.xf_tab_id_4);
			rec_id = XT_GET_DISK_4(record->xf.xf_rec_id_4);
			xn_id = XT_GET_DISK_4(record->xf.xf_xact_id_4);
			row_id = XT_GET_DISK_4(record->xf.xf_row_id_4);
			rec_len = XT_GET_DISK_2(record->xf.xf_size_2);
			xn_set = TRUE;
			type="rec";
			rec_buf = (XTTabRecExtDPtr) &record->xf.xf_rec_type_1;
			ext_rec = (XTTabRecExtDPtr) &record->xf.xf_rec_type_1;
			if (XT_REC_IS_EXT_DLOG(ext_rec->tr_rec_type_1)) {
				log_id = XT_GET_DISK_2(ext_rec->re_log_id_2);
				log_offset = XT_GET_DISK_6(ext_rec->re_log_offs_6);
			}
			else {
				ext_rec = NULL;
				fix_rec = (XTTabRecFixDPtr) &record->xf.xf_rec_type_1;
			}
			break;
		case XT_LOG_ENT_REC_FREED:
		case XT_LOG_ENT_REC_REMOVED:
		case XT_LOG_ENT_REC_REMOVED_EXT:
			op_no = XT_GET_DISK_4(record->fr.fr_op_seq_4);
			tab_id = XT_GET_DISK_4(record->fr.fr_tab_id_4);
			rec_id = XT_GET_DISK_4(record->fr.fr_rec_id_4);
			xn_id = XT_GET_DISK_4(record->fr.fr_xact_id_4);
			xn_set = TRUE;
			type="rec";
			break;
		case XT_LOG_ENT_REC_REMOVED_BI:
			op_no = XT_GET_DISK_4(record->rb.rb_op_seq_4);
			tab_id = XT_GET_DISK_4(record->rb.rb_tab_id_4);
			rec_id = XT_GET_DISK_4(record->rb.rb_rec_id_4);
			xn_id = XT_GET_DISK_4(record->rb.rb_xact_id_4);
			row_id = XT_GET_DISK_4(record->rb.rb_row_id_4);
			rec_len = XT_GET_DISK_2(record->rb.rb_size_2);
			xn_set = TRUE;
			type="rec";
			rec_buf = (XTTabRecExtDPtr) &record->rb.rb_rec_type_1;
			ext_rec = (XTTabRecExtDPtr) &record->rb.rb_rec_type_1;
			if (XT_REC_IS_EXT_DLOG(record->rb.rb_rec_type_1)) {
				log_id = XT_GET_DISK_2(ext_rec->re_log_id_2);
				log_offset = XT_GET_DISK_6(ext_rec->re_log_offs_6);
			}
			else {
				ext_rec = NULL;
				fix_rec = (XTTabRecFixDPtr) &record->rb.rb_rec_type_1;
			}
			break;
		case XT_LOG_ENT_REC_MOVED:
			op_no = XT_GET_DISK_4(record->xw.xw_op_seq_4);
			tab_id = XT_GET_DISK_4(record->xw.xw_tab_id_4);
			rec_id = XT_GET_DISK_4(record->xw.xw_rec_id_4);
			log_id = XT_GET_DISK_2(&record->xw.xw_rec_type_1);			// This is actually correct
			log_offset = XT_GET_DISK_6(record->xw.xw_next_rec_id_4);	// This is actually correct!
			type="rec";
			break;
		case XT_LOG_ENT_REC_CLEANED:
		case XT_LOG_ENT_REC_CLEANED_1:
		case XT_LOG_ENT_REC_UNLINKED:
			op_no = XT_GET_DISK_4(record->xw.xw_op_seq_4);
			tab_id = XT_GET_DISK_4(record->xw.xw_tab_id_4);
			rec_id = XT_GET_DISK_4(record->xw.xw_rec_id_4);
			type="rec";
			break;
		case XT_LOG_ENT_ROW_NEW:
		case XT_LOG_ENT_ROW_NEW_FL:
		case XT_LOG_ENT_ROW_ADD_REC:
		case XT_LOG_ENT_ROW_SET:
		case XT_LOG_ENT_ROW_FREED:
			op_no = XT_GET_DISK_4(record->xa.xa_op_seq_4);
			tab_id = XT_GET_DISK_4(record->xa.xa_tab_id_4);
			rec_id = XT_GET_DISK_4(record->xa.xa_row_id_4);
			type="row";
			break;
		case XT_LOG_ENT_NO_OP:
			op_no = XT_GET_DISK_4(record->no.no_op_seq_4);
			tab_id = XT_GET_DISK_4(record->no.no_tab_id_4);
			type="-";
			break;
		case XT_LOG_ENT_END_OF_LOG:
			break;
	}

	switch (record->xl.xl_status_1) {
		case XT_LOG_ENT_HEADER:
			rec_type = "HEADER";
			break;
		case XT_LOG_ENT_NEW_LOG:
			rec_type = "NEW LOG";
			break;
		case XT_LOG_ENT_DEL_LOG:
			sprintf(buffer, "DEL LOG log=%d ", (int) XT_GET_DISK_4(record->xl.xl_log_id_4));
			rec_type = buffer;
			break;
		case XT_LOG_ENT_NEW_TAB:
			rec_type = "NEW TABLE";
			break;
		case XT_LOG_ENT_COMMIT:
			rec_type = "COMMIT";
			xn_id = XT_GET_DISK_4(record->xe.xe_xact_id_4);
			xn_set = TRUE;
			break;
		case XT_LOG_ENT_ABORT:
			rec_type = "ABORT";
			xn_id = XT_GET_DISK_4(record->xe.xe_xact_id_4);
			xn_set = TRUE;
			break;
		case XT_LOG_ENT_CLEANUP:
			rec_type = "CLEANUP";
			xn_id = XT_GET_DISK_4(record->xc.xc_xact_id_4);
			xn_set = TRUE;
			break;
		case XT_LOG_ENT_REC_MODIFIED:
			rec_type = "MODIFIED";
			break;
		case XT_LOG_ENT_UPDATE:
			rec_type = "UPDATE";
			break;
		case XT_LOG_ENT_UPDATE_FL:
			rec_type = "UPDATE-FL";
			break;
		case XT_LOG_ENT_INSERT:
			rec_type = "INSERT";
			break;
		case XT_LOG_ENT_INSERT_FL:
			rec_type = "INSERT-FL";
			break;
		case XT_LOG_ENT_DELETE:
			rec_type = "DELETE";
			break;
		case XT_LOG_ENT_DELETE_FL:
			rec_type = "DELETE-FL-BG";
			break;
		case XT_LOG_ENT_UPDATE_BG:
			rec_type = "UPDATE-BG";
			break;
		case XT_LOG_ENT_UPDATE_FL_BG:
			rec_type = "UPDATE-FL-BG";
			break;
		case XT_LOG_ENT_INSERT_BG:
			rec_type = "INSERT-BG";
			break;
		case XT_LOG_ENT_INSERT_FL_BG:
			rec_type = "INSERT-FL-BG";
			break;
		case XT_LOG_ENT_DELETE_BG:
			rec_type = "DELETE-BG";
			break;
		case XT_LOG_ENT_DELETE_FL_BG:
			rec_type = "DELETE-FL-BG";
			break;
		case XT_LOG_ENT_REC_FREED:
			rec_type = "FREE REC";
			break;
		case XT_LOG_ENT_REC_REMOVED:
			rec_type = "REMOVED REC";
			break;
		case XT_LOG_ENT_REC_REMOVED_EXT:
			rec_type = "REMOVED-X REC";
			break;
		case XT_LOG_ENT_REC_REMOVED_BI:
			rec_type = "REMOVED-BI REC";
			break;
		case XT_LOG_ENT_REC_MOVED:
			rec_type = "MOVED REC";
			break;
		case XT_LOG_ENT_REC_CLEANED:
			rec_type = "CLEAN REC";
			break;
		case XT_LOG_ENT_REC_CLEANED_1:
			rec_type = "CLEAN REC-1";
			break;
		case XT_LOG_ENT_REC_UNLINKED:
			rec_type = "UNLINK REC";
			break;
		case XT_LOG_ENT_ROW_NEW:
			rec_type = "NEW ROW";
			break;
		case XT_LOG_ENT_ROW_NEW_FL:
			rec_type = "NEW ROW-FL";
			break;
		case XT_LOG_ENT_ROW_ADD_REC:
			rec_type = "REC ADD ROW";
			break;
		case XT_LOG_ENT_ROW_SET:
			rec_type = "SET ROW";
			break;
		case XT_LOG_ENT_ROW_FREED:
			rec_type = "FREE ROW";
			break;
		case XT_LOG_ENT_OP_SYNC:
			rec_type = "OP SYNC";
			break;
		case XT_LOG_ENT_NO_OP:
			rec_type = "NO OP";
			break;
		case XT_LOG_ENT_END_OF_LOG:
			rec_type = "END OF LOG";
			break;
	}

	if (log)
		PRINTF("log=%d offset=%d ", (int) log, (int) offset);
	PRINTF("%s ", rec_type);
	if (type)
		PRINTF("op=%lu tab=%lu %s=%lu ", (u_long) op_no, (u_long) tab_id, type, (u_long) rec_id);
	if (row_id)
		PRINTF("row=%lu ", (u_long) row_id);
	if (log_id)
		PRINTF("log=%lu offset=%lu ", (u_long) log_id, (u_long) log_offset);
	if (xn_set)
		PRINTF("xact=%lu ", (u_long) xn_id);

#ifdef TRACE_RECORD_DATA
	if (rec_buf) {
		switch (rec_buf->tr_rec_type_1 & XT_TAB_STATUS_MASK) {
			case XT_TAB_STATUS_FREED:
				PRINTF("FREE");
				break;
			case XT_TAB_STATUS_DELETE:
				PRINTF("DELE");
				break;
			case XT_TAB_STATUS_FIXED:
				PRINTF("FIX-");
				break;
			case XT_TAB_STATUS_VARIABLE:
				PRINTF("VAR-");
				break;
			case XT_TAB_STATUS_EXT_DLOG:
				PRINTF("EXT-");
				break;
		}
		if (rec_buf->tr_rec_type_1 & XT_TAB_STATUS_CLEANED_BIT)
			PRINTF("C");
		else
			PRINTF(" ");
	}
	if (ext_rec) {
		rec_len -= offsetof(XTTabRecExtDRec, re_data);
		xt_print_bytes((xtWord1 *) ext_rec, offsetof(XTTabRecExtDRec, re_data));
		PRINTF("| ");
		if (rec_len > 20)
			rec_len = 20;
		xt_print_bytes(ext_rec->re_data, rec_len);
	}
	if (fix_rec) {
		rec_len -= offsetof(XTTabRecFixDRec, rf_data);
		xt_print_bytes((xtWord1 *) fix_rec, offsetof(XTTabRecFixDRec, rf_data));
		PRINTF("| ");
		if (rec_len > 20)
			rec_len = 20;
		xt_print_bytes(fix_rec->rf_data, rec_len);
	}
#endif

	PRINTF("\n");
}

#ifdef DEBUG_PRINT
void check_rows(void)
{
	static XTOpenFilePtr of = NULL;

	if (!of)
		of = xt_open_file_ns("./test/test_tab-1.xtr", XT_FS_DEFAULT);
	if (of) {
		size_t size = (size_t) xt_seek_eof_file(NULL, of);
		xtWord8 *buffer = (xtWord8 *) xt_malloc_ns(size);
		xt_pread_file(of, 0, size, size, buffer, NULL);
		for (size_t i=0; i<size/8; i++) {
			if (!buffer[i])
				printf("%d is NULL\n", (int) i);
		}
	}
}

#endif

/* ----------------------------------------------------------------------
 * APPLYING CHANGES IN SEQUENCE
 */

typedef struct XTOperation {
	xtOpSeqNo				or_op_seq;
	xtWord4					or_op_len;
	xtLogID					or_log_id;
	xtLogOffset				or_log_offset;
} XTOperationRec, *XTOperationPtr;

static int xres_cmp_op_seq(struct XTThread *XT_UNUSED(self), register const void *XT_UNUSED(thunk), register const void *a, register const void *b)
{
	xtOpSeqNo		lf_op_seq = *((xtOpSeqNo *) a);
	XTOperationPtr	lf_ptr = (XTOperationPtr) b;

	if (lf_op_seq == lf_ptr->or_op_seq)
		return 0;
	if (XTTableSeq::xt_op_is_before(lf_op_seq, lf_ptr->or_op_seq))
		return -1;
	return 1;
}

xtPublic void xt_xres_init_tab(XTThreadPtr self, XTTableHPtr tab)
{
	tab->tab_op_list = xt_new_sortedlist(self, sizeof(XTOperationRec), 20, 1000, xres_cmp_op_seq, NULL, NULL, TRUE, FALSE);
}

xtPublic void xt_xres_exit_tab(XTThreadPtr self, XTTableHPtr tab)
{
	if (tab->tab_op_list) {
		xt_free_sortedlist(self, tab->tab_op_list);
		tab->tab_op_list = NULL;
	}
}

static xtBool xres_open_table(XTThreadPtr self, XTWriterStatePtr ws, xtTableID tab_id)
{
	XTOpenTablePtr	ot;

	if ((ot = ws->ws_ot)) {
		if (ot->ot_table->tab_id == tab_id)
			return OK;
		xt_db_return_table_to_pool(self, ot);
		ws->ws_ot = NULL;
	}

	if (ws->ws_tab_gone == tab_id)
		return FAILED;
	if ((ws->ws_ot = xt_db_open_pool_table(self, ws->ws_db, tab_id, NULL, TRUE))) {
		XTTableHPtr		tab;

		tab = ws->ws_ot->ot_table;
		if (!tab->tab_ind_rec_log_id) {
			/* Should not happen... */
			tab->tab_ind_rec_log_id = ws->ws_ind_rec_log_id;
			tab->tab_ind_rec_log_offset = ws->ws_ind_rec_log_offset;
		}
		return OK;
	}
	ws->ws_tab_gone = tab_id;
	return FAILED;
}

/* {INDEX-RECOV_ROWID}
 * Add missing index entries during recovery.
 * Set the row ID even if the index entry
 * is not committed. It will be removed later by
 * the sweeper.
 */
static xtBool xres_add_index_entries(XTOpenTablePtr ot, xtRowID row_id, xtRecordID rec_id, xtWord1 *rec_data)
{
	XTTableHPtr			tab = ot->ot_table;
	u_int				idx_cnt;
	XTIndexPtr			*ind;
	//XTIdxSearchKeyRec	key;

	if (tab->tab_dic.dic_disable_index)
		return OK;

	for (idx_cnt=0, ind=tab->tab_dic.dic_keys; idx_cnt<tab->tab_dic.dic_key_count; idx_cnt++, ind++) {
		if (!xt_idx_insert(ot, *ind, row_id, rec_id, rec_data, NULL, TRUE)) {
			/* Check the error, certain errors are recoverable! */
			XTThreadPtr self = xt_get_self();

			if (self->t_exception.e_xt_err == XT_SYSTEM_ERROR &&
				(XT_FILE_IN_USE(self->t_exception.e_sys_err) ||
				 XT_FILE_ACCESS_DENIED(self->t_exception.e_sys_err) ||
				 XT_FILE_TOO_MANY_OPEN(self->t_exception.e_sys_err) ||
				 self->t_exception.e_sys_err == XT_ENOMEM)) {
				ot->ot_err_index_no = (*ind)->mi_index_no;
				return FAILED;
			}

			/* TODO: Write something to the index header to indicate that
			 * it is corrupted.
			 */
			xt_tab_disable_index(ot->ot_table, XT_INDEX_CORRUPTED);
			xt_log_and_clear_exception_ns();
			return OK;
		}
	}
	return OK;
}

static void xres_remove_index_entries(XTOpenTablePtr ot, xtRecordID rec_id, xtWord1 *rec_data)
{
	XTTableHPtr	tab = ot->ot_table;
	u_int		idx_cnt;
	XTIndexPtr	*ind;

	if (tab->tab_dic.dic_disable_index)
		return;

	for (idx_cnt=0, ind=tab->tab_dic.dic_keys; idx_cnt<tab->tab_dic.dic_key_count; idx_cnt++, ind++) {
		if (!xt_idx_delete(ot, *ind, rec_id, rec_data))
			xt_log_and_clear_exception_ns();
	}
}

static xtWord1 *xres_load_record(XTThreadPtr self, XTOpenTablePtr ot, xtRecordID rec_id, xtWord1 *data, size_t red_size, XTInfoBufferPtr rec_buf, u_int cols_req)
{
	XTTableHPtr	tab = ot->ot_table;
	xtWord1		*rec_data;

	rec_data = ot->ot_row_rbuffer;

	ASSERT(red_size <= ot->ot_row_rbuf_size);
	ASSERT(tab->tab_dic.dic_rec_size <= ot->ot_row_rbuf_size);
	if (data) {
		if (rec_data != data)
			memcpy(rec_data, data, red_size);
	}
	else {
		/* It can be that less than 'dic_rec_size' was written for
		 * variable length type records.
		 * If this is the last record in the file, then we will read
		 * less than actual record size.
		 */
		if (!XT_PREAD_RR_FILE(ot->ot_rec_file, xt_rec_id_to_rec_offset(tab, rec_id), tab->tab_dic.dic_rec_size, 0, rec_data, &red_size, &self->st_statistics.st_rec, self))
			goto failed;
		
		if (red_size < sizeof(XTTabRecHeadDRec))
			return NULL;
	}
	
	if (XT_REC_IS_FIXED(rec_data[0]))
		rec_data = ot->ot_row_rbuffer + XT_REC_FIX_HEADER_SIZE;
	else {
		if (!xt_ib_alloc(NULL, rec_buf, tab->tab_dic.dic_mysql_buf_size))
			goto failed;
		if (XT_REC_IS_VARIABLE(rec_data[0])) {
			if (!myxt_load_row(ot, rec_data + XT_REC_FIX_HEADER_SIZE, rec_buf->ib_db.db_data, cols_req))
				goto failed;
		}
		else if (XT_REC_IS_EXT_DLOG(rec_data[0])) {
			if (red_size < XT_REC_EXT_HEADER_SIZE)
				return NULL;

			ASSERT(cols_req);
			if (cols_req && cols_req <= tab->tab_dic.dic_fix_col_count) {
				if (!myxt_load_row(ot, rec_data + XT_REC_EXT_HEADER_SIZE, rec_buf->ib_db.db_data, cols_req))
					goto failed;
			}
			else {
				if (!xt_tab_load_ext_data(ot, rec_id, rec_buf->ib_db.db_data, cols_req))
					goto failed;
			}
		}
		else
			/* This is possible, the record has already been cleaned up. */
			return NULL;
		rec_data = rec_buf->ib_db.db_data;
	}

	return rec_data;

	failed:
	/* Running out of memory should not be ignored. */
	if (self->t_exception.e_xt_err == XT_SYSTEM_ERROR &&
		self->t_exception.e_sys_err == XT_ENOMEM)
		xt_throw(self);
	xt_log_and_clear_exception_ns();
	return NULL;
}

/*
 * Apply a change from the log.
 *
 * This function is basically very straight forward, were it not
 * for the option to apply operations out of sequence.
 * (i.e. in_sequence == FALSE)
 *
 * If operations are applied in sequence, then they can be
 * applied blindly. The update operation is just executed as
 * it was logged.
 *
 * If the changes are not in sequence, then some operation are missing,
 * however, the operations that are present are in the correct order.
 *
 * This can only happen at the end of recovery!!!
 * After we have applied all operations in the log we may be
 * left with some operations that have not been applied
 * because operations were logged out of sequence.
 *
 * The application of these operations there has to take into
 * account the current state of the database.
 * They are then applied in a manner that maintains the
 * database consistency.
 *
 * For example, a record that is freed, is free by placing it
 * on the current free list. Part of the data logged for the
 * operation is ignored. Namely: the "next block" pointer
 * that was originally written into the freed record.
 */
static void xres_apply_change(XTThreadPtr self, XTOpenTablePtr ot, XTXactLogBufferDPtr record, xtBool in_sequence, xtBool check_index, XTInfoBufferPtr rec_buf)
{
	XTTableHPtr			tab = ot->ot_table;
	size_t				len;
	xtRecordID			rec_id;
	xtRefID				free_ref_id;
	XTTabRecFreeDRec	free_rec;
	xtRowID				row_id;
	XTTabRowRefDRec		row_buf;
	XTTabRecHeadDRec	rec_head;
	size_t				tfer;
	xtRecordID			link_rec_id, prev_link_rec_id;
	xtWord1				*rec_data = NULL;
	XTTabRecFreeDPtr	free_data;

	if (tab->tab_dic.dic_key_count == 0)
		check_index = FALSE;

	switch (record->xl.xl_status_1) {
		case XT_LOG_ENT_REC_MODIFIED:
		case XT_LOG_ENT_UPDATE:
		case XT_LOG_ENT_INSERT:
		case XT_LOG_ENT_DELETE:
		case XT_LOG_ENT_UPDATE_BG:
		case XT_LOG_ENT_INSERT_BG:
		case XT_LOG_ENT_DELETE_BG:
			rec_id = XT_GET_DISK_4(record->xu.xu_rec_id_4);

			/* This should be done before we apply change to table, as otherwise we lose
			 * the key value that we need to remove from index
			 */
			if (check_index && record->xl.xl_status_1 == XT_LOG_ENT_REC_MODIFIED) {
				if ((rec_data = xres_load_record(self, ot, rec_id, NULL, 0, rec_buf, tab->tab_dic.dic_ind_cols_req)))
					xres_remove_index_entries(ot, rec_id, rec_data);			
			}

			len = (size_t) XT_GET_DISK_2(record->xu.xu_size_2);
			if (!XT_PWRITE_RR_FILE(ot->ot_rec_file, xt_rec_id_to_rec_offset(tab, rec_id), len, (xtWord1 *) &record->xu.xu_rec_type_1, &ot->ot_thread->st_statistics.st_rec, ot->ot_thread))
				xt_throw(self);
			tab->tab_bytes_to_flush += len;

			if (check_index) {
				switch (record->xl.xl_status_1) {
					case XT_LOG_ENT_DELETE:
					case XT_LOG_ENT_DELETE_BG:
						break;
					default:
						if ((rec_data = xres_load_record(self, ot, rec_id, &record->xu.xu_rec_type_1, len, rec_buf, tab->tab_dic.dic_ind_cols_req))) {
							row_id = XT_GET_DISK_4(record->xu.xu_row_id_4);
							if (!xres_add_index_entries(ot, row_id, rec_id, rec_data))
								xt_throw(self);
						}
						break;
				}
			}

			if (!in_sequence) {
				/* A record has been allocated from the EOF, but out of sequence.
				 * This could leave a gap where other records were allocated
				 * from the EOF, but those operations have been lost!
				 * We compensate for this by adding all blocks between
				 * to the free list.
				 */
				free_rec.rf_rec_type_1 = XT_TAB_STATUS_FREED;
				free_rec.rf_not_used_1 = 0;
				while (tab->tab_head_rec_eof_id < rec_id) {
					XT_SET_DISK_4(free_rec.rf_next_rec_id_4, tab->tab_head_rec_free_id);
					if (!XT_PWRITE_RR_FILE(ot->ot_rec_file, tab->tab_head_rec_eof_id, sizeof(XTTabRecFreeDRec), (xtWord1 *) &free_rec, &ot->ot_thread->st_statistics.st_rec, ot->ot_thread))
						xt_throw(self);
					tab->tab_bytes_to_flush += sizeof(XTTabRecFreeDRec);
					tab->tab_head_rec_free_id = tab->tab_head_rec_eof_id;
					tab->tab_head_rec_eof_id++;
				}
			}
			if (tab->tab_head_rec_eof_id < rec_id + 1)
				tab->tab_head_rec_eof_id = rec_id + 1;
			tab->tab_flush_pending = TRUE;
			break;
		case XT_LOG_ENT_UPDATE_FL:
		case XT_LOG_ENT_INSERT_FL:
		case XT_LOG_ENT_DELETE_FL:
		case XT_LOG_ENT_UPDATE_FL_BG:
		case XT_LOG_ENT_INSERT_FL_BG:
		case XT_LOG_ENT_DELETE_FL_BG:
			rec_id = XT_GET_DISK_4(record->xf.xf_rec_id_4);
			len = (size_t) XT_GET_DISK_2(record->xf.xf_size_2);
			free_ref_id = XT_GET_DISK_4(record->xf.xf_free_rec_id_4);

			if (check_index &&
				record->xf.xf_status_1 != XT_LOG_ENT_DELETE_FL &&
				record->xf.xf_status_1 != XT_LOG_ENT_DELETE_FL_BG) {
				if ((rec_data = xres_load_record(self, ot, rec_id, &record->xf.xf_rec_type_1, len, rec_buf, tab->tab_dic.dic_ind_cols_req))) {
					row_id = XT_GET_DISK_4(record->xf.xf_row_id_4);
					if (!xres_add_index_entries(ot, row_id, rec_id, rec_data))
						xt_throw(self);
				}
			}

			if (!in_sequence) {
				/* This record was allocated from the free list.
				 * Because this operation is out of sequence, there
				 * could have been other allocations from the
				 * free list before this, that have gone missing.
				 * For this reason we have to search the current
				 * free list and remove the record.
				 */
				link_rec_id = tab->tab_head_rec_free_id;
				prev_link_rec_id = 0;
				while (link_rec_id) {
					if (!XT_PREAD_RR_FILE(ot->ot_rec_file, xt_rec_id_to_rec_offset(tab, link_rec_id), sizeof(XTTabRecFreeDRec), sizeof(XTTabRecFreeDRec), (xtWord1 *) &free_rec, NULL, &self->st_statistics.st_rec, self))
						xt_throw(self);
					if (link_rec_id == rec_id)
						break;
					prev_link_rec_id = link_rec_id;
					link_rec_id = XT_GET_DISK_4(free_rec.rf_next_rec_id_4);
				}
				if (link_rec_id == rec_id) {
					/* The block was found on the free list.
					 * remove it: */
					if (prev_link_rec_id) {
						/* We write the record from position 'link_rec_id' into
						 * position 'prev_link_rec_id'. This unlinks 'link_rec_id'!
						 */
						if (!XT_PWRITE_RR_FILE(ot->ot_rec_file, xt_rec_id_to_rec_offset(tab, prev_link_rec_id), sizeof(XTTabRecFreeDRec), (xtWord1 *) &free_rec, &ot->ot_thread->st_statistics.st_rec, ot->ot_thread))
							xt_throw(self);
						tab->tab_bytes_to_flush += sizeof(XTTabRecFreeDRec);
						free_ref_id = tab->tab_head_rec_free_id;
					}
					else
						/* The block is at the front of the list: */
						free_ref_id = XT_GET_DISK_4(free_rec.rf_next_rec_id_4);
				}
				else {
					/* Not found on the free list? */
					if (tab->tab_head_rec_eof_id < rec_id + 1)
						tab->tab_head_rec_eof_id = rec_id + 1;
					goto write_mod_data;
				}
			}
			if (tab->tab_head_rec_eof_id < rec_id + 1)
				tab->tab_head_rec_eof_id = rec_id + 1;
			tab->tab_head_rec_free_id = free_ref_id;
			tab->tab_head_rec_fnum--;
			write_mod_data:
			if (!XT_PWRITE_RR_FILE(ot->ot_rec_file, xt_rec_id_to_rec_offset(tab, rec_id), len, (xtWord1 *) &record->xf.xf_rec_type_1, &ot->ot_thread->st_statistics.st_rec, ot->ot_thread))
				xt_throw(self);
			tab->tab_bytes_to_flush += len;
			tab->tab_flush_pending = TRUE;
			break;
		case XT_LOG_ENT_REC_REMOVED:
		case XT_LOG_ENT_REC_REMOVED_EXT: {
			xtBool			record_loaded;
			XTTabRecExtDPtr	ext_rec;
			size_t			red_size;
			xtWord4			log_over_size = 0;
			xtLogID			data_log_id = 0;
			xtLogOffset		data_log_offset = 0;
			u_int			cols_required = 0;

			rec_id = XT_GET_DISK_4(record->fr.fr_rec_id_4);
			free_data = (XTTabRecFreeDPtr) &record->fr.fr_rec_type_1;

			/* This is a short-cut, it does not require loading the record: */
			if (!check_index && !tab->tab_dic.dic_blob_count && record->fr.fr_status_1 != XT_LOG_ENT_REC_REMOVED_EXT)
				goto do_rec_freed;

			ext_rec = (XTTabRecExtDPtr) ot->ot_row_rbuffer;

			if (!XT_PREAD_RR_FILE(ot->ot_rec_file, xt_rec_id_to_rec_offset(tab, rec_id), tab->tab_dic.dic_rec_size, 0, ext_rec, &red_size, &self->st_statistics.st_rec, self)) {
				xt_log_and_clear_exception_ns();
				goto do_rec_freed;
			}

			if (red_size < sizeof(XTTabRecHeadDRec))
				goto do_rec_freed;

			/* Check that the record is the same as the one originally removed.
			 * This can be different if recovery is repeated.
			 * For example:
			 * 
			 * log=21 offset=6304472 REMOVED-X REC op=360616 tab=7 rec=25874 
			 * log=21 offset=6309230 UPDATE-FL op=360618 tab=7 rec=25874 row=26667 log=1 offset=26503077 xact=209 
			 * log=21 offset=6317500 CLEAN REC op=360631 tab=7 rec=25874 
			 * 
			 * If this recovery sequence is repeated, then the REMOVED-X will free the
			 * extended record belonging to the update that came afterwards!
			 *
			 * Additional situation to consider:
			 *
			 * - A record "x" is created, and index entries created.
			 * - A checkpoint is made done.
			 * - Record "x" is deleted due to UPDATE.
			 * - The index entries are removed, but the index is not
			 *   flushed.
			 * - This deletion is written to disk by the writer.
			 * So we have the situation that the remove is on disk,
			 * but the index changes have not been made.
			 *
			 * In this case, skipping to "do_rec_freed" is incorrect.
			 */
			if (record->fr.fr_stat_id_1 != ext_rec->tr_stat_id_1 ||
				XT_GET_DISK_4(record->fr.fr_xact_id_4) != XT_GET_DISK_4(ext_rec->tr_xact_id_4))
				goto dont_remove_x_record;

			if (record->xl.xl_status_1 == XT_LOG_ENT_REC_REMOVED_EXT) {
				if (!XT_REC_IS_EXT_DLOG(ext_rec->tr_rec_type_1))
					goto dont_remove_x_record;
				if (red_size < offsetof(XTTabRecExtDRec, re_data))
					goto dont_remove_x_record;

				/* Save this for later (can be overwritten by xres_load_record(): */
				data_log_id = XT_GET_DISK_2(ext_rec->re_log_id_2);
				data_log_offset = XT_GET_DISK_6(ext_rec->re_log_offs_6);
				log_over_size = XT_GET_DISK_4(ext_rec->re_log_dat_siz_4);
			}
			dont_remove_x_record:

			record_loaded = FALSE;

			if (check_index) {
				cols_required = tab->tab_dic.dic_ind_cols_req;
				if (tab->tab_dic.dic_blob_cols_req > cols_required)
					cols_required = tab->tab_dic.dic_blob_cols_req;
				if (!(rec_data = xres_load_record(self, ot, rec_id, ot->ot_row_rbuffer, red_size, rec_buf, cols_required)))
					goto do_rec_freed;
				record_loaded = TRUE;
				xres_remove_index_entries(ot, rec_id, rec_data);
			}

			if (tab->tab_dic.dic_blob_count) {
				if (!record_loaded) {
					if (tab->tab_dic.dic_blob_cols_req > cols_required)
						cols_required = tab->tab_dic.dic_blob_cols_req;
					if (!(rec_data = xres_load_record(self, ot, rec_id, ot->ot_row_rbuffer, red_size, rec_buf, cols_required)))
						/* [(7)] REMOVE is followed by FREE:
						goto get_rec_offset;
						*/
						goto do_rec_freed;
					record_loaded = TRUE;
				}
			}

			if (record->xl.xl_status_1 == XT_LOG_ENT_REC_REMOVED_EXT) {
				/* Note: dlb_delete_log() may be repeated, but should handle this:
				 * 
				 * Example:
				 * log=5 offset=213334 CLEAN REC op=28175 tab=1 rec=317428 
				 * ...
				 * log=6 offset=321063 REMOVED-X REC op=33878 tab=1 rec=317428 
				 *
				 * When this sequence is repeated during recovery, then CLEAN REC
				 * will reset the status byte of the record so that it
				 * comes back to here!
				 *
				 * The check for zero is probably not required here.
				 */
				if (data_log_id && data_log_offset && log_over_size) {
					if (!ot->ot_thread->st_dlog_buf.dlb_delete_log(data_log_id, data_log_offset, log_over_size, tab->tab_id, rec_id, self)) {
						if (ot->ot_thread->t_exception.e_xt_err != XT_ERR_BAD_EXT_RECORD &&
							ot->ot_thread->t_exception.e_xt_err != XT_ERR_DATA_LOG_NOT_FOUND)
							xt_log_and_clear_exception_ns();
					}
				}
			}

			goto do_rec_freed;
		}
		case XT_LOG_ENT_REC_REMOVED_BI: {
			/*
			 * For deletion we need the complete before image because of the following problem.
			 *
			 * DROP TABLE IF EXISTS t1;
			 * CREATE TABLE t1 (ID int primary key auto_increment, value int, index (value)) engine=pbxt;
			 * 
			 * insert t1(value) values(50);
			 * 
			 * -- CHECKPOINT --
			 * 
			 * update t1 set value = 60;
			 * 
			 * -- PAUSE --
			 * 
			 * update t1 set value = 70;
			 * 
			 * -- CRASH --
			 * 
			 * select value from t1;
			 * select * from t1;
			 * 
			 * 081203 12:11:46 [Note] PBXT: Recovering from 1-148, bytes to read: 33554284
			 * log=1 offset=148 UPDATE-BG op=5 tab=1 rec=2 row=1 xact=3 
			 * log=1 offset=188 REC ADD ROW op=6 tab=1 row=1 
			 * log=1 offset=206 COMMIT xact=3 
			 * log=1 offset=216 REMOVED REC op=7 tab=1 rec=1 xact=2 
			 * log=1 offset=241 CLEAN REC op=8 tab=1 rec=2 
			 * log=1 offset=261 CLEANUP xact=3 
			 * log=1 offset=267 UPDATE-FL-BG op=9 tab=1 rec=1 row=1 xact=4 
			 * log=1 offset=311 REC ADD ROW op=10 tab=1 row=1 
			 * log=1 offset=329 COMMIT xact=4 
			 * log=1 offset=339 REMOVED REC op=11 tab=1 rec=2 xact=3 
			 * log=1 offset=364 CLEAN REC op=12 tab=1 rec=1 
			 * log=1 offset=384 CLEANUP xact=4 
			 * 081203 12:12:15 [Note] PBXT: Recovering complete at 1-390, bytes read: 33554284
			 * 
			 * mysql> select value from t1;
			 * +-------+
			 * | value |
			 * +-------+
			 * |    50 | 
			 * |    70 | 
			 * +-------+
			 * 2 rows in set (55.99 sec)
			 * 
			 * mysql> select * from t1;
			 * +----+-------+
			 * | ID | value |
			 * +----+-------+
			 * |  1 |    70 | 
			 * +----+-------+
			 * 1 row in set (0.00 sec)
			 */
			XTTabRecExtDPtr	ext_rec;
			xtWord4			log_over_size = 0;
			xtLogID			data_log_id = 0;
			xtLogOffset		data_log_offset = 0;
			u_int			cols_required = 0;
			xtBool			record_loaded;
			size_t			rec_size;		

			rec_id = XT_GET_DISK_4(record->rb.rb_rec_id_4);
			rec_size = XT_GET_DISK_2(record->rb.rb_size_2);

			ext_rec = (XTTabRecExtDPtr) &record->rb.rb_rec_type_1;

			if (XT_REC_IS_EXT_DLOG(record->rb.rb_rec_type_1)) {
				/* Save this for later (can be overwritten by xres_load_record(): */
				data_log_id = XT_GET_DISK_2(ext_rec->re_log_id_2);
				data_log_offset = XT_GET_DISK_6(ext_rec->re_log_offs_6);
				log_over_size = XT_GET_DISK_4(ext_rec->re_log_dat_siz_4);
			}

			record_loaded = FALSE;

			if (check_index) {
				cols_required = tab->tab_dic.dic_ind_cols_req;
				if (!(rec_data = xres_load_record(self, ot, rec_id, &record->rb.rb_rec_type_1, rec_size, rec_buf, cols_required)))
					goto go_on_to_free;
				record_loaded = TRUE;
				xres_remove_index_entries(ot, rec_id, rec_data);
			}

			if (data_log_id && data_log_offset && log_over_size) {
				if (!ot->ot_thread->st_dlog_buf.dlb_delete_log(data_log_id, data_log_offset, log_over_size, tab->tab_id, rec_id, self)) {
					if (ot->ot_thread->t_exception.e_xt_err != XT_ERR_BAD_EXT_RECORD &&
						ot->ot_thread->t_exception.e_xt_err != XT_ERR_DATA_LOG_NOT_FOUND)
						xt_log_and_clear_exception_ns();
				}
			}

			go_on_to_free:
			/* Use the new record type: */
			record->rb.rb_rec_type_1 = record->rb.rb_new_rec_type_1;
			free_data = (XTTabRecFreeDPtr) &record->rb.rb_rec_type_1;
			goto do_rec_freed;
		}
		case XT_LOG_ENT_REC_FREED:
			rec_id = XT_GET_DISK_4(record->fr.fr_rec_id_4);
			free_data = (XTTabRecFreeDPtr) &record->fr.fr_rec_type_1;
			do_rec_freed:
			if (!in_sequence) {
				size_t	red_size;

				/* Free the record.
				 * We place the record on front of the current
				 * free list.
				 *
				 * However, before we do this, we remove the record
				 * from its row list, if the record is on a row list.
				 *
				 * We do this here, because in the normal removal
				 * from the row list uses the operations:
				 *
				 * XT_LOG_ENT_REC_UNLINKED, XT_LOG_ENT_ROW_SET and
				 * XT_LOG_ENT_ROW_FREED.
				 *
				 * When operations are performed out of sequence,
				 * these operations are ignored for the purpose
				 * of removing the record from the row.
				 */
				if (!XT_PREAD_RR_FILE(ot->ot_rec_file, xt_rec_id_to_rec_offset(tab, rec_id), sizeof(XTTabRecHeadDRec), sizeof(XTTabRecHeadDRec), (xtWord1 *) &rec_head, NULL, &self->st_statistics.st_rec, self))
					xt_throw(self);
				/* The record is already free: */
				if (XT_REC_IS_FREE(rec_head.tr_rec_type_1))
					goto free_done;
				row_id = XT_GET_DISK_4(rec_head.tr_row_id_4);

				/* Search the row for this record: */
				if (!XT_PREAD_RR_FILE(ot->ot_row_file, xt_row_id_to_row_offset(tab, row_id), sizeof(XTTabRowRefDRec), sizeof(XTTabRowRefDRec), (xtWord1 *) &row_buf, NULL, &self->st_statistics.st_rec, self))
					xt_throw(self);
				link_rec_id = XT_GET_DISK_4(row_buf.rr_ref_id_4);
				prev_link_rec_id = 0;
				while (link_rec_id) {
					if (!XT_PREAD_RR_FILE(ot->ot_rec_file, xt_rec_id_to_rec_offset(tab, link_rec_id), sizeof(XTTabRecHeadDRec), 0, (xtWord1 *) &rec_head, &red_size, &self->st_statistics.st_rec, self)) {
						xt_log_and_clear_exception(self);
						break;
					}
					if (red_size < sizeof(XTTabRecHeadDRec))
						break;
					if (link_rec_id == rec_id)
						break;
					if (XT_GET_DISK_4(rec_head.tr_row_id_4) != row_id)
						break;
					switch (rec_head.tr_rec_type_1 & XT_TAB_STATUS_MASK) {
						case XT_TAB_STATUS_FREED:
							break;
						case XT_TAB_STATUS_DELETE:
						case XT_TAB_STATUS_FIXED:
						case XT_TAB_STATUS_VARIABLE:
						case XT_TAB_STATUS_EXT_DLOG:
							break;
						default:
							ASSERT(FALSE);
							goto exit_loop;
					}
					if (rec_head.tr_rec_type_1 & ~(XT_TAB_STATUS_CLEANED_BIT | XT_TAB_STATUS_MASK)) {
						ASSERT(FALSE);
						break;
					}
					prev_link_rec_id = link_rec_id;
					link_rec_id = XT_GET_DISK_4(rec_head.tr_prev_rec_id_4);
				}

				exit_loop:
				if (link_rec_id == rec_id) {
					/* The record was found on the row list, remove it: */
					if (prev_link_rec_id) {
						/* We write the previous variation pointer from position 'link_rec_id' into
						 * variation pointer of the 'prev_link_rec_id' record. This unlinks 'link_rec_id'!
						 */
						if (!XT_PWRITE_RR_FILE(ot->ot_rec_file, xt_rec_id_to_rec_offset(tab, prev_link_rec_id) + offsetof(XTTabRecHeadDRec, tr_prev_rec_id_4), XT_RECORD_ID_SIZE, (xtWord1 *) &rec_head.tr_prev_rec_id_4, &ot->ot_thread->st_statistics.st_rec, ot->ot_thread))
							xt_throw(self);
						tab->tab_bytes_to_flush += XT_RECORD_ID_SIZE;
					}
					else {
						/* The record is at the front of the row list: */
						xtRefID ref_id = XT_GET_DISK_4(rec_head.tr_prev_rec_id_4);
						XT_SET_DISK_4(row_buf.rr_ref_id_4, ref_id);
						if (!XT_PWRITE_RR_FILE(ot->ot_row_file, xt_row_id_to_row_offset(tab, row_id), sizeof(XTTabRowRefDRec), (xtWord1 *) &row_buf, &ot->ot_thread->st_statistics.st_rec, ot->ot_thread))
							xt_throw(self);
						tab->tab_bytes_to_flush += sizeof(XTTabRowRefDRec);
					}
				}				

				/* Now we free the record, by placing it at the front of
				 * the free list:
				 */
				XT_SET_DISK_4(free_data->rf_next_rec_id_4, tab->tab_head_rec_free_id);				
			}
			tab->tab_head_rec_free_id = rec_id;
			tab->tab_head_rec_fnum++;
			if (!XT_PWRITE_RR_FILE(ot->ot_rec_file, xt_rec_id_to_rec_offset(tab, rec_id), sizeof(XTTabRecFreeDRec), (xtWord1 *) free_data, &ot->ot_thread->st_statistics.st_rec, ot->ot_thread))
				xt_throw(self);
			tab->tab_bytes_to_flush += sizeof(XTTabRecFreeDRec);
			tab->tab_flush_pending = TRUE;
			free_done:
			break;
		case XT_LOG_ENT_REC_MOVED:
			len = 8;
			rec_id = XT_GET_DISK_4(record->xw.xw_rec_id_4);
			if (!XT_PWRITE_RR_FILE(ot->ot_rec_file, xt_rec_id_to_rec_offset(tab, rec_id) + offsetof(XTTabRecExtDRec, re_log_id_2), len, (xtWord1 *) &record->xw.xw_rec_type_1, &ot->ot_thread->st_statistics.st_rec, ot->ot_thread))
				xt_throw(self);
			tab->tab_bytes_to_flush += len;
			tab->tab_flush_pending = TRUE;
			break;
		case XT_LOG_ENT_REC_CLEANED:
			len = offsetof(XTTabRecHeadDRec, tr_prev_rec_id_4) + XT_RECORD_ID_SIZE;
			goto get_rec_offset;
		case XT_LOG_ENT_REC_CLEANED_1:
			len = 1;
			goto get_rec_offset;
		case XT_LOG_ENT_REC_UNLINKED:
			if (!in_sequence) {
				/* Unlink the record.
				 * This is done when the record is freed.
				 */
				break;
			}
			len = offsetof(XTTabRecHeadDRec, tr_prev_rec_id_4) + XT_RECORD_ID_SIZE;
			get_rec_offset:
			rec_id = XT_GET_DISK_4(record->xw.xw_rec_id_4);
			if (!XT_PWRITE_RR_FILE(ot->ot_rec_file, xt_rec_id_to_rec_offset(tab, rec_id), len, (xtWord1 *) &record->xw.xw_rec_type_1, &ot->ot_thread->st_statistics.st_rec, ot->ot_thread))
				xt_throw(self);
			tab->tab_bytes_to_flush += len;
			tab->tab_flush_pending = TRUE;
			break;
		case XT_LOG_ENT_ROW_NEW:
			len = offsetof(XTactRowAddedEntryDRec, xa_free_list_4);
			row_id = XT_GET_DISK_4(record->xa.xa_row_id_4);
			if (!in_sequence) {
				/* A row was allocated from the EOF. Because operations are missing.
				 * The blocks between the current EOF and the new EOF need to be
				 * place on the free list!
				 */				
				while (tab->tab_head_row_eof_id < row_id) {
					XT_SET_DISK_4(row_buf.rr_ref_id_4, tab->tab_head_row_free_id);
					if (!XT_PWRITE_RR_FILE(ot->ot_row_file, xt_row_id_to_row_offset(tab, tab->tab_head_row_eof_id), sizeof(XTTabRowRefDRec), (xtWord1 *) &row_buf, &ot->ot_thread->st_statistics.st_rec, ot->ot_thread))
						xt_throw(self);
					tab->tab_bytes_to_flush += sizeof(XTTabRowRefDRec);
					tab->tab_head_row_free_id = tab->tab_head_row_eof_id;
					tab->tab_head_row_eof_id++;
				}
			}
			if (tab->tab_head_row_eof_id < row_id + 1)
				tab->tab_head_row_eof_id = row_id + 1;
			tab->tab_flush_pending = TRUE;
			break;
		case XT_LOG_ENT_ROW_NEW_FL:
			len = sizeof(XTactRowAddedEntryDRec);
			row_id = XT_GET_DISK_4(record->xa.xa_row_id_4);
			free_ref_id = XT_GET_DISK_4(record->xa.xa_free_list_4);
			if (!in_sequence) {
				size_t red_size;
				/* The record was taken from the free list.
				 * If the operations were in sequence, then this would be
				 * the front of the free list now.
				 * However, because operations are missing, it may no
				 * longer be the front of the free list!
				 * Search and remove:
				 */
				link_rec_id = tab->tab_head_row_free_id;
				prev_link_rec_id = 0;
				while (link_rec_id) {
					if (!XT_PREAD_RR_FILE(ot->ot_row_file, xt_row_id_to_row_offset(tab, link_rec_id), sizeof(XTTabRowRefDRec), 0, (xtWord1 *) &row_buf, &red_size, &self->st_statistics.st_rec, self)) {
						xt_log_and_clear_exception(self);
						break;
					}
					if (red_size < sizeof(XTTabRowRefDRec))
						break;
					if (link_rec_id == row_id)
						break;
					prev_link_rec_id = link_rec_id;
					link_rec_id = XT_GET_DISK_4(row_buf.rr_ref_id_4);
				}
				if (link_rec_id == row_id) {
					/* The block was found on the free list, remove it: */
					if (prev_link_rec_id) {
						/* We write the record from position 'link_rec_id' into
						 * position 'prev_link_rec_id'. This unlinks 'link_rec_id'!
						 */
						if (!XT_PWRITE_RR_FILE(ot->ot_row_file, xt_row_id_to_row_offset(tab, prev_link_rec_id), sizeof(XTTabRowRefDRec), (xtWord1 *) &row_buf, &ot->ot_thread->st_statistics.st_rec, ot->ot_thread))
							xt_throw(self);
						tab->tab_bytes_to_flush += sizeof(XTTabRowRefDRec);
						free_ref_id = tab->tab_head_row_free_id;
					}
					else
						/* The block is at the front of the free list: */
						free_ref_id = XT_GET_DISK_4(row_buf.rr_ref_id_4);
				}
				else {
					/* Not found? */
					if (tab->tab_head_row_eof_id < row_id + 1)
						tab->tab_head_row_eof_id = row_id + 1;
					break;
				}
					
			}
			if (tab->tab_head_row_eof_id < row_id + 1)
				tab->tab_head_row_eof_id = row_id + 1;
			tab->tab_head_row_free_id = free_ref_id;
			tab->tab_head_row_fnum--;
			tab->tab_flush_pending = TRUE;
			break;
		case XT_LOG_ENT_ROW_FREED:
			row_id = XT_GET_DISK_4(record->wr.wr_row_id_4);
			if (!in_sequence) {
				/* Free the row.
				 * Since this operation is being performed out of sequence, we
				 * must assume that some other free and allocation operations
				 * must be missing.
				 * For this reason, we add the row to the front of the
				 * existing free list.
				 */
				XT_SET_DISK_4(record->wr.wr_ref_id_4, tab->tab_head_row_free_id);
			}
			tab->tab_head_row_free_id = row_id;
			tab->tab_head_row_fnum++;
			goto write_row_data;
		case XT_LOG_ENT_ROW_ADD_REC:
			row_id = XT_GET_DISK_4(record->wr.wr_row_id_4);
			if (!in_sequence) {
				if (!XT_PREAD_RR_FILE(ot->ot_row_file, xt_row_id_to_row_offset(tab, row_id), sizeof(XTTabRowRefDRec), 0, (xtWord1 *) &row_buf, &tfer, &self->st_statistics.st_rec, self))
					xt_throw(self);
				if (tfer == sizeof(XTTabRowRefDRec)) {
					/* Add a record to the front of the row.
					 * This is easy, but we have to make sure that the next
					 * pointer in the record is correct.
					 */
					rec_id = XT_GET_DISK_4(record->wr.wr_ref_id_4);
					if (!XT_PREAD_RR_FILE(ot->ot_rec_file, xt_rec_id_to_rec_offset(tab, rec_id), sizeof(XTTabRecHeadDRec), 0, (xtWord1 *) &rec_head, &tfer, &self->st_statistics.st_rec, self))
						xt_throw(self);
					if (tfer == sizeof(XTTabRecHeadDRec) && XT_GET_DISK_4(rec_head.tr_row_id_4) == row_id) {
						/* This is now the correct next pointer: */
						xtRecordID next_ref_id = XT_GET_DISK_4(row_buf.rr_ref_id_4);
						if (XT_GET_DISK_4(rec_head.tr_prev_rec_id_4) != next_ref_id &&
							rec_id != next_ref_id) {
							XT_SET_DISK_4(rec_head.tr_prev_rec_id_4, next_ref_id);
							if (!XT_PWRITE_RR_FILE(ot->ot_rec_file, xt_rec_id_to_rec_offset(tab, rec_id), sizeof(XTTabRecHeadDRec), (xtWord1 *) &rec_head, &ot->ot_thread->st_statistics.st_rec, ot->ot_thread))
								xt_throw(self);
							tab->tab_bytes_to_flush += sizeof(XTTabRecHeadDRec);
						}
					}
				}

			}
			goto write_row_data;
		case XT_LOG_ENT_ROW_SET:
			if (!in_sequence)
				/* This operation is ignored when out of sequence!
				 * The operation is used to remove a record from a row.
				 * This is done automatically when the record is freed.
				 */
				break;
			row_id = XT_GET_DISK_4(record->wr.wr_row_id_4);
			write_row_data:
			ASSERT_NS(XT_GET_DISK_4(record->wr.wr_ref_id_4) < tab->tab_head_rec_eof_id);
			if (!XT_PWRITE_RR_FILE(ot->ot_row_file, xt_row_id_to_row_offset(tab, row_id), sizeof(XTTabRowRefDRec), (xtWord1 *) &record->wr.wr_ref_id_4, &ot->ot_thread->st_statistics.st_rec, self))
				xt_throw(self);
			tab->tab_bytes_to_flush += sizeof(XTTabRowRefDRec);
			if (tab->tab_head_row_eof_id < row_id + 1)
				tab->tab_head_row_eof_id = row_id + 1;
			tab->tab_flush_pending = TRUE;
			break;
		case XT_LOG_ENT_NO_OP:
		case XT_LOG_ENT_END_OF_LOG:
			break;
	}
}

/*
 * Apply all operations that have been buffered
 * for a particular table.
 * Operations are buffered if they are
 * read from the log out of sequence.
 *
 * In this case we buffer, and wait for the
 * out of sequence operations to arrive.
 *
 * When the server is running, this will always be
 * the case. A delay occurs while a transaction 
 * fills its private log buffer.
 */
static void xres_apply_operations(XTThreadPtr self, XTWriterStatePtr ws, xtBool in_sequence)
{
	XTTableHPtr		tab = ws->ws_ot->ot_table;
	u_int			i = 0;
	XTOperationPtr	op;
	xtBool			check_index;

// XTDatabaseHPtr db, XTOpenTablePtr ot, XTXactSeqReadPtr sr, XTDataBufferPtr databuf
	xt_sl_lock(self, tab->tab_op_list);
	for (;;) {
		op = (XTOperationPtr) xt_sl_item_at(tab->tab_op_list, i);
		if (!op)
			break;
		if (in_sequence && tab->tab_head_op_seq+1 != op->or_op_seq)
			break;
		xt_db_set_size(self, &ws->ws_databuf, (size_t) op->or_op_len);
		if (!ws->ws_db->db_xlog.xlog_rnd_read(&ws->ws_seqread, op->or_log_id, op->or_log_offset, (size_t) op->or_op_len, ws->ws_databuf.db_data, NULL, self))
			xt_throw(self);
		check_index = ws->ws_in_recover && xt_comp_log_pos(op->or_log_id, op->or_log_offset, ws->ws_ind_rec_log_id, ws->ws_ind_rec_log_offset) >= 0;
		xres_apply_change(self, ws->ws_ot, (XTXactLogBufferDPtr) ws->ws_databuf.db_data, in_sequence, check_index, &ws->ws_rec_buf);
		tab->tab_head_op_seq = op->or_op_seq;
		if (tab->tab_wr_wake_freeer) {
			if (!XTTableSeq::xt_op_is_before(tab->tab_head_op_seq, tab->tab_wake_freeer_op))
				xt_wr_wake_freeer(self);
		}
		i++;
	}
	xt_sl_remove_from_front(self, tab->tab_op_list, i);
	xt_sl_unlock(self, tab->tab_op_list);
}

/* Check for operations still remaining on tables.
 * These operations are applied even though operations
 * in sequence are missing.
 */
xtBool xres_sync_operations(XTThreadPtr self, XTDatabaseHPtr db, XTWriterStatePtr ws)
{
	u_int			edx;
	XTTableEntryPtr	te_ptr;
	XTTableHPtr		tab;
	xtBool			op_synced = FALSE;

	xt_enum_tables_init(&edx);
	while ((te_ptr = xt_enum_tables_next(self, db, &edx))) {
		/* Dirty read of tab_op_list OK, here because this is the
		 * only thread that updates the list!
		 */
		if ((tab = te_ptr->te_table)) {
			if (xt_sl_get_size(tab->tab_op_list)) {
				op_synced = TRUE;
				if (xres_open_table(self, ws, te_ptr->te_tab_id))
					xres_apply_operations(self, ws, FALSE);
			}

			/* Update the pointer cache: */
			tab->tab_seq.xt_op_seq_set(self, tab->tab_head_op_seq+1);
			tab->tab_row_eof_id = tab->tab_head_row_eof_id;
			tab->tab_row_free_id = tab->tab_head_row_free_id;
			tab->tab_row_fnum = tab->tab_head_row_fnum;
			tab->tab_rec_eof_id = tab->tab_head_rec_eof_id;
			tab->tab_rec_free_id = tab->tab_head_rec_free_id;
			tab->tab_rec_fnum = tab->tab_head_rec_fnum;
		}
	}
	return op_synced;
}

/*
 * Operations from the log are applied in sequence order.
 * If the operations are out of sequence, they are buffered
 * until the missing operations appear.
 *
 * NOTE: No lock is required because there should only be
 * one thread that does this!
 */
xtPublic void xt_xres_apply_in_order(XTThreadPtr self, XTWriterStatePtr ws, xtLogID log_id, xtLogOffset log_offset, XTXactLogBufferDPtr record)
{
	xtOpSeqNo		op_seq;
	xtTableID		tab_id;
	size_t			len;
	xtBool			check_index;

// XTDatabaseHPtr db, XTOpenTablePtr *ot, XTXactSeqReadPtr sr, XTDataBufferPtr databuf
	switch (record->xl.xl_status_1) {
		case XT_LOG_ENT_REC_MODIFIED:
		case XT_LOG_ENT_UPDATE:
		case XT_LOG_ENT_INSERT:
		case XT_LOG_ENT_DELETE:
		case XT_LOG_ENT_UPDATE_BG:
		case XT_LOG_ENT_INSERT_BG:
		case XT_LOG_ENT_DELETE_BG:
			len = offsetof(XTactUpdateEntryDRec, xu_rec_type_1) + (size_t) XT_GET_DISK_2(record->xu.xu_size_2);
			op_seq = XT_GET_DISK_4(record->xu.xu_op_seq_4);
			tab_id = XT_GET_DISK_4(record->xu.xu_tab_id_4);
			break;
		case XT_LOG_ENT_UPDATE_FL:
		case XT_LOG_ENT_INSERT_FL:
		case XT_LOG_ENT_DELETE_FL:
		case XT_LOG_ENT_UPDATE_FL_BG:
		case XT_LOG_ENT_INSERT_FL_BG:
		case XT_LOG_ENT_DELETE_FL_BG:
			len = offsetof(XTactUpdateFLEntryDRec, xf_rec_type_1) + (size_t) XT_GET_DISK_2(record->xf.xf_size_2);
			op_seq = XT_GET_DISK_4(record->xf.xf_op_seq_4);
			tab_id = XT_GET_DISK_4(record->xf.xf_tab_id_4);
			break;
		case XT_LOG_ENT_REC_FREED:
		case XT_LOG_ENT_REC_REMOVED:
		case XT_LOG_ENT_REC_REMOVED_EXT:
			/* [(7)] REMOVE is now a extended version of FREE! */
			len = offsetof(XTactFreeRecEntryDRec, fr_rec_type_1) + sizeof(XTTabRecFreeDRec);
			goto fixed_len_data;
		case XT_LOG_ENT_REC_REMOVED_BI:
			len = offsetof(XTactRemoveBIEntryDRec, rb_rec_type_1) + (size_t) XT_GET_DISK_2(record->rb.rb_size_2);
			op_seq = XT_GET_DISK_4(record->rb.rb_op_seq_4);
			tab_id = XT_GET_DISK_4(record->rb.rb_tab_id_4);
			break;
		case XT_LOG_ENT_REC_MOVED:
			len = offsetof(XTactWriteRecEntryDRec, xw_rec_type_1) + 8;
			goto fixed_len_data;
		case XT_LOG_ENT_REC_CLEANED:
			len = offsetof(XTactWriteRecEntryDRec, xw_rec_type_1) + offsetof(XTTabRecHeadDRec, tr_prev_rec_id_4) + XT_RECORD_ID_SIZE;
			goto fixed_len_data;
		case XT_LOG_ENT_REC_CLEANED_1:
			len = offsetof(XTactWriteRecEntryDRec, xw_rec_type_1) + 1;
			goto fixed_len_data;
		case XT_LOG_ENT_REC_UNLINKED:
			len = offsetof(XTactWriteRecEntryDRec, xw_rec_type_1) + offsetof(XTTabRecHeadDRec, tr_prev_rec_id_4) + XT_RECORD_ID_SIZE;
			fixed_len_data:
			op_seq = XT_GET_DISK_4(record->xw.xw_op_seq_4);
			tab_id = XT_GET_DISK_4(record->xw.xw_tab_id_4);
			break;
		case XT_LOG_ENT_ROW_NEW:
			len = sizeof(XTactRowAddedEntryDRec) - 4;
			goto new_row;
		case XT_LOG_ENT_ROW_NEW_FL:
			len = sizeof(XTactRowAddedEntryDRec);
			new_row:
			op_seq = XT_GET_DISK_4(record->xa.xa_op_seq_4);
			tab_id = XT_GET_DISK_4(record->xa.xa_tab_id_4);
			break;
		case XT_LOG_ENT_ROW_ADD_REC:
		case XT_LOG_ENT_ROW_SET:
		case XT_LOG_ENT_ROW_FREED:
			len = offsetof(XTactWriteRowEntryDRec, wr_ref_id_4) + sizeof(XTTabRowRefDRec);
			op_seq = XT_GET_DISK_4(record->wr.wr_op_seq_4);
			tab_id = XT_GET_DISK_4(record->wr.wr_tab_id_4);
			break;
		case XT_LOG_ENT_NO_OP:
		case XT_LOG_ENT_END_OF_LOG:
			return;
		default:
			return;
	}

	if (!xres_open_table(self, ws, tab_id))
		return;

	XTTableHPtr tab = ws->ws_ot->ot_table;

	/* NOTE:
	 *
	 * During normal operation this is actually given.
	 *
	 * During recovery, it only applies to the record/row files
	 * The index file is flushed indepently, and changes may
	 * have been applied to the index (due to a call to flush index,
	 * which comes as a result of out of memory) that have not been
	 * applied to the record/row files.
	 *
	 * As a result we need to do the index checks that apply to this
	 * change.
	 *
	 * At the moment, I will just do everything, which should not
	 * hurt!
	 *
	 * This error can be repeated by running the test
	 * runTest(OUT_OF_CACHE_UPDATE_TEST, 32, OUT_OF_CACHE_UPDATE_TEST_UPDATE_COUNT, OUT_OF_CACHE_UPDATE_TEST_SET_SIZE)
	 * and crashing after a while.
	 *
	 * Do this by setting not_this to NULL. This will cause the test to
	 * hang after a while. After a restart the indexes are corrupt if the
	 * ws->ws_in_recover condition is not present here. 
	 */
	if (ws->ws_in_recover) {
		if (!tab->tab_recovery_done) {
			/* op_seq <= tab_head_op_seq + 1: */
			ASSERT(XTTableSeq::xt_op_is_before(op_seq, tab->tab_head_op_seq+2));
			if (XTTableSeq::xt_op_is_before(op_seq-1, tab->tab_head_op_seq))
				/* Adjust the operation sequence number: */
				tab->tab_head_op_seq = op_seq-1;
			tab->tab_recovery_done = TRUE;
		}
	}

	if (!XTTableSeq::xt_op_is_before(tab->tab_head_op_seq, op_seq))
		return;

 	if (tab->tab_head_op_seq+1 == op_seq) {
		/* I could use tab_ind_rec_log_id, but this may be a problem, if
		 * recovery does not recover up to the last committed transaction.
		 */ 
		check_index = ws->ws_in_recover && xt_comp_log_pos(log_id, log_offset, ws->ws_ind_rec_log_id, ws->ws_ind_rec_log_offset) >= 0;
		xres_apply_change(self, ws->ws_ot, record, TRUE, check_index, &ws->ws_rec_buf);
		tab->tab_head_op_seq = op_seq;
		if (tab->tab_wr_wake_freeer) {
			if (!XTTableSeq::xt_op_is_before(tab->tab_head_op_seq, tab->tab_wake_freeer_op))
				xt_wr_wake_freeer(self);
		}

		/* Apply any operations in the list that now follow on...
		 * NOTE: the tab_op_list only has be locked for modification.
		 * This is because only one thread ever changes the list
		 * (on startup and the writer), but the checkpoint thread
		 * reads it.
		 */		
		XTOperationPtr	op;
		if ((op = (XTOperationPtr) xt_sl_first_item(tab->tab_op_list))) {
			if (tab->tab_head_op_seq+1 == op->or_op_seq) {
				xres_apply_operations(self, ws, TRUE);
			}
		}
	}
	else {
		/* Add the operation to the list: */
		XTOperationRec op;

		op.or_op_seq = op_seq;
		op.or_op_len = len;
		op.or_log_id = log_id;
		op.or_log_offset = log_offset;
		xt_sl_lock(self, tab->tab_op_list);
		xt_sl_insert(self, tab->tab_op_list, &op_seq, &op);
		ASSERT(tab->tab_op_list->sl_usage_count < 1000000);
		xt_sl_unlock(self, tab->tab_op_list);
	}
}

/* ----------------------------------------------------------------------
 * CHECKPOINTING FUNCTIONALITY
 */

static xtBool xres_delete_data_log(XTDatabaseHPtr db, xtLogID log_id)
{
	XTDataLogFilePtr	data_log;
	char				path[PATH_MAX];

	db->db_datalogs.dlc_name(PATH_MAX, path, log_id);

	if (!db->db_datalogs.dlc_remove_data_log(log_id, TRUE))
		return FAILED;

	if (xt_fs_exists(path)) {
#ifdef DEBUG_LOG_DELETE
		printf("-- delete log: %s\n", path);
#endif
		if (!xt_fs_delete(NULL, path))
			return FAILED;
	}
	/* The log was deleted: */
	if (!db->db_datalogs.dlc_get_data_log(&data_log, log_id, TRUE, NULL))
		return FAILED;
	if (data_log) {
		if (!db->db_datalogs.dls_set_log_state(data_log, XT_DL_DELETED))
			return FAILED;
	}
	return OK;
}

static int xres_comp_flush_tabs(XTThreadPtr XT_UNUSED(self), register const void *XT_UNUSED(thunk), register const void *a, register const void *b)
{
	xtTableID				tab_id = *((xtTableID *) a);
	XTCheckPointTablePtr	cp_tab = (XTCheckPointTablePtr) b;

	if (tab_id < cp_tab->cpt_tab_id)
		return -1;
	if (tab_id > cp_tab->cpt_tab_id)
		return 1;
	return 0;
}

static void xres_init_checkpoint_state(XTThreadPtr self, XTCheckPointStatePtr cp)
{
	xt_init_mutex_with_autoname(self, &cp->cp_state_lock);
}

static void xres_free_checkpoint_state(XTThreadPtr self, XTCheckPointStatePtr cp)
{
	xt_free_mutex(&cp->cp_state_lock);
	if (cp->cp_table_ids) {
		xt_free_sortedlist(self, cp->cp_table_ids);
		cp->cp_table_ids = NULL;
	}
}

/*
 * Remove the deleted logs so that they can be re-used.
 * This is only possible after a checkpoint has been
 * written that does _not_ include these logs as logs
 * to be deleted!
 */
static xtBool xres_remove_data_logs(XTDatabaseHPtr db)
{
	u_int		no_of_logs = xt_sl_get_size(db->db_datalogs.dlc_deleted);
	xtLogID		*log_id_ptr;

	for (u_int i=0; i<no_of_logs; i++) {
		log_id_ptr = (xtLogID *) xt_sl_item_at(db->db_datalogs.dlc_deleted, i);
		if (!db->db_datalogs.dlc_remove_data_log(*log_id_ptr, FALSE))
			return FAILED;
	}
	xt_sl_set_size(db->db_datalogs.dlc_deleted, 0);
	return OK;
}

/* ----------------------------------------------------------------------
 * INIT & EXIT
 */

xtPublic void xt_xres_init(XTThreadPtr self, XTDatabaseHPtr db)
{
	xtLogID	max_log_id;

	xt_init_mutex_with_autoname(self, &db->db_cp_lock);
	xt_init_cond(self, &db->db_cp_cond);
	
	xres_init_checkpoint_state(self, &db->db_cp_state);
	db->db_restart.xres_init(self, db, &db->db_wr_log_id, &db->db_wr_log_offset, &max_log_id);

	/* It is also the position where transactions will start writing the
	 * log:
	 */
	if (!db->db_xlog.xlog_set_write_offset(db->db_wr_log_id, db->db_wr_log_offset, max_log_id, self))
		xt_throw(self);
}

xtPublic void xt_xres_exit(XTThreadPtr self, XTDatabaseHPtr db)
{
	db->db_restart.xres_exit(self);
	xres_free_checkpoint_state(self, &db->db_cp_state);
	xt_free_mutex(&db->db_cp_lock);
	xt_free_cond(&db->db_cp_cond);
}

/* ----------------------------------------------------------------------
 * RESTART FUNCTIONALITY
 */

/*
 * Restart the database. This function loads the restart position, and
 * applies all changes in the logs, until the end of the log, or
 * a corrupted record is found.
 *
 * The restart position is the position in the log where we know that
 * all the changes up to that point have been flushed to the
 * database.
 *
 * This is called the checkpoint position. The checkpoint position
 * is written alternatively to 2 restart files.
 *
 * To make a checkpoint:
 * Get the current log writer log offset.
 * For each table:
 *    Get the log offset of the next operation on the table, if an
 *    operation is queued for the table.
 *    Flush that table, and the operation sequence to the table.
 * For each unclean transaction:
 *    Get the log offset of the begin of the transaction.
 * Write the lowest of all log offsets to the restart file!
 */

void XTXactRestart::xres_init(XTThreadPtr self, XTDatabaseHPtr db, xtLogID *log_id, xtLogOffset *log_offset, xtLogID *max_log_id)
{
	char					path[PATH_MAX];
	XTOpenFilePtr			of = NULL;
	XTXlogCheckpointDPtr	res_1_buffer = NULL;
	XTXlogCheckpointDPtr	res_2_buffer = NULL;
	XTXlogCheckpointDPtr	use_buffer;
	xtLogID					ind_rec_log_id = 0;
	xtLogOffset				ind_rec_log_offset = 0;

	enter_();
	xres_db = db;

	ASSERT(!self->st_database);
	/* The following call stack:
	 * XTDatabaseLog::xlog_flush_pending()
	 * XTDatabaseLog::xlog_flush()
	 * xt_xlog_flush_log()
	 * xt_flush_indices()
	 * idx_out_of_memory_failure()
	 * xt_idx_delete()
	 * xres_remove_index_entries()
	 * xres_apply_change()
	 * xt_xres_apply_in_order()
	 * XTXactRestart::xres_restart()
	 * XTXactRestart::xres_init()
	 * Leads to st_database being used!
	 */
	self->st_database = db;

#ifdef SKIP_STARTUP_CHECKPOINT
	/* When debugging, we do not checkpoint immediately, just in case
	 * we detect a problem during recovery.
	 */
	xres_cp_required = FALSE;
#else
	xres_cp_required = TRUE;
#endif
	xres_cp_number = 0;
	try_(a) {

		/* Figure out which restart file to use.
		 */
		xres_name(PATH_MAX, path, 1);
		if ((of = xt_open_file(self, path, XT_FS_MISSING_OK))) {
			size_t res_1_size;

			res_1_size = (size_t) xt_seek_eof_file(self, of);
			res_1_buffer = (XTXlogCheckpointDPtr) xt_malloc(self, res_1_size);
			if (!xt_pread_file(of, 0, res_1_size, res_1_size, res_1_buffer, NULL, &self->st_statistics.st_x, self))
				xt_throw(self);
			xt_close_file(self, of);
			of = NULL;
			if (!xres_check_checksum(res_1_buffer, res_1_size)) {
				xt_free(self, res_1_buffer);
				res_1_buffer = NULL;
			}
		}

		xres_name(PATH_MAX, path, 2);
		if ((of = xt_open_file(self, path, XT_FS_MISSING_OK))) {
			size_t res_2_size;

			res_2_size = (size_t) xt_seek_eof_file(self, of);
			res_2_buffer = (XTXlogCheckpointDPtr) xt_malloc(self, res_2_size);
			if (!xt_pread_file(of, 0, res_2_size, res_2_size, res_2_buffer, NULL, &self->st_statistics.st_x, self))
				xt_throw(self);
			xt_close_file(self, of);
			of = NULL;
			if (!xres_check_checksum(res_2_buffer, res_2_size)) {
				xt_free(self, res_2_buffer);
				res_2_buffer = NULL;
			}
		}

		if (res_1_buffer && res_2_buffer) {
			if (xt_comp_log_pos(
				XT_GET_DISK_4(res_1_buffer->xcp_log_id_4),
				XT_GET_DISK_6(res_1_buffer->xcp_log_offs_6),
				XT_GET_DISK_4(res_2_buffer->xcp_log_id_4),
				XT_GET_DISK_6(res_2_buffer->xcp_log_offs_6)) > 0) {
				/* The first log is the further along than the second: */
				xt_free(self, res_2_buffer);
				res_2_buffer = NULL;
			}
			else {
				if (XT_GET_DISK_6(res_1_buffer->xcp_chkpnt_no_6) >
					XT_GET_DISK_6(res_2_buffer->xcp_chkpnt_no_6)) {
					xt_free(self, res_2_buffer);
					res_2_buffer = NULL;
				}
				else {
					xt_free(self, res_1_buffer);
					res_1_buffer = NULL;
				}
			}
		}

		if (res_1_buffer) {
			use_buffer = res_1_buffer;
			xres_next_res_no = 2;
		}
		else {
			use_buffer = res_2_buffer;
			xres_next_res_no = 1;
		}

		/* Read the checkpoint data: */
		if (use_buffer) {
			u_int		no_of_logs;
			xtLogID		xt_log_id;
			xtTableID	xt_tab_id;

			xres_cp_number = XT_GET_DISK_6(use_buffer->xcp_chkpnt_no_6);
			xres_cp_log_id = XT_GET_DISK_4(use_buffer->xcp_log_id_4);
			xres_cp_log_offset = XT_GET_DISK_6(use_buffer->xcp_log_offs_6);
			xt_tab_id = XT_GET_DISK_4(use_buffer->xcp_tab_id_4);
			if (xt_tab_id > db->db_curr_tab_id)
				db->db_curr_tab_id = xt_tab_id;
			db->db_xn_curr_id = XT_GET_DISK_4(use_buffer->xcp_xact_id_4);
			ind_rec_log_id = XT_GET_DISK_4(use_buffer->xcp_ind_rec_log_id_4);
			ind_rec_log_offset = XT_GET_DISK_6(use_buffer->xcp_ind_rec_log_offs_6);
			no_of_logs = XT_GET_DISK_2(use_buffer->xcp_log_count_2);

#ifdef DEBUG_PRINT
			printf("CHECKPOINT log=%d offset=%d ", (int) xres_cp_log_id, (int) xres_cp_log_offset);
			if (no_of_logs)
				printf("DELETED LOGS: ");
#endif

			/* Logs that are deleted are locked until _after_ the next
			 * checkpoint.
			 *
			 * To prevent the following problem from occuring:
			 * - Recovery is performed, and log X is deleted 
			 * - After delete a log is free for re-use.
			 *   New data is writen to log X.
			 * - Server crashes.
			 * - Recovery is performed from previous checkpoint,
			 *   and log X is deleted again.
			 *
			 * To lock the logs the are placed on the deleted list.
			 * After the next checkpoint, all logs on this list
			 * will be removed.
			 */
			for (u_int i=0; i<no_of_logs; i++) {
				xt_log_id = (xtLogID) XT_GET_DISK_2(use_buffer->xcp_del_log[i]);
#ifdef DEBUG_PRINT
				if (i != 0)
					printf(", ");
				printf("%d", (int) xt_log_id);
#endif
#ifdef DEBUG_KEEP_LOGS
				xt_dl_set_to_delete(self, db, xt_log_id);
#else
				if (!xres_delete_data_log(db, xt_log_id))
					xt_throw(self);
#endif
			}

#ifdef DEBUG_PRINT
			printf("\n");
#endif
		}
		else {
			/* Try to determine the correct start point. */
			xres_cp_number = 0;
			xres_cp_log_id = xt_xlog_get_min_log(self, db);
			xres_cp_log_offset = 0;
			ind_rec_log_id = xres_cp_log_id;
			ind_rec_log_offset = xres_cp_log_offset;

#ifdef DEBUG_PRINT
			printf("CHECKPOINT log=1 offset=0\n");
#endif
		}

		if (res_1_buffer) {
			xt_free(self, res_1_buffer);
			res_1_buffer = NULL;
		}
		if (res_2_buffer) {
			xt_free(self, res_2_buffer);
			res_2_buffer = NULL;
		}

		if (!xres_restart(self, log_id, log_offset, ind_rec_log_id, ind_rec_log_offset, max_log_id))
			xt_throw(self);
	}
	catch_(a) {
		self->st_database = NULL;
		if (of)
			xt_close_file(self, of);
		if (res_1_buffer)
			xt_free(self, res_1_buffer);
		if (res_2_buffer)
			xt_free(self, res_2_buffer);
		xres_exit(self);
		throw_();
	}
	cont_(a);
	self->st_database = NULL;

	exit_();
}

void XTXactRestart::xres_exit(XTThreadPtr XT_UNUSED(self))
{
}

void XTXactRestart::xres_name(size_t size, char *path, xtLogID log_id)
{
	char name[50];

	sprintf(name, "restart-%lu.xt", (u_long) log_id);
	xt_strcpy(size, path, xres_db->db_main_path);
	xt_add_system_dir(size, path);
	xt_add_dir_char(size, path);
	xt_strcat(size, path, name);
}

xtBool XTXactRestart::xres_check_checksum(XTXlogCheckpointDPtr buffer, size_t size)
{
	size_t		head_size;

	/* The minimum size: */
	if (size < offsetof(XTXlogCheckpointDRec, xcp_head_size_4) + 4)
		return FAILED;

	/* Check the sizes: */
	head_size = XT_GET_DISK_4(buffer->xcp_head_size_4);
	if (size < head_size)
		return FAILED;

	if (XT_GET_DISK_2(buffer->xcp_checksum_2) != xt_get_checksum(((xtWord1 *) buffer) + 2, size - 2, 1))
		return FAILED;

	if (XT_GET_DISK_2(buffer->xcp_version_2) != XT_CHECKPOINT_VERSION)
		return FAILED;

	return OK;
}

void XTXactRestart::xres_recover_progress(XTThreadPtr self, XTOpenFilePtr *of, int perc)
{
#ifdef XT_USE_GLOBAL_DB
	if (!perc) {
		char file_path[PATH_MAX];

		xt_strcpy(PATH_MAX, file_path, xres_db->db_main_path);
		xt_add_pbxt_file(PATH_MAX, file_path, "recovery-progress");
		*of = xt_open_file(self, file_path, XT_FS_CREATE | XT_FS_MAKE_PATH);
		xt_set_eof_file(self, *of, 0);
	}

	if (perc > 100) {
		char file_path[PATH_MAX];

		if (*of) {
			xt_close_file(self, *of);
			*of = NULL;
		}
		xt_strcpy(PATH_MAX, file_path, xres_db->db_main_path);
		xt_add_pbxt_file(PATH_MAX, file_path, "recovery-progress");
		if (xt_fs_exists(file_path))
			xt_fs_delete(self, file_path);
	}
	else {
		char number[40];

		sprintf(number, "%d", perc);
		if (!xt_pwrite_file(*of, 0, strlen(number), number, &self->st_statistics.st_x, self))
			xt_throw(self);
		if (!xt_flush_file(*of, &self->st_statistics.st_x, self))
			xt_throw(self);
	}
#endif
}

xtBool XTXactRestart::xres_restart(XTThreadPtr self, xtLogID *log_id, xtLogOffset *log_offset, xtLogID ind_rec_log_id, xtLogOffset ind_rec_log_offset, xtLogID *max_log_id)
{
	xtBool					ok = TRUE;
	XTDatabaseHPtr			db = xres_db;
	XTXactLogBufferDPtr		record;
	xtXactID				xn_id;
	XTXactDataPtr			xact;
	xtTableID				tab_id;
	XTWriterStateRec		ws;
	off_t					bytes_read = 0;
	off_t					bytes_to_read;
	volatile xtBool			print_progress = FALSE;
	volatile off_t			perc_size = 0, next_goal = 0;
	int						perc_complete = 1;
	XTOpenFilePtr			progress_file = NULL;
	xtBool					min_ram_xn_id_set = FALSE;
	u_int					log_count;

	memset(&ws, 0, sizeof(ws));

	ws.ws_db = db;
	ws.ws_in_recover = TRUE;
	ws.ws_ind_rec_log_id = ind_rec_log_id;
	ws.ws_ind_rec_log_offset = ind_rec_log_offset;

	/* Initialize the data log buffer (required if extended data is
	 * referenced).
	 * Note: this buffer is freed later. It is part of the thread
	 * "open database" state, and this means that a thread
	 * may not have another database open (in use) when
	 * it calls this functions.
	 */
	self->st_dlog_buf.dlb_init(db, xt_db_log_buffer_size);

	if (!db->db_xlog.xlog_seq_init(&ws.ws_seqread, xt_db_log_buffer_size, TRUE))
		return FAILED;

	bytes_to_read = xres_bytes_to_read(self, db, &log_count, max_log_id);
	/* Don't print anything about recovering an empty database: */
	if (bytes_to_read != 0)
		xt_logf(XT_NT_INFO, "PBXT: Recovering from %lu-%llu, bytes to read: %llu\n", (u_long) xres_cp_log_id, (u_llong) xres_cp_log_offset, (u_llong) bytes_to_read);
	if (bytes_to_read >= 10*1024*1024) {
		print_progress = TRUE;
		perc_size = bytes_to_read / 100;
		next_goal = perc_size;
		xres_recover_progress(self, &progress_file, 0);
	}

	if (!db->db_xlog.xlog_seq_start(&ws.ws_seqread, xres_cp_log_id, xres_cp_log_offset, FALSE)) {
		ok = FALSE;
		goto failed;
	}

	try_(a) {
		for (;;) {
			if (!db->db_xlog.xlog_seq_next(&ws.ws_seqread, &record, TRUE, self)) {
				ok = FALSE;
				break;
			}
			/* Increment before. If record is NULL then xseq_record_len will be zero,
			 * UNLESS the last record was of type XT_LOG_ENT_END_OF_LOG 
			 * which fills the log to align to block of size 512.
			 */
			bytes_read += ws.ws_seqread.xseq_record_len;
			if (!record)
				break;
#ifdef PRINT_LOG_ON_RECOVERY
			xt_print_log_record(ws.ws_seqread.xseq_rec_log_id, ws.ws_seqread.xseq_rec_log_offset, record);
#endif
			if (print_progress && bytes_read > next_goal) {
				if (((perc_complete - 1) % 25) == 0)
					xt_logf(XT_NT_INFO, "PBXT: ");
				if ((perc_complete % 25) == 0)
					xt_logf(XT_NT_INFO, "%2d\n", (int) perc_complete);
				else
					xt_logf(XT_NT_INFO, "%2d ", (int) perc_complete);
				xt_log_flush(self);
				xres_recover_progress(self, &progress_file, perc_complete);
				next_goal += perc_size;
				perc_complete++;
			}
			switch (record->xl.xl_status_1) {
				case XT_LOG_ENT_HEADER:
					break;
				case XT_LOG_ENT_NEW_LOG: {
					/* Adjust the bytes read for the fact that logs are written
					 * on 512 byte boundaries.
					 */
					off_t offs, eof = ws.ws_seqread.xseq_log_eof;

					offs = ws.ws_seqread.xseq_rec_log_offset + ws.ws_seqread.xseq_record_len;
					if (eof > offs)
						bytes_read += eof - offs;
					if (!db->db_xlog.xlog_seq_start(&ws.ws_seqread, XT_GET_DISK_4(record->xl.xl_log_id_4), 0, TRUE))
						xt_throw(self);
					break;
				}
				case XT_LOG_ENT_NEW_TAB:
					tab_id = XT_GET_DISK_4(record->xt.xt_tab_id_4);
					if (tab_id > db->db_curr_tab_id)
						db->db_curr_tab_id = tab_id;
					break;
				case XT_LOG_ENT_UPDATE_BG:
				case XT_LOG_ENT_INSERT_BG:
				case XT_LOG_ENT_DELETE_BG:
					xn_id = XT_GET_DISK_4(record->xu.xu_xact_id_4);
					goto start_xact;
				case XT_LOG_ENT_UPDATE_FL_BG:
				case XT_LOG_ENT_INSERT_FL_BG:
				case XT_LOG_ENT_DELETE_FL_BG:
					xn_id = XT_GET_DISK_4(record->xf.xf_xact_id_4);
					start_xact:
					if (xt_xn_is_before(db->db_xn_curr_id, xn_id))
						db->db_xn_curr_id = xn_id;

					if (!(xact = xt_xn_add_old_xact(db, xn_id, self)))
						xt_throw(self);

					xact->xd_begin_log = ws.ws_seqread.xseq_rec_log_id;
					xact->xd_begin_offset = ws.ws_seqread.xseq_rec_log_offset;

					xact->xd_end_xn_id = xn_id;
					xact->xd_end_time = db->db_xn_end_time;
					xact->xd_flags = (XT_XN_XAC_LOGGED | XT_XN_XAC_ENDED | XT_XN_XAC_RECOVERED | XT_XN_XAC_SWEEP);

					/* This may affect the "minimum RAM transaction": */
					if (!min_ram_xn_id_set || xt_xn_is_before(xn_id, db->db_xn_min_ram_id)) {
						min_ram_xn_id_set = TRUE;
						db->db_xn_min_ram_id = xn_id;
					}
					xt_xres_apply_in_order(self, &ws, ws.ws_seqread.xseq_rec_log_id, ws.ws_seqread.xseq_rec_log_offset, record);
					break;
				case XT_LOG_ENT_COMMIT:
				case XT_LOG_ENT_ABORT:
					xn_id = XT_GET_DISK_4(record->xe.xe_xact_id_4);
					if ((xact = xt_xn_get_xact(db, xn_id, self))) {
						xact->xd_end_xn_id = xn_id;
						xact->xd_flags |= XT_XN_XAC_ENDED | XT_XN_XAC_SWEEP;
						xact->xd_flags &= ~XT_XN_XAC_RECOVERED; // We can expect an end record on cleanup!
						if (record->xl.xl_status_1 == XT_LOG_ENT_COMMIT)
							xact->xd_flags |= XT_XN_XAC_COMMITTED;
					}
					break;
				case XT_LOG_ENT_CLEANUP:
					/* The transaction was cleaned up: */
					xn_id = XT_GET_DISK_4(record->xc.xc_xact_id_4);
					xt_xn_delete_xact(db, xn_id, self);
					break;
				case XT_LOG_ENT_OP_SYNC:
					xres_sync_operations(self, db, &ws);
					break;
				case XT_LOG_ENT_DEL_LOG:
					xtLogID rec_log_id;

					rec_log_id = XT_GET_DISK_4(record->xl.xl_log_id_4);
					xt_dl_set_to_delete(self, db, rec_log_id);
					break;
				default:
					xt_xres_apply_in_order(self, &ws, ws.ws_seqread.xseq_rec_log_id, ws.ws_seqread.xseq_rec_log_offset, record);
					break;
			}
		}

		if (xres_sync_operations(self, db, &ws)) {
			XTactOpSyncEntryDRec	op_sync;
			time_t					now = time(NULL);

			op_sync.os_status_1 = XT_LOG_ENT_OP_SYNC;
			op_sync.os_checksum_1 = XT_CHECKSUM_1(now) ^ XT_CHECKSUM_1(ws.ws_seqread.xseq_rec_log_id);
			XT_SET_DISK_4(op_sync.os_time_4, (xtWord4) now);
			/* TODO: If this is done, check to see that
			 * the byte written here are read back by the writter.
			 * This is in order to be in sync with 'xl_log_bytes_written'.
			 * i.e. xl_log_bytes_written == xl_log_bytes_read
			 */
			if (!db->db_xlog.xlog_write_thru(&ws.ws_seqread, sizeof(XTactOpSyncEntryDRec), (xtWord1 *) &op_sync, self))
				xt_throw(self);
		}
	}
	catch_(a) {
		ok = FALSE;
	}
	cont_(a);

	if (ok) {
		if (print_progress) {
			while (perc_complete <= 100) {
				if (((perc_complete - 1) % 25) == 0)
					xt_logf(XT_NT_INFO, "PBXT: ");
				if ((perc_complete % 25) == 0)
					xt_logf(XT_NT_INFO, "%2d\n", (int) perc_complete);
				else
					xt_logf(XT_NT_INFO, "%2d ", (int) perc_complete);
				xt_log_flush(self);
				xres_recover_progress(self, &progress_file, perc_complete);
				perc_complete++;
			}
		}
		if (bytes_to_read != 0)
			xt_logf(XT_NT_INFO, "PBXT: Recovering complete at %lu-%llu, bytes read: %llu\n", (u_long) ws.ws_seqread.xseq_rec_log_id, (u_llong) ws.ws_seqread.xseq_rec_log_offset, (u_llong) bytes_read);

		*log_id = ws.ws_seqread.xseq_rec_log_id;
		*log_offset = ws.ws_seqread.xseq_rec_log_offset;

		if (!min_ram_xn_id_set)
			/* This is true because if no transaction was placed in RAM then
			 * the next transaction in RAM will have the next ID: */
			db->db_xn_min_ram_id = db->db_xn_curr_id + 1;
	}

	failed:
	xt_free_writer_state(self, &ws);
	self->st_dlog_buf.dlb_exit(self);
	xres_recover_progress(self, &progress_file, 101);
	return ok;
}

xtBool XTXactRestart::xres_is_checkpoint_pending(xtLogID curr_log_id, xtLogOffset curr_log_offset)
{
	return xt_bytes_since_last_checkpoint(xres_db, curr_log_id, curr_log_offset) >= xt_db_checkpoint_frequency / 2;
}

/*
 * Calculate the bytes to be read for recovery.
 * This is only an estimate of the number of bytes that
 * will be read.
 */
off_t XTXactRestart::xres_bytes_to_read(XTThreadPtr self, XTDatabaseHPtr db, u_int *log_count, xtLogID *max_log_id)
{
	off_t				to_read = 0, eof;
	xtLogID				log_id = xres_cp_log_id;
	char				log_path[PATH_MAX];
	XTOpenFilePtr		of;
	XTXactLogHeaderDRec	log_head;
	size_t				head_size;
	size_t				red_size;

	*max_log_id = log_id;
	*log_count = 0;
	for (;;) {
		db->db_xlog.xlog_name(PATH_MAX, log_path, log_id);
		of = NULL;
		if (!xt_open_file_ns(&of, log_path, XT_FS_MISSING_OK))
			xt_throw(self);
		if (!of)
			break;
		pushr_(xt_close_file, of);

		/* Check the first record of the log, to see if it is valid. */
		if (!xt_pread_file(of, 0, sizeof(XTXactLogHeaderDRec), 0, (xtWord1 *) &log_head, &red_size, &self->st_statistics.st_xlog, self))
			xt_throw(self);
		/* The minimum size (old log size): */
		if (red_size < XT_MIN_LOG_HEAD_SIZE)
			goto done;
		head_size = XT_GET_DISK_4(log_head.xh_size_4);
		if (log_head.xh_status_1 != XT_LOG_ENT_HEADER)
			goto done;
		if (log_head.xh_checksum_1 != XT_CHECKSUM_1(log_id))
			goto done;
		if (XT_LOG_HEAD_MAGIC(&log_head, head_size) != XT_LOG_FILE_MAGIC)
			goto done;
		if (head_size > offsetof(XTXactLogHeaderDRec, xh_log_id_4) + 4) {
			if (XT_GET_DISK_4(log_head.xh_log_id_4) != log_id)
				goto done;
		}
		if (head_size > offsetof(XTXactLogHeaderDRec, xh_version_2) + 4) {
			if (XT_GET_DISK_2(log_head.xh_version_2) > XT_LOG_VERSION_NO) 				
				xt_throw_ulxterr(XT_CONTEXT, XT_ERR_NEW_TYPE_OF_XLOG, (u_long) log_id);
		}

		eof = xt_seek_eof_file(self, of);
		freer_(); // xt_close_file(of)
		if (log_id == xres_cp_log_id)
			to_read += (eof - xres_cp_log_offset);
		else
			to_read += eof;
		(*log_count)++;
		*max_log_id = log_id;
		log_id++;
	}
	return to_read;

	done:
	freer_(); // xt_close_file(of)
	return to_read;
}


/* ----------------------------------------------------------------------
 * C H E C K P O I N T    P R O C E S S
 */

typedef enum XTFileType {
	XT_FT_RECROW_FILE,
	XT_FT_INDEX_FILE
} XTFileType;

typedef struct XTDirtyFile {
	xtTableID				df_tab_id;
	XTFileType				df_file_type;
} XTDirtyFileRec, *XTDirtyFilePtr;

#define XT_MAX_FLUSH_FILES			200
#define XT_FLUSH_THRESHOLD			(2 * 1024 * 1024)

/* Sort files to be flused. */
#ifdef USE_LATER
static void xres_cp_flush_files(XTThreadPtr self, XTDatabaseHPtr db)
{
	u_int			edx;
	XTTableEntryPtr	te;
	XTDirtyFileRec	flush_list[XT_MAX_FLUSH_FILES];
	u_int			file_count = 0;
	XTIndexPtr		*iptr;
	u_int			dirty_blocks;
	XTOpenTablePtr	ot;
	XTTableHPtr		tab;

	retry:
	xt_enum_tables_init(&edx);
	xt_ht_lock(self, db->db_tables);
	pushr_(xt_ht_unlock, db->db_tables);
	while (file_count < XT_MAX_FLUSH_FILES &&
		(te = xt_enum_tables_next(self, db, &edx))) {
		if ((tab = te->te_table)) {
			if (tab->tab_bytes_to_flush >= XT_FLUSH_THRESHOLD) {
				flush_list[file_count].df_tab_id = te->te_tab_id;
				flush_list[file_count].df_file_type = XT_FT_RECROW_FILE;
				file_count++;
			}
			if (file_count == XT_MAX_FLUSH_FILES)
				break;
			iptr = tab->tab_dic.dic_keys;
			dirty_blocks = 0;
			for (u_int i=0;i<tab->tab_dic.dic_key_count; i++) {
				dirty_blocks += (*iptr)->mi_dirty_blocks;
				iptr++;
			}
			if ((dirty_blocks * XT_INDEX_PAGE_SIZE) >= XT_FLUSH_THRESHOLD) {
				flush_list[file_count].df_tab_id = te->te_tab_id;
				flush_list[file_count].df_file_type = XT_FT_INDEX_FILE;
				file_count++;
			}
		}
	}
	freer_(); // xt_ht_unlock(db->db_tables)

	for (u_int i=0;i<file_count && !self->t_quit; i++) {
		/* We want to flush about once a second: */ 
		xt_sleep_milli_second(400);
		if ((ot = xt_db_open_pool_table(self, db, flush_list[i].df_tab_id, NULL, TRUE))) {
			pushr_(xt_db_return_table_to_pool, ot);

			if (flush_list[i].df_file_type == XT_FT_RECROW_FILE) {
				if (!xt_flush_record_row(ot, NULL))
					xt_throw(self);
			}
			else {
				if (!xt_flush_indices(ot, NULL))
					xt_throw(self);
			}

			freer_(); // xt_db_return_table_to_pool(ot)
		}
	}
	
	if (file_count == 100)
		goto retry;
}
#endif

#ifdef xxx
void XTXactRestart::xres_checkpoint_pending(xtLogID log_id, xtLogOffset log_offset)
{
#ifdef TRACE_CHECKPOINT_ACTIVITY
	xtBool tmp = xres_cp_pending;
#endif
	xres_cp_pending = xres_is_checkpoint_pending(log_id, log_offset);
#ifdef TRACE_CHECKPOINT_ACTIVITY
	if (tmp) {
		if (!xres_cp_pending)
			printf("%s xres_cp_pending = FALSE\n", xt_get_self()->t_name);
	}
	else {
		if (xres_cp_pending)
			printf("%s xres_cp_pending = TRUE\n", xt_get_self()->t_name);
	}
#endif
}


	xres_checkpoint_pending();

	if (!xres_cp_required &&
		!xres_cp_pending &&
		xt_sl_get_size(db->db_datalogs.dlc_to_delete) == 0 &&
		xt_sl_get_size(db->db_datalogs.dlc_deleted) == 0)
		return FALSE;
#endif

#ifdef NEVER_CHECKPOINT
xtBool no_checkpoint = TRUE;
#endif

#define XT_CHECKPOINT_IF_NO_ACTIVITY		0
#define XT_CHECKPOINT_PAUSE_IF_ACTIVITY		1
#define XT_CHECKPOINT_NO_PAUSE				2

/*
 * This function performs table flush, as long as the system is idle.
 */
static xtBool xres_cp_checkpoint(XTThreadPtr self, XTDatabaseHPtr db, u_int curr_writer_total, xtBool force_checkpoint)
{
	XTCheckPointStatePtr	cp = &db->db_cp_state;
	XTOpenTablePtr			ot;
	XTCheckPointTablePtr	to_flush_ptr;
	XTCheckPointTableRec	to_flush;
	u_int					table_count = 0;
	xtBool					checkpoint_done;
	off_t					bytes_flushed = 0;
	int						check_type;

#ifdef NEVER_CHECKPOINT
	if (no_checkpoint)
		return FALSE;
#endif
	if (force_checkpoint) {
		if (db->db_restart.xres_cp_required)
			check_type = XT_CHECKPOINT_NO_PAUSE;
		else
			check_type = XT_CHECKPOINT_PAUSE_IF_ACTIVITY;
	}
	else
		check_type = XT_CHECKPOINT_IF_NO_ACTIVITY;	

	to_flush.cpt_tab_id = 0;
	to_flush.cpt_flushed = 0;

	/* Start a checkpoint: */
	if (!xt_begin_checkpoint(db, FALSE, self))
		xt_throw(self);

	while (!self->t_quit) {
		xt_lock_mutex_ns(&cp->cp_state_lock);
		table_count = 0;
		if (cp->cp_table_ids)
			table_count = xt_sl_get_size(cp->cp_table_ids);
		if (!cp->cp_running || cp->cp_flush_count >= table_count) {
			xt_unlock_mutex_ns(&cp->cp_state_lock);
			break;
		}
		if (cp->cp_next_to_flush > table_count)
			cp->cp_next_to_flush = 0;

		to_flush_ptr = (XTCheckPointTablePtr) xt_sl_item_at(cp->cp_table_ids, cp->cp_next_to_flush);
		if (to_flush_ptr)
			to_flush = *to_flush_ptr;
		xt_unlock_mutex_ns(&cp->cp_state_lock);

		if (to_flush_ptr) {
			if ((ot = xt_db_open_pool_table(self, db, to_flush.cpt_tab_id, NULL, TRUE))) {
				pushr_(xt_db_return_table_to_pool, ot);

				if (!(to_flush.cpt_flushed & XT_CPT_REC_ROW_FLUSHED)) {
					if (!xt_flush_record_row(ot, &bytes_flushed, FALSE))
						xt_throw(self);
				}

				xt_lock_mutex_ns(&cp->cp_state_lock);
				to_flush_ptr = NULL;
				if (cp->cp_running)
					to_flush_ptr = (XTCheckPointTablePtr) xt_sl_item_at(cp->cp_table_ids, cp->cp_next_to_flush);
				if (to_flush_ptr)
					to_flush = *to_flush_ptr;
				xt_unlock_mutex_ns(&cp->cp_state_lock);

				if (to_flush_ptr && !self->t_quit) {
					if (!(to_flush.cpt_flushed & XT_CPT_INDEX_FLUSHED)) {
						switch (check_type) {
							case XT_CHECKPOINT_IF_NO_ACTIVITY:
								if (bytes_flushed > 0 && curr_writer_total != db->db_xn_total_writer_count) {
									freer_(); // xt_db_return_table_to_pool(ot)
									goto end_checkpoint;
								}
								break;
							case XT_CHECKPOINT_PAUSE_IF_ACTIVITY:
								if (bytes_flushed > 2 * 1024 * 1024 && curr_writer_total != db->db_xn_total_writer_count) {
									curr_writer_total = db->db_xn_total_writer_count;
									bytes_flushed = 0;
									xt_sleep_milli_second(400);
								}
								break;
							case XT_CHECKPOINT_NO_PAUSE:
								break;
						}

						if (!self->t_quit) {
							if (!xt_flush_indices(ot, &bytes_flushed, FALSE))
								xt_throw(self);
							to_flush.cpt_flushed |= XT_CPT_INDEX_FLUSHED;
						}
					}
				}

				freer_(); // xt_db_return_table_to_pool(ot)
			}
			
			if ((to_flush.cpt_flushed & XT_CPT_ALL_FLUSHED) == XT_CPT_ALL_FLUSHED)
				cp->cp_next_to_flush++;
		}
		else
			cp->cp_next_to_flush++;

		if (self->t_quit)
			break;

		switch (check_type) {
			case XT_CHECKPOINT_IF_NO_ACTIVITY:
				if (bytes_flushed > 0 && curr_writer_total != db->db_xn_total_writer_count)
					goto end_checkpoint;
				break;
			case XT_CHECKPOINT_PAUSE_IF_ACTIVITY:
				if (bytes_flushed > 2 * 1024 * 1024 && curr_writer_total != db->db_xn_total_writer_count) {
					curr_writer_total = db->db_xn_total_writer_count;
					bytes_flushed = 0;
					xt_sleep_milli_second(400);
				}
				break;
			case XT_CHECKPOINT_NO_PAUSE:
				break;
		}
	}

	end_checkpoint:
	if (!xt_end_checkpoint(db, self, &checkpoint_done))
		xt_throw(self);
	return checkpoint_done;
}


/* Wait for the log writer to tell us to do something.
 */
static void xres_cp_wait_for_log_writer(XTThreadPtr self, XTDatabaseHPtr db, u_long milli_secs)
{
	xt_lock_mutex(self, &db->db_cp_lock);
	pushr_(xt_unlock_mutex, &db->db_cp_lock);
	if (!self->t_quit)
		xt_timed_wait_cond(self, &db->db_cp_cond, &db->db_cp_lock, milli_secs);
	freer_(); // xt_unlock_mutex(&db->db_cp_lock)
}

/*
 * This is the way checkpoint works:
 *
 * To write a checkpoint we need to flush all tables in
 * the database.
 *
 * Before flushing the first table we get the checkpoint
 * log position.
 *
 * After flushing all files we write of the checkpoint
 * log position.
 */
static void xres_cp_main(XTThreadPtr self)
{
	XTDatabaseHPtr		db = self->st_database;
	u_int				curr_writer_total;
	time_t				now;

	xt_set_low_priority(self);


	while (!self->t_quit) {
		/* Wait 2 seconds: */
		curr_writer_total = db->db_xn_total_writer_count;
		xt_db_approximate_time = time(NULL);
		now = xt_db_approximate_time;
		while (!self->t_quit && xt_db_approximate_time < now + 2 && !db->db_restart.xres_cp_required) {
			xres_cp_wait_for_log_writer(self, db, 400);
			xt_db_approximate_time = time(NULL);
			xt_db_free_unused_open_tables(self, db);
		}
		
		if (self->t_quit)
			break;

		if (curr_writer_total == db->db_xn_total_writer_count)
			/* No activity in 2 seconds: */
			xres_cp_checkpoint(self, db, curr_writer_total, FALSE);
		else {
			/* There server is busy, check if we need to
			 * write a checkpoint anyway...
			 */
			if (db->db_restart.xres_cp_required ||
				db->db_restart.xres_is_checkpoint_pending(db->db_xlog.xl_write_log_id, db->db_xlog.xl_write_log_offset)) {
				/* Flush tables, until the checkpoint is complete. */
				xres_cp_checkpoint(self, db, curr_writer_total, TRUE);
			}
		}

		if (curr_writer_total == db->db_xn_total_writer_count) {
			/* We did a checkpoint, and still, nothing has
			 * happened....
			 *
			 * Wait for something to happen:
			 */
			xtLogID		log_id;
			xtLogOffset	log_offset;

			while (!self->t_quit && curr_writer_total == db->db_xn_total_writer_count) {
				/* The writer position: */
				xt_lock_mutex(self, &db->db_wr_lock);
				pushr_(xt_unlock_mutex, &db->db_wr_lock);
				log_id = db->db_wr_log_id;
				log_offset = db->db_wr_log_offset;
				freer_(); // xt_unlock_mutex(&db->db_wr_lock)

				/* This condition means we could checkpoint: */
				if (!(xt_sl_get_size(db->db_datalogs.dlc_to_delete) == 0 &&
					xt_sl_get_size(db->db_datalogs.dlc_deleted) == 0 &&
					xt_comp_log_pos(log_id, log_offset, db->db_restart.xres_cp_log_id, db->db_restart.xres_cp_log_offset) <= 0))
					break;

				xres_cp_wait_for_log_writer(self, db, 400);
				xt_db_approximate_time = time(NULL);
				xt_db_free_unused_open_tables(self, db);
			}
		}
	}
}

static void *xres_cp_run_thread(XTThreadPtr self)
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
			xt_use_database(self, db, XT_FOR_CHECKPOINTER);

			/* This action is both safe and required (see details elsewhere) */
			xt_heap_release(self, self->st_database);

			xres_cp_main(self);
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
		count = 60;
		while (!self->t_quit && count > 0) {
			sleep(1);
			count--;
		}
	}

	myxt_destroy_thread(mysql_thread, TRUE);
	return NULL;
}

static void xres_cp_free_thread(XTThreadPtr self, void *data)
{
	XTDatabaseHPtr db = (XTDatabaseHPtr) data;

	if (db->db_cp_thread) {
		xt_lock_mutex(self, &db->db_cp_lock);
		pushr_(xt_unlock_mutex, &db->db_cp_lock);
		db->db_cp_thread = NULL;
		freer_(); // xt_unlock_mutex(&db->db_cp_lock)
	}
}

/* Start a checkpoint, if none has been started. */
xtPublic xtBool xt_begin_checkpoint(XTDatabaseHPtr db, xtBool have_table_lock, XTThreadPtr thread)
{
	XTCheckPointStatePtr	cp = &db->db_cp_state;
	xtLogID					log_id;
	xtLogOffset				log_offset;
	xtLogID					ind_rec_log_id;
	xtLogOffset				ind_rec_log_offset;
	u_int					edx;
	XTTableEntryPtr			te_ptr;
	XTTableHPtr				tab;
	XTOperationPtr			op;
	XTCheckPointTableRec	cpt;
	XTSortedListPtr			tables = NULL;

	/* First check if a checkpoint is already running: */
	xt_lock_mutex_ns(&cp->cp_state_lock);
	if (cp->cp_running) {
		xt_unlock_mutex_ns(&cp->cp_state_lock);
		return OK;
	}
	if (cp->cp_table_ids) {
		xt_free_sortedlist(NULL, cp->cp_table_ids);
		cp->cp_table_ids = NULL;
	}
	xt_unlock_mutex_ns(&cp->cp_state_lock);
	
	/* Flush the log before we continue. This is to ensure that
	 * before we write a checkpoint, that the changes
	 * done by the sweeper and the compactor, have been
	 * applied.
	 *
	 * Note, the sweeper does not flush the log, so this is
	 * necessary!
	 *
	 * --- I have removed this flush. It is actually just a
	 * minor optimisation, which pushes the flush position
	 * below ahead.
	 *
	 * Note that the writer position used for the checkpoint
	 * _will_ be behind the current log flush position.
	 *
	 * This is because the writer cannot apply log changes
	 * until they are flushed.
	 */
	/* This is an alternative to the above.
	if (!xt_xlog_flush_log(self))
		xt_throw(self);
	*/
	xt_lock_mutex_ns(&db->db_wr_lock);

	/* The theoretical maximum restart log postion, is the
	 * position of the writer thread:
	 */
	log_id = db->db_wr_log_id;
	log_offset = db->db_wr_log_offset;

	ind_rec_log_id = db->db_xlog.xl_flush_log_id;
	ind_rec_log_offset = db->db_xlog.xl_flush_log_offset;

	xt_unlock_mutex_ns(&db->db_wr_lock);

	/* Go through all the transactions, and find
	 * the lowest log start position of all the transactions.
	 */
	for (u_int i=0; i<XT_XN_NO_OF_SEGMENTS; i++) {
		XTXactSegPtr 	seg;

		seg = &db->db_xn_idx[i];
		XT_XACT_READ_LOCK(&seg->xs_tab_lock, self);
		for (u_int j=0; j<XT_XN_HASH_TABLE_SIZE; j++) {
			XTXactDataPtr	xact;
			
			xact = seg->xs_table[j];
			while (xact) {
				/* If the transaction is logged, but not cleaned: */
				if ((xact->xd_flags & (XT_XN_XAC_LOGGED | XT_XN_XAC_CLEANED)) == XT_XN_XAC_LOGGED) {
					if (xt_comp_log_pos(log_id, log_offset, xact->xd_begin_log, xact->xd_begin_offset) > 0) {
						log_id = xact->xd_begin_log;
						log_offset = xact->xd_begin_offset;
					}
				}
				xact = xact->xd_next_xact;
			}
		}
		XT_XACT_UNLOCK(&seg->xs_tab_lock, self, FALSE);
	}

#ifdef TRACE_CHECKPOINT
	printf("BEGIN CHECKPOINT %d-%llu\n", (int) log_id, (u_llong) log_offset);
#endif
	/* Go through all tables, and find the lowest log position.
	 * The log position stored by each table shows the position of
	 * the next operation that still needs to be applied.
	 *
	 * This comes from the list of operations which are
	 * queued for the table.
	 *
	 * This function also builds a list of tables!
	 */

	if (!(tables = xt_new_sortedlist_ns(sizeof(XTCheckPointTableRec), 20, xres_comp_flush_tabs, NULL, NULL)))
		return FAILED;

	xt_enum_tables_init(&edx);
	if (!have_table_lock)
		xt_ht_lock(NULL, db->db_tables);
	while ((te_ptr = xt_enum_tables_next(NULL, db, &edx))) {
		if ((tab = te_ptr->te_table)) {
			xt_sl_lock_ns(tab->tab_op_list, thread);
			if ((op = (XTOperationPtr) xt_sl_first_item(tab->tab_op_list))) {
				if (xt_comp_log_pos(log_id, log_offset, op->or_log_id, op->or_log_offset) > 0) {
					log_id = op->or_log_id;
					log_offset = op->or_log_offset;
				}
			}
			xt_sl_unlock(NULL, tab->tab_op_list);
			cpt.cpt_flushed = 0;
			cpt.cpt_tab_id = tab->tab_id;
#ifdef TRACE_CHECKPOINT
			printf("to flush: %d %s\n", (int) tab->tab_id, tab->tab_name->ps_path);
#endif
			if (!xt_sl_insert(NULL, tables, &tab->tab_id, &cpt)) {
				if (!have_table_lock)
					xt_ht_unlock(NULL, db->db_tables);
				xt_free_sortedlist(NULL, tables);
				return FAILED;
			}
		}
	}
	if (!have_table_lock)
		xt_ht_unlock(NULL, db->db_tables);

	xt_lock_mutex_ns(&cp->cp_state_lock);
	/* If there is a table list, then someone was faster than me! */
	if (!cp->cp_running && log_id && log_offset) {
		cp->cp_running = TRUE;
		cp->cp_log_id = log_id;
		cp->cp_log_offset = log_offset;

		cp->cp_ind_rec_log_id = ind_rec_log_id;
		cp->cp_ind_rec_log_offset = ind_rec_log_offset;

		cp->cp_flush_count = 0;
		cp->cp_next_to_flush = 0;
		cp->cp_table_ids = tables;
	}
	else
		xt_free_sortedlist(NULL, tables);
	xt_unlock_mutex_ns(&cp->cp_state_lock);

	/* At this point, log flushing can begin... */
	return OK;
}

/* End a checkpoint, if a checkpoint has been started,
 * and all checkpoint tables have been flushed
 */
xtPublic xtBool xt_end_checkpoint(XTDatabaseHPtr db, XTThreadPtr thread, xtBool *checkpoint_done)
{
	XTCheckPointStatePtr	cp = &db->db_cp_state;
	XTXlogCheckpointDPtr	cp_buf = NULL;
	char					path[PATH_MAX];
	XTOpenFilePtr			of;
	u_int					table_count;
	size_t					chk_size = 0; 
	u_int					no_of_logs = 0; 

#ifdef NEVER_CHECKPOINT
	return OK;
#endif
	/* Lock the checkpoint state so that only on thread can do this! */
	xt_lock_mutex_ns(&cp->cp_state_lock);
	if (!cp->cp_running)
		goto checkpoint_done;

	table_count = 0;
	if (cp->cp_table_ids)
		table_count = xt_sl_get_size(cp->cp_table_ids);
	if (cp->cp_flush_count < table_count) {
		/* Checkpoint is not done, yet! */
		xt_unlock_mutex_ns(&cp->cp_state_lock);
		if (checkpoint_done)
			*checkpoint_done = FALSE;
		return OK;
	}

	/* Check if anything has changed since the last checkpoint,
	 * if not, there is no need to write a new checkpoint!
	 */
	if (xt_sl_get_size(db->db_datalogs.dlc_to_delete) == 0 &&
		xt_sl_get_size(db->db_datalogs.dlc_deleted) == 0 &&
		xt_comp_log_pos(cp->cp_log_id, cp->cp_log_offset, db->db_restart.xres_cp_log_id, db->db_restart.xres_cp_log_offset) <= 0) {
		/* A checkpoint is required if the size of the deleted
		 * list is not zero. The reason is, I cannot remove the
		 * logs from the deleted list BEFORE a checkpoint has been
		 * done which does NOT include these logs.
		 *
		 * Even though the logs have already been deleted. They
		 * remain on the deleted list to ensure that they are NOT
		 * reused during this time, until the next checkpoint.
		 *
		 * This is done because if they are used, then on restart
		 * they would be deleted!
		 */
#ifdef TRACE_CHECKPOINT
		printf("--- END CHECKPOINT - no write\n");
#endif
		goto checkpoint_done;
	}

#ifdef TRACE_CHECKPOINT
	printf("--- END CHECKPOINT - write start point\n");
#endif
	xt_lock_mutex_ns(&db->db_datalogs.dlc_lock);

	no_of_logs = xt_sl_get_size(db->db_datalogs.dlc_to_delete);
	chk_size = offsetof(XTXlogCheckpointDRec, xcp_del_log) + no_of_logs * 2;
	xtLogID	*log_id_ptr;

	if (!(cp_buf = (XTXlogCheckpointDPtr) xt_malloc_ns(chk_size))) {
		xt_unlock_mutex_ns(&db->db_datalogs.dlc_lock);
		goto failed_0;
	}

	/* Increment the checkpoint number. This value is used if 2 checkpoint have the
	 * same log number. In this case checkpoints may differ in the log files
	 * that should be deleted. Here it is important to use the most recent
	 * log file!
	 */
	db->db_restart.xres_cp_number++;

	/* Create the checkpoint record: */
	XT_SET_DISK_4(cp_buf->xcp_head_size_4, chk_size);
	XT_SET_DISK_2(cp_buf->xcp_version_2, XT_CHECKPOINT_VERSION);
	XT_SET_DISK_6(cp_buf->xcp_chkpnt_no_6, db->db_restart.xres_cp_number);
	XT_SET_DISK_4(cp_buf->xcp_log_id_4, cp->cp_log_id);
	XT_SET_DISK_6(cp_buf->xcp_log_offs_6, cp->cp_log_offset);
	XT_SET_DISK_4(cp_buf->xcp_tab_id_4, db->db_curr_tab_id);
	XT_SET_DISK_4(cp_buf->xcp_xact_id_4, db->db_xn_curr_id);
	XT_SET_DISK_4(cp_buf->xcp_ind_rec_log_id_4, cp->cp_ind_rec_log_id);
	XT_SET_DISK_6(cp_buf->xcp_ind_rec_log_offs_6, cp->cp_ind_rec_log_offset);
	XT_SET_DISK_2(cp_buf->xcp_log_count_2, no_of_logs);

	for (u_int i=0; i<no_of_logs; i++) {
		log_id_ptr = (xtLogID *) xt_sl_item_at(db->db_datalogs.dlc_to_delete, i);
		XT_SET_DISK_2(cp_buf->xcp_del_log[i], (xtWord2) *log_id_ptr);
	}

	XT_SET_DISK_2(cp_buf->xcp_checksum_2, xt_get_checksum(((xtWord1 *) cp_buf) + 2, chk_size - 2, 1));

	xt_unlock_mutex_ns(&db->db_datalogs.dlc_lock);

	/* Write the checkpoint: */
	db->db_restart.xres_name(PATH_MAX, path, db->db_restart.xres_next_res_no);
	if (!(of = xt_open_file_ns(path, XT_FS_CREATE | XT_FS_MAKE_PATH)))
		goto failed_1;

	if (!xt_set_eof_file(NULL, of, 0))
		goto failed_2;
	if (!xt_pwrite_file(of, 0, chk_size, (xtWord1 *) cp_buf, &thread->st_statistics.st_x, thread))
		goto failed_2;
	if (!xt_flush_file(of, &thread->st_statistics.st_x, thread))
		goto failed_2;

	xt_close_file_ns(of);

	/* Next time write the other restart file: */
	db->db_restart.xres_next_res_no = (db->db_restart.xres_next_res_no % 2) + 1;
	db->db_restart.xres_cp_log_id = cp->cp_log_id;
	db->db_restart.xres_cp_log_offset = cp->cp_log_offset;
	db->db_restart.xres_cp_required = FALSE;

	/*
	 * Remove all the data logs that were deleted on the
	 * last checkpoint:
	 */
	if (!xres_remove_data_logs(db))
		goto failed_0;

#ifndef DEBUG_KEEP_LOGS
	/* After checkpoint, we can delete transaction logs that will no longer be required
	 * for recovery...
	 */
	if (cp->cp_log_id > 1) {
		xtLogID	current_log_id = cp->cp_log_id;
		xtLogID	del_log_id;

#ifdef XT_NUMBER_OF_LOGS_TO_SAVE
		if (pbxt_crash_debug) {
			/* To save the logs, we just consider them in use: */
			if (current_log_id > XT_NUMBER_OF_LOGS_TO_SAVE)
				current_log_id -= XT_NUMBER_OF_LOGS_TO_SAVE;
			else
				current_log_id = 1;
		}
#endif

		del_log_id = current_log_id - 1;

		while (del_log_id > 0) {
			db->db_xlog.xlog_name(PATH_MAX, path, del_log_id);
			if (!xt_fs_exists(path))
				break;
			del_log_id--;
		}

		/* This was the lowest log ID that existed: */
		del_log_id++;

		/* Delete all logs that still exist, that come before
		 * the current log:
		 *
		 * Do this from least to greatest to ensure no "holes" appear.
		 */
		while (del_log_id < current_log_id) {
			switch (db->db_xlog.xlog_delete_log(del_log_id, thread)) {
				case OK:
					break;
				case FAILED:
					goto exit_loop;
				case XT_ERR:
					goto failed_0;
			}
			del_log_id++;
		}
		exit_loop:;
	}

	/* And we can delete data logs in the list, and place them
	 * on the deleted list.
	 */
	xtLogID log_id;
	for (u_int i=0; i<no_of_logs; i++) {
		log_id = (xtLogID) XT_GET_DISK_2(cp_buf->xcp_del_log[i]);
		if (!xres_delete_data_log(db, log_id))
			goto failed_0;
	}
#endif

	xt_free_ns(cp_buf);
	cp_buf = NULL;

	checkpoint_done:
	cp->cp_running = FALSE;
	if (cp->cp_table_ids) {
		xt_free_sortedlist(NULL, cp->cp_table_ids);
		cp->cp_table_ids = NULL;
	}
	cp->cp_flush_count = 0;
	cp->cp_next_to_flush = 0;
	db->db_restart.xres_cp_required = FALSE;
	xt_unlock_mutex_ns(&cp->cp_state_lock);
	if (checkpoint_done)
		*checkpoint_done = TRUE;
	return OK;

	failed_2:
	xt_close_file_ns(of);

	failed_1:
	xt_free_ns(cp_buf);

	failed_0:
	if (cp_buf)
		xt_free_ns(cp_buf);
	xt_unlock_mutex_ns(&cp->cp_state_lock);
	return FAILED;
}

xtPublic xtWord8 xt_bytes_since_last_checkpoint(XTDatabaseHPtr db, xtLogID curr_log_id, xtLogOffset curr_log_offset)
{
	xtLogID					log_id;
	xtLogOffset				log_offset;
	size_t					byte_count = 0;

	log_id = db->db_restart.xres_cp_log_id;
	log_offset = db->db_restart.xres_cp_log_offset;

	/* Assume the logs have the threshold: */
	if (log_id < curr_log_id) {
		if (log_offset < xt_db_log_file_threshold)
			byte_count = (size_t) (xt_db_log_file_threshold - log_offset);
		log_offset = 0;
		log_id++;
	}
	while (log_id < curr_log_id) {
		byte_count += (size_t) xt_db_log_file_threshold;
		log_id++;
	}
	if (log_offset < curr_log_offset)
		byte_count += (size_t) (curr_log_offset - log_offset);

	return byte_count;
}

xtPublic void xt_start_checkpointer(XTThreadPtr self, XTDatabaseHPtr db)
{
	char name[PATH_MAX];

	sprintf(name, "CP-%s", xt_last_directory_of_path(db->db_main_path));
	xt_remove_dir_char(name);
	db->db_cp_thread = xt_create_daemon(self, name);
	xt_set_thread_data(db->db_cp_thread, db, xres_cp_free_thread);
	xt_run_thread(self, db->db_cp_thread, xres_cp_run_thread);
}

xtPublic void xt_wait_for_checkpointer(XTThreadPtr self, XTDatabaseHPtr db)
{
	time_t		then, now;
	xtBool		message = FALSE;
	xtLogID		log_id;
	xtLogOffset	log_offset;

	if (db->db_cp_thread) {
		then = time(NULL);
		for (;;) {
			xt_lock_mutex(self, &db->db_wr_lock);
			pushr_(xt_unlock_mutex, &db->db_wr_lock);
			log_id = db->db_wr_log_id;
			log_offset = db->db_wr_log_offset;
			freer_(); // xt_unlock_mutex(&db->db_wr_lock)

			if (xt_sl_get_size(db->db_datalogs.dlc_to_delete) == 0 &&
				xt_sl_get_size(db->db_datalogs.dlc_deleted) == 0 &&
				xt_comp_log_pos(log_id, log_offset, db->db_restart.xres_cp_log_id, db->db_restart.xres_cp_log_offset) <= 0)
				break;

			/* Do a final checkpoint before shutdown: */
			db->db_restart.xres_cp_required = TRUE;

			xt_lock_mutex(self, &db->db_cp_lock);
			pushr_(xt_unlock_mutex, &db->db_cp_lock);
			if (!xt_broadcast_cond_ns(&db->db_cp_cond)) {
				xt_log_and_clear_exception_ns();
				break;
			}
			freer_(); // xt_unlock_mutex(&db->db_cp_lock)

			xt_sleep_milli_second(10);

			now = time(NULL);
			if (now >= then + 16) {
				xt_logf(XT_NT_INFO, "Aborting wait for '%s' checkpointer\n", db->db_name);
				message = FALSE;
				break;
			}
			if (now >= then + 2) {
				if (!message) {
					message = TRUE;
					xt_logf(XT_NT_INFO, "Waiting for '%s' checkpointer...\n", db->db_name);
				}
			}
		}

		if (message)
			xt_logf(XT_NT_INFO, "Checkpointer '%s' done.\n", db->db_name);
	}
}

xtPublic void xt_stop_checkpointer(XTThreadPtr self, XTDatabaseHPtr db)
{
	XTThreadPtr thr_wr;

	if (db->db_cp_thread) {
		xt_lock_mutex(self, &db->db_cp_lock);
		pushr_(xt_unlock_mutex, &db->db_cp_lock);

		/* This pointer is safe as long as you have the transaction lock. */
		if ((thr_wr = db->db_cp_thread)) {
			xtThreadID tid = thr_wr->t_id;

			/* Make sure the thread quits when woken up. */
			xt_terminate_thread(self, thr_wr);

			xt_wake_checkpointer(self, db);

			freer_(); // xt_unlock_mutex(&db->db_cp_lock)

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
			db->db_cp_thread = NULL;
		}
		else
			freer_(); // xt_unlock_mutex(&db->db_cp_lock)
	}
}

xtPublic void xt_wake_checkpointer(XTThreadPtr self, XTDatabaseHPtr db)
{
	if (!xt_broadcast_cond_ns(&db->db_cp_cond))
		xt_log_and_clear_exception(self);
}

xtPublic void xt_free_writer_state(struct XTThread *self, XTWriterStatePtr ws)
{
	if (ws->ws_db)
		ws->ws_db->db_xlog.xlog_seq_exit(&ws->ws_seqread);
	xt_db_set_size(self, &ws->ws_databuf, 0);
	xt_ib_free(self, &ws->ws_rec_buf);
	if (ws->ws_ot) {
		xt_db_return_table_to_pool(self, ws->ws_ot);
		ws->ws_ot = NULL;
	}
}

xtPublic void xt_dump_xlogs(XTDatabaseHPtr db, xtLogID start_log)
{
	XTXactSeqReadRec	seq;
	XTXactLogBufferDPtr	record;
	xtLogID				log_id = db->db_restart.xres_cp_log_id;
	char				log_path[PATH_MAX];
	XTThreadPtr			thread = xt_get_self();

	/* Find the first log that still exists:*/
	for (;;) {
		log_id--;
		db->db_xlog.xlog_name(PATH_MAX, log_path, log_id);
		if (!xt_fs_exists(log_path))
			break;
	}
	log_id++;

	if (!db->db_xlog.xlog_seq_init(&seq, xt_db_log_buffer_size, FALSE))
		return;

	if (log_id < start_log)
		log_id = start_log;

	for (;;) {
		db->db_xlog.xlog_name(PATH_MAX, log_path, log_id);
		if (!xt_fs_exists(log_path))
			break;

		if (!db->db_xlog.xlog_seq_start(&seq, log_id, 0, FALSE))
			goto done;

		PRINTF("---------- DUMP LOG %d\n", (int) log_id);
		for (;;) {
			if (!db->db_xlog.xlog_seq_next(&seq, &record, TRUE, thread)) {
				PRINTF("---------- DUMP LOG %d ERROR\n", (int) log_id);
				xt_log_and_clear_exception_ns();
				break;
			}
			if (!record) {
				PRINTF("---------- DUMP LOG %d DONE\n", (int) log_id);
				break;
			}
			xt_print_log_record(seq.xseq_rec_log_id, seq.xseq_rec_log_offset, record);
		}

		log_id++;
	}

	done:
	db->db_xlog.xlog_seq_exit(&seq);
}

/* ----------------------------------------------------------------------
 * D A T A B A S E   R E C O V E R Y   T H R E A D
 */

extern XTDatabaseHPtr pbxt_database;

static void *xn_xres_run_recovery_thread(XTThreadPtr self)
{
	THD *mysql_thread;

	mysql_thread = (THD *)myxt_create_thread();

	while(!ha_resolve_by_legacy_type(mysql_thread, DB_TYPE_PBXT))
		xt_sleep_milli_second(1);

	xt_open_database(self, mysql_real_data_home, TRUE);
	pbxt_database = self->st_database;
	xt_heap_reference(self, pbxt_database);
	myxt_destroy_thread(mysql_thread, TRUE);

	return NULL;
}

xtPublic void xt_xres_start_database_recovery(XTThreadPtr self)
{
	char name[PATH_MAX];

	sprintf(name, "DB-RECOVERY-%s", xt_last_directory_of_path(mysql_real_data_home));
	xt_remove_dir_char(name);
	XTThreadPtr thread = xt_create_daemon(self, name);
	xt_run_thread(self, thread, xn_xres_run_recovery_thread);
}
