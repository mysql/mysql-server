/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "manager_unit_test.h"

namespace toku {

void manager_unit_test::test_create_destroy(void) {
    locktree::manager mgr;
    locktree::manager::lt_create_cb create_callback =
        (locktree::manager::lt_create_cb) (long) 1;
    locktree::manager::lt_destroy_cb destroy_callback =
        (locktree::manager::lt_destroy_cb) (long) 2;
    locktree::manager::lt_escalate_cb escalate_callback =
        (locktree::manager::lt_escalate_cb) (long) 3;
    void *extra = (void *) (long) 4;
    mgr.create(create_callback, destroy_callback, escalate_callback, extra);

    invariant(mgr.m_max_lock_memory == locktree::manager::DEFAULT_MAX_LOCK_MEMORY);
    invariant(mgr.m_current_lock_memory == 0);
    invariant(mgr.m_escalation_count == 0);
    invariant(mgr.m_escalation_time == 0);
    invariant(mgr.m_escalation_latest_result == 0);
    invariant(mgr.m_lock_wait_time_ms == locktree::manager::DEFAULT_LOCK_WAIT_TIME);

    invariant(mgr.m_locktree_map.size() == 0);
    invariant(mgr.m_lt_create_callback == create_callback);
    invariant(mgr.m_lt_destroy_callback == destroy_callback);
    invariant(mgr.m_lt_escalate_callback == escalate_callback);
    invariant(mgr.m_lt_escalate_callback_extra == extra);

    mgr.mutex_lock();
    mgr.mutex_unlock();

    mgr.destroy();
}

} /* namespace toku */

int main(void) {
    toku::manager_unit_test test;
    test.test_create_destroy();
    return 0; 
}
