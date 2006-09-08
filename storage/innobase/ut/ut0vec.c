#include "ut0vec.h"
#ifdef UNIV_NONINL
#include "ut0vec.ic"
#endif
#include <string.h>

/********************************************************************
Create a new vector with the given initial size. */

ib_vector_t*
ib_vector_create(
/*=============*/
				/* out: vector */
	mem_heap_t*	heap,	/* in: heap */
	ulint		size)	/* in: initial size */
{
	ib_vector_t*	vec;

	ut_a(size > 0);

	vec = mem_heap_alloc(heap, sizeof(*vec));

	vec->heap = heap;
	vec->data = mem_heap_alloc(heap, sizeof(void*) * size);
	vec->used = 0;
	vec->total = size;

	return(vec);
}

/********************************************************************
Push a new element to the vector, increasing its size if necessary. */

void
ib_vector_push(
/*===========*/
	ib_vector_t*	vec,	/* in: vector */
	void*		elem)	/* in: data element */
{
	if (vec->used >= vec->total) {
		void**	new_data;
		ulint	new_total = vec->total * 2;

		new_data = mem_heap_alloc(vec->heap,
					  sizeof(void*) * new_total);
		memcpy(new_data, vec->data, sizeof(void*) * vec->total);

		vec->data = new_data;
		vec->total = new_total;
	}

	vec->data[vec->used] = elem;
	vec->used++;
}
