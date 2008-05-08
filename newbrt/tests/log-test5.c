/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "log-internal.h"
#include "toku_assert.h"
#include <dirent.h>
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
    for (i=0; i<1000; i++) {
	r = ml_lock(&logger->input_lock);
	assert(r==0);

	int ilen=3+random()%5;
	struct logbytes *b = MALLOC_LOGBYTES(ilen+1);
	b->nbytes=ilen+1;
	snprintf(b->bytes, ilen+1, "a%0*d ", (int)ilen, i); // skip the trailing nul
	b->lsn=(LSN){23+i};
	r = toku_logger_log_bytes(logger, b, 0);
	assert(r==0);
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
	    struct stat statbuf;
	    r = stat(fname, &statbuf);
	    assert(r==0);
	    assert(statbuf.st_size<=LSIZE);
	}
	r = closedir(dir);
	assert(r==0);
    }
    return 0;
}
