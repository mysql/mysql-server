/* Test to see if DB->get works on a zeroed DBT. */

#include <db.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/stat.h>

#include "test.h"

DBT *dbt_init(DBT *dbt, void *data, u_int32_t size) {
    memset(dbt, 0, sizeof *dbt);
    dbt->data = data;
    dbt->size = size;
    return dbt;
}

void test_get (int dup_mode) {
    DB_ENV * const null_env = 0;
    DB_TXN * const null_txn = 0;
    DB *db;
    DBT key,data;
    int fnamelen = sizeof(DIR) + 30;
    char fname[fnamelen];
    int r;
    snprintf(fname, fnamelen, "%s/test%d.db", DIR, dup_mode);
    r = db_create (&db, null_env, 0);                                        assert(r == 0);
    r = db->set_flags(db, dup_mode);                                         assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);    assert(r == 0);
    dbt_init(&key, "a", 2);
    r = db->put(db, null_txn, &key, dbt_init(&data, "b", 2), 0);             assert(r==0);
    memset(&data, 0, sizeof(data));
    r = db->get(db, null_txn, &key, &data, 0);                               assert(r == 0);
    assert(strcmp(data.data, "b")==0);
    r = db->close(db, 0); 
}

int main (int argc, const char *argv[]) {
    parse_args(argc, argv);
    system("rm -rf " DIR);
    mkdir(DIR, 0777);

    test_get(0);
    test_get(DB_DUP + DB_DUPSORT);
    return 0;
}
