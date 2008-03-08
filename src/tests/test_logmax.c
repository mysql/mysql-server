/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <db.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "test.h"

void check_logmax (int max) {
    int any_too_big=0;
    DIR *dir = opendir(ENVDIR);
    struct dirent *ent;
    while ((ent=readdir(dir))) {
	if (ent->d_type==DT_REG && strncmp(ent->d_name, "log", 3)==0) {
	    // It is a "log*" file
#define FULL_LEN (sizeof(ENVDIR)+NAME_MAX+1)
	    char full_fname[FULL_LEN];
	    snprintf(full_fname, FULL_LEN, "%s/%s", ENVDIR, ent->d_name); 
	    struct stat sbuf;
	    int r = stat(full_fname, &sbuf);
	    assert(r==0);
	    if (verbose)
		printf("%s is of size %ld\n", ent->d_name, sbuf.st_size);
	    if (sbuf.st_size > max) any_too_big=1;
	}
    }
    assert(!any_too_big);
}

void test_logmax (int logmax) {
    int r;
    DB_ENV *env;
    DB *db;
    DB_TXN *tid;

    system("rm -rf " ENVDIR);
    r=mkdir(ENVDIR, 0777);       assert(r==0);
    r=db_env_create(&env, 0); assert(r==0);
    if (logmax>0) {
	r=env->set_lg_max(env, logmax);
	assert(r==0);
    }
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, 0777); CKERR(r);
    {
	u_int32_t lmax;
	r=env->get_lg_max(env, &lmax);
	assert(r==0);
	if (logmax>0) {
	    assert(lmax==logmax);
	} else {
	    assert(lmax>0);
	    
	}
    }
    r=db_create(&db, env, 0); CKERR(r);
    r=env->txn_begin(env, 0, &tid, 0); assert(r==0);
    r=db->open(db, tid, "foo.db", 0, DB_BTREE, DB_CREATE, 0777); CKERR(r);
    r=tid->commit(tid, 0);    assert(r==0);

    int i;
    int sum = 0;
    int effective_max;
    if (logmax>0) effective_max = logmax;
    else {
#ifdef TOKUDB
	effective_max = 100<<20;
#else
	effective_max = 10<<20;
#endif
    }

    r=env->txn_begin(env, 0, &tid, 0); CKERR(r);
    char there[1000];
    memset(there, 'a',sizeof(there));
    there[999]=0;
    for (i=0; sum<(effective_max*3)/2; i++) {
	DBT key,data;
	char hello[20];
	snprintf(hello, 20, "hello%d", i);
	r=db->put(db, tid,
		  dbt_init(&key, hello, strlen(hello)+1),
		  dbt_init(&data, there, sizeof(there)),
		  0);
	assert(r==0);
	sum+=strlen(hello)+1+sizeof(there);
	if ((i+1)%10==0) {
	    r=tid->commit(tid, 0); assert(r==0);
	    r=env->txn_begin(env, 0, &tid, 0); CKERR(r);
	}
    }
    if (verbose) printf("i=%d sum=%d effmax=%d\n", i, sum, effective_max);
    r=tid->commit(tid, 0); assert(r==0);
    r=db->close(db, 0); assert(r==0);
    r=env->close(env, 0); assert(r==0);
    check_logmax(effective_max);
}

int main (int argc, const char *argv[]) {
    parse_args(argc, argv);
    test_logmax(1<<20);
    test_logmax(-1);
    return 0;
}
