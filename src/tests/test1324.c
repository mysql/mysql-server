/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"
/* Test for #1324. Make sure rolltmp files are removed. */
#include <db.h>
#include <fcntl.h>

#ifndef USE_TDB
#error This test only works for TokuDB.
#endif

static void mkfile (const char *fname) {
    mode_t mode = S_IRWXU|S_IRWXG|S_IRWXO;
    int fd = open(fname, O_WRONLY | O_CREAT | O_BINARY, mode); 
    if (fd<0) perror("opening");
    assert(fd>=0);
    toku_os_full_write(fd, "hello\n", 6);
    int r = close(fd);                                                        assert(r==0);
}

static void
do_1324 (int moreflags)
{
    const char fname[] = ENVDIR "/__tokudb_rolltmp.12345";
    const char fnamekeep[] = ENVDIR "/keepme";

    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    mkfile(fname);
    mkfile(fnamekeep);

    const int envflags = DB_CREATE|DB_INIT_MPOOL|DB_INIT_TXN|DB_INIT_LOCK |DB_THREAD |DB_PRIVATE | DB_RECOVER | moreflags;

    {
	DB_ENV *env;
	int r;

        if (moreflags & DB_INIT_LOG) {
            // create the log
            r = db_env_create(&env, 0);                                                     CKERR(r);
            r = env->open(env, ENVDIR, envflags & ~DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);    CKERR(r);
            r = env->close(env, 0);                                                         CKERR(r);
        }

	r = db_env_create(&env, 0);                                           CKERR(r);
	r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);        CKERR(r);

	{
	    toku_struct_stat sbuf;
	    r = toku_stat(fname, &sbuf);
	    if (r==0) {
		fprintf(stderr, "The rolltmp file %s should have been deleted, but was not.\n", fname);
	    }
	    assert(r!=0);
	}
	{
	    toku_struct_stat sbuf;
	    r = toku_stat(fnamekeep, &sbuf);
	    if (r!=0) {
		fprintf(stderr, "The keepme file %s should NOT have been deleted, but was not.\n", fnamekeep);
	    }
	    assert(r==0);
	}

	r = env->close(env, 0);                                               CKERR(r);
    }
    system("ls -l " ENVDIR);
    // make sure we can open the env again.
    {
	DB_ENV *env;
	int r;
	r = db_env_create(&env, 0);                                           CKERR(r);
	r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);        CKERR(r);
	r = env->close(env, 0);                                               CKERR(r);
    }
}

int
test_main (int argc, char *const argv[])
{
    parse_args(argc, argv);
    do_1324(DB_INIT_LOG);
    do_1324(0);
    return 0;
}
