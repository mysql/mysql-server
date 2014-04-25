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

#ifndef FT_SEARCH_H
#define FT_SEARCH_H


enum ft_search_direction_e {
    FT_SEARCH_LEFT = 1,  /* search left -> right, finds min xy as defined by the compare function */
    FT_SEARCH_RIGHT = 2, /* search right -> left, finds max xy as defined by the compare function */
};

struct ft_search;

/* the search compare function should return 0 for all xy < kv and 1 for all xy >= kv
   the compare function should be a step function from 0 to 1 for a left to right search
   and 1 to 0 for a right to left search */

typedef int (*ft_search_compare_func_t)(const struct ft_search &, const DBT *);

/* the search object contains the compare function, search direction, and the kv pair that
   is used in the compare function.  the context is the user's private data */

typedef struct ft_search {
    ft_search_compare_func_t compare;
    enum ft_search_direction_e direction;
    const DBT *k;
    void *context;
    
    // To fix #3522, we need to remember the pivots that we have searched unsuccessfully.
    // For example, when searching right (left), we call search->compare() on the ith pivot key.  If search->compare(0 returns
    //  nonzero, then we search the ith subtree.  If that subsearch returns DB_NOTFOUND then maybe the key isn't present in the
    //  tree.  But maybe we are doing a DB_NEXT (DB_PREV), and everything was deleted.  So we remember the pivot, and later we
    //  will only search subtrees which contain keys that are bigger than (less than) the pivot.
    // The code is a kludge (even before this fix), and interacts strangely with the TOKUDB_FOUND_BUT_REJECTED (which is there
    //  because a failed DB_GET we would keep searching the rest of the tree).  We probably should write the various lookup
    //  codes (NEXT, PREV, CURRENT, etc) more directly, and we should probably use a binary search within a node to search the
    //  pivots so that we can support a larger fanout.
    // These changes (3312+3522) also (probably) introduce an isolation error (#3529).
    //  We must make sure we lock the right range for proper isolation level.
    //  There's probably a bug in which the following could happen.
    //      Thread A:  Searches through deleted keys A,B,D,E and finds nothing, so searches the next leaf, releasing the YDB lock.
    //      Thread B:  Inserts key C, and acquires the write lock, then commits.
    //      Thread A:  Resumes, searching F,G,H and return success.  Thread A then read-locks the range A-H, and doesn't notice
    //        the value C inserted by thread B.  Thus a failure of serialization.
    //     See #3529.
    // There also remains a potential thrashing problem.  When we get a TOKUDB_TRY_AGAIN, we unpin everything.  There's
    //   no guarantee that we will get everything pinned again.  We ought to keep nodes pinned when we retry, except that on the
    //   way out with a DB_NOTFOUND we ought to unpin those nodes.  See #3528.
    DBT pivot_bound;
    const DBT *k_bound;
} ft_search_t;

/* initialize the search compare object */
static inline ft_search_t *ft_search_init(ft_search_t *so, ft_search_compare_func_t compare, enum ft_search_direction_e direction, 
                                          const DBT *k, const DBT *k_bound, void *context) {
    so->compare = compare;
    so->direction = direction;
    so->k = k;
    so->context = context;
    toku_init_dbt(&so->pivot_bound);
    so->k_bound = k_bound;
    return so;
}

static inline void ft_search_finish(ft_search_t *so) {
    toku_destroy_dbt(&so->pivot_bound);
}

#endif
