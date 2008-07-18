/* The goal of this test.  Make sure that inserts stay behind deletes. */

#include "brt.h"
#include "key.h"
#include "toku_assert.h"
#include "brt-internal.h"


#include <stdio.h>
#include <string.h>
#include <unistd.h>


static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

enum { NODESIZE = 1024, KSIZE=NODESIZE-100, PSIZE=20 };

CACHETABLE ct;
BRT t;
int fnamelen;
char *fname;

void doit (void) {
    DISKOFF nodea,nodeb;
    u_int32_t fingerprinta=0;

    int r;
    
    fnamelen = strlen(__FILE__) + 20;
    fname = malloc(fnamelen);
    assert(fname!=0);

    snprintf(fname, fnamelen, "%s.brt", __FILE__);
    r = toku_brt_create_cachetable(&ct, 16*1024, ZERO_LSN, NULL_LOGGER); assert(r==0);
    unlink(fname);
    r = toku_open_brt(fname, 0, 1, &t, NODESIZE, ct, null_txn, toku_default_compare_fun, null_db);
    assert(r==0);
    free(fname);

    r = toku_testsetup_leaf(t, &nodea);
    assert(r==0);

    r = toku_testsetup_nonleaf(t, 1, &nodeb, 1, &nodea, &fingerprinta, 0, 0);
    assert(r==0);

    u_int32_t fingerprint=0;
    r = toku_testsetup_insert_to_nonleaf(t, nodeb, BRT_DELETE_ANY, "hello", 6, 0, 0, &fingerprint);
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
    r = toku_brt_lookup(t, &k, &v);
    assert(r==0);

    r = toku_close_brt(t, 0);       assert(r==0);
    r = toku_cachetable_close(&ct); assert(r==0);
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    doit();
    return 0;
}
