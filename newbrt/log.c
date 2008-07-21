/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#define _XOPEN_SOURCE 500
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "brt-internal.h"
#include "log-internal.h"
#include "wbuf.h"
#include "memory.h"
#include "log_header.h"

static char dev_null[] = "/dev/null";

void* toku_malloc_in_rollback(TOKUTXN txn, size_t size) {
    return malloc_in_memarena(txn->rollentry_arena, size);
}

void *toku_memdup_in_rollback(TOKUTXN txn, const void *v, size_t len) {
    void *r=toku_malloc_in_rollback(txn, len);
    memcpy(r,v,len);
    return r;
}

char *toku_strdup_in_rollback(TOKUTXN txn, const char *s) {
    return toku_memdup_in_rollback(txn, s, strlen(s)+1);
}

int toku_logger_fsync_null(int fd __attribute__((__unused__))) {
    return 0;
}

int toku_logger_find_next_unused_log_file(const char *directory, long long *result) {
    DIR *d=opendir(directory);
    long long max=-1; *result = max;
    struct dirent *de;
    if (d==0) return errno;
    while ((de=readdir(d))) {
	if (de==0) return errno;
	long long thisl;
	int r = sscanf(de->d_name, "log%llu.tokulog", &thisl);
	if (r==1 && thisl>max) max=thisl;
    }
    *result=max+1;
    int r = closedir(d);
    return r;
}

int logfilenamecompare (const void *ap, const void *bp) {
    char *a=*(char**)ap;
    char *b=*(char**)bp;
    return strcmp(a,b);
}

int toku_logger_find_logfiles (const char *directory, char ***resultp) {
    int result_limit=2;
    int n_results=0;
    char **MALLOC_N(result_limit, result);
    struct dirent *de;
    DIR *d=opendir(directory);
    if (d==0) {
        free(result);
        return errno;
    }
    int dirnamelen = strlen(directory);
    while ((de=readdir(d))) {
	if (de==0) return errno;
	long long thisl;
	int r = sscanf(de->d_name, "log%llu.tokulog", &thisl);
	if (r!=1) continue; // Skip over non-log files.
	if (n_results+1>=result_limit) {
	    result_limit*=2;
	    result = toku_realloc(result, result_limit*sizeof(*result));
	}
	int fnamelen = dirnamelen + strlen(de->d_name) + 2; // One for the slash and one for the trailing NUL.
	char *fname = toku_malloc(fnamelen);
	snprintf(fname, fnamelen, "%s/%s", directory, de->d_name);
	result[n_results++] = fname;
    }
    // Return them in increasing order.
    qsort(result, n_results, sizeof(result[0]), logfilenamecompare);
    *resultp    = result;
    result[n_results]=0; // make a trailing null
    return d ? closedir(d) : 0;
}

int toku_logger_create (TOKULOGGER *resultp) {
    int r;
    TAGMALLOC(TOKULOGGER, result);
    if (result==0) return errno;
    result->is_open=0;
    result->is_panicked=0;
    result->write_log_files = 1;
    result->lg_max = 100<<20; // 100MB default
    result->head = result->tail = 0;
    result->lsn = result->written_lsn = result->fsynced_lsn = (LSN){0};
    list_init(&result->live_txns);
    result->n_in_buf=0;
    result->n_in_file=0;
    result->directory=0;
    result->checkpoint_lsns[0]=result->checkpoint_lsns[1]=(LSN){0};
    *resultp=result;
    r = ml_init(&result->input_lock); if (r!=0) goto died0;
    r = ml_init(&result->output_lock); if (r!=0) goto died1;
    return 0;
    
 died1:
    ml_destroy(&result->input_lock);
 died0:
    toku_free(result);
    return r;
}

void toku_logger_set_cachetable (TOKULOGGER tl, CACHETABLE ct) {
    tl->ct = ct;
}

void toku_logger_write_log_files (TOKULOGGER tl, int write_log_files) {
    tl->write_log_files = write_log_files;
}

static int (*toku_os_fsync_function)(int)=fsync;

static const int log_format_version=0;

// Write something out.  Keep trying even if partial writes occur.
// On error: Return negative with errno set.
// On success return nbytes.
static int write_it (int fd, const void *bufv, int nbytes) {
    int org_nbytes=nbytes;
    const char *buf=bufv;
    while (nbytes>0) {
	int r = write(fd, buf, nbytes);
	if (r<0 || errno!=EAGAIN) return r;
	buf+=r;
	nbytes-=r;
    }
    return org_nbytes;
}

static int open_logfile (TOKULOGGER logger) {
    int r;
    int fnamelen = strlen(logger->directory)+50;
    char fname[fnamelen];
    snprintf(fname, fnamelen, "%s/log%012llu.tokulog", logger->directory, logger->next_log_file_number);
    if (logger->write_log_files) {
        logger->fd = creat(fname, O_EXCL | 0700);        if (logger->fd==-1) return errno;
    } else {
        logger->fd = open(dev_null, O_RDWR);             if (logger->fd==-1) return errno;
    }
    logger->next_log_file_number++;
    int version_l = htonl(log_format_version);
    r = write_it(logger->fd, "tokulogg", 8);             if (r!=8) return errno;
    r = write_it(logger->fd, &version_l, 4);             if (r!=4) return errno;
    logger->fsynced_lsn = logger->written_lsn;
    logger->n_in_file = 12;
    return 0;
}

static int close_and_open_logfile (TOKULOGGER logger) {
    int r;
    r=toku_os_fsync_function(logger->fd);                if (r!=0) return errno;
    r = close(logger->fd);                               if (r!=0) return errno;
    return open_logfile(logger);
}

int toku_logger_open (const char *directory, TOKULOGGER logger) {
    if (logger->is_open) return EINVAL;
    if (logger->is_panicked) return EINVAL;
    int r;
    long long nexti;
    r = toku_logger_find_next_unused_log_file(directory, &nexti);
    if (r!=0) return r;
    logger->directory = toku_strdup(directory);
    if (logger->directory==0) return errno;
    logger->next_log_file_number = nexti;
    open_logfile(logger);

    logger->lsn.lsn = 0; // WRONG!!!  This should actually be calculated by looking at the log file. 
    logger->written_lsn.lsn = 0;
    logger->fsynced_lsn.lsn = 0;

    logger->is_open = 1;
    if (!logger->write_log_files)
        toku_set_func_fsync(toku_logger_fsync_null);

    return 0;

}

void toku_logger_panic (TOKULOGGER logger, int err) {
    logger->panic_errno=err;
    logger->is_panicked=1;
}
int toku_logger_panicked(TOKULOGGER logger) {
    if (logger==0) return 0;
    return logger->is_panicked;
}
int toku_logger_is_open(TOKULOGGER logger) {
    if (logger==0) return 0;
    return logger->is_open;
}

int toku_logger_set_lg_max(TOKULOGGER logger, u_int32_t lg_max) {
    if (logger==0) return EINVAL; // no logger
    if (logger->is_panicked) return EINVAL;
    if (logger->is_open) return EINVAL;
    if (lg_max>(1<<30)) return EINVAL; // too big
    logger->lg_max = lg_max;
    return 0;
}
int toku_logger_get_lg_max(TOKULOGGER logger, u_int32_t *lg_maxp) {
    if (logger==0) return EINVAL; // no logger
    if (logger->is_panicked) return EINVAL;
    *lg_maxp = logger->lg_max;
    return 0;
    
}

// Enter holding both locks
// Exit holding only the output_lock
static int do_write (TOKULOGGER logger, int do_fsync) {
    int r;
    struct logbytes *list = logger->head;
    logger->head=logger->tail=0;
    logger->n_in_buf=0;
    r=ml_unlock(&logger->input_lock); if (r!=0) goto panic;
    logger->n_in_buf=0;
    while (list) {
	if (logger->n_in_file + list->nbytes <= logger->lg_max) {
	    if (logger->n_in_buf + list->nbytes <= LOGGER_BUF_SIZE) {
		memcpy(logger->buf+logger->n_in_buf, list->bytes, list->nbytes);
		logger->n_in_buf+=list->nbytes;
		logger->n_in_file+=list->nbytes;
		logger->written_lsn = list->lsn;
		struct logbytes *next=list->next;
		toku_free(list);
		list=next;
	    } else {
		// it doesn't fit in the buffer, but it does fit in the file.  So flush the buffer
		r=write_it(logger->fd, logger->buf, logger->n_in_buf);
		if (r!=logger->n_in_buf) { r=errno; goto panic; }
		logger->n_in_buf=0;
		// Special case for a log entry that's too big to fit in the buffer.
		if (list->nbytes > LOGGER_BUF_SIZE) {
		    r=write_it(logger->fd, list->bytes, list->nbytes);
		    if (r!=list->nbytes) { r=errno; goto panic; }
		    logger->n_in_file+=list->nbytes;
		    logger->written_lsn = list->lsn;
		    struct logbytes *next=list->next;
		    toku_free(list);
		    list=next;
		}
	    }
	} else {
	    // The new item doesn't fit in the file, so write the buffer, reopen the file, and try again
	    r=write_it(logger->fd, logger->buf, logger->n_in_buf);
	    logger->n_in_buf=0;
	    r=close_and_open_logfile(logger);   if (r!=0) goto panic;
	}
    }
    r=write_it(logger->fd, logger->buf, logger->n_in_buf);
    if (r!=logger->n_in_buf) { r=errno; goto panic; }
    logger->n_in_buf=0;
    if (do_fsync) {
	r = toku_os_fsync_function(logger->fd);
	logger->fsynced_lsn = logger->written_lsn;
    }
    return 0;
 panic:
    toku_logger_panic(logger, r);
    return r;
}

// enter holding input_lock
// exit holding no locks
int toku_logger_log_bytes (TOKULOGGER logger, struct logbytes *bytes, int do_fsync) {
    int r;
    if (logger->is_panicked) return EINVAL;
    logger->n_in_buf += bytes->nbytes;
    if (logger->tail) {
	logger->tail->next=bytes;
    } else {
	logger->head = bytes;
    }
    logger->tail = bytes;
    bytes->next = 0;
    if (logger->n_in_buf >= LOGGER_BUF_SIZE || do_fsync) {
	// We must flush it
	r=ml_unlock(&logger->input_lock); if (r!=0) goto panic;
	r=ml_lock(&logger->output_lock);  if (r!=0) goto panic;
	if (logger->written_lsn.lsn < bytes->lsn.lsn) {
	    // We found that our record has not yet been written, so we must write it, and everything else
	    r=ml_lock(&logger->input_lock);  if (r!=0) goto panic;
	    r=do_write(logger, do_fsync);    if (r!=0) goto panic;
	} else {
	    /* Our LSN has been written.  We have the output lock */
	    if (do_fsync && logger->fsynced_lsn.lsn > bytes->lsn.lsn) {
		/* But we need to fsync it. */
		r = toku_os_fsync_function(logger->fd);
		logger->fsynced_lsn = logger->written_lsn;
	    }
	}
	r=ml_unlock(&logger->output_lock);	if (r!=0) goto panic;
    } else {
	r=ml_unlock(&logger->input_lock);	if (r!=0) goto panic;
    }
    return 0;
 panic:
    toku_logger_panic(logger, r);
    return r;
}

// No locks held on entry
// No locks held on exit.
// No locks are needed, since you cannot legally close the log concurrently with doing anything else.
// But grab the locks just to be careful.
int toku_logger_close(TOKULOGGER *loggerp) {
    TOKULOGGER logger = *loggerp;
    if (logger->is_panicked) return EINVAL;
    int r = 0;
    if (!logger->is_open) goto is_closed;
    r = ml_lock(&logger->output_lock); if (r!=0) goto panic;
    r = ml_lock(&logger->input_lock); if (r!=0) goto panic;
    r = do_write(logger, 1);              if (r!=0) goto panic;
    if (logger->fd!=-1) {
	r = close(logger->fd);                if (r!=0) { r=errno; goto panic; }
    }
    logger->fd=-1;
    r = ml_unlock(&logger->output_lock);
 is_closed:
    logger->is_panicked=1; // Just in case this might help.
    if (logger->directory) toku_free(logger->directory);
    toku_free(logger);
    *loggerp=0;
    return r;
 panic:
    toku_logger_panic(logger, r);
    return r;
}

// Entry: Holds no locks
// Exit: Holds no locks 
// This is the exported fsync used by ydb.c
int toku_logger_fsync (TOKULOGGER logger) {
    int r;
    if (logger->is_panicked) return EINVAL;
    r = ml_lock(&logger->output_lock);   if (r!=0) goto panic;
    r = ml_lock(&logger->input_lock);   if (r!=0)  goto panic;
    r = do_write(logger, 1);
    r = ml_unlock(&logger->output_lock);  if (r!=0) goto panic;
    return 0;
 panic:
    toku_logger_panic(logger, r);
    return r; 
}

// wbuf points into logbytes
int toku_logger_finish (TOKULOGGER logger, struct logbytes *logbytes, struct wbuf *wbuf, int do_fsync) {
    if (logger->is_panicked) return EINVAL;
    u_int32_t checksum = murmur_finish(&wbuf->murmur);
    wbuf_int(wbuf, checksum);
    wbuf_int(wbuf, 4+wbuf->ndone);
    logbytes->nbytes=wbuf->ndone;
    return toku_logger_log_bytes(logger, logbytes, do_fsync);
}

static void note_txn_closing (TOKUTXN txn);

static void cleanup_txn (TOKUTXN txn) {
    memarena_close(&txn->rollentry_arena);
    if (txn->rollentry_filename!=0) {
	int r = close(txn->rollentry_fd);
	assert(r==0);
	r = unlink(txn->rollentry_filename);
	assert(r==0);
	toku_free(txn->rollentry_filename);
    }

    list_remove(&txn->live_txns_link);
    note_txn_closing(txn);
    toku_free(txn);
    return;
}

int toku_commit_rollback_item (TOKUTXN txn, struct roll_entry *item) {
    int r=0;
    rolltype_dispatch_assign(item, toku_commit_, r, txn);
    return r;
}

int toku_abort_rollback_item (TOKUTXN txn, struct roll_entry *item) {
    int r=0;
    rolltype_dispatch_assign(item, toku_rollback_, r, txn);
    if (r!=0) return r;
    return 0;
}

static int note_brt_used_in_parent_txn(OMTVALUE brtv, u_int32_t UU(index), void*parentv) {
    return toku_txn_note_brt(parentv, brtv);
}

int toku_logger_commit (TOKUTXN txn, int nosync) {
    // printf("%s:%d committing\n", __FILE__, __LINE__);
    // panic handled in log_commit
    int r = toku_log_commit(txn->logger, (LSN*)0, (txn->parent==0) && !nosync, txn->txnid64); // exits holding neither of the tokulogger locks.
    if (r==0) {
	if (txn->parent!=0) {
	    // First we must put a rollinclude entry into the parent if we have a rollentry file.
	    if (txn->rollentry_filename) {
		int len = strlen(txn->rollentry_filename);
		// Don't have to strdup the rollentry_filename because
		// we take ownership of it.
		BYTESTRING fname = {len, toku_strdup_in_rollback(txn, txn->rollentry_filename)};
		r = toku_logger_save_rollback_rollinclude(txn->parent, fname);
		if (r!=0) { cleanup_txn(txn); return r; }
		r = close(txn->rollentry_fd);
		if (r!=0) {
		    // We have to do the unlink ourselves, and then
		    // set txn->rollentry_filename=0 so that the cleanup
		    // won't try to close the fd again.
		    unlink(txn->rollentry_filename);
		    toku_free(txn->rollentry_filename);
		    txn->rollentry_filename = 0;
		    cleanup_txn(txn);
		    return r;
		}
		// Stop the cleanup from closing and unlinking the file.
		toku_free(txn->rollentry_filename);
		txn->rollentry_filename = 0;
	    }
	    // Append the list to the front of the parent.
	    if (txn->oldest_logentry) {
		// There are some entries, so link them in.
		txn->oldest_logentry->prev = txn->parent->newest_logentry;
		txn->parent->newest_logentry = txn->newest_logentry;
	    }
	    if (txn->parent->oldest_logentry==0) {
		txn->parent->oldest_logentry = txn->oldest_logentry;
	    }
	    txn->newest_logentry = txn->oldest_logentry = 0;
	    // Put all the memarena data into the parent.
	    memarena_move_buffers(txn->parent->rollentry_arena, txn->rollentry_arena);

	    // Note the open brts, the omts must be merged
	    r = toku_omt_iterate(txn->open_brts, note_brt_used_in_parent_txn, txn->parent);
	    assert(r==0);

	} else {
	    // do the commit calls and free everything
	    // we do the commit calls in reverse order too.
	    {
		struct roll_entry *item;
		//printf("%s:%d abort\n", __FILE__, __LINE__);
		while ((item=txn->newest_logentry)) {
		    txn->newest_logentry = item->prev;
		    r = toku_commit_rollback_item(txn, item);
		    if (r!=0) { cleanup_txn(txn); return r; }
		}
	    }

	    // Read stuff out of the file and execute it.
	    if (txn->rollentry_filename) {
		r = toku_commit_fileentries(txn->rollentry_fd, txn->rollentry_filesize, txn);
	    }
	}
    }
    cleanup_txn(txn);
    return r;
}

int toku_logger_log_checkpoint (TOKULOGGER logger) {
    if (logger->is_panicked) return EINVAL;
    int r = toku_cachetable_checkpoint(logger->ct);
    if (r!=0) return r;
    logger->checkpoint_lsns[1]=logger->checkpoint_lsns[0];
    logger->checkpoint_lsns[0]=logger->lsn;
    return toku_log_checkpoint(logger, (LSN*)0, 1);
}

int toku_logger_txn_begin (TOKUTXN parent_tokutxn, TOKUTXN *tokutxn, TOKULOGGER logger) {
    if (logger->is_panicked) return EINVAL;
    TAGMALLOC(TOKUTXN, result);
    if (result==0) return errno;
    int r =toku_log_xbegin(logger, &result->first_lsn, 0, parent_tokutxn ? parent_tokutxn->txnid64 : 0);
    if (r!=0) { toku_logger_panic(logger, r);  toku_free(result); return r; }
    if ((r=toku_omt_create(&result->open_brts))!=0) {
	toku_logger_panic(logger, r);
	toku_free(result);
	return r;
    }
    result->txnid64 = result->first_lsn.lsn;
    result->logger = logger;
    result->parent = parent_tokutxn;
    result->oldest_logentry = result->newest_logentry = 0;

    result->rollentry_arena = memarena_create();

    list_push(&logger->live_txns, &result->live_txns_link);
    result->rollentry_resident_bytecount=0;
    result->rollentry_filename = 0;
    result->rollentry_fd = -1;
    result->rollentry_filesize = 0;
    *tokutxn = result;
    return 0;
}

int toku_logger_log_fcreate (TOKUTXN txn, const char *fname, int mode) {
    if (txn==0) return 0;
    if (txn->logger->is_panicked) return EINVAL;
    BYTESTRING bs = { .len=strlen(fname), .data = toku_strdup_in_rollback(txn, fname) };
    int r = toku_log_fcreate (txn->logger, (LSN*)0, 0, toku_txn_get_txnid(txn), bs, mode);
    if (r!=0) return r;
    r = toku_logger_save_rollback_fcreate(txn, toku_txn_get_txnid(txn), bs);
    return r;
}

/* fopen isn't really an action.  It's just for bookkeeping.  We need to know the filename that goes with a filenum. */
int toku_logger_log_fopen (TOKUTXN txn, const char * fname, FILENUM filenum) {
    if (txn==0) return 0;
    if (txn->logger->is_panicked) return EINVAL;
    BYTESTRING bs;
    bs.len = strlen(fname);
    bs.data = (char*)fname;
    return toku_log_fopen (txn->logger, (LSN*)0, 0, toku_txn_get_txnid(txn), bs, filenum);
}

int toku_fread_u_int8_t_nocrclen (FILE *f, u_int8_t *v) {
    int vi=fgetc(f);
    if (vi==EOF) return -1;
    u_int8_t vc=vi;
    *v = vc;
    return 0;
}

int toku_fread_u_int8_t (FILE *f, u_int8_t *v, struct murmur *mm, u_int32_t *len) {
    int vi=fgetc(f);
    if (vi==EOF) return -1;
    u_int8_t vc=vi;
    murmur_add(mm, &vc, 1);
    (*len)++;
    *v = vc;
    return 0;
}

int toku_fread_u_int32_t_nocrclen (FILE *f, u_int32_t *v) {
    u_int8_t c0,c1,c2,c3;
    int r;
    r = toku_fread_u_int8_t_nocrclen (f, &c0); if (r!=0) return r;
    r = toku_fread_u_int8_t_nocrclen (f, &c1); if (r!=0) return r;
    r = toku_fread_u_int8_t_nocrclen (f, &c2); if (r!=0) return r;
    r = toku_fread_u_int8_t_nocrclen (f, &c3); if (r!=0) return r;
    *v = ((c0<<24)|
	  (c1<<16)|
	  (c2<< 8)|
	  (c3<<0));
    return 0;
}
int toku_fread_u_int32_t (FILE *f, u_int32_t *v, struct murmur *murmur, u_int32_t *len) {
    u_int8_t c0,c1,c2,c3;
    int r;
    r = toku_fread_u_int8_t (f, &c0, murmur, len); if(r!=0) return r;
    r = toku_fread_u_int8_t (f, &c1, murmur, len); if(r!=0) return r;
    r = toku_fread_u_int8_t (f, &c2, murmur, len); if(r!=0) return r;
    r = toku_fread_u_int8_t (f, &c3, murmur, len); if(r!=0) return r;
    *v = ((c0<<24)|
	  (c1<<16)|
	  (c2<< 8)|
	  (c3<<0));
    return 0;
}
int toku_fread_int32_t (FILE *f, int32_t *v, struct murmur *murmur, u_int32_t *len) {
    u_int32_t uv;
    int r = toku_fread_u_int32_t(f, &uv, murmur, len);
    int32_t rv = uv;
    if (r==0) *v=rv;
    return r;
}

int toku_fread_u_int64_t (FILE *f, u_int64_t *v, struct murmur *murmur, u_int32_t *len) {
    u_int32_t v1,v2;
    int r;
    r=toku_fread_u_int32_t(f, &v1, murmur, len);    if (r!=0) return r;
    r=toku_fread_u_int32_t(f, &v2, murmur, len);    if (r!=0) return r;
    *v = (((u_int64_t)v1)<<32 ) | ((u_int64_t)v2);
    return 0;
}
int toku_fread_LSN     (FILE *f, LSN *lsn, struct murmur *murmur, u_int32_t *len) {
    return toku_fread_u_int64_t (f, &lsn->lsn, murmur, len);
}
int toku_fread_FILENUM (FILE *f, FILENUM *filenum, struct murmur *murmur, u_int32_t *len) {
    return toku_fread_u_int32_t (f, &filenum->fileid, murmur, len);
}
int toku_fread_DISKOFF (FILE *f, DISKOFF *diskoff, struct murmur *murmur, u_int32_t *len) {
    int r = toku_fread_u_int64_t (f, (u_int64_t*)diskoff, murmur, len); // sign conversion will be OK.
    return r;
}
int toku_fread_TXNID   (FILE *f, TXNID *txnid, struct murmur *murmur, u_int32_t *len) {
    return toku_fread_u_int64_t (f, txnid, murmur, len);
}

// fills in the bs with malloced data.
int toku_fread_BYTESTRING (FILE *f, BYTESTRING *bs, struct murmur *murmur, u_int32_t *len) {
    int r=toku_fread_u_int32_t(f, (u_int32_t*)&bs->len, murmur, len);
    if (r!=0) return r;
    bs->data = toku_malloc(bs->len);
    u_int32_t i;
    for (i=0; i<bs->len; i++) {
	r=toku_fread_u_int8_t(f, (u_int8_t*)&bs->data[i], murmur, len);
	if (r!=0) {
	    toku_free(bs->data);
	    bs->data=0;
	    return r;
	}
    }
    return 0;
}

int toku_fread_LOGGEDBRTHEADER (FILE *f, LOGGEDBRTHEADER *v, struct murmur *murmur, u_int32_t *len) {
    int r;
    r = toku_fread_u_int32_t(f, &v->size,          murmur, len); if (r!=0) return r;
    r = toku_fread_u_int32_t(f, &v->flags,         murmur, len); if (r!=0) return r;
    r = toku_fread_u_int32_t(f, &v->nodesize,      murmur, len); if (r!=0) return r;
    r = toku_fread_DISKOFF  (f, &v->freelist,      murmur, len); if (r!=0) return r;
    r = toku_fread_DISKOFF  (f, &v->unused_memory, murmur, len); if (r!=0) return r;
    r = toku_fread_int32_t  (f, &v->n_named_roots, murmur, len); if (r!=0) return r;
    assert(v->n_named_roots==-1);
    r = toku_fread_DISKOFF  (f, &v->u.one.root,     murmur, len); if (r!=0) return r;
    return 0;
}

int toku_fread_INTPAIRARRAY (FILE *f, INTPAIRARRAY *v, struct murmur *murmur, u_int32_t *len) {
    int r;
    u_int32_t i;
    r = toku_fread_u_int32_t(f, &v->size, murmur, len); if (r!=0) return r;
    MALLOC_N(v->size, v->array);
    if (v->array==0) return errno;
    for (i=0; i<v->size; i++) {
	r = toku_fread_u_int32_t(f, &v->array[i].a, murmur, len); if (r!=0) return r;
	r = toku_fread_u_int32_t(f, &v->array[i].b, murmur, len); if (r!=0) return r;
    }
    return 0;
}

int toku_logprint_LSN (FILE *outf, FILE *inf, const char *fieldname, struct murmur *murmur, u_int32_t *len, const char *format __attribute__((__unused__))) {
    LSN v;
    int r = toku_fread_LSN(inf, &v, murmur, len);
    if (r!=0) return r;
    fprintf(outf, " %s=%" PRId64, fieldname, v.lsn);
    return 0;
}
int toku_logprint_TXNID (FILE *outf, FILE *inf, const char *fieldname, struct murmur *murmur, u_int32_t *len, const char *format __attribute__((__unused__))) {
    TXNID v;
    int r = toku_fread_TXNID(inf, &v, murmur, len);
    if (r!=0) return r;
    fprintf(outf, " %s=%" PRId64, fieldname, v);
    return 0;
}

int toku_logprint_u_int8_t (FILE *outf, FILE *inf, const char *fieldname, struct murmur *murmur, u_int32_t *len, const char *format) {
    u_int8_t v;
    int r = toku_fread_u_int8_t(inf, &v, murmur, len);
    if (r!=0) return r;
    fprintf(outf, " %s=%d", fieldname, v);
    if (format) fprintf(outf, format, v);
    else if (v=='\'') fprintf(outf, "('\'')");
    else if (isprint(v)) fprintf(outf, "('%c')", v);
    else {}/*nothing*/
    return 0;
    
}

int toku_logprint_u_int32_t (FILE *outf, FILE *inf, const char *fieldname, struct murmur *murmur, u_int32_t *len, const char *format) {
    u_int32_t v;
    int r = toku_fread_u_int32_t(inf, &v, murmur, len);
    if (r!=0) return r;
    fprintf(outf, " %s=", fieldname);
    fprintf(outf, format ? format : "%d", v);
    return 0;
    
}

void toku_print_BYTESTRING (FILE *outf, u_int32_t len, char *data) {
    fprintf(outf, "{len=%d data=\"", len);
    u_int32_t i;
    for (i=0; i<len; i++) {
	switch (data[i]) {
	case '"':  fprintf(outf, "\\\""); break;
	case '\\': fprintf(outf, "\\\\"); break;
	case '\n': fprintf(outf, "\\n");  break;
	default:
	    if (isprint(data[i])) fprintf(outf, "%c", data[i]);
	    else fprintf(outf, "\\%03o", (unsigned char)(data[i]));
	}
    }
    fprintf(outf, "\"}");
    
}

int toku_logprint_BYTESTRING (FILE *outf, FILE *inf, const char *fieldname, struct murmur *murmur, u_int32_t *len, const char *format __attribute__((__unused__))) {
    BYTESTRING bs;
    int r = toku_fread_BYTESTRING(inf, &bs, murmur, len);
    if (r!=0) return r;
    fprintf(outf, " %s=", fieldname);
    toku_print_BYTESTRING(outf, bs.len, bs.data);
    toku_free(bs.data);
    return 0;
}
int toku_logprint_FILENUM (FILE *outf, FILE *inf, const char *fieldname, struct murmur *murmur, u_int32_t *len, const char *format) {
    return toku_logprint_u_int32_t(outf, inf, fieldname, murmur, len, format);
    
}
int toku_logprint_DISKOFF (FILE *outf, FILE *inf, const char *fieldname, struct murmur *murmur, u_int32_t *len, const char *format __attribute__((__unused__))) {
    DISKOFF v;
    int r = toku_fread_DISKOFF(inf, &v, murmur, len);
    if (r!=0) return r;
    fprintf(outf, " %s=%lld", fieldname, v);
    return 0;
}
int toku_logprint_LOGGEDBRTHEADER (FILE *outf, FILE *inf, const char *fieldname, struct murmur *murmur, u_int32_t *len, const char *format __attribute__((__unused__))) {
    LOGGEDBRTHEADER v;
    int r = toku_fread_LOGGEDBRTHEADER(inf, &v, murmur, len);
    if (r!=0) return r;
    fprintf(outf, " %s={size=%d flags=%d nodesize=%d freelist=%lld unused_memory=%lld n_named_roots=%d", fieldname, v.size, v.flags, v.nodesize, v.freelist, v.unused_memory, v.n_named_roots);
    return 0;
    
}

int toku_logprint_INTPAIRARRAY (FILE *outf, FILE *inf, const char *fieldname, struct murmur *murmur, u_int32_t *len, const char *format __attribute__((__unused__))) {
    INTPAIRARRAY v;
    u_int32_t i;
    int r = toku_fread_INTPAIRARRAY(inf, &v, murmur, len);
    if (r!=0) return r;
    fprintf(outf, " %s={size=%d array={", fieldname, v.size);
    for (i=0; i<v.size; i++) {
	if (i!=0) fprintf(outf, " ");
	fprintf(outf, "{%d %d}", v.array[i].a, v.array[i].b);
    }
    toku_free(v.array);
    return 0;
}

int toku_read_and_print_logmagic (FILE *f, u_int32_t *versionp) {
    {
	char magic[8];
	int r=fread(magic, 1, 8, f);
	if (r!=8) {
	    return DB_BADFORMAT;
	}
	if (memcmp(magic, "tokulogg", 8)!=0) {
	    return DB_BADFORMAT;
	}
    }
    {
	int version;
    	int r=fread(&version, 1, 4, f);
	if (r!=4) {
	    return DB_BADFORMAT;
	}
	//printf("tokulog v.%d\n", ntohl(version));
	*versionp=ntohl(version);
    }
    return 0;
}

TXNID toku_txn_get_txnid (TOKUTXN txn) {
    if (txn==0) return 0;
    else return txn->txnid64;
}

LSN toku_txn_get_last_lsn (TOKUTXN txn) {
    if (txn==0) return (LSN){0};
    return txn->last_lsn;
}
LSN toku_logger_last_lsn(TOKULOGGER logger) {
    LSN result=logger->lsn;
    result.lsn--;
    return result;
}

TOKULOGGER toku_txn_logger (TOKUTXN txn) {
    return txn ? txn->logger : 0;
}

int toku_abort_logentry_commit (struct logtype_commit *le __attribute__((__unused__)), TOKUTXN txn) {
    toku_logger_panic(txn->logger, EINVAL);
    return EINVAL;
}

int toku_logger_abort(TOKUTXN txn) {
    //printf("%s:%d aborting\n", __FILE__, __LINE__);
    // Must undo everything.  Must undo it all in reverse order.
    // Build the reverse list
    //printf("%s:%d abort\n", __FILE__, __LINE__);
    {
	int r = toku_log_xabort(txn->logger, (LSN*)0, 0, txn->txnid64);
	if (r!=0) return r;
    }
    {
	struct roll_entry *item;
	while ((item=txn->newest_logentry)) {
	    txn->newest_logentry = item->prev;
	    int r = toku_abort_rollback_item(txn, item);
	    if (r!=0) { cleanup_txn(txn); return r; }
	}
    }
    list_remove(&txn->live_txns_link);
    // Read stuff out of the file and roll it back.
    if (txn->rollentry_filename) {
        int r = toku_rollback_fileentries(txn->rollentry_fd, txn->rollentry_filesize, txn);
        assert(r==0);
    }
    cleanup_txn(txn);
    return 0;
}

int toku_txnid2txn (TOKULOGGER logger, TXNID txnid, TOKUTXN *result) {
    if (logger==0) return -1;
    struct list *l;
    for (l = list_head(&logger->live_txns); l != &logger->live_txns; l = l->next) {
	TOKUTXN txn = list_struct(l, struct tokutxn, live_txns_link);
	assert(txn->tag==TYP_TOKUTXN);
	if (txn->txnid64==txnid) {
	    *result = txn;
	    return 0;
	}
    }
    // If there is no txn, then we treat it as the null txn.
    *result = 0;
    return 0;
}

int toku_set_func_fsync (int (*fsync_function)(int)) {
    toku_os_fsync_function = fsync_function;
    return 0;
}

// Find the earliest LSN in a log
static int peek_at_log (TOKULOGGER logger, char* filename, LSN *first_lsn) {
    logger=logger;
    int fd = open(filename, O_RDONLY);
    if (fd<0) { 
        if (logger->write_log_files) printf("couldn't open: %s\n", strerror(errno)); 
        return errno; 
    }
    enum { SKIP = 12+1+4 }; // read the 12 byte header, the first cmd, and the first len
    unsigned char header[SKIP+8];
    int r = read(fd, header, SKIP+8);
    if (r!=SKIP+8) return 0; // cannot determine that it's archivable, so we'll assume no.  If a later-log is archivable is then this one will be too.
    u_int64_t lsn = 0;
    int i;
    for (i=0; i<8; i++) lsn=(lsn<<8)+header[SKIP+i];

    r=close(fd);
    if (r!=0) { return 0; }

    first_lsn->lsn=lsn;
    return 0;
}

// Return a malloc'd array of malloc'd strings which are the filenames that can be archived.
int toku_logger_log_archive (TOKULOGGER logger, char ***logs_p, int flags) {
    if (flags!=0) return EINVAL; // don't know what to do.
    int all_n_logs;
    int i;
    char **all_logs;
    int r = toku_logger_find_logfiles (logger->directory, &all_logs);
    if (r!=0) return r;

    for (i=0; all_logs[i]; i++);
    all_n_logs=i;
    // get them into increasing order
    qsort(all_logs, all_n_logs, sizeof(all_logs[0]), logfilenamecompare);

    LSN oldest_live_txn_lsn={1LL<<63};
    {
	struct list *l;
	for (l=list_head(&logger->live_txns); l!=&logger->live_txns; l=l->next) {
	    TOKUTXN txn = list_struct(l, struct tokutxn, live_txns_link);
	    if (oldest_live_txn_lsn.lsn>txn->txnid64) {
		oldest_live_txn_lsn.lsn=txn->txnid64;
	    }
	}
    }
    //printf("%s:%d Oldest txn is %lld\n", __FILE__, __LINE__, (long long)oldest_live_txn_lsn.lsn);

    // Now starting at the last one, look for archivable ones.
    // Count the total number of bytes, because we have to return a single big array.  (That's the BDB interface.  Bleah...)
    LSN earliest_lsn_seen={(unsigned long long)(-1LL)};
    r = peek_at_log(logger, all_logs[all_n_logs-1], &earliest_lsn_seen); // try to find the lsn that's in the most recent log
    if ((earliest_lsn_seen.lsn <= logger->checkpoint_lsns[0].lsn)&&
	(earliest_lsn_seen.lsn <= logger->checkpoint_lsns[1].lsn)&&
	(earliest_lsn_seen.lsn <= oldest_live_txn_lsn.lsn)) {
	i=all_n_logs-1;
    } else {
	for (i=all_n_logs-2; i>=0; i--) { // start at all_n_logs-2 because we never archive the most recent log
	    r = peek_at_log(logger, all_logs[i], &earliest_lsn_seen);
	    if (r!=0) continue; // In case of error, just keep going
	    
	    //printf("%s:%d file=%s firstlsn=%lld checkpoint_lsns={%lld %lld}\n", __FILE__, __LINE__, all_logs[i], (long long)earliest_lsn_seen.lsn, (long long)logger->checkpoint_lsns[0].lsn, (long long)logger->checkpoint_lsns[1].lsn);
	    if ((earliest_lsn_seen.lsn <= logger->checkpoint_lsns[0].lsn)&&
		(earliest_lsn_seen.lsn <= logger->checkpoint_lsns[1].lsn)&&
		(earliest_lsn_seen.lsn <= oldest_live_txn_lsn.lsn)) {
		break;
	    }
	}
    }
    
    // all log files up to, but but not including, i can be archived.
    int n_to_archive=i;
    int count_bytes=0;
    for (i=0; i<n_to_archive; i++) {
	count_bytes+=1+strlen(all_logs[i]);
    }
    char **result;
    if (i==0) {
	result=0;
    } else {
	result = toku_malloc((1+n_to_archive)*sizeof(*result) + count_bytes);
	char  *base = (char*)(result+1+n_to_archive);
	for (i=0; i<n_to_archive; i++) {
	    int len=1+strlen(all_logs[i]);
	    result[i]=base;
	    memcpy(base, all_logs[i], len);
	    base+=len;
	}
	result[n_to_archive]=0;
    }
    for (i=0; all_logs[i]; i++) {
	free(all_logs[i]);
    }
    free(all_logs);
    *logs_p = result;
    return 0;
}

int toku_maybe_spill_rollbacks (TOKUTXN txn) {
    if (txn->rollentry_resident_bytecount>1<<20) {
	struct roll_entry *item;
	ssize_t bufsize = txn->rollentry_resident_bytecount;
	char *MALLOC_N(bufsize, buf);
	if (bufsize==0) return errno;
	struct wbuf w;
	wbuf_init(&w, buf, bufsize);
	while ((item=txn->oldest_logentry)) {
	    assert(item->prev==0);
	    u_int32_t rollback_fsize = toku_logger_rollback_fsize(item);
	    txn->rollentry_resident_bytecount -= rollback_fsize;
	    txn->oldest_logentry = item->next;
	    if (item->next) { item->next->prev=0; }
	    toku_logger_rollback_wbufwrite(&w, item);
	}
	assert(txn->rollentry_resident_bytecount==0);
	assert((ssize_t)w.ndone==bufsize);
	txn->oldest_logentry = txn->newest_logentry = 0;
	if (txn->rollentry_fd<0) {
	    const char filenamepart[] = "/__rolltmp.XXXXXX";
	    char  fnamelen = strlen(txn->logger->directory)+sizeof(filenamepart); 
	    assert(txn->rollentry_filename==0);
	    txn->rollentry_filename = toku_malloc(fnamelen);
	    if (txn->rollentry_filename==0) return errno;
	    snprintf(txn->rollentry_filename, fnamelen, "%s/__rolltmp.XXXXXX", txn->logger->directory);
	    txn->rollentry_fd = mkstemp(txn->rollentry_filename);
	    if (txn->rollentry_fd==-1) return errno;
	}
	ssize_t r = write_it(txn->rollentry_fd, buf, w.ndone);
	if (r<0) return r;
	assert(r==(ssize_t)w.ndone);
	txn->rollentry_filesize+=w.ndone;
	toku_free(buf);

	// Cleanup the rollback memory
	memarena_clear(txn->rollentry_arena);
    }
    return 0;
}

int toku_read_rollback_backwards(BREAD br, struct roll_entry **item, MEMARENA ma) {
    u_int32_t nbytes_n; ssize_t sr;
    if ((sr=bread_backwards(br, &nbytes_n, 4))!=4) { assert(sr<0); return errno; }
    u_int32_t n_bytes=ntohl(nbytes_n);
    unsigned char *buf = malloc_in_memarena(ma, n_bytes);
    if (buf==0) return errno;
    if ((sr=bread_backwards(br, buf, n_bytes-4))!=(ssize_t)n_bytes-4) { assert(sr<0); return errno; }
    int r = toku_parse_rollback(buf, n_bytes, item, ma);
    if (r!=0) return r;
    return 0;
}

static int find_xid (OMTVALUE v, void *txnv) {
    TOKUTXN txn = v;
    TOKUTXN txnfind = txnv;
    return txn->txnid64 - txnfind->txnid64;
}

static int find_filenum (OMTVALUE v, void *brtv) {
    BRT brt     = v;
    BRT brtfind = brtv;
    FILENUM fnum     = toku_cachefile_filenum(brt    ->cf);
    FILENUM fnumfind = toku_cachefile_filenum(brtfind->cf);
    if (fnum.fileid<fnumfind.fileid) return -1;
    if (fnum.fileid>fnumfind.fileid) return +1;
    return 0;
}


int toku_txn_note_brt (TOKUTXN txn, BRT brt) {
    OMTVALUE txnv;
    u_int32_t index;
    int r = toku_omt_find_zero(brt->txns, find_xid, txn, &txnv, &index, NULL);
    if (r==0) {
	// It's already there.
	assert((TOKUTXN)txnv==txn);
	return 0;
    }
    // Otherwise it's not there.
    r = toku_omt_insert_at(brt->txns, txn, index);
    assert(r==0);
    r = toku_omt_insert(txn->open_brts, brt, find_filenum, brt, 0);
    assert(r==0);
    return 0;
}

static int remove_brt (OMTVALUE txnv, u_int32_t UU(idx), void *brtv) {
    TOKUTXN txn = txnv;
    BRT     brt = brtv;
    OMTVALUE brtv_again=NULL;
    u_int32_t index;
    int r = toku_omt_find_zero(txn->open_brts, find_filenum, brt, &brtv_again, &index, NULL);
    assert(r==0);
    assert((void*)brtv_again==brtv);
    r = toku_omt_delete_at(txn->open_brts, index);
    assert(r==0);
    return 0;
}

int toku_txn_note_close_brt (BRT brt) {
    int r = toku_omt_iterate(brt->txns, remove_brt, brt);
    assert(r==0);
    return 0;
}

static int remove_txn (OMTVALUE brtv, u_int32_t UU(idx), void *txnv) {
    BRT brt     = brtv;
    TOKUTXN txn = txnv;
    OMTVALUE txnv_again=NULL;
    u_int32_t index;
    int r = toku_omt_find_zero(brt->txns, find_xid, txn, &txnv_again, &index, NULL);
    assert(r==0);
    assert((void*)txnv_again==txnv);
    r = toku_omt_delete_at(brt->txns, index);
    assert(r==0);
    return 0;
}

// for every BRT in txn, remove it.
static void note_txn_closing (TOKUTXN txn) {
    toku_omt_iterate(txn->open_brts, remove_txn, txn);
    toku_omt_destroy(&txn->open_brts);
}

int toku_txn_find_by_xid (BRT brt, TXNID xid, TOKUTXN *txnptr) {
    struct tokutxn fake_txn; fake_txn.txnid64 = xid;
    u_int32_t index;
    OMTVALUE txnv;
    int r = toku_omt_find_zero(brt->txns, find_xid, &fake_txn, &txnv, &index, NULL);
    if (r == 0) *txnptr = txnv;
    return r;
}
