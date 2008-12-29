/* -*- mode: C; c-basic-offset: 4 -*- */
#include <toku_portability.h>
/* See #627. */

#include "test.h"
#include <sys/stat.h>
#include <memory.h>

static void
do_627 (void) {
    int r;
    DB_ENV *env;
    DB *db;
    system("rm -rf " ENVDIR);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);       assert(r==0);

    r=db_env_create(&env, 0); assert(r==0);
    env->set_errfile(env, stderr);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);

    DB_TXN *t1, *t2;
    DBT a,b;
    r=env->txn_begin(env, 0, &t1, 0); assert(r==0);
    r=db->open(db, t1, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=db->put(db, t1, dbt_init(&a, "a", 2), dbt_init(&b, "b", 2), 0);
    r=t1->commit(t1, 0);    assert(r==0);

    r=env->txn_begin(env, 0, &t1, 0); assert(r==0);
    r=env->txn_begin(env, 0, &t2, 0); assert(r==0);

    DBC *c1,*c2;

    r=db->cursor(db, t1, &c1, 0); CKERR(r);
    r=db->cursor(db, t2, &c2, 0); CKERR(r);

    r=c1->c_get(c1, dbt_init(&a, "a", 2), dbt_init_malloc(&b), DB_SET); CKERR(r);
    toku_free(b.data);

    r=c2->c_get(c2, dbt_init(&a, "a", 2), dbt_init_malloc(&b), DB_SET); CKERR(r);
    toku_free(b.data);
    
    // This causes all hell to break loose in BDB 4.6, so we just cannot run this under BDB.
    //     PANIC: Invalid argument
    //     Expected DB_LOCK_NOTGRANTED, got DB_RUNRECOVERY: Fatal error, run database recovery
    //     bug627.bdb: bug627.c:44: do_627: Assertion `r==(-30994)' failed.
    //     Aborted
    r=c1->c_del(c1, 0);
    if (r!=DB_LOCK_NOTGRANTED) {
	fprintf(stderr, "Expected DB_LOCK_NOTGRANTED, got %s\n", db_strerror(r));
    }
    assert(r==DB_LOCK_NOTGRANTED);

    r=c1->c_close(c1); CKERR(r);
    r=t1->commit(t1, 0);    assert(r==0);

    r=c2->c_del(c2, 0); CKERR(r);
    r=c2->c_close(c2); CKERR(r);

    r=t2->commit(t2, 0);    assert(r==0);

    r=db->close(db, 0); CKERR(r);
    r=env->close(env, 0);  CKERR(r);
    
    
}

int
test_main (int argc, const char *argv[]) {
    parse_args(argc, argv);
    do_627();
    return 0;
}

