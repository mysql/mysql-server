#ifndef IB_VECTOR_H
#define IB_VECTOR_H

#include "univ.i"
#include "mem0mem.h"

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

/********************************************************************
Create a new vector with the given initial size. */

ib_vector_t*
ib_vector_create(
/*=============*/
				/* out: vector */
	mem_heap_t*	heap,	/* in: heap */
	ulint		size);	/* in: initial size */

/********************************************************************
Push a new element to the vector, increasing its size if necessary. */

void
ib_vector_push(
/*===========*/
	ib_vector_t*	vec,	/* in: vector */
	void*		elem);	/* in: data element */

/********************************************************************
Get the number of elements in the vector. */
UNIV_INLINE
ulint
ib_vector_size(
/*===========*/
				/* out: number of elements in vector */
	ib_vector_t*	vec);	/* in: vector */

/********************************************************************
Get the n'th element. */
UNIV_INLINE
void*
ib_vector_get(
/*==========*/
				/* out: n'th element */
	ib_vector_t*	vec,	/* in: vector */
	ulint		n);	/* in: element index to get */

/* See comment at beginning of file. */
struct ib_vector_struct {
	mem_heap_t*	heap;	/* heap */
	void**		data;	/* data elements */
	ulint		used;	/* number of elements currently used */
	ulint		total;	/* number of elements allocated */
};

#ifndef UNIV_NONINL
#include "ut0vec.ic"
#endif

#endif
