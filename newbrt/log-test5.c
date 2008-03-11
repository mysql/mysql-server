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
    int i;
    for (i=0; i<20; i++) {

	r = ml_lock(&logger->LSN_lock);
	assert(r==0);

	LSN lsn={23};
	char data[100];
	snprintf(data, sizeof(data), "a%04d", i);
	r = toku_logger_log_bytes(logger, strlen(data), data, lsn);
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
