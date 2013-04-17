/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/* Recover an env.  The logs are in argv[1].  The new database is created in the cwd. */

// Test:
//    cd ../src/tests/tmpdir
//    ../../../newbrt/recover ../dir.test_log2.c.tdb

#include "includes.h"
#include "log_header.h"
#include "varray.h"

static int toku_recover_trace = 0;

//#define DO_VERIFY_COUNTS
#ifdef DO_VERIFY_COUNTS
#define VERIFY_COUNTS(n) toku_verify_counts(n)
#else
#define VERIFY_COUNTS(n) ((void)0)
#endif

// These data structures really should be part of a recovery data structure.  Recovery could be multithreaded (on different environments...)  But this is OK since recovery can only happen in one
static CACHETABLE recover_ct;
static TOKULOGGER recover_logger;

// TODO why can't we use the cachetable to find by filenum?
static struct cf_pair {
    FILENUM filenum;
    CACHEFILE cf;
    BRT       brt; // set to zero on an fopen, but filled in when an fheader is seen.
} *cf_pairs;
static int n_cf_pairs=0, max_cf_pairs=0;

int toku_recover_init (void) {
    int r;

    r = toku_create_cachetable(&recover_ct, 1<<25, (LSN){0}, 0);
    assert(r == 0);
    r = toku_logger_create(&recover_logger);
    assert(r == 0);
    toku_logger_write_log_files(recover_logger, FALSE);
    toku_logger_set_cachetable(recover_logger, recover_ct);
    if (toku_recover_trace)
        printf("%s:%d\n", __FUNCTION__, __LINE__);
    return r;
}

static void toku_recover_close_dictionaries(void) {
    int r;

    int i;
    for (i=0; i<n_cf_pairs; i++) {
	if (cf_pairs[i].brt) {
	    r = toku_close_brt(cf_pairs[i].brt, 0, 0);
	    //r = toku_cachefile_close(&cf_pairs[i].cf);
	    assert(r == 0);
	}
    }
    n_cf_pairs = 0;
    toku_free(cf_pairs);
    cf_pairs = NULL;
}

void toku_recover_cleanup (void) {
    int r;

    if (cf_pairs) 
        toku_recover_close_dictionaries();

    r = toku_logger_close(&recover_logger);
    assert(r == 0);
    
    r = toku_cachetable_close(&recover_ct);
    assert(r == 0);

    if (toku_recover_trace)
        printf("%s:%d\n", __FUNCTION__, __LINE__);
}

// Null function supplied to transaction commit and abort
static void recover_yield(voidfp UU(f), void *UU(extra)) {
    // nothing
}

enum backward_state { BS_INIT, BS_SAW_CKPT_END, BS_SAW_CKPT };
struct backward_scan_state {
    enum backward_state bs;
    LSN checkpoint_lsn;
    int n_live_txns;
    TXNID min_live_txn;
};

static struct backward_scan_state initial_bss = {BS_INIT,{0},0,0};

static void toku_recover_commit (LSN UU(lsn), TXNID xid) {
    int r;

    // find the transaction by transaction id
    TOKUTXN txn;
    r = toku_txnid2txn(recover_logger, xid, &txn);
    assert(r == 0);

    // commit the transaction
    r = toku_txn_commit_txn(txn, TRUE, recover_yield, NULL);
    assert(r == 0);

    // close the transaction
    toku_txn_close_txn(txn);
}

static int toku_recover_backward_commit (struct logtype_commit *UU(l), struct backward_scan_state *UU(bs)) {
    // nothing
    return 0;
}

static void toku_recover_xabort (LSN UU(lsn), TXNID xid) {
    int r;

    // find the transaction by transaction id
    TOKUTXN txn;
    r = toku_txnid2txn(recover_logger, xid, &txn);
    assert(r == 0);

    // abort the transaction
    r = toku_txn_abort_txn(txn, recover_yield, NULL);
    assert(r == 0);

    // close the transaction
    toku_txn_close_txn(txn);
}

static int toku_recover_backward_xabort (struct logtype_xabort *UU(l), struct backward_scan_state *UU(bs)) {
    // nothing
    return 0;
}

static void create_dir_from_file (const char *fname) {
    int i;
    char *tmp=toku_strdup(fname);
    char ch;
    for (i=0; (ch=fname[i]); i++) {
        //
        // TODO: this may fail in windows, double check the absolute path names
        // and '/' as the directory delimiter or something
        //
	if (ch=='/') {
	    if (i>0) {
		tmp[i]=0;
		mode_t oldu = umask(0);
		int r = toku_os_mkdir(tmp, S_IRWXU);
		if (r!=0 && errno!=EEXIST) {
		    printf("error: %s\n", strerror(errno));
		}
		assert (r == 0 || (errno==EEXIST));
		umask(oldu);
		tmp[i]=ch;
	    }
	}
    }
    toku_free(tmp);
}

static int toku_recover_note_cachefile (FILENUM fnum, CACHEFILE cf, BRT brt) {
    if (max_cf_pairs==0) {
	n_cf_pairs=1;
	max_cf_pairs=2;
	MALLOC_N(max_cf_pairs, cf_pairs);
	if (cf_pairs==0) return errno;
    } else {
	if (n_cf_pairs>=max_cf_pairs) {
	    cf_pairs = toku_realloc(cf_pairs, 2*max_cf_pairs*sizeof(*cf_pairs));
            assert(cf_pairs);
	    max_cf_pairs*=2;
	}
	n_cf_pairs++;
    }
    cf_pairs[n_cf_pairs-1].filenum = fnum;
    cf_pairs[n_cf_pairs-1].cf      = cf;
    cf_pairs[n_cf_pairs-1].brt     = brt;
    return 0;
}

static int find_cachefile (FILENUM fnum, struct cf_pair **cf_pair) {
    int i;
    for (i=0; i<n_cf_pairs; i++) {
	if (fnum.fileid==cf_pairs[i].filenum.fileid) {
	    *cf_pair = cf_pairs+i;
	    return 0;
	}
    }
    return 1;
}

// Open the file if it is not already open.  If it is already open, then do nothing.
static void internal_toku_recover_fopen_or_fcreate (int flags, int mode, char *fixedfname, FILENUM filenum) {
    {
	struct cf_pair *pair = NULL;
	int r = find_cachefile(filenum, &pair);
	if (0==r) {
	    toku_free(fixedfname);
	    return;
	}
    }

    CACHEFILE cf;
    int fd = open(fixedfname, O_RDWR|O_BINARY|flags, mode);
    if (fd<0) {
	char org_wd[1000];
	char *wd=getcwd(org_wd, sizeof(org_wd));
	fprintf(stderr, "%s:%d Could not open file %s, cwd=%s, errno=%d (%s)\n",
		__FILE__, __LINE__, fixedfname, wd, errno, strerror(errno));
    }
    assert(fd>=0);
    BRT brt=0;
    int r = toku_brt_create(&brt);
    assert(r == 0);
    brt->fname = fixedfname;
    brt->h=0;
    brt->compare_fun = toku_default_compare_fun; // we'll need to set these to the right comparison function, or do without them.
    brt->dup_compare = toku_default_compare_fun;
    brt->db = 0;
    r = toku_cachetable_openfd(&cf, recover_ct, fd, fixedfname);
    assert(r == 0);
    brt->cf=cf;
    r = toku_read_brt_header_and_store_in_cachefile(brt->cf, &brt->h);
    if (r==TOKUDB_DICTIONARY_NO_HEADER) {
	r = toku_brt_alloc_init_header(brt);
    }
    toku_recover_note_cachefile(filenum, cf, brt);
}

static void toku_recover_fopen (LSN UU(lsn), TXNID UU(xid), BYTESTRING fname, FILENUM filenum) {
    char *fixedfname = fixup_fname(&fname);
    internal_toku_recover_fopen_or_fcreate(0, 0, fixedfname, filenum);
}

static int toku_recover_backward_fopen (struct logtype_fopen *UU(l), struct backward_scan_state *UU(bs)) {
    return 0;
}

// fcreate is like fopen except that the file must be created. Also creates the dir if needed.
static void toku_recover_fcreate (LSN UU(lsn), TXNID UU(xid), FILENUM filenum, BYTESTRING fname,u_int32_t mode) {
    char *fixedfname = fixup_fname(&fname);
    create_dir_from_file(fixedfname);
    internal_toku_recover_fopen_or_fcreate(O_CREAT|O_TRUNC, mode, fixedfname, filenum);
}

static int toku_recover_backward_fcreate (struct logtype_fcreate *UU(l), struct backward_scan_state *UU(bs)) {
    // nothing
    return 0;
}

static void toku_recover_enq_insert (LSN lsn __attribute__((__unused__)), FILENUM filenum, TXNID xid, BYTESTRING key, BYTESTRING val) {
    struct cf_pair *pair = NULL;
    int r = find_cachefile(filenum, &pair);
    if (r!=0) {
	// if we didn't find a cachefile, then we don't have to do anything.
	return;
    }    
    // TODO compare file LSN with this XID
    TOKUTXN txn;
    r = toku_txnid2txn(recover_logger, xid, &txn);
    assert(r == 0);
    DBT keydbt, valdbt;
    toku_fill_dbt(&keydbt, key.data, key.len);
    toku_fill_dbt(&valdbt, val.data, val.len);
    r = toku_brt_insert(pair->brt, &keydbt, &valdbt, txn);
    assert(r == 0);
}

static int toku_recover_backward_enq_insert (struct logtype_enq_insert *UU(l), struct backward_scan_state *UU(bs)) {
    // nothing
    return 0;
}

static void toku_recover_enq_delete_both (LSN lsn __attribute__((__unused__)), FILENUM filenum, TXNID xid, BYTESTRING key, BYTESTRING val) {
    struct cf_pair *pair = NULL;
    int r = find_cachefile(filenum, &pair);
    if (r!=0) {
	// if we didn't find a cachefile, then we don't have to do anything.
	return;
    }    
    // TODO compare file LSN with this XID
    TOKUTXN txn;
    r = toku_txnid2txn(recover_logger, xid, &txn);
    assert(r == 0);
    DBT keydbt, valdbt;
    toku_fill_dbt(&keydbt, key.data, key.len);
    toku_fill_dbt(&valdbt, val.data, val.len);
    r = toku_brt_delete_both(pair->brt, &keydbt, &valdbt, txn);
    assert(r == 0);
}

static int toku_recover_backward_enq_delete_both (struct logtype_enq_delete_both *UU(l), struct backward_scan_state *UU(bs)) {
    // nothing
    return 0;
}

static void toku_recover_enq_delete_any (LSN lsn __attribute__((__unused__)), FILENUM filenum, TXNID xid, BYTESTRING key, BYTESTRING UU(val)) {
    struct cf_pair *pair = NULL;
    int r = find_cachefile(filenum, &pair);
    if (r!=0) {
	// if we didn't find a cachefile, then we don't have to do anything.
	return;
    }    
    // TODO compare file LSN with this XID
    TOKUTXN txn;
    r = toku_txnid2txn(recover_logger, xid, &txn);
    assert(r == 0);
    DBT keydbt;
    toku_fill_dbt(&keydbt, key.data, key.len);
    r = toku_brt_delete(pair->brt, &keydbt, txn);
    assert(r == 0);
}

static int toku_recover_backward_enq_delete_any (struct logtype_enq_delete_any *UU(l), struct backward_scan_state *UU(bs)) {
    // nothing
    return 0;
}

static void toku_recover_fclose (LSN UU(lsn), BYTESTRING UU(fname), FILENUM filenum) {
    struct cf_pair *pair = NULL;
    int r = find_cachefile(filenum, &pair);
    assert(r == 0);
    r = toku_close_brt(pair->brt, 0, 0);
    assert(r == 0);
    pair->brt=0;
}

static int toku_recover_backward_fclose (struct logtype_fclose *UU(l), struct backward_scan_state *UU(bs)) {
    return 0;
}

static int toku_recover_begin_checkpoint (LSN UU(lsn), u_int64_t UU(timestamp)) {
    // nothing
    return 0;
}

static int toku_recover_backward_begin_checkpoint (struct logtype_begin_checkpoint *l, struct backward_scan_state *bs) {
    switch (bs->bs) {
    case BS_INIT:
	return 0; // incomplete checkpoint
    case BS_SAW_CKPT_END:
	assert(bs->checkpoint_lsn.lsn == l->lsn.lsn);
	bs->bs = BS_SAW_CKPT;
	if (bs->n_live_txns==0) {
	    fprintf(stderr, "Turning around at begin_checkpoint %" PRIu64 "\n", l->lsn.lsn);
	    return 1;
	}
	else return 0;
    case BS_SAW_CKPT:
	return 0; // ignore it
    }
    fprintf(stderr, "%s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)bs->bs);
    abort();
}

static int toku_recover_end_checkpoint (LSN UU(lsn), TXNID UU(xid), u_int64_t UU(timestamp)) {
    // nothing
    return 0;
}

static int toku_recover_backward_end_checkpoint (struct logtype_end_checkpoint *l, struct backward_scan_state *bs) {
    switch (bs->bs) {
    case BS_INIT:
	bs->bs = BS_SAW_CKPT_END;
	bs->checkpoint_lsn.lsn = l->txnid;
	return 0;
    case BS_SAW_CKPT_END:
	fprintf(stderr, "%s:%d Should not see two end_checkpoint log entries without an intervening begin_checkpoint\n", __FILE__, __LINE__);
	abort();
    case BS_SAW_CKPT:
	return 0;
    }
    fprintf(stderr, "%s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)bs->bs);
    abort();
}

static int toku_recover_fassociate (LSN UU(lsn), FILENUM UU(filenum), BYTESTRING UU(fname)) {
    // nothing
    return 0;
}

static int toku_recover_backward_fassociate (struct logtype_fassociate *l, struct backward_scan_state *UU(bs)) {
    char *fixedfname = fixup_fname(&l->fname);
    internal_toku_recover_fopen_or_fcreate(0, 0, fixedfname, l->filenum);
    return 0;
}

static int toku_recover_xstillopen (LSN UU(lsn), TXNID UU(xid), TXNID UU(parent)) {
    // nothing
    return 0;
}

static int toku_recover_backward_xstillopen (struct logtype_xstillopen *l, struct backward_scan_state *bs) {
    switch (bs->bs) {
    case BS_INIT:
	return 0; // ignore live txns from incomplete checkpoint
    case BS_SAW_CKPT_END:
	if (bs->n_live_txns==0) {
	    bs->min_live_txn = l->txnid;
	} else {
	    if (bs->min_live_txn > l->txnid)  bs->min_live_txn = l->txnid;
	}
	bs->n_live_txns++;
	return 0;
    case BS_SAW_CKPT:
	return 0; // ignore live txns from older checkpoints
    }
    fprintf(stderr, "%s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)bs->bs);
    abort();
}

static int toku_recover_xbegin (LSN lsn, TXNID parent_xid) {
    int r;

    // lookup the parent
    TOKUTXN parent = NULL;
    r = toku_txnid2txn(recover_logger, parent_xid, &parent);
    assert(r == 0);

    // create a transaction and bind it to the transaction id
    TOKUTXN txn = NULL;
    r = toku_txn_begin_with_xid(parent, &txn, recover_logger, lsn.lsn);
    assert(r == 0);
    return 0;
}

static int toku_recover_backward_xbegin (struct logtype_xbegin *l, struct backward_scan_state *bs) {
    switch (bs->bs) {
    case BS_INIT:
	return 0; // ignore txns that began after checkpoint
    case BS_SAW_CKPT_END:
	return 0; // ignore txns that began during the checkpoint
    case BS_SAW_CKPT:
	assert(bs->n_live_txns > 0); // the only thing we are doing here is looking for a live txn, so there better be one
	// If we got to the min, return nonzero
	if (bs->min_live_txn >= l->lsn.lsn) {
	    fprintf(stderr, "Turning around at xbegin %" PRIu64 "\n", l->lsn.lsn);
	    return 1;
	} else {
	    fprintf(stderr, "scanning back at xbegin %" PRIu64 " (looking for %" PRIu64 ")\n", l->lsn.lsn, bs->min_live_txn);
	    return 0;
	}
    }
    fprintf(stderr, "%s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)bs->bs);
    abort();
}

static int toku_recover_timestamp (LSN UU(lsn), u_int64_t UU(timestamp), BYTESTRING UU(comment)) {
    // nothing
    return 0;
}

static int toku_recover_backward_timestamp (struct logtype_timestamp *UU(l), struct backward_scan_state *UU(bs)) {
    // nothing
    return 0;
}

static int toku_recover_shutdown (LSN UU(lsn), u_int64_t UU(timestamp)) {
    // nothing
    return 0;
}

static int toku_recover_backward_shutdown (struct logtype_shutdown *UU(l), struct backward_scan_state *UU(bs)) {
    // nothing
    return 0;
}

static int toku_delete_rolltmp_files (const char *log_dir) {
    struct dirent *de;
    DIR *d = opendir(log_dir);
    if (d==0) {
	return errno;
    }
    int result = 0;
    while ((de=readdir(d))) {
	char rolltmp_prefix[] = "__rolltmp.";
	int r = memcmp(de->d_name, rolltmp_prefix, sizeof(rolltmp_prefix) - 1);
	if (r == 0) {
	    int fnamelen = strlen(log_dir) + strlen(de->d_name) + 2; // One for the slash and one for the trailing NUL.
	    char fname[fnamelen];
	    int l = snprintf(fname, fnamelen, "%s/%s", log_dir, de->d_name);
	    assert(l+1 == fnamelen);
	    r = unlink(fname);
	    if (r!=0) {
		result = errno;
		perror("Trying to delete a rolltmp file");
	    }
	}
    }
    {
	int r = closedir(d);
	if (r==-1) return errno;
    }
    return result;
}

// Effects: If there are no log files, or if there is a "clean" checkpoint at the end of the log,
// then we don't need recovery to run.  We skip the optional shutdown log entry.
// Returns: TRUE if we need recovery, FALSE if we do not need recovery.
int tokudb_needs_recovery(const char *log_dir) {
    int needs_recovery;
    int r;
    TOKULOGCURSOR logcursor = NULL;

    r = toku_logcursor_create(&logcursor, log_dir);
    if (r != 0) {
        needs_recovery = TRUE; goto exit;
    }
    
    struct log_entry *le = NULL;
    r = toku_logcursor_last(logcursor, &le);
    if (r == DB_NOTFOUND) {
        needs_recovery = FALSE; goto exit;
    }
    if (r != 0) {
        needs_recovery = TRUE; goto exit;
    }
    if (le->cmd == LT_shutdown) {
        r = toku_logcursor_prev(logcursor, &le);
        if (r != 0) {
            needs_recovery = TRUE; goto exit;
        }
    }
    if (le->cmd != LT_end_checkpoint) {
        needs_recovery = TRUE; goto exit;
    }
    struct log_entry end_checkpoint = *le;

    r = toku_logcursor_prev(logcursor, &le);
    if (r != 0 || le->cmd != LT_begin_checkpoint) {
        needs_recovery = TRUE; goto exit;
    }
    if (le->u.begin_checkpoint.lsn.lsn != end_checkpoint.u.end_checkpoint.txnid) {
        needs_recovery = TRUE; goto exit;
    }
    needs_recovery = FALSE;

 exit:
    if (logcursor) {
        r = toku_logcursor_destroy(&logcursor);
        assert(r == 0);
    }
    return needs_recovery;
}

// Sort the transactions in reverse order of transaction id
static int compare_txn(const void *a, const void *b) {
    TOKUTXN atxn = (TOKUTXN) * (void **) a;
    TOKUTXN btxn = (TOKUTXN) * (void **) b;
    return - (atxn->txnid64 - btxn->txnid64);
}

// Append a transaction to the set of live transactions
static int append_live_txn(OMTVALUE v, u_int32_t UU(i), void *extra) {
    TOKUTXN txn = (TOKUTXN) v;
    struct varray *live_txns = (struct varray *) extra;
    varray_append(live_txns, txn);
    return 0;
}

// Abort a live transaction
static void abort_live_txn(void *v, void *UU(extra)) {
    TOKUTXN txn = (TOKUTXN) v;

    // abort the transaction
    int r = toku_txn_abort_txn(txn, recover_yield, NULL);
    assert(r == 0);

    // close the transaction
    toku_txn_close_txn(txn);
}

static int really_do_recovery(const char *data_dir, const char *log_dir) {
    int r;

    char org_wd[1000];
    {
	char *wd=getcwd(org_wd, sizeof(org_wd));
	assert(wd!=0);
	//printf("%s:%d org_wd=\"%s\"\n", __FILE__, __LINE__, org_wd);
    }

    toku_recover_init();

    r = toku_logger_open(log_dir, recover_logger);
    assert(r == 0);

    LSN lastlsn = toku_logger_last_lsn(recover_logger);

    TOKULOGCURSOR logcursor;
    r = toku_logcursor_create(&logcursor, log_dir);
    assert(r == 0);

    r = chdir(data_dir); 
    assert(r == 0);

    struct log_entry *le;

    // scan backwards
    struct backward_scan_state bs = initial_bss;
    while (1) {
        le = NULL;
        r = toku_logcursor_prev(logcursor, &le);
        if (toku_recover_trace) 
            printf("%s:%d r=%d cmd=%c\n", __FUNCTION__, __LINE__, r, le ? le->cmd : '?');
        if (r != 0)
            break;
        logtype_dispatch_assign(le, toku_recover_backward_, r, &bs);
        if (r != 0) {
            if (toku_recover_trace) 
                printf("%s:%d r=%d cmd=%c\n", __FUNCTION__, __LINE__, r, le ? le->cmd : '?');
            logtype_dispatch_args(le, toku_recover_);
            break;
        }
    }

    // scan forwards
    while (1) {
        le = NULL;
        r = toku_logcursor_next(logcursor, &le);
        if (toku_recover_trace) 
            printf("%s:%d r=%d cmd=%c\n", __FUNCTION__, __LINE__, r, le ? le->cmd : '?');
        if (r != 0)
            break;
        logtype_dispatch_args(le, toku_recover_);
    }

    r = toku_logcursor_destroy(&logcursor);
    assert(r == 0);
   
    // restart logging
    toku_logger_restart(recover_logger, lastlsn);

    // abort all of the remaining live transactions in reverse transaction id order
    struct varray *live_txns = NULL;
    r = varray_create(&live_txns, 1);
    assert(r == 0);
    toku_omt_iterate(recover_logger->live_txns, append_live_txn, live_txns);
    varray_sort(live_txns, compare_txn);
    varray_iterate(live_txns, abort_live_txn, NULL);
    varray_destroy(&live_txns);

    // close the open dictionaries
    toku_recover_close_dictionaries();

    // write a recovery log entry
    BYTESTRING recover_comment = { strlen("recover"), "recover" };
    r = toku_log_timestamp(recover_logger, NULL, TRUE, 0, recover_comment);
    assert(r == 0);

    // checkpoint 
    // TODO: checkpoint locks needed here?
    r = toku_cachetable_begin_checkpoint(recover_ct, recover_logger);
    assert(r == 0);
    // TODO: what about the error_string?
    r = toku_cachetable_end_checkpoint(recover_ct, recover_logger, NULL);
    assert(r == 0);

    toku_recover_cleanup();

    r = chdir(org_wd); 
    assert(r == 0);

    return 0;
}

int tokudb_recover(const char *data_dir, const char *log_dir) {
    int r;
    int lockfd;

    {
        const char fname[] = "/__recoverylock_dont_delete_me";
	int namelen=strlen(data_dir);
	char lockfname[namelen+sizeof(fname)];

	int l = snprintf(lockfname, sizeof(lockfname), "%s%s", data_dir, fname);
	assert(l+1 == (signed)(sizeof(lockfname)));
        lockfd = toku_os_lock_file(lockfname);
	if (lockfd<0) {
	    printf("Couldn't run recovery because some other process holds the recovery lock %s\n", lockfname);
	    return errno;	}
    }

    r = toku_delete_rolltmp_files(log_dir);
    if (r != 0) { 
        toku_os_unlock_file(lockfd);
        return r;
    }

    int rr = 0;
    if (tokudb_needs_recovery(log_dir))
        rr = really_do_recovery(data_dir, log_dir);

    r = toku_os_unlock_file(lockfd);
    if (r != 0) return errno;

    //printf("%s:%d recovery successful! ls -l says\n", __FILE__, __LINE__);

    return rr;
}
