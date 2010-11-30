/*****************************************************************************

Copyright (c) 2006, 2009, Innobase Oy. All Rights Reserved.

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

/*******************************************************************//**
@file include/ut0vec.h
A vector of pointers to data items

Created 4/6/2006 Osku Salerma
************************************************************************/

#ifndef IB_VECTOR_H
#define IB_VECTOR_H

#include "univ.i"
#include "mem0mem.h"

/** An automatically resizing vector data type. */
typedef struct ib_vector_struct ib_vector_t;

/* An automatically resizing vector datatype with the following properties:

 -Contains void* items.

 -The items are owned by the caller.

 -All memory allocation is done through a heap owned by the caller, who is
 responsible for freeing it when done with the vector.

 -When the vector is resized, the old memory area is left allocated since it
 uses the same heap as the new memory area, so this is best used for
 relatively small or short-lived uses.
*/

/****************************************************************//**
Create a new vector with the given initial size.
@return	vector */
UNIV_INTERN
ib_vector_t*
ib_vector_create(
/*=============*/
	mem_heap_t*	heap,	/*!< in: heap */
	ulint		size);	/*!< in: initial size */

/****************************************************************//**
Push a new element to the vector, increasing its size if necessary. */
UNIV_INTERN
void
ib_vector_push(
/*===========*/
	ib_vector_t*	vec,	/*!< in: vector */
	void*		elem);	/*!< in: data element */

/****************************************************************//**
Get the number of elements in the vector.
@return	number of elements in vector */
UNIV_INLINE
ulint
ib_vector_size(
/*===========*/
	const ib_vector_t*	vec);	/*!< in: vector */

/****************************************************************//**
Test whether a vector is empty or not.
@return	TRUE if empty */
UNIV_INLINE
ibool
ib_vector_is_empty(
/*===============*/
	const ib_vector_t*	vec);	/*!< in: vector */

/****************************************************************//**
Get the n'th element.
@return	n'th element */
UNIV_INLINE
void*
ib_vector_get(
/*==========*/
	ib_vector_t*	vec,	/*!< in: vector */
	ulint		n);	/*!< in: element index to get */

/****************************************************************//**
Get last element. The vector must not be empty.
@return	last element */
UNIV_INLINE
void*
ib_vector_get_last(
/*===============*/
	ib_vector_t*	vec);	/*!< in: vector */

/****************************************************************//**
Set the n'th element. */
UNIV_INLINE
void
ib_vector_set(
/*==========*/
	ib_vector_t*	vec,	/*!< in/out: vector */
	ulint		n,	/*!< in: element index to set */
	void*		elem);	/*!< in: data element */

/****************************************************************//**
Remove the last element from the vector. */
UNIV_INLINE
void*
ib_vector_pop(
/*==========*/
	ib_vector_t*	vec);	/*!< in: vector */

/****************************************************************//**
Free the underlying heap of the vector. Note that vec is invalid
after this call. */
UNIV_INLINE
void
ib_vector_free(
/*===========*/
	ib_vector_t*	vec);	/*!< in,own: vector */

/** An automatically resizing vector data type. */
struct ib_vector_struct {
	mem_heap_t*	heap;	/*!< heap */
	void**		data;	/*!< data elements */
	ulint		used;	/*!< number of elements currently used */
	ulint		total;	/*!< number of elements allocated */
};

#ifndef UNIV_NONINL
#include "ut0vec.ic"
#endif

#endif
