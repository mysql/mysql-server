/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"

//Verify aborting transactions works properly when transaction 
//starts with an empty db and a table lock.

#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <db.h>
#include <memory.h>
#include <stdio.h>


DB_ENV *env;
DB *db;
DB_TXN *null_txn = NULL;
DB_TXN *txn;
uint32_t find_num;

long closemode = -1; // must be set to 0 or 1 on command line
long logsize   = -2; // must be set to a number from -1 to 20 inclusive, on command line.

// ENVDIR is defined in the Makefile
// And can be overridden by -e
static const char *envdir = ENVDIR;

static void
init(void) {
    int r;
    char rm_cmd[strlen("rm -rf ") + strlen(envdir) + 1];
    sprintf(rm_cmd, "rm -rf %s", envdir);
    r = system(rm_cmd);                                                                                       CKERR(r);
    r=toku_os_mkdir(envdir, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=db_env_create(&env, 0); CKERR(r);
    r=env->open(env, envdir, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_PRIVATE|DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);
    r=db->open(db, null_txn, "foo.db", 0, DB_BTREE, DB_CREATE|DB_EXCL, S_IRWXU|S_IRWXG|S_IRWXO);
    CKERR(r);
    r=db->close(db, 0); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);
    r=db->open(db, null_txn, "foo.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO);
    CKERR(r);
    r=env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r=db->pre_acquire_table_lock(db, txn); CKERR(r);
}

static void
tear_down(void) {
    int r;
    r = db->close(db, 0); CKERR(r);
    r = env->close(env, 0); CKERR(r);
}

static void
abort_txn(void) {
    find_num = 0;
    int r = txn->abort(txn); CKERR(r);
    txn = NULL;
}

static void
put(uint32_t k, uint32_t v) {
    int r;
    DBT key,val;

    dbt_init(&key, &k, sizeof(k));
    dbt_init(&val, &v, sizeof(v));
    r = db->put(db, txn, &key, &val, 0); CKERR(r);
}

static void
test_insert_and_abort(uint32_t num_to_insert) {
    find_num = 0;
    
    uint32_t k;
    uint32_t v;

    uint32_t i;
    for (i=0; i < num_to_insert; i++) {
        k = htonl(i);
        v = htonl(i+num_to_insert);
        put(k, v);
    }
    abort_txn();
}

static void
test_insert_and_abort_and_insert(uint32_t num_to_insert) {
    test_insert_and_abort(num_to_insert); 
    find_num = num_to_insert / 2;
    uint32_t k, v;
    uint32_t i;
    int r;
    r=env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r=db->pre_acquire_table_lock(db, txn); CKERR(r);
    for (i=0; i < find_num; i++) {
        k = htonl(i);
        v = htonl(i+5);
        put(k, v);
    }
    txn->commit(txn, 0);
    txn = NULL;
}

#define bit0 (1<<0)
#define bit1 (1<<1)

static int
do_nothing(DBT const *UU(a), DBT  const *UU(b), void *UU(c)) {
    return 0;
}

static void
verify_and_tear_down(int close_first) {
    int r;
    {
        char *filename;
#if USE_TDB
        {
            DBT dname;
            DBT iname;
            dbt_init(&dname, "foo.db", sizeof("foo.db"));
            dbt_init(&iname, NULL, 0);
            iname.flags |= DB_DBT_MALLOC;
            r = env->get_iname(env, &dname, &iname);
            CKERR(r);
            CAST_FROM_VOIDP(filename, iname.data);
            assert(filename);
        }
#else
        filename = toku_xstrdup("foo.db");
#endif
	toku_struct_stat statbuf;
	int size = strlen(filename) + strlen(envdir) + sizeof("/");
        char fullfile[size];
        int sp = snprintf(fullfile, size, "%s/%s", envdir, filename);
	assert(sp<size);
        toku_free(filename);
	r = toku_stat(fullfile, &statbuf);
	assert(r==0);
    }
    CKERR(r);
    if (close_first) {
        r=db->close(db, 0); CKERR(r);
        r=db_create(&db, env, 0); CKERR(r);
        r=db->open(db, null_txn, "foo.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO);
        CKERR(r);
    }
    DBC *cursor;
    r=env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r = db->cursor(db, txn, &cursor, 0); CKERR(r);
    uint32_t found = 0;
    do {
        r = cursor->c_getf_next(cursor, 0, do_nothing, NULL);
        if (r==0) found++;
    } while (r==0);
    CKERR2(r, DB_NOTFOUND);
    cursor->c_close(cursor);
    txn->commit(txn, 0);
    assert(found==find_num);
    tear_down();
}

static void
runtests(void) {
    int close_first = closemode;
    if (logsize == -1) {
        init();
        abort_txn();
        verify_and_tear_down(close_first);
    } else {
	uint32_t n = 1<<logsize;
	{
            if (verbose) {
                printf("\t%s:%d-%s() close_first=%d n=%06x\n",
                       __FILE__, __LINE__, __FUNCTION__, close_first, n);
                fflush(stdout);
            }
            init();
            test_insert_and_abort(n);
            verify_and_tear_down(close_first);

            init();
            test_insert_and_abort_and_insert(n);
            verify_and_tear_down(close_first);
        }
    }
}

static long parseint (const char *str) {
    errno = 0;
    char *end;
    long v = strtol(str, &end, 10);
    assert(errno==0 && *end==0);
    return v;
}

static void
parse_my_args (int argc, char * const argv[]) {
    const char *argv0=argv[0];
    while (argc>1) {
	int resultcode=0;
	if (strcmp(argv[1], "-v")==0) {
	    verbose++;
	} else if (strcmp(argv[1],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
        } else if (strcmp(argv[1],"-e") == 0 && argc > 2) {
            argc--; argv++;
            envdir = argv[1];
	} else if (strcmp(argv[1],"-c") == 0 && argc > 2) {
	    argc--; argv++;
	    closemode = parseint(argv[1]);
	} else if (strcmp(argv[1],"-l") == 0 && argc > 2) {
	    argc--; argv++;
	    logsize = parseint(argv[1]);
	} else if (strcmp(argv[1], "-h")==0) {
	do_usage:
	    fprintf(stderr, "Usage:\n%s [-v|-q] [-h] [-e <envdir>] -c <closemode (0 or 1)> -l <log of size, -1, or 0 through 20>\n", argv0);
	    exit(resultcode);
	} else {
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
    assert(closemode==0 || closemode==1);
    assert(logsize >= -1 && logsize <=20);
}

int
test_main(int argc, char *const argv[]) {
    parse_my_args(argc, argv);

    runtests();
    return 0;
}

