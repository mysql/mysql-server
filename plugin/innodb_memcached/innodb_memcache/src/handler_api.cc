/*****************************************************************************

Copyright (c) 2013, Oracle and/or its affiliates. All Rights Reserved.

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

#include <my_global.h>
#include <sql_priv.h>
#include <stdlib.h>
#include <ctype.h>
#include <mysql_version.h>
#include <mysql/plugin.h>
#include <my_dir.h>
#include "my_pthread.h"
#include "my_sys.h"
#include "m_string.h"
#include "sql_plugin.h"
#include "table.h"
#include "sql_class.h"
#include <sql_base.h>
#include "key.h"
#include "lock.h"
#include "transaction.h"
#include "sql_handler.h"
#include "handler.h"

#include "log_event.h"
#include "innodb_config.h"
#include "binlog.h"

/** Some handler functions defined in sql/sql_table.cc and sql/handler.cc etc.
and being used here */
extern int write_bin_log(THD *thd, bool clear_error,
			 char const *query, ulong query_length,
			 bool is_trans= false);

/** function to close a connection and thd, defined in sql/handler.cc */
extern void ha_close_connection(THD* thd);

/** binlog a row operation */
extern int binlog_log_row(TABLE*          table,
			  const uchar     *before_record,
			  const uchar     *after_record,
			  Log_func*       log_func);

/**********************************************************************//**
Create a THD object.
@return a pointer to the THD object, NULL if failed */
void*
handler_create_thd(
/*===============*/
	bool	enable_binlog)		/*!< in: whether to enable binlog */
{
	THD*	thd;

	if (enable_binlog && !binlog_enabled()) {
		fprintf(stderr, "  InnoDB_Memcached: MySQL server"
			 	" binlog not enabled\n");
		return(NULL);
	}

	my_thread_init();
	thd = new THD;

	if (!thd) {
		return(NULL);
	}

	my_net_init(&thd->net,(st_vio*) 0);
	thd->variables.pseudo_thread_id = thread_id++;
	thd->thread_id = thd->variables.pseudo_thread_id;
	thd->thread_stack = reinterpret_cast<char*>(&thd);
	thd->store_globals();

	if (enable_binlog) {
		thd->binlog_setup_trx_data();

		/* set binlog_format to "ROW" */
		thd->set_current_stmt_binlog_format_row();
	}

	return(thd);
}

/**********************************************************************//**
This is used to temporarily switch to another session, so that
POSIX thread looks like session attached to */
void
handler_thd_attach(
/*===============*/
	void*	my_thd,		/*!< in: THD* */
	void**	original_thd)	/*!< out: current THD */
{
	THD*	thd = static_cast<THD*>(my_thd);

	if (original_thd) {
		*original_thd = my_pthread_getspecific(THD*, THR_THD);
		assert(thd->mysys_var);
	}

	my_pthread_setspecific_ptr(THR_THD, thd);
	my_pthread_setspecific_ptr(THR_MALLOC, &thd->mem_root);
	set_mysys_var(thd->mysys_var);
}

/**********************************************************************//**
Returns a MySQL "TABLE" object with specified database name and table name.
@return a pointer to the TABLE object, NULL if does not exist */
void*
handler_open_table(
/*===============*/
	void*		my_thd,		/*!< in: THD* */
	const char*	db_name,	/*!< in: NUL terminated database name */
	const char*	table_name,	/*!< in: NUL terminated table name */
	int		lock_type)	/*!< in: lock mode */
{
	TABLE_LIST              tables;
	THD*			thd = static_cast<THD*>(my_thd);
	Open_table_context	table_ctx(thd, 0);
	thr_lock_type		lock_mode;

	lock_mode = (lock_type <= HDL_READ)
		     ? TL_READ
		     : TL_WRITE;

	tables.init_one_table(db_name, strlen(db_name), table_name,
			      strlen(table_name), table_name, lock_mode);

	tables.mdl_request.init(MDL_key::TABLE, db_name, table_name,
				(lock_mode > TL_READ)
				? MDL_SHARED_WRITE
				: MDL_SHARED_READ, MDL_TRANSACTION);

	if (!open_table(thd, &tables, &table_ctx)) {
		TABLE*	table = tables.table;
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
	void*		my_thd,		/*!< in: THD* */
	void*		my_table,	/*!< in: TABLE structure */
	int		mode)		/*!< in: type of DML */
{
	TABLE*		table = static_cast<TABLE*>(my_table);
	THD*		thd = static_cast<THD*>(my_thd);
	Log_func*	log_func;

	if (thd->get_binlog_table_maps() == 0) {
		/* Write the table map and BEGIN mark */
		thd->binlog_write_table_map(
			static_cast<TABLE*>(my_table), 1, 0);
	}

	switch (mode) {
	case HDL_UPDATE:
		/* Updated record must be allocated and filled */
		assert(table->record[1]);
		log_func = Update_rows_log_event::binlog_row_logging_function;
		binlog_log_row(table, table->record[1], table->record[0],
			       log_func);
		break;
	case HDL_INSERT:
		log_func = Write_rows_log_event::binlog_row_logging_function;
		binlog_log_row(table, 0, table->record[0], log_func);
		break;
	case HDL_DELETE:
		log_func = Delete_rows_log_event::binlog_row_logging_function;
		binlog_log_row(table, table->record[0], 0, log_func);
		break;
	default:
		assert(0);
	}
}

/**********************************************************************//**
Commit and flush binlog from cache to binlog file */
void
handler_binlog_commit(
/*==================*/
	void*		my_thd,		/*!< in: THD* */
	void*		my_table)	/*!< in: TABLE structure */
{
	THD*		thd = static_cast<THD*>(my_thd);

	if (tc_log) {
		tc_log->commit(thd, true);
	}
	trans_commit_stmt(thd);
}

/**********************************************************************//**
Rollback a transaction */
void
handler_binlog_rollback(
/*====================*/
	void*		my_thd,		/*!< in: THD* */
	void*		my_table)	/*!< in: TABLE structure */
{
	THD*		thd = static_cast<THD*>(my_thd);

	/*
	  Memcached plugin doesn't use thd_mark_transaction_to_rollback()
	  on deadlocks. So no special handling for this flag is needed.
	*/
	if (tc_log) {
		tc_log->rollback(thd, true);
	}
	trans_rollback_stmt(thd);
}

/**********************************************************************//**
Binlog a truncate table statement */
void
handler_binlog_truncate(
/*====================*/
	void*		my_thd,		/*!< in: THD* */
	char*		table_name)	/*!< in: table name */
{
	THD*		thd = (THD*) my_thd;
	char		query_str[MAX_FULL_NAME_LEN + 16];
	int		len;

	memset(query_str, 0, sizeof(query_str));

	assert(strlen(table_name) < MAX_FULL_NAME_LEN);

	snprintf(query_str, MAX_FULL_NAME_LEN + 16, "%s %s",
		 "truncate table", table_name);

	len = strlen(query_str);

	write_bin_log(thd, 1, query_str, len);
}

/**********************************************************************//**
Reset TABLE->record[0] */
void
handler_rec_init(
/*=============*/
	void*		my_table)	/*!< in/out: TABLE structure */
{
	empty_record(static_cast<TABLE*>(my_table));
}

/**********************************************************************//**
Store a string in TABLE->record[0] for field specified by "field_id" */
void
handler_rec_setup_str(
/*==================*/
	void*		my_table,	/*!< in/out: TABLE structure */
	int		field_id,	/*!< in: Field ID for the field */
	const char*	str,		/*!< in: string to set */
	int		len)		/*!< in: length of string */
{
	Field*		fld;
	TABLE*		table = static_cast<TABLE*>(my_table);

	fld = table->field[field_id];

	assert(len >= 0);

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
	TABLE*		table = static_cast<TABLE*>(my_table);

	fld = table->field[field_id];

	if (is_null) {
		fld->set_null();
	} else {
		fld->set_notnull();
		fld->store(value, unsigned_flag);
	}
}

/**********************************************************************//**
Set up an unsigned int64 field in TABLE->record[0] */
void
handler_rec_setup_uint64(
/*=====================*/
	void*		my_table,	/*!< in/out: TABLE structure */
	int		field_id,	/*!< in: Field ID for the field */
	unsigned long long
			value,		/*!< in: value to set */
	bool		unsigned_flag,	/*!< in: whether it is unsigned */
	bool		is_null)	/*!< in: whether it is null value */
{
	Field*		fld;
	TABLE*		table = static_cast<TABLE*>(my_table);

	fld = table->field[field_id];

	if (is_null) {
		fld->set_null();
	} else {
		fld->set_notnull();
		fld->store(value, unsigned_flag);
	}
}

/**********************************************************************//**
Store a record */
void
handler_store_record(
/*=================*/
	void*		my_table)	/*!< in: TABLE */
{
	store_record(static_cast<TABLE*>(my_table), record[1]);
}

/**********************************************************************//**
Close the handler */
void
handler_close_thd(
/*==============*/
	void*		my_thd)		/*!< in: THD */
{
	THD*	thd = static_cast<THD*>(my_thd);

	/* destructor will not free it, because net.vio is 0. */
	net_end(&thd->net);

	thd->release_resources();
	delete (thd);

	/* Don't have a THD anymore */
	my_pthread_setspecific_ptr(THR_THD,  0);
}

/**********************************************************************//**
Unlock a table and commit the transaction
return 0 if failed to commit the transaction */
int
handler_unlock_table(
/*=================*/
	void*		my_thd,		/*!< in: thread */
	void*		my_table,	/*!< in: Table metadata */
	int		mode)		/*!< in: mode */
{
	int			result;
	THD*			thd = static_cast<THD*>(my_thd);
	TABLE*			table = static_cast<TABLE*>(my_table);
	thr_lock_type		lock_mode;

	lock_mode = (mode & HDL_READ) ? TL_READ : TL_WRITE;

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
Following APIs can perform DMLs through MySQL handler interface. They
are currently disabled and under HANDLER_API_MEMCACHED define
**********************************************************************/

#ifdef HANDLER_API_MEMCACHED
/**********************************************************************//**
Search table for a record with particular search criteria
@return a pointer to table->record[0] */
uchar*
handler_select_rec(
/*===============*/
	TABLE*		my_table,	/*!< in: TABLE structure */
	field_arg_t*	srch_args,	/*!< in: field to search */
	int		idx_to_use)	/*!< in: index to use */
{
	KEY*		key_info = &my_table->key_info[0];
	uchar*		srch_buf = new uchar[key_info->key_length];
	size_t		total_key_len = 0;
	key_part_map	part_map;
	int		result;
	handler*	handle = my_table->file;

	assert(srch_args->num_arg <= key_info->key_parts);

	for (unsigned int i = 0; i < key_info->key_parts; i++) {
		KEY_PART_INFO*	key_part;

		key_part = &key_info->key_part[i];

		if (i < srch_args->num_arg) {
			int	srch_len = srch_args->len[i];

			assert(srch_len >= 0);

			if (srch_len != 0) {
				key_part->field->set_null();
			} else {
				char*	srch_value = srch_args->value[i];
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

	assert(key_info->key_length >= total_key_len);

	key_copy(srch_buf, my_table->record[0], key_info, total_key_len);
	my_table->read_set = &my_table->s->all_set;

	/* Max column supported is 4096 */
	assert(srch_args->num_arg <= 4096);

	part_map = (1 << srch_args->num_arg) - 1;

	handle->ha_index_or_rnd_end();
	handle->ha_index_init(idx_to_use, 1);

	result = handle->index_read_map(my_table->record[0], srch_buf,
					part_map, HA_READ_KEY_EXACT);

	delete[] srch_buf;

	if (!result) {
		return(my_table->record[0]);
	}

	return(NULL);
}

/**********************************************************************//**
Insert a record to the table
return 0 if successfully inserted */
int
handler_insert_rec(
/*===============*/
	TABLE*		my_table,	/*!< in: TABLE structure */
	field_arg_t*	store_args)	/*!< in: inserting row data */
{
	uchar*		insert_buf;
	KEY*		key_info = &(table->key_info[0]);
	handler*	handle = my_table->file;

	empty_record(my_table);

	assert(table->reginfo.lock_type > TL_READ
	       && table->reginfo.lock_type <= TL_WRITE_ONLY);

	insert_buf = my_table->record[0];
	memset(insert_buf, 0, my_table->s->null_bytes);

	assert(store_args->num_arg == key_info->key_parts);

	for (unsigned int i = 0; i < key_info->key_parts; i++) {
		Field*		fld;

		fld = table->field[i];
		if (store_args->len[i]) {
			fld->store(store_args->value[i],
				   store_args->len[i], &my_charset_bin);
			fld->set_notnull();
		} else {
			fld->set_null();
		}
	}

	return(handle->ha_write_row((uchar *)my_table->record[0]));
}

/**********************************************************************//**
Update a record
return 0 if successfully inserted */
int
handler_update_rec(
/*===============*/
	TABLE*		my_table,	/*!< in: TABLE structure */
	field_arg_t*	store_args)	/*!< in: update row data */
{
        uchar*		buf = my_table->record[0];
	handler*	handle = my_table->file;
	KEY*		key_info = &my_table->key_info[0];

        store_record(my_table, record[1]);

	for (unsigned int i = 0; i < key_info->key_parts; i++) {
		Field*		fld;

		fld = my_table->field[i];
		fld->store(store_args->value[i],
			   store_args->len[i], &my_charset_bin);
		fld->set_notnull();
	}

	return(handle->ha_update_row(my_table->record[1], buf));
}

/**********************************************************************//**
Delete a record
return 0 if successfully inserted */
int
handler_delete_rec(
/*===============*/
	TABLE*		my_table)	/*!< in: TABLE structure */
{
	return(my_table->file->ha_delete_row(my_table->record[0]));
}

/**********************************************************************//**
Lock a table
return a lock structure pointer on success, NULL on error */
MYSQL_LOCK *
handler_lock_table(
/*===============*/
	THD*		my_thd,		/*!< in: thread */
	TABLE*		my_table,	/*!< in: Table metadata */
	thr_lock_type	lock_mode)	/*!< in: lock mode */
{

	my_table->reginfo.lock_type = lock_mode;
	my_thd->lock = mysql_lock_tables(my_thd, &my_table, 1, 0);

	return(my_thd->lock);
}
#endif /* HANDLER_API_MEMCACHED */
