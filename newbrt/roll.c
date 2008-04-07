/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

/* rollback and rollforward routines. */

#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>

#include "log_header.h"
#include "log-internal.h"
#include "cachetable.h"
#include "key.h"

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

int toku_commit_cmdinsert (TXNID xid, FILENUM filenum, BYTESTRING key,BYTESTRING data,TOKUTXN txn) {
    CACHEFILE cf;
    BRT brt;
    //printf("%s:%d committing insert %s %s\n", __FILE__, __LINE__, key.data, data.data);
    int r = toku_cachefile_of_filenum(txn->logger->ct, filenum, &cf, &brt);
    assert(r==0);
    DBT key_dbt,data_dbt;
    BRT_CMD_S brtcmd = { BRT_COMMIT_BOTH, xid,
			 .u.id={toku_fill_dbt(&key_dbt,  key.data,  key.len),
				toku_fill_dbt(&data_dbt, data.data, data.len)}};
    return toku_brt_root_put_cmd(brt, &brtcmd, toku_txn_logger(txn));
}

int toku_rollback_cmdinsert (TXNID xid, FILENUM filenum, BYTESTRING key,BYTESTRING data,TOKUTXN txn) {
    CACHEFILE cf;
    BRT brt;
    int r = toku_cachefile_of_filenum(txn->logger->ct, filenum, &cf, &brt);
    assert(r==0);
    //printf("%s:%d aborting insert %s %s\n", __FILE__, __LINE__, key.data, data.data);
    DBT key_dbt,data_dbt;
    BRT_CMD_S brtcmd = { BRT_ABORT_BOTH, xid,
			 .u.id={toku_fill_dbt(&key_dbt,  key.data,  key.len),
				toku_fill_dbt(&data_dbt, data.data, data.len)}};
    return toku_brt_root_put_cmd(brt, &brtcmd, toku_txn_logger(txn));
}

int toku_commit_cmddeleteboth (TXNID xid, FILENUM filenum, BYTESTRING key,BYTESTRING data,TOKUTXN txn) {
    return toku_commit_cmdinsert(xid, filenum, key, data, txn);
}

int toku_rollback_cmddeleteboth (TXNID xid, FILENUM filenum, BYTESTRING key,BYTESTRING data,TOKUTXN txn) {
    return toku_rollback_cmdinsert(xid, filenum, key, data, txn);
}

int toku_commit_cmddelete (TXNID xid, FILENUM filenum, BYTESTRING key,TOKUTXN txn) {
    CACHEFILE cf;
    BRT brt;
    int r = toku_cachefile_of_filenum(txn->logger->ct, filenum, &cf, &brt);
    assert(r==0);
    //printf("%s:%d aborting delete %s %s\n", __FILE__, __LINE__, key.data, data.data);
    DBT key_dbt,data_dbt;
    BRT_CMD_S brtcmd = { BRT_COMMIT_ANY, xid,
			 .u.id={toku_fill_dbt(&key_dbt,  key.data,  key.len),
				toku_init_dbt(&data_dbt)}};
    return toku_brt_root_put_cmd(brt, &brtcmd, toku_txn_logger(txn));
}

int toku_rollback_cmddelete (TXNID xid, FILENUM filenum, BYTESTRING key,TOKUTXN txn) {
    CACHEFILE cf;
    BRT brt;
    int r = toku_cachefile_of_filenum(txn->logger->ct, filenum, &cf, &brt);
    assert(r==0);
    //printf("%s:%d aborting delete %s %s\n", __FILE__, __LINE__, key.data, data.data);
    DBT key_dbt,data_dbt;
    BRT_CMD_S brtcmd = { BRT_ABORT_ANY, xid,
			 .u.id={toku_fill_dbt(&key_dbt,  key.data,  key.len),
				toku_init_dbt(&data_dbt)}};
    return toku_brt_root_put_cmd(brt, &brtcmd, toku_txn_logger(txn));
}
