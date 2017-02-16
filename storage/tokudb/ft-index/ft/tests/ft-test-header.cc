/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."


#include "test.h"

// The purpose of this test is to verify that certain information in the 
// ft_header is properly serialized and deserialized. 


static TOKUTXN const null_txn = 0;

static void test_header (void) {
    FT_HANDLE t;
    int r;
    CACHETABLE ct;
    const char *fname = TOKU_TEST_FILENAME;

    // First create dictionary
    toku_cachetable_create(&ct, 0, ZERO_LSN, nullptr);
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
    toku_cachetable_create(&ct, 0, ZERO_LSN, nullptr);
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
