/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."

#include "test.h"
#include "cachetable-internal.h"
#include "cachetable-test.h"

//
// Wrapper for the checkpointer and necessary
// data to run the tests.
//
struct checkpointer_test {
  checkpointer m_cp;
  pair_list m_pl;

  // Tests
  void test_begin_checkpoint();
  void test_pending_bits();
  void test_end_checkpoint();

  // Test Helper
  void add_pairs(struct cachefile *cf,
    ctpair pairs[],
    uint32_t count,
    uint32_t k);
};

//------------------------------------------------------------------------------
// test_begin_checkpoint() -
//
// Description:
//
void checkpointer_test::test_begin_checkpoint() {
    cachefile_list cfl;
    ZERO_STRUCT(cfl);
    cfl.init();

    cachetable ctbl;
    ZERO_STRUCT(ctbl);
    ctbl.list.init();

    ZERO_STRUCT(m_cp);
    m_cp.init(&ctbl.list, NULL, &ctbl.ev, &cfl);

    // 1. Call checkpoint with NO cachefiles.
    m_cp.begin_checkpoint();

    // 2. Call checkpoint with ONE cachefile.
    //cachefile cf;
    struct cachefile cf;
    cf.next = NULL;
    cf.for_checkpoint = false;
    m_cp.m_cf_list->m_active_head = &cf;
    create_dummy_functions(&cf);

    m_cp.begin_checkpoint();
    assert(m_cp.m_checkpoint_num_files == 1);
    assert(cf.for_checkpoint == true);

    // 3. Call checkpoint with MANY cachefiles.
    const uint32_t count = 3;
    struct cachefile cfs[count];
    m_cp.m_cf_list->m_active_head = &cfs[0];
    for (uint32_t i = 0; i < count; ++i) {
        cfs[i].for_checkpoint = false;
        create_dummy_functions(&cfs[i]);
        if (i == count - 1) {
            cfs[i].next = NULL;
        } else {
            cfs[i].next = &cfs[i + 1];
        }
    }

    m_cp.begin_checkpoint();
    assert(m_cp.m_checkpoint_num_files == count);
    for (uint32_t i = 0; i < count; ++i) {
        assert(cfs[i].for_checkpoint == true);
    }
    ctbl.list.destroy();
    m_cp.destroy();
}

//------------------------------------------------------------------------------
// test_pending_bits() -
//
// Description:
//
void checkpointer_test::test_pending_bits() {
    cachefile_list cfl;
    ZERO_STRUCT(cfl);
    cfl.init();

    cachetable ctbl;
    ZERO_STRUCT(ctbl);
    ctbl.list.init();

    ZERO_STRUCT(m_cp);
    m_cp.init(&ctbl.list, NULL, &ctbl.ev, &cfl);

    //
    // 1. Empty hash chain.
    //
    m_cp.turn_on_pending_bits();

    //
    // 2. One entry in pair chain
    //
    struct cachefile cf;
    cf.cachetable = &ctbl;
    memset(&cf, 0, sizeof(cf));
    cf.next = NULL;
    cf.for_checkpoint = true;
    m_cp.m_cf_list->m_active_head = &cf;
    create_dummy_functions(&cf);

    CACHEKEY k;
    k.b = 0;
    uint32_t hash = toku_cachetable_hash(&cf, k);

    ctpair p;
    CACHETABLE_WRITE_CALLBACK cb;

    pair_attr_s attr;
    attr.size = 0;
    attr.nonleaf_size = 0;
    attr.leaf_size = 0;
    attr.rollback_size = 0;
    attr.cache_pressure_size = 0;
    attr.is_valid = true;

    ZERO_STRUCT(p);
    pair_init(&p,
        &cf,
        k,
        NULL,
        attr,
        CACHETABLE_CLEAN,
        hash,
        cb,
        NULL,
        &ctbl.list);

    m_cp.m_list->put(&p);

    m_cp.turn_on_pending_bits();
    assert(p.checkpoint_pending);
    m_cp.m_list->evict_completely(&p);

    //
    // 3. Many hash chain entries.
    //
    const uint32_t count = 3;
    ctpair pairs[count];
    ZERO_ARRAY(pairs);
    add_pairs(&cf, pairs, count, 0);

    m_cp.turn_on_pending_bits();

    for (uint32_t i = 0; i < count; ++i) {
        assert(pairs[i].checkpoint_pending);
    }
    for (uint32_t i = 0; i < count; ++i) {
        CACHEKEY key;
        key.b = i;
        uint32_t full_hash = toku_cachetable_hash(&cf, key);
        PAIR pp = m_cp.m_list->find_pair(&cf, key, full_hash);
        assert(pp);
        m_cp.m_list->evict_completely(pp);
    }

    ctbl.list.destroy();
    m_cp.destroy();
}

//------------------------------------------------------------------------------
// add_pairs() -
//
// Description: Adds data (pairs) to the list referenced in the checkpoitner.
//
void checkpointer_test::add_pairs(struct cachefile *cf,
    ctpair pairs[],
    uint32_t count,
    uint32_t k)
{
    pair_attr_s attr;
    attr.size = 0;
    attr.nonleaf_size = 0;
    attr.leaf_size = 0;
    attr.rollback_size = 0;
    attr.cache_pressure_size = 0;
    attr.is_valid = true;
    CACHETABLE_WRITE_CALLBACK cb;
    ZERO_STRUCT(cb);  // All nullptr

    for (uint32_t i = k; i < count + k; ++i) {
        CACHEKEY key;
        key.b = i;
        uint32_t full_hash = toku_cachetable_hash(cf, key);
        pair_init(&(pairs[i]),
            cf,
            key,
            nullptr,
            attr,
            CACHETABLE_CLEAN,
            full_hash,
            cb,
            nullptr,
            m_cp.m_list);

        m_cp.m_list->put(&pairs[i]);
    }
}

//------------------------------------------------------------------------------
// get_number_pending_pairs() -
//
// Description: Helper function that iterates over pending list, and returns
//   the number of pairs discovered.
//
static uint32_t get_number_pending_pairs(pair_list *list)
{
    PAIR p;
    uint32_t count = 0;
    PAIR head = list->m_pending_head;
    while((p = list->m_pending_head) != 0)
    {
        list->m_pending_head = list->m_pending_head->pending_next;
        count++;
    }

    list->m_pending_head = head;
    return count;
}

//------------------------------------------------------------------------------
// test_end_checkpoint() -
//
// Description:  Adds pairs to the list, before and after a checkpoint.
//
void checkpointer_test::test_end_checkpoint() {
    // 1. Init test.
    cachetable ctbl;
    ZERO_STRUCT(ctbl);
    ctbl.list.init();

    cachefile_list cfl;
    ZERO_STRUCT(cfl);
    cfl.init();

    struct cachefile cf;
    memset(&cf, 0, sizeof(cf));
    cf.next = NULL;
    cf.for_checkpoint = true;
    create_dummy_functions(&cf);

    ZERO_STRUCT(m_cp);
    m_cp.init(&ctbl.list, NULL, &ctbl.ev, &cfl);
    m_cp.m_cf_list->m_active_head = &cf;

    // 2. Add data before running checkpoint.
    const uint32_t count = 6;
    ctpair pairs[count];
    ZERO_ARRAY(pairs);
    add_pairs(&cf, pairs, count / 2, 0);
    assert(m_cp.m_list->m_n_in_table == count / 2);

    // 3. Call begin checkpoint.
    m_cp.begin_checkpoint();
    assert(m_cp.m_checkpoint_num_files == 1);
    for (uint32_t i = 0; i < count / 2; ++i)
    {
        assert(pairs[i].checkpoint_pending);
    }

    // 4. Add new data between starting and stopping checkpoint.
    add_pairs(&cf, pairs, count / 2, count / 2);
    assert(m_cp.m_list->m_n_in_table == count);
    for (uint32_t i = count / 2; i < count / 2; ++i)
    {
        assert(!pairs[i].checkpoint_pending);
    }

    uint32_t pending_pairs = 0;
    pending_pairs = get_number_pending_pairs(m_cp.m_list);
    assert(pending_pairs == count / 2);

    // 5. Call end checkpoint
    m_cp.end_checkpoint(NULL, NULL);

    pending_pairs = get_number_pending_pairs(m_cp.m_list);
    assert(pending_pairs == 0);

    // Verify that none of the pairs are pending a checkpoint.
    for (uint32_t i = 0; i < count; ++i)
    {
        assert(!pairs[i].checkpoint_pending);
    }

    // 6. Cleanup
    for (uint32_t i = 0; i < count; ++i) {
        CACHEKEY key;
        key.b = i;
        uint32_t full_hash = toku_cachetable_hash(&cf, key);
        PAIR pp = m_cp.m_list->find_pair(&cf, key, full_hash);
        assert(pp);
        m_cp.m_list->evict_completely(pp);
    }
    m_cp.destroy();
    ctbl.list.destroy();
}


//------------------------------------------------------------------------------
// test_main() -
//
// Description:
//
int
test_main(int argc, const char *argv[]) {
    int r = 0;
    default_parse_args(argc, argv);
    checkpointer_test cp_test;

    // Run the tests.
    cp_test.test_begin_checkpoint();
    cp_test.test_pending_bits();
    cp_test.test_end_checkpoint();

    return r;
}
