/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

/* Recover an env.  The logs are in argv[1].  The new database is created in the cwd. */

// Test:
//    cd ../src/tests/tmpdir
//    ../../../newbrt/recover ../dir.test_log2.c.tdb

#include "cachetable.h"
#include "key.h"
#include "log-internal.h"
#include "log_header.h"
#include "toku_assert.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

int tokudb_recover(const char *data_dir, const char *log_dir) {
    int r;
    int entrycount=0;
    int n_logfiles;
    char **logfiles;

    int lockfd;

    {
	int namelen=strlen(data_dir);
	char lockfname[namelen+20];

	snprintf(lockfname, sizeof(lockfname), "%s/__recoverylock_dont_delete_me", data_dir);
	lockfd = open(lockfname, O_RDWR|O_CREAT, S_IRUSR | S_IWUSR);
	if (lockfd<0) {
	    printf("Couldn't open %s\n", lockfname);
	    return errno;
	}
	r=flock(lockfd, LOCK_EX | LOCK_NB);
	if (r!=0) {
	    printf("Couldn't run recovery because some other process holds the recovery lock %s\n", lockfname);
	    return errno;
	}
    }

    r = toku_logger_find_logfiles(log_dir, &n_logfiles, &logfiles);
    if (r!=0) return r;
    int i;
    toku_recover_init();
    char org_wd[1000];
    {
	char *wd=getcwd(org_wd, sizeof(org_wd));
	assert(wd!=0);
	//printf("%s:%d org_wd=\"%s\"\n", __FILE__, __LINE__, org_wd);
    }
    char data_wd[1000];
    {
	r=chdir(data_dir); assert(r==0);
	char *wd=getcwd(data_wd, sizeof(data_wd));
	assert(wd!=0);
	//printf("%s:%d data_wd=\"%s\"\n", __FILE__, __LINE__, data_wd);
    }
    for (i=0; i<n_logfiles; i++) {
	//fprintf(stderr, "Opening %s\n", logfiles[i]);
	r=chdir(org_wd);
	assert(r==0);
	FILE *f = fopen(logfiles[i], "r");
	struct log_entry le;
	u_int32_t version;
	r=toku_read_and_print_logmagic(f, &version);
	assert(r==0 && version==0);
	r=chdir(data_wd);
	assert(r==0);
	while ((r = toku_log_fread(f, &le))==0) {
	    //printf("%lld: Got cmd %c\n", le.u.commit.lsn.lsn, le.cmd);
	    logtype_dispatch_args(&le, toku_recover_);
	    entrycount++;
	}
	if (r!=EOF) {
	    if (r==DB_BADFORMAT) {
		fprintf(stderr, "Bad log format at record %d\n", entrycount);
		return r;
	    } else {
		fprintf(stderr, "Huh? %s\n", strerror(r));
		return r;
	    }
	}
	fclose(f);
    }
    toku_recover_cleanup();
    for (i=0; i<n_logfiles; i++) {
	toku_free(logfiles[i]);
    }
    toku_free(logfiles);

    r=flock(lockfd, LOCK_UN);
    if (r!=0) return errno;

    r=chdir(org_wd);
    if (r!=0) return errno;

    //printf("%s:%d recovery successful! ls -l says\n", __FILE__, __LINE__);
    //system("ls -l");
    return 0;
}
