// test that an update calls back into the update function

#include "test.h"
#include "toku_pthread.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

DB_ENV *env;


static void checkpoint_callback_1(void * extra) {
    assert(extra == NULL);
    usleep(10*1024*1024);
}

static void *run_checkpoint(void *arg) {
    int r = env->txn_checkpoint(env, 0, 0, 0);
    assert_zero(r);
    return arg;
}

static uint64_t tdelta_usec(struct timeval *tend, struct timeval *tstart) {
    uint64_t t = tend->tv_sec * 1000000 + tend->tv_usec;
    t -= tstart->tv_sec * 1000000 + tstart->tv_usec;
    return t;
}

static void setup (void) {
    CHK(system("rm -rf " ENVDIR));
    CHK(toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO));
    CHK(db_env_create(&env, 0));
    db_env_set_checkpoint_callback(checkpoint_callback_1, NULL);
    env->set_errfile(env, stderr);
    CHK(env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO));
}

static void cleanup (void) {
    CHK(env->close(env, 0));
}

static void run_test(void) {
    DB* db = NULL;
    
    IN_TXN_COMMIT(env, NULL, txn_create, 0, {
            CHK(db_create(&db, env, 0));
            CHK(db->open(db, txn_create, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666));
        });
    DBT key, val;
    int i = 0;
    int v = 0;
    dbt_init(&key, &i, sizeof(i));
    dbt_init(&val, &v, sizeof(v));
    // put a value to make it dirty, just to make sure that checkpoint
    // will do something
    CHK(db->put(db, NULL, &key, &val, 0));

    // at this point, we have a db that is dirty. Now we want to do the following
    // have two threads each start a checkpoint
    // then have a third thread try to create a txn, do a write, 
    // and commit the txn. In 5.2.3, the commit of the txn would block
    // until the one of the checkpoints complete (which should take 10 seconds)
    // With the fix, the commit should return immedietely
    toku_pthread_t chkpt1_tid;
    toku_pthread_t chkpt2_tid;

    
    CHK(toku_pthread_create(&chkpt1_tid, NULL, run_checkpoint, NULL)); 
    CHK(toku_pthread_create(&chkpt2_tid, NULL, run_checkpoint, NULL)); 
    usleep(2*1024*1024);
    struct timeval tstart;
    gettimeofday(&tstart, NULL);
    DB_TXN *txn = NULL;
    CHK(env->txn_begin(env, NULL, &txn, 0));
    i = 1; v = 1;
    CHK(db->put(db, txn, &key, &val, 0));
    CHK(txn->commit(txn, 0));
    
    struct timeval tend; 
    gettimeofday(&tend, NULL);
    u_int64_t diff = tdelta_usec(&tend, &tstart);
    assert(diff < 5*1024*1024); 


    void *ret;
    CHK(toku_pthread_join(chkpt2_tid, &ret)); 
    CHK(toku_pthread_join(chkpt1_tid, &ret)); 

    CHK(db->close(db,0));
    db = NULL;
}

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    setup();
    run_test();
    cleanup();
    return 0;
}
