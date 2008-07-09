/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

/* rollback and rollforward routines. */

#include <stdlib.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <unistd.h>

#include "log_header.h"
#include "log-internal.h"
#include "cachetable.h"
#include "key.h"
#include "bread.h"

// these flags control whether or not we send commit messages for
// various operations
#define TOKU_DO_COMMIT_CMD_INSERT 0
#define TOKU_DO_COMMIT_CMD_DELETE 1
#define TOKU_DO_COMMIT_CMD_DELETE_BOTH 1

int toku_commit_fcreate (TXNID xid __attribute__((__unused__)),
			 BYTESTRING bs_fname __attribute__((__unused__)),
			 TOKUTXN    txn       __attribute__((__unused__))) {
    return 0;
}

int toku_rollback_fcreate (TXNID xid __attribute__((__unused__)),
			   BYTESTRING bs_fname,
			   TOKUTXN    txn       __attribute__((__unused__))) {
    char *fname = fixup_fname(&bs_fname);
    char *directory = txn->logger->directory;
    int  full_len=strlen(fname)+strlen(directory)+2;
    char full_fname[full_len];
    int l = snprintf(full_fname,full_len, "%s/%s", directory, fname);
    assert(l<=full_len);
    int r = unlink(full_fname);
    assert(r==0);
    free(fname);
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

static int do_insertion (enum brt_cmd_type type, TXNID xid, FILENUM filenum, BYTESTRING key, BYTESTRING *data,TOKUTXN txn) {
    CACHEFILE cf;
    //printf("%s:%d committing insert %s %s\n", __FILE__, __LINE__, key.data, data.data);
    int r = toku_cachefile_of_filenum(txn->logger->ct, filenum, &cf);
    assert(r==0);

    DBT key_dbt,data_dbt;
    BRT_CMD_S brtcmd = { type, xid,
			 .u.id={toku_fill_dbt(&key_dbt,  key.data,  key.len),
				data
				? toku_fill_dbt(&data_dbt, data->data, data->len)
				: toku_init_dbt(&data_dbt) }};
    OMTVALUE brtv=NULL;
    r = toku_omt_find_zero(txn->open_brts, find_brt_from_filenum, &filenum, &brtv, NULL, NULL);

    if (r==DB_NOTFOUND) {
	r = toku_cachefile_root_put_cmd(cf, &brtcmd, toku_txn_logger(txn));
	if (r!=0) return r;
    } else {
	assert(r==0);
	BRT brt = brtv;
	r = toku_brt_root_put_cmd(brt, &brtcmd, txn->logger);
    }
    return toku_cachefile_close(&cf, toku_txn_logger(txn));
}


static int do_nothing_with_filenum(TOKUTXN txn, FILENUM filenum) {
    CACHEFILE cf;
    int r = toku_cachefile_of_filenum(txn->logger->ct, filenum, &cf);
    assert(r==0);
    return toku_cachefile_close(&cf, toku_txn_logger(txn));
}


int toku_commit_cmdinsert (TXNID xid, FILENUM filenum, BYTESTRING key,BYTESTRING data,TOKUTXN txn) {
#if TOKU_DO_COMMIT_CMD_INSERT
    return do_insertion (BRT_COMMIT_BOTH, xid, filenum, key, &data, txn);
#else
    xid = xid; key = key; data = data;
    return do_nothing_with_filenum(txn, filenum);
#endif
}

int toku_rollback_cmdinsert (TXNID xid, FILENUM filenum, BYTESTRING key,BYTESTRING data,TOKUTXN txn) {
    return do_insertion (BRT_ABORT_BOTH, xid, filenum, key, &data, txn);
}

int toku_commit_cmddeleteboth (TXNID xid, FILENUM filenum, BYTESTRING key,BYTESTRING data,TOKUTXN txn) {
#if TOKU_DO_COMMIT_CMD_DELETE_BOTH
    return do_insertion (BRT_COMMIT_BOTH, xid, filenum, key, &data, txn);
#else
    xid = xid; key = key; data = data;
    return do_nothing_with_filenum(txn, filenum);
#endif
}

int toku_rollback_cmddeleteboth (TXNID xid, FILENUM filenum, BYTESTRING key,BYTESTRING data,TOKUTXN txn) {
    return do_insertion (BRT_ABORT_BOTH, xid, filenum, key, &data, txn);
}

int toku_commit_cmddelete (TXNID xid, FILENUM filenum, BYTESTRING key,TOKUTXN txn) {
#if TOKU_DO_COMMIT_CMD_DELETE
    return do_insertion (BRT_COMMIT_ANY, xid, filenum, key, 0, txn);
#else
    xid = xid; key = key;
    return do_nothing_with_filenum(txn, filenum);
#endif
}

int toku_rollback_cmddelete (TXNID xid, FILENUM filenum, BYTESTRING key,TOKUTXN txn) {
    return do_insertion (BRT_ABORT_ANY, xid, filenum, key, 0, txn);
}

int toku_commit_fileentries (int fd, off_t filesize, TOKUTXN txn) {
    BREAD f = create_bread_from_fd_initialize_at(fd, filesize, 1<<20);
    int r=0;
    while (bread_has_more(f)) {
        struct roll_entry *item;
        r = toku_read_rollback_backwards(f, &item);
        if (r!=0) goto finish;
        r = toku_commit_rollback_item(txn, item);
        if (r!=0) goto finish;
    }
 finish:
    { int r2 = close_bread_without_closing_fd(f); assert(r2==0); }
    return r;
}

int toku_rollback_fileentries (int fd, off_t filesize, TOKUTXN txn) {
    BREAD f = create_bread_from_fd_initialize_at(fd, filesize, 1<<20);
    assert(f);
    int r=0;
    while (bread_has_more(f)) {
        struct roll_entry *item;
        r = toku_read_rollback_backwards(f, &item);
        if (r!=0) goto finish;
        r = toku_abort_rollback_item(txn, item);
        if (r!=0) goto finish;
    }
 finish:
    { int r2 = close_bread_without_closing_fd(f); assert(r2==0); }
    return r;
}

int toku_commit_rollinclude (BYTESTRING bs,TOKUTXN txn) {
    int r;
    char *fname = fixup_fname(&bs);
    int fd = open(fname, O_RDONLY);
    assert(fd>=0);
    struct stat statbuf;
    r = fstat(fd, &statbuf);
    assert(r==0);
    r = toku_commit_fileentries(fd, statbuf.st_size, txn);
    assert(r==0);
    r = close(fd);
    assert(r==0);
    unlink(fname);
    free(fname);
    return 0;
}

int toku_rollback_rollinclude (BYTESTRING bs,TOKUTXN txn) {
    int r;
    char *fname = fixup_fname(&bs);
    int fd = open(fname, O_RDONLY);
    assert(fd>=0);
    struct stat statbuf;
    r = fstat(fd, &statbuf);
    assert(r==0);
    r = toku_rollback_fileentries(fd, statbuf.st_size, txn);
    assert(r==0);
    r = close(fd);
    assert(r==0);
    unlink(fname);
    free(fname);
    return 0;
}
