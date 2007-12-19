/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

// Try to open an environment where the directory does not exist 
// Try when the dir exists but is not an initialized env
// Try when the dir exists and we do DB_CREATE: it should work.
// And after that the open should work without a DB_CREATE
//   However, in BDB, after doing an DB_ENV->open and then a close, no state has changed
//   One must actually create a DB I think...

#include <assert.h>
#include <db.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <unistd.h>

// DIR is defined in the Makefile
#define CKERR(r) if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, db_strerror(r)); assert(r==0);

int main() {
    DB *db;
    DB_ENV *dbenv;
    int r;
    int do_private;

    for (do_private=0; do_private<2; do_private++) {
#ifdef USE_TDB
	if (do_private==0) continue; // See #208.
#endif
	int private_flags = do_private ? DB_PRIVATE : 0;
	
	system("rm -rf " DIR);
	r = db_env_create(&dbenv, 0);
	CKERR(r);
	r = dbenv->open(dbenv, DIR, private_flags|DB_INIT_MPOOL, 0);
	assert(r==ENOENT);
	dbenv->close(dbenv,0); // free memory
	
	system("rm -rf " DIR);
	mkdir(DIR, 0777);
	r = db_env_create(&dbenv, 0);
	CKERR(r);
	r = dbenv->open(dbenv, DIR, private_flags|DB_INIT_MPOOL, 0);
	if (r!=ENOENT) printf("%s:%d %d: %s\n", __FILE__, __LINE__, r,db_strerror(r));
	assert(r==ENOENT);
	dbenv->close(dbenv,0); // free memory
    }

    // Now make sure that if we have a non-private DB that we can tell if it opened or not.
    system("rm -rf " DIR);
    mkdir(DIR, 0777);
    r = db_env_create(&dbenv, 0);
    CKERR(r);
    r = dbenv->open(dbenv, DIR, DB_CREATE|DB_INIT_MPOOL, 0);
    CKERR(r);
    r=db_create(&db, dbenv, 0);
    CKERR(r);
    dbenv->close(dbenv,0); // free memory
    write(1,"d",1);
    r = db_env_create(&dbenv, 0);
    CKERR(r);
    r = dbenv->open(dbenv, DIR, DB_INIT_MPOOL, 0);
    CKERR(r);
    dbenv->close(dbenv,0); // free memory
    return 0;
}

