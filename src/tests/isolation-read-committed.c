// Test that isolation works right for subtransactions.
// In particular, check to see what happens if a subtransaction has different isolation level from its parent.

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    int r;
    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    DB_ENV *env;
    r = db_env_create(&env, 0);                                                         CKERR(r);
    env->set_errfile(env, stderr);
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);
    
    DB *db;
    {
        DB_TXN *txna;
        r = env->txn_begin(env, NULL, &txna, 0);                                        CKERR(r);

        r = db_create(&db, env, 0);                                                     CKERR(r);
        r = db->open(db, txna, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666);              CKERR(r);

        DBT key,val;
        r = db->put(db, txna, dbt_init(&key, "a", 4), dbt_init(&val, "a", 4), 0);       CKERR(r);

        r = txna->commit(txna, 0);                                                      CKERR(r);
    }
    DB_TXN *txn_put, *txn_committed, *txn_uncommitted;
    r = env->txn_begin(env, NULL, &txn_put, DB_READ_COMMITTED);                          CKERR(r);
    r = env->txn_begin(env, NULL, &txn_committed, DB_READ_COMMITTED);                          CKERR(r);
    r = env->txn_begin(env, NULL, &txn_uncommitted, DB_READ_UNCOMMITTED);                          CKERR(r);

    //
    // test a simple get
    //
    {
        DBT key,val;
        r = db->put(db, txn_put, dbt_init(&key, "x", 4), dbt_init(&val, "x", 4), 0);   CKERR(r);
        dbt_init_malloc(&val);
        r = db->get(db, txn_put, dbt_init(&key, "x", 4), &val, 0);  CKERR(r);
        toku_free(val.data);

        dbt_init_malloc(&val);
        r = db->get(db, txn_committed, dbt_init(&key, "x", 4), &val, 0);    CKERR2(r, DB_NOTFOUND);
        toku_free(val.data);

        dbt_init_malloc(&val);
        r = db->get(db, txn_uncommitted, dbt_init(&key, "x", 4), &val, 0);  CKERR(r);
        toku_free(val.data);
        
        r = db->del(db, txn_put, dbt_init(&key, "a", 4), 0);  CKERR(r);

        dbt_init_malloc(&val);
        r = db->get(db, txn_put, dbt_init(&key, "a", 4), &val, 0);  CKERR2(r, DB_NOTFOUND);
        toku_free(val.data);

        dbt_init_malloc(&val);
        r = db->get(db, txn_committed, dbt_init(&key, "a", 4), &val, 0);    CKERR(r);
        toku_free(val.data);

        dbt_init_malloc(&val);
        r = db->get(db, txn_uncommitted, dbt_init(&key, "a", 4), &val, 0);  CKERR2(r, DB_NOTFOUND);
        toku_free(val.data);

        val.data = NULL;
    }

    
    r = txn_put->commit(txn_put, 0);                                                          CKERR(r);
    r = txn_committed->commit(txn_committed, 0);                                             CKERR(r);
    r = txn_uncommitted->commit(txn_uncommitted, 0);                                             CKERR(r);

    r = env->txn_begin(env, NULL, &txn_put, DB_READ_COMMITTED);                          CKERR(r);
    r = env->txn_begin(env, NULL, &txn_committed, DB_READ_COMMITTED);                          CKERR(r);
    r = env->txn_begin(env, NULL, &txn_uncommitted, DB_READ_UNCOMMITTED);                          CKERR(r);

    //
    // test a simple get
    //
    {
        DBT key,val;
        DBT curr_key, curr_val;
        DBC* cursor_committed = NULL;
        DBC* cursor_uncommitted = NULL;
        memset(&curr_key, 0, sizeof(curr_key));
        memset(&curr_val, 0, sizeof(curr_val));

        r = db->cursor(db, txn_committed, &cursor_committed, 0); assert(r == 0);
        r = db->cursor(db, txn_uncommitted, &cursor_uncommitted, 0); assert(r == 0);

        r = db->put(db, txn_put, dbt_init(&key, "y", 4), dbt_init(&val, "y", 4), 0);   CKERR(r);

        r = cursor_uncommitted->c_get(cursor_uncommitted, &curr_key, &curr_val, DB_NEXT); CKERR(r);
        assert(((char *)(curr_key.data))[0] == 'x');
        assert(((char *)(curr_val.data))[0] == 'x');

        r = cursor_committed->c_get(cursor_committed, &curr_key, &curr_val, DB_NEXT); CKERR(r);
        assert(((char *)(curr_key.data))[0] == 'x');
        assert(((char *)(curr_val.data))[0] == 'x');
    


        r = cursor_committed->c_get(cursor_committed, &curr_key, &curr_val, DB_NEXT); CKERR2(r, DB_NOTFOUND);
        r = cursor_uncommitted->c_get(cursor_uncommitted, &curr_key, &curr_val, DB_NEXT); CKERR(r);
        assert(((char *)(curr_key.data))[0] == 'y');
        assert(((char *)(curr_val.data))[0] == 'y');

    }
    r = txn_put->commit(txn_put, 0);                                                          CKERR(r);
    r = txn_committed->commit(txn_committed, 0);                                             CKERR(r);
    r = txn_uncommitted->commit(txn_uncommitted, 0);                                             CKERR(r);


    r = db->close(db, 0);                                                               CKERR(r);
    r = env->close(env, 0);                                                             CKERR(r);
    
    return 0;
}
