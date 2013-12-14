/***************************************************************************//**

Copyright (c) 2011, 2013, Oracle Corpn. All Rights Reserved.

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
@file include/ut0bh.h
Binary min-heap interface.

Created 2010-05-28 by Sunny Bains
*******************************************************/

#ifndef INNOBASE_UT0BH_H
#define INNOBASE_UT0BH_H

#include "univ.i"

/** Comparison function for objects in the binary heap. */
typedef int (*ib_bh_cmp_t)(const void* p1, const void* p2);

struct ib_bh_t;

/**********************************************************************//**
Get the number of elements in the binary heap.
@return number of elements */
UNIV_INLINE
ulint
ib_bh_size(
/*=======*/
	const ib_bh_t*	ib_bh);			/*!< in: instance */

/**********************************************************************//**
Test if binary heap is empty.
@return TRUE if empty. */
UNIV_INLINE
ibool
ib_bh_is_empty(
/*===========*/
	const ib_bh_t*	ib_bh);			/*!< in: instance */

/**********************************************************************//**
Test if binary heap is full.
@return TRUE if full. */
UNIV_INLINE
ibool
ib_bh_is_full(
/*===========*/
	const ib_bh_t*	ib_bh);			/*!< in: instance */

/**********************************************************************//**
Get a pointer to the element.
@return pointer to element */
UNIV_INLINE
void*
ib_bh_get(
/*=======*/
	ib_bh_t*	ib_bh,			/*!< in: instance */
	ulint		i);			/*!< in: index */

/**********************************************************************//**
Copy an element to the binary heap.
@return pointer to copied element */
UNIV_INLINE
void*
ib_bh_set(
/*======*/
	ib_bh_t*	ib_bh,			/*!< in/out: instance */
	ulint		i,			/*!< in: index */
	const void*	elem);			/*!< in: element to add */

/**********************************************************************//**
Return the first element from the binary heap.
@return pointer to first element or NULL if empty. */
UNIV_INLINE
void*
ib_bh_first(
/*========*/
	ib_bh_t*	ib_bh);			/*!< in: instance */

/**********************************************************************//**
Return the last element from the binary heap.
@return pointer to last element or NULL if empty. */
UNIV_INLINE
void*
ib_bh_last(
/*========*/
	ib_bh_t*	ib_bh);			/*!< in/out: instance */

/**********************************************************************//**
Create a binary heap.
@return a new binary heap */
UNIV_INTERN
ib_bh_t*
ib_bh_create(
/*=========*/
	ib_bh_cmp_t	compare,		/*!< in: comparator */
	ulint		sizeof_elem,		/*!< in: size of one element */
	ulint		max_elems);		/*!< in: max elements allowed */

/**********************************************************************//**
Free a binary heap.
@return a new binary heap */
UNIV_INTERN
void
ib_bh_free(
/*=======*/
	ib_bh_t*	ib_bh);			/*!< in,own: instance */

/**********************************************************************//**
Add an element to the binary heap. Note: The element is copied.
@return pointer to added element or NULL if full. */
UNIV_INTERN
void*
ib_bh_push(
/*=======*/
	ib_bh_t*	ib_bh,			/*!< in/out: instance */
	const void*	elem);			/*!< in: element to add */

/**********************************************************************//**
Remove the first element from the binary heap. */
UNIV_INTERN
void
ib_bh_pop(
/*======*/
	ib_bh_t*	ib_bh);			/*!< in/out: instance */

/** Binary heap data structure */
struct ib_bh_t {
	ulint		max_elems;		/*!< max elements allowed */
	ulint		n_elems;		/*!< current size */
	ulint		sizeof_elem;		/*!< sizeof element */
	ib_bh_cmp_t	compare;		/*!< comparator */
};

#ifndef UNIV_NONINL
#include "ut0bh.ic"
#endif

#endif /* INNOBASE_UT0BH_H */
