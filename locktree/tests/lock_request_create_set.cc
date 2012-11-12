/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "lock_request_unit_test.h"

namespace toku {

// create and set the object's internals, destroy should not crash.
void lock_request_unit_test::test_create_destroy(void) {
    lock_request request;
    const uint64_t wait_time_magic = 5016342;
    request.create(wait_time_magic);

    invariant(request.m_txnid == TXNID_NONE);
    invariant(request.m_left_key == nullptr);
    invariant(request.m_right_key == nullptr);
    invariant(request.m_left_key_copy.flags == 0);
    invariant(request.m_left_key_copy.data == nullptr);
    invariant(request.m_right_key_copy.flags == 0);
    invariant(request.m_right_key_copy.data == nullptr);

    invariant(request.m_type == lock_request::type::UNKNOWN);
    invariant(request.m_lt == nullptr);

    invariant(request.m_complete_r == 0);
    invariant(request.m_state == lock_request::state::UNINITIALIZED);
    invariant(request.m_wait_time = wait_time_magic);

    request.destroy();
}

}

int main(void) {
    toku::lock_request_unit_test test;
    test.test_create_destroy();
    return 0;
}

