/*****************************************************************************

Copyright (c) 1995, 2016, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/buf0rea.h
The database buffer read

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#ifndef buf0rea_h
#define buf0rea_h

#include "univ.i"
#include "buf0buf.h"
#include "buf0types.h"

/** High-level function which reads a page asynchronously from a file to the
buffer buf_pool if it is not already there. Sets the io_fix flag and sets
an exclusive lock on the buffer frame. The flag is cleared and the x-lock
released by the i/o-handler thread.
@param[in]	page_id		page id
@param[in]	page_size	page size
@return TRUE if page has been read in, FALSE in case of failure */
ibool
buf_read_page(
	const page_id_t&	page_id,
	const page_size_t&	page_size);

/** High-level function which reads a page asynchronously from a file to the
buffer buf_pool if it is not already there. Sets the io_fix flag and sets
an exclusive lock on the buffer frame. The flag is cleared and the x-lock
released by the i/o-handler thread.
@param[in]	page_id		page id
@param[in]	page_size	page size
@param[in]	sync		true if synchronous aio is desired
@return TRUE if page has been read in, FALSE in case of failure */
ibool
buf_read_page_background(
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	bool			sync);

/** Applies a random read-ahead in buf_pool if there are at least a threshold
value of accessed pages from the random read-ahead area. Does not read any
page, not even the one at the position (space, offset), if the read-ahead
mechanism is not activated. NOTE 1: the calling thread may own latches on
pages: to avoid deadlocks this function must be written such that it cannot
end up waiting for these latches! NOTE 2: the calling thread must want
access to the page given: this rule is set to prevent unintended read-aheads
performed by ibuf routines, a situation which could result in a deadlock if
the OS does not support asynchronous i/o.
@param[in]	page_id		page id of a page which the current thread
wants to access
@param[in]	page_size	page size
@param[in]	inside_ibuf	TRUE if we are inside ibuf routine
@return number of page read requests issued; NOTE that if we read ibuf
pages, it may happen that the page at the given page number does not
get read even if we return a positive value! */
ulint
buf_read_ahead_random(
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	ibool			inside_ibuf);

/** Applies linear read-ahead if in the buf_pool the page is a border page of
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
@param[in]	page_id		page id; see NOTE 3 above
@param[in]	page_size	page size
@param[in]	inside_ibuf	TRUE if we are inside ibuf routine
@return number of page read requests issued */
ulint
buf_read_ahead_linear(
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	ibool			inside_ibuf);

/********************************************************************//**
Issues read requests for pages which the ibuf module wants to read in, in
order to contract the insert buffer tree. Technically, this function is like
a read-ahead function. */
void
buf_read_ibuf_merge_pages(
/*======================*/
	bool		sync,			/*!< in: true if the caller
						wants this function to wait
						for the highest address page
						to get read in, before this
						function returns */
	const space_id_t*	space_ids,	/*!< in: array of space ids */
	const page_no_t*	page_nos,	/*!< in: array of page numbers
						to read, with the highest page
						number the last in the
						array */
	ulint			n_stored);	/*!< in: number of elements
						in the arrays */

/** Issues read requests for pages which recovery wants to read in.
@param[in]	sync		true if the caller wants this function to wait
for the highest address page to get read in, before this function returns
@param[in]	space_id	tablespace id
@param[in]	page_nos	array of page numbers to read, with the
highest page number the last in the array
@param[in]	n_stored	number of page numbers in the array */

void
buf_read_recv_pages(
	bool			sync,
	space_id_t		space_id,
	const page_no_t*	page_nos,
	ulint			n_stored);

/** The size in pages of the area which the read-ahead algorithms read if
invoked */
#define	BUF_READ_AHEAD_AREA(b)		((b)->read_ahead_area)

/** @name Modes used in read-ahead @{ */
/** read only pages belonging to the insert buffer tree */
#define BUF_READ_IBUF_PAGES_ONLY	131
/** read any page */
#define BUF_READ_ANY_PAGE		132
/* @} */

#endif
