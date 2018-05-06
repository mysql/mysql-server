/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* See http://code.google.com/p/googletest/wiki/Primer */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"

#include <gtest/gtest.h>
#include <stddef.h>

#include "storage/innobase/include/univ.i"
#include "storage/innobase/include/ut0new.h"

namespace innodb_ut0new_unittest {

class C {
 public:
  C(int x = 42) : m_x(x) {}

  int m_x;
};

static void start() { ut_new_boot_safe(); }

/* test UT_NEW*() */
TEST(ut0new, utnew) {
  start();

  C *p;

  p = UT_NEW_NOKEY(C(12));
  EXPECT_EQ(12, p->m_x);
  UT_DELETE(p);

  p = UT_NEW(C(34), mem_key_buf_buf_pool);
  EXPECT_EQ(34, p->m_x);
  UT_DELETE(p);

  p = UT_NEW_ARRAY_NOKEY(C, 5);
  EXPECT_EQ(42, p[0].m_x);
  EXPECT_EQ(42, p[1].m_x);
  EXPECT_EQ(42, p[2].m_x);
  EXPECT_EQ(42, p[3].m_x);
  EXPECT_EQ(42, p[4].m_x);
  UT_DELETE_ARRAY(p);

  p = UT_NEW_ARRAY(C, 5, mem_key_buf_buf_pool);
  EXPECT_EQ(42, p[0].m_x);
  EXPECT_EQ(42, p[1].m_x);
  EXPECT_EQ(42, p[2].m_x);
  EXPECT_EQ(42, p[3].m_x);
  EXPECT_EQ(42, p[4].m_x);
  UT_DELETE_ARRAY(p);
}

/* test ut_*alloc*() */
TEST(ut0new, utmalloc) {
  start();

  int *p;

  p = static_cast<int *>(ut_malloc_nokey(sizeof(int)));
  *p = 12;
  ut_free(p);

  p = static_cast<int *>(ut_malloc(sizeof(int), mem_key_buf_buf_pool));
  *p = 34;
  ut_free(p);

  p = static_cast<int *>(ut_zalloc_nokey(sizeof(int)));
  EXPECT_EQ(0, *p);
  *p = 56;
  ut_free(p);

  p = static_cast<int *>(ut_zalloc(sizeof(int), mem_key_buf_buf_pool));
  EXPECT_EQ(0, *p);
  *p = 78;
  ut_free(p);

  p = static_cast<int *>(ut_malloc_nokey(sizeof(int)));
  *p = 90;
  p = static_cast<int *>(ut_realloc(p, 2 * sizeof(int)));
  EXPECT_EQ(90, p[0]);
  p[1] = 91;
  ut_free(p);
}

/* test ut_allocator() */
TEST(ut0new, utallocator) {
  start();

  typedef int basic_t;
  typedef ut_allocator<basic_t> vec_allocator_t;
  typedef std::vector<basic_t, vec_allocator_t> vec_t;

  vec_t v1;
  v1.push_back(21);
  v1.push_back(31);
  v1.push_back(41);
  EXPECT_EQ(21, v1[0]);
  EXPECT_EQ(31, v1[1]);
  EXPECT_EQ(41, v1[2]);

  /* We use "new" instead of "UT_NEW()" for simplicity here. Real InnoDB
  code should use UT_NEW(). */

  /* This could of course be written as:
  std::vector<int, ut_allocator<int> >*	v2
  = new std::vector<int, ut_allocator<int> >(ut_allocator<int>(
  mem_key_buf_buf_pool)); */
  vec_t *v2 = new vec_t(vec_allocator_t(mem_key_buf_buf_pool));
  v2->push_back(27);
  v2->push_back(37);
  v2->push_back(47);
  EXPECT_EQ(27, v2->at(0));
  EXPECT_EQ(37, v2->at(1));
  EXPECT_EQ(47, v2->at(2));
  delete v2;
}

static int n_construct = 0;

class cc_t {
 public:
  cc_t() {
    n_construct++;
    if (n_construct % 4 == 0) {
      throw(1);
    }
  }
};

struct big_t {
  char x[128];
};

/* test edge cases */
TEST(ut0new, edgecases) {
  ut_allocator<byte> alloc1(mem_key_buf_buf_pool);
  ut_new_pfx_t pfx;
  void *ret;
  const void *null_ptr = NULL;

  ret = alloc1.allocate_large(0, &pfx);
  EXPECT_EQ(null_ptr, ret);

#ifdef UNIV_PFS_MEMORY
  ret = alloc1.allocate(16);
  ASSERT_TRUE(ret != NULL);
  ret = alloc1.reallocate(ret, 0, UT_NEW_THIS_FILE_PSI_KEY);
  EXPECT_EQ(null_ptr, ret);

  ret = UT_NEW_ARRAY_NOKEY(byte, 0);
  EXPECT_EQ(null_ptr, ret);
#endif /* UNIV_PFS_MEMORY */

  ut_allocator<big_t> alloc2(mem_key_buf_buf_pool);

  const ut_allocator<big_t>::size_type too_many_elements =
      std::numeric_limits<ut_allocator<big_t>::size_type>::max() /
          sizeof(big_t) +
      1;

#ifdef UNIV_PFS_MEMORY
  ret = alloc2.allocate(16);
  ASSERT_TRUE(ret != NULL);
  void *ret2 =
      alloc2.reallocate(ret, too_many_elements, UT_NEW_THIS_FILE_PSI_KEY);
  EXPECT_EQ(null_ptr, ret2);
  /* If reallocate fails due to too many elements,
  memory is still allocated. Do explicit deallocate do avoid mem leak. */
  alloc2.deallocate(static_cast<big_t *>(ret));
#endif /* UNIV_PFS_MEMORY */

  bool threw = false;

  try {
    ret = alloc2.allocate(too_many_elements);
  } catch (...) {
    threw = true;
  }
  EXPECT_TRUE(threw);

  ret = alloc2.allocate(too_many_elements, NULL, PSI_NOT_INSTRUMENTED, false,
                        false);
  EXPECT_EQ(null_ptr, ret);

  threw = false;
  try {
    cc_t *cc = UT_NEW_ARRAY_NOKEY(cc_t, 16);
    /* Not reached, but silence a compiler warning
    about unused 'cc': */
    ASSERT_TRUE(cc != NULL);
  } catch (...) {
    threw = true;
  }
  EXPECT_TRUE(threw);
}

}  // namespace innodb_ut0new_unittest
