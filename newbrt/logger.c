/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id: log.c 12375 2009-05-28 14:14:47Z yfogel $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"

static const int log_format_version=TOKU_LOG_VERSION;

static toku_pthread_mutex_t logger_mutex = TOKU_PTHREAD_MUTEX_INITIALIZER;

static int (*toku_os_fsync_function)(int)=fsync;
static int open_logfile (TOKULOGGER logger);
static int delete_logfile(TOKULOGGER logger, long long index);
static int do_write (TOKULOGGER logger, int do_fsync);

int toku_logger_create (TOKULOGGER *resultp) {
    int r;
    TAGMALLOC(TOKULOGGER, result);
    if (result==0) return errno;
    result->is_open=0;
    result->is_panicked=0;
    result->write_log_files = TRUE;
    result->lg_max = 100<<20; // 100MB default
    result->head = result->tail = 0;
    result->lsn = result->written_lsn = result->fsynced_lsn = (LSN){0};
    r = toku_omt_create(&result->live_txns); if (r!=0) goto died0;
    result->n_in_buf=0;
    result->n_in_file=0;
    result->directory=0;
    result->checkpoint_lsn=(LSN){0};
    result->oldest_living_xid = TXNID_NONE_LIVING;
    result->write_block_size = BRT_DEFAULT_NODE_SIZE; // default logging size is the same as the default brt block size
    toku_logfilemgr_create(&result->logfilemgr);
    *resultp=result;
    r = ml_init(&result->input_lock); if (r!=0) goto died1;
    r = ml_init(&result->output_lock); if (r!=0) goto died2;
    return 0;

 died2:
    ml_destroy(&result->input_lock);
 died1:
    toku_omt_destroy(&result->live_txns);
 died0:
    toku_free(result);
    return r;
}

int toku_logger_open (const char *directory, TOKULOGGER logger) {
    if (logger->is_open) return EINVAL;
    if (logger->is_panicked) return EINVAL;

    int r;
    r = toku_logfilemgr_init(logger->logfilemgr, directory);
    if ( r!=0 ) 
        return r;
    logger->lsn = toku_logfilemgr_get_last_lsn(logger->logfilemgr);
    //printf("starting after LSN=%lu\n", logger->lsn.lsn);
    logger->written_lsn = logger->lsn;
    logger->fsynced_lsn = logger->lsn;

    long long nexti;
    r = toku_logger_find_next_unused_log_file(directory, &nexti);
    if (r!=0) return r;
    if (toku_os_is_absolute_name(directory)) {
        logger->directory = toku_strdup(directory);
    } else {
        char *cwd = getcwd(NULL, 0);
        if (cwd == NULL)
            return -1;
        char *new_log_dir = toku_malloc(strlen(cwd) + strlen(directory) + 2);
        if (new_log_dir == NULL)
            return -2;
        sprintf(new_log_dir, "%s/%s", cwd, directory);
        logger->directory = new_log_dir;
        toku_free(cwd);
    }
    if (logger->directory==0) return errno;
    logger->next_log_file_number = nexti;
    open_logfile(logger);

    logger->is_open = 1;
    return 0;
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
		if (logger->write_log_files) {
                    r = toku_os_fsync_function(logger->fd); assert(r == 0);
                }
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
// But grab the locks just to be careful, including one to prevent access
// between unlocking and destroying.
int toku_logger_close(TOKULOGGER *loggerp) {
    TOKULOGGER logger = *loggerp;
    if (logger->is_panicked) return EINVAL;
    int r = 0;
    int locked_logger = 0;
    if (!logger->is_open) goto is_closed;
    r = ml_lock(&logger->output_lock); if (r!=0) goto panic;
    r = ml_lock(&logger->input_lock);  if (r!=0) goto panic;
    r = toku_pthread_mutex_lock(&logger_mutex); if (r!=0) goto panic;
    locked_logger = 1;
    r = do_write(logger, 1);           if (r!=0) goto panic; //Releases the input lock
    if (logger->fd!=-1) {
	r = close(logger->fd);         if (r!=0) { r=errno; goto panic; }
    }
    logger->fd=-1;

    r = ml_unlock(&logger->output_lock);  if (r!=0) goto panic;
 is_closed:
    r = ml_destroy(&logger->output_lock); if (r!=0) goto panic;
    r = ml_destroy(&logger->input_lock);  if (r!=0) goto panic;
    logger->is_panicked=1; // Just in case this might help.
    if (logger->directory) toku_free(logger->directory);
    toku_omt_destroy(&logger->live_txns);
    toku_logfilemgr_destroy(&logger->logfilemgr);
    toku_free(logger);
    *loggerp=0;
    if (locked_logger) {
        r = toku_pthread_mutex_unlock(&logger_mutex); if (r!=0) goto panic;
    }
    return r;
 panic:
    toku_logger_panic(logger, r);
    return r;
}

int toku_logger_shutdown(TOKULOGGER logger) {
    int r = 0;
    if (logger->is_open) {
        if (toku_omt_size(logger->live_txns) == 0)
            r = toku_log_shutdown(logger, NULL, TRUE, 0);
    }
    return r;
}

#if 0
int toku_logger_log_checkpoint (TOKULOGGER logger) {
    if (logger->is_panicked) return EINVAL;
    int r = toku_cachetable_checkpoint(logger->ct);
    if (r!=0) return r;
    logger->checkpoint_lsns[1]=logger->checkpoint_lsns[0];
    logger->checkpoint_lsns[0]=logger->lsn;
    return toku_log_checkpoint(logger, (LSN*)0, 1);
}
#endif

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

void toku_logger_set_cachetable (TOKULOGGER logger, CACHETABLE ct) {
    logger->ct = ct;
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

int toku_logger_set_lg_bsize(TOKULOGGER logger, u_int32_t bsize) {
    if (logger==0) return EINVAL; // no logger
    if (logger->is_panicked) return EINVAL;
    if (logger->is_open) return EINVAL;
    if (bsize<=0 || bsize>(1<<30)) return EINVAL;
    logger->write_block_size = bsize;
    return 0;
}

int toku_logger_lock_init(void) {
    int r = toku_pthread_mutex_init(&logger_mutex, NULL);
    assert(r == 0);
    return r;
}

int toku_logger_lock_destroy(void) {
    int r = toku_pthread_mutex_destroy(&logger_mutex);
    assert(r == 0);
    return r;
}

int toku_logger_find_next_unused_log_file(const char *directory, long long *result) {
    DIR *d=opendir(directory);
    long long max=-1; *result = max;
    struct dirent *de;
    if (d==0) return errno;
    while ((de=readdir(d))) {
	if (de==0) return errno;
	long long thisl;
	int r = sscanf(de->d_name, "log%lld.tokulog", &thisl);
	if (r==1 && thisl>max) max=thisl;
    }
    *result=max+1;
    int r = closedir(d);
    return r;
}


static int logfilenamecompare (const void *ap, const void *bp) {
    char *a=*(char**)ap;
    char *b=*(char**)bp;
    return strcmp(a,b);
}

// Return the log files in sorted order
// Return a null_terminated array of strings, and also return the number of strings in the array.
int toku_logger_find_logfiles (const char *directory, char ***resultp, int *n_logfiles) {
    int result_limit=2;
    int n_results=0;
    char **MALLOC_N(result_limit, result);
    assert(result!= NULL);
    struct dirent *de;
    DIR *d=opendir(directory);
    if (d==0) {
        toku_free(result);
        return errno;
    }
    int dirnamelen = strlen(directory);
    while ((de=readdir(d))) {
	long long thisl;
	int r = sscanf(de->d_name, "log%lld.tokulog", &thisl);
	if (r!=1) continue; // Skip over non-log files.
	if (n_results+1>=result_limit) {
	    result_limit*=2;
	    result = toku_realloc(result, result_limit*sizeof(*result));
            // should we try to recover here?
            assert(result!=NULL);
	}
	int fnamelen = dirnamelen + strlen(de->d_name) + 2; // One for the slash and one for the trailing NUL.
	char *fname = toku_malloc(fnamelen);
        assert(fname!=NULL);
	snprintf(fname, fnamelen, "%s/%s", directory, de->d_name);
	result[n_results++] = fname;
    }
    // Return them in increasing order.
    qsort(result, n_results, sizeof(result[0]), logfilenamecompare);
    *resultp    = result;
    *n_logfiles = n_results;
    result[n_results]=0; // make a trailing null
    return d ? closedir(d) : 0;
}

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
    snprintf(fname, fnamelen, "%s/log%012lld.tokulog", logger->directory, logger->next_log_file_number);
    long long index = logger->next_log_file_number;
    if (logger->write_log_files) {
        logger->fd = open(fname, O_CREAT+O_WRONLY+O_TRUNC+O_EXCL+O_BINARY, S_IRWXU);     
        if (logger->fd==-1) return errno;
        logger->next_log_file_number++;
    } else {
        logger->fd = open(DEV_NULL_FILE, O_WRONLY+O_BINARY);
        // printf("%s: %s %d\n", __FUNCTION__, DEV_NULL_FILE, logger->fd); fflush(stdout);
        if (logger->fd==-1) return errno;
    }
    r = write_it(logger->fd, "tokulogg", 8);             if (r!=8) return errno;
    int version_l = toku_htonl(log_format_version); //version MUST be in network byte order regardless of disk order
    r = write_it(logger->fd, &version_l, 4);             if (r!=4) return errno;
    if ( logger->write_log_files ) {
        TOKULOGFILEINFO lf_info = toku_malloc(sizeof(struct toku_logfile_info));
        if (lf_info == NULL) 
            return ENOMEM;
        lf_info->index = index;
        lf_info->maxlsn = logger->written_lsn; // ?? not sure this is right, but better than 0 - DSW
        toku_logfilemgr_add_logfile_info(logger->logfilemgr, lf_info);
    }
    logger->fsynced_lsn = logger->written_lsn;
    logger->n_in_file = 12;
    return 0;
}

static int delete_logfile(TOKULOGGER logger, long long index) {
    int fnamelen = strlen(logger->directory)+50;
    char fname[fnamelen];
    snprintf(fname, fnamelen, "%s/log%012lld.tokulog", logger->directory, index);
    int r = remove(fname);
    return r;
}

int toku_logger_maybe_trim_log(TOKULOGGER logger, LSN trim_lsn) {
    int r=0;
    TOKULOGFILEMGR lfm = logger->logfilemgr;
    int n_logfiles = toku_logfilemgr_num_logfiles(lfm);

    TOKULOGFILEINFO lf_info = NULL;
    
    if ( logger->write_log_files ) {
        while ( n_logfiles > 1 ) { // don't delete current logfile
            lf_info = toku_logfilemgr_get_oldest_logfile_info(lfm);
            if ( lf_info->maxlsn.lsn > trim_lsn.lsn ) {
                // file contains an open LSN, can't delete this or any newer log files
                break;
            }
            // need to save copy - toku_logfilemgr_delete_oldest_logfile_info free's the lf_info
            long index = lf_info->index;
            toku_logfilemgr_delete_oldest_logfile_info(lfm);
            n_logfiles--;
            r = delete_logfile(logger, index);
            if (r!=0) {
                return r;
            }
        }
    }
    return r;
}

static int close_and_open_logfile (TOKULOGGER logger) {
    int r;
    if (logger->write_log_files) {
        r = toku_os_fsync_function(logger->fd);          if (r!=0) return errno;
    }
    r = close(logger->fd);                               if (r!=0) return errno;
    return open_logfile(logger);
}

void toku_logger_write_log_files (TOKULOGGER logger, BOOL write_log_files) {
    assert(!logger->is_open);
    logger->write_log_files = write_log_files;
}

// Enter holding both locks
// Exit holding only the output_lock
static int do_write (TOKULOGGER logger, int do_fsync) {
    int r;
    struct logbytes *list = logger->head;
    logger->head=logger->tail=0;
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
    r=ml_unlock(&logger->input_lock); if (r!=0) goto panic2;
    if (do_fsync) {
        if (logger->write_log_files) {
            r = toku_os_fsync_function(logger->fd); assert(r == 0);
        }
	logger->fsynced_lsn = logger->written_lsn;
    }
    if ( logger->write_log_files ) 
        toku_logfilemgr_update_last_lsn(logger->logfilemgr, logger->written_lsn);
    return 0;
 panic:
    ml_unlock(&logger->input_lock);
 panic2:
    toku_logger_panic(logger, r);
    return r;
}

int toku_logger_restart(TOKULOGGER logger, LSN lastlsn) {
    int r;

    // flush out the log buffer
    r = ml_lock(&logger->output_lock); assert(r == 0);
    r = ml_lock(&logger->input_lock); assert(r == 0);
    r = do_write(logger, TRUE); assert(r == 0);
    r = ml_unlock(&logger->output_lock); assert(r == 0);

    // close the log file
    r = close(logger->fd); assert(r == 0);
    logger->fd = -1;

    // reset the LSN's to the lastlsn when the logger was opened
    logger->lsn = logger->written_lsn = logger->fsynced_lsn = lastlsn;
    logger->write_log_files = TRUE;

    // open a new log file
    return open_logfile(logger);
}

int toku_logger_log_fcreate (TOKUTXN txn, const char *fname, FILENUM filenum, u_int32_t mode, u_int32_t treeflags) {
    if (txn==0) return 0;
    if (txn->logger->is_panicked) return EINVAL;
    BYTESTRING bs = { .len=strlen(fname), .data = toku_strdup_in_rollback(txn, fname) };
    int r = toku_log_fcreate (txn->logger, (LSN*)0, 0, toku_txn_get_txnid(txn), filenum, bs, mode, treeflags);
    if (r!=0) return r;
    r = toku_logger_save_rollback_fcreate(txn, toku_txn_get_txnid(txn), filenum, bs);
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

static int toku_fread_u_int8_t_nocrclen (FILE *f, u_int8_t *v) {
    int vi=fgetc(f);
    if (vi==EOF) return -1;
    u_int8_t vc=(u_int8_t)vi;
    *v = vc;
    return 0;
}

int toku_fread_u_int8_t (FILE *f, u_int8_t *v, struct x1764 *mm, u_int32_t *len) {
    int vi=fgetc(f);
    if (vi==EOF) return -1;
    u_int8_t vc=(u_int8_t)vi;
    x1764_add(mm, &vc, 1);
    (*len)++;
    *v = vc;
    return 0;
}

int toku_fread_u_int32_t_nocrclen (FILE *f, u_int32_t *v) {
    u_int32_t result;
    u_int8_t *cp = (u_int8_t*)&result;
    int r;
    r = toku_fread_u_int8_t_nocrclen (f, cp+0); if (r!=0) return r;
    r = toku_fread_u_int8_t_nocrclen (f, cp+1); if (r!=0) return r;
    r = toku_fread_u_int8_t_nocrclen (f, cp+2); if (r!=0) return r;
    r = toku_fread_u_int8_t_nocrclen (f, cp+3); if (r!=0) return r;
    *v = toku_dtoh32(result);

    return 0;
}
int toku_fread_u_int32_t (FILE *f, u_int32_t *v, struct x1764 *checksum, u_int32_t *len) {
    u_int32_t result;
    u_int8_t *cp = (u_int8_t*)&result;
    int r;
    r = toku_fread_u_int8_t (f, cp+0, checksum, len); if(r!=0) return r;
    r = toku_fread_u_int8_t (f, cp+1, checksum, len); if(r!=0) return r;
    r = toku_fread_u_int8_t (f, cp+2, checksum, len); if(r!=0) return r;
    r = toku_fread_u_int8_t (f, cp+3, checksum, len); if(r!=0) return r;
    *v = toku_dtoh32(result);
    return 0;
}

static int toku_fread_int32_t (FILE *f, int32_t *v, struct x1764 *checksum, u_int32_t *len) {
    u_int32_t uv;
    int r = toku_fread_u_int32_t(f, &uv, checksum, len);
    int32_t rv = uv;
    if (r==0) *v=rv;
    return r;
}

int toku_fread_u_int64_t (FILE *f, u_int64_t *v, struct x1764 *checksum, u_int32_t *len) {
    u_int32_t v1,v2;
    int r;
    r=toku_fread_u_int32_t(f, &v1, checksum, len);    if (r!=0) return r;
    r=toku_fread_u_int32_t(f, &v2, checksum, len);    if (r!=0) return r;
    *v = (((u_int64_t)v1)<<32 ) | ((u_int64_t)v2);
    return 0;
}
int toku_fread_LSN     (FILE *f, LSN *lsn, struct x1764 *checksum, u_int32_t *len) {
    return toku_fread_u_int64_t (f, &lsn->lsn, checksum, len);
}
int toku_fread_FILENUM (FILE *f, FILENUM *filenum, struct x1764 *checksum, u_int32_t *len) {
    return toku_fread_u_int32_t (f, &filenum->fileid, checksum, len);
}
int toku_fread_DISKOFF (FILE *f, DISKOFF *diskoff, struct x1764 *checksum, u_int32_t *len) {
    int r = toku_fread_u_int64_t (f, (u_int64_t*)diskoff, checksum, len); // sign conversion will be OK.
    return r;
}
int toku_fread_BLOCKNUM (FILE *f, BLOCKNUM *blocknum, struct x1764 *checksum, u_int32_t *len) {
    u_int64_t b = 0;
    int r = toku_fread_u_int64_t (f, &b, checksum, len); // sign conversion will be OK.
    blocknum->b=b;
    return r;
}
int toku_fread_TXNID   (FILE *f, TXNID *txnid, struct x1764 *checksum, u_int32_t *len) {
    return toku_fread_u_int64_t (f, txnid, checksum, len);
}

// fills in the bs with malloced data.
int toku_fread_BYTESTRING (FILE *f, BYTESTRING *bs, struct x1764 *checksum, u_int32_t *len) {
    int r=toku_fread_u_int32_t(f, (u_int32_t*)&bs->len, checksum, len);
    if (r!=0) return r;
    bs->data = toku_malloc(bs->len);
    u_int32_t i;
    for (i=0; i<bs->len; i++) {
	r=toku_fread_u_int8_t(f, (u_int8_t*)&bs->data[i], checksum, len);
	if (r!=0) {
	    toku_free(bs->data);
	    bs->data=0;
	    return r;
	}
    }
    return 0;
}

int toku_fread_LOGGEDBRTHEADER (FILE *f, LOGGEDBRTHEADER *v, struct x1764 *checksum, u_int32_t *len) {
    int r;
    r = toku_fread_u_int32_t(f, &v->size,          checksum, len); if (r!=0) return r;
    r = toku_fread_u_int32_t(f, &v->flags,         checksum, len); if (r!=0) return r;
    r = toku_fread_u_int32_t(f, &v->nodesize,      checksum, len); if (r!=0) return r;
    r = toku_fread_BLOCKNUM (f, &v->free_blocks,   checksum, len); if (r!=0) return r;
    r = toku_fread_BLOCKNUM (f, &v->unused_blocks, checksum, len); if (r!=0) return r;
    r = toku_fread_BLOCKNUM (f, &v->root,          checksum, len); if (r!=0) return r;
    r = toku_fread_BLOCKNUM (f, &v->btt_size,      checksum, len); if (r!=0) return r;
    r = toku_fread_DISKOFF  (f, &v->btt_diskoff,   checksum, len); if (r!=0) return r;
    XMALLOC_N(v->btt_size.b, v->btt_pairs);
    int64_t i;
    for (i=0; i<v->btt_size.b; i++) {
	r = toku_fread_DISKOFF(f, &v->btt_pairs[i].off, checksum, len);
	if (r!=0) { toku_free(v->btt_pairs); return r; }
	r = toku_fread_int32_t (f, &v->btt_pairs[i].size, checksum, len);
	if (r!=0) { toku_free(v->btt_pairs); return r; }
    }
    return 0;
}

int toku_fread_INTPAIRARRAY (FILE *f, INTPAIRARRAY *v, struct x1764 *checksum, u_int32_t *len) {
    int r;
    u_int32_t i;
    r = toku_fread_u_int32_t(f, &v->size, checksum, len); if (r!=0) return r;
    MALLOC_N(v->size, v->array);
    if (v->array==0) return errno;
    for (i=0; i<v->size; i++) {
	r = toku_fread_u_int32_t(f, &v->array[i].a, checksum, len); if (r!=0) return r;
	r = toku_fread_u_int32_t(f, &v->array[i].b, checksum, len); if (r!=0) return r;
    }
    return 0;
}

int toku_logprint_LSN (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format __attribute__((__unused__))) {
    LSN v;
    int r = toku_fread_LSN(inf, &v, checksum, len);
    if (r!=0) return r;
    fprintf(outf, " %s=%" PRIu64, fieldname, v.lsn);
    return 0;
}
int toku_logprint_TXNID (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format __attribute__((__unused__))) {
    TXNID v;
    int r = toku_fread_TXNID(inf, &v, checksum, len);
    if (r!=0) return r;
    fprintf(outf, " %s=%" PRIu64, fieldname, v);
    return 0;
}

int toku_logprint_u_int8_t (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format) {
    u_int8_t v;
    int r = toku_fread_u_int8_t(inf, &v, checksum, len);
    if (r!=0) return r;
    fprintf(outf, " %s=%d", fieldname, v);
    if (format) fprintf(outf, format, v);
    else if (v=='\'') fprintf(outf, "('\'')");
    else if (isprint(v)) fprintf(outf, "('%c')", v);
    else {}/*nothing*/
    return 0;

}

int toku_logprint_u_int32_t (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format) {
    u_int32_t v;
    int r = toku_fread_u_int32_t(inf, &v, checksum, len);
    if (r!=0) return r;
    fprintf(outf, " %s=", fieldname);
    fprintf(outf, format ? format : "%d", v);
    return 0;
}

int toku_logprint_u_int64_t (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format) {
    u_int64_t v;
    int r = toku_fread_u_int64_t(inf, &v, checksum, len);
    if (r!=0) return r;
    fprintf(outf, " %s=", fieldname);
    fprintf(outf, format ? format : "%"PRId64, v);
    return 0;
}

void toku_print_BYTESTRING (FILE *outf, u_int32_t len, char *data) {
    fprintf(outf, "{len=%u data=\"", len);
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

int toku_logprint_BYTESTRING (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format __attribute__((__unused__))) {
    BYTESTRING bs;
    int r = toku_fread_BYTESTRING(inf, &bs, checksum, len);
    if (r!=0) return r;
    fprintf(outf, " %s=", fieldname);
    toku_print_BYTESTRING(outf, bs.len, bs.data);
    toku_free(bs.data);
    return 0;
}
int toku_logprint_FILENUM (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format) {
    return toku_logprint_u_int32_t(outf, inf, fieldname, checksum, len, format);

}
int toku_logprint_DISKOFF (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format __attribute__((__unused__))) {
    DISKOFF v;
    int r = toku_fread_DISKOFF(inf, &v, checksum, len);
    if (r!=0) return r;
    fprintf(outf, " %s=%" PRId64, fieldname, v);
    return 0;
}
int toku_logprint_BLOCKNUM (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format __attribute__((__unused__))) {
    BLOCKNUM v;
    int r = toku_fread_BLOCKNUM(inf, &v, checksum, len);
    if (r!=0) return r;
    fprintf(outf, " %s=%"PRId64, fieldname, v.b);
    return 0;
}

int toku_logprint_LOGGEDBRTHEADER (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format __attribute__((__unused__))) {
    LOGGEDBRTHEADER v;
    int r = toku_fread_LOGGEDBRTHEADER(inf, &v, checksum, len);
    if (r!=0) return r;
    fprintf(outf, " %s={size=%u flags=%u nodesize=%u free_blocks=%" PRId64 " unused_memory=%" PRId64, fieldname, v.size, v.flags, v.nodesize, v.free_blocks.b, v.unused_blocks.b);
    fprintf(outf, " btt_size=%"  PRId64 " btt_diskoff=%" PRId64 " btt_pairs={", v.btt_size.b, v.btt_diskoff) ;
    int64_t i;
    for (i=0; i<v.btt_size.b; i++) {
	if (i>0) printf(" ");
	fprintf(outf, "%" PRId64 ",%d", v.btt_pairs[i].off, v.btt_pairs[i].size);
    }
    fprintf(outf, "}");
    return 0;

}

int toku_logprint_INTPAIRARRAY (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format __attribute__((__unused__))) {
    INTPAIRARRAY v;
    u_int32_t i;
    int r = toku_fread_INTPAIRARRAY(inf, &v, checksum, len);
    if (r!=0) return r;
    fprintf(outf, " %s={size=%u array={", fieldname, v.size);
    for (i=0; i<v.size; i++) {
	if (i!=0) fprintf(outf, " ");
	fprintf(outf, "{%u %u}", v.array[i].a, v.array[i].b);
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
	printf("tokulog v.%u\n", toku_ntohl(version));
        //version MUST be in network order regardless of disk order
	*versionp=toku_ntohl(version);
    }
    return 0;
}

int toku_read_logmagic (FILE *f, u_int32_t *versionp) {
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
	*versionp=toku_ntohl(version);
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
    return logger->lsn;
}

TOKULOGGER toku_txn_logger (TOKUTXN txn) {
    return txn ? txn->logger : 0;
}

//Heaviside function to search through an OMT by a TXNID
static int
find_by_xid (OMTVALUE v, void *txnidv) {
    TOKUTXN txn = v;
    TXNID   txnidfind = *(TXNID*)txnidv;
    if (txn->txnid64<txnidfind) return -1;
    if (txn->txnid64>txnidfind) return +1;
    return 0;
}

int toku_txnid2txn (TOKULOGGER logger, TXNID txnid, TOKUTXN *result) {
    if (logger==NULL) return -1;

    OMTVALUE txnfound;
    int rval;
    int r = toku_omt_find_zero(logger->live_txns, find_by_xid, &txnid, &txnfound, NULL, NULL);
    if (r==0) {
        TOKUTXN txn = txnfound;
        assert(txn->tag==TYP_TOKUTXN);
        assert(txn->txnid64==txnid);
        *result = txn;
        rval = 0;
    }
    else {
        assert(r==DB_NOTFOUND);
        // If there is no txn, then we treat it as the null txn.
        *result = NULL;
        rval    = 0;
    }
    return rval;
}

int toku_set_func_fsync (int (*fsync_function)(int)) {
    toku_os_fsync_function = fsync_function;
    return 0;
}

// Find the earliest LSN in a log
static int peek_at_log (TOKULOGGER logger, char* filename, LSN *first_lsn) {
    logger=logger;
    int fd = open(filename, O_RDONLY+O_BINARY);
    if (fd<0) {
        if (logger->write_log_files) printf("couldn't open: %s\n", strerror(errno));
        return errno;
    }
    enum { SKIP = 12+1+4 }; // read the 12 byte header, the first cmd, and the first len
    unsigned char header[SKIP+8];
    int r = read(fd, header, SKIP+8);
    if (r!=SKIP+8) return 0; // cannot determine that it's archivable, so we'll assume no.  If a later-log is archivable is then this one will be too.

    u_int64_t lsn;
    {
        struct rbuf rb;
        rb.buf   = header+SKIP;
        rb.size  = 8;
        rb.ndone = 0;
        lsn = rbuf_ulonglong(&rb);
    }

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
    int n_logfiles;
    int r = toku_logger_find_logfiles (logger->directory, &all_logs, &n_logfiles);
    if (r!=0) return r;

    for (i=0; all_logs[i]; i++);
    all_n_logs=i;
    // get them into increasing order
    qsort(all_logs, all_n_logs, sizeof(all_logs[0]), logfilenamecompare);

    LSN oldest_live_txn_lsn;
    {
        TXNID oldest_living_xid = toku_logger_get_oldest_living_xid(logger);
        if (oldest_living_xid == TXNID_NONE_LIVING)
            oldest_live_txn_lsn = MAX_LSN;
        else
            oldest_live_txn_lsn.lsn = oldest_living_xid;
    }

    //printf("%s:%d Oldest txn is %lld\n", __FILE__, __LINE__, (long long)oldest_live_txn_lsn.lsn);

    // Now starting at the last one, look for archivable ones.
    // Count the total number of bytes, because we have to return a single big array.  (That's the BDB interface.  Bleah...)
    LSN earliest_lsn_in_logfile={(unsigned long long)(-1LL)};
    r = peek_at_log(logger, all_logs[all_n_logs-1], &earliest_lsn_in_logfile); // try to find the lsn that's in the most recent log
    if ((earliest_lsn_in_logfile.lsn <= logger->checkpoint_lsn.lsn)&&
	(earliest_lsn_in_logfile.lsn <= oldest_live_txn_lsn.lsn)) {
	i=all_n_logs-1;
    } else {
	for (i=all_n_logs-2; i>=0; i--) { // start at all_n_logs-2 because we never archive the most recent log
	    r = peek_at_log(logger, all_logs[i], &earliest_lsn_in_logfile);
	    if (r!=0) continue; // In case of error, just keep going
	
	    //printf("%s:%d file=%s firstlsn=%lld checkpoint_lsns={%lld %lld}\n", __FILE__, __LINE__, all_logs[i], (long long)earliest_lsn_in_logfile.lsn, (long long)logger->checkpoint_lsns[0].lsn, (long long)logger->checkpoint_lsns[1].lsn);
	    if ((earliest_lsn_in_logfile.lsn <= logger->checkpoint_lsn.lsn)&&
		(earliest_lsn_in_logfile.lsn <= oldest_live_txn_lsn.lsn)) {
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
	toku_free(all_logs[i]);
    }
    toku_free(all_logs);
    *logs_p = result;
    return 0;
}


TOKUTXN toku_logger_txn_parent (TOKUTXN txn) {
    return txn->parent;
}

void toku_logger_note_checkpoint(TOKULOGGER logger, LSN lsn) {
    logger->checkpoint_lsn = lsn;
}

TXNID toku_logger_get_oldest_living_xid(TOKULOGGER logger) {
    TXNID rval = 0;
    if (logger)
        rval = logger->oldest_living_xid;
    return rval;
}

LSN toku_logger_get_oldest_living_lsn(TOKULOGGER logger) {
    LSN lsn = {0};
    if (logger) {
        if (logger->oldest_living_xid == TXNID_NONE_LIVING)
            lsn = MAX_LSN;
        else
            lsn.lsn = logger->oldest_living_xid;
    }
    return lsn;
}

