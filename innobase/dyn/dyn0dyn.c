/******************************************************
The dynamically allocated array

(c) 1996 Innobase Oy

Created 2/5/1996 Heikki Tuuri
*******************************************************/

#include "dyn0dyn.h"
#ifdef UNIV_NONINL
#include "dyn0dyn.ic"
#endif

/****************************************************************
Adds a new block to a dyn array. */

dyn_block_t*
dyn_array_add_block(
/*================*/
				/* out: created block */
	dyn_array_t*	arr)	/* in: dyn array */
{
	mem_heap_t*	heap;
	dyn_block_t*	block;

	ut_ad(arr);
	ut_ad(arr->magic_n == DYN_BLOCK_MAGIC_N);

	if (arr->heap == NULL) {
		UT_LIST_INIT(arr->base);
		UT_LIST_ADD_FIRST(list, arr->base, arr);

		arr->heap = mem_heap_create(sizeof(dyn_block_t));
	}	

	block = dyn_array_get_last_block(arr);
	block->used = block->used | DYN_BLOCK_FULL_FLAG;

	heap = arr->heap;

	block = mem_heap_alloc(heap, sizeof(dyn_block_t));

	block->used = 0;

	UT_LIST_ADD_LAST(list, arr->base, block);

	return(block);
}
