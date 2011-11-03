/*****************************************************************************

Copyright (c) 1995, 2009, Innobase Oy. All Rights Reserved.

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
@file include/buf0rea.h
The database buffer read

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#ifndef buf0rea_h
#define buf0rea_h

#include "univ.i"
#include "buf0types.h"

/********************************************************************//**
High-level function which reads a page asynchronously from a file to the
buffer buf_pool if it is not already there. Sets the io_fix flag and sets
an exclusive lock on the buffer frame. The flag is cleared and the x-lock
released by the i/o-handler thread.
@return TRUE if page has been read in, FALSE in case of failure */
UNIV_INTERN
ibool
buf_read_page(
/*==========*/
	ulint	space,	/*!< in: space id */
	ulint	zip_size,/*!< in: compressed page size in bytes, or 0 */
	ulint	offset);/*!< in: page number */
/********************************************************************//**
Applies a random read-ahead in buf_pool if there are at least a threshold
value of accessed pages from the random read-ahead area. Does not read any
page, not even the one at the position (space, offset), if the read-ahead
mechanism is not activated. NOTE 1: the calling thread may own latches on
pages: to avoid deadlocks this function must be written such that it cannot
end up waiting for these latches! NOTE 2: the calling thread must want
access to the page given: this rule is set to prevent unintended read-aheads
performed by ibuf routines, a situation which could result in a deadlock if
the OS does not support asynchronous i/o.
@return number of page read requests issued; NOTE that if we read ibuf
pages, it may happen that the page at the given page number does not
get read even if we return a positive value!
@return	number of page read requests issued */
UNIV_INTERN
ulint
buf_read_ahead_random(
/*==================*/
	ulint	space,		/*!< in: space id */
	ulint	zip_size,	/*!< in: compressed page size in bytes,
				or 0 */
	ulint	offset,		/*!< in: page number of a page which
				the current thread wants to access */
	ibool	inside_ibuf);	/*!< in: TRUE if we are inside ibuf
				routine */
/********************************************************************//**
Applies linear read-ahead if in the buf_pool the page is a border page of
a linear read-ahead area and all the pages in the area have been accessed.
Does not read any page if the read-ahead mechanism is not activated. Note
that the algorithm looks at the 'natural' adjacent successor and
predecessor of the page, which on the leaf level of a B-tree are the next
and previous page in the chain of leaves. To know these, the page specified
in (space, offset) must already be present in the buf_pool. Thus, the
natural way to use this function is to call it when a page in the buf_pool
is accessed the first time, calling this function just after it has been
bufferfixed.
NOTE 1: as this function looks at the natural predecessor and successor
fields on the page, what happens, if these are not initialized to any
sensible value? No problem, before applying read-ahead we check that the
area to read is within the span of the space, if not, read-ahead is not
applied. An uninitialized value may result in a useless read operation, but
only very improbably.
NOTE 2: the calling thread may own latches on pages: to avoid deadlocks this
function must be written such that it cannot end up waiting for these
latches!
NOTE 3: the calling thread must want access to the page given: this rule is
set to prevent unintended read-aheads performed by ibuf routines, a situation
which could result in a deadlock if the OS does not support asynchronous io.
@return	number of page read requests issued */
UNIV_INTERN
ulint
buf_read_ahead_linear(
/*==================*/
	ulint	space,		/*!< in: space id */
	ulint	zip_size,	/*!< in: compressed page size in bytes, or 0 */
	ulint	offset,		/*!< in: page number; see NOTE 3 above */
	ibool	inside_ibuf);	/*!< in: TRUE if we are inside ibuf routine */
/********************************************************************//**
Issues read requests for pages which the ibuf module wants to read in, in
order to contract the insert buffer tree. Technically, this function is like
a read-ahead function. */
UNIV_INTERN
void
buf_read_ibuf_merge_pages(
/*======================*/
	ibool		sync,		/*!< in: TRUE if the caller
					wants this function to wait
					for the highest address page
					to get read in, before this
					function returns */
	const ulint*	space_ids,	/*!< in: array of space ids */
	const ib_int64_t* space_versions,/*!< in: the spaces must have
					this version number
					(timestamp), otherwise we
					discard the read; we use this
					to cancel reads if DISCARD +
					IMPORT may have changed the
					tablespace size */
	const ulint*	page_nos,	/*!< in: array of page numbers
					to read, with the highest page
					number the last in the
					array */
	ulint		n_stored);	/*!< in: number of elements
					in the arrays */
/********************************************************************//**
Issues read requests for pages which recovery wants to read in. */
UNIV_INTERN
void
buf_read_recv_pages(
/*================*/
	ibool		sync,		/*!< in: TRUE if the caller
					wants this function to wait
					for the highest address page
					to get read in, before this
					function returns */
	ulint		space,		/*!< in: space id */
	ulint		zip_size,	/*!< in: compressed page size in
					bytes, or 0 */
	const ulint*	page_nos,	/*!< in: array of page numbers
					to read, with the highest page
					number the last in the
					array */
	ulint		n_stored);	/*!< in: number of page numbers
					in the array */

/** The size in pages of the area which the read-ahead algorithms read if
invoked */
#define	BUF_READ_AHEAD_AREA(b)					\
	ut_min(64, ut_2_power_up((b)->curr_size / 32))

/** @name Modes used in read-ahead @{ */
/** read only pages belonging to the insert buffer tree */
#define BUF_READ_IBUF_PAGES_ONLY	131
/** read any page */
#define BUF_READ_ANY_PAGE		132
/* @} */

#endif
