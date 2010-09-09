/*****************************************************************************

Copyright (c) 1997, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file include/read0read.h
Cursor read

Created 2/16/1997 Heikki Tuuri
*******************************************************/

#ifndef read0read_h
#define read0read_h

#include "univ.i"


#include "ut0byte.h"
#include "ut0lst.h"
#include "trx0trx.h"
#include "read0types.h"

/*********************************************************************//**
Opens a read view where exactly the transactions serialized before this
point in time are seen in the view.
@return	own: read view struct */
UNIV_INTERN
read_view_t*
read_view_open_now(
/*===============*/
	trx_id_t	cr_trx_id,	/*!< in: trx_id of creating
					transaction, or 0 used in purge */
	mem_heap_t*	heap);		/*!< in: memory heap from which
					allocated */
/*********************************************************************//**
Makes a copy of the oldest existing read view, or opens a new. The view
must be closed with ..._close.
@return	own: read view struct */
UNIV_INTERN
read_view_t*
read_view_oldest_copy_or_open_new(
/*==============================*/
	trx_id_t	cr_trx_id,	/*!< in: trx_id of creating
					transaction, or 0 used in purge */
	mem_heap_t*	heap);		/*!< in: memory heap from which
					allocated */
/*********************************************************************//**
Closes a read view. */
UNIV_INTERN
void
read_view_close(
/*============*/
	read_view_t*	view);	/*!< in: read view */
/*********************************************************************//**
Closes a consistent read view for MySQL. This function is called at an SQL
statement end if the trx isolation level is <= TRX_ISO_READ_COMMITTED. */
UNIV_INTERN
void
read_view_close_for_mysql(
/*======================*/
	trx_t*	trx);	/*!< in: trx which has a read view */
/*********************************************************************//**
Checks if a read view sees the specified transaction.
@return	TRUE if sees */
UNIV_INLINE
ibool
read_view_sees_trx_id(
/*==================*/
	const read_view_t*	view,	/*!< in: read view */
	trx_id_t		trx_id);/*!< in: trx id */
/*********************************************************************//**
Prints a read view to stderr. */
UNIV_INTERN
void
read_view_print(
/*============*/
	const read_view_t*	view);	/*!< in: read view */
/*********************************************************************//**
Create a consistent cursor view for mysql to be used in cursors. In this
consistent read view modifications done by the creating transaction or future
transactions are not visible. */
UNIV_INTERN
cursor_view_t*
read_cursor_view_create_for_mysql(
/*==============================*/
	trx_t*		cr_trx);/*!< in: trx where cursor view is created */
/*********************************************************************//**
Close a given consistent cursor view for mysql and restore global read view
back to a transaction read view. */
UNIV_INTERN
void
read_cursor_view_close_for_mysql(
/*=============================*/
	trx_t*		trx,		/*!< in: trx */
	cursor_view_t*	curview);	/*!< in: cursor view to be closed */
/*********************************************************************//**
This function sets a given consistent cursor view to a transaction
read view if given consistent cursor view is not NULL. Otherwise, function
restores a global read view to a transaction read view. */
UNIV_INTERN
void
read_cursor_set_for_mysql(
/*======================*/
	trx_t*		trx,	/*!< in: transaction where cursor is set */
	cursor_view_t*	curview);/*!< in: consistent cursor view to be set */

/** Read view lists the trx ids of those transactions for which a consistent
read should not see the modifications to the database. */

struct read_view_struct{
	ulint		type;	/*!< VIEW_NORMAL, VIEW_HIGH_GRANULARITY */
	undo_no_t	undo_no;/*!< 0 or if type is
				VIEW_HIGH_GRANULARITY
				transaction undo_no when this high-granularity
				consistent read view was created */
	trx_id_t	low_limit_no;
				/*!< The view does not need to see the undo
				logs for transactions whose transaction number
				is strictly smaller (<) than this value: they
				can be removed in purge if not needed by other
				views */
	trx_id_t	low_limit_id;
				/*!< The read should not see any transaction
				with trx id >= this value. In other words,
				this is the "high water mark". */
	trx_id_t	up_limit_id;
				/*!< The read should see all trx ids which
				are strictly smaller (<) than this value.
				In other words,
				this is the "low water mark". */
	ulint		n_trx_ids;
				/*!< Number of cells in the trx_ids array */
	trx_id_t*	trx_ids;/*!< Additional trx ids which the read should
				not see: typically, these are the active
				transactions at the time when the read is
				serialized, except the reading transaction
				itself; the trx ids in this array are in a
				descending order. These trx_ids should be
				between the "low" and "high" water marks,
				that is, up_limit_id and low_limit_id. */
	trx_id_t	creator_trx_id;
				/*!< trx id of creating transaction, or
				0 used in purge */
	UT_LIST_NODE_T(read_view_t) view_list;
				/*!< List of read views in trx_sys */
};

/** Read view types @{ */
#define VIEW_NORMAL		1	/*!< Normal consistent read view
					where transaction does not see changes
					made by active transactions except
					creating transaction. */
#define VIEW_HIGH_GRANULARITY	2	/*!< High-granularity read view where
					transaction does not see changes
					made by active transactions and own
					changes after a point in time when this
					read view was created. */
/* @} */

/** Implement InnoDB framework to support consistent read views in
cursors. This struct holds both heap where consistent read view
is allocated and pointer to a read view. */

struct cursor_view_struct{
	mem_heap_t*	heap;
				/*!< Memory heap for the cursor view */
	read_view_t*	read_view;
				/*!< Consistent read view of the cursor*/
	ulint		n_mysql_tables_in_use;
				/*!< number of Innobase tables used in the
				processing of this cursor */
};

#ifndef UNIV_NONINL
#include "read0read.ic"
#endif

#endif
