/*****************************************************************************

Copyright (c) 1997, 2016, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/row0uins.h
Fresh insert undo

Created 2/25/1997 Heikki Tuuri
*******************************************************/

#ifndef row0uins_h
#define row0uins_h

#include "univ.i"
#include "data0data.h"
#include "dict0types.h"
#include "trx0types.h"
#include "que0types.h"
#include "row0types.h"
#include "mtr0mtr.h"

/***********************************************************//**
Undoes a fresh insert of a row to a table. A fresh insert means that
the same clustered index unique key did not have any record, even delete
marked, at the time of the insert.  InnoDB is eager in a rollback:
if it figures out that an index record will be removed in the purge
anyway, it will remove it in the rollback.
@return DB_SUCCESS */
dberr_t
row_undo_ins(
/*=========*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr)	/*!< in: query thread */
	MY_ATTRIBUTE((nonnull, warn_unused_result));
#ifndef UNIV_NONINL
#include "row0uins.ic"
#endif

#endif
