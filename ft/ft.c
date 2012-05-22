/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: ft-ops.c 43396 2012-05-11 17:24:47Z zardosht $"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."


#include "includes.h"
#include <ft-cachetable-wrappers.h>

void
toku_ft_suppress_rollbacks(FT h, TOKUTXN txn) {
    TXNID txnid = toku_txn_get_txnid(txn);
    assert(h->txnid_that_created_or_locked_when_empty == TXNID_NONE ||
           h->txnid_that_created_or_locked_when_empty == txnid);
    h->txnid_that_created_or_locked_when_empty = txnid;
    TXNID rootid = toku_txn_get_root_txnid(txn);
    assert(h->root_that_created_or_locked_when_empty == TXNID_NONE ||
           h->root_that_created_or_locked_when_empty == rootid);
    h->root_that_created_or_locked_when_empty  = rootid;
}

void 
toku_reset_root_xid_that_created(FT h, TXNID new_root_xid_that_created) {
    // Reset the root_xid_that_created field to the given value.  
    // This redefines which xid created the dictionary.

    // hold lock around setting and clearing of dirty bit
    // (see cooperative use of dirty bit in ft_begin_checkpoint())
    toku_ft_lock (h);
    h->root_xid_that_created = new_root_xid_that_created;
    h->dirty = 1;
    toku_ft_unlock (h);
}

static void
ft_destroy(FT h) {
    if (!h->panic) assert(!h->checkpoint_header);

    //header and checkpoint_header have same Blocktable pointer
    //cannot destroy since it is still in use by CURRENT
    if (h->type == FT_CHECKPOINT_INPROGRESS) h->blocktable = NULL; 
    else {
        assert(h->type == FT_CURRENT);
        toku_blocktable_destroy(&h->blocktable);
        if (h->descriptor.dbt.data) toku_free(h->descriptor.dbt.data);
        if (h->cmp_descriptor.dbt.data) toku_free(h->cmp_descriptor.dbt.data);
        toku_ft_destroy_treelock(h);
        toku_omt_destroy(&h->txns);
    }
}

// Make a copy of the header for the purpose of a checkpoint
static void
ft_copy_for_checkpoint(FT h, LSN checkpoint_lsn) {
    assert(h->type == FT_CURRENT);
    assert(h->checkpoint_header == NULL);
    assert(h->panic==0);

    FT XMALLOC(ch);
    *ch = *h; //Do a shallow copy
    ch->type = FT_CHECKPOINT_INPROGRESS; //Different type
    //printf("checkpoint_lsn=%" PRIu64 "\n", checkpoint_lsn.lsn);
    ch->checkpoint_lsn = checkpoint_lsn;
    ch->panic_string = NULL;

    //ch->blocktable is SHARED between the two headers
    h->checkpoint_header = ch;
}

static void
ft_free(FT h) {
    ft_destroy(h);
    toku_free(h);
}

void
toku_ft_free (FT h) {
    ft_free(h);
}

void
toku_ft_init_treelock(FT h) {
    toku_mutex_init(&h->tree_lock, NULL);
}

void
toku_ft_destroy_treelock(FT h) {
    toku_mutex_destroy(&h->tree_lock);
}

void
toku_ft_grab_treelock(FT h) {
    toku_mutex_lock(&h->tree_lock);
}

void
toku_ft_release_treelock(FT h) {
    toku_mutex_unlock(&h->tree_lock);
}

/////////////////////////////////////////////////////////////////////////
// Start of Functions that are callbacks to the cachefule
//

// maps to cf->log_fassociate_during_checkpoint
static int
ft_log_fassociate_during_checkpoint (CACHEFILE cf, void *header_v) {
    FT h = header_v;
    char* fname_in_env = toku_cachefile_fname_in_env(cf);
    BYTESTRING bs = { strlen(fname_in_env), // don't include the NUL
                      fname_in_env };
    TOKULOGGER logger = toku_cachefile_logger(cf);
    FILENUM filenum = toku_cachefile_filenum (cf);
    int r = toku_log_fassociate(logger, NULL, 0, filenum, h->flags, bs);
    return r;
}

// maps to cf->log_suppress_rollback_during_checkpoint
static int
ft_log_suppress_rollback_during_checkpoint (CACHEFILE cf, void *header_v) {
    int r = 0;
    FT h = header_v;
    TXNID xid = h->txnid_that_created_or_locked_when_empty;
    if (xid != TXNID_NONE) {
        //Only log if useful.
        TOKULOGGER logger = toku_cachefile_logger(cf);
        FILENUM filenum = toku_cachefile_filenum (cf);
        r = toku_log_suppress_rollback(logger, NULL, 0, filenum, xid);
    }
    return r;
}

// Maps to cf->begin_checkpoint_userdata
// Create checkpoint-in-progress versions of header and translation (btt) (and fifo for now...).
//Has access to fd (it is protected).
static int
ft_begin_checkpoint (LSN checkpoint_lsn, void *header_v) {
    FT h = header_v;
    int r = h->panic;
    if (r==0) {
        // hold lock around copying and clearing of dirty bit
        toku_ft_lock (h);
        assert(h->type == FT_CURRENT);
        assert(h->checkpoint_header == NULL);
        ft_copy_for_checkpoint(h, checkpoint_lsn);
        h->dirty = 0;             // this is only place this bit is cleared        (in currentheader)
        // on_disk_stats includes on disk changes since last checkpoint,
        // so checkpoint_staging_stats now includes changes for checkpoint in progress.
        h->checkpoint_staging_stats = h->on_disk_stats;
        toku_block_translation_note_start_checkpoint_unlocked(h->blocktable);
        toku_ft_unlock (h);
    }
    return r;
}

// maps to cf->checkpoint_userdata
// Write checkpoint-in-progress versions of header and translation to disk (really to OS internal buffer).
// Copy current header's version of checkpoint_staging stat64info to checkpoint header.
// Must have access to fd (protected).
// Requires: all pending bits are clear.  This implies that no thread will modify the checkpoint_staging
// version of the stat64info.
static int
ft_checkpoint (CACHEFILE cf, int fd, void *header_v) {
    FT h = header_v;
    FT ch = h->checkpoint_header;
    int r = 0;
    if (h->panic!=0) goto handle_error;
    //printf("%s:%d allocated_limit=%lu writing queue to %lu\n", __FILE__, __LINE__,
    //             block_allocator_allocated_limit(h->block_allocator), h->unused_blocks.b*h->nodesize);
    assert(ch);
    if (ch->panic!=0) goto handle_error;
    assert(ch->type == FT_CHECKPOINT_INPROGRESS);
    if (ch->dirty) {            // this is only place this bit is tested (in checkpoint_header)
        TOKULOGGER logger = toku_cachefile_logger(cf);
        if (logger) {
            r = toku_logger_fsync_if_lsn_not_fsynced(logger, ch->checkpoint_lsn);
            if (r!=0) goto handle_error;
        }
        uint64_t now = (uint64_t) time(NULL); // 4018;
        h->time_of_last_modification = now;
        ch->time_of_last_modification = now;
        ch->checkpoint_count++;
        // Threadsafety of checkpoint_staging_stats here depends on there being no pending bits set,
        // so that all callers to flush callback should have the for_checkpoint argument false,
        // and therefore will not modify the checkpoint_staging_stats.
        // TODO 4184: If the flush callback is called with the for_checkpoint argument true even when all the pending bits
        //            are clear, then this is a problem.  
        ch->checkpoint_staging_stats = h->checkpoint_staging_stats;  
        // The in_memory_stats and on_disk_stats in the checkpoint header should be ignored, but we set them 
        // here just in case the serializer looks in the wrong place.
        ch->in_memory_stats = ch->checkpoint_staging_stats;  
        ch->on_disk_stats   = ch->checkpoint_staging_stats;  
                                                             
        // write translation and header to disk (or at least to OS internal buffer)
        r = toku_serialize_ft_to(fd, ch);
        if (r!=0) goto handle_error;
        ch->dirty = 0;                      // this is only place this bit is cleared (in checkpoint_header)
        
        // fsync the cachefile
        r = toku_cachefile_fsync(cf);
        if (r!=0) {
            goto handle_error;
        }
        h->checkpoint_count++;        // checkpoint succeeded, next checkpoint will save to alternate header location
        h->checkpoint_lsn = ch->checkpoint_lsn;  //Header updated.
    } 
    else {
        toku_block_translation_note_skipped_checkpoint(ch->blocktable);
    }
    if (0) {
handle_error:
        if (h->panic) r = h->panic;
        else if (ch->panic) {
            r = ch->panic;
            //Steal panic string.  Cannot afford to malloc.
            h->panic             = ch->panic;
            h->panic_string  = ch->panic_string;
        }
        else toku_block_translation_note_failed_checkpoint(ch->blocktable);
    }
    return r;

}

// maps to cf->end_checkpoint_userdata
// free unused disk space 
// (i.e. tell BlockAllocator to liberate blocks used by previous checkpoint).
// Must have access to fd (protected)
static int
ft_end_checkpoint (CACHEFILE UU(cachefile), int fd, void *header_v) {
    FT h = header_v;
    int r = h->panic;
    if (r==0) {
        assert(h->type == FT_CURRENT);
        toku_block_translation_note_end_checkpoint(h->blocktable, fd, h);
    }
    if (h->checkpoint_header) {         // could be NULL only if panic was true at begin_checkpoint
        ft_free(h->checkpoint_header);
        h->checkpoint_header = NULL;
    }
    return r;
}

// maps to cf->close_userdata
// Has access to fd (it is protected).
static int
ft_close (CACHEFILE cachefile, int fd, void *header_v, char **malloced_error_string, BOOL oplsn_valid, LSN oplsn) {
    FT h = header_v;
    assert(h->type == FT_CURRENT);
    toku_ft_lock(h);
    assert(!toku_ft_needed(h));
    toku_ft_unlock(h);
    int r = 0;
    if (h->panic) {
        r = h->panic;
    } else {
        assert(h->cf == cachefile);
        TOKULOGGER logger = toku_cachefile_logger(cachefile);
        LSN lsn = ZERO_LSN;
        //Get LSN
        if (oplsn_valid) {
            //Use recovery-specified lsn
            lsn = oplsn;
            //Recovery cannot reduce lsn of a header.
            if (lsn.lsn < h->checkpoint_lsn.lsn)
                lsn = h->checkpoint_lsn;
        }
        else {
            //Get LSN from logger
            lsn = ZERO_LSN; // if there is no logger, we use zero for the lsn
            if (logger) {
                char* fname_in_env = toku_cachefile_fname_in_env(cachefile);
                assert(fname_in_env);
                BYTESTRING bs = {.len=strlen(fname_in_env), .data=fname_in_env};
                r = toku_log_fclose(logger, &lsn, h->dirty, bs, toku_cachefile_filenum(cachefile)); // flush the log on close (if new header is being written), otherwise it might not make it out.
                if (r!=0) return r;
            }
        }
        if (h->dirty) {               // this is the only place this bit is tested (in currentheader)
            if (logger) { //Rollback cachefile MUST NOT BE CLOSED DIRTY
                          //It can be checkpointed only via 'checkpoint'
                assert(logger->rollback_cachefile != cachefile);
            }
            int r2;
            //assert(lsn.lsn!=0);
            r2 = ft_begin_checkpoint(lsn, header_v);
            if (r==0) r = r2;
            r2 = ft_checkpoint(cachefile, fd, h);
            if (r==0) r = r2;
            r2 = ft_end_checkpoint(cachefile, fd, header_v);
            if (r==0) r = r2;
            if (!h->panic) assert(!h->dirty);             // dirty bit should be cleared by begin_checkpoint and never set again (because we're closing the dictionary)
        }
    }
    if (malloced_error_string) *malloced_error_string = h->panic_string;
    if (r == 0) {
        r = h->panic;
    }
    toku_ft_free(h);
    return r;
}

// maps to cf->note_pin_by_checkpoint
//Must be protected by ydb lock.
//Is only called by checkpoint begin, which holds it
static int
ft_note_pin_by_checkpoint (CACHEFILE UU(cachefile), void *header_v)
{
    //Set arbitrary brt (for given header) as pinned by checkpoint.
    //Only one can be pinned (only one checkpoint at a time), but not worth verifying.
    FT h = header_v;
    assert(!h->pinned_by_checkpoint);
    h->pinned_by_checkpoint = true;
    return 0;
}

// maps to cf->note_unpin_by_checkpoint
//Must be protected by ydb lock.
//Called by end_checkpoint, which grabs ydb lock around note_unpin
static int
ft_note_unpin_by_checkpoint (CACHEFILE UU(cachefile), void *header_v)
{
    FT h = header_v;
    assert(h->pinned_by_checkpoint);
    h->pinned_by_checkpoint = false; //Unpin
    int r = 0;
    //Close if necessary
    if (!toku_ft_needed(h)) {
        //Close immediately.
        char *error_string = NULL;
        r = toku_remove_ft(h, &error_string, false, ZERO_LSN);
        lazy_assert_zero(r);
    }
    return r;

}

//
// End of Functions that are callbacks to the cachefile
/////////////////////////////////////////////////////////////////////////

static int setup_initial_ft_root_node (FT h, BLOCKNUM blocknum) {
    FTNODE XMALLOC(node);
    toku_initialize_empty_ftnode(node, blocknum, 0, 1, h->layout_version, h->nodesize, h->flags);
    BP_STATE(node,0) = PT_AVAIL;

    u_int32_t fullhash = toku_cachetable_hash(h->cf, blocknum);
    node->fullhash = fullhash;
    int r = toku_cachetable_put(h->cf, blocknum, fullhash,
                                node, make_ftnode_pair_attr(node),
                                get_write_callbacks_for_node(h));
    if (r != 0)
        toku_free(node);
    else
        toku_unpin_ftnode(h, node);
    return r;
}

static int
ft_init (FT ft, FT_OPTIONS options, CACHEFILE cf, TOKUTXN txn) {
    ft->type = FT_CURRENT;
    ft->checkpoint_header = NULL;
    toku_ft_init_treelock(ft);
    toku_blocktable_create_new(&ft->blocktable);
    //Assign blocknum for root block, also dirty the header
    toku_allocate_blocknum(ft->blocktable, &ft->root_blocknum, ft);

    toku_list_init(&ft->live_ft_handles);
    int r = toku_omt_create(&ft->txns);
    assert_zero(r);
    ft->flags = options->flags;
    ft->nodesize = options->nodesize;
    ft->basementnodesize = options->basementnodesize;
    ft->compression_method = options->compression_method;
    ft->compare_fun = options->compare_fun;
    ft->update_fun = options->update_fun;

    if (ft->cf!=NULL) assert(ft->cf == cf);
    ft->cf = cf;
    ft->root_xid_that_created = txn ? txn->ancestor_txnid64 : TXNID_NONE;
    ft->in_memory_stats          = ZEROSTATS;
    ft->on_disk_stats            = ZEROSTATS;
    ft->checkpoint_staging_stats = ZEROSTATS;
    ft->highest_unused_msn_for_upgrade.msn = MIN_MSN.msn - 1;

    r = setup_initial_ft_root_node(ft, ft->root_blocknum);
    if (r != 0) { 
        goto exit; 
    }
    //printf("%s:%d putting %p (%d)\n", __FILE__, __LINE__, ft, 0);
    toku_cachefile_set_userdata(ft->cf,
                                ft,
                                ft_log_fassociate_during_checkpoint,
                                ft_log_suppress_rollback_during_checkpoint,
                                ft_close,
                                ft_checkpoint,
                                ft_begin_checkpoint,
                                ft_end_checkpoint,
                                ft_note_pin_by_checkpoint,
                                ft_note_unpin_by_checkpoint);

    toku_block_verify_no_free_blocknums(ft->blocktable);
    r = 0;
exit:
    return r;
}


// allocate and initialize a brt header.
// t->ft->cf is not set to anything.
int 
toku_create_new_ft(FT *ftp, FT_OPTIONS options, CACHEFILE cf, TOKUTXN txn) {
    int r;
    invariant(ftp);

    FT XCALLOC(ft);
    

    ft->layout_version = FT_LAYOUT_VERSION;
    ft->layout_version_original = FT_LAYOUT_VERSION;
    ft->layout_version_read_from_disk = FT_LAYOUT_VERSION;             // fake, prevent unnecessary upgrade logic

    ft->build_id = BUILD_ID;
    ft->build_id_original = BUILD_ID;

    uint64_t now = (uint64_t) time(NULL);
    ft->time_of_creation = now;
    ft->time_of_last_modification = now;
    ft->time_of_last_verification = 0;

    memset(&ft->descriptor, 0, sizeof(ft->descriptor));
    memset(&ft->cmp_descriptor, 0, sizeof(ft->cmp_descriptor));

    r = ft_init(ft, options, cf, txn);
    if (r != 0) {
        goto exit;
    }

    *ftp = ft;
    r = 0;
exit:
    if (r != 0) {
        if (ft) {
            toku_free(ft);
            ft = NULL;
        }
        return r;
    }
    return r;
}

// TODO: (Zardosht) get rid of brt parameter
int toku_read_ft_and_store_in_cachefile (FT_HANDLE brt, CACHEFILE cf, LSN max_acceptable_lsn, FT *header, BOOL* was_open)
// If the cachefile already has the header, then just get it.
// If the cachefile has not been initialized, then don't modify anything.
// max_acceptable_lsn is the latest acceptable checkpointed version of the file.
{
    {
        FT h;
        if ((h=toku_cachefile_get_userdata(cf))!=0) {
            *header = h;
            *was_open = TRUE;
            assert(brt->options.update_fun == h->update_fun);
            assert(brt->options.compare_fun == h->compare_fun);
            return 0;
        }
    }
    *was_open = FALSE;
    FT h;
    int r;
    {
        int fd = toku_cachefile_get_and_pin_fd (cf);
        enum deserialize_error_code e = toku_deserialize_ft_from(fd, max_acceptable_lsn, &h);
        if (e == DS_XSUM_FAIL) {
            fprintf(stderr, "Checksum failure while reading header in file %s.\n", toku_cachefile_fname_in_env(cf));
            assert(false);  // make absolutely sure we crash before doing anything else
        } else if (e == DS_ERRNO) {
            r = errno;
        } else if (e == DS_OK) {
            r = 0;
        } else {
            assert(false);
        }
        toku_cachefile_unpin_fd(cf);
    }
    if (r!=0) return r;
    h->cf = cf;
    h->compare_fun = brt->options.compare_fun;
    h->update_fun = brt->options.update_fun;
    toku_cachefile_set_userdata(cf,
                                (void*)h,
                                ft_log_fassociate_during_checkpoint,
                                ft_log_suppress_rollback_during_checkpoint,
                                ft_close,
                                ft_checkpoint,
                                ft_begin_checkpoint,
                                ft_end_checkpoint,
                                ft_note_pin_by_checkpoint,
                                ft_note_unpin_by_checkpoint);
    *header = h;
    return 0;
}

void
toku_ft_note_ft_handle_open(FT ft, FT_HANDLE live) {
    toku_ft_lock(ft);
    live->ft = ft;
    toku_list_push(&ft->live_ft_handles, &live->live_ft_handle_link);
    toku_ft_unlock(ft);
}

int
toku_ft_needed(FT h) {
    return !toku_list_empty(&h->live_ft_handles) || toku_omt_size(h->txns) != 0 || h->pinned_by_checkpoint;
}

// Close brt.  If opsln_valid, use given oplsn as lsn in brt header instead of logging 
// the close and using the lsn provided by logging the close.  (Subject to constraint 
// that if a newer lsn is already in the dictionary, don't overwrite the dictionary.)
int toku_remove_ft (FT h, char **error_string, BOOL oplsn_valid, LSN oplsn) {
    assert(!h->pinned_by_checkpoint);
    int r = 0;
    // Must do this work before closing the cf
    if (h->cf) {
        if (error_string) assert(*error_string == 0);
        r = toku_cachefile_close(&h->cf, error_string, oplsn_valid, oplsn);
        if (r==0 && error_string) assert(*error_string == 0);
    }
    return r;
}

// gets the first existing BRT handle, if it exists. If no BRT handle exists
// for this header, returns NULL
FT_HANDLE toku_ft_get_some_existing_ft_handle(FT h) {
    FT_HANDLE ft_handle_ret = NULL;
    toku_ft_lock(h);
    if (!toku_list_empty(&h->live_ft_handles)) {
        ft_handle_ret = toku_list_struct(toku_list_head(&h->live_ft_handles), struct ft_handle, live_ft_handle_link);
    }
    toku_ft_unlock(h);
    return ft_handle_ret;
}

// Purpose: set fields in brt_header to capture accountability info for start of HOT optimize.
// Note: HOT accountability variables in header are modified only while holding header lock.
//       (Header lock is really needed for touching the dirty bit, but it's useful and 
//       convenient here for keeping the HOT variables threadsafe.)
void
toku_ft_note_hot_begin(FT_HANDLE brt) {
    FT h = brt->ft;
    time_t now = time(NULL);

    // hold lock around setting and clearing of dirty bit
    // (see cooperative use of dirty bit in ft_begin_checkpoint())
    toku_ft_lock(h);
    h->time_of_last_optimize_begin = now;
    h->count_of_optimize_in_progress++;
    h->dirty = 1;
    toku_ft_unlock(h);
}


// Purpose: set fields in brt_header to capture accountability info for end of HOT optimize.
// Note: See note for toku_ft_note_hot_begin().
void
toku_ft_note_hot_complete(FT_HANDLE brt, BOOL success, MSN msn_at_start_of_hot) {
    FT h = brt->ft;
    time_t now = time(NULL);

    toku_ft_lock(h);
    h->count_of_optimize_in_progress--;
    if (success) {
        h->time_of_last_optimize_end = now;
        h->msn_at_start_of_last_completed_optimize = msn_at_start_of_hot;
        // If we just successfully completed an optimization and no other thread is performing
        // an optimization, then the number of optimizations in progress is zero.
        // If there was a crash during a HOT optimization, this is how count_of_optimize_in_progress
        // would be reset to zero on the disk after recovery from that crash.  
        if (h->count_of_optimize_in_progress == h->count_of_optimize_in_progress_read_from_disk)
            h->count_of_optimize_in_progress = 0;
    }
    h->dirty = 1;
    toku_ft_unlock(h);
}


void
toku_ft_init(FT h, 
                     BLOCKNUM root_blocknum_on_disk, LSN checkpoint_lsn, TXNID root_xid_that_created, uint32_t target_nodesize, uint32_t target_basementnodesize, enum toku_compression_method compression_method) {
    memset(h, 0, sizeof *h);
    h->layout_version   = FT_LAYOUT_VERSION;
    h->layout_version_original = FT_LAYOUT_VERSION;
    h->build_id         = BUILD_ID;
    h->build_id_original = BUILD_ID;
    uint64_t now = (uint64_t) time(NULL);
    h->time_of_creation = now;
    h->time_of_last_modification = now;
    h->time_of_last_verification = 0;
    h->checkpoint_count = 1;
    h->checkpoint_lsn   = checkpoint_lsn;
    h->nodesize         = target_nodesize;
    h->basementnodesize = target_basementnodesize;
    h->root_blocknum    = root_blocknum_on_disk;
    h->flags            = 0;
    h->root_xid_that_created = root_xid_that_created;
    h->compression_method = compression_method;
    h->highest_unused_msn_for_upgrade.msn  = MIN_MSN.msn - 1;
}

// Open a brt for use by redirect.  The new brt must have the same dict_id as the old_ft passed in.  (FILENUM is assigned by the ft_handle_open() function.)
static int
ft_handle_open_for_redirect(FT_HANDLE *new_ftp, const char *fname_in_env, TOKUTXN txn, FT old_h) {
    int r;
    FT_HANDLE t;
    assert(old_h->dict_id.dictid != DICTIONARY_ID_NONE.dictid);
    r = toku_ft_handle_create(&t);
    assert_zero(r);
    r = toku_ft_set_bt_compare(t, old_h->compare_fun);
    assert_zero(r);
    r = toku_ft_set_update(t, old_h->update_fun);
    assert_zero(r);
    r = toku_ft_set_nodesize(t, old_h->nodesize);
    assert_zero(r);
    r = toku_ft_set_basementnodesize(t, old_h->basementnodesize);
    assert_zero(r);
    r = toku_ft_set_compression_method(t, old_h->compression_method);
    assert_zero(r);
    CACHETABLE ct = toku_cachefile_get_cachetable(old_h->cf);
    r = toku_ft_handle_open_with_dict_id(t, fname_in_env, 0, 0, ct, txn, old_h->dict_id);
    assert_zero(r);
    assert(t->ft->dict_id.dictid == old_h->dict_id.dictid);

    *new_ftp = t;
    return r;
}

// This function performs most of the work to redirect a dictionary to different file.
// It is called for redirect and to abort a redirect.  (This function is almost its own inverse.)
static int
dictionary_redirect_internal(const char *dst_fname_in_env, FT src_h, TOKUTXN txn, FT *dst_hp) {
    int r;

    FILENUM src_filenum = toku_cachefile_filenum(src_h->cf);
    FILENUM dst_filenum = FILENUM_NONE;

    FT dst_h = NULL;
    struct toku_list *list;
    // open a dummy brt based off of 
    // dst_fname_in_env to get the header
    // then we will change all the brt's to have
    // their headers point to dst_h instead of src_h
    FT_HANDLE tmp_dst_ft = NULL;
    r = ft_handle_open_for_redirect(&tmp_dst_ft, dst_fname_in_env, txn, src_h);
    assert_zero(r);
    dst_h = tmp_dst_ft->ft;

    // some sanity checks on dst_filenum
    dst_filenum = toku_cachefile_filenum(dst_h->cf);
    assert(dst_filenum.fileid!=FILENUM_NONE.fileid);
    assert(dst_filenum.fileid!=src_filenum.fileid); //Cannot be same file.

    // for each live brt, brt->ft is currently src_h
    // we want to change it to dummy_dst
    while (!toku_list_empty(&src_h->live_ft_handles)) {
        list = src_h->live_ft_handles.next;
        FT_HANDLE src_handle = NULL;
        src_handle = toku_list_struct(list, struct ft_handle, live_ft_handle_link);

        toku_ft_lock(src_h);
        toku_list_remove(&src_handle->live_ft_handle_link);
        toku_ft_unlock(src_h);
        
        toku_ft_note_ft_handle_open(dst_h, src_handle);
        if (src_handle->redirect_callback) {
            src_handle->redirect_callback(src_handle, src_handle->redirect_callback_extra);
        }
    }
    assert(dst_h);

    r = toku_ft_handle_close(tmp_dst_ft, FALSE, ZERO_LSN);
    assert_zero(r);

    *dst_hp = dst_h;
    return r;
}



//This is the 'abort redirect' function.  The redirect of old_h to new_h was done
//and now must be undone, so here we redirect new_h back to old_h.
int
toku_dictionary_redirect_abort(FT old_h, FT new_h, TOKUTXN txn) {
    char *old_fname_in_env = toku_cachefile_fname_in_env(old_h->cf);
    int r;
    {
        FILENUM old_filenum = toku_cachefile_filenum(old_h->cf);
        FILENUM new_filenum = toku_cachefile_filenum(new_h->cf);
        assert(old_filenum.fileid!=new_filenum.fileid); //Cannot be same file.

        //No living brts in old header.
        assert(toku_list_empty(&old_h->live_ft_handles));
    }

    // If application did not close all DBs using the new file, then there should 
    // be no zombies and we need to redirect the DBs back to the original file.
    if (!toku_list_empty(&new_h->live_ft_handles)) {
        FT dst_h;
        // redirect back from new_h to old_h
        r = dictionary_redirect_internal(old_fname_in_env, new_h, txn, &dst_h);
        assert_zero(r);
        assert(dst_h == old_h);
    }
    else {
        //No live brts.
        //No need to redirect back.
        r = 0;
    }
    return r;
}

/****
 * on redirect or abort:
 *  if redirect txn_note_doing_work(txn)
 *  if redirect connect src brt to txn (txn modified this brt)
 *  for each src brt
 *    open brt to dst file (create new brt struct)
 *    if redirect connect dst brt to txn 
 *    redirect db to new brt
 *    redirect cursors to new brt
 *  close all src brts
 *  if redirect make rollback log entry
 * 
 * on commit:
 *   nothing to do
 *
 *****/

int 
toku_dictionary_redirect (const char *dst_fname_in_env, FT_HANDLE old_ft, TOKUTXN txn) {
// Input args:
//   new file name for dictionary (relative to env)
//   old_ft is a live brt of open handle ({DB, BRT} pair) that currently refers to old dictionary file.
//   (old_ft may be one of many handles to the dictionary.)
//   txn that created the loader
// Requires: 
//   ydb_lock is held.
//   The brt is open.  (which implies there can be no zombies.)
//   The new file must be a valid dictionary.
//   The block size and flags in the new file must match the existing BRT.
//   The new file must already have its descriptor in it (and it must match the existing descriptor).
// Effect:   
//   Open new FTs (and related header and cachefile) to the new dictionary file with a new FILENUM.
//   Redirect all DBs that point to brts that point to the old file to point to brts that point to the new file.
//   Copy the dictionary id (dict_id) from the header of the original file to the header of the new file.
//   Create a rollback log entry.
//   The original BRT, header, cachefile and file remain unchanged.  They will be cleaned up on commmit.
//   If the txn aborts, then this operation will be undone
    int r;

    FT old_h = old_ft->ft;

    // dst file should not be open.  (implies that dst and src are different because src must be open.)
    {
        CACHETABLE ct = toku_cachefile_get_cachetable(old_h->cf);
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
        r = toku_txn_note_ft(txn, old_h);  // mark old brt as touched by this txn
        assert_zero(r);
    }

    FT new_h;
    r = dictionary_redirect_internal(dst_fname_in_env, old_h, txn, &new_h);
    assert_zero(r);

    // make rollback log entry
    if (txn) {
        assert(!toku_list_empty(&new_h->live_ft_handles));
        r = toku_txn_note_ft(txn, new_h); // mark new brt as touched by this txn

        FILENUM old_filenum = toku_cachefile_filenum(old_h->cf);
        FILENUM new_filenum = toku_cachefile_filenum(new_h->cf);
        r = toku_logger_save_rollback_dictionary_redirect(txn, old_filenum, new_filenum);
        assert_zero(r);

        TXNID xid = toku_txn_get_txnid(txn);
        toku_ft_suppress_rollbacks(new_h, txn);
        r = toku_log_suppress_rollback(txn->logger, NULL, 0, new_filenum, xid);
        assert_zero(r);
    }
    
cleanup:
    return r;
}

//Heaviside function to find a TOKUTXN by TOKUTXN (used to find the index)
static int find_xid (OMTVALUE v, void *txnv) {
    TOKUTXN txn = v;
    TOKUTXN txnfind = txnv;
    if (txn->txnid64<txnfind->txnid64) return -1;
    if (txn->txnid64>txnfind->txnid64) return +1;
    return 0;
}

// returns if ref was added
BOOL
toku_ft_maybe_add_txn_ref(FT h, TOKUTXN txn) {
    BOOL ref_added = FALSE;
    OMTVALUE txnv;
    u_int32_t index;
    toku_ft_lock(h);
    // Does brt already know about transaction txn?
    int r = toku_omt_find_zero(h->txns, find_xid, txn, &txnv, &index);
    if (r==0) {
        // It's already there.
        assert((TOKUTXN)txnv==txn);
        ref_added = FALSE;
        goto exit;
    }
    // Otherwise it's not there.
    // Insert reference to transaction into brt
    r = toku_omt_insert_at(h->txns, txn, index);
    assert(r==0);
    ref_added = TRUE;
exit:
    toku_ft_unlock(h);
    return ref_added;
}

void
toku_ft_remove_txn_ref(FT h, TOKUTXN txn) {
    OMTVALUE txnv_again=NULL;
    u_int32_t index;
    toku_ft_lock(h);
    int r = toku_omt_find_zero(h->txns, find_xid, txn, &txnv_again, &index);
    assert(r==0);
    assert(txnv_again == txn);
    r = toku_omt_delete_at(h->txns, index);
    assert(r==0);
    // TODO: (Zardosht) figure out how to properly do this
    // below this unlock, are depending on ydb lock
    toku_ft_unlock(h);
    if (!toku_ft_needed(h)) {
        //Close immediately.
        // I have no idea how this error string business works
        char *error_string = NULL;
        r = toku_remove_ft(h, &error_string, false, ZERO_LSN);
        lazy_assert_zero(r);
    }
}

void toku_calculate_root_offset_pointer (
    FT h, 
    CACHEKEY* root_key, 
    u_int32_t *roothash
    ) 
{
    *roothash = toku_cachetable_hash(h->cf, h->root_blocknum);
    *root_key = h->root_blocknum;
}

void toku_ft_set_new_root_blocknum(
    FT h, 
    CACHEKEY new_root_key
    ) 
{
    h->root_blocknum = new_root_key;
}

LSN toku_ft_checkpoint_lsn(FT h) {
    return h->checkpoint_lsn;
}

int toku_ft_set_panic(FT h, int panic, char *panic_string) {
    if (h->panic == 0) {
        h->panic = panic;
        if (h->panic_string) {
            toku_free(h->panic_string);
        }
        h->panic_string = toku_strdup(panic_string);
    }
    return 0;
}

void 
toku_ft_stat64 (FT h, struct ftstat64_s *s) {
    s->fsize = toku_cachefile_size(h->cf);
    // just use the in memory stats from the header
    // prevent appearance of negative numbers for numrows, numbytes
    int64_t n = h->in_memory_stats.numrows;
    if (n < 0) {
        n = 0;
    }
    s->nkeys = s->ndata = n;
    n = h->in_memory_stats.numbytes;
    if (n < 0) {
        n = 0;
    }
    s->dsize = n; 

    // 4018
    s->create_time_sec = h->time_of_creation;
    s->modify_time_sec = h->time_of_last_modification;
    s->verify_time_sec = h->time_of_last_verification;    
}

// TODO: (Zardosht), once the fdlock has been removed from cachetable, remove
// fd as parameter and access it in this function
int 
toku_update_descriptor(FT h, DESCRIPTOR d, int fd) 
// Effect: Change the descriptor in a tree (log the change, make sure it makes it to disk eventually).
//  Updates to the descriptor must be performed while holding some sort of lock.  (In the ydb layer
//  there is a row lock on the directory that provides exclusion.)
{
    int r = 0;
    DISKOFF offset;
    // 4 for checksum
    toku_realloc_descriptor_on_disk(h->blocktable, toku_serialize_descriptor_size(d)+4, &offset, h);
    r = toku_serialize_descriptor_contents_to_fd(fd, d, offset);
    if (r) {
        goto cleanup;
    }
    if (h->descriptor.dbt.data) {
        toku_free(h->descriptor.dbt.data);
    }
    h->descriptor.dbt.size = d->dbt.size;
    h->descriptor.dbt.data = toku_memdup(d->dbt.data, d->dbt.size);

    r = 0;
cleanup:
    return r;
}

void 
toku_ft_update_cmp_descriptor(FT h) {
    if (h->cmp_descriptor.dbt.data != NULL) {
        toku_free(h->cmp_descriptor.dbt.data);
    }
    h->cmp_descriptor.dbt.size = h->descriptor.dbt.size;
    h->cmp_descriptor.dbt.data = toku_xmemdup(
        h->descriptor.dbt.data, 
        h->descriptor.dbt.size
        );
}

void
toku_ft_update_stats(STAT64INFO headerstats, STAT64INFO_S delta) {
    (void) __sync_fetch_and_add(&(headerstats->numrows),  delta.numrows);
    (void) __sync_fetch_and_add(&(headerstats->numbytes), delta.numbytes);
}

void
toku_ft_decrease_stats(STAT64INFO headerstats, STAT64INFO_S delta) {
    (void) __sync_fetch_and_sub(&(headerstats->numrows),  delta.numrows);
    (void) __sync_fetch_and_sub(&(headerstats->numbytes), delta.numbytes);
}
