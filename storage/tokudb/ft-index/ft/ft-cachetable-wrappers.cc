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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <ft-cachetable-wrappers.h>

#include <fttypes.h>
#include <ft-flusher.h>
#include <ft-internal.h>
#include "ft.h"

static void
ftnode_get_key_and_fullhash(
    BLOCKNUM* cachekey,
    uint32_t* fullhash,
    void* extra)
{
    FT h = (FT) extra;
    BLOCKNUM name;
    toku_allocate_blocknum(h->blocktable, &name, h);
    *cachekey = name;
    *fullhash = toku_cachetable_hash(h->cf, name);
}

void
cachetable_put_empty_node_with_dep_nodes(
    FT h,
    uint32_t num_dependent_nodes,
    FTNODE* dependent_nodes,
    BLOCKNUM* name, //output
    uint32_t* fullhash, //output
    FTNODE* result)
{
    FTNODE XMALLOC(new_node);
    PAIR dependent_pairs[num_dependent_nodes];
    enum cachetable_dirty dependent_dirty_bits[num_dependent_nodes];
    for (uint32_t i = 0; i < num_dependent_nodes; i++) {
        dependent_pairs[i] = dependent_nodes[i]->ct_pair;
        dependent_dirty_bits[i] = (enum cachetable_dirty) dependent_nodes[i]->dirty;
    }

    toku_cachetable_put_with_dep_pairs(
        h->cf,
        ftnode_get_key_and_fullhash,
        new_node,
        make_pair_attr(sizeof(FTNODE)),
        get_write_callbacks_for_node(h),
        h,
        num_dependent_nodes,
        dependent_pairs,
        dependent_dirty_bits,
        name,
        fullhash,
        toku_node_save_ct_pair);
    *result = new_node;
}

void
create_new_ftnode_with_dep_nodes(
    FT ft,
    FTNODE *result,
    int height,
    int n_children,
    uint32_t num_dependent_nodes,
    FTNODE* dependent_nodes)
{
    uint32_t fullhash = 0;
    BLOCKNUM name;

    cachetable_put_empty_node_with_dep_nodes(
        ft,
        num_dependent_nodes,
        dependent_nodes,
        &name,
        &fullhash,
        result);

    assert(ft->h->basementnodesize > 0);
    if (height == 0) {
        assert(n_children > 0);
    }

    toku_initialize_empty_ftnode(
        *result,
        name,
        height,
        n_children,
        ft->h->layout_version,
        ft->h->flags);

    (*result)->fullhash = fullhash;
}

void
toku_create_new_ftnode (
    FT_HANDLE t,
    FTNODE *result,
    int height,
    int n_children)
{
    return create_new_ftnode_with_dep_nodes(
        t->ft,
        result,
        height,
        n_children,
        0,
        NULL);
}

//
// On success, this function assumes that the caller is trying to pin the node
// with a PL_READ lock. If message application is needed,
// then a PL_WRITE_CHEAP lock is grabbed
//
int
toku_pin_ftnode_batched(
    FT_HANDLE brt,
    BLOCKNUM blocknum,
    uint32_t fullhash,
    UNLOCKERS unlockers,
    ANCESTORS ancestors,
    const PIVOT_BOUNDS bounds,
    FTNODE_FETCH_EXTRA bfe,
    bool apply_ancestor_messages, // this bool is probably temporary, for #3972, once we know how range query estimates work, will revisit this
    FTNODE *node_p,
    bool* msgs_applied)
{
    void *node_v;
    *msgs_applied = false;
    FTNODE node = nullptr;
    MSN max_msn_in_path = ZERO_MSN;
    bool needs_ancestors_messages = false;
    // this function assumes that if you want ancestor messages applied,
    // you are doing a read for a query. This is so we can make some optimizations
    // below.
    if (apply_ancestor_messages) {
        paranoid_invariant(bfe->type == ftnode_fetch_subset);
    }
    
    int r = toku_cachetable_get_and_pin_nonblocking_batched(
            brt->ft->cf,
            blocknum,
            fullhash,
            &node_v,
            NULL,
            get_write_callbacks_for_node(brt->ft),
            toku_ftnode_fetch_callback,
            toku_ftnode_pf_req_callback,
            toku_ftnode_pf_callback,
            PL_READ,
            bfe, //read_extraargs
            unlockers);
    if (r != 0) {
        assert(r == TOKUDB_TRY_AGAIN); // Any other error and we should bomb out ASAP.
        goto exit;
    }
    node = static_cast<FTNODE>(node_v);
    if (apply_ancestor_messages && node->height == 0) {
        needs_ancestors_messages = toku_ft_leaf_needs_ancestors_messages(
            brt->ft, 
            node, 
            ancestors, 
            bounds, 
            &max_msn_in_path, 
            bfe->child_to_read
            );
        if (needs_ancestors_messages) {
            toku_unpin_ftnode_read_only(brt->ft, node);
            int rr = toku_cachetable_get_and_pin_nonblocking_batched(
                    brt->ft->cf,
                    blocknum,
                    fullhash,
                    &node_v,
                    NULL,
                    get_write_callbacks_for_node(brt->ft),
                    toku_ftnode_fetch_callback,
                    toku_ftnode_pf_req_callback,
                    toku_ftnode_pf_callback,
                    PL_WRITE_CHEAP,
                    bfe, //read_extraargs
                    unlockers);
            if (rr != 0) {
                assert(rr == TOKUDB_TRY_AGAIN); // Any other error and we should bomb out ASAP.
                r = TOKUDB_TRY_AGAIN;
                goto exit;
            }
            node = static_cast<FTNODE>(node_v);
            toku_apply_ancestors_messages_to_node(
                brt, 
                node, 
                ancestors, 
                bounds, 
                msgs_applied,
                bfe->child_to_read
                );
        } else {
            // At this point, we aren't going to run
            // toku_apply_ancestors_messages_to_node but that doesn't
            // mean max_msn_applied shouldn't be updated if possible
            // (this saves the CPU work involved in
            // toku_ft_leaf_needs_ancestors_messages).
            //
            // We still have a read lock, so we have not resolved
            // checkpointing.  If the node is pending and dirty, we
            // can't modify anything, including max_msn, until we
            // resolve checkpointing.  If we do, the node might get
            // written out that way as part of a checkpoint with a
            // root that was already written out with a smaller
            // max_msn.  During recovery, we would then inject a
            // message based on the root's max_msn, and that message
            // would get filtered by the leaf because it had too high
            // a max_msn value. (see #5407)
            //
            // So for simplicity we only update the max_msn if the
            // node is clean.  That way, in order for the node to get
            // written out, it would have to be dirtied.  That
            // requires a write lock, and a write lock requires you to
            // resolve checkpointing.
            if (!node->dirty) {
                toku_ft_bn_update_max_msn(node, max_msn_in_path, bfe->child_to_read);
            }
        }
    }
    *node_p = node;
exit:
    return r;
}

void
toku_pin_ftnode_off_client_thread_and_maybe_move_messages(
    FT h,
    BLOCKNUM blocknum,
    uint32_t fullhash,
    FTNODE_FETCH_EXTRA bfe,
    pair_lock_type lock_type,
    uint32_t num_dependent_nodes,
    FTNODE* dependent_nodes,
    FTNODE *node_p,
    bool move_messages)
{
    toku_pin_ftnode_off_client_thread_batched_and_maybe_move_messages(
        h,
        blocknum,
        fullhash,
        bfe,
        lock_type,
        num_dependent_nodes,
        dependent_nodes,
        node_p,
        move_messages
        );
}

void
toku_pin_ftnode_off_client_thread(
    FT h,
    BLOCKNUM blocknum,
    uint32_t fullhash,
    FTNODE_FETCH_EXTRA bfe,
    pair_lock_type lock_type,
    uint32_t num_dependent_nodes,
    FTNODE* dependent_nodes,
    FTNODE *node_p)
{
    toku_pin_ftnode_off_client_thread_and_maybe_move_messages(
            h, blocknum, fullhash, bfe, lock_type, num_dependent_nodes, dependent_nodes, node_p, true);
}

void
toku_pin_ftnode_off_client_thread_batched_and_maybe_move_messages(
    FT h,
    BLOCKNUM blocknum,
    uint32_t fullhash,
    FTNODE_FETCH_EXTRA bfe,
    pair_lock_type lock_type,
    uint32_t num_dependent_nodes,
    FTNODE* dependent_nodes,
    FTNODE *node_p,
    bool move_messages)
{
    void *node_v;
    PAIR dependent_pairs[num_dependent_nodes];
    enum cachetable_dirty dependent_dirty_bits[num_dependent_nodes];
    for (uint32_t i = 0; i < num_dependent_nodes; i++) {
        dependent_pairs[i] = dependent_nodes[i]->ct_pair;
        dependent_dirty_bits[i] = (enum cachetable_dirty) dependent_nodes[i]->dirty;
    }

    int r = toku_cachetable_get_and_pin_with_dep_pairs_batched(
        h->cf,
        blocknum,
        fullhash,
        &node_v,
        NULL,
        get_write_callbacks_for_node(h),
        toku_ftnode_fetch_callback,
        toku_ftnode_pf_req_callback,
        toku_ftnode_pf_callback,
        lock_type,
        bfe,
        num_dependent_nodes,
        dependent_pairs,
        dependent_dirty_bits
        );
    assert(r==0);
    FTNODE node = (FTNODE) node_v;
    if ((lock_type != PL_READ) && node->height > 0 && move_messages) {
        toku_move_ftnode_messages_to_stale(h, node);
    }
    *node_p = node;
}

void
toku_pin_ftnode_off_client_thread_batched(
    FT h,
    BLOCKNUM blocknum,
    uint32_t fullhash,
    FTNODE_FETCH_EXTRA bfe,
    pair_lock_type lock_type,
    uint32_t num_dependent_nodes,
    FTNODE* dependent_nodes,
    FTNODE *node_p)
{
    toku_pin_ftnode_off_client_thread_batched_and_maybe_move_messages(
            h, blocknum, fullhash, bfe, lock_type, num_dependent_nodes, dependent_nodes, node_p, true);
}

int toku_maybe_pin_ftnode_clean(FT ft, BLOCKNUM blocknum, uint32_t fullhash, pair_lock_type lock_type, FTNODE *nodep) {
    void *node_v;
    int r = toku_cachetable_maybe_get_and_pin_clean(ft->cf, blocknum, fullhash, lock_type, &node_v);
    if (r != 0) {
        goto cleanup;
    }
    CAST_FROM_VOIDP(*nodep, node_v);
    if ((*nodep)->height > 0 && lock_type != PL_READ) {
        toku_move_ftnode_messages_to_stale(ft, *nodep);
    }
cleanup:
    return r;
}

void
toku_unpin_ftnode_off_client_thread(FT ft, FTNODE node)
{
    int r = toku_cachetable_unpin(
        ft->cf,
        node->ct_pair,
        (enum cachetable_dirty) node->dirty,
        make_ftnode_pair_attr(node)
        );
    assert(r==0);
}

void
toku_unpin_ftnode(FT ft, FTNODE node)
{
    // printf("%*sUnpin %ld\n", 8-node->height, "", node->thisnodename.b);
    //VERIFY_NODE(brt,node);
    toku_unpin_ftnode_off_client_thread(ft, node);
}

void
toku_unpin_ftnode_read_only(FT ft, FTNODE node)
{
    int r = toku_cachetable_unpin(
        ft->cf,
        node->ct_pair,
        (enum cachetable_dirty) node->dirty,
        make_invalid_pair_attr()
        );
    assert(r==0);
}
