/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* Do we open directories with same priority as BDB? i.e. with home, without home, with DB_USE_ENVIRON/etc.. */
#include <limits.h>
#include <stdio.h>
#include <assert.h>
#include <db.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define CKERR(r) \
do {                                                                                                        \
    if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, db_strerror(r)); assert(r==0);  \
} while (0)

// DIR is defined in the Makefile

#define DB_HOME "DB_HOME"
#define DBNAME "test.db"

int rootfd;
char* db_dir;
char db_name[PATH_MAX];
int extra_flags;
char* home;


void reinit_config(int set_home, int set_DB_ENVIRON, int set_DB_HOME) {
    int r = 0;
    //Return to base dir
    r = fchdir(rootfd);                 assert(r == 0);

    r = system("rm -rf " DIR);          assert(r == 0);
    r = mkdir(DIR, 0777);               assert(r == 0);
    r = chdir(DIR);                     assert(r == 0);
    unsetenv(DB_HOME);

    if (set_home) {
        db_dir = "home";
        r = mkdir(db_dir, 0777);        assert(r == 0);
    }
    else if (set_DB_ENVIRON && set_DB_HOME) {
        db_dir = "DB_HOME";
        r = mkdir(db_dir, 0777);        assert(r == 0);
    }
    else db_dir = ".";
    
    if (set_home) home = "home"; else home = NULL;
    if (set_DB_ENVIRON) extra_flags = DB_USE_ENVIRON; else extra_flags = 0;
    if (set_DB_HOME) {r = setenv(DB_HOME, DB_HOME, 1);assert(r == 0);}

}

int main() {
    DB_ENV *env;
    DB_TXN * const null_txn = 0;
    DB *db;
    int r;
    int i;
    
    rootfd = open(".", O_RDONLY, 0); assert(rootfd >= 0);
    
    for (i = 0; i < 8; i++) {
        int set_home = i & 0x1;
        int set_DB_ENVIRON = i & 0x2;
        int set_DB_HOME = i & 0x4;
        
        reinit_config(set_home, set_DB_ENVIRON, set_DB_HOME);
        r = snprintf(db_name, sizeof(db_name), "%s/%s", db_dir, DBNAME);
        assert(r < sizeof(db_name));
        assert(r >= 0);
        
        r = db_env_create(&env, 0);
        CKERR(r);
        
        r = env->open(env, home, DB_CREATE|DB_PRIVATE|DB_INIT_MPOOL|extra_flags, 0);
// BDB does not check for this. TDB does more error checking.
#ifdef USE_TDB
        if (set_home && set_DB_ENVIRON) {
            assert(r == EINVAL);
            goto cleanup;
        }
        else
#endif
            CKERR(r);
        
        
        r = db_create(&db, env, 0);
        CKERR(r);

        r = db->open(db, null_txn, DBNAME, NULL, DB_BTREE, DB_CREATE, 0666);
        CKERR(r);

        r = db->close(db, 0);
        CKERR(r);

        //Verify it went in the right directory.
    	{
        	struct stat buf;
        	r = stat(db_name, &buf);
        	CKERR(r);
        }
#ifdef USE_TDB
cleanup:
#endif
        r = env->close(env, 0);
        CKERR(r);        
    }

    r = close(rootfd); assert(r == 0);
    return 0;
}
