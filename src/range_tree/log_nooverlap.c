/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/**
   \file linear.c
   \brief Range tree implementation
  
   See rangetree.h for documentation on the following. */

//Currently this is a stub implementation just so we can write and compile tests
//before actually implementing the range tree.

#include <rangetree.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
struct __toku_range_tree_local {
    //Logarithmic non-overlapping version only fields:
    toku_rbt*   rbt;
};
#include <rangetree-internal.h>

#include <tokuredblack.h>

/*
Redblack tree.

lookup (type) returns:
    pointer to data (or NULL if not found)
    'elementpointer' (to be used in finger_delete, finger_predecessor, finger_successor)
    'insertpointer'  (to be used in finger_insert)

Finger usefulness:
    1- Insert
        O(lg N) CMPs    We do a lookup(<=) (out elementpointer, out found, out insertpointer)
                        If found
                            If overlaps (found.right >= query.left) return error
                            Do a finger_successor(elementpointer)  (out found2)
         (0+1)  CMPs        if (found2 and overlaps (found2.left <= query.right) return error
                        else
                            Do a lookup(First) (out found2)
                            if (found2 and overlaps (found2.left <= query.right) return error
         (0)    CMPs    Do a finger_insert(data, insertpointer)
    2- Delete
        O(lg N) CMPs    We do a lookup (==). (out found, out elementpointer)
                        If !found return error.
                        (== already checks for left end point)
                        Data cmp is free (pointer)
         (0+1)  CMPs    if (found.right != to_insert.data || found.data != to_delete.data), return error.
         (0)    CMPs    Do a finger_delete(element_pointer)
    3- Predecessor:
        O(lg N) CMPs    Do a lookup(<) (out found, out elementpointer)
                        If !found return "not found"
         (0+1)  CMPs    If overlaps (found.right >= query)
         (0)    CMPs        do a finger_predecessor(elementpointer) (out found2)
                            If found2 return found2.
                            else return "not found"
                        else return found.
    4- Successor:
        O(lg N) CMPs    Do a lookup (>) (out found)
                        If found, return found.
                        return "not found."
    5- FindOverlaps
        O(lg N+1) CMPs  Do a lookup (<=) (out found, out elementpointer)
                        If found
         (0+1)  CMPs       if overlap (if found.right >= query.left)
                              Increaes buffer
                              add found to buffer
         (0)    CMPs        do a finger_successor(elementpointer) (out found, out elementpointer)
                        else
                           do a lookup (FIRST) (out found, out elementpointer)
        O(min(k,K))CMPs while (found && found.left <= query.right
                            Increaes buffer
                            add found to buffer
         (0)    CMPs        do a finger_successor(elementpointer) (out found, out elementpointer)
*/
    
static inline BOOL toku__rt_overlap(toku_range_tree* tree,
                             toku_range* a, toku_range* b) {
    assert(tree);
    assert(a);
    assert(b);
    //a->left <= b->right && b->left <= a->right
    return (tree->end_cmp(a->left, b->right) <= 0 &&
            tree->end_cmp(b->left, a->right) <= 0);
}

static inline BOOL toku__rt_exact(toku_range_tree* tree,
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
    toku_range_tree* tmptree;

    if (allow_overlaps) return ENOSYS;
    r = toku_rt_super_create(ptree, &tmptree, end_cmp, data_cmp, allow_overlaps,
                             user_malloc, user_free, user_realloc);
    if (0) {
        died1:
        user_free(temptree);
        return r;
    }
    if (r!=0) return r;
    
    //Any local initializers go here.
    tmptree->rbt = toku_rbt_init(void);
    if (!tmptree->rbt) { r = errno; goto died1; }
    *ptree = tmptree;

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
