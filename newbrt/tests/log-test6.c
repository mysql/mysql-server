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

    {
	r = ml_lock(&logger->input_lock);	                        assert(r==0);
	int lsize=LSIZE-12-2;
	r = toku_logger_make_space_in_inbuf(logger, lsize);           assert(r==0);
	snprintf(logger->inbuf.buf+logger->inbuf.n_in_buf, lsize, "a%*d", lsize-1, 0);
	logger->inbuf.n_in_buf += lsize;
	logger->lsn.lsn++;
	logger->inbuf.max_lsn_in_buf = logger->lsn;
	r = ml_unlock(&logger->input_lock);                              assert(r == 0);
    }

    {
	r = ml_lock(&logger->input_lock);                                assert(r==0);
	r = toku_logger_make_space_in_inbuf(logger, 2);                  assert(r==0);
	memcpy(logger->inbuf.buf+logger->inbuf.n_in_buf, "b1", 2);
	logger->inbuf.n_in_buf += 2;
	logger->lsn.lsn++;
	logger->inbuf.max_lsn_in_buf = logger->lsn;
	r = ml_unlock(&logger->input_lock);                              assert(r == 0);
    }

    r = toku_logger_close(&logger);
    assert(r == 0);

    {
        char logname[PATH_MAX];
	toku_struct_stat statbuf;
        sprintf(logname, dname "/log000000000000.tokulog%d", TOKU_LOG_VERSION);
	r = toku_stat(logname, &statbuf);
	assert(r==0);
	assert(statbuf.st_size<=LSIZE);
    }
    return 0;
}
