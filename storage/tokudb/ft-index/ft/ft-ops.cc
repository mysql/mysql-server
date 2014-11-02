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

/*

Managing the tree shape:  How insertion, deletion, and querying work

When we insert a message into the FT_HANDLE, here's what happens.

to insert a message at the root

    - find the root node
    - capture the next msn of the root node and assign it to the message
    - split the root if it needs to be split
    - insert the message into the root buffer
    - if the root is too full, then toku_ft_flush_some_child() of the root on a flusher thread

flusher functions use an advice struct with provides some functions to
call that tell it what to do based on the context of the flush. see ft-flusher.h

to flush some child, given a parent and some advice
    - pick the child using advice->pick_child()
    - remove that childs buffer from the parent
    - flush the buffer to the child
    - if the child has stable reactivity and
      advice->should_recursively_flush() is true, then
      toku_ft_flush_some_child() of the child
    - otherwise split the child if it needs to be split
    - otherwise maybe merge the child if it needs to be merged

flusher threads:

    flusher threads are created on demand as the result of internal nodes
    becoming gorged by insertions. this allows flushing to be done somewhere
    other than the client thread. these work items are enqueued onto
    the cachetable kibbutz and are done in a first in first out order.

cleaner threads:

    the cleaner thread wakes up every so often (say, 1 second) and chooses
    a small number (say, 5) of nodes as candidates for a flush. the one
    with the largest cache pressure is chosen to be flushed. cache pressure
    is a function of the size of the node in the cachetable plus the work done.
    the cleaner thread need not actually do a flush when awoken, so only
    nodes that have sufficient cache pressure are flushed.

checkpointing:

    the checkpoint thread wakes up every minute to checkpoint dirty nodes
    to disk. at the time of this writing, nodes during checkpoint are
    locked and cannot be queried or flushed to. a design in which nodes
    are copied before checkpoint is being considered as a way to reduce
    the performance variability caused by a checkpoint locking too
    many nodes and preventing other threads from traversing down the tree,
    for a query or otherwise.

To shrink a file: Let X be the size of the reachable data.
    We define an acceptable bloat constant of C.  For example we set C=2 if we are willing to allow the file to be as much as 2X in size.
    The goal is to find the smallest amount of stuff we can move to get the file down to size CX.
    That seems like a difficult problem, so we use the following heuristics:
       If we can relocate the last block to an lower location, then do so immediately.        (The file gets smaller right away, so even though the new location
         may even not be in the first CX bytes, we are making the file smaller.)
       Otherwise all of the earlier blocks are smaller than the last block (of size L).         So find the smallest region that has L free bytes in it.
         (This can be computed in one pass)
         Move the first allocated block in that region to some location not in the interior of the region.
               (Outside of the region is OK, and reallocating the block at the edge of the region is OK).
            This has the effect of creating a smaller region with at least L free bytes in it.
         Go back to the top (because by now some other block may have been allocated or freed).
    Claim: if there are no other allocations going on concurrently, then this algorithm will shrink the file reasonably efficiently.  By this I mean that
       each block of shrinkage does the smallest amount of work possible.  That doesn't mean that the work overall is minimized.
    Note: If there are other allocations and deallocations going on concurrently, we might never get enough space to move the last block.  But it takes a lot
      of allocations and deallocations to make that happen, and it's probably reasonable for the file not to shrink in this case.

To split or merge a child of a node:
Split_or_merge (node, childnum) {
  If the child needs to be split (it's a leaf with too much stuff or a nonleaf with too much fanout)
    fetch the node and the child into main memory.
    split the child, producing two nodes A and B, and also a pivot.   Don't worry if the resulting child is still too big or too small.         Fix it on the next pass.
    fixup node to point at the two new children.  Don't worry about the node getting too much fanout.
    return;
  If the child needs to be merged (it's a leaf with too little stuff (less than 1/4 full) or a nonleaf with too little fanout (less than 1/4)
    fetch node, the child  and a sibling of the child into main memory.
    move all messages from the node to the two children (so that the message buffers are empty)
    If the two siblings together fit into one node then
      merge the two siblings.
      fixup the node to point at one child
    Otherwise
      load balance the content of the two nodes
    Don't worry about the resulting children having too many messages or otherwise being too big or too small.        Fix it on the next pass.
  }
}

Here's how querying works:

lookups:
    - As of Dr. No, we don't do any tree shaping on lookup.
    - We don't promote eagerly or use aggressive promotion or passive-aggressive
    promotion.        We just push messages down according to the traditional FT_HANDLE
    algorithm on insertions.
    - when a node is brought into memory, we apply ancestor messages above it.

basement nodes, bulk fetch,  and partial fetch:
    - leaf nodes are comprised of N basement nodes, each of nominal size. when
    a query hits a leaf node. it may require one or more basement nodes to be in memory.
    - for point queries, we do not read the entire node into memory. instead,
      we only read in the required basement node
    - for range queries, cursors may return cursor continue in their callback
      to take a the shortcut path until the end of the basement node.
    - for range queries, cursors may prelock a range of keys (with or without a txn).
      the fractal tree will prefetch nodes aggressively until the end of the range.
    - without a prelocked range, range queries behave like successive point queries.

*/

#include <config.h>

#include "ft/cachetable/checkpoint.h"
#include "ft/cursor.h"
#include "ft/ft.h"
#include "ft/ft-cachetable-wrappers.h"
#include "ft/ft-flusher.h"
#include "ft/ft-internal.h"
#include "ft/msg.h"
#include "ft/leafentry.h"
#include "ft/logger/log-internal.h"
#include "ft/node.h"
#include "ft/serialize/block_table.h"
#include "ft/serialize/sub_block.h"
#include "ft/serialize/ft-serialize.h"
#include "ft/serialize/ft_layout_version.h"
#include "ft/serialize/ft_node-serialize.h"
#include "ft/txn/txn_manager.h"
#include "ft/ule.h"
#include "ft/txn/xids.h"

#include <toku_race_tools.h>

#include <portability/toku_atomic.h>

#include <util/context.h>
#include <util/mempool.h>
#include <util/status.h>
#include <util/rwlock.h>
#include <util/sort.h>
#include <util/scoped_malloc.h>

#include <stdint.h>

/* Status is intended for display to humans to help understand system behavior.
 * It does not need to be perfectly thread-safe.
 */
static FT_STATUS_S ft_status;

#define STATUS_INIT(k,c,t,l,inc) TOKUFT_STATUS_INIT(ft_status, k, c, t, "ft: " l, inc)

static toku_mutex_t ft_open_close_lock;

static void
status_init(void)
{
    // Note, this function initializes the keyname, type, and legend fields.
    // Value fields are initialized to zero by compiler.
    STATUS_INIT(FT_UPDATES,                                DICTIONARY_UPDATES, PARCOUNT, "dictionary updates", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_UPDATES_BROADCAST,                      DICTIONARY_BROADCAST_UPDATES, PARCOUNT, "dictionary broadcast updates", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_DESCRIPTOR_SET,                         DESCRIPTOR_SET, PARCOUNT, "descriptor set", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_MSN_DISCARDS,                           MESSAGES_IGNORED_BY_LEAF_DUE_TO_MSN, PARCOUNT, "messages ignored by leaf due to msn", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_TOTAL_RETRIES,                          nullptr, PARCOUNT, "total search retries due to TRY_AGAIN", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_SEARCH_TRIES_GT_HEIGHT,                 nullptr, PARCOUNT, "searches requiring more tries than the height of the tree", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_SEARCH_TRIES_GT_HEIGHTPLUS3,            nullptr, PARCOUNT, "searches requiring more tries than the height of the tree plus three", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_CREATE_LEAF,                            LEAF_NODES_CREATED, PARCOUNT, "leaf nodes created", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_CREATE_NONLEAF,                         NONLEAF_NODES_CREATED, PARCOUNT, "nonleaf nodes created", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_DESTROY_LEAF,                           LEAF_NODES_DESTROYED, PARCOUNT, "leaf nodes destroyed", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_DESTROY_NONLEAF,                        NONLEAF_NODES_DESTROYED, PARCOUNT, "nonleaf nodes destroyed", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_MSG_BYTES_IN,                           MESSAGES_INJECTED_AT_ROOT_BYTES, PARCOUNT, "bytes of messages injected at root (all trees)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_MSG_BYTES_OUT,                          MESSAGES_FLUSHED_FROM_H1_TO_LEAVES_BYTES, PARCOUNT, "bytes of messages flushed from h1 nodes to leaves", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_MSG_BYTES_CURR,                         MESSAGES_IN_TREES_ESTIMATE_BYTES, PARCOUNT, "bytes of messages currently in trees (estimate)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_MSG_NUM,                                MESSAGES_INJECTED_AT_ROOT, PARCOUNT, "messages injected at root", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_MSG_NUM_BROADCAST,                      BROADCASE_MESSAGES_INJECTED_AT_ROOT, PARCOUNT, "broadcast messages injected at root", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);

    STATUS_INIT(FT_NUM_BASEMENTS_DECOMPRESSED_NORMAL,      BASEMENTS_DECOMPRESSED_TARGET_QUERY, PARCOUNT, "basements decompressed as a target of a query", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_NUM_BASEMENTS_DECOMPRESSED_AGGRESSIVE,  BASEMENTS_DECOMPRESSED_PRELOCKED_RANGE, PARCOUNT, "basements decompressed for prelocked range", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_NUM_BASEMENTS_DECOMPRESSED_PREFETCH,    BASEMENTS_DECOMPRESSED_PREFETCH, PARCOUNT, "basements decompressed for prefetch", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_NUM_BASEMENTS_DECOMPRESSED_WRITE,       BASEMENTS_DECOMPRESSED_FOR_WRITE, PARCOUNT, "basements decompressed for write", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_NUM_MSG_BUFFER_DECOMPRESSED_NORMAL,     BUFFERS_DECOMPRESSED_TARGET_QUERY, PARCOUNT, "buffers decompressed as a target of a query", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_NUM_MSG_BUFFER_DECOMPRESSED_AGGRESSIVE, BUFFERS_DECOMPRESSED_PRELOCKED_RANGE, PARCOUNT, "buffers decompressed for prelocked range", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_NUM_MSG_BUFFER_DECOMPRESSED_PREFETCH,   BUFFERS_DECOMPRESSED_PREFETCH, PARCOUNT, "buffers decompressed for prefetch", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_NUM_MSG_BUFFER_DECOMPRESSED_WRITE,      BUFFERS_DECOMPRESSED_FOR_WRITE, PARCOUNT, "buffers decompressed for write", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);

    // Eviction statistics:
    STATUS_INIT(FT_FULL_EVICTIONS_LEAF,                    LEAF_NODE_FULL_EVICTIONS, PARCOUNT, "leaf node full evictions", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_FULL_EVICTIONS_LEAF_BYTES,              LEAF_NODE_FULL_EVICTIONS_BYTES, PARCOUNT, "leaf node full evictions (bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_FULL_EVICTIONS_NONLEAF,                 NONLEAF_NODE_FULL_EVICTIONS, PARCOUNT, "nonleaf node full evictions", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_FULL_EVICTIONS_NONLEAF_BYTES,           NONLEAF_NODE_FULL_EVICTIONS_BYTES, PARCOUNT, "nonleaf node full evictions (bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_PARTIAL_EVICTIONS_LEAF,                 LEAF_NODE_PARTIAL_EVICTIONS, PARCOUNT, "leaf node partial evictions", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_PARTIAL_EVICTIONS_LEAF_BYTES,           LEAF_NODE_PARTIAL_EVICTIONS_BYTES, PARCOUNT, "leaf node partial evictions (bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_PARTIAL_EVICTIONS_NONLEAF,              NONLEAF_NODE_PARTIAL_EVICTIONS, PARCOUNT, "nonleaf node partial evictions", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_PARTIAL_EVICTIONS_NONLEAF_BYTES,        NONLEAF_NODE_PARTIAL_EVICTIONS_BYTES, PARCOUNT, "nonleaf node partial evictions (bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);

    // Disk read statistics: 
    //
    // Pivots: For queries, prefetching, or writing.
    STATUS_INIT(FT_NUM_PIVOTS_FETCHED_QUERY,               PIVOTS_FETCHED_FOR_QUERY, PARCOUNT, "pivots fetched for query", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_BYTES_PIVOTS_FETCHED_QUERY,             PIVOTS_FETCHED_FOR_QUERY_BYTES, PARCOUNT, "pivots fetched for query (bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_TOKUTIME_PIVOTS_FETCHED_QUERY,          PIVOTS_FETCHED_FOR_QUERY_SECONDS, TOKUTIME, "pivots fetched for query (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_NUM_PIVOTS_FETCHED_PREFETCH,            PIVOTS_FETCHED_FOR_PREFETCH, PARCOUNT, "pivots fetched for prefetch", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_BYTES_PIVOTS_FETCHED_PREFETCH,          PIVOTS_FETCHED_FOR_PREFETCH_BYTES, PARCOUNT, "pivots fetched for prefetch (bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_TOKUTIME_PIVOTS_FETCHED_PREFETCH,       PIVOTS_FETCHED_FOR_PREFETCH_SECONDS, TOKUTIME, "pivots fetched for prefetch (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_NUM_PIVOTS_FETCHED_WRITE,               PIVOTS_FETCHED_FOR_WRITE, PARCOUNT, "pivots fetched for write", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_BYTES_PIVOTS_FETCHED_WRITE,             PIVOTS_FETCHED_FOR_WRITE_BYTES, PARCOUNT, "pivots fetched for write (bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_TOKUTIME_PIVOTS_FETCHED_WRITE,          PIVOTS_FETCHED_FOR_WRITE_SECONDS, TOKUTIME, "pivots fetched for write (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    // Basements: For queries, aggressive fetching in prelocked range, prefetching, or writing.
    STATUS_INIT(FT_NUM_BASEMENTS_FETCHED_NORMAL,           BASEMENTS_FETCHED_TARGET_QUERY, PARCOUNT, "basements fetched as a target of a query", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_BYTES_BASEMENTS_FETCHED_NORMAL,         BASEMENTS_FETCHED_TARGET_QUERY_BYTES, PARCOUNT, "basements fetched as a target of a query (bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_TOKUTIME_BASEMENTS_FETCHED_NORMAL,      BASEMENTS_FETCHED_TARGET_QUERY_SECONDS, TOKUTIME, "basements fetched as a target of a query (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_NUM_BASEMENTS_FETCHED_AGGRESSIVE,       BASEMENTS_FETCHED_PRELOCKED_RANGE, PARCOUNT, "basements fetched for prelocked range", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_BYTES_BASEMENTS_FETCHED_AGGRESSIVE,     BASEMENTS_FETCHED_PRELOCKED_RANGE_BYTES, PARCOUNT, "basements fetched for prelocked range (bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_TOKUTIME_BASEMENTS_FETCHED_AGGRESSIVE,  BASEMENTS_FETCHED_PRELOCKED_RANGE_SECONDS, TOKUTIME, "basements fetched for prelocked range (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_NUM_BASEMENTS_FETCHED_PREFETCH,         BASEMENTS_FETCHED_PREFETCH, PARCOUNT, "basements fetched for prefetch", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_BYTES_BASEMENTS_FETCHED_PREFETCH,       BASEMENTS_FETCHED_PREFETCH_BYTES, PARCOUNT, "basements fetched for prefetch (bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_TOKUTIME_BASEMENTS_FETCHED_PREFETCH,    BASEMENTS_FETCHED_PREFETCH_SECONDS, TOKUTIME, "basements fetched for prefetch (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_NUM_BASEMENTS_FETCHED_WRITE,            BASEMENTS_FETCHED_FOR_WRITE, PARCOUNT, "basements fetched for write", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_BYTES_BASEMENTS_FETCHED_WRITE,          BASEMENTS_FETCHED_FOR_WRITE_BYTES, PARCOUNT, "basements fetched for write (bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_TOKUTIME_BASEMENTS_FETCHED_WRITE,       BASEMENTS_FETCHED_FOR_WRITE_SECONDS, TOKUTIME, "basements fetched for write (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    // Buffers: For queries, aggressive fetching in prelocked range, prefetching, or writing.
    STATUS_INIT(FT_NUM_MSG_BUFFER_FETCHED_NORMAL,          BUFFERS_FETCHED_TARGET_QUERY, PARCOUNT, "buffers fetched as a target of a query", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_BYTES_MSG_BUFFER_FETCHED_NORMAL,        BUFFERS_FETCHED_TARGET_QUERY_BYTES, PARCOUNT, "buffers fetched as a target of a query (bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_TOKUTIME_MSG_BUFFER_FETCHED_NORMAL,     BUFFERS_FETCHED_TARGET_QUERY_SECONDS, TOKUTIME, "buffers fetched as a target of a query (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_NUM_MSG_BUFFER_FETCHED_AGGRESSIVE,      BUFFERS_FETCHED_PRELOCKED_RANGE, PARCOUNT, "buffers fetched for prelocked range", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_BYTES_MSG_BUFFER_FETCHED_AGGRESSIVE,    BUFFERS_FETCHED_PRELOCKED_RANGE_BYTES, PARCOUNT, "buffers fetched for prelocked range (bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_TOKUTIME_MSG_BUFFER_FETCHED_AGGRESSIVE, BUFFERS_FETCHED_PRELOCKED_RANGE_SECONDS, TOKUTIME, "buffers fetched for prelocked range (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_NUM_MSG_BUFFER_FETCHED_PREFETCH,        BUFFERS_FETCHED_PREFETCH, PARCOUNT, "buffers fetched for prefetch", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_BYTES_MSG_BUFFER_FETCHED_PREFETCH,      BUFFERS_FETCHED_PREFETCH_BYTES, PARCOUNT, "buffers fetched for prefetch (bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_TOKUTIME_MSG_BUFFER_FETCHED_PREFETCH,   BUFFERS_FETCHED_PREFETCH_SECONDS, TOKUTIME, "buffers fetched for prefetch (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_NUM_MSG_BUFFER_FETCHED_WRITE,           BUFFERS_FETCHED_FOR_WRITE, PARCOUNT, "buffers fetched for write", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_BYTES_MSG_BUFFER_FETCHED_WRITE,         BUFFERS_FETCHED_FOR_WRITE_BYTES, PARCOUNT, "buffers fetched for write (bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_TOKUTIME_MSG_BUFFER_FETCHED_WRITE,      BUFFERS_FETCHED_FOR_WRITE_SECONDS, TOKUTIME, "buffers fetched for write (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);

    // Disk write statistics.
    //
    // Leaf/Nonleaf: Not for checkpoint
    STATUS_INIT(FT_DISK_FLUSH_LEAF,                                         LEAF_NODES_FLUSHED_NOT_CHECKPOINT, PARCOUNT, "leaf nodes flushed to disk (not for checkpoint)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_DISK_FLUSH_LEAF_BYTES,                                   LEAF_NODES_FLUSHED_NOT_CHECKPOINT_BYTES, PARCOUNT, "leaf nodes flushed to disk (not for checkpoint) (bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_DISK_FLUSH_LEAF_UNCOMPRESSED_BYTES,                      LEAF_NODES_FLUSHED_NOT_CHECKPOINT_UNCOMPRESSED_BYTES, PARCOUNT, "leaf nodes flushed to disk (not for checkpoint) (uncompressed bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_DISK_FLUSH_LEAF_TOKUTIME,                                LEAF_NODES_FLUSHED_NOT_CHECKPOINT_SECONDS, TOKUTIME, "leaf nodes flushed to disk (not for checkpoint) (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_DISK_FLUSH_NONLEAF,                                      NONLEAF_NODES_FLUSHED_TO_DISK_NOT_CHECKPOINT, PARCOUNT, "nonleaf nodes flushed to disk (not for checkpoint)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_DISK_FLUSH_NONLEAF_BYTES,                                NONLEAF_NODES_FLUSHED_TO_DISK_NOT_CHECKPOINT_BYTES, PARCOUNT, "nonleaf nodes flushed to disk (not for checkpoint) (bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_DISK_FLUSH_NONLEAF_UNCOMPRESSED_BYTES,                   NONLEAF_NODES_FLUSHED_TO_DISK_NOT_CHECKPOINT_UNCOMPRESSED_BYTES, PARCOUNT, "nonleaf nodes flushed to disk (not for checkpoint) (uncompressed bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_DISK_FLUSH_NONLEAF_TOKUTIME,                             NONLEAF_NODES_FLUSHED_TO_DISK_NOT_CHECKPOINT_SECONDS, TOKUTIME, "nonleaf nodes flushed to disk (not for checkpoint) (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    // Leaf/Nonleaf: For checkpoint
    STATUS_INIT(FT_DISK_FLUSH_LEAF_FOR_CHECKPOINT,                          LEAF_NODES_FLUSHED_CHECKPOINT, PARCOUNT, "leaf nodes flushed to disk (for checkpoint)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_DISK_FLUSH_LEAF_BYTES_FOR_CHECKPOINT,                    LEAF_NODES_FLUSHED_CHECKPOINT_BYTES, PARCOUNT, "leaf nodes flushed to disk (for checkpoint) (bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_DISK_FLUSH_LEAF_UNCOMPRESSED_BYTES_FOR_CHECKPOINT,       LEAF_NODES_FLUSHED_CHECKPOINT_UNCOMPRESSED_BYTES, PARCOUNT, "leaf nodes flushed to disk (for checkpoint) (uncompressed bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_DISK_FLUSH_LEAF_TOKUTIME_FOR_CHECKPOINT,                 LEAF_NODES_FLUSHED_CHECKPOINT_SECONDS, TOKUTIME, "leaf nodes flushed to disk (for checkpoint) (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_DISK_FLUSH_NONLEAF_FOR_CHECKPOINT,                       NONLEAF_NODES_FLUSHED_TO_DISK_CHECKPOINT, PARCOUNT, "nonleaf nodes flushed to disk (for checkpoint)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_DISK_FLUSH_NONLEAF_BYTES_FOR_CHECKPOINT,                 NONLEAF_NODES_FLUSHED_TO_DISK_CHECKPOINT_BYTES, PARCOUNT, "nonleaf nodes flushed to disk (for checkpoint) (bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_DISK_FLUSH_NONLEAF_UNCOMPRESSED_BYTES_FOR_CHECKPOINT,    NONLEAF_NODES_FLUSHED_TO_DISK_CHECKPOINT_UNCOMPRESSED_BYTES, PARCOUNT, "nonleaf nodes flushed to disk (for checkpoint) (uncompressed bytes)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_DISK_FLUSH_NONLEAF_TOKUTIME_FOR_CHECKPOINT,              NONLEAF_NODES_FLUSHED_TO_DISK_CHECKPOINT_SECONDS, TOKUTIME, "nonleaf nodes flushed to disk (for checkpoint) (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_DISK_FLUSH_LEAF_COMPRESSION_RATIO,                       LEAF_NODE_COMPRESSION_RATIO, DOUBLE, "uncompressed / compressed bytes written (leaf)", TOKU_GLOBAL_STATUS|TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_DISK_FLUSH_NONLEAF_COMPRESSION_RATIO,                    NONLEAF_NODE_COMPRESSION_RATIO, DOUBLE, "uncompressed / compressed bytes written (nonleaf)", TOKU_GLOBAL_STATUS|TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_DISK_FLUSH_OVERALL_COMPRESSION_RATIO,                    OVERALL_NODE_COMPRESSION_RATIO, DOUBLE, "uncompressed / compressed bytes written (overall)", TOKU_GLOBAL_STATUS|TOKU_ENGINE_STATUS);

    // CPU time statistics for [de]serialization and [de]compression.
    STATUS_INIT(FT_LEAF_COMPRESS_TOKUTIME,                                  LEAF_COMPRESSION_TO_MEMORY_SECONDS, TOKUTIME, "leaf compression to memory (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_LEAF_SERIALIZE_TOKUTIME,                                 LEAF_SERIALIZATION_TO_MEMORY_SECONDS, TOKUTIME, "leaf serialization to memory (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_LEAF_DECOMPRESS_TOKUTIME,                                LEAF_DECOMPRESSION_TO_MEMORY_SECONDS, TOKUTIME, "leaf decompression to memory (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_LEAF_DESERIALIZE_TOKUTIME,                               LEAF_DESERIALIZATION_TO_MEMORY_SECONDS, TOKUTIME, "leaf deserialization to memory (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_NONLEAF_COMPRESS_TOKUTIME,                               NONLEAF_COMPRESSION_TO_MEMORY_SECONDS, TOKUTIME, "nonleaf compression to memory (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_NONLEAF_SERIALIZE_TOKUTIME,                              NONLEAF_SERIALIZATION_TO_MEMORY_SECONDS, TOKUTIME, "nonleaf serialization to memory (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_NONLEAF_DECOMPRESS_TOKUTIME,                             NONLEAF_DECOMPRESSION_TO_MEMORY_SECONDS, TOKUTIME, "nonleaf decompression to memory (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_NONLEAF_DESERIALIZE_TOKUTIME,                            NONLEAF_DESERIALIZATION_TO_MEMORY_SECONDS, TOKUTIME, "nonleaf deserialization to memory (seconds)", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);

    // Promotion statistics.
    STATUS_INIT(FT_PRO_NUM_ROOT_SPLIT,                     PROMOTION_ROOTS_SPLIT, PARCOUNT, "promotion: roots split", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_PRO_NUM_ROOT_H0_INJECT,                 PROMOTION_LEAF_ROOTS_INJECTED_INTO, PARCOUNT, "promotion: leaf roots injected into", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_PRO_NUM_ROOT_H1_INJECT,                 PROMOTION_H1_ROOTS_INJECTED_INTO, PARCOUNT, "promotion: h1 roots injected into", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_PRO_NUM_INJECT_DEPTH_0,                 PROMOTION_INJECTIONS_AT_DEPTH_0, PARCOUNT, "promotion: injections at depth 0", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_PRO_NUM_INJECT_DEPTH_1,                 PROMOTION_INJECTIONS_AT_DEPTH_1, PARCOUNT, "promotion: injections at depth 1", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_PRO_NUM_INJECT_DEPTH_2,                 PROMOTION_INJECTIONS_AT_DEPTH_2, PARCOUNT, "promotion: injections at depth 2", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_PRO_NUM_INJECT_DEPTH_3,                 PROMOTION_INJECTIONS_AT_DEPTH_3, PARCOUNT, "promotion: injections at depth 3", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_PRO_NUM_INJECT_DEPTH_GT3,               PROMOTION_INJECTIONS_LOWER_THAN_DEPTH_3, PARCOUNT, "promotion: injections lower than depth 3", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_PRO_NUM_STOP_NONEMPTY_BUF,              PROMOTION_STOPPED_NONEMPTY_BUFFER, PARCOUNT, "promotion: stopped because of a nonempty buffer", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_PRO_NUM_STOP_H1,                        PROMOTION_STOPPED_AT_HEIGHT_1, PARCOUNT, "promotion: stopped at height 1", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_PRO_NUM_STOP_LOCK_CHILD,                PROMOTION_STOPPED_CHILD_LOCKED_OR_NOT_IN_MEMORY, PARCOUNT, "promotion: stopped because the child was locked or not at all in memory", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_PRO_NUM_STOP_CHILD_INMEM,               PROMOTION_STOPPED_CHILD_NOT_FULLY_IN_MEMORY, PARCOUNT, "promotion: stopped because the child was not fully in memory", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_PRO_NUM_DIDNT_WANT_PROMOTE,             PROMOTION_STOPPED_AFTER_LOCKING_CHILD, PARCOUNT, "promotion: stopped anyway, after locking the child", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_BASEMENT_DESERIALIZE_FIXED_KEYSIZE,     BASEMENT_DESERIALIZATION_FIXED_KEY, PARCOUNT, "basement nodes deserialized with fixed-keysize", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_BASEMENT_DESERIALIZE_VARIABLE_KEYSIZE,  BASEMENT_DESERIALIZATION_VARIABLE_KEY, PARCOUNT, "basement nodes deserialized with variable-keysize", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(FT_PRO_RIGHTMOST_LEAF_SHORTCUT_SUCCESS,    nullptr, PARCOUNT, "promotion: succeeded in using the rightmost leaf shortcut", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_PRO_RIGHTMOST_LEAF_SHORTCUT_FAIL_POS,   nullptr, PARCOUNT, "promotion: tried the rightmost leaf shorcut but failed (out-of-bounds)", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_PRO_RIGHTMOST_LEAF_SHORTCUT_FAIL_REACTIVE,nullptr, PARCOUNT, "promotion: tried the rightmost leaf shorcut but failed (child reactive)", TOKU_ENGINE_STATUS);

    ft_status.initialized = true;
}
static void status_destroy(void) {
    for (int i = 0; i < FT_STATUS_NUM_ROWS; ++i) {
        if (ft_status.status[i].type == PARCOUNT) {
            destroy_partitioned_counter(ft_status.status[i].value.parcount);
        }
    }
}
#undef STATUS_INIT

#define STATUS_VAL(x)                                                               \
    (ft_status.status[x].type == PARCOUNT ?                                         \
        read_partitioned_counter(ft_status.status[x].value.parcount) :              \
        ft_status.status[x].value.num)

void
toku_ft_get_status(FT_STATUS s) {
    *s = ft_status;

    // Calculate compression ratios for leaf and nonleaf nodes
    const double compressed_leaf_bytes = STATUS_VAL(FT_DISK_FLUSH_LEAF_BYTES) +
                                         STATUS_VAL(FT_DISK_FLUSH_LEAF_BYTES_FOR_CHECKPOINT);
    const double uncompressed_leaf_bytes = STATUS_VAL(FT_DISK_FLUSH_LEAF_UNCOMPRESSED_BYTES) +
                                           STATUS_VAL(FT_DISK_FLUSH_LEAF_UNCOMPRESSED_BYTES_FOR_CHECKPOINT);
    const double compressed_nonleaf_bytes = STATUS_VAL(FT_DISK_FLUSH_NONLEAF_BYTES) +
                                            STATUS_VAL(FT_DISK_FLUSH_NONLEAF_BYTES_FOR_CHECKPOINT);
    const double uncompressed_nonleaf_bytes = STATUS_VAL(FT_DISK_FLUSH_NONLEAF_UNCOMPRESSED_BYTES) +
                                              STATUS_VAL(FT_DISK_FLUSH_NONLEAF_UNCOMPRESSED_BYTES_FOR_CHECKPOINT);

    if (compressed_leaf_bytes > 0) {
        s->status[FT_DISK_FLUSH_LEAF_COMPRESSION_RATIO].value.dnum = uncompressed_leaf_bytes / compressed_leaf_bytes;
    }
    if (compressed_nonleaf_bytes > 0) {
        s->status[FT_DISK_FLUSH_NONLEAF_COMPRESSION_RATIO].value.dnum = uncompressed_nonleaf_bytes / compressed_nonleaf_bytes;
    }
    if (compressed_leaf_bytes > 0 || compressed_nonleaf_bytes > 0) {
        s->status[FT_DISK_FLUSH_OVERALL_COMPRESSION_RATIO].value.dnum =
            (uncompressed_leaf_bytes + uncompressed_nonleaf_bytes) / (compressed_leaf_bytes + compressed_nonleaf_bytes);
    }
}

#define STATUS_INC(x, d)                                                            \
    do {                                                                            \
        if (ft_status.status[x].type == PARCOUNT) {                                 \
            increment_partitioned_counter(ft_status.status[x].value.parcount, d);   \
        } else {                                                                    \
            toku_sync_fetch_and_add(&ft_status.status[x].value.num, d);             \
        }                                                                           \
    } while (0)


void toku_note_deserialized_basement_node(bool fixed_key_size) {
    if (fixed_key_size) {
        STATUS_INC(FT_BASEMENT_DESERIALIZE_FIXED_KEYSIZE, 1);
    } else {
        STATUS_INC(FT_BASEMENT_DESERIALIZE_VARIABLE_KEYSIZE, 1);
    }
}

static void ft_verify_flags(FT UU(ft), FTNODE UU(node)) {
    paranoid_invariant(ft->h->flags == node->flags);
}

int toku_ft_debug_mode = 0;

uint32_t compute_child_fullhash (CACHEFILE cf, FTNODE node, int childnum) {
    paranoid_invariant(node->height>0);
    paranoid_invariant(childnum<node->n_children);
    return toku_cachetable_hash(cf, BP_BLOCKNUM(node, childnum));
}

//
// pivot bounds
// TODO: move me to ft/node.cc?
// 

pivot_bounds::pivot_bounds(const DBT &lbe_dbt, const DBT &ubi_dbt) :
    _lower_bound_exclusive(lbe_dbt), _upper_bound_inclusive(ubi_dbt) {
}

pivot_bounds pivot_bounds::infinite_bounds() {
    DBT dbt;
    toku_init_dbt(&dbt);

    // infinity is represented by an empty dbt
    invariant(toku_dbt_is_empty(&dbt));
    return pivot_bounds(dbt, dbt);
}

const DBT *pivot_bounds::lbe() const {
    return &_lower_bound_exclusive;
}

const DBT *pivot_bounds::ubi() const {
    return &_upper_bound_inclusive;
}

DBT pivot_bounds::_prepivotkey(FTNODE node, int childnum, const DBT &lbe_dbt) const {
    if (childnum == 0) {
        return lbe_dbt;
    } else {
        return node->pivotkeys.get_pivot(childnum - 1);
    }
}

DBT pivot_bounds::_postpivotkey(FTNODE node, int childnum, const DBT &ubi_dbt) const {
    if (childnum + 1 == node->n_children) {
        return ubi_dbt;
    } else {
        return node->pivotkeys.get_pivot(childnum);
    }
}

pivot_bounds pivot_bounds::next_bounds(FTNODE node, int childnum) const {
    return pivot_bounds(_prepivotkey(node, childnum, _lower_bound_exclusive),
                        _postpivotkey(node, childnum, _upper_bound_inclusive));
}

////////////////////////////////////////////////////////////////////////////////

static long get_avail_internal_node_partition_size(FTNODE node, int i) {
    paranoid_invariant(node->height > 0);
    return toku_bnc_memory_size(BNC(node, i));
}

static long ftnode_cachepressure_size(FTNODE node) {
    long retval = 0;
    bool totally_empty = true;
    if (node->height == 0) {
        goto exit;
    }
    else {
        for (int i = 0; i < node->n_children; i++) {
            if (BP_STATE(node,i) == PT_INVALID || BP_STATE(node,i) == PT_ON_DISK) {
                continue;
            }
            else if (BP_STATE(node,i) == PT_COMPRESSED) {
                SUB_BLOCK sb = BSB(node, i);
                totally_empty = false;
                retval += sb->compressed_size;
            }
            else if (BP_STATE(node,i) == PT_AVAIL) {
                totally_empty = totally_empty && (toku_bnc_n_entries(BNC(node, i)) == 0);
                retval += get_avail_internal_node_partition_size(node, i);
                retval += BP_WORKDONE(node, i);
            }
            else {
                abort();
            }
        }
    }
exit:
    if (totally_empty) {
        return 0;
    }
    return retval;
}

static long
ftnode_memory_size (FTNODE node)
// Effect: Estimate how much main memory a node requires.
{
    long retval = 0;
    int n_children = node->n_children;
    retval += sizeof(*node);
    retval += (n_children)*(sizeof(node->bp[0]));
    retval += node->pivotkeys.total_size();

    // now calculate the sizes of the partitions
    for (int i = 0; i < n_children; i++) {
        if (BP_STATE(node,i) == PT_INVALID || BP_STATE(node,i) == PT_ON_DISK) {
            continue;
        }
        else if (BP_STATE(node,i) == PT_COMPRESSED) {
            SUB_BLOCK sb = BSB(node, i);
            retval += sizeof(*sb);
            retval += sb->compressed_size;
        }
        else if (BP_STATE(node,i) == PT_AVAIL) {
            if (node->height > 0) {
                retval += get_avail_internal_node_partition_size(node, i);
            }
            else {
                BASEMENTNODE bn = BLB(node, i);
                retval += sizeof(*bn);
                retval += BLB_DATA(node, i)->get_memory_size();
            }
        }
        else {
            abort();
        }
    }
    return retval;
}

PAIR_ATTR make_ftnode_pair_attr(FTNODE node) {
    long size = ftnode_memory_size(node);
    long cachepressure_size = ftnode_cachepressure_size(node);
    PAIR_ATTR result={
        .size = size,
        .nonleaf_size = (node->height > 0) ? size : 0,
        .leaf_size = (node->height > 0) ? 0 : size,
        .rollback_size = 0,
        .cache_pressure_size = cachepressure_size,
        .is_valid = true
    };
    return result;
}

PAIR_ATTR make_invalid_pair_attr(void) {
    PAIR_ATTR result={
        .size = 0,
        .nonleaf_size = 0,
        .leaf_size = 0,
        .rollback_size = 0,
        .cache_pressure_size = 0,
        .is_valid = false
    };
    return result;
}


// assign unique dictionary id
static uint64_t dict_id_serial = 1;
static DICTIONARY_ID
next_dict_id(void) {
    uint64_t i = toku_sync_fetch_and_add(&dict_id_serial, 1);
    assert(i);        // guarantee unique dictionary id by asserting 64-bit counter never wraps
    DICTIONARY_ID d = {.dictid = i};
    return d;
}

// TODO: This isn't so pretty
void ftnode_fetch_extra::_create_internal(FT ft_) {
    ft = ft_;
    type = ftnode_fetch_none;
    search = nullptr;

    toku_init_dbt(&range_lock_left_key);
    toku_init_dbt(&range_lock_right_key);
    left_is_neg_infty = false;
    right_is_pos_infty = false;

    // -1 means 'unknown', which is the correct default state
    child_to_read = -1;
    disable_prefetching = false;
    read_all_partitions = false;

    bytes_read = 0;
    io_time = 0;
    deserialize_time = 0;
    decompress_time = 0;
}

void ftnode_fetch_extra::create_for_full_read(FT ft_) {
    _create_internal(ft_);

    type = ftnode_fetch_all;
}

void ftnode_fetch_extra::create_for_keymatch(FT ft_, const DBT *left, const DBT *right,
                                             bool disable_prefetching_, bool read_all_partitions_) {
    _create_internal(ft_);
    invariant(ft->h->type == FT_CURRENT);

    type = ftnode_fetch_keymatch;
    if (left != nullptr) {
        toku_copyref_dbt(&range_lock_left_key, *left);
    }
    if (right != nullptr) {
        toku_copyref_dbt(&range_lock_right_key, *right);
    }
    left_is_neg_infty = left == nullptr;
    right_is_pos_infty = right == nullptr;
    disable_prefetching = disable_prefetching_;
    read_all_partitions = read_all_partitions_;
}

void ftnode_fetch_extra::create_for_subset_read(FT ft_, ft_search *search_,
                                                const DBT *left, const DBT *right,
                                                bool left_is_neg_infty_, bool right_is_pos_infty_,
                                                bool disable_prefetching_, bool read_all_partitions_) {
    _create_internal(ft_);
    invariant(ft->h->type == FT_CURRENT);

    type = ftnode_fetch_subset;
    search = search_;
    if (left != nullptr) {
        toku_copyref_dbt(&range_lock_left_key, *left);
    }
    if (right != nullptr) {
        toku_copyref_dbt(&range_lock_right_key, *right);
    }
    left_is_neg_infty = left_is_neg_infty_;
    right_is_pos_infty = right_is_pos_infty_;
    disable_prefetching = disable_prefetching_;
    read_all_partitions = read_all_partitions_;
}

void ftnode_fetch_extra::create_for_min_read(FT ft_) {
    _create_internal(ft_);
    invariant(ft->h->type == FT_CURRENT);

    type = ftnode_fetch_none;
}

void ftnode_fetch_extra::create_for_prefetch(FT ft_, struct ft_cursor *cursor) {
    _create_internal(ft_);
    invariant(ft->h->type == FT_CURRENT);

    type = ftnode_fetch_prefetch;
    const DBT *left = &cursor->range_lock_left_key;
    if (left->data) {
        toku_clone_dbt(&range_lock_left_key, *left);
    }
    const DBT *right = &cursor->range_lock_right_key;
    if (right->data) {
        toku_clone_dbt(&range_lock_right_key, *right);
    }
    left_is_neg_infty = cursor->left_is_neg_infty;
    right_is_pos_infty = cursor->right_is_pos_infty;
    disable_prefetching = cursor->disable_prefetching;
}

void ftnode_fetch_extra::destroy(void) {
    toku_destroy_dbt(&range_lock_left_key);
    toku_destroy_dbt(&range_lock_right_key);
}

// Requires: child_to_read to have been set
bool ftnode_fetch_extra::wants_child_available(int childnum) const {
    return type == ftnode_fetch_all ||
        (child_to_read == childnum &&
         (type == ftnode_fetch_subset || type == ftnode_fetch_keymatch));
}

int ftnode_fetch_extra::leftmost_child_wanted(FTNODE node) const {
    paranoid_invariant(type == ftnode_fetch_subset ||
                       type == ftnode_fetch_prefetch ||
                       type == ftnode_fetch_keymatch);
    if (left_is_neg_infty) {
        return 0;
    } else if (range_lock_left_key.data == nullptr) {
        return -1;
    } else {
        return toku_ftnode_which_child(node, &range_lock_left_key, ft->cmp);
    }
}

int ftnode_fetch_extra::rightmost_child_wanted(FTNODE node) const {
    paranoid_invariant(type == ftnode_fetch_subset ||
                       type == ftnode_fetch_prefetch ||
                       type == ftnode_fetch_keymatch);
    if (right_is_pos_infty) {
        return node->n_children - 1;
    } else if (range_lock_right_key.data == nullptr) {
        return -1;
    } else {
        return toku_ftnode_which_child(node, &range_lock_right_key, ft->cmp);
    }
}

static int
ft_cursor_rightmost_child_wanted(FT_CURSOR cursor, FT_HANDLE ft_handle, FTNODE node)
{
    if (cursor->right_is_pos_infty) {
        return node->n_children - 1;
    } else if (cursor->range_lock_right_key.data == nullptr) {
        return -1;
    } else {
        return toku_ftnode_which_child(node, &cursor->range_lock_right_key, ft_handle->ft->cmp);
    }
}

STAT64INFO_S
toku_get_and_clear_basement_stats(FTNODE leafnode) {
    invariant(leafnode->height == 0);
    STAT64INFO_S deltas = ZEROSTATS;
    for (int i = 0; i < leafnode->n_children; i++) {
        BASEMENTNODE bn = BLB(leafnode, i);
        invariant(BP_STATE(leafnode,i) == PT_AVAIL);
        deltas.numrows  += bn->stat64_delta.numrows;
        deltas.numbytes += bn->stat64_delta.numbytes;
        bn->stat64_delta = ZEROSTATS;
    }
    return deltas;
}

void toku_ft_status_update_flush_reason(FTNODE node, 
        uint64_t uncompressed_bytes_flushed, uint64_t bytes_written,
        tokutime_t write_time, bool for_checkpoint) {
    if (node->height == 0) {
        if (for_checkpoint) {
            STATUS_INC(FT_DISK_FLUSH_LEAF_FOR_CHECKPOINT, 1);
            STATUS_INC(FT_DISK_FLUSH_LEAF_BYTES_FOR_CHECKPOINT, bytes_written);
            STATUS_INC(FT_DISK_FLUSH_LEAF_UNCOMPRESSED_BYTES_FOR_CHECKPOINT, uncompressed_bytes_flushed);
            STATUS_INC(FT_DISK_FLUSH_LEAF_TOKUTIME_FOR_CHECKPOINT, write_time);
        }
        else {
            STATUS_INC(FT_DISK_FLUSH_LEAF, 1);
            STATUS_INC(FT_DISK_FLUSH_LEAF_BYTES, bytes_written);
            STATUS_INC(FT_DISK_FLUSH_LEAF_UNCOMPRESSED_BYTES, uncompressed_bytes_flushed);
            STATUS_INC(FT_DISK_FLUSH_LEAF_TOKUTIME, write_time);
        }
    }
    else {
        if (for_checkpoint) {
            STATUS_INC(FT_DISK_FLUSH_NONLEAF_FOR_CHECKPOINT, 1);
            STATUS_INC(FT_DISK_FLUSH_NONLEAF_BYTES_FOR_CHECKPOINT, bytes_written);
            STATUS_INC(FT_DISK_FLUSH_NONLEAF_UNCOMPRESSED_BYTES_FOR_CHECKPOINT, uncompressed_bytes_flushed);
            STATUS_INC(FT_DISK_FLUSH_NONLEAF_TOKUTIME_FOR_CHECKPOINT, write_time);
        }
        else {
            STATUS_INC(FT_DISK_FLUSH_NONLEAF, 1);
            STATUS_INC(FT_DISK_FLUSH_NONLEAF_BYTES, bytes_written);
            STATUS_INC(FT_DISK_FLUSH_NONLEAF_UNCOMPRESSED_BYTES, uncompressed_bytes_flushed);
            STATUS_INC(FT_DISK_FLUSH_NONLEAF_TOKUTIME, write_time);
        }
    }
}

void toku_ftnode_checkpoint_complete_callback(void *value_data) {
    FTNODE node = static_cast<FTNODE>(value_data);
    if (node->height > 0) {
        for (int i = 0; i < node->n_children; ++i) {
            if (BP_STATE(node, i) == PT_AVAIL) {
                NONLEAF_CHILDINFO bnc = BNC(node, i);
                bnc->flow[1] = bnc->flow[0];
                bnc->flow[0] = 0;
            }
        }
    }
}

void toku_ftnode_clone_callback(
    void* value_data,
    void** cloned_value_data,
    long* clone_size,
    PAIR_ATTR* new_attr,
    bool for_checkpoint,
    void* write_extraargs
    )
{
    FTNODE node = static_cast<FTNODE>(value_data);
    toku_ftnode_assert_fully_in_memory(node);
    FT ft = static_cast<FT>(write_extraargs);
    FTNODE XCALLOC(cloned_node);
    if (node->height == 0) {
        // set header stats, must be done before rebalancing
        toku_ftnode_update_disk_stats(node, ft, for_checkpoint);
        // rebalance the leaf node
        toku_ftnode_leaf_rebalance(node, ft->h->basementnodesize);
    }

    cloned_node->oldest_referenced_xid_known = node->oldest_referenced_xid_known;
    cloned_node->max_msn_applied_to_node_on_disk = node->max_msn_applied_to_node_on_disk;
    cloned_node->flags = node->flags;
    cloned_node->blocknum = node->blocknum;
    cloned_node->layout_version = node->layout_version;
    cloned_node->layout_version_original = node->layout_version_original;
    cloned_node->layout_version_read_from_disk = node->layout_version_read_from_disk;
    cloned_node->build_id = node->build_id;
    cloned_node->height = node->height;
    cloned_node->dirty = node->dirty;
    cloned_node->fullhash = node->fullhash;
    cloned_node->n_children = node->n_children;

    XMALLOC_N(node->n_children, cloned_node->bp);
    // clone pivots
    cloned_node->pivotkeys.create_from_pivot_keys(node->pivotkeys);
    if (node->height > 0) {
        // need to move messages here so that we don't serialize stale
        // messages to the fresh tree - ft verify code complains otherwise.
        toku_move_ftnode_messages_to_stale(ft, node);
    }
    // clone partition
    toku_ftnode_clone_partitions(node, cloned_node);

    // clear dirty bit
    node->dirty = 0;
    cloned_node->dirty = 0;
    node->layout_version_read_from_disk = FT_LAYOUT_VERSION;
    // set new pair attr if necessary
    if (node->height == 0) {
        *new_attr = make_ftnode_pair_attr(node);
    }
    else {
        new_attr->is_valid = false;
    }
    *clone_size = ftnode_memory_size(cloned_node);
    *cloned_value_data = cloned_node;
}

void toku_ftnode_flush_callback(
    CACHEFILE UU(cachefile),
    int fd,
    BLOCKNUM blocknum,
    void *ftnode_v,
    void** disk_data,
    void *extraargs,
    PAIR_ATTR size __attribute__((unused)),
    PAIR_ATTR* new_size,
    bool write_me,
    bool keep_me,
    bool for_checkpoint,
    bool is_clone
    )
{
    FT ft = (FT) extraargs;
    FTNODE ftnode = (FTNODE) ftnode_v;
    FTNODE_DISK_DATA* ndd = (FTNODE_DISK_DATA*)disk_data;
    assert(ftnode->blocknum.b == blocknum.b);
    int height = ftnode->height;
    if (write_me) {
        toku_ftnode_assert_fully_in_memory(ftnode);
        if (height > 0 && !is_clone) {
            // cloned nodes already had their stale messages moved, see toku_ftnode_clone_callback()
            toku_move_ftnode_messages_to_stale(ft, ftnode);
        } else if (height == 0) {
            toku_ftnode_leaf_run_gc(ft, ftnode);
            if (!is_clone) {
                toku_ftnode_update_disk_stats(ftnode, ft, for_checkpoint);
            }
        }
        int r = toku_serialize_ftnode_to(fd, ftnode->blocknum, ftnode, ndd, !is_clone, ft, for_checkpoint);
        assert_zero(r);
        ftnode->layout_version_read_from_disk = FT_LAYOUT_VERSION;
    }
    if (!keep_me) {
        if (!is_clone) {
            long node_size = ftnode_memory_size(ftnode);
            if (ftnode->height == 0) {
                STATUS_INC(FT_FULL_EVICTIONS_LEAF, 1);
                STATUS_INC(FT_FULL_EVICTIONS_LEAF_BYTES, node_size);
            } else {
                STATUS_INC(FT_FULL_EVICTIONS_NONLEAF, 1);
                STATUS_INC(FT_FULL_EVICTIONS_NONLEAF_BYTES, node_size);
            }
            toku_free(*disk_data);
        }
        else {
            if (ftnode->height == 0) {
                for (int i = 0; i < ftnode->n_children; i++) {
                    if (BP_STATE(ftnode,i) == PT_AVAIL) {
                        BASEMENTNODE bn = BLB(ftnode, i);
                        toku_ft_decrease_stats(&ft->in_memory_stats, bn->stat64_delta);
                    }
                }
            }
        }
        toku_ftnode_free(&ftnode);
    }
    else {
        *new_size = make_ftnode_pair_attr(ftnode);
    }
}

void
toku_ft_status_update_pivot_fetch_reason(ftnode_fetch_extra *bfe)
{
    if (bfe->type == ftnode_fetch_prefetch) {
        STATUS_INC(FT_NUM_PIVOTS_FETCHED_PREFETCH, 1);
        STATUS_INC(FT_BYTES_PIVOTS_FETCHED_PREFETCH, bfe->bytes_read);
        STATUS_INC(FT_TOKUTIME_PIVOTS_FETCHED_PREFETCH, bfe->io_time);
    } else if (bfe->type == ftnode_fetch_all) {
        STATUS_INC(FT_NUM_PIVOTS_FETCHED_WRITE, 1);
        STATUS_INC(FT_BYTES_PIVOTS_FETCHED_WRITE, bfe->bytes_read);
        STATUS_INC(FT_TOKUTIME_PIVOTS_FETCHED_WRITE, bfe->io_time);
    } else if (bfe->type == ftnode_fetch_subset || bfe->type == ftnode_fetch_keymatch) {
        STATUS_INC(FT_NUM_PIVOTS_FETCHED_QUERY, 1);
        STATUS_INC(FT_BYTES_PIVOTS_FETCHED_QUERY, bfe->bytes_read);
        STATUS_INC(FT_TOKUTIME_PIVOTS_FETCHED_QUERY, bfe->io_time);
    }
}

int toku_ftnode_fetch_callback (CACHEFILE UU(cachefile), PAIR p, int fd, BLOCKNUM blocknum, uint32_t fullhash,
                                 void **ftnode_pv,  void** disk_data, PAIR_ATTR *sizep, int *dirtyp, void *extraargs) {
    assert(extraargs);
    assert(*ftnode_pv == NULL);
    FTNODE_DISK_DATA* ndd = (FTNODE_DISK_DATA*)disk_data;
    ftnode_fetch_extra *bfe = (ftnode_fetch_extra *)extraargs;
    FTNODE *node=(FTNODE*)ftnode_pv;
    // deserialize the node, must pass the bfe in because we cannot
    // evaluate what piece of the the node is necessary until we get it at
    // least partially into memory
    int r = toku_deserialize_ftnode_from(fd, blocknum, fullhash, node, ndd, bfe);
    if (r != 0) {
        if (r == TOKUDB_BAD_CHECKSUM) {
            fprintf(stderr,
                    "Checksum failure while reading node in file %s.\n",
                    toku_cachefile_fname_in_env(cachefile));
        } else {
            fprintf(stderr, "Error deserializing node, errno = %d", r);
        }
        // make absolutely sure we crash before doing anything else.
        abort();
    }

    if (r == 0) {
        *sizep = make_ftnode_pair_attr(*node);
        (*node)->ct_pair = p;
        *dirtyp = (*node)->dirty;  // deserialize could mark the node as dirty (presumably for upgrade)
    }
    return r;
}

static bool ft_compress_buffers_before_eviction = true;

void toku_ft_set_compress_buffers_before_eviction(bool compress_buffers) {
    ft_compress_buffers_before_eviction = compress_buffers;
}

void toku_ftnode_pe_est_callback(
    void* ftnode_pv,
    void* disk_data,
    long* bytes_freed_estimate,
    enum partial_eviction_cost *cost,
    void* UU(write_extraargs)
    )
{
    paranoid_invariant(ftnode_pv != NULL);
    long bytes_to_free = 0;
    FTNODE node = static_cast<FTNODE>(ftnode_pv);
    if (node->dirty || node->height == 0 ||
        node->layout_version_read_from_disk < FT_FIRST_LAYOUT_VERSION_WITH_BASEMENT_NODES) {
        *bytes_freed_estimate = 0;
        *cost = PE_CHEAP;
        goto exit;
    }

    //
    // we are dealing with a clean internal node
    //
    *cost = PE_EXPENSIVE;
    // now lets get an estimate for how much data we can free up
    // we estimate the compressed size of data to be how large
    // the compressed data is on disk
    for (int i = 0; i < node->n_children; i++) {
        if (BP_STATE(node,i) == PT_AVAIL && BP_SHOULD_EVICT(node,i)) {
            // calculate how much data would be freed if
            // we compress this node and add it to
            // bytes_to_free

            if (ft_compress_buffers_before_eviction) {
                // first get an estimate for how much space will be taken
                // after compression, it is simply the size of compressed
                // data on disk plus the size of the struct that holds it
                FTNODE_DISK_DATA ndd = (FTNODE_DISK_DATA) disk_data;
                uint32_t compressed_data_size = BP_SIZE(ndd, i);
                compressed_data_size += sizeof(struct sub_block);

                // now get the space taken now
                uint32_t decompressed_data_size = get_avail_internal_node_partition_size(node,i);
                bytes_to_free += (decompressed_data_size - compressed_data_size);
            } else {
                bytes_to_free += get_avail_internal_node_partition_size(node, i);
            }
        }
    }

    *bytes_freed_estimate = bytes_to_free;
exit:
    return;
}

// replace the child buffer with a compressed version of itself.
static void compress_internal_node_partition(FTNODE node, int i, enum toku_compression_method compression_method) {
    // if we should evict, compress the
    // message buffer into a sub_block
    assert(BP_STATE(node, i) == PT_AVAIL);
    assert(node->height > 0);
    SUB_BLOCK XMALLOC(sb);
    sub_block_init(sb);
    toku_create_compressed_partition_from_available(node, i, compression_method, sb);

    // now set the state to compressed
    set_BSB(node, i, sb);
    BP_STATE(node,i) = PT_COMPRESSED;
}

// callback for partially evicting a node
int toku_ftnode_pe_callback(void *ftnode_pv, PAIR_ATTR old_attr, void *write_extraargs,
                            void (*finalize)(PAIR_ATTR new_attr, void *extra), void *finalize_extra) {
    FTNODE node = (FTNODE) ftnode_pv;
    FT ft = (FT) write_extraargs;
    int num_partial_evictions = 0;

    // Hold things we intend to destroy here.
    // They will be taken care of after finalize().
    int num_basements_to_destroy = 0;
    int num_buffers_to_destroy = 0;
    int num_pointers_to_free = 0;
    BASEMENTNODE basements_to_destroy[node->n_children];
    NONLEAF_CHILDINFO buffers_to_destroy[node->n_children];
    void *pointers_to_free[node->n_children * 2];

    // Don't partially evict dirty nodes
    if (node->dirty) {
        goto exit;
    }
    // Don't partially evict nodes whose partitions can't be read back
    // from disk individually
    if (node->layout_version_read_from_disk < FT_FIRST_LAYOUT_VERSION_WITH_BASEMENT_NODES) {
        goto exit;
    }
    //
    // partial eviction for nonleaf nodes
    //
    if (node->height > 0) {
        for (int i = 0; i < node->n_children; i++) {
            if (BP_STATE(node,i) == PT_AVAIL) {
                if (BP_SHOULD_EVICT(node,i)) {
                    NONLEAF_CHILDINFO bnc = BNC(node, i);
                    if (ft_compress_buffers_before_eviction &&
                        // We may not serialize and compress a partition in memory if its
                        // in memory layout version is different than what's on disk (and
                        // therefore requires upgrade).
                        //
                        // Auto-upgrade code assumes that if a node's layout version read
                        // from disk is not current, it MUST require upgrade. Breaking
                        // this rule would cause upgrade code to upgrade this partition
                        // again after we serialize it as the current version, which is bad.
                        node->layout_version == node->layout_version_read_from_disk) {
                        toku_ft_bnc_move_messages_to_stale(ft, bnc);
                        compress_internal_node_partition(
                            node,
                            i,
                            // Always compress with quicklz
                            TOKU_QUICKLZ_METHOD
                            );
                    } else {
                        // We're not compressing buffers before eviction. Simply
                        // detach the buffer and set the child's state to on-disk.
                        set_BNULL(node, i);
                        BP_STATE(node, i) = PT_ON_DISK;
                    }
                    buffers_to_destroy[num_buffers_to_destroy++] = bnc;
                    num_partial_evictions++;
                }
                else {
                    BP_SWEEP_CLOCK(node,i);
                }
            }
            else {
                continue;
            }
        }
    }
    //
    // partial eviction strategy for basement nodes:
    //  if the bn is compressed, evict it
    //  else: check if it requires eviction, if it does, evict it, if not, sweep the clock count
    //
    else {
        for (int i = 0; i < node->n_children; i++) {
            // Get rid of compressed stuff no matter what.
            if (BP_STATE(node,i) == PT_COMPRESSED) {
                SUB_BLOCK sb = BSB(node, i);
                pointers_to_free[num_pointers_to_free++] = sb->compressed_ptr;
                pointers_to_free[num_pointers_to_free++] = sb;
                set_BNULL(node, i);
                BP_STATE(node,i) = PT_ON_DISK;
                num_partial_evictions++;
            }
            else if (BP_STATE(node,i) == PT_AVAIL) {
                if (BP_SHOULD_EVICT(node,i)) {
                    BASEMENTNODE bn = BLB(node, i);
                    basements_to_destroy[num_basements_to_destroy++] = bn;
                    toku_ft_decrease_stats(&ft->in_memory_stats, bn->stat64_delta);
                    set_BNULL(node, i);
                    BP_STATE(node, i) = PT_ON_DISK;
                    num_partial_evictions++;
                }
                else {
                    BP_SWEEP_CLOCK(node,i);
                }
            }
            else if (BP_STATE(node,i) == PT_ON_DISK) {
                continue;
            }
            else {
                abort();
            }
        }
    }

exit:
    // call the finalize callback with a new pair attr
    int height = node->height;
    PAIR_ATTR new_attr = make_ftnode_pair_attr(node);
    finalize(new_attr, finalize_extra);

    // destroy everything now that we've called finalize(),
    // and, by contract, and it's safe to do expensive work.
    for (int i = 0; i < num_basements_to_destroy; i++) {
        destroy_basement_node(basements_to_destroy[i]);
    }
    for (int i = 0; i < num_buffers_to_destroy; i++) {
        destroy_nonleaf_childinfo(buffers_to_destroy[i]);
    }
    for (int i = 0; i < num_pointers_to_free; i++) {
        toku_free(pointers_to_free[i]);
    }
    // stats
    if (num_partial_evictions > 0) {
        if (height == 0) {
            long delta = old_attr.leaf_size - new_attr.leaf_size;
            STATUS_INC(FT_PARTIAL_EVICTIONS_LEAF, num_partial_evictions);
            STATUS_INC(FT_PARTIAL_EVICTIONS_LEAF_BYTES, delta);
        } else {
            long delta = old_attr.nonleaf_size - new_attr.nonleaf_size;
            STATUS_INC(FT_PARTIAL_EVICTIONS_NONLEAF, num_partial_evictions);
            STATUS_INC(FT_PARTIAL_EVICTIONS_NONLEAF_BYTES, delta);
        }
    }
    return 0;
}

// We touch the clock while holding a read lock.
// DRD reports a race but we want to ignore it.
// Using a valgrind suppressions file is better than the DRD_IGNORE_VAR macro because it's more targeted.
// We need a function to have something a drd suppression can reference
// see src/tests/drd.suppressions (unsafe_touch_clock)
static void unsafe_touch_clock(FTNODE node, int i) {
    toku_drd_unsafe_set(&node->bp[i].clock_count, static_cast<unsigned char>(1));
}

// Callback that states if a partial fetch of the node is necessary
// Currently, this function is responsible for the following things:
//  - reporting to the cachetable whether a partial fetch is required (as required by the contract of the callback)
//  - A couple of things that are NOT required by the callback, but we do for efficiency and simplicity reasons:
//   - for queries, set the value of bfe->child_to_read so that the query that called this can proceed with the query
//      as opposed to having to evaluate toku_ft_search_which_child again. This is done to make the in-memory query faster
//   - touch the necessary partition's clock. The reason we do it here is so that there is one central place it is done, and not done
//      by all the various callers
//
bool toku_ftnode_pf_req_callback(void* ftnode_pv, void* read_extraargs) {
    // placeholder for now
    bool retval = false;
    FTNODE node = (FTNODE) ftnode_pv;
    ftnode_fetch_extra *bfe = (ftnode_fetch_extra *) read_extraargs;
    //
    // The three types of fetches that the ft layer may request are:
    //  - ftnode_fetch_none: no partitions are necessary (example use: stat64)
    //  - ftnode_fetch_subset: some subset is necessary (example use: toku_ft_search)
    //  - ftnode_fetch_all: entire node is necessary (example use: flush, split, merge)
    // The code below checks if the necessary partitions are already in memory,
    // and if they are, return false, and if not, return true
    //
    if (bfe->type == ftnode_fetch_none) {
        retval = false;
    }
    else if (bfe->type == ftnode_fetch_all) {
        retval = false;
        for (int i = 0; i < node->n_children; i++) {
            unsafe_touch_clock(node,i);
            // if we find a partition that is not available,
            // then a partial fetch is required because
            // the entire node must be made available
            if (BP_STATE(node,i) != PT_AVAIL) {
                retval = true;
            }
        }
    }
    else if (bfe->type == ftnode_fetch_subset) {
        // we do not take into account prefetching yet
        // as of now, if we need a subset, the only thing
        // we can possibly require is a single basement node
        // we find out what basement node the query cares about
        // and check if it is available
        paranoid_invariant(bfe->search);
        bfe->child_to_read = toku_ft_search_which_child(
            bfe->ft->cmp,
            node,
            bfe->search
            );
        unsafe_touch_clock(node,bfe->child_to_read);
        // child we want to read is not available, must set retval to true
        retval = (BP_STATE(node, bfe->child_to_read) != PT_AVAIL);
    }
    else if (bfe->type == ftnode_fetch_prefetch) {
        // makes no sense to have prefetching disabled
        // and still call this function
        paranoid_invariant(!bfe->disable_prefetching);
        int lc = bfe->leftmost_child_wanted(node);
        int rc = bfe->rightmost_child_wanted(node);
        for (int i = lc; i <= rc; ++i) {
            if (BP_STATE(node, i) != PT_AVAIL) {
                retval = true;
            }
        }
    } else if (bfe->type == ftnode_fetch_keymatch) {
        // we do not take into account prefetching yet
        // as of now, if we need a subset, the only thing
        // we can possibly require is a single basement node
        // we find out what basement node the query cares about
        // and check if it is available
        if (node->height == 0) {
            int left_child = bfe->leftmost_child_wanted(node);
            int right_child = bfe->rightmost_child_wanted(node);
            if (left_child == right_child) {
                bfe->child_to_read = left_child;
                unsafe_touch_clock(node,bfe->child_to_read);
                // child we want to read is not available, must set retval to true
                retval = (BP_STATE(node, bfe->child_to_read) != PT_AVAIL);
            }
        }
    } else {
        // we have a bug. The type should be known
        abort();
    }
    return retval;
}

static void
ft_status_update_partial_fetch_reason(
    ftnode_fetch_extra *bfe,
    int childnum,
    enum pt_state state,
    bool is_leaf
    )
{
    invariant(state == PT_COMPRESSED || state == PT_ON_DISK);
    if (is_leaf) {
        if (bfe->type == ftnode_fetch_prefetch) {
            if (state == PT_COMPRESSED) {
                STATUS_INC(FT_NUM_BASEMENTS_DECOMPRESSED_PREFETCH, 1);
            } else {
                STATUS_INC(FT_NUM_BASEMENTS_FETCHED_PREFETCH, 1);
                STATUS_INC(FT_BYTES_BASEMENTS_FETCHED_PREFETCH, bfe->bytes_read);
                STATUS_INC(FT_TOKUTIME_BASEMENTS_FETCHED_PREFETCH, bfe->io_time);
            }
        } else if (bfe->type == ftnode_fetch_all) {
            if (state == PT_COMPRESSED) {
                STATUS_INC(FT_NUM_BASEMENTS_DECOMPRESSED_WRITE, 1);
            } else {
                STATUS_INC(FT_NUM_BASEMENTS_FETCHED_WRITE, 1);
                STATUS_INC(FT_BYTES_BASEMENTS_FETCHED_WRITE, bfe->bytes_read);
                STATUS_INC(FT_TOKUTIME_BASEMENTS_FETCHED_WRITE, bfe->io_time);
            }
        } else if (childnum == bfe->child_to_read) {
            if (state == PT_COMPRESSED) {
                STATUS_INC(FT_NUM_BASEMENTS_DECOMPRESSED_NORMAL, 1);
            } else {
                STATUS_INC(FT_NUM_BASEMENTS_FETCHED_NORMAL, 1);
                STATUS_INC(FT_BYTES_BASEMENTS_FETCHED_NORMAL, bfe->bytes_read);
                STATUS_INC(FT_TOKUTIME_BASEMENTS_FETCHED_NORMAL, bfe->io_time);
            }
        } else {
            if (state == PT_COMPRESSED) {
                STATUS_INC(FT_NUM_BASEMENTS_DECOMPRESSED_AGGRESSIVE, 1);
            } else {
                STATUS_INC(FT_NUM_BASEMENTS_FETCHED_AGGRESSIVE, 1);
                STATUS_INC(FT_BYTES_BASEMENTS_FETCHED_AGGRESSIVE, bfe->bytes_read);
                STATUS_INC(FT_TOKUTIME_BASEMENTS_FETCHED_AGGRESSIVE, bfe->io_time);
            }
        }
    }
    else {
        if (bfe->type == ftnode_fetch_prefetch) {
            if (state == PT_COMPRESSED) {
                STATUS_INC(FT_NUM_MSG_BUFFER_DECOMPRESSED_PREFETCH, 1);
            } else {
                STATUS_INC(FT_NUM_MSG_BUFFER_FETCHED_PREFETCH, 1);
                STATUS_INC(FT_BYTES_MSG_BUFFER_FETCHED_PREFETCH, bfe->bytes_read);
                STATUS_INC(FT_TOKUTIME_MSG_BUFFER_FETCHED_PREFETCH, bfe->io_time);
            }
        } else if (bfe->type == ftnode_fetch_all) {
            if (state == PT_COMPRESSED) {
                STATUS_INC(FT_NUM_MSG_BUFFER_DECOMPRESSED_WRITE, 1);
            } else {
                STATUS_INC(FT_NUM_MSG_BUFFER_FETCHED_WRITE, 1);
                STATUS_INC(FT_BYTES_MSG_BUFFER_FETCHED_WRITE, bfe->bytes_read);
                STATUS_INC(FT_TOKUTIME_MSG_BUFFER_FETCHED_WRITE, bfe->io_time);
            }
        } else if (childnum == bfe->child_to_read) {
            if (state == PT_COMPRESSED) {
                STATUS_INC(FT_NUM_MSG_BUFFER_DECOMPRESSED_NORMAL, 1);
            } else {
                STATUS_INC(FT_NUM_MSG_BUFFER_FETCHED_NORMAL, 1);
                STATUS_INC(FT_BYTES_MSG_BUFFER_FETCHED_NORMAL, bfe->bytes_read);
                STATUS_INC(FT_TOKUTIME_MSG_BUFFER_FETCHED_NORMAL, bfe->io_time);
            }
        } else {
            if (state == PT_COMPRESSED) {
                STATUS_INC(FT_NUM_MSG_BUFFER_DECOMPRESSED_AGGRESSIVE, 1);
            } else {
                STATUS_INC(FT_NUM_MSG_BUFFER_FETCHED_AGGRESSIVE, 1);
                STATUS_INC(FT_BYTES_MSG_BUFFER_FETCHED_AGGRESSIVE, bfe->bytes_read);
                STATUS_INC(FT_TOKUTIME_MSG_BUFFER_FETCHED_AGGRESSIVE, bfe->io_time);
            }
        }
    }
}

void toku_ft_status_update_serialize_times(FTNODE node, tokutime_t serialize_time, tokutime_t compress_time) {
    if (node->height == 0) {
        STATUS_INC(FT_LEAF_SERIALIZE_TOKUTIME, serialize_time);
        STATUS_INC(FT_LEAF_COMPRESS_TOKUTIME, compress_time);
    } else {
        STATUS_INC(FT_NONLEAF_SERIALIZE_TOKUTIME, serialize_time);
        STATUS_INC(FT_NONLEAF_COMPRESS_TOKUTIME, compress_time);
    }
}

void toku_ft_status_update_deserialize_times(FTNODE node, tokutime_t deserialize_time, tokutime_t decompress_time) {
    if (node->height == 0) {
        STATUS_INC(FT_LEAF_DESERIALIZE_TOKUTIME, deserialize_time);
        STATUS_INC(FT_LEAF_DECOMPRESS_TOKUTIME, decompress_time);
    } else {
        STATUS_INC(FT_NONLEAF_DESERIALIZE_TOKUTIME, deserialize_time);
        STATUS_INC(FT_NONLEAF_DECOMPRESS_TOKUTIME, decompress_time);
    }
}

void toku_ft_status_note_msn_discard(void) {
    STATUS_INC(FT_MSN_DISCARDS, 1);
}

void toku_ft_status_note_update(bool broadcast) {
    if (broadcast) {
        STATUS_INC(FT_UPDATES_BROADCAST, 1);
    } else {
        STATUS_INC(FT_UPDATES, 1);
    }
}

void toku_ft_status_note_msg_bytes_out(size_t buffsize) {
    STATUS_INC(FT_MSG_BYTES_OUT, buffsize);
    STATUS_INC(FT_MSG_BYTES_CURR, -buffsize);
}
void toku_ft_status_note_ftnode(int height, bool created) {
    if (created) {
        if (height == 0) {
            STATUS_INC(FT_CREATE_LEAF, 1);
        } else {
            STATUS_INC(FT_CREATE_NONLEAF, 1);
        }
    } else {
        // created = false means destroyed
    }
}

// callback for partially reading a node
// could have just used toku_ftnode_fetch_callback, but wanted to separate the two cases to separate functions
int toku_ftnode_pf_callback(void* ftnode_pv, void* disk_data, void* read_extraargs, int fd, PAIR_ATTR* sizep) {
    int r = 0;
    FTNODE node = (FTNODE) ftnode_pv;
    FTNODE_DISK_DATA ndd = (FTNODE_DISK_DATA) disk_data;
    ftnode_fetch_extra *bfe = (ftnode_fetch_extra *) read_extraargs;
    // there must be a reason this is being called. If we get a garbage type or the type is ftnode_fetch_none,
    // then something went wrong
    assert((bfe->type == ftnode_fetch_subset) || (bfe->type == ftnode_fetch_all) || (bfe->type == ftnode_fetch_prefetch) || (bfe->type == ftnode_fetch_keymatch));
    // determine the range to prefetch
    int lc, rc;
    if (!bfe->disable_prefetching &&
        (bfe->type == ftnode_fetch_subset || bfe->type == ftnode_fetch_prefetch)
        )
    {
        lc = bfe->leftmost_child_wanted(node);
        rc = bfe->rightmost_child_wanted(node);
    } else {
        lc = -1;
        rc = -1;
    }
    for (int i = 0; i < node->n_children; i++) {
        if (BP_STATE(node,i) == PT_AVAIL) {
            continue;
        }
        if ((lc <= i && i <= rc) || bfe->wants_child_available(i)) {
            enum pt_state state = BP_STATE(node, i);
            if (state == PT_COMPRESSED) {
                r = toku_deserialize_bp_from_compressed(node, i, bfe);
            } else {
                invariant(state == PT_ON_DISK);
                r = toku_deserialize_bp_from_disk(node, ndd, i, fd, bfe);
            }
            ft_status_update_partial_fetch_reason(bfe, i, state, (node->height == 0));
        }

        if (r != 0) {
            if (r == TOKUDB_BAD_CHECKSUM) {
                fprintf(stderr,
                        "Checksum failure while reading node partition in file %s.\n",
                        toku_cachefile_fname_in_env(bfe->ft->cf));
            } else {
                fprintf(stderr,
                        "Error while reading node partition %d\n",
                        get_maybe_error_errno());
            }
            abort();
        }
    }

    *sizep = make_ftnode_pair_attr(node);

    return 0;
}

int toku_msg_leafval_heaviside(DBT const &kdbt, const struct toku_msg_leafval_heaviside_extra &be) {
    return be.cmp(&kdbt, be.key);
}

static void
ft_init_new_root(FT ft, FTNODE oldroot, FTNODE *newrootp)
// Effect:  Create a new root node whose two children are the split of oldroot.
//  oldroot is unpinned in the process.
//  Leave the new root pinned.
{
    FTNODE newroot;

    BLOCKNUM old_blocknum = oldroot->blocknum;
    uint32_t old_fullhash = oldroot->fullhash;
    
    int new_height = oldroot->height+1;
    uint32_t new_fullhash;
    BLOCKNUM new_blocknum;

    cachetable_put_empty_node_with_dep_nodes(
        ft,
        1,
        &oldroot,
        &new_blocknum,
        &new_fullhash,
        &newroot
        );
    
    assert(newroot);
    assert(new_height > 0);
    toku_initialize_empty_ftnode (
        newroot, 
        new_blocknum, 
        new_height, 
        1, 
        ft->h->layout_version, 
        ft->h->flags
        );
    newroot->fullhash = new_fullhash;
    MSN msna = oldroot->max_msn_applied_to_node_on_disk;
    newroot->max_msn_applied_to_node_on_disk = msna;
    BP_STATE(newroot,0) = PT_AVAIL;
    newroot->dirty = 1;

    // Set the first child to have the new blocknum,
    // and then swap newroot with oldroot. The new root
    // will inherit the hash/blocknum/pair from oldroot,
    // keeping the root blocknum constant.
    BP_BLOCKNUM(newroot, 0) = new_blocknum;
    toku_ftnode_swap_pair_values(newroot, oldroot);

    toku_ft_split_child(
        ft,
        newroot,
        0, // childnum to split
        oldroot,
        SPLIT_EVENLY
        );

    // ft_split_child released locks on newroot
    // and oldroot, so now we repin and
    // return to caller
    ftnode_fetch_extra bfe;
    bfe.create_for_full_read(ft);
    toku_pin_ftnode(
        ft,
        old_blocknum,
        old_fullhash,
        &bfe,
        PL_WRITE_EXPENSIVE, // may_modify_node
        newrootp,
        true
        );
}

static void inject_message_in_locked_node(
    FT ft, 
    FTNODE node, 
    int childnum, 
    const ft_msg &msg, 
    size_t flow_deltas[],
    txn_gc_info *gc_info
    ) 
{
    // No guarantee that we're the writer, but oh well.
    // TODO(leif): Implement "do I have the lock or is it someone else?"
    // check in frwlock.  Should be possible with TOKU_PTHREAD_DEBUG, nop
    // otherwise.
    invariant(toku_ctpair_is_write_locked(node->ct_pair));
    toku_ftnode_assert_fully_in_memory(node);

    // Take the newer of the two oldest referenced xid values from the node and gc_info.
    // The gc_info usually has a newer value, because we got it at the top of this call
    // stack from the txn manager. But sometimes the node has a newer value, if some
    // other thread sees a newer value and writes to this node before we got the lock.
    if (gc_info->oldest_referenced_xid_for_implicit_promotion > node->oldest_referenced_xid_known) {
        node->oldest_referenced_xid_known = gc_info->oldest_referenced_xid_for_implicit_promotion;
    } else if (gc_info->oldest_referenced_xid_for_implicit_promotion < node->oldest_referenced_xid_known) {
        gc_info->oldest_referenced_xid_for_implicit_promotion = node->oldest_referenced_xid_known;
    }

    // Get the MSN from the header.  Now that we have a write lock on the
    // node we're injecting into, we know no other thread will get an MSN
    // after us and get that message into our subtree before us.
    MSN msg_msn = { .msn = toku_sync_add_and_fetch(&ft->h->max_msn_in_ft.msn, 1) };
    ft_msg msg_with_msn(msg.kdbt(), msg.vdbt(), msg.type(), msg_msn, msg.xids());
    paranoid_invariant(msg_with_msn.msn().msn > node->max_msn_applied_to_node_on_disk.msn);

    STAT64INFO_S stats_delta = {0,0};
    toku_ftnode_put_msg(
        ft->cmp,
        ft->update_fun,
        node,
        childnum,
        msg_with_msn,
        true,
        gc_info,
        flow_deltas,
        &stats_delta
        );
    if (stats_delta.numbytes || stats_delta.numrows) {
        toku_ft_update_stats(&ft->in_memory_stats, stats_delta);
    }
    //
    // assumption is that toku_ftnode_put_msg will
    // mark the node as dirty.
    // enforcing invariant here.
    //
    paranoid_invariant(node->dirty != 0);

    // update some status variables
    if (node->height != 0) {
        size_t msgsize = msg.total_size();
        STATUS_INC(FT_MSG_BYTES_IN, msgsize);
        STATUS_INC(FT_MSG_BYTES_CURR, msgsize);
        STATUS_INC(FT_MSG_NUM, 1);
        if (ft_msg_type_applies_all(msg.type())) {
            STATUS_INC(FT_MSG_NUM_BROADCAST, 1);
        }
    }

    // verify that msn of latest message was captured in root node
    paranoid_invariant(msg_with_msn.msn().msn == node->max_msn_applied_to_node_on_disk.msn);

    if (node->blocknum.b == ft->rightmost_blocknum.b) {
        if (toku_drd_unsafe_fetch(&ft->seqinsert_score) < FT_SEQINSERT_SCORE_THRESHOLD) {
            // we promoted to the rightmost leaf node and the seqinsert score has not yet saturated.
            toku_sync_fetch_and_add(&ft->seqinsert_score, 1);
        }
    } else if (toku_drd_unsafe_fetch(&ft->seqinsert_score) != 0) {
        // we promoted to something other than the rightmost leaf node and the score should reset
        toku_drd_unsafe_set(&ft->seqinsert_score, static_cast<uint32_t>(0));
    }

    // if we call toku_ft_flush_some_child, then that function unpins the root
    // otherwise, we unpin ourselves
    if (node->height > 0 && toku_ftnode_nonleaf_is_gorged(node, ft->h->nodesize)) {
        toku_ft_flush_node_on_background_thread(ft, node);
    }
    else {
        toku_unpin_ftnode(ft, node);
    }
}

// seqinsert_loc is a bitmask.
// The root counts as being both on the "left extreme" and on the "right extreme".
// Therefore, at the root, you're at LEFT_EXTREME | RIGHT_EXTREME.
typedef char seqinsert_loc;
static const seqinsert_loc NEITHER_EXTREME = 0;
static const seqinsert_loc LEFT_EXTREME = 1;
static const seqinsert_loc RIGHT_EXTREME = 2;

static bool process_maybe_reactive_child(FT ft, FTNODE parent, FTNODE child, int childnum, seqinsert_loc loc)
// Effect:
//  If child needs to be split or merged, do that.
//  parent and child will be unlocked if this happens
// Requires: parent and child are read locked
// Returns:
//  true if relocking is needed
//  false otherwise
{
    enum reactivity re = toku_ftnode_get_reactivity(ft, child);
    enum reactivity newre;
    BLOCKNUM child_blocknum;
    uint32_t child_fullhash;
    switch (re) {
    case RE_STABLE:
        return false;
    case RE_FISSIBLE:
        {
            // We only have a read lock on the parent.  We need to drop both locks, and get write locks.
            BLOCKNUM parent_blocknum = parent->blocknum;
            uint32_t parent_fullhash = toku_cachetable_hash(ft->cf, parent_blocknum);
            int parent_height = parent->height;
            int parent_n_children = parent->n_children;
            toku_unpin_ftnode_read_only(ft, child);
            toku_unpin_ftnode_read_only(ft, parent);
            ftnode_fetch_extra bfe;
            bfe.create_for_full_read(ft);
            FTNODE newparent, newchild;
            toku_pin_ftnode(ft, parent_blocknum, parent_fullhash, &bfe, PL_WRITE_CHEAP, &newparent, true);
            if (newparent->height != parent_height || newparent->n_children != parent_n_children ||
                childnum >= newparent->n_children || toku_bnc_n_entries(BNC(newparent, childnum))) {
                // If the height changed or childnum is now off the end, something clearly got split or merged out from under us.
                // If something got injected in this node, then it got split or merged and we shouldn't be splitting it.
                // But we already unpinned the child so we need to have the caller re-try the pins.
                toku_unpin_ftnode_read_only(ft, newparent);
                return true;
            }
            // It's ok to reuse the same childnum because if we get something
            // else we need to split, well, that's crazy, but let's go ahead
            // and split it.
            child_blocknum = BP_BLOCKNUM(newparent, childnum);
            child_fullhash = compute_child_fullhash(ft->cf, newparent, childnum);
            toku_pin_ftnode_with_dep_nodes(ft, child_blocknum, child_fullhash, &bfe, PL_WRITE_CHEAP, 1, &newparent, &newchild, true);
            newre = toku_ftnode_get_reactivity(ft, newchild);
            if (newre == RE_FISSIBLE) {
                enum split_mode split_mode;
                if (newparent->height == 1 && (loc & LEFT_EXTREME) && childnum == 0) {
                    split_mode = SPLIT_RIGHT_HEAVY;
                } else if (newparent->height == 1 && (loc & RIGHT_EXTREME) && childnum == newparent->n_children - 1) {
                    split_mode = SPLIT_LEFT_HEAVY;
                } else {
                    split_mode = SPLIT_EVENLY;
                }
                toku_ft_split_child(ft, newparent, childnum, newchild, split_mode);
            } else {
                // some other thread already got it, just unpin and tell the
                // caller to retry
                toku_unpin_ftnode_read_only(ft, newchild);
                toku_unpin_ftnode_read_only(ft, newparent);
            }
            return true;
        }
    case RE_FUSIBLE:
        {
            if (parent->height == 1) {
                // prevent re-merging of recently unevenly-split nodes
                if (((loc & LEFT_EXTREME) && childnum <= 1) ||
                    ((loc & RIGHT_EXTREME) && childnum >= parent->n_children - 2)) {
                    return false;
                }
            }

            int parent_height = parent->height;
            BLOCKNUM parent_blocknum = parent->blocknum;
            uint32_t parent_fullhash = toku_cachetable_hash(ft->cf, parent_blocknum);
            toku_unpin_ftnode_read_only(ft, child);
            toku_unpin_ftnode_read_only(ft, parent);
            ftnode_fetch_extra bfe;
            bfe.create_for_full_read(ft);
            FTNODE newparent, newchild;
            toku_pin_ftnode(ft, parent_blocknum, parent_fullhash, &bfe, PL_WRITE_CHEAP, &newparent, true);
            if (newparent->height != parent_height || childnum >= newparent->n_children) {
                // looks like this is the root and it got merged, let's just start over (like in the split case above)
                toku_unpin_ftnode_read_only(ft, newparent);
                return true;
            }
            child_blocknum = BP_BLOCKNUM(newparent, childnum);
            child_fullhash = compute_child_fullhash(ft->cf, newparent, childnum);
            toku_pin_ftnode_with_dep_nodes(ft, child_blocknum, child_fullhash, &bfe, PL_READ, 1, &newparent, &newchild, true);
            newre = toku_ftnode_get_reactivity(ft, newchild);
            if (newre == RE_FUSIBLE && newparent->n_children >= 2) {
                toku_unpin_ftnode_read_only(ft, newchild);
                toku_ft_merge_child(ft, newparent, childnum);
            } else {
                // Could be a weird case where newparent has only one
                // child.  In this case, we want to inject here but we've
                // already unpinned the caller's copy of parent so we have
                // to ask them to re-pin, or they could (very rarely)
                // dereferenced memory in a freed node.  TODO: we could
                // give them back the copy of the parent we pinned.
                //
                // Otherwise, some other thread already got it, just unpin
                // and tell the caller to retry
                toku_unpin_ftnode_read_only(ft, newchild);
                toku_unpin_ftnode_read_only(ft, newparent);
            }
            return true;
        }
    }
    abort();
}

static void inject_message_at_this_blocknum(FT ft, CACHEKEY cachekey, uint32_t fullhash, const ft_msg &msg, size_t flow_deltas[], txn_gc_info *gc_info)
// Effect:
//  Inject message into the node at this blocknum (cachekey).
//  Gets a write lock on the node for you.
{
    toku::context inject_ctx(CTX_MESSAGE_INJECTION);
    FTNODE node;
    ftnode_fetch_extra bfe;
    bfe.create_for_full_read(ft);
    toku_pin_ftnode(ft, cachekey, fullhash, &bfe, PL_WRITE_CHEAP, &node, true);
    toku_ftnode_assert_fully_in_memory(node);
    paranoid_invariant(node->fullhash==fullhash);
    ft_verify_flags(ft, node);
    inject_message_in_locked_node(ft, node, -1, msg, flow_deltas, gc_info);
}

__attribute__((const))
static inline bool should_inject_in_node(seqinsert_loc loc, int height, int depth)
// We should inject directly in a node if:
//  - it's a leaf, or
//  - it's a height 1 node not at either extreme, or
//  - it's a depth 2 node not at either extreme
{
    return (height == 0 || (loc == NEITHER_EXTREME && (height <= 1 || depth >= 2)));
}

static void ft_verify_or_set_rightmost_blocknum(FT ft, BLOCKNUM b)
// Given: 'b', the _definitive_ and constant rightmost blocknum of 'ft'
{
    if (toku_drd_unsafe_fetch(&ft->rightmost_blocknum.b) == RESERVED_BLOCKNUM_NULL) {
        toku_ft_lock(ft);
        if (ft->rightmost_blocknum.b == RESERVED_BLOCKNUM_NULL) {
            toku_drd_unsafe_set(&ft->rightmost_blocknum, b);
        }
        toku_ft_unlock(ft);
    }
    // The rightmost blocknum only transitions from RESERVED_BLOCKNUM_NULL to non-null.
    // If it's already set, verify that the stored value is consistent with 'b'
    invariant(toku_drd_unsafe_fetch(&ft->rightmost_blocknum.b) == b.b);
}

bool toku_bnc_should_promote(FT ft, NONLEAF_CHILDINFO bnc) {
    static const double factor = 0.125;
    const uint64_t flow_threshold = ft->h->nodesize * factor;
    return bnc->flow[0] >= flow_threshold || bnc->flow[1] >= flow_threshold;
}

static void push_something_in_subtree(
    FT ft, 
    FTNODE subtree_root, 
    int target_childnum, 
    const ft_msg &msg, 
    size_t flow_deltas[], 
    txn_gc_info *gc_info,
    int depth, 
    seqinsert_loc loc, 
    bool just_did_split_or_merge
    )
// Effects:
//  Assign message an MSN from ft->h.
//  Put message in the subtree rooted at node.  Due to promotion the message may not be injected directly in this node.
//  Unlock node or schedule it to be unlocked (after a background flush).
//   Either way, the caller is not responsible for unlocking node.
// Requires:
//  subtree_root is read locked and fully in memory.
// Notes:
//  In Ming, the basic rules of promotion are as follows:
//   Don't promote broadcast messages.
//   Don't promote past non-empty buffers.
//   Otherwise, promote at most to height 1 or depth 2 (whichever is highest), as far as the birdie asks you to promote.
//    We don't promote to leaves because injecting into leaves is expensive, mostly because of #5605 and some of #5552.
//    We don't promote past depth 2 because we found that gives us enough parallelism without costing us too much pinning work.
//
//    This is true with the following caveats:
//     We always promote all the way to the leaves on the rightmost and leftmost edges of the tree, for sequential insertions.
//      (That means we can promote past depth 2 near the edges of the tree.)
//
//   When the birdie is still saying we should promote, we use get_and_pin so that we wait to get the node.
//   If the birdie doesn't say to promote, we try maybe_get_and_pin.  If we get the node cheaply, and it's dirty, we promote anyway.
{
    toku_ftnode_assert_fully_in_memory(subtree_root);
    if (should_inject_in_node(loc, subtree_root->height, depth)) {
        switch (depth) {
        case 0:
            STATUS_INC(FT_PRO_NUM_INJECT_DEPTH_0, 1); break;
        case 1:
            STATUS_INC(FT_PRO_NUM_INJECT_DEPTH_1, 1); break;
        case 2:
            STATUS_INC(FT_PRO_NUM_INJECT_DEPTH_2, 1); break;
        case 3:
            STATUS_INC(FT_PRO_NUM_INJECT_DEPTH_3, 1); break;
        default:
            STATUS_INC(FT_PRO_NUM_INJECT_DEPTH_GT3, 1); break;
        }
        // If the target node is a non-root leaf node on the right extreme,
        // set the rightmost blocknum. We know there are no messages above us
        // because promotion would not chose to inject directly into this leaf
        // otherwise. We explicitly skip the root node because then we don't have
        // to worry about changing the rightmost blocknum when the root splits.
        if (subtree_root->height == 0 && loc == RIGHT_EXTREME && subtree_root->blocknum.b != ft->h->root_blocknum.b) {
            ft_verify_or_set_rightmost_blocknum(ft, subtree_root->blocknum);
        }
        inject_message_in_locked_node(ft, subtree_root, target_childnum, msg, flow_deltas, gc_info);
    } else {
        int r;
        int childnum;
        NONLEAF_CHILDINFO bnc;

        // toku_ft_root_put_msg should not have called us otherwise.
        paranoid_invariant(ft_msg_type_applies_once(msg.type()));

        childnum = (target_childnum >= 0 ? target_childnum
                    : toku_ftnode_which_child(subtree_root, msg.kdbt(), ft->cmp));
        bnc = BNC(subtree_root, childnum);

        if (toku_bnc_n_entries(bnc) > 0) {
            // The buffer is non-empty, give up on promoting.
            STATUS_INC(FT_PRO_NUM_STOP_NONEMPTY_BUF, 1);
            goto relock_and_push_here;
        }

        seqinsert_loc next_loc;
        if ((loc & LEFT_EXTREME) && childnum == 0) {
            next_loc = LEFT_EXTREME;
        } else if ((loc & RIGHT_EXTREME) && childnum == subtree_root->n_children - 1) {
            next_loc = RIGHT_EXTREME;
        } else {
            next_loc = NEITHER_EXTREME;
        }

        if (next_loc == NEITHER_EXTREME && subtree_root->height <= 1) {
            // Never promote to leaf nodes except on the edges
            STATUS_INC(FT_PRO_NUM_STOP_H1, 1);
            goto relock_and_push_here;
        }

        {
            const BLOCKNUM child_blocknum = BP_BLOCKNUM(subtree_root, childnum);
            ft->blocktable.verify_blocknum_allocated(child_blocknum);
            const uint32_t child_fullhash = toku_cachetable_hash(ft->cf, child_blocknum);

            FTNODE child;
            {
                const int child_height = subtree_root->height - 1;
                const int child_depth = depth + 1;
                // If we're locking a leaf, or a height 1 node or depth 2
                // node in the middle, we know we won't promote further
                // than that, so just get a write lock now.
                const pair_lock_type lock_type = (should_inject_in_node(next_loc, child_height, child_depth)
                                                  ? PL_WRITE_CHEAP
                                                  : PL_READ);
                if (next_loc != NEITHER_EXTREME || (toku_bnc_should_promote(ft, bnc) && depth <= 1)) {
                    // If we're on either extreme, or the birdie wants to
                    // promote and we're in the top two levels of the
                    // tree, don't stop just because someone else has the
                    // node locked.
                    ftnode_fetch_extra bfe;
                    bfe.create_for_full_read(ft);
                    if (lock_type == PL_WRITE_CHEAP) {
                        // We intend to take the write lock for message injection
                        toku::context inject_ctx(CTX_MESSAGE_INJECTION);
                        toku_pin_ftnode(ft, child_blocknum, child_fullhash, &bfe, lock_type, &child, true);
                    } else {
                        // We're going to keep promoting
                        toku::context promo_ctx(CTX_PROMO);
                        toku_pin_ftnode(ft, child_blocknum, child_fullhash, &bfe, lock_type, &child, true);
                    }
                } else {
                    r = toku_maybe_pin_ftnode_clean(ft, child_blocknum, child_fullhash, lock_type, &child);
                    if (r != 0) {
                        // We couldn't get the child cheaply, so give up on promoting.
                        STATUS_INC(FT_PRO_NUM_STOP_LOCK_CHILD, 1);
                        goto relock_and_push_here;
                    }
                    if (toku_ftnode_fully_in_memory(child)) {
                        // toku_pin_ftnode... touches the clock but toku_maybe_pin_ftnode... doesn't.
                        // This prevents partial eviction.
                        for (int i = 0; i < child->n_children; ++i) {
                            BP_TOUCH_CLOCK(child, i);
                        }
                    } else {
                        // We got the child, but it's not fully in memory.  Give up on promoting.
                        STATUS_INC(FT_PRO_NUM_STOP_CHILD_INMEM, 1);
                        goto unlock_child_and_push_here;
                    }
                }
            }
            paranoid_invariant_notnull(child);

            if (!just_did_split_or_merge) {
                BLOCKNUM subtree_root_blocknum = subtree_root->blocknum;
                uint32_t subtree_root_fullhash = toku_cachetable_hash(ft->cf, subtree_root_blocknum);
                const bool did_split_or_merge = process_maybe_reactive_child(ft, subtree_root, child, childnum, loc);
                if (did_split_or_merge) {
                    // Need to re-pin this node and try at this level again.
                    FTNODE newparent;
                    ftnode_fetch_extra bfe;
                    bfe.create_for_full_read(ft); // should be fully in memory, we just split it
                    toku_pin_ftnode(ft, subtree_root_blocknum, subtree_root_fullhash, &bfe, PL_READ, &newparent, true);
                    push_something_in_subtree(ft, newparent, -1, msg, flow_deltas, gc_info, depth, loc, true);
                    return;
                }
            }

            if (next_loc != NEITHER_EXTREME || child->dirty || toku_bnc_should_promote(ft, bnc)) {
                push_something_in_subtree(ft, child, -1, msg, flow_deltas, gc_info, depth + 1, next_loc, false);
                toku_sync_fetch_and_add(&bnc->flow[0], flow_deltas[0]);
                // The recursive call unpinned the child, but
                // we're responsible for unpinning subtree_root.
                toku_unpin_ftnode_read_only(ft, subtree_root);
                return;
            }

            STATUS_INC(FT_PRO_NUM_DIDNT_WANT_PROMOTE, 1);
        unlock_child_and_push_here:
            // We locked the child, but we decided not to promote.
            // Unlock the child, and fall through to the next case.
            toku_unpin_ftnode_read_only(ft, child);
        }
    relock_and_push_here:
        // Give up on promoting.
        // We have subtree_root read-locked and we don't have a child locked.
        // Drop the read lock, grab a write lock, and inject here.
        {
            // Right now we have a read lock on subtree_root, but we want
            // to inject into it so we get a write lock instead.
            BLOCKNUM subtree_root_blocknum = subtree_root->blocknum;
            uint32_t subtree_root_fullhash = toku_cachetable_hash(ft->cf, subtree_root_blocknum);
            toku_unpin_ftnode_read_only(ft, subtree_root);
            switch (depth) {
            case 0:
                STATUS_INC(FT_PRO_NUM_INJECT_DEPTH_0, 1); break;
            case 1:
                STATUS_INC(FT_PRO_NUM_INJECT_DEPTH_1, 1); break;
            case 2:
                STATUS_INC(FT_PRO_NUM_INJECT_DEPTH_2, 1); break;
            case 3:
                STATUS_INC(FT_PRO_NUM_INJECT_DEPTH_3, 1); break;
            default:
                STATUS_INC(FT_PRO_NUM_INJECT_DEPTH_GT3, 1); break;
            }
            inject_message_at_this_blocknum(ft, subtree_root_blocknum, subtree_root_fullhash, msg, flow_deltas, gc_info);
        }
    }
}

void toku_ft_root_put_msg(
    FT ft, 
    const ft_msg &msg, 
    txn_gc_info *gc_info
    )
// Effect:
//  - assign msn to message and update msn in the header
//  - push the message into the ft

// As of Clayface, the root blocknum is a constant, so preventing a
// race between message injection and the split of a root is the job
// of the cachetable's locking rules.
//
// We also hold the MO lock for a number of reasons, but an important
// one is to make sure that a begin_checkpoint may not start while
// this code is executing. A begin_checkpoint does (at least) two things
// that can interfere with the operations here:
//  - Copies the header to a checkpoint header. Because we may change
//    the max_msn_in_ft below, we don't want the header to be copied in
//    the middle of these operations.
//  - Takes note of the log's LSN. Because this put operation has
//    already been logged, this message injection must be included
//    in any checkpoint that contains this put's logentry.
//    Holding the mo lock throughout this function ensures that fact.
{
    toku::context promo_ctx(CTX_PROMO);

    // blackhole fractal trees drop all messages, so do nothing.
    if (ft->blackhole) {
        return;
    }

    FTNODE node;

    uint32_t fullhash;
    CACHEKEY root_key;
    toku_calculate_root_offset_pointer(ft, &root_key, &fullhash);
    ftnode_fetch_extra bfe;
    bfe.create_for_full_read(ft);

    size_t flow_deltas[] = { message_buffer::msg_memsize_in_buffer(msg), 0 };

    pair_lock_type lock_type;
    lock_type = PL_READ; // try first for a read lock
    // If we need to split the root, we'll have to change from a read lock
    // to a write lock and check again.  We change the variable lock_type
    // and jump back to here.
 change_lock_type:
    // get the root node
    toku_pin_ftnode(ft, root_key, fullhash, &bfe, lock_type, &node, true);
    toku_ftnode_assert_fully_in_memory(node);
    paranoid_invariant(node->fullhash==fullhash);
    ft_verify_flags(ft, node);

    // First handle a reactive root.
    // This relocking for split algorithm will cause every message
    // injection thread to change lock type back and forth, when only one
    // of them needs to in order to handle the split.  That's not great,
    // but root splits are incredibly rare.
    enum reactivity re = toku_ftnode_get_reactivity(ft, node);
    switch (re) {
    case RE_STABLE:
    case RE_FUSIBLE: // cannot merge anything at the root
        if (lock_type != PL_READ) {
            // We thought we needed to split, but someone else got to
            // it before us.  Downgrade to a read lock.
            toku_unpin_ftnode_read_only(ft, node);
            lock_type = PL_READ;
            goto change_lock_type;
        }
        break;
    case RE_FISSIBLE:
        if (lock_type == PL_READ) {
            // Here, we only have a read lock on the root.  In order
            // to split it, we need a write lock, but in the course of
            // gaining the write lock, someone else may have gotten in
            // before us and split it.  So we upgrade to a write lock
            // and check again.
            toku_unpin_ftnode_read_only(ft, node);
            lock_type = PL_WRITE_CHEAP;
            goto change_lock_type;
        } else {
            // We have a write lock, now we can split.
            ft_init_new_root(ft, node, &node);
            // Then downgrade back to a read lock, and we can finally
            // do the injection.
            toku_unpin_ftnode(ft, node);
            lock_type = PL_READ;
            STATUS_INC(FT_PRO_NUM_ROOT_SPLIT, 1);
            goto change_lock_type;
        }
        break;
    }
    // If we get to here, we have a read lock and the root doesn't
    // need to be split.  It's safe to inject the message.
    paranoid_invariant(lock_type == PL_READ);
    // We cannot assert that we have the read lock because frwlock asserts
    // that its mutex is locked when we check if there are any readers.
    // That wouldn't give us a strong guarantee that we have the read lock
    // anyway.

    // Now, either inject here or promote.  We decide based on a heuristic:
    if (node->height == 0 || !ft_msg_type_applies_once(msg.type())) {
        // If the root's a leaf or we're injecting a broadcast, drop the read lock and inject here.
        toku_unpin_ftnode_read_only(ft, node);
        STATUS_INC(FT_PRO_NUM_ROOT_H0_INJECT, 1);
        inject_message_at_this_blocknum(ft, root_key, fullhash, msg, flow_deltas, gc_info);
    } else if (node->height > 1) {
        // If the root's above height 1, we are definitely eligible for promotion.
        push_something_in_subtree(ft, node, -1, msg, flow_deltas, gc_info, 0, LEFT_EXTREME | RIGHT_EXTREME, false);
    } else {
        // The root's height 1.  We may be eligible for promotion here.
        // On the extremes, we want to promote, in the middle, we don't.
        int childnum = toku_ftnode_which_child(node, msg.kdbt(), ft->cmp);
        if (childnum == 0 || childnum == node->n_children - 1) {
            // On the extremes, promote.  We know which childnum we're going to, so pass that down too.
            push_something_in_subtree(ft, node, childnum, msg, flow_deltas, gc_info, 0, LEFT_EXTREME | RIGHT_EXTREME, false);
        } else {
            // At height 1 in the middle, don't promote, drop the read lock and inject here.
            toku_unpin_ftnode_read_only(ft, node);
            STATUS_INC(FT_PRO_NUM_ROOT_H1_INJECT, 1);
            inject_message_at_this_blocknum(ft, root_key, fullhash, msg, flow_deltas, gc_info);
        }
    }
}

// TODO: Remove me, I'm boring.
static int ft_compare_keys(FT ft, const DBT *a, const DBT *b)
// Effect: Compare two keys using the given fractal tree's comparator/descriptor
{
    return ft->cmp(a, b);
}

static LEAFENTRY bn_get_le_and_key(BASEMENTNODE bn, int idx, DBT *key)
// Effect: Gets the i'th leafentry from the given basement node and
//         fill its key in *key
// Requires: The i'th leafentry exists.
{
    LEAFENTRY le;
    uint32_t le_len;
    void *le_key;
    int r = bn->data_buffer.fetch_klpair(idx, &le, &le_len, &le_key);
    invariant_zero(r);
    toku_fill_dbt(key, le_key, le_len);
    return le;
}

static LEAFENTRY ft_leaf_leftmost_le_and_key(FTNODE leaf, DBT *leftmost_key)
// Effect: If a leftmost key exists in the given leaf, toku_fill_dbt()
//         the key into *leftmost_key
// Requires: Leaf is fully in memory and pinned for read or write.
// Return: leafentry if it exists, nullptr otherwise
{
    for (int i = 0; i < leaf->n_children; i++) {
        BASEMENTNODE bn = BLB(leaf, i);
        if (bn->data_buffer.num_klpairs() > 0) {
            // Get the first (leftmost) leafentry and its key
            return bn_get_le_and_key(bn, 0, leftmost_key);
        }
    }
    return nullptr;
}

static LEAFENTRY ft_leaf_rightmost_le_and_key(FTNODE leaf, DBT *rightmost_key)
// Effect: If a rightmost key exists in the given leaf, toku_fill_dbt()
//         the key into *rightmost_key
// Requires: Leaf is fully in memory and pinned for read or write.
// Return: leafentry if it exists, nullptr otherwise
{
    for (int i = leaf->n_children - 1; i >= 0; i--) {
        BASEMENTNODE bn = BLB(leaf, i);
        size_t num_les = bn->data_buffer.num_klpairs();
        if (num_les > 0) {
            // Get the last (rightmost) leafentry and its key
            return bn_get_le_and_key(bn, num_les - 1, rightmost_key);
        }
    }
    return nullptr;
}

static int ft_leaf_get_relative_key_pos(FT ft, FTNODE leaf, const DBT *key, bool *nondeleted_key_found, int *target_childnum)
// Effect: Determines what the relative position of the given key is with
//         respect to a leaf node, and if it exists.
// Requires: Leaf is fully in memory and pinned for read or write.
// Requires: target_childnum is non-null
// Return: < 0 if key is less than the leftmost key in the leaf OR the relative position is unknown, for any reason.
//         0 if key is in the bounds [leftmost_key, rightmost_key] for this leaf or the leaf is empty
//         > 0 if key is greater than the rightmost key in the leaf
//         *nondeleted_key_found is set (if non-null) if the target key was found and is not deleted, unmodified otherwise
//         *target_childnum is set to the child that (does or would) contain the key, if calculated, unmodified otherwise
{
    DBT rightmost_key;
    LEAFENTRY rightmost_le = ft_leaf_rightmost_le_and_key(leaf, &rightmost_key);
    if (rightmost_le == nullptr) {
        // If we can't get a rightmost key then the leaf is empty.
        // In such a case, we don't have any information about what keys would be in this leaf.
        // We have to assume the leaf node that would contain this key is to the left.
        return -1;
    }
    // We have a rightmost leafentry, so it must exist in some child node
    invariant(leaf->n_children > 0);

    int relative_pos = 0;
    int c = ft_compare_keys(ft, key, &rightmost_key);
    if (c > 0) {
        relative_pos = 1;
        *target_childnum = leaf->n_children - 1;
    } else if (c == 0) {
        if (nondeleted_key_found != nullptr && !le_latest_is_del(rightmost_le)) {
            *nondeleted_key_found = true;
        }
        relative_pos = 0;
        *target_childnum = leaf->n_children - 1;
    } else {
        // The key is less than the rightmost. It may still be in bounds if it's >= the leftmost.
        DBT leftmost_key;
        LEAFENTRY leftmost_le = ft_leaf_leftmost_le_and_key(leaf, &leftmost_key);
        invariant_notnull(leftmost_le); // Must exist because a rightmost exists
        c = ft_compare_keys(ft, key, &leftmost_key);
        if (c > 0) {
            if (nondeleted_key_found != nullptr) {
                // The caller wants to know if a nondeleted key can be found.
                LEAFENTRY target_le;
                int childnum = toku_ftnode_which_child(leaf, key, ft->cmp);
                BASEMENTNODE bn = BLB(leaf, childnum);
                struct toku_msg_leafval_heaviside_extra extra(ft->cmp, key);
                int r = bn->data_buffer.find_zero<decltype(extra), toku_msg_leafval_heaviside>(
                    extra,
                    &target_le,
                    nullptr, nullptr, nullptr
                    );
                *target_childnum = childnum;
                if (r == 0 && !le_latest_is_del(target_le)) {
                    *nondeleted_key_found = true;
                }
            }
            relative_pos = 0;
        } else if (c == 0) {
            if (nondeleted_key_found != nullptr && !le_latest_is_del(leftmost_le)) {
                *nondeleted_key_found = true;
            }
            relative_pos = 0;
            *target_childnum = 0;
        } else {
            relative_pos = -1;
        }
    }

    return relative_pos;
}

static void ft_insert_directly_into_leaf(FT ft, FTNODE leaf, int target_childnum, DBT *key, DBT *val,
                                         XIDS message_xids, enum ft_msg_type type, txn_gc_info *gc_info);
static int getf_nothing(uint32_t, const void *, uint32_t, const void *, void *, bool);

static int ft_maybe_insert_into_rightmost_leaf(FT ft, DBT *key, DBT *val, XIDS message_xids, enum ft_msg_type type,
                                               txn_gc_info *gc_info, bool unique)
// Effect: Pins the rightmost leaf node and attempts to do an insert.
//         There are three reasons why we may not succeed.
//         - The rightmost leaf is too full and needs a split. 
//         - The key to insert is not within the provable bounds of this leaf node.
//         - The key is within bounds, but it already exists.
// Return: 0 if this function did insert, DB_KEYEXIST if a unique key constraint exists and
//         some nondeleted leafentry with the same key exists
//         < 0 if this function did not insert, for a reason other than DB_KEYEXIST.
// Note: Treat this function as a possible, but not necessary, optimization for insert.
// Rationale: We want O(1) insertions down the rightmost path of the tree.
{
    int r = -1;

    uint32_t rightmost_fullhash;
    BLOCKNUM rightmost_blocknum;
    FTNODE rightmost_leaf = nullptr;

    // Don't do the optimization if our heurstic suggests that
    // insertion pattern is not sequential.
    if (toku_drd_unsafe_fetch(&ft->seqinsert_score) < FT_SEQINSERT_SCORE_THRESHOLD) {
        goto cleanup;
    }

    // We know the seqinsert score is high enough that we should
    // attempt to directly insert into the rightmost leaf. Because
    // the score is non-zero, the rightmost blocknum must have been
    // set. See inject_message_in_locked_node(), which only increases
    // the score if the target node blocknum == rightmost_blocknum
    rightmost_blocknum = ft->rightmost_blocknum;
    invariant(rightmost_blocknum.b != RESERVED_BLOCKNUM_NULL);

    // Pin the rightmost leaf with a write lock.
    rightmost_fullhash = toku_cachetable_hash(ft->cf, rightmost_blocknum);
    ftnode_fetch_extra bfe;
    bfe.create_for_full_read(ft);
    toku_pin_ftnode(ft, rightmost_blocknum, rightmost_fullhash, &bfe, PL_WRITE_CHEAP, &rightmost_leaf, true);

    // The rightmost blocknum never chances once it is initialized to something
    // other than null. Verify that the pinned node has the correct blocknum.
    invariant(rightmost_leaf->blocknum.b == rightmost_blocknum.b);

    // If the rightmost leaf is reactive, bail out out and let the normal promotion pass
    // take care of it. This also ensures that if any of our ancestors are reactive,
    // they'll be taken care of too.
    if (toku_ftnode_get_leaf_reactivity(rightmost_leaf, ft->h->nodesize) != RE_STABLE) {
        STATUS_INC(FT_PRO_RIGHTMOST_LEAF_SHORTCUT_FAIL_REACTIVE, 1);
        goto cleanup;
    }

    // The groundwork has been laid for an insertion directly into the rightmost
    // leaf node. We know that it is pinned for write, fully in memory, has
    // no messages above it, and is not reactive.
    //
    // Now, two more things must be true for this insertion to actually happen:
    // 1. The key to insert is within the bounds of this leafnode, or to the right.
    // 2. If there is a uniqueness constraint, it passes.
    bool nondeleted_key_found;
    int relative_pos;
    int target_childnum;

    nondeleted_key_found = false;
    target_childnum = -1;
    relative_pos = ft_leaf_get_relative_key_pos(ft, rightmost_leaf, key,
                                                unique ? &nondeleted_key_found : nullptr,
                                                &target_childnum);
    if (relative_pos >= 0) {
        STATUS_INC(FT_PRO_RIGHTMOST_LEAF_SHORTCUT_SUCCESS, 1);
        if (unique && nondeleted_key_found) {
            r = DB_KEYEXIST;
        } else {
            ft_insert_directly_into_leaf(ft, rightmost_leaf, target_childnum,
                                         key, val, message_xids, type, gc_info);
            r = 0;
        }
    } else {
        STATUS_INC(FT_PRO_RIGHTMOST_LEAF_SHORTCUT_FAIL_POS, 1);
        r = -1;
    }

cleanup:
    // If we did the insert, the rightmost leaf was unpinned for us.
    if (r != 0 && rightmost_leaf != nullptr) {
        toku_unpin_ftnode(ft, rightmost_leaf);
    }

    return r;
}
 
static void ft_txn_log_insert(FT ft, DBT *key, DBT *val, TOKUTXN txn, bool do_logging, enum ft_msg_type type);

int toku_ft_insert_unique(FT_HANDLE ft_h, DBT *key, DBT *val, TOKUTXN txn, bool do_logging) {
// Effect: Insert a unique key-val pair into the fractal tree.
// Return: 0 on success, DB_KEYEXIST if the overwrite constraint failed
    XIDS message_xids = txn != nullptr ? toku_txn_get_xids(txn) : toku_xids_get_root_xids();

    TXN_MANAGER txn_manager = toku_ft_get_txn_manager(ft_h);
    txn_manager_state txn_state_for_gc(txn_manager);

    TXNID oldest_referenced_xid_estimate = toku_ft_get_oldest_referenced_xid_estimate(ft_h);
    txn_gc_info gc_info(&txn_state_for_gc,
                        oldest_referenced_xid_estimate,
                        // no messages above us, we can implicitly promote uxrs based on this xid
                        oldest_referenced_xid_estimate,
                        true);
    int r = ft_maybe_insert_into_rightmost_leaf(ft_h->ft, key, val, message_xids, FT_INSERT, &gc_info, true);
    if (r != 0 && r != DB_KEYEXIST) {
        // Default to a regular unique check + insert algorithm if we couldn't
        // do it based on the rightmost leaf alone.
        int lookup_r = toku_ft_lookup(ft_h, key, getf_nothing, nullptr);
        if (lookup_r == DB_NOTFOUND) {
            toku_ft_send_insert(ft_h, key, val, message_xids, FT_INSERT, &gc_info);
            r = 0;
        } else {
            r = DB_KEYEXIST;
        }
    }

    if (r == 0) {
        ft_txn_log_insert(ft_h->ft, key, val, txn, do_logging, FT_INSERT);
    }
    return r;
}

// Effect: Insert the key-val pair into an ft.
void toku_ft_insert (FT_HANDLE ft_handle, DBT *key, DBT *val, TOKUTXN txn) {
    toku_ft_maybe_insert(ft_handle, key, val, txn, false, ZERO_LSN, true, FT_INSERT);
}

void toku_ft_load_recovery(TOKUTXN txn, FILENUM old_filenum, char const * new_iname, int do_fsync, int do_log, LSN *load_lsn) {
    paranoid_invariant(txn);
    toku_txn_force_fsync_on_commit(txn);  //If the txn commits, the commit MUST be in the log
                                          //before the (old) file is actually unlinked
    TOKULOGGER logger = toku_txn_logger(txn);

    BYTESTRING new_iname_bs = {.len=(uint32_t) strlen(new_iname), .data=(char*)new_iname};
    toku_logger_save_rollback_load(txn, old_filenum, &new_iname_bs);
    if (do_log && logger) {
        TXNID_PAIR xid = toku_txn_get_txnid(txn);
        toku_log_load(logger, load_lsn, do_fsync, txn, xid, old_filenum, new_iname_bs);
    }
}

// 2954
// this function handles the tasks needed to be recoverable
//  - write to rollback log
//  - write to recovery log
void toku_ft_hot_index_recovery(TOKUTXN txn, FILENUMS filenums, int do_fsync, int do_log, LSN *hot_index_lsn)
{
    paranoid_invariant(txn);
    TOKULOGGER logger = toku_txn_logger(txn);

    // write to the rollback log
    toku_logger_save_rollback_hot_index(txn, &filenums);
    if (do_log && logger) {
        TXNID_PAIR xid = toku_txn_get_txnid(txn);
        // write to the recovery log
        toku_log_hot_index(logger, hot_index_lsn, do_fsync, txn, xid, filenums);
    }
}

// Effect: Optimize the ft.
void toku_ft_optimize (FT_HANDLE ft_h) {
    TOKULOGGER logger = toku_cachefile_logger(ft_h->ft->cf);
    if (logger) {
        TXNID oldest = toku_txn_manager_get_oldest_living_xid(logger->txn_manager);

        XIDS root_xids = toku_xids_get_root_xids();
        XIDS message_xids;
        if (oldest == TXNID_NONE_LIVING) {
            message_xids = root_xids;
        }
        else {
            int r = toku_xids_create_child(root_xids, &message_xids, oldest);
            invariant(r == 0);
        }

        DBT key;
        DBT val;
        toku_init_dbt(&key);
        toku_init_dbt(&val);
        ft_msg msg(&key, &val, FT_OPTIMIZE, ZERO_MSN, message_xids);

        TXN_MANAGER txn_manager = toku_ft_get_txn_manager(ft_h);
        txn_manager_state txn_state_for_gc(txn_manager);

        TXNID oldest_referenced_xid_estimate = toku_ft_get_oldest_referenced_xid_estimate(ft_h);
        txn_gc_info gc_info(&txn_state_for_gc,
                            oldest_referenced_xid_estimate,
                            // no messages above us, we can implicitly promote uxrs based on this xid
                            oldest_referenced_xid_estimate,
                            true);
        toku_ft_root_put_msg(ft_h->ft, msg, &gc_info);
        toku_xids_destroy(&message_xids);
    }
}

void toku_ft_load(FT_HANDLE ft_handle, TOKUTXN txn, char const * new_iname, int do_fsync, LSN *load_lsn) {
    FILENUM old_filenum = toku_cachefile_filenum(ft_handle->ft->cf);
    int do_log = 1;
    toku_ft_load_recovery(txn, old_filenum, new_iname, do_fsync, do_log, load_lsn);
}

// ft actions for logging hot index filenums
void toku_ft_hot_index(FT_HANDLE ft_handle __attribute__ ((unused)), TOKUTXN txn, FILENUMS filenums, int do_fsync, LSN *lsn) {
    int do_log = 1;
    toku_ft_hot_index_recovery(txn, filenums, do_fsync, do_log, lsn);
}

void
toku_ft_log_put (TOKUTXN txn, FT_HANDLE ft_handle, const DBT *key, const DBT *val) {
    TOKULOGGER logger = toku_txn_logger(txn);
    if (logger) {
        BYTESTRING keybs = {.len=key->size, .data=(char *) key->data};
        BYTESTRING valbs = {.len=val->size, .data=(char *) val->data};
        TXNID_PAIR xid = toku_txn_get_txnid(txn);
        toku_log_enq_insert(logger, (LSN*)0, 0, txn, toku_cachefile_filenum(ft_handle->ft->cf), xid, keybs, valbs);
    }
}

void
toku_ft_log_put_multiple (TOKUTXN txn, FT_HANDLE src_ft, FT_HANDLE *fts, uint32_t num_fts, const DBT *key, const DBT *val) {
    assert(txn);
    assert(num_fts > 0);
    TOKULOGGER logger = toku_txn_logger(txn);
    if (logger) {
        FILENUM         fnums[num_fts];
        uint32_t i;
        for (i = 0; i < num_fts; i++) {
            fnums[i] = toku_cachefile_filenum(fts[i]->ft->cf);
        }
        FILENUMS filenums = {.num = num_fts, .filenums = fnums};
        BYTESTRING keybs = {.len=key->size, .data=(char *) key->data};
        BYTESTRING valbs = {.len=val->size, .data=(char *) val->data};
        TXNID_PAIR xid = toku_txn_get_txnid(txn);
        FILENUM src_filenum = src_ft ? toku_cachefile_filenum(src_ft->ft->cf) : FILENUM_NONE;
        toku_log_enq_insert_multiple(logger, (LSN*)0, 0, txn, src_filenum, filenums, xid, keybs, valbs);
    }
}

TXN_MANAGER toku_ft_get_txn_manager(FT_HANDLE ft_h) {
    TOKULOGGER logger = toku_cachefile_logger(ft_h->ft->cf);
    return logger != nullptr ? toku_logger_get_txn_manager(logger) : nullptr;
}

TXNID toku_ft_get_oldest_referenced_xid_estimate(FT_HANDLE ft_h) {
    TXN_MANAGER txn_manager = toku_ft_get_txn_manager(ft_h);
    return txn_manager != nullptr ? toku_txn_manager_get_oldest_referenced_xid_estimate(txn_manager) : TXNID_NONE;
}

static void ft_txn_log_insert(FT ft, DBT *key, DBT *val, TOKUTXN txn, bool do_logging, enum ft_msg_type type) {
    paranoid_invariant(type == FT_INSERT || type == FT_INSERT_NO_OVERWRITE);

    //By default use committed messages
    TXNID_PAIR xid = toku_txn_get_txnid(txn);
    if (txn) {
        BYTESTRING keybs = {key->size, (char *) key->data};
        toku_logger_save_rollback_cmdinsert(txn, toku_cachefile_filenum(ft->cf), &keybs);
        toku_txn_maybe_note_ft(txn, ft);
    }
    TOKULOGGER logger = toku_txn_logger(txn);
    if (do_logging && logger) {
        BYTESTRING keybs = {.len=key->size, .data=(char *) key->data};
        BYTESTRING valbs = {.len=val->size, .data=(char *) val->data};
        if (type == FT_INSERT) {
            toku_log_enq_insert(logger, (LSN*)0, 0, txn, toku_cachefile_filenum(ft->cf), xid, keybs, valbs);
        }
        else {
            toku_log_enq_insert_no_overwrite(logger, (LSN*)0, 0, txn, toku_cachefile_filenum(ft->cf), xid, keybs, valbs);
        }
    }
}

void toku_ft_maybe_insert (FT_HANDLE ft_h, DBT *key, DBT *val, TOKUTXN txn, bool oplsn_valid, LSN oplsn, bool do_logging, enum ft_msg_type type) {
    ft_txn_log_insert(ft_h->ft, key, val, txn, do_logging, type);

    LSN treelsn;
    if (oplsn_valid && oplsn.lsn <= (treelsn = toku_ft_checkpoint_lsn(ft_h->ft)).lsn) {
        // do nothing
    } else {
        XIDS message_xids = txn ? toku_txn_get_xids(txn) : toku_xids_get_root_xids();

        TXN_MANAGER txn_manager = toku_ft_get_txn_manager(ft_h);
        txn_manager_state txn_state_for_gc(txn_manager);

        TXNID oldest_referenced_xid_estimate = toku_ft_get_oldest_referenced_xid_estimate(ft_h);
        txn_gc_info gc_info(&txn_state_for_gc,
                            oldest_referenced_xid_estimate,
                            // no messages above us, we can implicitly promote uxrs based on this xid
                            oldest_referenced_xid_estimate,
                            txn != nullptr ? !txn->for_recovery : false);
        int r = ft_maybe_insert_into_rightmost_leaf(ft_h->ft, key, val, message_xids, FT_INSERT, &gc_info, false);
        if (r != 0) {
            toku_ft_send_insert(ft_h, key, val, message_xids, type, &gc_info);
        }
    }
}

static void ft_insert_directly_into_leaf(FT ft, FTNODE leaf, int target_childnum, DBT *key, DBT *val,
                                         XIDS message_xids, enum ft_msg_type type, txn_gc_info *gc_info)
// Effect: Insert directly into a leaf node a fractal tree. Does not do any logging.
// Requires: Leaf is fully in memory and pinned for write.
// Requires: If this insertion were to happen through the root node, the promotion
//           algorithm would have selected the given leaf node as the point of injection.
//           That means this function relies on the current implementation of promotion.
{
    ft_msg msg(key, val, type, ZERO_MSN, message_xids);
    size_t flow_deltas[] = { 0, 0 }; 
    inject_message_in_locked_node(ft, leaf, target_childnum, msg, flow_deltas, gc_info);
}

static void
ft_send_update_msg(FT_HANDLE ft_h, const ft_msg &msg, TOKUTXN txn) {
    TXN_MANAGER txn_manager = toku_ft_get_txn_manager(ft_h);
    txn_manager_state txn_state_for_gc(txn_manager);

    TXNID oldest_referenced_xid_estimate = toku_ft_get_oldest_referenced_xid_estimate(ft_h);
    txn_gc_info gc_info(&txn_state_for_gc,
                        oldest_referenced_xid_estimate,
                        // no messages above us, we can implicitly promote uxrs based on this xid
                        oldest_referenced_xid_estimate,
                        txn != nullptr ? !txn->for_recovery : false);
    toku_ft_root_put_msg(ft_h->ft, msg, &gc_info);
}

void toku_ft_maybe_update(FT_HANDLE ft_h, const DBT *key, const DBT *update_function_extra,
                      TOKUTXN txn, bool oplsn_valid, LSN oplsn,
                      bool do_logging) {
    TXNID_PAIR xid = toku_txn_get_txnid(txn);
    if (txn) {
        BYTESTRING keybs = { key->size, (char *) key->data };
        toku_logger_save_rollback_cmdupdate(
            txn, toku_cachefile_filenum(ft_h->ft->cf), &keybs);
        toku_txn_maybe_note_ft(txn, ft_h->ft);
    }

    TOKULOGGER logger;
    logger = toku_txn_logger(txn);
    if (do_logging && logger) {
        BYTESTRING keybs = {.len=key->size, .data=(char *) key->data};
        BYTESTRING extrabs = {.len=update_function_extra->size,
                              .data = (char *) update_function_extra->data};
        toku_log_enq_update(logger, NULL, 0, txn,
                                toku_cachefile_filenum(ft_h->ft->cf),
                                xid, keybs, extrabs);
    }

    LSN treelsn;
    if (oplsn_valid && oplsn.lsn <= (treelsn = toku_ft_checkpoint_lsn(ft_h->ft)).lsn) {
        // do nothing
    } else {
        XIDS message_xids = txn ? toku_txn_get_xids(txn) : toku_xids_get_root_xids();
        ft_msg msg(key, update_function_extra, FT_UPDATE, ZERO_MSN, message_xids);
        ft_send_update_msg(ft_h, msg, txn);
    }
}

void toku_ft_maybe_update_broadcast(FT_HANDLE ft_h, const DBT *update_function_extra,
                                TOKUTXN txn, bool oplsn_valid, LSN oplsn,
                                bool do_logging, bool is_resetting_op) {
    TXNID_PAIR xid = toku_txn_get_txnid(txn);
    uint8_t  resetting = is_resetting_op ? 1 : 0;
    if (txn) {
        toku_logger_save_rollback_cmdupdatebroadcast(txn, toku_cachefile_filenum(ft_h->ft->cf), resetting);
        toku_txn_maybe_note_ft(txn, ft_h->ft);
    }

    TOKULOGGER logger;
    logger = toku_txn_logger(txn);
    if (do_logging && logger) {
        BYTESTRING extrabs = {.len=update_function_extra->size,
                              .data = (char *) update_function_extra->data};
        toku_log_enq_updatebroadcast(logger, NULL, 0, txn,
                                         toku_cachefile_filenum(ft_h->ft->cf),
                                         xid, extrabs, resetting);
    }

    //TODO(yoni): remove treelsn here and similar calls (no longer being used)
    LSN treelsn;
    if (oplsn_valid &&
        oplsn.lsn <= (treelsn = toku_ft_checkpoint_lsn(ft_h->ft)).lsn) {

    } else {
        DBT empty_dbt;
        XIDS message_xids = txn ? toku_txn_get_xids(txn) : toku_xids_get_root_xids();
        ft_msg msg(toku_init_dbt(&empty_dbt), update_function_extra, FT_UPDATE_BROADCAST_ALL, ZERO_MSN, message_xids);
        ft_send_update_msg(ft_h, msg, txn);
    }
}

void toku_ft_send_insert(FT_HANDLE ft_handle, DBT *key, DBT *val, XIDS xids, enum ft_msg_type type, txn_gc_info *gc_info) {
    ft_msg msg(key, val, type, ZERO_MSN, xids);
    toku_ft_root_put_msg(ft_handle->ft, msg, gc_info);
}

void toku_ft_send_commit_any(FT_HANDLE ft_handle, DBT *key, XIDS xids, txn_gc_info *gc_info) {
    DBT val;
    ft_msg msg(key, toku_init_dbt(&val), FT_COMMIT_ANY, ZERO_MSN, xids);
    toku_ft_root_put_msg(ft_handle->ft, msg, gc_info);
}

void toku_ft_delete(FT_HANDLE ft_handle, DBT *key, TOKUTXN txn) {
    toku_ft_maybe_delete(ft_handle, key, txn, false, ZERO_LSN, true);
}

void
toku_ft_log_del(TOKUTXN txn, FT_HANDLE ft_handle, const DBT *key) {
    TOKULOGGER logger = toku_txn_logger(txn);
    if (logger) {
        BYTESTRING keybs = {.len=key->size, .data=(char *) key->data};
        TXNID_PAIR xid = toku_txn_get_txnid(txn);
        toku_log_enq_delete_any(logger, (LSN*)0, 0, txn, toku_cachefile_filenum(ft_handle->ft->cf), xid, keybs);
    }
}

void
toku_ft_log_del_multiple (TOKUTXN txn, FT_HANDLE src_ft, FT_HANDLE *fts, uint32_t num_fts, const DBT *key, const DBT *val) {
    assert(txn);
    assert(num_fts > 0);
    TOKULOGGER logger = toku_txn_logger(txn);
    if (logger) {
        FILENUM         fnums[num_fts];
        uint32_t i;
        for (i = 0; i < num_fts; i++) {
            fnums[i] = toku_cachefile_filenum(fts[i]->ft->cf);
        }
        FILENUMS filenums = {.num = num_fts, .filenums = fnums};
        BYTESTRING keybs = {.len=key->size, .data=(char *) key->data};
        BYTESTRING valbs = {.len=val->size, .data=(char *) val->data};
        TXNID_PAIR xid = toku_txn_get_txnid(txn);
        FILENUM src_filenum = src_ft ? toku_cachefile_filenum(src_ft->ft->cf) : FILENUM_NONE;
        toku_log_enq_delete_multiple(logger, (LSN*)0, 0, txn, src_filenum, filenums, xid, keybs, valbs);
    }
}

void toku_ft_maybe_delete(FT_HANDLE ft_h, DBT *key, TOKUTXN txn, bool oplsn_valid, LSN oplsn, bool do_logging) {
    XIDS message_xids = toku_xids_get_root_xids(); //By default use committed messages
    TXNID_PAIR xid = toku_txn_get_txnid(txn);
    if (txn) {
        BYTESTRING keybs = {key->size, (char *) key->data};
        toku_logger_save_rollback_cmddelete(txn, toku_cachefile_filenum(ft_h->ft->cf), &keybs);
        toku_txn_maybe_note_ft(txn, ft_h->ft);
        message_xids = toku_txn_get_xids(txn);
    }
    TOKULOGGER logger = toku_txn_logger(txn);
    if (do_logging && logger) {
        BYTESTRING keybs = {.len=key->size, .data=(char *) key->data};
        toku_log_enq_delete_any(logger, (LSN*)0, 0, txn, toku_cachefile_filenum(ft_h->ft->cf), xid, keybs);
    }

    LSN treelsn;
    if (oplsn_valid && oplsn.lsn <= (treelsn = toku_ft_checkpoint_lsn(ft_h->ft)).lsn) {
        // do nothing
    } else {
        TXN_MANAGER txn_manager = toku_ft_get_txn_manager(ft_h);
        txn_manager_state txn_state_for_gc(txn_manager);

        TXNID oldest_referenced_xid_estimate = toku_ft_get_oldest_referenced_xid_estimate(ft_h);
        txn_gc_info gc_info(&txn_state_for_gc,
                            oldest_referenced_xid_estimate,
                            // no messages above us, we can implicitly promote uxrs based on this xid
                            oldest_referenced_xid_estimate,
                            txn != nullptr ? !txn->for_recovery : false);
        toku_ft_send_delete(ft_h, key, message_xids, &gc_info);
    }
}

void toku_ft_send_delete(FT_HANDLE ft_handle, DBT *key, XIDS xids, txn_gc_info *gc_info) {
    DBT val; toku_init_dbt(&val);
    ft_msg msg(key, toku_init_dbt(&val), FT_DELETE_ANY, ZERO_MSN, xids);
    toku_ft_root_put_msg(ft_handle->ft, msg, gc_info);
}

/* ******************** open,close and create  ********************** */

// Test only function (not used in running system). This one has no env
int toku_open_ft_handle (const char *fname, int is_create, FT_HANDLE *ft_handle_p, int nodesize,
                   int basementnodesize,
                   enum toku_compression_method compression_method,
                   CACHETABLE cachetable, TOKUTXN txn,
                   int (*compare_fun)(DB *, const DBT*,const DBT*)) {
    FT_HANDLE ft_handle;
    const int only_create = 0;

    toku_ft_handle_create(&ft_handle);
    toku_ft_handle_set_nodesize(ft_handle, nodesize);
    toku_ft_handle_set_basementnodesize(ft_handle, basementnodesize);
    toku_ft_handle_set_compression_method(ft_handle, compression_method);
    toku_ft_handle_set_fanout(ft_handle, 16);
    toku_ft_set_bt_compare(ft_handle, compare_fun);

    int r = toku_ft_handle_open(ft_handle, fname, is_create, only_create, cachetable, txn);
    if (r != 0) {
        return r;
    }

    *ft_handle_p = ft_handle;
    return r;
}

static bool use_direct_io = true;

void toku_ft_set_direct_io (bool direct_io_on) {
    use_direct_io = direct_io_on;
}

static inline int ft_open_maybe_direct(const char *filename, int oflag, int mode) {
    if (use_direct_io) {
        return toku_os_open_direct(filename, oflag, mode);
    } else {
        return toku_os_open(filename, oflag, mode);
    }
}

static const mode_t file_mode = S_IRUSR+S_IWUSR+S_IRGRP+S_IWGRP+S_IROTH+S_IWOTH;

// open a file for use by the ft
// Requires:  File does not exist.
static int ft_create_file(FT_HANDLE UU(ft_handle), const char *fname, int *fdp) {
    int r;
    int fd;
    int er;
    fd = ft_open_maybe_direct(fname, O_RDWR | O_BINARY, file_mode);
    assert(fd==-1);
    if ((er = get_maybe_error_errno()) != ENOENT) {
        return er;
    }
    fd = ft_open_maybe_direct(fname, O_RDWR | O_CREAT | O_BINARY, file_mode);
    if (fd==-1) {
        r = get_error_errno();
        return r;
    }

    r = toku_fsync_directory(fname);
    if (r == 0) {
        *fdp = fd;
    } else {
        int rr = close(fd);
        assert_zero(rr);
    }
    return r;
}

// open a file for use by the ft.  if the file does not exist, error
static int ft_open_file(const char *fname, int *fdp) {
    int fd;
    fd = ft_open_maybe_direct(fname, O_RDWR | O_BINARY, file_mode);
    if (fd==-1) {
        return get_error_errno();
    }
    *fdp = fd;
    return 0;
}

void
toku_ft_handle_set_compression_method(FT_HANDLE t, enum toku_compression_method method)
{
    if (t->ft) {
        toku_ft_set_compression_method(t->ft, method);
    }
    else {
        t->options.compression_method = method;
    }
}

void
toku_ft_handle_get_compression_method(FT_HANDLE t, enum toku_compression_method *methodp)
{
    if (t->ft) {
        toku_ft_get_compression_method(t->ft, methodp);
    }
    else {
        *methodp = t->options.compression_method;
    }
}

void
toku_ft_handle_set_fanout(FT_HANDLE ft_handle, unsigned int fanout)
{
    if (ft_handle->ft) {
        toku_ft_set_fanout(ft_handle->ft, fanout);
    }
    else {
        ft_handle->options.fanout = fanout;
    }
}

void
toku_ft_handle_get_fanout(FT_HANDLE ft_handle, unsigned int *fanout)
{
    if (ft_handle->ft) {
        toku_ft_get_fanout(ft_handle->ft, fanout);
    }
    else {
        *fanout = ft_handle->options.fanout;
    }
}

// The memcmp magic byte may be set on a per fractal tree basis to communicate
// that if two keys begin with this byte, they may be compared with the builtin
// key comparison function. This greatly optimizes certain in-memory workloads,
// such as lookups by OID primary key in TokuMX.
int toku_ft_handle_set_memcmp_magic(FT_HANDLE ft_handle, uint8_t magic) {
    if (magic == comparator::MEMCMP_MAGIC_NONE) {
        return EINVAL;
    }
    if (ft_handle->ft != nullptr) {
        // if the handle is already open, then we cannot set the memcmp magic
        // (because it may or may not have been set by someone else already)
        return EINVAL;
    }
    ft_handle->options.memcmp_magic = magic;
    return 0;
}

static int
verify_builtin_comparisons_consistent(FT_HANDLE t, uint32_t flags) {
    if ((flags & TOKU_DB_KEYCMP_BUILTIN) && (t->options.compare_fun != toku_builtin_compare_fun)) {
        return EINVAL;
    }
    return 0;
}

//
// See comments in toku_db_change_descriptor to understand invariants 
// in the system when this function is called
//
void toku_ft_change_descriptor(
    FT_HANDLE ft_h,
    const DBT* old_descriptor,
    const DBT* new_descriptor,
    bool do_log,
    TOKUTXN txn,
    bool update_cmp_descriptor
    )
{
    DESCRIPTOR_S new_d;

    // if running with txns, save to rollback + write to recovery log
    if (txn) {
        // put information into rollback file
        BYTESTRING old_desc_bs = { old_descriptor->size, (char *) old_descriptor->data };
        BYTESTRING new_desc_bs = { new_descriptor->size, (char *) new_descriptor->data };
        toku_logger_save_rollback_change_fdescriptor(
            txn,
            toku_cachefile_filenum(ft_h->ft->cf),
            &old_desc_bs
            );
        toku_txn_maybe_note_ft(txn, ft_h->ft);

        if (do_log) {
            TOKULOGGER logger = toku_txn_logger(txn);
            TXNID_PAIR xid = toku_txn_get_txnid(txn);
            toku_log_change_fdescriptor(
                logger, NULL, 0,
                txn,
                toku_cachefile_filenum(ft_h->ft->cf),
                xid,
                old_desc_bs,
                new_desc_bs,
                update_cmp_descriptor
                );
        }
    }

    // write new_descriptor to header
    new_d.dbt = *new_descriptor;
    toku_ft_update_descriptor(ft_h->ft, &new_d);
    // very infrequent operation, worth precise threadsafe count
    STATUS_INC(FT_DESCRIPTOR_SET, 1);

    if (update_cmp_descriptor) {
        toku_ft_update_cmp_descriptor(ft_h->ft);
    }
}

static void
toku_ft_handle_inherit_options(FT_HANDLE t, FT ft) {
    struct ft_options options = {
        .nodesize = ft->h->nodesize,
        .basementnodesize = ft->h->basementnodesize,
        .compression_method = ft->h->compression_method,
        .fanout = ft->h->fanout,
        .flags = ft->h->flags,
        .memcmp_magic = ft->cmp.get_memcmp_magic(),
        .compare_fun = ft->cmp.get_compare_func(),
        .update_fun = ft->update_fun
    };
    t->options = options;
    t->did_set_flags = true;
}

// This is the actual open, used for various purposes, such as normal use, recovery, and redirect.
// fname_in_env is the iname, relative to the env_dir  (data_dir is already in iname as prefix).
// The checkpointed version (checkpoint_lsn) of the dictionary must be no later than max_acceptable_lsn .
// Requires: The multi-operation client lock must be held to prevent a checkpoint from occuring.
static int
ft_handle_open(FT_HANDLE ft_h, const char *fname_in_env, int is_create, int only_create, CACHETABLE cachetable, TOKUTXN txn, FILENUM use_filenum, DICTIONARY_ID use_dictionary_id, LSN max_acceptable_lsn) {
    int r;
    bool txn_created = false;
    char *fname_in_cwd = NULL;
    CACHEFILE cf = NULL;
    FT ft = NULL;
    bool did_create = false;
    toku_ft_open_close_lock();

    if (ft_h->did_set_flags) {
        r = verify_builtin_comparisons_consistent(ft_h, ft_h->options.flags);
        if (r!=0) { goto exit; }
    }

    assert(is_create || !only_create);
    FILENUM reserved_filenum;
    reserved_filenum = use_filenum;
    fname_in_cwd = toku_cachetable_get_fname_in_cwd(cachetable, fname_in_env);
    bool was_already_open;
    {
        int fd = -1;
        r = ft_open_file(fname_in_cwd, &fd);
        if (reserved_filenum.fileid == FILENUM_NONE.fileid) {
            reserved_filenum = toku_cachetable_reserve_filenum(cachetable);
        }
        if (r==ENOENT && is_create) {
            did_create = true;
            if (txn) {
                BYTESTRING bs = { .len=(uint32_t) strlen(fname_in_env), .data = (char*)fname_in_env };
                toku_logger_save_rollback_fcreate(txn, reserved_filenum, &bs); // bs is a copy of the fname relative to the environment
            }
            txn_created = (bool)(txn!=NULL);
            toku_logger_log_fcreate(txn, fname_in_env, reserved_filenum, file_mode, ft_h->options.flags, ft_h->options.nodesize, ft_h->options.basementnodesize, ft_h->options.compression_method);
            r = ft_create_file(ft_h, fname_in_cwd, &fd);
            if (r) { goto exit; }
        }
        if (r) { goto exit; }
        r=toku_cachetable_openfd_with_filenum(&cf, cachetable, fd, fname_in_env, reserved_filenum, &was_already_open);
        if (r) { goto exit; }
    }
    assert(ft_h->options.nodesize>0);
    if (is_create) {
        r = toku_read_ft_and_store_in_cachefile(ft_h, cf, max_acceptable_lsn, &ft);
        if (r==TOKUDB_DICTIONARY_NO_HEADER) {
            toku_ft_create(&ft, &ft_h->options, cf, txn);
        }
        else if (r!=0) {
            goto exit;
        }
        else if (only_create) {
            assert_zero(r);
            r = EEXIST;
            goto exit;
        }
        // if we get here, then is_create was true but only_create was false,
        // so it is ok for toku_read_ft_and_store_in_cachefile to have read
        // the header via toku_read_ft_and_store_in_cachefile
    } else {
        r = toku_read_ft_and_store_in_cachefile(ft_h, cf, max_acceptable_lsn, &ft);
        if (r) { goto exit; }
    }
    if (!ft_h->did_set_flags) {
        r = verify_builtin_comparisons_consistent(ft_h, ft_h->options.flags);
        if (r) { goto exit; }
    } else if (ft_h->options.flags != ft->h->flags) {                  /* if flags have been set then flags must match */
        r = EINVAL;
        goto exit;
    }

    // Ensure that the memcmp magic bits are consistent, if set.
    if (ft->cmp.get_memcmp_magic() != toku::comparator::MEMCMP_MAGIC_NONE &&
        ft_h->options.memcmp_magic != toku::comparator::MEMCMP_MAGIC_NONE &&
        ft_h->options.memcmp_magic != ft->cmp.get_memcmp_magic()) {
        r = EINVAL;
        goto exit;
    }
    toku_ft_handle_inherit_options(ft_h, ft);

    if (!was_already_open) {
        if (!did_create) { //Only log the fopen that OPENs the file.  If it was already open, don't log.
            toku_logger_log_fopen(txn, fname_in_env, toku_cachefile_filenum(cf), ft_h->options.flags);
        }
    }
    int use_reserved_dict_id;
    use_reserved_dict_id = use_dictionary_id.dictid != DICTIONARY_ID_NONE.dictid;
    if (!was_already_open) {
        DICTIONARY_ID dict_id;
        if (use_reserved_dict_id) {
            dict_id = use_dictionary_id;
        }
        else {
            dict_id = next_dict_id();
        }
        ft->dict_id = dict_id;
    }
    else {
        // dict_id is already in header
        if (use_reserved_dict_id) {
            assert(ft->dict_id.dictid == use_dictionary_id.dictid);
        }
    }
    assert(ft);
    assert(ft->dict_id.dictid != DICTIONARY_ID_NONE.dictid);
    assert(ft->dict_id.dictid < dict_id_serial);

    // important note here,
    // after this point, where we associate the header
    // with the ft_handle, the function is not allowed to fail
    // Code that handles failure (located below "exit"),
    // depends on this
    toku_ft_note_ft_handle_open(ft, ft_h);
    if (txn_created) {
        assert(txn);
        toku_txn_maybe_note_ft(txn, ft);
    }

    // Opening an ft may restore to previous checkpoint.
    // Truncate if necessary.
    {
        int fd = toku_cachefile_get_fd (ft->cf);
        ft->blocktable.maybe_truncate_file_on_open(fd);
    }

    r = 0;
exit:
    if (fname_in_cwd) {
        toku_free(fname_in_cwd);
    }
    if (r != 0 && cf) {
        if (ft) {
            // we only call toku_ft_note_ft_handle_open
            // when the function succeeds, so if we are here,
            // then that means we have a reference to the header
            // but we have not linked it to this ft. So,
            // we can simply try to remove the header.
            // We don't need to unlink this ft from the header
            toku_ft_grab_reflock(ft);
            bool needed = toku_ft_needed_unlocked(ft);
            toku_ft_release_reflock(ft);
            if (!needed) {
                // close immediately.
                toku_ft_evict_from_memory(ft, false, ZERO_LSN);
            }
        }
        else {
            toku_cachefile_close(&cf, false, ZERO_LSN);
        }
    }
    toku_ft_open_close_unlock();
    return r;
}

// Open an ft for the purpose of recovery, which requires that the ft be open to a pre-determined FILENUM
// and may require a specific checkpointed version of the file.
// (dict_id is assigned by the ft_handle_open() function.)
int
toku_ft_handle_open_recovery(FT_HANDLE t, const char *fname_in_env, int is_create, int only_create, CACHETABLE cachetable, TOKUTXN txn, FILENUM use_filenum, LSN max_acceptable_lsn) {
    int r;
    assert(use_filenum.fileid != FILENUM_NONE.fileid);
    r = ft_handle_open(t, fname_in_env, is_create, only_create, cachetable,
                 txn, use_filenum, DICTIONARY_ID_NONE, max_acceptable_lsn);
    return r;
}

// Open an ft in normal use.  The FILENUM and dict_id are assigned by the ft_handle_open() function.
// Requires: The multi-operation client lock must be held to prevent a checkpoint from occuring.
int
toku_ft_handle_open(FT_HANDLE t, const char *fname_in_env, int is_create, int only_create, CACHETABLE cachetable, TOKUTXN txn) {
    int r;
    r = ft_handle_open(t, fname_in_env, is_create, only_create, cachetable, txn, FILENUM_NONE, DICTIONARY_ID_NONE, MAX_LSN);
    return r;
}

// clone an ft handle. the cloned handle has a new dict_id but refers to the same fractal tree
int 
toku_ft_handle_clone(FT_HANDLE *cloned_ft_handle, FT_HANDLE ft_handle, TOKUTXN txn) {
    FT_HANDLE result_ft_handle; 
    toku_ft_handle_create(&result_ft_handle);

    // we're cloning, so the handle better have an open ft and open cf
    invariant(ft_handle->ft);
    invariant(ft_handle->ft->cf);

    // inherit the options of the ft whose handle is being cloned.
    toku_ft_handle_inherit_options(result_ft_handle, ft_handle->ft);

    // we can clone the handle by creating a new handle with the same fname
    CACHEFILE cf = ft_handle->ft->cf;
    CACHETABLE ct = toku_cachefile_get_cachetable(cf);
    const char *fname_in_env = toku_cachefile_fname_in_env(cf);
    int r = toku_ft_handle_open(result_ft_handle, fname_in_env, false, false, ct, txn); 
    if (r != 0) {
        toku_ft_handle_close(result_ft_handle);
        result_ft_handle = NULL;
    }
    *cloned_ft_handle = result_ft_handle;
    return r;
}

// Open an ft in normal use.  The FILENUM and dict_id are assigned by the ft_handle_open() function.
int
toku_ft_handle_open_with_dict_id(
    FT_HANDLE t,
    const char *fname_in_env,
    int is_create,
    int only_create,
    CACHETABLE cachetable,
    TOKUTXN txn,
    DICTIONARY_ID use_dictionary_id
    )
{
    int r;
    r = ft_handle_open(
        t,
        fname_in_env,
        is_create,
        only_create,
        cachetable,
        txn,
        FILENUM_NONE,
        use_dictionary_id,
        MAX_LSN
        );
    return r;
}

DICTIONARY_ID
toku_ft_get_dictionary_id(FT_HANDLE ft_handle) {
    FT ft = ft_handle->ft;
    return ft->dict_id;
}

void toku_ft_set_flags(FT_HANDLE ft_handle, unsigned int flags) {
    ft_handle->did_set_flags = true;
    ft_handle->options.flags = flags;
}

void toku_ft_get_flags(FT_HANDLE ft_handle, unsigned int *flags) {
    *flags = ft_handle->options.flags;
}

void toku_ft_get_maximum_advised_key_value_lengths (unsigned int *max_key_len, unsigned int *max_val_len)
// return the maximum advisable key value lengths.  The ft doesn't enforce these.
{
    *max_key_len = 32*1024;
    *max_val_len = 32*1024*1024;
}


void toku_ft_handle_set_nodesize(FT_HANDLE ft_handle, unsigned int nodesize) {
    if (ft_handle->ft) {
        toku_ft_set_nodesize(ft_handle->ft, nodesize);
    }
    else {
        ft_handle->options.nodesize = nodesize;
    }
}

void toku_ft_handle_get_nodesize(FT_HANDLE ft_handle, unsigned int *nodesize) {
    if (ft_handle->ft) {
        toku_ft_get_nodesize(ft_handle->ft, nodesize);
    }
    else {
        *nodesize = ft_handle->options.nodesize;
    }
}

void toku_ft_handle_set_basementnodesize(FT_HANDLE ft_handle, unsigned int basementnodesize) {
    if (ft_handle->ft) {
        toku_ft_set_basementnodesize(ft_handle->ft, basementnodesize);
    }
    else {
        ft_handle->options.basementnodesize = basementnodesize;
    }
}

void toku_ft_handle_get_basementnodesize(FT_HANDLE ft_handle, unsigned int *basementnodesize) {
    if (ft_handle->ft) {
        toku_ft_get_basementnodesize(ft_handle->ft, basementnodesize);
    }
    else {
        *basementnodesize = ft_handle->options.basementnodesize;
    }
}

void toku_ft_set_bt_compare(FT_HANDLE ft_handle, int (*bt_compare)(DB*, const DBT*, const DBT*)) {
    ft_handle->options.compare_fun = bt_compare;
}

void toku_ft_set_redirect_callback(FT_HANDLE ft_handle, on_redirect_callback redir_cb, void* extra) {
    ft_handle->redirect_callback = redir_cb;
    ft_handle->redirect_callback_extra = extra;
}

void toku_ft_set_update(FT_HANDLE ft_handle, ft_update_func update_fun) {
    ft_handle->options.update_fun = update_fun;
}

const toku::comparator &toku_ft_get_comparator(FT_HANDLE ft_handle) {
    invariant_notnull(ft_handle->ft);
    return ft_handle->ft->cmp;
}

static void
ft_remove_handle_ref_callback(FT UU(ft), void *extra) {
    FT_HANDLE CAST_FROM_VOIDP(handle, extra);
    toku_list_remove(&handle->live_ft_handle_link);
}

static void ft_handle_close(FT_HANDLE ft_handle, bool oplsn_valid, LSN oplsn) {
    FT ft = ft_handle->ft;
    // There are error paths in the ft_handle_open that end with ft_handle->ft == nullptr.
    if (ft != nullptr) {
        toku_ft_remove_reference(ft, oplsn_valid, oplsn, ft_remove_handle_ref_callback, ft_handle);
    }
    toku_free(ft_handle);
}

// close an ft handle during normal operation. the underlying ft may or may not close,
// depending if there are still references. an lsn for this close will come from the logger.
void toku_ft_handle_close(FT_HANDLE ft_handle) {
    ft_handle_close(ft_handle, false, ZERO_LSN);
}

// close an ft handle during recovery. the underlying ft must close, and will use the given lsn.
void toku_ft_handle_close_recovery(FT_HANDLE ft_handle, LSN oplsn) {
    // the ft must exist if closing during recovery. error paths during 
    // open for recovery should close handles using toku_ft_handle_close()
    invariant_notnull(ft_handle->ft);
    ft_handle_close(ft_handle, true, oplsn);
}

// TODO: remove this, callers should instead just use toku_ft_handle_close()
int toku_close_ft_handle_nolsn(FT_HANDLE ft_handle, char **UU(error_string)) {
    toku_ft_handle_close(ft_handle);
    return 0;
}

void toku_ft_handle_create(FT_HANDLE *ft_handle_ptr) {
    FT_HANDLE XMALLOC(ft_handle);
    memset(ft_handle, 0, sizeof *ft_handle);
    toku_list_init(&ft_handle->live_ft_handle_link);
    ft_handle->options.flags = 0;
    ft_handle->did_set_flags = false;
    ft_handle->options.nodesize = FT_DEFAULT_NODE_SIZE;
    ft_handle->options.basementnodesize = FT_DEFAULT_BASEMENT_NODE_SIZE;
    ft_handle->options.compression_method = TOKU_DEFAULT_COMPRESSION_METHOD;
    ft_handle->options.fanout = FT_DEFAULT_FANOUT;
    ft_handle->options.compare_fun = toku_builtin_compare_fun;
    ft_handle->options.update_fun = NULL;
    *ft_handle_ptr = ft_handle;
}

/******************************* search ***************************************/

// Return true if this key is within the search bound.  If there is no search bound then the tree search continues.
static bool search_continue(ft_search *search, void *key, uint32_t key_len) {
    bool result = true;
    if (search->direction == FT_SEARCH_LEFT && search->k_bound) {
        FT_HANDLE CAST_FROM_VOIDP(ft_handle, search->context);
        DBT this_key = { .data = key, .size = key_len };
        // search continues if this key <= key bound
        result = (ft_handle->ft->cmp(&this_key, search->k_bound) <= 0);
    }
    return result;
}

static int heaviside_from_search_t(const DBT &kdbt, ft_search &search) {
    int cmp = search.compare(search,
                              search.k ? &kdbt : 0);
    // The search->compare function returns only 0 or 1
    switch (search.direction) {
    case FT_SEARCH_LEFT:   return cmp==0 ? -1 : +1;
    case FT_SEARCH_RIGHT:  return cmp==0 ? +1 : -1; // Because the comparison runs backwards for right searches.
    }
    abort(); return 0;
}

// This is a bottom layer of the search functions.
static int
ft_search_basement_node(
    BASEMENTNODE bn,
    ft_search *search,
    FT_GET_CALLBACK_FUNCTION getf,
    void *getf_v,
    bool *doprefetch,
    FT_CURSOR ftcursor,
    bool can_bulk_fetch
    )
{
    // Now we have to convert from ft_search to the heaviside function with a direction.  What a pain...

    int direction;
    switch (search->direction) {
    case FT_SEARCH_LEFT:   direction = +1; goto ok;
    case FT_SEARCH_RIGHT:  direction = -1; goto ok;
    }
    return EINVAL;  // This return and the goto are a hack to get both compile-time and run-time checking on enum
ok: ;
    uint32_t idx = 0;
    LEAFENTRY le;
    uint32_t keylen;
    void *key;
    int r = bn->data_buffer.find<decltype(*search), heaviside_from_search_t>(
        *search, 
        direction, 
        &le, 
        &key, 
        &keylen, 
        &idx
        );
    if (r!=0) return r;

    if (toku_ft_cursor_is_leaf_mode(ftcursor))
        goto got_a_good_value;        // leaf mode cursors see all leaf entries
    if (le_val_is_del(le, ftcursor->is_snapshot_read, ftcursor->ttxn)) {
        // Provisionally deleted stuff is gone.
        // So we need to scan in the direction to see if we can find something.
        // Every 100 deleted leaf entries check if the leaf's key is within the search bounds.
        for (uint n_deleted = 1; ; n_deleted++) {
            switch (search->direction) {
            case FT_SEARCH_LEFT:
                idx++;
                if (idx >= bn->data_buffer.num_klpairs() ||
                    ((n_deleted % 64) == 0 && !search_continue(search, key, keylen))) {
                    if (ftcursor->interrupt_cb && ftcursor->interrupt_cb(ftcursor->interrupt_cb_extra)) {
                        return TOKUDB_INTERRUPTED;
                    }
                    return DB_NOTFOUND;
                }
                break;
            case FT_SEARCH_RIGHT:
                if (idx == 0) {
                    if (ftcursor->interrupt_cb && ftcursor->interrupt_cb(ftcursor->interrupt_cb_extra)) {
                        return TOKUDB_INTERRUPTED;
                    }
                    return DB_NOTFOUND;
                }
                idx--;
                break;
            default:
                abort();
            }
            r = bn->data_buffer.fetch_klpair(idx, &le, &keylen, &key);
            assert_zero(r); // we just validated the index
            if (!le_val_is_del(le, ftcursor->is_snapshot_read, ftcursor->ttxn)) {
                goto got_a_good_value;
            }
        }
    }
got_a_good_value:
    {
        uint32_t vallen;
        void *val;

        le_extract_val(le, toku_ft_cursor_is_leaf_mode(ftcursor),
                       ftcursor->is_snapshot_read, ftcursor->ttxn,
                       &vallen, &val);
        r = toku_ft_cursor_check_restricted_range(ftcursor, key, keylen);
        if (r == 0) {
            r = getf(keylen, key, vallen, val, getf_v, false);
        }
        if (r == 0 || r == TOKUDB_CURSOR_CONTINUE) {
            // 
            // IMPORTANT: bulk fetch CANNOT go past the current basement node,
            // because there is no guarantee that messages have been applied
            // to other basement nodes, as part of #5770
            //
            if (r == TOKUDB_CURSOR_CONTINUE && can_bulk_fetch) {
                r = toku_ft_cursor_shortcut(ftcursor, direction, idx, &bn->data_buffer,
                                            getf, getf_v, &keylen, &key, &vallen, &val);
            }

            toku_destroy_dbt(&ftcursor->key);
            toku_destroy_dbt(&ftcursor->val);
            if (!ftcursor->is_temporary) {
                toku_memdup_dbt(&ftcursor->key, key, keylen);
                toku_memdup_dbt(&ftcursor->val, val, vallen);
            }
            // The search was successful.  Prefetching can continue.
            *doprefetch = true;
        }
    }
    if (r == TOKUDB_CURSOR_CONTINUE) r = 0;
    return r;
}

static int
ft_search_node (
    FT_HANDLE ft_handle,
    FTNODE node,
    ft_search *search,
    int child_to_search,
    FT_GET_CALLBACK_FUNCTION getf,
    void *getf_v,
    bool *doprefetch,
    FT_CURSOR ftcursor,
    UNLOCKERS unlockers,
    ANCESTORS,
    const pivot_bounds &bounds,
    bool can_bulk_fetch
    );

static int
ftnode_fetch_callback_and_free_bfe(CACHEFILE cf, PAIR p, int fd, BLOCKNUM blocknum, uint32_t fullhash, void **ftnode_pv, void** UU(disk_data), PAIR_ATTR *sizep, int *dirtyp, void *extraargs)
{
    int r = toku_ftnode_fetch_callback(cf, p, fd, blocknum, fullhash, ftnode_pv, disk_data, sizep, dirtyp, extraargs);
    ftnode_fetch_extra *CAST_FROM_VOIDP(bfe, extraargs);
    bfe->destroy();
    toku_free(bfe);
    return r;
}

static int
ftnode_pf_callback_and_free_bfe(void *ftnode_pv, void* disk_data, void *read_extraargs, int fd, PAIR_ATTR *sizep)
{
    int r = toku_ftnode_pf_callback(ftnode_pv, disk_data, read_extraargs, fd, sizep);
    ftnode_fetch_extra *CAST_FROM_VOIDP(bfe, read_extraargs);
    bfe->destroy();
    toku_free(bfe);
    return r;
}

CACHETABLE_WRITE_CALLBACK get_write_callbacks_for_node(FT ft) {
    CACHETABLE_WRITE_CALLBACK wc;
    wc.flush_callback = toku_ftnode_flush_callback;
    wc.pe_est_callback = toku_ftnode_pe_est_callback;
    wc.pe_callback = toku_ftnode_pe_callback;
    wc.cleaner_callback = toku_ftnode_cleaner_callback;
    wc.clone_callback = toku_ftnode_clone_callback;
    wc.checkpoint_complete_callback = toku_ftnode_checkpoint_complete_callback;
    wc.write_extraargs = ft;
    return wc;
}

static void
ft_node_maybe_prefetch(FT_HANDLE ft_handle, FTNODE node, int childnum, FT_CURSOR ftcursor, bool *doprefetch) {
    // the number of nodes to prefetch
    const int num_nodes_to_prefetch = 1;

    // if we want to prefetch in the tree
    // then prefetch the next children if there are any
    if (*doprefetch && toku_ft_cursor_prefetching(ftcursor) && !ftcursor->disable_prefetching) {
        int rc = ft_cursor_rightmost_child_wanted(ftcursor, ft_handle, node);
        for (int i = childnum + 1; (i <= childnum + num_nodes_to_prefetch) && (i <= rc); i++) {
            BLOCKNUM nextchildblocknum = BP_BLOCKNUM(node, i);
            uint32_t nextfullhash = compute_child_fullhash(ft_handle->ft->cf, node, i);
            ftnode_fetch_extra *XCALLOC(bfe);
            bfe->create_for_prefetch(ft_handle->ft, ftcursor);
            bool doing_prefetch = false;
            toku_cachefile_prefetch(
                ft_handle->ft->cf,
                nextchildblocknum,
                nextfullhash,
                get_write_callbacks_for_node(ft_handle->ft),
                ftnode_fetch_callback_and_free_bfe,
                toku_ftnode_pf_req_callback,
                ftnode_pf_callback_and_free_bfe,
                bfe,
                &doing_prefetch
                );
            if (!doing_prefetch) {
                bfe->destroy();
                toku_free(bfe);
            }
            *doprefetch = false;
        }
    }
}

struct unlock_ftnode_extra {
    FT_HANDLE ft_handle;
    FTNODE node;
    bool msgs_applied;
};

// When this is called, the cachetable lock is held
static void
unlock_ftnode_fun (void *v) {
    struct unlock_ftnode_extra *x = NULL;
    CAST_FROM_VOIDP(x, v);
    FT_HANDLE ft_handle = x->ft_handle;
    FTNODE node = x->node;
    // CT lock is held
    int r = toku_cachetable_unpin_ct_prelocked_no_flush(
        ft_handle->ft->cf,
        node->ct_pair,
        (enum cachetable_dirty) node->dirty,
        x->msgs_applied ? make_ftnode_pair_attr(node) : make_invalid_pair_attr()
        );
    assert_zero(r);
}

/* search in a node's child */
static int
ft_search_child(FT_HANDLE ft_handle, FTNODE node, int childnum, ft_search *search, FT_GET_CALLBACK_FUNCTION getf, void *getf_v, bool *doprefetch, FT_CURSOR ftcursor, UNLOCKERS unlockers,
                 ANCESTORS ancestors, const pivot_bounds &bounds, bool can_bulk_fetch)
// Effect: Search in a node's child.  Searches are read-only now (at least as far as the hardcopy is concerned).
{
    struct ancestors next_ancestors = {node, childnum, ancestors};

    BLOCKNUM childblocknum = BP_BLOCKNUM(node,childnum);
    uint32_t fullhash = compute_child_fullhash(ft_handle->ft->cf, node, childnum);
    FTNODE childnode = nullptr;

    // If the current node's height is greater than 1, then its child is an internal node.
    // Therefore, to warm the cache better (#5798), we want to read all the partitions off disk in one shot.
    bool read_all_partitions = node->height > 1;
    ftnode_fetch_extra bfe;
    bfe.create_for_subset_read(
        ft_handle->ft,
        search,
        &ftcursor->range_lock_left_key,
        &ftcursor->range_lock_right_key,
        ftcursor->left_is_neg_infty,
        ftcursor->right_is_pos_infty,
        ftcursor->disable_prefetching,
        read_all_partitions
        );
    bool msgs_applied = false;
    {
        int rr = toku_pin_ftnode_for_query(ft_handle, childblocknum, fullhash,
                                         unlockers,
                                         &next_ancestors, bounds,
                                         &bfe,
                                         true,
                                         &childnode,
                                         &msgs_applied);
        if (rr==TOKUDB_TRY_AGAIN) {
            return rr;
        }
        invariant_zero(rr);
    }

    struct unlock_ftnode_extra unlock_extra = { ft_handle, childnode, msgs_applied };
    struct unlockers next_unlockers = { true, unlock_ftnode_fun, (void *) &unlock_extra, unlockers };
    int r = ft_search_node(ft_handle, childnode, search, bfe.child_to_read, getf, getf_v, doprefetch, ftcursor, &next_unlockers, &next_ancestors, bounds, can_bulk_fetch);
    if (r!=TOKUDB_TRY_AGAIN) {
        // maybe prefetch the next child
        if (r == 0 && node->height == 1) {
            ft_node_maybe_prefetch(ft_handle, node, childnum, ftcursor, doprefetch);
        }

        assert(next_unlockers.locked);
        if (msgs_applied) {
            toku_unpin_ftnode(ft_handle->ft, childnode);
        }
        else {
            toku_unpin_ftnode_read_only(ft_handle->ft, childnode);
        }
    } else {
        // try again.

        // there are two cases where we get TOKUDB_TRY_AGAIN
        //  case 1 is when some later call to toku_pin_ftnode returned
        //  that value and unpinned all the nodes anyway. case 2
        //  is when ft_search_node had to stop its search because
        //  some piece of a node that it needed was not in memory. In this case,
        //  the node was not unpinned, so we unpin it here
        if (next_unlockers.locked) {
            if (msgs_applied) {
                toku_unpin_ftnode(ft_handle->ft, childnode);
            }
            else {
                toku_unpin_ftnode_read_only(ft_handle->ft, childnode);
            }
        }
    }

    return r;
}

static inline int
search_which_child_cmp_with_bound(const toku::comparator &cmp, FTNODE node, int childnum,
                                  ft_search *search, DBT *dbt) {
    return cmp(toku_copyref_dbt(dbt, node->pivotkeys.get_pivot(childnum)), &search->pivot_bound);
}

int
toku_ft_search_which_child(const toku::comparator &cmp, FTNODE node, ft_search *search) {
    if (node->n_children <= 1) return 0;

    DBT pivotkey;
    toku_init_dbt(&pivotkey);
    int lo = 0;
    int hi = node->n_children - 1;
    int mi;
    while (lo < hi) {
        mi = (lo + hi) / 2;
        node->pivotkeys.fill_pivot(mi, &pivotkey);
        // search->compare is really strange, and only works well with a
        // linear search, it makes binary search a pita.
        //
        // if you are searching left to right, it returns
        //   "0" for pivots that are < the target, and
        //   "1" for pivots that are >= the target
        // if you are searching right to left, it's the opposite.
        //
        // so if we're searching from the left and search->compare says
        // "1", we want to go left from here, if it says "0" we want to go
        // right.  searching from the right does the opposite.
        bool c = search->compare(*search, &pivotkey);
        if (((search->direction == FT_SEARCH_LEFT) && c) ||
            ((search->direction == FT_SEARCH_RIGHT) && !c)) {
            hi = mi;
        } else {
            assert(((search->direction == FT_SEARCH_LEFT) && !c) ||
                   ((search->direction == FT_SEARCH_RIGHT) && c));
            lo = mi + 1;
        }
    }
    // ready to return something, if the pivot is bounded, we have to move
    // over a bit to get away from what we've already searched
    if (search->pivot_bound.data != nullptr) {
        if (search->direction == FT_SEARCH_LEFT) {
            while (lo < node->n_children - 1 &&
                   search_which_child_cmp_with_bound(cmp, node, lo, search, &pivotkey) <= 0) {
                // searching left to right, if the comparison says the
                // current pivot (lo) is left of or equal to our bound,
                // don't search that child again
                lo++;
            }
        } else {
            while (lo > 0 &&
                   search_which_child_cmp_with_bound(cmp, node, lo - 1, search, &pivotkey) >= 0) {
                // searching right to left, same argument as just above
                // (but we had to pass lo - 1 because the pivot between lo
                // and the thing just less than it is at that position in
                // the pivot keys array)
                lo--;
            }
        }
    }
    return lo;
}

static void
maybe_search_save_bound(
    FTNODE node,
    int child_searched,
    ft_search *search)
{
    int p = (search->direction == FT_SEARCH_LEFT) ? child_searched : child_searched - 1;
    if (p >= 0 && p < node->n_children-1) {
        toku_destroy_dbt(&search->pivot_bound);
        toku_clone_dbt(&search->pivot_bound, node->pivotkeys.get_pivot(p));
    }
}

// Returns true if there are still children left to search in this node within the search bound (if any).
static bool search_try_again(FTNODE node, int child_to_search, ft_search *search) {
    bool try_again = false;
    if (search->direction == FT_SEARCH_LEFT) {
        if (child_to_search < node->n_children-1) {
            try_again = true;
            // if there is a search bound and the bound is within the search pivot then continue the search
            if (search->k_bound) {
                FT_HANDLE CAST_FROM_VOIDP(ft_handle, search->context);
                try_again = (ft_handle->ft->cmp(search->k_bound, &search->pivot_bound) > 0);
            }
        }
    } else if (search->direction == FT_SEARCH_RIGHT) {
        if (child_to_search > 0)
            try_again = true;
    }
    return try_again;
}

static int
ft_search_node(
    FT_HANDLE ft_handle,
    FTNODE node,
    ft_search *search,
    int child_to_search,
    FT_GET_CALLBACK_FUNCTION getf,
    void *getf_v,
    bool *doprefetch,
    FT_CURSOR ftcursor,
    UNLOCKERS unlockers,
    ANCESTORS ancestors,
    const pivot_bounds &bounds,
    bool can_bulk_fetch
    )
{
    int r = 0;
    // assert that we got a valid child_to_search
    invariant(child_to_search >= 0);
    invariant(child_to_search < node->n_children);
    //
    // At this point, we must have the necessary partition available to continue the search
    //
    assert(BP_STATE(node,child_to_search) == PT_AVAIL);
    const pivot_bounds next_bounds = bounds.next_bounds(node, child_to_search);
    if (node->height > 0) {
        r = ft_search_child(
            ft_handle,
            node,
            child_to_search,
            search,
            getf,
            getf_v,
            doprefetch,
            ftcursor,
            unlockers,
            ancestors,
            next_bounds,
            can_bulk_fetch
            );
    }
    else {
        r = ft_search_basement_node(
            BLB(node, child_to_search),
            search,
            getf,
            getf_v,
            doprefetch,
            ftcursor,
            can_bulk_fetch
            );
    }
    if (r == 0) {
        return r; //Success
    }

    if (r != DB_NOTFOUND) {
        return r; //Error (or message to quit early, such as TOKUDB_FOUND_BUT_REJECTED or TOKUDB_TRY_AGAIN)
    }
    // not really necessary, just put this here so that reading the
    // code becomes simpler. The point is at this point in the code,
    // we know that we got DB_NOTFOUND and we have to continue
    assert(r == DB_NOTFOUND);
    // we have a new pivotkey
    if (node->height == 0) {
        // when we run off the end of a basement, try to lock the range up to the pivot. solves #3529
        const DBT *pivot = search->direction == FT_SEARCH_LEFT ? next_bounds.ubi() : // left -> right
                                                                 next_bounds.lbe();  // right -> left
        if (pivot != nullptr) {
            int rr = getf(pivot->size, pivot->data, 0, nullptr, getf_v, true);
            if (rr != 0) {
                return rr; // lock was not granted
            }
        }
    }

    // If we got a DB_NOTFOUND then we have to search the next record.        Possibly everything present is not visible.
    // This way of doing DB_NOTFOUND is a kludge, and ought to be simplified.  Something like this is needed for DB_NEXT, but
    //        for point queries, it's overkill.  If we got a DB_NOTFOUND on a point query then we should just stop looking.
    // When releasing locks on I/O we must not search the same subtree again, or we won't be guaranteed to make forward progress.
    // If we got a DB_NOTFOUND, then the pivot is too small if searching from left to right (too large if searching from right to left).
    // So save the pivot key in the search object.
    maybe_search_save_bound(node, child_to_search, search);

    // as part of #5770, if we can continue searching,
    // we MUST return TOKUDB_TRY_AGAIN,
    // because there is no guarantee that messages have been applied
    // on any other path.
    if (search_try_again(node, child_to_search, search)) {
        r = TOKUDB_TRY_AGAIN;
    }

    return r;
}

int toku_ft_search(FT_HANDLE ft_handle, ft_search *search, FT_GET_CALLBACK_FUNCTION getf, void *getf_v, FT_CURSOR ftcursor, bool can_bulk_fetch)
// Effect: Perform a search.  Associate cursor with a leaf if possible.
// All searches are performed through this function.
{
    int r;
    uint trycount = 0;     // How many tries did it take to get the result?
    FT ft = ft_handle->ft;

    toku::context search_ctx(CTX_SEARCH);

try_again:

    trycount++;

    //
    // Here is how searches work
    // At a high level, we descend down the tree, using the search parameter
    // to guide us towards where to look. But the search parameter is not
    // used here to determine which child of a node to read (regardless
    // of whether that child is another node or a basement node)
    // The search parameter is used while we are pinning the node into
    // memory, because that is when the system needs to ensure that
    // the appropriate partition of the child we are using is in memory.
    // So, here are the steps for a search (and this applies to this function
    // as well as ft_search_child:
    //  - Take the search parameter, and create a ftnode_fetch_extra, that will be used by toku_pin_ftnode
    //  - Call toku_pin_ftnode with the bfe as the extra for the fetch callback (in case the node is not at all in memory)
    //       and the partial fetch callback (in case the node is perhaps partially in memory) to the fetch the node
    //  - This eventually calls either toku_ftnode_fetch_callback or  toku_ftnode_pf_req_callback depending on whether the node is in
    //     memory at all or not.
    //  - Within these functions, the "ft_search search" parameter is used to evaluate which child the search is interested in.
    //     If the node is not in memory at all, toku_ftnode_fetch_callback will read the node and decompress only the partition for the
    //     relevant child, be it a message buffer or basement node. If the node is in memory, then toku_ftnode_pf_req_callback
    //     will tell the cachetable that a partial fetch is required if and only if the relevant child is not in memory. If the relevant child
    //     is not in memory, then toku_ftnode_pf_callback is called to fetch the partition.
    //  - These functions set bfe->child_to_read so that the search code does not need to reevaluate it.
    //  - Just to reiterate, all of the last item happens within toku_ftnode_pin(_holding_lock)
    //  - At this point, toku_ftnode_pin_holding_lock has returned, with bfe.child_to_read set,
    //  - ft_search_node is called, assuming that the node and its relevant partition are in memory.
    //
    ftnode_fetch_extra bfe;
    bfe.create_for_subset_read(
        ft,
        search,
        &ftcursor->range_lock_left_key,
        &ftcursor->range_lock_right_key,
        ftcursor->left_is_neg_infty,
        ftcursor->right_is_pos_infty,
        ftcursor->disable_prefetching,
        true // We may as well always read the whole root into memory, if it's a leaf node it's a tiny tree anyway.
        );
    FTNODE node = NULL;
    {
        uint32_t fullhash;
        CACHEKEY root_key;
        toku_calculate_root_offset_pointer(ft, &root_key, &fullhash);
        toku_pin_ftnode(
            ft,
            root_key,
            fullhash,
            &bfe,
            PL_READ, // may_modify_node set to false, because root cannot change during search
            &node,
            true
            );
    }

    uint tree_height = node->height + 1;  // How high is the tree?  This is the height of the root node plus one (leaf is at height 0).


    struct unlock_ftnode_extra unlock_extra   = {ft_handle,node,false};
    struct unlockers                unlockers      = {true, unlock_ftnode_fun, (void*)&unlock_extra, (UNLOCKERS)NULL};

    {
        bool doprefetch = false;
        //static int counter = 0;         counter++;
        r = ft_search_node(ft_handle, node, search, bfe.child_to_read, getf, getf_v, &doprefetch, ftcursor, &unlockers, (ANCESTORS)NULL, pivot_bounds::infinite_bounds(), can_bulk_fetch);
        if (r==TOKUDB_TRY_AGAIN) {
            // there are two cases where we get TOKUDB_TRY_AGAIN
            //  case 1 is when some later call to toku_pin_ftnode returned
            //  that value and unpinned all the nodes anyway. case 2
            //  is when ft_search_node had to stop its search because
            //  some piece of a node that it needed was not in memory.
            //  In this case, the node was not unpinned, so we unpin it here
            if (unlockers.locked) {
                toku_unpin_ftnode_read_only(ft_handle->ft, node);
            }
            goto try_again;
        } else {
            assert(unlockers.locked);
        }
    }

    assert(unlockers.locked);
    toku_unpin_ftnode_read_only(ft_handle->ft, node);


    //Heaviside function (+direction) queries define only a lower or upper
    //bound.  Some queries require both an upper and lower bound.
    //They do this by wrapping the FT_GET_CALLBACK_FUNCTION with another
    //test that checks for the other bound.  If the other bound fails,
    //it returns TOKUDB_FOUND_BUT_REJECTED which means not found, but
    //stop searching immediately, as opposed to DB_NOTFOUND
    //which can mean not found, but keep looking in another leaf.
    if (r==TOKUDB_FOUND_BUT_REJECTED) r = DB_NOTFOUND;
    else if (r==DB_NOTFOUND) {
        //We truly did not find an answer to the query.
        //Therefore, the FT_GET_CALLBACK_FUNCTION has NOT been called.
        //The contract specifies that the callback function must be called
        //for 'r= (0|DB_NOTFOUND|TOKUDB_FOUND_BUT_REJECTED)'
        //TODO: #1378 This is not the ultimate location of this call to the
        //callback.  It is surely wrong for node-level locking, and probably
        //wrong for the STRADDLE callback for heaviside function(two sets of key/vals)
        int r2 = getf(0,NULL, 0,NULL, getf_v, false);
        if (r2!=0) r = r2;
    }
    {   // accounting (to detect and measure thrashing)
        uint retrycount = trycount - 1;         // how many retries were needed?
        if (retrycount) {
            STATUS_INC(FT_TOTAL_RETRIES, retrycount);
        }
        if (retrycount > tree_height) {         // if at least one node was read from disk more than once
            STATUS_INC(FT_SEARCH_TRIES_GT_HEIGHT, 1);
            if (retrycount > (tree_height+3))
                STATUS_INC(FT_SEARCH_TRIES_GT_HEIGHTPLUS3, 1);
        }
    }
    return r;
}

/* ********************************* delete **************************************/
static int
getf_nothing (uint32_t UU(keylen), const void *UU(key), uint32_t UU(vallen), const void *UU(val), void *UU(pair_v), bool UU(lock_only)) {
    return 0;
}

int toku_ft_cursor_delete(FT_CURSOR cursor, int flags, TOKUTXN txn) {
    int r;

    int unchecked_flags = flags;
    bool error_if_missing = (bool) !(flags&DB_DELETE_ANY);
    unchecked_flags &= ~DB_DELETE_ANY;
    if (unchecked_flags!=0) r = EINVAL;
    else if (toku_ft_cursor_not_set(cursor)) r = EINVAL;
    else {
        r = 0;
        if (error_if_missing) {
            r = toku_ft_cursor_current(cursor, DB_CURRENT, getf_nothing, NULL);
        }
        if (r == 0) {
            toku_ft_delete(cursor->ft_handle, &cursor->key, txn);
        }
    }
    return r;
}

/* ********************* keyrange ************************ */

struct keyrange_compare_s {
    FT ft;
    const DBT *key;
};

// TODO: Remove me, I'm boring
static int keyrange_compare(DBT const &kdbt, const struct keyrange_compare_s &s) {
    return s.ft->cmp(&kdbt, s.key);
}

static void
keysrange_in_leaf_partition (FT_HANDLE ft_handle, FTNODE node,
                             DBT* key_left, DBT* key_right,
                             int left_child_number, int right_child_number, uint64_t estimated_num_rows,
                             uint64_t *less, uint64_t* equal_left, uint64_t* middle,
                             uint64_t* equal_right, uint64_t* greater, bool* single_basement_node)
// If the partition is in main memory then estimate the number
// Treat key_left == NULL as negative infinity
// Treat key_right == NULL as positive infinity
{
    paranoid_invariant(node->height == 0); // we are in a leaf
    paranoid_invariant(!(key_left == NULL && key_right != NULL));
    paranoid_invariant(left_child_number <= right_child_number);
    bool single_basement = left_child_number == right_child_number;
    paranoid_invariant(!single_basement || (BP_STATE(node, left_child_number) == PT_AVAIL));
    if (BP_STATE(node, left_child_number) == PT_AVAIL) {
        int r;
        // The partition is in main memory then get an exact count.
        struct keyrange_compare_s s_left = {ft_handle->ft, key_left};
        BASEMENTNODE bn = BLB(node, left_child_number);
        uint32_t idx_left = 0;
        // if key_left is NULL then set r==-1 and idx==0.
        r = key_left ? bn->data_buffer.find_zero<decltype(s_left), keyrange_compare>(s_left, nullptr, nullptr, nullptr, &idx_left) : -1;
        *less = idx_left;
        *equal_left = (r==0) ? 1 : 0;

        uint32_t size = bn->data_buffer.num_klpairs();
        uint32_t idx_right = size;
        r = -1;
        if (single_basement && key_right) {
            struct keyrange_compare_s s_right = {ft_handle->ft, key_right};
            r = bn->data_buffer.find_zero<decltype(s_right), keyrange_compare>(s_right, nullptr, nullptr, nullptr, &idx_right);
        }
        *middle = idx_right - idx_left - *equal_left;
        *equal_right = (r==0) ? 1 : 0;
        *greater = size - idx_right - *equal_right;
    } else {
        paranoid_invariant(!single_basement);
        uint32_t idx_left = estimated_num_rows / 2;
        if (!key_left) {
            //Both nullptr, assume key_left belongs before leftmost entry, key_right belongs after rightmost entry
            idx_left = 0;
            paranoid_invariant(!key_right);
        }
        // Assume idx_left and idx_right point to where key_left and key_right belong, (but are not there).
        *less = idx_left;
        *equal_left = 0;
        *middle = estimated_num_rows - idx_left;
        *equal_right = 0;
        *greater = 0;
    }
    *single_basement_node = single_basement;
}

static int
toku_ft_keysrange_internal (FT_HANDLE ft_handle, FTNODE node,
                            DBT* key_left, DBT* key_right, bool may_find_right,
                            uint64_t* less, uint64_t* equal_left, uint64_t* middle,
                            uint64_t* equal_right, uint64_t* greater, bool* single_basement_node,
                            uint64_t estimated_num_rows,
                            ftnode_fetch_extra *min_bfe, // set up to read a minimal read.
                            ftnode_fetch_extra *match_bfe, // set up to read a basement node iff both keys in it
                            struct unlockers *unlockers, ANCESTORS ancestors, const pivot_bounds &bounds)
// Implementation note: Assign values to less, equal, and greater, and then on the way out (returning up the stack) we add more values in.
{
    int r = 0;
    // if KEY is NULL then use the leftmost key.
    int left_child_number = key_left ? toku_ftnode_which_child (node, key_left, ft_handle->ft->cmp) : 0;
    int right_child_number = node->n_children;  // Sentinel that does not equal left_child_number.
    if (may_find_right) {
        right_child_number = key_right ? toku_ftnode_which_child (node, key_right, ft_handle->ft->cmp) : node->n_children - 1;
    }

    uint64_t rows_per_child = estimated_num_rows / node->n_children;
    if (node->height == 0) {
        keysrange_in_leaf_partition(ft_handle, node, key_left, key_right, left_child_number, right_child_number,
                                    rows_per_child, less, equal_left, middle, equal_right, greater, single_basement_node);

        *less    += rows_per_child * left_child_number;
        if (*single_basement_node) {
            *greater += rows_per_child * (node->n_children - left_child_number - 1);
        } else {
            *middle += rows_per_child * (node->n_children - left_child_number - 1);
        }
    } else {
        // do the child.
        struct ancestors next_ancestors = {node, left_child_number, ancestors};
        BLOCKNUM childblocknum = BP_BLOCKNUM(node, left_child_number);
        uint32_t fullhash = compute_child_fullhash(ft_handle->ft->cf, node, left_child_number);
        FTNODE childnode;
        bool msgs_applied = false;
        bool child_may_find_right = may_find_right && left_child_number == right_child_number;
        r = toku_pin_ftnode_for_query(
            ft_handle,
            childblocknum,
            fullhash,
            unlockers,
            &next_ancestors,
            bounds,
            child_may_find_right ? match_bfe : min_bfe,
            false,
            &childnode,
            &msgs_applied
            );
        paranoid_invariant(!msgs_applied);
        if (r != TOKUDB_TRY_AGAIN) {
            assert_zero(r);

            struct unlock_ftnode_extra unlock_extra   = {ft_handle,childnode,false};
            struct unlockers next_unlockers = {true, unlock_ftnode_fun, (void*)&unlock_extra, unlockers};
            const pivot_bounds next_bounds = bounds.next_bounds(node, left_child_number);

            r = toku_ft_keysrange_internal(ft_handle, childnode, key_left, key_right, child_may_find_right,
                                           less, equal_left, middle, equal_right, greater, single_basement_node,
                                           rows_per_child, min_bfe, match_bfe, &next_unlockers, &next_ancestors, next_bounds);
            if (r != TOKUDB_TRY_AGAIN) {
                assert_zero(r);

                *less    += rows_per_child * left_child_number;
                if (*single_basement_node) {
                    *greater += rows_per_child * (node->n_children - left_child_number - 1);
                } else {
                    *middle += rows_per_child * (node->n_children - left_child_number - 1);
                }

                assert(unlockers->locked);
                toku_unpin_ftnode_read_only(ft_handle->ft, childnode);
            }
        }
    }
    return r;
}

void toku_ft_keysrange(FT_HANDLE ft_handle, DBT* key_left, DBT* key_right, uint64_t *less_p, uint64_t* equal_left_p, uint64_t* middle_p, uint64_t* equal_right_p, uint64_t* greater_p, bool* middle_3_exact_p)
// Effect: Return an estimate  of the number of keys to the left, the number equal (to left key), number between keys, number equal to right key, and the number to the right of both keys.
//   The values are an estimate.
//   If you perform a keyrange on two keys that are in the same basement, equal_less, middle, and equal_right will be exact.
//   4184: What to do with a NULL key?
//   key_left==NULL is treated as -infinity
//   key_right==NULL is treated as +infinity
//   If KEY is NULL then the system picks an arbitrary key and returns it.
//   key_right can be non-null only if key_left is non-null;
{
    if (!key_left && key_right) {
        // Simplify internals by only supporting key_right != null when key_left != null
        // If key_right != null and key_left == null, then swap them and fix up numbers.
        uint64_t less = 0, equal_left = 0, middle = 0, equal_right = 0, greater = 0;
        toku_ft_keysrange(ft_handle, key_right, nullptr, &less, &equal_left, &middle, &equal_right, &greater, middle_3_exact_p);
        *less_p = 0;
        *equal_left_p = 0;
        *middle_p = less;
        *equal_right_p = equal_left;
        *greater_p = middle;
        invariant_zero(equal_right);
        invariant_zero(greater);
        return;
    }
    paranoid_invariant(!(!key_left && key_right));
    ftnode_fetch_extra min_bfe;
    ftnode_fetch_extra match_bfe;
    min_bfe.create_for_min_read(ft_handle->ft); // read pivot keys but not message buffers
    match_bfe.create_for_keymatch(ft_handle->ft, key_left, key_right, false, false); // read basement node only if both keys in it.
try_again:
    {
        uint64_t less = 0, equal_left = 0, middle = 0, equal_right = 0, greater = 0;
        bool single_basement_node = false;
        FTNODE node = NULL;
        {
            uint32_t fullhash;
            CACHEKEY root_key;
            toku_calculate_root_offset_pointer(ft_handle->ft, &root_key, &fullhash);
            toku_pin_ftnode(
                ft_handle->ft,
                root_key,
                fullhash,
                &match_bfe,
                PL_READ, // may_modify_node, cannot change root during keyrange
                &node,
                true
                );
        }

        struct unlock_ftnode_extra unlock_extra = {ft_handle,node,false};
        struct unlockers unlockers = {true, unlock_ftnode_fun, (void*)&unlock_extra, (UNLOCKERS)NULL};

        {
            int r;
            int64_t numrows = ft_handle->ft->in_memory_stats.numrows;
            if (numrows < 0)
                numrows = 0;  // prevent appearance of a negative number
            r = toku_ft_keysrange_internal (ft_handle, node, key_left, key_right, true,
                                            &less, &equal_left, &middle, &equal_right, &greater,
                                            &single_basement_node, numrows,
                                            &min_bfe, &match_bfe, &unlockers, (ANCESTORS)NULL, pivot_bounds::infinite_bounds());
            assert(r == 0 || r == TOKUDB_TRY_AGAIN);
            if (r == TOKUDB_TRY_AGAIN) {
                assert(!unlockers.locked);
                goto try_again;
            }
            // May need to do a second query.
            if (!single_basement_node && key_right != nullptr) {
                // "greater" is stored in "middle"
                invariant_zero(equal_right);
                invariant_zero(greater);
                uint64_t less2 = 0, equal_left2 = 0, middle2 = 0, equal_right2 = 0, greater2 = 0;
                bool ignore;
                r = toku_ft_keysrange_internal (ft_handle, node, key_right, nullptr, false,
                                                &less2, &equal_left2, &middle2, &equal_right2, &greater2,
                                                &ignore, numrows,
                                                &min_bfe, &match_bfe, &unlockers, (ANCESTORS)nullptr, pivot_bounds::infinite_bounds());
                assert(r == 0 || r == TOKUDB_TRY_AGAIN);
                if (r == TOKUDB_TRY_AGAIN) {
                    assert(!unlockers.locked);
                    goto try_again;
                }
                invariant_zero(equal_right2);
                invariant_zero(greater2);
                // Update numbers.
                // less is already correct.
                // equal_left is already correct.

                // "middle" currently holds everything greater than left_key in first query
                // 'middle2' currently holds everything greater than right_key in second query
                // 'equal_left2' is how many match right_key

                // Prevent underflow.
                if (middle >= equal_left2 + middle2) {
                    middle -= equal_left2 + middle2;
                } else {
                    middle = 0;
                }
                equal_right = equal_left2;
                greater = middle2;
            }
        }
        assert(unlockers.locked);
        toku_unpin_ftnode_read_only(ft_handle->ft, node);
        if (!key_right) {
            paranoid_invariant_zero(equal_right);
            paranoid_invariant_zero(greater);
        }
        if (!key_left) {
            paranoid_invariant_zero(less);
            paranoid_invariant_zero(equal_left);
        }
        *less_p        = less;
        *equal_left_p  = equal_left;
        *middle_p      = middle;
        *equal_right_p = equal_right;
        *greater_p     = greater;
        *middle_3_exact_p = single_basement_node;
    }
}

struct get_key_after_bytes_iterate_extra {
    uint64_t skip_len;
    uint64_t *skipped;
    void (*callback)(const DBT *, uint64_t, void *);
    void *cb_extra;
};

static int get_key_after_bytes_iterate(const void* key, const uint32_t keylen, const LEAFENTRY & le, const uint32_t UU(idx), struct get_key_after_bytes_iterate_extra * const e) {
    // only checking the latest val, mvcc will make this inaccurate
    uint64_t pairlen = keylen + le_latest_vallen(le);
    if (*e->skipped + pairlen > e->skip_len) {
        // found our key!
        DBT end_key;
        toku_fill_dbt(&end_key, key, keylen);
        e->callback(&end_key, *e->skipped, e->cb_extra);
        return 1;
    } else {
        *e->skipped += pairlen;
        return 0;
    }
}

static int get_key_after_bytes_in_basementnode(FT ft, BASEMENTNODE bn, const DBT *start_key, uint64_t skip_len, void (*callback)(const DBT *, uint64_t, void *), void *cb_extra, uint64_t *skipped) {
    int r;
    uint32_t idx_left = 0;
    if (start_key != nullptr) {
        struct keyrange_compare_s cmp = {ft, start_key};
        r = bn->data_buffer.find_zero<decltype(cmp), keyrange_compare>(cmp, nullptr, nullptr, nullptr, &idx_left);
        assert(r == 0 || r == DB_NOTFOUND);
    }
    struct get_key_after_bytes_iterate_extra iter_extra = {skip_len, skipped, callback, cb_extra};
    r = bn->data_buffer.iterate_on_range<get_key_after_bytes_iterate_extra, get_key_after_bytes_iterate>(idx_left, bn->data_buffer.num_klpairs(), &iter_extra);

    // Invert the sense of r == 0 (meaning the iterate finished, which means we didn't find what we wanted)
    if (r == 1) {
        r = 0;
    } else {
        r = DB_NOTFOUND;
    }
    return r;
}

static int get_key_after_bytes_in_subtree(FT_HANDLE ft_h, FT ft, FTNODE node, UNLOCKERS unlockers, ANCESTORS ancestors, const pivot_bounds &bounds, ftnode_fetch_extra *bfe, ft_search *search, uint64_t subtree_bytes, const DBT *start_key, uint64_t skip_len, void (*callback)(const DBT *, uint64_t, void *), void *cb_extra, uint64_t *skipped);

static int get_key_after_bytes_in_child(FT_HANDLE ft_h, FT ft, FTNODE node, UNLOCKERS unlockers, ANCESTORS ancestors, const pivot_bounds &bounds, ftnode_fetch_extra *bfe, ft_search *search, int childnum, uint64_t subtree_bytes, const DBT *start_key, uint64_t skip_len, void (*callback)(const DBT *, uint64_t, void *), void *cb_extra, uint64_t *skipped) {
    int r;
    struct ancestors next_ancestors = {node, childnum, ancestors};
    BLOCKNUM childblocknum = BP_BLOCKNUM(node, childnum);
    uint32_t fullhash = compute_child_fullhash(ft->cf, node, childnum);
    FTNODE child;
    bool msgs_applied = false;
    r = toku_pin_ftnode_for_query(ft_h, childblocknum, fullhash, unlockers, &next_ancestors, bounds, bfe, false, &child, &msgs_applied);
    paranoid_invariant(!msgs_applied);
    if (r == TOKUDB_TRY_AGAIN) {
        return r;
    }
    assert_zero(r);
    struct unlock_ftnode_extra unlock_extra = {ft_h, child, false};
    struct unlockers next_unlockers = {true, unlock_ftnode_fun, (void *) &unlock_extra, unlockers};
    const pivot_bounds next_bounds = bounds.next_bounds(node, childnum);
    return get_key_after_bytes_in_subtree(ft_h, ft, child, &next_unlockers, &next_ancestors, next_bounds, bfe, search, subtree_bytes, start_key, skip_len, callback, cb_extra, skipped);
}

static int get_key_after_bytes_in_subtree(FT_HANDLE ft_h, FT ft, FTNODE node, UNLOCKERS unlockers, ANCESTORS ancestors, const pivot_bounds &bounds, ftnode_fetch_extra *bfe, ft_search *search, uint64_t subtree_bytes, const DBT *start_key, uint64_t skip_len, void (*callback)(const DBT *, uint64_t, void *), void *cb_extra, uint64_t *skipped) {
    int r;
    int childnum = toku_ft_search_which_child(ft->cmp, node, search);
    const uint64_t child_subtree_bytes = subtree_bytes / node->n_children;
    if (node->height == 0) {
        r = DB_NOTFOUND;
        for (int i = childnum; r == DB_NOTFOUND && i < node->n_children; ++i) {
            // The theory here is that a leaf node could only be very
            // unbalanced if it's dirty, which means all its basements are
            // available.  So if a basement node is available, we should
            // check it as carefully as possible, but if it's compressed
            // or on disk, then it should be fairly well balanced so we
            // can trust the fanout calculation.
            if (BP_STATE(node, i) == PT_AVAIL) {
                r = get_key_after_bytes_in_basementnode(ft, BLB(node, i), (i == childnum) ? start_key : nullptr, skip_len, callback, cb_extra, skipped);
            } else {
                *skipped += child_subtree_bytes;
                if (*skipped >= skip_len && i < node->n_children - 1) {
                    DBT pivot;
                    callback(node->pivotkeys.fill_pivot(i, &pivot), *skipped, cb_extra);
                    r = 0;
                }
                // Otherwise, r is still DB_NOTFOUND.  If this is the last
                // basement node, we'll return DB_NOTFOUND and that's ok.
                // Some ancestor in the call stack will check the next
                // node over and that will call the callback, or if no
                // such node exists, we're at the max key and we should
                // return DB_NOTFOUND up to the top.
            }
        }
    } else {
        r = get_key_after_bytes_in_child(ft_h, ft, node, unlockers, ancestors, bounds, bfe, search, childnum, child_subtree_bytes, start_key, skip_len, callback, cb_extra, skipped);
        for (int i = childnum + 1; r == DB_NOTFOUND && i < node->n_children; ++i) {
            if (*skipped + child_subtree_bytes < skip_len) {
                *skipped += child_subtree_bytes;
            } else {
                r = get_key_after_bytes_in_child(ft_h, ft, node, unlockers, ancestors, bounds, bfe, search, i, child_subtree_bytes, nullptr, skip_len, callback, cb_extra, skipped);    
            }
        }
    }

    if (r != TOKUDB_TRY_AGAIN) {
        assert(unlockers->locked);
        toku_unpin_ftnode_read_only(ft, node);
        unlockers->locked = false;
    }
    return r;
}

int toku_ft_get_key_after_bytes(FT_HANDLE ft_h, const DBT *start_key, uint64_t skip_len, void (*callback)(const DBT *end_key, uint64_t actually_skipped, void *extra), void *cb_extra)
// Effect:
//  Call callback with end_key set to the largest key such that the sum of the sizes of the key/val pairs in the range [start_key, end_key) is <= skip_len.
//  Call callback with actually_skipped set to the sum of the sizes of the key/val pairs in the range [start_key, end_key).
// Notes:
//  start_key == nullptr is interpreted as negative infinity.
//  end_key == nullptr is interpreted as positive infinity.
//  Only the latest val is counted toward the size, in the case of MVCC data.
// Implementation:
//  This is an estimated calculation.  We assume for a node that each of its subtrees have equal size.  If the tree is a single basement node, then we will be accurate, but otherwise we could be quite off.
// Returns:
//  0 on success
//  an error code otherwise
{
    FT ft = ft_h->ft;
    ftnode_fetch_extra bfe;
    bfe.create_for_min_read(ft);
    while (true) {
        FTNODE root;
        {
            uint32_t fullhash;
            CACHEKEY root_key;
            toku_calculate_root_offset_pointer(ft, &root_key, &fullhash);
            toku_pin_ftnode(ft, root_key, fullhash, &bfe, PL_READ, &root, true);
        }
        struct unlock_ftnode_extra unlock_extra = {ft_h, root, false};
        struct unlockers unlockers = {true, unlock_ftnode_fun, (void*)&unlock_extra, (UNLOCKERS) nullptr};
        ft_search search;
        ft_search_init(&search, (start_key == nullptr ? toku_ft_cursor_compare_one : toku_ft_cursor_compare_set_range), FT_SEARCH_LEFT, start_key, nullptr, ft_h);
        
        int r;
        // We can't do this because of #5768, there may be dictionaries in the wild that have negative stats.  This won't affect mongo so it's ok:
        //paranoid_invariant(ft->in_memory_stats.numbytes >= 0);
        int64_t numbytes = ft->in_memory_stats.numbytes;
        if (numbytes < 0) {
            numbytes = 0;
        }
        uint64_t skipped = 0;
        r = get_key_after_bytes_in_subtree(ft_h, ft, root, &unlockers, nullptr, pivot_bounds::infinite_bounds(), &bfe, &search, (uint64_t) numbytes, start_key, skip_len, callback, cb_extra, &skipped);
        assert(!unlockers.locked);
        if (r != TOKUDB_TRY_AGAIN) {
            if (r == DB_NOTFOUND) {
                callback(nullptr, skipped, cb_extra);
                r = 0;
            }
            return r;
        }
    }
}

//Test-only wrapper for the old one-key range function
void toku_ft_keyrange(FT_HANDLE ft_handle, DBT *key, uint64_t *less,  uint64_t *equal,  uint64_t *greater) {
    uint64_t zero_equal_right, zero_greater;
    bool ignore;
    toku_ft_keysrange(ft_handle, key, nullptr, less, equal, greater, &zero_equal_right, &zero_greater, &ignore);
    invariant_zero(zero_equal_right);
    invariant_zero(zero_greater);
}

void toku_ft_handle_stat64 (FT_HANDLE ft_handle, TOKUTXN UU(txn), struct ftstat64_s *s) {
    toku_ft_stat64(ft_handle->ft, s);
}

void toku_ft_handle_get_fractal_tree_info64(FT_HANDLE ft_h, struct ftinfo64 *s) {
    toku_ft_get_fractal_tree_info64(ft_h->ft, s);
}

int toku_ft_handle_iterate_fractal_tree_block_map(FT_HANDLE ft_h, int (*iter)(uint64_t,int64_t,int64_t,int64_t,int64_t,void*), void *iter_extra) {
    return toku_ft_iterate_fractal_tree_block_map(ft_h->ft, iter, iter_extra);
}

/* ********************* debugging dump ************************ */
static int
toku_dump_ftnode (FILE *file, FT_HANDLE ft_handle, BLOCKNUM blocknum, int depth, const DBT *lorange, const DBT *hirange) {
    int result=0;
    FTNODE node;
    toku_get_node_for_verify(blocknum, ft_handle, &node);
    result=toku_verify_ftnode(ft_handle, ft_handle->ft->h->max_msn_in_ft, ft_handle->ft->h->max_msn_in_ft, false, node, -1, lorange, hirange, NULL, NULL, 0, 1, 0);
    uint32_t fullhash = toku_cachetable_hash(ft_handle->ft->cf, blocknum);
    ftnode_fetch_extra bfe;
    bfe.create_for_full_read(ft_handle->ft);
    toku_pin_ftnode(
        ft_handle->ft,
        blocknum,
        fullhash,
        &bfe,
        PL_WRITE_EXPENSIVE,
        &node,
        true
        );
    assert(node->fullhash==fullhash);
    fprintf(file, "%*sNode=%p\n", depth, "", node);

    fprintf(file, "%*sNode %" PRId64 " height=%d n_children=%d  keyrange=%s %s\n",
            depth, "", blocknum.b, node->height, node->n_children, (char*)(lorange ? lorange->data : 0), (char*)(hirange ? hirange->data : 0));
    {
        int i;
        for (i=0; i+1< node->n_children; i++) {
            fprintf(file, "%*spivotkey %d =", depth+1, "", i);
            toku_print_BYTESTRING(file, node->pivotkeys.get_pivot(i).size, (char *) node->pivotkeys.get_pivot(i).data);
            fprintf(file, "\n");
        }
        for (i=0; i< node->n_children; i++) {
            if (node->height > 0) {
                NONLEAF_CHILDINFO bnc = BNC(node, i);
                fprintf(file, "%*schild %d buffered (%d entries):", depth+1, "", i, toku_bnc_n_entries(bnc));
                struct print_msg_fn {
                    FILE *file;
                    int depth;
                    print_msg_fn(FILE *f, int d) : file(f), depth(d) { }
                    int operator()(const ft_msg &msg, bool UU(is_fresh)) {
                        fprintf(file, "%*s xid=%" PRIu64 " %u (type=%d) msn=0x%" PRIu64 "\n",
                                      depth+2, "",
                                      toku_xids_get_innermost_xid(msg.xids()),
                                      static_cast<unsigned>(toku_dtoh32(*(int*)msg.kdbt()->data)),
                                      msg.type(), msg.msn().msn);
                        return 0;
                    }
                } print_fn(file, depth);
                bnc->msg_buffer.iterate(print_fn);
            }
            else {
                int size = BLB_DATA(node, i)->num_klpairs();
                if (0)
                    for (int j=0; j<size; j++) {
                        LEAFENTRY le;
                        void* keyp = NULL;
                        uint32_t keylen = 0;
                        int r = BLB_DATA(node,i)->fetch_klpair(j, &le, &keylen, &keyp);
                        assert_zero(r);
                        fprintf(file, " [%d]=", j);
                        print_klpair(file, keyp, keylen, le);
                        fprintf(file, "\n");
                    }
                fprintf(file, "\n");
            }
        }
        if (node->height > 0) {
            for (i=0; i<node->n_children; i++) {
                fprintf(file, "%*schild %d\n", depth, "", i);
                if (i>0) {
                    char *CAST_FROM_VOIDP(key, node->pivotkeys.get_pivot(i - 1).data);
                    fprintf(file, "%*spivot %d len=%u %u\n", depth+1, "", i-1, node->pivotkeys.get_pivot(i - 1).size, (unsigned)toku_dtoh32(*(int*)key));
                }
                DBT x, y;
                toku_dump_ftnode(file, ft_handle, BP_BLOCKNUM(node, i), depth+4,
                                  (i==0) ? lorange : node->pivotkeys.fill_pivot(i - 1, &x),
                                  (i==node->n_children-1) ? hirange : node->pivotkeys.fill_pivot(i, &y));
            }
        }
    }
    toku_unpin_ftnode(ft_handle->ft, node);
    return result;
}

int toku_dump_ft(FILE *f, FT_HANDLE ft_handle) {
    FT ft = ft_handle->ft;
    invariant_notnull(ft);
    ft->blocktable.dump_translation_table(f);

    uint32_t fullhash = 0;
    CACHEKEY root_key;
    toku_calculate_root_offset_pointer(ft_handle->ft, &root_key, &fullhash);
    return toku_dump_ftnode(f, ft_handle, root_key, 0, 0, 0);
}

int toku_ft_layer_init(void) {
    int r = 0;
    //Portability must be initialized first
    r = toku_portability_init();
    if (r) { goto exit; }
    r = db_env_set_toku_product_name("tokudb");
    if (r) { goto exit; }

    partitioned_counters_init();
    status_init();
    txn_status_init();
    toku_ule_status_init();
    toku_checkpoint_init();
    toku_ft_serialize_layer_init();
    toku_mutex_init(&ft_open_close_lock, NULL);
    toku_scoped_malloc_init();
exit:
    return r;
}

void toku_ft_layer_destroy(void) {
    toku_mutex_destroy(&ft_open_close_lock);
    toku_ft_serialize_layer_destroy();
    toku_checkpoint_destroy();
    status_destroy();
    txn_status_destroy();
    toku_ule_status_destroy();
    toku_context_status_destroy();
    partitioned_counters_destroy();
    toku_scoped_malloc_destroy();
    //Portability must be cleaned up last
    toku_portability_destroy();
}

// This lock serializes all opens and closes because the cachetable requires that clients do not try to open or close a cachefile in parallel.  We made
// it coarser by not allowing any cachefiles to be open or closed in parallel.
void toku_ft_open_close_lock(void) {
    toku_mutex_lock(&ft_open_close_lock);
}

void toku_ft_open_close_unlock(void) {
    toku_mutex_unlock(&ft_open_close_lock);
}

// Prepare to remove a dictionary from the database when this transaction is committed:
//  - mark transaction as NEED fsync on commit
//  - make entry in rollback log
//  - make fdelete entry in recovery log
//
// Effect: when the txn commits, the ft's cachefile will be marked as unlink
//         on close. see toku_commit_fdelete and how unlink on close works
//         in toku_cachefile_close();
// Requires: serialized with begin checkpoint
//           this does not need to take the open close lock because
//           1.) the ft/cf cannot go away because we have a live handle.
//           2.) we're not setting the unlink on close bit _here_. that
//           happens on txn commit (as the name suggests).
//           3.) we're already holding the multi operation lock to 
//           synchronize with begin checkpoint.
// Contract: the iname of the ft should never be reused.
void toku_ft_unlink_on_commit(FT_HANDLE handle, TOKUTXN txn) {
    assert(txn);

    CACHEFILE cf = handle->ft->cf;
    FT CAST_FROM_VOIDP(ft, toku_cachefile_get_userdata(cf));

    toku_txn_maybe_note_ft(txn, ft);

    // If the txn commits, the commit MUST be in the log before the file is actually unlinked
    toku_txn_force_fsync_on_commit(txn); 
    // make entry in rollback log
    FILENUM filenum = toku_cachefile_filenum(cf);
    toku_logger_save_rollback_fdelete(txn, filenum);
    // make entry in recovery log
    toku_logger_log_fdelete(txn, filenum);
}

// Non-transactional version of fdelete
//
// Effect: The ft file is unlinked when the handle closes and it's ft is not
//         pinned by checkpoint. see toku_remove_ft_ref() and how unlink on
//         close works in toku_cachefile_close();
// Requires: serialized with begin checkpoint
void toku_ft_unlink(FT_HANDLE handle) {
    CACHEFILE cf;
    cf = handle->ft->cf;
    toku_cachefile_unlink_on_close(cf);
}

int toku_ft_get_fragmentation(FT_HANDLE ft_handle, TOKU_DB_FRAGMENTATION report) {
    int fd = toku_cachefile_get_fd(ft_handle->ft->cf);
    toku_ft_lock(ft_handle->ft);

    int64_t file_size;
    int r = toku_os_get_file_size(fd, &file_size);
    if (r == 0) {
        report->file_size_bytes = file_size;
        ft_handle->ft->blocktable.get_fragmentation_unlocked(report);
    }
    toku_ft_unlock(ft_handle->ft);
    return r;
}

static bool is_empty_fast_iter (FT_HANDLE ft_handle, FTNODE node) {
    if (node->height > 0) {
        for (int childnum=0; childnum<node->n_children; childnum++) {
            if (toku_bnc_nbytesinbuf(BNC(node, childnum)) != 0) {
                return 0; // it's not empty if there are bytes in buffers
            }
            FTNODE childnode;
            {
                BLOCKNUM childblocknum = BP_BLOCKNUM(node,childnum);
                uint32_t fullhash =  compute_child_fullhash(ft_handle->ft->cf, node, childnum);
                ftnode_fetch_extra bfe;
                bfe.create_for_full_read(ft_handle->ft);
                // don't need to pass in dependent nodes as we are not
                // modifying nodes we are pinning
                toku_pin_ftnode(
                    ft_handle->ft,
                    childblocknum,
                    fullhash,
                    &bfe,
                    PL_READ, // may_modify_node set to false, as nodes not modified
                    &childnode,
                    true
                    );
            }
            int child_is_empty = is_empty_fast_iter(ft_handle, childnode);
            toku_unpin_ftnode(ft_handle->ft, childnode);
            if (!child_is_empty) return 0;
        }
        return 1;
    } else {
        // leaf:  If the dmt is empty, we are happy.
        for (int i = 0; i < node->n_children; i++) {
            if (BLB_DATA(node, i)->num_klpairs()) {
                return false;
            }
        }
        return true;
    }
}

bool toku_ft_is_empty_fast (FT_HANDLE ft_handle)
// A fast check to see if the tree is empty.  If there are any messages or leafentries, we consider the tree to be nonempty.  It's possible that those
// messages and leafentries would all optimize away and that the tree is empty, but we'll say it is nonempty.
{
    uint32_t fullhash;
    FTNODE node;
    {
        CACHEKEY root_key;
        toku_calculate_root_offset_pointer(ft_handle->ft, &root_key, &fullhash);
        ftnode_fetch_extra bfe;
        bfe.create_for_full_read(ft_handle->ft);
        toku_pin_ftnode(
            ft_handle->ft,
            root_key,
            fullhash,
            &bfe,
            PL_READ, // may_modify_node set to false, node does not change
            &node,
            true
            );
    }
    bool r = is_empty_fast_iter(ft_handle, node);
    toku_unpin_ftnode(ft_handle->ft, node);
    return r;
}

// test-only
int toku_ft_strerror_r(int error, char *buf, size_t buflen)
{
    if (error>=0) {
        return (long) strerror_r(error, buf, buflen);
    } else {
        switch (error) {
        case DB_KEYEXIST:
            snprintf(buf, buflen, "Key exists");
            return 0;
        case TOKUDB_CANCELED:
            snprintf(buf, buflen, "User canceled operation");
            return 0;
        default:
            snprintf(buf, buflen, "Unknown error %d", error);
            return EINVAL;
        }
    }
}

int toku_keycompare(const void *key1, uint32_t key1len, const void *key2, uint32_t key2len) {
    int comparelen = key1len < key2len ? key1len : key2len;
    int c = memcmp(key1, key2, comparelen);
    if (__builtin_expect(c != 0, 1)) {
        return c;
    } else {
        if (key1len < key2len) {
            return -1;
        } else if (key1len > key2len) { 
            return 1;
        } else {
            return 0;
        }
    }
}

int toku_builtin_compare_fun(DB *db __attribute__((__unused__)), const DBT *a, const DBT*b) {
    return toku_keycompare(a->data, a->size, b->data, b->size);
}

#include <toku_race_tools.h>
void __attribute__((__constructor__)) toku_ft_helgrind_ignore(void);
void
toku_ft_helgrind_ignore(void) {
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&ft_status, sizeof ft_status);
}

#undef STATUS_INC
