/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/* rollback and rollforward routines. */

#include "includes.h"
#include "checkpoint.h"
#include "xids.h"
#include "roll.h"

int
toku_commit_fdelete (u_int8_t   file_was_open,
                     FILENUM    filenum,    // valid if file_was_open
                     BYTESTRING bs_fname,   // cwd/iname
                     TOKUTXN    txn,
                     YIELDF     UU(yield),
                     void      *UU(yield_v),
                     LSN        UU(oplsn)) //oplsn is the lsn of the commit
{
    //TODO: #2037 verify the file is (user) closed
    //Remove reference to the fd in the cachetable
    CACHEFILE cf;
    int r;
    if (file_was_open) {  // file was open when toku_brt_remove_on_commit() was called
        r = toku_cachefile_of_filenum(txn->logger->ct, filenum, &cf);
        if (r==ENOENT) { //Missing file on recovered transaction is not an error
            assert(txn->recovered_from_checkpoint);
            r = 0;
            goto done;
        }
        assert(r == 0);  // must still be open  (toku_brt_remove_on_commit() incremented refcount)
        {
            (void)toku_cachefile_get_and_pin_fd(cf);
            assert(!toku_cachefile_is_dev_null_unlocked(cf));
            struct brt_header *h = toku_cachefile_get_userdata(cf);
            DICTIONARY_ID dict_id = h->dict_id;
            toku_logger_call_remove_finalize_callback(txn->logger, dict_id);
            toku_cachefile_unpin_fd(cf);
        }
        r = toku_cachefile_redirect_nullfd(cf);
        assert(r==0);
    }
    {
	char *fname_in_env = fixup_fname(&bs_fname);
	char *fname_in_cwd = toku_cachetable_get_fname_in_cwd(txn->logger->ct, fname_in_env);

	r = unlink(fname_in_cwd);
	assert(r==0 || errno==ENOENT);
	toku_free(fname_in_env);
	toku_free(fname_in_cwd);
    }
done:
    return 0;
}

int
toku_rollback_fdelete (u_int8_t   UU(file_was_open),
                       FILENUM    UU(filenum),
                       BYTESTRING UU(bs_fname),
                       TOKUTXN    UU(txn),
                       YIELDF     UU(yield),
                       void*      UU(yield_v),
                       LSN        UU(oplsn)) //oplsn is the lsn of the abort
{
    //Rolling back an fdelete is an no-op.
    return 0;
}

int
toku_commit_fcreate (FILENUM UU(filenum),
                     BYTESTRING UU(bs_fname),
                     TOKUTXN    UU(txn),
                     YIELDF     UU(yield),
                     void      *UU(yield_v),
                     LSN        UU(oplsn))
{
    return 0;
}

int
toku_rollback_fcreate (FILENUM    filenum,
                       BYTESTRING bs_fname,  // cwd/iname
                       TOKUTXN    txn,
                       YIELDF     UU(yield),
                       void*      UU(yield_v),
                       LSN        UU(oplsn))
{
    //TODO: #2037 verify the file is (user) closed

    //Remove reference to the fd in the cachetable
    CACHEFILE cf = NULL;
    int r = toku_cachefile_of_filenum(txn->logger->ct, filenum, &cf);
    if (r==ENOENT) { //Missing file on recovered transaction is not an error
        assert(txn->recovered_from_checkpoint);
        r = 0;
        goto done;
    }
    assert(r == 0);
    {
        (void)toku_cachefile_get_and_pin_fd(cf);
        assert(!toku_cachefile_is_dev_null_unlocked(cf));
        struct brt_header *h = toku_cachefile_get_userdata(cf);
        DICTIONARY_ID dict_id = h->dict_id;
        toku_logger_call_remove_finalize_callback(txn->logger, dict_id);
        toku_cachefile_unpin_fd(cf);
    }
    r = toku_cachefile_redirect_nullfd(cf);
    assert(r==0);

    {
	char *fname_in_env = fixup_fname(&bs_fname);
	char *fname_in_cwd = toku_cachetable_get_fname_in_cwd(txn->logger->ct, fname_in_env);

	r = unlink(fname_in_cwd);
	assert(r==0 || errno==ENOENT);
	toku_free(fname_in_env);
	toku_free(fname_in_cwd);
    }
done:
    return 0;
}

static int find_brt_from_filenum (OMTVALUE v, void *filenumvp) {
    FILENUM *filenump=filenumvp;
    BRT brt = v;
    FILENUM thisfnum = toku_cachefile_filenum(brt->cf);
    if (thisfnum.fileid<filenump->fileid) return -1;
    if (thisfnum.fileid>filenump->fileid) return +1;
    return 0;
}


// Input arg reset_root_xid_that_created TRUE means that this operation has changed the definition of this dictionary.
// (Example use is for schema change committed with txn that inserted cmdupdatebroadcast message.)
static int do_insertion (enum brt_msg_type type, FILENUM filenum, BYTESTRING key, BYTESTRING *data, TOKUTXN txn, LSN oplsn,
			 BOOL reset_root_xid_that_created) {
    CACHEFILE cf;
    // 2954 - ignore messages for aborted hot-index
    int r = toku_txn_ignore_contains(txn, filenum);
    if ( r != ENOENT ) goto done;  // ENOENT => filenum not in ignore list
    //printf("%s:%d committing insert %s %s\n", __FILE__, __LINE__, key.data, data.data);
    r = toku_cachefile_of_filenum(txn->logger->ct, filenum, &cf);
    if (r==ENOENT) { //Missing file on recovered transaction is not an error
        assert(txn->recovered_from_checkpoint);
        r = 0;
        goto done;
    }
    assert(r==0);

    (void)toku_cachefile_get_and_pin_fd(cf);
    if (!toku_cachefile_is_dev_null_unlocked(cf)) {
        OMTVALUE brtv=NULL;
        r = toku_omt_find_zero(txn->open_brts, find_brt_from_filenum, &filenum, &brtv, NULL, NULL);
        assert(r==0);
        BRT brt = brtv;

        LSN treelsn = toku_brt_checkpoint_lsn(brt);
        if (oplsn.lsn != 0 && oplsn.lsn <= treelsn.lsn) {
            r = 0;
            goto cleanup;
        }

        DBT key_dbt,data_dbt;
        XIDS xids = toku_txn_get_xids(txn);
        BRT_MSG_S brtcmd = { type, xids,
                             .u.id={(key.len > 0)
                                    ? toku_fill_dbt(&key_dbt,  key.data,  key.len)
                                    : toku_init_dbt(&key_dbt),
                                    data
                                    ? toku_fill_dbt(&data_dbt, data->data, data->len)
                                    : toku_init_dbt(&data_dbt) }};

        r = toku_brt_root_put_cmd(brt, &brtcmd);
	if (r == 0 && reset_root_xid_that_created) {
	    TXNID new_root_xid_that_created = xids_get_outermost_xid(xids);
	    toku_reset_root_xid_that_created(brt, new_root_xid_that_created);
	}
    }
cleanup:
    toku_cachefile_unpin_fd(cf);
done:
    return r;
}


static int do_nothing_with_filenum(TOKUTXN UU(txn), FILENUM UU(filenum)) {
    return 0;
}


int toku_commit_cmdinsert (FILENUM filenum, BYTESTRING key, TOKUTXN txn, YIELDF UU(yield), void *UU(yieldv), LSN oplsn) {
#if TOKU_DO_COMMIT_CMD_INSERT
    return do_insertion (BRT_COMMIT_ANY, filenum, key, 0, txn, oplsn, FALSE);
#else
    key = key; oplsn = oplsn;
    return do_nothing_with_filenum(txn, filenum);
#endif
}

int
toku_rollback_cmdinsert (FILENUM    filenum,
                         BYTESTRING key,
                         TOKUTXN    txn,
                         YIELDF     UU(yield),
                         void *     UU(yieldv),
                         LSN        oplsn)
{
    return do_insertion (BRT_ABORT_ANY, filenum, key, 0, txn, oplsn, FALSE);
}

int
toku_commit_cmdupdate(FILENUM    filenum,
                      BYTESTRING key,
                      TOKUTXN    txn,
                      YIELDF     UU(yield),
                      void *     UU(yieldv),
                      LSN        oplsn)
{
    return do_insertion(BRT_COMMIT_ANY, filenum, key, 0, txn, oplsn, FALSE);
}

int
toku_rollback_cmdupdate(FILENUM    filenum,
                        BYTESTRING key,
                        TOKUTXN    txn,
                        YIELDF     UU(yield),
                        void *     UU(yieldv),
                        LSN        oplsn)
{
    return do_insertion(BRT_ABORT_ANY, filenum, key, 0, txn, oplsn, FALSE);
}

int
toku_commit_cmdupdatebroadcast(FILENUM    filenum,
                               BOOL       is_resetting_op,
                               TOKUTXN    txn,
                               YIELDF     UU(yield),
                               void *     UU(yieldv),
                               LSN        oplsn)
{
    // if is_resetting_op, reset root_xid_that_created in
    // relevant brtheader.
    BOOL reset_root_xid_that_created = (is_resetting_op ? TRUE : FALSE);
    const enum brt_msg_type msg_type = (is_resetting_op
                                        ? BRT_COMMIT_BROADCAST_ALL
                                        : BRT_COMMIT_BROADCAST_TXN);
    BYTESTRING nullkey = { 0, NULL };
    return do_insertion(msg_type, filenum, nullkey, 0, txn, oplsn, reset_root_xid_that_created);
}

int
toku_rollback_cmdupdatebroadcast(FILENUM    filenum,
                                 BOOL       UU(is_resetting_op),
                                 TOKUTXN    txn,
                                 YIELDF     UU(yield),
                                 void *     UU(yieldv),
                                 LSN        oplsn)
{
    BYTESTRING nullkey = { 0, NULL };
    return do_insertion(BRT_ABORT_BROADCAST_TXN, filenum, nullkey, 0, txn, oplsn, FALSE);
}

int
toku_commit_cmddelete (FILENUM    filenum,
                       BYTESTRING key,
                       TOKUTXN    txn,
                       YIELDF     UU(yield),
                       void *     UU(yieldv),
                       LSN        oplsn)
{
#if TOKU_DO_COMMIT_CMD_DELETE
    return do_insertion (BRT_COMMIT_ANY, filenum, key, 0, txn, oplsn, FALSE);
#else
    key = key; oplsn = oplsn;
    return do_nothing_with_filenum(txn, filenum);
#endif
}

int
toku_rollback_cmddelete (FILENUM    filenum,
                         BYTESTRING key,
                         TOKUTXN    txn,
                         YIELDF     UU(yield),
                         void *     UU(yieldv),
                         LSN        oplsn)
{
    return do_insertion (BRT_ABORT_ANY, filenum, key, 0, txn, oplsn, FALSE);
}

static int
toku_apply_rollinclude (TXNID      xid,
                        uint64_t   num_nodes,
                        BLOCKNUM   spilled_head,
                        uint32_t   spilled_head_hash,
                        BLOCKNUM   spilled_tail,
                        uint32_t   spilled_tail_hash,
                        TOKUTXN    txn,
                        YIELDF     yield,
                        void *     yieldv,
                        LSN        oplsn,
                        apply_rollback_item func) {
    int r;
    struct roll_entry *item;
    int count=0;

    BLOCKNUM next_log      = spilled_tail;
    uint32_t next_log_hash = spilled_tail_hash;
    uint64_t last_sequence = num_nodes;

    BOOL found_head = FALSE;
    assert(next_log.b != ROLLBACK_NONE.b);
    while (next_log.b != ROLLBACK_NONE.b) {
        ROLLBACK_LOG_NODE log;
        //pin log
        r = toku_get_and_pin_rollback_log(txn, xid, last_sequence - 1, next_log, next_log_hash, &log);
        assert(r==0);
        last_sequence = log->sequence;
        
        r = toku_maybe_prefetch_older_rollback_log(txn, log);
        assert(r==0);

        while ((item=log->newest_logentry)) {
            log->newest_logentry = item->prev;
            r = func(txn, item, yield, yieldv, oplsn);
            if (r!=0) return r;
            count++;
            if (count%2 == 0) yield(NULL, NULL, yieldv);
        }
        if (next_log.b == spilled_head.b) {
            assert(!found_head);
            found_head = TRUE;
            assert(log->sequence == 0);
        }
        next_log      = log->older;
        next_log_hash = log->older_hash;
        {
            //Clean up transaction structure to prevent
            //toku_txn_close from double-freeing
            spilled_tail      = next_log;
            spilled_tail_hash = next_log_hash;
            if (found_head) {
                assert(next_log.b == ROLLBACK_NONE.b);
                spilled_head      = next_log;
                spilled_head_hash = next_log_hash;
            }
        }
        //Unpins log
        r = toku_delete_rollback_log(txn, log);
        assert(r==0);
    }
    return r;
}

int
toku_commit_rollinclude (TXNID      xid,
                         uint64_t   num_nodes,
                         BLOCKNUM   spilled_head,
                         uint32_t   spilled_head_hash,
                         BLOCKNUM   spilled_tail,
                         uint32_t   spilled_tail_hash,
                         TOKUTXN    txn,
                         YIELDF     yield,
                         void *     yieldv,
                         LSN        oplsn) {
    int r;
    r = toku_apply_rollinclude(xid, num_nodes,
                               spilled_head, spilled_head_hash,
                               spilled_tail, spilled_tail_hash,
                               txn, yield, yieldv, oplsn,
                               toku_commit_rollback_item);
    return r;
}

int
toku_rollback_rollinclude (TXNID      xid,
                           uint64_t   num_nodes,
                           BLOCKNUM   spilled_head,
                           uint32_t   spilled_head_hash,
                           BLOCKNUM   spilled_tail,
                           uint32_t   spilled_tail_hash,
                           TOKUTXN    txn,
                           YIELDF     yield,
                           void *     yieldv,
                           LSN        oplsn) {
    int r;
    r = toku_apply_rollinclude(xid, num_nodes,
                               spilled_head, spilled_head_hash,
                               spilled_tail, spilled_tail_hash,
                               txn, yield, yieldv, oplsn,
                               toku_abort_rollback_item);
    return r;
}

int
toku_commit_load (BYTESTRING old_iname,
                  BYTESTRING UU(new_iname),
                  TOKUTXN    txn,
                  YIELDF     UU(yield),
                  void      *UU(yield_v),
                  LSN        UU(oplsn))
{
    CACHEFILE cf;
    int r;
    char *fname_in_env = fixup_fname(&old_iname); //Delete old file
    r = toku_cachefile_of_iname_in_env(txn->logger->ct, fname_in_env, &cf);
    if (r==0) {
        r = toku_cachefile_redirect_nullfd(cf);
        assert(r==0);
    }
    else {
        assert(r==ENOENT);
    }
    char *fname_in_cwd = toku_cachetable_get_fname_in_cwd(txn->logger->ct, fname_in_env);
    r = unlink(fname_in_cwd);
    assert(r==0 || errno==ENOENT);
    toku_free(fname_in_env);
    toku_free(fname_in_cwd);
    return 0;
}

int
toku_rollback_load (BYTESTRING UU(old_iname),
                    BYTESTRING new_iname,
                    TOKUTXN    txn,
                    YIELDF     UU(yield),
                    void      *UU(yield_v),
                    LSN        UU(oplsn)) 
{
    CACHEFILE cf;
    int r;
    char *fname_in_env = fixup_fname(&new_iname); //Delete new file
    r = toku_cachefile_of_iname_in_env(txn->logger->ct, fname_in_env, &cf);
    if (r==0) {
        r = toku_cachefile_redirect_nullfd(cf);
        assert(r==0);
    }
    else {
        assert(r==ENOENT);
    }
    char *fname_in_cwd = toku_cachetable_get_fname_in_cwd(txn->logger->ct, fname_in_env);
    r = unlink(fname_in_cwd);
    assert(r==0 || errno==ENOENT);
    toku_free(fname_in_env);
    toku_free(fname_in_cwd);
    return 0;
}

//2954
int
toku_commit_hot_index (FILENUMS UU(hot_index_filenums),
                       TOKUTXN  UU(txn), 
                       YIELDF   UU(yield), 
                       void *   UU(yield_v), 
                       LSN      UU(oplsn))
{
    // nothing
    return 0;
}

//2954
// function called by toku_omt_iterate to add hot_index filenums to
//  each live txn's ignore list when a hot index is aborted
static int 
live_txn_ignore(OMTVALUE vtxn, u_int32_t UU(idx) , void *vfn) {
    TOKUTXN                  txn = vtxn;
    FILENUMS *hot_index_filenums = vfn;
    int r;
    for (uint32_t i=0; i<hot_index_filenums->num;i++) {
        r = toku_txn_ignore_add(txn, hot_index_filenums->filenums[i]);
        invariant(r==0);
    }
    return 0;
}

int
toku_rollback_hot_index (FILENUMS UU(hot_index_filenums),
                         TOKUTXN  UU(txn), 
                         YIELDF   UU(yield), 
                         void *   UU(yield_v), 
                         LSN      UU(oplsn))
{
    int r = toku_omt_iterate(txn->logger->live_txns, live_txn_ignore, &hot_index_filenums);
    return r;
}

int
toku_commit_dictionary_redirect (FILENUM UU(old_filenum),
                                 FILENUM UU(new_filenum),
                                 TOKUTXN UU(txn),
                                 YIELDF  UU(yield),
                                 void *  UU(yield_v),
                                 LSN     UU(oplsn)) //oplsn is the lsn of the commit
{
    //Redirect only has meaning during normal operation (NOT during recovery).
    if (!txn->recovered_from_checkpoint) {
        //NO-OP
    }
    return 0;
}

int
toku_rollback_dictionary_redirect (FILENUM old_filenum,
                                   FILENUM new_filenum,
                                   TOKUTXN txn,
                                   YIELDF  UU(yield),
                                   void *  UU(yield_v),
                                   LSN     UU(oplsn)) //oplsn is the lsn of the abort
{
    int r = 0;
    //Redirect only has meaning during normal operation (NOT during recovery).
    if (!txn->recovered_from_checkpoint) {
        CACHEFILE new_cf = NULL;
        r = toku_cachefile_of_filenum(txn->logger->ct, new_filenum, &new_cf);
        assert(r == 0);
        struct brt_header *new_h = toku_cachefile_get_userdata(new_cf);

        CACHEFILE old_cf = NULL;
        r = toku_cachefile_of_filenum(txn->logger->ct, old_filenum, &old_cf);
        assert(r == 0);
        struct brt_header *old_h = toku_cachefile_get_userdata(old_cf);

        //Redirect back from new to old.
        r = toku_dictionary_redirect_abort(old_h, new_h, txn);
        assert(r==0);
    }
    return r;
}

int
toku_commit_change_fdescriptor(FILENUM    filenum,
                               BYTESTRING UU(old_descriptor),
                               TOKUTXN    txn,
                               YIELDF     UU(yield),
                               void *     UU(yieldv),
                               LSN        UU(oplsn))
{
    return do_nothing_with_filenum(txn, filenum);
}

int
toku_rollback_change_fdescriptor(FILENUM    filenum,
                               BYTESTRING old_descriptor,
                               TOKUTXN    txn,
                               YIELDF     UU(yield),
                               void *     UU(yieldv),
                               LSN        UU(oplsn))
{
    CACHEFILE cf;
    int r;
    int fd;
    r = toku_cachefile_of_filenum(txn->logger->ct, filenum, &cf);
    if (r==ENOENT) { //Missing file on recovered transaction is not an error
        assert(txn->recovered_from_checkpoint);
        r = 0;
        goto done;
    }
    assert(r==0);

    fd = toku_cachefile_get_and_pin_fd(cf);
    if (!toku_cachefile_is_dev_null_unlocked(cf)) {
        OMTVALUE brtv=NULL;
        r = toku_omt_find_zero(txn->open_brts, find_brt_from_filenum, &filenum, &brtv, NULL, NULL);
        assert(r==0);
        BRT brt = brtv;
        DESCRIPTOR_S d;

        toku_fill_dbt(&d.dbt,  old_descriptor.data,  old_descriptor.len);
        r = toku_update_descriptor(brt->h, &d, fd);
        assert(r == 0);
    }
    toku_cachefile_unpin_fd(cf);
done:
    return r;
}



