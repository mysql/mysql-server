/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"
#include <ft/log_header.h>
#include "checkpoint.h"
#include "txn_manager.h"

static const char recovery_lock_file[] = "/__tokudb_recoverylock_dont_delete_me";

int tokudb_recovery_trace = 0;                    // turn on recovery tracing, default off.

//#define DO_VERIFY_COUNTS
#ifdef DO_VERIFY_COUNTS
#define VERIFY_COUNTS(n) toku_verify_or_set_counts(n, FALSE)
#else
#define VERIFY_COUNTS(n) ((void)0)
#endif

// time in seconds between recovery progress reports
#define TOKUDB_RECOVERY_PROGRESS_TIME 15

struct scan_state {
    enum {
        BACKWARD_NEWER_CHECKPOINT_END = 1,
        BACKWARD_BETWEEN_CHECKPOINT_BEGIN_END,
        FORWARD_BETWEEN_CHECKPOINT_BEGIN_END,
        FORWARD_NEWER_CHECKPOINT_END,
    } ss;
    LSN checkpoint_begin_lsn;
    LSN checkpoint_end_lsn;
    uint64_t checkpoint_end_timestamp;
    uint64_t checkpoint_begin_timestamp;
    uint32_t checkpoint_num_fassociate;
    uint32_t checkpoint_num_xstillopen;
    TXNID last_xid;
};

static const char *scan_state_strings[] = {
    "?", "bw_newer", "bw_between", "fw_between", "fw_newer",
};

static void scan_state_init(struct scan_state *ss) {
    ss->ss = BACKWARD_NEWER_CHECKPOINT_END;
    ss->checkpoint_begin_lsn = ZERO_LSN;
    ss->checkpoint_end_lsn = ZERO_LSN;
    ss->checkpoint_num_fassociate = 0;
    ss->checkpoint_num_xstillopen = 0;
    ss->last_xid = 0;
}

static const char *scan_state_string(struct scan_state *ss) {
    assert(BACKWARD_NEWER_CHECKPOINT_END <= ss->ss && ss->ss <= FORWARD_NEWER_CHECKPOINT_END);
    return scan_state_strings[ss->ss];
}

// File map tuple
struct file_map_tuple {
    FILENUM filenum;
    FT_HANDLE ft_handle;     // NULL ft_handle means it's a rollback file.
    char *iname;
    struct __toku_db fake_db;
};

static void file_map_tuple_init(struct file_map_tuple *tuple, FILENUM filenum, FT_HANDLE brt, char *iname) {
    tuple->filenum = filenum;
    tuple->ft_handle = brt;
    tuple->iname = iname;
    // use a fake DB for comparisons, using the ft's cmp descriptor
    memset(&tuple->fake_db, 0, sizeof(tuple->fake_db));
    tuple->fake_db.cmp_descriptor = &tuple->ft_handle->ft->cmp_descriptor;
    tuple->fake_db.descriptor = &tuple->ft_handle->ft->descriptor;
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

// The recovery environment
struct recover_env {
    DB_ENV *env;
    prepared_txn_callback_t    prepared_txn_callback;    // at the end of recovery, all the prepared txns are passed back to the ydb layer to make them into valid transactions.
    keep_cachetable_callback_t keep_cachetable_callback; // after recovery, store the cachetable into the environment.
    CACHETABLE ct;
    TOKULOGGER logger;
    ft_compare_func bt_compare;
    ft_update_func update_function;
    generate_row_for_put_func generate_row_for_put;
    generate_row_for_del_func generate_row_for_del;
    struct scan_state ss;
    struct file_map fmap;
    BOOL goforward;
    bool destroy_logger_at_end; // If true then destroy the logger when we are done.  If false then set the logger into write-files mode when we are done with recovery.*/
};
typedef struct recover_env *RECOVER_ENV;


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

static void file_map_close_dictionaries(struct file_map *fmap, BOOL recovery_succeeded, LSN oplsn) {
    int r;

    while (1) {
        u_int32_t n = toku_omt_size(fmap->filenums);
        if (n == 0)
            break;
        OMTVALUE v;
        r = toku_omt_fetch(fmap->filenums, n-1, &v);
        assert(r == 0);
        r = toku_omt_delete_at(fmap->filenums, n-1);
        assert(r == 0);
        struct file_map_tuple *tuple = v;
        assert(tuple->ft_handle);
        if (!recovery_succeeded) {
            // don't update the brt on close
            r = toku_ft_handle_set_panic(tuple->ft_handle, DB_RUNRECOVERY, "recovery failed");
            assert(r==0);
        }
        // Logging is on again, but we must pass the right LSN into close.
        if (tuple->ft_handle) { // it's a DB, not a rollback file
            toku_ft_handle_close_recovery(tuple->ft_handle, oplsn);
        } else {
            assert(tuple->ft_handle==NULL);
        }
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

static int file_map_insert (struct file_map *fmap, FILENUM fnum, FT_HANDLE brt, char *iname) {
    struct file_map_tuple *tuple = toku_malloc(sizeof (struct file_map_tuple));
    assert(tuple);
    file_map_tuple_init(tuple, fnum, brt, iname);
    int r = toku_omt_insert(fmap->filenums, tuple, file_map_h, &fnum, NULL);
    return r;
}

static void file_map_remove(struct file_map *fmap, FILENUM fnum) {
    OMTVALUE v; u_int32_t idx;
    int r = toku_omt_find_zero(fmap->filenums, file_map_h, &fnum, &v, &idx);
    if (r == 0) {
        struct file_map_tuple *tuple = v;
        r = toku_omt_delete_at(fmap->filenums, idx);
        file_map_tuple_destroy(tuple);
        toku_free(tuple);
    }
}

// Look up file info: given FILENUM, return file_map_tuple (or DB_NOTFOUND)
static int file_map_find(struct file_map *fmap, FILENUM fnum, struct file_map_tuple **file_map_tuple) {
    OMTVALUE v; u_int32_t idx;
    int r = toku_omt_find_zero(fmap->filenums, file_map_h, &fnum, &v, &idx);
    if (r == 0) {
        struct file_map_tuple *tuple = v;
        assert(tuple->filenum.fileid == fnum.fileid);
        *file_map_tuple = tuple;
    }
    else assert(r==DB_NOTFOUND);
    return r;
}

static int recover_env_init (RECOVER_ENV renv,
                             const char *env_dir,
                             DB_ENV *env,
                             prepared_txn_callback_t    prepared_txn_callback,
                             keep_cachetable_callback_t keep_cachetable_callback,
                             TOKULOGGER logger,
                             ft_compare_func bt_compare,
                             ft_update_func update_function,
                             generate_row_for_put_func generate_row_for_put,
                             generate_row_for_del_func generate_row_for_del,
                             size_t cachetable_size) {
    int r;

    // If we are passed a logger use it, otherwise create one.
    renv->destroy_logger_at_end = logger==NULL;
    if (logger) {
        renv->logger = logger;
    } else {
        r = toku_logger_create(&renv->logger);
        assert(r == 0);
    }
    toku_logger_write_log_files(renv->logger, FALSE);
    r = toku_create_cachetable(&renv->ct, cachetable_size ? cachetable_size : 1<<25, (LSN){0}, renv->logger);
    assert(r == 0);
    toku_cachetable_set_env_dir(renv->ct, env_dir);
    if (keep_cachetable_callback) keep_cachetable_callback(env, renv->ct);
    toku_logger_set_cachetable(renv->logger, renv->ct);
    renv->env                      = env;
    renv->prepared_txn_callback    = prepared_txn_callback;
    renv->keep_cachetable_callback = keep_cachetable_callback;
    renv->bt_compare               = bt_compare;
    renv->update_function          = update_function;
    renv->generate_row_for_put     = generate_row_for_put;
    renv->generate_row_for_del     = generate_row_for_del;
    file_map_init(&renv->fmap);
    renv->goforward = FALSE;

    if (tokudb_recovery_trace)
        fprintf(stderr, "%s:%d\n", __FUNCTION__, __LINE__);
    return r;
}

static void recover_env_cleanup (RECOVER_ENV renv, bool recovery_succeeded) {
    int r;

    assert(toku_omt_size(renv->fmap.filenums)==0);
    //file_map_close_dictionaries(renv, &renv->fmap, recovery_succeeded, oplsn);
    file_map_destroy(&renv->fmap);

    if (renv->destroy_logger_at_end) {
        r = toku_logger_close_rollback(renv->logger, !recovery_succeeded);
        assert(r==0);
        r = toku_logger_close(&renv->logger);
        assert(r == 0);
    } else {
        toku_logger_write_log_files(renv->logger, true);
    }

    if (renv->keep_cachetable_callback) {
        renv->ct = NULL;
    } else {
        r = toku_cachetable_close(&renv->ct);
        assert(r == 0);
    }

    if (tokudb_recovery_trace)
        fprintf(stderr, "%s:%d\n", __FUNCTION__, __LINE__);
}

static const char *recover_state(RECOVER_ENV renv) {
    return scan_state_string(&renv->ss);
}

// Open the file if it is not already open.  If it is already open, then do nothing.
static int internal_recover_fopen_or_fcreate (RECOVER_ENV renv, BOOL must_create, int UU(mode), BYTESTRING *bs_iname, FILENUM filenum, u_int32_t treeflags,
                                              TOKUTXN txn, uint32_t nodesize, uint32_t basementnodesize, enum toku_compression_method compression_method, LSN max_acceptable_lsn) {
    int r;
    FT_HANDLE brt = NULL;
    char *iname = fixup_fname(bs_iname);

    r = toku_ft_handle_create(&brt);
    assert(r == 0);

    r = toku_ft_set_flags(brt, treeflags);
    assert(r == 0);

    if (nodesize != 0) {
        toku_ft_handle_set_nodesize(brt, nodesize);
    }

    if (basementnodesize != 0) {
        toku_ft_handle_set_basementnodesize(brt, basementnodesize);
    }

    if (compression_method != TOKU_DEFAULT_COMPRESSION_METHOD) {
        toku_ft_handle_set_compression_method(brt, compression_method);
    }

    // set the key compare functions
    if (!(treeflags & TOKU_DB_KEYCMP_BUILTIN) && renv->bt_compare) {
        r = toku_ft_set_bt_compare(brt, renv->bt_compare);
        assert(r == 0);
    }

    if (renv->update_function) {
        r = toku_ft_set_update(brt, renv->update_function);
        assert(r == 0);
    }

    // TODO mode (FUTURE FEATURE)
    //mode = mode;

    r = toku_ft_handle_open_recovery(brt, iname, must_create, must_create, renv->ct, txn, filenum, max_acceptable_lsn);
    if (r != 0) {
        //Note:  If ft_handle_open fails, then close_ft will NOT write a header to disk.
        //No need to provide lsn, so use the regular toku_ft_handle_close function
        toku_ft_handle_close(brt);
        toku_free(iname);
        if (r == ENOENT) //Not an error to simply be missing.
            r = 0;
        return r;
    }

    file_map_insert(&renv->fmap, filenum, brt, iname);
    return 0;
}

static int toku_recover_begin_checkpoint (struct logtype_begin_checkpoint *l, RECOVER_ENV renv) {
    int r;
    TXN_MANAGER mgr = toku_logger_get_txn_manager(renv->logger);
    switch (renv->ss.ss) {
    case FORWARD_BETWEEN_CHECKPOINT_BEGIN_END:
        assert(l->lsn.lsn == renv->ss.checkpoint_begin_lsn.lsn);
        invariant(renv->ss.last_xid == TXNID_NONE);
        renv->ss.last_xid = l->last_xid;
        toku_txn_manager_set_last_xid_from_recovered_checkpoint(mgr, l->last_xid);

        r = 0;
        break;
    case FORWARD_NEWER_CHECKPOINT_END:
        assert(l->lsn.lsn > renv->ss.checkpoint_end_lsn.lsn);
        // Verify last_xid is no older than the previous begin
        invariant(l->last_xid >= renv->ss.last_xid);
        // Verify last_xid is no older than the newest txn
        invariant(l->last_xid >= toku_txn_manager_get_last_xid(mgr));

        r = 0; // ignore it (log only has a begin checkpoint)
        break;
    default:
        fprintf(stderr, "Tokudb recovery %s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)renv->ss.ss);
        abort();
        break;
    }
    return r;
}

static int toku_recover_backward_begin_checkpoint (struct logtype_begin_checkpoint *l, RECOVER_ENV renv) {
    int r;
    time_t tnow = time(NULL);
    fprintf(stderr, "%.24s Tokudb recovery bw_begin_checkpoint at %"PRIu64" timestamp %"PRIu64" (%s)\n", ctime(&tnow), l->lsn.lsn, l->timestamp, recover_state(renv));
    switch (renv->ss.ss) {
    case BACKWARD_NEWER_CHECKPOINT_END:
        // incomplete checkpoint, nothing to do
        r = 0;
        break;
    case BACKWARD_BETWEEN_CHECKPOINT_BEGIN_END:
        assert(l->lsn.lsn == renv->ss.checkpoint_begin_lsn.lsn);
        renv->ss.ss = FORWARD_BETWEEN_CHECKPOINT_BEGIN_END;
        renv->ss.checkpoint_begin_timestamp = l->timestamp;
        renv->goforward = TRUE;
        tnow = time(NULL);
        fprintf(stderr, "%.24s Tokudb recovery turning around at begin checkpoint %"PRIu64" time %"PRIu64"\n", 
                ctime(&tnow), l->lsn.lsn, 
                renv->ss.checkpoint_end_timestamp - renv->ss.checkpoint_begin_timestamp);
        r = 0;
        break;
    default:
        fprintf(stderr, "Tokudb recovery %s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)renv->ss.ss);
        abort();
        break;
    }
    return r;
}

static int toku_recover_end_checkpoint (struct logtype_end_checkpoint *l, RECOVER_ENV renv) {
    int r;
    switch (renv->ss.ss) {
    case FORWARD_BETWEEN_CHECKPOINT_BEGIN_END:
        assert(l->lsn_begin_checkpoint.lsn == renv->ss.checkpoint_begin_lsn.lsn);
        assert(l->lsn.lsn == renv->ss.checkpoint_end_lsn.lsn);
        assert(l->num_fassociate_entries == renv->ss.checkpoint_num_fassociate);
        assert(l->num_xstillopen_entries == renv->ss.checkpoint_num_xstillopen);
        renv->ss.ss = FORWARD_NEWER_CHECKPOINT_END;
        r = 0;
        break;
    case FORWARD_NEWER_CHECKPOINT_END:
        assert(0);
        return 0;
    default:
        assert(0);
        return 0;
    }
    return r;
}

static int toku_recover_backward_end_checkpoint (struct logtype_end_checkpoint *l, RECOVER_ENV renv) {
    time_t tnow = time(NULL);
    fprintf(stderr, "%.24s Tokudb recovery bw_end_checkpoint at %"PRIu64" timestamp %"PRIu64" xid %"PRIu64" (%s)\n", ctime(&tnow), l->lsn.lsn, l->timestamp, l->lsn_begin_checkpoint.lsn, recover_state(renv));
    switch (renv->ss.ss) {
    case BACKWARD_NEWER_CHECKPOINT_END:
        renv->ss.ss = BACKWARD_BETWEEN_CHECKPOINT_BEGIN_END;
        renv->ss.checkpoint_begin_lsn.lsn = l->lsn_begin_checkpoint.lsn;
        renv->ss.checkpoint_end_lsn.lsn   = l->lsn.lsn;
        renv->ss.checkpoint_end_timestamp = l->timestamp;
        return 0;
    case BACKWARD_BETWEEN_CHECKPOINT_BEGIN_END:
        fprintf(stderr, "Tokudb recovery %s:%d Should not see two end_checkpoint log entries without an intervening begin_checkpoint\n", __FILE__, __LINE__);
        abort();
    default:
        break;
    }
    fprintf(stderr, "Tokudb recovery %s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)renv->ss.ss);
    abort();
}

static int toku_recover_fassociate (struct logtype_fassociate *l, RECOVER_ENV renv) {
    struct file_map_tuple *tuple = NULL;
    int r = file_map_find(&renv->fmap, l->filenum, &tuple);
    char *fname = fixup_fname(&l->iname);
    switch (renv->ss.ss) {
    case FORWARD_BETWEEN_CHECKPOINT_BEGIN_END:
        renv->ss.checkpoint_num_fassociate++;
        assert(r==DB_NOTFOUND); //Not open
        // Open it if it exists.
        // If rollback file, specify which checkpointed version of file we need (not just the latest)
        // because we cannot use a rollback log that is later than the last complete checkpoint.  See #3113.
        {
            BOOL rollback_file = (0==strcmp(fname, ROLLBACK_CACHEFILE_NAME));
            LSN max_acceptable_lsn = MAX_LSN;
            if (rollback_file) {
                max_acceptable_lsn = renv->ss.checkpoint_begin_lsn;
                FT_HANDLE t;
                r = toku_ft_handle_create(&t);
                assert(r==0);
                r = toku_ft_handle_open_recovery(t, ROLLBACK_CACHEFILE_NAME, false, false, renv->ct, (TOKUTXN)NULL, l->filenum, max_acceptable_lsn);
                renv->logger->rollback_cachefile = t->ft->cf;
            } else {
                r = internal_recover_fopen_or_fcreate(renv, FALSE, 0, &l->iname, l->filenum, l->treeflags, NULL, 0, 0, TOKU_DEFAULT_COMPRESSION_METHOD, max_acceptable_lsn);
                assert(r==0);
            }
        }
        // try to open the file again and if we get it, restore
        // the unlink on close bit.
        int ret = file_map_find(&renv->fmap, l->filenum, &tuple);
        if (ret == 0 && l->unlink_on_close) {
            toku_cachefile_unlink_on_close(tuple->ft_handle->ft->cf);
        }
        break;
    case FORWARD_NEWER_CHECKPOINT_END:
        if (r == 0) { //IF it is open
            // assert that the filenum maps to the correct iname
            assert(strcmp(fname, tuple->iname) == 0);
        }
        r = 0;
        break;
    default:
        assert(0);
        return 0;
    }
    toku_free(fname);

    return r;
}

static int toku_recover_backward_fassociate (struct logtype_fassociate *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int
recover_transaction(TOKUTXN *txnp, TXNID xid, TXNID parentxid, TOKULOGGER logger) {
    int r;

    // lookup the parent
    TOKUTXN parent = NULL;
    if (parentxid != TXNID_NONE) {
        r = toku_txnid2txn(logger, parentxid, &parent);
        assert(r == 0);
        assert(parent!=NULL);
    }

    // create a transaction and bind it to the transaction id
    TOKUTXN txn = NULL;
    {
        //Verify it does not yet exist.
        r = toku_txnid2txn(logger, xid, &txn);
        assert(r == 0);
        assert(txn==NULL);
    }
    r = toku_txn_begin_with_xid(parent, &txn, logger, xid, TXN_SNAPSHOT_NONE, NULL, true);
    assert(r == 0);
    // We only know about it because it was logged.  Restore the log bit.
    // Logging is 'off' but it will still set the bit.
    toku_maybe_log_begin_txn_for_write_operation(txn);
    if (txnp) *txnp = txn;
    return 0;
}

static int recover_xstillopen_internal (TOKUTXN         *txnp,
                                        LSN           UU(lsn),
                                        TXNID            xid,
                                        TXNID            parentxid,
                                        u_int64_t        rollentry_raw_count,
                                        FILENUMS         open_filenums,
                                        u_int8_t         force_fsync_on_commit,
                                        u_int64_t        num_rollback_nodes,
                                        u_int64_t        num_rollentries,
                                        BLOCKNUM         spilled_rollback_head,
                                        BLOCKNUM         spilled_rollback_tail,
                                        BLOCKNUM         current_rollback,
                                        u_int32_t     UU(crc),
                                        u_int32_t     UU(len),
                                        RECOVER_ENV      renv) {
    int r;
    *txnp = NULL;
    switch (renv->ss.ss) {
    case FORWARD_BETWEEN_CHECKPOINT_BEGIN_END: {
        renv->ss.checkpoint_num_xstillopen++;
        invariant(renv->ss.last_xid != TXNID_NONE);
        invariant(xid <= renv->ss.last_xid);
        TOKUTXN txn = NULL;
        { //Create the transaction.
            r = recover_transaction(&txn, xid, parentxid, renv->logger);
            assert(r==0);
            assert(txn!=NULL);
            *txnp = txn;
        }
        { //Recover rest of transaction.
#define COPY_TO_INFO(field) .field = field
            struct txninfo info = {
                COPY_TO_INFO(rollentry_raw_count),
                .num_fts  = 0,    //Set afterwards
                .open_fts = NULL, //Set afterwards
                COPY_TO_INFO(force_fsync_on_commit),
                COPY_TO_INFO(num_rollback_nodes),
                COPY_TO_INFO(num_rollentries),
                COPY_TO_INFO(spilled_rollback_head),
                COPY_TO_INFO(spilled_rollback_tail),
                COPY_TO_INFO(current_rollback)
            };
#undef COPY_TO_INFO
            //Generate open_fts
            FT array[open_filenums.num]; //Allocate maximum possible requirement
            info.open_fts = array;
            uint32_t i;
            for (i = 0; i < open_filenums.num; i++) {
                //open_filenums.filenums[]
                struct file_map_tuple *tuple = NULL;
                r = file_map_find(&renv->fmap, open_filenums.filenums[i], &tuple);
                if (r==0) {
                    info.open_fts[info.num_fts++] = tuple->ft_handle->ft;
                }
                else {
                    assert(r==DB_NOTFOUND);
                }
            }
            r = toku_txn_load_txninfo(txn, &info);
            assert(r==0);
        }
        break;
    }
    case FORWARD_NEWER_CHECKPOINT_END: {
        // assert that the transaction exists
        TOKUTXN txn = NULL;
        r = toku_txnid2txn(renv->logger, xid, &txn);
        assert(r == 0 && txn != NULL);
        r = 0;
        *txnp = txn;
        break;
    }
    default:
        assert(0);
        return 0;
    }
    return r;
}

static int toku_recover_xstillopen (struct logtype_xstillopen *l, RECOVER_ENV renv) {
    TOKUTXN txn;
    return recover_xstillopen_internal (&txn,
                                        l->lsn,
                                        l->xid,
                                        l->parentxid,
                                        l->rollentry_raw_count,
                                        l->open_filenums,
                                        l->force_fsync_on_commit,
                                        l->num_rollback_nodes,
                                        l->num_rollentries,
                                        l->spilled_rollback_head,
                                        l->spilled_rollback_tail,
                                        l->current_rollback,
                                        l->crc,
                                        l->len,
                                        renv);
}

static int toku_recover_xstillopenprepared (struct logtype_xstillopenprepared *l, RECOVER_ENV renv) {
    TOKUTXN txn;
    int r = recover_xstillopen_internal (&txn,
                                         l->lsn,
                                         l->xid,
                                         (TXNID)0,
                                         l->rollentry_raw_count,
                                         l->open_filenums,
                                         l->force_fsync_on_commit,
                                         l->num_rollback_nodes,
                                         l->num_rollentries,
                                         l->spilled_rollback_head,
                                         l->spilled_rollback_tail,
                                         l->current_rollback,
                                         l->crc,
                                         l->len,
                                         renv);
    if (r != 0) {
        goto exit;
    }
    switch (renv->ss.ss) {
        case FORWARD_BETWEEN_CHECKPOINT_BEGIN_END: {
            r = toku_txn_prepare_txn(txn, l->xa_xid);
            break;
        }
        case FORWARD_NEWER_CHECKPOINT_END: {
            assert(txn->state == TOKUTXN_PREPARING);
            break;
        }
        default: {
            assert(0);
        }
    }
exit:
    return r;
}

static int toku_recover_backward_xstillopen (struct logtype_xstillopen *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}
static int toku_recover_backward_xstillopenprepared (struct logtype_xstillopenprepared *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_suppress_rollback (struct logtype_suppress_rollback *UU(l), RECOVER_ENV UU(renv)) {
    struct file_map_tuple *tuple = NULL;
    int r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r==0) {
        //File is open
        TOKUTXN txn = NULL;
        r = toku_txnid2txn(renv->logger, l->xid, &txn);
        assert(r == 0);
        assert(txn!=NULL);
        FT ft = tuple->ft_handle->ft;
        toku_ft_suppress_rollbacks(ft, txn);
        toku_txn_maybe_note_ft(txn, ft);
    }
    return 0;
}

static int toku_recover_backward_suppress_rollback (struct logtype_suppress_rollback *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_xbegin (struct logtype_xbegin *l, RECOVER_ENV renv) {
    int r;
    r = recover_transaction(NULL, l->xid, l->parentxid, renv->logger);
    return r;
}

static int toku_recover_backward_xbegin (struct logtype_xbegin *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_xcommit (struct logtype_xcommit *l, RECOVER_ENV renv) {
    int r;

    // find the transaction by transaction id
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(r == 0);
    assert(txn!=NULL);

    // commit the transaction
    r = toku_txn_commit_with_lsn(txn, TRUE, l->lsn,
                                 NULL, NULL);
    assert(r == 0);

    // close the transaction
    toku_txn_close_txn(txn);

    return 0;
}

static int toku_recover_backward_xcommit (struct logtype_xcommit *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_xprepare (struct logtype_xprepare *l, RECOVER_ENV renv) {
    int r;

    // find the transaction by transaction id
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(r == 0);
    assert(txn!=NULL);

    // Save the transaction
    r = toku_txn_prepare_txn(txn, l->xa_xid);
    assert(r == 0);

    return 0;
}

static int toku_recover_backward_xprepare (struct logtype_xprepare *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}



static int toku_recover_xabort (struct logtype_xabort *l, RECOVER_ENV renv) {
    int r;

    // find the transaction by transaction id
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(r == 0);
    assert(txn!=NULL);

    // abort the transaction
    r = toku_txn_abort_with_lsn(txn, l->lsn, NULL, NULL);
    assert(r == 0);

    // close the transaction
    toku_txn_close_txn(txn);

    return 0;
}

static int toku_recover_backward_xabort (struct logtype_xabort *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

// fcreate is like fopen except that the file must be created.
static int toku_recover_fcreate (struct logtype_fcreate *l, RECOVER_ENV renv) {
    int r;

    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(r == 0);

    // assert that filenum is closed
    struct file_map_tuple *tuple = NULL;
    r = file_map_find(&renv->fmap, l->filenum, &tuple);
    assert(r==DB_NOTFOUND);

    assert(txn!=NULL);

    //unlink if it exists (recreate from scratch).
    char *iname = fixup_fname(&l->iname);
    char *iname_in_cwd = toku_cachetable_get_fname_in_cwd(renv->ct, iname);
    r = unlink(iname_in_cwd);
    if (r != 0 && errno != ENOENT) {
        fprintf(stderr, "Tokudb recovery %s:%d unlink %s %d\n", __FUNCTION__, __LINE__, iname, errno);
        toku_free(iname);
        return r;
    }
    assert(0!=strcmp(iname, ROLLBACK_CACHEFILE_NAME)); //Creation of rollback cachefile never gets logged.
    toku_free(iname_in_cwd);
    toku_free(iname);

    BOOL must_create = TRUE;
    r = internal_recover_fopen_or_fcreate(renv, must_create, l->mode, &l->iname, l->filenum, l->treeflags, txn, l->nodesize, l->basementnodesize, (enum toku_compression_method) l->compression_method, MAX_LSN);
    return r;
}

static int toku_recover_backward_fcreate (struct logtype_fcreate *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}



static int toku_recover_fopen (struct logtype_fopen *l, RECOVER_ENV renv) {
    int r;

    // assert that filenum is closed
    struct file_map_tuple *tuple = NULL;
    r = file_map_find(&renv->fmap, l->filenum, &tuple);
    assert(r==DB_NOTFOUND);

    BOOL must_create = FALSE;
    TOKUTXN txn = NULL;
    char *fname = fixup_fname(&l->iname);

    assert(0!=strcmp(fname, ROLLBACK_CACHEFILE_NAME)); //Rollback cachefile can be opened only via fassociate.
    r = internal_recover_fopen_or_fcreate(renv, must_create, 0, &l->iname, l->filenum, l->treeflags, txn, 0, 0, TOKU_DEFAULT_COMPRESSION_METHOD, MAX_LSN);

    toku_free(fname);
    return r;
}

static int toku_recover_backward_fopen (struct logtype_fopen *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_change_fdescriptor (struct logtype_change_fdescriptor *l, RECOVER_ENV renv) {
    int r;
    struct file_map_tuple *tuple = NULL;
    r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r==0) {
        TOKUTXN txn = NULL;
        //Maybe do the descriptor (lsn filter)
        r = toku_txnid2txn(renv->logger, l->xid, &txn);
        assert(r == 0);
        DBT old_descriptor, new_descriptor;
        toku_fill_dbt(
            &old_descriptor, 
            l->old_descriptor.data, 
            l->old_descriptor.len
            );
        toku_fill_dbt(
            &new_descriptor, 
            l->new_descriptor.data, 
            l->new_descriptor.len
            );
        r = toku_ft_change_descriptor(
            tuple->ft_handle, 
            &old_descriptor, 
            &new_descriptor, 
            FALSE, 
            txn,
            l->update_cmp_descriptor
            );
        assert(r==0);
    }    
    return 0;
}

static int toku_recover_backward_change_fdescriptor (struct logtype_change_fdescriptor *UU(l), RECOVER_ENV UU(renv)) {
    return 0;
}


// if file referred to in l is open, close it
static int toku_recover_fclose (struct logtype_fclose *l, RECOVER_ENV renv) {
    struct file_map_tuple *tuple = NULL;
    int r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r == 0) {  // if file is open
        char *iname = fixup_fname(&l->iname);
        assert(strcmp(tuple->iname, iname) == 0);  // verify that file_map has same iname as log entry

        if (0!=strcmp(iname, ROLLBACK_CACHEFILE_NAME)) {
            //Rollback cachefile is closed manually at end of recovery, not here
            toku_ft_handle_close_recovery(tuple->ft_handle, l->lsn);
        }
        file_map_remove(&renv->fmap, l->filenum);
        toku_free(iname);
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
    int r = toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(r == 0);
    assert(txn != NULL);

    // if the forward scan in recovery found this file and opened it, we
    // need to mark the txn to remove the ft on commit. if the file was
    // not found and not opened, we don't need to do anything - the ft
    // is already gone, so we're happy.
    struct file_map_tuple *tuple;
    r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r == 0) {
        r = toku_ft_unlink_on_commit(tuple->ft_handle, txn);
    }
    return 0;
}

static int toku_recover_backward_fdelete (struct logtype_fdelete *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_enq_insert (struct logtype_enq_insert *l, RECOVER_ENV renv) {
    int r;
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(r == 0);
    assert(txn!=NULL);
    struct file_map_tuple *tuple = NULL;
    r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r==0) {
        //Maybe do the insertion if we found the cachefile.
        DBT keydbt, valdbt;
        toku_fill_dbt(&keydbt, l->key.data, l->key.len);
        toku_fill_dbt(&valdbt, l->value.data, l->value.len);
        r = toku_ft_maybe_insert(tuple->ft_handle, &keydbt, &valdbt, txn, TRUE, l->lsn, FALSE, FT_INSERT);
        assert(r == 0);
        toku_txn_maybe_note_ft(txn, tuple->ft_handle->ft);
    }
    return 0;
}

static int toku_recover_backward_enq_insert (struct logtype_enq_insert *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_enq_insert_no_overwrite (struct logtype_enq_insert_no_overwrite *l, RECOVER_ENV renv) {
    int r;
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(r == 0);
    assert(txn!=NULL);
    struct file_map_tuple *tuple = NULL;
    r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r==0) {
        //Maybe do the insertion if we found the cachefile.
        DBT keydbt, valdbt;
        toku_fill_dbt(&keydbt, l->key.data, l->key.len);
        toku_fill_dbt(&valdbt, l->value.data, l->value.len);
        r = toku_ft_maybe_insert(tuple->ft_handle, &keydbt, &valdbt, txn, TRUE, l->lsn, FALSE, FT_INSERT_NO_OVERWRITE);
        assert(r == 0);
    }    
    return 0;
}

static int toku_recover_backward_enq_insert_no_overwrite (struct logtype_enq_insert_no_overwrite *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_enq_delete_any (struct logtype_enq_delete_any *l, RECOVER_ENV renv) {
    int r;
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(r == 0);
    assert(txn!=NULL);
    struct file_map_tuple *tuple = NULL;
    r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r==0) {
        //Maybe do the deletion if we found the cachefile.
        DBT keydbt;
        toku_fill_dbt(&keydbt, l->key.data, l->key.len);
        r = toku_ft_maybe_delete(tuple->ft_handle, &keydbt, txn, TRUE, l->lsn, FALSE);
        assert(r == 0);
    }    
    return 0;
}

static int toku_recover_backward_enq_delete_any (struct logtype_enq_delete_any *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_enq_insert_multiple (struct logtype_enq_insert_multiple *l, RECOVER_ENV renv) {
    int r;
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(r == 0);
    assert(txn!=NULL);
    DB *src_db = NULL;
    BOOL do_inserts = TRUE;
    {
        struct file_map_tuple *tuple = NULL;
        r = file_map_find(&renv->fmap, l->src_filenum, &tuple);
        if (l->src_filenum.fileid == FILENUM_NONE.fileid)
            assert(r==DB_NOTFOUND);
        else {
            if (r == 0)
                src_db = &tuple->fake_db;
            else
                do_inserts = FALSE; // src file was probably deleted, #3129
        }
    }
    
    if (do_inserts) {
        DBT src_key, src_val, dest_key, dest_val;
        toku_fill_dbt(&src_key, l->src_key.data, l->src_key.len);
        toku_fill_dbt(&src_val, l->src_val.data, l->src_val.len);
        toku_init_dbt_flags(&dest_key, DB_DBT_REALLOC);
        toku_init_dbt_flags(&dest_val, DB_DBT_REALLOC);

        for (uint32_t file = 0; file < l->dest_filenums.num; file++) {
            struct file_map_tuple *tuple = NULL;
            r = file_map_find(&renv->fmap, l->dest_filenums.filenums[file], &tuple);
            if (r==0) {
                // We found the cachefile.  (maybe) Do the insert.
                DB *db = &tuple->fake_db;
                r = renv->generate_row_for_put(db, src_db, &dest_key, &dest_val, &src_key, &src_val);
                assert(r==0);
                r = toku_ft_maybe_insert(tuple->ft_handle, &dest_key, &dest_val, txn, TRUE, l->lsn, FALSE, FT_INSERT);
                assert(r == 0);

                //flags==0 means generate_row_for_put callback changed it
                //(and freed any memory necessary to do so) so that values are now stored
                //in temporary memory that does not need to be freed.  We need to continue
                //using DB_DBT_REALLOC however.
                if (dest_key.flags == 0) 
                    toku_init_dbt_flags(&dest_key, DB_DBT_REALLOC);
                if (dest_val.flags == 0)
                    toku_init_dbt_flags(&dest_val, DB_DBT_REALLOC);
            }
        }

        if (dest_key.data) toku_free(dest_key.data); //TODO: #2321 May need windows hack
        if (dest_val.data) toku_free(dest_val.data); //TODO: #2321 May need windows hack
    }

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
    assert(txn!=NULL);
    DB *src_db = NULL;
    BOOL do_deletes = TRUE;
    {
        struct file_map_tuple *tuple = NULL;
        r = file_map_find(&renv->fmap, l->src_filenum, &tuple);
        if (l->src_filenum.fileid == FILENUM_NONE.fileid)
            assert(r==DB_NOTFOUND);
        else {
            if (r == 0)
                src_db = &tuple->fake_db;
            else
                do_deletes = FALSE; // src file was probably deleted, #3129
        }
    }

    if (do_deletes) {
        DBT src_key, src_val, dest_key;
        toku_fill_dbt(&src_key, l->src_key.data, l->src_key.len);
        toku_fill_dbt(&src_val, l->src_val.data, l->src_val.len);
        toku_init_dbt_flags(&dest_key, DB_DBT_REALLOC);

        for (uint32_t file = 0; file < l->dest_filenums.num; file++) {
            struct file_map_tuple *tuple = NULL;
            r = file_map_find(&renv->fmap, l->dest_filenums.filenums[file], &tuple);
            if (r==0) {
                // We found the cachefile.  (maybe) Do the delete.
                DB *db = &tuple->fake_db;
                r = renv->generate_row_for_del(db, src_db, &dest_key, &src_key, &src_val);
                assert(r==0);
                r = toku_ft_maybe_delete(tuple->ft_handle, &dest_key, txn, TRUE, l->lsn, FALSE);
                assert(r == 0);

                //flags==0 indicates the return values are stored in temporary memory that does
                //not need to be freed.  We need to continue using DB_DBT_REALLOC however.
                if (dest_key.flags == 0)
                    toku_init_dbt_flags(&dest_key, DB_DBT_REALLOC);
            }
        }
        
        if (dest_key.flags & DB_DBT_REALLOC && dest_key.data) toku_free(dest_key.data); //TODO: #2321 May need windows hack
    }

    return 0;
}

static int toku_recover_backward_enq_delete_multiple (struct logtype_enq_delete_multiple *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_enq_update(struct logtype_enq_update *l, RECOVER_ENV renv) {
    int r;
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(r == 0);
    assert(txn != NULL);
    struct file_map_tuple *tuple = NULL;
    r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r == 0) {
        // Maybe do the update if we found the cachefile.
        DBT key, extra;
        toku_fill_dbt(&key, l->key.data, l->key.len);
        toku_fill_dbt(&extra, l->extra.data, l->extra.len);
        r = toku_ft_maybe_update(tuple->ft_handle, &key, &extra, txn, TRUE, l->lsn,
                                  FALSE);
        assert(r == 0);
    }
    return 0;
}

static int toku_recover_enq_updatebroadcast(struct logtype_enq_updatebroadcast *l, RECOVER_ENV renv) {
    int r;
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(r == 0);
    assert(txn != NULL);
    struct file_map_tuple *tuple = NULL;
    r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r == 0) {
        // Maybe do the update broadcast if we found the cachefile.
        DBT extra;
        toku_fill_dbt(&extra, l->extra.data, l->extra.len);
        r = toku_ft_maybe_update_broadcast(tuple->ft_handle, &extra, txn, TRUE,
                                            l->lsn, FALSE, l->is_resetting_op);
        assert(r == 0);
    }
    return 0;
}

static int toku_recover_backward_enq_update(struct logtype_enq_update *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_backward_enq_updatebroadcast(struct logtype_enq_updatebroadcast *UU(l), RECOVER_ENV UU(renv)) {
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

static int toku_recover_shutdown_up_to_19 (struct logtype_shutdown_up_to_19 *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

static int toku_recover_backward_shutdown_up_to_19 (struct logtype_shutdown_up_to_19 *UU(l), RECOVER_ENV UU(renv)) {
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

static int toku_recover_load(struct logtype_load *UU(l), RECOVER_ENV UU(renv)) {
    int r;
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(r == 0);
    assert(txn!=NULL);
    char *new_iname = fixup_fname(&l->new_iname);

    r = toku_ft_load_recovery(txn, l->old_filenum, new_iname, 0, 0, (LSN*)NULL);
    assert(r==0);

    toku_free(new_iname);
    return 0;
}

static int toku_recover_backward_load(struct logtype_load *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

// #2954
static int toku_recover_hot_index(struct logtype_hot_index *UU(l), RECOVER_ENV UU(renv)) {
    int r;
    TOKUTXN txn = NULL;
    r = toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(r == 0);
    assert(txn!=NULL);
    // just make an entry in the rollback log 
    //   - set do_log = 0 -> don't write to recovery log
    r = toku_ft_hot_index_recovery(txn, l->hot_index_filenums, 0, 0, (LSN*)NULL);
    assert(r == 0);
    return 0;
}

// #2954
static int toku_recover_backward_hot_index(struct logtype_hot_index *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

// Effects: If there are no log files, or if there is a clean "shutdown" at
// the end of the log, then we don't need recovery to run.
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
    if (r == 0) {
        needs_recovery = le->cmd != LT_shutdown;
    }
    else {
        needs_recovery = !(r == DB_NOTFOUND && ignore_log_empty);
    }
 exit:
    if (logcursor) {
        r = toku_logcursor_destroy(&logcursor);
        assert(r == 0);
    }
    return needs_recovery;
}

static uint32_t recover_get_num_live_txns(RECOVER_ENV renv) {
    return toku_txn_manager_num_live_txns(renv->logger->txn_manager);
}

static int
is_txn_unprepared (OMTVALUE txnv, u_int32_t UU(index), void* extra) {
    TOKUTXN txn = txnv;
    if (txn->state != TOKUTXN_PREPARING) {
        *(TOKUTXN *)extra = txn;
        return -1; // return -1 to get iterator to return
    }
    return 0;
}


static int find_an_unprepared_txn (RECOVER_ENV renv, TOKUTXN *txnp) {
    TOKUTXN txn = NULL;
    int r = toku_txn_manager_iter_over_live_txns(
        renv->logger->txn_manager,
        is_txn_unprepared,
        &txn
        );
    assert(r == 0 || r == -1);
    if (txn != NULL) {
        *txnp = txn;
        return 0;
    }
    return DB_NOTFOUND;
}

static int
call_prepare_txn_callback_iter (OMTVALUE txnv, u_int32_t UU(index), void* extra) {
    TOKUTXN txn = txnv;
    RECOVER_ENV renv = extra;
    renv->prepared_txn_callback(renv->env, txn);
    return 0;
}

// abort all of the remaining live transactions in descending transaction id order
static void recover_abort_live_txns(RECOVER_ENV renv) {
    while (1) {
        TOKUTXN txn;
        int r = find_an_unprepared_txn(renv, &txn);
        if (r==0) {
            // abort the transaction
            r = toku_txn_abort_txn(txn, NULL, NULL);
            assert(r == 0);
            
            // close the transaction
            toku_txn_close_txn(txn);
        } else if (r==DB_NOTFOUND) {
            break;
        } else {
            abort();
        }
    }

    // Now we have only prepared txns.  These prepared txns don't have full DB_TXNs in them, so we need to make some.
    int r = toku_txn_manager_iter_over_live_txns(
        renv->logger->txn_manager,
        call_prepare_txn_callback_iter,
        renv
        );
    assert_zero(r);
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
    fprintf(stderr, "%.24s Tokudb recovery starting in env %s\n", ctime(&tnow), env_dir);

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

    {
        toku_struct_stat buf;
        if (toku_stat(env_dir, &buf)!=0) {
            fprintf(stderr, "%.24s Tokudb recovery error: directory does not exist: %s\n", ctime(&tnow), env_dir);
            rr = errno; goto errorexit;
        } else if (!S_ISDIR(buf.st_mode)) {
            fprintf(stderr, "%.24s Tokudb recovery error: this file is supposed to be a directory, but is not: %s\n", ctime(&tnow), env_dir);
            rr = ENOTDIR; goto errorexit;
        }
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
            if (r == DB_NOTFOUND)
                break;
            rr = DB_RUNRECOVERY; 
            goto errorexit;
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
        assert(renv->ss.ss == BACKWARD_BETWEEN_CHECKPOINT_BEGIN_END ||
               renv->ss.ss == BACKWARD_NEWER_CHECKPOINT_END);
        logtype_dispatch_assign(le, toku_recover_backward_, r, renv);
        if (tokudb_recovery_trace) 
            recover_trace_le(__FUNCTION__, __LINE__, r, le);
        if (r != 0) {
            if (tokudb_recovery_trace) 
                fprintf(stderr, "DB_RUNRECOVERY: %s:%d r=%d\n", __FUNCTION__, __LINE__, r);
            rr = DB_RUNRECOVERY; 
            goto errorexit;
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
        assert(renv->ss.ss == FORWARD_BETWEEN_CHECKPOINT_BEGIN_END ||
               renv->ss.ss == FORWARD_NEWER_CHECKPOINT_END);
        logtype_dispatch_assign(le, toku_recover_, r, renv);
        if (tokudb_recovery_trace) 
            recover_trace_le(__FUNCTION__, __LINE__, r, le);
        if (r != 0) {
            if (tokudb_recovery_trace) 
                fprintf(stderr, "DB_RUNRECOVERY: %s:%d r=%d\n", __FUNCTION__, __LINE__, r);
            rr = DB_RUNRECOVERY; 
            goto errorexit;
        }

        // get the next log entry
        le = NULL;
        r = toku_logcursor_next(logcursor, &le);
        if (tokudb_recovery_trace) 
            recover_trace_le(__FUNCTION__, __LINE__, r, le);
        if (r != 0) {
            if (r == DB_NOTFOUND)
                break;
            rr = DB_RUNRECOVERY; 
            goto errorexit;
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
    {
        uint32_t n = recover_get_num_live_txns(renv);
        if (n > 0) {
            tnow = time(NULL);
            fprintf(stderr, "%.24s Tokudb recovery has %"PRIu32" live transaction%s\n", ctime(&tnow), n, n > 1 ? "s" : "");
        }
    }
    recover_abort_live_txns(renv);
    {
        uint32_t n = recover_get_num_live_txns(renv);
        if (n > 0) {
            tnow = time(NULL);
            fprintf(stderr, "%.24s Tokudb recovery has %"PRIu32" prepared transaction%s\n", ctime(&tnow), n, n > 1 ? "s" : "");
        }
    }

    // close the open dictionaries
    uint32_t n = file_map_get_num_dictionaries(&renv->fmap);
    if (n > 0) {
        tnow = time(NULL);
        fprintf(stderr, "%.24s Tokudb recovery closing %"PRIu32" dictionar%s\n", ctime(&tnow), n, n > 1 ? "ies" : "y");
    }
    file_map_close_dictionaries(&renv->fmap, TRUE, lastlsn);

    // write a recovery log entry
    BYTESTRING recover_comment = { strlen("recover"), "recover" };
    r = toku_log_comment(renv->logger, NULL, TRUE, 0, recover_comment);
    assert(r == 0);

    // checkpoint 
    tnow = time(NULL);
    fprintf(stderr, "%.24s Tokudb recovery making a checkpoint\n", ctime(&tnow));
    r = toku_checkpoint(renv->ct, renv->logger, NULL, NULL, NULL, NULL, RECOVERY_CHECKPOINT);
    assert(r == 0);
    tnow = time(NULL);
    fprintf(stderr, "%.24s Tokudb recovery done\n", ctime(&tnow));

    return 0;

 errorexit:
    tnow = time(NULL);
    fprintf(stderr, "%.24s Tokudb recovery failed %d\n", ctime(&tnow), rr);

    if (logcursor) {
        r = toku_logcursor_destroy(&logcursor);
        assert(r == 0);
    }

    return rr;
}

int
toku_recover_lock(const char *lock_dir, int *lockfd) {
    if (!lock_dir)
        return ENOENT;
    int namelen=strlen(lock_dir);
    char lockfname[namelen+sizeof(recovery_lock_file)];

    int l = snprintf(lockfname, sizeof(lockfname), "%s%s", lock_dir, recovery_lock_file);
    assert(l+1 == (signed)(sizeof(lockfname)));
    *lockfd = toku_os_lock_file(lockfname);
    if (*lockfd < 0) {
        int e = errno;
        fprintf(stderr, "Couldn't run recovery because some other process holds the recovery lock %s\n", lockfname);
        return e;
    }
    return 0;
}

int
toku_recover_unlock(int lockfd) {
    int r = toku_os_unlock_file(lockfd);
    if (r != 0)
        return errno;
    return 0;
}



int tokudb_recover(DB_ENV *env,
                   prepared_txn_callback_t    prepared_txn_callback,                   
                   keep_cachetable_callback_t keep_cachetable_callback,
                   TOKULOGGER logger,
                   const char *env_dir, const char *log_dir,
                   ft_compare_func bt_compare,
                   ft_update_func update_function,
                   generate_row_for_put_func generate_row_for_put,
                   generate_row_for_del_func generate_row_for_del,
                   size_t cachetable_size) {
    int r;
    int lockfd = -1;

    r = toku_recover_lock(log_dir, &lockfd);
    if (r != 0)
        return r;

    int rr = 0;
    if (tokudb_needs_recovery(log_dir, FALSE)) {
        struct recover_env renv;
        r = recover_env_init(&renv,
                             env_dir,
                             env,
                             prepared_txn_callback,
                             keep_cachetable_callback,
                             logger,
                             bt_compare,
                             update_function,
                             generate_row_for_put,
                             generate_row_for_del,
                             cachetable_size);
        assert(r == 0);

        rr = do_recovery(&renv, env_dir, log_dir);

        recover_env_cleanup(&renv, (BOOL)(rr == 0));
    }

    r = toku_recover_unlock(lockfd);
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
