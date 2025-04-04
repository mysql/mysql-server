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

#include "storage/innobase/include/detail/fts/fts.h"
#include "storage/innobase/include/fts0fts.h"
#include "storage/innobase/include/fts0types.h"
#include "storage/innobase/include/os0event.h"
#include "storage/innobase/include/sync0debug.h"
#include "storage/innobase/include/univ.i"
#include "storage/innobase/include/ut0rbt.h"

namespace innodb_fts0fts_unittest {

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

TEST(fts0fts, fts_doc_id_field_cmp) {
  // doc_ids are in ascending order
  auto doc_ids_size = sizeof(doc_ids) / sizeof(doc_ids[0]);
  for (size_t i = 1; i < doc_ids_size; ++i) {
    dummy obj1{doc_ids[i - 1]};
    dummy obj2{doc_ids[i]};
    EXPECT_LT(fts_doc_id_field_cmp<dummy>(&obj1, &obj2), 0);
    obj1.doc_id = doc_ids[i];
    obj2.doc_id = doc_ids[i - 1];
    EXPECT_GT(fts_doc_id_field_cmp<dummy>(&obj1, &obj2), 0);
    obj2.doc_id = doc_ids[i];
    EXPECT_EQ(fts_doc_id_field_cmp<dummy>(&obj1, &obj2), 0);
  }

  /*Test in the context of RBT, where fts_doc_id_field_cmp is intended
    to be used */

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

// RAII wrapper for initialization needed to create and use RW locks
struct os_support_t {
  os_support_t() {
    os_event_global_init();
    sync_check_init(1);
  }
  ~os_support_t() {
    sync_check_close();
    os_event_global_destroy();
  }
};

TEST(fts0fts, fts_reset_get_doc) {
  os_support_t dummy;
  dict_table_t *table =
      static_cast<dict_table_t *>(calloc(1, sizeof(dict_table_t)));

  struct fts_cache_wrapper_t {
    fts_cache_t *const cache{};
    fts_cache_wrapper_t(dict_table_t *table) : cache{fts_cache_create(table)} {}
    ~fts_cache_wrapper_t() { detail::fts_cache_destroy(cache); }
  } cache_wrapper(table);
  fts_cache_t *const cache = cache_wrapper.cache;

  rw_lock_x_lock(&cache->init_lock, UT_LOCATION_HERE);
  cache->get_docs = detail::fts_get_docs_create(cache);
  // Provide some content in get_docs for fts_reset_get_doc to overwrite
  fts_get_doc_t *get_doc =
      static_cast<fts_get_doc_t *>(ib_vector_push(cache->get_docs, nullptr));
  memset(get_doc, 0x0, sizeof(*get_doc));

  {
    fts_index_cache_t ic1{};
    fts_index_cache_t ic2{};
    ib_vector_push(cache->indexes, &ic1);
    ib_vector_push(cache->indexes, &ic2);
  }

  detail::fts_reset_get_doc(cache);

  rw_lock_x_unlock(&cache->init_lock);

  auto index_cache1 =
      static_cast<fts_index_cache_t *>(ib_vector_get(cache->indexes, 0));
  auto index_cache2 =
      static_cast<fts_index_cache_t *>(ib_vector_get(cache->indexes, 1));

  EXPECT_EQ(ib_vector_size(cache->get_docs), 2);

  get_doc = static_cast<fts_get_doc_t *>(ib_vector_get(cache->get_docs, 0));
  EXPECT_EQ(get_doc->index_cache, index_cache1);
  EXPECT_EQ(get_doc->get_document_graph, nullptr);
  EXPECT_EQ(get_doc->cache, cache);

  get_doc = static_cast<fts_get_doc_t *>(ib_vector_get(cache->get_docs, 1));
  EXPECT_EQ(get_doc->index_cache, index_cache2);
  EXPECT_EQ(get_doc->get_document_graph, nullptr);
  EXPECT_EQ(get_doc->cache, cache);

  free(table);
}
}  // namespace innodb_fts0fts_unittest
