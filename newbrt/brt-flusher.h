/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef BRT_FLUSHER
#define BRT_FLUSHER
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// This must be first to make the 64-bit file mode work right in Linux
#include <brttypes.h>
#include <c_dialects.h>

C_BEGIN

void
toku_flusher_thread_set_callback(
    void (*callback_f)(int, void*),
    void* extra
    );

int
toku_brtnode_cleaner_callback_internal(
    void *brtnode_pv,
    BLOCKNUM blocknum,
    u_int32_t fullhash,
    void *extraargs,
    BRT_STATUS brt_status
    );

void
flush_node_on_background_thread(
    BRT brt,
    BRTNODE parent,
    BRT_STATUS brt_status
    );

void
brtleaf_split(
    struct brt_header* h,
    BRTNODE node,
    BRTNODE *nodea,
    BRTNODE *nodeb,
    DBT *splitk,
    BOOL create_new_node,
    u_int32_t num_dependent_nodes,
    BRTNODE* dependent_nodes
    );

void
brt_nonleaf_split(
    struct brt_header* h,
    BRTNODE node,
    BRTNODE *nodea,
    BRTNODE *nodeb,
    DBT *splitk,
    u_int32_t num_dependent_nodes,
    BRTNODE* dependent_nodes
    );

C_END

#endif // End of header guardian.
