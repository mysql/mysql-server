/***********************************************************************

Copyright (c) 2011, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

***********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "innodb_api.h"
#include <errno.h>
#include "memcached/util.h"


/*********************************************************************
Open a table and return a cursor for the table. */
ib_err_t
innodb_api_begin(
/*=============*/
	innodb_engine_t*engine,		/*!< in: InnoDB Memcached engine */
	const char*	dbname,		/*!< in: database name */
	const char*	name,		/*!< in: table name */
	ib_trx_t	ib_trx,		/*!< in: transaction */
	ib_crsr_t*	crsr,		/*!< in/out: innodb cursor */
	ib_crsr_t*	idx_crsr,	/*!< in/out: innodb index cursor */
	ib_lck_mode_t	lock_mode)	/*!< in:  lock mode */
{
	ib_err_t	err = DB_SUCCESS;
	char		table_name[MAX_TABLE_NAME_LEN + MAX_DATABASE_NAME_LEN];

	if (!(*crsr)) {
#ifdef __WIN__
		sprintf(table_name, "%s\%s", dbname, name);
#else
		snprintf(table_name, sizeof(table_name),
			 "%s/%s", dbname, name);
#endif

		err  = ib_cb_open_table(table_name, ib_trx, crsr);

		if (err != DB_SUCCESS) {
			fprintf(stderr, "  InnoDB_Memcached: Unable open"
					" table '%s'\n", table_name);
			return(err);
		}

		ib_cb_cursor_lock(*crsr, lock_mode);

		if (engine) {
			meta_info_t*	meta_info = &engine->meta_info;
			meta_index_t*	meta_index = &meta_info->m_index;
			if (meta_index->m_use_idx == META_SECONDARY) {
				int	index_type;
				ib_id_t	index_id;

				ib_cb_cursor_open_index_using_name(
					*crsr, meta_index->m_name,
					idx_crsr, &index_type, &index_id);

				ib_cb_cursor_lock(*idx_crsr, lock_mode);
			}
		}
	} else {
		ib_cb_cursor_new_trx(*crsr, ib_trx);
		ib_cb_cursor_lock(*crsr, lock_mode);

		if (engine) {
			meta_info_t*	meta_info = &engine->meta_info;
			meta_index_t*	meta_index = &meta_info->m_index;
			if (meta_index->m_use_idx == META_SECONDARY) {
				ib_cb_cursor_new_trx(*idx_crsr, ib_trx);
				ib_cb_cursor_lock(*idx_crsr, lock_mode);
			}
		}
	}
		
	return(err);
}

/*************************************************************//**
Position a row accord to key, and fetch value if needed
@return DB_SUCCESS if successful otherwise, error code */
static
uint64_t
innodb_api_read_int(
/*================*/
	ib_col_meta_t*	m_col,
	ib_tpl_t        read_tpl,
	int		i)
{
	uint64_t	value = 0;

	assert (m_col->type == IB_INT);
	assert (m_col->type_len == 8 || m_col->type_len == 4);

	if (m_col->attr == IB_COL_UNSIGNED) {
		if (m_col->type_len == 8) {
			ib_cb_tuple_read_u64(read_tpl, i, &value);
		} else if (m_col->type_len == 4) {
			uint32_t	value32;
			ib_cb_tuple_read_u32(read_tpl, i, &value32);
			value = (uint64_t) value32;
		}
	} else {
		if (m_col->type_len == 8) {
			int64_t		value64;
			ib_cb_tuple_read_i64(read_tpl, i, &value64);
			value = (uint64_t) value64;
		} else if (m_col->type_len == 4) {
			ib_i32_t	value32;
			ib_cb_tuple_read_i32(read_tpl, i, &value32);
			value = (uint64_t) value32;
		}
	}

	return(value);
}

/*************************************************************//**
Position a row accord to key, and fetch value if needed
@return DB_SUCCESS if successful otherwise, error code */
static
ib_err_t
innodb_api_write_int(
/*=================*/
	ib_tpl_t        tpl,
	int		field,
	uint64_t	value)
{
	ib_col_meta_t   col_meta;
	ib_col_meta_t*	m_col = &col_meta;
	int		data_len;

	data_len = ib_cb_col_get_meta(tpl, field, m_col);

	assert (m_col->type == IB_INT);
	assert (m_col->type_len == 8 || m_col->type_len == 4);

	if (m_col->attr == IB_COL_UNSIGNED) {
		if (m_col->type_len == 8) {
			ib_cb_tuple_write_u64(tpl, field, value);
		} else if (m_col->type_len == 4) {
			uint32_t	value32;
			value32 = (uint32_t) value;
			
			ib_cb_tuple_write_u32(tpl, field, value32);
		}
	} else {
		if (m_col->type_len == 8) {
			int64_t		value64;
			value64 = (int64_t) value;

			ib_cb_tuple_write_i64(tpl, field, value64);
		} else if (m_col->type_len == 4) {
			int32_t		value32;
			value32 = (int32_t) value;
			ib_cb_tuple_write_i32(tpl, field, value32);
		}
	}

	return(DB_SUCCESS);
}

static
bool
innodb_api_fill_mci(
/*================*/
	ib_tpl_t	read_tpl,
	int		col_id,
	mci_column_t*	mci_item)
{
	ib_ulint_t      data_len;
	ib_col_meta_t   col_meta;

	data_len = ib_cb_col_get_meta(read_tpl, col_id, &col_meta);

	if (data_len == IB_SQL_NULL) {
		mci_item->m_str = NULL;
		mci_item->m_len = 0;
	} else {
		mci_item->m_str = (char*)ib_cb_col_get_value(read_tpl, col_id);
		mci_item->m_len = data_len;
	}

	mci_item->m_is_str = TRUE;
	mci_item->m_enabled = TRUE;

	return(TRUE);
}

/*************************************************************//**
Position a row accord to key, and fetch value if needed
@return DB_SUCCESS if successful otherwise, error code */
static
ib_err_t
innodb_api_fill_value(
/*==================*/
	meta_info_t*	meta_info, 
	mci_item_t*	item,		/*!< in: result */
	ib_tpl_t	read_tpl,
	int		col_id)
{
	ib_err_t	err = DB_NOT_FOUND;

	if (meta_info->m_num_add == 0) {
		meta_column_t*	col_info = meta_info->m_item;

		if (col_id == col_info[META_VALUE].m_field_id) {

			innodb_api_fill_mci(read_tpl, col_id,
					    &item->mci_item[MCI_COL_VALUE]);
			err = DB_SUCCESS;
		}
	} else {
		int	i;
		for (i = 0; i < meta_info->m_num_add; i++) {
			if (col_id == meta_info->m_add_item[i].m_field_id) {
				innodb_api_fill_mci(read_tpl, col_id,
						    &item->mci_add_value[i]);
				err = DB_SUCCESS;
				break;
			}
		}
	}

	return(err);
}

/*************************************************************//**
Position a row accord to key, and fetch value if needed
@return DB_SUCCESS if successful otherwise, error code */
ib_err_t
innodb_api_search(
/*==============*/
	innodb_engine_t*	engine,	/*!< in: InnoDB Memcached engine */
	innodb_conn_data_t*	cursor_data,/*!< in: cursor info */
	ib_crsr_t*		crsr,	/*!< out: cursor used to seacrh */
	const char*		key,	/*!< in: key to search */
	int			len,	/*!< in: key length */
	mci_item_t*		item,	/*!< in: result */
	ib_tpl_t*		r_tpl)	/*!< in: tpl for other DML operations */
{
	ib_tpl_t        key_tpl;
	ib_err_t        err = DB_SUCCESS;
	meta_info_t*	meta_info = &engine->meta_info;
	meta_column_t*	col_info = meta_info->m_item;
	meta_index_t*	meta_index = &meta_info->m_index;
	ib_crsr_t	srch_crsr;

	if (meta_index->m_use_idx == META_SECONDARY) {
		ib_crsr_t	idx_crsr;
		idx_crsr = cursor_data->c_idx_crsr;
		ib_cursor_set_cluster_access(idx_crsr);

		key_tpl = ib_cb_search_tuple_create(idx_crsr);
		srch_crsr = idx_crsr;

	} else {
		key_tpl = ib_cb_search_tuple_create(cursor_data->c_crsr);
		srch_crsr = cursor_data->c_crsr;
	}

	err = ib_cb_col_set_value(key_tpl, 0, (char*) key, len);

	ib_cb_cursor_set_match_mode(srch_crsr, IB_EXACT_MATCH);
	err = ib_cb_moveto(srch_crsr, key_tpl, IB_CUR_GE);

	if (err != DB_SUCCESS) {
		if (r_tpl) {
			*r_tpl = NULL;
		}
		goto func_exit;
	}

	/* If item is NULL, the function is used to position the cursor */
	if (item) {
		ib_tpl_t	read_tpl;
		int		n_cols;
		int		i;

		memset(item, 0, sizeof(*item));

		read_tpl = ib_cb_read_tuple_create(cursor_data->c_crsr);

		err = ib_cb_read_row(srch_crsr, read_tpl);

		n_cols = ib_cb_tuple_get_n_cols(read_tpl);


		if (meta_info->m_num_add > 0) {
			item->mci_add_value = malloc(
				meta_info->m_num_add
				* sizeof(*item->mci_add_value));
			item->mci_add_num = meta_info->m_num_add;
		} else {
			item->mci_add_value = NULL;
			item->mci_add_num = 0;
		}

		/* The table must have at least MCI_ITEM_TO_GET columns for key
		value store, and the cas and time expiration info */
		assert(n_cols >= MCI_ITEM_TO_GET);

		for (i = 0; i < n_cols; ++i) {
			ib_ulint_t      data_len;
			ib_col_meta_t   col_meta;

			data_len = ib_cb_col_get_meta(read_tpl, i, &col_meta);

			if (i == col_info[META_KEY].m_field_id) {
				assert(data_len != IB_SQL_NULL);
				item->mci_item[MCI_COL_KEY].m_str =
					(char*)ib_cb_col_get_value(read_tpl, i);
				item->mci_item[MCI_COL_KEY].m_len = data_len;
				item->mci_item[MCI_COL_KEY].m_is_str = TRUE;
				item->mci_item[MCI_COL_KEY].m_enabled = TRUE;
			} else if (meta_info->flag_enabled
				   && i == col_info[META_FLAG].m_field_id) {
				item->mci_item[MCI_COL_FLAG].m_digit =
					 innodb_api_read_int(
						&col_info[META_FLAG].m_col,
						read_tpl, i);
				item->mci_item[MCI_COL_FLAG].m_is_str = FALSE;
				item->mci_item[MCI_COL_FLAG].m_len = data_len;
				item->mci_item[MCI_COL_FLAG].m_enabled = TRUE;
			} else if (meta_info->cas_enabled
				   && i == col_info[META_CAS].m_field_id) {
				item->mci_item[MCI_COL_CAS].m_digit =
					 innodb_api_read_int(
						&col_info[META_CAS].m_col,
						read_tpl, i);
				item->mci_item[MCI_COL_CAS].m_is_str = FALSE;
				item->mci_item[MCI_COL_CAS].m_len = data_len;
				item->mci_item[MCI_COL_CAS].m_enabled = TRUE;
			} else if (meta_info->exp_enabled
				   && i == col_info[META_EXP].m_field_id) {
				item->mci_item[MCI_COL_EXP].m_digit =
					 innodb_api_read_int(
						&col_info[META_EXP].m_col,
						read_tpl, i);
				item->mci_item[MCI_COL_EXP].m_is_str = FALSE;
				item->mci_item[MCI_COL_EXP].m_len = data_len;
				item->mci_item[MCI_COL_EXP].m_enabled = TRUE;
			} else {
				innodb_api_fill_value(meta_info, item,
						      read_tpl, i);
			}
		}

		if (r_tpl) {
			*r_tpl = read_tpl;
		} else if (key_tpl) {
			ib_cb_tuple_delete(read_tpl);
		}
	}

func_exit:
	*crsr = srch_crsr;

	ib_cb_tuple_delete(key_tpl);

	return(err);
}

/*************************************************************//**
Get montonically increased cas ID */
static
uint64_t
mci_get_cas(void)
/*=============*/
{
	static uint64_t cas_id = 0;
	return(++cas_id);
}

/*************************************************************//**
Get current time */
uint64_t
mci_get_time(void)
/*==============*/
{
	struct timeval tv;

	gettimeofday(&tv,NULL);

	return ((ulonglong)tv.tv_sec);
}

/*************************************************************//**
Insert a row
@return DB_SUCCESS if successful otherwise, error code */
static
ib_err_t
innodb_api_set_multi_cols(
/*======================*/
	ib_tpl_t	tpl,		/*!< in: tuple for insert */
	meta_info_t*	meta_info,	/*!< in: metadata info */
	char*		value,		/*!< in: value to insert */
	int		value_len)	/*!< in: value length */
{
	ib_err_t	err = DB_ERROR;
	meta_column_t*	col_info;
	char*		last;
	char*		col_val;
	char*		end = value + value_len;
	int		i = 0;
	char*		sep = meta_info->m_separator;

	col_info = meta_info->m_add_item;

	for (col_val = strtok_r(value, sep, &last);
	     col_val && last <= end && i < meta_info->m_num_add;
	     col_val = strtok_r(NULL, sep, &last), i++) {

		err = ib_cb_col_set_value(tpl, col_info[i].m_field_id,
					  col_val, strlen(col_val));

		if (err != DB_SUCCESS) {
			break;
		}
	}

	return(err);
}

/*************************************************************//**
Insert a row
@return DB_SUCCESS if successful otherwise, error code */
static
ib_err_t
innodb_api_set_tpl(
/*===============*/
	ib_tpl_t	tpl,		/*!< in: tuple for insert */
	meta_info_t*	meta_info,	/*!< in: metadata info */
	meta_column_t*	col_info,	/*!< in: insert col info */
	const char*	key,		/*!< in: key */
	int		key_len,	/*!< in: key length */
	const char*	value,		/*!< in: value to insert */
	int		value_len,	/*!< in: value length */
	uint64_t	cas,		/*!< in: cas */
	uint64_t	exp,		/*!< in: expiration */
	uint64_t	flag)		/*!< in: flag */
{
	ib_err_t	err = DB_ERROR;

	err = ib_cb_col_set_value(tpl, col_info[META_KEY].m_field_id,
				  key, key_len);
	assert(err == DB_SUCCESS);

	if (meta_info->m_num_add > 0) {
		err = innodb_api_set_multi_cols(tpl, meta_info,
						(char*) value, value_len);
	} else {
		err = ib_cb_col_set_value(tpl, col_info[META_VALUE].m_field_id,
					  value, value_len);
	}

	assert(err == DB_SUCCESS);

	if (meta_info->cas_enabled) {
		err = innodb_api_write_int(
			tpl, col_info[META_CAS].m_field_id, cas); 
		assert(err == DB_SUCCESS);
	}

	if (meta_info->exp_enabled) {
		err = innodb_api_write_int(
			tpl, col_info[META_EXP].m_field_id, exp);
		assert(err == DB_SUCCESS);
	}

	if (meta_info->flag_enabled) {
		err = innodb_api_write_int(
			tpl, col_info[META_FLAG].m_field_id, flag);
		assert(err == DB_SUCCESS);
	}

	return(err);
}

/*************************************************************//**
Insert a row
@return DB_SUCCESS if successful otherwise, error code */
ib_err_t
innodb_api_insert(
/*==============*/
	innodb_engine_t*	engine,	/*!< in: InnoDB Memcached engine */
	innodb_conn_data_t*     cursor_data,/*!< in: cursor info */
	const char*		key,	/*!< in: value to insert */
	int			len,	/*!< in: value length */
	uint64_t		exp,	/*!< in: expire time */
	uint64_t*		cas,	/*!< in/out: cas value */
	uint64_t		flags)	/*!< in: flags */
{
	uint64_t	new_cas;
	ib_err_t	err = DB_ERROR;
	ib_tpl_t	tpl = NULL;
	meta_info_t*	meta_info = &engine->meta_info;
	meta_column_t*	col_info = meta_info->m_item;

	new_cas = mci_get_cas();

	tpl = ib_cb_read_tuple_create(cursor_data->c_crsr);
	assert(tpl != NULL);

	if (exp) {
		uint64_t	time;
		time = mci_get_time();
		exp += time;
	}

	err = innodb_api_set_tpl(tpl, meta_info, col_info, key, len,
				 key + len, strlen(key) - len,
				 new_cas, exp, flags);

	err = ib_cb_insert_row(cursor_data->c_crsr, tpl);
	assert(err == DB_SUCCESS);

	if (err == DB_SUCCESS) {
		*cas = new_cas;
	}

	ib_cb_tuple_delete(tpl);
	return(err);
}

/*************************************************************//**
Update a row, it is used by "replace", "prepend", "append" and "set"
commands
@return DB_SUCCESS if successful otherwise, error code */
static
ib_err_t
innodb_api_update(
/*==============*/
	innodb_engine_t*	engine,	/*!< in: InnoDB Memcached engine */
	innodb_conn_data_t*     cursor_data,/*!< in: cursor info */
	ib_crsr_t		srch_crsr,/*!< in: cursor to use for write */
	const char*		key,	/*!< in: value to insert */
	int			len,	/*!< in: value length */
	uint64_t		exp,	/*!< in: expire time */
	uint64_t*		cas,	/*!< in/out: cas value */
	uint64_t		flags,	/*!< in: flags */
	ib_tpl_t		old_tpl)/*!< in: tuple being updated */
{
	uint64_t	new_cas;
	ib_tpl_t	new_tpl;
	meta_info_t*	meta_info = &engine->meta_info;
	meta_column_t*	col_info = meta_info->m_item;
	ib_err_t	err = DB_SUCCESS;

	assert(old_tpl != NULL);

	new_tpl = ib_cb_read_tuple_create(cursor_data->c_crsr);
	assert(new_tpl != NULL);

	new_cas = mci_get_cas();

	if (exp) {
		uint64_t	time;
		time = mci_get_time();
		exp += time;
	}

	err = innodb_api_set_tpl(new_tpl, meta_info, col_info, key, len,
				 key + len, strlen(key) - len,
				 new_cas, exp, flags);

	err = ib_cb_update_row(srch_crsr, old_tpl, new_tpl);
	assert(err == DB_SUCCESS);

	ib_cb_tuple_delete(new_tpl);

	if (err == DB_SUCCESS) {
		*cas = new_cas;
	}

	return(err);
}

/*************************************************************//**
Delete a row, implements the "remove" command
@return DB_SUCCESS if successful otherwise, error code */
ENGINE_ERROR_CODE
innodb_api_delete(
/*==============*/
	innodb_engine_t*	engine,	/*!< in: InnoDB Memcached engine */
	innodb_conn_data_t*     cursor_data,/*!< in: cursor info */
	const char*		key,	/*!< in: value to insert */
	int			len)	/*!< in: value length */
{
	ib_err_t	err = DB_SUCCESS;
	ib_crsr_t	srch_crsr = cursor_data->c_crsr;

	err = innodb_api_search(engine, cursor_data, &srch_crsr, key, len,
				NULL, NULL);

	if (err != DB_SUCCESS) {
		return(ENGINE_KEY_ENOENT);
	}

	err = ib_cb_delete_row(srch_crsr);

	return(err == DB_SUCCESS ? ENGINE_SUCCESS : ENGINE_KEY_ENOENT);
}

/*************************************************************//**
Link the value with a string, driver function for command
"prepend" or "append"
@return DB_SUCCESS if successful otherwise, error code */
static
ib_err_t
innodb_api_link(
/*============*/
	innodb_engine_t*	engine,	/*!< in: InnoDB Memcached engine */
	innodb_conn_data_t*     cursor_data,/*!< in: cursor info */
	ib_crsr_t		srch_crsr,/*!< in: cursor to use for write */
	const char*		key,	/*!< in: key value */
	int			len,	/*!< in: key length */
	uint64_t		exp,	/*!< in: expire time */
	uint64_t*		cas,	/*!< out: cas value */
	uint64_t		flags,	/*!< in: flags */
	bool			append,	/*!< in: Whether to append or prepend
					the value */
	ib_tpl_t		old_tpl,/*!< in: tuple being updated */
	mci_item_t*		result)	/*!< in: tuple data for tuple being
					updated */
{
	ib_err_t	err = DB_SUCCESS;
	char*		append_buf;
	int		before_len;
	int		total_len;
	ib_tpl_t	new_tpl;
	uint64_t	new_cas;
	meta_info_t*	meta_info = &engine->meta_info;
	meta_column_t*	col_info = meta_info->m_item;

	before_len = result->mci_item[MCI_COL_VALUE].m_len;

	total_len = before_len + strlen(key) - len;

	append_buf = (char*)malloc(total_len);

	if (append) {
		memcpy(append_buf, result->mci_item[MCI_COL_VALUE].m_str,
		       before_len);
		memcpy(append_buf + before_len, key + len,
		       strlen(key) - len);
	} else {
		memcpy(append_buf, key + len, strlen(key) - len);
		memcpy(append_buf + strlen(key) - len,
		       result->mci_item[MCI_COL_VALUE].m_str, before_len);
	}

	new_tpl = ib_cb_read_tuple_create(cursor_data->c_crsr);
	new_cas = mci_get_cas();

	if (exp) {
		uint64_t	time;
		time = mci_get_time();
		exp += time;
	}

	err = innodb_api_set_tpl(new_tpl, meta_info, col_info, key, len,
				 append_buf, total_len,
				 new_cas, exp, flags);

	err = ib_cb_update_row(srch_crsr, old_tpl, new_tpl);

	ib_cb_tuple_delete(new_tpl);

	free(append_buf);

	if (err == DB_SUCCESS) {
		*cas = new_cas;
	}

	return(err);

}

/*************************************************************//**
Update a row with arithmetic operation
@return ENGINE_SUCCESS if successful otherwise ENGINE_NOT_STORED */
ENGINE_ERROR_CODE
innodb_api_arithmetic(
/*==================*/
	innodb_engine_t*	engine,	/*!< in: InnoDB Memcached engine */
	innodb_conn_data_t*     cursor_data,/*!< in: cursor info */
	const char*		key,	/*!< in: key value */
	int			len,	/*!< in: key length */
	int			delta,	/*!< in: value to add or subtract */
	bool			increment, /*!< in: increment or decrement */
	uint64_t*		cas,	/*!< out: cas */
	rel_time_t		exp_time, /*!< in: expire time */
	bool			create,	/*!< in: whether to create new entry
					if not found */
	uint64_t		initial,/*!< in: initialize value */
	uint64_t*		out_result) /*!< in: arithmetic result */
{

	ib_err_t	err = DB_SUCCESS;
	char		value_buf[128];
	mci_item_t	result;
	ib_tpl_t	old_tpl;
	ib_tpl_t	new_tpl;
	uint64_t	value = 0;
	ibool		create_new = FALSE;
	char*		end_ptr;
	meta_info_t*	meta_info = &engine->meta_info;
	meta_column_t*	col_info = meta_info->m_item;
	ib_crsr_t       srch_crsr = cursor_data->c_crsr;

	err = innodb_api_search(engine, cursor_data, &srch_crsr, key, len,
				&result, &old_tpl);

	if (err != DB_SUCCESS) {
		ib_tuple_delete(old_tpl);

		if (create) {
			snprintf(value_buf, sizeof(value_buf), "%llu", initial);
			create_new = TRUE;
			goto create_new_value;
		} else {
			return(DB_RECORD_NOT_FOUND);
		}
	}

	if (result.mci_item[MCI_COL_VALUE].m_len >= (sizeof(value_buf) - 1)) {
		ib_cb_tuple_delete(old_tpl);
		return ENGINE_EINVAL;
	}

	errno = 0;	

	if (result.mci_item[MCI_COL_VALUE].m_str) {
		value = strtoull(result.mci_item[MCI_COL_VALUE].m_str,
				&end_ptr, 10);
	}
	
	if (errno == ERANGE) {
		ib_cb_tuple_delete(old_tpl);
		return ENGINE_EINVAL;
	}

	if (increment) {
		value += delta;
	} else {
		if (delta > value) {
			value = 0;
		} else {
			value -= delta;
		}
	}


	snprintf(value_buf, sizeof(value_buf), "%llu", value);
create_new_value:
	*cas = mci_get_cas();

	new_tpl = ib_cb_read_tuple_create(cursor_data->c_crsr);

	/* The cas, exp and flags field are not changing, so use the
	data from result */
	err = innodb_api_set_tpl(new_tpl, meta_info, col_info, key, len,
				 value_buf, strlen(value_buf),
				 result.mci_item[MCI_COL_CAS].m_digit,
				 result.mci_item[MCI_COL_EXP].m_digit,
				 result.mci_item[MCI_COL_FLAG].m_digit);

	*cas = result.mci_item[MCI_COL_CAS].m_digit;

	assert(err == DB_SUCCESS);

	if (create_new) {
		err = ib_cb_insert_row(cursor_data->c_crsr, new_tpl);
		*out_result = initial;
	} else {
		err = ib_cb_update_row(srch_crsr, old_tpl, new_tpl);
		*out_result = value;
	}

	ib_cb_tuple_delete(new_tpl);
	ib_cb_tuple_delete(old_tpl);

	return(err == DB_SUCCESS ? ENGINE_SUCCESS : ENGINE_NOT_STORED);
}

/*************************************************************//**
This is the interface to following commands:
	1) add
	2) replace
	3) append
	4) prepend
	5) set
	6) cas
@return ENGINE_SUCCESS if successful otherwise, error code */
ENGINE_ERROR_CODE
innodb_api_store(
/*=============*/
	innodb_engine_t*	engine,	/*!< in: InnoDB Memcached engine */
	innodb_conn_data_t*	cursor_data,/*!< in: cursor info */
	const char*		key,	/*!< in: key value */
	int			len,	/*!< in: key length */
	uint64_t		exp,	/*!< in: expire time */
	uint64_t*		cas,	/*!< out: cas value */
	uint64_t		input_cas,/*!< in: cas value supplied by user */
	uint64_t		flags,	/*!< in: flags */
	ENGINE_STORE_OPERATION	op)	/*!< in: Operations */
{
	ib_err_t	err = DB_ERROR;
	mci_item_t	result;
	ib_tpl_t	old_tpl = NULL;
	ENGINE_ERROR_CODE stored = ENGINE_NOT_STORED;
	ib_crsr_t	srch_crsr = cursor_data->c_crsr;

	err = innodb_api_search(engine, cursor_data, &srch_crsr,
				key, len, &result, &old_tpl);
	switch (op) {
	case OPERATION_ADD:
		/* Only add if the key does not exist */
		if (err != DB_SUCCESS) {
			err = innodb_api_insert(engine, cursor_data, key, len,
						exp, cas, flags);
		} else {
			err = DB_ERROR;
		} 
		break;
	case OPERATION_REPLACE:
		if (err == DB_SUCCESS) {
			err = innodb_api_update(engine, cursor_data, srch_crsr,
						key, len, exp,
						cas, flags, old_tpl);
		}
		break;
	case OPERATION_APPEND:
	case OPERATION_PREPEND:
		/* FIXME: Check cas is used for append and prepend */
		/* if (*cas != result.mci_item[MCI_COL_CAS].m_digit) {
			stored = ENGINE_KEY_EEXISTS;
			break;
		} */

		if (err == DB_SUCCESS) {
			err = innodb_api_link(engine, cursor_data, srch_crsr,
					      key, len, exp,
					      cas, flags,
					      (op == OPERATION_APPEND),
					      old_tpl, &result);
		}
		break;
	case OPERATION_SET:
		if (err == DB_SUCCESS) {
			err = innodb_api_update(engine, cursor_data,
						srch_crsr, key, len,
						exp, cas, flags,
						old_tpl);
		} else {
			err = innodb_api_insert(engine, cursor_data, key, len,
						exp, cas, flags);
		}
		break;
	case OPERATION_CAS:
		if (err != DB_SUCCESS) {
			stored = ENGINE_KEY_ENOENT;

		} else if (input_cas == result.mci_item[MCI_COL_CAS].m_digit) {
			err = innodb_api_update(engine, cursor_data, srch_crsr,
						key, len, exp,
						cas, flags, old_tpl);

		} else {
			stored = ENGINE_KEY_EEXISTS;
		}
		break;
	}

	ib_tuple_delete(old_tpl);

	if (err == DB_SUCCESS && stored == ENGINE_NOT_STORED) {
		stored = ENGINE_SUCCESS;
	}

	return(stored);
}
/*********************************************************************
Implement the flush_all command */
ENGINE_ERROR_CODE
innodb_api_flush(
/*=============*/
	const char*	dbname,		/*!< in: database name */
	const char*	name)		/*!< in: table name */
{
	ib_err_t	err = DB_SUCCESS;
	char		table_name[MAX_TABLE_NAME_LEN + MAX_DATABASE_NAME_LEN];
	ib_id_t		new_id;

#ifdef __WIN__
	sprintf(table_name, "%s\%s", dbname, name);
#else
	snprintf(table_name, sizeof(table_name), "%s/%s", dbname, name);
#endif
	/* currently, we implement engine flush as truncate table */
	err  = ib_cb_table_truncate(table_name, &new_id);
		
	return(ENGINE_SUCCESS);
}
/*************************************************************//**
reset the cursor */
void
innodb_api_cursor_reset(
/*====================*/
	innodb_conn_data_t*	conn_data)	/*!< in/out: cursor affiliated
						with a connection */
{
	ib_cb_cursor_reset(conn_data->c_crsr);
        ib_cb_cursor_reset(conn_data->c_idx_crsr);

	//pthread_mutex_lock(&innodb_eng->conn_mutex);
	assert(conn_data->c_in_use);
	conn_data->c_in_use = FALSE;
	//pthread_mutex_unlock(&innodb_eng->conn_mutex);

	return;
}
