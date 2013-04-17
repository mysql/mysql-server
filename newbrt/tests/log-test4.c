/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "test.h"

#include "includes.h"

#define dname __FILE__ ".dir"
#define rmrf "rm -rf " dname "/"

// create and close, making sure that everything is deallocated properly.

int
test_main (int argc __attribute__((__unused__)),
	  const char *argv[] __attribute__((__unused__))) {
    int r;
    char logname[PATH_MAX];
    r = system(rmrf);
    CKERR(r);
    r = toku_os_mkdir(dname, S_IRWXU);                               assert(r==0);
    TOKULOGGER logger;
    r = toku_logger_create(&logger);                                 assert(r == 0);
    r = toku_logger_open(dname, logger);                             assert(r == 0);

    {
	r = ml_lock(&logger->input_lock);                                assert(r == 0);
	r = toku_logger_make_space_in_inbuf(logger, 5);                  assert(r == 0);
	snprintf(logger->inbuf.buf+logger->inbuf.n_in_buf, 5, "a1234");
	logger->inbuf.n_in_buf+=5;
	logger->lsn.lsn++;
	logger->inbuf.max_lsn_in_buf = logger->lsn;
	r = ml_unlock(&logger->input_lock);                              assert(r == 0);
    }

    r = toku_logger_close(&logger);                                  assert(r == 0);
    {
	toku_struct_stat statbuf;
        sprintf(logname, dname "/log000000000000.tokulog%d", TOKU_LOG_VERSION);
	r = toku_stat(logname, &statbuf);
	assert(r==0);
	assert(statbuf.st_size==12+5);
    }
    r = system(rmrf);
    CKERR(r);
    return 0;
}
