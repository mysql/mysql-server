#include "log-internal.h"
#include "memory.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

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

int tokulogger_create_and_open_logger (const char *directory, TOKULOGGER *resultp) {
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
    result->fd = -1;
    result->next_log_file_number = nexti;
    result->n_in_buf = 0;
    *resultp=result;
    return 0;
}

int tokulogger_log_bytes(TOKULOGGER logger, int nbytes, char *bytes) {
    int r;
    if (logger->fd==-1) {
	int  fnamelen = strlen(logger->directory)+50;
	char fname[fnamelen];
	snprintf(fname, fnamelen, "%s/log%012llu.tokulog", logger->directory, logger->next_log_file_number);
	logger->fd = creat(fname, O_EXCL | 0700);
	if (logger->fd==-1) return errno;
    }
    logger->next_log_file_number++;
    if (logger->n_in_buf + nbytes > LOGGER_BUF_SIZE) {
	struct iovec v[2];
	v[0].iov_base = logger->buf;
	v[0].iov_len  = logger->n_in_buf;
	v[1].iov_base = bytes;
	v[1].iov_len  = nbytes;
	r=writev(logger->fd, v, 2);
	if (r!=logger->n_in_buf + nbytes) return errno;
	logger->n_in_file += logger->n_in_buf+nbytes;
	logger->n_in_buf=0;
	if (logger->n_in_file > 100<<20) {
	    r = close(logger->fd);
	    if (r!=0) return errno;
	    logger->fd=0;
	    logger->n_in_file = 0;
	}
    } else {
	memcpy(logger->buf+logger->n_in_buf, bytes, nbytes);
	logger->n_in_buf += nbytes;
    }
    return 0;
}
