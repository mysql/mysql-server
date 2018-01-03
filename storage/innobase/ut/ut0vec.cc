/*****************************************************************************

Copyright (c) 2006, 2016, Oracle and/or its affiliates. All Rights Reserved.

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

/*******************************************************************//**
@file ut/ut0vec.cc
A vector of pointers to data items

Created 4/6/2006 Osku Salerma
************************************************************************/

#include "ut0vec.h"
#include "mem0mem.h"

/********************************************************************
Create a new vector with the given initial size. */
ib_vector_t*
ib_vector_create(
/*=============*/
					/* out: vector */
	ib_alloc_t*	allocator,	/* in: vector allocator */
	ulint		sizeof_value,	/* in: size of data item */
	ulint		size)		/* in: initial size */
{
	ib_vector_t*	vec;

	ut_a(size > 0);

	vec = static_cast<ib_vector_t*>(
		allocator->mem_malloc(allocator, sizeof(*vec)));

	vec->used = 0;
	vec->total = size;
	vec->allocator = allocator;
	vec->sizeof_value = sizeof_value;

	vec->data = static_cast<void*>(
		allocator->mem_malloc(allocator, vec->sizeof_value * size));

	return(vec);
}

/********************************************************************
Resize the vector, currently the vector can only grow and we
expand the number of elements it can hold by 2 times. */
void
ib_vector_resize(
/*=============*/
	ib_vector_t*	vec)		/* in: vector */
{
	ulint		new_total = vec->total * 2;
	ulint		old_size = vec->used * vec->sizeof_value;
	ulint		new_size = new_total * vec->sizeof_value;

	vec->data = static_cast<void*>(vec->allocator->mem_resize(
		vec->allocator, vec->data, old_size, new_size));

	vec->total = new_total;
}
