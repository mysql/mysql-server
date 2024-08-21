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

#include "storage/innobase/include/fts0fts.h"
#include "storage/innobase/include/fts0types.h"
#include "storage/innobase/include/univ.i"
#include "storage/innobase/include/ut0rbt.h"

namespace innodb_ut0rbt_unittest {

/* Doc id array for testing with values exceeding 32-bit integer limit */
const doc_id_t doc_ids[] = {
    17574,      89783,      94755,      97537,      101358,     101361,
    102587,     103571,     104018,     106821,     108647,     109352,
    109379,     110325,     122868,     210682130,  231275441,  234172769,
    366236849,  526467159,  1675241735, 1675243405, 1947751899, 1949940363,
    2033691953, 2148227299, 2256289791, 2294223591, 2367501260, 2792700091,
    2792701220, 2817121627, 2820680352, 2821165664, 3253312130, 3404918378,
    3532599429, 3538712078, 3539373037, 3546479309, 3566641838, 3580209634,
    3580871267, 3693930556, 3693932734, 3693932983, 3781949558, 3839877411,
    3930968983, 4146309172, 4524715523, 4524715525, 4534911119, 4597818456};

const doc_id_t search_doc_id = 1675241735;

namespace {
struct dummy {
  doc_id_t doc_id;
};
}  // namespace

TEST(ut0rbt, fts_doc_id_cmp) {
  ib_rbt_t *doc_id_rbt = rbt_create(sizeof(dummy), fts_doc_id_field_cmp<dummy>);

  /* Insert doc ids into rbtree. */
  for (auto doc_id : doc_ids) {
    ib_rbt_bound_t parent;
    dummy obj;
    obj.doc_id = doc_id;

    if (rbt_search(doc_id_rbt, &parent, &obj.doc_id) != 0) {
      rbt_add_node(doc_id_rbt, &parent, &obj);
    }
  }

  /* Check if doc id exists in rbtree */
  ib_rbt_bound_t parent;
  EXPECT_EQ(rbt_search(doc_id_rbt, &parent, &search_doc_id), 0);

  rbt_free(doc_id_rbt);
}
}  // namespace innodb_ut0rbt_unittest
