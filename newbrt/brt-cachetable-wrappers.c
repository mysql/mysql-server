/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <brt-cachetable-wrappers.h>

#include <brttypes.h>
#include <brt-internal.h>
#include <cachetable.h>

static void
brtnode_get_key_and_fullhash(
    BLOCKNUM* cachekey,
    u_int32_t* fullhash,
    void* extra)
{
    struct brt_header* h = extra;
    BLOCKNUM name;
    toku_allocate_blocknum(h->blocktable, &name, h);
    *cachekey = name;
    *fullhash = toku_cachetable_hash(h->cf, name);
}

void
cachetable_put_empty_node_with_dep_nodes(
    struct brt_header* h,
    u_int32_t num_dependent_nodes,
    BRTNODE* dependent_nodes,
    BLOCKNUM* name, //output
    u_int32_t* fullhash, //output
    BRTNODE* result)
{
    BRTNODE XMALLOC(new_node);
    CACHEFILE dependent_cf[num_dependent_nodes];
    BLOCKNUM dependent_keys[num_dependent_nodes];
    u_int32_t dependent_fullhash[num_dependent_nodes];
    enum cachetable_dirty dependent_dirty_bits[num_dependent_nodes];
    for (u_int32_t i = 0; i < num_dependent_nodes; i++) {
        dependent_cf[i] = h->cf;
        dependent_keys[i] = dependent_nodes[i]->thisnodename;
        dependent_fullhash[i] = toku_cachetable_hash(h->cf, dependent_nodes[i]->thisnodename);
        dependent_dirty_bits[i] = (enum cachetable_dirty) dependent_nodes[i]->dirty;
    }

    int r = toku_cachetable_put_with_dep_pairs(
        h->cf,
        brtnode_get_key_and_fullhash,
        new_node,
        make_pair_attr(sizeof(BRTNODE)),
        toku_brtnode_flush_callback,
        toku_brtnode_pe_est_callback,
        toku_brtnode_pe_callback,
        toku_brtnode_cleaner_callback,
        h,
        h,
        num_dependent_nodes,
        dependent_cf,
        dependent_keys,
        dependent_fullhash,
        dependent_dirty_bits,
        name,
        fullhash);
    assert_zero(r);

    *result = new_node;
}

void
create_new_brtnode_with_dep_nodes(
    struct brt_header* h,
    BRTNODE *result,
    int height,
    int n_children,
    u_int32_t num_dependent_nodes,
    BRTNODE* dependent_nodes)
{
    u_int32_t fullhash = 0;
    BLOCKNUM name;

    cachetable_put_empty_node_with_dep_nodes(
        h,
        num_dependent_nodes,
        dependent_nodes,
        &name,
        &fullhash,
        result);

    assert(h->nodesize > 0);
    assert(h->basementnodesize > 0);
    if (height == 0) {
        assert(n_children > 0);
    }

    toku_initialize_empty_brtnode(
        *result,
        name,
        height,
        n_children,
        h->layout_version,
        h->nodesize,
        h->flags);

    assert((*result)->nodesize > 0);
    (*result)->fullhash = fullhash;
}

void
toku_create_new_brtnode (
    BRT t,
    BRTNODE *result,
    int height,
    int n_children)
{
    return create_new_brtnode_with_dep_nodes(
        t->h,
        result,
        height,
        n_children,
        0,
        NULL);
}

//
// The intent of toku_pin_brtnode(_holding_lock) is to abstract the process of retrieving a node from
// the rest of brt.c, so that there is only one place where we need to worry applying ancestor
// messages to a leaf node. The idea is for all of brt.c (search, splits, merges, flushes, etc)
// to access a node via toku_pin_brtnode(_holding_lock)
//
int
toku_pin_brtnode(
    BRT brt,
    BLOCKNUM blocknum,
    u_int32_t fullhash,
    UNLOCKERS unlockers,
    ANCESTORS ancestors,
    const PIVOT_BOUNDS bounds,
    BRTNODE_FETCH_EXTRA bfe,
    BOOL apply_ancestor_messages, // this BOOL is probably temporary, for #3972, once we know how range query estimates work, will revisit this
    BRTNODE *node_p)
{
    void *node_v;
    int r = toku_cachetable_get_and_pin_nonblocking(
            brt->cf,
            blocknum,
            fullhash,
            &node_v,
            NULL,
            toku_brtnode_flush_callback,
            toku_brtnode_fetch_callback,
            toku_brtnode_pe_est_callback,
            toku_brtnode_pe_callback,
            toku_brtnode_pf_req_callback,
            toku_brtnode_pf_callback,
            toku_brtnode_cleaner_callback,
            bfe, //read_extraargs
            brt->h, //write_extraargs
            unlockers);
    if (r==0) {
        BRTNODE node = node_v;
        if (apply_ancestor_messages) {
            maybe_apply_ancestors_messages_to_node(brt, node, ancestors, bounds);
        }
        *node_p = node;
        // printf("%*sPin %ld\n", 8-node->height, "", blocknum.b);
    } else {
        assert(r==TOKUDB_TRY_AGAIN); // Any other error and we should bomb out ASAP.
        // printf("%*sPin %ld try again\n", 8, "", blocknum.b);
    }
    return r;
}

// see comments for toku_pin_brtnode
void
toku_pin_brtnode_holding_lock(
    BRT brt,
    BLOCKNUM blocknum,
    u_int32_t fullhash,
    ANCESTORS ancestors,
    const PIVOT_BOUNDS bounds,
    BRTNODE_FETCH_EXTRA bfe,
    BOOL apply_ancestor_messages, // this BOOL is probably temporary, for #3972, once we know how range query estimates work, will revisit this
    BRTNODE *node_p)
{
    void *node_v;
    int r = toku_cachetable_get_and_pin(
        brt->cf,
        blocknum,
        fullhash,
        &node_v,
        NULL,
        toku_brtnode_flush_callback,
        toku_brtnode_fetch_callback,
        toku_brtnode_pe_est_callback,
        toku_brtnode_pe_callback,
        toku_brtnode_pf_req_callback,
        toku_brtnode_pf_callback,
        toku_brtnode_cleaner_callback,
        bfe,
        brt->h
        );
    assert(r==0);
    BRTNODE node = node_v;
    if (apply_ancestor_messages) maybe_apply_ancestors_messages_to_node(brt, node, ancestors, bounds);
    *node_p = node;
}

void
toku_pin_brtnode_off_client_thread(
    struct brt_header* h,
    BLOCKNUM blocknum,
    u_int32_t fullhash,
    BRTNODE_FETCH_EXTRA bfe,
    u_int32_t num_dependent_nodes,
    BRTNODE* dependent_nodes,
    BRTNODE *node_p)
{
    void *node_v;
    CACHEFILE dependent_cf[num_dependent_nodes];
    BLOCKNUM dependent_keys[num_dependent_nodes];
    u_int32_t dependent_fullhash[num_dependent_nodes];
    enum cachetable_dirty dependent_dirty_bits[num_dependent_nodes];
    for (u_int32_t i = 0; i < num_dependent_nodes; i++) {
        dependent_cf[i] = h->cf;
        dependent_keys[i] = dependent_nodes[i]->thisnodename;
        dependent_fullhash[i] = toku_cachetable_hash(h->cf, dependent_nodes[i]->thisnodename);
        dependent_dirty_bits[i] = (enum cachetable_dirty) dependent_nodes[i]->dirty;
    }

    int r = toku_cachetable_get_and_pin_with_dep_pairs(
        h->cf,
        blocknum,
        fullhash,
        &node_v,
        NULL,
        toku_brtnode_flush_callback,
        toku_brtnode_fetch_callback,
        toku_brtnode_pe_est_callback,
        toku_brtnode_pe_callback,
        toku_brtnode_pf_req_callback,
        toku_brtnode_pf_callback,
        toku_brtnode_cleaner_callback,
        bfe,
        h,
        num_dependent_nodes,
        dependent_cf,
        dependent_keys,
        dependent_fullhash,
        dependent_dirty_bits
        );
    assert(r==0);
    BRTNODE node = node_v;
    *node_p = node;
}

void
checkpoint_nodes(struct brt_header* h,
                 u_int32_t num_dependent_nodes,
                 BRTNODE* dependent_nodes)
{
    CACHEFILE dependent_cf[num_dependent_nodes];
    BLOCKNUM dependent_keys[num_dependent_nodes];
    u_int32_t dependent_fullhash[num_dependent_nodes];
    enum cachetable_dirty dependent_dirty_bits[num_dependent_nodes];
    for (u_int32_t i = 0; i < num_dependent_nodes; i++) {
        dependent_cf[i] = h->cf;
        dependent_keys[i] = dependent_nodes[i]->thisnodename;
        dependent_fullhash[i] = toku_cachetable_hash(h->cf, dependent_nodes[i]->thisnodename);
        dependent_dirty_bits[i] = (enum cachetable_dirty) dependent_nodes[i]->dirty;
    }
    toku_checkpoint_pairs(
        h->cf,
        num_dependent_nodes,
        dependent_cf,
        dependent_keys,
        dependent_fullhash,
        dependent_dirty_bits
        );
}

void
toku_unpin_brtnode_off_client_thread(struct brt_header* h, BRTNODE node)
// Effect: Unpin a brt node.
{
    int r = toku_cachetable_unpin(
        h->cf,
        node->thisnodename,
        node->fullhash,
        (enum cachetable_dirty) node->dirty,
        make_brtnode_pair_attr(node)
        );
    assert(r==0);
}

void
toku_unpin_brtnode(BRT brt, BRTNODE node)
// Effect: Unpin a brt node.
{
    // printf("%*sUnpin %ld\n", 8-node->height, "", node->thisnodename.b);
    VERIFY_NODE(brt,node);
    toku_unpin_brtnode_off_client_thread(brt->h, node);
}

