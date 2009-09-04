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
toku_commit_fcreate (TXNID UU(xid),
                     FILENUM UU(filenum),
		     BYTESTRING UU(bs_fname),
		     TOKUTXN    UU(txn),
		     YIELDF     UU(yield),
		     void      *UU(yield_v))
{
    return 0;
}

int
toku_rollback_fcreate (TXNID      UU(xid),
                       FILENUM    filenum,
		       BYTESTRING bs_fname,
		       TOKUTXN    txn,
		       YIELDF     yield,
		       void*      yield_v)
{
    yield(toku_checkpoint_safe_client_lock, yield_v);
    char *fname = fixup_fname(&bs_fname);
    char *directory = txn->logger->directory;
    int  full_len=strlen(fname)+strlen(directory)+2;
    char full_fname[full_len];
    int l = snprintf(full_fname,full_len, "%s/%s", directory, fname);
    assert(l+1 == full_len);
    //Remove reference to the fd in the cachetable
    CACHEFILE cf;
    int r = toku_cachefile_of_filenum(txn->logger->ct, filenum, &cf);
    if (r == 0) {
        r = toku_cachefile_redirect_nullfd(cf);
        assert(r==0);
    }
    r = unlink(full_fname);
    assert(r==0);
    toku_free(fname);
    toku_checkpoint_safe_client_unlock();
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

static int do_insertion (enum brt_msg_type type, FILENUM filenum, BYTESTRING key, BYTESTRING *data,TOKUTXN txn) {
    CACHEFILE cf;
    //printf("%s:%d committing insert %s %s\n", __FILE__, __LINE__, key.data, data.data);
    int r = toku_cachefile_of_filenum(txn->logger->ct, filenum, &cf);
    assert(r==0);

    DBT key_dbt,data_dbt;
    XIDS xids = toku_txn_get_xids(txn);
    BRT_MSG_S brtcmd = { type, xids,
			 .u.id={toku_fill_dbt(&key_dbt,  key.data,  key.len),
				data
				? toku_fill_dbt(&data_dbt, data->data, data->len)
				: toku_init_dbt(&data_dbt) }};
    OMTVALUE brtv=NULL;
    r = toku_omt_find_zero(txn->open_brts, find_brt_from_filenum, &filenum, &brtv, NULL, NULL);

    assert(r==0);
    BRT brt = brtv;
    r = toku_brt_root_put_cmd(brt, &brtcmd, txn->logger);
    return r;
}


static int do_nothing_with_filenum(TOKUTXN txn, FILENUM filenum) {
    CACHEFILE cf;
    int r = toku_cachefile_of_filenum(txn->logger->ct, filenum, &cf);
    assert(r==0);
    return r;
}


int toku_commit_cmdinsert (FILENUM filenum, BYTESTRING key, TOKUTXN txn, YIELDF UU(yield), void *UU(yieldv)) {
#if TOKU_DO_COMMIT_CMD_INSERT
    return do_insertion (BRT_COMMIT_ANY, filenum, key, 0, txn);
#else
    key = key;
    return do_nothing_with_filenum(txn, filenum);
#endif
}

int
toku_commit_cmdinsertboth (FILENUM    filenum,
			   BYTESTRING key,
			   BYTESTRING data,
			   TOKUTXN    txn,
			   YIELDF     UU(yield),
			   void *     UU(yieldv))
{
#if TOKU_DO_COMMIT_CMD_INSERT
    return do_insertion (BRT_COMMIT_BOTH, filenum, key, &data, txn);
#else
    key = key; data = data;
    return do_nothing_with_filenum(txn, filenum);
#endif
}

int
toku_rollback_cmdinsert (FILENUM    filenum,
			 BYTESTRING key,
			 TOKUTXN    txn,
			 YIELDF     UU(yield),
			 void *     UU(yieldv))
{
    return do_insertion (BRT_ABORT_ANY, filenum, key, 0, txn);
}

int
toku_rollback_cmdinsertboth (FILENUM    filenum,
			     BYTESTRING key,
			     BYTESTRING data,
			     TOKUTXN    txn,
			     YIELDF     UU(yield),
			     void *     UU(yieldv))
{
    return do_insertion (BRT_ABORT_BOTH, filenum, key, &data, txn);
}

int
toku_commit_cmddeleteboth (FILENUM    filenum,
			   BYTESTRING key,
			   BYTESTRING data,
			   TOKUTXN    txn,
			   YIELDF     UU(yield),
			   void *     UU(yieldv))
{
#if TOKU_DO_COMMIT_CMD_DELETE_BOTH
    return do_insertion (BRT_COMMIT_BOTH, filenum, key, &data, txn);
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
			     void *     UU(yieldv))
{
    return do_insertion (BRT_ABORT_BOTH, filenum, key, &data, txn);
}

int
toku_commit_cmddelete (FILENUM filenum,
		       BYTESTRING key,
		       TOKUTXN txn,
		       YIELDF     UU(yield),
		       void *     UU(yieldv))
{
#if TOKU_DO_COMMIT_CMD_DELETE
    return do_insertion (BRT_COMMIT_ANY, filenum, key, 0, txn);
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
			 void *     UU(yieldv))
{
    return do_insertion (BRT_ABORT_ANY, filenum, key, 0, txn);
}

int
toku_commit_fileentries (int        fd,
			 TOKUTXN    txn,
			 YIELDF     yield,
			 void *     yieldv)
{
    BREAD f = create_bread_from_fd_initialize_at(fd);
    int r=0;
    MEMARENA ma = memarena_create();
    int count=0;
    while (bread_has_more(f)) {
        struct roll_entry *item;
        r = toku_read_rollback_backwards(f, &item, ma);
        if (r!=0) goto finish;
        r = toku_commit_rollback_item(txn, item, yield, yieldv);
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
			   void *     yieldv)
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
        r = toku_abort_rollback_item(txn, item, yield, yieldv);
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
			 void *     yieldv) {
    int r;
    char *fname = fixup_fname(&bs);
    int fd = open(fname, O_RDONLY+O_BINARY);
    assert(fd>=0);
    r = toku_commit_fileentries(fd, txn, yield, yieldv);
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
			   void *     yieldv)
{
    int r;
    char *fname = fixup_fname(&bs);
    int fd = open(fname, O_RDONLY+O_BINARY);
    assert(fd>=0);
    r = toku_rollback_fileentries(fd, txn, yield, yieldv);
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
                                        YIELDF yield,
                                        void* yield_v)
{
    yield(toku_checkpoint_safe_client_lock, yield_v);
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
	r = toku_brt_truncate(brt);
	assert(r==0);
    }
    toku_checkpoint_safe_client_unlock();

    return r; 
}

int
toku_commit_tablelock_on_empty_table (FILENUM filenum, TOKUTXN txn, YIELDF UU(yield), void* UU(yield_v))
{
    return do_nothing_with_filenum(txn, filenum);
}
