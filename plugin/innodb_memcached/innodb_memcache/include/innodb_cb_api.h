/***********************************************************************

Copyright (c) 2011, 2013, Oracle and/or its affiliates. All Rights Reserved.

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
@file innodb_cb_api.h

Created 03/15/2011      Jimmy Yang
*******************************************************/

#ifndef innodb_cb_api_h
#define innodb_cb_api_h

#include "api0api.h"

/** Following are callback function defines for InnoDB APIs, mapped to
functions defined in api0api.c */

typedef
ib_err_t
(*cb_open_table_t)(
/*===============*/
	const char*	name,
	ib_trx_t	ib_trx,
	ib_crsr_t*	ib_crsr);

typedef
ib_err_t
(*cb_read_row_t)(
/*=============*/
        ib_crsr_t	ib_crsr,
        ib_tpl_t	ib_tpl,
	void**		row_buf,
	ib_ulint_t*	row_buf_len);

typedef
ib_err_t
(*cb_insert_row_t)(
/*===============*/
	ib_crsr_t	ib_crsr,
	const ib_tpl_t	ib_tpl);

typedef
ib_err_t
(*cb_cursor_delete_row_t)(
/*======================*/
	ib_crsr_t	ib_crsr);

typedef
ib_err_t
(*cb_cursor_update_row_t)(
/*======================*/
	ib_crsr_t	ib_crsr,
	const ib_tpl_t	ib_old_tpl,
	const ib_tpl_t	ib_new_tpl);

typedef
ib_err_t
(*cb_cursor_moveto_t)(
/*==================*/
	ib_crsr_t	ib_crsr,
	ib_tpl_t	ib_tpl,
	ib_srch_mode_t	ib_srch_mode);

typedef
ib_tpl_t
(*cb_sec_search_tuple_create_t)(
/*============================*/
	ib_crsr_t	ib_crsr) ;

typedef
ib_tpl_t
(*cb_sec_read_tuple_create_t)(
/*==========================*/
	ib_crsr_t	ib_crsr);

typedef
void
(*cb_tuple_delete_t)(
/*=================*/
	ib_tpl_t	ib_tpl);

typedef
ib_err_t
(*cb_tuple_copy_t)(
/*===============*/
	ib_tpl_t	ib_dst_tpl,
	const ib_tpl_t	ib_src_tpl);

typedef
ib_err_t
(*cb_tuple_read_u8_t)(
/*==================*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	i,
	ib_u8_t*	ival) ;

typedef
ib_err_t
(*cb_tuple_read_u16_t)(
/*===================*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	i,
	ib_u16_t*	ival) ;

typedef
ib_err_t
(*cb_tuple_read_u32_t)(
/*===================*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	i,
	ib_u32_t*	ival) ;

typedef
ib_err_t
(*cb_tuple_read_u64_t)(
/*===================*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	i,
	ib_u64_t*	ival) ;

typedef
ib_err_t
(*cb_tuple_write_u8_t)(
/*===================*/
	ib_tpl_t	ib_tpl,
	int		col_no,
	ib_u8_t		val) ;

typedef
ib_err_t
(*cb_tuple_write_u16_t)(
/*====================*/
	ib_tpl_t	ib_tpl,
	int		col_no,
	ib_u16_t	val) ;

typedef
ib_err_t
(*cb_tuple_write_u32_t)(
/*====================*/
	ib_tpl_t	ib_tpl,
	int		col_no,
	ib_u32_t	val) ;

typedef
ib_err_t
(*cb_tuple_write_u64_t)(
/*====================*/
	ib_tpl_t	ib_tpl,
	int		col_no,
	ib_u64_t	val);

typedef
ib_err_t
(*cb_tuple_read_i8_t)(
/*==================*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	i,
	ib_i8_t*	ival);

typedef
ib_err_t
(*cb_tuple_read_i16_t)(
/*===================*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	i,
	ib_i16_t*	ival);

typedef
ib_err_t
(*cb_tuple_read_i32_t)(
/*===================*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	i,
	ib_i32_t*	ival);

typedef
ib_err_t
(*cb_tuple_read_i64_t)(
/*===================*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	i,
	ib_i64_t*	ival);

typedef
ib_err_t
(*cb_tuple_write_i8_t)(
/*===================*/
	ib_tpl_t	ib_tpl,
	int		col_no,
	ib_i8_t		val);

typedef
ib_err_t
(*cb_tuple_write_i16_t)(
/*====================*/
	ib_tpl_t	ib_tpl,
	int		col_no,
	ib_i16_t	val);

typedef
ib_err_t
(*cb_tuple_write_i32_t)(
/*====================*/
	ib_tpl_t	ib_tpl,
	int		col_no,
	ib_i32_t	val);

typedef
ib_err_t
(*cb_tuple_write_i64_t)(
/*====================*/
	ib_tpl_t	ib_tpl,
	int		col_no,
	ib_i64_t	val);

typedef
ib_err_t
(*cb_col_set_value_t)(
/*==================*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	col_no,
	const void*	src,
	ib_ulint_t	len,
	bool		need_cpy) ;

typedef
const void*
(*cb_col_get_value_t)(
/*==================*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	i);

typedef
ib_ulint_t
(*cb_col_get_meta_t)(
/*=================*/
	ib_tpl_t	ib_tpl,
	ib_ulint_t	i,
	ib_col_meta_t*	ib_col_meta);

typedef
ib_trx_t
(*cb_trx_begin_t)(
/*==============*/
	ib_trx_level_t	ib_trx_level,
	bool		read_write,
	bool		auto_commit);

typedef
ib_err_t
(*cb_trx_commit_t)(
/*===============*/
	ib_trx_t	ib_trx);

typedef
ib_err_t
(*cb_trx_rollback_t)(
/*=================*/
	ib_trx_t	ib_trx);

typedef
ib_err_t
(*cb_trx_start_t)(
/*==============*/
	ib_trx_t	ib_trx,
	ib_trx_level_t	ib_trx_level,
	bool		read_write,
	bool		auto_commit,
	void*		thd);

typedef
ib_trx_state_t
(*cb_trx_state_t)(
/*==============*/
	ib_trx_t	ib_trx);

typedef
ib_trx_state_t
(*cb_trx_release_t)(
/*================*/
	ib_trx_t	ib_trx);

typedef
ib_ulint_t
(*cb_tuple_get_n_cols_t)(
/*=====================*/
	const ib_tpl_t	ib_tpl);

typedef
void
(*cb_cursor_set_match_mode_t)(
/*==========================*/
	ib_crsr_t	ib_crsr,
	ib_match_mode_t	match_mode);

typedef
ib_err_t
(*cb_cursor_lock_t)(
/*================*/
	ib_crsr_t	ib_crsr,
	ib_lck_mode_t	ib_lck_mode);

typedef
ib_err_t
(*cb_cursor_set_lock_t)(
/*====================*/
	ib_crsr_t	ib_crsr,
	ib_lck_mode_t	ib_lck_mode);

typedef
ib_err_t
(*cb_cursor_close_t)(
/*=================*/
	ib_crsr_t	ib_crsr);

typedef
void*
(*cb_open_table_by_name_t)(
/*=======================*/
	const char*	name);

typedef
ib_err_t
(*cb_cursor_new_trx_t)(
/*===================*/
	ib_crsr_t	ib_crsr,
	ib_trx_t	ib_trx);

typedef
ib_err_t
(*cb_cursor_reset_t)(
/*=================*/
	ib_crsr_t	ib_crsr);

typedef
char*
(*cb_col_get_name_t)(
/*=================*/
	ib_crsr_t	ib_crsr,
	ib_ulint_t	i);

typedef
char*
(*cb_get_idx_field_name)(
/*=====================*/
	ib_crsr_t	ib_crsr,
	ib_ulint_t	i);

typedef
ib_err_t
(*cb_table_truncate_t)(
/*===================*/
	const char*	table_name,
	ib_id_u64_t*	table_id);

typedef
ib_err_t
(*cb_cursor_first_t)(
/*=================*/
        ib_crsr_t       ib_crsr);

typedef
ib_err_t
(*cb_cursor_next_t)(
/*================*/
        ib_crsr_t       ib_crsr);

typedef
ib_err_t
(*cb_cursor_last_t)(
/*================*/
        ib_crsr_t       ib_crsr);

typedef
void
(*cb_cursor_clear_trx_t)(
/*=====================*/
        ib_crsr_t       ib_crsr);

typedef
ib_err_t
(*cb_close_thd_t)(
/*==============*/
	void*		thd);

typedef
int
(*cb_get_cfg_t)();
/*=============*/

typedef
ib_err_t
(*cb_cursor_open_index_using_name_t)(
/*=================================*/
	ib_crsr_t	ib_open_crsr,
	const char*	index_name,
	ib_crsr_t*	ib_crsr,
	int*		idx_type,
        ib_id_u64_t*	idx_id);

typedef
void
(*cb_cursor_set_cluster_access_t)(
/*==============================*/
	ib_crsr_t	ib_crsr);

typedef
ib_err_t
(*cb_cursor_commit_trx_t)(
/*======================*/
	ib_crsr_t	ib_crsr,
	ib_trx_t	ib_trx);

typedef
int
(*cb_cfg_trx_level_t)();
/*===================*/


typedef
ib_ulint_t
(*cb_get_n_user_cols)(
/*==================*/
	const ib_tpl_t  ib_tpl);

typedef
ib_err_t
(*cb_trx_get_start_time)(
/*======================*/
	ib_trx_t	ib_trx);

typedef
ib_ulint_t
(*cb_bk_commit_interval)();
/*======================*/

typedef
ib_err_t
(*cb_cursor_stmt_begin)(
/*====================*/
	ib_crsr_t	ib_crsr);

cb_open_table_t			ib_cb_open_table;
cb_read_row_t			ib_cb_read_row;
cb_insert_row_t			ib_cb_insert_row;
cb_cursor_delete_row_t		ib_cb_delete_row;
cb_cursor_update_row_t		ib_cb_update_row;
cb_cursor_moveto_t		ib_cb_moveto;
cb_sec_search_tuple_create_t	ib_cb_search_tuple_create;
cb_sec_read_tuple_create_t	ib_cb_read_tuple_create;
cb_tuple_delete_t		ib_cb_tuple_delete;
cb_tuple_copy_t			ib_cb_tuple_copy;
cb_tuple_read_u8_t		ib_cb_tuple_read_u8;
cb_tuple_read_u16_t		ib_cb_tuple_read_u16;
cb_tuple_read_u32_t		ib_cb_tuple_read_u32;
cb_tuple_read_u64_t		ib_cb_tuple_read_u64;
cb_tuple_write_u8_t		ib_cb_tuple_write_u8;
cb_tuple_write_u16_t		ib_cb_tuple_write_u16;
cb_tuple_write_u32_t		ib_cb_tuple_write_u32;
cb_tuple_write_u64_t		ib_cb_tuple_write_u64;
cb_tuple_read_i8_t		ib_cb_tuple_read_i8;
cb_tuple_read_i16_t		ib_cb_tuple_read_i16;
cb_tuple_read_i32_t		ib_cb_tuple_read_i32;
cb_tuple_read_i64_t		ib_cb_tuple_read_i64;
cb_tuple_write_i8_t		ib_cb_tuple_write_i8;
cb_tuple_write_i16_t		ib_cb_tuple_write_i16;
cb_tuple_write_i32_t		ib_cb_tuple_write_i32;
cb_tuple_write_i64_t		ib_cb_tuple_write_i64;
cb_col_set_value_t		ib_cb_col_set_value;
cb_col_get_value_t		ib_cb_col_get_value;
cb_col_get_meta_t		ib_cb_col_get_meta;
cb_trx_begin_t			ib_cb_trx_begin;
cb_trx_commit_t			ib_cb_trx_commit;
cb_trx_rollback_t		ib_cb_trx_rollback;
cb_trx_start_t			ib_cb_trx_start;
cb_trx_state_t			ib_cb_trx_state;
cb_trx_release_t		ib_cb_trx_release;
cb_tuple_get_n_cols_t		ib_cb_tuple_get_n_cols;
cb_cursor_set_match_mode_t	ib_cb_cursor_set_match_mode;
cb_cursor_lock_t		ib_cb_cursor_lock;
cb_cursor_close_t		ib_cb_cursor_close;
cb_cursor_new_trx_t		ib_cb_cursor_new_trx;
cb_open_table_by_name_t		ib_cb_open_table_by_name;
cb_cursor_reset_t		ib_cb_cursor_reset;
cb_col_get_name_t		ib_cb_col_get_name;
cb_get_idx_field_name		ib_cb_get_idx_field_name;
cb_table_truncate_t		ib_cb_table_truncate;
cb_cursor_first_t		ib_cb_cursor_first;
cb_cursor_next_t		ib_cb_cursor_next;
cb_cursor_last_t		ib_cb_cursor_last;
cb_cursor_open_index_using_name_t	ib_cb_cursor_open_index_using_name;
cb_close_thd_t			ib_cb_close_thd;
cb_get_cfg_t			ib_cb_get_cfg;
cb_cursor_set_cluster_access_t	ib_cb_cursor_set_cluster_access;
cb_cursor_commit_trx_t		ib_cb_cursor_commit_trx;
cb_cfg_trx_level_t		ib_cb_cfg_trx_level;
cb_get_n_user_cols		ib_cb_get_n_user_cols;
cb_cursor_set_lock_t		ib_cb_cursor_set_lock;
cb_cursor_clear_trx_t		ib_cb_cursor_clear_trx;
cb_trx_get_start_time		ib_cb_trx_get_start_time;
cb_bk_commit_interval		ib_cb_cfg_bk_commit_interval;
cb_cursor_stmt_begin		ib_cb_cursor_stmt_begin;

#endif /* innodb_cb_api_h */
