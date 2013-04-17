/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#ifndef TOKU_LOCKTREE_UNIT_TEST_H
#define TOKU_LOCKTREE_UNIT_TEST_H

#include "test.h"

#include "locktree.h"
#include "concurrent_tree.h"

namespace toku {

class locktree_unit_test {
public:
    // test simple create and destroy of the locktree
    void test_create_destroy(void);

    // test that get/set userdata works, and that get_manager() works
    void test_misc(void);

    // test that simple read and write locks can be acquired
    void test_simple_lock(void);

    // test that:
    // - existing read locks can be upgrading to write locks
    // - overlapping locks are consolidated in the tree
    // - dominated locks succeed and are not stored in the tree
    void test_overlapping_relock(void);

    // test write lock conflicts when read or write locks exist
    // test read lock conflicts when write locks exist
    void test_conflicts(void);
    
    // test that ranges with infinite endpoints work
    void test_infinity(void);

    // make sure the single txnid optimization does not screw
    // up when there is more than one txnid with locks in the tree
    void test_single_txnid_optimization(void);

private:


    template <typename F>
    static void locktree_iterate(const locktree *lt, F *function) {
        concurrent_tree::locked_keyrange ltr;
        keyrange infinite_range = keyrange::get_infinite_range();

        ltr.prepare(lt->m_rangetree);
        ltr.acquire(infinite_range);
        ltr.iterate<F>(function);
        ltr.release();
    }

    static size_t num_row_locks(const locktree *lt) {
        struct count_fn_obj {
            size_t count;
            bool fn(const keyrange &range, TXNID txnid) {
                (void) range;
                (void) txnid;
                count++;
                return true;
            }
        } count_fn;
        count_fn.count = 0;
        locktree_iterate<count_fn_obj>(lt, &count_fn);
        return count_fn.count;
    }
};

} /* namespace toku */

#endif /* TOKU_LOCKTREE_UNIT_TEST_H */
