#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#ifndef BRT_SEARCH_H
#define BRT_SEARCH_H

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

enum brt_search_direction_e {
    BRT_SEARCH_LEFT = 1,  /* search left -> right, finds min xy as defined by the compare function */
    BRT_SEARCH_RIGHT = 2, /* search right -> left, finds max xy as defined by the compare function */
};

struct brt_search;

/* the search compare function should return 0 for all xy < kv and 1 for all xy >= kv
   the compare function should be a step function from 0 to 1 for a left to right search
   and 1 to 0 for a right to left search */

typedef int (*brt_search_compare_func_t)(struct brt_search */*so*/, DBT *);

/* the search object contains the compare function, search direction, and the kv pair that
   is used in the compare function.  the context is the user's private data */

typedef struct brt_search {
    brt_search_compare_func_t compare;
    enum brt_search_direction_e direction;
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
    BOOL have_pivot_bound;
    DBT  pivot_bound;
} brt_search_t;

/* initialize the search compare object */
static inline brt_search_t *brt_search_init(brt_search_t *so, brt_search_compare_func_t compare, enum brt_search_direction_e direction, const DBT *k, void *context) {
    so->compare = compare;
    so->direction = direction;
    so->k = k;
    so->context = context;
    so->have_pivot_bound = FALSE;
    return so;
}

static inline void brt_search_finish(brt_search_t *so) {
    if (so->have_pivot_bound) toku_free(so->pivot_bound.data);
}

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif
