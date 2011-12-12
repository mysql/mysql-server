/*****************************************************************************

Copyright (c) 1996, 2011, Oracle and/or its affiliates. All Rights Reserved.

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
enum trx_que_enum {
	TRX_QUE_RUNNING,		/*!< transaction is running */
	TRX_QUE_LOCK_WAIT,		/*!< transaction is waiting for
					a lock */
	TRX_QUE_ROLLING_BACK,		/*!< transaction is rolling back */
	TRX_QUE_COMMITTING		/*!< transaction is committing */
};

/** Transaction states (trx_t::state) */
enum trx_state_enum {
	TRX_STATE_NOT_STARTED,
	TRX_STATE_ACTIVE,
	TRX_STATE_PREPARED,			/* Support for 2PC/XA */
	TRX_STATE_COMMITTED_IN_MEMORY
};

/** Memory objects */
/* @{ */
/** Transaction */
typedef struct trx_struct	trx_t;
/** The locks and state of an active transaction */
typedef struct trx_lock_struct	trx_lock_t;
/** Transaction system */
typedef struct trx_sys_struct	trx_sys_t;
/** Doublewrite information */
typedef struct trx_doublewrite_struct	trx_doublewrite_t;
/** Signal */
typedef struct trx_sig_struct	trx_sig_t;
/** Rollback segment */
typedef struct trx_rseg_struct	trx_rseg_t;
/** Transaction undo log */
typedef struct trx_undo_struct	trx_undo_t;
/** Array of undo numbers of undo records being rolled back or purged */
typedef struct trx_undo_arr_struct trx_undo_arr_t;
/** A cell of trx_undo_arr_t */
typedef struct trx_undo_inf_struct trx_undo_inf_t;
/** The control structure used in the purge operation */
typedef struct trx_purge_struct	trx_purge_t;
/** Rollback command node in a query graph */
typedef struct roll_node_struct	roll_node_t;
/** Commit command node in a query graph */
typedef struct commit_node_struct commit_node_t;
/** SAVEPOINT command node in a query graph */
typedef struct trx_named_savept_struct trx_named_savept_t;
/** Transaction concurrency state */
typedef enum trx_state_enum trx_state_t;
/** Transaction query thread state */
typedef enum trx_que_enum trx_que_t;
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

/** Transaction savepoint */
typedef struct trx_savept_struct trx_savept_t;
/** Transaction savepoint */
struct trx_savept_struct{
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
