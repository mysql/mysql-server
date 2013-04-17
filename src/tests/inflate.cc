/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/* Idea: inflate a node by 
 *    create a 2-level tree
 *    Nodes are A B C D E F G H
 *    Fill them up sequentially so they'll all be near 4MB.
 *    Close the file
 *    Insert some more to H (buffered in the root)
 *    Delete stuff from G (so that H merges with G)
 *    G ends up too big.
 */

#include "test.h"

DB_ENV *env;
DB *db;
const char dbname[] = "foo.db";
const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_PRIVATE|DB_INIT_TXN;

static void
open_em (void)
{
    int r;
    r = db_env_create(&env, 0);                                         CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);      CKERR(r);
    r = db_create(&db, env, 0);                                         CKERR(r);
    r = db->open(db, NULL, dbname, NULL, DB_BTREE, DB_CREATE, 0666);    CKERR(r);
}

static void
close_em (void)
{
    int r;
    r = db->close(db, 0);   CKERR(r);
    r = env->close(env, 0); CKERR(r);
}

static void
reopen_em (void)
{
    close_em();
    open_em();
}


static void
setup(void)
{
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    r = db_env_create(&env, 0);                                         CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);      CKERR(r);
    r = db_create(&db, env, 0);                                         CKERR(r);
    r = db->set_pagesize(db, 8192);                                     CKERR(r);
    r = db->open(db, NULL, dbname, NULL, DB_BTREE, DB_CREATE, 0666);    CKERR(r);
}

char  vdata[150];

static void
insert_n (uint32_t ah) {
    uint32_t an = htonl(ah);
    DBT key;
    dbt_init(&key, &an, 4);
    DBT val;
    dbt_init(&val, vdata, sizeof vdata);
    int r = db->put(db, NULL, &key, &val, 0);
    CKERR(r);
}

static void
delete_n (uint32_t ah)
{
    uint32_t an = htonl(ah);
    DBT key;
    dbt_init(&key, &an, 4);
    int r = db->del(db, NULL, &key, DB_DELETE_ANY);
#ifdef USE_BDB
    assert(r==0 || r==DB_NOTFOUND);
#else
    CKERR(r);
#endif
}

static void
get_n (uint32_t ah, int expect_r)
{
    uint32_t an = htonl(ah);
    DBT key;
    dbt_init(&key, &an, 4);
    DBT val;
    dbt_init_malloc(&val);
    int r = db->get(db, NULL, &key, &val, 0);
    assert(r==expect_r);
    if (r==0) toku_free(val.data);
}

static void
doit (void)
{
    uint32_t N=100;
    for (uint32_t i=0; i<N; i++) {
	insert_n(i<<16);
    }
    reopen_em();
    for (uint32_t j=0; j<46; j++) {
	insert_n(('.'<<16) + 1 +j);
    }
    for (uint32_t i=N-1; i<N; i++) {
	delete_n(i<<16);
	get_n(i<<16, DB_NOTFOUND);
    }
    reopen_em();
    insert_n(N<<16);
    get_n(N<<16, 0);
    reopen_em();
    for (uint32_t i='J'; i<N+1; i++) {
	delete_n(i<<16);
	get_n(i<<16, DB_NOTFOUND);
    }
    reopen_em();
    reopen_em();
    for (uint32_t j=0; j<46; j++) {
	insert_n(('.'<<16) + 1 +j +46);
    }
    for (uint32_t i=0; i<13; i++) {
	delete_n((73 - i)<< 16);
	get_n((73-i) << 16, DB_NOTFOUND); 
    }
    reopen_em(); // now a node is 9143 bytes
}

int test_main (int argc __attribute__((__unused__)), char * const argv[] __attribute__((__unused__))) {
    setup();
    doit();
    close_em();
    return 0;
}
