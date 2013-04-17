/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <toku_portability.h>
#include <db.h>

#include "test.h"

static DBT *
dbt_init_user (DBT *d, void *uptr, int ulen) {
    memset(d, 0, sizeof *d);
    d->data = uptr;
    d->ulen = ulen;
    d->flags = DB_DBT_USERMEM;
    return d;
}

static void
db_put (DB *db, int k, int v) {
    DB_TXN * const null_txn = 0;
    DBT key, val;
    int r = db->put(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), DB_YESOVERWRITE);
    assert(r == 0);
}

static char annotated_envdir[]= ENVDIR "           "; 

static void test_get_both(int n, int dup_mode, int op) {
    if (verbose) printf("test_get_both_range:%d %d %d\n", n, dup_mode, op);

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    char fname[sizeof(ENVDIR)+100];
    snprintf(fname, sizeof(fname), "%s/test_icdi_search_brt", annotated_envdir);
    int r;

    unlink(fname);

    /* create the dup database file */
    r = db_create(&db, null_env, 0); assert(r == 0);
    r = db->set_flags(db, dup_mode); assert(r == 0);
    r = db->set_pagesize(db, 4096); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    /* insert n unique kv pairs */
    int i;
    for (i=0; i<n; i++) {
        int k = htonl(10*i);
        int v = htonl(0);
        db_put(db, k, v);
    } 

    if (dup_mode) {
        /* insert a bunch of duplicate kv pairs */
        for (i=1; i<n; i++) {
            int k = htonl(10*(n/2));
            int v = htonl(10*i);
            db_put(db, k, v);
        }
    }

    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0); assert(r == 0);
    for (i=0; i<10*n; i++) {
        int k = htonl(i);
        int j;
        for (j=0; j<10*n; j++) {
            int v = htonl(j);
            DBT key, val;
            r = cursor->c_get(cursor, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), op);
            if (r == 0) {
                assert((i % 10) == 0);
                int kk, vv;
                r = cursor->c_get(cursor, dbt_init_user(&key, &kk, sizeof kk), dbt_init_user(&val, &vv, sizeof vv), DB_CURRENT);
                assert(r == 0);
                assert(key.size == sizeof kk);
                kk = htonl(kk);
                assert(val.size == sizeof vv);
                vv = htonl(vv);
                if (verbose > 1) printf("%d %d -> %d %d\n", i, j, kk, vv);
                assert(kk == i);
                assert(vv == ((j+9)/10)*10);
            } else if (r == DB_NOTFOUND) {
                if ((i%10) != 0 || j > 0)
                    ;
                else
                    printf("nf %d %d\n", i, j);
            } else
                assert(0);
        }
    }
    r = cursor->c_close(cursor); assert(r == 0);

    r = db->close(db, 0); assert(r == 0);
}


int
test_main(int argc, const char *argv[]) {
    unsigned long doi=0;
    int i;
    char flags = 0;
    for (i=1; i<argc; i++) {
        const char *arg = argv[i];
        if (0 == strcmp(arg, "-v")) {
            verbose++;
	} else if (0 == strcmp(arg, "-q")) {
            verbose--;
	    if (verbose<0) verbose=0;
        } else if (0==strcmp(arg, "-i")) {
	    i++; assert(i<argc);
	    char *end; 
	    doi = strtoul(argv[i], &end, 10);
	    assert(doi!=LONG_MAX);
	    assert(end!=argv[i]);
	    assert(*end==0);
	} else if (0==strcmp(arg, "-a")) {
	    flags = 'a';
	} else if (0==strcmp(arg, "-b")) {
	    flags = 'b';
	} else if (0==strcmp(arg, "-c")) {
	    flags = 'c';
	} else {
	    fprintf(stderr, "Usage: %s [-v] [-a|-b|-c] [-i I]\n", argv[0]);
	    exit(1);
	}
    }
  
    {
	char envdir_without_suffix[sizeof(ENVDIR)] = ENVDIR;
	assert(sizeof(ENVDIR)>4);
	//printf("envdir=%s\n(size=%ld) -5 char=%c\n", ENVDIR, sizeof(ENVDIR), envdir_without_suffix[sizeof(ENVDIR)-5]);
	assert(envdir_without_suffix[sizeof(ENVDIR)-5]=='.');
	assert(envdir_without_suffix[sizeof(ENVDIR)-4]=='t' || envdir_without_suffix[sizeof(ENVDIR)-4]=='b');
	assert(envdir_without_suffix[sizeof(ENVDIR)-3]=='d');
	assert(envdir_without_suffix[sizeof(ENVDIR)-2]=='b');
	assert(envdir_without_suffix[sizeof(ENVDIR)-1]==0);
	envdir_without_suffix[sizeof(ENVDIR)-5]=0;

	char doi_string[10];
	if (doi==0) doi_string[0]=0;
	else snprintf(doi_string, sizeof(doi_string), ".%lu", doi);
	
	char flags_string[10];
	switch (flags) {
	case 0:   flags_string[0]=0; break;
	case 'a': case 'b': case 'c': snprintf(flags_string, sizeof(flags_string), ".%c", flags); break;
	default: assert(0);
	}
	
	#ifdef USE_TDB
	char bdb_tdb_char='t';
	#else
	char bdb_tdb_char='b';
	#endif

	snprintf(annotated_envdir, sizeof(annotated_envdir), "%s%s%s.%cdb",
		 envdir_without_suffix, doi_string, flags_string, bdb_tdb_char);
    }
    {
	char rmcmd[sizeof(annotated_envdir)+10];
	snprintf(rmcmd, sizeof(rmcmd), "rm -rf %s", annotated_envdir);
	system(rmcmd);
    }
    toku_os_mkdir(annotated_envdir, S_IRWXU+S_IRWXG+S_IRWXO);

    if (doi==0) { 
	for (i=1; i <= 256; i *= 2) {
	    if (flags==0 || flags=='a')  test_get_both(i, 0, DB_GET_BOTH);
	    if (flags==0 || flags=='b')  test_get_both(i, 0, DB_GET_BOTH_RANGE);
	    if (flags==0 || flags=='c')  test_get_both(i, DB_DUP + DB_DUPSORT, DB_GET_BOTH_RANGE);
	}
    } else {
	if (flags==0 || flags=='a')  test_get_both(doi, 0, DB_GET_BOTH);
	if (flags==0 || flags=='b')  test_get_both(doi, 0, DB_GET_BOTH_RANGE);
	if (flags==0 || flags=='c')  test_get_both(doi, DB_DUP + DB_DUPSORT, DB_GET_BOTH_RANGE);
    }

    return 0;
}
