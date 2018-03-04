/*****************************************************************************

Copyright (c) 1996, 2017, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/trx0roll.h
Transaction rollback

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0roll_h
#define trx0roll_h

#include "univ.i"
#include "trx0trx.h"
#include "trx0types.h"
#include "mtr0mtr.h"
#include "trx0sys.h"

#ifdef UNIV_HOTBACKUP
# include"que0que.h"
#endif /* UNIV_HOTBACKUP */

extern bool	trx_rollback_or_clean_is_active;

/*******************************************************************//**
Determines if this transaction is rolling back an incomplete transaction
in crash recovery.
@return TRUE if trx is an incomplete transaction that is being rolled
back in crash recovery */
ibool
trx_is_recv(
/*========*/
	const trx_t*	trx);	/*!< in: transaction */
/*******************************************************************//**
Returns a transaction savepoint taken at this point in time.
@return savepoint */
trx_savept_t
trx_savept_take(
/*============*/
	trx_t*	trx);	/*!< in: transaction */

/********************************************************************//**
Get next undo log record from redo and noredo rollback segments.
@return undo log record copied to heap, NULL if none left, or if the
undo number of the top record would be less than the limit */
trx_undo_rec_t*
trx_roll_pop_top_rec_of_trx(
/*========================*/
	trx_t*		trx,		/*!< in: transaction */
	undo_no_t	limit,		/*!< in: least undo number we need */
	roll_ptr_t*	roll_ptr,	/*!< out: roll pointer to undo record */
	mem_heap_t*	heap);		/*!< in: memory heap where copied */

/*******************************************************************//**
Rollback or clean up any incomplete transactions which were
encountered in crash recovery.  If the transaction already was
committed, then we clean up a possible insert undo log. If the
transaction was not yet committed, then we roll it back. */
void
trx_rollback_or_clean_recovered(
/*============================*/
	ibool	all);	/*!< in: FALSE=roll back dictionary transactions;
			TRUE=roll back all non-PREPARED transactions */

/** Rollback or clean up any incomplete transactions which were
encountered in crash recovery.  If the transaction already was
committed, then we clean up a possible insert undo log. If the
transaction was not yet committed, then we roll it back.
Note: this is done in a background thread. */
void
trx_recovery_rollback_thread();

/*********************************************************************//**
Creates a rollback command node struct.
@return own: rollback node struct */
roll_node_t*
roll_node_create(
/*=============*/
	mem_heap_t*	heap);	/*!< in: mem heap where created */
/***********************************************************//**
Performs an execution step for a rollback command node in a query graph.
@return query thread to run next, or NULL */
que_thr_t*
trx_rollback_step(
/*==============*/
	que_thr_t*	thr);	/*!< in: query thread */
/*******************************************************************//**
Rollback a transaction used in MySQL.
@return error code or DB_SUCCESS */
dberr_t
trx_rollback_for_mysql(
/*===================*/
	trx_t*	trx);	/*!< in/out: transaction */
/*******************************************************************//**
Rollback the latest SQL statement for MySQL.
@return error code or DB_SUCCESS */
dberr_t
trx_rollback_last_sql_stat_for_mysql(
/*=================================*/
	trx_t*	trx);	/*!< in/out: transaction */
/*******************************************************************//**
Rollback a transaction to a given savepoint or do a complete rollback.
@return error code or DB_SUCCESS */
dberr_t
trx_rollback_to_savepoint(
/*======================*/
	trx_t*		trx,	/*!< in: transaction handle */
	trx_savept_t*	savept);	/*!< in: pointer to savepoint undo number, if
				partial rollback requested, or NULL for
				complete rollback */
/*******************************************************************//**
Rolls back a transaction back to a named savepoint. Modifications after the
savepoint are undone but InnoDB does NOT release the corresponding locks
which are stored in memory. If a lock is 'implicit', that is, a new inserted
row holds a lock where the lock information is carried by the trx id stored in
the row, these locks are naturally released in the rollback. Savepoints which
were set after this savepoint are deleted.
@return if no savepoint of the name found then DB_NO_SAVEPOINT,
otherwise DB_SUCCESS */
dberr_t
trx_rollback_to_savepoint_for_mysql(
/*================================*/
	trx_t*		trx,			/*!< in: transaction handle */
	const char*	savepoint_name,		/*!< in: savepoint name */
	int64_t*	mysql_binlog_cache_pos)	/*!< out: the MySQL binlog cache
						position corresponding to this
						savepoint; MySQL needs this
						information to remove the
						binlog entries of the queries
						executed after the savepoint */
	MY_ATTRIBUTE((warn_unused_result));
/*******************************************************************//**
Creates a named savepoint. If the transaction is not yet started, starts it.
If there is already a savepoint of the same name, this call erases that old
savepoint and replaces it with a new. Savepoints are deleted in a transaction
commit or rollback.
@return always DB_SUCCESS */
dberr_t
trx_savepoint_for_mysql(
/*====================*/
	trx_t*		trx,			/*!< in: transaction handle */
	const char*	savepoint_name,		/*!< in: savepoint name */
	int64_t		binlog_cache_pos);	/*!< in: MySQL binlog cache
						position corresponding to this
						connection at the time of the
						savepoint */
/*******************************************************************//**
Releases a named savepoint. Savepoints which
were set after this savepoint are deleted.
@return if no savepoint of the name found then DB_NO_SAVEPOINT,
otherwise DB_SUCCESS */
dberr_t
trx_release_savepoint_for_mysql(
/*============================*/
	trx_t*		trx,			/*!< in: transaction handle */
	const char*	savepoint_name)		/*!< in: savepoint name */
	MY_ATTRIBUTE((warn_unused_result));
/*******************************************************************//**
Frees savepoint structs starting from savep. */
void
trx_roll_savepoints_free(
/*=====================*/
	trx_t*			trx,	/*!< in: transaction handle */
	trx_named_savept_t*	savep);	/*!< in: free all savepoints > this one;
					if this is NULL, free all savepoints
					of trx */
/** Rollback node states */
enum roll_node_state {
	ROLL_NODE_NONE = 0,		/*!< Unknown state */
	ROLL_NODE_SEND,			/*!< about to send a rollback signal to
					the transaction */
	ROLL_NODE_WAIT			/*!< rollback signal sent to the
					transaction, waiting for completion */
};

/** Rollback command node in a query graph */
struct roll_node_t{
	que_common_t		common;	/*!< node type: QUE_NODE_ROLLBACK */
	enum roll_node_state	state;	/*!< node execution state */
	bool			partial;/*!< TRUE if we want a partial
					rollback */
	trx_savept_t		savept;	/*!< savepoint to which to
					roll back, in the case of a
					partial rollback */
	que_thr_t*		undo_thr;/*!< undo query graph */
};

/** A savepoint set with SQL's "SAVEPOINT savepoint_id" command */
struct trx_named_savept_t{
	char*		name;		/*!< savepoint name */
	trx_savept_t	savept;		/*!< the undo number corresponding to
					the savepoint */
	int64_t		mysql_binlog_cache_pos;
					/*!< the MySQL binlog cache position
					corresponding to this savepoint, not
					defined if the MySQL binlogging is not
					enabled */
	UT_LIST_NODE_T(trx_named_savept_t)
			trx_savepoints;	/*!< the list of savepoints of a
					transaction */
};

#include "trx0roll.ic"

#endif
