/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."


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
    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    unlink(fname);
    r = toku_open_ft_handle(fname, 1, &t, 1024, 256, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);
    // now insert some info into the header
    FT ft = t->ft;
    ft->h->dirty = 1;
    // cast away const because we actually want to fiddle with the header
    // in this test
    *((int *) &ft->h->layout_version_original) = 13;
    ft->layout_version_read_from_disk = 14;
    *((uint32_t *) &ft->h->build_id_original) = 1234;
    ft->in_memory_stats          = (STAT64INFO_S) {10, 11};
    ft->h->on_disk_stats            = (STAT64INFO_S) {20, 21};
    r = toku_close_ft_handle_nolsn(t, 0);     assert(r==0);
    toku_cachetable_close(&ct);

    // Now read dictionary back into memory and examine some header fields
    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    r = toku_open_ft_handle(fname, 0, &t, 1024, 256, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);

    ft = t->ft;
    STAT64INFO_S expected_stats = {20, 21};  // on checkpoint, on_disk_stats copied to ft->checkpoint_header->on_disk_stats
    assert(ft->h->layout_version == FT_LAYOUT_VERSION);
    assert(ft->h->layout_version_original == 13);
    assert(ft->layout_version_read_from_disk == FT_LAYOUT_VERSION);
    assert(ft->h->build_id_original == 1234);
    assert(ft->in_memory_stats.numrows == expected_stats.numrows);
    assert(ft->h->on_disk_stats.numbytes  == expected_stats.numbytes);
    r = toku_close_ft_handle_nolsn(t, 0);     assert(r==0);
    toku_cachetable_close(&ct);
}

int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);
    test_header();
    test_header(); /* Make sure it works twice. Redundant, but it's a very cheap test. */
    if (verbose) printf("test_header ok\n");
    return 0;
}
