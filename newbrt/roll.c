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

#define ABORTIT { le=le; txn=txn; fprintf(stderr, "%s:%d (%s) not ready to go\n", __FILE__, __LINE__, __func__); abort(); }

int toku_rollback_fcreate (BYTESTRING bs_fname,
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

//int toku_rollback_newbrtnode (struct logtype_newbrtnode *le, TOKUTXN txn) {
//    // All that must be done is to put the node on the freelist.
//    // Since we don't have a freelist right now, we don't have anything to do.
//    // We'll fix this later (See #264)
//    le=le;
//    txn=txn;
//    return 0;
//}

int toku_rollback_insertatleaf (FILENUM filenum, BYTESTRING key,BYTESTRING data, TOKUTXN txn) {
    CACHEFILE cf;
    BRT brt;
    int r = toku_cachefile_of_filenum(txn->logger->ct, filenum, &cf, &brt);
    assert(r==0);
    DBT key_dbt,data_dbt;
    r = toku_brt_delete_both(brt,
			     toku_fill_dbt(&key_dbt, key.data, key.len),
			     toku_fill_dbt(&data_dbt, data.data, data.len),
			     0);
    return r;
}

int toku_rollback_deleteatleaf (FILENUM filenum, BYTESTRING key, BYTESTRING data,TOKUTXN txn) {
    CACHEFILE cf;
    BRT brt;
    int r = toku_cachefile_of_filenum(txn->logger->ct, filenum, &cf, &brt);
    assert(r==0);
    DBT key_dbt,data_dbt;
    r = toku_brt_insert(brt,
			toku_fill_dbt(&key_dbt, key.data, key.len),
			toku_fill_dbt(&data_dbt, data.data, data.len),
			0); // Do the insertion unconditionally
    return r;
}

int toku_rollback_xactiontouchednonleaf(FILENUM filenum, DISKOFFARRAY array __attribute__((__unused__)), DISKOFF diskoff, TOKUTXN txn) {
    CACHEFILE cf;
    BRT brt;
    int r = toku_cachefile_of_filenum(txn->logger->ct, filenum, &cf, &brt);
    assert(r==0);
    r = toku_brt_nonleaf_expunge_xaction(brt,  diskoff, txn->txnid64);
    assert(r==0);
    //printf("%s:%d node=%lld has Rollback parents = {", __FILE__, __LINE__, (long long)diskoff);
    //int i; for (i=0; i<array.len; i++) printf(" %lld", array.array[i]);
    //printf("}\n");
    if (array.len!=0) printf("%s:%d array.len!=0 and we didn't fix up the fingerprints.\n", __FILE__, __LINE__);
    return 0;
}
