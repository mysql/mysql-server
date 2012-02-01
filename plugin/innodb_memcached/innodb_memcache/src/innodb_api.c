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
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

***********************************************************************/

/**************************************************//**
@file innodb_api.c
InnoDB APIs to support memcached commands

Created 04/12/2011 Jimmy Yang
*******************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include "innodb_api.h"
#include "memcached/util.h"
#include <innodb_cb_api.h>

/** Whether to update all columns' value or a specific column value */
#define UPDATE_ALL_VAL_COL	-1

/** Defines for handler_binlog_row()'s mode field */
#define HDL_UPDATE		1
#define HDL_INSERT		2
#define HDL_DELETE		3

/** InnoDB API callback functions */
static ib_cb_t* innodb_memcached_api[] = {
	(ib_cb_t*) &ib_cb_open_table,
	(ib_cb_t*) &ib_cb_read_row,
	(ib_cb_t*) &ib_cb_insert_row,
	(ib_cb_t*) &ib_cb_delete_row,
	(ib_cb_t*) &ib_cb_update_row,
	(ib_cb_t*) &ib_cb_moveto,
	(ib_cb_t*) &ib_cb_cursor_first,
	(ib_cb_t*) &ib_cb_cursor_last,
	(ib_cb_t*) &ib_cb_cursor_set_match_mode,
	(ib_cb_t*) &ib_cb_search_tuple_create,
	(ib_cb_t*) &ib_cb_read_tuple_create,
	(ib_cb_t*) &ib_cb_tuple_delete,
	(ib_cb_t*) &ib_cb_tuple_copy,
	(ib_cb_t*) &ib_cb_tuple_read_u32,
	(ib_cb_t*) &ib_cb_tuple_write_u32,
	(ib_cb_t*) &ib_cb_tuple_read_u64,
	(ib_cb_t*) &ib_cb_tuple_write_u64,
	(ib_cb_t*) &ib_cb_tuple_read_i32,
	(ib_cb_t*) &ib_cb_tuple_write_i32,
	(ib_cb_t*) &ib_cb_tuple_read_i64,
	(ib_cb_t*) &ib_cb_tuple_write_i64,
	(ib_cb_t*) &ib_cb_tuple_get_n_cols,
	(ib_cb_t*) &ib_cb_col_set_value,
	(ib_cb_t*) &ib_cb_col_get_value,
	(ib_cb_t*) &ib_cb_col_get_meta,
	(ib_cb_t*) &ib_cb_trx_begin,
	(ib_cb_t*) &ib_cb_trx_commit,
	(ib_cb_t*) &ib_cb_trx_rollback,
	(ib_cb_t*) &ib_cb_trx_start,
	(ib_cb_t*) &ib_cb_trx_release,
	(ib_cb_t*) &ib_cb_trx_state,
	(ib_cb_t*) &ib_cb_cursor_lock,
	(ib_cb_t*) &ib_cb_cursor_close,
	(ib_cb_t*) &ib_cb_cursor_new_trx,
	(ib_cb_t*) &ib_cb_cursor_reset,
	(ib_cb_t*) &ib_cb_open_table_by_name,
	(ib_cb_t*) &ib_cb_col_get_name,
	(ib_cb_t*) &ib_cb_table_truncate,
	(ib_cb_t*) &ib_cb_cursor_open_index_using_name,
	(ib_cb_t*) &ib_cb_close_thd,
	(ib_cb_t*) &ib_cb_binlog_enabled,
	(ib_cb_t*) &ib_cb_cursor_set_cluster_access,
	(ib_cb_t*) &ib_cb_cursor_commit_trx
};

/*************************************************************//**
Register InnoDB Callback functions */
void
register_innodb_cb(
/*===============*/
	char*	p)		/*!<in: Pointer to callback function array */
{
	int	i;
	int	array_size;
	ib_cb_t*func_ptr = (ib_cb_t*) p;

	array_size = sizeof(innodb_memcached_api) / sizeof(ib_cb_t);

	for (i = 0; i < array_size; i++) {
		*innodb_memcached_api[i] = *(ib_cb_t*)func_ptr;
		func_ptr++;
	}
}

/*************************************************************//**
Open a table and return a cursor for the table.
@return DB_SUCCESS if table successfully opened */
ib_err_t
innodb_api_begin(
/*=============*/
	innodb_engine_t*engine,		/*!< in: InnoDB Memcached engine */
	const char*	dbname,		/*!< in: database name */
	const char*	name,		/*!< in: table name */
	innodb_conn_data_t* conn_data,	/*!< in: connnection specific data */
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
			fprintf(stderr, "  InnoDB_Memcached: Unable to open"
					" table '%s'\n", table_name);
			return(err);
		}

		err = ib_cb_cursor_lock(*crsr, lock_mode);

		if (err != DB_SUCCESS) {
			fprintf(stderr, "  InnoDB_Memcached: Fail to lock"
					" table '%s'\n", table_name);
			return(err);
		}

		if (engine) {
			meta_info_t*	meta_info = &engine->meta_info;
			meta_index_t*	meta_index = &meta_info->m_index;

			/* Open the cursor */
			if (meta_index->m_use_idx == META_SECONDARY) {
				int		index_type;
				ib_id_u64_t	index_id;

				ib_cb_cursor_open_index_using_name(
					*crsr, meta_index->m_name,
					idx_crsr, &index_type, &index_id);

				ib_cb_cursor_lock(*idx_crsr, lock_mode);
			}

			/* Create a "Fake" THD if binlog is enabled */
			if (conn_data && engine->enable_binlog) {
				if (!conn_data->thd) {
					conn_data->thd = handler_create_thd();
				}
			}
		}
	} else {
		ib_cb_cursor_new_trx(*crsr, ib_trx);
		err = ib_cb_cursor_lock(*crsr, lock_mode);

		if (err != DB_SUCCESS) {
			fprintf(stderr, "  InnoDB_Memcached: Fail to lock"
					" table '%s'\n", name);
			return(err);
		}

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
Read an integer value from an InnoDB tuple
@return integer value fetched */
static
uint64_t
innodb_api_read_int(
/*================*/
	ib_col_meta_t*	m_col,		/*!< in: column info */
	ib_tpl_t        read_tpl,	/*!< in: tuple to read */
	int		i)		/*!< in: column number */
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
set up an integer type tuple field for write
@return DB_SUCCESS if successful otherwise, error code */
static
ib_err_t
innodb_api_write_int(
/*=================*/
	ib_tpl_t        tpl,		/*!< in/out: tuple to set */
	int		field,		/*!< in: field to set */
	uint64_t	value,		/*!< in: value */
	void*		table)		/*!< in/out: MySQL table. Only needed
					when binlog is enabled */
{
	ib_col_meta_t   col_meta;
	ib_col_meta_t*	m_col = &col_meta;

	ib_cb_col_get_meta(tpl, field, m_col);

	assert(m_col->type == IB_INT);
	assert(m_col->type_len == 8 || m_col->type_len == 4);

	if (m_col->attr == IB_COL_UNSIGNED) {
		if (m_col->type_len == 8) {
			ib_cb_tuple_write_u64(tpl, field, value);

			/* If table is non-NULL, set up corresponding
			TABLE->record[0] field for replication */
			if (table) {
				handler_rec_setup_int(
					table, field, value, TRUE, FALSE);
			}
		} else if (m_col->type_len == 4) {
			uint32_t	value32;
			value32 = (uint32_t) value;

			ib_cb_tuple_write_u32(tpl, field, value32);
			if (table) {
				handler_rec_setup_int(
					table, field, value32, TRUE, FALSE);
			}
		}

	} else {
		if (m_col->type_len == 8) {
			int64_t		value64;
			value64 = (int64_t) value;

			ib_cb_tuple_write_i64(tpl, field, value64);
			if (table) {
				handler_rec_setup_int(
					table, field, value64, FALSE, FALSE);
			}
		} else if (m_col->type_len == 4) {
			int32_t		value32;
			value32 = (int32_t) value;
			ib_cb_tuple_write_i32(tpl, field, value32);
			if (table) {
				handler_rec_setup_int(
					table, field, value32, FALSE, FALSE);
			}
		}
	}

	return(DB_SUCCESS);
}

/*************************************************************//**
Fetch data from a read tuple and instantiate a "mci_item" structure
@return TRUE if successful */
static
bool
innodb_api_fill_mci(
/*================*/
	ib_tpl_t	read_tpl,	/*!< in: Read tuple */
	int		col_id,		/*!< in: Column ID for the column to
					read */
	mci_column_t*	mci_item)	/*!< out: item to fill */
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
	mci_item->m_allocated = FALSE;

	return(TRUE);
}

/*************************************************************//**
Copy data from a read tuple and instantiate a "mci_item"
@return TRUE if successful */
static
bool
innodb_api_copy_mci(
/*================*/
	ib_tpl_t	read_tpl,	/*!< in: Read tuple */
	int		col_id,		/*!< in: Column ID for the column to
					read */
	mci_column_t*	mci_item)	/*!< out: item to fill */
{
	ib_ulint_t      data_len;
	ib_col_meta_t   col_meta;

	data_len = ib_cb_col_get_meta(read_tpl, col_id, &col_meta);

	if (data_len == IB_SQL_NULL) {
		mci_item->m_str = NULL;
		mci_item->m_len = 0;
		mci_item->m_allocated = FALSE;
	} else {
		mci_item->m_str = malloc(data_len);
		mci_item->m_allocated = TRUE;
		memcpy(mci_item->m_str, ib_cb_col_get_value(read_tpl, col_id),
		       data_len);
		mci_item->m_len = data_len;
	}

	mci_item->m_is_str = TRUE;
	mci_item->m_enabled = TRUE;

	return(TRUE);
}

/*************************************************************//**
Fetch value from a read cursor into "mci_items"
@return DB_SUCCESS if successful */
static
ib_err_t
innodb_api_fill_value(
/*==================*/
	meta_info_t*	meta_info,	/*!< in: Metadata */
	mci_item_t*	item,		/*!< out: result */
	ib_tpl_t	read_tpl,	/*!< in: read tuple */
	int		col_id,		/*!< in: column Id */
	bool		alloc_mem)	/*!< in: allocate memory */
{
	ib_err_t	err = DB_NOT_FOUND;

	/* If just read a single "value", fill mci_item[MCI_COL_VALUE],
	otherwise, fill multiple value in mci_add_value[i] */
	if (meta_info->m_num_add == 0) {
		meta_column_t*	col_info = meta_info->m_item;

		if (col_id == col_info[META_VALUE].m_field_id) {

			if (alloc_mem) {
				innodb_api_copy_mci(
					read_tpl, col_id,
					&item->mci_item[MCI_COL_VALUE]);
			} else {
				innodb_api_fill_mci(
					read_tpl, col_id,
					&item->mci_item[MCI_COL_VALUE]);
			}

			err = DB_SUCCESS;
		}
	} else {
		int	i;
		for (i = 0; i < meta_info->m_num_add; i++) {
			if (col_id == meta_info->m_add_item[i].m_field_id) {
				if (alloc_mem) {
					innodb_api_copy_mci(
						read_tpl, col_id,
						&item->mci_add_value[i]);
				} else {
					innodb_api_fill_mci(
						read_tpl, col_id,
						&item->mci_add_value[i]);
				}

				err = DB_SUCCESS;
				break;
			}
		}
	}

	return(err);
}

/*************************************************************//**
Position a row according to the search key, and fetch value if needed
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
	ib_tpl_t*		r_tpl,	/*!< in: tpl for other DML
					operations */
	bool			sel_only) /*!< in: for select only */
{
	ib_err_t	err = DB_SUCCESS;
	meta_info_t*	meta_info = &engine->meta_info;
	meta_column_t*	col_info = meta_info->m_item;
	meta_index_t*	meta_index = &meta_info->m_index;
	ib_tpl_t	key_tpl;
	ib_crsr_t	srch_crsr;

	/* If m_use_idx is set to META_SECONDARY, we will use the
	secondary index to find the record first */
	if (meta_index->m_use_idx == META_SECONDARY) {
		ib_crsr_t	idx_crsr;

		if (sel_only) {
			idx_crsr = cursor_data->c_r_idx_crsr;
		} else {
			idx_crsr = cursor_data->c_idx_crsr;
		}

		ib_cb_cursor_set_cluster_access(idx_crsr);

		key_tpl = ib_cb_search_tuple_create(idx_crsr);

		srch_crsr = idx_crsr;

	} else {
		ib_crsr_t	c_crsr;
		if (sel_only) {
			c_crsr = cursor_data->c_r_crsr;
		} else {
			c_crsr = cursor_data->c_crsr;
		}

		key_tpl = ib_cb_search_tuple_create(c_crsr);
		srch_crsr = c_crsr;
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

	/* If item is NULL, this function is used just to position the cursor.
	Otherwise, fetch the data from the read tuple */
	if (item) {
		ib_tpl_t	read_tpl;
		int		n_cols;
		int		i;

		memset(item, 0, sizeof(*item));

		read_tpl = ib_cb_read_tuple_create(
				sel_only ? cursor_data->c_r_crsr
					 : cursor_data->c_crsr);

		err = ib_cb_read_row(srch_crsr, read_tpl);

		n_cols = ib_cb_tuple_get_n_cols(read_tpl);

		if (meta_info->m_num_add > 0) {
			/* If there are multiple values to read,allocate
			memory */
			item->mci_add_value = malloc(
				meta_info->m_num_add
				* sizeof(*item->mci_add_value));
			item->mci_add_num = meta_info->m_num_add;
		} else {
			item->mci_add_value = NULL;
			item->mci_add_num = 0;
		}

		/* The table must have at least MCI_ITEM_TO_GET columns for key
		value, cas and time expiration info */
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

				if (data_len == IB_SQL_NULL) {
					item->mci_item[MCI_COL_FLAG].m_is_null
						= true;
				} else {
					item->mci_item[MCI_COL_FLAG].m_digit =
						innodb_api_read_int(
						&col_info[META_FLAG].m_col,
						read_tpl, i);
					item->mci_item[MCI_COL_FLAG].m_is_str
						 = FALSE;
					item->mci_item[MCI_COL_FLAG].m_len
						 = data_len;
					item->mci_item[MCI_COL_FLAG].m_enabled
						 = TRUE;
				}
			} else if (meta_info->cas_enabled
				   && i == col_info[META_CAS].m_field_id) {
				if (data_len == IB_SQL_NULL) {
					item->mci_item[MCI_COL_CAS].m_is_null
						= true;
				}
				item->mci_item[MCI_COL_CAS].m_digit =
					 innodb_api_read_int(
						&col_info[META_CAS].m_col,
						read_tpl, i);
				item->mci_item[MCI_COL_CAS].m_is_str = FALSE;
				item->mci_item[MCI_COL_CAS].m_len = data_len;
				item->mci_item[MCI_COL_CAS].m_enabled = TRUE;
			} else if (meta_info->exp_enabled
				   && i == col_info[META_EXP].m_field_id) {
				if (data_len == IB_SQL_NULL) {
					item->mci_item[MCI_COL_EXP].m_is_null
						= true;
				}

				item->mci_item[MCI_COL_EXP].m_digit =
					 innodb_api_read_int(
						&col_info[META_EXP].m_col,
						read_tpl, i);
				item->mci_item[MCI_COL_EXP].m_is_str = FALSE;
				item->mci_item[MCI_COL_EXP].m_len = data_len;
				item->mci_item[MCI_COL_EXP].m_enabled = TRUE;
			} else {
				innodb_api_fill_value(meta_info, item,
						      read_tpl, i,
						      sel_only);
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
Get montonically increasing cas (check and set) ID.
@return new cas ID */
static
uint64_t
mci_get_cas(void)
/*=============*/
{
	static uint64_t cas_id = 0;

#if defined(HAVE_IB_GCC_ATOMIC_BUILTINS)
	return(__sync_add_and_fetch(&cas_id, 1));
#else
	/* FIXME: need mutex protection */
	return(++cas_id);
#endif
}

/*************************************************************//**
Get current time
@return time in seconds */
uint64_t
mci_get_time(void)
/*==============*/
{
	struct timeval tv;

	gettimeofday(&tv,NULL);

	return ((uint64_t)tv.tv_sec);
}

/*************************************************************//**
Set up a record with multiple columns for insertion
@return DB_SUCCESS if successful, otherwise, error code */
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

	/* Input values are separated with "sep" */
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
Set up a MySQL "TABLE" record in table->record[0] for binlogging */
static
void
innodb_api_setup_hdl_rec(
/*=====================*/
	mci_item_t*		item,		/*!< in: item contain data
						to set on table->record[0] */
	meta_column_t*		col_info,	/*!< in: column information */
	void*			table)		/*!< out: MySQL TABLE* */
{
	int	i;

	for (i = 0; i < MCI_ITEM_TO_GET; i++) {
		if (item->mci_item[i].m_is_str) {
			handler_rec_setup_str(
				table, col_info[META_KEY + i].m_field_id,
				item->mci_item[i].m_str,
				item->mci_item[i].m_len);
		} else {
			handler_rec_setup_int(
				table, col_info[META_KEY + i].m_field_id,
				item->mci_item[i].m_digit, TRUE,
				item->mci_item[i].m_is_null);
		}
	}
}

/*************************************************************//**
Set up an insert tuple
@return DB_SUCCESS if successful otherwise, error code */
static
ib_err_t
innodb_api_set_tpl(
/*===============*/
	ib_tpl_t	tpl,		/*!< in/out: tuple for insert */
	ib_tpl_t	old_tpl,	/*!< in: old tuple */
	meta_info_t*	meta_info,	/*!< in: metadata info */
	meta_column_t*	col_info,	/*!< in: insert col info */
	const char*	key,		/*!< in: key */
	int		key_len,	/*!< in: key length */
	const char*	value,		/*!< in: value to insert */
	int		value_len,	/*!< in: value length */
	uint64_t	cas,		/*!< in: cas */
	uint64_t	exp,		/*!< in: expiration */
	uint64_t	flag,		/*!< in: flag */
	int		col_to_set,	/*!< in: column to set */
	void*		table)		/*!< in: MySQL TABLE* */
{
	ib_err_t	err = DB_ERROR;

	if (old_tpl) {
		ib_cb_tuple_copy(tpl, old_tpl);
	}

	err = ib_cb_col_set_value(tpl, col_info[META_KEY].m_field_id,
				  key, key_len);

	/* If "table" is not NULL, we need to setup MySQL record
	for binlogging */
	if (table) {
		handler_rec_init(table);
		handler_rec_setup_str(
			table, col_info[META_KEY].m_field_id, key, key_len);
	}

	assert(err == DB_SUCCESS);

	if (meta_info->m_num_add > 0) {
		if (col_to_set == UPDATE_ALL_VAL_COL) {
			err = innodb_api_set_multi_cols(tpl, meta_info,
							(char*) value,
							value_len);
		} else {
			err = ib_cb_col_set_value(
				tpl,
				meta_info->m_add_item[col_to_set].m_field_id,
				value, value_len);
		}
	} else {
		err = ib_cb_col_set_value(tpl, col_info[META_VALUE].m_field_id,
					  value, value_len);
		if (table) {
			handler_rec_setup_str(
				table, col_info[META_VALUE].m_field_id,
				value, value_len);
		}
	}

	assert(err == DB_SUCCESS);

	if (meta_info->cas_enabled) {
		err = innodb_api_write_int(
			tpl, col_info[META_CAS].m_field_id, cas, table);
		assert(err == DB_SUCCESS);
	}

	if (meta_info->exp_enabled) {
		err = innodb_api_write_int(
			tpl, col_info[META_EXP].m_field_id, exp, table);
		assert(err == DB_SUCCESS);
	}

	if (meta_info->flag_enabled) {
		err = innodb_api_write_int(
			tpl, col_info[META_FLAG].m_field_id, flag, table);
		assert(err == DB_SUCCESS);
	}

	return(err);
}

/*************************************************************//**
Insert a row
@return DB_SUCCESS if successful, otherwise, error code */
ib_err_t
innodb_api_insert(
/*==============*/
	innodb_engine_t*	engine,	/*!< in: InnoDB Memcached engine */
	innodb_conn_data_t*     cursor_data,/*!< in: cursor info */
	const char*		key,	/*!< in: key and value to insert */
	int			len,	/*!< in: key length */
	uint32_t		val_len,/*!< in: value length */
	uint64_t		exp,	/*!< in: expiration time */
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

	/* Set expiration time */
	if (exp) {
		uint64_t	time;
		time = mci_get_time();
		exp += time;
	}

	err = innodb_api_set_tpl(tpl, NULL, meta_info, col_info, key, len,
				 key + len, val_len,
				 new_cas, exp, flags, UPDATE_ALL_VAL_COL,
				 cursor_data->mysql_tbl);

	err = ib_cb_insert_row(cursor_data->c_crsr, tpl);

	if (err == DB_SUCCESS) {
		*cas = new_cas;

		if (engine->enable_binlog && cursor_data->mysql_tbl) {
			handler_binlog_row(cursor_data->mysql_tbl,
					   HDL_INSERT);
			handler_binlog_flush(cursor_data->thd,
					     cursor_data->mysql_tbl);
		}

	} else {
		ib_cb_trx_rollback(cursor_data->c_trx);
		cursor_data->c_trx = NULL;
	}

	ib_cb_tuple_delete(tpl);
	return(err);
}

/*************************************************************//**
Update a row, called by innodb_api_store(), it is used by memcached's
"replace", "prepend", "append" and "set" commands
@return DB_SUCCESS if successful, otherwise, error code */
static
ib_err_t
innodb_api_update(
/*==============*/
	innodb_engine_t*	engine,	/*!< in: InnoDB Memcached engine */
	innodb_conn_data_t*     cursor_data,/*!< in: cursor info */
	ib_crsr_t		srch_crsr,/*!< in: cursor to use for write */
	const char*		key,	/*!< in: key and value to insert */
	int			len,	/*!< in: key length */
	uint32_t		val_len,/*!< in: value length */
	uint64_t		exp,	/*!< in: expire time */
	uint64_t*		cas,	/*!< in/out: cas value */
	uint64_t		flags,	/*!< in: flags */
	ib_tpl_t		old_tpl,/*!< in: tuple being updated */
	mci_item_t*		result)	/*!< in: item info for the tuple being
					updated */
{
	uint64_t	new_cas;
	ib_tpl_t	new_tpl;
	meta_info_t*	meta_info = &engine->meta_info;
	meta_column_t*	col_info = meta_info->m_item;
	ib_err_t	err = DB_SUCCESS;

	assert(old_tpl != NULL);

	new_tpl = ib_cb_read_tuple_create(cursor_data->c_crsr);
	assert(new_tpl != NULL);

	/* cas will be updated for each update */
	new_cas = mci_get_cas();

	if (exp) {
		uint64_t	time;

		time = mci_get_time();
		exp += time;
	}

	if (engine->enable_binlog) {
		innodb_api_setup_hdl_rec(result, col_info,
					 cursor_data->mysql_tbl);
		handler_store_record(cursor_data->mysql_tbl);
	}

	err = innodb_api_set_tpl(new_tpl, old_tpl, meta_info, col_info, key,
				 len, key + len, val_len,
				 new_cas, exp, flags, UPDATE_ALL_VAL_COL,
				 cursor_data->mysql_tbl);

	err = ib_cb_update_row(srch_crsr, old_tpl, new_tpl);

	if (err == DB_SUCCESS) {
		*cas = new_cas;

		if (engine->enable_binlog) {
			assert(cursor_data->mysql_tbl);

			handler_binlog_row(cursor_data->mysql_tbl,
					   HDL_UPDATE);
			handler_binlog_flush(cursor_data->thd,
                                             cursor_data->mysql_tbl);
		}

	} else {
		ib_cb_trx_rollback(cursor_data->c_trx);
		cursor_data->c_trx = NULL;
	}

	ib_cb_tuple_delete(new_tpl);

	return(err);
}

/*************************************************************//**
Delete a row, support the memcached "remove" command
@return ENGINE_SUCCESS if successful otherwise, error code */
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
	mci_item_t	result;

	/* First look for the record, and check whether it exists */
	err = innodb_api_search(engine, cursor_data, &srch_crsr, key, len,
				&result, NULL, FALSE);

	if (err != DB_SUCCESS) {
		return(ENGINE_KEY_ENOENT);
	}

	/* The "result" structure contains only pointers to the data value
	when returning from innodb_api_search(), so store the delete row info
	before calling ib_cb_delete_row() */
	if (engine->enable_binlog) {
		meta_info_t*	meta_info = &engine->meta_info;
		meta_column_t*	col_info = meta_info->m_item;

		if (!cursor_data->mysql_tbl) {
			cursor_data->mysql_tbl = handler_open_table(
				cursor_data->thd,
				meta_info->m_item[META_DB].m_str,
				meta_info->m_item[META_TABLE].m_str, -2);
		}

		assert(cursor_data->mysql_tbl);

		innodb_api_setup_hdl_rec(&result, col_info,
					 cursor_data->mysql_tbl);
	}

	err = ib_cb_delete_row(srch_crsr);

	/* Do the binlog of the row being deleted */
	if (engine->enable_binlog) {
		if (err == DB_SUCCESS) {
			handler_binlog_row(cursor_data->mysql_tbl, HDL_DELETE);
			handler_binlog_flush(cursor_data->thd,
					     cursor_data->mysql_tbl);
		}

		handler_unlock_table(cursor_data->thd,
				     cursor_data->mysql_tbl, HDL_READ);
		cursor_data->mysql_tbl = NULL;
	}

	return(err == DB_SUCCESS ? ENGINE_SUCCESS : ENGINE_KEY_ENOENT);
}

/*************************************************************//**
Link the value with a string, called by innodb_api_store(), and
used for memcached's "prepend" or "append" commands
@return DB_SUCCESS if successful, otherwise, error code */
static
ib_err_t
innodb_api_link(
/*============*/
	innodb_engine_t*	engine,	/*!< in: InnoDB Memcached engine */
	innodb_conn_data_t*     cursor_data,/*!< in: cursor info */
	ib_crsr_t		srch_crsr,/*!< in: cursor to use for write */
	const char*		key,	/*!< in: key value */
	int			len,	/*!< in: key length */
	uint32_t		val_len,/*!< in: value length */
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
	char*		before_val;
	int		column_used;

	if (engine->enable_binlog) {
		assert(cursor_data->mysql_tbl);

		innodb_api_setup_hdl_rec(result, col_info,
					 cursor_data->mysql_tbl);
		handler_store_record(cursor_data->mysql_tbl);
	}

	/* If we have multiple value columns, the column to append the
	string needs to be defined. We will use user supplied flags
	as an indication on which column to apply the operation. Otherwise,
	the first column will be appended / prepended */
	if (meta_info->m_num_add > 0) {
		if (flags < meta_info->m_num_add) {
			column_used = flags;
		} else {
			column_used = 0;
		}

		before_len = result->mci_add_value[column_used].m_len;
		before_val = result->mci_add_value[column_used].m_str;
	} else {
		before_len = result->mci_item[MCI_COL_VALUE].m_len;
		before_val = result->mci_item[MCI_COL_VALUE].m_str;
		column_used = UPDATE_ALL_VAL_COL;
	}

	total_len = before_len + val_len;

	append_buf = (char*)malloc(total_len);

	if (append) {
		memcpy(append_buf, before_val, before_len);
		memcpy(append_buf + before_len, key + len, val_len);
	} else {
		memcpy(append_buf, key + len, val_len);
		memcpy(append_buf + val_len, before_val, before_len);
	}

	new_tpl = ib_cb_read_tuple_create(cursor_data->c_crsr);
	new_cas = mci_get_cas();

	if (exp) {
		uint64_t	time;
		time = mci_get_time();
		exp += time;
	}

	err = innodb_api_set_tpl(new_tpl, old_tpl, meta_info, col_info,
				 key, len, append_buf, total_len,
				 new_cas, exp, flags, column_used,
				 cursor_data->mysql_tbl);

	err = ib_cb_update_row(srch_crsr, old_tpl, new_tpl);

	ib_cb_tuple_delete(new_tpl);

	free(append_buf);

	if (err == DB_SUCCESS) {
		*cas = new_cas;

		if (engine->enable_binlog) {
			handler_binlog_row(cursor_data->mysql_tbl,
					   HDL_UPDATE);
			handler_binlog_flush(cursor_data->thd,
					     cursor_data->mysql_tbl);
		}
	}

	return(err);
}

/*************************************************************//**
Update a row with arithmetic operations
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
	bool		create_new = FALSE;
	char*		end_ptr;
	meta_info_t*	meta_info = &engine->meta_info;
	meta_column_t*	col_info = meta_info->m_item;
	ib_crsr_t       srch_crsr = cursor_data->c_crsr;
	char*		before_val;
	int		before_len;
	int		column_used = 0;
	ENGINE_ERROR_CODE ret = ENGINE_SUCCESS;

	err = innodb_api_search(engine, cursor_data, &srch_crsr, key, len,
				&result, &old_tpl, FALSE);

	if (engine->enable_binlog && !cursor_data->mysql_tbl
	    && (err == DB_SUCCESS || create)) {
		cursor_data->mysql_tbl = handler_open_table(
			cursor_data->thd, meta_info->m_item[META_DB].m_str,
			meta_info->m_item[META_TABLE].m_str, -2);
	}

	/* Can't find the row, decide whether to insert a new row */
	if (err != DB_SUCCESS) {
		ib_cb_tuple_delete(old_tpl);

		/* If create is true, insert a new row */
		if (create) {
			snprintf(value_buf, sizeof(value_buf), "%llu", initial);
			create_new = TRUE;
			goto create_new_value;
		} else {
			return(DB_RECORD_NOT_FOUND);
		}
	}

	/* Save the original value, this would be an update */
	if (engine->enable_binlog) {
		innodb_api_setup_hdl_rec(&result, col_info,
					 cursor_data->mysql_tbl);
		handler_store_record(cursor_data->mysql_tbl);
	}

	/* If we have multiple value columns, the column to append the
	string needs to be defined. We will use user supplied flags
	as an indication on which column to apply the operation. Otherwise,
	the first column will be appended / prepended */
	if (meta_info->m_num_add > 0) {
		uint64_t flags = result.mci_item[MCI_COL_FLAG].m_digit;

		if (flags < meta_info->m_num_add) {
			column_used = flags;
		} else {
			column_used = 0;
		}

		before_len = result.mci_add_value[column_used].m_len;
		before_val = result.mci_add_value[column_used].m_str;
	} else {
		before_len = result.mci_item[MCI_COL_VALUE].m_len;
		before_val = result.mci_item[MCI_COL_VALUE].m_str;
		column_used = UPDATE_ALL_VAL_COL;
	}

	if (before_len >= (sizeof(value_buf) - 1)) {
		ib_cb_tuple_delete(old_tpl);
		ret = ENGINE_EINVAL;
		goto func_exit;
	}

	errno = 0;

	if (before_val) {
		value = strtoull(before_val, &end_ptr, 10);
	}

	if (errno == ERANGE) {
		ib_cb_tuple_delete(old_tpl);
		ret = ENGINE_EINVAL;
		goto func_exit;
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
	err = innodb_api_set_tpl(new_tpl, old_tpl, meta_info, col_info,
				 key, len, value_buf, strlen(value_buf),
				 *cas,
				 result.mci_item[MCI_COL_EXP].m_digit,
				 result.mci_item[MCI_COL_FLAG].m_digit,
				 column_used, cursor_data->mysql_tbl);

	assert(err == DB_SUCCESS);

	if (create_new) {
		err = ib_cb_insert_row(cursor_data->c_crsr, new_tpl);
		*out_result = initial;

		if (engine->enable_binlog) {
			handler_binlog_row(cursor_data->mysql_tbl, HDL_INSERT);
			handler_binlog_flush(cursor_data->thd,
					     cursor_data->mysql_tbl);
		}
	} else {
		err = ib_cb_update_row(srch_crsr, old_tpl, new_tpl);
		*out_result = value;

		if (engine->enable_binlog) {
			handler_binlog_row(cursor_data->mysql_tbl, HDL_UPDATE);
			handler_binlog_flush(cursor_data->thd,
					     cursor_data->mysql_tbl);
		}
	}

	ib_cb_tuple_delete(new_tpl);
	ib_cb_tuple_delete(old_tpl);

func_exit:
	if (engine->enable_binlog && cursor_data->mysql_tbl) {
		handler_unlock_table(cursor_data->thd,
				     cursor_data->mysql_tbl, HDL_READ);
		cursor_data->mysql_tbl = NULL;
	}

	if (ret == ENGINE_SUCCESS) {
		ret = (err == DB_SUCCESS) ? ENGINE_SUCCESS : ENGINE_NOT_STORED;
	}

	return(ret);
}

/*************************************************************//**
This is the main interface to following memcached commands:
	1) add
	2) replace
	3) append
	4) prepend
	5) set
	6) cas
@return ENGINE_SUCCESS if successful, otherwise, error code */
ENGINE_ERROR_CODE
innodb_api_store(
/*=============*/
	innodb_engine_t*	engine,	/*!< in: InnoDB Memcached engine */
	innodb_conn_data_t*	cursor_data,/*!< in: cursor info */
	const char*		key,	/*!< in: key value */
	int			len,	/*!< in: key length */
	uint32_t		val_len,/*!< in: value length */
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

	/* First check whether record with the key value exists */
	err = innodb_api_search(engine, cursor_data, &srch_crsr,
				key, len, &result, &old_tpl, FALSE);

	if (engine->enable_binlog && !cursor_data->mysql_tbl) {
		meta_info_t*            meta_info = &engine->meta_info;

		cursor_data->mysql_tbl = handler_open_table(
			cursor_data->thd, meta_info->m_item[META_DB].m_str,
			meta_info->m_item[META_TABLE].m_str, -2);
	}

	switch (op) {
	case OPERATION_ADD:
		/* Only add if the key does not exist */
		if (err != DB_SUCCESS) {
			err = innodb_api_insert(engine, cursor_data, key, len,
						val_len, exp, cas, flags);
		} else {
			err = DB_ERROR;
		}
		break;
	case OPERATION_REPLACE:
		if (err == DB_SUCCESS) {
			err = innodb_api_update(engine, cursor_data, srch_crsr,
						key, len, val_len, exp,
						cas, flags, old_tpl, &result);
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
					      key, len, val_len, exp,
					      cas, flags,
					      (op == OPERATION_APPEND),
					      old_tpl, &result);
		}
		break;
	case OPERATION_SET:
		if (err == DB_SUCCESS) {
			err = innodb_api_update(engine, cursor_data,
						srch_crsr, key, len, val_len,
						exp, cas, flags,
						old_tpl, &result);
		} else {
			err = innodb_api_insert(engine, cursor_data, key, len,
						val_len, exp, cas, flags);
		}
		break;
	case OPERATION_CAS:
		if (err != DB_SUCCESS) {
			stored = ENGINE_KEY_ENOENT;

		} else if (input_cas == result.mci_item[MCI_COL_CAS].m_digit) {
			err = innodb_api_update(engine, cursor_data, srch_crsr,
						key, len, val_len, exp,
						cas, flags, old_tpl, &result);

		} else {
			stored = ENGINE_KEY_EEXISTS;
		}
		break;
	}

	ib_cb_tuple_delete(old_tpl);

	if (engine->enable_binlog && cursor_data->mysql_tbl) {
		handler_unlock_table(cursor_data->thd,
				     cursor_data->mysql_tbl, HDL_READ);
		cursor_data->mysql_tbl = NULL;
	}

	if (err == DB_SUCCESS && stored == ENGINE_NOT_STORED) {
		stored = ENGINE_SUCCESS;
	}

	return(stored);
}

/*************************************************************//**
Implement the "flush_all" command, map to InnoDB's "trunk table" operation
return ENGINE_SUCCESS is all successful */
ENGINE_ERROR_CODE
innodb_api_flush(
/*=============*/
	innodb_engine_t*	engine,	/*!< in: InnoDB Memcached engine */
	const char*		dbname,	/*!< in: database name */
	const char*		name)	/*!< in: table name */
{
	ib_err_t	err = DB_SUCCESS;
	char		table_name[MAX_TABLE_NAME_LEN
				   + MAX_DATABASE_NAME_LEN + 1];
	ib_id_u64_t	new_id;

#ifdef __WIN__
	sprintf(table_name, "%s\%s", dbname, name);
#else
	snprintf(table_name, sizeof(table_name), "%s/%s", dbname, name);
#endif
	/* currently, we implement engine flush as truncate table */
	err  = ib_cb_table_truncate(table_name, &new_id);

	/* If binlog is enabled, log the truncate table statement */
	if (err == DB_SUCCESS && engine->enable_binlog) {
		void*  thd = handler_create_thd();

		snprintf(table_name, sizeof(table_name), "%s.%s", dbname, name);
		handler_binlog_truncate(thd, table_name);

		handler_close_thd(thd);
	}

	return(err);
}

/*************************************************************//**
Increment read and write counters, if they exceed the batch size,
commit the transaction. */
void
innodb_api_cursor_reset(
/*====================*/
	innodb_engine_t*	engine,		/*!< in: InnoDB Memcached
						engine */
	innodb_conn_data_t*	conn_data,	/*!< in/out: cursor affiliated
						with a connection */
	op_type_t		op_type)	/*!< in: type of DML performed */
{
	bool		commit_trx = FALSE;

	switch (op_type) {
	case CONN_OP_READ:
		conn_data->c_r_count++;
		conn_data->c_r_count_commit++;
		break;
	case CONN_OP_DELETE:
	case CONN_OP_WRITE:
		conn_data->c_w_count++;
		conn_data->c_w_count_commit++;
		break;
	case CONN_OP_FLUSH:
		break;
	}

	if (conn_data->c_crsr
	    && (conn_data->c_w_count_commit >= engine->w_batch_size
	        || (op_type == CONN_OP_FLUSH))) {
		ib_cb_cursor_reset(conn_data->c_crsr);

		if (conn_data->c_idx_crsr) {
			ib_cb_cursor_reset(conn_data->c_idx_crsr);
		}

		if (conn_data->c_trx) {
			LOCK_CONN_IF_NOT_LOCKED(op_type == CONN_OP_FLUSH,
						engine);
			ib_cb_cursor_commit_trx(
				conn_data->c_crsr, conn_data->c_trx);
			conn_data->c_trx = NULL;
			commit_trx = TRUE;
			conn_data->c_in_use = FALSE;

			UNLOCK_CONN_IF_NOT_LOCKED(op_type == CONN_OP_FLUSH,
						  engine);
		}

		conn_data->c_w_count_commit = 0;
	}

	if (conn_data->c_r_crsr
	    && (conn_data->c_r_count_commit > engine->r_batch_size
	        || (op_type == CONN_OP_FLUSH))) {
		ib_cb_cursor_reset(conn_data->c_r_crsr);

		if (conn_data->c_r_idx_crsr) {
			ib_cb_cursor_reset(conn_data->c_r_idx_crsr);
		}

		if (conn_data->c_r_trx) {
			LOCK_CONN_IF_NOT_LOCKED(op_type == CONN_OP_FLUSH,
						engine);
			ib_cb_cursor_commit_trx(
				conn_data->c_r_crsr,
				conn_data->c_r_trx);
			conn_data->c_r_trx = NULL;
			commit_trx = TRUE;
			conn_data->c_in_use = FALSE;
			UNLOCK_CONN_IF_NOT_LOCKED(op_type == CONN_OP_FLUSH,
						  engine);
		}
		conn_data->c_r_count_commit = 0;
	}

	if (!commit_trx) {
		pthread_mutex_lock(&engine->conn_mutex);
		assert(conn_data->c_in_use);
		conn_data->c_in_use = FALSE;
		pthread_mutex_unlock(&engine->conn_mutex);
	}

	return;
}

/** Following are a set of InnoDB callback function wrappers for functions
that will be used outside innodb_api.c */

/*************************************************************//**
Close a cursor
@return DB_SUCCESS if successful or error code */
ib_err_t
innodb_cb_cursor_close(
/*===================*/
	ib_crsr_t	ib_crsr)	/*!< in: cursor to close */
{
	return(ib_cb_cursor_close(ib_crsr));
}

/*************************************************************//**
Commit the transaction
@return DB_SUCCESS if successful or error code */
ib_err_t
innodb_cb_trx_commit(
/*=================*/
	ib_trx_t	ib_trx)		/*!< in: transaction to commit */
{
	return(ib_cb_trx_commit(ib_trx));
}

/*************************************************************//**
Close table associated to the connection
@return DB_SUCCESS if successful or error code */
ib_err_t
innodb_cb_close_thd(
/*=================*/
	void*		thd)		/*!<in: THD */
{
	return(ib_cb_close_thd(thd));
}

/*************************************************************//**
Start a transaction
@return DB_SUCCESS if successful or error code */
ib_trx_t
innodb_cb_trx_begin(
/*================*/
	ib_trx_level_t	ib_trx_level)	/*!< in:  trx isolation level */
{
	return(ib_cb_trx_begin(ib_trx_level));
}

/*****************************************************************//**
update the cursor with new transactions and also reset the cursor
@return DB_SUCCESS or err code */
ib_err_t
innodb_cb_cursor_new_trx(
/*=====================*/
	ib_crsr_t	ib_crsr,	/*!< in/out: InnoDB cursor */
	ib_trx_t	ib_trx)		/*!< in: transaction */
{
	return(ib_cb_cursor_new_trx(ib_crsr, ib_trx));
}

/*****************************************************************//**
Set the Lock an InnoDB cursor/table.
@return DB_SUCCESS or error code */
ib_err_t
innodb_cb_cursor_lock(
/*==================*/
	ib_crsr_t	ib_crsr,	/*!< in/out: InnoDB cursor */
	ib_lck_mode_t	ib_lck_mode)	/*!< in: InnoDB lock mode */
{

	return(ib_cb_cursor_lock(ib_crsr, ib_lck_mode));
}

/*****************************************************************//**
Create an InnoDB tuple used for index/table search.
@return own: Tuple for current index */
ib_tpl_t
innodb_cb_read_tuple_create(
/*========================*/
	ib_crsr_t	ib_crsr)	/*!< in: Cursor instance */
{
	return(ib_cb_read_tuple_create(ib_crsr));
}

/*****************************************************************//**
Move cursor to the first record in the table.
@return DB_SUCCESS or err code */
ib_err_t
innodb_cb_cursor_first(
/*===================*/
	ib_crsr_t	ib_crsr)	/*!< in: InnoDB cursor instance */
{
	return(ib_cb_cursor_first(ib_crsr));
}

/*****************************************************************//**
Read current row.
@return DB_SUCCESS or err code */
ib_err_t
innodb_cb_read_row(
/*===============*/
	ib_crsr_t	ib_crsr,	/*!< in: InnoDB cursor instance */
	ib_tpl_t	ib_tpl)		/*!< out: read cols into this tuple */
{
	return(ib_cb_read_row(ib_crsr, ib_tpl));
}

/*****************************************************************//**
Get a column type, length and attributes from the tuple.
@return len of column data */
ib_ulint_t
innodb_cb_col_get_meta(
/*===================*/
	ib_tpl_t	ib_tpl,		/*!< in: tuple instance */
	ib_ulint_t	i,		/*!< in: column index in tuple */
	ib_col_meta_t*	ib_col_meta)	/*!< out: column meta data */
{
	return(ib_cb_col_get_meta(ib_tpl, i, ib_col_meta));
}

/*****************************************************************//**
Destroy an InnoDB tuple. */
void
innodb_cb_tuple_delete(
/*===================*/
	ib_tpl_t	ib_tpl)		/*!< in,own: Tuple instance to delete */
{
	ib_cb_tuple_delete(ib_tpl);
	return;
}

/*****************************************************************//**
Return the number of columns in the tuple definition.
@return number of columns */
ib_ulint_t
innodb_cb_tuple_get_n_cols(
/*=======================*/
	const ib_tpl_t	ib_tpl)		/*!< in: Tuple for table/index */
{
	return(ib_cb_tuple_get_n_cols(ib_tpl));
}

/*****************************************************************//**
Get a column value pointer from the tuple.
@return NULL or pointer to buffer */
const void*
innodb_cb_col_get_value(
/*====================*/
	ib_tpl_t	ib_tpl,		/*!< in: tuple instance */
	ib_ulint_t	i)		/*!< in: column index in tuple */
{
	return(ib_cb_col_get_value(ib_tpl, i));
}

/********************************************************************//**
Open a table using the table name.
@return table instance if found */
ib_err_t
innodb_cb_open_table(
/*=================*/
	const char*	name,		/*!< in: table name to lookup */
	ib_trx_t	ib_trx,		/*!< in: transaction */
	ib_crsr_t*	ib_crsr)	/*!< in: cursor to be used */
{
	return(ib_cb_open_table(name, ib_trx, ib_crsr));
}

/*****************************************************************//**
Get a column name from the tuple.
@return name of the column */
char*
innodb_cb_col_get_name(
/*===================*/
	ib_crsr_t	ib_crsr,	/*!< in: InnoDB cursor instance */
	ib_ulint_t	i)		/*!< in: column index in tuple */
{
	return(ib_cb_col_get_name(ib_crsr, i));
}

/*****************************************************************//**
Open an InnoDB secondary index cursor and return a cursor handle to it.
@return DB_SUCCESS or err code */
ib_err_t
innodb_cb_cursor_open_index_using_name(
/*===================================*/
	ib_crsr_t	ib_open_crsr,	/*!< in: open/active cursor */
	const char*	index_name,	/*!< in: secondary index name */
	ib_crsr_t*	ib_crsr,	/*!< out,own: InnoDB index cursor */
	int*		idx_type,	/*!< out: index is cluster index */
	ib_id_u64_t*	idx_id)		/*!< out: index id */
{
	return(ib_cb_cursor_open_index_using_name(ib_open_crsr, index_name,
						  ib_crsr, idx_type, idx_id));
}

/*****************************************************************//**
Check whether the binlog option is turned on
(innodb_direct_access_enable_binlog)
@return TRUE if on */
bool
innodb_cb_binlog_enabled()
/*======================*/
{
	return((bool) ib_cb_binlog_enabled());
}
