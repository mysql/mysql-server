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
#include <stdint.h>
#include <string.h>
#include <tokuredblack.h>
struct __toku_range_tree_local {
    //Logarithmic non-overlapping version only fields:
    struct toku_rbt_tree* rbt;
    /*
     * if iter_at_beginning is TRUE, then iteration is at beginning,
     * if iter_at_beginning is FALSE and iterator_finger is NOT NULL, 
     * iteration is ongoing, 
     * if iter_at_beginning is FALSE and iterator_finger is NULL,
     * iteration has ended or in invalid state. 
     */
    
    /*
     * BOOL that says if iteration is at beginning.
     * If at beginning, toku_rt_next returns first element
     * in the range_tree, otherwise returns successor of curr_range
     */ 
    BOOL iter_at_beginning;
    /**
     * The range we are currently at in the iterator, if it is set to NULL, that means
     * the iteration is no longer valid and/or the iteration is done 
     * */
    struct toku_rbt_node* iterator_finger;
};
#include <rangetree-internal.h>


/*
Redblack tree.

lookup (type) returns:
    pointer to data (or NULL if not found)
    'elementpointer' (to be used in finger_delete, finger_predecessor, finger_successor)
    'insertpointer'  (to be used in finger_insert)

Finger usefulness:
*/

//FIRST PASS
int toku_rt_create(toku_range_tree** ptree,
                   int (*end_cmp)(const toku_point*,const toku_point*),
                   int (*data_cmp)(const DB_TXN*,const DB_TXN*),
                   BOOL allow_overlaps,
                   void* (*user_malloc) (size_t),
                   void  (*user_free)   (void*),
                   void* (*user_realloc)(void*, size_t)) {
    int r = ENOSYS;
    toku_range_tree* temptree = NULL;

    if (allow_overlaps) return EINVAL;
    r = toku_rt_super_create(ptree, &temptree, end_cmp, data_cmp, allow_overlaps,
                             user_malloc, user_free, user_realloc);
    if (r!=0) { goto cleanup; }
    
    //Any local initializers go here.
    r = toku_rbt_init(end_cmp, &temptree->i.rbt, user_malloc, user_free, user_realloc);
    if (r!=0) { goto cleanup; }

    /*
     * Start range tree in invalid iteration state, toku_rt_start_scan must
     * be called to start iteration
     */
    temptree->i.iter_at_beginning = FALSE;
    temptree->i.iterator_finger = NULL;
    *ptree = temptree;
    r = 0;
cleanup:
    if (r!=0) {
        if (temptree) user_free(temptree);
    }
    return r;
}

//FIRST PASS
int toku_rt_close(toku_range_tree* tree) {
    if (!tree) { return EINVAL; }
    toku_rbt_destroy(tree->i.rbt);
    tree->free(tree);
    return 0;
}

/*
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
int toku_rt_find(toku_range_tree* tree, toku_range* query, u_int32_t k,
                 toku_range** buf, u_int32_t* buflen, u_int32_t* numfound) {
    int r = ENOSYS;

    if (!tree || !query || !buf || !buflen || !numfound ||
        query->data != NULL || *buflen == 0) {
        r = EINVAL; goto cleanup;
    }
    assert(!tree->allow_overlaps);

    struct toku_rbt_node* ignore_insert = NULL;
    struct toku_rbt_node* succ_finger   = NULL;
    toku_range*           data          = NULL;
    u_int32_t             temp_numfound = 0;

    /* k = 0 means return ALL. (infinity) */
    if (k == 0) { k = UINT32_MAX; }

    r = toku_rbt_lookup(RB_LULTEQ, query, tree->i.rbt, &ignore_insert, &succ_finger, &data);
    if (r!=0) { goto cleanup; }
    if (data != NULL) {
        if (tree->end_cmp(data->right, query->left) >= 0) {
            r = toku__rt_increase_buffer(tree, buf, buflen, temp_numfound + 1);
            if (r!=0) { goto cleanup; }
            (*buf)[temp_numfound++] = *data;
        }
        if (temp_numfound < k) {
            r = toku_rbt_finger_successor(&succ_finger, &data);
            if (r!=0) { goto cleanup; }
        }
    }
    else {
        r = toku_rbt_lookup(RB_LUFIRST, NULL, tree->i.rbt, &ignore_insert, &succ_finger, &data);
        if (r!=0) { goto cleanup; }
    }

    while (temp_numfound < k && data != NULL) {
        if (tree->end_cmp(data->left, query->right) > 0) { break; }
        r = toku__rt_increase_buffer(tree, buf, buflen, temp_numfound + 1);
        if (r!=0) { goto cleanup; }
        (*buf)[temp_numfound++] = *data;
        r = toku_rbt_finger_successor(&succ_finger, &data);
        if (r!=0) { goto cleanup; }
    }

    *numfound = temp_numfound;
    r = 0;
cleanup:
    return r;    
}

/*
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
*/
int toku_rt_insert(toku_range_tree* tree, toku_range* range) {
    int r = ENOSYS;
    if (!tree || !range) { r = EINVAL; goto cleanup; }
    assert(!tree->allow_overlaps);

    struct toku_rbt_node* insert_finger = NULL;
    struct toku_rbt_node* ignore_insert = NULL;
    struct toku_rbt_node* succ_finger   = NULL;
    toku_range*           data          = NULL;

    r = toku_rbt_lookup(RB_LULTEQ, range, tree->i.rbt, &insert_finger, &succ_finger, &data);
    if (r!=0) { goto cleanup; }
    if (data != NULL) {
        if (tree->end_cmp(data->right, range->left) >= 0) {
            r = EDOM; goto cleanup;
        }
        r = toku_rbt_finger_successor(&succ_finger, &data);
        if (r!=0) { goto cleanup; }
    }
    else {
        r = toku_rbt_lookup(RB_LUFIRST, NULL, tree->i.rbt, &ignore_insert, &succ_finger, &data);
        if (r!=0) { goto cleanup; }
    }
    if (data != NULL && tree->end_cmp(data->left, range->right) <= 0) {
        r = EDOM; goto cleanup;
    }
    r = toku_rbt_finger_insert(range, tree->i.rbt, insert_finger);
    if (r!=0) { goto cleanup; }

    tree->numelements++;
    /*
     * invalidate iteration, because we have inserted node
     */
    tree->i.iterator_finger = NULL;
    r = 0;
cleanup:
    return r;
}

/*
2- Delete
    O(lg N) CMPs    We do a lookup (==). (out found, out elementpointer)
                    If !found return error.
                    (== already checks for left end point)
                    Data cmp is free (pointer)
     (0+1)  CMPs    if (found.right != to_insert.right || found.data != to_delete.data), return error.
     (0)    CMPs    Do a finger_delete(element_pointer)
*/
int toku_rt_delete(toku_range_tree* tree, toku_range* range) {
/* TODO: */
    int r = ENOSYS;

    if (!tree || !range) { r = EINVAL; goto cleanup; }
    assert(!tree->allow_overlaps);

    struct toku_rbt_node* ignore_insert = NULL;
    struct toku_rbt_node* delete_finger = NULL;
    toku_range*           data          = NULL;

    r = toku_rbt_lookup(RB_LUEQUAL, range, tree->i.rbt,
                        &ignore_insert, &delete_finger, &data);
    if (r!=0) { goto cleanup; }
    if (!data ||
        tree->data_cmp(data->data, range->data)  != 0 ||
        tree->end_cmp(data->right, range->right) != 0) {
        r = EDOM; goto cleanup;
    }

    r = toku_rbt_finger_delete(delete_finger, tree->i.rbt);
    if (r!=0) { goto cleanup; }

    /*
     * invalidate iteration, because we have deleted node
     */
    tree->i.iterator_finger = NULL;
    tree->numelements--;
    r = 0;    
cleanup:
    return r;
}

/*
3- Predecessor:
    O(lg N) CMPs    Do a lookup(<) (out found, out elementpointer)
                    If !found return "not found"
     (0+1)  CMPs    If overlaps (found.right >= query)
     (0)    CMPs        do a finger_predecessor(elementpointer) (out found2)
                        If found2 return found2.
                        else return "not found"
                    else return found.
*/
int toku_rt_predecessor (toku_range_tree* tree, toku_point* point,
                         toku_range* pred, BOOL* wasfound) {
    int r = ENOSYS;
    if (!tree || !point || !pred || !wasfound || tree->allow_overlaps) {
        r = EINVAL; goto cleanup;
    }

    struct toku_rbt_node* ignore_insert = NULL;
    struct toku_rbt_node* pred_finger   = NULL;
    toku_range*           data          = NULL;
    toku_range range;
    range.left  = point;
    range.right = point;
    range.data  = NULL;

    r = toku_rbt_lookup(RB_LULESS, &range, tree->i.rbt, &ignore_insert, &pred_finger, &data);
    if (r!=0) { goto cleanup; }

    if (!data) {
        *wasfound = FALSE;
        r = 0;
        goto cleanup;
    }
    if (tree->end_cmp(data->right, point) < 0) {
        *wasfound = TRUE;
        *pred = *data;
        r = 0;
        goto cleanup;
    }
    r = toku_rbt_finger_predecessor(&pred_finger, &data);
    if (r!=0) { goto cleanup; }
    if (!data) {
        *wasfound = FALSE;
        r = 0;
        goto cleanup;
    }
    *wasfound = TRUE;
    *pred = *data;
    r = 0;

cleanup:    
    return r;    
}    

/*
4- Successor:
    O(lg N) CMPs    Do a lookup (>) (out found)
                    If found, return found.
                    return "not found."
*/
int toku_rt_successor (toku_range_tree* tree, toku_point* point,
                       toku_range* succ, BOOL* wasfound) {
    int r = ENOSYS;
    if (!tree || !point || !succ || !wasfound || tree->allow_overlaps) {
        r = EINVAL; goto cleanup;
    }

    struct toku_rbt_node* ignore_insert = NULL;
    struct toku_rbt_node* succ_finger   = NULL;
    toku_range*           data          = NULL;
    toku_range range;
    range.left  = point;
    range.right = point;
    range.data  = NULL;

    r = toku_rbt_lookup(RB_LUGREAT, &range, tree->i.rbt, &ignore_insert, &succ_finger, &data);
    if (r!=0) { goto cleanup; }

    if (!data) {
        *wasfound = FALSE;
        r = 0;
        goto cleanup;
    }
    *wasfound = TRUE;
    *succ = *data;
    r = 0;
cleanup:    
    return r;    
}

int toku_rt_get_allow_overlaps(toku_range_tree* tree, BOOL* allowed) {
    if (!tree || !allowed) return EINVAL;
    assert(!tree->allow_overlaps);
    *allowed = tree->allow_overlaps;
    return 0;
}

int toku_rt_get_size(toku_range_tree* tree, u_int32_t* size) {
    if (!tree || !size) return EINVAL;
    *size = tree->numelements;
    return 0;
}

void toku_rt_start_scan (toku_range_tree* range_tree) {
    assert(range_tree);
    range_tree->i.iter_at_beginning = TRUE;
    range_tree->i.iterator_finger = NULL;
    return;
}

int toku_rt_next (toku_range_tree* range_tree, toku_range* out_range) {
    int r = ENOSYS;
    toku_range* ret_range = NULL;
    struct toku_rbt_node* ignore_insert = NULL;
    if (!range_tree || !out_range) { r = EINVAL; goto cleanup; }
    /* Check to see if range tree is in invalid iteation state */
    if (!range_tree->i.iter_at_beginning && !range_tree->i.iterator_finger){ 
        r = EDOM; goto cleanup; 
    }
    
    if (range_tree->i.iter_at_beginning) {
        r = toku_rbt_lookup(RB_LUFIRST, NULL, range_tree->i.rbt, 
                &ignore_insert, &range_tree->i.iterator_finger, &ret_range);
        if (r != 0) { goto cleanup; }
    }
    else {
        /*
         * If there is no successor because we have arrived at the end of the iteration,
         * or because of some unexpected error, ret_range will have the value of NULL,
         * which we want to return to the user in such cases
         */
        r = toku_rbt_finger_successor(&range_tree->i.iterator_finger, &ret_range);
        if (r != 0) { goto cleanup; }
    }

    out_range->left = ret_range->left;
    out_range->right = ret_range->right;
    out_range->data = ret_range->data;
    r = 0;
    
cleanup:
    return r;
}

