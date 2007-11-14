#include <unistd.h>
#include "log-internal.h"
#include "wbuf.h"
#include "memory.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include "../src/ydb-internal.h"

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

    result->lsn.lsn = 0; // WRONG!!!  This should actually be calculated by looking at the log file. 

    *resultp=result;
    return tokulogger_log_bytes(result, 0, "");
}

int tokulogger_log_bytes(TOKULOGGER logger, int nbytes, void *bytes) {
    int r;
    //fprintf(stderr, "%s:%d logging %d bytes\n", __FILE__, __LINE__, nbytes);
    if (logger->fd==-1) {
	int  fnamelen = strlen(logger->directory)+50;
	char fname[fnamelen];
	snprintf(fname, fnamelen, "%s/log%012llu.tokulog", logger->directory, logger->next_log_file_number);
	fprintf(stderr, "%s:%d creat(%s, ...)\n", __FILE__, __LINE__, fname);
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
	//fprintf(stderr, "%s:%d flushing log due to buffer overflow\n", __FILE__, __LINE__);
	r=writev(logger->fd, v, 2);
	if (r!=logger->n_in_buf + nbytes) return errno;
	logger->n_in_file += logger->n_in_buf+nbytes;
	logger->n_in_buf=0;
	if (logger->n_in_file > 100<<20) {
	    fprintf(stderr, "%s:%d closing logfile\n", __FILE__, __LINE__);
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

int tokulogger_log_close(TOKULOGGER *loggerp) {
    TOKULOGGER logger = *loggerp;
    int r = 0;
    if (logger->fd!=-1) {
	printf("%s:%d closing log: n_in_buf=%d\n", __FILE__, __LINE__, logger->n_in_buf);
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

int tokulogger_fsync (TOKULOGGER logger) {
    //return 0;/// NO TXN
    //fprintf(stderr, "%s:%d syncing log\n", __FILE__, __LINE__);
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

static int tokulogger_finish (TOKULOGGER logger, struct wbuf *wbuf) {
    wbuf_int(wbuf, toku_crc32(0, wbuf->buf, wbuf->ndone));
    wbuf_int(wbuf, 4+wbuf->ndone);
    return tokulogger_log_bytes(logger, wbuf->ndone, wbuf->buf);
}

// Log an insertion of a key-value pair into a particular node of the tree.
int tokulogger_log_brt_insert_with_no_overwrite (TOKULOGGER logger,
						 TXNID txnid,
						 FILENUM fileid,
						 DISKOFF diskoff,
						 unsigned char *key,
						 int keylen,
						 unsigned char *val,
						 int vallen) {
    int buflen=(keylen+vallen+4+4 // key and value
		+1 // command
		+8 // lsn
		+8 // txnid
		+4 // fileid
		+8 // diskoff
		+8 // crc and len
		);
    unsigned char buf[buflen];
    struct wbuf wbuf;
    wbuf_init(&wbuf, buf, buflen) ;
    wbuf_char(&wbuf, LT_INSERT_WITH_NO_OVERWRITE);
    wbuf_lsn (&wbuf, logger->lsn); logger->lsn.lsn++;
    wbuf_txnid(&wbuf, txnid);
    wbuf_filenum(&wbuf, fileid);
    wbuf_diskoff(&wbuf, diskoff);
    wbuf_bytes(&wbuf, key, keylen);
    wbuf_bytes(&wbuf, val, vallen);
    return tokulogger_finish (logger, &wbuf);
}

int tokulogger_log_phys_add_or_delete_in_leaf (DB *db, TOKUTXN txn, DISKOFF diskoff, int is_add, const struct kv_pair *pair) {
    if (txn==0) return 0;
    assert(db);
    int keylen = pair->keylen;
    int vallen = pair->vallen;
    const int buflen=(keylen+vallen+4+4 // the key and value
		      +1 // log command
		      +8 // lsn
		      +8 // txnid
		      +8 // fileid
		      +8 // diskoff
		      +8 // crc & len
		      );
    unsigned char buf[buflen];
    struct wbuf wbuf;
    wbuf_init(&wbuf, buf, buflen) ;
    wbuf_char(&wbuf, is_add ? LT_INSERT_WITH_NO_OVERWRITE : LT_DELETE);
    wbuf_lsn (&wbuf, txn->logger->lsn);
    txn->logger->lsn.lsn++;
    wbuf_txnid(&wbuf, txn->txnid64);
    wbuf_filenum(&wbuf, db->i->fileid);
    wbuf_diskoff(&wbuf, diskoff);
    wbuf_bytes(&wbuf, kv_pair_key_const(pair), keylen);
    wbuf_bytes(&wbuf, kv_pair_val_const(pair), vallen);
    return tokulogger_finish(txn->logger, &wbuf);
}

int tokulogger_log_commit (TOKUTXN txn) {
    struct wbuf wbuf;
    const int buflen = (1 // log command
			+8 // lsn
			+8 // txnid
			+8 // crc & len
			);
    unsigned char buf[buflen];
    wbuf_init(&wbuf, buf, buflen);
    wbuf_char(&wbuf, LT_COMMIT);
    wbuf_lsn (&wbuf, txn->logger->lsn);
    txn->logger->lsn.lsn++;
    wbuf_txnid(&wbuf, txn->txnid64);
    int r = tokulogger_finish(txn->logger, &wbuf);
    if (r!=0) return r;
    if (txn->parent) return 0;
    else return tokulogger_fsync(txn->logger);
}

int tokulogger_log_checkpoint (TOKULOGGER logger, LSN *lsn) {
    struct wbuf wbuf;
    const int buflen =10;
    unsigned char buf[buflen];
    wbuf_init(&wbuf, buf, buflen);
    wbuf_char(&wbuf, LT_CHECKPOINT);
    wbuf_lsn (&wbuf, logger->lsn);
    *lsn = logger->lsn;
    logger->lsn.lsn++;
    return tokulogger_log_bytes(logger, wbuf.ndone, wbuf.buf);
    
}

int tokutxn_begin (TOKUTXN parent_tokutxn, TOKUTXN *tokutxn, TXNID txnid64, TOKULOGGER logger) {
    TAGMALLOC(TOKUTXN, result);
    if (result==0) return errno;
    result->txnid64 = txnid64;
    result->logger = logger;
    result->parent = parent_tokutxn;
    *tokutxn = result;
    return 0;
}

int tokulogger_log_block_rename (TOKULOGGER logger, FILENUM fileid, DISKOFF olddiskoff, DISKOFF newdiskoff, DISKOFF parentdiskoff, int childnum) {
    const int buflen=(+1 // log command
		      +8 // lsn
		      +8 // fileid
		      +8 // olddiskoff
		      +8 // newdiskoff
		      +8 // parentdiskoff
		      +4 // childnum
		      +8 // crc & len
		      );
    unsigned char buf[buflen];
    struct wbuf wbuf;
    wbuf_init   (&wbuf, buf, buflen) ;
    wbuf_char   (&wbuf, LT_BLOCK_RENAME);
    wbuf_lsn    (&wbuf, logger->lsn);
    logger->lsn.lsn++;
    wbuf_filenum(&wbuf, fileid);
    wbuf_diskoff(&wbuf, olddiskoff);
    wbuf_diskoff(&wbuf, newdiskoff);
    wbuf_diskoff(&wbuf, parentdiskoff);
    wbuf_int    (&wbuf, childnum);
    return tokulogger_finish(logger, &wbuf);
}

/*
int brtenv_checkpoint (BRTENV env) {
    init the checkpointing lock
    acquire_spinlock(&env->checkpointing);
    release_spinlock(&env->checkpointing);
    return -1;
}
*/
