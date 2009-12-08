/*****************************************************************************

Copyright (c) 2007, 2009, Innobase Oy. All Rights Reserved.

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
@file include/lock0iter.h
Lock queue iterator type and function prototypes.

Created July 16, 2007 Vasil Dimov
*******************************************************/

#ifndef lock0iter_h
#define lock0iter_h

#include "univ.i"
#include "lock0types.h"

typedef struct lock_queue_iterator_struct {
	const lock_t*	current_lock;
	/* In case this is a record lock queue (not table lock queue)
	then bit_no is the record number within the heap in which the
	record is stored. */
	ulint		bit_no;
} lock_queue_iterator_t;

/*******************************************************************//**
Initialize lock queue iterator so that it starts to iterate from
"lock". bit_no specifies the record number within the heap where the
record is stored. It can be undefined (ULINT_UNDEFINED) in two cases:
1. If the lock is a table lock, thus we have a table lock queue;
2. If the lock is a record lock and it is a wait lock. In this case
   bit_no is calculated in this function by using
   lock_rec_find_set_bit(). There is exactly one bit set in the bitmap
   of a wait lock. */
UNIV_INTERN
void
lock_queue_iterator_reset(
/*======================*/
	lock_queue_iterator_t*	iter,	/*!< out: iterator */
	const lock_t*		lock,	/*!< in: lock to start from */
	ulint			bit_no);/*!< in: record number in the
					heap */

/*******************************************************************//**
Gets the previous lock in the lock queue, returns NULL if there are no
more locks (i.e. the current lock is the first one). The iterator is
receded (if not-NULL is returned).
@return	previous lock or NULL */

const lock_t*
lock_queue_iterator_get_prev(
/*=========================*/
	lock_queue_iterator_t*	iter);	/*!< in/out: iterator */

#endif /* lock0iter_h */
