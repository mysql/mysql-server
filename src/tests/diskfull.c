/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"
/* Simulate disk full by making pwrite return ENOSPC */
/* Strategy, repeatedly run a test, and on the Ith run of the test  make the Ith write fail. */

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#define DOERR(r) do { if (r!=0) { did_fail=1; fprintf(error_file, "%s:%d error %d (%s)\n", __FILE__, __LINE__, r, db_strerror(r)); }} while (0)

static void
do_db_work(void) {
    int r;
    int did_fail=0;
    {

	r=system("rm -rf " ENVDIR); CKERR(r);
	r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);                          assert(r==0);

	FILE *error_file = 0;
	if (verbose==0) {
	    error_file = fopen(ENVDIR "/stderr", "w");                             assert(error_file);
	}
        else error_file = stderr;

	DB_ENV *env;
	DB_TXN *tid;
	DB     *db;
	DBT key,data;

	r=db_env_create(&env, 0);                                                  assert(r==0);
	r = env->set_redzone(env, 0);    CKERR(r);
	env->set_errfile(env, error_file ? error_file : stderr);
	// Don't set the lg bsize for the small experiment.
	r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE|DB_THREAD, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
	r=db_create(&db, env, 0);                                                  CKERR(r);
	r=env->txn_begin(env, 0, &tid, 0);                                         assert(r==0);
	r=db->open(db, tid, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO);  DOERR(r);
	if (did_fail) {
	    r=tid->abort(tid);                                                     CKERR(r);
	} else {
	    r=tid->commit(tid, 0);                                                 DOERR(r);
	}
	if (did_fail) goto shutdown1;

	r=env->txn_begin(env, 0, &tid, 0);                                         assert(r==0);
	r=db->put(db, tid, dbt_init(&key, "a", 2), dbt_init(&data, "b", 2), 0);    DOERR(r);
	if (did_fail) {
	    r = tid->abort(tid);                                                   CKERR2s(r, 0, ENOSPC);
	} else {
	    r=tid->commit(tid, 0);                                                 DOERR(r);
	}

    shutdown1:
	r=db->close(db, 0);                                                        DOERR(r);
	r=env->close(env, 0);                                                      DOERR(r);
	if (error_file && error_file!=stderr) fclose(error_file);
	if (did_fail) return;
    }
    {
	r=system("rm -rf " ENVDIR); CKERR(r);
	r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);                          assert(r==0);

	FILE *error_file = 0;
	if (verbose==0) {
	    error_file = fopen(ENVDIR "/stderr", "w");                             assert(error_file);
	}
        else error_file = stderr;

	DB_ENV *env;
	DB_TXN *tid;
	DB     *db;
	DBT key,data;

	// Repeat with more put operations 
	r=db_env_create(&env, 0);                                                  assert(r==0);
	r = env->set_redzone(env, 0);    CKERR(r);
	env->set_errfile(env, error_file ? error_file : stderr);
	r=env->set_lg_bsize(env, 4096);                                            assert(r==0);
	r=env->set_cachesize(env, 0, 1, 1);                                        assert(r==0);
	r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE|DB_THREAD, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
	r=db_create(&db, env, 0);                                                  CKERR(r);
	r=db->set_pagesize(db, 4096);
	r=env->txn_begin(env, 0, &tid, 0);                                         assert(r==0);
	r=db->open(db, tid, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); DOERR(r);
	if (did_fail) {
	    r = tid->abort(tid);                                                   CKERR2s(r, 0, ENOSPC);
	} else {
	    r=tid->commit(tid, 0);                                                 DOERR(r);
	}
	if (did_fail) goto shutdown2;

	// Put an extra item in
	r=env->txn_begin(env, 0, &tid, 0);                                         assert(r==0);
	r=db->put(db, tid, dbt_init(&key, "a", 2), dbt_init(&data, "b", 2), 0);    DOERR(r);
	if (did_fail) {
	    r=tid->abort(tid);                                                     CKERR(r);
	} else {
	    r=tid->commit(tid, 0);                                                 DOERR(r);
	}
	if (did_fail) goto shutdown2;

	r=env->txn_begin(env, 0, &tid, 0);                                         assert(r==0);
	{
	    int i;
	    for (i=0; i<100; i++) {
		int kvsize=50;
		int kvsize_i = kvsize / sizeof(int);
		int keyi[kvsize_i],vali[kvsize_i];
		int j;
		keyi[0] = vali[0] = toku_htonl(i);
		for (j=1; j<kvsize_i; j++) {
		    keyi[j] = random();
		    vali[j] = random();
		}
		r=db->put(db, tid, dbt_init(&key, keyi, sizeof keyi), dbt_init(&data, vali, sizeof vali), 0);
		DOERR(r);
		if (did_fail) goto break_out_of_loop;
	    }
	}
    break_out_of_loop:
	//system("ls -l " ENVDIR);
	if (did_fail) {
	    r = tid->abort(tid);                                                   CKERR2s(r, 0, ENOSPC);
	} else {
	    r=tid->commit(tid, 0);                                                 DOERR(r);
	}
    shutdown2:
	r=db->close(db, 0);                                                        DOERR(r);
	r=env->close(env, 0);                                                      DOERR(r);
	if (error_file && error_file!=stderr) fclose(error_file);
    }
}

static int write_count = 0;
#define FAIL_NEVER 0x7FFFFFFF
static int fail_at = FAIL_NEVER;

static ssize_t
pwrite_counting_and_failing (int fd, const void *buf, size_t size, toku_off_t off)
{
    write_count++;
    if (write_count>fail_at) {
        if (verbose>1) { printf("Failure imminent at %d:\n", fail_at); fflush(stdout); }
	errno = ENOSPC;
	return -1;
    } else {
	return pwrite(fd, buf, size, off);
    }
}

static ssize_t
write_counting_and_failing (int fd, const void *buf, size_t size)
{
    write_count++;
    if (write_count>fail_at) {
        if (verbose>1) { printf("Failure imminent at %d:\n", fail_at); fflush(stdout); }
	errno = ENOSPC;
	return -1;
    } else {
	return write(fd, buf, size);
    }
}

static void
do_writes_that_fail (void) {
    if (verbose) { printf("About to fail at %d:\n", fail_at); fflush(stdout); }
    toku_set_assert_on_write_enospc(TRUE);
    db_env_set_func_pwrite(pwrite_counting_and_failing);
    db_env_set_func_full_pwrite(pwrite_counting_and_failing);
    db_env_set_func_write (write_counting_and_failing);
    db_env_set_func_full_write (write_counting_and_failing);
    write_count=0;
    do_db_work();
    if (fail_at != FAIL_NEVER && write_count <= fail_at) {
	abort(); // if we don't encounter the write (because there are not enough), then in fail_at mode, we need to abort so that the test will be happy.
    }
    printf("%d", write_count);
}

static void
diskfull_parse_args (int argc, char * const argv[]) {
    int c;
    char *argv0 = argv[0];
    while ((c = getopt(argc, (char * const *)argv, "cC:vq")) != -1) {
	switch(c) {
        case 'C':
            fail_at =  atoi(optarg);
	    break;
	case 'v':
	    verbose++;
            break;
	case 'q':
	    verbose--;
	    if (verbose<0) verbose=0;
            break;
	default:
do_usage:
	    fprintf(stderr, "Usage:\n%s [-v|-q] [-C number]\n", argv0);
	    exit(1);
	}
    }
    if (argc!=optind) {
        goto do_usage;
    }
}

int
test_main (int argc, char * const argv[]) {
    diskfull_parse_args(argc, argv);
    do_writes_that_fail();
    return 0;
}
