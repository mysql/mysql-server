/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/**
   \file linear.c
   \brief Range tree implementation
  
   See rangetree.h for documentation on the following. */

//Currently this is a stub implementation just so we can write and compile tests
//before actually implementing the range tree.

#include "rangetree.h"
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
struct __toku_range_tree_local {
    //Logarithmic non-overlapping version only fields:
    toku_rbt*   rbt;
};
#include "rangetree-internal.h"

/*
Changes required in red black tree for max performance:
Add finger object.. or keep track of it local?
(or keep it local?)

Finger ops:
    New  Finger.. or equivalent.
    These overwrite the finger:
        FINGER_FIND_LESS_THAN_OR_EQUAL
        FINGER_FIND_EQUAL
        FINGER_FIND_LESS_THAN
        FINGER_FIND_GREATER_THAN
    These use the finger, with boolean (overwrite or leave alone)
        FINGER_NEXT
        FINGER_PREV
    These use the finger
        FINGER_INSERT
        FINGER_DELETE
Subset of this that would just support FindOverlaps:
    These overwrite the finger:
        FINGER_FIND_LESS_THAN_OR_EQUAL
        FINGER_NEXT
    Almost for free:        
        FINGER_FIND_LESS_THAN_OR_EQUAL
        FINGER_FIND_EQUAL
        FINGER_FIND_LESS_THAN
        FINGER_FIND_GREATER_THAN
        FINGER_NEXT
        FINGER_PREV
    Not for free:
        FINGER_INSERT
        FINGER_DELETE

    Things to add if we want to be nice to redblacklib
        FINGER_FIND_GREATER_THAN_OR_EQUAL
        
Finger usefulness:
    1- Insert
        O(lg N) CMPs    We do a find <=.  If found and overlaps (found.right >= query.left) return error
                        Next op is either NO_UPDATE, or alternatively, use a copy of the finger.  (Return to original finger for INSERT)
         (0+1)  CMPs    Do a FINGER_NEXT_NO_UPDATE.  If found and overlaps (found.left <= query.right) return error
         (0)    CMPs    Do a FINGER_INSERT
    2- Delete
        O(lg N) CMPs    We do a find ==.  If !found return error.
                        (== already checks for left end point)
                        Data cmp is free (pointer)
         (0+1)  CMPs    if (found.right != to_insert.data || found.data != to_delete.data), return error.
         (0)    CMPs    Do a FINGER_DELETE
    3- Predecessor:
        O(lg N) CMPs    Do a find <
                        If !found return not found
         (0+1)  CMPs    If overlaps (found.right >= query)
         (0)    CMPs    Do a FINGER_PREV. If found return it.
                            Return not found.
                        return it.
    4- Successor:
        O(lg N) CMPs    Do a find >.
                        If found, return it.
                        return not found.
    5- FindOverlaps
        O(lg N+1) CMPs  Do a find <=.  If found (test for overlap (if found.right >= query.left) if so, Increaes buffer, add to buffer)
        while (Do a FINGER_NEXT_AND_UPDATE) {
           If not found, DONE (return what we've found)
           if not overlap (if found.left > query.right) then DONE (return what we've found)
           Increase buffer
           Add to buffer
        }
*/
    
static BOOL toku__rt_overlap(toku_range_tree* tree,
                             toku_range* a, toku_range* b) {
    assert(tree);
    assert(a);
    assert(b);
    //a->left <= b->right && b->left <= a->right
    return (tree->end_cmp(a->left, b->right) <= 0 &&
            tree->end_cmp(b->left, a->right) <= 0);
}

static BOOL toku__rt_exact(toku_range_tree* tree,
                            toku_range* a, toku_range* b) {
    assert(tree);
    assert(a);
    assert(b);

    return (tree->end_cmp (a->left,  b->left)  == 0 &&
            tree->end_cmp (a->right, b->right) == 0 &&
            tree->data_cmp(a->data,  b->data)  == 0);
}

int toku_rt_create(toku_range_tree** ptree,
                   int (*end_cmp)(toku_point*,toku_point*),
                   int (*data_cmp)(DB_TXN*,DB_TXN*),
		           BOOL allow_overlaps,
                   void* (*user_malloc) (size_t),
                   void  (*user_free)   (void*),
                   void* (*user_realloc)(void*, size_t)) {
    int r;
    toku_range_tree* temptree;

    if (allow_overlaps) return ENOSYS;
    r = toku_rt_super_create(&temptree, end_cmp, data_cmp, allow_overlaps,
                             user_malloc, user_free, user_realloc);
    if (0) {
        died1:
        user_free(temptree);
        return r;
    }
    if (r!=0) return r;
    
    //Any local initializers go here.
    temptree->rbt = toku_rbt_init(void);
    if (!temptree->rbt) { r = errno; goto died1; }
    *ptree = temptree;

    return 0;
}

int toku_rt_close(toku_range_tree* tree) {
    if (!tree)                                           return EINVAL;
    toku_rbt_destroy(tree->rbt);
    tree->free(tree);
    return 0;
}

int toku_rt_find(toku_range_tree* tree, toku_range* query, u_int32_t k,
                 toku_range** buf, u_int32_t* buflen, u_int32_t* numfound) {
/* TODO: RED BLACK TREE does not support partial scan.
*/
    if (!tree || !query || !buf || !buflen || !numfound) return EINVAL;
    if (query->data != NULL)                             return EINVAL;
    if (*buflen == 0)                                    return EINVAL;
    assert(!tree->allow_overlaps);
   
    u_int32_t temp_numfound = 0;
    int r;
    u_int32_t i;
    
    for (i = 0; i < tree->numelements; i++) {
        if (toku__rt_overlap(tree, query, &tree->ranges[i])) {
            r = toku__rt_increase_buffer(tree, buf, buflen, temp_numfound + 1);
            if (r != 0) return r;
            (*buf)[temp_numfound++] = tree->ranges[i];
            //k == 0 means limit of infinity, this is not a bug.
            if (temp_numfound == k) break;
        }
    }
    *numfound = temp_numfound;
    return 0;
}

int toku_rt_insert(toku_range_tree* tree, toku_range* range) {
/* TODO: */
    if (!tree || !range)                                 return EINVAL;
    assert(!tree->allow_overlaps);

    u_int32_t i;
    int r;

    //EDOM cases
    if (tree->allow_overlaps) {
        for (i = 0; i < tree->numelements; i++) {
            if (toku__rt_exact  (tree, range, &tree->ranges[i])) return EDOM;
        }
    }
    else {
        for (i = 0; i < tree->numelements; i++) {
            if (toku__rt_overlap(tree, range, &tree->ranges[i])) return EDOM;
        }
    }
    r = toku__rt_increase_capacity(tree, tree->numelements + 1);
    if (r != 0) return r;
    tree->ranges[tree->numelements++] = *range;
    return 0;
}

int toku_rt_delete(toku_range_tree* tree, toku_range* range) {
/* TODO: */
    if (!tree || !range)                                 return EINVAL;
    u_int32_t i;
    assert(!tree->allow_overlaps);
    
    for (i = 0;
         i < tree->numelements &&
         !toku__rt_exact(tree, range, &(tree->ranges[i]));
         i++) {}
    //EDOM case: Not Found
    if (i == tree->numelements) return EDOM;
    if (i < tree->numelements - 1) {
        tree->ranges[i] = tree->ranges[tree->numelements - 1];
    }
    toku__rt_decrease_capacity(tree, --tree->numelements);
    return 0;
}

int toku_rt_predecessor (toku_range_tree* tree, toku_point* point,
                         toku_range* pred, BOOL* wasfound) {
/* TODO: */
    if (!tree || !point || !pred || !wasfound)           return EINVAL;
    if (tree->allow_overlaps)                            return EINVAL;
    toku_range* best = NULL;
    u_int32_t i;

    for (i = 0; i < tree->numelements; i++) {
        if (toku__rt_p_cmp(tree, point, &tree->ranges[i]) > 0 &&
            (!best || tree->end_cmp(best->left, tree->ranges[i].left) < 0)) {
            best = &tree->ranges[i];
        }
    }
    *wasfound = best != NULL;
    if (best) *pred = *best;
    return 0;
}

int toku_rt_successor (toku_range_tree* tree, toku_point* point,
                       toku_range* succ, BOOL* wasfound) {
/* TODO: */
    if (!tree || !point || !succ || !wasfound)           return EINVAL;
    if (tree->allow_overlaps)                            return EINVAL;
    toku_range* best = NULL;
    u_int32_t i;

    for (i = 0; i < tree->numelements; i++) {
        if (toku__rt_p_cmp(tree, point, &tree->ranges[i]) < 0 &&
            (!best || tree->end_cmp(best->left, tree->ranges[i].left) > 0)) {
            best = &tree->ranges[i];
        }
    }
    *wasfound = best != NULL;
    if (best) *succ = *best;
    return 0;
}

int toku_rt_get_allow_overlaps(toku_range_tree* tree, BOOL* allowed) {
    if (!tree || !allowed)                               return EINVAL;
    assert(!tree->allow_overlaps);
    *allowed = tree->allow_overlaps;
    return 0;
}

int toku_rt_get_size(toku_range_tree* tree, u_int32_t* size) {
/* TODO: */
    if (!tree || !size)                                  return EINVAL;
    *size = tree->numelements;
    return 0;
}
