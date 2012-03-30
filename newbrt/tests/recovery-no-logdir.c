#ident "$Id$"
// test recovery with a NULL log directory

#include "test.h"
#include "includes.h"

#define TESTDIR "dir." __FILE__ 

static int 
run_test(void) {
    int r;

    // setup the test dir
    r = system("rm -rf " TESTDIR);
    CKERR(r);
    r = toku_os_mkdir(TESTDIR, S_IRWXU); assert(r == 0);

    // run recovery
    r = tokudb_recover(NULL,
		       NULL_keep_zombie_callback,
		       NULL_prepared_txn_callback,
		       NULL_keep_cachetable_callback,
		       NULL_setup_db_callback,
		       NULL_close_db_callback,
		       NULL_logger, NULL, NULL, 0, 0, 0, NULL, 0); 
    assert(r != 0);

    r = system("rm -rf " TESTDIR);
    CKERR(r);

    return 0;
}

int
test_main(int UU(argc), const char *UU(argv[])) {
    int r;
    r = run_test();
    return r;
}
