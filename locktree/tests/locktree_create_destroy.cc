/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "locktree_unit_test.h"

namespace toku {

// test simple create and destroy of the locktree
void locktree_unit_test::test_create_destroy(void) {
    locktree::manager mgr;
    mgr.create(nullptr, nullptr);
    DESCRIPTOR desc = nullptr;
    DICTIONARY_ID dict_id = { 1 };
    locktree *lt = mgr.get_lt(dict_id, desc, compare_dbts, nullptr);

    locktree::lt_lock_request_info *info = lt->get_lock_request_info();
    invariant_notnull(info);
    toku_mutex_lock(&info->mutex);
    toku_mutex_unlock(&info->mutex);

    invariant(lt->m_dict_id.dictid == dict_id.dictid);
    invariant(lt->m_reference_count == 1);
    invariant(lt->m_rangetree != nullptr);
    invariant(lt->m_userdata == nullptr);
    invariant(info->pending_lock_requests.size() == 0);

    mgr.release_lt(lt);
    mgr.destroy();
}

} /* namespace toku */

int main(void) {
    toku::locktree_unit_test test;
    test.test_create_destroy();
    return 0;
}
