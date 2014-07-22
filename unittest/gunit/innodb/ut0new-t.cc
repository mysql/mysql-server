/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"

#include <gtest/gtest.h>

#include "univ.i"

#include "ut0new.h"

namespace innodb_ut0new_unittest {

class C {
public:
	C(int x = 42)
	:
	m_x(x)
	{
	}

	int	m_x;
};

static
void
start()
{
	static bool	ut_new_boot_called = false;

	if (!ut_new_boot_called) {
		ut_new_boot();
		ut_new_boot_called = true;
	}
}

/* test UT_NEW*() */
TEST(ut0new, utnew)
{
	start();

	C*	p;

	p = UT_NEW_NOKEY(C(12));
	EXPECT_EQ(p->m_x, 12);
	UT_DELETE(p);

	p = UT_NEW(C(34), mem_key_buf_buf_pool);
	EXPECT_EQ(p->m_x, 34);
	UT_DELETE(p);

	p = UT_NEW_ARRAY_NOKEY(C, 5);
	EXPECT_EQ(p[0].m_x, 42);
	EXPECT_EQ(p[1].m_x, 42);
	EXPECT_EQ(p[2].m_x, 42);
	EXPECT_EQ(p[3].m_x, 42);
	EXPECT_EQ(p[4].m_x, 42);
	UT_DELETE_ARRAY(p);

	p = UT_NEW_ARRAY(C, 5, mem_key_buf_buf_pool);
	EXPECT_EQ(p[0].m_x, 42);
	EXPECT_EQ(p[1].m_x, 42);
	EXPECT_EQ(p[2].m_x, 42);
	EXPECT_EQ(p[3].m_x, 42);
	EXPECT_EQ(p[4].m_x, 42);
	UT_DELETE_ARRAY(p);
}

/* test ut_*alloc*() */
TEST(ut0new, utmalloc)
{
	start();

	int*	p;

	p = static_cast<int*>(ut_malloc_nokey(sizeof(int)));
	*p = 12;
	ut_free(p);

	p = static_cast<int*>(ut_malloc(sizeof(int), mem_key_buf_buf_pool));
	*p = 34;
	ut_free(p);

	p = static_cast<int*>(ut_zalloc_nokey(sizeof(int)));
	EXPECT_EQ(*p, 0);
	*p = 56;
	ut_free(p);

	p = static_cast<int*>(ut_zalloc(sizeof(int), mem_key_buf_buf_pool));
	EXPECT_EQ(*p, 0);
	*p = 78;
	ut_free(p);

	p = static_cast<int*>(ut_malloc_nokey(sizeof(int)));
	*p = 90;
	p = static_cast<int*>(ut_realloc(p, 2 * sizeof(int)));
	EXPECT_EQ(p[0], 90);
	p[1] = 91;
	ut_free(p);
}

/* test ut_allocator() */
TEST(ut0new, utallocator)
{
	start();

	typedef int					basic_t;
	typedef ut_allocator<basic_t>			vec_allocator_t;
	typedef std::vector<basic_t, vec_allocator_t>	vec_t;

	vec_t	v1;
	v1.push_back(21);
	v1.push_back(31);
	v1.push_back(41);
	EXPECT_EQ(v1[0], 21);
	EXPECT_EQ(v1[1], 31);
	EXPECT_EQ(v1[2], 41);

	/* We use "new" instead of "UT_NEW()" for simplicity here. Real InnoDB
	code should use UT_NEW(). */

	/* This could of course be written as:
	std::vector<int, ut_allocator<int> >*	v2
	= new std::vector<int, ut_allocator<int> >(ut_allocator<int>(
	mem_key_buf_buf_pool)); */
	vec_t*	v2 = new vec_t(vec_allocator_t(mem_key_buf_buf_pool));
	v2->push_back(27);
	v2->push_back(37);
	v2->push_back(47);
	EXPECT_EQ(v2->at(0), 27);
	EXPECT_EQ(v2->at(1), 37);
	EXPECT_EQ(v2->at(2), 47);
	delete v2;
}

}
