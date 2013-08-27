/*****************************************************************************

Copyright (c) 1996, 2013, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/trx0types.h
Transaction system global type definitions

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0types_h
#define trx0types_h

#include "ut0byte.h"

/** printf(3) format used for printing DB_TRX_ID and other system fields */
#define TRX_ID_FMT		IB_ID_FMT

/** maximum length that a formatted trx_t::id could take, not including
the terminating NUL character. */
#define TRX_ID_MAX_LEN		17

/** Transaction execution states when trx->state == TRX_STATE_ACTIVE */
enum trx_que_t {
	TRX_QUE_RUNNING,		/*!< transaction is running */
	TRX_QUE_LOCK_WAIT,		/*!< transaction is waiting for
					a lock */
	TRX_QUE_ROLLING_BACK,		/*!< transaction is rolling back */
	TRX_QUE_COMMITTING		/*!< transaction is committing */
};

/** Transaction states (trx_t::state) */
enum trx_state_t {
	TRX_STATE_NOT_STARTED,
	TRX_STATE_ACTIVE,
	TRX_STATE_PREPARED,			/* Support for 2PC/XA */
	TRX_STATE_COMMITTED_IN_MEMORY
};

/** Type of data dictionary operation */
enum trx_dict_op_t {
	/** The transaction is not modifying the data dictionary. */
	TRX_DICT_OP_NONE = 0,
	/** The transaction is creating a table or an index, or
	dropping a table.  The table must be dropped in crash
	recovery.  This and TRX_DICT_OP_NONE are the only possible
	operation modes in crash recovery. */
	TRX_DICT_OP_TABLE = 1,
	/** The transaction is creating or dropping an index in an
	existing table.  In crash recovery, the data dictionary
	must be locked, but the table must not be dropped. */
	TRX_DICT_OP_INDEX = 2
};

/** Memory objects */
/* @{ */
/** Transaction */
struct trx_t;
/** The locks and state of an active transaction */
struct trx_lock_t;
/** Transaction system */
struct trx_sys_t;
/** Signal */
struct trx_sig_t;
/** Rollback segment */
struct trx_rseg_t;
/** Transaction undo log */
struct trx_undo_t;
/** Array of undo numbers of undo records being rolled back or purged */
struct trx_undo_arr_t;
/** A cell of trx_undo_arr_t */
struct trx_undo_inf_t;
/** The control structure used in the purge operation */
struct trx_purge_t;
/** Rollback command node in a query graph */
struct roll_node_t;
/** Commit command node in a query graph */
struct commit_node_t;
/** SAVEPOINT command node in a query graph */
struct trx_named_savept_t;
/* @} */

/** Rollback contexts */
enum trx_rb_ctx {
	RB_NONE = 0,	/*!< no rollback */
	RB_NORMAL,	/*!< normal rollback */
	RB_RECOVERY_PURGE_REC,
			/*!< rolling back an incomplete transaction,
			in crash recovery, rolling back an
			INSERT that was performed by updating a
			delete-marked record; if the delete-marked record
			no longer exists in an active read view, it will
			be purged */
	RB_RECOVERY	/*!< rolling back an incomplete transaction,
			in crash recovery */
};

/** Row identifier (DB_ROW_ID, DATA_ROW_ID) */
typedef ib_id_t	row_id_t;
/** Transaction identifier (DB_TRX_ID, DATA_TRX_ID) */
typedef ib_id_t	trx_id_t;
/** Rollback pointer (DB_ROLL_PTR, DATA_ROLL_PTR) */
typedef ib_id_t	roll_ptr_t;
/** Undo number */
typedef ib_id_t	undo_no_t;

/** Maximum transaction identifier */
#define TRX_ID_MAX	IB_ID_MAX

/** Transaction savepoint */
struct trx_savept_t{
	undo_no_t	least_undo_no;	/*!< least undo number to undo */
};

/** File objects */
/* @{ */
/** Transaction system header */
typedef byte	trx_sysf_t;
/** Rollback segment header */
typedef byte	trx_rsegf_t;
/** Undo segment header */
typedef byte	trx_usegf_t;
/** Undo log header */
typedef byte	trx_ulogf_t;
/** Undo log page header */
typedef byte	trx_upagef_t;

/** Undo log record */
typedef	byte	trx_undo_rec_t;
/* @} */

#endif
