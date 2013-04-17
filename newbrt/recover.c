/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"
#include "log_header.h"
#include "checkpoint.h"

int tokudb_recovery_trace = 0;                    // turn on recovery tracing, default off.

//#define DO_VERIFY_COUNTS
#ifdef DO_VERIFY_COUNTS
#define VERIFY_COUNTS(n) toku_verify_counts(n)
#else
#define VERIFY_COUNTS(n) ((void)0)
#endif

// time in seconds between recovery progress reports
#define TOKUDB_RECOVERY_PROGRESS_TIME 15

struct scan_state {
    enum {
        BACKWARD_NEWER_CHECKPOINT_END = 1,
        BACKWARD_BETWEEN_CHECKPOINT_BEGIN_END,
        BACKWARD_OLDER_CHECKPOINT_BEGIN,
        FORWARD_OLDER_CHECKPOINT_BEGIN,
        FORWARD_BETWEEN_CHECKPOINT_BEGIN_END,
        FORWARD_NEWER_CHECKPOINT_END,
    } ss;
    LSN checkpoint_lsn;
    uint64_t checkpoint_timestamp;
    int n_live_txns;
    TXNID oldest_txnid;
};

static void scan_state_init(struct scan_state *ss) {
    ss->ss = BACKWARD_NEWER_CHECKPOINT_END; ss->checkpoint_lsn = ZERO_LSN; ss->n_live_txns = 0; ss->oldest_txnid = 0;
}

static const char *scan_state_strings[] = {
    "?", "bw_newer", "bw_between", "bw_older", "fw_older", "fw_between", "fw_newer",
};

static const char *scan_state_string(struct scan_state *ss) {
    assert(BACKWARD_NEWER_CHECKPOINT_END <= ss->ss && ss->ss <= FORWARD_NEWER_CHECKPOINT_END);
    return scan_state_strings[ss->ss];
}

// File map tuple
struct file_map_tuple {
    FILENUM filenum;
    BRT brt;
    char *iname;
};

static void file_map_tuple_init(struct file_map_tuple *tuple, FILENUM filenum, BRT brt, char *iname) {
    tuple->filenum = filenum;
    tuple->brt = brt;
    tuple->iname = iname;
}

static void file_map_tuple_destroy(struct file_map_tuple *tuple) {
    if (tuple->iname) {
        toku_free(tuple->iname);
        tuple->iname = NULL;
    }
}

// Map filenum to brt
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

static uint32_t file_map_get_num_dictionaries(struct file_map *fmap) {
    return toku_omt_size(fmap->filenums);
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
            if (tokudb_recovery_trace)
                fprintf(stderr, "%s:%d %d %s\n", __FUNCTION__, __LINE__, r, error_string);
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

static int file_map_insert (struct file_map *fmap, FILENUM fnum, BRT brt, char *iname) {
    struct file_map_tuple *tuple = toku_malloc(sizeof (struct file_map_tuple));
    assert(tuple);
    file_map_tuple_init(tuple, fnum, brt, iname);
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
    generate_keys_vals_for_put_func generate_keys_vals_for_put;
    cleanup_keys_vals_for_put_func  cleanup_keys_vals_for_put;
    generate_keys_for_del_func      generate_keys_for_del;
    cleanup_keys_for_del_func       cleanup_keys_for_del;
    struct scan_state ss;
    struct file_map fmap;
    BOOL goforward;
};
typedef struct recover_env *RECOVER_ENV;

static int recover_env_init (RECOVER_ENV renv, brt_compare_func bt_compare, brt_compare_func dup_compare,
                             generate_keys_vals_for_put_func generate_keys_vals_for_put,
                             cleanup_keys_vals_for_put_func  cleanup_keys_vals_for_put,
                             generate_keys_for_del_func      generate_keys_for_del,
                             cleanup_keys_for_del_func       cleanup_keys_for_del,
                             size_t cachetable_size) {
    int r;

    r = toku_create_cachetable(&renv->ct, cachetable_size ? cachetable_size : 1<<25, (LSN){0}, 0);
    assert(r == 0);
    r = toku_logger_create(&renv->logger);
    assert(r == 0);
    toku_logger_write_log_files(renv->logger, FALSE);
    toku_logger_set_cachetable(renv->logger, renv->ct);
    renv->bt_compare = bt_compare;
    renv->dup_compare = dup_compare;
    renv->generate_keys_vals_for_put = generate_keys_vals_for_put;
    renv->cleanup_keys_vals_for_put = cleanup_keys_vals_for_put;
    renv->generate_keys_for_del = generate_keys_for_del;
    renv->cleanup_keys_for_del = cleanup_keys_for_del;
    file_map_init(&renv->fmap);
    renv->goforward = FALSE;

    if (tokudb_recovery_trace)
        fprintf(stderr, "%s:%d\n", __FUNCTION__, __LINE__);
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

    if (tokudb_recovery_trace)
        fprintf(stderr, "%s:%d\n", __FUNCTION__, __LINE__);
}

static const char *recover_state(RECOVER_ENV renv) {
    return scan_state_string(&renv->ss);
}

// Null function supplied to transaction commit and abort
static void recover_yield(voidfp UU(f), void *UU(extra)) {
    // nothing
}

static int
abort_on_upgrade(DB* UU(pdb),
                 u_int32_t UU(old_version), const DBT *UU(old_descriptor), const DBT *UU(old_key), const DBT *UU(old_val),
                 u_int32_t UU(new_version), const DBT *UU(new_descriptor), const DBT *UU(new_key), const DBT *UU(new_val)) {
    assert(FALSE); //Must not upgrade.
    return ENOSYS;
}

// Open the file if it is not already open.  If it is already open, then do nothing.
static int internal_toku_recover_fopen_or_fcreate (RECOVER_ENV renv, BOOL must_create, int mode, BYTESTRING *bs_iname, FILENUM filenum, u_int32_t treeflags, 
                                                   u_int32_t descriptor_version, BYTESTRING* descriptor, int recovery_force_fcreate, TOKUTXN txn) {
    int r;
    char *iname = fixup_fname(bs_iname);

    BRT brt = NULL;
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
    if (descriptor_version > 0) {
        DBT descriptor_dbt;
        toku_fill_dbt(&descriptor_dbt, descriptor->data, descriptor->len);
        r = toku_brt_set_descriptor(brt, descriptor_version, &descriptor_dbt, abort_on_upgrade);
        if (r!=0) goto close_brt;
    }
    r = toku_brt_open_recovery(brt, iname, iname, must_create, must_create, renv->ct, txn, fake_db, recovery_force_fcreate);
    if (r != 0) {
    close_brt:
        ;
        //Note:  If brt_open fails, then close_brt will NOT write a header to disk.
        //No need to provide lsn
        int rr = toku_close_brt(brt, NULL); assert(rr == 0);
        toku_free(iname);
        toku_free(fake_db); //Free memory allocated for the fake db.
        if (r==ENOENT) //Not an error to simply be missing.
            r = 0;
        return r;
    }

    file_map_insert(&renv->fmap, filenum, brt, iname);
    return 0;
}

static int
maybe_do_fclose_during_recover_backward(RECOVER_ENV renv, FILENUM filenum, BYTESTRING *bs_iname) {
    // close the tree
    struct file_map_tuple *tuple = NULL;
    int r = file_map_find(&renv->fmap, filenum, &tuple);
    if (r == 0) {
        char *iname = fixup_fname(bs_iname);
        assert(strcmp(tuple->iname, iname) == 0);
        toku_free(iname);

        struct scan_state *ss = &renv->ss;
        assert(ss->ss == BACKWARD_OLDER_CHECKPOINT_BEGIN);

        //Must keep existing lsn.
        //The only way this should be dirty, is if its doing a file-format upgrade.
        //If not dirty, header will not be written.
        DB *fake_db = tuple->brt->db; //Need to free the fake db that was malloced
        r = toku_close_brt_lsn(tuple->brt, 0, TRUE, tuple->brt->h->checkpoint_lsn);
        assert(r == 0);
        toku_free(fake_db); //Must free the DB after the brt is closed
        file_map_remove(&renv->fmap, filenum);
    }
    return 0;
}

// fcreate is like fopen except that the file must be created.
static int toku_recover_fcreate (struct logtype_fcreate *l, RECOVER_ENV renv) {
    struct scan_state *ss = &renv->ss;
    int r;

    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->txnid, &txn);
    assert(r == 0);

    // assert that filenum is closed
    struct file_map_tuple *tuple = NULL;
    r = file_map_find(&renv->fmap, l->filenum, &tuple);
    assert(r != 0);

    BOOL must_create;
    if (ss->ss == FORWARD_OLDER_CHECKPOINT_BEGIN) {
        must_create = FALSE; // do not create file if it does not exist
    } else {
        assert(txn != NULL); //Straddle txns cannot happen after checkpoint
        assert(ss->ss == FORWARD_BETWEEN_CHECKPOINT_BEGIN_END || ss->ss == FORWARD_NEWER_CHECKPOINT_END);
        must_create = TRUE;

        // maybe unlink
        char *iname = fixup_fname(&l->iname);
        r = unlink(iname);
        if (r != 0 && errno != ENOENT) {
            fprintf(stderr, "Tokudb recovery %s:%d unlink %s %d\n", __FUNCTION__, __LINE__, iname, errno);
            toku_free(iname);
            return r;
        }
        toku_free(iname);
    }

    r = internal_toku_recover_fopen_or_fcreate(renv, must_create, l->mode, &l->iname, l->filenum, l->treeflags, l->descriptor_version, &l->descriptor, 1, txn);
    return r;
}

static int toku_recover_backward_fcreate (struct logtype_fcreate *l, RECOVER_ENV renv) {
    int r = maybe_do_fclose_during_recover_backward(renv, l->filenum, &l->iname);
    assert(r==0);
    return 0;
}

static int toku_recover_fopen (struct logtype_fopen *l, RECOVER_ENV renv) {
    // check if the file is already open by backward scan of fassociate
    struct file_map_tuple *tuple = NULL;
    char *iname = fixup_fname(&l->iname);
    int r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r == 0) {
        // file is already opened by fassociate
        assert(strcmp(tuple->iname, iname) == 0);
    } else {
        // file is not open, open it
        r = internal_toku_recover_fopen_or_fcreate(renv, FALSE, 0, &l->iname, l->filenum, l->treeflags, 0, NULL, 0, NULL);
    }
    toku_free(iname);
    return r;
}

static int toku_recover_backward_fopen (struct logtype_fopen *l, RECOVER_ENV renv) {
    int r = maybe_do_fclose_during_recover_backward(renv, l->filenum, &l->iname);
    assert(r==0);
    return 0;
}

static int toku_recover_fclose (struct logtype_fclose *l, RECOVER_ENV renv) {
    struct file_map_tuple *tuple = NULL;
    int r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r == 0) {
        char *iname = fixup_fname(&l->iname);
        assert(strcmp(tuple->iname, iname) == 0);
        toku_free(iname);

        DB *fake_db = tuple->brt->db; //Need to free the fake db that was malloced
        r = toku_close_brt_lsn(tuple->brt, 0, TRUE, l->lsn);
        assert(r == 0);
        toku_free(fake_db); //Must free the DB after the brt is closed
        file_map_remove(&renv->fmap, l->filenum);
    }
    return 0;
}

static int toku_recover_backward_fclose (struct logtype_fclose *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

// fdelete is a transactional file delete.
static int toku_recover_fdelete (struct logtype_fdelete *l, RECOVER_ENV renv) {
    TOKUTXN txn = NULL;
    int r = toku_txnid2txn(renv->logger, l->txnid, &txn);
    assert(r == 0);
    if (txn == NULL) {
        //This is a straddle txn.
        assert(renv->ss.ss == FORWARD_OLDER_CHECKPOINT_BEGIN); //cannot happen after checkpoint begin
        return 0;
    }
    char *fixediname = fixup_fname(&l->iname);
    { //Skip if does not exist.
        toku_struct_stat buf;
        r = toku_stat(fixediname, &buf);
        if (r==-1 && errno==ENOENT)
            goto cleanup;
    }
    
    // txn exists and file exists, so create fdelete rollback entry
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
    } else {
        //This is a straddle txn.
        assert(renv->ss.ss == FORWARD_OLDER_CHECKPOINT_BEGIN); //cannot happen after checkpoint begin
    }
    return 0;
}

static int toku_recover_backward_tablelock_on_empty_table(struct logtype_tablelock_on_empty_table *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_enq_insert (struct logtype_enq_insert *l, RECOVER_ENV renv) {
    int r;
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(r == 0);
    if (txn == NULL) {
        //This is a straddle txn.
        assert(renv->ss.ss == FORWARD_OLDER_CHECKPOINT_BEGIN); //cannot happen after checkpoint begin
        return 0;
    }
    struct file_map_tuple *tuple = NULL;
    r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r!=0) {
	// if we didn't find a cachefile, then we don't have to do anything.
	return 0;
    }    
    DBT keydbt, valdbt;
    toku_fill_dbt(&keydbt, l->key.data, l->key.len);
    toku_fill_dbt(&valdbt, l->value.data, l->value.len);
    r = toku_brt_maybe_insert(tuple->brt, &keydbt, &valdbt, txn, TRUE, l->lsn, FALSE);
    assert(r == 0);

    return 0;
}

static int toku_recover_backward_enq_insert (struct logtype_enq_insert *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_enq_insert_multiple (struct logtype_enq_insert_multiple *l, RECOVER_ENV renv) {
    int r;
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(r == 0);
    if (txn == NULL) {
        //This is a straddle txn.
        assert(renv->ss.ss == FORWARD_OLDER_CHECKPOINT_BEGIN); //cannot happen after checkpoint begin
        return 0;
    }
#if 0
                                int (*generate_keys_vals_for_put) (DBT *row, uint32_t num_dbs, DB *dbs[num_dbs], DBT keys[num_dbs], DBT vals[num_dbs], void *extra),
                                int (*cleanup_keys_vals_for_put) (DBT *row, uint32_t num_dbs, DB *dbs[num_dbs], DBT keys[num_dbs], DBT vals[num_dbs], void *extra),
                                int (*generate_keys_for_del) (DBT *row, uint32_t num_dbs, DB *dbs[num_dbs], DBT keys[num_dbs], void *extra),
                                int (*cleanup_keys_for_del) (DBT *row, uint32_t num_dbs, DB *dbs[num_dbs], DBT keys[num_dbs], void *extra));
#endif
    DB* dbs[l->filenums.num];
    memset(dbs, 0, sizeof(dbs));
    uint32_t file;
    uint32_t num_dbs = 0;
    for (file = 0; file < l->filenums.num; file++) {
        struct file_map_tuple *tuple = NULL;
        r = file_map_find(&renv->fmap, l->filenums.filenums[file], &tuple);
        if (r!=0) {
            // if we didn't find a cachefile, then we don't have to do anything for this file.
            continue;
        }
        dbs[num_dbs++] = tuple->brt->db;
    }
    if (num_dbs == 0) //All files are closed/deleted.  We're done.
        return 0;
    DBT keydbts[num_dbs], valdbts[num_dbs], rowdbt;
    memset(keydbts, 0, sizeof(keydbts));
    memset(valdbts, 0, sizeof(valdbts));
    //Generate all the DBTs
    toku_fill_dbt(&rowdbt, l->row.data, l->row.len);
    r = renv->generate_keys_vals_for_put(&rowdbt, num_dbs, dbs, keydbts, valdbts, NULL);
    assert(r==0);

    uint32_t which_db = 0;
    for (file = 0; file < l->filenums.num; file++) {
        struct file_map_tuple *tuple = NULL;
        r = file_map_find(&renv->fmap, l->filenums.filenums[file], &tuple);
        if (r!=0) {
            // if we didn't find a cachefile, then we don't have to do anything for this file.
            continue;
        }
        assert(tuple->brt->db == dbs[which_db]);

        r = toku_brt_maybe_insert(tuple->brt, &keydbts[which_db], &valdbts[which_db], txn, TRUE, l->lsn, FALSE);
        assert(r == 0);
        which_db++;
    }
    assert(which_db == num_dbs);

    //Do cleanup of all dbts.
    r = renv->cleanup_keys_vals_for_put(&rowdbt, num_dbs, dbs, keydbts, valdbts, NULL);
    assert(r==0);

    return 0;
}

static int toku_recover_backward_enq_insert_multiple (struct logtype_enq_insert_multiple *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_enq_delete_multiple (struct logtype_enq_delete_multiple *l, RECOVER_ENV renv) {
    int r;
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(r == 0);
    if (txn == NULL) {
        //This is a straddle txn.
        assert(renv->ss.ss == FORWARD_OLDER_CHECKPOINT_BEGIN); //cannot happen after checkpoint begin
        return 0;
    }

    DB* dbs[l->filenums.num];
    memset(dbs, 0, sizeof(dbs));
    uint32_t file;
    uint32_t num_dbs = 0;
    for (file = 0; file < l->filenums.num; file++) {
        struct file_map_tuple *tuple = NULL;
        r = file_map_find(&renv->fmap, l->filenums.filenums[file], &tuple);
        if (r!=0) {
            // if we didn't find a cachefile, then we don't have to do anything for this file.
            continue;
        }
        dbs[num_dbs++] = tuple->brt->db;
    }
    if (num_dbs == 0) //All files are closed/deleted.  We're done.
        return 0;
    DBT keydbts[num_dbs], rowdbt;
    memset(keydbts, 0, sizeof(keydbts));
 
    //Generate all the DBTs
    toku_fill_dbt(&rowdbt, l->row.data, l->row.len);
    r = renv->generate_keys_for_del(&rowdbt, num_dbs, dbs, keydbts, NULL);
    assert(r==0);

    uint32_t which_db = 0;
    for (file = 0; file < l->filenums.num; file++) {
        struct file_map_tuple *tuple = NULL;
        r = file_map_find(&renv->fmap, l->filenums.filenums[file], &tuple);
        if (r!=0) {
            // if we didn't find a cachefile, then we don't have to do anything for this file.
            continue;
        }
        assert(tuple->brt->db == dbs[which_db]);

        r = toku_brt_maybe_delete(tuple->brt, &keydbts[which_db], txn, TRUE, l->lsn, FALSE);
        assert(r == 0);
        which_db++;
    }
    assert(which_db == num_dbs);

    //Do cleanup of all dbts.
    r = renv->cleanup_keys_for_del(&rowdbt, num_dbs, dbs, keydbts, NULL);
    assert(r==0);

    return 0;
}

static int toku_recover_backward_enq_delete_multiple (struct logtype_enq_delete_multiple *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}


static int toku_recover_enq_delete_both (struct logtype_enq_delete_both *l, RECOVER_ENV renv) {
    int r;
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(r == 0);
    if (txn == NULL) {
        //This is a straddle txn.
        assert(renv->ss.ss == FORWARD_OLDER_CHECKPOINT_BEGIN); //cannot happen after checkpoint begin
        return 0;
    }
    struct file_map_tuple *tuple = NULL;
    r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r!=0) {
	// if we didn't find a cachefile, then we don't have to do anything.
	return 0;
    }    
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
    int r;
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(r == 0);
    if (txn == NULL) {
        //This is a straddle txn.
        assert(renv->ss.ss == FORWARD_OLDER_CHECKPOINT_BEGIN); //cannot happen after checkpoint begin
        return 0;
    }
    struct file_map_tuple *tuple = NULL;
    r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r!=0) {
	// if we didn't find a cachefile, then we don't have to do anything.
	return 0;
    }
    DBT keydbt;
    toku_fill_dbt(&keydbt, l->key.data, l->key.len);
    r = toku_brt_maybe_delete(tuple->brt, &keydbt, txn, TRUE, l->lsn, FALSE);
    assert(r == 0);

    return 0;
}

static int toku_recover_backward_enq_delete_any (struct logtype_enq_delete_any *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_begin_checkpoint (struct logtype_begin_checkpoint *l, RECOVER_ENV renv) {
    switch (renv->ss.ss) {
    case FORWARD_OLDER_CHECKPOINT_BEGIN:
        assert(l->lsn.lsn <= renv->ss.checkpoint_lsn.lsn);
        if (l->lsn.lsn == renv->ss.checkpoint_lsn.lsn)
            renv->ss.ss = FORWARD_BETWEEN_CHECKPOINT_BEGIN_END;
	return 0;
    case FORWARD_BETWEEN_CHECKPOINT_BEGIN_END:
        assert(0);
	return 0;
    case FORWARD_NEWER_CHECKPOINT_END:
        assert(l->lsn.lsn > renv->ss.checkpoint_lsn.lsn);
        return 0; // ignore it (log only has a begin checkpoint)
    default:
        break;
    }
    fprintf(stderr, "Tokudb recovery %s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)renv->ss.ss);
    abort();
    // nothing
    return 0;
}

static int toku_recover_backward_begin_checkpoint (struct logtype_begin_checkpoint *l, RECOVER_ENV renv) {
    time_t tnow = time(NULL);
    fprintf(stderr, "%.24s Tokudb recovery bw_begin_checkpoint at %"PRIu64" timestamp %"PRIu64" (%s)\n", ctime(&tnow), l->lsn.lsn, l->timestamp, recover_state(renv));
    switch (renv->ss.ss) {
    case BACKWARD_OLDER_CHECKPOINT_BEGIN:
        assert(l->lsn.lsn < renv->ss.checkpoint_lsn.lsn);
	return 0; // ignore it
    case BACKWARD_BETWEEN_CHECKPOINT_BEGIN_END:
	assert(renv->ss.checkpoint_lsn.lsn == l->lsn.lsn);
	if (renv->ss.n_live_txns==0) {
            renv->ss.ss = FORWARD_OLDER_CHECKPOINT_BEGIN;
            renv->goforward = TRUE;
            tnow = time(NULL);
	    fprintf(stderr, "%.24s Tokudb recovery turning around at begin checkpoint %"PRIu64" time %"PRIu64"\n", 
                    ctime(&tnow), l->lsn.lsn, renv->ss.checkpoint_timestamp - l->timestamp);
	} else {
            renv->ss.ss = BACKWARD_OLDER_CHECKPOINT_BEGIN;
            tnow = time(NULL);
	    fprintf(stderr, "%.24s Tokudb recovery begin checkpoint at %"PRIu64" looking for %"PRIu64" time %"PRIu64".  Scanning backwards through %"PRIu64" log entries.\n", 
                    ctime(&tnow), l->lsn.lsn, renv->ss.oldest_txnid, renv->ss.checkpoint_timestamp - l->timestamp, l->lsn.lsn - renv->ss.oldest_txnid);
        }
        return 0;
    case BACKWARD_NEWER_CHECKPOINT_END:
	return 0; // incomplete checkpoint
    default:
        break;
    }
    fprintf(stderr, "Tokudb recovery %s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)renv->ss.ss);
    abort();
}

static int toku_recover_end_checkpoint (struct logtype_end_checkpoint *l, RECOVER_ENV renv) {
    switch (renv->ss.ss) {
    case FORWARD_OLDER_CHECKPOINT_BEGIN:
        assert(l->lsn.lsn < renv->ss.checkpoint_lsn.lsn);
        return 0;
    case FORWARD_BETWEEN_CHECKPOINT_BEGIN_END:
        assert(l->txnid == renv->ss.checkpoint_lsn.lsn);
        renv->ss.ss = FORWARD_NEWER_CHECKPOINT_END;
        return 0;
    case FORWARD_NEWER_CHECKPOINT_END:
        assert(0);
        return 0;
    default:
        assert(0);
        return 0;
    }
}

static int toku_recover_backward_end_checkpoint (struct logtype_end_checkpoint *l, RECOVER_ENV renv) {
    time_t tnow = time(NULL);
    fprintf(stderr, "%.24s Tokudb recovery bw_end_checkpoint at %"PRIu64" timestamp %"PRIu64" txnid %"PRIu64" (%s)\n", ctime(&tnow), l->lsn.lsn, l->timestamp, l->txnid, recover_state(renv));
    switch (renv->ss.ss) {
    case BACKWARD_OLDER_CHECKPOINT_BEGIN:
	return 0;
    case BACKWARD_BETWEEN_CHECKPOINT_BEGIN_END:
	fprintf(stderr, "Tokudb recovery %s:%d Should not see two end_checkpoint log entries without an intervening begin_checkpoint\n", __FILE__, __LINE__);
	abort();
    case BACKWARD_NEWER_CHECKPOINT_END:
	renv->ss.ss = BACKWARD_BETWEEN_CHECKPOINT_BEGIN_END;
	renv->ss.checkpoint_lsn.lsn = l->txnid;
        renv->ss.checkpoint_timestamp = l->timestamp;
	return 0;
    default:
        break;
    }
    fprintf(stderr, "Tokudb recovery %s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)renv->ss.ss);
    abort();
}

static int toku_recover_fassociate (struct logtype_fassociate *l, RECOVER_ENV renv) {
    switch (renv->ss.ss) {
    case FORWARD_OLDER_CHECKPOINT_BEGIN:
        return 0;
    case FORWARD_BETWEEN_CHECKPOINT_BEGIN_END:
    case FORWARD_NEWER_CHECKPOINT_END: {
        struct file_map_tuple *tuple = NULL;
        int r = file_map_find(&renv->fmap, l->filenum, &tuple);
        if (r == 0) {
            // assert that the filenum maps to the correct iname
            char *fname = fixup_fname(&l->iname);
            assert(strcmp(fname, tuple->iname) == 0);
            toku_free(fname);
        }
        return 0;
    }
    default:
        assert(0);
        return 0;
    }
}

static int toku_recover_backward_fassociate (struct logtype_fassociate *l, RECOVER_ENV renv) {
    switch (renv->ss.ss) {
    case BACKWARD_OLDER_CHECKPOINT_BEGIN:
        return 0;
    case BACKWARD_BETWEEN_CHECKPOINT_BEGIN_END: {
        // assert that the filenum is not open
        struct file_map_tuple *tuple = NULL;
        int r = file_map_find(&renv->fmap, l->filenum, &tuple);
        assert(r != 0);

        // open it
        r = internal_toku_recover_fopen_or_fcreate(renv, FALSE, 0, &l->iname, l->filenum, l->treeflags, 0, NULL, 0, NULL);
        return r;
    }
    case BACKWARD_NEWER_CHECKPOINT_END:
        return 0;
    default:
        assert(0);
        return 0;
    }
}

static int toku_recover_xstillopen (struct logtype_xstillopen *UU(l), RECOVER_ENV UU(renv)) {
    switch (renv->ss.ss) {
    case FORWARD_OLDER_CHECKPOINT_BEGIN:
        return 0;
    case FORWARD_BETWEEN_CHECKPOINT_BEGIN_END:
    case FORWARD_NEWER_CHECKPOINT_END: {
        // assert that the transaction exists
        TOKUTXN txn = NULL;
        int r = toku_txnid2txn(renv->logger, l->txnid, &txn);
        assert(r == 0 && txn != NULL);
        return 0;
    }
    default:
        assert(0);
        return 0;
    }
}

static int toku_recover_backward_xstillopen (struct logtype_xstillopen *l, RECOVER_ENV renv) {
    switch (renv->ss.ss) {
    case BACKWARD_OLDER_CHECKPOINT_BEGIN:
	return 0; // ignore live txns from older checkpoints
    case BACKWARD_BETWEEN_CHECKPOINT_BEGIN_END:
        assert(l->txnid < renv->ss.checkpoint_lsn.lsn);
	if (renv->ss.n_live_txns == 0)
	    renv->ss.oldest_txnid = l->txnid;
	else if (toku_txnid_older(l->txnid, renv->ss.oldest_txnid))  
            renv->ss.oldest_txnid = l->txnid;
	renv->ss.n_live_txns++;
	return 0;
    case BACKWARD_NEWER_CHECKPOINT_END:
	return 0; // ignore live txns from incomplete checkpoint
    default:
        break;
    }
    fprintf(stderr, "Tokudb recovery %s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)renv->ss.ss);
    abort();
}

static int toku_recover_xbegin (struct logtype_xbegin *l, RECOVER_ENV renv) {
    int r;

    // lookup the parent
    TOKUTXN parent = NULL;
    if (l->parenttxnid != 0) {
        r = toku_txnid2txn(renv->logger, l->parenttxnid, &parent);
        assert(r == 0);
        if (parent == NULL) {
            //This is a straddle txn.
            assert(renv->ss.ss == FORWARD_OLDER_CHECKPOINT_BEGIN); //cannot happen after checkpoint begin
            return 0;
        }
    }

    // create a transaction and bind it to the transaction id
    TOKUTXN txn = NULL;
    r = toku_txn_begin_with_xid(parent, &txn, renv->logger, l->lsn.lsn);
    assert(r == 0);
    return 0;
}

static int toku_recover_backward_xbegin (struct logtype_xbegin *l, RECOVER_ENV renv) {
    struct scan_state *ss = &renv->ss;
    switch (ss->ss) {
    case BACKWARD_OLDER_CHECKPOINT_BEGIN:
	assert(ss->n_live_txns > 0); // the only thing we are doing here is looking for a live txn, so there better be one
        assert(ss->oldest_txnid <= l->lsn.lsn); //Did not pass it.
	if (ss->oldest_txnid == l->lsn.lsn) {
            renv->goforward = TRUE;
            renv->ss.ss = FORWARD_OLDER_CHECKPOINT_BEGIN;
            time_t tnow = time(NULL);
            fprintf(stderr, "%.24s Tokudb recovery turning around at xbegin %" PRIu64 " live txns=%d (%s)\n", ctime(&tnow), l->lsn.lsn, renv->ss.n_live_txns, recover_state(renv));
	} else {
	    if (tokudb_recovery_trace)
                fprintf(stderr, "Tokudb recovery scanning back at xbegin %" PRIu64 " looking for %" PRIu64 " (%s)\n", l->lsn.lsn, ss->oldest_txnid, recover_state(renv));
	}
        return 0;
    case BACKWARD_BETWEEN_CHECKPOINT_BEGIN_END:
        assert(l->lsn.lsn > renv->ss.checkpoint_lsn.lsn);
        return 0; // ignore txns that began during the checkpoint
    case BACKWARD_NEWER_CHECKPOINT_END:
        return 0; // ignore txns that began after checkpoint
    default:
        break;
    }
    fprintf(stderr, "Tokudb recovery %s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)ss->ss);
    abort();
}

static int toku_recover_commit (struct logtype_commit *l, RECOVER_ENV renv) {
    int r;

    // find the transaction by transaction id
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->txnid, &txn);
    assert(r == 0);
    if (txn == NULL) {
        //This is a straddle txn.
        assert(renv->ss.ss == FORWARD_OLDER_CHECKPOINT_BEGIN); // cannot happen after checkpoint begin
        return 0;
    }

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
    if (txn == NULL) {
        //This is a straddle txn.
        assert(renv->ss.ss == FORWARD_OLDER_CHECKPOINT_BEGIN); // cannot happen after checkpoint begin
        return 0;
    }

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

static int toku_recover_comment (struct logtype_comment *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_backward_comment (struct logtype_comment *UU(l), RECOVER_ENV UU(renv)) {
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
	char rolltmp_prefix[] = "__tokudb_rolltmp.";
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
    if (le->cmd == LT_comment) {
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

static uint32_t recover_get_num_live_txns(RECOVER_ENV renv) {
    return toku_omt_size(renv->logger->live_txns);
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

static void recover_trace_le(const char *f, int l, int r, struct log_entry *le) {
    if (le) {
        LSN thislsn = toku_log_entry_get_lsn(le);
        fprintf(stderr, "%s:%d r=%d cmd=%c lsn=%"PRIu64"\n", f, l, r, le->cmd, thislsn.lsn);
    } else
        fprintf(stderr, "%s:%d r=%d cmd=?\n", f, l, r);
}

// For test purposes only.
static void (*recover_callback_fx)(void*)  = NULL;
static void * recover_callback_args        = NULL;
static void (*recover_callback2_fx)(void*) = NULL;
static void * recover_callback2_args       = NULL;


static int do_recovery(RECOVER_ENV renv, const char *env_dir, const char *log_dir) {
    int r;
    int rr = 0;
    TOKULOGCURSOR logcursor = NULL;
    struct log_entry *le = NULL;
    
    time_t tnow = time(NULL);
    fprintf(stderr, "%.24s Tokudb recovery starting\n", ctime(&tnow));

    char org_wd[1000];
    {
	char *wd=getcwd(org_wd, sizeof(org_wd));
	assert(wd!=0);
    }

    r = toku_logger_open(log_dir, renv->logger);
    assert(r == 0);

    // grab the last LSN so that it can be restored when the log is restarted
    LSN lastlsn = toku_logger_last_lsn(renv->logger);
    LSN thislsn;

    // there must be at least one log entry
    r = toku_logcursor_create(&logcursor, log_dir);
    assert(r == 0);
    
    r = toku_logcursor_last(logcursor, &le);
    if (r != 0) {
        if (tokudb_recovery_trace) 
            fprintf(stderr, "RUNRECOVERY: %s:%d r=%d\n", __FUNCTION__, __LINE__, r);
        rr = DB_RUNRECOVERY; goto errorexit;
    }

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
    scan_state_init(&renv->ss);
    tnow = time(NULL);
    time_t tlast = tnow;
    fprintf(stderr, "%.24s Tokudb recovery scanning backward from %"PRIu64"\n", ctime(&tnow), lastlsn.lsn);
    for (unsigned i=0; 1; i++) {

        // get the previous log entry (first time gets the last one)
        le = NULL;
        r = toku_logcursor_prev(logcursor, &le);
        if (tokudb_recovery_trace) 
            recover_trace_le(__FUNCTION__, __LINE__, r, le);
        if (r != 0) {
            if (r == DB_RUNRECOVERY) {
                rr = DB_RUNRECOVERY; goto errorexit;
            }
            break;
        }

        // trace progress
        if ((i % 1000) == 0) {
            tnow = time(NULL);
            if (tnow - tlast >= TOKUDB_RECOVERY_PROGRESS_TIME) {
                thislsn = toku_log_entry_get_lsn(le);
                fprintf(stderr, "%.24s Tokudb recovery scanning backward from %"PRIu64" at %"PRIu64" (%s)\n", ctime(&tnow), lastlsn.lsn, thislsn.lsn, recover_state(renv));
                tlast = tnow;
            }
        }

        // dispatch the log entry handler
        assert(renv->ss.ss == BACKWARD_OLDER_CHECKPOINT_BEGIN ||
               renv->ss.ss == BACKWARD_BETWEEN_CHECKPOINT_BEGIN_END ||
               renv->ss.ss == BACKWARD_NEWER_CHECKPOINT_END);
        logtype_dispatch_assign(le, toku_recover_backward_, r, renv);
        if (tokudb_recovery_trace) 
            recover_trace_le(__FUNCTION__, __LINE__, r, le);
        if (r != 0) {
            if (tokudb_recovery_trace) 
                fprintf(stderr, "DB_RUNRECOVERY: %s:%d r=%d\n", __FUNCTION__, __LINE__, r);
            rr = DB_RUNRECOVERY; goto errorexit;
        }
        if (renv->goforward)
            break;
    }

    // run first callback
    if (recover_callback_fx) 
        recover_callback_fx(recover_callback_args);

    // scan forwards
    assert(le);
    thislsn = toku_log_entry_get_lsn(le);
    tnow = time(NULL);
    fprintf(stderr, "%.24s Tokudb recovery starts scanning forward to %"PRIu64" from %"PRIu64" left %"PRIu64" (%s)\n", ctime(&tnow), lastlsn.lsn, thislsn.lsn, lastlsn.lsn - thislsn.lsn, recover_state(renv));

    for (unsigned i=0; 1; i++) {

        // trace progress
        if ((i % 1000) == 0) {
            tnow = time(NULL);
            if (tnow - tlast >= TOKUDB_RECOVERY_PROGRESS_TIME) {
                thislsn = toku_log_entry_get_lsn(le);
                fprintf(stderr, "%.24s Tokudb recovery scanning forward to %"PRIu64" at %"PRIu64" left %"PRIu64" (%s)\n", ctime(&tnow), lastlsn.lsn, thislsn.lsn, lastlsn.lsn - thislsn.lsn, recover_state(renv));
                tlast = tnow;
            }
        }

        // dispatch the log entry handler (first time calls the forward handler for the log entry at the turnaround
        assert(renv->ss.ss == FORWARD_OLDER_CHECKPOINT_BEGIN ||
               renv->ss.ss == FORWARD_BETWEEN_CHECKPOINT_BEGIN_END ||
               renv->ss.ss == FORWARD_NEWER_CHECKPOINT_END);
        logtype_dispatch_assign(le, toku_recover_, r, renv);
        if (tokudb_recovery_trace) 
            recover_trace_le(__FUNCTION__, __LINE__, r, le);
        if (r != 0) {
            if (tokudb_recovery_trace) 
                fprintf(stderr, "DB_RUNRECOVERY: %s:%d r=%d\n", __FUNCTION__, __LINE__, r);
            rr = DB_RUNRECOVERY; goto errorexit;
        }

        // get the next log entry
        le = NULL;
        r = toku_logcursor_next(logcursor, &le);
        if (tokudb_recovery_trace) 
            recover_trace_le(__FUNCTION__, __LINE__, r, le);
        if (r != 0) {
            if (r == DB_RUNRECOVERY) {
                rr = DB_RUNRECOVERY; goto errorexit;
            }
            break;
        }        
    }

    // verify the final recovery state
    assert(renv->ss.ss == FORWARD_NEWER_CHECKPOINT_END);   

    r = toku_logcursor_destroy(&logcursor);
    assert(r == 0);

    // run second callback
    if (recover_callback2_fx) 
        recover_callback2_fx(recover_callback2_args);

    // restart logging
    toku_logger_restart(renv->logger, lastlsn);

    // abort the live transactions
    uint32_t n = recover_get_num_live_txns(renv);
    if (n > 0) {
        tnow = time(NULL);
        fprintf(stderr, "%.24s Tokudb recovery aborting %"PRIu32" live transaction%s\n", ctime(&tnow), n, n > 1 ? "s" : "");
    }
    recover_abort_live_txns(renv);

    // close the open dictionaries
    n = file_map_get_num_dictionaries(&renv->fmap);
    if (n > 0) {
        tnow = time(NULL);
        fprintf(stderr, "%.24s Tokudb recovery closing %"PRIu32" dictionar%s\n", ctime(&tnow), n, n > 1 ? "ies" : "y");
    }
    file_map_close_dictionaries(&renv->fmap, TRUE);

    // write a recovery log entry
    BYTESTRING recover_comment = { strlen("recover"), "recover" };
    r = toku_log_comment(renv->logger, NULL, TRUE, 0, recover_comment);
    assert(r == 0);

    // checkpoint 
    tnow = time(NULL);
    fprintf(stderr, "%.24s Tokudb recovery making a checkpoint\n", ctime(&tnow));
    r = toku_checkpoint(renv->ct, renv->logger, NULL, NULL, NULL, NULL);
    assert(r == 0);
    tnow = time(NULL);
    fprintf(stderr, "%.24s Tokudb recovery done\n", ctime(&tnow));

    r = chdir(org_wd); 
    assert(r == 0);

    return 0;

 errorexit:
    tnow = time(NULL);
    fprintf(stderr, "%.24s Tokudb recovery failed %d\n", ctime(&tnow), rr);

    if (logcursor) {
        r = toku_logcursor_destroy(&logcursor);
        assert(r == 0);
    }

    r = chdir(org_wd); 
    assert(r == 0);

    return rr;
}

static int recover_lock(const char *lock_dir, int *lockfd) {
    if (!lock_dir)
        return ENOENT;

    const char fname[] = "/__tokudb_recoverylock_dont_delete_me";
    int namelen=strlen(lock_dir);
    char lockfname[namelen+sizeof(fname)];

    int l = snprintf(lockfname, sizeof(lockfname), "%s%s", lock_dir, fname);
    assert(l+1 == (signed)(sizeof(lockfname)));
    *lockfd = toku_os_lock_file(lockfname);
    if (*lockfd < 0) {
        int e = errno;
        fprintf(stderr, "Couldn't run recovery because some other process holds the recovery lock %s\n", lockfname);
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

int tokudb_recover(const char *env_dir, const char *log_dir,
                   brt_compare_func bt_compare,
                   brt_compare_func dup_compare,
                   generate_keys_vals_for_put_func generate_keys_vals_for_put,
                   cleanup_keys_vals_for_put_func  cleanup_keys_vals_for_put,
                   generate_keys_for_del_func      generate_keys_for_del,
                   cleanup_keys_for_del_func       cleanup_keys_for_del,
                   size_t cachetable_size) {
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
        r = recover_env_init(&renv, bt_compare, dup_compare,
                             generate_keys_vals_for_put,
                             cleanup_keys_vals_for_put,
                             generate_keys_for_del,
                             cleanup_keys_for_del,
                             cachetable_size);
        assert(r == 0);

        rr = do_recovery(&renv, env_dir, log_dir);

        recover_env_cleanup(&renv, (BOOL)(rr == 0));
    }

    r = recover_unlock(lockfd);
    if (r != 0)
        return r;

    return rr;
}

// Return 0 if recovery log exists, ENOENT if log is missing
int 
tokudb_recover_log_exists(const char * log_dir) {
    int r;
    TOKULOGCURSOR logcursor;

    r = toku_logcursor_create(&logcursor, log_dir);
    if (r == 0) {
	int rclose;
	r = toku_logcursor_log_exists(logcursor);  // return ENOENT if no log
	rclose = toku_logcursor_destroy(&logcursor);
	assert(rclose == 0);
    }
    else
	r = ENOENT;
    
    return r;
}

void toku_recover_set_callback (void (*callback_fx)(void*), void* callback_args) {
    recover_callback_fx   = callback_fx;
    recover_callback_args = callback_args;
}

void toku_recover_set_callback2 (void (*callback_fx)(void*), void* callback_args) {
    recover_callback2_fx   = callback_fx;
    recover_callback2_args = callback_args;
}
