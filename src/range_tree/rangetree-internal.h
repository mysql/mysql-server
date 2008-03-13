/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#if !defined(TOKU_RANGE_TREE_INTERNAL_H)
#define TOKU_RANGE_TREE_INTERNAL_H
/** Export the internal representation to a sensible name */
/*  These lines will remain. */
typedef struct __toku_range_tree_local toku_range_tree_local;
struct __toku_range_tree_local;

/** \brief Internal range representation 
    Internal representation of a range tree. Some fields depend on the
    implementation of range trees, and some others are shared.
    Parameters are never modified on failure with the exception of
    buf and buflen.
 */
struct __toku_range_tree {
    //Shared fields:
    /** A comparison function, as in bsearch(3), to compare the end-points of 
        a range. It is assumed to be commutative. */
    int       (*end_cmp)(const toku_point*,const toku_point*);  
    /** A comparison function, as in bsearch(3), to compare the data associated
        with a range */
    int       (*data_cmp)(const DB_TXN*,const DB_TXN*);
    /** Whether this tree allows ranges to overlap */
    BOOL        allow_overlaps;
    /** The number of ranges in the range tree */
    u_int32_t   numelements;
    /** The user malloc function */
    void*     (*malloc) (size_t);
    /** The user free function */
    void      (*free)   (void*);
    /** The user realloc function */
    void*     (*realloc)(void*, size_t);

    toku_range_tree_local i;
};

/*
 *  Returns:
 *      0:      Point \in range
 *      < 0:    Point strictly less than the range.
 *      > 0:    Point strictly greater than the range.
 */
static inline int toku__rt_p_cmp(toku_range_tree* tree,
                           toku_point* point, toku_range* range) {
    if (tree->end_cmp(point, range->left) < 0)  return -1;
    if (tree->end_cmp(point, range->right) > 0) return 1;
    return 0;
}
    
static inline int toku__rt_increase_buffer(toku_range_tree* tree, toku_range** buf,
                                     u_int32_t* buflen, u_int32_t num) {
    assert(buf);
    //TODO: SOME ATTRIBUTE TO REMOVE NEVER EXECUTABLE ERROR: assert(buflen);
    if (*buflen < num) {
        u_int32_t temp_len = *buflen;
        while (temp_len < num) temp_len *= 2;
        toku_range* temp_buf =
                             tree->realloc(*buf, temp_len * sizeof(toku_range));
        if (!temp_buf) return errno;
        *buf = temp_buf;
        *buflen = temp_len;
    }
    return 0;
}

static inline int toku_rt_super_create(toku_range_tree** upperptree,
                   toku_range_tree** ptree,
                   int (*end_cmp)(const toku_point*,const toku_point*),
                   int (*data_cmp)(const DB_TXN*,const DB_TXN*),
                   BOOL allow_overlaps,
                   void* (*user_malloc) (size_t),
                   void  (*user_free)   (void*),
                   void* (*user_realloc)(void*, size_t)) {
    toku_range_tree* temptree;
    if (!upperptree || !ptree || !end_cmp || !data_cmp ||
        !user_malloc || !user_free || !user_realloc)              return EINVAL;
    
    temptree = (toku_range_tree*)user_malloc(sizeof(toku_range_tree));
    if (!temptree) return ENOMEM;
    
    //Any initializers go here.
    memset(temptree, 0, sizeof(*temptree));
    temptree->end_cmp        = end_cmp;
    temptree->data_cmp       = data_cmp;
    temptree->allow_overlaps = allow_overlaps;
    temptree->malloc  = user_malloc;
    temptree->free    = user_free;
    temptree->realloc = user_realloc;
    *ptree = temptree;

    return 0;
}

#endif  /* #if !defined(TOKU_RANGE_TREE_INTERNAL_H) */
