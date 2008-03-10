/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/**
   \file  locktree.c
   \brief Lock trees: implementation
*/
  

#include <locktree.h>
#include <ydb-internal.h>
#include <brt-internal.h>

/* TODO: Yoni should check that all asserts make sense instead of panic,
         and all early returns make sense instead of panic,
         and vice versa. */
/* TODO: During integration, create a db panic function to take care of this.
         The panic function will go in ydb.c.
         We may have to return the panic return code something.
         We know the DB will always return EINVAL afterwards, but
         what is the INITIAL panic return?
         ALSO maybe make ticket, maybe it should be doing DB_RUNRECOVERY after
         instead of EINVAL.
*/
/* TODO: During integration, make sure we first verify the NULL CONSISTENCY,
         (return EINVAL if necessary) before making lock tree calls. */


static inline int toku__lt_panic(toku_lock_tree *tree, int r) {
    return tree->panic(tree->db, r);
}
                
static inline int toku__lt_callback(toku_lock_tree *tree, DB_TXN* txn) {
    return tree->lock_callback ? tree->lock_callback(txn, tree) : 0;
}

const u_int32_t __toku_default_buflen = 2;

static const DBT __toku_lt_infinity;
static const DBT __toku_lt_neg_infinity;

const DBT* const toku_lt_infinity     = &__toku_lt_infinity;
const DBT* const toku_lt_neg_infinity = &__toku_lt_neg_infinity;

char* toku_lt_strerror(TOKU_LT_ERROR r) {
    if (r >= 0) return strerror(r);
    if (r == TOKU_LT_INCONSISTENT) {
        return "Locking data structures have become inconsistent.\n";
    }
    return "Unknown error in locking data structures.\n";
}
/* Compare two payloads assuming that at least one of them is infinite */ 
static inline int toku__infinite_compare(const DBT* a, const DBT* b) {
    if    (a == b)                      return  0;
    if    (a == toku_lt_infinity)       return  1;
    if    (b == toku_lt_infinity)       return -1;
    if    (a == toku_lt_neg_infinity)   return -1;
    assert(b == toku_lt_neg_infinity);  return  1;
}

static inline BOOL toku__lt_is_infinite(const DBT* p) {
    if (p == toku_lt_infinity || p == toku_lt_neg_infinity) {
        DBT* dbt = (DBT*)p;
        assert(!dbt->data && !dbt->size);
        return TRUE;
    }
    return FALSE;
}

/* Verifies that NULL data and size are consistent.
   i.e. The size is 0 if and only if the data is NULL. */
static inline int toku__lt_verify_null_key(const DBT* key) {
    if (key && key->size && !key->data) return EINVAL;
    return 0;
}

static inline DBT* toku__recreate_DBT(DBT* dbt, void* payload, u_int32_t length) {
    memset(dbt, 0, sizeof(DBT));
    dbt->data = payload;
    dbt->size = length;
    return dbt;
}

static inline int toku__lt_txn_cmp(const DB_TXN* a, const DB_TXN* b) {
    return a < b ? -1 : (a != b);
}

int toku__lt_point_cmp(const toku_point* x, const toku_point* y) {
    int partial_result;
    DBT point_1;
    DBT point_2;

    assert(x && y);
    assert(x->lt);
    assert(x->lt == y->lt);

    if (toku__lt_is_infinite(x->key_payload) ||
        toku__lt_is_infinite(y->key_payload)) {
        /* If either payload is infinite, then:
           - if duplicates are allowed, the data must be the same 
             infinite value. 
           - if duplicates are not allowed, the data is irrelevant
             In either case, we do not have to compare data: the key will
             be the sole determinant of the comparison */
        return toku__infinite_compare(x->key_payload, y->key_payload);
    }
    partial_result = x->lt->compare_fun(x->lt->db,
                     toku__recreate_DBT(&point_1, x->key_payload, x->key_len),
                     toku__recreate_DBT(&point_2, y->key_payload, y->key_len));
    if (partial_result) return partial_result;
    
    if (!x->lt->duplicates) return 0;

    if (toku__lt_is_infinite(x->data_payload) ||
        toku__lt_is_infinite(y->data_payload)) {
        return toku__infinite_compare(x->data_payload, y->data_payload);
    }
    return x->lt->dup_compare(x->lt->db,
                   toku__recreate_DBT(&point_1, x->data_payload, x->data_len),
                   toku__recreate_DBT(&point_2, y->data_payload, y->data_len));
}

/* Functions to update the range count and compare it with the
   maximum number of ranges */
static inline BOOL toku__lt_range_test_incr(toku_lock_tree* tree, u_int32_t replace) {
    assert(tree);
    assert(tree->num_ranges);
    assert(replace <= *tree->num_ranges);
    return *tree->num_ranges - replace < tree->max_ranges;
} 

static inline void toku__lt_range_incr(toku_lock_tree* tree, u_int32_t replace) {
    assert(toku__lt_range_test_incr(tree, replace));
    *tree->num_ranges -= replace;
    *tree->num_ranges += 1;
}

static inline void toku__lt_range_decr(toku_lock_tree* tree, u_int32_t ranges) {
    assert(tree);
    assert(tree->num_ranges);
    assert(*tree->num_ranges >= ranges);
    *tree->num_ranges -= ranges;
}

static inline void toku__p_free(toku_lock_tree* tree, toku_point* point) {
    assert(point);
    if (!toku__lt_is_infinite(point->key_payload)) {
        tree->free(point->key_payload);
    }
    if (!toku__lt_is_infinite(point->data_payload)) {
        tree->free(point->data_payload);
    }
    tree->free(point);
}

/*
   Allocate and copy the payload.
*/
static inline int toku__payload_copy(toku_lock_tree* tree,
                               void** payload_out, u_int32_t* len_out,
                               void*  payload_in,  u_int32_t  len_in) {
    assert(payload_out && len_out);
    if (!len_in) {
        assert(!payload_in || toku__lt_is_infinite(payload_in));
        *payload_out = payload_in;
        *len_out     = len_in;
    }
    else {
        assert(payload_in);
        *payload_out = tree->malloc((size_t)len_in);
        if (!*payload_out) return errno;
        *len_out     = len_in;
        memcpy(*payload_out, payload_in, (size_t)len_in);
    }
    return 0;
}

static inline int toku__p_makecopy(toku_lock_tree* tree, toku_point** ppoint) {
    assert(ppoint);
    toku_point*     point      = *ppoint;
    toku_point*     temp_point = NULL;
    int r;

    temp_point = (toku_point*)tree->malloc(sizeof(toku_point));
    if (0) {
        died1: tree->free(temp_point); return r; }
    if (!temp_point) return errno;
    memcpy(temp_point, point, sizeof(toku_point));

    r = toku__payload_copy(tree,
                            &temp_point->key_payload, &temp_point->key_len,
                                  point->key_payload,       point->key_len);
    if (0) {
        died2:
        if (!toku__lt_is_infinite(temp_point->key_payload)) {
            tree->free(temp_point->key_payload); }
        goto died1; }
    if (r!=0) goto died1;
    toku__payload_copy(tree,
                        &temp_point->data_payload, &temp_point->data_len,
                              point->data_payload,       point->data_len);
    if (r!=0) goto died2;
    *ppoint = temp_point;
    return 0;
}

/* Provides access to a selfread tree for a particular transaction.
   Returns NULL if it does not exist yet. */
toku_range_tree* toku__lt_ifexist_selfread(toku_lock_tree* tree, DB_TXN* txn) {
    assert(tree && txn);
    toku_rt_forest* forest = toku_rth_find(tree->rth, txn);
    return forest ? forest->self_read : NULL;
}

/* Provides access to a selfwrite tree for a particular transaction.
   Returns NULL if it does not exist yet. */
toku_range_tree* toku__lt_ifexist_selfwrite(toku_lock_tree* tree,
                                             DB_TXN* txn) {
    assert(tree && txn);
    toku_rt_forest* forest = toku_rth_find(tree->rth, txn);
    return forest ? forest->self_write : NULL;
}

/* Provides access to a selfread tree for a particular transaction.
   Creates it if it does not exist. */
static inline int toku__lt_selfread(toku_lock_tree* tree, DB_TXN* txn,
                              toku_range_tree** pselfread) {
    int r;
    assert(tree && txn && pselfread);

    toku_rt_forest* forest = toku_rth_find(tree->rth, txn);
    if (!forest) {
        /* Neither selfread nor selfwrite exist. */
        r = toku_rth_insert(tree->rth, txn);
        if (r!=0) return r;
        forest = toku_rth_find(tree->rth, txn);
    }
    assert(forest);
    if (!forest->self_read) {
        r = toku_rt_create(&forest->self_read,
                           toku__lt_point_cmp, toku__lt_txn_cmp,
                           FALSE,
                           tree->malloc, tree->free, tree->realloc);
        if (r!=0) return r;
        assert(forest->self_read);
    }
    *pselfread = forest->self_read;
    return 0;
}

/* Provides access to a selfwrite tree for a particular transaction.
   Creates it if it does not exist. */
static inline int toku__lt_selfwrite(toku_lock_tree* tree, DB_TXN* txn,
                               toku_range_tree** pselfwrite) {
    int r;
    assert(tree && txn && pselfwrite);

    toku_rt_forest* forest = toku_rth_find(tree->rth, txn);
    if (!forest) {
        r = toku_rth_insert(tree->rth, txn);
        if (r!=0) return r;
        forest = toku_rth_find(tree->rth, txn);
    }
    assert(forest);
    if (!forest->self_write) {
        r = toku_rt_create(&forest->self_write,
                           toku__lt_point_cmp, toku__lt_txn_cmp,
                           FALSE,
                           tree->malloc, tree->free, tree->realloc);
        if (r!=0) return r;
        assert(forest->self_write);
    }
    *pselfwrite = forest->self_write;
    return 0;
}


/*
    This function only supports non-overlapping trees.
    Uses the standard definition of dominated from the design document.
    Determines whether 'query' is dominated by 'rt'.
*/
static inline int toku__lt_rt_dominates(toku_lock_tree* tree, toku_range* query,
                                  toku_range_tree* rt, BOOL* dominated) {
    assert(tree && query && dominated);
    if (!rt) {
        *dominated = FALSE;
        return 0;
    }
    
    BOOL            allow_overlaps;
    const u_int32_t query_size = 1;
    toku_range      buffer[query_size];
    u_int32_t       buflen     = query_size;
    toku_range*     buf        = &buffer[0];
    u_int32_t       numfound;
    int             r;

    /* Sanity check. (Function only supports non-overlap range trees.) */
    r = toku_rt_get_allow_overlaps(rt, &allow_overlaps);
    if (r!=0) return r;
    assert(!allow_overlaps);

    r = toku_rt_find(rt, query, query_size, &buf, &buflen, &numfound);
    if (r!=0) return r;
    if (numfound == 0) {
        *dominated = FALSE;
        return 0;
    }
    assert(numfound == 1);
    *dominated = (toku__lt_point_cmp(query->left,  buf[0].left) >= 0 &&
                  toku__lt_point_cmp(query->right, buf[0].right) <= 0);
    return 0;
}

typedef enum
       {TOKU_NO_CONFLICT, TOKU_MAYBE_CONFLICT, TOKU_YES_CONFLICT} toku_conflict;
/*
    This function checks for conflicts in the borderwrite tree.
    If no range overlaps, there is no conflict.
    If >= 2 ranges overlap the query then, by definition of borderwrite,
    at least one overlapping regions must not be 'self'. Design document
    explains why this MUST cause a conflict.
    If exactly one range overlaps and its data == self, there is no conflict.
    If exactly one range overlaps and its data != self, there might be a
    conflict.  We need to check the 'peer'write table to verify.
*/
static inline int toku__lt_borderwrite_conflict(toku_lock_tree* tree, DB_TXN* self,
                                       toku_range* query,
                                       toku_conflict* conflict, DB_TXN** peer) {
    assert(tree && self && query && conflict && peer);
    toku_range_tree* rt = tree->borderwrite;
    assert(rt);

    const u_int32_t query_size = 2;
    toku_range   buffer[query_size];
    u_int32_t     buflen     = query_size;
    toku_range*  buf        = &buffer[0];
    u_int32_t     numfound;
    int          r;

    r = toku_rt_find(rt, query, query_size, &buf, &buflen, &numfound);
    if (r!=0) return r;
    assert(numfound <= query_size);
    *peer = NULL;
    if      (numfound == 2) *conflict = TOKU_YES_CONFLICT;
    else if (numfound == 0 || buf[0].data == self) *conflict = TOKU_NO_CONFLICT;
    else {
        *conflict = TOKU_MAYBE_CONFLICT;
        *peer = buf[0].data;
    }
    return 0;
}

/*
    Determines whether 'query' meets 'rt'.
    This function supports only non-overlapping trees with homogeneous 
    transactions, i.e., a selfwrite or selfread table only.
    Uses the standard definition of 'query' meets 'tree' at 'data' from the
    design document.
*/
static inline int toku__lt_meets(toku_lock_tree* tree, toku_range* query, 
                           toku_range_tree* rt, BOOL* met) {
    assert(tree && query && rt && met);
    const u_int32_t query_size = 1;
    toku_range   buffer[query_size];
    u_int32_t     buflen     = query_size;
    toku_range*  buf        = &buffer[0];
    u_int32_t     numfound;
    int          r;
    BOOL         allow_overlaps;

    /* Sanity check. (Function only supports non-overlap range trees.) */
    r = toku_rt_get_allow_overlaps(rt, &allow_overlaps);
    if (r!=0) return r;
    assert(!allow_overlaps);

    r = toku_rt_find(rt, query, query_size, &buf, &buflen, &numfound);
    if (r!=0) return r;
    assert(numfound <= query_size);
    *met = numfound != 0;
    return 0;
}

/* 
    Determines whether 'query' meets 'rt' at txn2 not equal to txn.
    This function supports all range trees, but queries must be a single point. 
    Uses the standard definition of 'query' meets 'tree' at 'data' from the
    design document.
*/
static inline int toku__lt_meets_peer(toku_lock_tree* tree, toku_range* query, 
                                       toku_range_tree* rt, BOOL is_homogenous,
                                       DB_TXN* self, BOOL* met) {
    assert(tree && query && rt && self && met);
    assert(query->left == query->right);

    const u_int32_t query_size = is_homogenous ? 1 : 2;
    toku_range   buffer[2];
    u_int32_t    buflen     = query_size;
    toku_range*  buf        = &buffer[0];
    u_int32_t    numfound;
    int          r;

    r = toku_rt_find(rt, query, query_size, &buf, &buflen, &numfound);
    if (r!=0) return r;
    assert(numfound <= query_size);
    *met = numfound == 2 || (numfound == 1 && buf[0].data != self);
    return 0;
}

/*
    Utility function to implement: (from design document)
    if K meets E at v'!=t and K meets W_v' then return failure.
*/
static inline int toku__lt_check_borderwrite_conflict(toku_lock_tree* tree,
                                               DB_TXN* txn, toku_range* query) {
    assert(tree && txn && query);
    toku_conflict conflict;
    DB_TXN* peer;
    toku_range_tree* peer_selfwrite;
    int r;
    
    r = toku__lt_borderwrite_conflict(tree, txn, query, &conflict, &peer);
    if (r!=0) return r;
    if (conflict == TOKU_MAYBE_CONFLICT) {
        assert(peer);
        peer_selfwrite = toku__lt_ifexist_selfwrite(tree, peer);
        if (!peer_selfwrite) return toku__lt_panic(tree, TOKU_LT_INCONSISTENT);

        BOOL met;
        r = toku__lt_meets(tree, query, peer_selfwrite, &met);
        if (r!=0)   return r;
        conflict = met ? TOKU_YES_CONFLICT : TOKU_NO_CONFLICT;
    }
    if    (conflict == TOKU_YES_CONFLICT) return DB_LOCK_DEADLOCK;
    assert(conflict == TOKU_NO_CONFLICT);
    return 0;
}

static inline void toku__payload_from_dbt(void** payload, u_int32_t* len,
                                           const DBT* dbt) {
    assert(payload && len && dbt);
    if (toku__lt_is_infinite(dbt)) *payload = (void*)dbt;
    else if (!dbt->size) {
        *payload = NULL;
        *len     = 0;
    } else {
        assert(dbt->data);
        *payload = dbt->data;
        *len     = dbt->size;
    }
}

static inline void toku__init_point(toku_point* point, toku_lock_tree* tree,
                              const DBT* key, const DBT* data) {
    assert(point && tree && key);
    assert(!tree->duplicates == !data);
    memset(point, 0, sizeof(toku_point));
    point->lt = tree;

    toku__payload_from_dbt(&point->key_payload, &point->key_len, key);
    if (tree->duplicates) {
        assert(data);
        toku__payload_from_dbt(&point->data_payload, &point->data_len, data);
    }
    else {
        assert(data == NULL);
        point->data_payload = NULL;
        point->data_len     = 0;
    }
}

static inline void toku__init_query(toku_range* query,
                              toku_point* left, toku_point* right) {
    query->left  = left;
    query->right = right;
    query->data  = NULL;
}

/*
    Memory ownership: 
     - to_insert we own (it's static)
     - to_insert.left, .right are toku_point's, and we own them.
       If we have consolidated, we own them because we had allocated
       them earlier, but
       if we have not consolidated we need to gain ownership now: 
       we will gain ownership by copying all payloads and 
       allocating the points. 
     - to_insert.{left,right}.{key_payload, data_payload} are owned by lt,
       we made copies from the DB at consolidation time 
*/

static inline void toku__init_insert(toku_range* to_insert,
                               toku_point* left, toku_point* right,
                               DB_TXN* txn) {
    to_insert->left  = left;
    to_insert->right = right;
    to_insert->data  = txn;
}

/* Returns whether the point already exists
   as an endpoint of the given range. */
static inline BOOL toku__lt_p_independent(toku_point* point, toku_range* range) {
    assert(point && range);
    return point != range->left && point != range->right;
}

static inline int toku__lt_extend_extreme(toku_lock_tree* tree,toku_range* to_insert,
                                    BOOL* alloc_left, BOOL* alloc_right,
                                    u_int32_t numfound) {
    assert(to_insert && tree && alloc_left && alloc_right);
    u_int32_t i;
    assert(numfound <= tree->buflen);
    for (i = 0; i < numfound; i++) {
        int c;
        /* Find the extreme left end-point among overlapping ranges */
        if ((c = toku__lt_point_cmp(tree->buf[i].left, to_insert->left))
            <= 0) {
            if ((!*alloc_left && c == 0) ||
                !toku__lt_p_independent(tree->buf[i].left, to_insert)) {
                return toku__lt_panic(tree, TOKU_LT_INCONSISTENT); }
            *alloc_left      = FALSE;
            to_insert->left  = tree->buf[i].left;
        }
        /* Find the extreme right end-point */
        if ((c = toku__lt_point_cmp(tree->buf[i].right, to_insert->right))
            >= 0) {
            if ((!*alloc_right && c == 0) ||
                (tree->buf[i].right == to_insert->left &&
                 tree->buf[i].left  != to_insert->left) ||
                 tree->buf[i].right == to_insert->right) {
                return toku__lt_panic(tree, TOKU_LT_INCONSISTENT); }
            *alloc_right     = FALSE;
            to_insert->right = tree->buf[i].right;
        }
    }
    return 0;
}

static inline int toku__lt_alloc_extreme(toku_lock_tree* tree, toku_range* to_insert,
                                   BOOL alloc_left, BOOL* alloc_right) {
    assert(to_insert && alloc_right);
    BOOL copy_left = FALSE;
    int r;
    
    /* The pointer comparison may speed up the evaluation in some cases, 
       but it is not strictly needed */
    if (alloc_left && alloc_right &&
        (to_insert->left == to_insert->right ||
         toku__lt_point_cmp(to_insert->left, to_insert->right) == 0)) {
        *alloc_right = FALSE;
        copy_left    = TRUE;
    }

    if (alloc_left) {
        r = toku__p_makecopy(tree, &to_insert->left);
        if (0) { died1:
            if (alloc_left) toku__p_free(tree, to_insert->left); return r; }
        if (r!=0) return r;
    }
    if (*alloc_right) {
        assert(!copy_left);
        r = toku__p_makecopy(tree, &to_insert->right);
        if (r!=0) goto died1;
    }
    else if (copy_left) to_insert->right = to_insert->left;
    return 0;
}

static inline int toku__lt_delete_overlapping_ranges(toku_lock_tree* tree,
                                               toku_range_tree* rt,
                                               u_int32_t numfound) {
    assert(tree && rt);
    int r;
    u_int32_t i;
    assert(numfound <= tree->buflen);
    for (i = 0; i < numfound; i++) {
        r = toku_rt_delete(rt, &tree->buf[i]);
        if (r!=0) return r;
    }
    return 0;
}

static inline int toku__lt_free_points(toku_lock_tree* tree, toku_range* to_insert,
                                  u_int32_t numfound, toku_range_tree *rt) {
    assert(tree && to_insert);
    assert(numfound <= tree->buflen);

    int r;
    u_int32_t i;
    for (i = 0; i < numfound; i++) {
        if (rt != NULL) {
            r = toku_rt_delete(rt, &tree->buf[i]);
            if (r!=0) return toku__lt_panic(tree, r);
        }
        /*
           We will maintain the invariant: (separately for read and write
           environments)
           (toku__lt_point_cmp(a, b) == 0 && a.txn == b.txn) => a == b
        */
        /* Do not double-free. */
        if (tree->buf[i].right != tree->buf[i].left &&
            toku__lt_p_independent(tree->buf[i].right, to_insert)) {
            toku__p_free(tree, tree->buf[i].right);
        }
        if (toku__lt_p_independent(tree->buf[i].left,  to_insert)) {
            toku__p_free(tree, tree->buf[i].left);
        }
    }
    return 0;
}

/* Consolidate the new range and all the overlapping ranges */
static inline int toku__consolidate(toku_lock_tree* tree,
                              toku_range* query, toku_range* to_insert,
                              DB_TXN* txn) {
    int r;
    BOOL             alloc_left    = TRUE;
    BOOL             alloc_right   = TRUE;
    toku_range_tree* selfread;
    assert(tree && to_insert && txn);
#if !defined(TOKU_RT_NOOVERLAPS)
    toku_range_tree* mainread      = tree->mainread;
    assert(mainread);
#endif
    /* Find the self read tree */
    r = toku__lt_selfread(tree, txn, &selfread);
    if (r!=0) return r;
    assert(selfread);
    /* Find all overlapping ranges in the self-read */
    u_int32_t numfound;
    r = toku_rt_find(selfread, query, 0, &tree->buf, &tree->buflen,
                     &numfound);
    if (r!=0) return r;
    assert(numfound <= tree->buflen);
    /* Find the extreme left and right point of the consolidated interval */
    r = toku__lt_extend_extreme(tree, to_insert, &alloc_left, &alloc_right,
                                 numfound);
    if (r!=0) return r;
    if (!toku__lt_range_test_incr(tree, numfound)) return ENOMEM;
    /* Allocate the consolidated range */
    r = toku__lt_alloc_extreme(tree, to_insert, alloc_left, &alloc_right);
    if (0) { died1:
        if (alloc_left)  toku__p_free(tree, to_insert->left);
        if (alloc_right) toku__p_free(tree, to_insert->right); return r; }
    if (r!=0) return r;
    /* From this point on we have to panic if we cannot finish. */
    /* Delete overlapping ranges from selfread ... */
    r = toku__lt_delete_overlapping_ranges(tree, selfread, numfound);
    if (r!=0) return toku__lt_panic(tree, r);
    /* ... and mainread.
       Growth direction: if we had no overlaps, the next line
       should be commented out */
#if !defined(TOKU_RT_NOOVERLAPS)
    r = toku__lt_delete_overlapping_ranges(tree, mainread, numfound);
    if (r!=0) return toku__lt_panic(tree, r);
#endif
    /* Free all the points from ranges in tree->buf[0]..tree->buf[numfound-1] */
    toku__lt_free_points(tree, to_insert, numfound, NULL);
    /* We don't necessarily need to panic after here unless numfound > 0
       Which indicates we deleted something. */
    /* Insert extreme range into selfread. */
    /* VL */
    r = toku_rt_insert(selfread, to_insert);
#if !defined(TOKU_RT_NOOVERLAPS)
    int r2;
    if (0) { died2: r2 = toku_rt_delete(selfread, to_insert);
        if (r2!=0) return toku__lt_panic(tree, r2); goto died1; }
#endif
    if (r!=0) {
        /* If we deleted/merged anything, this is a panic situation. */
        if (numfound) return toku__lt_panic(tree, TOKU_LT_INCONSISTENT);
        goto died1; }
#if !defined(TOKU_RT_NOOVERLAPS)
    /* Insert extreme range into mainread. */
    assert(tree->mainread);
    r = toku_rt_insert(tree->mainread, to_insert);
    if (r!=0) {
        /* If we deleted/merged anything, this is a panic situation. */
        if (numfound) return toku__lt_panic(tree, TOKU_LT_INCONSISTENT);
        goto died2; }
#endif
    toku__lt_range_incr(tree, numfound);
    return 0;
}

static inline void toku__lt_init_full_query(toku_lock_tree* tree, toku_range* query,
                                      toku_point* left, toku_point* right) {
    toku__init_point(left,  tree,       (DBT*)toku_lt_neg_infinity,
                      tree->duplicates ? (DBT*)toku_lt_neg_infinity : NULL);
    toku__init_point(right, tree,       (DBT*)toku_lt_infinity,
                      tree->duplicates ? (DBT*)toku_lt_infinity : NULL);
    toku__init_query(query, left, right);
}

static inline int toku__lt_free_contents_slow(toku_lock_tree* tree,
                                        toku_range_tree* rt,
                                        toku_range_tree* rtdel) {
    int r;
    toku_range query;
    toku_point left;
    toku_point right;
    u_int32_t numfound;

    toku__lt_init_full_query(tree, &query, &left, &right);
    /*
        To free space the fast way, we need to allocate more space.
        Since we can't, we free the slow way.
        We do not optimize this, we take one entry at a time,
        delete it from the tree, and then free the memory.
    */
    do {
        r = toku_rt_find(rt, &query, 1, &tree->buf, &tree->buflen, &numfound);
        if (r!=0)      break;
        if (!numfound) break;
        assert(numfound == 1);
        r = toku_rt_delete(rt, &tree->buf[0]);
        if (r!=0)      break;
        r = toku__lt_free_points(tree, &query, numfound, rtdel);
    } while (TRUE);
    return r;
}

static inline int toku__lt_free_contents(toku_lock_tree* tree, toku_range_tree* rt,
                                   toku_range_tree *rtdel) {
    assert(tree);
    if (!rt) return 0;
    
    int r;
    int r2;

    toku_range query;
    toku_point left;
    toku_point right;
    toku__lt_init_full_query(tree, &query, &left, &right);

    u_int32_t numfound;
    r = toku_rt_find(rt, &query, 0, &tree->buf, &tree->buflen, &numfound);
    if (r==0)           r = toku__lt_free_points(tree, &query, numfound, 
                                                  rtdel);
    else if (r==ENOMEM) r = toku__lt_free_contents_slow(tree, rt, rtdel);
    r2 = toku_rt_close(rt);
    assert(r2 == 0);
    return r;
}

static inline BOOL toku__r_backwards(toku_range* range) {
    assert(range && range->left && range->right);
    toku_point* left  = (toku_point*)range->left;
    toku_point* right = (toku_point*)range->right;

    /* Optimization: if all the pointers are equal, clearly left == right. */
    return (left->key_payload  != right->key_payload ||
            left->data_payload != right->data_payload) &&
           toku__lt_point_cmp(left, right) > 0;
}


static inline int toku__lt_preprocess(toku_lock_tree* tree, DB_TXN* txn,
                                const DBT* key_left,  const DBT** pdata_left,
                                const DBT* key_right, const DBT** pdata_right,
                                toku_point* left, toku_point* right,
                                toku_range* query) {
    assert(pdata_left && pdata_right);
    if (!tree || !txn || !key_left || !key_right)           return EINVAL;
    if (!tree->duplicates) *pdata_right = *pdata_left = NULL;
    const DBT* data_left  = *pdata_left;
    const DBT* data_right = *pdata_right;
    if (tree->duplicates  && (!data_left || !data_right))   return EINVAL;
    if (tree->duplicates  && key_left != data_left &&
        toku__lt_is_infinite(key_left))                    return EINVAL;
    if (tree->duplicates  && key_right != data_right &&
        toku__lt_is_infinite(key_right))                   return EINVAL;

    int r;
    /* Verify that NULL keys have payload and size that are mutually 
       consistent*/
    if ((r = toku__lt_verify_null_key(key_left))   != 0) return r;
    if ((r = toku__lt_verify_null_key(data_left))  != 0) return r;
    if ((r = toku__lt_verify_null_key(key_right))  != 0) return r;
    if ((r = toku__lt_verify_null_key(data_right)) != 0) return r;

    toku__init_point(left,  tree, key_left,  data_left);
    toku__init_point(right, tree, key_right, data_right);
    toku__init_query(query, left, right);
    /* Verify left <= right, otherwise return EDOM. */
    if (toku__r_backwards(query))                          return EDOM;
    tree->dups_final = TRUE;

    r = toku__lt_callback(tree, txn);
    if (r!=0) return r;
    return 0;
}

static inline int toku__lt_get_border(toku_lock_tree* tree, BOOL in_borderwrite,
                                toku_range* pred, toku_range* succ,
                                BOOL* found_p,    BOOL* found_s,
                                toku_range* to_insert) {
    assert(tree && pred && succ && found_p && found_s);                                    
    int r;
    toku_range_tree* rt;
    rt = in_borderwrite ? tree->borderwrite : 
                          toku__lt_ifexist_selfwrite(tree, tree->buf[0].data);
    if (!rt)  return toku__lt_panic(tree, TOKU_LT_INCONSISTENT);
    r = toku_rt_predecessor(rt, to_insert->left,  pred, found_p);
    if (r!=0) return r;
    r = toku_rt_successor  (rt, to_insert->right, succ, found_s);
    if (r!=0) return r;
    return 0;
}

static inline int toku__lt_expand_border(toku_lock_tree* tree, toku_range* to_insert,
                                   toku_range* pred, toku_range* succ,
                                   BOOL  found_p,    BOOL  found_s) {
    assert(tree && to_insert && pred && succ);
    int r;
    if      (found_p && pred->data == to_insert->data) {
        r = toku_rt_delete(tree->borderwrite, pred);
        if (r!=0) return r;
        to_insert->left = pred->left;
    }
    else if (found_s && succ->data == to_insert->data) {
        r = toku_rt_delete(tree->borderwrite, succ);
        if (r!=0) return r;
        to_insert->right = succ->right;
    }
    return 0;
}

static inline int toku__lt_split_border(toku_lock_tree* tree, toku_range* to_insert,
                                   toku_range* pred, toku_range* succ,
                                   BOOL  found_p,    BOOL  found_s) {
    assert(tree && to_insert && pred && succ);
    int r;
    assert(tree->buf[0].data != to_insert->data);
    if (!found_s || !found_p) return toku__lt_panic(tree, TOKU_LT_INCONSISTENT);

    r = toku_rt_delete(tree->borderwrite, &tree->buf[0]);
    if (r!=0) return toku__lt_panic(tree, r);

    pred->left  = tree->buf[0].left;
    succ->right = tree->buf[0].right;
    if (toku__r_backwards(pred) || toku__r_backwards(succ)) {
        return toku__lt_panic(tree, TOKU_LT_INCONSISTENT);}

    r = toku_rt_insert(tree->borderwrite, pred);
    if (r!=0) return toku__lt_panic(tree, r);
    r = toku_rt_insert(tree->borderwrite, succ);
    if (r!=0) return toku__lt_panic(tree, r);
    return 0;
}

/*
    Algorithm:
    Find everything (0 or 1 ranges) it overlaps in borderwrite.
    If 0:
        Retrieve predecessor and successor.
        if both found
            assert(predecessor.data != successor.data)
        if predecessor found, and pred.data == my.data
            'merge' (extend to) predecessor.left
                To do this, delete predecessor,
                insert combined me and predecessor.
                then done/return
        do same check for successor.
        if not same, then just insert the actual item into borderwrite.
     if found == 1:
        If data == my data, done/return
        (overlap someone else, retrieve the peer)
        Get the selfwrite for the peer.
        Get successor of my point in peer_selfwrite
        get pred of my point in peer_selfwrite.
        Old range = O.left, O.right
        delete old range,
        insert      O.left, pred.right
        insert      succ.left, O.right
        NO MEMORY GETS FREED!!!!!!!!!!, it all is tied to selfwrites.
        insert point,point into borderwrite
     done with borderwrite.
     insert point,point into selfwrite.
*/
static inline int toku__lt_borderwrite_insert(toku_lock_tree* tree,
                                        toku_range* query,
                                        toku_range* to_insert) {
    assert(tree && query && to_insert);
    int r;
    toku_range_tree* borderwrite = tree->borderwrite;   assert(borderwrite);
    const u_int32_t query_size = 1;

    u_int32_t numfound;
    r = toku_rt_find(borderwrite, query, query_size, &tree->buf, &tree->buflen,
                     &numfound);
    if (r!=0) return toku__lt_panic(tree, r);
    assert(numfound <= query_size);

    /* No updated needed in borderwrite: we return right away. */
    if (numfound == 1 && tree->buf[0].data == to_insert->data) return 0;

    /* Find predecessor and successors */
    toku_range pred;
    toku_range succ;
    BOOL found_p;
    BOOL found_s;

    r = toku__lt_get_border(tree, numfound == 0, &pred, &succ, 
                             &found_p, &found_s, to_insert);
    if (r!=0) return toku__lt_panic(tree, r);
    
    if (numfound == 0) {
        if (found_p && found_s && pred.data == succ.data) {
            return toku__lt_panic(tree, TOKU_LT_INCONSISTENT); }
        r = toku__lt_expand_border(tree, to_insert, &pred,   &succ,
                                                      found_p, found_s);
        if (r!=0) return toku__lt_panic(tree, r);
    }
    else {  
        r = toku__lt_split_border( tree, to_insert, &pred, &succ, 
                                                      found_p, found_s);
        if (r!=0) return toku__lt_panic(tree, r);
    }
    r = toku_rt_insert(borderwrite, to_insert);
    if (r!=0) return toku__lt_panic(tree, r);
    return 0;
}

int toku_lt_create(toku_lock_tree** ptree, DB* db, BOOL duplicates,
                   int   (*panic)(DB*, int), u_int32_t max_ranges,
                   u_int32_t* num_ranges,
                   int   (*compare_fun)(DB*,const DBT*,const DBT*),
                   int   (*dup_compare)(DB*,const DBT*,const DBT*),
                   void* (*user_malloc) (size_t),
                   void  (*user_free)   (void*),
                   void* (*user_realloc)(void*, size_t)) {
    if (!ptree || !db || !compare_fun || !dup_compare || !panic ||
        !max_ranges || !num_ranges || !user_malloc || !user_free ||
        !user_realloc) { return EINVAL; }
    int r;

    toku_lock_tree* tmp_tree = (toku_lock_tree*)user_malloc(sizeof(*tmp_tree));
    if (0) { died1: user_free(tmp_tree); return r; }
    if (!tmp_tree) return errno;
    memset(tmp_tree, 0, sizeof(toku_lock_tree));
    tmp_tree->db               = db;
    tmp_tree->duplicates       = duplicates;
    tmp_tree->panic            = panic;
    tmp_tree->max_ranges       = max_ranges;
    tmp_tree->num_ranges       = num_ranges;
    tmp_tree->compare_fun      = compare_fun;
    tmp_tree->dup_compare      = dup_compare;
    tmp_tree->malloc           = user_malloc;
    tmp_tree->free             = user_free;
    tmp_tree->realloc          = user_realloc;
#if defined(TOKU_RT_NOOVERLAPS)
    if (0) { died2: goto died1; }
#else
    r = toku_rt_create(&tmp_tree->mainread,
                       toku__lt_point_cmp, toku__lt_txn_cmp, TRUE,
                       user_malloc, user_free, user_realloc);
    if (0) { died2: toku_rt_close(tmp_tree->mainread); goto died1; }
    if (r!=0) goto died1;
#endif
    r = toku_rt_create(&tmp_tree->borderwrite,
                       toku__lt_point_cmp, toku__lt_txn_cmp, FALSE,
                       user_malloc, user_free, user_realloc);
    if (0) { died3: toku_rt_close(tmp_tree->borderwrite); goto died2; }
    if (r!=0) goto died2;
    r = toku_rth_create(&tmp_tree->rth, user_malloc, user_free, user_realloc);
    if (0) { died4: toku_rth_close(tmp_tree->rth); goto died3; }
    if (r!=0) goto died3;
    tmp_tree->buflen = __toku_default_buflen;
    tmp_tree->buf    = (toku_range*)
                        user_malloc(tmp_tree->buflen * sizeof(toku_range));
    if (!tmp_tree->buf) { r = errno; goto died4; }
    *ptree = tmp_tree;
    return 0;
}

int toku_lt_close(toku_lock_tree* tree) {
    if (!tree)                                              return EINVAL;
    int r;
    int r2 = 0;
#if !defined(TOKU_RT_NOOVERLAPS)
    r = toku_rt_close(tree->mainread);
    if        (r!=0) r2 = r;
#endif
    r = toku_rt_close(tree->borderwrite);
    if (!r2 && r!=0) r2 = r;

    toku_rth_start_scan(tree->rth);
    toku_rt_forest* forest;
    
    while ((forest = toku_rth_next(tree->rth)) != NULL) {
        r = toku__lt_free_contents(tree, forest->self_read,  NULL);
        if (!r2 && r!=0) r2 = r;
        r = toku__lt_free_contents(tree, forest->self_write, NULL);
        if (!r2 && r!=0) r2 = r;
    }
    toku_rth_close(tree->rth);

    tree->free(tree->buf);
    tree->free(tree);
    return r2;
}

int toku_lt_acquire_read_lock(toku_lock_tree* tree, DB_TXN* txn,
                              const DBT* key, const DBT* data) {
    return toku_lt_acquire_range_read_lock(tree, txn, key, data, key, data);
}

int toku_lt_acquire_range_read_lock(toku_lock_tree* tree, DB_TXN* txn,
                                  const DBT* key_left,  const DBT* data_left,
                                  const DBT* key_right, const DBT* data_right) {
    int r;
    toku_point left;
    toku_point right;
    toku_range query;
    BOOL dominated;
    
    r = toku__lt_preprocess(tree, txn, key_left,  &data_left,
                                        key_right, &data_right,
                                           &left,       &right,
                             &query);
    if (r!=0) return r;

    /*
        For transaction 'txn' to acquire a read-lock on range 'K'=['left','right']:
            if 'K' is dominated by selfwrite('txn') then return success.
            else if 'K' is dominated by selfread('txn') then return success.
            else if 'K' meets borderwrite at 'peer' ('peer'!='txn') &&
                    'K' meets selfwrite('peer') then return failure.
            else
                add 'K' to selfread('txn') and selfwrite('txn').
                This requires merging.. see below.
    */

    /* if 'K' is dominated by selfwrite('txn') then return success. */
    r = toku__lt_rt_dominates(tree, &query, 
                            toku__lt_ifexist_selfwrite(tree, txn), &dominated);
    if (r || dominated) return r;

    /* else if 'K' is dominated by selfread('txn') then return success. */
    r = toku__lt_rt_dominates(tree, &query, 
                            toku__lt_ifexist_selfread(tree, txn), &dominated);
    if (r || dominated) return r; 
    /*
        else if 'K' meets borderwrite at 'peer' ('peer'!='txn') &&
                'K' meets selfwrite('peer') then return failure.
    */
    r = toku__lt_check_borderwrite_conflict(tree, txn, &query);
    if (r!=0) return r;
    /* Now need to merge, copy the memory and insert. */
    toku_range  to_insert;
    toku__init_insert(&to_insert, &left, &right, txn);
    /* Consolidate the new range and all the overlapping ranges */
    return toku__consolidate(tree, &query, &to_insert, txn);
}

static int toku__lt_write_point_conflicts_reads(toku_lock_tree* tree,
                                               DB_TXN* txn, toku_range* query) {
    int r    = 0;
    BOOL met = FALSE;
#if defined(TOKU_RT_NOOVERLAPS)
    toku_rth_start_scan(tree->rth);
    toku_rt_forest* forest;
    
    while ((forest = toku_rth_next(tree->rth)) != NULL) {
        if (forest->self_read != NULL && forest->hash_key != txn) {
            r = toku__lt_meets_peer(tree, query, forest->self_read, TRUE, txn,
                            &met);
            if (r!=0) goto cleanup;
            if (met)  { r = DB_LOCK_DEADLOCK; goto cleanup; }
        }
    }
#else
    toku_range_tree* mainread = tree->mainread; assert(mainread);
    r = toku__lt_meets_peer(tree, query, mainread, FALSE, txn, &met);
    if (r!=0) goto cleanup;
    if (met)  { r = DB_LOCK_DEADLOCK; goto cleanup; }
#endif
    r = 0;
cleanup:
    return r;
}

int toku_lt_acquire_write_lock(toku_lock_tree* tree, DB_TXN* txn,
                               const DBT* key, const DBT* data) {
    int r;
    toku_point endpoint;
    toku_range query;
    BOOL dominated;
    
    r = toku__lt_preprocess(tree, txn, key,      &data,
                                        key,      &data,
                                       &endpoint, &endpoint,
                             &query);
    if (r!=0) return r;
    /* if 'K' is dominated by selfwrite('txn') then return success. */
    r = toku__lt_rt_dominates(tree, &query, 
                            toku__lt_ifexist_selfwrite(tree, txn), &dominated);
    if (r || dominated) return r;
    /* else if K meets mainread at 'txn2' then return failure */
    r = toku__lt_write_point_conflicts_reads(tree, txn, &query);
    if (r!=0) return r;
    /*
        else if 'K' meets borderwrite at 'peer' ('peer'!='txn') &&
                'K' meets selfwrite('peer') then return failure.
    */
    r = toku__lt_check_borderwrite_conflict(tree, txn, &query);
    if (r!=0) return r;
    /*  Now need to copy the memory and insert.
        No merging required in selfwrite.
        This is a point, and if merging was possible it would have been
        dominated by selfwrite.
    */
    toku_range to_insert;
    toku__init_insert(&to_insert, &endpoint, &endpoint, txn);
    if (!toku__lt_range_test_incr(tree, 0)) return ENOMEM; 

    BOOL dummy = TRUE;
    r = toku__lt_alloc_extreme(tree, &to_insert, TRUE, &dummy);
    if (0) { died1:  toku__p_free(tree, to_insert.left);   return r; }
    if (r!=0) return r;
    toku_range_tree* selfwrite;
    r = toku__lt_selfwrite(tree, txn, &selfwrite);
    if (r!=0) goto died1;
    assert(selfwrite);
    /* VL change this: r = toku_rt_insert(selfwrite, &to_insert); */
    r = toku_rt_insert(selfwrite, &to_insert);
    if (r!=0) goto died1;
    /* Need to update borderwrite. */
    r = toku__lt_borderwrite_insert(tree, &query, &to_insert);
    if (r!=0) return toku__lt_panic(tree, r);
    toku__lt_range_incr(tree, 0);
    return 0;
}

int toku_lt_acquire_range_write_lock(toku_lock_tree* tree, DB_TXN* txn,
                                  const DBT* key_left,  const DBT* data_left,
                                  const DBT* key_right, const DBT* data_right) {
    int r;
    toku_point left;
    toku_point right;
    toku_range query;

    if (key_left == key_right &&
        (data_left == data_right || (tree && !tree->duplicates))) {
        return toku_lt_acquire_write_lock(tree, txn, key_left, data_left);
    }

    r = toku__lt_preprocess(tree, txn, key_left,  &data_left,
                                        key_right, &data_right,
                                           &left,       &right,
                             &query);
    if (r!=0) return r;

    return ENOSYS;
    //We are not ready for this.
    //Not needed for Feb 1 release.
}


static inline int toku__sweep_border(toku_lock_tree* tree, toku_range* range) {
    assert(tree && range);
    toku_range_tree* borderwrite = tree->borderwrite;
    assert(borderwrite);

    /* Find overlapping range in borderwrite */
    int r;
    const u_int32_t query_size = 1;
    toku_range      buffer[query_size];
    u_int32_t       buflen     = query_size;
    toku_range*     buf        = &buffer[0];
    u_int32_t       numfound;

    toku_range query = *range;
    query.data = NULL;
    r = toku_rt_find(borderwrite, &query, query_size, &buf, &buflen, &numfound);
    if (r!=0) return r;
    assert(numfound <= query_size);
    
    /*  If none exists or data is not ours (we have already deleted the real
        overlapping range), continue to the end of the loop (i.e., return) */
    if (!numfound || buf[0].data != range->data) return 0;
    assert(numfound == 1);

    /* Delete s from borderwrite */
    r = toku_rt_delete(borderwrite, &buf[0]);
    if (r!=0) return r;

    /* Find pred(s.left), and succ(s.right) */
    toku_range pred;
    toku_range succ;
    BOOL found_p;
    BOOL found_s;

    r = toku__lt_get_border(tree, TRUE, &pred, &succ, &found_p, &found_s,
                             &buf[0]);
    if (r!=0) return r;
    if (found_p && found_s && pred.data == succ.data &&
        pred.data == buf[0].data) { 
        return toku__lt_panic(tree, TOKU_LT_INCONSISTENT); }

    /* If both found and pred.data=succ.data, merge pred and succ (expand?)
       free_points */
    if (!found_p || !found_s || pred.data != succ.data) return 0;

    r = toku_rt_delete(borderwrite, &pred);
    if (r!=0) return r;
    r = toku_rt_delete(borderwrite, &succ);
    if (r!=0) return r;

    pred.right = succ.right;
    r = toku_rt_insert(borderwrite, &pred);
    if (r!=0) return r;

    return 0;
}

/*
  Algorithm:
    For each range r in selfwrite
      Find overlapping range s in borderwrite 
      If none exists or data is not ours (we have already deleted the real
        overlapping range), continue
      Delete s from borderwrite
      Find pred(s.left), and succ(s.right)
      If both found and pred.data=succ.data, merge pred and succ (expand?)
    free_points
*/
static inline int toku__lt_border_delete(toku_lock_tree* tree, toku_range_tree* rt) {
    int r;
    assert(tree);
    if (!rt) return 0;

    /* Find the ranges in rt */
    toku_range query;
    toku_point left;
    toku_point right;
    toku__lt_init_full_query(tree, &query, &left, &right);

    u_int32_t numfound;
    r = toku_rt_find(rt, &query, 0, &tree->buf, &tree->buflen, &numfound);
    if (r!=0) return r;
    assert(numfound <= tree->buflen);
    
    u_int32_t i;
    for (i = 0; i < numfound; i++) {
        r = toku__sweep_border(tree, &tree->buf[i]);
        if (r!=0) return r;
    }

    return 0;
}

int toku_lt_unlock(toku_lock_tree* tree, DB_TXN* txn) {
    if (!tree || !txn) return EINVAL;
    int r;
    toku_range_tree *selfwrite = toku__lt_ifexist_selfwrite(tree, txn);
    toku_range_tree *selfread  = toku__lt_ifexist_selfread (tree, txn);

    u_int32_t ranges = 0;

    if (selfread) {
        u_int32_t size;
        r = toku_rt_get_size(selfread, &size);
        assert(r==0);
        ranges += size;
        r = toku__lt_free_contents(tree, selfread, tree->mainread);
        if (r!=0) return toku__lt_panic(tree, r);
    }

    if (selfwrite) {
        u_int32_t size;
        r = toku_rt_get_size(selfwrite, &size);
        assert(r==0);
        ranges += size;
        r = toku__lt_border_delete(tree, selfwrite);
        if (r!=0) return toku__lt_panic(tree, r);
        r = toku__lt_free_contents(tree, selfwrite, NULL);
        if (r!=0) return toku__lt_panic(tree, r);
    }

    if (selfread || selfwrite) toku_rth_delete(tree->rth, txn);
    
    toku__lt_range_decr(tree, ranges);

    return 0;
}

int toku_lt_set_dups(toku_lock_tree* tree, BOOL duplicates) {
    if (!tree)              return EINVAL;
    if (tree->dups_final)   return EDOM;
    tree->duplicates = duplicates;
    return 0;
}

int toku_lt_set_txn_add_lt_callback(toku_lock_tree* tree,
                                    int (*callback)(DB_TXN*, toku_lock_tree*)) {
    if (!tree || !callback) return EINVAL;
    if (tree->dups_final)   return EDOM;
    tree->lock_callback = callback;
    return 0;
}
