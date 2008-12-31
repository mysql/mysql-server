/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"

#include <db.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

// Return the offset
static int
grep_for_in_logs (const char *str) {
#ifdef TOKUDB
#define lname ENVDIR "//log000000000000.tokulog"
#else
#define lname ENVDIR "//log.0000000001"
#endif
    int fd = open(lname, O_RDONLY);
    assert(fd>=0);
    int64_t file_size;
    int r = toku_os_get_file_size(fd, &file_size);
    assert(r==0);
    void *addr_v = mmap(0, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    assert(addr_v!=MAP_FAILED);
    char *fstr = addr_v;
    int   searchlen=strlen(str);
    int i;
    for (i=0; i+searchlen<file_size; i++) {
	if (memcmp(str, fstr+i, searchlen)==0) {
	    return i;
	}
    }
    return -1;
}

int
test_main (int UU(argc), const char UU(*argv[])) {
    int r;
    DB_ENV *env;
    DB *db;
    DB_TXN *tid;

    system("rm -rf " ENVDIR);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);       assert(r==0);
    r=db_env_create(&env, 0); assert(r==0);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);
    r=env->txn_begin(env, 0, &tid, 0); assert(r==0);
    r=db->open(db, tid, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=tid->commit(tid, 0);    assert(r==0);

    {
	DBT key,data;
	char hello[]="hello";
	char there[]="there";
	r=env->txn_begin(env, 0, &tid, 0); CKERR(r);
	r=db->put(db, tid,
		  dbt_init(&key, hello, sizeof(hello)),
		  dbt_init(&data, there, sizeof(there)),
		  0);
	r=grep_for_in_logs(hello);
	assert(r==-1);
	r=env->log_flush(env, 0); CKERR(r);
	r=grep_for_in_logs(hello);
	assert(r>=0);
	r=tid->commit(tid, 0);    CKERR(r);
    }
    r=db->close(db, 0); assert(r==0);
    r=env->close(env, 0); assert(r==0);
    return 0;
}
