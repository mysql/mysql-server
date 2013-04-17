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
typedef toku_range *OMTVALUE;
#include "../../newbrt/omt.h"

struct __toku_range_tree_local {
    //Logarithmic non-overlapping version only fields:
    OMT omt;
};
#include <rangetree-internal.h>


int toku_rt_create(toku_range_tree** ptree,
                   int (*end_cmp)(const toku_point*,const toku_point*),
                   int (*data_cmp)(const TXNID,const TXNID),
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
    r = toku_omt_create(&temptree->i.omt);
    if (r!=0) { goto cleanup; }

    *ptree = temptree;
    r = 0;
cleanup:
    if (r!=0) {
        if (temptree) user_free(temptree);
    }
    return r;
}

static int rt_clear_helper(OMTVALUE value, u_int32_t UU(index), void* extra) {
    void  (*user_free)(void*) = (void(*)(void*))extra;
    user_free(value);
    return 0;
}

int toku_rt_close(toku_range_tree* tree) {
    if (!tree) { return EINVAL; }
    int r = toku_omt_iterate(tree->i.omt, rt_clear_helper, tree->free);
    assert(r==0);
    toku_omt_destroy(&tree->i.omt);
    tree->free(tree);
    return 0;
}

void toku_rt_clear(toku_range_tree* tree) {
    assert(tree);
    int r = toku_omt_iterate(tree->i.omt, rt_clear_helper, tree->free);
    assert(r==0);
    toku_omt_clear(tree->i.omt);
    tree->numelements = 0;
}

typedef struct {
    int         (*end_cmp)(const toku_point*,const toku_point*);  
    toku_interval query;
} rt_heavi_extra;

static int rt_heaviside(OMTVALUE candidate, void* extra) {
    toku_range*     range_candidate = candidate;
    rt_heavi_extra* info            = extra;

    if (info->end_cmp(range_candidate->ends.right, info->query.left)  < 0) return -1;
    if (info->end_cmp(range_candidate->ends.left,  info->query.right) > 0) return 1;
    return 0;
}

typedef struct {
    int            (*end_cmp)(const toku_point*,const toku_point*);  
    toku_interval    query;
    u_int32_t        k;
    u_int32_t        numfound;
    toku_range_tree* rt;
    toku_range**     buf;
    u_int32_t*       buflen;
} rt_find_info;

static int rt_find_helper(OMTVALUE value, u_int32_t UU(index), void* extra) {
    rt_find_info* info  = extra;
    toku_range*   range = value;
    int r = ENOSYS;

    if (info->end_cmp(range->ends.left, info->query.right) > 0) {
        r = TOKUDB_SUCCEEDED_EARLY;
        goto cleanup;
    }

    r = toku__rt_increase_buffer(info->rt, info->buf, info->buflen, info->numfound + 1);
    if (r!=0) goto cleanup;
    (*info->buf)[info->numfound++] = *range;
    if (info->numfound>=info->k) {
        r = TOKUDB_SUCCEEDED_EARLY;
        goto cleanup;
    }
    r = 0;
cleanup:
    return r;
} 

int toku_rt_find(toku_range_tree* tree, toku_interval* query, u_int32_t k,
                 toku_range** buf, u_int32_t* buflen, u_int32_t* numfound) {
    int r = ENOSYS;

    if (!tree || !query || !buf || !buflen || !numfound || *buflen == 0) {
        r = EINVAL; goto cleanup;
    }
    assert(!tree->allow_overlaps);

    /* k = 0 means return ALL. (infinity) */
    if (k == 0) { k = UINT32_MAX; }

    u_int32_t leftmost;
    u_int32_t rightmost = toku_omt_size(tree->i.omt);
    rt_heavi_extra extra;
    extra.end_cmp = tree->end_cmp;
    extra.query   = *query;

    r = toku_omt_find_zero(tree->i.omt, rt_heaviside, &extra, NULL, &leftmost, NULL);
    if (r==DB_NOTFOUND) {
        /* Nothing overlaps. */
        *numfound = 0;
        r = 0;
        goto cleanup;
    }
    if (r!=0) goto cleanup;
    rt_find_info info;
    info.end_cmp  = tree->end_cmp;
    info.query    = *query;
    info.k        = k;
    info.numfound = 0;
    info.rt       = tree;
    info.buf      = buf;
    info.buflen   = buflen;

    r = toku_omt_iterate_on_range(tree->i.omt, leftmost, rightmost, rt_find_helper, &info);
    if (r==TOKUDB_SUCCEEDED_EARLY) r=0;
    if (r!=0) goto cleanup;
    *numfound = info.numfound;
    r = 0;
cleanup:
    return r;    
}

int toku_rt_insert(toku_range_tree* tree, toku_range* range) {
    int r = ENOSYS;
    toku_range* insert_range = NULL;
    if (!tree || !range) { r = EINVAL; goto cleanup; }
    assert(!tree->allow_overlaps);

    u_int32_t       index;
    rt_heavi_extra  extra;
    extra.end_cmp = tree->end_cmp;
    extra.query   = range->ends;

    r = toku_omt_find_zero(tree->i.omt, rt_heaviside, &extra, NULL, &index, NULL);
    if (r==0) { r = EDOM; goto cleanup; }
    if (r!=DB_NOTFOUND) goto cleanup;
    insert_range = tree->malloc(sizeof(*insert_range));
    *insert_range = *range;
    if ((r = toku_omt_insert_at(tree->i.omt, insert_range, index))) goto cleanup;

    tree->numelements++;
    r = 0;
cleanup:
    if (r!=0) {
        if (insert_range) tree->free(insert_range);
    }
    return r;
}

int toku_rt_delete(toku_range_tree* tree, toku_range* range) {
    int r = ENOSYS;
    if (!tree || !range) { r = EINVAL; goto cleanup; }
    assert(!tree->allow_overlaps);

    OMTVALUE value = NULL;
    u_int32_t   index;
    rt_heavi_extra extra;
    extra.end_cmp = tree->end_cmp;
    extra.query   = range->ends;

    r = toku_omt_find_zero(tree->i.omt, rt_heaviside, &extra, &value, &index, NULL);
    if (r!=0) { r = EDOM; goto cleanup; }
    assert(value);
    toku_range* data = value;
    if (tree->end_cmp(data->ends.left,  range->ends.left) ||
        tree->end_cmp(data->ends.right, range->ends.right) ||
        tree->data_cmp(data->data,      range->data)) {
        r = EDOM;
        goto cleanup;
    }
    if ((r = toku_omt_delete_at(tree->i.omt, index))) goto cleanup;
    tree->free(data);

    tree->numelements--;
    r = 0;
cleanup:
    return r;
}

static inline int rt_neightbor(toku_range_tree* tree, toku_point* point,
                               toku_range* neighbor, BOOL* wasfound, int direction) {
    int r = ENOSYS;
    if (!tree || !point || !neighbor || !wasfound || tree->allow_overlaps) {
        r = EINVAL; goto cleanup;
    }
    u_int32_t   index;
    OMTVALUE value  = NULL;
    rt_heavi_extra extra;
    extra.end_cmp     = tree->end_cmp;
    extra.query.left  = point;
    extra.query.right = point;

    assert(direction==1 || direction==-1);
    r = toku_omt_find(tree->i.omt, rt_heaviside, &extra, direction, &value, &index, NULL);
    if (r==DB_NOTFOUND) {
        *wasfound = FALSE;
        r = 0;
        goto cleanup;
    }
    if (r!=0) goto cleanup;
    assert(value);
    toku_range* data = value;
    *wasfound = TRUE;
    *neighbor = *data;
    r = 0;
cleanup:    
    return r;    
}

int toku_rt_predecessor (toku_range_tree* tree, toku_point* point,
                         toku_range* pred, BOOL* wasfound) {
    return rt_neightbor(tree, point, pred, wasfound, -1);
}

int toku_rt_successor (toku_range_tree* tree, toku_point* point,
                       toku_range* succ, BOOL* wasfound) {
    return rt_neightbor(tree, point, succ, wasfound, 1);
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

typedef struct {
    int (*f)(toku_range*,void*);
    void* extra;
} rt_iter_info;

static int rt_iterate_helper(OMTVALUE value, u_int32_t UU(index), void* extra) {
    rt_iter_info* info = extra;
    return info->f(value, info->extra);
}

int toku_rt_iterate(toku_range_tree* tree, int (*f)(toku_range*,void*), void* extra) {
    rt_iter_info info;
    info.f = f;
    info.extra = extra;
    return toku_omt_iterate(tree->i.omt, rt_iterate_helper, &info);
}

