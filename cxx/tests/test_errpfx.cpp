/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <db_cxx.h>
#include <errno.h>
#include <assert.h>


#include <iostream>
using namespace std;

void test_db_env(void) {
    DbEnv dbenv(DB_CXX_NO_EXCEPTIONS);
    int r;
    
    r = dbenv.set_data_dir("..");   assert(r == 0);
    r = dbenv.set_data_dir(NULL);   assert(r == EINVAL);
    dbenv.set_errpfx("Prefix");
    dbenv.set_errfile(stdout);
    dbenv.err(0, "Hello %s!\n", "Name");
}

int main()
{
    test_db_env();
    return 0;
}
