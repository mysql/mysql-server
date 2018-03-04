/*****************************************************************************

Copyright (c) 2008, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************//**
@file include/api0misc.h
InnoDB Native API

3/20/2011 Jimmy Yang extracted from Embedded InnoDB
2008 Created by Sunny Bains
*******************************************************/

#ifndef api0misc_h
#define	api0misc_h

#include <sys/types.h>

#include "handler.h"
#include "os0file.h"
#include "que0que.h"
#include "trx0trx.h"
#include "univ.i"

/** Whether binlog is enabled for applications using InnoDB APIs */
extern bool                  ib_binlog_enabled;

/** Whether MySQL MDL is enabled for applications using InnoDB APIs */
extern bool                  ib_mdl_enabled;

/** Whether InnoDB row lock is disabled for applications using InnoDB APIs */
extern bool                  ib_disable_row_lock;

/** configure value for transaction isolation level */
extern ulong			ib_trx_level_setting;

/** configure value for background commit interval (in seconds) */
extern ulong			ib_bk_commit_interval;

struct ib_sdi_key {
	const dd::sdi_key_t*	sdi_key;
};

struct ib_sdi_vector {
	dd::sdi_vector_t*	sdi_vector;
};

/* Handles user errors and lock waits detected by the database engine.
@param[out]	new_err	possible new error encountered in lock wait, or if no
new error, the value of trx->error_state at the entry of this function
@param[in]	trx	transaction
@param[in]	thr	query thread
@param[in]	savept	savepoint or NULL
@param[in]	is_sdi	true if table is SDI
@return TRUE if it was a lock wait and we should continue running
the query thread. */
ibool
ib_handle_errors(
	dberr_t*	new_err,
	trx_t*		trx,
	que_thr_t*	thr,
	trx_savept_t*	savept,
	bool		is_sdi);

/*************************************************************************
Sets a lock on a table.
@return error code or DB_SUCCESS */
dberr_t
ib_trx_lock_table_with_retry(
/*=========================*/
	trx_t*		trx,		/*!< in/out: transaction */
	dict_table_t*	table,		/*!< in: table to lock */
	enum lock_mode	mode);		/*!< in: lock mode */

#endif /* api0misc_h */
