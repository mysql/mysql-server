/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: test_get_max_row_size.cc 45903 2012-07-19 13:06:39Z leifwalsh $"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include "test.h"


static int update_fun(DB *UU(db),
                      const DBT *UU(key),
                      const DBT *UU(old_val), const DBT *UU(extra),
                      void (*set_val)(const DBT *new_val,
                                      void *set_extra),
                      void *UU(set_extra)) 
{
    abort();
    assert(set_val != NULL);        
    return 0;
}

static int generate_row_for_put(
    DB *UU(dest_db), 
    DB *UU(src_db), 
    DBT *UU(dest_key), 
    DBT *UU(dest_val), 
    const DBT *UU(src_key), 
    const DBT *UU(src_val)
    ) 
{
    abort();
    return 0;
}

static int generate_row_for_del(
    DB *UU(dest_db), 
    DB *UU(src_db),
    DBT *UU(dest_key),
    const DBT *UU(src_key), 
    const DBT *UU(src_val)
    ) 
{
    abort();
    return 0;
}

static void test_invalid_ops(uint32_t iso_flags) {
    int r;
    DB * db;
    DB_ENV * env;

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, 0755); { int chk_r = r; CKERR(chk_r); }

    // set things up
    r = db_env_create(&env, 0); 
    CKERR(r);
    r = env->set_generate_row_callback_for_put(env,generate_row_for_put); 
    CKERR(r);
    r = env->set_generate_row_callback_for_del(env,generate_row_for_del); 
    CKERR(r);
    env->set_update(env, update_fun);
    r = env->open(env, TOKU_TEST_FILENAME, DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE, 0755); 
    CKERR(r);
    r = db_create(&db, env, 0); 
    CKERR(r);

    DB_TXN* txn = NULL;
    r = env->txn_begin(env, 0, &txn, iso_flags | DB_TXN_READ_ONLY);
    CKERR(r);

    r = db->open(db, txn, "foo.db", NULL, DB_BTREE, DB_CREATE, 0644); 
    CKERR2(r, EINVAL);
    r = db->open(db, NULL, "foo.db", NULL, DB_BTREE, DB_CREATE, 0644); 
    CKERR(r);

    int k = 1;
    int v = 10;
    DBT key, val;
    dbt_init(&key, &k, sizeof k);
    dbt_init(&val, &v, sizeof v);

    uint32_t db_flags = 0;
    uint32_t indexer_flags = 0;
    DB_INDEXER* indexer;
    r = env->create_indexer(
        env,
        txn,
        &indexer,
        db,
        1,
        &db,
        &db_flags,
        indexer_flags
        );
    CKERR2(r, EINVAL);


    // test invalid operations of ydb_db.cc,
    // db->open tested above
    DB_LOADER* loader;
    uint32_t put_flags = 0;
    uint32_t dbt_flags = 0;
    r = env->create_loader(env, txn, &loader, NULL, 1, &db, &put_flags, &dbt_flags, 0); 
    CKERR2(r, EINVAL);

    r = db->change_descriptor(db, txn, &key, 0);
    CKERR2(r, EINVAL);
    
    //
    // test invalid operations return EINVAL from ydb_write.cc
    //
    r = db->put(db, txn, &key, &val,0);
    CKERR2(r, EINVAL);
    r = db->del(db, txn, &key, DB_DELETE_ANY);
    CKERR2(r, EINVAL);
    r = db->update(db, txn, &key, &val, 0);
    CKERR2(r, EINVAL);
    r = db->update_broadcast(db, txn, &val, 0);
    CKERR2(r, EINVAL);
    
    r = env->put_multiple(env, NULL, txn, &key, &val, 1, &db, &key, &val, 0);
    CKERR2(r, EINVAL);
    r = env->del_multiple(env, NULL, txn, &key, &val, 1, &db, &key, 0);
    CKERR2(r, EINVAL);
    uint32_t flags;
    r = env->update_multiple(
        env, NULL, txn, 
        &key, &val, 
        &key, &val, 
        1, &db, &flags, 
        1, &key, 
        1, &val
        );
    CKERR2(r, EINVAL);

    r = db->close(db, 0); 
    CKERR(r);

    // test invalid operations of ydb.cc, dbrename and dbremove
    r = env->dbremove(env, txn, "foo.db", NULL, 0);
    CKERR2(r, EINVAL);
    // test invalid operations of ydb.cc, dbrename and dbremove
    r = env->dbrename(env, txn, "foo.db", NULL, "bar.db", 0);
    CKERR2(r, EINVAL);

    r = txn->commit(txn, 0);
    CKERR(r);    

    // clean things up
    r = env->close(env, 0); 
    CKERR(r);
}


int test_main(int argc, char * const argv[]) {
    (void) argc;
    (void) argv;
    test_invalid_ops(0);
    test_invalid_ops(DB_TXN_SNAPSHOT);
    test_invalid_ops(DB_READ_COMMITTED);
    test_invalid_ops(DB_READ_UNCOMMITTED);
    return 0;
}
