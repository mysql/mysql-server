/***************************************************************************//**

Copyright (c) 2010, Oracle Corpn. All Rights Reserved.

Portions of this file contain modifications contributed and copyrighted by
Sun Microsystems, Inc. Those modifications are gratefully acknowledged and
are described briefly in the InnoDB documentation. The contributions by
Sun Microsystems are incorporated with their permission, and subject to the
conditions contained in the file COPYING.Sun_Microsystems.

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

/******************************************************************//**
@file ut/ut0bh.c
Binary min-heap implementation.

Created 2010-05-28 by Sunny Bains
*******************************************************/

#include "ut0bh.h"
#include "ut0mem.h"

#include <string.h>

/** Binary heap data structure */
struct ib_bh_struct {
	ulint		max_elems;		/*!< max elements allowed */
	ulint		n_elems;		/*!< current size */
	ulint		sizeof_elem;		/*!< sizeof element */
	ib_bh_cmp_t	compare;		/*!< comparator */
};

/**********************************************************************//**
Get the number of elements in the binary heap.
@return number of elements */
UNIV_INTERN
ulint
ib_bh_size(
/*=======*/
	const ib_bh_t*	ib_bh)			/*!< in: instance */
{
	return(ib_bh->n_elems);
}

/**********************************************************************//**
Test if binary heap is empty.
@return TRUE if empty. */
UNIV_INTERN
ibool
ib_bh_is_empty(
/*===========*/
	const ib_bh_t*	ib_bh)			/*!< in: instance */
{
	return(ib_bh_size(ib_bh) == 0);
}

/**********************************************************************//**
Test if binary heap is full.
@return TRUE if full. */
UNIV_INTERN
ibool
ib_bh_is_full(
/*===========*/
	const ib_bh_t*	ib_bh)			/*!< in: instance */
{
	return(ib_bh_size(ib_bh) >= ib_bh->max_elems);
}

/**********************************************************************//**
Get a pointer to the element.
@return pointer to element */
UNIV_INTERN
void*
ib_bh_get(
/*=======*/
	ib_bh_t*	ib_bh,			/*!< in: instance */
	ulint		i)			/*!< in: index */
{
	byte*		ptr = (byte*) (ib_bh + 1);

	ut_a(i < ib_bh_size(ib_bh));

	return(ptr + (ib_bh->sizeof_elem * i));
}

/**********************************************************************//**
Copy an element to the binary heap.
@return pointer to copied element */
UNIV_INTERN
void*
ib_bh_set(
/*======*/
	ib_bh_t*	ib_bh,			/*!< in,out: instance */
	ulint		i,			/*!< in: index */
	const void*	elem)			/*!< in: element to add */
{
	void*		ptr = ib_bh_get(ib_bh, i);

	memcpy(ptr, elem, ib_bh->sizeof_elem);

	return(ptr);
}

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
	ib_bh_t*	ib_bh)			/*!< in,own: instance */
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
	ib_bh_t*	ib_bh,			/*!< in,out: instance */
	const void*	elem)			/*!< in: element to add */
{
	void*		ptr = NULL;

	if (!ib_bh_is_full(ib_bh)) {
		ulint	i;

		if (ib_bh_is_empty(ib_bh)) {
			++ib_bh->n_elems;
			return(ib_bh_set(ib_bh, 0, elem));
		}

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
Return the first element from the binary heap. 
@return pointer to first element or NULL if empty. */
UNIV_INTERN
void*
ib_bh_first(
/*========*/
	ib_bh_t*	ib_bh)			/*!< in,out: instance */
{
	return(ib_bh_is_empty(ib_bh) ? NULL : ib_bh_get(ib_bh, 0));
}

/**********************************************************************//**
Return the last element from the binary heap. 
@return pointer to last element or NULL if empty. */
UNIV_INTERN
void*
ib_bh_last(
/*========*/
	ib_bh_t*	ib_bh)			/*!< in,out: instance */
{
	return(ib_bh_is_empty(ib_bh)
	       ? NULL 
	       : ib_bh_get(ib_bh, ib_bh_size(ib_bh) - 1));
}

/**********************************************************************//**
Remove the first element from the binary heap. */
UNIV_INTERN
void
ib_bh_pop(
/*======*/
	ib_bh_t*	ib_bh)			/*!< in,out: instance */
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

		if (ib_bh->compare(last, ptr) > 0) {
			ib_bh_set(ib_bh, parent, ptr);
		} else {
			break;
		}

		parent = (ptr - (byte*) ib_bh_first(ib_bh)) / ib_bh->sizeof_elem;

		if ((parent << 1) < ib_bh_size(ib_bh)) {
			ptr = (byte*) ib_bh_get(ib_bh, parent << 1);
		} else {
			break;
		}
	}

	--ib_bh->n_elems;

	ib_bh_set(ib_bh, parent, last);
}
