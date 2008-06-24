/* -*- mode: C; c-basic-offset: 4 -*- */
#include <db.h>
#include <sys/stat.h>
#include "test.h"
#include <arpa/inet.h>

unsigned char N=5;
int fact(int n) {
    if (n<=2) return n;
    else return n*fact(n-1);
}

void swapc (unsigned char *a, unsigned char *b) {
    unsigned char tmp=*a;
    *a=*b;
    *b=tmp;
}

DB_ENV *env;
DB *db;

void run (void) {
    int r;
    DB_TXN *txn, *txn2;
    char v101=101, v102=102, v1=1, v2=1;
    // Add (1,102) to the tree
    // In one txn
    //   add (1,101) to the tree
    // In another concurrent txn
    //   look up (1,102) and do  DB_NEXT
    // That should be fine in TokuDB.
    // It fails before #938 is fixed.
    // It also fails for BDB for other reasons (page-level locking vs. row-level locking)
    {
	r=env->txn_begin(env, 0, &txn, 0);                            CKERR(r);
	DBT k,v;
	r=db->put(db, txn, dbt_init(&k, &v2, 1), dbt_init(&v, &v102, 1), DB_YESOVERWRITE); CKERR(r);

	r=txn->commit(txn, 0);                                        CKERR(r);
    }
    {
	r=env->txn_begin(env, 0, &txn, 0);                            CKERR(r);
	r=env->txn_begin(env, 0, &txn2, 0);                           CKERR(r);    

	DBT k,v;

	r=db->put(db, txn, dbt_init(&k, &v1, 1), dbt_init(&v, &v101, 1), DB_YESOVERWRITE); CKERR(r);

	DBC *c2;
	r=db->cursor(db, txn2, &c2, 0);                                CKERR(r);


	r=c2->c_get(c2, dbt_init(&k, &v2, 1), dbt_init(&v, &v102, 1), DB_GET_BOTH); CKERR(r);
	r=c2->c_get(c2, dbt_init_malloc(&k), dbt_init_malloc(&v), DB_NEXT);  assert(r==DB_NOTFOUND);

	r=c2->c_close(c2);
	r=txn->commit(txn, 0);                                             CKERR(r);
	r=txn2->commit(txn2, 0);                                             CKERR(r);
    }
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);

    system("rm -rf " ENVDIR);
    mkdir(ENVDIR, 0777);

    int r;

    DB_TXN *txn;
    {
        r = db_env_create(&env, 0);                                   CKERR(r);
	r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, 0777); CKERR(r);
	env->set_errfile(env, stderr);
	r=env->txn_begin(env, 0, &txn, 0);                            CKERR(r);
	r=db_create(&db, env, 0);                                     CKERR(r);
	r=db->set_flags(db, DB_DUP|DB_DUPSORT);
	r=db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, 0777);  CKERR(r);
	r=txn->commit(txn, 0);                                        CKERR(r);
    }
    //printf("fact(%d)=%d\n", N, fact(N));
    run();
    {
	r=db->close(db, 0);                                           CKERR(r);
	r=env->close(env, 0);                                         CKERR(r);
    }

    return 0;
}
