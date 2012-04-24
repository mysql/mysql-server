/*****************************************************************************

Copyright (c) 1997, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/row0umod.h
Undo modify of a row

Created 2/27/1997 Heikki Tuuri
*******************************************************/

#ifndef row0umod_h
#define row0umod_h

#include "univ.i"
#include "data0data.h"
#include "dict0types.h"
#include "trx0types.h"
#include "que0types.h"
#include "row0types.h"
#include "mtr0mtr.h"

/***********************************************************//**
Undoes a modify operation on a row of a table.
@return	DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
row_undo_mod(
/*=========*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr)	/*!< in: query thread */
	__attribute__((nonnull, warn_unused_result));

#ifndef UNIV_NONINL
#include "row0umod.ic"
#endif

#endif
