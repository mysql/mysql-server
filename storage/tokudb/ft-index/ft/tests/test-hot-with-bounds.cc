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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
/* The goal of this test.  Make sure that inserts stay behind deletes. */


#include "test.h"

#include <ft-cachetable-wrappers.h>
#include "ft-flusher.h"
#include "ft-flusher-internal.h"
#include "cachetable/checkpoint.h"

static TOKUTXN const null_txn = 0;

enum { NODESIZE = 1024, KSIZE=NODESIZE-100, TOKU_PSIZE=20 };

static void
doit (void) {
    BLOCKNUM node_leaf[3];
    BLOCKNUM node_root;
    
    CACHETABLE ct;
    FT_HANDLE t;

    int r;

    toku_cachetable_create(&ct, 500*1024*1024, ZERO_LSN, nullptr);
    unlink(TOKU_TEST_FILENAME);
    r = toku_open_ft_handle(TOKU_TEST_FILENAME, 1, &t, NODESIZE, NODESIZE/2, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);

    toku_testsetup_initialize();  // must precede any other toku_testsetup calls

    r = toku_testsetup_leaf(t, &node_leaf[0], 1, NULL, NULL);
    assert(r==0);
    r = toku_testsetup_leaf(t, &node_leaf[1], 1, NULL, NULL);
    assert(r==0);
    r = toku_testsetup_leaf(t, &node_leaf[2], 1, NULL, NULL);
    assert(r==0);

    int keylens[2];
    keylens[0] = 2;
    keylens[1] = 2;
    char first[2];
    first[0] = 'f';
    first[1] = 0;
    char second[2];
    second[0] = 'p';
    second[1] = 0;

    char* keys[2];
    keys[0] = first;
    keys[1] = second;
    r = toku_testsetup_nonleaf(t, 1, &node_root, 3, node_leaf, keys, keylens);
    assert(r==0);

    r = toku_testsetup_root(t, node_root);
    assert(r==0);


    r = toku_testsetup_insert_to_nonleaf(
        t, 
        node_root, 
        FT_INSERT,
        "a",
        2,
        NULL,
        0
        );
    r = toku_testsetup_insert_to_nonleaf(
        t, 
        node_root, 
        FT_INSERT,
        "m",
        2,
        NULL,
        0
        );

    r = toku_testsetup_insert_to_nonleaf(
        t, 
        node_root, 
        FT_INSERT,
        "z",
        2,
        NULL,
        0
        );


    // at this point, we have inserted three messages into
    // the root, one in each buffer, let's verify this.

    FTNODE node = NULL;
    ftnode_fetch_extra bfe;
    bfe.create_for_min_read(t->ft);
    toku_pin_ftnode(
        t->ft, 
        node_root,
        toku_cachetable_hash(t->ft->cf, node_root),
        &bfe,
        PL_WRITE_EXPENSIVE, 
        &node,
        true
        );
    assert(node->height == 1);
    assert(node->n_children == 3);
    assert(toku_bnc_nbytesinbuf(BNC(node, 0)) > 0);
    assert(toku_bnc_nbytesinbuf(BNC(node, 1)) > 0);
    assert(toku_bnc_nbytesinbuf(BNC(node, 2)) > 0);
    toku_unpin_ftnode(t->ft, node);

    // now let's run a hot optimize, that should only flush the middle buffer
    DBT left;
    toku_fill_dbt(&left, "g", 2);
    DBT right;
    toku_fill_dbt(&right, "n", 2);
    uint64_t loops_run = 0;
    r = toku_ft_hot_optimize(t, &left, &right, NULL, NULL, &loops_run);
    assert(r==0);

    // at this point, we have should have flushed
    // only the middle buffer, let's verify this.
    node = NULL;
    bfe.create_for_min_read(t->ft);
    toku_pin_ftnode(
        t->ft, 
        node_root,
        toku_cachetable_hash(t->ft->cf, node_root),
        &bfe,
        PL_WRITE_EXPENSIVE, 
        &node,
        true
        );
    assert(node->height == 1);
    assert(node->n_children == 3);
    assert(toku_bnc_nbytesinbuf(BNC(node, 0)) > 0);
    assert(toku_bnc_nbytesinbuf(BNC(node, 1)) == 0);
    assert(toku_bnc_nbytesinbuf(BNC(node, 2)) > 0);
    toku_unpin_ftnode(t->ft, node);

    r = toku_close_ft_handle_nolsn(t, 0);    assert(r==0);
    toku_cachetable_close(&ct);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    default_parse_args(argc, argv);
    doit();
    return 0;
}
