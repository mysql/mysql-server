/* Recover an env.  The logs are in argv[1].  The new database is created in the cwd. */

// Test:
//    cd ../src/tests/tmpdir
//    ../../../newbrt/recover ../dir.test_log2.c.tdb

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#include "log_header.h"
#include "log-internal.h"

int main (int argc, char *argv[]) {
    const char *dir;
    int r;
    assert(argc==2);
    dir = argv[1];
    int n_logfiles;
    char **logfiles;
    r = tokulogger_find_logfiles(dir, &n_logfiles, &logfiles);
    if (r!=0) exit(1);
    int i;
    for (i=0; i<n_logfiles; i++) {
	fprintf(stderr, "Opening %s\n", logfiles[i]);
	FILE *f = fopen(logfiles[i], "r");
	struct log_entry le;
	u_int32_t version;
	r=read_and_print_logmagic(f, &version);
	assert(r==0 && version==0);
	while ((r = tokulog_fread(f, &le))==0) {
	    printf("Got cmd %c\n", le.cmd);
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
    for (i=0; i<n_logfiles; i++) {
	toku_free(logfiles[i]);
    }
    toku_free(logfiles);
    return 0;
}
