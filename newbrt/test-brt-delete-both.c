/* Check to see that delete_both works on both dup and nodup databases. *
 * For recovery to work right, delete_both must work on both cases.
 * Specifically, for a nodup database delete_both must not remove pairs unless
 * they match both key and value.
 */

#include <unistd.h>

#include "brt.h"
#include "test.h"
#include "toku_assert.h"

static TOKUTXN const null_txn = 0;

void doit (void) {
    int r;
    CACHETABLE ct;
    BRT t;
    char fname[] = __FILE__ ".tdb";
    DBT k,v;
    if (verbose) printf("%s\n", __FUNCTION__);
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER); assert(r==0);
    unlink(fname);
    r = toku_brt_create(&t); assert(r==0);
    r = toku_brt_open(t, fname, fname, 0, 1, 1, 0, ct, null_txn, (DB*)0); assert(r==0);

    r = toku_brt_insert(t, toku_fill_dbt(&k, "a", 2), toku_fill_dbt(&v, "x", 2), null_txn);
    assert(r==0);

    r = toku_brt_delete_both(t, toku_fill_dbt(&k, "a", 2), toku_fill_dbt(&v, "y", 2), null_txn);
    assert(r==0);

    r = toku_brt_lookup(t, toku_fill_dbt(&k, "a", 2), toku_init_dbt(&v));
    assert(r==0);
    assert(v.size==2 && strcmp(v.data, "x")==0);

    r = toku_brt_delete_both(t, toku_fill_dbt(&k, "a", 2), toku_fill_dbt(&v, "x", 2), null_txn);
    assert(r==0);
    
    r = toku_brt_lookup(t, toku_fill_dbt(&k, "a", 2), toku_init_dbt(&v));
    assert(r==DB_NOTFOUND);

    r = toku_close_brt(t);              assert(r==0);
    r = toku_cachetable_close(&ct);     assert(r==0);
}

int main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    doit();
    if (verbose) printf("test ok\n");
    return 0;
}
