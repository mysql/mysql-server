/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "includes.h"
#include "test.h"

// The purpose of this test is to verify that certain information in the 
// brt_header is properly serialized and deserialized. 


static TOKUTXN const null_txn = 0;

static void test_header (void) {
    FT_HANDLE t;
    int r;
    CACHETABLE ct;
    char fname[]= __SRCFILE__ ".ft_handle";

    // First create dictionary
    r = toku_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);
    unlink(fname);
    r = toku_open_ft_handle(fname, 1, &t, 1024, 256, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);
    // now insert some info into the header
    FT h = t->ft;
    h->dirty = 1;
    h->layout_version_original = 13;
    h->layout_version_read_from_disk = 14;
    h->build_id_original = 1234;
    h->in_memory_stats          = (STAT64INFO_S) {10, 11};
    h->on_disk_stats            = (STAT64INFO_S) {20, 21};
    h->checkpoint_staging_stats = (STAT64INFO_S) {30, 31};
    r = toku_close_ft_handle_nolsn(t, 0);     assert(r==0);
    r = toku_cachetable_close(&ct);
    assert(r==0);

    // Now read dictionary back into memory and examine some header fields
    r = toku_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);
    r = toku_open_ft_handle(fname, 0, &t, 1024, 256, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);

    h = t->ft;
    STAT64INFO_S expected_stats = {20, 21};  // on checkpoint, on_disk_stats copied to checkpoint_staging_stats 
    assert(h->layout_version == FT_LAYOUT_VERSION);
    assert(h->layout_version_original == 13);
    assert(h->layout_version_read_from_disk == FT_LAYOUT_VERSION);
    assert(h->build_id_original == 1234);
    assert(h->in_memory_stats.numrows == expected_stats.numrows);
    assert(h->on_disk_stats.numbytes  == expected_stats.numbytes);
    r = toku_close_ft_handle_nolsn(t, 0);     assert(r==0);
    r = toku_cachetable_close(&ct);
    assert(r==0);


    
}

int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);
    test_header();
    test_header(); /* Make sure it works twice. Redundant, but it's a very cheap test. */
    if (verbose) printf("test_header ok\n");
    return 0;
}
