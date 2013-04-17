/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "test.h"

#include "includes.h"

#define dname __FILE__ ".dir"
#define rmrf "rm -rf " dname "/"

// create and close, making sure that everything is deallocated properly.

#define LSIZE 100

int
test_main (int argc __attribute__((__unused__)),
	  const char *argv[] __attribute__((__unused__))) {
    int r;
    r = system(rmrf);
    CKERR(r);
    r = toku_os_mkdir(dname, S_IRWXU);    assert(r==0);
    TOKULOGGER logger;
    r = toku_logger_create(&logger);
    assert(r == 0);
    r = toku_logger_set_lg_max(logger, LSIZE);
    {
	u_int32_t n;
	r = toku_logger_get_lg_max(logger, &n);
	assert(n==LSIZE);
    }
    r = toku_logger_open(dname, logger);
    assert(r == 0);
    int i;
    for (i=0; i<1000; i++) {
	r = ml_lock(&logger->input_lock);                                                    assert(r==0);

	int ilen=3+random()%5;
	r = toku_logger_make_space_in_inbuf(logger, ilen+1);                                 assert(r==0);
	snprintf(logger->inbuf.buf+logger->inbuf.n_in_buf, ilen+1, "a%0*d ", (int)ilen, i);
	logger->inbuf.n_in_buf+=(ilen+1);
	logger->lsn.lsn++;
	logger->inbuf.max_lsn_in_buf = logger->lsn;
	r = ml_unlock(&logger->input_lock);                                                  assert(r == 0);
	r = toku_logger_fsync(logger);                                                      assert(r == 0);
    }
    r = toku_logger_close(&logger);
    assert(r == 0);

    {
	DIR *dir=opendir(dname);
	assert(dir);
	struct dirent *dirent;
	while ((dirent=readdir(dir))) {
	    if (strncmp(dirent->d_name, "log", 3)!=0) continue;
	    char fname[sizeof(dname)+256+1];
	    snprintf(fname, sizeof(fname), "%s/%s", dname, dirent->d_name);
	    toku_struct_stat statbuf;
	    r = toku_stat(fname, &statbuf);
	    assert(r==0);
	    assert(statbuf.st_size<=LSIZE+10);
	}
	r = closedir(dir);
	assert(r==0);
    }
    return 0;
}
