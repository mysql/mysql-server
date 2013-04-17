/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ident "$Id: brt.c 43396 2012-05-11 17:24:47Z zardosht $"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."


#include "includes.h"

void
toku_brt_header_suppress_rollbacks(struct brt_header *h, TOKUTXN txn) {
    TXNID txnid = toku_txn_get_txnid(txn);
    assert(h->txnid_that_created_or_locked_when_empty == TXNID_NONE ||
           h->txnid_that_created_or_locked_when_empty == txnid);
    h->txnid_that_created_or_locked_when_empty = txnid;
    TXNID rootid = toku_txn_get_root_txnid(txn);
    assert(h->root_that_created_or_locked_when_empty == TXNID_NONE ||
           h->root_that_created_or_locked_when_empty == rootid);
    h->root_that_created_or_locked_when_empty  = rootid;
}

static void
brtheader_destroy(struct brt_header *h) {
    if (!h->panic) assert(!h->checkpoint_header);

    //header and checkpoint_header have same Blocktable pointer
    //cannot destroy since it is still in use by CURRENT
    if (h->type == BRTHEADER_CHECKPOINT_INPROGRESS) h->blocktable = NULL; 
    else {
        assert(h->type == BRTHEADER_CURRENT);
        toku_blocktable_destroy(&h->blocktable);
        if (h->descriptor.dbt.data) toku_free(h->descriptor.dbt.data);
        if (h->cmp_descriptor.dbt.data) toku_free(h->cmp_descriptor.dbt.data);
        toku_brtheader_destroy_treelock(h);
        toku_omt_destroy(&h->txns);
    }
}

static int
brtheader_alloc(struct brt_header **hh) {
    int r = 0;
    if ((CALLOC(*hh))==0) {
        assert(errno==ENOMEM);
        r = ENOMEM;
    }
    return r;
}

// Make a copy of the header for the purpose of a checkpoint
static void
brtheader_copy_for_checkpoint(struct brt_header *h, LSN checkpoint_lsn) {
    assert(h->type == BRTHEADER_CURRENT);
    assert(h->checkpoint_header == NULL);
    assert(h->panic==0);

    struct brt_header* XMALLOC(ch);
    *ch = *h; //Do a shallow copy
    ch->type = BRTHEADER_CHECKPOINT_INPROGRESS; //Different type
    //printf("checkpoint_lsn=%" PRIu64 "\n", checkpoint_lsn.lsn);
    ch->checkpoint_lsn = checkpoint_lsn;
    ch->panic_string = NULL;

    //ch->blocktable is SHARED between the two headers
    h->checkpoint_header = ch;
}

static void
brtheader_free(struct brt_header *h)
{
    brtheader_destroy(h);
    toku_free(h);
}

void
toku_brtheader_free (struct brt_header *h) {
    brtheader_free(h);
}

void
toku_brtheader_init_treelock(struct brt_header* h) {
    int r = toku_pthread_mutex_init(&h->tree_lock, NULL); assert(r == 0);
}

void
toku_brtheader_destroy_treelock(struct brt_header* h) {
    int r = toku_pthread_mutex_destroy(&h->tree_lock); assert(r == 0);
}

void
toku_brtheader_grab_treelock(struct brt_header* h) {
    int r = toku_pthread_mutex_lock(&h->tree_lock); assert(r == 0);
}

void
toku_brtheader_release_treelock(struct brt_header* h) {
    int r = toku_pthread_mutex_unlock(&h->tree_lock); assert(r == 0);
}

static int
brtheader_log_fassociate_during_checkpoint (CACHEFILE cf, void *header_v) {
    struct brt_header *h = header_v;
    char* fname_in_env = toku_cachefile_fname_in_env(cf);
    BYTESTRING bs = { strlen(fname_in_env), // don't include the NUL
                      fname_in_env };
    TOKULOGGER logger = toku_cachefile_logger(cf);
    FILENUM filenum = toku_cachefile_filenum (cf);
    int r = toku_log_fassociate(logger, NULL, 0, filenum, h->flags, bs);
    return r;
}

static int
brtheader_log_suppress_rollback_during_checkpoint (CACHEFILE cf, void *header_v) {
    int r = 0;
    struct brt_header *h = header_v;
    TXNID xid = h->txnid_that_created_or_locked_when_empty;
    if (xid != TXNID_NONE) {
        //Only log if useful.
        TOKULOGGER logger = toku_cachefile_logger(cf);
        FILENUM filenum = toku_cachefile_filenum (cf);
        r = toku_log_suppress_rollback(logger, NULL, 0, filenum, xid);
    }
    return r;
}


static int brtheader_note_pin_by_checkpoint (CACHEFILE cachefile, void *header_v);
static int brtheader_note_unpin_by_checkpoint (CACHEFILE cachefile, void *header_v);

static int setup_initial_brtheader_root_node (struct brt_header* h, BLOCKNUM blocknum) {
    BRTNODE XMALLOC(node);
    toku_initialize_empty_brtnode(node, blocknum, 0, 1, h->layout_version, h->nodesize, h->flags, h);
    BP_STATE(node,0) = PT_AVAIL;

    u_int32_t fullhash = toku_cachetable_hash(h->cf, blocknum);
    node->fullhash = fullhash;
    int r = toku_cachetable_put(h->cf, blocknum, fullhash,
                                node, make_brtnode_pair_attr(node),
                                get_write_callbacks_for_node(h));
    if (r != 0)
        toku_free(node);
    else
        toku_unpin_brtnode(h, node);
    return r;
}


static int
brt_init_header_partial (BRT t, CACHEFILE cf, TOKUTXN txn) {
    int r;
    t->h->flags = t->flags;
    if (t->h->cf!=NULL) assert(t->h->cf == cf);
    t->h->cf = cf;
    t->h->nodesize=t->nodesize;
    t->h->basementnodesize=t->basementnodesize;
    t->h->root_xid_that_created = txn ? txn->ancestor_txnid64 : TXNID_NONE;
    t->h->compare_fun = t->compare_fun;
    t->h->update_fun = t->update_fun;
    t->h->in_memory_stats          = ZEROSTATS;
    t->h->on_disk_stats            = ZEROSTATS;
    t->h->checkpoint_staging_stats = ZEROSTATS;
    t->h->highest_unused_msn_for_upgrade.msn = MIN_MSN.msn - 1;

    BLOCKNUM root = t->h->root_blocknum;
    r = setup_initial_brtheader_root_node(t->h, root);
    if (r != 0) { 
        goto exit; 
    }
    //printf("%s:%d putting %p (%d)\n", __FILE__, __LINE__, t->h, 0);
    toku_cachefile_set_userdata(t->h->cf,
                                t->h,
                                brtheader_log_fassociate_during_checkpoint,
                                brtheader_log_suppress_rollback_during_checkpoint,
                                toku_brtheader_close,
                                toku_brtheader_checkpoint,
                                toku_brtheader_begin_checkpoint,
                                toku_brtheader_end_checkpoint,
                                brtheader_note_pin_by_checkpoint,
                                brtheader_note_unpin_by_checkpoint);
exit:
    return r;
}

static int
brt_init_header (BRT t, CACHEFILE cf, TOKUTXN txn) {
    t->h->type = BRTHEADER_CURRENT;
    t->h->checkpoint_header = NULL;
    toku_brtheader_init_treelock(t->h);
    toku_blocktable_create_new(&t->h->blocktable);
    BLOCKNUM root;
    //Assign blocknum for root block, also dirty the header
    toku_allocate_blocknum(t->h->blocktable, &root, t->h);
    t->h->root_blocknum = root;
    t->h->compression_method = TOKU_DEFAULT_COMPRESSION_METHOD;

    toku_list_init(&t->h->live_brts);
    int r = toku_omt_create(&t->h->txns);
    assert_zero(r);
    r = brt_init_header_partial(t, cf, txn);
    if (r==0) toku_block_verify_no_free_blocknums(t->h->blocktable);
    return r;
}


// allocate and initialize a brt header.
// t->h->cf is not set to anything.
// TODO: (Zardosht) make this function return a header and set
// it to t->h in the caller
int 
toku_create_new_brtheader(BRT t, CACHEFILE cf, TOKUTXN txn) {
    int r;

    r = brtheader_alloc(&t->h);
    if (r != 0) {
        if (0) { died2: toku_free(t->h); }
        t->h=0;
        return r;
    }

    t->h->layout_version = BRT_LAYOUT_VERSION;
    t->h->layout_version_original = BRT_LAYOUT_VERSION;
    t->h->layout_version_read_from_disk = BRT_LAYOUT_VERSION;             // fake, prevent unnecessary upgrade logic

    t->h->build_id = BUILD_ID;
    t->h->build_id_original = BUILD_ID;

    uint64_t now = (uint64_t) time(NULL);
    t->h->time_of_creation = now;
    t->h->time_of_last_modification = now;
    t->h->time_of_last_verification = 0;

    memset(&t->h->descriptor, 0, sizeof(t->h->descriptor));
    memset(&t->h->cmp_descriptor, 0, sizeof(t->h->cmp_descriptor));

    r = brt_init_header(t, cf, txn);
    if (r != 0) goto died2;
    return r;
}

// TODO: (Zardosht) get rid of brt parameter
int toku_read_brt_header_and_store_in_cachefile (BRT brt, CACHEFILE cf, LSN max_acceptable_lsn, struct brt_header **header, BOOL* was_open)
// If the cachefile already has the header, then just get it.
// If the cachefile has not been initialized, then don't modify anything.
// max_acceptable_lsn is the latest acceptable checkpointed version of the file.
{
    {
        struct brt_header *h;
        if ((h=toku_cachefile_get_userdata(cf))!=0) {
            *header = h;
            *was_open = TRUE;
            assert(brt->update_fun == h->update_fun);
            assert(brt->compare_fun == h->compare_fun);
            return 0;
        }
    }
    *was_open = FALSE;
    struct brt_header *h;
    int r;
    {
        int fd = toku_cachefile_get_and_pin_fd (cf);
        enum deserialize_error_code e = toku_deserialize_brtheader_from(fd, max_acceptable_lsn, &h);
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
    h->compare_fun = brt->compare_fun;
    h->update_fun = brt->update_fun;
    toku_cachefile_set_userdata(cf,
                                (void*)h,
                                brtheader_log_fassociate_during_checkpoint,
                                brtheader_log_suppress_rollback_during_checkpoint,
                                toku_brtheader_close,
                                toku_brtheader_checkpoint,
                                toku_brtheader_begin_checkpoint,
                                toku_brtheader_end_checkpoint,
                                brtheader_note_pin_by_checkpoint,
                                brtheader_note_unpin_by_checkpoint);
    *header = h;
    return 0;
}

void
toku_brtheader_note_brt_open(BRT live) {
    struct brt_header *h = live->h;
    toku_brtheader_lock(h);
    toku_list_push(&h->live_brts, &live->live_brt_link);
    h->dictionary_opened = TRUE;
    toku_brtheader_unlock(h);
}

// Open a brt for use by redirect.  The new brt must have the same dict_id as the old_brt passed in.  (FILENUM is assigned by the brt_open() function.)
static int
brt_open_for_redirect(BRT *new_brtp, const char *fname_in_env, TOKUTXN txn, struct brt_header* old_h) {
    int r;
    BRT t;
    assert(old_h->dict_id.dictid != DICTIONARY_ID_NONE.dictid);
    r = toku_brt_create(&t);
    assert_zero(r);
    r = toku_brt_set_bt_compare(t, old_h->compare_fun);
    assert_zero(r);
    r = toku_brt_set_update(t, old_h->update_fun);
    assert_zero(r);
    r = toku_brt_set_nodesize(t, old_h->nodesize);
    assert_zero(r);
    r = toku_brt_set_basementnodesize(t, old_h->basementnodesize);
    assert_zero(r);
    CACHETABLE ct = toku_cachefile_get_cachetable(old_h->cf);
    r = toku_brt_open_with_dict_id(t, fname_in_env, 0, 0, ct, txn, old_h->dict_id);
    assert_zero(r);
    assert(t->h->dict_id.dictid == old_h->dict_id.dictid);

    *new_brtp = t;
    return r;
}

// This function performs most of the work to redirect a dictionary to different file.
// It is called for redirect and to abort a redirect.  (This function is almost its own inverse.)
static int
dictionary_redirect_internal(const char *dst_fname_in_env, struct brt_header *src_h, TOKUTXN txn, struct brt_header **dst_hp) {
    int r;

    FILENUM src_filenum = toku_cachefile_filenum(src_h->cf);
    FILENUM dst_filenum = FILENUM_NONE;

    struct brt_header *dst_h = NULL;
    struct toku_list *list;
    // open a dummy brt based off of 
    // dst_fname_in_env to get the header
    // then we will change all the brt's to have
    // their headers point to dst_h instead of src_h
    BRT tmp_dst_brt = NULL;
    r = brt_open_for_redirect(&tmp_dst_brt, dst_fname_in_env, txn, src_h);
    assert_zero(r);
    dst_h = tmp_dst_brt->h;

    // some sanity checks on dst_filenum
    dst_filenum = toku_cachefile_filenum(dst_h->cf);
    assert(dst_filenum.fileid!=FILENUM_NONE.fileid);
    assert(dst_filenum.fileid!=src_filenum.fileid); //Cannot be same file.

    // for each live brt, brt->h is currently src_h
    // we want to change it to dummy_dst
    while (!toku_list_empty(&src_h->live_brts)) {
        list = src_h->live_brts.next;
        BRT src_brt = NULL;
        src_brt = toku_list_struct(list, struct brt, live_brt_link);

        toku_brtheader_lock(src_h);
        toku_list_remove(&src_brt->live_brt_link);
        toku_brtheader_unlock(src_h);
        
        src_brt->h = dst_h;
        
        toku_brtheader_note_brt_open(src_brt);
        if (src_brt->redirect_callback) {
            src_brt->redirect_callback(src_brt, src_brt->redirect_callback_extra);
        }
    }
    assert(dst_h);

    r = toku_brt_close(tmp_dst_brt, FALSE, ZERO_LSN);
    assert_zero(r);

    *dst_hp = dst_h;
    return r;
}

// Create checkpoint-in-progress versions of header and translation (btt) (and fifo for now...).
//Has access to fd (it is protected).
int
toku_brtheader_begin_checkpoint (LSN checkpoint_lsn, void *header_v) {
    struct brt_header *h = header_v;
    int r = h->panic;
    if (r==0) {
        // hold lock around copying and clearing of dirty bit
        toku_brtheader_lock (h);
        assert(h->type == BRTHEADER_CURRENT);
        assert(h->checkpoint_header == NULL);
        brtheader_copy_for_checkpoint(h, checkpoint_lsn);
        h->dirty = 0;             // this is only place this bit is cleared        (in currentheader)
        // on_disk_stats includes on disk changes since last checkpoint,
        // so checkpoint_staging_stats now includes changes for checkpoint in progress.
        h->checkpoint_staging_stats = h->on_disk_stats;
        toku_block_translation_note_start_checkpoint_unlocked(h->blocktable);
        toku_brtheader_unlock (h);
    }
    return r;
}

int
toku_brt_header_needed(struct brt_header* h) {
    return !toku_list_empty(&h->live_brts) || toku_omt_size(h->txns) != 0 || h->pinned_by_checkpoint;
}

//Must be protected by ydb lock.
//Is only called by checkpoint begin, which holds it
static int
brtheader_note_pin_by_checkpoint (CACHEFILE UU(cachefile), void *header_v)
{
    //Set arbitrary brt (for given header) as pinned by checkpoint.
    //Only one can be pinned (only one checkpoint at a time), but not worth verifying.
    struct brt_header *h = header_v;
    assert(!h->pinned_by_checkpoint);
    h->pinned_by_checkpoint = true;
    return 0;
}

//Must be protected by ydb lock.
//Called by end_checkpoint, which grabs ydb lock around note_unpin
static int
brtheader_note_unpin_by_checkpoint (CACHEFILE UU(cachefile), void *header_v)
{
    struct brt_header *h = header_v;
    assert(h->pinned_by_checkpoint);
    h->pinned_by_checkpoint = false; //Unpin
    int r = 0;
    //Close if necessary
    if (!toku_brt_header_needed(h)) {
        //Close immediately.
        char *error_string = NULL;
        r = toku_remove_brtheader(h, &error_string, false, ZERO_LSN);
        lazy_assert_zero(r);
    }
    return r;

}

// Write checkpoint-in-progress versions of header and translation to disk (really to OS internal buffer).
// Copy current header's version of checkpoint_staging stat64info to checkpoint header.
// Must have access to fd (protected).
// Requires: all pending bits are clear.  This implies that no thread will modify the checkpoint_staging
// version of the stat64info.
int
toku_brtheader_checkpoint (CACHEFILE cf, int fd, void *header_v) {
    struct brt_header *h = header_v;
    struct brt_header *ch = h->checkpoint_header;
    int r = 0;
    if (h->panic!=0) goto handle_error;
    //printf("%s:%d allocated_limit=%lu writing queue to %lu\n", __FILE__, __LINE__,
    //             block_allocator_allocated_limit(h->block_allocator), h->unused_blocks.b*h->nodesize);
    assert(ch);
    if (ch->panic!=0) goto handle_error;
    assert(ch->type == BRTHEADER_CHECKPOINT_INPROGRESS);
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
        r = toku_serialize_brt_header_to(fd, ch);
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

// free unused disk space 
// (i.e. tell BlockAllocator to liberate blocks used by previous checkpoint).
// Must have access to fd (protected)
int
toku_brtheader_end_checkpoint (CACHEFILE UU(cachefile), int fd, void *header_v) {
    struct brt_header *h = header_v;
    int r = h->panic;
    if (r==0) {
        assert(h->type == BRTHEADER_CURRENT);
        toku_block_translation_note_end_checkpoint(h->blocktable, fd, h);
    }
    if (h->checkpoint_header) {         // could be NULL only if panic was true at begin_checkpoint
        brtheader_free(h->checkpoint_header);
        h->checkpoint_header = NULL;
    }
    return r;
}

//Has access to fd (it is protected).
int
toku_brtheader_close (CACHEFILE cachefile, int fd, void *header_v, char **malloced_error_string, BOOL oplsn_valid, LSN oplsn) {
    struct brt_header *h = header_v;
    assert(h->type == BRTHEADER_CURRENT);
    toku_brtheader_lock(h);
    assert(!toku_brt_header_needed(h));
    toku_brtheader_unlock(h);
    int r = 0;
    if (h->panic) {
        r = h->panic;
    } else if (h->dictionary_opened) { //Otherwise header has never fully been created.
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
            r2 = toku_brtheader_begin_checkpoint(lsn, header_v);
            if (r==0) r = r2;
            r2 = toku_brtheader_checkpoint(cachefile, fd, h);
            if (r==0) r = r2;
            r2 = toku_brtheader_end_checkpoint(cachefile, fd, header_v);
            if (r==0) r = r2;
            if (!h->panic) assert(!h->dirty);             // dirty bit should be cleared by begin_checkpoint and never set again (because we're closing the dictionary)
        }
    }
    if (malloced_error_string) *malloced_error_string = h->panic_string;
    if (r == 0) {
        r = h->panic;
    }
    toku_brtheader_free(h);
    return r;
}

// Close brt.  If opsln_valid, use given oplsn as lsn in brt header instead of logging 
// the close and using the lsn provided by logging the close.  (Subject to constraint 
// that if a newer lsn is already in the dictionary, don't overwrite the dictionary.)
int toku_remove_brtheader (struct brt_header* h, char **error_string, BOOL oplsn_valid, LSN oplsn) {
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

// Purpose: set fields in brt_header to capture accountability info for start of HOT optimize.
// Note: HOT accountability variables in header are modified only while holding header lock.
//       (Header lock is really needed for touching the dirty bit, but it's useful and 
//       convenient here for keeping the HOT variables threadsafe.)
void
toku_brt_header_note_hot_begin(BRT brt) {
    struct brt_header *h = brt->h;
    time_t now = time(NULL);

    // hold lock around setting and clearing of dirty bit
    // (see cooperative use of dirty bit in toku_brtheader_begin_checkpoint())
    toku_brtheader_lock(h);
    h->time_of_last_optimize_begin = now;
    h->count_of_optimize_in_progress++;
    h->dirty = 1;
    toku_brtheader_unlock(h);
}


// Purpose: set fields in brt_header to capture accountability info for end of HOT optimize.
// Note: See note for toku_brt_header_note_hot_begin().
void
toku_brt_header_note_hot_complete(BRT brt, BOOL success, MSN msn_at_start_of_hot) {
    struct brt_header *h = brt->h;
    time_t now = time(NULL);

    toku_brtheader_lock(h);
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
    toku_brtheader_unlock(h);
}


void
toku_brt_header_init(struct brt_header *h, 
                     BLOCKNUM root_blocknum_on_disk, LSN checkpoint_lsn, TXNID root_xid_that_created, uint32_t target_nodesize, uint32_t target_basementnodesize, enum toku_compression_method compression_method) {
    memset(h, 0, sizeof *h);
    h->layout_version   = BRT_LAYOUT_VERSION;
    h->layout_version_original = BRT_LAYOUT_VERSION;
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

//This is the 'abort redirect' function.  The redirect of old_h to new_h was done
//and now must be undone, so here we redirect new_h back to old_h.
int
toku_dictionary_redirect_abort(struct brt_header *old_h, struct brt_header *new_h, TOKUTXN txn) {
    char *old_fname_in_env = toku_cachefile_fname_in_env(old_h->cf);
    int r;
    {
        FILENUM old_filenum = toku_cachefile_filenum(old_h->cf);
        FILENUM new_filenum = toku_cachefile_filenum(new_h->cf);
        assert(old_filenum.fileid!=new_filenum.fileid); //Cannot be same file.

        //No living brts in old header.
        assert(toku_list_empty(&old_h->live_brts));
    }

    // If application did not close all DBs using the new file, then there should 
    // be no zombies and we need to redirect the DBs back to the original file.
    if (!toku_list_empty(&new_h->live_brts)) {
        struct brt_header *dst_h;
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
toku_dictionary_redirect (const char *dst_fname_in_env, BRT old_brt, TOKUTXN txn) {
// Input args:
//   new file name for dictionary (relative to env)
//   old_brt is a live brt of open handle ({DB, BRT} pair) that currently refers to old dictionary file.
//   (old_brt may be one of many handles to the dictionary.)
//   txn that created the loader
// Requires: 
//   ydb_lock is held.
//   The brt is open.  (which implies there can be no zombies.)
//   The new file must be a valid dictionary.
//   The block size and flags in the new file must match the existing BRT.
//   The new file must already have its descriptor in it (and it must match the existing descriptor).
// Effect:   
//   Open new BRTs (and related header and cachefile) to the new dictionary file with a new FILENUM.
//   Redirect all DBs that point to brts that point to the old file to point to brts that point to the new file.
//   Copy the dictionary id (dict_id) from the header of the original file to the header of the new file.
//   Create a rollback log entry.
//   The original BRT, header, cachefile and file remain unchanged.  They will be cleaned up on commmit.
//   If the txn aborts, then this operation will be undone
    int r;

    struct brt_header * old_h = old_brt->h;

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
        r = toku_txn_note_brt(txn, old_h);  // mark old brt as touched by this txn
        assert_zero(r);
    }

    struct brt_header *new_h;
    r = dictionary_redirect_internal(dst_fname_in_env, old_h, txn, &new_h);
    assert_zero(r);

    // make rollback log entry
    if (txn) {
        assert(!toku_list_empty(&new_h->live_brts));
        r = toku_txn_note_brt(txn, new_h); // mark new brt as touched by this txn

        FILENUM old_filenum = toku_cachefile_filenum(old_h->cf);
        FILENUM new_filenum = toku_cachefile_filenum(new_h->cf);
        r = toku_logger_save_rollback_dictionary_redirect(txn, old_filenum, new_filenum);
        assert_zero(r);

        TXNID xid = toku_txn_get_txnid(txn);
        toku_brt_header_suppress_rollbacks(new_h, txn);
        r = toku_log_suppress_rollback(txn->logger, NULL, 0, new_filenum, xid);
        assert_zero(r);
    }
    
cleanup:
    return r;
}

