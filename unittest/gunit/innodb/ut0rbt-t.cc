/* Copyright (c) 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>
#include "storage/innobase/include/ut0rbt.h"

namespace innodb_ut0rbt_unittest {

namespace {

/* RBT comparator function is only required to rturn value <0 if arg1 < arg2.
   For the test, we use a conventional comparator function */
template <typename T>
int compare(const void *p1, const void *p2) {
  const T *arg1 = static_cast<const T *>(p1);
  const T *arg2 = static_cast<const T *>(p2);

  if (*arg1 < *arg2) {
    return -1;
  } else if (*arg2 < *arg1) {
    return 1;
  } else {
    return 0;
  }
}

}  // namespace

TEST(ut0rbt, create_add_search) {
  ib_rbt_t *doc_id_rbt =
      rbt_create(sizeof(unsigned int), compare<unsigned int>);

  /* Add sorted values to rbtree. */
  for (unsigned int i = 0; i < 10; ++i) {
    ib_rbt_bound_t parent;
    unsigned int obj{i};

    ASSERT_NE(rbt_search(doc_id_rbt, &parent, &obj), 0);
    rbt_add_node(doc_id_rbt, &parent, &obj);
  }

  unsigned int search_key = 4;

  /* Check if an added value exists in rbtree */
  ib_rbt_bound_t parent;
  EXPECT_EQ(rbt_search(doc_id_rbt, &parent, &search_key), 0);

  rbt_free(doc_id_rbt);
}
}  // namespace innodb_ut0rbt_unittest
