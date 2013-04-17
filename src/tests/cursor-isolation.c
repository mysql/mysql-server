// Test that flag settings for cursor isolation works

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

int test_main (int argc, char * const argv[]) {
  parse_args(argc, argv);
  int r;
  r = system("rm -rf " ENVDIR);
  CKERR(r);
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

  DB_TXN *txn_serializable, *txn_committed, *txn_uncommitted;
  DBC* cursor = NULL;
  r = env->txn_begin(env, NULL, &txn_serializable, DB_SERIALIZABLE);                          CKERR(r);
  r = env->txn_begin(env, NULL, &txn_committed, DB_READ_COMMITTED);                          CKERR(r);
  r = env->txn_begin(env, NULL, &txn_uncommitted, DB_READ_UNCOMMITTED);                          CKERR(r);


  r = db->cursor(db, txn_serializable, &cursor, DB_SERIALIZABLE|DB_READ_COMMITTED); CKERR2(r, EINVAL);
  r = db->cursor(db, txn_serializable, &cursor, DB_SERIALIZABLE|DB_READ_UNCOMMITTED); CKERR2(r, EINVAL);
  r = db->cursor(db, txn_serializable, &cursor, DB_READ_UNCOMMITTED|DB_READ_COMMITTED); CKERR2(r, EINVAL);


  r = db->cursor(db, txn_serializable, &cursor, 0); CKERR(r);
  r = cursor->c_close(cursor); CKERR(r);
  cursor = NULL;

  r = db->cursor(db, txn_serializable, &cursor, DB_SERIALIZABLE); CKERR(r);
  r = cursor->c_close(cursor); CKERR(r);
  cursor = NULL;

  r = db->cursor(db, txn_serializable, &cursor, DB_READ_COMMITTED); CKERR2(r, EINVAL);
  cursor = NULL;

  r = db->cursor(db, txn_serializable, &cursor, DB_READ_UNCOMMITTED); CKERR2(r, EINVAL);
  cursor = NULL;

  r = db->cursor(db, txn_committed, &cursor, 0); CKERR(r);
  r = cursor->c_close(cursor); CKERR(r);
  cursor = NULL;

  r = db->cursor(db, txn_committed, &cursor, DB_SERIALIZABLE); CKERR(r);
  r = cursor->c_close(cursor); CKERR(r);
  cursor = NULL;

  r = db->cursor(db, txn_committed, &cursor, DB_READ_COMMITTED); CKERR2(r, EINVAL);
  cursor = NULL;

  r = db->cursor(db, txn_committed, &cursor, DB_READ_UNCOMMITTED); CKERR2(r, EINVAL);
  cursor = NULL;

  r = db->cursor(db, txn_uncommitted, &cursor, 0); CKERR(r);
  r = cursor->c_close(cursor); CKERR(r);
  cursor = NULL;

  r = db->cursor(db, txn_uncommitted, &cursor, DB_SERIALIZABLE); CKERR(r);
  r = cursor->c_close(cursor); CKERR(r);
  cursor = NULL;

  r = db->cursor(db, txn_uncommitted, &cursor, DB_READ_COMMITTED); CKERR2(r, EINVAL);
  cursor = NULL;

  r = db->cursor(db, txn_uncommitted, &cursor, DB_READ_UNCOMMITTED); CKERR2(r, EINVAL);
  cursor = NULL;



    
  r = txn_serializable->commit(txn_serializable, 0);                                                          CKERR(r);
  r = txn_committed->commit(txn_committed, 0);                                             CKERR(r);
  r = txn_uncommitted->commit(txn_uncommitted, 0);                                             CKERR(r);



  r = db->close(db, 0);                                                               CKERR(r);
  r = env->close(env, 0);                                                             CKERR(r);
    
  return 0;
}
