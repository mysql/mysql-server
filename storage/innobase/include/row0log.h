/*****************************************************************************

Copyright (c) 2011, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/row0log.h
Modification log for online index creation

Created 2011-05-26 Marko Makela
*******************************************************/

#ifndef row0log_h
#define row0log_h

#include "univ.i"
#include "row0types.h"
#include "data0types.h"
#include "dict0types.h"
#include "trx0types.h"

/******************************************************//**
Allocate the row log for an index and flag the index
for online creation.
@retval true if success, false if not */
UNIV_INTERN
bool
row_log_allocate(
/*=============*/
	dict_index_t*	index)	/*!< in/out: index */
	__attribute__((nonnull));
/******************************************************//**
Free the row log for an index on which online creation was aborted. */
UNIV_INTERN
void
row_log_free(
/*=========*/
	dict_index_t*	index)	/*!< in/out: index */
	__attribute__((nonnull));

/******************************************************//**
Logs an operation to a secondary index that is (or was) being created. */
UNIV_INTERN
void
row_log_online_op(
/*==============*/
	dict_index_t*	index,	/*!< in/out: index, S-locked */
	const dtuple_t*	tuple,	/*!< in: index tuple */
	trx_id_t	trx_id,	/*!< in: transaction ID or 0 if not known */
	enum row_op	op)	/*!< in: operation */
	UNIV_COLD __attribute__((nonnull));

/******************************************************//**
Get the latest transaction ID that has invoked row_log_online_op()
during online creation.
@return latest transaction ID, or 0 if nothing was logged */
UNIV_INTERN
trx_id_t
row_log_get_max_trx(
/*================*/
	dict_index_t*	index)	/*!< in: index, must be locked */
	__attribute__((nonnull, warn_unused_result));

/******************************************************//**
Merge the row log to the index upon completing index creation.
@return DB_SUCCESS, or error code on failure */
UNIV_INTERN
ulint
row_log_apply(
/*==========*/
	trx_t*		trx,	/*!< in: transaction (for checking if
				the operation was interrupted) */
	dict_index_t*	index,	/*!< in/out: index */
	struct TABLE*	table)	/*!< in/out: MySQL table
				(for reporting duplicates) */
	__attribute__((nonnull, warn_unused_result));

#endif /* row0log.h */
