/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "manager_unit_test.h"

namespace toku {

void manager_unit_test::test_params(void) {
    int r;
    locktree::manager mgr;
    mgr.create(nullptr, nullptr, nullptr, nullptr);

    uint64_t new_max_lock_memory = 15307752356;
    r = mgr.set_max_lock_memory(new_max_lock_memory);
    invariant(r == 0);
    invariant(mgr.get_max_lock_memory() == new_max_lock_memory);

    uint64_t new_lock_wait_time = 62345234;
    mgr.set_lock_wait_time(new_lock_wait_time);
    invariant(mgr.get_lock_wait_time() == new_lock_wait_time);

    mgr.m_current_lock_memory = 100000;
    r = mgr.set_max_lock_memory(mgr.m_current_lock_memory - 1);
    invariant(r == EDOM);
    invariant(mgr.get_max_lock_memory() == new_max_lock_memory);

    mgr.m_current_lock_memory = 0;
    mgr.destroy();
}

} /* namespace toku */

int main(void) {
    toku::manager_unit_test test;
    test.test_params();
    return 0; 
}
