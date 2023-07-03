<<<<<<< HEAD
/* Copyright (c) 2014, 2022, Oracle and/or its affiliates.
=======
/* Copyright (c) 2014, 2023, Oracle and/or its affiliates.
>>>>>>> pr/231

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

#include <gtest/gtest.h>
#include "my_config.h"

#include <stdlib.h>
#include <string.h>

#include "unittest/gunit/benchmark.h"
#include "unittest/gunit/gunit_test_main.h"

namespace calloc_unittest {

<<<<<<< HEAD
static void malloc_test(size_t num_iterations) {
  for (size_t i = 0; i < num_iterations; ++i) {
    void *rawmem1 = malloc(malloc_chunk_size);
    memset(rawmem1, 0, malloc_chunk_size);
    void *rawmem2 = malloc(malloc_chunk_size);
    memset(rawmem2, 0, malloc_chunk_size);
=======
/*
  Set num_iterations to a reasonable value (e.g. 200000), build release
  and run with 'calloc-t --disable-tap-output' to see the time taken.
*/
#if !defined(NDEBUG)
// There is no point in benchmarking anything in debug mode.
static int num_iterations = 2;
#else
// Set this so that each test case takes a few seconds.
// And set it back to a small value before pushing!!
static int num_iterations = 2000;
#endif

class CallocTest : public ::testing::Test {
 protected:
  CallocTest(){};

  void malloc_test(int count);
  void calloc_test(int count);
};

void CallocTest::malloc_test(int count) {
  for (int i = 1; i <= count; i++) {
    size_t size = i * 10;
    void *rawmem1 = malloc(size);
    memset(rawmem1, 0, size);
    void *rawmem2 = malloc(size);
    memset(rawmem2, 0, size);
>>>>>>> pr/231
    // We need to prevent the optimizer from removing the whole loop.
    EXPECT_FALSE(compare_malloc_chunks(rawmem1, rawmem2, malloc_chunk_size));
    free(rawmem1);
    free(rawmem2);
  }
  SetBytesProcessed(num_iterations * malloc_chunk_size);
}

static void calloc_test(size_t num_iterations) {
  for (size_t i = 0; i < num_iterations; ++i) {
    void *rawmem1 = calloc(malloc_chunk_size, 1);
    void *rawmem2 = calloc(malloc_chunk_size, 1);
    // We need to prevent the optimizer from removing the whole loop.
    EXPECT_FALSE(compare_malloc_chunks(rawmem1, rawmem2, malloc_chunk_size));
    free(rawmem1);
    free(rawmem2);
  }
  SetBytesProcessed(num_iterations * malloc_chunk_size);
}

static void malloc_test_warmup(size_t num_iterations) {
  malloc_test(num_iterations);
}

BENCHMARK(malloc_test_warmup)
BENCHMARK(malloc_test)
BENCHMARK(calloc_test)

}  // namespace calloc_unittest
