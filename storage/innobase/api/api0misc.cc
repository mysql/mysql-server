/*****************************************************************************

Copyright (c) 2008, 2012, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file api/api0misc.cc
InnoDB Native API

2008-08-01 Created by Sunny Bains
3/20/2011 Jimmy Yang extracted from Embedded InnoDB
*******************************************************/

#include <errno.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include "api0misc.h"
#include "trx0roll.h"
#include "srv0srv.h"
#include "dict0mem.h"
#include "dict0dict.h"
#include "pars0pars.h"
#include "row0sel.h"
#include "lock0lock.h"
#include "ha_prototypes.h"
#include <m_ctype.h>
#include <mysys_err.h>
#include <mysql/plugin.h>

/*********************************************************************//**
Sets a lock on a table.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
ib_trx_lock_table_with_retry(
/*=========================*/
	trx_t*		trx,		/*!< in/out: transaction */
	dict_table_t*	table,		/*!< in: table to lock */
	enum lock_mode	mode)		/*!< in: LOCK_X or LOCK_S */
{
	que_thr_t*	thr;
	dberr_t		err;
	mem_heap_t*	heap;
	sel_node_t*	node;

	heap = mem_heap_create(512);

	trx->op_info = "setting table lock";

	node = sel_node_create(heap);
	thr = pars_complete_graph_for_exec(node, trx, heap);
	thr->graph->state = QUE_FORK_ACTIVE;

	/* We use the select query graph as the dummy graph needed
	in the lock module call */

	thr = que_fork_get_first_thr(static_cast<que_fork_t*>(
		que_node_get_parent(thr)));
	que_thr_move_to_run_state_for_mysql(thr, trx);

run_again:
	thr->run_node = thr;
	thr->prev_node = thr->common.parent;

	err = lock_table(0, table, mode, thr);

	trx->error_state = err;

	if (UNIV_LIKELY(err == DB_SUCCESS)) {
		que_thr_stop_for_mysql_no_error(thr, trx);
	} else {
		que_thr_stop_for_mysql(thr);

		if (err != DB_QUE_THR_SUSPENDED) {
			ibool	was_lock_wait;

			was_lock_wait = ib_handle_errors(&err, trx, thr, NULL);

			if (was_lock_wait) {
				goto run_again;
			}
		} else {
			que_thr_t*	run_thr;
			que_node_t*	parent;

			parent = que_node_get_parent(thr);
			run_thr = que_fork_start_command(
				static_cast<que_fork_t*>(parent));

			ut_a(run_thr == thr);

			/* There was a lock wait but the thread was not
			in a ready to run or running state. */
			trx->error_state = DB_LOCK_WAIT;

			goto run_again;
		}
	}

	que_graph_free(thr->graph);
	trx->op_info = "";

	return(err);
}
/****************************************************************//**
Handles user errors and lock waits detected by the database engine.
@return TRUE if it was a lock wait and we should continue running
the query thread */
UNIV_INTERN
ibool
ib_handle_errors(
/*=============*/
        dberr_t*	new_err,/*!< out: possible new error encountered in
                                lock wait, or if no new error, the value
                                of trx->error_state at the entry of this
                                function */
        trx_t*          trx,    /*!< in: transaction */
        que_thr_t*      thr,    /*!< in: query thread */
        trx_savept_t*   savept) /*!< in: savepoint or NULL */
{
        dberr_t		err;
handle_new_error:
        err = trx->error_state;

        ut_a(err != DB_SUCCESS);

        trx->error_state = DB_SUCCESS;

        switch (err) {
        case DB_LOCK_WAIT_TIMEOUT:
		trx_rollback_for_mysql(trx);
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

                trx_rollback_for_mysql(trx);
                break;

        case DB_MUST_GET_MORE_FILE_SPACE:

                exit(1);

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
