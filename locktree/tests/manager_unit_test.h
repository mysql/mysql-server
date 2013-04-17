/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#ifndef TOKU_MANAGER_TEST_H
#define TOKU_MANAGER_TEST_H

#include <toku_assert.h>
#include <locktree/locktree.h>

namespace toku {

class manager_unit_test {
public:
    void test_create_destroy(void);
    
    void test_params(void);

    void test_lt_map(void);

    void test_reference_release_lt(void);
};

} /* namespace toku */

#endif /* TOKU_MANAGER_TEST_H */
