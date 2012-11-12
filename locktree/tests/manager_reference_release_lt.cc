/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "manager_unit_test.h"

namespace toku {

static void create_cb(locktree *lt, void *extra) {
    lt->set_userdata(extra);
    bool *k = (bool *) extra;
    invariant(!(*k));
    (*k) = true;
}

static void destroy_cb(locktree *lt) {
    bool *k = (bool *) lt->get_userdata();
    invariant(*k);
    (*k) = false;
}

void manager_unit_test::test_reference_release_lt(void) {
    locktree::manager mgr;
    mgr.create(create_cb, destroy_cb);

    DICTIONARY_ID a = { 0 };
    DICTIONARY_ID b = { 1 };
    DICTIONARY_ID c = { 2 };
    bool aok = false;
    bool bok = false;
    bool cok = false;
    
    int d = 5;
    DESCRIPTOR_S desc_s;
    desc_s.dbt.data = &d;
    desc_s.dbt.size = desc_s.dbt.ulen = sizeof(d);
    desc_s.dbt.flags = DB_DBT_USERMEM;

    locktree *alt = mgr.get_lt(a, &desc_s, nullptr, &aok);
    invariant_notnull(alt);
    locktree *blt = mgr.get_lt(b, &desc_s, nullptr, &bok);
    invariant_notnull(alt);
    locktree *clt = mgr.get_lt(c, &desc_s, nullptr, &cok);
    invariant_notnull(alt);

    // three distinct locktrees should have been returned
    invariant(alt != blt && alt != clt && blt != clt);

    // on create callbacks should have been called
    invariant(aok);
    invariant(bok);
    invariant(cok);

    // add 3 refs. b should still exist.
    mgr.reference_lt(blt);
    mgr.reference_lt(blt);
    mgr.reference_lt(blt);
    invariant(bok);
    // remove 3 refs. b should still exist.
    mgr.release_lt(blt);
    mgr.release_lt(blt);
    mgr.release_lt(blt);
    invariant(bok);

    // get another handle on a and b, they shoudl be the same
    // as the original alt and blt
    locktree *blt2 = mgr.get_lt(b, &desc_s, nullptr, &bok);
    invariant(blt2 == blt);
    locktree *alt2 = mgr.get_lt(a, &desc_s, nullptr, &aok);
    invariant(alt2 == alt);

    // remove one ref from everything. c should die. a and b are ok.
    mgr.release_lt(alt);
    mgr.release_lt(blt);
    mgr.release_lt(clt);
    invariant(aok);
    invariant(bok);
    invariant(!cok);

    // release a and b. both should die.
    mgr.release_lt(blt2);
    mgr.release_lt(alt2);
    invariant(!aok);
    invariant(!bok);
    
    mgr.destroy();
}

} /* namespace toku */

int main(void) {
    toku::manager_unit_test test;
    test.test_reference_release_lt();
    return 0; 
}
