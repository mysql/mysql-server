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

#include "ft/serialize/block_table.h"
#include "ft/ft.h"
#include "ft/ft-cachetable-wrappers.h"
#include "ft/ft-internal.h"
#include "ft/logger/log-internal.h"
#include "ft/log_header.h"
#include "ft/node.h"
#include "ft/serialize/ft-serialize.h"
#include "ft/serialize/ft_node-serialize.h"

#include <memory.h>
#include <toku_assert.h>
#include <portability/toku_atomic.h>

void 
toku_reset_root_xid_that_created(FT ft, TXNID new_root_xid_that_created) {
    // Reset the root_xid_that_created field to the given value.  
    // This redefines which xid created the dictionary.

    // hold lock around setting and clearing of dirty bit
    // (see cooperative use of dirty bit in ft_begin_checkpoint())
    toku_ft_lock(ft);
    ft->h->root_xid_that_created = new_root_xid_that_created;
    ft->h->dirty = 1;
    toku_ft_unlock(ft);
}

static void
ft_destroy(FT ft) {
    //header and checkpoint_header have same Blocktable pointer
    //cannot destroy since it is still in use by CURRENT
    assert(ft->h->type == FT_CURRENT);
    ft->blocktable.destroy();
    ft->cmp.destroy();
    toku_destroy_dbt(&ft->descriptor.dbt);
    toku_destroy_dbt(&ft->cmp_descriptor.dbt);
    toku_ft_destroy_reflock(ft);
    toku_free(ft->h);
}

// Make a copy of the header for the purpose of a checkpoint
// Not reentrant for a single FT.
// See ft_checkpoint for explanation of why
// FT lock must be held.
static void
ft_copy_for_checkpoint_unlocked(FT ft, LSN checkpoint_lsn) {
    assert(ft->h->type == FT_CURRENT);
    assert(ft->checkpoint_header == NULL);

    FT_HEADER XMEMDUP(ch, ft->h);
    ch->type = FT_CHECKPOINT_INPROGRESS; //Different type
    //printf("checkpoint_lsn=%" PRIu64 "\n", checkpoint_lsn.lsn);
    ch->checkpoint_lsn = checkpoint_lsn;

    //ch->blocktable is SHARED between the two headers
    ft->checkpoint_header = ch;
}

void
toku_ft_free (FT ft) {
    ft_destroy(ft);
    toku_free(ft);
}

void
toku_ft_init_reflock(FT ft) {
    toku_mutex_init(&ft->ft_ref_lock, NULL);
}

void
toku_ft_destroy_reflock(FT ft) {
    toku_mutex_destroy(&ft->ft_ref_lock);
}

void
toku_ft_grab_reflock(FT ft) {
    toku_mutex_lock(&ft->ft_ref_lock);
}

void
toku_ft_release_reflock(FT ft) {
    toku_mutex_unlock(&ft->ft_ref_lock);
}

/////////////////////////////////////////////////////////////////////////
// Start of Functions that are callbacks to the cachefule
//

// maps to cf->log_fassociate_during_checkpoint
static void
ft_log_fassociate_during_checkpoint (CACHEFILE cf, void *header_v) {
    FT ft = (FT) header_v;
    char* fname_in_env = toku_cachefile_fname_in_env(cf);
    BYTESTRING bs = { .len = (uint32_t) strlen(fname_in_env), // don't include the NUL
                      .data = fname_in_env };
    TOKULOGGER logger = toku_cachefile_logger(cf);
    FILENUM filenum = toku_cachefile_filenum(cf);
    bool unlink_on_close = toku_cachefile_is_unlink_on_close(cf);
    toku_log_fassociate(logger, NULL, 0, filenum, ft->h->flags, bs, unlink_on_close);
}

// Maps to cf->begin_checkpoint_userdata
// Create checkpoint-in-progress versions of header and translation (btt)
// Has access to fd (it is protected).
//
// Not reentrant for a single FT (see ft_checkpoint)
static void ft_begin_checkpoint (LSN checkpoint_lsn, void *header_v) {
    FT ft = (FT) header_v;
    // hold lock around copying and clearing of dirty bit
    toku_ft_lock (ft);
    assert(ft->h->type == FT_CURRENT);
    assert(ft->checkpoint_header == NULL);
    ft_copy_for_checkpoint_unlocked(ft, checkpoint_lsn);
    ft->h->dirty = 0;             // this is only place this bit is cleared        (in currentheader)
    ft->blocktable.note_start_checkpoint_unlocked();
    toku_ft_unlock (ft);
}

// #4922: Hack to remove data corruption race condition.
// Reading (and upgrading) a node up to version 19 causes this.
// We COULD skip this if we know that no nodes remained (as of last checkpoint)
// that are below version 19.
// If there are no nodes < version 19 this is harmless (field is unused).
// If there are, this will make certain the value is at least as low as necessary,
// and not much lower.  (Too low is good, too high can cause data corruption).
// TODO(yoni): If we ever stop supporting upgrades of nodes < version 19 we can delete this.
// TODO(yoni): If we know no nodes are left to upgrade, we can skip this. (Probably not worth doing).
static void
ft_hack_highest_unused_msn_for_upgrade_for_checkpoint(FT ft) {
    if (ft->h->layout_version_original < FT_LAYOUT_VERSION_19) {
        ft->checkpoint_header->highest_unused_msn_for_upgrade = ft->h->highest_unused_msn_for_upgrade;
    }
}

// maps to cf->checkpoint_userdata
// Write checkpoint-in-progress versions of header and translation to disk (really to OS internal buffer).
// Copy current header's version of checkpoint_staging stat64info to checkpoint header.
// Must have access to fd (protected).
// Requires: all pending bits are clear.  This implies that no thread will modify the checkpoint_staging
// version of the stat64info.
//
// No locks are taken for checkpoint_count/lsn because this is single threaded.  Can be called by:
//  - ft_close
//  - end_checkpoint
// checkpoints hold references to FTs and so they cannot be closed during a checkpoint.
// ft_close is not reentrant for a single FT
// end_checkpoint is not reentrant period
static void ft_checkpoint (CACHEFILE cf, int fd, void *header_v) {
    FT ft = (FT) header_v;
    FT_HEADER ch = ft->checkpoint_header;
    assert(ch);
    assert(ch->type == FT_CHECKPOINT_INPROGRESS);
    if (ch->dirty) {            // this is only place this bit is tested (in checkpoint_header)
        TOKULOGGER logger = toku_cachefile_logger(cf);
        if (logger) {
            toku_logger_fsync_if_lsn_not_fsynced(logger, ch->checkpoint_lsn);
        }
        uint64_t now = (uint64_t) time(NULL);
        ft->h->time_of_last_modification = now;
        ch->time_of_last_modification = now;
        ch->checkpoint_count++;
        ft_hack_highest_unused_msn_for_upgrade_for_checkpoint(ft);
                                                             
        // write translation and header to disk (or at least to OS internal buffer)
        toku_serialize_ft_to(fd, ch, &ft->blocktable, ft->cf);
        ch->dirty = 0;                      // this is only place this bit is cleared (in checkpoint_header)
        
        // fsync the cachefile
        toku_cachefile_fsync(cf);
        ft->h->checkpoint_count++;        // checkpoint succeeded, next checkpoint will save to alternate header location
        ft->h->checkpoint_lsn = ch->checkpoint_lsn;  //Header updated.
    } else {
        ft->blocktable.note_skipped_checkpoint();
    }
}

// maps to cf->end_checkpoint_userdata
// free unused disk space 
// (i.e. tell BlockAllocator to liberate blocks used by previous checkpoint).
// Must have access to fd (protected)
static void ft_end_checkpoint(CACHEFILE UU(cf), int fd, void *header_v) {
    FT ft = (FT) header_v;
    assert(ft->h->type == FT_CURRENT);
    ft->blocktable.note_end_checkpoint(fd);
    toku_free(ft->checkpoint_header);
    ft->checkpoint_header = nullptr;
}

// maps to cf->close_userdata
// Has access to fd (it is protected).
static void ft_close(CACHEFILE cachefile, int fd, void *header_v, bool oplsn_valid, LSN oplsn) {
    FT ft = (FT) header_v;
    assert(ft->h->type == FT_CURRENT);
    // We already have exclusive access to this field already, so skip the locking.
    // This should already never fail.
    invariant(!toku_ft_needed_unlocked(ft));
    assert(ft->cf == cachefile);
    TOKULOGGER logger = toku_cachefile_logger(cachefile);
    LSN lsn = ZERO_LSN;
    //Get LSN
    if (oplsn_valid) {
        //Use recovery-specified lsn
        lsn = oplsn;
        //Recovery cannot reduce lsn of a header.
        if (lsn.lsn < ft->h->checkpoint_lsn.lsn) {
            lsn = ft->h->checkpoint_lsn;
        }
    }
    else {
        //Get LSN from logger
        lsn = ZERO_LSN; // if there is no logger, we use zero for the lsn
        if (logger) {
            char* fname_in_env = toku_cachefile_fname_in_env(cachefile);
            assert(fname_in_env);
            BYTESTRING bs = {.len=(uint32_t) strlen(fname_in_env), .data=fname_in_env};
            toku_log_fclose(logger, &lsn, ft->h->dirty, bs, toku_cachefile_filenum(cachefile)); // flush the log on close (if new header is being written), otherwise it might not make it out.
        }
    }
    if (ft->h->dirty) {               // this is the only place this bit is tested (in currentheader)
        bool do_checkpoint = true;
        if (logger && logger->rollback_cachefile == cachefile) {
            do_checkpoint = false;
        }
        if (do_checkpoint) {
            ft_begin_checkpoint(lsn, header_v);
            ft_checkpoint(cachefile, fd, ft);
            ft_end_checkpoint(cachefile, fd, header_v);
            assert(!ft->h->dirty); // dirty bit should be cleared by begin_checkpoint and never set again (because we're closing the dictionary)
        }
    }
}

// maps to cf->free_userdata
static void ft_free(CACHEFILE cachefile UU(), void *header_v) {
    FT ft = (FT) header_v;
    toku_ft_free(ft);
}

// maps to cf->note_pin_by_checkpoint
//Must be protected by ydb lock.
//Is only called by checkpoint begin, which holds it
static void ft_note_pin_by_checkpoint (CACHEFILE UU(cachefile), void *header_v) {
    // Note: open_close lock is held by checkpoint begin
    FT ft = (FT) header_v;
    toku_ft_grab_reflock(ft);
    assert(!ft->pinned_by_checkpoint);
    assert(toku_ft_needed_unlocked(ft));
    ft->pinned_by_checkpoint = true;
    toku_ft_release_reflock(ft);
}

// Requires: the reflock is held.
static void unpin_by_checkpoint_callback(FT ft, void *extra) {
    invariant(extra == NULL);
    invariant(ft->pinned_by_checkpoint);
    ft->pinned_by_checkpoint = false;
}

// maps to cf->note_unpin_by_checkpoint
//Must be protected by ydb lock.
//Called by end_checkpoint, which grabs ydb lock around note_unpin
static void ft_note_unpin_by_checkpoint (CACHEFILE UU(cachefile), void *header_v) {
    FT ft = (FT) header_v;
    toku_ft_remove_reference(ft, false, ZERO_LSN, unpin_by_checkpoint_callback, NULL);
}

//
// End of Functions that are callbacks to the cachefile
/////////////////////////////////////////////////////////////////////////

static void setup_initial_ft_root_node(FT ft, BLOCKNUM blocknum) {
    FTNODE XCALLOC(node);
    toku_initialize_empty_ftnode(node, blocknum, 0, 1, ft->h->layout_version, ft->h->flags);
    BP_STATE(node,0) = PT_AVAIL;

    uint32_t fullhash = toku_cachetable_hash(ft->cf, blocknum);
    node->fullhash = fullhash;
    toku_cachetable_put(ft->cf, blocknum, fullhash,
                        node, make_ftnode_pair_attr(node),
                        get_write_callbacks_for_node(ft),
                        toku_ftnode_save_ct_pair);
    toku_unpin_ftnode(ft, node);
}

static void ft_init(FT ft, FT_OPTIONS options, CACHEFILE cf) {
    // fake, prevent unnecessary upgrade logic
    ft->layout_version_read_from_disk = FT_LAYOUT_VERSION;
    ft->checkpoint_header = NULL;

    toku_list_init(&ft->live_ft_handles);

    // intuitively, the comparator points to the FT's cmp descriptor
    ft->cmp.create(options->compare_fun, &ft->cmp_descriptor, options->memcmp_magic);
    ft->update_fun = options->update_fun;

    if (ft->cf != NULL) {
        assert(ft->cf == cf);
    }
    ft->cf = cf;
    ft->in_memory_stats = ZEROSTATS;

    setup_initial_ft_root_node(ft, ft->h->root_blocknum);
    toku_cachefile_set_userdata(ft->cf,
                                ft,
                                ft_log_fassociate_during_checkpoint,
                                ft_close,
                                ft_free,
                                ft_checkpoint,
                                ft_begin_checkpoint,
                                ft_end_checkpoint,
                                ft_note_pin_by_checkpoint,
                                ft_note_unpin_by_checkpoint);

    ft->blocktable.verify_no_free_blocknums();
}


static FT_HEADER
ft_header_create(FT_OPTIONS options, BLOCKNUM root_blocknum, TXNID root_xid_that_created)
{
    uint64_t now = (uint64_t) time(NULL);
    struct ft_header h = {
        .type = FT_CURRENT,
        .dirty = 0,
        .checkpoint_count = 0,
        .checkpoint_lsn = ZERO_LSN,
        .layout_version = FT_LAYOUT_VERSION,
        .layout_version_original = FT_LAYOUT_VERSION,
        .build_id = BUILD_ID,
        .build_id_original = BUILD_ID,
        .time_of_creation = now,
        .root_xid_that_created = root_xid_that_created,
        .time_of_last_modification = now,
        .time_of_last_verification = 0,
        .root_blocknum = root_blocknum,
        .flags = options->flags,
        .nodesize = options->nodesize,
        .basementnodesize = options->basementnodesize,
        .compression_method = options->compression_method,
        .fanout = options->fanout,
        .highest_unused_msn_for_upgrade = { .msn = (MIN_MSN.msn - 1) },
        .max_msn_in_ft = ZERO_MSN,
        .time_of_last_optimize_begin = 0,
        .time_of_last_optimize_end = 0,
        .count_of_optimize_in_progress = 0,
        .count_of_optimize_in_progress_read_from_disk = 0,
        .msn_at_start_of_last_completed_optimize = ZERO_MSN,
        .on_disk_stats = ZEROSTATS
    };
    return (FT_HEADER) toku_xmemdup(&h, sizeof h);
}

// allocate and initialize a fractal tree.
void toku_ft_create(FT *ftp, FT_OPTIONS options, CACHEFILE cf, TOKUTXN txn) {
    invariant(ftp);

    FT XCALLOC(ft);
    ft->h = ft_header_create(options, make_blocknum(0), (txn ? txn->txnid.parent_id64: TXNID_NONE));

    toku_ft_init_reflock(ft);

    // Assign blocknum for root block, also dirty the header
    ft->blocktable.create();
    ft->blocktable.allocate_blocknum(&ft->h->root_blocknum, ft);

    ft_init(ft, options, cf);

    *ftp = ft;
}

// TODO: (Zardosht) get rid of ft parameter
int toku_read_ft_and_store_in_cachefile (FT_HANDLE ft_handle, CACHEFILE cf, LSN max_acceptable_lsn, FT *header)
// If the cachefile already has the header, then just get it.
// If the cachefile has not been initialized, then don't modify anything.
// max_acceptable_lsn is the latest acceptable checkpointed version of the file.
{
    FT ft = nullptr;
    if ((ft = (FT) toku_cachefile_get_userdata(cf)) != nullptr) {
        *header = ft;
        assert(ft_handle->options.update_fun == ft->update_fun);
        return 0;
    }

    int fd = toku_cachefile_get_fd(cf);
    int r = toku_deserialize_ft_from(fd, max_acceptable_lsn, &ft);
    if (r == TOKUDB_BAD_CHECKSUM) {
        fprintf(stderr, "Checksum failure while reading header in file %s.\n", toku_cachefile_fname_in_env(cf));
        assert(false);  // make absolutely sure we crash before doing anything else
    } else if (r != 0) {
        return r;
    }

    invariant_notnull(ft);
    // intuitively, the comparator points to the FT's cmp descriptor
    ft->cmp.create(ft_handle->options.compare_fun, &ft->cmp_descriptor, ft_handle->options.memcmp_magic);
    ft->update_fun = ft_handle->options.update_fun;
    ft->cf = cf;
    toku_cachefile_set_userdata(cf,
                                reinterpret_cast<void *>(ft),
                                ft_log_fassociate_during_checkpoint,
                                ft_close,
                                ft_free,
                                ft_checkpoint,
                                ft_begin_checkpoint,
                                ft_end_checkpoint,
                                ft_note_pin_by_checkpoint,
                                ft_note_unpin_by_checkpoint);
    *header = ft;
    return 0;
}

void
toku_ft_note_ft_handle_open(FT ft, FT_HANDLE live) {
    toku_ft_grab_reflock(ft);
    live->ft = ft;
    toku_list_push(&ft->live_ft_handles, &live->live_ft_handle_link);
    toku_ft_release_reflock(ft);
}

// the reference count for a ft is the number of txn's that
// touched it plus the number of open handles plus one if
// pinned by a checkpoint.
static int
ft_get_reference_count(FT ft) {
    uint32_t pinned_by_checkpoint = ft->pinned_by_checkpoint ? 1 : 0;
    int num_handles = toku_list_num_elements_est(&ft->live_ft_handles);
    return pinned_by_checkpoint + ft->num_txns + num_handles;
}

// a ft is needed in memory iff its reference count is non-zero
bool
toku_ft_needed_unlocked(FT ft) {
    return ft_get_reference_count(ft) != 0;
}

// get the reference count and return true if it was 1
bool
toku_ft_has_one_reference_unlocked(FT ft) {
    return ft_get_reference_count(ft) == 1;
}

// evict a ft from memory by closing its cachefile. any future work
// will have to read in the ft in a new cachefile and new FT object.
void toku_ft_evict_from_memory(FT ft, bool oplsn_valid, LSN oplsn) {
    assert(ft->cf);
    toku_cachefile_close(&ft->cf, oplsn_valid, oplsn);
}

// Verifies there exists exactly one ft handle and returns it.
FT_HANDLE toku_ft_get_only_existing_ft_handle(FT ft) {
    FT_HANDLE ft_handle_ret = NULL;
    toku_ft_grab_reflock(ft);
    assert(toku_list_num_elements_est(&ft->live_ft_handles) == 1);
    ft_handle_ret = toku_list_struct(toku_list_head(&ft->live_ft_handles), struct ft_handle, live_ft_handle_link);
    toku_ft_release_reflock(ft);
    return ft_handle_ret;
}

// Purpose: set fields in ft_header to capture accountability info for start of HOT optimize.
// Note: HOT accountability variables in header are modified only while holding header lock.
//       (Header lock is really needed for touching the dirty bit, but it's useful and 
//       convenient here for keeping the HOT variables threadsafe.)
void
toku_ft_note_hot_begin(FT_HANDLE ft_handle) {
    FT ft = ft_handle->ft;
    time_t now = time(NULL);

    // hold lock around setting and clearing of dirty bit
    // (see cooperative use of dirty bit in ft_begin_checkpoint())
    toku_ft_lock(ft);
    ft->h->time_of_last_optimize_begin = now;
    ft->h->count_of_optimize_in_progress++;
    ft->h->dirty = 1;
    toku_ft_unlock(ft);
}


// Purpose: set fields in ft_header to capture accountability info for end of HOT optimize.
// Note: See note for toku_ft_note_hot_begin().
void
toku_ft_note_hot_complete(FT_HANDLE ft_handle, bool success, MSN msn_at_start_of_hot) {
    FT ft = ft_handle->ft;
    time_t now = time(NULL);

    toku_ft_lock(ft);
    ft->h->count_of_optimize_in_progress--;
    if (success) {
        ft->h->time_of_last_optimize_end = now;
        ft->h->msn_at_start_of_last_completed_optimize = msn_at_start_of_hot;
        // If we just successfully completed an optimization and no other thread is performing
        // an optimization, then the number of optimizations in progress is zero.
        // If there was a crash during a HOT optimization, this is how count_of_optimize_in_progress
        // would be reset to zero on the disk after recovery from that crash.  
        if (ft->h->count_of_optimize_in_progress == ft->h->count_of_optimize_in_progress_read_from_disk)
            ft->h->count_of_optimize_in_progress = 0;
    }
    ft->h->dirty = 1;
    toku_ft_unlock(ft);
}


void
toku_ft_init(FT ft,
             BLOCKNUM root_blocknum_on_disk,
             LSN checkpoint_lsn,
             TXNID root_xid_that_created,
             uint32_t target_nodesize,
             uint32_t target_basementnodesize,
             enum toku_compression_method compression_method,
             uint32_t fanout
             )
{
    memset(ft, 0, sizeof *ft);
    struct ft_options options = {
        .nodesize = target_nodesize,
        .basementnodesize = target_basementnodesize,
        .compression_method = compression_method,
        .fanout = fanout,
        .flags = 0,
        .memcmp_magic = 0,
        .compare_fun = NULL,
        .update_fun = NULL
    };
    ft->h = ft_header_create(&options, root_blocknum_on_disk, root_xid_that_created);
    ft->h->checkpoint_count = 1;
    ft->h->checkpoint_lsn   = checkpoint_lsn;
}

// Open an ft for use by redirect.  The new ft must have the same dict_id as the old_ft passed in.  (FILENUM is assigned by the ft_handle_open() function.)
static int
ft_handle_open_for_redirect(FT_HANDLE *new_ftp, const char *fname_in_env, TOKUTXN txn, FT old_ft) {
    FT_HANDLE ft_handle;
    assert(old_ft->dict_id.dictid != DICTIONARY_ID_NONE.dictid);
    toku_ft_handle_create(&ft_handle);
    toku_ft_set_bt_compare(ft_handle, old_ft->cmp.get_compare_func());
    toku_ft_set_update(ft_handle, old_ft->update_fun);
    toku_ft_handle_set_nodesize(ft_handle, old_ft->h->nodesize);
    toku_ft_handle_set_basementnodesize(ft_handle, old_ft->h->basementnodesize);
    toku_ft_handle_set_compression_method(ft_handle, old_ft->h->compression_method);
    toku_ft_handle_set_fanout(ft_handle, old_ft->h->fanout);
    CACHETABLE ct = toku_cachefile_get_cachetable(old_ft->cf);
    int r = toku_ft_handle_open_with_dict_id(ft_handle, fname_in_env, 0, 0, ct, txn, old_ft->dict_id);
    if (r != 0) {
        goto cleanup;
    }
    assert(ft_handle->ft->dict_id.dictid == old_ft->dict_id.dictid);
    *new_ftp = ft_handle;

 cleanup:
    if (r != 0) {
        toku_ft_handle_close(ft_handle);
    }
    return r;
}

// This function performs most of the work to redirect a dictionary to different file.
// It is called for redirect and to abort a redirect.  (This function is almost its own inverse.)
static int
dictionary_redirect_internal(const char *dst_fname_in_env, FT src_ft, TOKUTXN txn, FT *dst_ftp) {
    int r;

    FILENUM src_filenum = toku_cachefile_filenum(src_ft->cf);
    FILENUM dst_filenum = FILENUM_NONE;

    FT dst_ft = NULL;
    struct toku_list *list;
    // open a dummy ft based off of 
    // dst_fname_in_env to get the header
    // then we will change all the ft's to have
    // their headers point to dst_ft instead of src_ft
    FT_HANDLE tmp_dst_ft = NULL;
    r = ft_handle_open_for_redirect(&tmp_dst_ft, dst_fname_in_env, txn, src_ft);
    if (r != 0) {
        goto cleanup;
    }
    dst_ft = tmp_dst_ft->ft;

    // some sanity checks on dst_filenum
    dst_filenum = toku_cachefile_filenum(dst_ft->cf);
    assert(dst_filenum.fileid!=FILENUM_NONE.fileid);
    assert(dst_filenum.fileid!=src_filenum.fileid); //Cannot be same file.

    // for each live ft_handle, ft_handle->ft is currently src_ft
    // we want to change it to dummy_dst
    toku_ft_grab_reflock(src_ft);
    while (!toku_list_empty(&src_ft->live_ft_handles)) {
        list = src_ft->live_ft_handles.next;
        FT_HANDLE src_handle = NULL;
        src_handle = toku_list_struct(list, struct ft_handle, live_ft_handle_link);

        toku_list_remove(&src_handle->live_ft_handle_link);

        toku_ft_note_ft_handle_open(dst_ft, src_handle);
        if (src_handle->redirect_callback) {
            src_handle->redirect_callback(src_handle, src_handle->redirect_callback_extra);
        }
    }
    assert(dst_ft);
    // making sure that we are not leaking src_ft
    assert(toku_ft_needed_unlocked(src_ft));
    toku_ft_release_reflock(src_ft);

    toku_ft_handle_close(tmp_dst_ft);

    *dst_ftp = dst_ft;
cleanup:
    return r;
}



//This is the 'abort redirect' function.  The redirect of old_ft to new_ft was done
//and now must be undone, so here we redirect new_ft back to old_ft.
int
toku_dictionary_redirect_abort(FT old_ft, FT new_ft, TOKUTXN txn) {
    char *old_fname_in_env = toku_cachefile_fname_in_env(old_ft->cf);
    int r;
    {
        FILENUM old_filenum = toku_cachefile_filenum(old_ft->cf);
        FILENUM new_filenum = toku_cachefile_filenum(new_ft->cf);
        assert(old_filenum.fileid!=new_filenum.fileid); //Cannot be same file.

        //No living fts in old header.
        toku_ft_grab_reflock(old_ft);
        assert(toku_list_empty(&old_ft->live_ft_handles));
        toku_ft_release_reflock(old_ft);
    }

    FT dst_ft;
    // redirect back from new_ft to old_ft
    r = dictionary_redirect_internal(old_fname_in_env, new_ft, txn, &dst_ft);
    if (r == 0) {
        assert(dst_ft == old_ft);
    }
    return r;
}

/****
 * on redirect or abort:
 *  if redirect txn_note_doing_work(txn)
 *  if redirect connect src ft to txn (txn modified this ft)
 *  for each src ft
 *    open ft to dst file (create new ft struct)
 *    if redirect connect dst ft to txn 
 *    redirect db to new ft
 *    redirect cursors to new ft
 *  close all src fts
 *  if redirect make rollback log entry
 * 
 * on commit:
 *   nothing to do
 *
 *****/

int 
toku_dictionary_redirect (const char *dst_fname_in_env, FT_HANDLE old_ft_h, TOKUTXN txn) {
// Input args:
//   new file name for dictionary (relative to env)
//   old_ft_h is a live ft of open handle ({DB, FT_HANDLE} pair) that currently refers to old dictionary file.
//   (old_ft_h may be one of many handles to the dictionary.)
//   txn that created the loader
// Requires: 
//   multi operation lock is held.
//   The ft is open.  (which implies there can be no zombies.)
//   The new file must be a valid dictionary.
//   The block size and flags in the new file must match the existing FT.
//   The new file must already have its descriptor in it (and it must match the existing descriptor).
// Effect:   
//   Open new FTs (and related header and cachefile) to the new dictionary file with a new FILENUM.
//   Redirect all DBs that point to fts that point to the old file to point to fts that point to the new file.
//   Copy the dictionary id (dict_id) from the header of the original file to the header of the new file.
//   Create a rollback log entry.
//   The original FT, header, cachefile and file remain unchanged.  They will be cleaned up on commmit.
//   If the txn aborts, then this operation will be undone
    int r;

    FT old_ft = old_ft_h->ft;

    // dst file should not be open.  (implies that dst and src are different because src must be open.)
    {
        CACHETABLE ct = toku_cachefile_get_cachetable(old_ft->cf);
        CACHEFILE cf;
        r = toku_cachefile_of_iname_in_env(ct, dst_fname_in_env, &cf);
        if (r==0) {
            r = EINVAL;
            goto cleanup;
        }
        assert(r==ENOENT);
        r = 0;
    }

    if (txn) {
        toku_txn_maybe_note_ft(txn, old_ft);  // mark old ft as touched by this txn
    }

    FT new_ft;
    r = dictionary_redirect_internal(dst_fname_in_env, old_ft, txn, &new_ft);
    if (r != 0) {
        goto cleanup;
    }

    // make rollback log entry
    if (txn) {
        toku_txn_maybe_note_ft(txn, new_ft); // mark new ft as touched by this txn

        // There is no recovery log entry for redirect,
        // and rollback log entries are not allowed for read-only transactions.
        // Normally the recovery log entry would ensure the begin was logged.
        if (!txn->begin_was_logged) {
          toku_maybe_log_begin_txn_for_write_operation(txn);
        }
        FILENUM old_filenum = toku_cachefile_filenum(old_ft->cf);
        FILENUM new_filenum = toku_cachefile_filenum(new_ft->cf);
        toku_logger_save_rollback_dictionary_redirect(txn, old_filenum, new_filenum);
    }

cleanup:
    return r;
}

// Insert reference to transaction into ft
void
toku_ft_add_txn_ref(FT ft) {
    toku_ft_grab_reflock(ft);
    ++ft->num_txns;
    toku_ft_release_reflock(ft);
}

static void
remove_txn_ref_callback(FT ft, void *UU(context)) {
    invariant(ft->num_txns > 0);
    --ft->num_txns;
}

void
toku_ft_remove_txn_ref(FT ft) {
    toku_ft_remove_reference(ft, false, ZERO_LSN, remove_txn_ref_callback, NULL);
}

void toku_calculate_root_offset_pointer (
    FT ft, 
    CACHEKEY* root_key, 
    uint32_t *roothash
    ) 
{
    *roothash = toku_cachetable_hash(ft->cf, ft->h->root_blocknum);
    *root_key = ft->h->root_blocknum;
}

void toku_ft_set_new_root_blocknum(
    FT ft, 
    CACHEKEY new_root_key
    ) 
{
    ft->h->root_blocknum = new_root_key;
}

LSN toku_ft_checkpoint_lsn(FT ft) {
    return ft->h->checkpoint_lsn;
}

void 
toku_ft_stat64 (FT ft, struct ftstat64_s *s) {
    s->fsize = toku_cachefile_size(ft->cf);
    // just use the in memory stats from the header
    // prevent appearance of negative numbers for numrows, numbytes
    int64_t n = ft->in_memory_stats.numrows;
    if (n < 0) {
        n = 0;
    }
    s->nkeys = s->ndata = n;
    n = ft->in_memory_stats.numbytes;
    if (n < 0) {
        n = 0;
    }
    s->dsize = n; 
    s->create_time_sec = ft->h->time_of_creation;
    s->modify_time_sec = ft->h->time_of_last_modification;
    s->verify_time_sec = ft->h->time_of_last_verification;    
}

void toku_ft_get_fractal_tree_info64(FT ft, struct ftinfo64 *info) {
    ft->blocktable.get_info64(info);
}

int toku_ft_iterate_fractal_tree_block_map(FT ft, int (*iter)(uint64_t,int64_t,int64_t,int64_t,int64_t,void*), void *iter_extra) {
    uint64_t this_checkpoint_count = ft->h->checkpoint_count;
    return ft->blocktable.iterate_translation_tables(this_checkpoint_count, iter, iter_extra);
}

void 
toku_ft_update_descriptor(FT ft, DESCRIPTOR desc) 
// Effect: Changes the descriptor in a tree (log the change, make sure it makes it to disk eventually).
// requires: the ft is fully user-opened with a valid cachefile.
//           descriptor updates cannot happen in parallel for an FT 
//           (ydb layer uses a row lock to enforce this)
{
    assert(ft->cf);
    int fd = toku_cachefile_get_fd(ft->cf);
    toku_ft_update_descriptor_with_fd(ft, desc, fd);
}

// upadate the descriptor for an ft and serialize it using
// the given descriptor instead of reading the descriptor
// from the ft's cachefile. we do this so serialize code can
// update a descriptor before the ft is fully opened and has
// a valid cachefile.
void
toku_ft_update_descriptor_with_fd(FT ft, DESCRIPTOR desc, int fd) {
    // the checksum is four bytes, so that's where the magic number comes from
    // make space for the new descriptor and write it out to disk
    DISKOFF offset, size;
    size = toku_serialize_descriptor_size(desc) + 4;
    ft->blocktable.realloc_descriptor_on_disk(size, &offset, ft, fd);
    toku_serialize_descriptor_contents_to_fd(fd, desc, offset);

    // cleanup the old descriptor and set the in-memory descriptor to the new one
    toku_destroy_dbt(&ft->descriptor.dbt);
    toku_clone_dbt(&ft->descriptor.dbt, desc->dbt);
}

void toku_ft_update_cmp_descriptor(FT ft) {
    // cleanup the old cmp descriptor and clone it as the in-memory descriptor
    toku_destroy_dbt(&ft->cmp_descriptor.dbt);
    toku_clone_dbt(&ft->cmp_descriptor.dbt, ft->descriptor.dbt);
}

DESCRIPTOR toku_ft_get_descriptor(FT_HANDLE ft_handle) {
    return &ft_handle->ft->descriptor;
}

DESCRIPTOR toku_ft_get_cmp_descriptor(FT_HANDLE ft_handle) {
    return &ft_handle->ft->cmp_descriptor;
}

void
toku_ft_update_stats(STAT64INFO headerstats, STAT64INFO_S delta) {
    (void) toku_sync_fetch_and_add(&(headerstats->numrows),  delta.numrows);
    (void) toku_sync_fetch_and_add(&(headerstats->numbytes), delta.numbytes);
}

void
toku_ft_decrease_stats(STAT64INFO headerstats, STAT64INFO_S delta) {
    (void) toku_sync_fetch_and_sub(&(headerstats->numrows),  delta.numrows);
    (void) toku_sync_fetch_and_sub(&(headerstats->numbytes), delta.numbytes);
}

void
toku_ft_remove_reference(FT ft, bool oplsn_valid, LSN oplsn, remove_ft_ref_callback remove_ref, void *extra) {
    toku_ft_grab_reflock(ft);
    if (toku_ft_has_one_reference_unlocked(ft)) {
        toku_ft_release_reflock(ft);

        toku_ft_open_close_lock();
        toku_ft_grab_reflock(ft);

        remove_ref(ft, extra);
        bool needed = toku_ft_needed_unlocked(ft);
        toku_ft_release_reflock(ft);

        // if we're running during recovery, we must close the underlying ft.
        // we know we're running in recovery if we were passed a valid lsn.
        if (oplsn_valid) {
            assert(!needed);
        }
        if (!needed) {
            // close header
            toku_ft_evict_from_memory(ft, oplsn_valid, oplsn);
        }
    
        toku_ft_open_close_unlock();
    }
    else {
        remove_ref(ft, extra);
        toku_ft_release_reflock(ft);
    }
}

void toku_ft_set_nodesize(FT ft, unsigned int nodesize) {
    toku_ft_lock(ft);
    ft->h->nodesize = nodesize;
    ft->h->dirty = 1;
    toku_ft_unlock(ft);
}

void toku_ft_get_nodesize(FT ft, unsigned int *nodesize) {
    toku_ft_lock(ft);
    *nodesize = ft->h->nodesize;
    toku_ft_unlock(ft);
}

void toku_ft_set_basementnodesize(FT ft, unsigned int basementnodesize) {
    toku_ft_lock(ft);
    ft->h->basementnodesize = basementnodesize;
    ft->h->dirty = 1;
    toku_ft_unlock(ft);
}

void toku_ft_get_basementnodesize(FT ft, unsigned int *basementnodesize) {
    toku_ft_lock(ft);
    *basementnodesize = ft->h->basementnodesize;
    toku_ft_unlock(ft);
}

void toku_ft_set_compression_method(FT ft, enum toku_compression_method method) {
    toku_ft_lock(ft);
    ft->h->compression_method = method;
    ft->h->dirty = 1;
    toku_ft_unlock(ft);
}

void toku_ft_get_compression_method(FT ft, enum toku_compression_method *methodp) {
    toku_ft_lock(ft);
    *methodp = ft->h->compression_method;
    toku_ft_unlock(ft);
}

void toku_ft_set_fanout(FT ft, unsigned int fanout) {
    toku_ft_lock(ft);
    ft->h->fanout = fanout;
    ft->h->dirty = 1;
    toku_ft_unlock(ft);
}

void toku_ft_get_fanout(FT ft, unsigned int *fanout) {
    toku_ft_lock(ft);
    *fanout = ft->h->fanout;
    toku_ft_unlock(ft);
}

// mark the ft as a blackhole. any message injections will be a no op.
void toku_ft_set_blackhole(FT_HANDLE ft_handle) {
    ft_handle->ft->blackhole = true;
}

struct garbage_helper_extra {
    FT ft;
    size_t total_space;
    size_t used_space;
};

static int
garbage_leafentry_helper(const void* key UU(), const uint32_t keylen, const LEAFENTRY & le, uint32_t UU(idx), struct garbage_helper_extra * const info) {
    //TODO #warning need to reanalyze for split
    info->total_space += leafentry_disksize(le) + keylen + sizeof(keylen);
    if (!le_latest_is_del(le)) {
        info->used_space += LE_CLEAN_MEMSIZE(le_latest_vallen(le)) + keylen + sizeof(keylen);
    }
    return 0;
}

static int
garbage_helper(BLOCKNUM blocknum, int64_t UU(size), int64_t UU(address), void *extra) {
    struct garbage_helper_extra *CAST_FROM_VOIDP(info, extra);
    FTNODE node;
    FTNODE_DISK_DATA ndd;
    ftnode_fetch_extra bfe;
    bfe.create_for_full_read(info->ft);
    int fd = toku_cachefile_get_fd(info->ft->cf);
    int r = toku_deserialize_ftnode_from(fd, blocknum, 0, &node, &ndd, &bfe);
    if (r != 0) {
        goto no_node;
    }
    if (node->height > 0) {
        goto exit;
    }
    for (int i = 0; i < node->n_children; ++i) {
        bn_data* bd = BLB_DATA(node, i);
        r = bd->iterate<struct garbage_helper_extra, garbage_leafentry_helper>(info);
        if (r != 0) {
            goto exit;
        }
    }
exit:
    toku_ftnode_free(&node);
    toku_free(ndd);
no_node:
    return r;
}

void toku_ft_get_garbage(FT ft, uint64_t *total_space, uint64_t *used_space) {
// Effect: Iterates the FT's blocktable and calculates the total and used space for leaf blocks.
// Note: It is ok to call this function concurrently with reads/writes to the table since
//       the blocktable lock is held, which means no new allocations or file writes can occur.
    invariant_notnull(total_space);
    invariant_notnull(used_space);
    struct garbage_helper_extra info = {
        .ft = ft,
        .total_space = 0,
        .used_space = 0
    };
    ft->blocktable.iterate(block_table::TRANSLATION_CHECKPOINTED, garbage_helper, &info, true, true);
    *total_space = info.total_space;
    *used_space = info.used_space;
}


#if !defined(TOKUDB_REVISION)
#error
#endif

#define xstr(X) str(X)
#define str(X) #X
#define static_version_string xstr(DB_VERSION_MAJOR) "." \
                              xstr(DB_VERSION_MINOR) "." \
                              xstr(DB_VERSION_PATCH) " build " \
                              xstr(TOKUDB_REVISION)
struct toku_product_name_strings_struct toku_product_name_strings;

char toku_product_name[TOKU_MAX_PRODUCT_NAME_LENGTH];
void tokuft_update_product_name_strings(void) {
    // DO ALL STRINGS HERE.. maybe have a separate FT layer version as well
    {
        int n = snprintf(toku_product_name_strings.db_version,
                         sizeof(toku_product_name_strings.db_version),
                         "%s %s", toku_product_name, static_version_string);
        assert(n >= 0);
        assert((unsigned)n < sizeof(toku_product_name_strings.db_version));
    }
    {
        int n = snprintf(toku_product_name_strings.fileopsdirectory,
                         sizeof(toku_product_name_strings.fileopsdirectory),
                         "%s.directory", toku_product_name);
        assert(n >= 0);
        assert((unsigned)n < sizeof(toku_product_name_strings.fileopsdirectory));
    }
    {
        int n = snprintf(toku_product_name_strings.environmentdictionary,
                         sizeof(toku_product_name_strings.environmentdictionary),
                         "%s.environment", toku_product_name);
        assert(n >= 0);
        assert((unsigned)n < sizeof(toku_product_name_strings.environmentdictionary));
    }
    {
        int n = snprintf(toku_product_name_strings.rollback_cachefile,
                         sizeof(toku_product_name_strings.rollback_cachefile),
                         "%s.rollback", toku_product_name);
        assert(n >= 0);
        assert((unsigned)n < sizeof(toku_product_name_strings.rollback_cachefile));
    }
    {
        int n = snprintf(toku_product_name_strings.single_process_lock,
                         sizeof(toku_product_name_strings.single_process_lock),
                         "__%s_lock_dont_delete_me", toku_product_name);
        assert(n >= 0);
        assert((unsigned)n < sizeof(toku_product_name_strings.single_process_lock));
    }
}
#undef xstr
#undef str

int
toku_single_process_lock(const char *lock_dir, const char *which, int *lockfd) {
    if (!lock_dir)
        return ENOENT;
    int namelen=strlen(lock_dir)+strlen(which);
    char lockfname[namelen+sizeof("/_") + strlen(toku_product_name_strings.single_process_lock)];

    int l = snprintf(lockfname, sizeof(lockfname), "%s/%s_%s",
                     lock_dir, toku_product_name_strings.single_process_lock, which);
    assert(l+1 == (signed)(sizeof(lockfname)));
    *lockfd = toku_os_lock_file(lockfname);
    if (*lockfd < 0) {
        int e = get_error_errno();
        fprintf(stderr, "Couldn't start tokuft because some other tokuft process is using the same directory [%s] for [%s]\n", lock_dir, which);
        return e;
    }
    return 0;
}

int
toku_single_process_unlock(int *lockfd) {
    int fd = *lockfd;
    *lockfd = -1;
    if (fd>=0) {
        int r = toku_os_unlock_file(fd);
        if (r != 0)
            return get_error_errno();
    }
    return 0;
}

int tokuft_num_envs = 0;
int
db_env_set_toku_product_name(const char *name) {
    if (tokuft_num_envs > 0) {
        return EINVAL;
    }
    if (!name || strlen(name) < 1) {
        return EINVAL;
    }
    if (strlen(name) >= sizeof(toku_product_name)) {
        return ENAMETOOLONG;
    }
    if (strncmp(toku_product_name, name, sizeof(toku_product_name))) {
        strcpy(toku_product_name, name);
        tokuft_update_product_name_strings();
    }
    return 0;
}

