/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* See http://code.google.com/p/googletest/wiki/Primer */

#include <gtest/gtest.h>

#include "handler.h"

#include "univ.i"

#include "mem0mem.h"
#include "srv0conc.h"
#include "srv0srv.h"

namespace innodb_mem0mem_unittest {

static bool	innodb_inited = false;

class mem0mem : public ::testing::Test {
protected:
	virtual void
	SetUp()
	{
		if (!innodb_inited) {
			srv_max_n_threads = srv_sync_array_size + 1;
			os_sync_init();
			sync_init();
			mem_init(1024);

			innodb_inited = true;
		}
	}
};

/* test mem_heap_is_top() */
TEST_F(mem0mem, memheapistop)
{
	mem_heap_t*	heap;
	const char*	str = "aabbccddeeff";
	size_t		str_len = strlen(str);
	char*		str_in_heap;
	void*		dummy;

#define INITIAL_HEAP_SIZE	512

	heap = mem_heap_create(INITIAL_HEAP_SIZE);

	str_in_heap = mem_heap_strdup(heap, str);

	EXPECT_TRUE(mem_heap_is_top(heap, str_in_heap, str_len + 1));

	/* Check with a random pointer to make sure that mem_heap_is_top()
	does not return true unconditionally. */
	EXPECT_FALSE(mem_heap_is_top(heap, "foo", 4));

	/* Allocate another chunk and check that our string is not at the
	top anymore. */
	dummy = mem_heap_alloc(heap, 32);
	ut_a(dummy != NULL);
	EXPECT_FALSE(mem_heap_is_top(heap, str_in_heap, str_len + 1));

	/* Cause the heap to allocate a second block and retest. */
	dummy = mem_heap_alloc(heap, INITIAL_HEAP_SIZE + 1);
	str_in_heap = mem_heap_strdup(heap, str);
	EXPECT_TRUE(mem_heap_is_top(heap, str_in_heap, str_len + 1));

	/* Allocate another chunk, free it, and then confirm that our string
	is still the topmost element. */
	const ulint	x = 64;
	dummy = mem_heap_alloc(heap, x);
	EXPECT_FALSE(mem_heap_is_top(heap, str_in_heap, str_len + 1));
	mem_heap_free_top(heap, x);
	EXPECT_TRUE(mem_heap_is_top(heap, str_in_heap, str_len + 1));

	mem_heap_free(heap);
}

/* test mem_heap_replace() */
TEST_F(mem0mem, memheapreplace)
{
	mem_heap_t*	heap;
	void*		p1;
	const ulint	p1_size = 16;
	void*		p2;
	const ulint	p2_size = 32;
	void*		p3;
	const ulint	p3_size = 64;
	void*		p4;
	const ulint	p4_size = 128;

	heap = mem_heap_create(1024);

	p1 = mem_heap_alloc(heap, p1_size);
	p2 = mem_heap_alloc(heap, p2_size);
	p3 = mem_heap_replace(heap, p1, p1_size, p3_size);

	EXPECT_NE(p2, p3);
	EXPECT_TRUE(mem_heap_is_top(heap, p3, p3_size));

	p4 = mem_heap_replace(heap, p3, p3_size, p4_size);

	EXPECT_EQ(p3, p4);
	EXPECT_TRUE(mem_heap_is_top(heap, p4, p4_size));

	mem_heap_free(heap);
}

}
