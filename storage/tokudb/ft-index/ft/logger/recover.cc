/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."


#include <config.h>

#include "ft/cachetable/cachetable.h"
#include "ft/cachetable/checkpoint.h"
#include "ft/ft.h"
#include "ft/log_header.h"
#include "ft/logger/log-internal.h"
#include "ft/logger/logcursor.h"
#include "ft/txn/txn_manager.h"
#include "util/omt.h"

int tokuft_recovery_trace = 0;                    // turn on recovery tracing, default off.

//#define DO_VERIFY_COUNTS
#ifdef DO_VERIFY_COUNTS
#define VERIFY_COUNTS(n) toku_verify_or_set_counts(n, false)
#else
#define VERIFY_COUNTS(n) ((void)0)
#endif

// time in seconds between recovery progress reports
#define TOKUFT_RECOVERY_PROGRESS_TIME 15
time_t tokuft_recovery_progress_time = TOKUFT_RECOVERY_PROGRESS_TIME;

enum ss {
    BACKWARD_NEWER_CHECKPOINT_END = 1,
    BACKWARD_BETWEEN_CHECKPOINT_BEGIN_END,
    FORWARD_BETWEEN_CHECKPOINT_BEGIN_END,
    FORWARD_NEWER_CHECKPOINT_END,
};

struct scan_state {
    enum ss ss;
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

static void file_map_tuple_init(struct file_map_tuple *tuple, FILENUM filenum, FT_HANDLE ft_handle, char *iname) {
    tuple->filenum = filenum;
    tuple->ft_handle = ft_handle;
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

// Map filenum to ft_handle
struct file_map {
    toku::omt<struct file_map_tuple *> *filenums;
};

// The recovery environment
struct recover_env {
    DB_ENV *env;
    prepared_txn_callback_t    prepared_txn_callback;    // at the end of recovery, all the prepared txns are passed back to the ydb layer to make them into valid transactions.
    keep_cachetable_callback_t keep_cachetable_callback; // after recovery, store the cachetable into the environment.
    CACHETABLE ct;
    TOKULOGGER logger;
    CHECKPOINTER cp;
    ft_compare_func bt_compare;
    ft_update_func update_function;
    generate_row_for_put_func generate_row_for_put;
    generate_row_for_del_func generate_row_for_del;
    DBT_ARRAY dest_keys;
    DBT_ARRAY dest_vals;
    struct scan_state ss;
    struct file_map fmap;
    bool goforward;
    bool destroy_logger_at_end; // If true then destroy the logger when we are done.  If false then set the logger into write-files mode when we are done with recovery.*/
};
typedef struct recover_env *RECOVER_ENV;


static void file_map_init(struct file_map *fmap) {
    XMALLOC(fmap->filenums);
    fmap->filenums->create();
}

static void file_map_destroy(struct file_map *fmap) {
    fmap->filenums->destroy();
    toku_free(fmap->filenums);
    fmap->filenums = nullptr;
}

static uint32_t file_map_get_num_dictionaries(struct file_map *fmap) {
    return fmap->filenums->size();
}

static void file_map_close_dictionaries(struct file_map *fmap, LSN oplsn) {
    int r;

    while (1) {
        uint32_t n = fmap->filenums->size();
        if (n == 0) {
            break;
        }
        struct file_map_tuple *tuple;
        r = fmap->filenums->fetch(n - 1, &tuple);
        assert(r == 0);
        r = fmap->filenums->delete_at(n - 1);
        assert(r == 0);
        assert(tuple->ft_handle);
        // Logging is on again, but we must pass the right LSN into close.
        if (tuple->ft_handle) { // it's a DB, not a rollback file
            toku_ft_handle_close_recovery(tuple->ft_handle, oplsn);
        }
        file_map_tuple_destroy(tuple);
        toku_free(tuple);
    }
}

static int file_map_h(struct file_map_tuple *const &a, const FILENUM &b) {
    if (a->filenum.fileid < b.fileid) {
        return -1;
    } else if (a->filenum.fileid > b.fileid) {
        return 1;
    } else {
        return 0;
    }
}

static int file_map_insert (struct file_map *fmap, FILENUM fnum, FT_HANDLE ft_handle, char *iname) {
    struct file_map_tuple *XMALLOC(tuple);
    file_map_tuple_init(tuple, fnum, ft_handle, iname);
    int r = fmap->filenums->insert<FILENUM, file_map_h>(tuple, fnum, nullptr);
    return r;
}

static void file_map_remove(struct file_map *fmap, FILENUM fnum) {
    uint32_t idx;
    struct file_map_tuple *tuple;
    int r = fmap->filenums->find_zero<FILENUM, file_map_h>(fnum, &tuple, &idx);
    if (r == 0) {
        r = fmap->filenums->delete_at(idx);
        file_map_tuple_destroy(tuple);
        toku_free(tuple);
    }
}

// Look up file info: given FILENUM, return file_map_tuple (or DB_NOTFOUND)
static int file_map_find(struct file_map *fmap, FILENUM fnum, struct file_map_tuple **file_map_tuple) {
    uint32_t idx;
    struct file_map_tuple *tuple;
    int r = fmap->filenums->find_zero<FILENUM, file_map_h>(fnum, &tuple, &idx);
    if (r == 0) {
        assert(tuple->filenum.fileid == fnum.fileid);
        *file_map_tuple = tuple;
    } else {
        assert(r == DB_NOTFOUND);
    }
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
    int r = 0;

    // If we are passed a logger use it, otherwise create one.
    renv->destroy_logger_at_end = logger==NULL;
    if (logger) {
        renv->logger = logger;
    } else {
        r = toku_logger_create(&renv->logger);
        assert(r == 0);
    }
    toku_logger_write_log_files(renv->logger, false);
    toku_cachetable_create(&renv->ct, cachetable_size ? cachetable_size : 1<<25, (LSN){0}, renv->logger);
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
    renv->goforward = false;
    renv->cp = toku_cachetable_get_checkpointer(renv->ct);
    toku_dbt_array_init(&renv->dest_keys, 1);
    toku_dbt_array_init(&renv->dest_vals, 1);
    if (tokuft_recovery_trace)
        fprintf(stderr, "%s:%d\n", __FUNCTION__, __LINE__);
    return r;
}

static void recover_env_cleanup (RECOVER_ENV renv) {
    invariant_zero(renv->fmap.filenums->size());
    file_map_destroy(&renv->fmap);

    if (renv->destroy_logger_at_end) {
        toku_logger_close_rollback(renv->logger);
        int r = toku_logger_close(&renv->logger);
        assert(r == 0);
    } else {
        toku_logger_write_log_files(renv->logger, true);
    }

    if (renv->keep_cachetable_callback) {
        renv->ct = NULL;
    } else {
        toku_cachetable_close(&renv->ct);
    }
    toku_dbt_array_destroy(&renv->dest_keys);
    toku_dbt_array_destroy(&renv->dest_vals);

    if (tokuft_recovery_trace)
        fprintf(stderr, "%s:%d\n", __FUNCTION__, __LINE__);
}

static const char *recover_state(RECOVER_ENV renv) {
    return scan_state_string(&renv->ss);
}

// Open the file if it is not already open.  If it is already open, then do nothing.
static int internal_recover_fopen_or_fcreate (RECOVER_ENV renv, bool must_create, int UU(mode), BYTESTRING *bs_iname, FILENUM filenum, uint32_t treeflags,
                                              TOKUTXN txn, uint32_t nodesize, uint32_t basementnodesize, enum toku_compression_method compression_method, LSN max_acceptable_lsn) {
    int r = 0;
    FT_HANDLE ft_handle = NULL;
    char *iname = fixup_fname(bs_iname);

    toku_ft_handle_create(&ft_handle);
    toku_ft_set_flags(ft_handle, treeflags);

    if (nodesize != 0) {
        toku_ft_handle_set_nodesize(ft_handle, nodesize);
    }

    if (basementnodesize != 0) {
        toku_ft_handle_set_basementnodesize(ft_handle, basementnodesize);
    }

    if (compression_method != TOKU_DEFAULT_COMPRESSION_METHOD) {
        toku_ft_handle_set_compression_method(ft_handle, compression_method);
    }

    // set the key compare functions
    if (!(treeflags & TOKU_DB_KEYCMP_BUILTIN) && renv->bt_compare) {
        toku_ft_set_bt_compare(ft_handle, renv->bt_compare);
    }

    if (renv->update_function) {
        toku_ft_set_update(ft_handle, renv->update_function);
    }

    // TODO mode (FUTURE FEATURE)
    //mode = mode;

    r = toku_ft_handle_open_recovery(ft_handle, iname, must_create, must_create, renv->ct, txn, filenum, max_acceptable_lsn);
    if (r != 0) {
        //Note:  If ft_handle_open fails, then close_ft will NOT write a header to disk.
        //No need to provide lsn, so use the regular toku_ft_handle_close function
        toku_ft_handle_close(ft_handle);
        toku_free(iname);
        if (r == ENOENT) //Not an error to simply be missing.
            r = 0;
        return r;
    }

    file_map_insert(&renv->fmap, filenum, ft_handle, iname);
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
        fprintf(stderr, "TokuFT recovery %s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)renv->ss.ss);
        abort();
        break;
    }
    return r;
}

static int toku_recover_backward_begin_checkpoint (struct logtype_begin_checkpoint *l, RECOVER_ENV renv) {
    int r;
    time_t tnow = time(NULL);
    fprintf(stderr, "%.24s TokuFT recovery bw_begin_checkpoint at %" PRIu64 " timestamp %" PRIu64 " (%s)\n", ctime(&tnow), l->lsn.lsn, l->timestamp, recover_state(renv));
    switch (renv->ss.ss) {
    case BACKWARD_NEWER_CHECKPOINT_END:
        // incomplete checkpoint, nothing to do
        r = 0;
        break;
    case BACKWARD_BETWEEN_CHECKPOINT_BEGIN_END:
        assert(l->lsn.lsn == renv->ss.checkpoint_begin_lsn.lsn);
        renv->ss.ss = FORWARD_BETWEEN_CHECKPOINT_BEGIN_END;
        renv->ss.checkpoint_begin_timestamp = l->timestamp;
        renv->goforward = true;
        tnow = time(NULL);
        fprintf(stderr, "%.24s TokuFT recovery turning around at begin checkpoint %" PRIu64 " time %" PRIu64 "\n", 
                ctime(&tnow), l->lsn.lsn, 
                renv->ss.checkpoint_end_timestamp - renv->ss.checkpoint_begin_timestamp);
        r = 0;
        break;
    default:
        fprintf(stderr, "TokuFT recovery %s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)renv->ss.ss);
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
    fprintf(stderr, "%.24s TokuFT recovery bw_end_checkpoint at %" PRIu64 " timestamp %" PRIu64 " xid %" PRIu64 " (%s)\n", ctime(&tnow), l->lsn.lsn, l->timestamp, l->lsn_begin_checkpoint.lsn, recover_state(renv));
    switch (renv->ss.ss) {
    case BACKWARD_NEWER_CHECKPOINT_END:
        renv->ss.ss = BACKWARD_BETWEEN_CHECKPOINT_BEGIN_END;
        renv->ss.checkpoint_begin_lsn.lsn = l->lsn_begin_checkpoint.lsn;
        renv->ss.checkpoint_end_lsn.lsn   = l->lsn.lsn;
        renv->ss.checkpoint_end_timestamp = l->timestamp;
        return 0;
    case BACKWARD_BETWEEN_CHECKPOINT_BEGIN_END:
        fprintf(stderr, "TokuFT recovery %s:%d Should not see two end_checkpoint log entries without an intervening begin_checkpoint\n", __FILE__, __LINE__);
        abort();
    default:
        break;
    }
    fprintf(stderr, "TokuFT recovery %s: %d Unknown checkpoint state %d\n", __FILE__, __LINE__, (int)renv->ss.ss);
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
            bool rollback_file = (0==strcmp(fname, toku_product_name_strings.rollback_cachefile));
            LSN max_acceptable_lsn = MAX_LSN;
            if (rollback_file) {
                max_acceptable_lsn = renv->ss.checkpoint_begin_lsn;
                FT_HANDLE t;
                toku_ft_handle_create(&t);
                r = toku_ft_handle_open_recovery(t, toku_product_name_strings.rollback_cachefile, false, false, renv->ct, (TOKUTXN)NULL, l->filenum, max_acceptable_lsn);
                renv->logger->rollback_cachefile = t->ft->cf;
                toku_logger_initialize_rollback_cache(renv->logger, t->ft);
            } else {
                r = internal_recover_fopen_or_fcreate(renv, false, 0, &l->iname, l->filenum, l->treeflags, NULL, 0, 0, TOKU_DEFAULT_COMPRESSION_METHOD, max_acceptable_lsn);
                assert(r==0);
            }
        }
        // try to open the file again and if we get it, restore
        // the unlink on close bit.
        int ret;
        ret = file_map_find(&renv->fmap, l->filenum, &tuple);
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
recover_transaction(TOKUTXN *txnp, TXNID_PAIR xid, TXNID_PAIR parentxid, TOKULOGGER logger) {
    int r;

    // lookup the parent
    TOKUTXN parent = NULL;
    if (!txn_pair_is_none(parentxid)) {
        toku_txnid2txn(logger, parentxid, &parent);
        assert(parent!=NULL);
    }
    else {
        invariant(xid.child_id64 == TXNID_NONE);
    }

    // create a transaction and bind it to the transaction id
    TOKUTXN txn = NULL;
    {
        //Verify it does not yet exist.
        toku_txnid2txn(logger, xid, &txn);
        assert(txn==NULL);
    }
    r = toku_txn_begin_with_xid(
        parent, 
        &txn, 
        logger, 
        xid, 
        TXN_SNAPSHOT_NONE, 
        NULL, 
        true, // for_recovery
        false // read_only
        );
    assert(r == 0);
    // We only know about it because it was logged.  Restore the log bit.
    // Logging is 'off' but it will still set the bit.
    toku_maybe_log_begin_txn_for_write_operation(txn);
    if (txnp) *txnp = txn;
    return 0;
}

static int recover_xstillopen_internal (TOKUTXN         *txnp,
                                        LSN           UU(lsn),
                                        TXNID_PAIR       xid,
                                        TXNID_PAIR       parentxid,
                                        uint64_t        rollentry_raw_count,
                                        FILENUMS         open_filenums,
                                        bool             force_fsync_on_commit,
                                        uint64_t        num_rollback_nodes,
                                        uint64_t        num_rollentries,
                                        BLOCKNUM         spilled_rollback_head,
                                        BLOCKNUM         spilled_rollback_tail,
                                        BLOCKNUM         current_rollback,
                                        uint32_t     UU(crc),
                                        uint32_t     UU(len),
                                        RECOVER_ENV      renv) {
    int r;
    *txnp = NULL;
    switch (renv->ss.ss) {
    case FORWARD_BETWEEN_CHECKPOINT_BEGIN_END: {
        renv->ss.checkpoint_num_xstillopen++;
        invariant(renv->ss.last_xid != TXNID_NONE);
        invariant(xid.parent_id64 <= renv->ss.last_xid);
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
        toku_txnid2txn(renv->logger, xid, &txn);
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
                                         TXNID_PAIR_NONE,
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
            toku_txn_prepare_txn(txn, l->xa_xid, 0);
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

static int toku_recover_xbegin (struct logtype_xbegin *l, RECOVER_ENV renv) {
    int r;
    r = recover_transaction(NULL, l->xid, l->parentxid, renv->logger);
    return r;
}

static int toku_recover_backward_xbegin (struct logtype_xbegin *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

struct toku_txn_progress_extra {
    time_t tlast;
    LSN lsn;
    const char *type;
    TXNID_PAIR xid;
    uint64_t last_total;
};

static void toku_recover_txn_progress(TOKU_TXN_PROGRESS txn_progress, void *extra) {
    toku_txn_progress_extra *txn_progress_extra = static_cast<toku_txn_progress_extra *>(extra);
    if (txn_progress_extra->last_total == 0)
        txn_progress_extra->last_total = txn_progress->entries_total;
    else
        assert(txn_progress_extra->last_total == txn_progress->entries_total);
    time_t tnow = time(NULL);
    if (tnow - txn_progress_extra->tlast >= tokuft_recovery_progress_time) {
        txn_progress_extra->tlast = tnow;
        fprintf(stderr, "%.24s TokuFT ", ctime(&tnow));
        if (txn_progress_extra->lsn.lsn != 0)
            fprintf(stderr, "lsn %" PRIu64 " ", txn_progress_extra->lsn.lsn);
        fprintf(stderr, "%s xid %" PRIu64 ":%" PRIu64 " ",
                txn_progress_extra->type, txn_progress_extra->xid.parent_id64, txn_progress_extra->xid.child_id64);
        fprintf(stderr, "%" PRIu64 "/%" PRIu64 " ",
                txn_progress->entries_processed, txn_progress->entries_total);
        if (txn_progress->entries_total > 0)
            fprintf(stderr, "%.0f%% ", ((double) txn_progress->entries_processed / (double) txn_progress->entries_total) * 100.0);
        fprintf(stderr, "\n");
    }
}

static int toku_recover_xcommit (struct logtype_xcommit *l, RECOVER_ENV renv) {
    // find the transaction by transaction id
    TOKUTXN txn = NULL;
    toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(txn!=NULL);

    // commit the transaction
    toku_txn_progress_extra extra = { time(NULL), l->lsn, "commit", l->xid, 0 };
    int r = toku_txn_commit_with_lsn(txn, true, l->lsn, toku_recover_txn_progress, &extra);
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
    // find the transaction by transaction id
    TOKUTXN txn = NULL;
    toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(txn!=NULL);

    // Save the transaction
    toku_txn_prepare_txn(txn, l->xa_xid, 0);

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
    toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(txn!=NULL);

    // abort the transaction
    toku_txn_progress_extra extra = { time(NULL), l->lsn, "abort", l->xid, 0 };
    r = toku_txn_abort_with_lsn(txn, l->lsn, toku_recover_txn_progress, &extra);
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
    toku_txnid2txn(renv->logger, l->xid, &txn);

    // assert that filenum is closed
    struct file_map_tuple *tuple = NULL;
    r = file_map_find(&renv->fmap, l->filenum, &tuple);
    assert(r==DB_NOTFOUND);

    assert(txn!=NULL);

    //unlink if it exists (recreate from scratch).
    char *iname = fixup_fname(&l->iname);
    char *iname_in_cwd = toku_cachetable_get_fname_in_cwd(renv->ct, iname);
    r = unlink(iname_in_cwd);
    if (r != 0) {
        int er = get_error_errno();
        if (er != ENOENT) {
            fprintf(stderr, "TokuFT recovery %s:%d unlink %s %d\n", __FUNCTION__, __LINE__, iname, er);
            toku_free(iname);
            return r;
        }
    }
    assert(0!=strcmp(iname, toku_product_name_strings.rollback_cachefile)); //Creation of rollback cachefile never gets logged.
    toku_free(iname_in_cwd);
    toku_free(iname);

    bool must_create = true;
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

    bool must_create = false;
    TOKUTXN txn = NULL;
    char *fname = fixup_fname(&l->iname);

    assert(0!=strcmp(fname, toku_product_name_strings.rollback_cachefile)); //Rollback cachefile can be opened only via fassociate.
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
        toku_txnid2txn(renv->logger, l->xid, &txn);
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
        toku_ft_change_descriptor(
            tuple->ft_handle, 
            &old_descriptor, 
            &new_descriptor, 
            false, 
            txn,
            l->update_cmp_descriptor
            );
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

        if (0!=strcmp(iname, toku_product_name_strings.rollback_cachefile)) {
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
    toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(txn != NULL);

    // if the forward scan in recovery found this file and opened it, we
    // need to mark the txn to remove the ft on commit. if the file was
    // not found and not opened, we don't need to do anything - the ft
    // is already gone, so we're happy.
    struct file_map_tuple *tuple;
    int r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r == 0) {
        toku_ft_unlink_on_commit(tuple->ft_handle, txn);
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
    toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(txn!=NULL);
    struct file_map_tuple *tuple = NULL;
    r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r==0) {
        //Maybe do the insertion if we found the cachefile.
        DBT keydbt, valdbt;
        toku_fill_dbt(&keydbt, l->key.data, l->key.len);
        toku_fill_dbt(&valdbt, l->value.data, l->value.len);
        toku_ft_maybe_insert(tuple->ft_handle, &keydbt, &valdbt, txn, true, l->lsn, false, FT_INSERT);
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
    toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(txn!=NULL);
    struct file_map_tuple *tuple = NULL;
    r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r==0) {
        //Maybe do the insertion if we found the cachefile.
        DBT keydbt, valdbt;
        toku_fill_dbt(&keydbt, l->key.data, l->key.len);
        toku_fill_dbt(&valdbt, l->value.data, l->value.len);
        toku_ft_maybe_insert(tuple->ft_handle, &keydbt, &valdbt, txn, true, l->lsn, false, FT_INSERT_NO_OVERWRITE);
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
    toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(txn!=NULL);
    struct file_map_tuple *tuple = NULL;
    r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r==0) {
        //Maybe do the deletion if we found the cachefile.
        DBT keydbt;
        toku_fill_dbt(&keydbt, l->key.data, l->key.len);
        toku_ft_maybe_delete(tuple->ft_handle, &keydbt, txn, true, l->lsn, false);
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
    toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(txn!=NULL);
    DB *src_db = NULL;
    bool do_inserts = true;
    {
        struct file_map_tuple *tuple = NULL;
        r = file_map_find(&renv->fmap, l->src_filenum, &tuple);
        if (l->src_filenum.fileid == FILENUM_NONE.fileid)
            assert(r==DB_NOTFOUND);
        else {
            if (r == 0)
                src_db = &tuple->fake_db;
            else
                do_inserts = false; // src file was probably deleted, #3129
        }
    }
    
    if (do_inserts) {
        DBT src_key, src_val;

        toku_fill_dbt(&src_key, l->src_key.data, l->src_key.len);
        toku_fill_dbt(&src_val, l->src_val.data, l->src_val.len);

        for (uint32_t file = 0; file < l->dest_filenums.num; file++) {
            struct file_map_tuple *tuple = NULL;
            r = file_map_find(&renv->fmap, l->dest_filenums.filenums[file], &tuple);
            if (r==0) {
                // We found the cachefile.  (maybe) Do the insert.
                DB *db = &tuple->fake_db;

                DBT_ARRAY key_array;
                DBT_ARRAY val_array;
                if (db != src_db) {
                    r = renv->generate_row_for_put(db, src_db, &renv->dest_keys, &renv->dest_vals, &src_key, &src_val);
                    assert(r==0);
                    invariant(renv->dest_keys.size <= renv->dest_keys.capacity);
                    invariant(renv->dest_vals.size <= renv->dest_vals.capacity);
                    invariant(renv->dest_keys.size == renv->dest_vals.size);
                    key_array = renv->dest_keys;
                    val_array = renv->dest_vals;
                } else {
                    key_array.size = key_array.capacity = 1;
                    key_array.dbts = &src_key;

                    val_array.size = val_array.capacity = 1;
                    val_array.dbts = &src_val;
                }
                for (uint32_t i = 0; i < key_array.size; i++) {
                    toku_ft_maybe_insert(tuple->ft_handle, &key_array.dbts[i], &val_array.dbts[i], txn, true, l->lsn, false, FT_INSERT);
                }
            }
        }
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
    toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(txn!=NULL);
    DB *src_db = NULL;
    bool do_deletes = true;
    {
        struct file_map_tuple *tuple = NULL;
        r = file_map_find(&renv->fmap, l->src_filenum, &tuple);
        if (l->src_filenum.fileid == FILENUM_NONE.fileid)
            assert(r==DB_NOTFOUND);
        else {
            if (r == 0) {
                src_db = &tuple->fake_db;
            } else {
                do_deletes = false; // src file was probably deleted, #3129
            }
        }
    }

    if (do_deletes) {
        DBT src_key, src_val;
        toku_fill_dbt(&src_key, l->src_key.data, l->src_key.len);
        toku_fill_dbt(&src_val, l->src_val.data, l->src_val.len);

        for (uint32_t file = 0; file < l->dest_filenums.num; file++) {
            struct file_map_tuple *tuple = NULL;
            r = file_map_find(&renv->fmap, l->dest_filenums.filenums[file], &tuple);
            if (r==0) {
                // We found the cachefile.  (maybe) Do the delete.
                DB *db = &tuple->fake_db;

                DBT_ARRAY key_array;
                if (db != src_db) {
                    r = renv->generate_row_for_del(db, src_db, &renv->dest_keys, &src_key, &src_val);
                    assert(r==0);
                    invariant(renv->dest_keys.size <= renv->dest_keys.capacity);
                    key_array = renv->dest_keys;
                } else {
                    key_array.size = key_array.capacity = 1;
                    key_array.dbts = &src_key;
                }
                for (uint32_t i = 0; i < key_array.size; i++) {
                    toku_ft_maybe_delete(tuple->ft_handle, &key_array.dbts[i], txn, true, l->lsn, false);
                }
            }
        }
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
    toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(txn != NULL);
    struct file_map_tuple *tuple = NULL;
    r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r == 0) {
        // Maybe do the update if we found the cachefile.
        DBT key, extra;
        toku_fill_dbt(&key, l->key.data, l->key.len);
        toku_fill_dbt(&extra, l->extra.data, l->extra.len);
        toku_ft_maybe_update(tuple->ft_handle, &key, &extra, txn, true, l->lsn, false);
    }
    return 0;
}

static int toku_recover_enq_updatebroadcast(struct logtype_enq_updatebroadcast *l, RECOVER_ENV renv) {
    int r;
    TOKUTXN txn = NULL;
    toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(txn != NULL);
    struct file_map_tuple *tuple = NULL;
    r = file_map_find(&renv->fmap, l->filenum, &tuple);
    if (r == 0) {
        // Maybe do the update broadcast if we found the cachefile.
        DBT extra;
        toku_fill_dbt(&extra, l->extra.data, l->extra.len);
        toku_ft_maybe_update_broadcast(tuple->ft_handle, &extra, txn, true,
                                            l->lsn, false, l->is_resetting_op);
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
    TOKUTXN txn = NULL;
    toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(txn!=NULL);
    char *new_iname = fixup_fname(&l->new_iname);

    toku_ft_load_recovery(txn, l->old_filenum, new_iname, 0, 0, (LSN*)NULL);

    toku_free(new_iname);
    return 0;
}

static int toku_recover_backward_load(struct logtype_load *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

// #2954
static int toku_recover_hot_index(struct logtype_hot_index *UU(l), RECOVER_ENV UU(renv)) {
    TOKUTXN txn = NULL;
    toku_txnid2txn(renv->logger, l->xid, &txn);
    assert(txn!=NULL);
    // just make an entry in the rollback log 
    //   - set do_log = 0 -> don't write to recovery log
    toku_ft_hot_index_recovery(txn, l->hot_index_filenums, 0, 0, (LSN*)NULL);
    return 0;
}

// #2954
static int toku_recover_backward_hot_index(struct logtype_hot_index *UU(l), RECOVER_ENV UU(renv)) {
    // nothing
    return 0;
}

// Effects: If there are no log files, or if there is a clean "shutdown" at
// the end of the log, then we don't need recovery to run.
// Returns: true if we need recovery, otherwise false.
int tokuft_needs_recovery(const char *log_dir, bool ignore_log_empty) {
    int needs_recovery;
    int r;
    TOKULOGCURSOR logcursor = NULL;

    r = toku_logcursor_create(&logcursor, log_dir);
    if (r != 0) {
        needs_recovery = true; goto exit;
    }
    
    struct log_entry *le;
    le = NULL;
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
    return toku_txn_manager_num_live_root_txns(renv->logger->txn_manager);
}

static int is_txn_unprepared(TOKUTXN txn, void* extra) {
    TOKUTXN* ptxn = (TOKUTXN *)extra;
    if (txn->state != TOKUTXN_PREPARING) {
        *ptxn = txn;
        return -1; // return -1 to get iterator to return
    }
    return 0;
}

static int find_an_unprepared_txn (RECOVER_ENV renv, TOKUTXN *txnp) {
    TOKUTXN txn = nullptr;
    int r = toku_txn_manager_iter_over_live_root_txns(
        renv->logger->txn_manager,
        is_txn_unprepared,
        &txn
        );
    assert(r == 0 || r == -1);
    if (txn != nullptr) {
        *txnp = txn;
        return 0;
    }
    return DB_NOTFOUND;
}

static int call_prepare_txn_callback_iter(TOKUTXN txn, void* extra) {
    RECOVER_ENV* renv = (RECOVER_ENV *)extra;
    invariant(txn->state == TOKUTXN_PREPARING);
    invariant(txn->child == NULL);
    (*renv)->prepared_txn_callback((*renv)->env, txn);
    return 0;
}

static void recover_abort_live_txn(TOKUTXN txn) {
    fprintf(stderr, "%s %" PRIu64 "\n", __FUNCTION__, txn->txnid.parent_id64);
    // recursively abort all children first
    if (txn->child != NULL) {
        recover_abort_live_txn(txn->child);
    }
    // sanity check that the recursive call successfully NULLs out txn->child
    invariant(txn->child == NULL);
    // abort the transaction
    toku_txn_progress_extra extra = { time(NULL), ZERO_LSN, "abort live", txn->txnid, 0 };
    int r = toku_txn_abort_txn(txn, toku_recover_txn_progress, &extra);
    assert(r == 0);
    
    // close the transaction
    toku_txn_close_txn(txn);
}

// abort all of the remaining live transactions in descending transaction id order
static void recover_abort_all_live_txns(RECOVER_ENV renv) {
    while (1) {
        TOKUTXN txn;
        int r = find_an_unprepared_txn(renv, &txn);
        if (r==0) {
            recover_abort_live_txn(txn);
        } else if (r==DB_NOTFOUND) {
            break;
        } else {
            abort();
        }
    }

    // Now we have only prepared txns.  These prepared txns don't have full DB_TXNs in them, so we need to make some.
    int r = toku_txn_manager_iter_over_live_root_txns(
        renv->logger->txn_manager,
        call_prepare_txn_callback_iter,
        &renv
        );
    assert_zero(r);
}

static void recover_trace_le(const char *f, int l, int r, struct log_entry *le) {
    if (le) {
        LSN thislsn = toku_log_entry_get_lsn(le);
        fprintf(stderr, "%s:%d r=%d cmd=%c lsn=%" PRIu64 "\n", f, l, r, le->cmd, thislsn.lsn);
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
    fprintf(stderr, "%.24s TokuFT recovery starting in env %s\n", ctime(&tnow), env_dir);

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
        if (tokuft_recovery_trace) 
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
            rr = get_error_errno();
            fprintf(stderr, "%.24s TokuFT recovery error: directory does not exist: %s\n", ctime(&tnow), env_dir);
            goto errorexit;
        } else if (!S_ISDIR(buf.st_mode)) {
            fprintf(stderr, "%.24s TokuFT recovery error: this file is supposed to be a directory, but is not: %s\n", ctime(&tnow), env_dir);
            rr = ENOTDIR; goto errorexit;
        }
    }
    // scan backwards
    scan_state_init(&renv->ss);
    tnow = time(NULL);
    time_t tlast;
    tlast = tnow;
    fprintf(stderr, "%.24s TokuFT recovery scanning backward from %" PRIu64 "\n", ctime(&tnow), lastlsn.lsn);
    for (unsigned i=0; 1; i++) {

        // get the previous log entry (first time gets the last one)
        le = NULL;
        r = toku_logcursor_prev(logcursor, &le);
        if (tokuft_recovery_trace) 
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
            if (tnow - tlast >= tokuft_recovery_progress_time) {
                thislsn = toku_log_entry_get_lsn(le);
                fprintf(stderr, "%.24s TokuFT recovery scanning backward from %" PRIu64 " at %" PRIu64 " (%s)\n",
                        ctime(&tnow), lastlsn.lsn, thislsn.lsn, recover_state(renv));
                tlast = tnow;
            }
        }

        // dispatch the log entry handler
        assert(renv->ss.ss == BACKWARD_BETWEEN_CHECKPOINT_BEGIN_END ||
               renv->ss.ss == BACKWARD_NEWER_CHECKPOINT_END);
        logtype_dispatch_assign(le, toku_recover_backward_, r, renv);
        if (tokuft_recovery_trace) 
            recover_trace_le(__FUNCTION__, __LINE__, r, le);
        if (r != 0) {
            if (tokuft_recovery_trace) 
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
    fprintf(stderr, "%.24s TokuFT recovery starts scanning forward to %" PRIu64 " from %" PRIu64 " left %" PRIu64 " (%s)\n",
            ctime(&tnow), lastlsn.lsn, thislsn.lsn, lastlsn.lsn - thislsn.lsn, recover_state(renv));

    for (unsigned i=0; 1; i++) {

        // trace progress
        if ((i % 1000) == 0) {
            tnow = time(NULL);
            if (tnow - tlast >= tokuft_recovery_progress_time) {
                thislsn = toku_log_entry_get_lsn(le);
                fprintf(stderr, "%.24s TokuFT recovery scanning forward to %" PRIu64 " at %" PRIu64 " left %" PRIu64 " (%s)\n",
                        ctime(&tnow), lastlsn.lsn, thislsn.lsn, lastlsn.lsn - thislsn.lsn, recover_state(renv));
                tlast = tnow;
            }
        }

        // dispatch the log entry handler (first time calls the forward handler for the log entry at the turnaround
        assert(renv->ss.ss == FORWARD_BETWEEN_CHECKPOINT_BEGIN_END ||
               renv->ss.ss == FORWARD_NEWER_CHECKPOINT_END);
        logtype_dispatch_assign(le, toku_recover_, r, renv);
        if (tokuft_recovery_trace) 
            recover_trace_le(__FUNCTION__, __LINE__, r, le);
        if (r != 0) {
            if (tokuft_recovery_trace) 
                fprintf(stderr, "DB_RUNRECOVERY: %s:%d r=%d\n", __FUNCTION__, __LINE__, r);
            rr = DB_RUNRECOVERY; 
            goto errorexit;
        }

        // get the next log entry
        le = NULL;
        r = toku_logcursor_next(logcursor, &le);
        if (tokuft_recovery_trace) 
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
            fprintf(stderr, "%.24s TokuFT recovery has %" PRIu32 " live transaction%s\n", ctime(&tnow), n, n > 1 ? "s" : "");
        }
    }
    recover_abort_all_live_txns(renv);
    {
        uint32_t n = recover_get_num_live_txns(renv);
        if (n > 0) {
            tnow = time(NULL);
            fprintf(stderr, "%.24s TokuFT recovery has %" PRIu32 " prepared transaction%s\n", ctime(&tnow), n, n > 1 ? "s" : "");
        }
    }

    // close the open dictionaries
    uint32_t n;
    n = file_map_get_num_dictionaries(&renv->fmap);
    if (n > 0) {
        tnow = time(NULL);
        fprintf(stderr, "%.24s TokuFT recovery closing %" PRIu32 " dictionar%s\n", ctime(&tnow), n, n > 1 ? "ies" : "y");
    }
    file_map_close_dictionaries(&renv->fmap, lastlsn);

    {
        // write a recovery log entry
        BYTESTRING recover_comment = { static_cast<uint32_t>(strlen("recover")), (char *) "recover" };
        toku_log_comment(renv->logger, NULL, true, 0, recover_comment);
    }

    // checkpoint 
    tnow = time(NULL);
    fprintf(stderr, "%.24s TokuFT recovery making a checkpoint\n", ctime(&tnow));
    r = toku_checkpoint(renv->cp, renv->logger, NULL, NULL, NULL, NULL, RECOVERY_CHECKPOINT);
    assert(r == 0);
    tnow = time(NULL);
    fprintf(stderr, "%.24s TokuFT recovery done\n", ctime(&tnow));

    return 0;

 errorexit:
    tnow = time(NULL);
    fprintf(stderr, "%.24s TokuFT recovery failed %d\n", ctime(&tnow), rr);

    if (logcursor) {
        r = toku_logcursor_destroy(&logcursor);
        assert(r == 0);
    }

    return rr;
}

int
toku_recover_lock(const char *lock_dir, int *lockfd) {
    int e = toku_single_process_lock(lock_dir, "recovery", lockfd);
    if (e != 0 && e != ENOENT) {
        fprintf(stderr, "Couldn't run recovery because some other process holds the recovery lock\n");
    }
    return e;
}

int
toku_recover_unlock(int lockfd) {
    int lockfd_copy = lockfd;
    return toku_single_process_unlock(&lockfd_copy);
}

int tokuft_recover(DB_ENV *env,
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
    if (tokuft_needs_recovery(log_dir, false)) {
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

        recover_env_cleanup(&renv);
    }

    r = toku_recover_unlock(lockfd);
    if (r != 0)
        return r;

    return rr;
}

// Return 0 if recovery log exists, ENOENT if log is missing
int 
tokuft_recover_log_exists(const char * log_dir) {
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
