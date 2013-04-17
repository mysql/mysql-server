/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id: log.c 12375 2009-05-28 14:14:47Z yfogel $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"

static const int log_format_version=TOKU_LOG_VERSION;

static toku_pthread_mutex_t logger_mutex = TOKU_PTHREAD_MUTEX_INITIALIZER;
static u_int32_t logger_lock_ctr = 0;	// useful for debug at a live installation

static int open_logfile (TOKULOGGER logger);
static int toku_logger_write_buffer (TOKULOGGER logger, int do_fsync);
static int delete_logfile(TOKULOGGER logger, long long index);

u_int32_t toku_logger_get_lock_ctr(void) {
    return logger_lock_ctr;
}

int toku_logger_create (TOKULOGGER *resultp) {
    int r;
    TAGMALLOC(TOKULOGGER, result);
    if (result==0) return errno;
    result->is_open=FALSE;
    result->is_panicked=FALSE;
    result->panic_errno = 0;
    result->write_log_files = TRUE;
    result->trim_log_files = TRUE;
    result->directory=0;
    result->remove_finalize_callback = NULL;
    // fd is uninitialized on purpose
    // ct is uninitialized on purpose
    result->lg_max = 100<<20; // 100MB default
    // lsn is uninitialized
    r = toku_omt_create(&result->live_txns); if (r!=0) goto panic;
    result->inbuf  = (struct logbuf) {0, LOGGER_MIN_BUF_SIZE, toku_xmalloc(LOGGER_MIN_BUF_SIZE), ZERO_LSN};
    result->outbuf = (struct logbuf) {0, LOGGER_MIN_BUF_SIZE, toku_xmalloc(LOGGER_MIN_BUF_SIZE), ZERO_LSN};
    // written_lsn is uninitialized
    // fsynced_lsn is uninitialized
    result->checkpoint_lsn = ZERO_LSN;
    // next_log_file_number is uninitialized
    // n_in_file is uninitialized
    result->write_block_size = BRT_DEFAULT_NODE_SIZE; // default logging size is the same as the default brt block size
    result->oldest_living_xid = TXNID_NONE_LIVING;
    toku_logfilemgr_create(&result->logfilemgr);
    *resultp=result;
    r = ml_init(&result->input_lock);  if (r!=0) goto panic;
    r = ml_init(&result->output_lock); if (r!=0) goto panic;
    return 0;

 panic:
    toku_logger_panic(result, r);
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
    logger->written_lsn = logger->lsn;
    logger->fsynced_lsn = logger->lsn;
    logger->inbuf.max_lsn_in_buf  = logger->lsn;
    logger->outbuf.max_lsn_in_buf = logger->lsn;

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

// No locks held on entry
// No locks held on exit.
// Perhaps no locks are needed, since you cannot legally close the log concurrently with doing anything else.
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
    logger_lock_ctr++;
    locked_logger = 1;
    r = toku_logger_write_buffer(logger, 1);           if (r!=0) goto panic; //Releases the input lock
    if (logger->fd!=-1) {
	r = close(logger->fd);         if (r!=0) { r=errno; goto panic; }
    }
    logger->fd=-1;

    r = ml_unlock(&logger->output_lock);  if (r!=0) goto panic;
 is_closed:
    toku_free(logger->inbuf.buf);
    toku_free(logger->outbuf.buf);
    r = ml_destroy(&logger->output_lock); if (r!=0) goto panic;
    r = ml_destroy(&logger->input_lock);  if (r!=0) goto panic;
    logger->is_panicked=1; // Just in case this might help.
    if (logger->directory) toku_free(logger->directory);
    toku_omt_destroy(&logger->live_txns);
    toku_logfilemgr_destroy(&logger->logfilemgr);
    toku_free(logger);
    *loggerp=0;
    if (locked_logger) {
	logger_lock_ctr++;
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
        if (toku_omt_size(logger->live_txns) == 0) {
            BYTESTRING comment = { strlen("shutdown"), "shutdown" };
            r = toku_log_comment(logger, NULL, TRUE, 0, comment);
        }
    }
    return r;
}

// Write data to a file.  Keep trying even if partial writes occur.
// On error: Return negative with errno set.
// On success return nbytes.
static int write_it (int fd, const void *bufv, int nbytes) {
    toku_os_full_write(fd, bufv, nbytes);
    return nbytes;
}

// close the current file, and open the next one.
// Entry: The output lock is held.
// Exit:  The output lock is stlil held.
static int close_and_open_logfile (TOKULOGGER logger) {
    int r;
    if (logger->write_log_files) {
        r = toku_file_fsync(logger->fd);                 if (r!=0) return errno;
	assert(logger->fsynced_lsn.lsn <= logger->written_lsn.lsn);
	logger->fsynced_lsn = logger->written_lsn;
        toku_logfilemgr_update_last_lsn(logger->logfilemgr, logger->written_lsn);          // fixes t:2294
    }
    r = close(logger->fd);                               if (r!=0) return errno;
    return open_logfile(logger);
}

static int
max_int (int a, int b)
{
    if (a>b) return a;
    return b;
}

int
toku_logger_make_space_in_inbuf (TOKULOGGER logger, int n_bytes_needed)
// Entry: Holds the inlock
// Exit:  Holds the inlock
// Effect: Upon exit, the inlock is held and there are at least n_bytes_needed in the buffer.
//  May release the inlock, so this is not atomic.
// Implementation: Makes space in the inbuf, possibly by writing the inbuf to disk or increasing the size of the inbuf.  There is no fsync.
// Arguments:  logger:         the logger (side effects)
//             n_bytes_needed: how many bytes to make space for.
{
    int r;
    if (logger->inbuf.n_in_buf + n_bytes_needed <= LOGGER_MIN_BUF_SIZE) return 0;
    r = ml_unlock(&logger->input_lock);  if (r!=0) goto panic;
    r = ml_lock(&logger->output_lock);   if (r!=0) goto panic;
    r = ml_lock(&logger->input_lock);    if (r!=0) goto panic;
    // Some other thread may have written the log out while we didn't have the lock.  If we can squeeze it in now, then be happy
    if (logger->inbuf.n_in_buf + n_bytes_needed <= LOGGER_MIN_BUF_SIZE) return 0;
    if (logger->inbuf.n_in_buf > 0) {
	// There isn't enough space, and there is something in the buffer, so write the inbuf.
	{
	    struct logbuf tmp = logger->inbuf;
	    logger->inbuf = logger->outbuf;
	    logger->outbuf = tmp;
	    assert(logger->inbuf.n_in_buf == 0);
	}
	r = write_it(logger->fd, logger->outbuf.buf, logger->outbuf.n_in_buf);
	if (r!=logger->outbuf.n_in_buf) { r = errno; goto panic; }
	assert(logger->outbuf.max_lsn_in_buf.lsn > logger->written_lsn.lsn); // since there is something in the buffer, its LSN must be bigger than what's previously written.
	logger->written_lsn =  logger->outbuf.max_lsn_in_buf;
	logger->n_in_file   += logger->outbuf.n_in_buf;
	logger->outbuf.n_in_buf = 0;
	if (logger->n_in_file > logger->lg_max) {
	    r = close_and_open_logfile(logger);  if (r!=0) goto panic; // set
	}
    }
    // the inbuf is empty.  Is it big enough?
    if (n_bytes_needed > logger->inbuf.buf_size) {
	assert(n_bytes_needed < (1<<30)); // it seems unlikely to work if a logentry gets that big.
	int new_size = max_int(logger->inbuf.buf_size * 2, n_bytes_needed); // make it at least twice as big, and big enough for n_bytes
	assert(new_size < (1<<30));
	XREALLOC_N(new_size, logger->inbuf.buf);
	logger->inbuf.buf_size = new_size;
    }
    r = ml_unlock(&logger->output_lock);  if (r!=0) goto panic;
    return 0;
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
    r = ml_lock(&logger->output_lock);       if (r!=0) goto panic;
    r = ml_lock(&logger->input_lock);        if (r!=0) goto panic;
    r = toku_logger_write_buffer(logger, 1); if (r!=0) goto panic;
    r = ml_unlock(&logger->output_lock);     if (r!=0) goto panic;
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
    logger_lock_ctr = 0;
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
    long long maxf=-1; *result = maxf;
    struct dirent *de;
    if (d==0) return errno;
    while ((de=readdir(d))) {
	if (de==0) return errno;
	long long thisl;
	int r = sscanf(de->d_name, "log%lld.tokulog", &thisl);
	if (r==1 && thisl>maxf) maxf=thisl;
    }
    *result=maxf+1;
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
        lf_info->maxlsn = logger->written_lsn; 
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
    
    if ( logger->write_log_files && logger->trim_log_files) {
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

void toku_logger_write_log_files (TOKULOGGER logger, BOOL write_log_files) {
    assert(!logger->is_open);
    logger->write_log_files = write_log_files;
}

void toku_logger_trim_log_files (TOKULOGGER logger, BOOL trim_log_files) {
    assert(logger);
    logger->trim_log_files = trim_log_files;
}

int toku_logger_maybe_fsync (TOKULOGGER logger, LSN lsn, int do_fsync)
// Effect: If fsync is nonzero, then make sure that the log is flushed and synced at least up to lsn.
// Entry: Holds input lock.
// Exit:  Holds no locks.
// The input lock may be released and then reacquired.  Thus this function does not run atomically.
{
    int r;
    BOOL have_input_lock = TRUE;
    if (do_fsync && logger->fsynced_lsn.lsn < lsn.lsn) {
	// need to fsync and not enough is done
	// reacquire the locks (acquire output lock first)
	r = ml_unlock(&logger->input_lock);    if (r!=0) goto panic;
	r = ml_lock(&logger->output_lock);     if (r!=0) goto panic;
	r = ml_lock(&logger->input_lock);      if (r!=0) goto panic;

    
	// it's possible that the written lsn is now written enough that we are happy.   If not then do the I/O
	if (logger->fsynced_lsn.lsn < lsn.lsn) {
	    // now we actually do the I/O
	    struct logbuf tmp = logger->inbuf;
	    logger->inbuf = logger->outbuf;
	    logger->outbuf = tmp;
	    r = ml_unlock(&logger->input_lock); // release the input lock now, so group commit can operate
	    if (r!=0) goto panic;
	    have_input_lock = FALSE;
	    if (logger->outbuf.n_in_buf>0) {
		r = write_it(logger->fd, logger->outbuf.buf, logger->outbuf.n_in_buf);
		if (r!=logger->outbuf.n_in_buf) { r = errno; goto panic; }
		assert(logger->outbuf.max_lsn_in_buf.lsn > logger->written_lsn.lsn); // since there is something in the buffer, its LSN must be bigger than what's previously written.
		logger->written_lsn = logger->outbuf.max_lsn_in_buf;
		logger->n_in_file += logger->outbuf.n_in_buf;
		logger->outbuf.n_in_buf = 0;
	    }
	    if (logger->n_in_file > logger->lg_max) {
		r = close_and_open_logfile(logger);  if (r!=0) goto panic;
		logger->fsynced_lsn = logger->outbuf.max_lsn_in_buf;
	    } else {
		assert(logger->fsynced_lsn.lsn < logger->written_lsn.lsn); // the fsynced_lsn was less than lsn, but not less than the written lsn?
		r = toku_file_fsync(logger->fd);
		if (r!=0) { r = errno; goto panic; }
		logger->fsynced_lsn = logger->written_lsn;
	    }
	}
	r = ml_unlock(&logger->output_lock);
	if (r!=0) goto panic;
    }
    if (have_input_lock) {
	r = ml_unlock(&logger->input_lock);
	if (r!=0) goto panic2;
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

static int
toku_logger_write_buffer (TOKULOGGER logger, int do_fsync) 
// Entry:  Holds both locks.
// Exit:   Holds only the output lock.
// Effect:  Write the buffers to the output.  If DO_FSYNC is true, then fsync.
{
    int r;
    {
	struct logbuf tmp = logger->inbuf;
	logger->inbuf = logger->outbuf;
	logger->outbuf = tmp;
	assert(logger->inbuf.n_in_buf == 0);
    }
    r = ml_unlock(&logger->input_lock);
    if (r!=0)
	goto panic;
    if (logger->outbuf.n_in_buf > 0) {
	r = write_it(logger->fd, logger->outbuf.buf, logger->outbuf.n_in_buf);
	if (r != logger->outbuf.n_in_buf) {
	    r = errno;
	    goto panic;
	}
	assert(logger->outbuf.max_lsn_in_buf.lsn > logger->written_lsn.lsn); // since there is something in the buffer, its LSN must be bigger than what's previously written.
	logger->written_lsn = logger->outbuf.max_lsn_in_buf;
	logger->n_in_file += logger->outbuf.n_in_buf;
	logger->outbuf.n_in_buf = 0;
	if (logger->n_in_file > logger->lg_max) {
	    r = close_and_open_logfile(logger);  if (r!=0) goto panic;
	    logger->fsynced_lsn = logger->outbuf.max_lsn_in_buf;
	} else if (do_fsync) {
	    r = toku_file_fsync(logger->fd);
	    logger->fsynced_lsn = logger->outbuf.max_lsn_in_buf;
            toku_logfilemgr_update_last_lsn(logger->logfilemgr, logger->written_lsn);  // t:2294
	} else {
	    /* nothing */
	}
    }
    return 0;
 panic:
    toku_logger_panic(logger, r);
    return r;
}

int toku_logger_restart(TOKULOGGER logger, LSN lastlsn) {
    int r;

    // flush out the log buffer
    r = ml_lock(&logger->output_lock); assert(r == 0);
    r = ml_lock(&logger->input_lock); assert(r == 0);
    r = toku_logger_write_buffer(logger, TRUE); assert(r == 0);
    r = ml_unlock(&logger->output_lock); assert(r == 0);

    // close the log file
    r = close(logger->fd); assert(r == 0);
    logger->fd = -1;

    // reset the LSN's to the lastlsn when the logger was opened
    logger->lsn = logger->written_lsn = logger->fsynced_lsn = lastlsn;
    logger->write_log_files = TRUE;
    logger->trim_log_files = TRUE;

    // open a new log file
    return open_logfile(logger);
}

// fname is the iname
int toku_logger_log_fcreate (TOKUTXN txn, const char *fname, FILENUM filenum, u_int32_t mode, u_int32_t treeflags, DESCRIPTOR descriptor_p) {
    if (txn==0) return 0;
    if (txn->logger->is_panicked) return EINVAL;
    BYTESTRING bs_fname = { .len=strlen(fname), .data = (char *) fname };
    BYTESTRING bs_descriptor = { .len=descriptor_p->dbt.size, .data = descriptor_p->dbt.data };
    // fsync log on fcreate
    int r = toku_log_fcreate (txn->logger, (LSN*)0, 1, toku_txn_get_txnid(txn), filenum, bs_fname, mode, treeflags, descriptor_p->version, bs_descriptor);
    return r;
}


// fname is the iname 
int toku_logger_log_fdelete (TOKUTXN txn, const char *fname) {
    if (txn==0) return 0;
    if (txn->logger->is_panicked) return EINVAL;
    BYTESTRING bs = { .len=strlen(fname), .data = (char *) fname };
    //No fsync.
    int r = toku_log_fdelete (txn->logger, (LSN*)0, 0, toku_txn_get_txnid(txn), bs);
    return r;
}




/* fopen isn't really an action.  It's just for bookkeeping.  We need to know the filename that goes with a filenum. */
int toku_logger_log_fopen (TOKUTXN txn, const char * fname, FILENUM filenum, uint32_t treeflags) {
    if (txn==0) return 0;
    if (txn->logger->is_panicked) return EINVAL;
    BYTESTRING bs;
    bs.len = strlen(fname);
    bs.data = (char*)fname;
    return toku_log_fopen (txn->logger, (LSN*)0, 0, bs, filenum, treeflags);
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

// fills in the fs with malloced data.
int toku_fread_FILENUMS (FILE *f, FILENUMS *fs, struct x1764 *checksum, u_int32_t *len) {
    int r=toku_fread_u_int32_t(f, (u_int32_t*)&fs->num, checksum, len);
    if (r!=0) return r;
    fs->filenums = toku_malloc(fs->num * sizeof(FILENUM));
    u_int32_t i;
    for (i=0; i<fs->num; i++) {
	r=toku_fread_FILENUM (f, &fs->filenums[i], checksum, len);
	if (r!=0) {
	    toku_free(fs->filenums);
	    fs->filenums=0;
	    return r;
	}
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

static void
toku_print_FILENUMS (FILE *outf, u_int32_t num, FILENUM *filenums) {
    fprintf(outf, "{num=%u filenums=\"", num);
    u_int32_t i;
    for (i=0; i<num; i++) {
        if (i>0)
            fprintf(outf, ",");
        fprintf(outf, "0x%"PRIx32, filenums[i].fileid);
    }
    fprintf(outf, "\"}");

}

int toku_logprint_FILENUMS (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format __attribute__((__unused__))) {
    FILENUMS bs;
    int r = toku_fread_FILENUMS(inf, &bs, checksum, len);
    if (r!=0) return r;
    fprintf(outf, " %s=", fieldname);
    toku_print_FILENUMS(outf, bs.num, bs.filenums);
    toku_free(bs.filenums);
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

LSN
toku_logger_get_next_lsn(TOKULOGGER logger) {
    return logger->lsn;
}

//////////////////////////////////////////////////////////////////////////////////////
// remove_finalize_callback is set when environment is created so that when 
// a file removal is committed (or a file creation is aborted), the brt
// layer can call the ydb-layer callback to clean up the lock tree.


// called from toku_env_open()
void 
toku_logger_set_remove_finalize_callback(TOKULOGGER logger, void (*funcp)(int, void *), void * extra) {
    logger->remove_finalize_callback = funcp;
    logger->remove_finalize_callback_extra = extra;
}

// called when a transaction that deleted a file is committed, or
// when a transaction that created a file is aborted.
// During recovery, there is no ydb layer, so no callback exists.
void
toku_logger_call_remove_finalize_callback(TOKULOGGER logger, int fd) {
    if (logger->remove_finalize_callback)
        logger->remove_finalize_callback(fd, logger->remove_finalize_callback_extra);
}
