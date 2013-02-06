/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "manager_unit_test.h"

namespace toku {

void manager_unit_test::test_lt_map(void) {
    locktree::manager mgr;
    mgr.create(nullptr, nullptr, nullptr, nullptr);

    locktree aa;
    locktree bb;
    locktree cc;
    locktree *alt = &aa;
    locktree *blt = &bb;
    locktree *clt = &cc;
    DICTIONARY_ID a = { 1 };
    DICTIONARY_ID b = { 2 };
    DICTIONARY_ID c = { 3 };
    DICTIONARY_ID d = { 4 };
    alt->m_dict_id = a;
    blt->m_dict_id = b;
    clt->m_dict_id = c;

    mgr.locktree_map_put(alt);
    mgr.locktree_map_put(blt);
    mgr.locktree_map_put(clt);

    locktree *lt;

    lt = mgr.locktree_map_find(a);
    invariant(lt == alt);
    lt = mgr.locktree_map_find(c);
    invariant(lt == clt);
    lt = mgr.locktree_map_find(b);
    invariant(lt == blt);

    mgr.locktree_map_remove(alt);
    lt = mgr.locktree_map_find(a);
    invariant(lt == nullptr);
    lt = mgr.locktree_map_find(c);
    invariant(lt == clt);
    lt = mgr.locktree_map_find(b);
    invariant(lt == blt);
    lt = mgr.locktree_map_find(d);
    invariant(lt == nullptr);

    mgr.locktree_map_remove(clt);
    mgr.locktree_map_remove(blt);
    lt = mgr.locktree_map_find(c);
    invariant(lt == nullptr);
    lt = mgr.locktree_map_find(b);
    invariant(lt == nullptr);

    mgr.destroy();
}

} /* namespace toku */

int main(void) {
    toku::manager_unit_test test;
    test.test_lt_map();
    return 0; 
}
