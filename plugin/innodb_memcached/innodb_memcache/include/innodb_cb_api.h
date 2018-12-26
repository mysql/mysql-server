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
 @file innodb_cb_api.h

 Created 03/15/2011      Jimmy Yang
 *******************************************************/

#ifndef innodb_cb_api_h
#define innodb_cb_api_h

#include "api0api.h"

/** Following are callback function defines for InnoDB APIs, mapped to
functions defined in api0api.c */

typedef ib_err_t (*cb_open_table_t)(
    /*===============*/
    const char *name, ib_trx_t ib_trx, ib_crsr_t *ib_crsr);

typedef ib_err_t (*cb_read_row_t)(
    /*=============*/
    ib_crsr_t ib_crsr, ib_tpl_t ib_tpl, ib_tpl_t cmp_tpl, int mode,
    void **row_buf, ib_ulint_t *row_buf_len, ib_ulint_t *row_buf_used);

typedef ib_err_t (*cb_insert_row_t)(
    /*===============*/
    ib_crsr_t ib_crsr, const ib_tpl_t ib_tpl);

typedef ib_err_t (*cb_cursor_delete_row_t)(
    /*======================*/
    ib_crsr_t ib_crsr);

typedef ib_err_t (*cb_cursor_update_row_t)(
    /*======================*/
    ib_crsr_t ib_crsr, const ib_tpl_t ib_old_tpl, const ib_tpl_t ib_new_tpl);

typedef ib_err_t (*cb_cursor_moveto_t)(
    /*==================*/
    ib_crsr_t ib_crsr, ib_tpl_t ib_tpl, ib_srch_mode_t ib_srch_mode,
    unsigned int direction);

typedef ib_tpl_t (*cb_sec_search_tuple_create_t)(
    /*============================*/
    ib_crsr_t ib_crsr);

typedef ib_tpl_t (*cb_sec_read_tuple_create_t)(
    /*==========================*/
    ib_crsr_t ib_crsr);

typedef void (*cb_tuple_delete_t)(
    /*=================*/
    ib_tpl_t ib_tpl);

typedef ib_err_t (*cb_tuple_read_u8_t)(
    /*==================*/
    ib_tpl_t ib_tpl, ib_ulint_t i, ib_u8_t *ival);

typedef ib_err_t (*cb_tuple_read_u16_t)(
    /*===================*/
    ib_tpl_t ib_tpl, ib_ulint_t i, ib_u16_t *ival);

typedef ib_err_t (*cb_tuple_read_u32_t)(
    /*===================*/
    ib_tpl_t ib_tpl, ib_ulint_t i, ib_u32_t *ival);

typedef ib_err_t (*cb_tuple_read_u64_t)(
    /*===================*/
    ib_tpl_t ib_tpl, ib_ulint_t i, ib_u64_t *ival);

typedef ib_err_t (*cb_tuple_read_i8_t)(
    /*==================*/
    ib_tpl_t ib_tpl, ib_ulint_t i, ib_i8_t *ival);

typedef ib_err_t (*cb_tuple_read_i16_t)(
    /*===================*/
    ib_tpl_t ib_tpl, ib_ulint_t i, ib_i16_t *ival);

typedef ib_err_t (*cb_tuple_read_i32_t)(
    /*===================*/
    ib_tpl_t ib_tpl, ib_ulint_t i, ib_i32_t *ival);

typedef ib_err_t (*cb_tuple_read_i64_t)(
    /*===================*/
    ib_tpl_t ib_tpl, ib_ulint_t i, ib_i64_t *ival);

typedef ib_err_t (*cb_col_set_value_t)(
    /*==================*/
    ib_tpl_t ib_tpl, ib_ulint_t col_no, const void *src, ib_ulint_t len,
    bool need_cpy);

typedef const void *(*cb_col_get_value_t)(
    /*==================*/
    ib_tpl_t ib_tpl, ib_ulint_t i);

typedef ib_ulint_t (*cb_col_get_meta_t)(
    /*=================*/
    ib_tpl_t ib_tpl, ib_ulint_t i, ib_col_meta_t *ib_col_meta);

typedef ib_trx_t (*cb_trx_begin_t)(
    /*==============*/
    ib_trx_level_t ib_trx_level, bool read_write, bool auto_commit, void *thd);

typedef ib_err_t (*cb_trx_commit_t)(
    /*===============*/
    ib_trx_t ib_trx);

typedef ib_err_t (*cb_trx_rollback_t)(
    /*=================*/
    ib_trx_t ib_trx);

typedef ib_err_t (*cb_trx_start_t)(
    /*==============*/
    ib_trx_t ib_trx, ib_trx_level_t ib_trx_level, bool read_write,
    bool auto_commit, void *thd);

typedef ib_err_t (*cb_trx_release_t)(
    /*================*/
    ib_trx_t ib_trx);

typedef ib_ulint_t (*cb_tuple_get_n_cols_t)(
    /*=====================*/
    const ib_tpl_t ib_tpl);

typedef void (*cb_cursor_set_match_mode_t)(
    /*==========================*/
    ib_crsr_t ib_crsr, ib_match_mode_t match_mode);

typedef ib_err_t (*cb_cursor_lock_t)(
    /*================*/
    ib_crsr_t ib_crsr, ib_lck_mode_t ib_lck_mode);

typedef ib_err_t (*cb_cursor_set_lock_t)(
    /*====================*/
    ib_crsr_t ib_crsr, ib_lck_mode_t ib_lck_mode);

typedef ib_err_t (*cb_cursor_close_t)(
    /*=================*/
    ib_crsr_t ib_crsr);

typedef ib_err_t (*cb_cursor_new_trx_t)(
    /*===================*/
    ib_crsr_t ib_crsr, ib_trx_t ib_trx);

typedef ib_err_t (*cb_cursor_reset_t)(
    /*=================*/
    ib_crsr_t ib_crsr);

typedef char *(*cb_col_get_name_t)(
    /*=================*/
    ib_crsr_t ib_crsr, ib_ulint_t i);

typedef char *(*cb_get_idx_field_name)(
    /*=====================*/
    ib_crsr_t ib_crsr, ib_ulint_t i);

typedef ib_err_t (*cb_cursor_first_t)(
    /*=================*/
    ib_crsr_t ib_crsr);

typedef ib_err_t (*cb_cursor_next_t)(
    /*================*/
    ib_crsr_t ib_crsr);

typedef ib_err_t (*cb_close_thd_t)(
    /*==============*/
    void *thd);

typedef int (*cb_get_cfg_t)();
/*=============*/

typedef ib_err_t (*cb_cursor_open_index_using_name_t)(
    /*=================================*/
    ib_crsr_t ib_open_crsr, const char *index_name, ib_crsr_t *ib_crsr,
    int *idx_type, ib_id_u64_t *idx_id);

typedef void (*cb_cursor_set_cluster_access_t)(
    /*==============================*/
    ib_crsr_t ib_crsr);

typedef ib_err_t (*cb_cursor_commit_trx_t)(
    /*======================*/
    ib_crsr_t ib_crsr, ib_trx_t ib_trx);

typedef ib_trx_level_t (*cb_cfg_trx_level_t)();
/*===================*/

typedef ib_ulint_t (*cb_get_n_user_cols)(
    /*==================*/
    const ib_tpl_t ib_tpl);

typedef ib_err_t (*cb_trx_get_start_time)(
    /*=====================*/
    ib_trx_t ib_trx);

typedef ib_ulint_t (*cb_bk_commit_interval)();
/*======================*/

typedef const char *(*cb_ut_strerr)(
    /*============*/
    ib_err_t num);

typedef ib_err_t (*cb_cursor_stmt_begin)(
    /*====================*/
    ib_crsr_t ib_crsr);

typedef ib_u32_t (*cb_trx_read_only_t)(
    /*==================*/
    ib_trx_t ib_trx);

#ifdef UNIV_MEMCACHED_SDI
typedef ib_err_t (*cb_sdi_get)(ib_crsr_t ib_crsr, const char *key, void *sdi,
                               uint64_t *sdi_len, ib_trx_t trx);

typedef ib_err_t (*cb_sdi_delete)(ib_crsr_t ib_crsr, const char *key,
                                  ib_trx_t trx);

typedef ib_err_t (*cb_sdi_set)(ib_crsr_t ib_crsr, const char *key,
                               const void *sdi, uint64_t *sdi_len,
                               ib_trx_t trx);

typedef ib_err_t (*cb_sdi_create)(ib_crsr_t ib_crsr);

typedef ib_err_t (*cb_sdi_drop)(ib_crsr_t ib_crsr);

typedef ib_err_t (*cb_sdi_get_keys)(ib_crsr_t ib_crsr, const char *key,
                                    void *sdi, uint64_t list_buf_len);
#endif /* UNIV_MEMCACHED_SDI */

typedef ib_u32_t (*cb_is_virtual_table)(ib_crsr_t ib_crsr);

extern cb_open_table_t ib_cb_open_table;
extern cb_read_row_t ib_cb_read_row;
extern cb_insert_row_t ib_cb_insert_row;
extern cb_cursor_delete_row_t ib_cb_delete_row;
extern cb_cursor_update_row_t ib_cb_update_row;
extern cb_cursor_moveto_t ib_cb_moveto;
extern cb_sec_search_tuple_create_t ib_cb_search_tuple_create;
extern cb_sec_read_tuple_create_t ib_cb_read_tuple_create;
extern cb_tuple_delete_t ib_cb_tuple_delete;
extern cb_tuple_read_u8_t ib_cb_tuple_read_u8;
extern cb_tuple_read_u16_t ib_cb_tuple_read_u16;
extern cb_tuple_read_u32_t ib_cb_tuple_read_u32;
extern cb_tuple_read_u64_t ib_cb_tuple_read_u64;
extern cb_tuple_read_i8_t ib_cb_tuple_read_i8;
extern cb_tuple_read_i16_t ib_cb_tuple_read_i16;
extern cb_tuple_read_i32_t ib_cb_tuple_read_i32;
extern cb_tuple_read_i64_t ib_cb_tuple_read_i64;
extern cb_col_set_value_t ib_cb_col_set_value;
extern cb_col_get_value_t ib_cb_col_get_value;
extern cb_col_get_meta_t ib_cb_col_get_meta;
extern cb_trx_begin_t ib_cb_trx_begin;
extern cb_trx_commit_t ib_cb_trx_commit;
extern cb_trx_rollback_t ib_cb_trx_rollback;
extern cb_trx_start_t ib_cb_trx_start;
extern cb_trx_release_t ib_cb_trx_release;
extern cb_tuple_get_n_cols_t ib_cb_tuple_get_n_cols;
extern cb_cursor_set_match_mode_t ib_cb_cursor_set_match_mode;
extern cb_cursor_lock_t ib_cb_cursor_lock;
extern cb_cursor_close_t ib_cb_cursor_close;
extern cb_cursor_new_trx_t ib_cb_cursor_new_trx;
extern cb_cursor_reset_t ib_cb_cursor_reset;
extern cb_col_get_name_t ib_cb_col_get_name;
extern cb_get_idx_field_name ib_cb_get_idx_field_name;
extern cb_cursor_first_t ib_cb_cursor_first;
extern cb_cursor_next_t ib_cb_cursor_next;
extern cb_cursor_open_index_using_name_t ib_cb_cursor_open_index_using_name;
extern cb_close_thd_t ib_cb_close_thd;
extern cb_get_cfg_t ib_cb_get_cfg;
extern cb_cursor_set_cluster_access_t ib_cb_cursor_set_cluster_access;
extern cb_cursor_commit_trx_t ib_cb_cursor_commit_trx;
extern cb_cfg_trx_level_t ib_cb_cfg_trx_level;
extern cb_get_n_user_cols ib_cb_get_n_user_cols;
extern cb_cursor_set_lock_t ib_cb_cursor_set_lock;
extern cb_trx_get_start_time ib_cb_trx_get_start_time;
extern cb_bk_commit_interval ib_cb_cfg_bk_commit_interval;
extern cb_ut_strerr ib_cb_ut_strerr;
extern cb_cursor_stmt_begin ib_cb_cursor_stmt_begin;
#ifdef UNIV_MEMCACHED_SDI
extern cb_sdi_get ib_cb_sdi_get;
extern cb_sdi_delete ib_cb_sdi_delete;
extern cb_sdi_set ib_cb_sdi_set;
extern cb_sdi_create ib_cb_sdi_create;
extern cb_sdi_drop ib_cb_sdi_drop;
extern cb_sdi_get_keys ib_cb_sdi_get_keys;
#endif /* UNIV_MEMCACHED_SDI */
extern cb_trx_read_only_t ib_cb_trx_read_only;
extern cb_is_virtual_table ib_cb_is_virtual_table;

#endif /* innodb_cb_api_h */
