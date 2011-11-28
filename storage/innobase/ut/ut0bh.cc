/***************************************************************************//**

Copyright (c) 2010, 2011, Oracle and/or its affiliates. All Rights Reserved.

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

/******************************************************************//**
@file ut/ut0bh.cc
Binary min-heap implementation.

Created 2010-05-28 by Sunny Bains
*******************************************************/

#include "ut0bh.h"
#include "ut0mem.h"

#ifdef UNIV_NONINL
#include "ut0bh.ic"
#endif

#include <string.h>

/**********************************************************************//**
Create a binary heap.
@return a new binary heap */
UNIV_INTERN
ib_bh_t*
ib_bh_create(
/*=========*/
	ib_bh_cmp_t	compare,		/*!< in: comparator */
	ulint		sizeof_elem,		/*!< in: size of one element */
	ulint		max_elems)		/*!< in: max elements allowed */
{
	ulint		sz;
	ib_bh_t*	ib_bh;

	sz = sizeof(*ib_bh) + (sizeof_elem * max_elems);

	ib_bh = (ib_bh_t*) ut_malloc(sz);
	memset(ib_bh, 0x0, sz);

	ib_bh->compare = compare;
	ib_bh->max_elems = max_elems;
	ib_bh->sizeof_elem = sizeof_elem;

	return(ib_bh);
}

/**********************************************************************//**
Free a binary heap.
@return a new binary heap */
UNIV_INTERN
void
ib_bh_free(
/*=======*/
	ib_bh_t*	ib_bh)			/*!< in/own: instance */
{
	ut_free(ib_bh);
}

/**********************************************************************//**
Add an element to the binary heap. Note: The element is copied.
@return pointer to added element or NULL if full. */
UNIV_INTERN
void*
ib_bh_push(
/*=======*/
	ib_bh_t*	ib_bh,			/*!< in/out: instance */
	const void*	elem)			/*!< in: element to add */
{
	void*		ptr;

	if (ib_bh_is_full(ib_bh)) {
		return(NULL);
	} else if (ib_bh_is_empty(ib_bh)) {
		++ib_bh->n_elems;
		return(ib_bh_set(ib_bh, 0, elem));
	} else {
		ulint	i;

		i = ib_bh->n_elems;

		++ib_bh->n_elems;

		for (ptr = ib_bh_get(ib_bh, i >> 1);
		     i > 0 && ib_bh->compare(ptr, elem) > 0;
		     i >>= 1, ptr = ib_bh_get(ib_bh, i >> 1)) {

			ib_bh_set(ib_bh, i, ptr);
		}

		ptr = ib_bh_set(ib_bh, i, elem);
	}

	return(ptr);
}

/**********************************************************************//**
Remove the first element from the binary heap. */
UNIV_INTERN
void
ib_bh_pop(
/*======*/
	ib_bh_t*	ib_bh)			/*!< in/out: instance */
{
	byte*		ptr;
	byte*		last;
	ulint		parent = 0;

	if (ib_bh_is_empty(ib_bh)) {
		return;
	} else if (ib_bh_size(ib_bh) == 1) {
		--ib_bh->n_elems;
		return;
	}

	last = (byte*) ib_bh_last(ib_bh);

	/* Start from the child node */
	ptr = (byte*) ib_bh_get(ib_bh, 1);

	while (ptr < last) {
		/* If the "right" child node is < "left" child node */
		if (ib_bh->compare(ptr + ib_bh->sizeof_elem, ptr) < 0) {
			ptr += ib_bh->sizeof_elem;
		}

		if (ib_bh->compare(last, ptr) <= 0) {
			break;
		}

		ib_bh_set(ib_bh, parent, ptr);

		parent = (ptr - (byte*) ib_bh_first(ib_bh))
		       / ib_bh->sizeof_elem;

		if ((parent << 1) >= ib_bh_size(ib_bh)) {
			break;
		}

		ptr = (byte*) ib_bh_get(ib_bh, parent << 1);
	}

	--ib_bh->n_elems;

	ib_bh_set(ib_bh, parent, last);
}
