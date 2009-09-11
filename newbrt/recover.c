/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

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

struct backward_scan_state {
    enum backward_state { BS_INIT, BS_SAW_CKPT_END, BS_SAW_CKPT } bs;
    LSN checkpoint_lsn;
    int n_live_txns;
    TXNID min_live_txn;
};

static void backward_scan_state_init(struct backward_scan_state *bs) {
    bs->bs = BS_INIT; bs->checkpoint_lsn = ZERO_LSN; bs->n_live_txns = 0; bs->min_live_txn = 0;
}

// Map filenum to brt
// TODO why can't we use the cachetable to find by filenum?
// TODO O(n) time for linear search.  should we use an OMT?
struct file_map {
    struct cf_tuple {
        FILENUM   filenum;
        CACHEFILE cf;
        BRT       brt; // set to zero on an fopen, but filled in when an fheader is seen.
        char     *fname;
    } *cf_tuples;
    int n_cf_tuples, max_cf_tuples;
};

static void file_map_init(struct file_map *fmap) {
    fmap->cf_tuples = NULL;
    fmap->n_cf_tuples = fmap->max_cf_tuples = 0;
}

static void file_map_close_dictionaries(struct file_map *fmap) {
    int r;

    for (int i=0; i<fmap->n_cf_tuples; i++) {
        struct cf_tuple *tuple = &fmap->cf_tuples[i];
	if (tuple->brt) {
	    r = toku_close_brt(tuple->brt, 0, 0);
	    //r = toku_cachefile_close(&cf_tuples[i].cf);
	    assert(r == 0);
	}
        if (tuple->fname) {
            toku_free(tuple->fname);
            tuple->fname = NULL;
        }
    }
    fmap->n_cf_tuples = fmap->max_cf_tuples = 0;
    if (fmap->cf_tuples) {
        toku_free(fmap->cf_tuples);
        fmap->cf_tuples = NULL;
    }
}

static int file_map_add (struct file_map *fmap, FILENUM fnum, CACHEFILE cf, BRT brt, char *fname) {
    if (fmap->cf_tuples == NULL) {
	fmap->max_cf_tuples = 1;
	MALLOC_N(fmap->max_cf_tuples, fmap->cf_tuples);
	if (fmap->cf_tuples == NULL) {
            fmap->max_cf_tuples = 0;
            return errno;
        }
	fmap->n_cf_tuples=1;
    } else {
	if (fmap->n_cf_tuples >= fmap->max_cf_tuples) {
            struct cf_tuple *new_cf_tuples = toku_realloc(fmap->cf_tuples, 2*fmap->max_cf_tuples*sizeof(struct cf_tuple));
            if (new_cf_tuples == NULL) 
                return errno;
            fmap->cf_tuples = new_cf_tuples;
	    fmap->max_cf_tuples *= 2;
	}
	fmap->n_cf_tuples++;
    }
    struct cf_tuple *tuple = &fmap->cf_tuples[fmap->n_cf_tuples-1];
    tuple->filenum = fnum;
    tuple->cf      = cf;
    tuple->brt     = brt;
    tuple->fname   = fname;
    return 0;
}

static int find_cachefile (struct file_map *fmap, FILENUM fnum, struct cf_tuple **cf_tuple) {
    for (int i=0; i<fmap->n_cf_tuples; i++) {
	if (fnum.fileid==fmap->cf_tuples[i].filenum.fileid) {
	    *cf_tuple = &fmap->cf_tuples[i];
	    return 0;
	}
    }
    return 1;
}

struct recover_env {
    CACHETABLE ct;
    TOKULOGGER logger;
    brt_compare_func bt_compare;
    brt_compare_func dup_compare;
    struct backward_scan_state bs;
    struct file_map fmap;
};
typedef struct recover_env *RECOVER_ENV;

static int recover_env_init (RECOVER_ENV renv, brt_compare_func bt_compare, brt_compare_func dup_compare) {
    int r;

    r = toku_create_cachetable(&renv->ct, 1<<25, (LSN){0}, 0);
    assert(r == 0);
    r = toku_logger_create(&renv->logger);
    assert(r == 0);
    toku_logger_write_log_files(renv->logger, FALSE);
    toku_logger_set_cachetable(renv->logger, renv->ct);
    renv->bt_compare = bt_compare;
    renv->dup_compare = dup_compare;
    file_map_init(&renv->fmap);

    if (toku_recover_trace)
        printf("%s:%d\n", __FUNCTION__, __LINE__);
    return r;
}

static void recover_env_cleanup (RECOVER_ENV renv) {
    int r;

    file_map_close_dictionaries(&renv->fmap);

    r = toku_logger_close(&renv->logger);
    assert(r == 0);
    
    r = toku_cachetable_close(&renv->ct);
    assert(r == 0);

    if (toku_recover_trace)
        printf("%s:%d\n", __FUNCTION__, __LINE__);
}

// Null function supplied to transaction commit and abort
static void recover_yield(voidfp UU(f), void *UU(extra)) {
    // nothing
}

static void toku_recover_commit (LSN lsn, TXNID xid, RECOVER_ENV renv) {
    int r;

    // find the transaction by transaction id
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, xid, &txn);
    assert(r == 0 && txn);

    // commit the transaction
    r = toku_txn_commit_with_lsn(txn, TRUE, recover_yield, NULL, lsn);
    assert(r == 0);

    // close the transaction
    toku_txn_close_txn(txn);
}

static int toku_recover_backward_commit (struct logtype_commit *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static void toku_recover_xabort (LSN lsn, TXNID xid, RECOVER_ENV renv) {
    int r;

    // find the transaction by transaction id
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, xid, &txn);
    assert(r == 0 && txn);

    // abort the transaction
    r = toku_txn_abort_with_lsn(txn, recover_yield, NULL, lsn);
    assert(r == 0);

    // close the transaction
    toku_txn_close_txn(txn);
}

static int toku_recover_backward_xabort (struct logtype_xabort *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_backward_tablelock_on_empty_table(struct logtype_tablelock_on_empty_table *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static void create_dir_from_file (const char *fname) {
    char *tmp=toku_strdup(fname);
    char ch;
    for (int i=0; (ch=fname[i]); i++) {
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

// Open the file if it is not already open.  If it is already open, then do nothing.
static void internal_toku_recover_fopen_or_fcreate (RECOVER_ENV renv, int flags, int mode, char *fixedfname, FILENUM filenum, u_int32_t treeflags) {
    int r;

    // already open
    struct cf_tuple *tuple = NULL;
    r = find_cachefile(&renv->fmap, filenum, &tuple);
    if (r == 0) {
        assert(strcmp(tuple->fname, fixedfname) == 0);
        toku_free(fixedfname);
        return;
    }

    if (flags & O_TRUNC) {
        // maybe unlink
        r = unlink(fixedfname);
        if (r != 0)
            printf("%s:%d unlink %d\n", __FUNCTION__, __LINE__, errno);
    }

    BRT brt=0;
    r = toku_brt_create(&brt);
    assert(r == 0);

    // create tree with treeflags, otherwise use the treeflags from the tree
    if (flags & O_CREAT)
        toku_brt_set_flags(brt, treeflags);

    // set the key compare functions
    if (renv->bt_compare)
        toku_brt_set_bt_compare(brt, renv->bt_compare);
    if (renv->dup_compare)
        toku_brt_set_dup_compare(brt, renv->dup_compare);

    // bind to filenum when opened
    toku_brt_set_filenum(brt, filenum);

    // TODO mode
    mode = mode;

    r = toku_brt_open(brt, fixedfname, fixedfname, (flags & O_CREAT) != 0, FALSE, renv->ct, NULL, NULL);
    assert(r == 0);

    file_map_add(&renv->fmap, filenum, NULL, brt, fixedfname);
}

static void toku_recover_fopen (LSN UU(lsn), TXNID UU(xid), BYTESTRING fname, FILENUM filenum, RECOVER_ENV renv) {
    char *fixedfname = fixup_fname(&fname);
    internal_toku_recover_fopen_or_fcreate(renv, 0, 0, fixedfname, filenum, 0);
}

static int toku_recover_backward_fopen (struct logtype_fopen *l, RECOVER_ENV renv) {
    if (renv->bs.bs == BS_SAW_CKPT_END) {
        // close the tree
        struct cf_tuple *tuple = NULL;
        int r = find_cachefile(&renv->fmap, l->filenum, &tuple);
        if (r == 0) {
            r = toku_close_brt(tuple->brt, 0, 0);
            assert(r == 0);
            tuple->brt=0;
        }
    }
    return 0;
}

// fcreate is like fopen except that the file must be created. Also creates the dir if needed.
static void toku_recover_fcreate (LSN UU(lsn), TXNID UU(xid), FILENUM filenum, BYTESTRING fname, u_int32_t mode, u_int32_t treeflags, RECOVER_ENV renv) {
    char *fixedfname = fixup_fname(&fname);
    create_dir_from_file(fixedfname);
    internal_toku_recover_fopen_or_fcreate(renv, O_CREAT|O_TRUNC, mode, fixedfname, filenum, treeflags);
}

static int toku_recover_backward_fcreate (struct logtype_fcreate *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static void toku_recover_enq_insert (LSN lsn, FILENUM filenum, TXNID xid, BYTESTRING key, BYTESTRING val, RECOVER_ENV renv) {
    struct cf_tuple *tuple = NULL;
    int r = find_cachefile(&renv->fmap, filenum, &tuple);
    if (r!=0) {
	// if we didn't find a cachefile, then we don't have to do anything.
	return;
    }    

    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, xid, &txn);
    assert(r == 0 && txn);
    DBT keydbt, valdbt;
    toku_fill_dbt(&keydbt, key.data, key.len);
    toku_fill_dbt(&valdbt, val.data, val.len);
    r = toku_brt_maybe_insert(tuple->brt, &keydbt, &valdbt, txn, lsn);
    assert(r == 0);
}

static int toku_recover_tablelock_on_empty_table(LSN UU(lsn), FILENUM filenum, TXNID xid, RECOVER_ENV renv) {
    struct cf_tuple *tuple = NULL;
    int r = find_cachefile(&renv->fmap, filenum, &tuple);
    if (r!=0) {
	// if we didn't find a cachefile, then we don't have to do anything.
	return 0;
    }    

    TOKUTXN txn;
    r = toku_txnid2txn(renv->logger, xid, &txn);
    assert(r == 0 && txn);
    r = toku_brt_note_table_lock(tuple->brt, txn);
    assert(r == 0);
    return 0;
}

static int toku_recover_backward_enq_insert (struct logtype_enq_insert *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static void toku_recover_enq_delete_both (LSN lsn, FILENUM filenum, TXNID xid, BYTESTRING key, BYTESTRING val, RECOVER_ENV renv) {
    struct cf_tuple *tuple = NULL;
    int r = find_cachefile(&renv->fmap, filenum, &tuple);
    if (r!=0) {
	// if we didn't find a cachefile, then we don't have to do anything.
	return;
    }    

    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, xid, &txn);
    assert(r == 0 && txn);
    DBT keydbt, valdbt;
    toku_fill_dbt(&keydbt, key.data, key.len);
    toku_fill_dbt(&valdbt, val.data, val.len);
    r = toku_brt_maybe_delete_both(tuple->brt, &keydbt, &valdbt, txn, lsn);
    assert(r == 0);
}

static int toku_recover_backward_enq_delete_both (struct logtype_enq_delete_both *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static void toku_recover_enq_delete_any (LSN lsn, FILENUM filenum, TXNID xid, BYTESTRING key, RECOVER_ENV renv) {
    struct cf_tuple *tuple = NULL;
    int r = find_cachefile(&renv->fmap, filenum, &tuple);
    if (r!=0) {
	// if we didn't find a cachefile, then we don't have to do anything.
	return;
    }    

    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, xid, &txn);
    assert(r == 0 && txn);
    DBT keydbt;
    toku_fill_dbt(&keydbt, key.data, key.len);
    r = toku_brt_maybe_delete(tuple->brt, &keydbt, txn, lsn);
    assert(r == 0);
}

static int toku_recover_backward_enq_delete_any (struct logtype_enq_delete_any *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static void toku_recover_fclose (LSN UU(lsn), BYTESTRING fname, FILENUM filenum, RECOVER_ENV UU(renv)) {
    struct cf_tuple *tuple = NULL;
    int r = find_cachefile(&renv->fmap, filenum, &tuple);
    if (r == 0) {
        char *fixedfname = fixup_fname(&fname);
        assert(strcmp(tuple->fname, fixedfname) == 0);
        toku_free(fixedfname);
        r = toku_close_brt(tuple->brt, 0, 0);
        assert(r == 0);
        tuple->brt=0;
    }
}

static int toku_recover_backward_fclose (struct logtype_fclose *l, RECOVER_ENV renv) {
    if (renv->bs.bs == BS_SAW_CKPT) {
        // tree open
        char *fixedfname = fixup_fname(&l->fname);
        internal_toku_recover_fopen_or_fcreate(renv, 0, 0, fixedfname, l->filenum, 0);
    }
    return 0;
}

static int toku_recover_begin_checkpoint (LSN UU(lsn), u_int64_t UU(timestamp), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_backward_begin_checkpoint (struct logtype_begin_checkpoint *l, RECOVER_ENV renv) {
    struct backward_scan_state *bs = &renv->bs;
    switch (bs->bs) {
    case BS_INIT:
	return 0; // incomplete checkpoint
    case BS_SAW_CKPT_END:
	assert(bs->checkpoint_lsn.lsn == l->lsn.lsn);
	bs->bs = BS_SAW_CKPT;
	if (bs->n_live_txns==0) {
	    fprintf(stderr, "Turning around at begin_checkpoint %" PRIu64 "\n", l->lsn.lsn);
	    return 1;
	} else 
            return 0;
    case BS_SAW_CKPT:
	return 0; // ignore it
    }
    fprintf(stderr, "%s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)bs->bs);
    abort();
}

static int toku_recover_end_checkpoint (LSN UU(lsn), TXNID UU(xid), u_int64_t UU(timestamp), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_backward_end_checkpoint (struct logtype_end_checkpoint *l, RECOVER_ENV renv) {
    struct backward_scan_state *bs = &renv->bs;
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

static int toku_recover_fassociate (LSN UU(lsn), FILENUM UU(filenum), BYTESTRING UU(fname), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_backward_fassociate (struct logtype_fassociate *l, RECOVER_ENV renv) {
    char *fixedfname = fixup_fname(&l->fname);
    internal_toku_recover_fopen_or_fcreate(renv, 0, 0, fixedfname, l->filenum, 0);
    return 0;
}

static int toku_recover_xstillopen (LSN UU(lsn), TXNID UU(xid), TXNID UU(parent), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_backward_xstillopen (struct logtype_xstillopen *l, RECOVER_ENV renv) {
    struct backward_scan_state *bs = &renv->bs;
    switch (bs->bs) {
    case BS_INIT:
	return 0; // ignore live txns from incomplete checkpoint
    case BS_SAW_CKPT_END:
	if (bs->n_live_txns == 0)
	    bs->min_live_txn = l->txnid;
	else if (toku_txnid_older(l->txnid, bs->min_live_txn))  
            bs->min_live_txn = l->txnid;
	bs->n_live_txns++;
	return 0;
    case BS_SAW_CKPT:
	return 0; // ignore live txns from older checkpoints
    }
    fprintf(stderr, "%s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)bs->bs);
    abort();
}

static int toku_recover_xbegin (LSN lsn, TXNID parent_xid, RECOVER_ENV renv) {
    int r;

    // lookup the parent
    TOKUTXN parent = NULL;
    if (parent_xid != 0) {
        r = toku_txnid2txn(renv->logger, parent_xid, &parent);
        assert(r == 0 && parent);
    }

    // create a transaction and bind it to the transaction id
    TOKUTXN txn = NULL;
    r = toku_txn_begin_with_xid(parent, &txn, renv->logger, lsn.lsn);
    assert(r == 0);
    return 0;
}

static int toku_recover_backward_xbegin (struct logtype_xbegin *l, RECOVER_ENV renv) {
    struct backward_scan_state *bs = &renv->bs;
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
	    fprintf(stderr, "Scanning back at xbegin %" PRIu64 " (looking for %" PRIu64 ")\n", l->lsn.lsn, bs->min_live_txn);
	    return 0;
	}
    }
    fprintf(stderr, "%s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)bs->bs);
    abort();
}

static int toku_recover_timestamp (LSN UU(lsn), u_int64_t UU(timestamp), BYTESTRING UU(comment), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_backward_timestamp (struct logtype_timestamp *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_shutdown (LSN UU(lsn), u_int64_t UU(timestamp), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_backward_shutdown (struct logtype_shutdown *UU(l), RECOVER_ENV UU(renv)) {
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
// then we don't need recovery to run.  Skip the shutdown log entry if there is one.
// Returns: TRUE if we need recovery, otherwise FALSE.
int tokudb_needs_recovery(const char *log_dir, BOOL ignore_log_empty) {
    int needs_recovery;
    int r;
    TOKULOGCURSOR logcursor = NULL;

    r = toku_logcursor_create(&logcursor, log_dir);
    if (r != 0) {
        needs_recovery = TRUE; goto exit;
    }
    
    struct log_entry *le = NULL;
    r = toku_logcursor_last(logcursor, &le);
    if (r == DB_NOTFOUND && ignore_log_empty) {
        needs_recovery = FALSE; goto exit;
    }
    if (r != 0) {
        needs_recovery = TRUE; goto exit;
    }
    if (le->cmd == LT_shutdown || le->cmd == LT_timestamp) {
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
    // TODO this is wrong.  we want if (older(atxn, btxn)) return -1
    if (toku_txnid_eq(atxn->txnid64, btxn->txnid64))
        return 0;
    if (toku_txnid_older(atxn->txnid64, btxn->txnid64))
        return +1;
    else
        return -1;
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

static int do_recovery(RECOVER_ENV renv, const char *data_dir, const char *log_dir) {
    int r;
    int rr = 0;
    TOKULOGCURSOR logcursor = NULL;
    struct log_entry *le = NULL;

    char org_wd[1000];
    {
	char *wd=getcwd(org_wd, sizeof(org_wd));
	assert(wd!=0);
	//printf("%s:%d org_wd=\"%s\"\n", __FILE__, __LINE__, org_wd);
    }

    r = toku_logger_open(log_dir, renv->logger);
    assert(r == 0);

    LSN lastlsn = toku_logger_last_lsn(renv->logger);

    // there must be at least one log entry
    r = toku_logcursor_create(&logcursor, log_dir);
    assert(r == 0);
    
    r = toku_logcursor_last(logcursor, &le);
    if (r != 0) {
        rr = DB_RUNRECOVERY; goto errorexit;
    }

    // TODO use logcursor->invalidate()
    r = toku_logcursor_destroy(&logcursor);
    assert(r == 0);

    r = toku_logcursor_create(&logcursor, log_dir);
    assert(r == 0);

    r = chdir(data_dir); 
    if (r != 0) {
        // no data directory error
        rr = errno; goto errorexit;
    }

    // scan backwards
    backward_scan_state_init(&renv->bs);
    while (1) {
        le = NULL;
        r = toku_logcursor_prev(logcursor, &le);
        if (toku_recover_trace) 
            printf("%s:%d r=%d cmd=%c\n", __FUNCTION__, __LINE__, r, le ? le->cmd : '?');
        if (r != 0) {
            if (r == DB_RUNRECOVERY) {
                rr = DB_RUNRECOVERY; goto errorexit;
            }
            break;
        }
        logtype_dispatch_assign(le, toku_recover_backward_, r, renv);
        if (r != 0) {
            if (toku_recover_trace) 
                printf("%s:%d r=%d cmd=%c\n", __FUNCTION__, __LINE__, r, le ? le->cmd : '?');
            logtype_dispatch_args(le, toku_recover_, renv);
            break;
        }
    }

    // scan forwards
    while (1) {
        le = NULL;
        r = toku_logcursor_next(logcursor, &le);
        if (toku_recover_trace) 
            printf("%s:%d r=%d cmd=%c\n", __FUNCTION__, __LINE__, r, le ? le->cmd : '?');
        if (r != 0) {
            if (r == DB_RUNRECOVERY) {
                rr = DB_RUNRECOVERY; goto errorexit;
            }
            break;
        }
        logtype_dispatch_args(le, toku_recover_, renv);
    }

    r = toku_logcursor_destroy(&logcursor);
    assert(r == 0);
   
    // restart logging
    toku_logger_restart(renv->logger, lastlsn);

    // abort all of the remaining live transactions in reverse transaction id order
    struct varray *live_txns = NULL;
    r = varray_create(&live_txns, 1);
    assert(r == 0);
    toku_omt_iterate(renv->logger->live_txns, append_live_txn, live_txns);
    varray_sort(live_txns, compare_txn);
    varray_iterate(live_txns, abort_live_txn, NULL);
    varray_destroy(&live_txns);

    // close the open dictionaries
    file_map_close_dictionaries(&renv->fmap);

    // write a recovery log entry
    BYTESTRING recover_comment = { strlen("recover"), "recover" };
    r = toku_log_timestamp(renv->logger, NULL, TRUE, 0, recover_comment);
    assert(r == 0);

    // checkpoint 
    // TODO: checkpoint locks needed here?
    r = toku_cachetable_begin_checkpoint(renv->ct, renv->logger);
    assert(r == 0);
    // TODO: what about the error_string?
    r = toku_cachetable_end_checkpoint(renv->ct, renv->logger, NULL, NULL, NULL);
    assert(r == 0);

    r = chdir(org_wd); 
    assert(r == 0);

    return 0;

 errorexit:
    if (logcursor) {
        r = toku_logcursor_destroy(&logcursor);
        assert(r == 0);
    }

    r = chdir(org_wd); 
    assert(r == 0);

    return rr;
}

static int recover_lock(const char *lock_dir, int *lockfd) {
    const char fname[] = "/__recoverylock_dont_delete_me";
    int namelen=strlen(lock_dir);
    char lockfname[namelen+sizeof(fname)];

    int l = snprintf(lockfname, sizeof(lockfname), "%s%s", lock_dir, fname);
    assert(l+1 == (signed)(sizeof(lockfname)));
    *lockfd = toku_os_lock_file(lockfname);
    if (*lockfd < 0) {
        int e = errno;
        printf("Couldn't run recovery because some other process holds the recovery lock %s\n", lockfname);
        return e;
    }
    return 0;
}

static int recover_unlock(int lockfd) {
    int r = toku_os_unlock_file(lockfd);
    if (r != 0)
        return errno;
    return 0;
}

int tokudb_recover_delete_rolltmp_files(const char *UU(data_dir), const char *log_dir) {
    int lockfd = -1;

    int r = recover_lock(log_dir, &lockfd);
    if (r != 0)
        return r;

    r = toku_delete_rolltmp_files(log_dir);
    
    int rr = recover_unlock(lockfd);
    if (r == 0 && rr != 0)
        r = rr;
    
    return r;
}

int tokudb_recover(const char *data_dir, const char *log_dir, brt_compare_func bt_compare, brt_compare_func dup_compare) {
    int r;
    int lockfd = -1;

    r = recover_lock(log_dir, &lockfd);
    if (r != 0)
        return r;

    r = toku_delete_rolltmp_files(log_dir);
    if (r != 0) { 
        (void) recover_unlock(lockfd);
        return r;
    }

    int rr = 0;
    if (tokudb_needs_recovery(log_dir, FALSE)) {
        struct recover_env renv;
        r = recover_env_init(&renv, bt_compare, dup_compare);
        assert(r == 0);

        rr = do_recovery(&renv, data_dir, log_dir);

        recover_env_cleanup(&renv);
    }

    r = recover_unlock(lockfd);
    if (r != 0)
        return r;

    return rr;
}
