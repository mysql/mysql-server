/***********************************************************************

Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

***********************************************************************/

/**************************************************/ /**
 @file
 InnoDB APIs to support memcached commands

 Created 04/12/2011 Jimmy Yang
 *******************************************************/

#include "innodb_api.h"
#include <errno.h>
#include <innodb_cb_api.h>
#include <innodb_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "memcached/util.h"

/** Whether to update all columns' value or a specific column value */
#define UPDATE_ALL_VAL_COL -1

extern bool release_mdl_lock;

extern option_t config_option_names[];

cb_open_table_t ib_cb_open_table;
cb_read_row_t ib_cb_read_row;
cb_insert_row_t ib_cb_insert_row;
cb_cursor_delete_row_t ib_cb_delete_row;
cb_cursor_update_row_t ib_cb_update_row;
cb_cursor_moveto_t ib_cb_moveto;
cb_sec_search_tuple_create_t ib_cb_search_tuple_create;
cb_sec_read_tuple_create_t ib_cb_read_tuple_create;
cb_tuple_delete_t ib_cb_tuple_delete;
cb_tuple_read_u8_t ib_cb_tuple_read_u8;
cb_tuple_read_u16_t ib_cb_tuple_read_u16;
cb_tuple_read_u32_t ib_cb_tuple_read_u32;
cb_tuple_read_u64_t ib_cb_tuple_read_u64;
cb_tuple_read_i8_t ib_cb_tuple_read_i8;
cb_tuple_read_i16_t ib_cb_tuple_read_i16;
cb_tuple_read_i32_t ib_cb_tuple_read_i32;
cb_tuple_read_i64_t ib_cb_tuple_read_i64;
cb_col_set_value_t ib_cb_col_set_value;
cb_col_get_value_t ib_cb_col_get_value;
cb_col_get_meta_t ib_cb_col_get_meta;
cb_trx_begin_t ib_cb_trx_begin;
cb_trx_commit_t ib_cb_trx_commit;
cb_trx_rollback_t ib_cb_trx_rollback;
cb_trx_start_t ib_cb_trx_start;
cb_trx_release_t ib_cb_trx_release;
cb_tuple_get_n_cols_t ib_cb_tuple_get_n_cols;
cb_cursor_set_match_mode_t ib_cb_cursor_set_match_mode;
cb_cursor_lock_t ib_cb_cursor_lock;
cb_cursor_close_t ib_cb_cursor_close;
cb_cursor_new_trx_t ib_cb_cursor_new_trx;
cb_cursor_reset_t ib_cb_cursor_reset;
cb_col_get_name_t ib_cb_col_get_name;
cb_get_idx_field_name ib_cb_get_idx_field_name;
cb_cursor_first_t ib_cb_cursor_first;
cb_cursor_next_t ib_cb_cursor_next;
cb_cursor_open_index_using_name_t ib_cb_cursor_open_index_using_name;
cb_close_thd_t ib_cb_close_thd;
cb_get_cfg_t ib_cb_get_cfg;
cb_cursor_set_cluster_access_t ib_cb_cursor_set_cluster_access;
cb_cursor_commit_trx_t ib_cb_cursor_commit_trx;
cb_cfg_trx_level_t ib_cb_cfg_trx_level;
cb_get_n_user_cols ib_cb_get_n_user_cols;
cb_cursor_set_lock_t ib_cb_cursor_set_lock;
cb_trx_get_start_time ib_cb_trx_get_start_time;
cb_bk_commit_interval ib_cb_cfg_bk_commit_interval;
cb_ut_strerr ib_cb_ut_strerr;
cb_cursor_stmt_begin ib_cb_cursor_stmt_begin;
#ifdef UNIV_MEMCACHED_SDI
cb_sdi_get ib_cb_sdi_get;
cb_sdi_delete ib_cb_sdi_delete;
cb_sdi_set ib_cb_sdi_set;
cb_sdi_create ib_cb_sdi_create;
cb_sdi_drop ib_cb_sdi_drop;
cb_sdi_get_keys ib_cb_sdi_get_keys;
#endif /* UNIV_MEMCACHED_SDI */
cb_trx_read_only_t ib_cb_trx_read_only;
cb_is_virtual_table ib_cb_is_virtual_table;

/** InnoDB API callback functions */
static ib_cb_t *innodb_memcached_api[] = {
    (ib_cb_t *)&ib_cb_open_table,
    (ib_cb_t *)&ib_cb_read_row,
    (ib_cb_t *)&ib_cb_insert_row,
    (ib_cb_t *)&ib_cb_delete_row,
    (ib_cb_t *)&ib_cb_update_row,
    (ib_cb_t *)&ib_cb_moveto,
    (ib_cb_t *)&ib_cb_cursor_first,
    (ib_cb_t *)&ib_cb_cursor_next,
    (ib_cb_t *)&ib_cb_cursor_set_match_mode,
    (ib_cb_t *)&ib_cb_search_tuple_create,
    (ib_cb_t *)&ib_cb_read_tuple_create,
    (ib_cb_t *)&ib_cb_tuple_delete,
    (ib_cb_t *)&ib_cb_tuple_read_u8,
    (ib_cb_t *)&ib_cb_tuple_read_u16,
    (ib_cb_t *)&ib_cb_tuple_read_u32,
    (ib_cb_t *)&ib_cb_tuple_read_u64,
    (ib_cb_t *)&ib_cb_tuple_read_i8,
    (ib_cb_t *)&ib_cb_tuple_read_i16,
    (ib_cb_t *)&ib_cb_tuple_read_i32,
    (ib_cb_t *)&ib_cb_tuple_read_i64,
    (ib_cb_t *)&ib_cb_tuple_get_n_cols,
    (ib_cb_t *)&ib_cb_col_set_value,
    (ib_cb_t *)&ib_cb_col_get_value,
    (ib_cb_t *)&ib_cb_col_get_meta,
    (ib_cb_t *)&ib_cb_trx_begin,
    (ib_cb_t *)&ib_cb_trx_commit,
    (ib_cb_t *)&ib_cb_trx_rollback,
    (ib_cb_t *)&ib_cb_trx_start,
    (ib_cb_t *)&ib_cb_trx_release,
    (ib_cb_t *)&ib_cb_cursor_lock,
    (ib_cb_t *)&ib_cb_cursor_close,
    (ib_cb_t *)&ib_cb_cursor_new_trx,
    (ib_cb_t *)&ib_cb_cursor_reset,
    (ib_cb_t *)&ib_cb_col_get_name,
    (ib_cb_t *)&ib_cb_cursor_open_index_using_name,
    (ib_cb_t *)&ib_cb_get_cfg,
    (ib_cb_t *)&ib_cb_cursor_set_cluster_access,
    (ib_cb_t *)&ib_cb_cursor_commit_trx,
    (ib_cb_t *)&ib_cb_cfg_trx_level,
    (ib_cb_t *)&ib_cb_get_n_user_cols,
    (ib_cb_t *)&ib_cb_cursor_set_lock,
    (ib_cb_t *)&ib_cb_get_idx_field_name,
    (ib_cb_t *)&ib_cb_trx_get_start_time,
    (ib_cb_t *)&ib_cb_cfg_bk_commit_interval,
    (ib_cb_t *)&ib_cb_ut_strerr,
    (ib_cb_t *)&ib_cb_cursor_stmt_begin,
#ifdef UNIV_MEMCACHED_SDI
    (ib_cb_t *)&ib_cb_sdi_get,
    (ib_cb_t *)&ib_cb_sdi_delete,
    (ib_cb_t *)&ib_cb_sdi_set,
    (ib_cb_t *)&ib_cb_sdi_create,
    (ib_cb_t *)&ib_cb_sdi_drop,
    (ib_cb_t *)&ib_cb_sdi_get_keys,
#endif /* UNIV_MEMCACHED_SDI */
    (ib_cb_t *)&ib_cb_trx_read_only,
    (ib_cb_t *)&ib_cb_is_virtual_table};

/** Set expiration time. If the exp sent by client is larger than
60*60*24*30 (number of seconds in 30 days), it  will be considered
to be real Unix time value rather than an offset from current time */
#define SET_EXP_TIME(exp)          \
  if (exp) {                       \
    if (exp < 60 * 60 * 24 * 30) { \
      exp += mci_get_time();       \
    }                              \
  }

/*************************************************************/ /**
 Register InnoDB Callback functions */
void register_innodb_cb(
    /*===============*/
    void *p) /*!<in: Pointer to callback function array */
{
  int i;
  int array_size;
  ib_cb_t *func_ptr = (ib_cb_t *)p;

  array_size = sizeof(innodb_memcached_api) / sizeof(*innodb_memcached_api);

  for (i = 0; i < array_size; i++) {
    *innodb_memcached_api[i] = *(ib_cb_t *)func_ptr;
    func_ptr++;
  }
}

/*************************************************************/ /**
 Open a table and return a cursor for the table.
 @return DB_SUCCESS if table successfully opened */
ib_err_t innodb_api_begin(
    /*=============*/
    innodb_engine_t *engine,       /*!< in: InnoDB Memcached engine */
    const char *dbname,            /*!< in: NUL terminated database name */
    const char *name,              /*!< in: NUL terminated table name */
    innodb_conn_data_t *conn_data, /*!< in/out: connnection specific
                                   data */
    ib_trx_t ib_trx,               /*!< in: transaction */
    ib_crsr_t *crsr,               /*!< out: innodb cursor */
    ib_crsr_t *idx_crsr,           /*!< out: innodb index cursor */
    ib_lck_mode_t lock_mode)       /*!< in:  lock mode */
{
  ib_err_t err = DB_SUCCESS;
  char table_name[MAX_TABLE_NAME_LEN + MAX_DATABASE_NAME_LEN];

  if (!*crsr) {
#ifdef _WIN32
    sprintf(table_name, "%s\%s", dbname, name);
#else
    snprintf(table_name, sizeof(table_name), "%s/%s", dbname, name);
#endif

    err = ib_cb_open_table(table_name, ib_trx, crsr);

    if (err != DB_SUCCESS) {
      fprintf(stderr,
              " InnoDB_Memcached: Unable to open"
              " table '%s'\n",
              table_name);
      return (err);
    }

    /* If MDL is enabled, we need to create mysql handler. */
    if (engine) {
      if (lock_mode == IB_LOCK_NONE) {
        /* Skip MDL locking for enabling reads */
      } else if (conn_data && (engine->enable_binlog || engine->enable_mdl ||
                               lock_mode == IB_LOCK_TABLE_X)) {
        /* Create a "Fake" THD if binlog is enabled */
        /* For flush_all which request IB_LOCK_TABLE_X
        lock, we need to add MDL lock. It's because we need
        to block DMLs from sql layer. */

        if (!conn_data->thd) {
          conn_data->thd = handler_create_thd(engine->enable_binlog);

          if (!conn_data->thd) {
            innodb_cb_cursor_close(*crsr);
            *crsr = NULL;
            return (DB_ERROR);
          }
        }

        if (!conn_data->mysql_tbl) {
          int lock_type =
              (lock_mode == IB_LOCK_TABLE_X ? HDL_FLUSH : HDL_WRITE);
          conn_data->mysql_tbl =
              handler_open_table(conn_data->thd, dbname, name, lock_type);
        }
      }
    }

    err = innodb_cb_cursor_lock(engine, *crsr, lock_mode);

    if (err != DB_SUCCESS) {
      fprintf(stderr,
              " InnoDB_Memcached: Fail to lock"
              " table '%s'\n",
              table_name);
      return (err);
    }

    if (engine) {
      meta_cfg_info_t *meta_info = conn_data->conn_meta;
      meta_index_t *meta_index = &meta_info->index_info;

      if (!engine->enable_mdl || !conn_data->mysql_tbl) {
        err = innodb_verify_low(meta_info, *crsr, true);

        if (err != DB_SUCCESS) {
          fprintf(stderr,
                  " InnoDB_Memcached:"
                  " Table definition"
                  " modified for"
                  " table '%s'\n",
                  table_name);
          return (err);
        }
      }

      /* Open the cursor */
      if (meta_index->srch_use_idx == META_USE_SECONDARY) {
        int index_type;
        ib_id_u64_t index_id;

        ib_cb_cursor_open_index_using_name(*crsr, meta_index->idx_name,
                                           idx_crsr, &index_type, &index_id);

        err = innodb_cb_cursor_lock(engine, *idx_crsr, lock_mode);
      }
    }
  } else {
    ib_cb_cursor_new_trx(*crsr, ib_trx);

    err = innodb_cb_cursor_lock(engine, *crsr, lock_mode);

    if (err != DB_SUCCESS) {
      fprintf(stderr,
              " InnoDB_Memcached: Fail to lock"
              " table '%s'\n",
              name);
      return (err);
    }

    if (engine) {
      meta_cfg_info_t *meta_info = conn_data->conn_meta;
      meta_index_t *meta_index = &meta_info->index_info;

      /* set up secondary index cursor */
      if (meta_index->srch_use_idx == META_USE_SECONDARY) {
        ib_cb_cursor_new_trx(*idx_crsr, ib_trx);
        err = innodb_cb_cursor_lock(engine, *idx_crsr, lock_mode);
      }
    }
  }

  return (err);
}

/*************************************************************/ /**
 Read an unsigned int64 value from an InnoDB tuple
 @return integer value fetched */
static uint64_t innodb_api_read_uint64(
    /*===================*/
    const ib_col_meta_t *m_col, /*!< in: column info */
    ib_tpl_t read_tpl,          /*!< in: tuple to read */
    int i)                      /*!< in: column number */
{
  uint64_t value64;

  assert(m_col->type == IB_INT && m_col->type_len == sizeof(uint64_t) &&
         m_col->attr & IB_COL_UNSIGNED);

  ib_cb_tuple_read_u64(read_tpl, i, &value64);

  return (value64);
}

/*************************************************************/ /**
 Read an integer value from an InnoDB tuple
 @return integer value fetched */
static int64_t innodb_api_read_int(
    /*================*/
    const ib_col_meta_t *m_col, /*!< in: column info */
    ib_tpl_t read_tpl,          /*!< in: tuple to read */
    int i)                      /*!< in: column number */
{
  int64_t value = 0;

  assert(m_col->type == IB_INT);
  assert(m_col->type_len == sizeof(uint64_t) ||
         m_col->type_len == sizeof(uint32_t) ||
         m_col->type_len == sizeof(uint16_t) ||
         m_col->type_len == sizeof(uint8_t));

  if (m_col->attr & IB_COL_UNSIGNED) {
    if (m_col->type_len == sizeof(uint64_t)) {
      /* We handle uint64 in innodb_api_read_uint64 */
      assert(0);
    } else if (m_col->type_len == sizeof(uint32_t)) {
      uint32_t value32;
      ib_cb_tuple_read_u32(read_tpl, i, &value32);
      value = (int64_t)value32;
    } else if (m_col->type_len == sizeof(uint16_t)) {
      uint16_t value16;
      ib_cb_tuple_read_u16(read_tpl, i, &value16);
      value = (int64_t)value16;
    } else if (m_col->type_len == sizeof(uint8_t)) {
      uint8_t value8;
      ib_cb_tuple_read_u8(read_tpl, i, &value8);
      value = (int64_t)value8;
    }
  } else {
    if (m_col->type_len == sizeof(int64_t)) {
      ib_cb_tuple_read_i64(read_tpl, i, &value);
    } else if (m_col->type_len == sizeof(int32_t)) {
      int32_t value32;
      ib_cb_tuple_read_i32(read_tpl, i, &value32);
      value = (int64_t)value32;
    } else if (m_col->type_len == sizeof(int16_t)) {
      int16_t value16;
      ib_cb_tuple_read_i16(read_tpl, i, &value16);
      value = (int64_t)value16;
    } else if (m_col->type_len == sizeof(int8_t)) {
      int8_t value8;
      ib_cb_tuple_read_i8(read_tpl, i, &value8);
      value = (int64_t)value8;
    }
  }

  return (value);
}

/*************************************************************/ /**
 set up an integer type tuple field for write
 @return DB_SUCCESS if successful otherwise, error code */
static ib_err_t innodb_api_write_int(
    /*=================*/
    ib_tpl_t tpl,  /*!< in/out: tuple to set */
    int field,     /*!< in: field to set */
    int64_t value, /*!< in: value */
    void *table)   /*!< in/out: MySQL table. Only needed
                   when binlog is enabled */
{
  ib_col_meta_t col_meta;
  ib_col_meta_t *m_col = &col_meta;
  void *src = NULL;
  int64_t value_i64;
  uint32_t value_u32;
  int32_t value_i32;
  uint16_t value_u16;
  int16_t value_i16;
  uint8_t value_u8;
  int8_t value_i8;

  ib_cb_col_get_meta(tpl, field, m_col);

  assert(m_col->type == IB_INT);
  assert(m_col->type_len == 8 || m_col->type_len == 4 || m_col->type_len == 2 ||
         m_col->type_len == 1);

  if (m_col->attr & IB_COL_UNSIGNED) {
    if (m_col->type_len == 8) {
      src = &value;

      /* If table is non-NULL, set up corresponding
      TABLE->record[0] field for replication */
      if (table) {
        handler_rec_setup_uint64(table, field, value, true, false);
      }
    } else if (m_col->type_len == 4) {
      value_u32 = (uint32_t)value;

      src = &value_u32;
      if (table) {
        handler_rec_setup_int(table, field, value_u32, true, false);
      }
    } else if (m_col->type_len == 2) {
      value_u16 = (uint16_t)value;

      src = &value_u16;
      if (table) {
        handler_rec_setup_int(table, field, value_u16, true, false);
      }
    } else if (m_col->type_len == 1) {
      value_u8 = (uint8_t)value;

      src = &value_u8;
      if (table) {
        handler_rec_setup_int(table, field, value_u8, true, false);
      }
    }
  } else {
    if (m_col->type_len == 8) {
      value_i64 = (int64_t)value;

      src = &value_i64;
      if (table) {
        handler_rec_setup_int(table, field, value_i64, false, false);
      }
    } else if (m_col->type_len == 4) {
      value_i32 = (int32_t)value;

      src = &value_i32;
      if (table) {
        handler_rec_setup_int(table, field, value_i32, false, false);
      }
    } else if (m_col->type_len == 2) {
      value_i16 = (int16_t)value;

      src = &value_i16;
      if (table) {
        handler_rec_setup_int(table, field, value_i16, false, false);
      }
    } else if (m_col->type_len == 1) {
      value_i8 = (int8_t)value;

      src = &value_i8;
      if (table) {
        handler_rec_setup_int(table, field, value_i8, false, false);
      }
    }
  }

  ib_cb_col_set_value(tpl, field, src, m_col->type_len, true);
  return (DB_SUCCESS);
}

/*************************************************************/ /**
 set up an unsigned int64 type tuple field for write
 @return DB_SUCCESS if successful otherwise, error code */
static ib_err_t innodb_api_write_uint64(
    /*====================*/
    ib_tpl_t tpl,   /*!< in/out: tuple to set */
    int field,      /*!< in: field to set */
    uint64_t value, /*!< in: value */
    void *table)    /*!< in/out: MySQL table. Only needed
                    when binlog is enabled */
{
  ib_col_meta_t col_meta;
  ib_col_meta_t *m_col = &col_meta;
  void *src = NULL;

  ib_cb_col_get_meta(tpl, field, m_col);

  assert(m_col->type == IB_INT && m_col->type_len == 8 &&
         m_col->attr & IB_COL_UNSIGNED);

  src = &value;

  /* If table is non-NULL, set up corresponding
  TABLE->record[0] field for replication */
  if (table) {
    handler_rec_setup_uint64(table, field, value, true, false);
  }

  ib_cb_col_set_value(tpl, field, src, m_col->type_len, true);
  return (DB_SUCCESS);
}

/*************************************************************/ /**
 set up a tuple field
 @return DB_SUCCESS if successful otherwise, error code */
static ib_err_t innodb_api_setup_field_value(
    /*=========================*/
    ib_tpl_t tpl,            /*!< in/out: tuple to set */
    int field_id,            /*!< in: field to set */
    meta_column_t *col_info, /*!< in: insert col info */
    const char *value,       /*!< in: value */
    ib_ulint_t val_len,      /*!< in: value length */
    void *table,             /*!< in/out: MySQL table. Only needed
                             when binlog is enabled */
    bool need_cpy)           /*!< in: if need memcpy */
{
  ib_err_t err = DB_ERROR;

  /* if value is null, we just set the field to NULL. */
  if (val_len == IB_SQL_NULL) {
    assert(value == NULL);
    err = ib_cb_col_set_value(tpl, field_id, value, val_len, need_cpy);
    return (err);
  }

  if (col_info->col_meta.type == IB_INT) {
    char *end_ptr;
    char val_buf[256];

    /* Need this memcpy to strip the junk */
    memcpy(val_buf, value, val_len);
    val_buf[val_len] = 0;

    if (col_info->col_meta.attr & IB_COL_UNSIGNED &&
        col_info->col_meta.type_len == 8) {
      uint64_t int_value = 0;

      int_value = strtoull(val_buf, &end_ptr, 10);

      /* If the value is not a valid string of integer,
      we will return error. */
      if (end_ptr == val_buf) {
        fprintf(stderr,
                " InnoDB_Memcached: Unable to convert"
                " value '%s' to integer\n",
                value);
        return (DB_ERROR);
      }

      err = innodb_api_write_uint64(tpl, field_id, int_value, table);
    } else {
      int64_t int_value = 0;

      int_value = strtoll(val_buf, &end_ptr, 10);
      /* If the value is not a valid string of integer,
      we will return error. */
      if (end_ptr == val_buf) {
        fprintf(stderr,
                " InnoDB_Memcached: Unable to convert"
                " value '%s' to integer\n",
                val_buf);
        return (DB_ERROR);
      }
      err = innodb_api_write_int(tpl, field_id, int_value, table);
    }

  } else {
    err = ib_cb_col_set_value(tpl, field_id, value, val_len, need_cpy);

    if (table) {
      handler_rec_setup_str(table, field_id, value, val_len);
    }
  }

  return (err);
}

/*************************************************************/ /**
 Fetch data from a read tuple and instantiate a "mci_column_t" structure
 @return true if successful */
static bool innodb_api_fill_mci(
    /*================*/
    ib_tpl_t read_tpl,      /*!< in: Read tuple */
    int col_id,             /*!< in: Column ID for the column to
                            read */
    mci_column_t *mci_item) /*!< out: item to fill */
{
  ib_ulint_t data_len;
  ib_col_meta_t col_meta;

  data_len = ib_cb_col_get_meta(read_tpl, col_id, &col_meta);

  if (data_len == IB_SQL_NULL) {
    mci_item->value_str = NULL;
    mci_item->value_len = 0;
    mci_item->is_str = true;
  } else {
    if (col_meta.type == IB_INT) {
      if (col_meta.attr & IB_COL_UNSIGNED && data_len == 8) {
        mci_item->value_int =
            innodb_api_read_uint64(&col_meta, read_tpl, col_id);
      } else {
        mci_item->value_int = innodb_api_read_int(&col_meta, read_tpl, col_id);
      }

      mci_item->value_str = NULL;
      mci_item->value_len = sizeof(mci_item->value_int);
      mci_item->is_str = false;
      mci_item->is_unsigned = (col_meta.attr & IB_COL_UNSIGNED);
    } else {
      mci_item->value_str = (char *)ib_cb_col_get_value(read_tpl, col_id);
      mci_item->value_len = data_len;
      mci_item->is_str = true;
    }
  }

  mci_item->allocated = false;
  mci_item->is_valid = true;

  return (true);
}

/*************************************************************/ /**
 Fetch value from a read cursor into "mci_items"
 @return DB_SUCCESS if successful */
static ib_err_t innodb_api_fill_value(
    /*==================*/
    meta_cfg_info_t *meta_info, /*!< in: Metadata */
    mci_item_t *item,           /*!< out: item to fill */
    ib_tpl_t read_tpl,          /*!< in: read tuple */
    int col_id)                 /*!< in: column Id */
{
  ib_err_t err = DB_NOT_FOUND;

  /* If just read a single "value", fill mci_item[MCI_COL_VALUE],
  otherwise, fill multiple value in extra_col_value[i] */
  if (meta_info->n_extra_col == 0) {
    meta_column_t *col_info = meta_info->col_info;

    if (col_id == col_info[CONTAINER_VALUE].field_id) {
      innodb_api_fill_mci(read_tpl, col_id, &item->col_value[MCI_COL_VALUE]);

      err = DB_SUCCESS;
    }
  } else {
    int i;

    for (i = 0; i < meta_info->n_extra_col; i++) {
      if (col_id == meta_info->extra_col_info[i].field_id) {
        innodb_api_fill_mci(read_tpl, col_id, &item->extra_col_value[i]);

        err = DB_SUCCESS;
        break;
      }
    }
  }

  return (err);
}

/*************************************************************/ /**
 Position a row according to the search key, and fetch value if needed
 @return DB_SUCCESS if successful otherwise, error code */
ib_err_t innodb_api_search(
    /*==============*/
    innodb_conn_data_t *cursor_data, /*!< in/out: cursor info */
    ib_crsr_t *crsr,                 /*!< in/out: cursor used to search */
    const char *key,                 /*!< in: key to search */
    int len,                         /*!< in: key length */
    mci_item_t *item,                /*!< in: result */
    ib_tpl_t *r_tpl,                 /*!< in: tpl for other DML
                                     operations */
    bool sel_only,                   /*!< in: for select only */
    innodb_range_key_t *range_key)   /*!< in: search mode if not exact search */
{
  ib_err_t err = DB_SUCCESS;
  meta_cfg_info_t *meta_info = cursor_data->conn_meta;
  meta_column_t *col_info = meta_info->col_info;
  meta_index_t *meta_index = &meta_info->index_info;
  ib_tpl_t key_tpl;
  ib_tpl_t cmp_tpl = NULL;
  ib_crsr_t srch_crsr;

  if (item) {
    memset(item, 0, sizeof(*item));
  }

  /* If srch_use_idx is set to META_USE_SECONDARY, we will use the
  secondary index to find the record first */
  if (meta_index->srch_use_idx == META_USE_SECONDARY) {
    ib_crsr_t idx_crsr;

    if (sel_only) {
      idx_crsr = cursor_data->idx_read_crsr;
    } else {
      idx_crsr = cursor_data->idx_crsr;
    }

    ib_cb_cursor_set_cluster_access(idx_crsr);

    if (!cursor_data->idx_tpl) {
      key_tpl = ib_cb_search_tuple_create(idx_crsr);
      cursor_data->idx_tpl = key_tpl;
    } else {
      key_tpl = cursor_data->idx_tpl;
    }

    srch_crsr = idx_crsr;

  } else {
    ib_crsr_t crsr;

    if (sel_only) {
      crsr = cursor_data->read_crsr;

      if (!cursor_data->sel_tpl) {
        key_tpl = ib_cb_search_tuple_create(crsr);
        cursor_data->sel_tpl = key_tpl;
      } else {
        key_tpl = cursor_data->sel_tpl;
      }
    } else {
      crsr = cursor_data->crsr;

      if (!cursor_data->tpl) {
        key_tpl = ib_cb_search_tuple_create(crsr);
        cursor_data->tpl = key_tpl;
      } else {
        key_tpl = cursor_data->tpl;
      }
    }

    srch_crsr = crsr;
  }

  /* If it is range select, we will need to setup the upper bound
  compare tuple */
  if (range_key && range_key->bound == RANGE_BOUND) {
    assert(sel_only);

    if (meta_index->srch_use_idx == META_USE_SECONDARY) {
      cmp_tpl = ib_cb_search_tuple_create(cursor_data->idx_read_crsr);
    } else {
      cmp_tpl = ib_cb_search_tuple_create(cursor_data->read_crsr);
    }

    err = innodb_api_setup_field_value(key_tpl, 0, &col_info[CONTAINER_KEY],
                                       range_key->start, range_key->start_len,
                                       NULL, true);

    err = innodb_api_setup_field_value(cmp_tpl, 0, &col_info[CONTAINER_KEY],
                                       range_key->end, range_key->end_len, NULL,
                                       true);
  } else {
    err = innodb_api_setup_field_value(key_tpl, 0, &col_info[CONTAINER_KEY],
                                       key, len, NULL, true);
  }

  if (!range_key) {
    /* Exact search */
    ib_cb_cursor_set_match_mode(srch_crsr, IB_EXACT_MATCH);

    err = ib_cb_moveto(srch_crsr, key_tpl, IB_CUR_GE, 0);
  } else if (range_key->bound == UPPER_BOUND) {
    err = innodb_api_setup_field_value(key_tpl, 0, &col_info[CONTAINER_KEY],
                                       range_key->end, range_key->end_len, NULL,
                                       true);
    /* Range search for < (less than) */
    if (!cursor_data->range) {
      innodb_cb_cursor_first(srch_crsr);
    } else {
      ib_cb_cursor_next(srch_crsr);
    }
    cmp_tpl = key_tpl;
  } else {
    int direction;

    direction = !cursor_data->range ? 0 : 1;

    /* Range search */
    ib_cb_cursor_set_match_mode(srch_crsr, IB_CLOSEST_MATCH);
    err = ib_cb_moveto(srch_crsr, key_tpl,
                       ib_srch_mode_t(range_key->start_mode), direction);
  }

  if (err != DB_SUCCESS) {
    if (r_tpl) {
      *r_tpl = NULL;
    }
    goto func_exit;
  }

  /* If item is NULL, this function is used just to position the cursor.
  Otherwise, fetch the data from the read tuple */
  if (item) {
    ib_tpl_t read_tpl;
    int n_cols;
    int i;

    if (!cursor_data->read_tpl) {
      read_tpl = ib_cb_read_tuple_create(sel_only ? cursor_data->read_crsr
                                                  : cursor_data->crsr);
      cursor_data->read_tpl = read_tpl;
    } else {
      read_tpl = cursor_data->read_tpl;
    }

    err = ib_cb_read_row(srch_crsr, read_tpl, cmp_tpl,
                         range_key ? range_key->end_mode : 0,
                         cursor_data->row_buf, &(cursor_data->row_buf_slot),
                         &(cursor_data->row_buf_used));

    if (err != DB_SUCCESS) {
      if (r_tpl) {
        *r_tpl = NULL;
      }
      goto func_exit;
    }

    if (sel_only) {
      cursor_data->result_in_use = true;
    }

    n_cols = ib_cb_tuple_get_n_cols(read_tpl);

    if (meta_info->n_extra_col > 0) {
      /* If there are multiple values to read,allocate
      memory */
      item->extra_col_value = (mci_column_t *)malloc(
          meta_info->n_extra_col * sizeof(*item->extra_col_value));
      item->n_extra_col = meta_info->n_extra_col;
    } else {
      item->extra_col_value = NULL;
      item->n_extra_col = 0;
    }

    /* The table must have at least MCI_COL_TO_GET(5) columns
    for memcached key, value, flag, cas and time expiration info */
    assert(n_cols >= MCI_COL_TO_GET);

    for (i = 0; i < n_cols; ++i) {
      ib_ulint_t data_len;
      ib_col_meta_t col_meta;

      data_len = ib_cb_col_get_meta(read_tpl, i, &col_meta);

      if (i == col_info[CONTAINER_KEY].field_id) {
        assert(data_len != IB_SQL_NULL);
        item->col_value[MCI_COL_KEY].value_str =
            (char *)ib_cb_col_get_value(read_tpl, i);
        item->col_value[MCI_COL_KEY].value_len = data_len;
        item->col_value[MCI_COL_KEY].is_str = true;
        item->col_value[MCI_COL_KEY].is_valid = true;
      } else if (meta_info->flag_enabled &&
                 i == col_info[CONTAINER_FLAG].field_id) {
        mci_column_t *col_value;
        ib_col_meta_t *col_meta;

        col_value = &(item->col_value[MCI_COL_FLAG]);
        col_meta = &col_info[CONTAINER_FLAG].col_meta;
        if (data_len == IB_SQL_NULL) {
          col_value->is_null = true;
        } else {
          if (col_meta->attr & IB_COL_UNSIGNED && data_len == 8) {
            col_value->value_int =
                innodb_api_read_uint64(col_meta, read_tpl, i);
          } else {
            col_value->value_int = innodb_api_read_int(col_meta, read_tpl, i);
          }
          col_value->is_str = false;
          col_value->value_len = data_len;
          col_value->is_valid = true;
        }
      } else if (meta_info->cas_enabled &&
                 i == col_info[CONTAINER_CAS].field_id) {
        mci_column_t *col_value;
        ib_col_meta_t *col_meta;

        col_value = &(item->col_value[MCI_COL_CAS]);
        col_meta = &col_info[CONTAINER_CAS].col_meta;
        if (data_len == IB_SQL_NULL) {
          col_value->is_null = true;
        } else {
          if (col_meta->attr & IB_COL_UNSIGNED && data_len == 8) {
            col_value->value_int =
                innodb_api_read_uint64(col_meta, read_tpl, i);
          } else {
            /* Since the CAS value * must be
            unsigned, we just cast sout the sign
            value. */
            col_value->value_int = innodb_api_read_int(col_meta, read_tpl, i);
          }
          col_value->is_str = false;
          col_value->value_len = data_len;
          col_value->is_valid = true;
        }
      } else if (meta_info->exp_enabled &&
                 i == col_info[CONTAINER_EXP].field_id) {
        mci_column_t *col_value;
        ib_col_meta_t *col_meta;

        col_value = &(item->col_value[MCI_COL_EXP]);
        col_meta = &col_info[CONTAINER_EXP].col_meta;
        if (data_len == IB_SQL_NULL) {
          col_value->is_null = true;
        } else {
          if (col_meta->attr & IB_COL_UNSIGNED && data_len == 8) {
            col_value->value_int =
                innodb_api_read_uint64(col_meta, read_tpl, i);
          } else {
            col_value->value_int = innodb_api_read_int(col_meta, read_tpl, i);
          }
          col_value->is_str = false;
          col_value->value_len = data_len;
          col_value->is_valid = true;
        }
      }

      if ((meta_info->n_extra_col == 0 &&
           i == col_info[CONTAINER_VALUE].field_id) ||
          meta_info->n_extra_col) {
        innodb_api_fill_value(meta_info, item, read_tpl, i);
      }
    }

    if (r_tpl) {
      *r_tpl = read_tpl;
    } else if (key_tpl && !sel_only) {
      cursor_data->result_in_use = false;
    }
  }

func_exit:
  *crsr = srch_crsr;

  return (err);
}

/** Get montonically increasing cas (check and set) ID.
@param[in]	eng	InnoDB Memcached engine
@return new cas ID */
static uint64_t mci_get_cas(
    /*========*/
    innodb_engine_t *eng
#if defined(HAVE_GCC_ATOMIC_BUILTINS)
    __attribute__((unused))
#endif /* HAVE_GCC_ATOMIC_BUILTINS */
) {
  static uint64_t cas_id = 0;

#if defined(HAVE_GCC_SYNC_BUILTINS)
  return (__sync_add_and_fetch(&cas_id, 1));
#else
  pthread_mutex_lock(&eng->cas_mutex);
  cas_id++;
  pthread_mutex_unlock(&eng->cas_mutex);
  return (cas_id);
#endif
}

/*************************************************************/ /**
 Get current time
 @return time in seconds */
uint64_t mci_get_time(void)
/*==============*/
{
  struct timeval tv;

  /* FIXME: need to address it different when port the project to
  Windows. Please see ut_gettimeofday() */
  gettimeofday(&tv, NULL);

  return ((uint64_t)tv.tv_sec);
}

/*************************************************************/ /**
 Set up a record with multiple columns for insertion
 @return DB_SUCCESS if successful, otherwise, error code */
static ib_err_t innodb_api_set_multi_cols(
    /*======================*/
    ib_tpl_t tpl,               /*!< in: tuple for insert */
    meta_cfg_info_t *meta_info, /*!< in: metadata info */
    char *value,                /*!< in: value to insert */
    int value_len,              /*!< in: value length */
    void *table)                /*!< in: MySQL TABLE* */
{
  ib_err_t err = DB_ERROR;
  meta_column_t *col_info;
  char *last;
  char *col_val;
  char *end;
  int i = 0;
  char *sep;
  size_t sep_len;
  char *my_value;

  if (!value_len) {
    return (DB_SUCCESS);
  }

  col_info = meta_info->extra_col_info;
  my_value = (char *)malloc(value_len + 1);

  if (!my_value) {
    return (DB_ERROR);
  }

  memcpy(my_value, value, value_len);
  my_value[value_len] = 0;
  value = my_value;
  end = value + value_len;

  /* Get the default setting if user did not config it */
  GET_OPTION(meta_info, OPTION_ID_COL_SEP, sep, sep_len);
  assert(sep_len > 0);

  if (value[0] == *sep) {
    err = innodb_api_setup_field_value(tpl, col_info[i].field_id, &col_info[i],
                                       NULL, IB_SQL_NULL, table, true);
    i++;

    if (err != DB_SUCCESS) {
      free(my_value);
      return (err);
    }
    value++;
  }

  /* Input values are separated with "sep" */
  for (col_val = strtok_r(value, sep, &last);
       last <= end && i < meta_info->n_extra_col;
       col_val = strtok_r(NULL, sep, &last), i++) {
    if (!col_val) {
      err =
          innodb_api_setup_field_value(tpl, col_info[i].field_id, &col_info[i],
                                       NULL, IB_SQL_NULL, table, true);
      break;
    } else {
      err =
          innodb_api_setup_field_value(tpl, col_info[i].field_id, &col_info[i],
                                       col_val, strlen(col_val), table, true);

      if (table) {
        handler_rec_setup_str(table, col_info[i].field_id, col_val,
                              strlen(col_val));
      }
    }

    if (err != DB_SUCCESS) {
      break;
    }
  }

  for (; i < meta_info->n_extra_col; i++) {
    err = innodb_api_setup_field_value(tpl, col_info[i].field_id, &col_info[i],
                                       NULL, IB_SQL_NULL, table, true);

    if (err != DB_SUCCESS) {
      break;
    }
  }

  free(my_value);
  return (err);
}

/*************************************************************/ /**
 Set up a MySQL "TABLE" record in table->record[0] for binlogging */
static void innodb_api_setup_hdl_rec(
    /*=====================*/
    mci_item_t *item,        /*!< in: item contain data
                             to set on table->record[0] */
    meta_column_t *col_info, /*!< in: column information */
    void *table)             /*!< out: MySQL TABLE* */
{
  int i;

  for (i = 0; i < MCI_COL_TO_GET; i++) {
    if (item->col_value[i].is_str) {
      handler_rec_setup_str(table, col_info[CONTAINER_KEY + i].field_id,
                            item->col_value[i].value_str,
                            item->col_value[i].value_len);
    } else {
      handler_rec_setup_int(table, col_info[CONTAINER_KEY + i].field_id,
                            item->col_value[i].value_int, true,
                            item->col_value[i].is_null);
    }
  }
}

/*************************************************************/ /**
 Set up an insert tuple
 @return DB_SUCCESS if successful otherwise, error code */
static ib_err_t innodb_api_set_tpl(
    /*===============*/
    ib_tpl_t tpl,               /*!< in/out: tuple for insert */
    meta_cfg_info_t *meta_info, /*!< in: metadata info */
    meta_column_t *col_info,    /*!< in: insert col info */
    const char *key,            /*!< in: key */
    int key_len,                /*!< in: key length */
    const char *value,          /*!< in: value to insert */
    int value_len,              /*!< in: value length */
    uint64_t cas,               /*!< in: cas */
    uint64_t exp,               /*!< in: expiration */
    uint64_t flag,              /*!< in: flag */
    int col_to_set,             /*!< in: column to set */
    void *table,                /*!< in: MySQL TABLE* */
    bool need_cpy)              /*!< in: if need memcpy */
{
  ib_err_t err = DB_ERROR;

  /* If "table" is not NULL, we need to setup MySQL record
  for binlogging */
  if (table) {
    handler_rec_init(table);
  }

  err = innodb_api_setup_field_value(tpl, col_info[CONTAINER_KEY].field_id,
                                     &col_info[CONTAINER_KEY], key, key_len,
                                     table, need_cpy);

  if (err != DB_SUCCESS) {
    return (err);
  }

  assert(err == DB_SUCCESS);

  if (meta_info->n_extra_col > 0) {
    if (col_to_set == UPDATE_ALL_VAL_COL) {
      err = innodb_api_set_multi_cols(tpl, meta_info, (char *)value, value_len,
                                      table);
    } else {
      err = innodb_api_setup_field_value(
          tpl, meta_info->extra_col_info[col_to_set].field_id,
          &meta_info->extra_col_info[col_to_set], (char *)value, value_len,
          table, need_cpy);
    }
  } else {
    err = innodb_api_setup_field_value(tpl, col_info[CONTAINER_VALUE].field_id,
                                       &col_info[CONTAINER_VALUE], value,
                                       value_len, table, need_cpy);
  }

  if (err != DB_SUCCESS) {
    return (err);
  }

  if (meta_info->cas_enabled) {
    err =
        innodb_api_write_int(tpl, col_info[CONTAINER_CAS].field_id, cas, table);
    if (err != DB_SUCCESS) {
      return (err);
    }
  }

  if (meta_info->exp_enabled) {
    err =
        innodb_api_write_int(tpl, col_info[CONTAINER_EXP].field_id, exp, table);
    if (err != DB_SUCCESS) {
      return (err);
    }
  }

  if (meta_info->flag_enabled) {
    err = innodb_api_write_int(tpl, col_info[CONTAINER_FLAG].field_id, flag,
                               table);
    if (err != DB_SUCCESS) {
      return (err);
    }
  }

  return (err);
}

/*************************************************************/ /**
 Insert a row
 @return DB_SUCCESS if successful, otherwise, error code */
ib_err_t innodb_api_insert(
    /*==============*/
    innodb_engine_t *engine,         /*!< in: InnoDB Memcached engine */
    innodb_conn_data_t *cursor_data, /*!< in/out: cursor info */
    const char *key,                 /*!< in: key and value to insert */
    int len,                         /*!< in: key length */
    uint32_t val_len,                /*!< in: value length */
    uint64_t exp,                    /*!< in: expiration time */
    uint64_t *cas,                   /*!< in/out: cas value */
    uint64_t flags)                  /*!< in: flags */
{
  uint64_t new_cas;
  ib_err_t err = DB_ERROR;
  ib_tpl_t tpl = NULL;
  meta_cfg_info_t *meta_info = cursor_data->conn_meta;
  meta_column_t *col_info = meta_info->col_info;

  new_cas = mci_get_cas(engine);

  tpl = ib_cb_read_tuple_create(cursor_data->crsr);
  assert(tpl != NULL);

  /* Set expiration time */
  SET_EXP_TIME(exp);

  assert(!cursor_data->mysql_tbl || engine->enable_binlog ||
         engine->enable_mdl);

  err = innodb_api_set_tpl(
      tpl, meta_info, col_info, key, len, key + len, val_len, new_cas, exp,
      flags, UPDATE_ALL_VAL_COL,
      engine->enable_binlog ? cursor_data->mysql_tbl : NULL, false);

  if (err == DB_SUCCESS) {
    err = ib_cb_insert_row(cursor_data->crsr, tpl);
  }

  if (err == DB_SUCCESS) {
    *cas = new_cas;

    if (engine->enable_binlog && cursor_data->mysql_tbl) {
      handler_binlog_row(cursor_data->thd, cursor_data->mysql_tbl, HDL_INSERT);
    }
  }
  ib_cb_tuple_delete(tpl);

  return (err);
}

/*************************************************************/ /**
 Update a row, called by innodb_api_store(), it is used by memcached's
 "replace", "prepend", "append" and "set" commands
 @return DB_SUCCESS if successful, otherwise, error code */
static ib_err_t innodb_api_update(
    /*==============*/
    innodb_engine_t *engine,         /*!< in: InnoDB Memcached engine */
    innodb_conn_data_t *cursor_data, /*!< in/out: cursor info */
    ib_crsr_t srch_crsr,             /*!< in: cursor to use for write */
    const char *key,                 /*!< in: key and value to insert */
    int len,                         /*!< in: key length */
    uint32_t val_len,                /*!< in: value length */
    uint64_t exp,                    /*!< in: expire time */
    uint64_t *cas,                   /*!< in/out: cas value */
    uint64_t flags,                  /*!< in: flags */
    ib_tpl_t old_tpl,                /*!< in: tuple being updated */
    mci_item_t *result)              /*!< in: item info for the tuple being
                                     updated */
{
  uint64_t new_cas;
  ib_tpl_t new_tpl;
  meta_cfg_info_t *meta_info = cursor_data->conn_meta;
  meta_column_t *col_info = meta_info->col_info;
  ib_err_t err = DB_SUCCESS;

  assert(old_tpl != NULL);

  new_tpl = ib_cb_read_tuple_create(cursor_data->crsr);
  assert(new_tpl != NULL);

  /* cas will be updated for each update */
  new_cas = mci_get_cas(engine);

  SET_EXP_TIME(exp);

  if (engine->enable_binlog) {
    innodb_api_setup_hdl_rec(result, col_info, cursor_data->mysql_tbl);
    handler_store_record(cursor_data->mysql_tbl);
  }

  assert(!cursor_data->mysql_tbl || engine->enable_binlog ||
         engine->enable_mdl);

  err = innodb_api_set_tpl(
      new_tpl, meta_info, col_info, key, len, key + len, val_len, new_cas, exp,
      flags, UPDATE_ALL_VAL_COL,
      engine->enable_binlog ? cursor_data->mysql_tbl : NULL, true);

  if (err == DB_SUCCESS) {
    err = ib_cb_update_row(srch_crsr, old_tpl, new_tpl);
  }

  if (err == DB_SUCCESS) {
    *cas = new_cas;

    if (engine->enable_binlog) {
      assert(cursor_data->mysql_tbl);

      handler_binlog_row(cursor_data->thd, cursor_data->mysql_tbl, HDL_UPDATE);
    }
  }

  ib_cb_tuple_delete(new_tpl);

  return (err);
}

/*************************************************************/ /**
 Delete a row, support the memcached "remove" command
 @return ENGINE_SUCCESS if successful otherwise, error code */
ENGINE_ERROR_CODE
innodb_api_delete(
    /*==============*/
    innodb_engine_t *engine,         /*!< in: InnoDB Memcached engine */
    innodb_conn_data_t *cursor_data, /*!< in/out: cursor info */
    const char *key,                 /*!< in: value to insert */
    int len)                         /*!< in: value length */
{
  ib_err_t err = DB_SUCCESS;
  ib_crsr_t srch_crsr = cursor_data->crsr;
  mci_item_t result;
  ib_tpl_t tpl_delete;

  /* First look for the record, and check whether it exists */
  err = innodb_api_search(cursor_data, &srch_crsr, key, len, &result,
                          &tpl_delete, false, 0);

  if (err != DB_SUCCESS) {
    return (ENGINE_KEY_ENOENT);
  }

  /* The "result" structure contains only pointers to the data value
  when returning from innodb_api_search(), so store the delete row info
  before calling ib_cb_delete_row() */
  if (engine->enable_binlog) {
    meta_cfg_info_t *meta_info = cursor_data->conn_meta;
    meta_column_t *col_info = meta_info->col_info;

    assert(cursor_data->mysql_tbl);

    innodb_api_setup_hdl_rec(&result, col_info, cursor_data->mysql_tbl);
  }

  err = ib_cb_delete_row(srch_crsr);

  /* Do the binlog of the row being deleted */
  if (engine->enable_binlog) {
    if (err == DB_SUCCESS) {
      handler_binlog_row(cursor_data->thd, cursor_data->mysql_tbl, HDL_DELETE);
    }
  }

  return (err == DB_SUCCESS ? ENGINE_SUCCESS : ENGINE_KEY_ENOENT);
}

/*************************************************************/ /**
 Link the value with a string, called by innodb_api_store(), and
 used for memcached's "prepend" or "append" commands
 @return DB_SUCCESS if successful, otherwise, error code */
static ib_err_t innodb_api_link(
    /*============*/
    innodb_engine_t *engine,         /*!< in: InnoDB Memcached engine */
    innodb_conn_data_t *cursor_data, /*!< in/out: cursor info */
    ib_crsr_t srch_crsr,             /*!< in: cursor to use for write */
    const char *key,                 /*!< in: key value */
    int len,                         /*!< in: key length */
    uint32_t val_len,                /*!< in: value length */
    uint64_t exp,                    /*!< in: expire time */
    uint64_t *cas,                   /*!< out: cas value */
    uint64_t flags,                  /*!< in: flags */
    bool append,                     /*!< in: Whether to append or prepend
                                     the value */
    ib_tpl_t old_tpl,                /*!< in: tuple being updated */
    mci_item_t *result)              /*!< in: tuple data for tuple being
                                     updated */
{
  ib_err_t err = DB_SUCCESS;
  char *append_buf;
  int before_len;
  int total_len;
  ib_tpl_t new_tpl;
  uint64_t new_cas;
  meta_cfg_info_t *meta_info = cursor_data->conn_meta;
  meta_column_t *col_info = meta_info->col_info;
  char *before_val;
  int column_used;

  if (engine->enable_binlog) {
    assert(cursor_data->mysql_tbl);

    innodb_api_setup_hdl_rec(result, col_info, cursor_data->mysql_tbl);
    handler_store_record(cursor_data->mysql_tbl);
  }

  /* If we have multiple value columns, the column to append the
  string needs to be defined. We will use user supplied flags
  as an indication on which column to apply the operation. Otherwise,
  the first column will be appended / prepended */
  if (meta_info->n_extra_col > 0) {
    if (flags < (uint64_t)meta_info->n_extra_col) {
      column_used = flags;
    } else {
      column_used = 0;
    }

    /* For int column, we don't support append command. */
    if (append && !result->extra_col_value[column_used].is_str) {
      return DB_UNSUPPORTED;
    }

    before_len = result->extra_col_value[column_used].value_len;
    before_val = result->extra_col_value[column_used].value_str;
  } else {
    /* For int column, we don't support append command. */
    if (append && !result->col_value[MCI_COL_VALUE].is_str) {
      return DB_UNSUPPORTED;
    }

    before_len = result->col_value[MCI_COL_VALUE].value_len;
    before_val = result->col_value[MCI_COL_VALUE].value_str;
    column_used = UPDATE_ALL_VAL_COL;
  }

  total_len = before_len + val_len;

  append_buf = (char *)malloc(total_len);

  if (append) {
    memcpy(append_buf, before_val, before_len);
    memcpy(append_buf + before_len, key + len, val_len);
  } else {
    memcpy(append_buf, key + len, val_len);
    memcpy(append_buf + val_len, before_val, before_len);
  }

  new_tpl = ib_cb_read_tuple_create(cursor_data->crsr);

  new_cas = mci_get_cas(engine);

  if (exp) {
    uint64_t time;
    time = mci_get_time();
    exp += time;
  }

  assert(!cursor_data->mysql_tbl || engine->enable_binlog ||
         engine->enable_mdl);

  err = innodb_api_set_tpl(
      new_tpl, meta_info, col_info, key, len, append_buf, total_len, new_cas,
      exp, flags, column_used,
      engine->enable_binlog ? cursor_data->mysql_tbl : NULL, true);

  if (err == DB_SUCCESS) {
    err = ib_cb_update_row(srch_crsr, old_tpl, new_tpl);
  }

  free(append_buf);
  append_buf = NULL;

  if (err == DB_SUCCESS) {
    *cas = new_cas;

    if (engine->enable_binlog) {
      handler_binlog_row(cursor_data->thd, cursor_data->mysql_tbl, HDL_UPDATE);
    }
  }

  ib_cb_tuple_delete(new_tpl);

  return (err);
}

/*************************************************************/ /**
 Update a row with arithmetic operations
 @return ENGINE_SUCCESS if successful otherwise ENGINE_NOT_STORED */
ENGINE_ERROR_CODE
innodb_api_arithmetic(
    /*==================*/
    innodb_engine_t *engine,         /*!< in: InnoDB Memcached engine */
    innodb_conn_data_t *cursor_data, /*!< in/out: cursor info */
    const char *key,                 /*!< in: key value */
    int len,                         /*!< in: key length */
    int delta,                       /*!< in: value to add or subtract */
    bool increment,                  /*!< in: increment or decrement */
    uint64_t *cas,                   /*!< out: cas */
    rel_time_t exp_time __attribute__((unused)),
    /*!< in: expire time */
    bool create,          /*!< in: whether to create new entry
                          if not found */
    uint64_t initial,     /*!< in: initialize value */
    uint64_t *out_result) /*!< in: arithmetic result */
{
  ib_err_t err = DB_SUCCESS;
  char value_buf[128];
  mci_item_t result;
  ib_tpl_t old_tpl;
  ib_tpl_t new_tpl;
  uint64_t value = 0;
  bool create_new = false;
  char *end_ptr;
  meta_cfg_info_t *meta_info = cursor_data->conn_meta;
  meta_column_t *col_info = meta_info->col_info;
  ib_crsr_t srch_crsr = cursor_data->crsr;
  char *before_val;
  unsigned int before_len;
  int column_used = 0;
  ENGINE_ERROR_CODE ret = ENGINE_SUCCESS;

  err = innodb_api_search(cursor_data, &srch_crsr, key, len, &result, &old_tpl,
                          false, 0);

  /* If the return message is not success or record not found, just
  exit */
  if (err != DB_SUCCESS && err != DB_RECORD_NOT_FOUND) {
    *out_result = 0;
    goto func_exit;
  }

  memset(value_buf, 0, 128);

  /* Can't find the row, decide whether to insert a new row */
  if (err != DB_SUCCESS) {
    /* If create is true, insert a new row */
    if (create) {
      snprintf(value_buf, sizeof(value_buf), "%" PRIu64, initial);
      create_new = true;
      goto create_new_value;
    } else {
      /* cursor_data->mysql_tbl can't be created.
      So safe to return here */
      return (ENGINE_KEY_ENOENT);
    }
  }

  /* Save the original value, this would be an update */
  if (engine->enable_binlog) {
    innodb_api_setup_hdl_rec(&result, col_info, cursor_data->mysql_tbl);
    handler_store_record(cursor_data->mysql_tbl);
  }

  /* If we have multiple value columns, the column to append the
  string needs to be defined. We will use user supplied flags
  as an indication on which column to apply the operation. Otherwise,
  the first column will be appended / prepended */
  if (meta_info->n_extra_col > 0) {
    uint64_t flags = result.col_value[MCI_COL_FLAG].value_int;

    if (flags < (uint64_t)meta_info->n_extra_col) {
      column_used = flags;
    } else {
      column_used = 0;
    }

    before_len = result.extra_col_value[column_used].value_len;
    if (result.extra_col_value[column_used].is_str) {
      before_val = result.extra_col_value[column_used].value_str;
      if (before_val) {
        value = strtoull(before_val, &end_ptr, 10);
      }
    } else {
      value = result.extra_col_value[column_used].value_int;
    }
  } else {
    before_len = result.col_value[MCI_COL_VALUE].value_len;
    if (result.col_value[MCI_COL_VALUE].is_str) {
      before_val = result.col_value[MCI_COL_VALUE].value_str;
      if (before_val) {
        value = strtoull(before_val, &end_ptr, 10);
      }
    } else {
      value = result.col_value[MCI_COL_VALUE].value_int;
    }
    column_used = UPDATE_ALL_VAL_COL;
  }

  if (before_len >= (sizeof(value_buf) - 1)) {
    ret = ENGINE_EINVAL;
    goto func_exit;
  }

  errno = 0;

  if (errno == ERANGE) {
    ret = ENGINE_EINVAL;
    goto func_exit;
  }

  if (increment) {
    value += delta;
  } else {
    if (delta > (int)value) {
      value = 0;
    } else {
      value -= delta;
    }
  }

  snprintf(value_buf, sizeof(value_buf), "%" PRIu64, value);
create_new_value:
  *cas = mci_get_cas(engine);

  new_tpl = ib_cb_read_tuple_create(cursor_data->crsr);

  assert(!cursor_data->mysql_tbl || engine->enable_binlog ||
         engine->enable_mdl);

  /* The cas, exp and flags field are not changing, so use the
  data from result */
  err = innodb_api_set_tpl(
      new_tpl, meta_info, col_info, key, len, value_buf, strlen(value_buf),
      *cas, result.col_value[MCI_COL_EXP].value_int,
      result.col_value[MCI_COL_FLAG].value_int, column_used,
      engine->enable_binlog ? cursor_data->mysql_tbl : NULL, true);

  if (err != DB_SUCCESS) {
    ib_cb_tuple_delete(new_tpl);
    goto func_exit;
  }

  if (create_new) {
    err = ib_cb_insert_row(cursor_data->crsr, new_tpl);
    *out_result = initial;

    if (engine->enable_binlog) {
      handler_binlog_row(cursor_data->thd, cursor_data->mysql_tbl, HDL_INSERT);
    }
  } else {
    err = ib_cb_update_row(srch_crsr, old_tpl, new_tpl);
    *out_result = value;

    if (engine->enable_binlog) {
      handler_binlog_row(cursor_data->thd, cursor_data->mysql_tbl, HDL_UPDATE);
    }
  }

  ib_cb_tuple_delete(new_tpl);

func_exit:
  /* Free memory of result. */
  if (result.extra_col_value) {
    free(result.extra_col_value);
  } else {
    if (result.col_value[MCI_COL_VALUE].allocated) {
      free(result.col_value[MCI_COL_VALUE].value_str);
      result.col_value[MCI_COL_VALUE].allocated = false;
    }
  }

  if (ret == ENGINE_SUCCESS) {
    ret = (err == DB_SUCCESS) ? ENGINE_SUCCESS : ENGINE_NOT_STORED;
  }

  return (ret);
}

/*************************************************************/ /**
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
    innodb_engine_t *engine,         /*!< in: InnoDB Memcached engine */
    innodb_conn_data_t *cursor_data, /*!< in/out: cursor info */
    const char *key,                 /*!< in: key value */
    int len,                         /*!< in: key length */
    uint32_t val_len,                /*!< in: value length */
    uint64_t exp,                    /*!< in: expire time */
    uint64_t *cas,                   /*!< out: cas value */
    uint64_t input_cas,              /*!< in: cas value supplied by user */
    uint64_t flags,                  /*!< in: flags */
    ENGINE_STORE_OPERATION op)       /*!< in: Operations */
{
  ib_err_t err = DB_ERROR;
  mci_item_t result;
  ib_tpl_t old_tpl = NULL;
  ENGINE_ERROR_CODE stored = ENGINE_NOT_STORED;
  ib_crsr_t srch_crsr = cursor_data->crsr;

  /* Skip search for add operation. Rely on the unique index of
  key to check any duplicates */
  if (op == OPERATION_ADD) {
    err = DB_RECORD_NOT_FOUND;
    memset(&result, 0, sizeof(result));
  } else {
    /* First check whether record with the key value exists */
    err = innodb_api_search(cursor_data, &srch_crsr, key, len, &result,
                            &old_tpl, false, 0);
  }

  /* If the return message is not success or record not found, just
  exit */
  if (err != DB_SUCCESS && err != DB_RECORD_NOT_FOUND) {
    goto func_exit;
  }

  switch (op) {
    case OPERATION_ADD:
      err = innodb_api_insert(engine, cursor_data, key, len, val_len, exp, cas,
                              flags);
      break;
    case OPERATION_REPLACE:
      if (err == DB_SUCCESS) {
        err = innodb_api_update(engine, cursor_data, srch_crsr, key, len,
                                val_len, exp, cas, flags, old_tpl, &result);
      }
      break;
    case OPERATION_APPEND:
    case OPERATION_PREPEND:
      /* FIXME: Check cas is used for append and prepend */
      /* if (*cas != result.col_value[MCI_COL_CAS].value_int) {
              stored = ENGINE_KEY_EEXISTS;
              break;
      } */

      if (err == DB_SUCCESS) {
        err = innodb_api_link(engine, cursor_data, srch_crsr, key, len, val_len,
                              exp, cas, flags, (op == OPERATION_APPEND),
                              old_tpl, &result);
      }
      break;
    case OPERATION_SET:
      if (err == DB_SUCCESS) {
        err = innodb_api_update(engine, cursor_data, srch_crsr, key, len,
                                val_len, exp, cas, flags, old_tpl, &result);
      } else {
        err = innodb_api_insert(engine, cursor_data, key, len, val_len, exp,
                                cas, flags);
      }
      break;
    case OPERATION_CAS:
      if (err != DB_SUCCESS) {
        stored = ENGINE_KEY_ENOENT;

      } else if (input_cas == result.col_value[MCI_COL_CAS].value_int) {
        err = innodb_api_update(engine, cursor_data, srch_crsr, key, len,
                                val_len, exp, cas, flags, old_tpl, &result);

      } else {
        stored = ENGINE_KEY_EEXISTS;
      }
      break;
  }

  /* Free memory of result. */
  if (result.extra_col_value) {
    free(result.extra_col_value);
  } else {
    if (result.col_value[MCI_COL_VALUE].allocated) {
      free(result.col_value[MCI_COL_VALUE].value_str);
      result.col_value[MCI_COL_VALUE].allocated = false;
    }
  }

func_exit:
  if (err == DB_SUCCESS && stored == ENGINE_NOT_STORED) {
    stored = ENGINE_SUCCESS;
  }

  return (stored);
}

/*************************************************************/ /**
 Implement the "flush_all" command, map to InnoDB's DELETE operation
 return ENGINE_SUCCESS is all successful */
ENGINE_ERROR_CODE
innodb_api_flush(
    /*=============*/
    innodb_engine_t *engine,       /*!< in: InnoDB Memcached engine */
    innodb_conn_data_t *conn_data, /*!< in/out: cursor affiliated
                                 with a connection */
    const char *dbname,            /*!< in: database name */
    const char *name)              /*!< in: table name */
{
  ib_err_t err;
  char table_name[MAX_TABLE_NAME_LEN + MAX_DATABASE_NAME_LEN + 1];
  ib_crsr_t crsr = conn_data->crsr;

  err = innodb_cb_cursor_lock(engine, crsr, IB_LOCK_X);

  if (err != DB_SUCCESS) {
    fprintf(stderr,
            " InnoDB_Memcached: Fail to lock"
            " table '%s.%s'\n",
            dbname, name);
    return (ENGINE_ERROR_CODE(err));
  }

  for (err = ib_cb_cursor_first(crsr); err == DB_SUCCESS;
       err = ib_cb_cursor_next(crsr)) {
    err = ib_cb_delete_row(crsr);

    if (err == DB_RECORD_NOT_FOUND) {
      err = DB_SUCCESS;
    }
  }

  if (err == DB_END_OF_INDEX) {
    err = DB_SUCCESS;
  }

  /* If binlog is enabled, log the truncate table statement */
  if (err == DB_SUCCESS && engine->enable_binlog) {
    void *thd = conn_data->thd;

    snprintf(table_name, sizeof(table_name), "%s.%s", dbname, name);
    handler_binlog_truncate(thd, table_name);
  }

  return (ENGINE_ERROR_CODE(err));
}

/*************************************************************/ /**
 Increment read and write counters, if they exceed the batch size,
 commit the transaction. */
bool innodb_reset_conn(
    /*==============*/
    innodb_conn_data_t *conn_data, /*!< in/out: cursor affiliated
                                   with a connection */
    bool has_lock,                 /*!< in: has lock on
                                   connection */
    bool commit,                   /*!< in: commit or abort trx */
    bool has_binlog)               /*!< in: binlog enabled */
{
  bool commit_trx = false;

  LOCK_CURRENT_CONN_IF_NOT_LOCKED(has_lock, conn_data);

  if (conn_data->crsr) {
    ib_cb_cursor_reset(conn_data->crsr);
  }

  if (conn_data->read_crsr) {
    ib_cb_cursor_reset(conn_data->read_crsr);
  }

  if (conn_data->idx_crsr) {
    ib_cb_cursor_reset(conn_data->idx_crsr);
  }

  if (conn_data->idx_read_crsr) {
    ib_cb_cursor_reset(conn_data->idx_read_crsr);
  }

  if (conn_data->crsr_trx) {
    ib_crsr_t ib_crsr;
    meta_cfg_info_t *meta_info = conn_data->conn_meta;
    meta_index_t *meta_index = &meta_info->index_info;

    if (meta_index->srch_use_idx == META_USE_SECONDARY) {
      assert(conn_data->idx_crsr || conn_data->idx_read_crsr);

      ib_crsr =
          conn_data->idx_crsr ? conn_data->idx_crsr : conn_data->idx_read_crsr;
    } else {
      assert(conn_data->crsr || conn_data->read_crsr);

      ib_crsr = conn_data->crsr ? conn_data->crsr : conn_data->read_crsr;
    }

    if (commit) {
      if (has_binlog && conn_data->thd && conn_data->mysql_tbl) {
        handler_binlog_commit(conn_data->thd, conn_data->mysql_tbl);
      }

      ib_cb_cursor_commit_trx(ib_crsr, conn_data->crsr_trx);
    } else {
      if (has_binlog && conn_data->thd && conn_data->mysql_tbl) {
        handler_binlog_rollback(conn_data->thd, conn_data->mysql_tbl);
      }

      ib_cb_trx_rollback(conn_data->crsr_trx);
    }

    commit_trx = true;
    conn_data->in_use = false;
  }

  conn_data->n_writes_since_commit = 0;
  conn_data->n_reads_since_commit = 0;

  UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(has_lock, conn_data);
  return (commit_trx);
}

/*************************************************************/ /**
 Increment read and write counters, if they exceed the batch size,
 commit the transaction. */
void innodb_api_cursor_reset(
    /*====================*/
    innodb_engine_t *engine,       /*!< in: InnoDB Memcached
                                   engine */
    innodb_conn_data_t *conn_data, /*!< in/out: cursor affiliated
                                   with a connection */
    conn_op_type_t op_type,        /*!< in: type of DML performed */
    bool commit)                   /*!< in: commit or abort trx */
{
  bool commit_trx = false;

  switch (op_type) {
    case CONN_OP_READ:
      conn_data->n_total_reads++;
      conn_data->n_reads_since_commit++;
      break;
    case CONN_OP_DELETE:
    case CONN_OP_WRITE:
      conn_data->n_total_writes++;
      conn_data->n_writes_since_commit++;
      break;
    case CONN_OP_FLUSH:
      break;
  }

  if (release_mdl_lock ||
      conn_data->n_reads_since_commit >= engine->read_batch_size ||
      conn_data->n_writes_since_commit >= engine->write_batch_size ||
      (op_type == CONN_OP_FLUSH) || !commit) {
    commit_trx = innodb_reset_conn(conn_data, op_type == CONN_OP_FLUSH, commit,
                                   engine->enable_binlog);
  }

  if (!commit_trx) {
    LOCK_CURRENT_CONN_IF_NOT_LOCKED(op_type == CONN_OP_FLUSH, conn_data);
    if (op_type != CONN_OP_FLUSH) {
      assert(conn_data->in_use);
    }

    conn_data->in_use = false;
    UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(op_type == CONN_OP_FLUSH, conn_data);
  }
}

/** Following are a set of InnoDB callback function wrappers for functions
that will be used outside innodb_api.c */

/*************************************************************/ /**
 Close a cursor
 @return DB_SUCCESS if successful or error code */
ib_err_t innodb_cb_cursor_close(
    /*===================*/
    ib_crsr_t ib_crsr) /*!< in/out: cursor to close */
{
  return (ib_cb_cursor_close(ib_crsr));
}

/*************************************************************/ /**
 Commit the transaction
 @return DB_SUCCESS if successful or error code */
ib_err_t innodb_cb_trx_commit(
    /*=================*/
    ib_trx_t ib_trx) /*!< in/out: transaction to commit */
{
  return (ib_cb_trx_commit(ib_trx));
}

/*****************************************************************/ /**
 update the cursor with new transactions and also reset the cursor
 @return DB_SUCCESS or err code */
ib_err_t innodb_cb_cursor_new_trx(
    /*=====================*/
    ib_crsr_t ib_crsr, /*!< in/out: InnoDB cursor */
    ib_trx_t ib_trx)   /*!< in: transaction */
{
  return (ib_cb_cursor_new_trx(ib_crsr, ib_trx));
}

/*****************************************************************/ /**
 Set the Lock an InnoDB cursor/table.
 @return DB_SUCCESS or error code */
ib_err_t innodb_cb_cursor_lock(
    /*==================*/
    innodb_engine_t *eng,      /*!< in: InnoDB Memcached engine */
    ib_crsr_t ib_crsr,         /*!< in/out: InnoDB cursor */
    ib_lck_mode_t ib_lck_mode) /*!< in: InnoDB lock mode */
{
  ib_err_t err = DB_SUCCESS;

  if (ib_lck_mode == IB_LOCK_TABLE_X) {
    /* Table lock only */
    err = ib_cb_cursor_lock(ib_crsr, IB_LOCK_X);
  } else if (eng && eng->cfg_status & IB_CFG_DISABLE_ROWLOCK) {
    /* Table lock only */
    if (ib_lck_mode == IB_LOCK_X) {
      err = ib_cb_cursor_lock(ib_crsr, IB_LOCK_IX);
    } else {
      err = ib_cb_cursor_lock(ib_crsr, IB_LOCK_IS);
    }
  } else {
    err = ib_cb_cursor_set_lock(ib_crsr, ib_lck_mode);
  }

  return (err);
}

/*****************************************************************/ /**
 Create an InnoDB tuple used for index/table search.
 @return own: Tuple for current index */
ib_tpl_t innodb_cb_read_tuple_create(
    /*========================*/
    ib_crsr_t ib_crsr) /*!< in: Cursor instance */
{
  return (ib_cb_read_tuple_create(ib_crsr));
}

/*****************************************************************/ /**
 Move cursor to the first record in the table.
 @return DB_SUCCESS or err code */
ib_err_t innodb_cb_cursor_first(
    /*===================*/
    ib_crsr_t ib_crsr) /*!< in: InnoDB cursor instance */
{
  return (ib_cb_cursor_first(ib_crsr));
}

/*****************************************************************/ /**
 Get a column type, length and attributes from the tuple.
 @return len of column data */
ib_ulint_t innodb_cb_col_get_meta(
    /*===================*/
    ib_tpl_t ib_tpl,            /*!< in: tuple instance */
    ib_ulint_t i,               /*!< in: column index in tuple */
    ib_col_meta_t *ib_col_meta) /*!< out: column meta data */
{
  return (ib_cb_col_get_meta(ib_tpl, i, ib_col_meta));
}

/*****************************************************************/ /**
 Destroy an InnoDB tuple. */
void innodb_cb_tuple_delete(
    /*===================*/
    ib_tpl_t ib_tpl) /*!< in,own: Tuple instance to delete */
{
  ib_cb_tuple_delete(ib_tpl);
  return;
}

/*****************************************************************/ /**
 Return the number of columns in the tuple definition.
 @return number of columns */
ib_ulint_t innodb_cb_tuple_get_n_cols(
    /*=======================*/
    const ib_tpl_t ib_tpl) /*!< in: Tuple for table/index */
{
  return (ib_cb_tuple_get_n_cols(ib_tpl));
}

/*****************************************************************/ /**
 Get a column value pointer from the tuple.
 @return NULL or pointer to buffer */
const void *innodb_cb_col_get_value(
    /*====================*/
    ib_tpl_t ib_tpl, /*!< in: tuple instance */
    ib_ulint_t i)    /*!< in: column index in tuple */
{
  return (ib_cb_col_get_value(ib_tpl, i));
}

/********************************************************************/ /**
 Open a table using the table name.
 @return table instance if found */
ib_err_t innodb_cb_open_table(
    /*=================*/
    const char *name,   /*!< in: table name to lookup */
    ib_trx_t ib_trx,    /*!< in: transaction */
    ib_crsr_t *ib_crsr) /*!< in: cursor to be used */
{
  return (ib_cb_open_table(name, ib_trx, ib_crsr));
}

/*****************************************************************/ /**
 Get a column name from the tuple.
 @return name of the column */
char *innodb_cb_col_get_name(
    /*===================*/
    ib_crsr_t ib_crsr, /*!< in: InnoDB cursor instance */
    ib_ulint_t i)      /*!< in: column index in tuple */
{
  return (ib_cb_col_get_name(ib_crsr, i));
}

/*****************************************************************/ /**
 Open an InnoDB secondary index cursor and return a cursor handle to it.
 @return DB_SUCCESS or err code */
ib_err_t innodb_cb_cursor_open_index_using_name(
    /*===================================*/
    ib_crsr_t ib_open_crsr, /*!< in: open/active cursor */
    const char *index_name, /*!< in: secondary index name */
    ib_crsr_t *ib_crsr,     /*!< out,own: InnoDB index cursor */
    int *idx_type,          /*!< out: index is cluster index */
    ib_id_u64_t *idx_id)    /*!< out: index id */
{
  return (ib_cb_cursor_open_index_using_name(ib_open_crsr, index_name, ib_crsr,
                                             idx_type, idx_id));
}
/*****************************************************************/ /**
 Get InnoDB API configure option
 @return configure status */
int innodb_cb_get_cfg()
/*===============*/
{
  return (ib_cb_get_cfg());
}
