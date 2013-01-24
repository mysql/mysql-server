/*****************************************************************************

Copyright (c) 1996, 2013, Oracle and/or its affiliates. All Rights Reserved.

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

/**************************************************//**
@file dyn/dyn0dyn.cc
The dynamically allocated array

Created 2/5/1996 Heikki Tuuri
*******************************************************/

#include "dyn0dyn.h"
#ifdef UNIV_NONINL
#include "dyn0dyn.ic"
#endif

/************************************************************//**
Adds a new block to a dyn array.
@return	created block */
UNIV_INTERN
dyn_block_t*
dyn_array_add_block(
/*================*/
	dyn_array_t*	arr)	/*!< in/out: dyn array */
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

	block = static_cast<dyn_block_t*>(
		mem_heap_alloc(heap, sizeof(dyn_block_t)));

	block->used = 0;

	UT_LIST_ADD_LAST(list, arr->base, block);

	return(block);
}
