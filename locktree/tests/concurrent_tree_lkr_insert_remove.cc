/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "concurrent_tree_unit_test.h"

namespace toku {

// "random" (derived from the digits of PI) but deterministic keys
const uint64_t keys[] = {
    141, 592, 653, 589, 793, 238, 462, 643, 383, 327, 950, 288, 419,
    716, 939, 937, 510, 582, 97, 494, 459, 230, 781, 640, 628, 620, 899,
    862, 803, 482, 534, 211, 706, 798, 214, 808, 651, 328, 239, 664, 709,
    384, 460, 955, 58, 223, 172, 535, 940, 812, 848,
};
const uint64_t num_keys = sizeof(keys) / sizeof(keys[0]);

static const DBT *get_ith_key_from_set(uint64_t i) {
    return get_dbt(keys[i]);
}

static void verify_unique_keys(void) {
    for (uint64_t i = 0; i < num_keys; i++) {
        for (uint64_t j = 0; j < num_keys; j++) {
            if (i != j) {
                invariant(keys[i] != keys[j]);
            }
        }
    }
}

static uint64_t check_for_range_and_count(concurrent_tree::locked_keyrange *lkr,
        comparator *cmp, const keyrange &range, bool range_should_exist) {

    struct check_fn_obj {
        comparator *cmp;
        uint64_t count;
        keyrange target_range;
        bool target_range_found;

        bool fn(const keyrange &query_range, TXNID txnid) { 
            (void) txnid;
            if (query_range.compare(cmp, target_range) == keyrange::comparison::EQUALS) {
                invariant(!target_range_found);
                target_range_found = true;
            }
            count++;
            return true;
        }
    } check_fn;
    check_fn.cmp = cmp;
    check_fn.count = 0;
    check_fn.target_range = range;
    check_fn.target_range_found = false;

    lkr->iterate<check_fn_obj>(&check_fn);

    if (range_should_exist) {
        invariant(check_fn.target_range_found);
    } else {
        invariant(!check_fn.target_range_found);
    }
    return check_fn.count;
}

// test that insert/remove work properly together, confirming
// whether keys exist using iterate()
void concurrent_tree_unit_test::test_lkr_insert_remove(void) {
    verify_unique_keys();
    comparator cmp;
    cmp.create(compare_dbts, nullptr);

    concurrent_tree tree;
    tree.create(&cmp);

    // prepare and acquire the infinte range
    concurrent_tree::locked_keyrange lkr;
    lkr.prepare(&tree);
    lkr.acquire(keyrange::get_infinite_range());

    // populate the tree with all the keys
    uint64_t n;
    const uint64_t cap = 15;
    for (uint64_t i = 0; i < num_keys; i++) {
        keyrange range;
        range.create(get_ith_key_from_set(i), get_ith_key_from_set(i));
        // insert an element. it should exist and the
        // count should be correct.
        lkr.insert(range, i);
        n = check_for_range_and_count(&lkr, &cmp, range, true);
        if (i >= cap) {
            invariant(n == cap + 1);
            // remove an element previously inserted. it should
            // no longer exist and the count should be correct.
            range.create(get_ith_key_from_set(i - cap), get_ith_key_from_set(i - cap));
            lkr.remove(range);
            n = check_for_range_and_count(&lkr, &cmp, range, false);
            invariant(n == cap);
        } else {
            invariant(n == i + 1);
        }
    }

    // clean up the rest of the keys
    for (uint64_t i = 0; i < cap; i++) {
        keyrange range;
        range.create(get_ith_key_from_set(num_keys - i - 1), get_ith_key_from_set(num_keys - i - 1));
        lkr.remove(range);
        n = check_for_range_and_count(&lkr, &cmp, range, false);
        invariant(n == (cap - i - 1));
    }

    lkr.release();
    tree.destroy();
}

} /* namespace toku */

int main(void) {
    toku::concurrent_tree_unit_test test;
    test.test_lkr_insert_remove();
    return 0;
}
