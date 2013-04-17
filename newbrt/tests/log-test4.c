/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "includes.h"

#define dname __FILE__ ".dir"
#define rmrf "rm -rf " dname "/"

// create and close, making sure that everything is deallocated properly.

int main (int argc __attribute__((__unused__)),
	  char *argv[] __attribute__((__unused__))) {
    int r;
    system(rmrf);
    r = toku_os_mkdir(dname, S_IRWXU);    assert(r==0);
    TOKULOGGER logger;
    r = toku_logger_create(&logger);
    assert(r == 0);
    r = toku_logger_open(dname, logger);
    assert(r == 0);
    {
	struct logbytes *b = MALLOC_LOGBYTES(5);
	b->nbytes=5;
	memcpy(b->bytes, "a1234", 5);
	b->lsn=(LSN){0};
	r = ml_lock(&logger->input_lock);
	assert(r==0);
	r = toku_logger_log_bytes(logger, b, 0);
	assert(r==0);
	assert(logger->input_lock.is_locked==0);
    }
    r = toku_logger_close(&logger);
    assert(r == 0);
    {
	struct stat statbuf;
	r = stat(dname "/log000000000000.tokulog", &statbuf);
	assert(r==0);
	assert(statbuf.st_size==12+5);
    }
    return 0;
}
