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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2006-06-07	Paul McCullagh
 *
 * H&G2JCtL
 *
 * This file contains PBXT streaming interface.
 */

#include "xt_config.h"

#ifdef XT_STREAMING
#include "ha_pbxt.h"

#include "thread_xt.h"
#include "strutil_xt.h"
#include "table_xt.h"
#include "myxt_xt.h"
#include "xaction_xt.h"
#include "database_xt.h"
#include "streaming_xt.h"

extern PBMSEngineRec pbxt_engine;

static PBMS_API pbxt_streaming;

/* ----------------------------------------------------------------------
 * INIT & EXIT
 */

xtPublic xtBool xt_init_streaming(void)
{
	XTThreadPtr				self = NULL;
	int						err;
	PBMSResultRec		result;

	if ((err = pbxt_streaming.registerEngine(&pbxt_engine, &result))) {
		xt_logf(XT_CONTEXT, XT_LOG_ERROR, "%s\n", result.mr_message);
		return FAILED;
	}
	return OK;
}

xtPublic void xt_exit_streaming(void)
{
	pbxt_streaming.deregisterEngine(&pbxt_engine);
}

/* ----------------------------------------------------------------------
 * UTILITY FUNCTIONS
 */

static void str_result_to_exception(XTExceptionPtr e, int r, PBMSResultPtr result)
{
	char *str, *end_str;

	e->e_xt_err = r;
	e->e_sys_err = result->mr_code;
	xt_strcpy(XT_ERR_MSG_SIZE, e->e_err_msg, result->mr_message);

	e->e_source_line = 0;
	str = result->mr_stack;
	if ((end_str = strchr(str, '('))) {
		xt_strcpy_term(XT_MAX_FUNC_NAME_SIZE, e->e_func_name, str, '(');
		str = end_str+1;
		if ((end_str = strchr(str, ':'))) {
			xt_strcpy_term(XT_SOURCE_FILE_NAME_SIZE, e->e_source_file, str, ':');
			str = end_str+1;
			if ((end_str = strchr(str, ')'))) {
				char number[40];
				
				xt_strcpy_term(40, number, str, ')');
				e->e_source_line = atol(number);
				str = end_str+1;
				if (*str == '\n')
					str++;
			}
		}
	}
	
	if (e->e_source_line == 0) {
		*e->e_func_name = 0;
		*e->e_source_file = 0;
		xt_strcpy(XT_ERR_MSG_SIZE, e->e_catch_trace, result->mr_stack);
	}
	else
		xt_strcpy(XT_ERR_MSG_SIZE, e->e_catch_trace, str);
}

static void str_exception_to_result(XTExceptionPtr e, PBMSResultPtr result)
{
	int len;

	if (e->e_sys_err)
		result->mr_code = e->e_sys_err;
	else
		result->mr_code = e->e_xt_err;
	xt_strcpy(MS_RESULT_MESSAGE_SIZE, result->mr_message, e->e_err_msg);
	xt_strcpy(MS_RESULT_STACK_SIZE, result->mr_stack, e->e_func_name);
	xt_strcat(MS_RESULT_STACK_SIZE, result->mr_stack, "(");
	xt_strcat(MS_RESULT_STACK_SIZE, result->mr_stack, e->e_source_file);
	xt_strcat(MS_RESULT_STACK_SIZE, result->mr_stack, ":");
	xt_strcati(MS_RESULT_STACK_SIZE, result->mr_stack, (int) e->e_source_line);
	xt_strcat(MS_RESULT_STACK_SIZE, result->mr_stack, ")");
	len = strlen(result->mr_stack);
	if (strncmp(result->mr_stack, e->e_catch_trace, len) == 0)
		xt_strcat(MS_RESULT_STACK_SIZE, result->mr_stack, e->e_catch_trace + len);
	else {
		xt_strcat(MS_RESULT_STACK_SIZE, result->mr_stack, "\n");
		xt_strcat(MS_RESULT_STACK_SIZE, result->mr_stack, e->e_catch_trace);
	}
}

static XTIndexPtr str_find_index(XTTableHPtr tab, u_int *col_list, u_int col_cnt)
{
	u_int			i, j;
	XTIndexPtr		*ind;					/* MySQL/PBXT key description */

	ind = tab->tab_dic.dic_keys;
	for (i=0; i<tab->tab_dic.dic_key_count; i++) {
		if ((*ind)->mi_seg_count == col_cnt) {
			for (j=0; j<(*ind)->mi_seg_count; j++) {
				if ((*ind)->mi_seg[j].col_idx != col_list[j])
					goto loop;
			}
			return *ind;
		}
		
		loop:
		ind++;
	}
	return NULL;
}

static XTThreadPtr str_set_current_thread(THD *thd, PBMSResultPtr result)
{
	XTThreadPtr		self;
	XTExceptionRec	e;

	if (!(self = xt_ha_set_current_thread(thd, &e))) {
		str_exception_to_result(&e, result);
		return NULL;
	}
	return self;
}

/* ----------------------------------------------------------------------
 * BLOB STREAMING INTERFACE
 */

static void pbxt_close_conn(void *thread)
{
	xt_ha_close_connection((THD *) thread);
}

static int pbxt_open_table(void *thread, const char *table_url, void **open_table, PBMSResultPtr result)
{
	THD				*thd = (THD *) thread;
	XTThreadPtr		self;
	XTTableHPtr		tab = NULL;
	XTOpenTablePtr	ot = NULL;
	int				err = MS_OK;

	if (!(self = str_set_current_thread(thd, result)))
		return MS_ERR_ENGINE;

	try_(a) {
		xt_ha_open_database_of_table(self, (XTPathStrPtr) table_url);
		if (!(tab = xt_use_table(self, (XTPathStrPtr) table_url, FALSE, TRUE, NULL))) {
			err = MS_ERR_UNKNOWN_TABLE;
			goto done;
		}
		if (!(ot = xt_open_table(tab)))
			throw_();
		ot->ot_thread = self;
		done:;
	}
	catch_(a) {
		str_exception_to_result(&self->t_exception, result);
		err = MS_ERR_ENGINE;
	}
	cont_(a);
	if (tab)
		xt_heap_release(self, tab);
	*open_table = ot;
	return err;
}

static void pbxt_close_table(void *thread, void *open_table_ptr)
{
	THD						*thd = (THD *) thread;
	volatile XTThreadPtr	self, new_self = NULL;
	XTOpenTablePtr			ot = (XTOpenTablePtr) open_table_ptr;
	XTExceptionRec			e;

	if (thd) {
		if (!(self = xt_ha_set_current_thread(thd, &e))) {
			xt_log_exception(NULL, &e, XT_LOG_DEFAULT);
			return;
		}
	}
	else if (!(self = xt_get_self())) {
		if (!(new_self = xt_create_thread("TempForClose", FALSE, TRUE, &e))) {
			xt_log_exception(NULL, &e, XT_LOG_DEFAULT);
			return;
		}
		self = new_self;
	}

	ot->ot_thread = self;
	try_(a) {
		xt_close_table(ot, TRUE, FALSE);
	}
	catch_(a) {
		xt_log_and_clear_exception(self);
	}
	cont_(a);
	if (new_self)
		xt_free_thread(self);
}

static int pbxt_lock_table(void *thread, int *xact, void *open_table, int lock_type, PBMSResultPtr result)
{
	THD				*thd = (THD *) thread;
	XTThreadPtr		self;
	XTOpenTablePtr	ot = (XTOpenTablePtr) open_table;
	int				err = MS_OK;

	if (!(self = str_set_current_thread(thd, result)))
		return MS_ERR_ENGINE;

	if (lock_type != MS_LOCK_NONE) {
		try_(a) {
			xt_ha_open_database_of_table(self, ot->ot_table->tab_name);
			ot->ot_thread = self;
		}
		catch_(a) {
			str_exception_to_result(&self->t_exception, result);
			err = MS_ERR_ENGINE;
		}
		cont_(a);
	}

	if (!err && *xact == MS_XACT_BEGIN) {
		if (self->st_xact_data)
			*xact = MS_XACT_NONE;
		else {
			if (xt_xn_begin(self)) {
				*xact = MS_XACT_COMMIT;
			}
			else {
				str_exception_to_result(&self->t_exception, result);
				err = MS_ERR_ENGINE;
			}
		}
	}

	return err;
}

static int pbxt_unlock_table(void *thread, int xact, void *XT_UNUSED(open_table), PBMSResultPtr result)
{
	THD				*thd = (THD *) thread;
	XTThreadPtr		self = xt_ha_thd_to_self(thd);
	int				err = MS_OK;

	if (xact == MS_XACT_COMMIT) {
		if (!xt_xn_commit(self)) {
			str_exception_to_result(&self->t_exception, result);
			err = MS_ERR_ENGINE;
		}
	}
	else if (xact == MS_XACT_ROLLBACK) {
		xt_xn_rollback(self);
	}

	return err;
}

static int pbxt_send_blob(void *thread, void *open_table, const char *blob_column, const char *blob_url_p, void *stream, PBMSResultPtr result)
{
	THD					*thd = (THD *) thread;
	XTThreadPtr			self = xt_ha_thd_to_self(thd);
	XTOpenTablePtr		ot = (XTOpenTablePtr) open_table;
	int					err = MS_OK;
	u_int				blob_col_idx, col_idx;
	char				col_name[XT_IDENTIFIER_NAME_SIZE];
	XTStringBufferRec	value;
	u_int				col_list[XT_MAX_COLS_PER_INDEX];
	u_int				col_cnt;
	char				col_names[XT_ERR_MSG_SIZE - 200];
	XTIdxSearchKeyRec	search_key;
	XTIndexPtr			ind;
	char				*blob_data;
	size_t				blob_len;
	const char			*blob_url = blob_url_p;

	memset(&value, 0, sizeof(value));

	*col_names = 0;

	ot->ot_thread = self;
	try_(a) {
		if (ot->ot_row_wbuf_size < ot->ot_table->tab_dic.dic_mysql_buf_size) {
			xt_realloc(self, (void **) &ot->ot_row_wbuffer, ot->ot_table->tab_dic.dic_mysql_buf_size);
			ot->ot_row_wbuf_size = ot->ot_table->tab_dic.dic_mysql_buf_size;
		}

		xt_strcpy_url(XT_IDENTIFIER_NAME_SIZE, col_name, blob_column);
		if (!myxt_find_column(ot, &blob_col_idx, col_name))
			xt_throw_tabcolerr(XT_CONTEXT, XT_ERR_COLUMN_NOT_FOUND, ot->ot_table->tab_name, blob_column);

		/* Prepare a row for the condition: */
		const char *ptr;

		col_cnt = 0;
		while (*blob_url) {
			ptr = xt_strchr(blob_url, '=');
			xt_strncpy_url(XT_IDENTIFIER_NAME_SIZE, col_name, blob_url, (size_t) (ptr - blob_url));
			if (!myxt_find_column(ot, &col_idx, col_name))
				xt_throw_tabcolerr(XT_CONTEXT, XT_ERR_COLUMN_NOT_FOUND, ot->ot_table->tab_name, col_name);
			if (*col_names)
				xt_strcat(sizeof(col_names), col_names, ", ");
			xt_strcat(sizeof(col_names), col_names, col_name);
			blob_url = ptr;
			if (*blob_url == '=')
				blob_url++;
			ptr = xt_strchr(blob_url, '&');
			value.sb_len = 0;
			xt_sb_concat_url_len(self, &value, blob_url, (size_t) (ptr - blob_url));
			blob_url = ptr;
			if (*blob_url == '&')
				blob_url++;
			if (!myxt_set_column(ot, (char *) ot->ot_row_rbuffer, col_idx, value.sb_cstring, value.sb_len))
				xt_throw_tabcolerr(XT_CONTEXT, XT_ERR_CONVERSION, ot->ot_table->tab_name, col_name);
			if (col_cnt < XT_MAX_COLS_PER_INDEX) {
				col_list[col_cnt] = col_idx;
				col_cnt++;
			}
		}

		/* Find a matching index: */		
		if (!(ind = str_find_index(ot->ot_table, col_list, col_cnt)))
			xt_throw_ixterr(XT_CONTEXT, XT_ERR_NO_MATCHING_INDEX, col_names);

		search_key.sk_key_value.sv_flags = 0;
		search_key.sk_key_value.sv_rec_id = 0;
		search_key.sk_key_value.sv_row_id = 0;
		search_key.sk_key_value.sv_key = search_key.sk_key_buf;
		search_key.sk_key_value.sv_length = myxt_create_key_from_row(ind, search_key.sk_key_buf, ot->ot_row_rbuffer, NULL);
		search_key.sk_on_key = FALSE;

		if (!xt_idx_search(ot, ind, &search_key))
			xt_throw(self);

		if (!ot->ot_curr_rec_id)
			xt_throw_taberr(XT_CONTEXT, XT_ERR_NO_ROWS, ot->ot_table->tab_name);
			
		while (ot->ot_curr_rec_id) {
			if (!search_key.sk_on_key)
				xt_throw_taberr(XT_CONTEXT, XT_ERR_NO_ROWS, ot->ot_table->tab_name);

			retry:
			/* X TODO - Check if the write buffer is big enough here! */
			switch (xt_tab_read_record(ot, ot->ot_row_wbuffer)) {
				case FALSE:
					if (xt_idx_next(ot, ind, &search_key))
						break;
				case XT_ERR:
					xt_throw(self);
				case XT_NEW:
					if (xt_idx_match_search(ot, ind, &search_key, ot->ot_row_wbuffer, XT_S_MODE_MATCH))
						goto success;
					if (!xt_idx_next(ot, ind, &search_key))
						xt_throw(self);
					break;
				case XT_RETRY:
					goto retry;
				default:
					goto success;
			}
		}

		success:
		myxt_get_column_data(ot, (char *) ot->ot_row_wbuffer, blob_col_idx, &blob_data, &blob_len);

		/* 
		 * Write the content length, then write the HTTP
		 * header, and then the content.
		 */
		err = pbxt_streaming.setContentLength(stream, blob_len, result);
		if (!err)
			err = pbxt_streaming.writeHead(stream, result);
		if (!err)
			err = pbxt_streaming.writeStream(stream, (void *) blob_data, blob_len, result);
	}
	catch_(a) {
		str_exception_to_result(&self->t_exception, result);
		if (result->mr_code == XT_ERR_NO_ROWS)
			err = MS_ERR_NOT_FOUND;
		else
			err = MS_ERR_ENGINE;
	}
	cont_(a);
	if (ot->ot_ind_rhandle) {
		xt_ind_release_handle(ot->ot_ind_rhandle, FALSE, self);
		ot->ot_ind_rhandle = NULL;
	}
	xt_sb_set_size(NULL, &value, 0);
	return err;
}

int pbxt_lookup_ref(void *thread, void *open_table, unsigned short col_index, PBMSEngineRefPtr eng_ref, PBMSFieldRefPtr field_ref, PBMSResultPtr result)
{
	THD				*thd = (THD *) thread;
	XTThreadPtr		self = xt_ha_thd_to_self(thd);
	XTOpenTablePtr	ot = (XTOpenTablePtr) open_table;
	int				err = MS_OK;
	u_int			i, len;
	char			*data;
	XTIndexPtr		ind = NULL;

	ot->ot_thread = self;
	if (ot->ot_row_wbuf_size < ot->ot_table->tab_dic.dic_mysql_buf_size) {
		xt_realloc(self, (void **) &ot->ot_row_wbuffer, ot->ot_table->tab_dic.dic_mysql_buf_size);
		ot->ot_row_wbuf_size = ot->ot_table->tab_dic.dic_mysql_buf_size;
	}

	ot->ot_curr_rec_id = (xtRecordID) XT_GET_DISK_8(eng_ref->er_data);
	switch (xt_tab_dirty_read_record(ot, ot->ot_row_wbuffer)) {
		case FALSE:
			err = MS_ERR_ENGINE;
			break;
		default:
			break;
	}

	if (err) {
		str_exception_to_result(&self->t_exception, result);
		goto exit;
	}

	myxt_get_column_name(ot, col_index, PBMS_FIELD_COL_SIZE, field_ref->fr_column);

	for (i=0; i<ot->ot_table->tab_dic.dic_key_count; i++) {
		ind = ot->ot_table->tab_dic.dic_keys[i];
		if (ind->mi_flags & (HA_UNIQUE_CHECK | HA_NOSAME))
			break; 
	}

	if (ind) {
		len = 0;
		data = field_ref->fr_cond;
		for (i=0; i<ind->mi_seg_count; i++) {
			if (i > 0) {
				xt_strcat(PBMS_FIELD_COND_SIZE, data, "&");
				len = strlen(data);
			}
			myxt_get_column_name(ot, ind->mi_seg[i].col_idx, PBMS_FIELD_COND_SIZE - len, data + len);
			len = strlen(data);
			xt_strcat(PBMS_FIELD_COND_SIZE, data, "=");
			len = strlen(data);
			myxt_get_column_as_string(ot, (char *) ot->ot_row_wbuffer, ind->mi_seg[i].col_idx, PBMS_FIELD_COND_SIZE - len, data + len);
			len = strlen(data);
		}
	}
	else
		xt_strcpy(PBMS_FIELD_COND_SIZE, field_ref->fr_cond, "*no unique key*");

	exit:
	return err;
}

PBMSEngineRec pbxt_engine = {
	MS_ENGINE_VERSION,
	0,
	FALSE,
	"PBXT",
	NULL,
	pbxt_close_conn,
	pbxt_open_table,
	pbxt_close_table,
	pbxt_lock_table,
	pbxt_unlock_table,
	pbxt_send_blob,
	pbxt_lookup_ref
};

/* ----------------------------------------------------------------------
 * CALL IN FUNCTIONS
 */

xtPublic void xt_pbms_close_all_tables(const char *table_url)
{
	pbxt_streaming.closeAllTables(table_url);
}

xtPublic xtBool xt_pbms_close_connection(void *thd, XTExceptionPtr e)
{
	PBMSResultRec	result;
	int				err;

	err = pbxt_streaming.closeConn(thd, &result);
	if (err) {
		str_result_to_exception(e, err, &result);
		return FAILED;
	}
	return OK;
}

xtPublic xtBool xt_pbms_open_table(void **open_table, char *table_path)
{
	PBMSResultRec	result;
	int				err;

	err = pbxt_streaming.openTable(open_table, table_path, &result);
	if (err) {
		XTThreadPtr	thread = xt_get_self();

		str_result_to_exception(&thread->t_exception, err, &result);
		return FAILED;
	}
	return OK;
}

xtPublic void xt_pbms_close_table(void *open_table)
{
	PBMSResultRec	result;
	int				err;

	err = pbxt_streaming.closeTable(open_table, &result);
	if (err) {
		XTThreadPtr	thread = xt_get_self();

		str_result_to_exception(&thread->t_exception, err, &result);
		xt_log_exception(thread, &thread->t_exception, XT_LOG_DEFAULT);
	}
}

xtPublic xtBool xt_pbms_use_blob(void *open_table, char **ret_blob_url, char *blob_url, unsigned short col_index)
{
	PBMSResultRec	result;
	int				err;

	err = pbxt_streaming.useBlob(open_table, ret_blob_url, blob_url, col_index, &result);
	if (err) {
		XTThreadPtr	thread = xt_get_self();

		str_result_to_exception(&thread->t_exception, err, &result);
		return FAILED;
	}
	return OK;
}

xtPublic xtBool xt_pbms_retain_blobs(void *open_table, PBMSEngineRefPtr eng_ref)
{
	PBMSResultRec	result;
	int				err;

	err = pbxt_streaming.retainBlobs(open_table, eng_ref, &result);
	if (err) {
		XTThreadPtr	thread = xt_get_self();

		str_result_to_exception(&thread->t_exception, err, &result);
		return FAILED;
	}
	return OK;
}

xtPublic void xt_pbms_release_blob(void *open_table, char *blob_url, unsigned short col_index, PBMSEngineRefPtr eng_ref)
{
	PBMSResultRec	result;
	int				err;

	err = pbxt_streaming.releaseBlob(open_table, blob_url, col_index, eng_ref, &result);
	if (err) {
		XTThreadPtr	thread = xt_get_self();

		str_result_to_exception(&thread->t_exception, err, &result);
		xt_log_exception(thread, &thread->t_exception, XT_LOG_DEFAULT);
	}
}

xtPublic void xt_pbms_drop_table(const char *table_path)
{
	PBMSResultRec	result;
	int				err;

	err = pbxt_streaming.dropTable(table_path, &result);
	if (err) {
		XTThreadPtr	thread = xt_get_self();

		str_result_to_exception(&thread->t_exception, err, &result);
		xt_log_exception(thread, &thread->t_exception, XT_LOG_DEFAULT);
	}
}

xtPublic void xt_pbms_rename_table(const char *from_table, const char *to_table)
{
	PBMSResultRec	result;
	int				err;

	err = pbxt_streaming.renameTable(from_table, to_table, &result);
	if (err) {
		XTThreadPtr	thread = xt_get_self();

		str_result_to_exception(&thread->t_exception, err, &result);
		xt_log_exception(thread, &thread->t_exception, XT_LOG_DEFAULT);
	}
}

#endif // XT_STREAMING
