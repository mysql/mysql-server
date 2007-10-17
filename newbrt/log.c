#include <unistd.h>
#include "log-internal.h"
#include "wbuf.h"
#include "memory.h"
#include "../src/ydb-internal.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>

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
    int r = closedir(d);
    return r;
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
    if (result->directory==0) goto died0;
    result->fd = -1;
    result->next_log_file_number = nexti;
    result->n_in_buf = 0;
    *resultp=result;
    return tokulogger_log_bytes(result, 0, "");
}

int tokulogger_log_bytes(TOKULOGGER logger, int nbytes, void *bytes) {
    int r;
    //printf("%s:%d logging %d bytes\n", __FILE__, __LINE__, nbytes);
    if (logger->fd==-1) {
	int  fnamelen = strlen(logger->directory)+50;
	char fname[fnamelen];
	snprintf(fname, fnamelen, "%s/log%012llu.tokulog", logger->directory, logger->next_log_file_number);
	printf("%s:%d creat(%s, ...)\n", __FILE__, __LINE__, fname);
	logger->fd = creat(fname, O_EXCL | 0700);
	if (logger->fd==-1) return errno;
	logger->next_log_file_number++;
    }
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
	    logger->fd=-1;
	    logger->n_in_file = 0;
	}
    } else {
	memcpy(logger->buf+logger->n_in_buf, bytes, nbytes);
	logger->n_in_buf += nbytes;
    }
    return 0;
}

// Log an insertion of a key-value pair into a particular node of the tree.
int tokulogger_log_brt_insert_with_no_overwrite (TOKULOGGER logger,
						 TXNID txnid,
						 diskoff diskoff,
						 unsigned char *key,
						 int keylen,
						 unsigned char *val,
						 int vallen) {
    int buflen=30+keylen+vallen;
    unsigned char buf[buflen];
    struct wbuf wbuf;
    wbuf_init(&wbuf, buf, buflen) ;
    wbuf_char(&wbuf, LT_INSERT_WITH_NO_OVERWRITE);
    wbuf_txnid(&wbuf, txnid);
    wbuf_diskoff(&wbuf, diskoff);
    wbuf_bytes(&wbuf, key, keylen);
    wbuf_bytes(&wbuf, val, vallen);
    return tokulogger_log_bytes(logger, wbuf.ndone, wbuf.buf);
}

int tokulogger_log_close(TOKULOGGER *loggerp) {
    TOKULOGGER logger = *loggerp;
    int r = 0;
    if (logger->fd!=-1) {
	printf("%s:%d n_in_buf=%d\n", __FILE__, __LINE__, logger->n_in_buf);
	if (logger->n_in_buf>0) {
	    r = write(logger->fd, logger->buf, logger->n_in_buf);
	    if (r==-1) return errno;
	}
	r = close(logger->fd);
    }
    toku_free(logger->directory);
    toku_free(logger);
    *loggerp=0;
    return r;
}
#if 0
int tokulogger_log_brt_remove (TOKULOGGER logger,
			       TXNID txnid,
			       diskoff diskoff,
			       unsigned char *key,
			       int keylen,
			       unsigned char *val,
			       int vallen) {
n    
}
#endif

int tokulogger_log_phys_add_or_delete_in_leaf (DB *db, TOKUTXN txn, diskoff diskoff, int is_add, const struct kv_pair *pair) {
    if (txn==0) return 0;
    int keylen = pair->keylen;
    int vallen = pair->vallen;
    int buflen=(keylen+vallen+4+4 // the key and value
		+1 // log command
		+8 // txnid
		+8 // fileid
		+8 // diskoff
		);
    unsigned char buf[buflen];
    struct wbuf wbuf;
    wbuf_init(&wbuf, buf, buflen) ;
    wbuf_char(&wbuf, is_add ? LT_INSERT_WITH_NO_OVERWRITE : LT_DELETE);
    wbuf_txnid(&wbuf, txn->txnid64);
    wbuf_fileid(&wbuf, db->i->fileid);
    wbuf_diskoff(&wbuf, diskoff);
    wbuf_bytes(&wbuf, kv_pair_key_const(pair), keylen);
    wbuf_bytes(&wbuf, kv_pair_val_const(pair), vallen);
    return tokulogger_log_bytes(txn->logger, wbuf.ndone, wbuf.buf);

}

int tokulogger_fsync (TOKULOGGER logger) {
    //return 0;/// NO TXN
    if (logger->n_in_buf>0) {
	int r = write(logger->fd, logger->buf, logger->n_in_buf);
	if (r==-1) return errno;
	logger->n_in_buf=0;
    }
    {
	int r = fsync(logger->fd);
	if (r!=0) return errno;
    }
    return 0;
}

int tokulogger_log_commit (TOKUTXN txn) {
    struct wbuf wbuf;
    int buflen =30;
    unsigned char buf[buflen];
    wbuf_init(&wbuf, buf, buflen);
    wbuf_char(&wbuf, LT_COMMIT);
    wbuf_txnid(&wbuf, txn->txnid64);
    int r = tokulogger_log_bytes(txn->logger, wbuf.ndone, wbuf.buf);
    if (r!=0) return r;
    return tokulogger_fsync(txn->logger);
}

int tokutxn_begin (TOKUTXN *tokutxn, TXNID txnid64, TOKULOGGER logger) {
    TAGMALLOC(TOKUTXN, result);
    if (result==0) return errno;
    result->txnid64 = txnid64;
    result->logger = logger;
    *tokutxn = result;
    return 0;
}

