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

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>
#include "my_sys.h"

#include <algorithm>
#include <vector>

namespace myqsort_vs_stdsort_unittest {

static int cmp_double(void *cmp_arg, double *a,double *b)
{
  return *a < *b ? -1 : *a == *b ? 0 : 1;
}

#if defined(GTEST_HAS_PARAM_TEST)

#if !defined(DBUG_OFF)
// There is no point in benchmarking anything in debug mode.
const size_t num_iterations= 1ULL;
#else
// Set this so that each test case takes a few seconds.
// And set it back to a small value before pushing!!
// const size_t num_iterations= 2000000ULL;
const size_t num_iterations= 2ULL;
#endif

class DoubleSortCompareTest : public ::testing::TestWithParam<int>
{
public:
  static void SetUpTestCase()
  {
    doubles_to_sort.reserve(1000);
    for (int ix= 0; ix < 1000; ++ix)
    {
      doubles_to_sort.push_back(ix);
    }
    // Remove comment to get results for randomized data.
    // std::random_shuffle(doubles_to_sort.begin(), doubles_to_sort.end());
  }

  virtual void SetUp()
  {
    num_elements= GetParam();
  }

  int num_elements;
  static std::vector<double> doubles_to_sort;
};
std::vector<double> DoubleSortCompareTest::doubles_to_sort;


int test_values[]= {10, 100, 1000};

INSTANTIATE_TEST_CASE_P(Sort, DoubleSortCompareTest,
                        ::testing::ValuesIn(test_values));

TEST_P(DoubleSortCompareTest, StdSort)
{
  for (size_t ix= 0; ix < num_iterations; ++ix)
  {
    std::vector<double> data(doubles_to_sort.begin(),
                             doubles_to_sort.begin() + num_elements);
    std::sort(data.begin(), data.end());
  }
}

TEST_P(DoubleSortCompareTest, MyQSort)
{
  for (size_t ix= 0; ix < num_iterations; ++ix)
  {
    std::vector<double> data(doubles_to_sort.begin(),
                             doubles_to_sort.begin() + num_elements);
    my_qsort2(&data[0], num_elements, sizeof(double),
              reinterpret_cast<qsort2_cmp>(cmp_double), NULL);
  }
}

#endif  // GTEST_HAS_PARAM_TEST

}
