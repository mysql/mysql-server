/*****************************************************************************

Copyright (c) 2012, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

****************************************************************************/

/**************************************************//**
@file handler_api.c

Created 3/14/2011 Jimmy Yang
*******************************************************/

#include "handler_api.h"
#include "log_event.h"

extern int write_bin_log(THD *thd, bool clear_error,
                  char const *query, ulong query_length,
                  bool is_trans= FALSE);

/**********************************************************************//**
Create a THD object.
@return a pointer to the THD object, NULL if failed */
void*
handler_create_thd(void)
/*====================*/
{
	THD*	thd;

	my_thread_init();
	thd = new THD;

	if (!thd) {
		return(NULL);
	}

	my_net_init(&thd->net,(st_vio*) 0);
	thd->thread_id= thd->variables.pseudo_thread_id= thread_id++;
	thd->thread_stack = (char*) &thd;
	thd->store_globals();

	thd->binlog_setup_trx_data();

	/* set binlog_format to "ROW" */
	thd->set_current_stmt_binlog_format_row();

	return(thd);
}

/**********************************************************************//**
Returns a MySQL "TABLE" object with specified database name and table name.
@return a pointer to the TABLE object, NULL if does not exist */
void*
handler_open_table(
/*===============*/
	void*		my_thd,		/*!< in: THD* */
	const char*	db_name,	/*!< in: database name */
	const char*	table_name,	/*!< in: table name */
	int		lock_type)	/*!< in: lock mode */
{
	TABLE_LIST              tables;
        TABLE*                  table = 0;
	THD*			thd = (THD*) my_thd;
	Open_table_context	ot_act(thd, 0);
	enum thr_lock_type	lock_mode;

	lock_mode = (lock_type < TL_IGNORE)
			? TL_READ
			: (enum thr_lock_type) lock_type;

	tables.init_one_table(db_name, strlen(db_name), table_name,
			      strlen(table_name), table_name, lock_mode);

	tables.mdl_request.init(MDL_key::TABLE, db_name, table_name,
				(lock_mode > TL_READ)
				? MDL_SHARED_WRITE
				: MDL_SHARED_READ, MDL_TRANSACTION);

	if (!open_table(thd, &tables, thd->mem_root, &ot_act)) {
		table = tables.table;
		table->use_all_columns();
		return(table);
	}

	return(NULL);
}

/**********************************************************************//**
Wrapper of function binlog_log_row() to binlog an operation on a row */
void
handler_binlog_row(
/*===============*/
	void*		my_table,	/*!< in: TABLE structure */
	int		mode)		/*!< in: type of DML */
{
	TABLE*		table = (TABLE*) my_table;
	Log_func	*log_func;


	if (mode == HDL_UPDATE) {
		assert(table->record[1]);
		log_func = Update_rows_log_event::binlog_row_logging_function;
		binlog_log_row(table, table->record[1], table->record[0],
			       log_func);
	} else if (mode == HDL_INSERT) {
		log_func = Write_rows_log_event::binlog_row_logging_function;
		binlog_log_row(table, 0, table->record[0], log_func);
	} else if (mode == HDL_DELETE) {
		log_func = Delete_rows_log_event::binlog_row_logging_function;
		binlog_log_row(table, table->record[0], 0, log_func);
	}
}

/**********************************************************************//**
Flush binlog from cache to binlog file */
void
handler_binlog_flush(
/*=================*/
	void*		my_thd,		/*!< in: THD* */
	void*		my_table)	/*!< in: TABLE structure */
{
	THD*		thd = (THD*) my_thd;
	int		ret;

	thd->binlog_write_table_map((TABLE*) my_table, 1, 0);
	ret = tc_log->log_xid(thd, 0);

	if (ret) {
		tc_log->unlog(ret, 0);
	}
	ret = trans_commit_stmt(thd);
}

/**********************************************************************//**
binlog a truncate table statement */
void
handler_binlog_truncate(
/*====================*/
	void*		my_thd,		/*!< in: THD* */
	char*		table_name)	/*!< in: table name */
{
	THD*		thd = (THD*) my_thd;
	char		query_str[NAME_LEN * 2 + 16];
	int		len;

	memset(query_str, 0, sizeof(query_str));

	strcpy(query_str,"truncate table ");
	strcat(query_str, table_name);

	len = strlen(query_str);

	write_bin_log(thd, 1, query_str, len);
}

/**********************************************************************//**
Reset TABLE->record[0] */
void
handler_rec_init(
/*=============*/
	void*		table)		/*!< in/out: TABLE structure */
{
	empty_record((TABLE*) table);
}

/**********************************************************************//**
Set up a char based field in TABLE->record[0] */
void
handler_rec_setup_str(
/*==================*/
	void*		my_table,	/*!< in/out: TABLE structure */
	int		field_id,	/*!< in: Field ID for the field */
	const char*	str,		/*!< in: string to set */
	int		len)		/*!< in: length of string */
{
	Field*		fld;
	TABLE*		table = (TABLE*) my_table;

	fld = table->field[field_id];

	if (len) {
		fld->store(str, len, &my_charset_bin);
		fld->set_notnull();
	} else {
		fld->set_null();
	}
}
/**********************************************************************//**
Set up an integer field in TABLE->record[0] */
void
handler_rec_setup_int(
/*==================*/
	void*		my_table,	/*!< in/out: TABLE structure */
	int		field_id,	/*!< in: Field ID for the field */
	int		value,		/*!< in: value to set */
	bool		unsigned_flag,	/*!< in: whether it is unsigned */
	bool		is_null)	/*!< in: whether it is null value */
{
	Field*		fld;
	TABLE*		table = (TABLE*) my_table;

	fld = table->field[field_id];

	if (is_null) {
		fld->set_null();
	} else {
		fld->set_notnull();
		fld->store(value, unsigned_flag);
	}
}
/**********************************************************************//**
copy an record */
void
handler_store_record(
/*=================*/
	void*		table)		/*!< in: TABLE */
{
	store_record((TABLE*) table, record[1]);
}
/**********************************************************************//**
close an handler */
void
handler_close_thd(
/*==============*/
	void*		my_thd)		/*!< in: thread */
{
	delete ((THD*) my_thd);

	/* don't have a THD anymore */
	my_pthread_setspecific_ptr(THR_THD,  0);
}

/**********************************************************************//**
Unlock a table and commit the transaction
return 0 if fail to commit the transaction */
int
handler_unlock_table(
/*=================*/
	void*		my_thd,		/*!< in: thread */
	void*		my_table,	/*!< in: Table metadata */
	int		mode)		/*!< in: mode */
{
	int			result;
	THD*			thd = (THD*) my_thd;
	TABLE*			table = (TABLE*) my_table;
	enum thr_lock_type	lock_mode;

	lock_mode = (mode & HDL_READ)
			? TL_READ
			: TL_WRITE;

	if (lock_mode == TL_WRITE) {
		query_cache_invalidate3(thd, table, 1);
		table->file->ha_release_auto_increment();
	}

	result = trans_commit_stmt(thd);

	if (thd->lock) {
		mysql_unlock_tables(thd, thd->lock);
	}

	close_mysql_tables(thd);
	thd->lock = 0;

	return(result);
}

/********************************************************************** 
Following APIs  can perform DMLs through MySQL handler interface. They 
are currently disabled and under HANDLER_API_MEMCACHED define 
**********************************************************************/

#ifdef HANDLER_API_MEMCACHED
/**********************************************************************//**
Search table for a record with particular search criteria
@return a pointer to table->record[0] */
uchar*
handler_select_rec(
/*===============*/
	THD*		thd,		/*!< in: thread */
	TABLE*		table,		/*!< in: TABLE structure */
	field_arg_t*	srch_args,	/*!< in: field to search */
	int		idx_to_use)	/*!< in: index to use */
{
	KEY*		key_info = &(table->key_info[0]);
	uchar*		srch_buf = (uchar*) malloc(
					key_info->key_length);
	size_t		total_key_len = 0;
	key_part_map	part_map;
	int		result;
	handler*	handle = table->file;

	assert(srch_args->num_arg <=  key_info->key_parts);

	for (unsigned int i = 0; i < key_info->key_parts; i ++) {
		KEY_PART_INFO*	key_part;
		int		srch_len;
		char*		srch_value;

		key_part = &key_info->key_part[i];

		if (i < srch_args->num_arg) {
			srch_value = srch_args->value[i];
			srch_len = srch_args->len[i];

			if (!srch_len) {
				key_part->field->set_null();
			} else {
				key_part->field->set_notnull();
				key_part->field->store(srch_value,
						       srch_len,
						       &my_charset_bin);
				total_key_len += key_part->store_length;
			}
		} else {
			key_part->field->set_null();
		}
	}

	key_copy(srch_buf, table->record[0], key_info, total_key_len);
	table->read_set = &table->s->all_set;
	part_map = (1 << srch_args->num_arg) - 1;

	handle->ha_index_or_rnd_end();
	handle->ha_index_init(idx_to_use, 1);

	result = handle->index_read_map(table->record[0], srch_buf,
					part_map, HA_READ_KEY_EXACT);

	free(srch_buf);

	if (!result) {
		return(table->record[0]);
	}

	return(NULL);
}
/**********************************************************************//**
Insert a record to the table
return 0 if successfully inserted */
int
handler_insert_rec(
/*===============*/
	THD*		thd,		/*!< in: thread */
	TABLE*		table,		/*!< in: TABLE structure */
	field_arg_t*	store_args)	/*!< in: inserting row data */
{
	uchar*		insert_buf;
	KEY*		key_info = &(table->key_info[0]);
	int		result;
	handler*	handle = table->file;

	empty_record(table);

	assert(table->reginfo.lock_type > TL_READ);

	insert_buf = table->record[0];
	memset(insert_buf, 0, table->s->null_bytes);

	assert(store_args->num_arg == key_info->key_parts);

	for (unsigned int ix = 0; ix < key_info->key_parts; ix++) {
		Field*		fld;

		fld = table->field[ix];
		if (store_args->len[ix]) {
			fld->store(store_args->value[ix],
				   store_args->len[ix], &my_charset_bin);
			fld->set_notnull();
		} else {
			fld->set_null();
		}
	}

	result = handle->ha_write_row((uchar *)table->record[0]);

	return(result);
}

/**********************************************************************//**
Update a record
return 0 if successfully inserted */
int
handler_update_rec(
/*===============*/
	THD*		thd,		/*!< in: thread */
	TABLE*		table,		/*!< in: TABLE structure */
	field_arg_t*	store_args)	/*!< in: update row data */
{
	int		result;
        uchar*		buf = table->record[0];
	handler*	handle = table->file;
	KEY*		key_info = &(table->key_info[0]);

        store_record(table, record[1]);

	for (unsigned int ix = 0; ix < key_info->key_parts; ix++) {
		Field*		fld;

		fld = table->field[ix];
		fld->store(store_args->value[ix],
			   store_args->len[ix], &my_charset_bin);
		fld->set_notnull();
	}

        result = handle->ha_update_row(table->record[1], buf);

	return(result);
}

/**********************************************************************//**
Delete a record
return 0 if successfully inserted */
int
handler_delete_rec(
/*===============*/
	THD*		thd,		/*!< in: thread */
	TABLE*		table)		/*!< in: TABLE structure */
{
	int		result;
	handler*	handle = table->file;

	result = handle->ha_delete_row(table->record[0]);

	return(result);
}

/**********************************************************************//**
Lock a table
return a lock structure pointer on success, NULL on error */
MYSQL_LOCK *
handler_lock_table(
/*===============*/
	THD*		thd,		/*!< in: thread */
	TABLE*		table,		/*!< in: Table metadata */
	enum thr_lock_type lock_mode)	/*!< in: lock mode */
{

	table->reginfo.lock_type = lock_mode;
	thd->lock = mysql_lock_tables(thd, &table, 1, 0);

	return(thd->lock);
}
#endif /* HANDLER_API_MEMCACHED */

