/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <db.h>

#include "test.h"

#if USE_BDB
#define DB_YESOVERWRITE 0
#endif

int db_put(DB *db, DB_TXN *txn, int k, int v) {
    DBT key, val;
    int r = db->put(db, txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), DB_YESOVERWRITE);
    return r;
}

/* create a tree with 15 of 16 leaf nodes
   each of the leaves should be about 1/2 full
   then almost fill leaf 0 and leaf 13 to almost full
   reopen the tree to flush all of leaves out of the cache
   create a cursor on leaf 0 to pull it in memory
   fill the root buffer 13
   insert to leaf 0.  this should cause leaf 0 to split, cause the root to expand to 16 children, but
   cause the root node to be too big. flush to leaf 16 causing another leaf split, causing the root
   to expand to 17 nodes, which causes the root to split

   the magic number where found via experimentation */

void test_hsoc(int pagesize, int dup_mode) {
    if (verbose) printf("test_hsoc:%d %d\n", pagesize, dup_mode);

    int npp = pagesize / 16;
    int n = npp + 13*npp/2;

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = DIR "/" "test.hsoc.brt";
    int r;

    system("rm -rf " DIR);
    r=mkdir(DIR, 0777); assert(r==0);

    /* create the dup database file */
    r = db_create(&db, null_env, 0); assert(r == 0);
    r = db->set_flags(db, dup_mode); assert(r == 0);
    r = db->set_pagesize(db, pagesize); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    int i;

    /* force 15 leaves (14 splits)  */
    if (verbose) printf("force15\n");
    for (i=0; i<n; i++) {
        r = db_put(db, null_txn, htonl(i), i); assert(r == 0);
    } 

    /* almost fill leaf 0 */
    if (verbose) printf("fill0\n");
    for (i=0; i<(npp/2)-4; i++) {
        r = db_put(db, null_txn, htonl(0), n+i); assert(r == 0);
    }

    /* almost fill leaf 15 */
    if (verbose) printf("fill15\n");
    for (i=0; i<111; i++) { // for (i=0; i<(npp/2)-4; i++) {
        r = db_put(db, null_txn, htonl(n), i); assert(r == 0);
    }

    /* reopen the database to force nonleaf buffering */
    if (verbose) printf("reopen\n");
    r = db->close(db, 0); assert(r == 0);
    r = db_create(&db, null_env, 0); assert(r == 0);
    r = db->set_flags(db, dup_mode); assert(r == 0);
    r = db->set_pagesize(db, pagesize); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666); assert(r == 0);

    /* do a cursor get k=0 to pull in leaf 0 */
    DBC *cursor;

    r = db->cursor(db, null_txn, &cursor, 0); assert(r == 0);

    DBT key, val;
    r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_FIRST); assert(r == 0);
    free(key.data); free(val.data);

    /* fill up buffer 2 in the root node */
    for (i=0; i<216; i++) {
        r = db_put(db, null_txn, htonl(npp), i); assert(r == 0);
    }

    /* push a cmd to leaf 0 to cause it to split */
    for (i=0; i<3; i++) {
        r = db_put(db, null_txn, htonl(0), 2*n+i); assert(r == 0);
    }

    r = cursor->c_close(cursor); assert(r == 0);

    r = db->close(db, 0); assert(r == 0);
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);

    test_hsoc(4096, DB_DUP + DB_DUPSORT);

    return 0;
}
