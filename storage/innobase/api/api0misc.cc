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
@file api/api0misc.cc
InnoDB Native API

2008-08-01 Created by Sunny Bains
3/20/2011 Jimmy Yang extracted from Embedded InnoDB
*******************************************************/

#include "api0misc.h"
#include "dict0dict.h"
#include "dict0mem.h"
#include "ha_prototypes.h"
#include "lock0lock.h"
#include "my_inttypes.h"
#include "pars0pars.h"
#include "row0sel.h"
#include "srv0srv.h"
#include "trx0roll.h"

/*********************************************************************//**
Sets a lock on a table.
@return error code or DB_SUCCESS */
dberr_t
ib_trx_lock_table_with_retry(
/*=========================*/
	trx_t*		trx,		/*!< in/out: transaction */
	dict_table_t*	table,		/*!< in: table to lock */
	enum lock_mode	mode)		/*!< in: LOCK_X or LOCK_S */
{
	trx->op_info = "setting table lock";

	return(lock_table_for_trx(table, trx, mode));
}
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
	bool		is_sdi)
{
	dberr_t		err;
handle_new_error:
	err = trx->error_state;

	ut_a(err != DB_SUCCESS);

	trx->error_state = DB_SUCCESS;

	switch (err) {
	case DB_LOCK_WAIT_TIMEOUT:
		if (!is_sdi) {
			trx_rollback_for_mysql(trx);
		}
		break;
		/* fall through */
	case DB_DUPLICATE_KEY:
	case DB_FOREIGN_DUPLICATE_KEY:
	case DB_TOO_BIG_RECORD:
	case DB_ROW_IS_REFERENCED:
	case DB_NO_REFERENCED_ROW:
	case DB_CANNOT_ADD_CONSTRAINT:
	case DB_TOO_MANY_CONCURRENT_TRXS:
	case DB_OUT_OF_FILE_SPACE:
		if (savept) {
			/* Roll back the latest, possibly incomplete
			insertion or update */

			trx_rollback_to_savepoint(trx, savept);
		}
		break;
	case DB_LOCK_WAIT:
		lock_wait_suspend_thread(thr);

		if (trx->error_state != DB_SUCCESS) {
			que_thr_stop_for_mysql(thr);

			goto handle_new_error;
		}

		*new_err = err;

		return(TRUE); /* Operation needs to be retried. */

	case DB_DEADLOCK:
	case DB_LOCK_TABLE_FULL:
		/* Roll back the whole transaction; this resolution was added
		to version 3.23.43 */
		ut_ad(!is_sdi);
		trx_rollback_for_mysql(trx);
		break;

	case DB_MUST_GET_MORE_FILE_SPACE:

		ut_error;

	case DB_CORRUPTION:
	case DB_FOREIGN_EXCEED_MAX_CASCADE:
		break;
	default:
		ut_error;
	}

	if (trx->error_state != DB_SUCCESS) {
		*new_err = trx->error_state;
	} else {
		*new_err = err;
	}

	trx->error_state = DB_SUCCESS;

	return(FALSE);
}
