/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "log-internal.h"
#include "toku_assert.h"
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define dname __FILE__ ".dir"
#define rmrf "rm -rf " dname "/"

// create and close, making sure that everything is deallocated properly.

#define LSIZE 100

int main (int argc __attribute__((__unused__)),
	  char *argv[] __attribute__((__unused__))) {
    int r;
    system(rmrf);
    r = mkdir(dname, 0700);    assert(r==0);
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
	r = ml_lock(&logger->input_lock);
	assert(r==0);

	int lsize=LSIZE-12-2;
	struct logbytes *b = MALLOC_LOGBYTES(lsize);
	b->nbytes=lsize;
	snprintf(b->bytes, lsize, "a%*d", LSIZE-12-2, 0);
	b->lsn=(LSN){23};
	r = toku_logger_log_bytes(logger, b, 0);
	assert(r==0);
    }

    {
	r = ml_lock(&logger->input_lock);
	assert(r==0);

	struct logbytes *b = MALLOC_LOGBYTES(2);
	b->lsn=(LSN){24};
	b->nbytes=2;
	memcpy(b->bytes, "b1", 2);
	r = toku_logger_log_bytes(logger, b, 0);
	assert(r==0);
    }

    r = toku_logger_close(&logger);
    assert(r == 0);

    {
	struct stat statbuf;
	r = stat(dname "/log000000000000.tokulog", &statbuf);
	assert(r==0);
	assert(statbuf.st_size<=LSIZE);
    }
    return 0;
}
