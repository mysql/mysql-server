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

static void setup_logger(int which) {
    char dnamewhich[200];
    int r;
    snprintf(dnamewhich, sizeof(dnamewhich), "%s_%d", dname, which);
    r = toku_os_mkdir(dnamewhich, S_IRWXU);    if (r!=0) printf("file %s error (%d) %s\n", dnamewhich, errno, strerror(errno)); assert(r==0);
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

static void play_with_logger(int which) {
    int r;
    {
	r = ml_lock(&logger[which]->input_lock);	                 assert(r==0);
	int lsize=LSIZE-12-2;
	r = toku_logger_make_space_in_inbuf(logger[which], lsize);       assert(r==0); 
	snprintf(logger[which]->inbuf.buf+logger[which]->inbuf.n_in_buf, lsize, "a%*d", lsize-1, 0);
	logger[which]->inbuf.n_in_buf += lsize;
	logger[which]->lsn.lsn++;
	logger[which]->inbuf.max_lsn_in_buf = logger[which]->lsn;
	r = ml_unlock(&logger[which]->input_lock);                       assert(r == 0);
    }

    {
	r = ml_lock(&logger[which]->input_lock);                         assert(r==0);
	r = toku_logger_make_space_in_inbuf(logger[which], 2);           assert(r==0);
	memcpy(logger[which]->inbuf.buf+logger[which]->inbuf.n_in_buf, "b1", 2);
	logger[which]->inbuf.n_in_buf += 2;
	logger[which]->lsn.lsn++;
	logger[which]->inbuf.max_lsn_in_buf = logger[which]->lsn;
	r = ml_unlock(&logger[which]->input_lock);                       assert(r == 0);
    }
}

static void tear_down_logger(int which) {
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
    int r;
    for (loop = 0; loop < numloops; loop++) {
        r = system(rmrf);
        CKERR(r);
        for (i = 0; i < NUM_LOGGERS; i++) setup_logger(i);
        for (i = 0; i < NUM_LOGGERS; i++) play_with_logger(i);
        for (i = 0; i < NUM_LOGGERS; i++) tear_down_logger(i);
    }
    r = system(rmrf);
    CKERR(r);

    return 0;
}
