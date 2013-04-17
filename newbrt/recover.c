/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"
#include "log_header.h"
#include "checkpoint.h"

int toku_recover_trace = 0;

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

// File map tuple
struct file_map_tuple {
    FILENUM filenum;
    BRT brt;
    char *fname;
};

static void file_map_tuple_init(struct file_map_tuple *tuple, FILENUM filenum, BRT brt, char *fname) {
    tuple->filenum = filenum;
    tuple->brt = brt;
    tuple->fname = fname;
}

static void file_map_tuple_destroy(struct file_map_tuple *tuple) {
    if (tuple->fname) {
        toku_free(tuple->fname);
        tuple->fname = NULL;
    }
}

// Map filenum to brt, fname
// TODO why can't we use the cachetable to find by filenum?
struct file_map {
    OMT filenums;
};

static void file_map_init(struct file_map *fmap) {
    int r = toku_omt_create(&fmap->filenums);
    assert(r == 0);
}

static void file_map_destroy(struct file_map *fmap) {
    toku_omt_destroy(&fmap->filenums);
}

static void file_map_close_dictionaries(struct file_map *fmap, BOOL recovery_succeeded) {
    int r;

    while (1) {
        u_int32_t n = toku_omt_size(fmap->filenums);
        if (n == 0)
            break;
        OMTVALUE v;
        r = toku_omt_fetch(fmap->filenums, n-1, &v, NULL);
        assert(r == 0);
        r = toku_omt_delete_at(fmap->filenums, n-1);
        assert(r == 0);
        struct file_map_tuple *tuple = v;
	assert(tuple->brt);
        if (!recovery_succeeded) {
            // don't update the brt on close
            toku_brt_set_panic(tuple->brt, DB_RUNRECOVERY, "recovery failed");
        }
        //Logging is already back on.  No need to pass LSN into close.
        char *error_string = NULL;
        DB *fake_db = tuple->brt->db; //Need to free the fake db that was malloced
        r = toku_close_brt(tuple->brt, &error_string);
        if (!recovery_succeeded) {
            printf("%s:%d %d %s\n", __FUNCTION__, __LINE__, r, error_string);
            assert(r != 0);
        } else
            assert(r == 0);
        if (error_string)
            toku_free(error_string);
        toku_free(fake_db); //Must free the DB after the brt is closed

        file_map_tuple_destroy(tuple);
        toku_free(tuple);
    }
}

static int file_map_h(OMTVALUE omtv, void *v) {
    struct file_map_tuple *a = omtv;
    FILENUM *b = v;
    if (a->filenum.fileid < b->fileid) return -1;
    if (a->filenum.fileid > b->fileid) return +1;
    return 0;
}

static int file_map_insert (struct file_map *fmap, FILENUM fnum, BRT brt, char *fname) {
    struct file_map_tuple *tuple = toku_malloc(sizeof (struct file_map_tuple));
    assert(tuple);
    file_map_tuple_init(tuple, fnum, brt, fname);
    int r = toku_omt_insert(fmap->filenums, tuple, file_map_h, &fnum, NULL);
    return r;
}

static void file_map_remove(struct file_map *fmap, FILENUM fnum) {
    OMTVALUE v; u_int32_t idx;
    int r = toku_omt_find_zero(fmap->filenums, file_map_h, &fnum, &v, &idx, NULL);
    if (r == 0) {
        struct file_map_tuple *tuple = v;
        r = toku_omt_delete_at(fmap->filenums, idx);
        file_map_tuple_destroy(tuple);
        toku_free(tuple);
    }
}

static int file_map_find(struct file_map *fmap, FILENUM fnum, struct file_map_tuple **file_map_tuple) {
    OMTVALUE v; u_int32_t idx;
    int r = toku_omt_find_zero(fmap->filenums, file_map_h, &fnum, &v, &idx, NULL);
    if (r == 0) {
        struct file_map_tuple *tuple = v;
        assert(tuple->filenum.fileid == fnum.fileid);
        *file_map_tuple = tuple;
    }
    return r;
}

// The recovery environment
struct recover_env {
    CACHETABLE ct;
    TOKULOGGER logger;
    brt_compare_func bt_compare;
    brt_compare_func dup_compare;
    struct backward_scan_state bs;
    struct file_map fmap;
    BOOL goforward;
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
    renv->goforward = FALSE;

    if (toku_recover_trace)
        printf("%s:%d\n", __FUNCTION__, __LINE__);
    return r;
}

static void recover_env_cleanup (RECOVER_ENV renv, BOOL recovery_succeeded) {
    int r;

    file_map_close_dictionaries(&renv->fmap, recovery_succeeded);
    file_map_destroy(&renv->fmap);

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

static int toku_recover_commit (struct logtype_commit *l, RECOVER_ENV renv) {
    int r;

    // find the transaction by transaction id
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->txnid, &txn);
    assert(r == 0);
    if (txn == NULL)
        return 0;

    // commit the transaction
    r = toku_txn_commit_with_lsn(txn, TRUE, recover_yield, NULL, l->lsn);
    assert(r == 0);

    // close the transaction
    toku_txn_close_txn(txn);

    return 0;
}

static int toku_recover_backward_commit (struct logtype_commit *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_xabort (struct logtype_xabort *l, RECOVER_ENV renv) {
    int r;

    // find the transaction by transaction id
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->txnid, &txn);
    assert(r == 0);
    if (txn == NULL)
        return 0;

    // abort the transaction
    r = toku_txn_abort_with_lsn(txn, recover_yield, NULL, l->lsn);
    assert(r == 0);

    // close the transaction
    toku_txn_close_txn(txn);

    return 0;
}

static int toku_recover_backward_xabort (struct logtype_xabort *UU(l), RECOVER_ENV UU(renv)) {
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

static int
abort_on_upgrade(DB* UU(pdb),
                 u_int32_t UU(old_version), const DBT *UU(old_descriptor), const DBT *UU(old_key), const DBT *UU(old_val),
                 u_int32_t UU(new_version), const DBT *UU(new_descriptor), const DBT *UU(new_key), const DBT *UU(new_val)) {
    assert(FALSE); //Must not upgrade.
    return ENOSYS;
}


// Open the file if it is not already open.  If it is already open, then do nothing.
static int internal_toku_recover_fopen_or_fcreate (RECOVER_ENV renv, int flags, int mode, char *fixedfname, FILENUM filenum, u_int32_t treeflags, u_int32_t descriptor_version, BYTESTRING* descriptor) {
    int r;

    // already open
    struct file_map_tuple *tuple = NULL;
    r = file_map_find(&renv->fmap, filenum, &tuple);
    if (r == 0) {
        assert(strcmp(tuple->fname, fixedfname) == 0);
        toku_free(fixedfname);
        return 0;
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

    toku_brt_set_flags(brt, treeflags);

    // set the key compare functions
    if (!(treeflags & TOKU_DB_KEYCMP_BUILTIN) && renv->bt_compare)
        toku_brt_set_bt_compare(brt, renv->bt_compare);
    if (!(treeflags & TOKU_DB_VALCMP_BUILTIN) && renv->dup_compare)
        toku_brt_set_dup_compare(brt, renv->dup_compare);

    // bind to filenum when opened
    toku_brt_set_filenum(brt, filenum);

    // TODO mode (FUTURE FEATURE)
    mode = mode;

    //Create fake DB for comparison functions.
    DB *XCALLOC(fake_db);
    if (flags&O_CREAT && descriptor_version > 0) {
        DBT descriptor_dbt;
        toku_fill_dbt(&descriptor_dbt, descriptor->data, descriptor->len);
        r = toku_brt_set_descriptor(brt, descriptor_version, &descriptor_dbt, abort_on_upgrade);
        if (r!=0) goto close_brt;
    }
    r = toku_brt_open(brt, fixedfname, fixedfname, (flags & O_CREAT) != 0, FALSE, renv->ct, NULL, fake_db);
    if (r != 0) {
close_brt:;
        //Note:  If brt_open fails, then close_brt will NOT write a header to disk.
        //No need to provide lsn
        int rr = toku_close_brt(brt, NULL); assert(rr == 0);
        toku_free(fixedfname);
        toku_free(fake_db); //Free memory allocated for the fake db.
        if (r==ENOENT) //Not an error to simply be missing.
            r = 0;
        return r;
    }

    file_map_insert(&renv->fmap, filenum, brt, fixedfname);
    return 0;
}

static int toku_recover_fopen (struct logtype_fopen *l, RECOVER_ENV renv) {
    char *fixedfname = fixup_fname(&l->iname);
    return internal_toku_recover_fopen_or_fcreate(renv, 0, 0, fixedfname, l->filenum, l->treeflags, 0, NULL);
}

static int
maybe_do_fclose_during_recover_backward(RECOVER_ENV renv, FILENUM filenum) {
    if (renv->bs.bs == BS_SAW_CKPT_END) {
        // close the tree
        struct file_map_tuple *tuple = NULL;
        int r = file_map_find(&renv->fmap, filenum, &tuple);
        if (r == 0) {
            //Must keep existing lsn.
            //The only way this should be dirty, is if its doing a file-format upgrade.
            //If not dirty, header will not be written.
            DB *fake_db = tuple->brt->db; //Need to free the fake db that was malloced
            r = toku_close_brt_lsn(tuple->brt, 0, TRUE, tuple->brt->h->checkpoint_lsn);
            assert(r == 0);
            toku_free(fake_db); //Must free the DB after the brt is closed
            file_map_remove(&renv->fmap, filenum);
        }
    }
    return 0;
}

static int toku_recover_backward_fopen (struct logtype_fopen *l, RECOVER_ENV renv) {
    int r = maybe_do_fclose_during_recover_backward(renv, l->filenum);
    assert(r==0);
    return 0;
}

// fcreate is like fopen except that the file must be created. Also creates the dir if needed.
static int toku_recover_fcreate (struct logtype_fcreate *l, RECOVER_ENV renv) {
    char *fixedfname = fixup_fname(&l->iname);
    create_dir_from_file(fixedfname);
    return internal_toku_recover_fopen_or_fcreate(renv, O_CREAT|O_TRUNC, l->mode, fixedfname, l->filenum, l->treeflags, l->descriptor_version, &l->descriptor);
}

static int toku_recover_backward_fcreate (struct logtype_fcreate *UU(l), RECOVER_ENV UU(renv)) {
    int r = maybe_do_fclose_during_recover_backward(renv, l->filenum);
    assert(r==0);
    return 0;
}

// fdelete is a transactional file delete.
static int toku_recover_fdelete (struct logtype_fdelete *l, RECOVER_ENV renv) {
    TOKUTXN txn = NULL;
    int r = toku_txnid2txn(renv->logger, l->txnid, &txn);
    assert(r == 0);
    //TODO: #??? (insert ticket number) bug that could leave orphaned files here.
    if (txn == NULL)
        return 0;
    char *fixediname = fixup_fname(&l->iname);
    { //Skip if does not exist.
        toku_struct_stat buf;
        r = toku_stat(fixediname, &buf);
        if (r==-1 && errno==ENOENT)
            goto cleanup;
    }
    DBT iname_dbt;
    toku_fill_dbt(&iname_dbt, fixediname, strlen(fixediname)+1);
    r = toku_brt_remove_on_commit(txn, &iname_dbt, &iname_dbt);
    assert(r==0);
cleanup:
    toku_free(fixediname);
    return 0;
}

static int toku_recover_backward_fdelete (struct logtype_fdelete *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_enq_insert (struct logtype_enq_insert *l, RECOVER_ENV renv) {
    struct file_map_tuple *tuple = NULL;
    int r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r!=0) {
	// if we didn't find a cachefile, then we don't have to do anything.
	return 0;
    }    
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(r == 0);
    if (txn == NULL)
        return 0;
    DBT keydbt, valdbt;
    toku_fill_dbt(&keydbt, l->key.data, l->key.len);
    toku_fill_dbt(&valdbt, l->value.data, l->value.len);
    r = toku_brt_maybe_insert(tuple->brt, &keydbt, &valdbt, txn, TRUE, l->lsn);
    assert(r == 0);

    return 0;
}

static int toku_recover_backward_enq_insert (struct logtype_enq_insert *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_tablelock_on_empty_table(struct logtype_tablelock_on_empty_table *l, RECOVER_ENV renv) {
    struct file_map_tuple *tuple = NULL;
    int r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r!=0) {
	// if we didn't find a cachefile, then we don't have to do anything.
	return 0;
    }    
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(r == 0);
    if (txn != NULL) {
        r = toku_brt_note_table_lock(tuple->brt, txn);
        assert(r == 0);
    }
    return 0;
}

static int toku_recover_backward_tablelock_on_empty_table(struct logtype_tablelock_on_empty_table *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_enq_delete_both (struct logtype_enq_delete_both *l, RECOVER_ENV renv) {
    struct file_map_tuple *tuple = NULL;
    int r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r!=0) {
	// if we didn't find a cachefile, then we don't have to do anything.
	return 0;
    }    
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(r == 0);
    if (txn == NULL)
        return 0;
    DBT keydbt, valdbt;
    toku_fill_dbt(&keydbt, l->key.data, l->key.len);
    toku_fill_dbt(&valdbt, l->value.data, l->value.len);
    r = toku_brt_maybe_delete_both(tuple->brt, &keydbt, &valdbt, txn, TRUE, l->lsn);
    assert(r == 0);

    return 0;
}

static int toku_recover_backward_enq_delete_both (struct logtype_enq_delete_both *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_enq_delete_any (struct logtype_enq_delete_any *l, RECOVER_ENV renv) {
    struct file_map_tuple *tuple = NULL;
    int r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r!=0) {
	// if we didn't find a cachefile, then we don't have to do anything.
	return 0;
    }
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(r == 0);
    if (txn == NULL)
        return 0;
    DBT keydbt;
    toku_fill_dbt(&keydbt, l->key.data, l->key.len);
    r = toku_brt_maybe_delete(tuple->brt, &keydbt, txn, TRUE, l->lsn);
    assert(r == 0);

    return 0;
}

static int toku_recover_backward_enq_delete_any (struct logtype_enq_delete_any *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_fclose (struct logtype_fclose *l, RECOVER_ENV UU(renv)) {
    struct file_map_tuple *tuple = NULL;
    int r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r == 0) {
        char *fixedfname = fixup_fname(&l->iname);
        assert(strcmp(tuple->fname, fixedfname) == 0);
        toku_free(fixedfname);
        DB *fake_db = tuple->brt->db; //Need to free the fake db that was malloced
        r = toku_close_brt_lsn(tuple->brt, 0, TRUE, l->lsn);
        assert(r == 0);
        toku_free(fake_db); //Must free the DB after the brt is closed
        file_map_remove(&renv->fmap, l->filenum);
    }
    return 0;
}

static int toku_recover_backward_fclose (struct logtype_fclose *l, RECOVER_ENV renv) {
    if (renv->bs.bs == BS_SAW_CKPT) {
        // tree open
        char *fixedfname = fixup_fname(&l->iname);
        internal_toku_recover_fopen_or_fcreate(renv, 0, 0, fixedfname, l->filenum, l->treeflags, 0, NULL);
    }
    return 0;
}

static int toku_recover_begin_checkpoint (struct logtype_begin_checkpoint *UU(l), RECOVER_ENV UU(renv)) {
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
            renv->goforward = TRUE;
	    return 0;
	} else 
            return 0;
    case BS_SAW_CKPT:
	return 0; // ignore it
    }
    fprintf(stderr, "%s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)bs->bs);
    abort();
}

static int toku_recover_end_checkpoint (struct logtype_end_checkpoint *UU(l), RECOVER_ENV UU(renv)) {
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

static int toku_recover_fassociate (struct logtype_fassociate *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_backward_fassociate (struct logtype_fassociate *l, RECOVER_ENV renv) {
    char *fixedfname = fixup_fname(&l->iname);
    return internal_toku_recover_fopen_or_fcreate(renv, 0, 0, fixedfname, l->filenum, l->treeflags, 0, NULL);
}

static int toku_recover_xstillopen (struct logtype_xstillopen *UU(l), RECOVER_ENV UU(renv)) {
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

static int toku_recover_xbegin (struct logtype_xbegin *l, RECOVER_ENV renv) {
    int r;

    // lookup the parent
    TOKUTXN parent = NULL;
    if (l->parenttxnid != 0) {
        r = toku_txnid2txn(renv->logger, l->parenttxnid, &parent);
        assert(r == 0);
        if (parent == NULL)
            return 0;
    }

    // create a transaction and bind it to the transaction id
    TOKUTXN txn = NULL;
    r = toku_txn_begin_with_xid(parent, &txn, renv->logger, l->lsn.lsn);
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
            renv->goforward = TRUE;
	    return 0;
	} else {
	    fprintf(stderr, "Scanning back at xbegin %" PRIu64 " (looking for %" PRIu64 ")\n", l->lsn.lsn, bs->min_live_txn);
	    return 0;
	}
    }
    fprintf(stderr, "%s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)bs->bs);
    abort();
}

static int toku_recover_timestamp (struct logtype_timestamp *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_backward_timestamp (struct logtype_timestamp *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_shutdown (struct logtype_shutdown *UU(l), RECOVER_ENV UU(renv)) {
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

// abort all of the remaining live transactions in descending transaction id order
static void recover_abort_live_txns(RECOVER_ENV renv) {
    int r;
    while (1) {
        u_int32_t n_live_txns = toku_omt_size(renv->logger->live_txns);
        if (n_live_txns == 0)
            break;
        OMTVALUE v;
        r = toku_omt_fetch(renv->logger->live_txns, n_live_txns-1, &v, NULL);
        if (r != 0)
            break;
        TOKUTXN txn = (TOKUTXN) v;

        // abort the transaction
        r = toku_txn_abort_txn(txn, recover_yield, NULL);
        assert(r == 0);

        // close the transaction
        toku_txn_close_txn(txn);
    }
}

static int do_recovery(RECOVER_ENV renv, const char *env_dir, const char *log_dir) {
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

    // grab the last LSN so that it can be restored when the log is restarted
    LSN lastlsn = toku_logger_last_lsn(renv->logger);

    // there must be at least one log entry
    r = toku_logcursor_create(&logcursor, log_dir);
    assert(r == 0);
    
    r = toku_logcursor_last(logcursor, &le);
    if (r != 0) {
        if (toku_recover_trace) 
            printf("RUNRECOVERY: %s:%d r=%d\n", __FUNCTION__, __LINE__, r);
        rr = DB_RUNRECOVERY; goto errorexit;
    }

    // TODO use logcursor->invalidate()
    r = toku_logcursor_destroy(&logcursor);
    assert(r == 0);

    r = toku_logcursor_create(&logcursor, log_dir);
    assert(r == 0);

    r = chdir(env_dir); 
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
        if (toku_recover_trace) 
            printf("%s:%d r=%d cmd=%c\n", __FUNCTION__, __LINE__, r, le ? le->cmd : '?');
        if (r != 0) {
            if (toku_recover_trace) 
                printf("DB_RUNRECOVERY: %s:%d r=%d\n", __FUNCTION__, __LINE__, r);
            rr = DB_RUNRECOVERY; goto errorexit;
        }
        if (renv->goforward) {
            logtype_dispatch_assign(le, toku_recover_, r, renv);
            if (toku_recover_trace) 
                printf("%s:%d r=%d cmd=%c\n", __FUNCTION__, __LINE__, r, le ? le->cmd : '?');
            if (r != 0) {
                if (toku_recover_trace) 
                    printf("DB_RUNRECOVERY: %s:%d r=%d\n", __FUNCTION__, __LINE__, r);
                rr = DB_RUNRECOVERY; goto errorexit;
            }
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
        logtype_dispatch_assign(le, toku_recover_, r, renv);
        if (toku_recover_trace) 
            printf("%s:%d r=%d cmd=%c\n", __FUNCTION__, __LINE__, r, le ? le->cmd : '?');
        if (r != 0) {
            if (toku_recover_trace) 
                printf("DB_RUNRECOVERY: %s:%d r=%d\n", __FUNCTION__, __LINE__, r);
            rr = DB_RUNRECOVERY; goto errorexit;
        }
    }

    r = toku_logcursor_destroy(&logcursor);
    assert(r == 0);
   
    // restart logging
    toku_logger_restart(renv->logger, lastlsn);

    // abort the live transactions 
    recover_abort_live_txns(renv);

    // close the open dictionaries
    file_map_close_dictionaries(&renv->fmap, TRUE);

    // write a recovery log entry
    BYTESTRING recover_comment = { strlen("recover"), "recover" };
    r = toku_log_timestamp(renv->logger, NULL, TRUE, 0, recover_comment);
    assert(r == 0);

    // checkpoint 
    r = toku_checkpoint(renv->ct, renv->logger, NULL, NULL, NULL, NULL);
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
    const char fname[] = "/__tokudb_recoverylock_dont_delete_me";
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

int tokudb_recover(const char *env_dir, const char *log_dir, brt_compare_func bt_compare, brt_compare_func dup_compare) {
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

        rr = do_recovery(&renv, env_dir, log_dir);

        recover_env_cleanup(&renv, (BOOL)(rr == 0));
    }

    r = recover_unlock(lockfd);
    if (r != 0)
        return r;

    return rr;
}
