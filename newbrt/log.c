#include "log-internal.h"
#include "memory.h"
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>

int tokulogger_find_next_unused_log_file(const char *directory, long long *result) {
    DIR *d=opendir(directory);
    long long max=-1;
    struct dirent *de;
    if (d==0) return errno;
    while ((de=readdir(d))) {
	if (de==0) return -errno;
	long long thisl;
	int r = sscanf(de->d_name, "log%llu.tokulog", &thisl);
	if (r==1 && thisl>max) max=thisl;
    }
    *result=max+1;
    return 0;
}

int tokulogger_create_and_open_logger (const char *directory) {
    TAGMALLOC(TOKULOGGER, result);
    if (result==0) return -1;
    int r;
    long long nexti;
    r = tokulogger_find_next_unused_log_file(directory, &nexti);
    if (r!=0) {
    died0:
	toku_free(result);
	return nexti;
    }
    result->directory = toku_strdup(directory);
    if (result->directory!=0) goto died0;
    result->f = 0;
    result->next_log_file_number = nexti;
    result->n_in_buf = 0;
    return 0;
}
