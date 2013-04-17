/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
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
