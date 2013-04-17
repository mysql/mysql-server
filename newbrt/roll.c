/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
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
    char *fname = fixup_fname(&bs_fname);

    //Remove reference to the fd in the cachetable
    CACHEFILE cf;
    int r;
    if (file_was_open) {  // file was open when toku_brt_remove_on_commit() was called
	r = toku_cachefile_of_filenum(txn->logger->ct, filenum, &cf);
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
    r = unlink(fname);  // pathname relative to cwd
    assert(r==0);
    toku_free(fname);
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
    char *fname = fixup_fname(&bs_fname);

    //Remove reference to the fd in the cachetable
    CACHEFILE cf = NULL;
    int r = toku_cachefile_of_filenum(txn->logger->ct, filenum, &cf);
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
    r = unlink(fname);  // fname is relative to cwd
    assert(r==0);
    toku_free(fname);
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

static int do_insertion (enum brt_msg_type type, FILENUM filenum, BYTESTRING key, BYTESTRING *data, TOKUTXN txn, LSN oplsn) {
    CACHEFILE cf;
    //printf("%s:%d committing insert %s %s\n", __FILE__, __LINE__, key.data, data.data);
    int r = toku_cachefile_of_filenum(txn->logger->ct, filenum, &cf);
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
                             .u.id={toku_fill_dbt(&key_dbt,  key.data,  key.len),
                                    data
                                    ? toku_fill_dbt(&data_dbt, data->data, data->len)
                                    : toku_init_dbt(&data_dbt) }};

        r = toku_brt_root_put_cmd(brt, &brtcmd);
    }
cleanup:
    toku_cachefile_unpin_fd(cf);
    return r;
}


static int do_nothing_with_filenum(TOKUTXN UU(txn), FILENUM UU(filenum)) {
    return 0;
}


int toku_commit_cmdinsert (FILENUM filenum, BYTESTRING key, TOKUTXN txn, YIELDF UU(yield), void *UU(yieldv), LSN oplsn) {
#if TOKU_DO_COMMIT_CMD_INSERT
    return do_insertion (BRT_COMMIT_ANY, filenum, key, 0, txn, oplsn);
#else
    key = key; oplsn = oplsn;
    return do_nothing_with_filenum(txn, filenum);
#endif
}

int
toku_commit_cmdinsertboth (FILENUM    filenum,
			   BYTESTRING key,
			   BYTESTRING data,
			   TOKUTXN    txn,
			   YIELDF     UU(yield),
			   void *     UU(yieldv),
                           LSN        oplsn)
{
#if TOKU_DO_COMMIT_CMD_INSERT
    return do_insertion (BRT_COMMIT_BOTH, filenum, key, &data, txn, oplsn);
#else
    key = key; data = data; oplsn = oplsn;
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
    return do_insertion (BRT_ABORT_ANY, filenum, key, 0, txn, oplsn);
}

int
toku_rollback_cmdinsertboth (FILENUM    filenum,
			     BYTESTRING key,
			     BYTESTRING data,
			     TOKUTXN    txn,
			     YIELDF     UU(yield),
			     void *     UU(yieldv),
                             LSN        oplsn)
{
    return do_insertion (BRT_ABORT_BOTH, filenum, key, &data, txn, oplsn);
}

int
toku_commit_cmddeleteboth (FILENUM    filenum,
			   BYTESTRING key,
			   BYTESTRING data,
			   TOKUTXN    txn,
			   YIELDF     UU(yield),
			   void *     UU(yieldv),
                           LSN        oplsn)
{
#if TOKU_DO_COMMIT_CMD_DELETE_BOTH
    return do_insertion (BRT_COMMIT_BOTH, filenum, key, &data, txn, oplsn);
#else
    xid = xid; key = key; data = data;
    return do_nothing_with_filenum(txn, filenum);
#endif
}

int
toku_rollback_cmddeleteboth (FILENUM    filenum,
			     BYTESTRING key,
			     BYTESTRING data,
			     TOKUTXN    txn,
			     YIELDF     UU(yield),
			     void *     UU(yieldv),
                             LSN        oplsn)
{
    return do_insertion (BRT_ABORT_BOTH, filenum, key, &data, txn, oplsn);
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
    return do_insertion (BRT_COMMIT_ANY, filenum, key, 0, txn, oplsn);
#else
    xid = xid; key = key;
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
    return do_insertion (BRT_ABORT_ANY, filenum, key, 0, txn, oplsn);
}

int
toku_commit_fileentries (int        fd,
			 TOKUTXN    txn,
			 YIELDF     yield,
			 void *     yieldv,
                         LSN        oplsn)
{
    BREAD f = create_bread_from_fd_initialize_at(fd);
    int r=0;
    MEMARENA ma = memarena_create();
    int count=0;
    while (bread_has_more(f)) {
        struct roll_entry *item;
        r = toku_read_rollback_backwards(f, &item, ma);
        if (r!=0) goto finish;
        r = toku_commit_rollback_item(txn, item, yield, yieldv, oplsn);
        if (r!=0) goto finish;
	memarena_clear(ma);
	count++;
	if (count%2==0) yield(NULL, yieldv); 
    }
 finish:
    { int r2 = close_bread_without_closing_fd(f); assert(r2==0); }
    memarena_close(&ma);
    return r;
}

int
toku_rollback_fileentries (int        fd,
			   TOKUTXN    txn,
			   YIELDF     yield,
			   void *     yieldv,
                           LSN        oplsn)
{
    BREAD f = create_bread_from_fd_initialize_at(fd);
    assert(f);
    int r=0;
    MEMARENA ma = memarena_create();
    int count=0;
    while (bread_has_more(f)) {
        struct roll_entry *item;
        r = toku_read_rollback_backwards(f, &item, ma);
        if (r!=0) goto finish;
        r = toku_abort_rollback_item(txn, item, yield, yieldv, oplsn);
        if (r!=0) goto finish;
	memarena_clear(ma);
	count++;
	if (count%2==0) yield(NULL, yieldv); 
    }
 finish:
    { int r2 = close_bread_without_closing_fd(f); assert(r2==0); }
    memarena_close(&ma);
    return r;
}

int
toku_commit_rollinclude (BYTESTRING bs,
			 TOKUTXN    txn,
			 YIELDF     yield,
			 void *     yieldv,
                         LSN        oplsn) {
    int r;
    char *fname = fixup_fname(&bs);
    int fd = open(fname, O_RDONLY+O_BINARY);
    assert(fd>=0);
    r = toku_commit_fileentries(fd, txn, yield, yieldv, oplsn);
    assert(r==0);
    r = close(fd);
    assert(r==0);
    unlink(fname);
    toku_free(fname);
    return 0;
}

int
toku_rollback_rollinclude (BYTESTRING bs,
			   TOKUTXN    txn,
			   YIELDF     yield,
			   void *     yieldv,
                           LSN        oplsn)
{
    int r;
    char *fname = fixup_fname(&bs);
    int fd = open(fname, O_RDONLY+O_BINARY);
    assert(fd>=0);
    r = toku_rollback_fileentries(fd, txn, yield, yieldv, oplsn);
    assert(r==0);
    r = close(fd);
    assert(r==0);
    unlink(fname);
    toku_free(fname);
    return 0;
}

int
toku_rollback_tablelock_on_empty_table (FILENUM filenum,
                                        TOKUTXN txn,
                                        YIELDF  yield,
                                        void*   yield_v,
                                        LSN     UU(oplsn))
{
    //TODO: Replace truncate function with something that doesn't need to mess with checkpoints.
    // on rollback we have to make the file be empty, since we locked an empty table, and then may have done things to it.

    CACHEFILE cf;
    //printf("%s:%d committing insert %s %s\n", __FILE__, __LINE__, key.data, data.data);
    int r = toku_cachefile_of_filenum(txn->logger->ct, filenum, &cf);
    assert(r==0);

    OMTVALUE brtv=NULL;
    r = toku_omt_find_zero(txn->open_brts, find_brt_from_filenum, &filenum, &brtv, NULL, NULL);
    if (r==0) {
	// If r!=0 it could be because we grabbed a log on an empty table that doesn't even exist, and we never put anything into it.
	// So, just don't do anything in this case.
	BRT brt = brtv;
        toku_poll_txn_progress_function(txn, FALSE, TRUE);
	yield(toku_checkpoint_safe_client_lock, yield_v);
        toku_poll_txn_progress_function(txn, FALSE, FALSE);
	r = toku_brt_truncate(brt);
	assert(r==0);
	toku_checkpoint_safe_client_unlock();
    }

    return r; 
}

int
toku_commit_tablelock_on_empty_table (FILENUM filenum, TOKUTXN txn, YIELDF UU(yield), void* UU(yield_v), LSN UU(oplsn))
{
    return do_nothing_with_filenum(txn, filenum);
}

int
toku_commit_load (BYTESTRING UU(old_iname),
                  BYTESTRING UU(new_iname),
                  TOKUTXN    UU(txn),
                  YIELDF     UU(yield),
                  void      *UU(yield_v),
                  LSN        UU(oplsn))
{
    // TODO 2216: need to implement
    assert(1);
    return 0;
}

int
toku_rollback_load (BYTESTRING UU(old_iname),
                    BYTESTRING UU(new_iname),
                    TOKUTXN    UU(txn),
                    YIELDF     UU(yield),
                    void      *UU(yield_v),
                    LSN        UU(oplsn)) 
{
    // TODO 2216: need to implement
    assert(1);
    return 0;
}


int
toku_commit_dictionary_redirect (FILENUM UU(old_filenum),
			         FILENUM UU(new_filenum),
                                 TOKUTXN UU(txn),
                                 YIELDF  UU(yield),
                                 void *  UU(yield_v),
                                 LSN     UU(oplsn)) //oplsn is the lsn of the commit
{
    //NO-OP
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
    return r;
}

