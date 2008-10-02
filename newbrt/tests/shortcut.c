#include "includes.h"

static const char fname[]= __FILE__ ".brt";

static TOKUTXN const null_txn = 0;
CACHETABLE ct;
BRT brt;
BRT_CURSOR cursor;

static int test_brt_cursor_keycompare(DB *db __attribute__((unused)), const DBT *a, const DBT *b) {
    return toku_keycompare(a->data, a->size, b->data, b->size);
}
int main (int argc __attribute__((__unused__)), char *argv[]  __attribute__((__unused__))) {
    int r;
    DB a_db;
    DB *db = &a_db;
    DBT key,val;

    unlink(fname);

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);                               assert(r==0);
    r = toku_open_brt(fname, 0, 1, &brt, 1<<12, ct, null_txn, test_brt_cursor_keycompare, db);   assert(r==0);
    r = toku_brt_cursor(brt, &cursor, 0);                                                        assert(r==0);

    int i;
    for (i=0; i<1000; i++) {
	char string[100];
	snprintf(string, sizeof(string), "%04d", i);
	r = toku_brt_insert(brt, toku_fill_dbt(&key, string, 5), toku_fill_dbt(&val, string, 5), 0);       assert(r==0);
    }

    r = toku_brt_cursor_get(cursor, &key, &val, DB_NEXT, null_txn);                              assert(r==0);
    assert(strcmp(key.data, "0000")==0);
    assert(strcmp(val.data, "0000")==0);

    r = toku_brt_cursor_get(cursor, &key, &val, DB_NEXT, null_txn);                              assert(r==0);
    assert(strcmp(key.data, "0001")==0);
    assert(strcmp(val.data, "0001")==0);

    // This will invalidate due to the root counter bumping, but the OMT itself will still be valid.
    r = toku_brt_insert(brt, toku_fill_dbt(&key, "d", 2), toku_fill_dbt(&val, "w", 2), 0);       assert(r==0);

    r = toku_brt_cursor_get(cursor, &key, &val, DB_NEXT, null_txn);                              assert(r==0);
    assert(strcmp(key.data, "0002")==0);
    assert(strcmp(val.data, "0002")==0);

    r = toku_brt_cursor_close(cursor);                                                           assert(r==0);
    r = toku_close_brt(brt, 0);                                                                  assert(r==0);
    r = toku_cachetable_close(&ct);                                                              assert(r==0);
    return 0;
}
