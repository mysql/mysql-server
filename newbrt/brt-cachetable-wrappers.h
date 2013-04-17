/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef BRT_CACHETABLE_WRAPPERS_H
#define BRT_CACHETABLE_WRAPPERS_H

#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <c_dialects.h>
#include <brttypes.h>

C_BEGIN

void
cachetable_put_empty_node_with_dep_nodes(
    struct brt_header* h,
    u_int32_t num_dependent_nodes,
    BRTNODE* dependent_nodes,
    BLOCKNUM* name, //output
    u_int32_t* fullhash, //output
    BRTNODE* result
    );

void
create_new_brtnode_with_dep_nodes(
    struct brt_header* h,
    BRTNODE *result,
    int height,
    int n_children,
    u_int32_t num_dependent_nodes,
    BRTNODE* dependent_nodes
    );

void
toku_create_new_brtnode (
    BRT t,
    BRTNODE *result,
    int height,
    int n_children
    );

void
toku_pin_brtnode_off_client_thread(
    struct brt_header* h,
    BLOCKNUM blocknum,
    u_int32_t fullhash,
    BRTNODE_FETCH_EXTRA bfe,
    u_int32_t num_dependent_nodes,
    BRTNODE* dependent_nodes,
    BRTNODE *node_p
    );

void
checkpoint_nodes(
    struct brt_header* h,
    u_int32_t num_dependent_nodes,
    BRTNODE* dependent_nodes
    );

int
toku_pin_brtnode(
    BRT brt,
    BLOCKNUM blocknum,
    u_int32_t fullhash,
    UNLOCKERS unlockers,
    ANCESTORS ancestors,
    const PIVOT_BOUNDS pbounds,
    BRTNODE_FETCH_EXTRA bfe,
    BOOL apply_ancestor_messages, // this BOOL is probably temporary, for #3972, once we know how range query estimates work, will revisit this
    BRTNODE *node_p
    ) __attribute__((__warn_unused_result__));

void
toku_pin_brtnode_holding_lock(
    BRT brt,
    BLOCKNUM blocknum,
    u_int32_t fullhash,
    ANCESTORS ancestors,
    const PIVOT_BOUNDS pbounds,
    BRTNODE_FETCH_EXTRA bfe,
    BOOL apply_ancestor_messages,
    BRTNODE *node_p
    );

void
toku_unpin_brtnode_off_client_thread(struct brt_header* h, BRTNODE node);

void
toku_unpin_brtnode(BRT brt, BRTNODE node);

C_END

#endif
