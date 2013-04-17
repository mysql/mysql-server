/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "test.h"

#include "includes.h"

#define dname __FILE__ ".dir"
#define rmrf "rm -rf " dname "_*/"

// create and close, making sure that everything is deallocated properly.

#define LSIZE 100
#define NUM_LOGGERS 10
TOKULOGGER logger[NUM_LOGGERS];

void setup_logger(int which) {
    char dnamewhich[200];
    int r;
    sprintf(dnamewhich, "%s_%d", dname, which);
    r = toku_os_mkdir(dnamewhich, S_IRWXU);    assert(r==0);
    r = toku_logger_create(&logger[which]);
    assert(r == 0);
    r = toku_logger_set_lg_max(logger[which], LSIZE);
    {
	u_int32_t n;
	r = toku_logger_get_lg_max(logger[which], &n);
	assert(n==LSIZE);
    }
    r = toku_logger_open(dnamewhich, logger[which]);
    assert(r == 0);
}

void play_with_logger(int which) {
    int r;
    {
	r = ml_lock(&logger[which]->input_lock);
	assert(r==0);

	int lsize=LSIZE-12-2;
	struct logbytes *b = MALLOC_LOGBYTES(lsize);
	b->nbytes=lsize;
	snprintf(b->bytes, lsize, "a%*d", LSIZE-12-2, 0);
	b->lsn=(LSN){23};
	r = toku_logger_log_bytes(logger[which], b, 0);
	assert(r==0);
    }

    {
	r = ml_lock(&logger[which]->input_lock);
	assert(r==0);

	struct logbytes *b = MALLOC_LOGBYTES(2);
	b->lsn=(LSN){24};
	b->nbytes=2;
	memcpy(b->bytes, "b1", 2);
	r = toku_logger_log_bytes(logger[which], b, 0);
	assert(r==0);
    }
}

void tear_down_logger(int which) {
    int r;
    r = toku_logger_close(&logger[which]);
    assert(r == 0);
}

int
test_main (int argc __attribute__((__unused__)),
	  const char *argv[] __attribute__((__unused__))) {
    int i;
    int loop;
    const int numloops = 100;
    for (loop = 0; loop < numloops; loop++) {
        system(rmrf);
        for (i = 0; i < NUM_LOGGERS; i++) setup_logger(i);
        for (i = 0; i < NUM_LOGGERS; i++) play_with_logger(i);
        for (i = 0; i < NUM_LOGGERS; i++) tear_down_logger(i);
    }
    system(rmrf);

    return 0;
}
