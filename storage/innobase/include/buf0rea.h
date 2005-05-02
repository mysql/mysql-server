/******************************************************
The database buffer read

(c) 1995 Innobase Oy

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#ifndef buf0rea_h
#define buf0rea_h

#include "univ.i"
#include "buf0types.h"

/************************************************************************
High-level function which reads a page asynchronously from a file to the
buffer buf_pool if it is not already there. Sets the io_fix flag and sets
an exclusive lock on the buffer frame. The flag is cleared and the x-lock
released by the i/o-handler thread. Does a random read-ahead if it seems
sensible. */

ulint
buf_read_page(
/*==========*/
			/* out: number of page read requests issued: this can
			be > 1 if read-ahead occurred */
	ulint	space,	/* in: space id */
	ulint	offset);/* in: page number */
/************************************************************************
Applies linear read-ahead if in the buf_pool the page is a border page of
a linear read-ahead area and all the pages in the area have been accessed.
Does not read any page if the read-ahead mechanism is not activated. Note
that the the algorithm looks at the 'natural' adjacent successor and
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
which could result in a deadlock if the OS does not support asynchronous io. */

ulint
buf_read_ahead_linear(
/*==================*/
			/* out: number of page read requests issued */
	ulint	space,	/* in: space id */
	ulint	offset);/* in: page number of a page; NOTE: the current thread
			must want access to this page (see NOTE 3 above) */
/************************************************************************
Issues read requests for pages which the ibuf module wants to read in, in
order to contract the insert buffer tree. Technically, this function is like
a read-ahead function. */

void
buf_read_ibuf_merge_pages(
/*======================*/
	ibool	sync,		/* in: TRUE if the caller wants this function
				to wait for the highest address page to get
				read in, before this function returns */
	ulint*	space_ids,	/* in: array of space ids */
	ib_longlong* space_versions,/* in: the spaces must have this version
				number (timestamp), otherwise we discard the
				read; we use this to cancel reads if
				DISCARD + IMPORT may have changed the
				tablespace size */
	ulint*	page_nos,	/* in: array of page numbers to read, with the
				highest page number the last in the array */
	ulint	n_stored);	/* in: number of page numbers in the array */
/************************************************************************
Issues read requests for pages which recovery wants to read in. */

void
buf_read_recv_pages(
/*================*/
	ibool	sync,		/* in: TRUE if the caller wants this function
				to wait for the highest address page to get
				read in, before this function returns */
	ulint	space,		/* in: space id */
	ulint*	page_nos,	/* in: array of page numbers to read, with the
				highest page number the last in the array */
	ulint	n_stored);	/* in: number of page numbers in the array */

/* The size in pages of the area which the read-ahead algorithms read if
invoked */

#define	BUF_READ_AHEAD_AREA	ut_min(64, ut_2_power_up(buf_pool->curr_size / 32))

/* Modes used in read-ahead */
#define BUF_READ_IBUF_PAGES_ONLY	131
#define BUF_READ_ANY_PAGE		132

#endif
