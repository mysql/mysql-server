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

  TokuDB, Tokutek Fractal Tree Indexing Library.
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

#ident "Copyright (c) 2011-2013 Tokutek Inc.  All rights reserved."

// generate a tree with a single leaf node containing duplicate keys
// check that ft verify finds them


#include <ft-cachetable-wrappers.h>
#include "test.h"

static FTNODE
make_node(FT_HANDLE ft, int height) {
    FTNODE node = NULL;
    int n_children = (height == 0) ? 1 : 0;
    toku_create_new_ftnode(ft, &node, height, n_children);
    if (n_children) BP_STATE(node,0) = PT_AVAIL;
    return node;
}

static void
append_leaf(FTNODE leafnode, void *key, size_t keylen, void *val, size_t vallen) {
    assert(leafnode->height == 0);

    DBT thekey; toku_fill_dbt(&thekey, key, keylen);
    DBT theval; toku_fill_dbt(&theval, val, vallen);

    // get an index that we can use to create a new leaf entry
    uint32_t idx = BLB_DATA(leafnode, 0)->num_klpairs();

    // apply an insert to the leaf node
    MSN msn = next_dummymsn();
    FT_MSG_S msg = { FT_INSERT, msn, xids_get_root_xids(), .u={.id = { &thekey, &theval }} };
    txn_gc_info gc_info(nullptr, TXNID_NONE, TXNID_NONE, false);
    toku_ft_bn_apply_msg_once(BLB(leafnode, 0), &msg, idx, NULL, &gc_info, NULL, NULL);

    // dont forget to dirty the node
    leafnode->dirty = 1;
}

static void 
populate_leaf(FTNODE leafnode, int k, int v) {
    append_leaf(leafnode, &k, sizeof k, &v, sizeof v);
}

static void 
test_dup_in_leaf(int do_verify) {
    int r;

    // cleanup
    const char *fname = TOKU_TEST_FILENAME;
    r = unlink(fname);
    assert(r == 0 || (r == -1 && errno == ENOENT));

    // create a cachetable
    CACHETABLE ct = NULL;
    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);

    // create the ft
    TOKUTXN null_txn = NULL;
    FT_HANDLE ft = NULL;
    r = toku_open_ft_handle(fname, 1, &ft, 1024, 256, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r == 0);

    // discard the old root block

    FTNODE newroot = make_node(ft, 0);
    populate_leaf(newroot, htonl(2), 1);
    populate_leaf(newroot, htonl(2), 2);

    // set the new root to point to the new tree
    toku_ft_set_new_root_blocknum(ft->ft, newroot->thisnodename);

    // unpin the new root
    toku_unpin_ftnode(ft->ft, newroot);

    if (do_verify) {
        r = toku_verify_ft(ft);
        assert(r != 0);
    }

    // flush to the file system
    r = toku_close_ft_handle_nolsn(ft, 0);     
    assert(r == 0);

    // shutdown the cachetable
    toku_cachetable_close(&ct);
}

static int
usage(void) {
    return 1;
}

int
test_main (int argc , const char *argv[]) {
    int do_verify = 1;
    initialize_dummymsn();
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "-q") == 0) {
            verbose = 0;
            continue;
        }
        if (strcmp(arg, "--verify") == 0 && i+1 < argc) {
            do_verify = atoi(argv[++i]);
            continue;
        }
        return usage();
    }
    test_dup_in_leaf(do_verify);
    return 0;
}
