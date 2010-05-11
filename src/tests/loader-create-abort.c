/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "$Id$"

// Ensure that loader->abort free all of its resources.  The test just creates a loader and then
// aborts it.

#include "test.h"
#include <db.h>

static int loader_flags = 0;

static int put_multiple_generate(DB *UU(dest_db), DB *UU(src_db), DBT *UU(dest_key), DBT *UU(dest_val), const DBT *UU(src_key), const DBT *UU(src_val), void *UU(extra)) {
    return ENOMEM;
}

static void loader_open_abort(void) {
    int r;

    r = system("rm -rf " ENVDIR);                                                                             CKERR(r);
    r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);                                                       CKERR(r);

    DB_ENV *env;
    r = db_env_create(&env, 0);                                                                               CKERR(r);
    r = env->set_generate_row_callback_for_put(env, put_multiple_generate);
    CKERR(r);
    int envflags = DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE;
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                                            CKERR(r); 
    env->set_errfile(env, stderr);

    DB_TXN *txn;
    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);

    DB_LOADER *loader;
    r = env->create_loader(env, txn, &loader, NULL, 0, NULL, NULL, NULL, loader_flags); CKERR(r);
    
    r = loader->abort(loader); CKERR(r);

    r = txn->commit(txn, 0); CKERR(r);

    r = env->close(env, 0); CKERR(r);
}

static void do_args(int argc, char * const argv[]) {
    int resultcode;
    char *cmd = argv[0];
    argc--; argv++;
    while (argc>0) {
        if (strcmp(argv[0], "-h")==0) {
	    resultcode=0;
	do_usage:
	    fprintf(stderr, "Usage: [-h] [-v] [-q] [-p]\n%s\n", cmd);
	    exit(resultcode);
        } else if (strcmp(argv[0], "-v")==0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
        } else if (strcmp(argv[0], "-p") == 0) {
            loader_flags = LOADER_USE_PUTS;
	} else {
	    fprintf(stderr, "Unknown arg: %s\n", argv[0]);
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}

int test_main(int argc, char * const *argv) {
    do_args(argc, argv);
    loader_open_abort();
    return 0;
}
