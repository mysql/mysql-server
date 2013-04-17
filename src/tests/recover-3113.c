/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ident "$Id$"
#include "test.h"
#include <db.h>
#include <sys/stat.h>


/*****************
 * Purpose: Verify fix for 3113
 * Bug:     Rollback log is checkpointed along with other cachefiles,
 *          but system crashes before checkpoint_end is written to recovery log.
 *          When recovery runs, it uses latest rollback log, which is out of synch
 *          with recovery log.  Latest version of rollback log would be correct for 
 *          last checkpoint if it completed, but version of rollback log needed
 *          is for last complete checkpoint.
 * Fix:     When opening rollback log for recovery, do not use latest, but use 
 *          latest that is no newer than last complete checkpoint.
 * Test:    begin txn
 *          insert
 *          commit
 *          complete checkpoint (no live txns in checkpoint)
 *          begin txn
 *          insert
 *          begin checkpoint (txn in checkpointed rollback log)
 *          crash using callback2 (just before checkpoint_end is written to disk)
 *          attempt to recover, should crash with 3113
 */        

static const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;
static char *namea="a.db";
static void checkpoint_callback_2(void * UU(extra));
static DB_ENV *env;
static BOOL do_test=FALSE, do_recover=FALSE;

static void
run_test(void) {
    int r;

    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    r = db_env_create(&env, 0);                                                         CKERR(r);
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);
    
    DB *db;
    r = db_create(&db, env, 0);                                                         CKERR(r);
    r = db->open(db, NULL, namea, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);      CKERR(r);

    // txn_begin; insert <a,a>; txn_abort
    {
        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);                                         CKERR(r);
	DBT k={.data="a", .size=2};
	DBT v={.data="a", .size=2};
	r = db->put(db, txn, &k, &v, DB_YESOVERWRITE);                                  CKERR(r);
        r = txn->commit(txn, 0);                                                            CKERR(r);
    }

    // checkpoint, no live txns in rollback log
    r = env->txn_checkpoint(env, 0, 0, 0);                                              CKERR(r);

    {
        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);                                         CKERR(r);
	DBT k={.data="b", .size=2};
	DBT v={.data="b", .size=2};
	r = db->put(db, txn, &k, &v, DB_YESOVERWRITE);                                  CKERR(r);
    }

    // cause crash at next checkpoint, after xstillopen written, before checkpoint_end is written
    db_env_set_checkpoint_callback2(checkpoint_callback_2, NULL);

    // checkpoint, putting xstillopen in recovery log (txn is still active)
    r = env->txn_checkpoint(env, 0, 0, 0);                                              CKERR(r);

}
 

static void checkpoint_callback_2(void * UU(extra)) {
    toku_hard_crash_on_purpose();
}


static void run_recover (void) {
    int r;

    // Recovery starts from oldest_living_txn, which is older than any inserts done in run_test,
    // so recovery always runs over the entire log.

    // run recovery
    r = db_env_create(&env, 0);                                                         CKERR(r);
    r = env->open(env, ENVDIR, envflags + DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);         CKERR(r);
    r = env->close(env, 0);                                                             CKERR(r);
    exit(0);

}

static void test_parse_args (int argc, char * const argv[]) {
    int resultcode;
    char * cmd = argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0], "-v") == 0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
	} else if (strcmp(argv[0], "--test")==0) {
	    do_test=TRUE;
        } else if (strcmp(argv[0], "--recover") == 0) {
            do_recover=TRUE;
	} else if (strcmp(argv[0], "-h")==0) {
	    resultcode=0;
	do_usage:
	    fprintf(stderr, "Usage:\n%s [-v|-q]* [-h] {--test | --recover } \n", cmd);
	    exit(resultcode);
	} else {
	    fprintf(stderr, "Unknown arg: %s\n", argv[0]);
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}



int
test_main (int argc, char * const argv[]) {
    test_parse_args(argc, argv);

    if (do_test)
	run_test();
    else if (do_recover)
	run_recover();

    return 0;
}

