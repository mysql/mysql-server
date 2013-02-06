/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "locktree_unit_test.h"

namespace toku {

static DBT *expected_a;
static DBT *expected_b;
static DESCRIPTOR expected_descriptor;
static int expected_comparison_magic = 55;

static int my_compare_dbts(DB *db, const DBT *a, const DBT *b) {
    invariant(db->cmp_descriptor == expected_descriptor);
    (void) a; 
    (void) b;
    return expected_comparison_magic;
}

// test that get/set userdata works, and that get_manager() works
void locktree_unit_test::test_misc(void) {
    locktree::manager mgr;
    mgr.create(nullptr, nullptr, nullptr, nullptr);
    DESCRIPTOR desc = nullptr;
    DICTIONARY_ID dict_id = { 1 };
    locktree *lt = mgr.get_lt(dict_id, desc, my_compare_dbts, nullptr);

    invariant(lt->get_userdata() == nullptr);
    int userdata;
    lt->set_userdata(&userdata);
    invariant(lt->get_userdata() == &userdata);
    lt->set_userdata(nullptr);
    invariant(lt->get_userdata() == nullptr);

    int r;
    DBT dbt_a, dbt_b;
    DESCRIPTOR_S d1, d2;
    expected_a = &dbt_a;
    expected_b = &dbt_b;

    // make sure the comparator object has the correct
    // descriptor when we set the locktree's descriptor
    lt->set_descriptor(&d1);
    expected_descriptor = &d1;
    r = lt->m_cmp->compare(&dbt_a, &dbt_b);
    invariant(r == expected_comparison_magic);
    lt->set_descriptor(&d2);
    expected_descriptor = &d2;
    r = lt->m_cmp->compare(&dbt_a, &dbt_b);
    invariant(r == expected_comparison_magic);

    mgr.release_lt(lt);
    mgr.destroy();
}

} /* namespace toku */

int main(void) {
    toku::locktree_unit_test test;
    test.test_misc();
    return 0;
}
