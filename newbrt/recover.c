/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* Recover an env.  The logs are in argv[1].  The new database is created in the cwd. */

// Test:
//    cd ../src/tests/tmpdir
//    ../../../newbrt/recover ../dir.test_log2.c.tdb

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "log_header.h"
#include "log-internal.h"
#include "cachetable.h"
#include "key.h"

int main (int argc, char *argv[]) {
    const char *dir;
    int r;
    int entrycount=0;
    assert(argc==2);
    dir = argv[1];
    int n_logfiles;
    char **logfiles;
    r = toku_logger_find_logfiles(dir, &n_logfiles, &logfiles);
    if (r!=0) exit(1);
    int i;
    toku_recover_init();
    for (i=0; i<n_logfiles; i++) {
	//fprintf(stderr, "Opening %s\n", logfiles[i]);
	FILE *f = fopen(logfiles[i], "r");
	struct log_entry le;
	u_int32_t version;
	r=toku_read_and_print_logmagic(f, &version);
	assert(r==0 && version==0);
	while ((r = toku_log_fread(f, &le))==0) {
	    //printf("%lld: Got cmd %c\n", le.u.commit.lsn.lsn, le.cmd);
	    logtype_dispatch(&le, toku_recover_);
	    entrycount++;
	}
	if (r!=EOF) {
	    if (r==DB_BADFORMAT) {
		fprintf(stderr, "Bad log format\n");
		exit(1);
	    } else {
		fprintf(stderr, "Huh? %s\n", strerror(r));
		exit(1);
	    }
	}
	fclose(f);
    }
    toku_recover_cleanup();
    for (i=0; i<n_logfiles; i++) {
	toku_free(logfiles[i]);
    }
    toku_free(logfiles);
    return 0;
}
