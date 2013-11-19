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

// Verify that a message with an old msn is ignored
// by toku_apply_cmd_to_leaf()
//
// method:
//  - inject valid message, verify that new value is in row
//  - inject message with same msn and new value, verify that original value is still in key  (verify cmd.msn == node.max_msn is rejected)
//  - inject valid message with new value2, verify that row has new value2 
//  - inject message with old msn, verify that row still has value2   (verify cmd.msn < node.max_msn is rejected)


// TODO: 
//  - verify that no work is done by messages that should be ignored (via workdone arg to ft_leaf_put_cmd())
//  - maybe get counter of messages ignored for old msn (once the counter is implemented in ft-ops.c)

#include "ft-internal.h"
#include <ft-cachetable-wrappers.h>

#include "test.h"

static FTNODE
make_node(FT_HANDLE brt, int height) {
    FTNODE node = NULL;
    int n_children = (height == 0) ? 1 : 0;
    toku_create_new_ftnode(brt, &node, height, n_children);
    if (n_children) BP_STATE(node,0) = PT_AVAIL;
    return node;
}

static void
append_leaf(FT_HANDLE brt, FTNODE leafnode, void *key, uint32_t keylen, void *val, uint32_t vallen) {
    assert(leafnode->height == 0);

    DBT thekey; toku_fill_dbt(&thekey, key, keylen);
    DBT theval; toku_fill_dbt(&theval, val, vallen);
    DBT badval; toku_fill_dbt(&badval, (char*)val+1, vallen);
    DBT val2;   toku_fill_dbt(&val2, (char*)val+2, vallen);

    struct check_pair pair  = {keylen, key, vallen, val, 0};
    struct check_pair pair2 = {keylen, key, vallen, (char*)val+2, 0};

    // apply an insert to the leaf node
    MSN msn = next_dummymsn();
    brt->ft->h->max_msn_in_ft = msn;
    FT_MSG_S cmd = { FT_INSERT, msn, xids_get_root_xids(), .u={.id = { &thekey, &theval }} };

    toku_ft_leaf_apply_cmd(brt->ft->compare_fun, brt->ft->update_fun, &brt->ft->cmp_descriptor, leafnode, -1, &cmd, make_gc_info(false), nullptr, nullptr);
    {
	int r = toku_ft_lookup(brt, &thekey, lookup_checkf, &pair);
	assert(r==0);
	assert(pair.call_count==1);
    }

    FT_MSG_S badcmd = { FT_INSERT, msn, xids_get_root_xids(), .u={.id = { &thekey, &badval }} };
    toku_ft_leaf_apply_cmd(brt->ft->compare_fun, brt->ft->update_fun, &brt->ft->cmp_descriptor, leafnode, -1, &badcmd, make_gc_info(false), nullptr, nullptr);

    // message should be rejected for duplicate msn, row should still have original val
    {
	int r = toku_ft_lookup(brt, &thekey, lookup_checkf, &pair);
	assert(r==0);
	assert(pair.call_count==2);
    }

    // now verify that message with proper msn gets through
    msn = next_dummymsn();
    brt->ft->h->max_msn_in_ft = msn;
    FT_MSG_S cmd2 = { FT_INSERT, msn, xids_get_root_xids(), .u={.id = { &thekey, &val2 }} };
    toku_ft_leaf_apply_cmd(brt->ft->compare_fun, brt->ft->update_fun, &brt->ft->cmp_descriptor, leafnode, -1, &cmd2, make_gc_info(false), nullptr, nullptr);

    // message should be accepted, val should have new value
    {
	int r = toku_ft_lookup(brt, &thekey, lookup_checkf, &pair2);
	assert(r==0);
	assert(pair2.call_count==1);
    }

    // now verify that message with lesser (older) msn is rejected
    msn.msn = msn.msn - 10;
    FT_MSG_S cmd3 = { FT_INSERT, msn, xids_get_root_xids(), .u={.id = { &thekey, &badval } }};
    toku_ft_leaf_apply_cmd(brt->ft->compare_fun, brt->ft->update_fun, &brt->ft->cmp_descriptor, leafnode, -1, &cmd3, make_gc_info(false), nullptr, nullptr);

    // message should be rejected, val should still have value in pair2
    {
	int r = toku_ft_lookup(brt, &thekey, lookup_checkf, &pair2);
	assert(r==0);
	assert(pair2.call_count==2);
    }

    // dont forget to dirty the node
    leafnode->dirty = 1;
}

static void 
populate_leaf(FT_HANDLE brt, FTNODE leafnode, int k, int v) {
    char vbuf[32]; // store v in a buffer large enough to dereference unaligned int's
    memset(vbuf, 0, sizeof vbuf);
    memcpy(vbuf, &v, sizeof v);
    append_leaf(brt, leafnode, &k, sizeof k, vbuf, sizeof v);
}

static void 
test_msnfilter(int do_verify) {
    int r;

    // cleanup
    const char *fname = TOKU_TEST_FILENAME;
    r = unlink(fname);
    if (r != 0) {
        assert(r == -1);
        assert(get_error_errno() == ENOENT);
    }

    // create a cachetable
    CACHETABLE ct = NULL;
    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);

    // create the brt
    TOKUTXN null_txn = NULL;
    FT_HANDLE brt = NULL;
    r = toku_open_ft_handle(fname, 1, &brt, 1024, 256, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r == 0);

    FTNODE newroot = make_node(brt, 0);

    // set the new root to point to the new tree
    toku_ft_set_new_root_blocknum(brt->ft, newroot->thisnodename);

    // KLUDGE: Unpin the new root so toku_ft_lookup() can pin it.  (Pin lock is no longer a recursive
    //         mutex.)  Just leaving it unpinned for this test program works  because it is the only 
    //         node in the cachetable and won't be evicted.  The right solution would be to lock the 
    //         node and unlock it again before and after each message injection, but that requires more
    //         work than it's worth (setting up dummy callbacks, etc.)
    //         
    toku_unpin_ftnode(brt->ft, newroot);

    populate_leaf(brt, newroot, htonl(2), 1);

    if (do_verify) {
        r = toku_verify_ft(brt);
        assert(r == 0);
    }

    // flush to the file system
    r = toku_close_ft_handle_nolsn(brt, 0);     
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
    test_msnfilter(do_verify);
    return 0;
}
