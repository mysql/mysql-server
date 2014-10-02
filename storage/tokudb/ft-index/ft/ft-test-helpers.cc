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

#include <config.h>

#include "ft/ft.h"
#include "ft/ft-cachetable-wrappers.h"
#include "ft/ft-internal.h"
#include "ft/ft-flusher.h"
#include "ft/serialize/ft_node-serialize.h"
#include "ft/node.h"
#include "ft/ule.h"

// dummymsn needed to simulate msn because messages are injected at a lower level than toku_ft_root_put_msg()
#define MIN_DUMMYMSN ((MSN) {(uint64_t)1 << 62})
static MSN dummymsn;      
static int testsetup_initialized = 0;


// Must be called before any other test_setup_xxx() functions are called.
void
toku_testsetup_initialize(void) {
    if (testsetup_initialized == 0) {
        testsetup_initialized = 1;
        dummymsn = MIN_DUMMYMSN;
    }
}

static MSN
next_dummymsn(void) {
    ++(dummymsn.msn);
    return dummymsn;
}


bool ignore_if_was_already_open;
int toku_testsetup_leaf(FT_HANDLE ft_handle, BLOCKNUM *blocknum, int n_children, char **keys, int *keylens) {
    FTNODE node;
    assert(testsetup_initialized);
    toku_create_new_ftnode(ft_handle, &node, 0, n_children);
    for (int i = 0; i < n_children; i++) {
        BP_STATE(node, i) = PT_AVAIL;
    }

    DBT *XMALLOC_N(n_children - 1, pivotkeys);
    for (int i = 0; i + 1 < n_children; i++) {
        toku_memdup_dbt(&pivotkeys[i], keys[i], keylens[i]);
    }
    node->pivotkeys.create_from_dbts(pivotkeys, n_children - 1);
    for (int i = 0; i + 1 < n_children; i++) {
        toku_destroy_dbt(&pivotkeys[i]);
    }
    toku_free(pivotkeys);

    *blocknum = node->blocknum;
    toku_unpin_ftnode(ft_handle->ft, node);
    return 0;
}

// Don't bother to clean up carefully if something goes wrong.  (E.g., it's OK to have malloced stuff that hasn't been freed.)
int toku_testsetup_nonleaf (FT_HANDLE ft_handle, int height, BLOCKNUM *blocknum, int n_children, BLOCKNUM *children, char **keys, int *keylens) {
    FTNODE node;
    assert(testsetup_initialized);
    toku_create_new_ftnode(ft_handle, &node, height, n_children);
    for (int i = 0; i < n_children; i++) {
        BP_BLOCKNUM(node, i) = children[i];
        BP_STATE(node,i) = PT_AVAIL;
    }
    DBT *XMALLOC_N(n_children - 1, pivotkeys);
    for (int i = 0; i + 1 < n_children; i++) {
        toku_memdup_dbt(&pivotkeys[i], keys[i], keylens[i]);
    }
    node->pivotkeys.create_from_dbts(pivotkeys, n_children - 1);
    for (int i = 0; i + 1 < n_children; i++) {
        toku_destroy_dbt(&pivotkeys[i]);
    }
    toku_free(pivotkeys);

    *blocknum = node->blocknum;
    toku_unpin_ftnode(ft_handle->ft, node);
    return 0;
}

int toku_testsetup_root(FT_HANDLE ft_handle, BLOCKNUM blocknum) {
    assert(testsetup_initialized);
    ft_handle->ft->h->root_blocknum = blocknum;
    return 0;
}

int toku_testsetup_get_sersize(FT_HANDLE ft_handle, BLOCKNUM diskoff) // Return the size on disk
{
    assert(testsetup_initialized);
    void *node_v;
    ftnode_fetch_extra bfe;
    bfe.create_for_full_read(ft_handle->ft);
    int r  = toku_cachetable_get_and_pin(
        ft_handle->ft->cf, diskoff,
        toku_cachetable_hash(ft_handle->ft->cf, diskoff),
        &node_v,
        NULL,
        get_write_callbacks_for_node(ft_handle->ft),
        toku_ftnode_fetch_callback,
        toku_ftnode_pf_req_callback,
        toku_ftnode_pf_callback,
        true,
        &bfe
        );
    assert(r==0);
    FTNODE CAST_FROM_VOIDP(node, node_v);
    int size = toku_serialize_ftnode_size(node);
    toku_unpin_ftnode(ft_handle->ft, node);
    return size;
}

int toku_testsetup_insert_to_leaf (FT_HANDLE ft_handle, BLOCKNUM blocknum, const char *key, int keylen, const char *val, int vallen) {
    void *node_v;
    int r;

    assert(testsetup_initialized);

    ftnode_fetch_extra bfe;
    bfe.create_for_full_read(ft_handle->ft);
    r = toku_cachetable_get_and_pin(
        ft_handle->ft->cf,
        blocknum,
        toku_cachetable_hash(ft_handle->ft->cf, blocknum),
        &node_v,
        NULL,
        get_write_callbacks_for_node(ft_handle->ft),
	toku_ftnode_fetch_callback,
        toku_ftnode_pf_req_callback,
        toku_ftnode_pf_callback,
        true,
	&bfe
	);
    if (r!=0) return r;
    FTNODE CAST_FROM_VOIDP(node, node_v);
    toku_verify_or_set_counts(node);
    assert(node->height==0);

    DBT kdbt, vdbt;
    ft_msg msg(toku_fill_dbt(&kdbt, key, keylen), toku_fill_dbt(&vdbt, val, vallen),
               FT_INSERT, next_dummymsn(), toku_xids_get_root_xids());

    static size_t zero_flow_deltas[] = { 0, 0 };
    txn_gc_info gc_info(nullptr, TXNID_NONE, TXNID_NONE, true);
    toku_ftnode_put_msg(ft_handle->ft->cmp,
                        ft_handle->ft->update_fun,
                        node,
                        -1,
                        msg,
                        true,
                        &gc_info,
                        zero_flow_deltas,
                        NULL
                        );

    toku_verify_or_set_counts(node);

    toku_unpin_ftnode(ft_handle->ft, node);
    return 0;
}

static int
testhelper_string_key_cmp(DB *UU(e), const DBT *a, const DBT *b)
{
    char *CAST_FROM_VOIDP(s, a->data), *CAST_FROM_VOIDP(t, b->data);
    return strcmp(s, t);
}


void
toku_pin_node_with_min_bfe(FTNODE* node, BLOCKNUM b, FT_HANDLE t)
{
    ftnode_fetch_extra bfe;
    bfe.create_for_min_read(t->ft);
    toku_pin_ftnode(
        t->ft, 
        b,
        toku_cachetable_hash(t->ft->cf, b),
        &bfe,
        PL_WRITE_EXPENSIVE,
        node,
        true
        );
}

int toku_testsetup_insert_to_nonleaf (FT_HANDLE ft_handle, BLOCKNUM blocknum, enum ft_msg_type msgtype, const char *key, int keylen, const char *val, int vallen) {
    void *node_v;
    int r;

    assert(testsetup_initialized);

    ftnode_fetch_extra bfe;
    bfe.create_for_full_read(ft_handle->ft);
    r = toku_cachetable_get_and_pin(
        ft_handle->ft->cf,
        blocknum,
        toku_cachetable_hash(ft_handle->ft->cf, blocknum),
        &node_v,
        NULL,
        get_write_callbacks_for_node(ft_handle->ft),
	toku_ftnode_fetch_callback,
        toku_ftnode_pf_req_callback,
        toku_ftnode_pf_callback,
        true,
	&bfe
        );
    if (r!=0) return r;
    FTNODE CAST_FROM_VOIDP(node, node_v);
    assert(node->height>0);

    DBT k;
    int childnum = toku_ftnode_which_child(node, toku_fill_dbt(&k, key, keylen), ft_handle->ft->cmp);

    XIDS xids_0 = toku_xids_get_root_xids();
    MSN msn = next_dummymsn();
    toku::comparator cmp;
    cmp.create(testhelper_string_key_cmp, nullptr);
    toku_bnc_insert_msg(BNC(node, childnum), key, keylen, val, vallen, msgtype, msn, xids_0, true, cmp);
    cmp.destroy();
    // Hack to get the test working. The problem is that this test
    // is directly queueing something in a FIFO instead of 
    // using ft APIs.
    node->max_msn_applied_to_node_on_disk = msn;
    node->dirty = 1;
    // Also hack max_msn_in_ft
    ft_handle->ft->h->max_msn_in_ft = msn;

    toku_unpin_ftnode(ft_handle->ft, node);
    return 0;
}
