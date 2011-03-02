/* The goal of this test.  Make sure that inserts stay behind deletes. */


#include "test.h"
#include "includes.h"

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

enum { NODESIZE = 1024, KSIZE=NODESIZE-100, TOKU_PSIZE=20 };

CACHETABLE ct;
BRT t;
int fnamelen;
char *fname;

static void
doit (void) {
    BLOCKNUM nodea,nodeb;

    int r;
    
    fnamelen = strlen(__FILE__) + 20;
    fname = toku_malloc(fnamelen);
    assert(fname!=0);

    snprintf(fname, fnamelen, "%s.brt", __FILE__);
    r = toku_brt_create_cachetable(&ct, 16*1024, ZERO_LSN, NULL_LOGGER); assert(r==0);
    unlink(fname);
    r = toku_open_brt(fname, 1, &t, NODESIZE, ct, null_txn, toku_builtin_compare_fun, null_db);
    assert(r==0);
    toku_free(fname);

    r = toku_testsetup_leaf(t, &nodea);
    assert(r==0);

    r = toku_testsetup_nonleaf(t, 1, &nodeb, 1, &nodea, 0, 0);
    assert(r==0);

    r = toku_testsetup_insert_to_nonleaf(t, nodeb, BRT_DELETE_ANY, "hello", 6, 0, 0);
    assert(r==0);

    r = toku_testsetup_root(t, nodeb);
    assert(r==0);
    
    DBT k,v;
    r = toku_brt_insert(t,
			toku_fill_dbt(&k, "hello", 6),
			toku_fill_dbt(&v, "there", 6),
			null_txn);
    assert(r==0);

    memset(&v, 0, sizeof(v));
    struct check_pair pair = {6, "hello", 6, "there", 0};
    r = toku_brt_lookup(t, &k, lookup_checkf, &pair);
    assert(r==0);
    assert(pair.call_count == 1);

    r = toku_close_brt(t, 0);    assert(r==0);
    r = toku_cachetable_close(&ct); assert(r==0);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    doit();
    return 0;
}
