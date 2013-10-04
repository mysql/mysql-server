/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef FT_FLUSHER_INTERNAL_H
#define FT_FLUSHER_INTERNAL_H
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

#include <fttypes.h>

#define flt_flush_before_applying_inbox 1
#define flt_flush_before_child_pin 2
#define ft_flush_aflter_child_pin 3
#define flt_flush_before_split 4
#define flt_flush_during_split 5
#define flt_flush_before_merge 6
#define ft_flush_aflter_merge 7
#define ft_flush_aflter_rebalance 8
#define flt_flush_before_unpin_remove 9
#define flt_flush_before_pin_second_node_for_merge 10

typedef struct flusher_advice FLUSHER_ADVICE;

/**
 * Choose a child to flush to.  Returns a childnum, or -1 if we should
 * go no further.
 *
 * Flusher threads: pick the heaviest child buffer
 * Cleaner threads: pick the heaviest child buffer
 * Cleaner thread merging leaf nodes: follow down to a key
 * Hot optimize table: follow down to the right of a key
 */
typedef int (*FA_PICK_CHILD)(FT h, FTNODE parent, void* extra);

/**
 * Decide whether to call `toku_ft_flush_some_child` on the child if it is
 * stable and a nonleaf node.
 *
 * Flusher threads: yes if child is gorged
 * Cleaner threads: yes if child is gorged
 * Cleaner thread merging leaf nodes: always yes
 * Hot optimize table: always yes
 */
typedef bool (*FA_SHOULD_RECURSIVELY_FLUSH)(FTNODE child, void* extra);

/**
 * Called if the child needs merging.  Should do something to get the
 * child out of a fusible state.  Must unpin parent and child.
 *
 * Flusher threads: just do the merge
 * Cleaner threads: if nonleaf, just merge, otherwise start a "cleaner
 *                  thread merge"
 * Cleaner thread merging leaf nodes: just do the merge
 * Hot optimize table: just do the merge
 */
typedef void (*FA_MAYBE_MERGE_CHILD)(struct flusher_advice *fa,
                              FT h,
                              FTNODE parent,
                              int childnum,
                              FTNODE child,
                              void* extra);

/**
 * Cleaner threads may need to destroy basement nodes which have been
 * brought more up to date than the height 1 node flushing to them.
 * This function is used to determine if we need to check for basement
 * nodes that are too up to date, and then destroy them if we find
 * them.
 *
 * Flusher threads: no
 * Cleaner threads: yes
 * Cleaner thread merging leaf nodes: no
 * Hot optimize table: no
 */
typedef bool (*FA_SHOULD_DESTROY_BN)(void* extra);

/**
 * Update `ft_flusher_status` in whatever way necessary.  Called once
 * by `toku_ft_flush_some_child` right before choosing what to do next (split,
 * merge, recurse), with the number of nodes that were dirtied by this
 * execution of `toku_ft_flush_some_child`.
 */
typedef void (*FA_UPDATE_STATUS)(FTNODE child, int dirtied, void* extra);

/**
 * Choose whether to go to the left or right child after a split.  Called
 * by `ft_split_child`.  If -1 is returned, `ft_split_child` defaults to
 * the old behavior.
 */
typedef int (*FA_PICK_CHILD_AFTER_SPLIT)(FT h,
                                         FTNODE node,
                                         int childnuma,
                                         int childnumb,
                                         void* extra);

/**
 * A collection of callbacks used by the flushing machinery to make
 * various decisions.  There are implementations of each of these
 * functions for flusher threads (flt_*), cleaner threads (ct_*), , and hot
 * optimize table (hot_*).
 */
struct flusher_advice {
    FA_PICK_CHILD pick_child;
    FA_SHOULD_RECURSIVELY_FLUSH should_recursively_flush;
    FA_MAYBE_MERGE_CHILD maybe_merge_child;
    FA_SHOULD_DESTROY_BN should_destroy_basement_nodes;
    FA_UPDATE_STATUS update_status;
    FA_PICK_CHILD_AFTER_SPLIT pick_child_after_split;
    void* extra; // parameter passed into callbacks
};

void
flusher_advice_init(
    struct flusher_advice *fa,
    FA_PICK_CHILD pick_child,
    FA_SHOULD_DESTROY_BN should_destroy_basement_nodes,
    FA_SHOULD_RECURSIVELY_FLUSH should_recursively_flush,
    FA_MAYBE_MERGE_CHILD maybe_merge_child,
    FA_UPDATE_STATUS update_status,
    FA_PICK_CHILD_AFTER_SPLIT pick_child_after_split,
    void* extra
    );

void toku_ft_flush_some_child(
    FT ft,
    FTNODE parent,
    struct flusher_advice *fa
    );

bool
always_recursively_flush(FTNODE child, void* extra);

bool
never_recursively_flush(FTNODE UU(child), void* UU(extra));

bool
dont_destroy_basement_nodes(void* extra);

void
default_merge_child(struct flusher_advice *fa,
                    FT h,
                    FTNODE parent,
                    int childnum,
                    FTNODE child,
                    void* extra);

int
default_pick_child_after_split(FT h,
                               FTNODE parent,
                               int childnuma,
                               int childnumb,
                               void *extra);


#endif // End of header guardian.
