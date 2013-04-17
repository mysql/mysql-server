#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

//
// test that nodes written out for checkpointing properly update the stats
// insert a bunch of elements, but not too many. The amount of data should
// fit into a single leaf node. Then we :
//  - checkpoint
//  - close dictionary
//  - reopen dictionary
//  - call stat64
// prior to the fix for 4657, the stats would return
// 0 rows. After the fix, the stats should return an
// accurate number of rows
//

int test_main (int argc, char * const argv[]) {
  parse_args(argc, argv);
  int r;
  r = system("rm -rf " ENVDIR);
  CKERR(r);
  toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
  DB_ENV *env;
  r = db_env_create(&env, 0);                                                         CKERR(r);
  env->set_errfile(env, stderr);
  r = env->set_default_bt_compare(env, int64_dbt_cmp); CKERR(r);
  r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);
    
  DB *db;
  {
    DB_TXN *txna;
    r = env->txn_begin(env, NULL, &txna, 0);                                        CKERR(r);

    r = db_create(&db, env, 0);                                                     CKERR(r);
    CKERR(r);
    r = db->open(db, txna, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666);              CKERR(r);

    r = txna->commit(txna, 0);                                                      CKERR(r);
  }
  if (verbose) printf("starting insertion of even elements\n");
  //
  // now insert 1000 elements
  //
  DB_TXN* txn;
  r = env->txn_begin(env, NULL, &txn, 0);
  CKERR(r);
  for (u_int32_t i = 0; i < 1000; i++) {
      DBT key,val;
      u_int64_t key_data = i;
      u_int64_t val_data = i;
      r = db->put(
          db,
          txn,
          dbt_init(&key, &key_data, sizeof(key_data)),
          dbt_init(&val, &val_data, sizeof(val_data)),
          0
          );       
      CKERR(r);
  }
  r = txn->commit(txn, 0);
  CKERR(r);

  //
  // the assumption here is that the db consists
  // of a single leaf node that is the root.
  //

  r = env->txn_checkpoint(env, 0, 0, 0);
  CKERR(r);
  r = db->close(db, 0);
  CKERR(r);

  // now reopen 
  r = db_create(&db, env, 0);
  CKERR(r);
  r = db->open(db, NULL, "foo.db", NULL, DB_BTREE, DB_THREAD, 0666);
  CKERR(r);
  DB_BTREE_STAT64 dict_stats;
  r = db->stat64(
      db, 
      NULL, 
      &dict_stats
      );
  CKERR(r);
  // check that stats are correct
  assert(dict_stats.bt_nkeys == 1000);
  
  r = db->close(db, 0);
  CKERR(r);
  
  r = env->close(env, 0);
  CKERR(r);
    
  return 0;
}
