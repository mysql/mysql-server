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

#include "xt_config.h"

#include <stdio.h>
#ifndef XT_WIN
#include <unistd.h>
#include <signal.h>
#endif
#include <stdlib.h>

#ifndef DRIZZLED
#include "mysql_priv.h"
#endif

#include "ha_pbxt.h"

#include "filesys_xt.h"
#include "database_xt.h"
#include "memory_xt.h"
#include "strutil_xt.h"
#include "sortedlist_xt.h"
#include "util_xt.h"
#include "heap_xt.h"
#include "table_xt.h"
#include "trace_xt.h"
#include "myxt_xt.h"

static void dl_wake_co_thread(XTDatabaseHPtr db);

/*
 * --------------------------------------------------------------------------------
 * SEQUENTIAL READING
 */

xtBool XTDataSeqRead::sl_seq_init(struct XTDatabase *db, size_t buffer_size)
{
	sl_db = db;
	sl_buffer_size = buffer_size;

	sl_log_file = NULL;
	sl_log_eof = 0;

	sl_buf_log_offset = 0;
	sl_buffer_len = 0;
	sl_buffer = (xtWord1 *) xt_malloc_ns(buffer_size);

	sl_rec_log_id = 0;
	sl_rec_log_offset = 0;
	sl_record_len = 0;
	sl_extra_garbage = 0;

	return sl_buffer != NULL;
}

void XTDataSeqRead::sl_seq_exit()
{
	if (sl_log_file) {
		xt_close_file_ns(sl_log_file);
		sl_log_file  = NULL;
	}
	if (sl_buffer) {
		xt_free_ns(sl_buffer);
		sl_buffer = NULL;
	}
}

XTOpenFilePtr XTDataSeqRead::sl_seq_open_file()
{
	return sl_log_file;
}

void XTDataSeqRead::sl_seq_pos(xtLogID *log_id, xtLogOffset *log_offset)
{
	*log_id = sl_rec_log_id;
	*log_offset = sl_rec_log_offset;
}

xtBool XTDataSeqRead::sl_seq_start(xtLogID log_id, xtLogOffset log_offset, xtBool missing_ok)
{
	if (sl_rec_log_id != log_id) {
		if (sl_log_file) {
			xt_close_file_ns(sl_log_file);
			sl_log_file  = NULL;
		}

		sl_rec_log_id = log_id;
		sl_buf_log_offset = sl_rec_log_offset;
		sl_buffer_len = 0;

		if (!sl_db->db_datalogs.dlc_open_log(&sl_log_file, log_id, missing_ok ? XT_FS_MISSING_OK : XT_FS_DEFAULT))
			return FAILED;
		if (sl_log_file)
			sl_log_eof = xt_seek_eof_file(NULL, sl_log_file);
	}
	sl_rec_log_offset = log_offset;
	sl_record_len = 0;
	return OK;
}

xtBool XTDataSeqRead::sl_rnd_read(xtLogOffset log_offset, size_t size, xtWord1 *buffer, size_t *data_read, struct XTThread *thread)
{
	if (!sl_log_file) {
		*data_read = 0;
		return OK;
	}
	return xt_pread_file(sl_log_file, log_offset, size, 0, buffer, data_read, &thread->st_statistics.st_data, thread);
}

/*
 * Unlike the transaction log sequential reader, this function only returns
 * the header of a record.
 *
 * {SKIP-GAPS}
 * This function now skips gaps. This should not be required, because in normal
 * operation, no gaps should be created.
 *
 * However, if his happens there is a danger that a valid record after the
 * gap will be lost.
 *
 * So, if we find an invalid record, we scan through the log to find the next
 * valid record. Note, that there is still a danger that will will find
 * data that looks like a valid record, but is not.
 *
 * In this case, this "pseudo record" may cause the function to actually skip
 * valid records.
 *
 * Note, any such malfunction will eventually cause the record to be lost forever
 * after the garbage collector has run.
 */
xtBool XTDataSeqRead::sl_seq_next(XTXactLogBufferDPtr *ret_entry, struct XTThread *thread)
{
	XTXactLogBufferDPtr	record;
	size_t				tfer;
	size_t				len = 0;
	size_t				rec_offset;
	size_t				max_rec_len;
	xtBool				reread_from_buffer;
	xtWord4				size;
	xtLogOffset			gap_start = 0;

	/* Go to the next record (xseq_record_len must be initialized
	 * to 0 for this to work.
	 */
	retry:
	sl_rec_log_offset += sl_record_len;
	sl_record_len = 0;

	if (sl_rec_log_offset < sl_buf_log_offset ||
		sl_rec_log_offset >= sl_buf_log_offset + (xtLogOffset) sl_buffer_len) {
		/* The current position is nowhere near the buffer, read data into the
		 * buffer:
		 */
		tfer = sl_buffer_size;
		if (!sl_rnd_read(sl_rec_log_offset, tfer, sl_buffer, &tfer, thread))
			return FAILED;
		sl_buf_log_offset = sl_rec_log_offset;
		sl_buffer_len = tfer;

		/* Should we go to the next log? */
		if (!tfer)
			goto return_empty;
	}

	/* The start of the record is in the buffer: */
	read_from_buffer:
	rec_offset = (size_t) (sl_rec_log_offset - sl_buf_log_offset);
	max_rec_len = sl_buffer_len - rec_offset;
	reread_from_buffer = FALSE;
	size = 0;

	/* Check the type of record: */
	record = (XTXactLogBufferDPtr) (sl_buffer + rec_offset);
	switch (record->xl.xl_status_1) {
		case XT_LOG_ENT_HEADER:
			if (sl_rec_log_offset != 0)
				goto scan_to_next_record;
			if (offsetof(XTXactLogHeaderDRec, xh_size_4) + 4 > max_rec_len) {
				reread_from_buffer = TRUE;
				goto read_more;
			}
			len = XT_GET_DISK_4(record->xh.xh_size_4);
			if (len > max_rec_len) {
				reread_from_buffer = TRUE;
				goto read_more;
			}

			if (record->xh.xh_checksum_1 != XT_CHECKSUM_1(sl_rec_log_id))
				goto return_empty;
			if (XT_LOG_HEAD_MAGIC(record, len) != XT_LOG_FILE_MAGIC)
				goto return_empty;
			if (len > offsetof(XTXactLogHeaderDRec, xh_log_id_4) + 4) {
				if (XT_GET_DISK_4(record->xh.xh_log_id_4) != sl_rec_log_id)
					goto return_empty;
			}
			break;
		case XT_LOG_ENT_EXT_REC_OK:
		case XT_LOG_ENT_EXT_REC_DEL:
			if (gap_start) {
				xt_logf(XT_NS_CONTEXT, XT_LOG_WARNING, "Gap in data log %lu, start: %llu, size: %llu\n", (u_long) sl_rec_log_id, (u_llong) gap_start, (u_llong) (sl_rec_log_offset - gap_start));
				gap_start = 0;
			}
			len = offsetof(XTactExtRecEntryDRec, er_data);
			if (len > max_rec_len) {
				reread_from_buffer = TRUE;
				goto read_more;
			}
			size = XT_GET_DISK_4(record->er.er_data_size_4);
			/* Verify the record as good as we can! */
			if (!size)
				goto scan_to_next_record;
			if (sl_rec_log_offset + (xtLogOffset) offsetof(XTactExtRecEntryDRec, er_data) + size > sl_log_eof)
				goto scan_to_next_record;
			if (!XT_GET_DISK_4(record->er.er_tab_id_4))
				goto scan_to_next_record;
			if (!XT_GET_DISK_4(record->er.er_rec_id_4))
				goto scan_to_next_record;
			break;
		default:
			/* Note, we no longer assume EOF.
			 * Instead, we skip to the next value record. */
			goto scan_to_next_record;
	}

	if (len <= max_rec_len) {
		/* The record is completely in the buffer: */
		sl_record_len = len+size;
		*ret_entry = record;
		return OK;
	}
	
	read_more:
	/* The record is partially in the buffer. */
	memmove(sl_buffer, sl_buffer + rec_offset, max_rec_len);
	sl_buf_log_offset += rec_offset;
	sl_buffer_len = max_rec_len;

	/* Read the rest, as far as possible: */
	tfer = sl_buffer_size - max_rec_len;
	if (!sl_rnd_read(sl_buf_log_offset + max_rec_len, tfer, sl_buffer + max_rec_len, &tfer, thread))
		return FAILED;
	sl_buffer_len += tfer;

	if (sl_buffer_len < len)
		/* A partial record is in the log, must be the end of the log: */
		goto return_empty;

	if (reread_from_buffer)
		goto read_from_buffer;

	/* The record is not completely in the buffer: */
	sl_record_len = len;
	*ret_entry = (XTXactLogBufferDPtr) sl_buffer;
	return OK;

	scan_to_next_record:
	if (!gap_start) {
		gap_start = sl_rec_log_offset;
		xt_logf(XT_NS_CONTEXT, XT_LOG_WARNING, "Gap found in data log %lu, starting at offset %llu\n", (u_long) sl_rec_log_id, (u_llong) gap_start);
	}
	sl_record_len = 1;
	sl_extra_garbage++;
	goto retry;

	return_empty:
	if (gap_start) {
		xt_logf(XT_NS_CONTEXT, XT_LOG_WARNING, "Gap in data log %lu, start: %llu, size: %llu\n", (u_long) sl_rec_log_id, (u_llong) gap_start, (u_llong) (sl_rec_log_offset - gap_start));
		gap_start = 0;
	}
	*ret_entry = NULL;
	return OK;
}

void XTDataSeqRead::sl_seq_skip(size_t size)
{
	sl_record_len += size;
}

void XTDataSeqRead::sl_seq_skip_to(off_t log_offset)
{
	if (log_offset >= sl_rec_log_offset)
		sl_record_len = (size_t) (log_offset - sl_rec_log_offset);
}

/*
 * --------------------------------------------------------------------------------
 * STATIC UTILITIES
 */

static xtBool dl_create_log_header(XTDataLogFilePtr data_log, XTOpenFilePtr of, XTThreadPtr thread)
{
	XTXactLogHeaderDRec	header;

	/* The header was not completely written, so write a new one: */
	memset(&header, 0, sizeof(XTXactLogHeaderDRec));
	header.xh_status_1 = XT_LOG_ENT_HEADER;
	header.xh_checksum_1 = XT_CHECKSUM_1(data_log->dlf_log_id);
	XT_SET_DISK_4(header.xh_size_4, sizeof(XTXactLogHeaderDRec));
	XT_SET_DISK_8(header.xh_free_space_8, 0);
	XT_SET_DISK_8(header.xh_file_len_8, sizeof(XTXactLogHeaderDRec));
	XT_SET_DISK_4(header.xh_log_id_4, data_log->dlf_log_id);
	XT_SET_DISK_2(header.xh_version_2, XT_LOG_VERSION_NO);
	XT_SET_DISK_4(header.xh_magic_4, XT_LOG_FILE_MAGIC);
	if (!xt_pwrite_file(of, 0, sizeof(XTXactLogHeaderDRec), &header, &thread->st_statistics.st_data, thread))
		return FAILED;
	if (!xt_flush_file(of, &thread->st_statistics.st_data, thread))
		return FAILED;
	return OK;
}

static xtBool dl_write_garbage_level(XTDataLogFilePtr data_log, XTOpenFilePtr of, xtBool flush, XTThreadPtr thread)
{
	XTXactLogHeaderDRec	header;

	/* The header was not completely written, so write a new one: */
	XT_SET_DISK_8(header.xh_free_space_8, data_log->dlf_garbage_count);
	if (!xt_pwrite_file(of, offsetof(XTXactLogHeaderDRec, xh_free_space_8), 8, (xtWord1 *) &header.xh_free_space_8, &thread->st_statistics.st_data, thread))
		return FAILED;
	if (flush && !xt_flush_file(of, &thread->st_statistics.st_data, thread))
		return FAILED;
	return OK;
}

/*
 * {SKIP-GAPS}
 * Extra garbage is the amount of space skipped during recovery of the data
 * log file. We assume this space has not be counted as garbage, 
 * and add it to the garbage count.
 *
 * This may mean that our estimate of garbaged is higher than it should
 * be, but that is better than the other way around.
 *
 * The fact is, there should not be any gaps in the data log files, so
 * this is actually an exeption which should not occur.
 */
static xtBool dl_write_log_header(XTDataLogFilePtr data_log, XTOpenFilePtr of, xtLogOffset extra_garbage, XTThreadPtr thread)
{
	XTXactLogHeaderDRec	header;

	XT_SET_DISK_8(header.xh_file_len_8, data_log->dlf_log_eof);

	if (extra_garbage) {
		data_log->dlf_garbage_count += extra_garbage;
		if (data_log->dlf_garbage_count > data_log->dlf_log_eof)
			data_log->dlf_garbage_count = data_log->dlf_log_eof;
		XT_SET_DISK_8(header.xh_free_space_8, data_log->dlf_garbage_count);
		if (!xt_pwrite_file(of, offsetof(XTXactLogHeaderDRec, xh_free_space_8), 16, (xtWord1 *) &header.xh_free_space_8, &thread->st_statistics.st_data, thread))
			return FAILED;
	}
	else {
		if (!xt_pwrite_file(of, offsetof(XTXactLogHeaderDRec, xh_file_len_8), 8, (xtWord1 *) &header.xh_file_len_8, &thread->st_statistics.st_data, thread))
			return FAILED;
	}
	if (!xt_flush_file(of, &thread->st_statistics.st_data, thread))
		return FAILED;
	return OK;
}

static void dl_free_seq_read(XTThreadPtr self __attribute__((unused)), XTDataSeqReadPtr seq_read)
{
	seq_read->sl_seq_exit();
}

static void dl_recover_log(XTThreadPtr self, XTDatabaseHPtr db, XTDataLogFilePtr data_log)
{
	XTDataSeqReadRec	seq_read;
	XTXactLogBufferDPtr	record;

	if (!seq_read.sl_seq_init(db, xt_db_log_buffer_size))
		xt_throw(self);
	pushr_(dl_free_seq_read, &seq_read);

	seq_read.sl_seq_start(data_log->dlf_log_id, 0, FALSE);

	for (;;) {
		if (!seq_read.sl_seq_next(&record, self))
			xt_throw(self);
		if (!record)
			break;
		switch (record->xh.xh_status_1) {
			case XT_LOG_ENT_HEADER:
				data_log->dlf_garbage_count = XT_GET_DISK_8(record->xh.xh_free_space_8);
				data_log->dlf_start_offset = XT_GET_DISK_8(record->xh.xh_comp_pos_8);
				seq_read.sl_seq_skip_to((off_t) XT_GET_DISK_8(record->xh.xh_file_len_8)); 
				break;
		}
	}

	ASSERT_NS(seq_read.sl_log_eof == seq_read.sl_rec_log_offset);
	data_log->dlf_log_eof = seq_read.sl_rec_log_offset;

	if (data_log->dlf_log_eof < sizeof(XTXactLogHeaderDRec)) {
		data_log->dlf_log_eof = sizeof(XTXactLogHeaderDRec);
		if (!dl_create_log_header(data_log, seq_read.sl_log_file, self))
			xt_throw(self);
	}
	else {
		if (!dl_write_log_header(data_log, seq_read.sl_log_file, seq_read.sl_extra_garbage, self))
			xt_throw(self);
	}

	freer_(); // dl_free_seq_read(&seq_read)
}

/*
 * --------------------------------------------------------------------------------
 * D A T A  L O G  C AC H E
 */

void XTDataLogCache::dls_remove_log(XTDataLogFilePtr data_log)
{
	xtLogID log_id = data_log->dlf_log_id;

	switch (data_log->dlf_state) {
		case XT_DL_HAS_SPACE:
			xt_sl_delete(NULL, dlc_has_space, &log_id);
			break;
		case XT_DL_TO_COMPACT:
			xt_sl_delete(NULL, dlc_to_compact, &log_id);
			break;
		case XT_DL_TO_DELETE:
			xt_sl_delete(NULL, dlc_to_delete, &log_id);
			break;
		case XT_DL_DELETED:
			xt_sl_delete(NULL, dlc_deleted, &log_id);
			break;
	}
}

int XTDataLogCache::dls_get_log_state(XTDataLogFilePtr data_log)
{
	if (data_log->dlf_to_much_garbage())
		return XT_DL_TO_COMPACT;
	if (data_log->dlf_space_avaliable() > 0)
		return XT_DL_HAS_SPACE;
	return XT_DL_READ_ONLY;
}

xtBool XTDataLogCache::dls_set_log_state(XTDataLogFilePtr data_log, int state)
{
	xtLogID log_id = data_log->dlf_log_id;

	xt_lock_mutex_ns(&dlc_lock);
	if (state == XT_DL_MAY_COMPACT) {
		if (data_log->dlf_state != XT_DL_UNKNOWN &&
			data_log->dlf_state != XT_DL_HAS_SPACE &&
			data_log->dlf_state != XT_DL_READ_ONLY)
			goto ok;
		state = XT_DL_TO_COMPACT;
	}
	if (state == XT_DL_UNKNOWN)
		state = dls_get_log_state(data_log);
	switch (state) {
		case XT_DL_HAS_SPACE:
			if (data_log->dlf_state != XT_DL_HAS_SPACE) {
				dls_remove_log(data_log);
				if (!xt_sl_insert(NULL, dlc_has_space, &log_id, &log_id))
					goto failed;
			}
			break;
		case XT_DL_TO_COMPACT:
#ifdef DEBUG_LOG_DELETE
			printf("-- set to compact: %d\n", (int) log_id);
#endif
			if (data_log->dlf_state != XT_DL_TO_COMPACT) {
				dls_remove_log(data_log);
				if (!xt_sl_insert(NULL, dlc_to_compact, &log_id, &log_id))
					goto failed;
			}
			dl_wake_co_thread(dlc_db);
			break;
		case XT_DL_COMPACTED:
#ifdef DEBUG_LOG_DELETE
			printf("-- set compacted: %d\n", (int) log_id);
#endif
			if (data_log->dlf_state != state)
				dls_remove_log(data_log);
			break;
		case XT_DL_TO_DELETE:
#ifdef DEBUG_LOG_DELETE
			printf("-- set to delete log: %d\n", (int) log_id);
#endif
			if (data_log->dlf_state != XT_DL_TO_DELETE) {
				dls_remove_log(data_log);
				if (!xt_sl_insert(NULL, dlc_to_delete, &log_id, &log_id))
					goto failed;
			}
			break;
		case XT_DL_DELETED:
#ifdef DEBUG_LOG_DELETE
			printf("-- set DELETED log: %d\n", (int) log_id);
#endif
			if (data_log->dlf_state != XT_DL_DELETED) {
				dls_remove_log(data_log);
				if (!xt_sl_insert(NULL, dlc_deleted, &log_id, &log_id))
					goto failed;
			}
			break;
		default:
			if (data_log->dlf_state != state)
				dls_remove_log(data_log);
			break;
	}
	data_log->dlf_state = state;

	ok:
	xt_unlock_mutex_ns(&dlc_lock);
	return OK;

	failed:
	xt_unlock_mutex_ns(&dlc_lock);
	return FAILED;
}

static int dl_cmp_log_id(XTThreadPtr XT_UNUSED(self), register const void *XT_UNUSED(thunk), register const void *a, register const void *b)
{
	xtLogID			log_id_a = *((xtLogID *) a);
	xtLogID			log_id_b = *((xtLogID *) b);

	if (log_id_a == log_id_b)
		return 0;
	if (log_id_a < log_id_b)
		return -1;
	return 1;
}

void XTDataLogCache::dlc_init(XTThreadPtr self, XTDatabaseHPtr db)
{
	XTOpenDirPtr		od;
	char				log_dir[PATH_MAX];
	char				*file;
	xtLogID				log_id;
	XTDataLogFilePtr	data_log= NULL;

	memset(this, 0, sizeof(XTDataLogCacheRec));
	dlc_db = db;
	try_(a) {
		xt_init_mutex_with_autoname(self, &dlc_lock);
		xt_init_cond(self, &dlc_cond);
		for (u_int i=0; i<XT_DL_NO_OF_SEGMENTS; i++) {
			xt_init_mutex_with_autoname(self, &dlc_segment[i].dls_lock);
			xt_init_cond(self, &dlc_segment[i].dls_cond);
		}
		dlc_has_space = xt_new_sortedlist(self, sizeof(xtLogID), 20, 10, dl_cmp_log_id, NULL, NULL, FALSE, FALSE);
		dlc_to_compact = xt_new_sortedlist(self, sizeof(xtLogID), 20, 10, dl_cmp_log_id, NULL, NULL, FALSE, FALSE);
		dlc_to_delete = xt_new_sortedlist(self, sizeof(xtLogID), 20, 10, dl_cmp_log_id, NULL, NULL, FALSE, FALSE);
		dlc_deleted = xt_new_sortedlist(self, sizeof(xtLogID), 20, 10, dl_cmp_log_id, NULL, NULL, FALSE, FALSE);
		xt_init_mutex_with_autoname(self, &dlc_mru_lock);
		xt_init_mutex_with_autoname(self, &dlc_head_lock);

		xt_strcpy(PATH_MAX, log_dir, dlc_db->db_main_path);
		xt_add_data_dir(PATH_MAX, log_dir);
		if (xt_fs_exists(log_dir)) {
			pushsr_(od, xt_dir_close, xt_dir_open(self, log_dir, NULL));
			while (xt_dir_next(self, od)) {
				file = xt_dir_name(self, od);
				if (xt_ends_with(file, ".xt")) {
					if ((log_id = (xtLogID) xt_file_name_to_id(file))) {
						if (!dlc_get_data_log(&data_log, log_id, TRUE, NULL))
							xt_throw(self);
						dl_recover_log(self, db, data_log);
						if (!dls_set_log_state(data_log, XT_DL_UNKNOWN))
							xt_throw(self);
					}
				}
			}
			freer_();
		}
	}
	catch_(a) {
		dlc_exit(self);
		xt_throw(self);
	}
	cont_(a);
}

void XTDataLogCache::dlc_exit(XTThreadPtr self)
{
	XTDataLogFilePtr	data_log, tmp_data_log;
	XTOpenLogFilePtr	open_log, tmp_open_log;

	if (dlc_has_space) {
		xt_free_sortedlist(self, dlc_has_space);
		dlc_has_space = NULL;
	}
	if (dlc_to_compact) {
		xt_free_sortedlist(self, dlc_to_compact);
		dlc_to_compact = NULL;
	}
	if (dlc_to_delete) {
		xt_free_sortedlist(self, dlc_to_delete);
		dlc_to_delete = NULL;
	}
	if (dlc_deleted) {
		xt_free_sortedlist(self, dlc_deleted);
		dlc_deleted = NULL;
	}
	for (u_int i=0; i<XT_DL_NO_OF_SEGMENTS; i++) {
		for (u_int j=0; j<XT_DL_SEG_HASH_TABLE_SIZE; j++) {
			data_log = dlc_segment[i].dls_hash_table[j];
			while (data_log) {
				if (data_log->dlf_log_file) {
					xt_close_file_ns(data_log->dlf_log_file);
					data_log->dlf_log_file = NULL;
				}

				open_log = data_log->dlf_free_list;
				while (open_log) {
					if (open_log->odl_log_file)
						xt_close_file(self, open_log->odl_log_file);
					tmp_open_log = open_log;
					open_log = open_log->odl_next_free;
					xt_free(self, tmp_open_log);
				}
				tmp_data_log = data_log;
				data_log = data_log->dlf_next_hash;

				xt_free(self, tmp_data_log);
			}
		}
		xt_free_mutex(&dlc_segment[i].dls_lock);
		xt_free_cond(&dlc_segment[i].dls_cond);
	}
	xt_free_mutex(&dlc_head_lock);
	xt_free_mutex(&dlc_mru_lock);
	xt_free_mutex(&dlc_lock);
	xt_free_cond(&dlc_cond);
}

void XTDataLogCache::dlc_name(size_t size, char *path, xtLogID log_id)
{
	char name[50];

	sprintf(name, "dlog-%lu.xt", (u_long) log_id);
	xt_strcpy(size, path, dlc_db->db_main_path);
	xt_add_data_dir(size, path);
	xt_add_dir_char(size, path);
	xt_strcat(size, path, name);
}

xtBool XTDataLogCache::dlc_open_log(XTOpenFilePtr *fh, xtLogID log_id, int mode)
{
	char log_path[PATH_MAX];

	dlc_name(PATH_MAX, log_path, log_id);
	return xt_open_file_ns(fh, log_path, mode);
}

xtBool XTDataLogCache::dlc_unlock_log(XTDataLogFilePtr data_log)
{
	if (data_log->dlf_log_file) {
		xt_close_file_ns(data_log->dlf_log_file);
		data_log->dlf_log_file = NULL;
	}

	return dls_set_log_state(data_log, XT_DL_UNKNOWN);
}

XTDataLogFilePtr XTDataLogCache::dlc_get_log_for_writing(off_t space_required, struct XTThread *thread)
{
	xtLogID				log_id, *log_id_ptr = NULL;
	size_t				size;
	size_t				idx;
	XTDataLogFilePtr	data_log = NULL;

	xt_lock_mutex_ns(&dlc_lock);

	/* Look for an existing log with enough space: */
	size = xt_sl_get_size(dlc_has_space);
	for (idx=0; idx<size; idx++) {
		log_id_ptr = (xtLogID *) xt_sl_item_at(dlc_has_space, idx);
		if (!dlc_get_data_log(&data_log, *log_id_ptr, FALSE, NULL))
			goto failed;
		if (data_log) {
			if (data_log->dlf_space_avaliable() >= space_required)
				break;
			data_log = NULL;
		}
		else {
			ASSERT_NS(FALSE);
			xt_sl_delete_item_at(NULL, dlc_has_space, idx);
			idx--;
			size--;
		}
	}

	if (data_log) {
		/* Found a log: */
		if (!dlc_open_log(&data_log->dlf_log_file, *log_id_ptr, XT_FS_DEFAULT))
			goto failed;
		xt_sl_delete_item_at(NULL, dlc_has_space, idx);
	}
	else {
		/* Create a new log: */
		log_id = dlc_next_log_id;
		for (u_int i=0; i<XT_DL_MAX_LOG_ID; i++) {
			log_id++;
			if (log_id > XT_DL_MAX_LOG_ID)
				log_id = 1;
			if (!dlc_get_data_log(&data_log, log_id, FALSE, NULL))
				goto failed;
			if (!data_log)
				break;
		}
		dlc_next_log_id = log_id;
		if (data_log) {
			xt_register_ulxterr(XT_REG_CONTEXT, XT_ERR_LOG_MAX_EXCEEDED, (u_long) XT_DL_MAX_LOG_ID);
			goto failed;
		}
		if (!dlc_get_data_log(&data_log, log_id, TRUE, NULL))
			goto failed;
		if (!dlc_open_log(&data_log->dlf_log_file, log_id, XT_FS_CREATE | XT_FS_MAKE_PATH))
			goto failed;
		data_log->dlf_log_eof = sizeof(XTXactLogHeaderDRec);
		if (!dl_create_log_header(data_log, data_log->dlf_log_file, thread)) {
			xt_close_file_ns(data_log->dlf_log_file);
			goto failed;
		}
		/* By setting this late we ensure that the error
		 * will be repeated.
		 */ 
		dlc_next_log_id = log_id;
	}
	data_log->dlf_state = XT_DL_EXCLUSIVE;

	xt_unlock_mutex_ns(&dlc_lock);
	return data_log;

	failed:
	xt_unlock_mutex_ns(&dlc_lock);
	return NULL;
}

xtBool XTDataLogCache::dlc_get_data_log(XTDataLogFilePtr *lf, xtLogID log_id, xtBool create, XTDataLogSegPtr *ret_seg)
{
	register XTDataLogSegPtr	seg;
	register u_int				hash_idx;
	register XTDataLogFilePtr	data_log;

	/* Which segment, and hash index: */
	seg = &dlc_segment[log_id & XT_DL_SEGMENT_MASK];
	hash_idx = (log_id >> XT_DL_SEGMENT_SHIFTS) % XT_DL_SEG_HASH_TABLE_SIZE;

	/* Lock the segment: */
	xt_lock_mutex_ns(&seg->dls_lock);

	/* Find the log file on the hash list: */
	data_log = seg->dls_hash_table[hash_idx];
	while (data_log) {
		if (data_log->dlf_log_id == log_id)
			break;
		data_log = data_log->dlf_next_hash;
	}

	if (!data_log && create) {
		/* Create a new log file structure: */
		if (!(data_log = (XTDataLogFilePtr) xt_calloc_ns(sizeof(XTDataLogFileRec))))
			goto failed;
		data_log->dlf_log_id = log_id;
		data_log->dlf_next_hash = seg->dls_hash_table[hash_idx];
		seg->dls_hash_table[hash_idx] = data_log;
	}

	if (ret_seg) {
		/* This gives the caller the lock: */
		*ret_seg = seg;
		*lf = data_log;
		return OK;
	}

	xt_unlock_mutex_ns(&seg->dls_lock);
	*lf = data_log;
	return OK;

	failed:
	xt_unlock_mutex_ns(&seg->dls_lock);
	return FAILED;
}

/*
 * If just_close is FALSE, then a log is being deleted.
 * This means that that the log may still be in exclusive use by
 * some thread. So we just close the log!
 */
xtBool XTDataLogCache::dlc_remove_data_log(xtLogID log_id, xtBool just_close)
{
	register XTDataLogSegPtr	seg;
	register u_int				hash_idx;
	register XTDataLogFilePtr	data_log;
	XTOpenLogFilePtr			open_log, tmp_open_log;

	/* Which segment, and hash index: */
	seg = &dlc_segment[log_id & XT_DL_SEGMENT_MASK];
	hash_idx = (log_id >> XT_DL_SEGMENT_SHIFTS) % XT_DL_SEG_HASH_TABLE_SIZE;

	/* Lock the segment: */
	retry:
	xt_lock_mutex_ns(&seg->dls_lock);

	/* Find the log file on the hash list: */
	data_log = seg->dls_hash_table[hash_idx];
	while (data_log) {
		if (data_log->dlf_log_id == log_id)
			break;
		data_log = data_log->dlf_next_hash;
	}

	if (data_log) {
		xt_lock_mutex_ns(&dlc_mru_lock);

		open_log = data_log->dlf_free_list;
		while (open_log) {
			if (open_log->odl_log_file)
				xt_close_file_ns(open_log->odl_log_file);

			/* Remove from MRU list: */
			if (dlc_lru_open_log == open_log) {
				dlc_lru_open_log = open_log->odl_mr_used;
				ASSERT_NS(!open_log->odl_lr_used);
			}
			else if (open_log->odl_lr_used)
				open_log->odl_lr_used->odl_mr_used = open_log->odl_mr_used;
			if (dlc_mru_open_log == open_log) {
				dlc_mru_open_log = open_log->odl_lr_used;
				ASSERT_NS(!open_log->odl_mr_used);
			}
			else if (open_log->odl_mr_used)
				open_log->odl_mr_used->odl_lr_used = open_log->odl_lr_used;

			data_log->dlf_open_count--;
			tmp_open_log = open_log;
			open_log = open_log->odl_next_free;
			xt_free_ns(tmp_open_log);
		}
		data_log->dlf_free_list = NULL;

		xt_unlock_mutex_ns(&dlc_mru_lock);

		if (data_log->dlf_open_count) {
			if (!xt_timed_wait_cond_ns(&seg->dls_cond, &seg->dls_lock, 2000))
				goto failed;
			xt_unlock_mutex_ns(&seg->dls_lock);
			goto retry;
		}

		/* Close the exclusive file if required: */
		if (data_log->dlf_log_file) {
			xt_close_file_ns(data_log->dlf_log_file);
			data_log->dlf_log_file = NULL;
		}

		if (!just_close) {
			/* Remove the log from the hash list: */
			XTDataLogFilePtr ptr, pptr = NULL;

			ptr = seg->dls_hash_table[hash_idx];
			while (ptr) {
				if (ptr == data_log)
					break;
				pptr = ptr;
				ptr = ptr->dlf_next_hash;
			}
			
			if (ptr == data_log) {
				if (pptr)
					pptr->dlf_next_hash = ptr->dlf_next_hash;
				else
					seg->dls_hash_table[hash_idx] = ptr->dlf_next_hash;
			}

			xt_free_ns(data_log);
		}
	}

	xt_unlock_mutex_ns(&seg->dls_lock);
	return OK;

	failed:
	xt_unlock_mutex_ns(&seg->dls_lock);
	return FAILED;
}

xtBool XTDataLogCache::dlc_get_open_log(XTOpenLogFilePtr *ol, xtLogID log_id)
{
	register XTDataLogSegPtr	seg;
	register u_int				hash_idx;
	register XTDataLogFilePtr	data_log;
	register XTOpenLogFilePtr	open_log;
	char						path[PATH_MAX];

	/* Which segment, and hash index: */
	seg = &dlc_segment[log_id & XT_DL_SEGMENT_MASK];
	hash_idx = (log_id >> XT_DL_SEGMENT_SHIFTS) % XT_DL_SEG_HASH_TABLE_SIZE;

	/* Lock the segment: */
	xt_lock_mutex_ns(&seg->dls_lock);

	/* Find the log file on the hash list: */
	data_log = seg->dls_hash_table[hash_idx];
	while (data_log) {
		if (data_log->dlf_log_id == log_id)
			break;
		data_log = data_log->dlf_next_hash;
	}

	if (!data_log) {
		/* Create a new log file structure: */
		dlc_name(PATH_MAX, path, log_id);
		if (!xt_fs_exists(path)) {
			xt_register_ixterr(XT_REG_CONTEXT, XT_ERR_DATA_LOG_NOT_FOUND, path);
			goto failed;
		}
		if (!(data_log = (XTDataLogFilePtr) xt_calloc_ns(sizeof(XTDataLogFileRec))))
			goto failed;
		data_log->dlf_log_id = log_id;
		data_log->dlf_next_hash = seg->dls_hash_table[hash_idx];
		seg->dls_hash_table[hash_idx] = data_log;
	}

	if ((open_log = data_log->dlf_free_list)) {
		/* Remove from the free list: */
		if ((data_log->dlf_free_list = open_log->odl_next_free))
			data_log->dlf_free_list->odl_prev_free = NULL;

		/* This file has been most recently used: */
		if (XT_TIME_DIFF(open_log->odl_ru_time, dlc_ru_now) > (XT_DL_LOG_POOL_SIZE >> 1)) {
			/* Move to the front of the MRU list: */
			xt_lock_mutex_ns(&dlc_mru_lock);

			open_log->odl_ru_time = ++dlc_ru_now;
			if (dlc_mru_open_log != open_log) {
				/* Remove from the MRU list: */
				if (dlc_lru_open_log == open_log) {
					dlc_lru_open_log = open_log->odl_mr_used;
					ASSERT_NS(!open_log->odl_lr_used);
				}
				else if (open_log->odl_lr_used)
					open_log->odl_lr_used->odl_mr_used = open_log->odl_mr_used;
				if (open_log->odl_mr_used)
					open_log->odl_mr_used->odl_lr_used = open_log->odl_lr_used;

				/* Make the file the most recently used: */
				if ((open_log->odl_lr_used = dlc_mru_open_log))
					dlc_mru_open_log->odl_mr_used = open_log;
				open_log->odl_mr_used = NULL;
				dlc_mru_open_log = open_log;
				if (!dlc_lru_open_log)
					dlc_lru_open_log = open_log;
			}
			xt_unlock_mutex_ns(&dlc_mru_lock);
		}
	}
	else {
		/* Create a new open file: */
		if (!(open_log = (XTOpenLogFilePtr) xt_calloc_ns(sizeof(XTOpenLogFileRec))))
			goto failed;
		dlc_name(PATH_MAX, path, log_id);
		if (!xt_open_file_ns(&open_log->odl_log_file, path, XT_FS_DEFAULT)) {
			xt_free_ns(open_log);
			goto failed;
		}
		open_log->olf_log_id = log_id;
		open_log->odl_data_log = data_log;
		data_log->dlf_open_count++;

		/* Make the new open file the most recently used: */
		xt_lock_mutex_ns(&dlc_mru_lock);
		open_log->odl_ru_time = ++dlc_ru_now;
		if ((open_log->odl_lr_used = dlc_mru_open_log))
			dlc_mru_open_log->odl_mr_used = open_log;
		open_log->odl_mr_used = NULL;
		dlc_mru_open_log = open_log;
		if (!dlc_lru_open_log)
			dlc_lru_open_log = open_log;
		dlc_open_count++;
		xt_unlock_mutex_ns(&dlc_mru_lock);
	}

	open_log->odl_in_use = TRUE;
	xt_unlock_mutex_ns(&seg->dls_lock);
	*ol = open_log;

	if (dlc_open_count > XT_DL_LOG_POOL_SIZE) {
		u_int	target = XT_DL_LOG_POOL_SIZE / 4 * 3;
		xtLogID	free_log_id;

		/* Remove some open files: */
		while (dlc_open_count > target) {
			XTOpenLogFilePtr to_free = dlc_lru_open_log;

			if (!to_free || to_free->odl_in_use)
				break;

			/* Dirty read the file ID: */
			free_log_id = to_free->olf_log_id;

			seg = &dlc_segment[free_log_id & XT_DL_SEGMENT_MASK];

			/* Lock the segment: */
			xt_lock_mutex_ns(&seg->dls_lock);

			/* Lock the MRU list: */
			xt_lock_mutex_ns(&dlc_mru_lock);

			/* Check if we have the same open file: */
			if (dlc_lru_open_log == to_free && !to_free->odl_in_use) {
				data_log = to_free->odl_data_log;
		
				/* Remove from the MRU list: */
				dlc_lru_open_log = to_free->odl_mr_used;
				ASSERT_NS(!to_free->odl_lr_used);

				if (dlc_mru_open_log == to_free) {
					dlc_mru_open_log = to_free->odl_lr_used;
					ASSERT_NS(!to_free->odl_mr_used);
				}
				else if (to_free->odl_mr_used)
					to_free->odl_mr_used->odl_lr_used = to_free->odl_lr_used;

				/* Remove from the free list of the file: */
				if (data_log->dlf_free_list == to_free) {
					data_log->dlf_free_list = to_free->odl_next_free;
					ASSERT_NS(!to_free->odl_prev_free);
				}
				else if (to_free->odl_prev_free)
					to_free->odl_prev_free->odl_next_free = to_free->odl_next_free;
				if (to_free->odl_next_free)
					to_free->odl_next_free->odl_prev_free = to_free->odl_prev_free;
				ASSERT_NS(data_log->dlf_open_count > 0);
				data_log->dlf_open_count--;
				dlc_open_count--;
			}
			else
				to_free = NULL;

			xt_unlock_mutex_ns(&dlc_mru_lock);
			xt_unlock_mutex_ns(&seg->dls_lock);

			if (to_free) {
				xt_close_file_ns(to_free->odl_log_file);
				xt_free_ns(to_free);
			}
		}
	}

	return OK;

	failed:
	xt_unlock_mutex_ns(&seg->dls_lock);
	return FAILED;
}

void XTDataLogCache::dlc_release_open_log(XTOpenLogFilePtr open_log)
{
	register XTDataLogSegPtr	seg;
	register XTDataLogFilePtr	data_log = open_log->odl_data_log;

	/* Which segment, and hash index: */
	seg = &dlc_segment[open_log->olf_log_id & XT_DL_SEGMENT_MASK];

	xt_lock_mutex_ns(&seg->dls_lock);
	open_log->odl_next_free = data_log->dlf_free_list;
	open_log->odl_prev_free = NULL;
	if (data_log->dlf_free_list)
		data_log->dlf_free_list->odl_prev_free = open_log;
	data_log->dlf_free_list = open_log;
	open_log->odl_in_use = FALSE;

	/* Wakeup any exclusive lockers: */
	if (!xt_broadcast_cond_ns(&seg->dls_cond))
		xt_log_and_clear_exception_ns();

	xt_unlock_mutex_ns(&seg->dls_lock);
}

/*
 * --------------------------------------------------------------------------------
 * D A T A   L O G   F I L E
 */

off_t XTDataLogFile::dlf_space_avaliable()
{
	if (dlf_log_eof < xt_db_data_log_threshold)
		return xt_db_data_log_threshold - dlf_log_eof;
	return 0;
}

xtBool XTDataLogFile::dlf_to_much_garbage()
{
	if (!dlf_log_eof)
		return FALSE;
	return dlf_garbage_count * 100 / dlf_log_eof >= xt_db_garbage_threshold;
}

/*
 * --------------------------------------------------------------------------------
 * D A T A   L O G   B U F F E R
 */

void XTDataLogBuffer::dlb_init(XTDatabaseHPtr db, size_t buffer_size)
{
	ASSERT_NS(!dlb_db);
	ASSERT_NS(!dlb_buffer_size);
	ASSERT_NS(!dlb_data_log);
	ASSERT_NS(!dlb_log_buffer);
	dlb_db = db;
	dlb_buffer_size = buffer_size;
}

void XTDataLogBuffer::dlb_exit(XTThreadPtr self)
{
	dlb_close_log(self);
	if (dlb_log_buffer) {
		xt_free(self, dlb_log_buffer);
		dlb_log_buffer = NULL;
	}
	dlb_db = NULL;
	dlb_buffer_offset = 0;
	dlb_buffer_size = 0;
	dlb_buffer_len = 0;
	dlb_flush_required = FALSE;
#ifdef DEBUG
	dlb_max_write_offset = 0;
#endif
}

xtBool XTDataLogBuffer::dlb_close_log(XTThreadPtr thread)
{
	if (dlb_data_log) {
		/* Flush and commit the data in the old log: */
		if (!dlb_flush_log(TRUE, thread))
			return FAILED;

		if (!dlb_db->db_datalogs.dlc_unlock_log(dlb_data_log))
			return FAILED;
		dlb_data_log = NULL;
	}
	return OK;
}

/* When I use 'thread' instead of 'self', this means
 * that I will not throw an error.
 */
xtBool XTDataLogBuffer::dlb_get_log_offset(xtLogID *log_id, xtLogOffset *out_offset, size_t req_size, struct XTThread *thread)
{
	/* Note, I am allowing a log to grow beyond the threshold.
	 * The amount depends on the maximum extended record size.
	 * If I don't some logs will never fill up, because of only having
	 * a few more bytes available.
	 */
	if (!dlb_data_log || dlb_data_log->dlf_space_avaliable() == 0) {
		/* Release the old log: */
		if (!dlb_close_log(thread))
			return FAILED;

		if (!dlb_log_buffer) {
			if (!(dlb_log_buffer = (xtWord1 *) xt_malloc_ns(dlb_buffer_size)))
				return FAILED;
		}

		/* I could use req_size instead of 1, but this would mean some logs
		 * are never filled up.
		 */
		if (!(dlb_data_log = dlb_db->db_datalogs.dlc_get_log_for_writing(1, thread)))
			return FAILED;
#ifdef DEBUG
		dlb_max_write_offset = dlb_data_log->dlf_log_eof;
#endif
	}

	*log_id = dlb_data_log->dlf_log_id;
	*out_offset = dlb_data_log->dlf_log_eof;
	return OK;
}

xtBool XTDataLogBuffer::dlb_flush_log(xtBool commit, XTThreadPtr thread)
{
	if (!dlb_data_log || !dlb_data_log->dlf_log_file)
		return OK;

	if (dlb_buffer_len) {
		if (!xt_pwrite_file(dlb_data_log->dlf_log_file, dlb_buffer_offset, dlb_buffer_len, dlb_log_buffer, &thread->st_statistics.st_data, thread))
			return FAILED;
#ifdef DEBUG
		if (dlb_buffer_offset + (xtLogOffset) dlb_buffer_len > dlb_max_write_offset)
			dlb_max_write_offset = dlb_buffer_offset + (xtLogOffset) dlb_buffer_len;
#endif
		dlb_buffer_len = 0;
		dlb_flush_required = TRUE;
	}

	if (commit && dlb_flush_required) {
#ifdef DEBUG
		/* This would normally be equal, however, in the case
		 * where some other thread flushes the compactors
		 * data log, the eof, can be greater than the
		 * write offset.
		 *
		 * This occurs because the flush can come between the 
		 * dlb_get_log_offset() and dlb_write_thru_log() calls.
		 */
		ASSERT_NS(dlb_data_log->dlf_log_eof >= dlb_max_write_offset);
#endif
		if (!xt_flush_file(dlb_data_log->dlf_log_file, &thread->st_statistics.st_data, thread))
			return FAILED;
		dlb_flush_required = FALSE;
	}
	return OK;
}

xtBool XTDataLogBuffer::dlb_write_thru_log(xtLogID XT_NDEBUG_UNUSED(log_id), xtLogOffset log_offset, size_t size, xtWord1 *data, XTThreadPtr thread)
{
	ASSERT_NS(log_id == dlb_data_log->dlf_log_id);

	if (dlb_buffer_len)
		dlb_flush_log(FALSE, thread);

	if (!xt_pwrite_file(dlb_data_log->dlf_log_file, log_offset, size, data, &thread->st_statistics.st_data, thread))
		return FAILED;
	/* Increment of dlb_data_log->dlf_log_eof was moved here from dlb_get_log_offset()
	 * to ensure it is done after a successful update of the log, otherwise otherwise a 
	 * gap occurs in the log which cause eof to be detected  in middle of the log
	 */
	dlb_data_log->dlf_log_eof += size;
#ifdef DEBUG
	if (log_offset + size > dlb_max_write_offset)
		dlb_max_write_offset = log_offset + size;
#endif
	dlb_flush_required = TRUE;
	return OK;
}

xtBool XTDataLogBuffer::dlb_append_log(xtLogID XT_NDEBUG_UNUSED(log_id), xtLogOffset log_offset, size_t size, xtWord1 *data, XTThreadPtr thread)
{
	ASSERT_NS(log_id == dlb_data_log->dlf_log_id);

	if (dlb_buffer_len) {
		/* Should be the case, we only write by appending: */
		ASSERT_NS(dlb_buffer_offset + (xtLogOffset) dlb_buffer_len == log_offset);
		/* Check if we are appending to the existing value in the buffer: */
		if (dlb_buffer_offset + (xtLogOffset) dlb_buffer_len == log_offset) {
			/* Can we just append: */
			if (dlb_buffer_size >= dlb_buffer_len + size) {
				memcpy(dlb_log_buffer + dlb_buffer_len, data, size);
				dlb_buffer_len += size;
				dlb_data_log->dlf_log_eof += size;
				return OK;
			}
		}
		if (dlb_flush_log(FALSE, thread) != OK)
			return FAILED;
	}
	
	ASSERT_NS(dlb_buffer_len == 0);
	
	if (dlb_buffer_size >= size) {
		dlb_buffer_offset = log_offset;
		dlb_buffer_len = size;
		memcpy(dlb_log_buffer, data, size);
		dlb_data_log->dlf_log_eof += size;
		return OK;
	}

	/* Write directly: */
	if (!xt_pwrite_file(dlb_data_log->dlf_log_file, log_offset, size, data, &thread->st_statistics.st_data, thread))
		return FAILED;
#ifdef DEBUG
	if (log_offset + size > dlb_max_write_offset)
		dlb_max_write_offset = log_offset + size;
#endif
	dlb_flush_required = TRUE;
	dlb_data_log->dlf_log_eof += size;
	return OK;
}

xtBool XTDataLogBuffer::dlb_read_log(xtLogID log_id, xtLogOffset log_offset, size_t size, xtWord1 *data, XTThreadPtr thread)
{
	size_t				red_size;
	XTOpenLogFilePtr	open_log;

	if (dlb_data_log && log_id == dlb_data_log->dlf_log_id) {
		/* Reading from the write log, I can do this quicker: */
		if (dlb_buffer_len) {
			/* If it is in the buffer, then it is completely in the buffer. */
			if (log_offset >= dlb_buffer_offset) {
				if (log_offset + (xtLogOffset) size <= dlb_buffer_offset + (xtLogOffset) dlb_buffer_len) {
					memcpy(data, dlb_log_buffer + (log_offset - dlb_buffer_offset), size);
					return OK;
				}
				/* Should not happen, reading past EOF: */
				ASSERT_NS(FALSE);
				memset(data, 0, size);
				return OK;
			}
			/* In the write log, but not in the buffer,
			 * must be completely not in the log,
			 * because only whole records are written to the
			 * log:
			 */
			ASSERT_NS(log_offset + (xtLogOffset) size <= dlb_buffer_offset);
		}		
		return xt_pread_file(dlb_data_log->dlf_log_file, log_offset, size, size, data, NULL, &thread->st_statistics.st_data, thread);
	}

	/* Read from some other log: */
	if (!dlb_db->db_datalogs.dlc_get_open_log(&open_log, log_id))
		return FAILED;

	if (!xt_pread_file(open_log->odl_log_file, log_offset, size, 0, data, &red_size, &thread->st_statistics.st_data, thread)) {
		dlb_db->db_datalogs.dlc_release_open_log(open_log);
		return FAILED;
	}

	dlb_db->db_datalogs.dlc_release_open_log(open_log);

	if (red_size < size)
		memset(data + red_size, 0, size - red_size);

	return OK;
}

/*
 * We assume that the given reference may not be valid.
 * Only valid references actually cause a delete.
 * Invalid references are logged, and ignored.
 *
 * Note this routine does not lock the compactor.
 * This can lead to the some incorrect calculation is the
 * amount of garbage. But nothing serious I think.
 */
xtBool XTDataLogBuffer::dlb_delete_log(xtLogID log_id, xtLogOffset log_offset, size_t size, xtTableID tab_id, xtRecordID rec_id, XTThreadPtr thread)
{
	XTactExtRecEntryDRec	record;
	xtWord1					status = XT_LOG_ENT_EXT_REC_DEL;
	XTOpenLogFilePtr		open_log;
	xtBool					to_much_garbage;
	XTDataLogFilePtr		data_log;

	if (!dlb_read_log(log_id, log_offset, offsetof(XTactExtRecEntryDRec, er_data), (xtWord1 *) &record, thread))
		return FAILED;

	/* Already deleted: */
	if (record.er_status_1 == XT_LOG_ENT_EXT_REC_DEL)
		return OK;

	if (record.er_status_1 != XT_LOG_ENT_EXT_REC_OK ||
		size != XT_GET_DISK_4(record.er_data_size_4) ||
		tab_id != XT_GET_DISK_4(record.er_tab_id_4) ||
		rec_id != XT_GET_DISK_4(record.er_rec_id_4)) {
		xt_register_xterr(XT_REG_CONTEXT, XT_ERR_BAD_EXT_RECORD);
		return FAILED;
	}

	if (dlb_data_log && log_id == dlb_data_log->dlf_log_id) {
		/* Writing to the write log, I can do this quicker: */
		if (dlb_buffer_len) {
			/* If it is in the buffer, then it is completely in the buffer. */
			if (log_offset >= dlb_buffer_offset) {
				if (log_offset + 1 <= dlb_buffer_offset + (xtLogOffset) dlb_buffer_len) {
					*(dlb_log_buffer + (log_offset - dlb_buffer_offset)) = XT_LOG_ENT_EXT_REC_DEL;
					goto inc_garbage_count;
				}
				/* Should not happen, writing past EOF: */
				ASSERT_NS(FALSE);
				return OK;
			}
			ASSERT_NS(log_offset + (xtLogOffset) size <= dlb_buffer_offset);
		}

		if (!xt_pwrite_file(dlb_data_log->dlf_log_file, log_offset, 1, &status, &thread->st_statistics.st_data, thread))
			return FAILED;
		
		inc_garbage_count:
		xt_lock_mutex_ns(&dlb_db->db_datalogs.dlc_head_lock);
		dlb_data_log->dlf_garbage_count += offsetof(XTactExtRecEntryDRec, er_data) + size;
		ASSERT_NS(dlb_data_log->dlf_garbage_count < dlb_data_log->dlf_log_eof);
		if (!dl_write_garbage_level(dlb_data_log, dlb_data_log->dlf_log_file, FALSE, thread)) {
			xt_unlock_mutex_ns(&dlb_db->db_datalogs.dlc_head_lock);
			return FAILED;
		}
		dlb_flush_required = TRUE;
		xt_unlock_mutex_ns(&dlb_db->db_datalogs.dlc_head_lock);
		return OK;
	}

	/* Write to some other log, open the log: */
	if (!dlb_db->db_datalogs.dlc_get_open_log(&open_log, log_id))
		return FAILED;

	/* Write the status byte: */
	if (!xt_pwrite_file(open_log->odl_log_file, log_offset, 1, &status, &thread->st_statistics.st_data, thread))
		goto failed;

	data_log = open_log->odl_data_log;

	/* Adjust the garbage level in the header. */
	xt_lock_mutex_ns(&dlb_db->db_datalogs.dlc_head_lock);
	data_log->dlf_garbage_count += offsetof(XTactExtRecEntryDRec, er_data) + size;
	ASSERT_NS(data_log->dlf_garbage_count < data_log->dlf_log_eof);
	if (!dl_write_garbage_level(data_log, open_log->odl_log_file, FALSE, thread)) {
		xt_unlock_mutex_ns(&dlb_db->db_datalogs.dlc_head_lock);
		goto failed;
	}
	to_much_garbage = data_log->dlf_to_much_garbage();
	xt_unlock_mutex_ns(&dlb_db->db_datalogs.dlc_head_lock);

	if (to_much_garbage &&
		(data_log->dlf_state == XT_DL_HAS_SPACE || data_log->dlf_state == XT_DL_READ_ONLY)) {
		/* There is too much garbage, it may be compacted. */
		if (!dlb_db->db_datalogs.dls_set_log_state(data_log, XT_DL_MAY_COMPACT))
			goto failed;
	}

	/* Release the open log: */
	dlb_db->db_datalogs.dlc_release_open_log(open_log);
	
	return OK;

	failed:
	dlb_db->db_datalogs.dlc_release_open_log(open_log);
	return FAILED;
}

/*
 * Delete all the extended data belonging to a particular
 * table.
 */
xtPublic void xt_dl_delete_ext_data(XTThreadPtr self, XTTableHPtr tab, xtBool XT_UNUSED(missing_ok), xtBool have_table_lock)
{
	XTOpenTablePtr	ot;
	xtRecordID		page_rec_id, offs_rec_id;
	XTTabRecExtDPtr	rec_buf;
	xtWord4			log_over_size;
	xtLogID			log_id;
	xtLogOffset		log_offset;
	xtWord1			*page_data;

	page_data = (xtWord1 *) xt_malloc(self, tab->tab_recs.tci_page_size);
	pushr_(xt_free, page_data);

	/* Scan the table, and remove all exended data... */
	if (!(ot = xt_open_table(tab))) {
		if (self->t_exception.e_xt_err == XT_SYSTEM_ERROR &&
			XT_FILE_NOT_FOUND(self->t_exception.e_sys_err))
			return;
		xt_throw(self);
	}
	ot->ot_thread = self;

	/* {LOCK-EXT-REC} This lock is to stop the compactor changing records 
	 * while we are doing the delete.
	 */
	xt_lock_mutex_ns(&tab->tab_db->db_co_ext_lock);

	page_rec_id = 1;
	while (page_rec_id < tab->tab_rec_eof_id) {
		/* NOTE: There is a good reason for using xt_tc_read_page().
		 * A deadlock can occur if using read, which can run out of
		 * memory, which waits for the freeer, which may need to
		 * open a table, which requires the db->db_tables lock,
		 * which is owned by the this thread, when the function
		 * is called from drop table.
		 *
		 * xt_tc_read_page() should work because no more changes
		 * should happen to the table while we are dropping it.
		 */
		if (!tab->tab_recs.xt_tc_read_page(ot->ot_rec_file, page_rec_id, page_data, self))
			goto failed;

		for (offs_rec_id=0; offs_rec_id<tab->tab_recs.tci_rows_per_page && page_rec_id+offs_rec_id < tab->tab_rec_eof_id; offs_rec_id++) {
			rec_buf = (XTTabRecExtDPtr) (page_data + (offs_rec_id * tab->tab_recs.tci_rec_size));
			if (XT_REC_IS_EXT_DLOG(rec_buf->tr_rec_type_1)) {
				log_over_size = XT_GET_DISK_4(rec_buf->re_log_dat_siz_4);
				XT_GET_LOG_REF(log_id, log_offset, rec_buf);

				if (!self->st_dlog_buf.dlb_delete_log(log_id, log_offset, log_over_size, tab->tab_id, page_rec_id+offs_rec_id, self)) {
					if (self->t_exception.e_xt_err != XT_ERR_BAD_EXT_RECORD &&
						self->t_exception.e_xt_err != XT_ERR_DATA_LOG_NOT_FOUND)
						xt_log_and_clear_exception(self);
				}
			}
		}

		page_rec_id += tab->tab_recs.tci_rows_per_page;
	}

	xt_unlock_mutex_ns(&tab->tab_db->db_co_ext_lock);

	xt_close_table(ot, TRUE, have_table_lock);
	
	freer_(); // xt_free(page_data)
	return;
	
	failed:
	xt_unlock_mutex_ns(&tab->tab_db->db_co_ext_lock);

	xt_close_table(ot, TRUE, have_table_lock);
	xt_throw(self);
}

/*
 * --------------------------------------------------------------------------------
 * GARBAGE COLLECTOR THREAD
 */

xtPublic void xt_dl_init_db(XTThreadPtr self, XTDatabaseHPtr db)
{
	xt_init_mutex_with_autoname(self, &db->db_co_ext_lock);
	xt_init_mutex_with_autoname(self, &db->db_co_dlog_lock);
}

xtPublic void xt_dl_exit_db(XTThreadPtr self, XTDatabaseHPtr db)
{
	xt_stop_compactor(self, db);	// Already done!
	db->db_co_thread = NULL;
	xt_free_mutex(&db->db_co_ext_lock);
	xt_free_mutex(&db->db_co_dlog_lock);
}

xtPublic void xt_dl_set_to_delete(XTThreadPtr self, XTDatabaseHPtr db, xtLogID log_id)
{
	XTDataLogFilePtr data_log;

	if (!db->db_datalogs.dlc_get_data_log(&data_log, log_id, FALSE, NULL))
		xt_throw(self);
	if (data_log) {
		if (!db->db_datalogs.dls_set_log_state(data_log, XT_DL_TO_DELETE))
			xt_throw(self);
	}
}

xtPublic void xt_dl_log_status(XTThreadPtr self, XTDatabaseHPtr db, XTStringBufferPtr strbuf)
{
	XTSortedListPtr		list;
	XTDataLogFilePtr	data_log;
	XTDataLogSegPtr		seg;
	u_int				no_of_logs;
	xtLogID				*log_id_ptr;

	list = xt_new_sortedlist(self, sizeof(xtLogID), 20, 10, dl_cmp_log_id, NULL, NULL, FALSE, FALSE);
	pushr_(xt_free_sortedlist, list);

	for (u_int i=0; i<XT_DL_NO_OF_SEGMENTS; i++) {
		for (u_int j=0; j<XT_DL_SEG_HASH_TABLE_SIZE; j++) {
			seg = &db->db_datalogs.dlc_segment[i];
			data_log = seg->dls_hash_table[j];
			while (data_log) {
				xt_sl_insert(self, list, &data_log->dlf_log_id, &data_log->dlf_log_id);
				data_log = data_log->dlf_next_hash;
			}
		}
	}

	no_of_logs = xt_sl_get_size(list);
	for (u_int i=0; i<no_of_logs; i++) {
		log_id_ptr = (xtLogID *) xt_sl_item_at(list, i);
		if (!db->db_datalogs.dlc_get_data_log(&data_log, *log_id_ptr, FALSE, &seg))
			xt_throw(self);
		if (data_log) {
			xt_sb_concat(self, strbuf, "d-log: ");
			xt_sb_concat_int8(self, strbuf, data_log->dlf_log_id);
			xt_sb_concat(self, strbuf, " status=");
			switch (data_log->dlf_state) {
				case XT_DL_UNKNOWN:
					xt_sb_concat(self, strbuf, "?");
					break;
				case XT_DL_HAS_SPACE:
					xt_sb_concat(self, strbuf, "has-space ");
					break;
				case XT_DL_READ_ONLY:
					xt_sb_concat(self, strbuf, "read-only ");
					break;
				case XT_DL_TO_COMPACT:
					xt_sb_concat(self, strbuf, "to-compact");
					break;
				case XT_DL_COMPACTED:
					xt_sb_concat(self, strbuf, "compacted ");
					break;
				case XT_DL_TO_DELETE:
					xt_sb_concat(self, strbuf, "to-delete ");
					break;
				case XT_DL_DELETED:
					xt_sb_concat(self, strbuf, "deleted   ");
					break;
				case XT_DL_EXCLUSIVE:
					xt_sb_concat(self, strbuf, "x-locked  ");
					break;
			}
			xt_sb_concat(self, strbuf, " eof=");
			xt_sb_concat_int8(self, strbuf, data_log->dlf_log_eof);
			xt_sb_concat(self, strbuf, " garbage=");
			xt_sb_concat_int8(self, strbuf, data_log->dlf_garbage_count);
			xt_sb_concat(self, strbuf, " g%=");
			if (data_log->dlf_log_eof)
				xt_sb_concat_int8(self, strbuf, data_log->dlf_garbage_count * 100 / data_log->dlf_log_eof);
			else
				xt_sb_concat(self, strbuf, "100");
			xt_sb_concat(self, strbuf, " open=");
			xt_sb_concat_int8(self, strbuf, data_log->dlf_open_count);
			xt_sb_concat(self, strbuf, "\n");
		}
		xt_unlock_mutex_ns(&seg->dls_lock);
	}

	freer_(); // xt_free_sortedlist(list)
}

xtPublic void xt_dl_delete_logs(XTThreadPtr self, XTDatabaseHPtr db)
{
	char			path[PATH_MAX];
	XTOpenDirPtr	od;
	char			*file;
	xtLogID			log_id;

	xt_strcpy(PATH_MAX, path, db->db_main_path);
	xt_add_data_dir(PATH_MAX, path);
	if (!xt_fs_exists(path))
		return;
	pushsr_(od, xt_dir_close, xt_dir_open(self, path, NULL));
	while (xt_dir_next(self, od)) {
		file = xt_dir_name(self, od);
		if ((log_id = (xtLogID) xt_file_name_to_id(file))) {
			if (!db->db_datalogs.dlc_remove_data_log(log_id, TRUE))
				xt_log_and_clear_exception(self);
		}
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

typedef struct XTCompactorState {
	XTSeqLogReadPtr			cs_seqread;
	XTOpenTablePtr			cs_ot;
	XTDataBufferRec			cs_databuf;
} XTCompactorStateRec, *XTCompactorStatePtr;

static void dl_free_compactor_state(XTThreadPtr self, XTCompactorStatePtr cs)
{
	if (cs->cs_seqread) {
		cs->cs_seqread->sl_seq_exit();
		delete cs->cs_seqread;
		cs->cs_seqread = NULL;
	}
	if (cs->cs_ot) {
		xt_db_return_table_to_pool(self, cs->cs_ot);
		cs->cs_ot = NULL;
	}
	xt_db_set_size(self, &cs->cs_databuf, 0);
}

static XTOpenTablePtr dl_cs_get_open_table(XTThreadPtr self, XTCompactorStatePtr cs, xtTableID tab_id)
{
	if (cs->cs_ot) {
		if (cs->cs_ot->ot_table->tab_id == tab_id)
			return cs->cs_ot;

		xt_db_return_table_to_pool(self, cs->cs_ot);
		cs->cs_ot = NULL;
	}

	if (!cs->cs_ot) {
		if (!(cs->cs_ot = xt_db_open_pool_table(self, self->st_database, tab_id, NULL, TRUE)))
			return NULL;
	}

	return cs->cs_ot;
}

static void dl_co_wait(XTThreadPtr self, XTDatabaseHPtr db, u_int secs)
{
	xt_lock_mutex(self, &db->db_datalogs.dlc_lock);
	pushr_(xt_unlock_mutex, &db->db_datalogs.dlc_lock);
	if (!self->t_quit)
		xt_timed_wait_cond(self, &db->db_datalogs.dlc_cond, &db->db_datalogs.dlc_lock, secs * 1000);
	freer_(); // xt_unlock_mutex(&db->db_datalogs.dlc_lock)
}

/*
 * Collect all the garbage in a file by moving all valid records
 * into some other data log and updating the handles.
 */
static xtBool dl_collect_garbage(XTThreadPtr self, XTDatabaseHPtr db, XTDataLogFilePtr data_log)
{
	XTXactLogBufferDPtr	record;
	size_t				size;
	xtTableID			tab_id;
	xtRecordID			rec_id;
	XTCompactorStateRec	cs;
	XTOpenTablePtr		ot;
	XTTableHPtr			tab;
	XTTabRecExtDRec		rec_buffer;
	size_t				src_size;
	xtLogID				src_log_id;
	xtLogOffset			src_log_offset;
	xtLogID				curr_log_id;
	xtLogOffset			curr_log_offset;
	xtLogID				dest_log_id;
	xtLogOffset			dest_log_offset;
	off_t				garbage_count = 0;

	memset(&cs, 0, sizeof(XTCompactorStateRec));

	if (!(cs.cs_seqread = new XTDataSeqRead()))
		xt_throw_errno(XT_CONTEXT, XT_ENOMEM);

	if (!cs.cs_seqread->sl_seq_init(db, xt_db_log_buffer_size)) {
		delete cs.cs_seqread;
		xt_throw(self);
	}
	pushr_(dl_free_compactor_state, &cs);

	if (!cs.cs_seqread->sl_seq_start(data_log->dlf_log_id, data_log->dlf_start_offset, FALSE))
		xt_throw(self);

	for (;;) {
		if (self->t_quit) {
			/* Flush the destination log: */
			xt_lock_mutex(self, &db->db_co_dlog_lock);
			pushr_(xt_unlock_mutex, &db->db_co_dlog_lock);
			if (!self->st_dlog_buf.dlb_flush_log(TRUE, self))
				xt_throw(self);
			freer_(); // xt_unlock_mutex(&db->db_co_dlog_lock)

			/* Flush the transaction log. */
			if (!xt_xlog_flush_log(self))
				xt_throw(self);

			xt_lock_mutex_ns(&db->db_datalogs.dlc_head_lock);
			data_log->dlf_garbage_count += garbage_count;
			ASSERT(data_log->dlf_garbage_count < data_log->dlf_log_eof);
			if (!dl_write_garbage_level(data_log, cs.cs_seqread->sl_seq_open_file(), TRUE, self)) {
				xt_unlock_mutex_ns(&db->db_datalogs.dlc_head_lock);
				xt_throw(self);
			}
			xt_unlock_mutex_ns(&db->db_datalogs.dlc_head_lock);

			freer_(); // dl_free_compactor_state(&cs)
			return FAILED;
		}
		if (!cs.cs_seqread->sl_seq_next(&record, self))
			xt_throw(self);
		cs.cs_seqread->sl_seq_pos(&curr_log_id, &curr_log_offset);
		if (!record) {
			data_log->dlf_start_offset = curr_log_offset;
			break;
		}
		switch (record->xh.xh_status_1) {
			case XT_LOG_ENT_EXT_REC_OK:
				size = XT_GET_DISK_4(record->er.er_data_size_4);
				tab_id = XT_GET_DISK_4(record->er.er_tab_id_4);
				rec_id = XT_GET_DISK_4(record->er.er_rec_id_4);
				
				if (!(ot = dl_cs_get_open_table(self, &cs, tab_id)))
					break;
				tab = ot->ot_table;
				
				/* All this is required for a valid record address: */
				if (!rec_id || rec_id >= tab->tab_rec_eof_id)
					break;

				/* {LOCK-EXT-REC} It is important to prevent the compactor from modifying
				 * a record that has been freed (and maybe allocated again).
				 *
				 * Consider the following sequence:
				 *
				 * 1. Compactor reads the record.
				 * 2. The record is freed and reallocated.
				 * 3. The compactor updates the record.
				 *
				 * To prevent this, the compactor locks out the
				 * sweeper using the db_co_ext_lock lock. The db_co_ext_lock lock
				 * prevents a extended record from being moved and removed at the
				 * same time.
				 *
				 * The compactor also checks the status of the record before
				 * moving a record.
				 */
				xt_lock_mutex(self, &db->db_co_ext_lock);
				pushr_(xt_unlock_mutex, &db->db_co_ext_lock);

				/* Read the record: */
				if (!xt_tab_get_rec_data(ot, rec_id, offsetof(XTTabRecExtDRec, re_data), (xtWord1 *) &rec_buffer)) {
					xt_log_and_clear_warning(self);
					freer_(); // xt_unlock_mutex(&db->db_co_ext_lockk)
					break;
				}

				/* [(7)] REMOVE is followed by FREE:
				if (XT_REC_IS_REMOVED(rec_buffer.tr_rec_type_1) || !XT_REC_IS_EXT_DLOG(rec_buffer.tr_rec_type_1)) {
				*/
				if (!XT_REC_IS_EXT_DLOG(rec_buffer.tr_rec_type_1)) {
					freer_(); // xt_unlock_mutex(&db->db_co_ext_lock)
					break;
				}

				XT_GET_LOG_REF(src_log_id, src_log_offset, &rec_buffer);
				src_size = (size_t) XT_GET_DISK_4(rec_buffer.re_log_dat_siz_4);

				/* Does the record agree with the current position: */
				if (curr_log_id != src_log_id ||
					curr_log_offset != src_log_offset ||
					size != src_size) {
					freer_(); // xt_unlock_mutex(&db->db_co_ext_lock)
					break;
				}

				size = offsetof(XTactExtRecEntryDRec, er_data) + size;

				/* Allocate space in a destination log: */
				xt_lock_mutex(self, &db->db_co_dlog_lock);
				pushr_(xt_unlock_mutex, &db->db_co_dlog_lock);
				if (!self->st_dlog_buf.dlb_get_log_offset(&dest_log_id, &dest_log_offset, size, self))
					xt_throw(self);
				freer_(); // xt_unlock_mutex(&db->db_co_dlog_lock)

				/* This record is referenced by the data: */
				xt_db_set_size(self, &cs.cs_databuf, size);
				if (!cs.cs_seqread->sl_rnd_read(src_log_offset, size, cs.cs_databuf.db_data, NULL, self))
					xt_throw(self);

				/* The problem with writing to the buffer here, is that other
				 * threads want to read the data! */
				xt_lock_mutex(self, &db->db_co_dlog_lock);
				pushr_(xt_unlock_mutex, &db->db_co_dlog_lock);
				if (!self->st_dlog_buf.dlb_write_thru_log(dest_log_id, dest_log_offset, size, cs.cs_databuf.db_data, self))
					xt_throw(self);
				freer_(); // xt_unlock_mutex(&db->db_co_dlog_lock)

				/* Make sure we flush the compactor target log, before we
				 * flush the transaction log!!
				 * This is done here [(8)]
				 */

				XT_SET_LOG_REF(&rec_buffer, dest_log_id, dest_log_offset);
				xtOpSeqNo op_seq;
				if (!xt_tab_put_log_rec_data(ot, XT_LOG_ENT_REC_MOVED, 0, rec_id, 8, (xtWord1 *) &rec_buffer.re_log_id_2, &op_seq))
					xt_throw(self);
				tab->tab_co_op_seq = op_seq;

				/* Only records that were actually moved, count as garbage now!
				 * This means, lost records, remain "lost" as far as the garbage
				 * count is concerned!
				 */
				garbage_count += size;
				freer_(); // xt_unlock_mutex(&db->db_co_ext_lock)
				break;
		}
		data_log->dlf_start_offset = curr_log_offset;
	}

	/* Flush the distination log. */
	xt_lock_mutex(self, &db->db_co_dlog_lock);
	pushr_(xt_unlock_mutex, &db->db_co_dlog_lock);
	if (!self->st_dlog_buf.dlb_flush_log(TRUE, self))
		xt_throw(self);
	freer_(); // xt_unlock_mutex(&db->db_co_dlog_lock)
	
	/* Flush the transaction log. */
	if (!xt_xlog_flush_log(self))
		xt_throw(self);

	/* Save state in source log header. */
	xt_lock_mutex_ns(&db->db_datalogs.dlc_head_lock);
	data_log->dlf_garbage_count += garbage_count;
	ASSERT(data_log->dlf_garbage_count < data_log->dlf_log_eof);
	if (!dl_write_garbage_level(data_log, cs.cs_seqread->sl_seq_open_file(), TRUE, self)) {
		xt_unlock_mutex_ns(&db->db_datalogs.dlc_head_lock);
		xt_throw(self);
	}
	xt_unlock_mutex_ns(&db->db_datalogs.dlc_head_lock);

	/* Wait for the writer to write all the changes.
	 * Then we can start the delete process for the log:
	 *
	 * Note, if we do not wait, then it could be some operations are held up,
	 * by being out of sequence. This could cause the log to be deleted
	 * before all the operations have been performed (which are on a table
	 * basis).
	 *
	 */
	for (;;) {
		u_int			edx;
		XTTableEntryPtr tab_ptr;
		xtBool			wait;

		if (self->t_quit) {
			freer_(); // dl_free_compactor_state(&cs)
			return FAILED;
		}
		wait = FALSE;
		xt_ht_lock(self, db->db_tables);
		pushr_(xt_ht_unlock, db->db_tables);
		xt_enum_tables_init(&edx);
		while ((tab_ptr = xt_enum_tables_next(self, db, &edx))) {
			if (tab_ptr->te_table && tab_ptr->te_table->tab_co_op_seq > tab_ptr->te_table->tab_head_op_seq) {
				wait = TRUE;
				break;
			}
		}
		freer_(); // xt_ht_unlock(db->db_tables)
		
		if (!wait)
			break;

		/* Nobody will wake me, so check again shortly! */
		dl_co_wait(self, db, 1);		
	}

	db->db_datalogs.dls_set_log_state(data_log, XT_DL_COMPACTED);

#ifdef DEBUG_LOG_DELETE
	printf("-- MARK FOR DELETE IN LOG: %d\n", (int) data_log->dlf_log_id);
#endif
	/* Log that this log should be deleted on the next checkpoint: */
	// transaction log...
	XTXactNewLogEntryDRec	log_rec;
	log_rec.xl_status_1 = XT_LOG_ENT_DEL_LOG;
	log_rec.xl_checksum_1 = XT_CHECKSUM_1(data_log->dlf_log_id);
	XT_SET_DISK_4(log_rec.xl_log_id_4, data_log->dlf_log_id);
	if (!xt_xlog_log_data(self, sizeof(XTXactNewLogEntryDRec), (XTXactLogBufferDPtr) &log_rec, TRUE)) {
		db->db_datalogs.dls_set_log_state(data_log, XT_DL_TO_COMPACT);
		xt_throw(self);
	}

	freer_(); // dl_free_compactor_state(&cs)
	return OK;
}

static void dl_co_not_busy(XTThreadPtr XT_UNUSED(self), XTDatabaseHPtr db)
{
	db->db_co_busy = FALSE;
}

static void dl_co_main(XTThreadPtr self, xtBool once_off)
{
	XTDatabaseHPtr		db = self->st_database;
	xtLogID				*log_id_ptr, log_id;
	XTDataLogFilePtr	data_log = NULL;

	xt_set_low_priority(self);

	while (!self->t_quit) {
		while (!self->t_quit) {
			xt_lock_mutex_ns(&db->db_datalogs.dlc_lock);
			if ((log_id_ptr = (xtLogID *) xt_sl_first_item(db->db_datalogs.dlc_to_compact))) {
				log_id = *log_id_ptr;
			}
			else
				log_id = 0;
			xt_unlock_mutex_ns(&db->db_datalogs.dlc_lock);
			if (!log_id)
				break;
			if (!db->db_datalogs.dlc_get_data_log(&data_log, log_id, FALSE, NULL))
				xt_throw(self);
			ASSERT(data_log);
			if (data_log) {
				db->db_co_busy = TRUE;
				pushr_(dl_co_not_busy, db);
				dl_collect_garbage(self, db, data_log);
				freer_(); // dl_co_not_busy(db)
			}
			else {
				xt_lock_mutex_ns(&db->db_datalogs.dlc_lock);
				xt_sl_delete(self, db->db_datalogs.dlc_to_compact, &log_id);
				xt_unlock_mutex_ns(&db->db_datalogs.dlc_lock);
			}
		}

		if (once_off)
			break;

		/* Wait for a signal that a data log can be collected: */
		dl_co_wait(self, db, 120);
	}
}

static void *dl_run_co_thread(XTThreadPtr self)
{
	XTDatabaseHPtr	db = (XTDatabaseHPtr) self->t_data;
	int				count;
	void			*mysql_thread;

	if (!(mysql_thread = myxt_create_thread()))
		xt_throw(self);

	while (!self->t_quit) {
		try_(a) {
			/*
			 * The garbage collector requires that the database
			 * is in use because.
			 */
			xt_use_database(self, db, XT_FOR_COMPACTOR);

			/* This action is both safe and required:
			 *
			 * safe: releasing the database is safe because as
			 * long as this thread is running the database
			 * reference is valid, and this reference cannot
			 * be the only one to the database because
			 * otherwize this thread would not be running.
			 *
			 * required: releasing the database is necessary
			 * otherwise we cannot close the database
			 * correctly because we only shutdown this
			 * thread when the database is closed and we
			 * only close the database when all references
			 * are removed.
			 */
			xt_heap_release(self, self->st_database);

			dl_co_main(self, FALSE);
		}
		catch_(a) {
			if (!(self->t_exception.e_xt_err == XT_SIGNAL_CAUGHT &&
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
		while (!self->t_quit && count > 0) {
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

static void dl_free_co_thread(XTThreadPtr self, void *data)
{
	XTDatabaseHPtr db = (XTDatabaseHPtr) data;

	if (db->db_co_thread) {
		xt_lock_mutex(self, &db->db_datalogs.dlc_lock);
		pushr_(xt_unlock_mutex, &db->db_datalogs.dlc_lock);
		db->db_co_thread = NULL;
		freer_(); // xt_unlock_mutex(&db->db_datalogs.dlc_lock)
	}
}

xtPublic void xt_start_compactor(XTThreadPtr self, XTDatabaseHPtr db)
{
	char name[PATH_MAX];

	sprintf(name, "GC-%s", xt_last_directory_of_path(db->db_main_path));
	xt_remove_dir_char(name);
	db->db_co_thread = xt_create_daemon(self, name);
	xt_set_thread_data(db->db_co_thread, db, dl_free_co_thread);
	xt_run_thread(self, db->db_co_thread, dl_run_co_thread);
}

static void dl_wake_co_thread(XTDatabaseHPtr db)
{
	if (!xt_signal_cond(NULL, &db->db_datalogs.dlc_cond))
		xt_log_and_clear_exception_ns();
}

xtPublic void xt_stop_compactor(XTThreadPtr self, XTDatabaseHPtr db)
{
	XTThreadPtr thr_co;

	if (db->db_co_thread) {
		xt_lock_mutex(self, &db->db_datalogs.dlc_lock);
		pushr_(xt_unlock_mutex, &db->db_datalogs.dlc_lock);

		/* This pointer is safe as long as you have the transaction lock. */
		if ((thr_co = db->db_co_thread)) {
			xtThreadID tid = thr_co->t_id;

			/* Make sure the thread quits when woken up. */
			xt_terminate_thread(self, thr_co);

			dl_wake_co_thread(db);
	
			freer_(); // xt_unlock_mutex(&db->db_datalogs.dlc_lock)

			/*
			 * This seems to kill the whole server sometimes!!
			 * SIGTERM is going to a different thread??!
			xt_kill_thread(thread);
			 */
			xt_wait_for_thread(tid, FALSE);
	
			/* PMC - This should not be necessary to set the signal here, but in the
			 * debugger the handler is not called!!?
			thr_co->t_delayed_signal = SIGTERM;
			xt_kill_thread(thread);
			 */
			db->db_co_thread = NULL;
		}
		else
			freer_(); // xt_unlock_mutex(&db->db_datalogs.dlc_lock)
	}
}

