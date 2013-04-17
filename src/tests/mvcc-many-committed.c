// Test that isolation works right for subtransactions.
// In particular, check to see what happens if a subtransaction has different isolation level from its parent.

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    DB_ENV *env;
    u_int32_t i = 0;
    u_int32_t num_read_txns = 1000;
    r = db_env_create(&env, 0);                                                         CKERR(r);
    env->set_errfile(env, stderr);
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);
    
    DB *db;

    DB_TXN* create_txn;
    DB_TXN* read_txns[num_read_txns];
    DB_TXN* read_uncommitted_txn;
    memset(read_txns, 0, sizeof(read_txns));

    r = env->txn_begin(env, NULL, &create_txn, 0);                                        CKERR(r);

    r = db_create(&db, env, 0);                                                     CKERR(r);
    r = db->open(db, create_txn, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666);              CKERR(r);
    r = create_txn->commit(create_txn, 0);                                                      CKERR(r);

    DBT key,val;

    for (i = 0; i < num_read_txns; i++) {
        DB_TXN* put_txn = NULL;
        u_int32_t data = i;
        r = env->txn_begin(env, NULL, &put_txn, DB_TXN_SNAPSHOT);
        CKERR(r);
        r = db->put(
            db, 
            put_txn, 
            dbt_init(&key, "a", 4), 
            dbt_init(&val, &data, 4), 
            0
            );       
        CKERR(r);
        r = put_txn->commit(put_txn, 0);
        CKERR(r);
        //this should read the above put
        r = env->txn_begin(env, NULL, &read_txns[i], DB_TXN_SNAPSHOT);
        CKERR(r);
            
    }

    for (i = 0; i < num_read_txns; i++) {
        DBT curr_key, curr_val;
        memset(&curr_key, 0, sizeof(curr_key));
        memset(&curr_val, 0, sizeof(curr_val));
        DBC* snapshot_cursor = NULL;
        r = db->cursor(db, read_txns[i], &snapshot_cursor, 0); CKERR(r);        
        r = snapshot_cursor->c_get(snapshot_cursor, &curr_key, &curr_val, DB_NEXT); CKERR(r);
        assert(((char *)(curr_key.data))[0] == 'a');
        assert((*(u_int32_t *)(curr_val.data)) == i);
        assert(curr_key.size == 4);
        assert(curr_val.size == 4);
        snapshot_cursor->c_close(snapshot_cursor);
    }
    {
        DBT curr_key, curr_val;
        memset(&curr_key, 0, sizeof(curr_key));
        memset(&curr_val, 0, sizeof(curr_val));
        r = env->txn_begin(env, NULL, &read_uncommitted_txn, DB_READ_UNCOMMITTED);
        CKERR(r);
        DBC* read_uncommitted_cursor = NULL;
        r = db->cursor(db, read_uncommitted_txn, &read_uncommitted_cursor, 0); CKERR(r);        
        r = read_uncommitted_cursor->c_get(
            read_uncommitted_cursor, 
            &curr_key, 
            &curr_val, 
            DB_NEXT
            ); 
        CKERR(r);
        assert(((char *)(curr_key.data))[0] == 'a');
        assert((*(u_int32_t *)(curr_val.data)) == (num_read_txns - 1));
        assert(curr_key.size == 4);
        assert(curr_val.size == 4);
        read_uncommitted_cursor->c_close(read_uncommitted_cursor);
    }
    for (i = 0; i < num_read_txns; i++) {
        r = read_txns[i]->commit(read_txns[i], 0);
        CKERR(r);
    }
    r = read_uncommitted_txn->commit(read_uncommitted_txn, 0);

    r = db->close(db, 0);                                                               CKERR(r);
    r = env->close(env, 0);                                                             CKERR(r);
    
    return 0;
}
