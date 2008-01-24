/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <locktree.h>
#include <ydb-internal.h>
#include <brt-internal.h>

//TODO: Modify toku_lt_panic to use the panic thingy from DB
static int __toku_lt_panic(toku_lock_tree *tree, int r) {
    tree->panicked = TRUE;
    return r;
}
                
const unsigned __toku_default_buflen = 2;

static const DBT __toku_lt_infinity;
static const DBT __toku_lt_neg_infinity;

const DBT* const toku_lt_infinity     = &__toku_lt_infinity;
const DBT* const toku_lt_neg_infinity = &__toku_lt_neg_infinity;

/* Compare two payloads assuming that at least one of them is infinite */ 
static int __toku_infinite_compare(void* a, void* b) {
    if (a == b)                    return  0;
    if (a == toku_lt_infinity)     return  1;
    if (b == toku_lt_infinity)     return -1;
    if (a == toku_lt_neg_infinity) return -1;
    if (b == toku_lt_neg_infinity) return  1;
    assert(FALSE);
}

static BOOL __toku_lt_is_infinite(const void* p) {
    return (p == toku_lt_infinity) || (p == toku_lt_neg_infinity);
}

static DBT* __toku_recreate_DBT(DBT* dbt, void* payload, u_int32_t length) {
    memset(dbt, 0, sizeof(DBT));
    dbt->data = payload;
    dbt->size = length;
    return dbt;
}

static int __toku_lt_txn_cmp(void* a, void* b) {
    return a < b ? -1 : (a != b);
}

int __toku_lt_point_cmp(void* a, void* b) {
    int partial_result;
    DBT point_1;
    DBT point_2;

    toku_point* x = (toku_point*)a;
    toku_point* y = (toku_point*)b;

    if (__toku_lt_is_infinite(x->key_payload) ||
        __toku_lt_is_infinite(y->key_payload)) {
        /* If either payload is infinite, then:
           - if duplicates are allowed, the data must be the same 
             infinite value. 
           - if duplicates are not allowed, the data is irrelevant
             In either case, we do not have to compare data: the key will
             be the sole determinant of the comparison */
        return __toku_infinite_compare(x->key_payload, y->key_payload);
    }
    partial_result = x->lt->compare_fun(x->lt->db,
                     __toku_recreate_DBT(&point_1, x->key_payload, x->key_len),
                     __toku_recreate_DBT(&point_2, y->key_payload, y->key_len));
    if (partial_result) return partial_result;
    
    if (!x->lt->duplicates) return 0;

    if (__toku_lt_is_infinite(x->data_payload) ||
        __toku_lt_is_infinite(y->data_payload)) {
        return __toku_infinite_compare(x->data_payload, y->data_payload);
    }
    return x->lt->dup_compare(x->lt->db,
                   __toku_recreate_DBT(&point_1, x->data_payload, x->data_len),
                   __toku_recreate_DBT(&point_2, y->data_payload, y->data_len));
}

static int __toku_p_free(toku_lock_tree* tree, toku_point* point) {
    assert(point);
    if (!__toku_lt_is_infinite(point->key_payload)) {
        tree->free(point->key_payload);
    }
    if (!__toku_lt_is_infinite(point->data_payload)) {
        tree->free(point->data_payload);
    }
    tree->free(point);
    return 0;
}

/*
   Allocate and copy the payload.
*/
static int __toku_payload_copy(toku_lock_tree* tree,
                               void** payload_out, u_int32_t* len_out,
                               void*  payload_in,  u_int32_t  len_in) {
    assert(payload_out && len_out);
    if (__toku_lt_is_infinite(payload_in)) {
        assert(!len_in);
        *payload_out = payload_in;
        *len_out     = 0;
    }
    else if (!len_in || !payload_in) {
        *payload_out = NULL;
        *len_out     = 0;
    }
    else {
        *payload_out = tree->malloc(len_in);
        if (!*payload_out) return errno;
        *len_out     = len_in;
        memcpy(payload_out, payload_in, len_in);
    }
    return 0;
}

static int __toku_p_makecopy(toku_lock_tree* tree, void** ppoint) {
    assert(ppoint);
    toku_point*     point      = *(toku_point**)ppoint;
    toku_point*     temp_point = NULL;
    int r;

    temp_point = (toku_point*)tree->malloc(sizeof(toku_point));
    if (0) {
        died1:
        tree->free(temp_point);
        return r;
    }
    if (!temp_point) return errno;
    memcpy(temp_point, point, sizeof(toku_point));

    r = __toku_payload_copy(tree,
                            &temp_point->key_payload, &temp_point->key_len,
                                  point->key_payload,       point->key_len);
    if (0) {
        died2:
        if (!__toku_lt_is_infinite(temp_point->key_payload)) {
            tree->free(temp_point->key_payload);
        }
        goto died1;
    }
    if (r!=0) goto died1;
    __toku_payload_copy(tree,
                        &temp_point->data_payload, &temp_point->data_len,
                              point->data_payload,       point->data_len);
    if (r!=0) goto died2;
    *ppoint = temp_point;
    return 0;
}

/* Provides access to a selfwrite tree for a particular transaction.
   Returns NULL if it does not exist yet. */
static toku_range_tree* __toku_lt_ifexist_selfwrite(toku_lock_tree* tree,
                                                    DB_TXN* txn) {
    assert(tree && txn);
    //TODO: Implement real version.
    return tree->selfwrite;
}

/* Provides access to a selfread tree for a particular transaction.
   Returns NULL if it does not exist yet. */
static toku_range_tree* __toku_lt_ifexist_selfread(toku_lock_tree* tree,
                                                   DB_TXN* txn) {
    assert(tree && txn);
    //TODO: Implement.
    return tree->selfread;
}

/* Provides access to a selfwrite tree for a particular transaction.
   Creates it if it does not exist. */
static int __toku_lt_selfwrite(toku_lock_tree* tree, DB_TXN* txn,
                               toku_range_tree** pselfwrite) {
    assert(tree && txn && pselfwrite);
    *pselfwrite = tree->selfwrite;
    //TODO: Implement.
    return 0;
}

/* Provides access to a selfread tree for a particular transaction.
   Creates it if it does not exist. */
static int __toku_lt_selfread(toku_lock_tree* tree, DB_TXN* txn,
                              toku_range_tree** pselfread) {
    assert(tree && txn && pselfread);
    *pselfread = tree->selfread;
    //TODO: Implement.
    return 0;
}

/*
    This function only supports non-overlapping trees.
    Uses the standard definition of dominated from the design document.
    Determines whether 'query' is dominated by 'rt'.
*/
static int __toku_lt_rt_dominates(toku_lock_tree* tree, toku_range* query,
                                  toku_range_tree* rt, BOOL* dominated) {
    assert(tree && query && rt && dominated);
    BOOL allow_overlaps;
    toku_range  buffer[1];
    toku_range* buf = &buffer[0];
    unsigned    buflen = sizeof(buf) / sizeof(buf[0]);
    unsigned    numfound;
    int         r;

    /* Sanity check. (Function only supports non-overlap range trees.) */
    r = toku_rt_get_allow_overlaps(rt, &allow_overlaps);
    if (r!=0) return r;
    assert(!allow_overlaps);

    r = toku_rt_find(rt, query, 1, &buf, &buflen, &numfound);
    if (r!=0) return r;
    if (numfound == 0) {
        *dominated = FALSE;
        return 0;
    }
    assert(numfound == 1);
    *dominated = (__toku_lt_point_cmp(query->left,  buf[0].left) >= 0 &&
                  __toku_lt_point_cmp(query->right, buf[0].right) <= 0);
    return 0;
}

/* Will Toku dominate the world? We do not know. But, this function will 
   check whether toku_range_trees dominate a query range. */
static int __toku_lt_dominated(toku_lock_tree *tree, toku_range *query, 
                               toku_range_tree* rt, BOOL *dominated) {
    int r = 0;
    
    assert (tree && query && dominated);
    *dominated = FALSE;
    
    if (rt) r = __toku_lt_rt_dominates(tree, query, rt, dominated);

    return r;
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
static int __toku_lt_borderwrite_conflict(toku_lock_tree* tree, DB_TXN* self,
                                       toku_range* query,
                                       toku_conflict* conflict, DB_TXN** peer) {
    assert(tree && self && query && conflict && peer);
    toku_range_tree* rt = tree->borderwrite;
    assert(rt);
    
    toku_range  buffer[2];
    toku_range* buf = &buffer[0];
    unsigned    buflen = sizeof(buf) / sizeof(buf[0]);
    unsigned    numfound;
    int         r;

    r = toku_rt_find(rt, query, 2, &buf, &buflen, &numfound);
    if (r!=0) return r;
    assert(numfound <= 2);
    *peer = NULL;
    if      (numfound == 2)      *conflict = TOKU_YES_CONFLICT;
    else if (numfound == 0)      *conflict = TOKU_NO_CONFLICT;
    else {
        assert(numfound == 1);
        if (buf[0].data == self) *conflict = TOKU_NO_CONFLICT;
        else {
            *conflict = TOKU_MAYBE_CONFLICT;
            *peer = buf[0].data;
        }
    }
    return 0;
}

/*
    This function supports only non-overlapping trees.
    Uses the standard definition of 'query' meets 'tree' at 'data' from the
    design document.
    Determines whether 'query' meets 'rt'.
*/
static int __toku_lt_meets(toku_lock_tree* tree, DB_TXN* self,
                           toku_range* query, toku_range_tree* rt, BOOL* met) {
    assert(tree && self && query && rt && met);
    toku_range  buffer[1];
    toku_range* buf = &buffer[0];
    unsigned    buflen = sizeof(buf) / sizeof(buf[0]);
    unsigned    numfound;
    int         r;
    BOOL        allow_overlaps;

    /* Sanity check. (Function only supports non-overlap range trees.) */
    r = toku_rt_get_allow_overlaps(rt, &allow_overlaps);
    if (r!=0) return r;
    assert(!allow_overlaps);

    r = toku_rt_find(rt, query, 1, &buf, &buflen, &numfound);
    if (r!=0) return r;
    assert(numfound == 0 || numfound == 1);
    *met = numfound != 0;
    return 0;
}

/*
    Utility function to implement: (from design document)
    if K meets E at v'!=t and K meets W_v' then return failure.
*/
static int __toku_lt_check_borderwrite_conflict(toku_lock_tree* tree,
                                               DB_TXN* txn, toku_range* query) {
    assert(tree && txn && query);
    toku_conflict conflict;
    DB_TXN* peer;
    toku_range_tree* borderwrite = tree->borderwrite;
    toku_range_tree* peer_selfwrite;
    assert(borderwrite);
    int r;
    
    r = __toku_lt_borderwrite_conflict(tree, txn, query, &conflict, &peer);
    if (r!=0) return r;
    if (conflict == TOKU_MAYBE_CONFLICT) {
        assert(peer);
        peer_selfwrite = __toku_lt_ifexist_selfwrite(tree, peer);
        assert(peer_selfwrite);

        BOOL met;
        r = __toku_lt_meets(tree, txn, query, peer_selfwrite, &met);
        if (r!=0)   return r;
        if (met)    conflict = TOKU_YES_CONFLICT;
        else        conflict = TOKU_NO_CONFLICT;
    }
    if (conflict == TOKU_YES_CONFLICT) return DB_LOCK_NOTGRANTED;
    assert(conflict == TOKU_NO_CONFLICT);
    return 0;
}

static void __toku_payload_from_dbt(void** payload, u_int32_t* len,
                                    const DBT* dbt) {
    assert(payload && len && dbt);
    if (__toku_lt_is_infinite(dbt)) {
        *payload = (void*)dbt;
        *len     = 0;
    }
    else if (!dbt->data || !dbt->size) {
        *len     = 0;
        *payload = NULL;
    }
    else {
        *len     = dbt->size;
        *payload = dbt->data;
    }
}

static void __toku_init_point(toku_point* point, toku_lock_tree* tree,
                              const DBT* key, const DBT* data) {
    assert(point && tree && key && data);
    memset(point, 0, sizeof(toku_point));
    point->lt = tree;
    
    __toku_payload_from_dbt(&point->key_payload, &point->key_len, key);
    if (tree->duplicates) {
        assert(data);
        __toku_payload_from_dbt(&point->data_payload, &point->data_len, data);
    }
    else {
        assert(data == NULL);
        point->data_payload = NULL;
        point->data_len     = 0;
    }
}

static void __toku_init_query(toku_range* query,
                              toku_point* left, toku_point* right) {
    query->left  = left;
    query->right = right;
    query->data  = NULL;
}

static void __toku_init_insert(toku_range* to_insert,
                               toku_point* left, toku_point* right,
                               DB_TXN* txn) {
    to_insert->left  = left;
    to_insert->right = right;
    to_insert->data  = txn;
}

static void __toku_lt_extend_extreme(toku_lock_tree* tree,toku_range* to_insert,
                                     BOOL* alloc_left, BOOL* alloc_right,
                                     unsigned numfound) {
    assert(to_insert && tree && alloc_left && alloc_right);
    unsigned i;
    for (i = 0; i < numfound; i++) {
        int c;
        /* Find the extreme left end-point among overlapping ranges */
        if ((c = __toku_lt_point_cmp(tree->buf[i].left, to_insert->left))
            <= 0) {
            assert(*alloc_left || c < 0);
            assert(tree->buf[i].left != to_insert->left);
            assert(tree->buf[i].left != to_insert->right);
            *alloc_left      = FALSE;
            to_insert->left  = tree->buf[i].left;
        }
        /* Find the extreme right end-point */
        if ((c = __toku_lt_point_cmp(tree->buf[i].right, to_insert->right))
            >= 0) {
            assert(*alloc_right || c > 0);
            assert(tree->buf[i].right != to_insert->left ||
                   (tree->buf[i].left == to_insert->left &&
                    tree->buf[i].left == tree->buf[i].right));
            assert(tree->buf[i].right != to_insert->right);
            *alloc_right     = FALSE;
            to_insert->right = tree->buf[i].right;
        }
    }
}

static int __toku_lt_alloc_extreme(toku_lock_tree* tree, toku_range* to_insert,
                                   BOOL alloc_left, BOOL* alloc_right) {
    assert(to_insert && alloc_right);
    BOOL copy_left = FALSE;
    int r;
    
    if (alloc_left && alloc_right &&
        __toku_lt_point_cmp(to_insert->left, to_insert->right) == 0) {
        *alloc_right = FALSE;
        copy_left    = TRUE;
    }
    if (alloc_left) {
        r = __toku_p_makecopy(tree, &to_insert->left);
        if (0) {
            died1:
            if (alloc_left) __toku_p_free(tree, to_insert->left);
            return r;
        }
        if (r!=0) return r;
    }
    if (*alloc_right) {
        assert(!copy_left);
        r = __toku_p_makecopy(tree, &to_insert->right);
        if (r!=0) goto died1;
    }
    else if (copy_left) to_insert->right = to_insert->left;
    return 0;
}

static void __toku_lt_delete_overlapping_ranges(toku_lock_tree* tree,
                                                toku_range_tree* rt,
                                                unsigned numfound) {
    assert(tree && rt);
    int r;
    unsigned i;
    for (i = 0; i < numfound; i++) {
        r = toku_rt_delete(rt, &tree->buf[i]);
        assert(r==0);
    }
}

static void __toku_lt_free_points(toku_lock_tree* tree, toku_range* to_insert,
                                  unsigned numfound) {
    assert(tree && to_insert);

    unsigned i;
    for (i = 0; i < numfound; i++) {
        /*
           We will maintain the invariant: (separately for read and write
           environments)
           (__toku_lt_point_cmp(a, b) == 0 && a.txn == b.txn) => a == b
        */
        /* Do not double-free. */
        if (tree->buf[i].right != tree->buf[i].left &&
            tree->buf[i].right != to_insert->left &&
            tree->buf[i].right != to_insert->right) {
            __toku_p_free(tree, tree->buf[i].right);
        }
        if (tree->buf[i].left != to_insert->left &&
            tree->buf[i].left != to_insert->right) {
            __toku_p_free(tree, tree->buf[i].left);
        }
    }
}

/* Consolidate the new range and all the overlapping ranges */
static int __toku_consolidate(toku_lock_tree* tree,
                              toku_range* query, toku_range* to_insert,
                              DB_TXN* txn) {
    int r;
    BOOL             alloc_left    = TRUE;
    BOOL             alloc_right   = TRUE;
    toku_range_tree* selfread;
    assert(tree && to_insert && txn);
    toku_range_tree* mainread      = tree->mainread;
    assert(mainread);
    
    /** This is so important that it should go into doxygen at some point,
        either here or in the .h file
       
       Memory ownership: 
       - tree->buf is an array of toku_range's, which the lt owns
         The contents of tree->buf are volatile (this is a buffer space
         that we pass around to various functions, and every time we
         invoke a new function, its previous contents may become 
         meaningless)
       - tree->buf[i].left, .right are toku_points (ultimately a struct), 
         also owned by lt. We gave a pointer only to this memory to the 
         range tree earlier when we inserted a range, but the range tree
         does not own it!
       - tree->buf[i].{left,right}.{key_payload,data_payload} is owned by
         the lt, we made copies from the DB at some point
       - to_insert we own (it's static)
       - to_insert.left, .right are toku_point's, and we own them.
         If we have consolidated, we own them because we had allocated
         them earlier, but
         if we have not consolidated we need to gain ownership now: 
         we will gain ownership by copying all payloads and 
         allocating the points. 
        -to_insert.{left,right}.{key_payload, data_payload} are owned by lt,
         we made copies from the DB at consolidation time 
    */   
    
    /* Find the self read tree */
    r = __toku_lt_selfread(tree, txn, &selfread);
    if (r!=0) return r;
    assert(selfread);

    /* Find all overlapping ranges in the self-read */
    unsigned numfound;
    r = toku_rt_find(selfread, query, 0, &tree->buf, &tree->buflen,
                     &numfound);
    if (r!=0) return r;

    /* Find the extreme left and right point of the consolidated interval */
    __toku_lt_extend_extreme(tree, to_insert, &alloc_left, &alloc_right,
                             numfound);

    /* Allocate the consolidated range */
    r = __toku_lt_alloc_extreme(tree, to_insert, alloc_left, &alloc_right);
    if (0) {
        died1:
        if (alloc_left)  __toku_p_free(tree, to_insert->left);
        if (alloc_right) __toku_p_free(tree, to_insert->right);
        return r;
    }
    if (r!=0) return r;
    
    /* Delete overlapping ranges from selfread ... */
    __toku_lt_delete_overlapping_ranges(tree, selfread, numfound);
    /* ... and mainread.
       Growth direction: if we had no overlaps, the next line
       should be commented out */
    __toku_lt_delete_overlapping_ranges(tree, mainread, numfound);
    
    /* Free all the points from ranges in tree->buf[0]..tree->buf[numfound-1] */
    __toku_lt_free_points(tree, to_insert, numfound);
    
    /* Insert extreme range into selfread. */
    r = toku_rt_insert(selfread, to_insert);
    int r2;
    if (0) {
        died2:
        r2 = toku_rt_delete(selfread, to_insert);
        assert(r2==0);
        goto died1;
    }
    if (r!=0) {
        if (numfound) r = __toku_lt_panic(tree, r);
        goto died1;
    }
    assert(tree->mainread);

    /* Insert extreme range into mainread. */
    r = toku_rt_insert(tree->mainread, to_insert);
    if (r!=0) {
        if (numfound) r = __toku_lt_panic(tree, r);
        goto died2;
    }
    return 0;
}

static void __toku_lt_free_contents(toku_lock_tree* tree, toku_range_tree* rt) {
    assert(tree);
    if (!rt) return;
    
    int r;

    toku_range query;
    toku_point left;
    toku_point right;
    __toku_init_point(&left,  tree, (DBT*)toku_lt_neg_infinity,
                      tree->duplicates ? (DBT*)toku_lt_neg_infinity : NULL);
    __toku_init_point(&right, tree, (DBT*)toku_lt_infinity,
                      tree->duplicates ? (DBT*)toku_lt_infinity : NULL);
    __toku_init_query(&query, &left, &right);

    unsigned numfound;
    r = toku_rt_find(rt, &query, 0, &tree->buf, &tree->buflen, &numfound);
    if (r!=0) {
        /*
            To free space the fast way, we need to allocate more space.
            Since we can't, we free the slow way.
            We do not optimize this, we take one entry at a time,
            delete it from the tree, and then free the memory.
        */
        do {
            r = toku_rt_find(rt, &query, 1, &tree->buf, &tree->buflen,
                             &numfound);
            assert(r==0);
            if (!numfound) break;
            assert(numfound == 1);
            r = toku_rt_delete(rt, &tree->buf[0]);
            assert(r==0);
            __toku_lt_free_points(tree, &query, numfound);
        } while (TRUE);
    }
    else __toku_lt_free_points(tree, &query, numfound);
    r = toku_rt_close(rt);
    assert(r == 0);
}

int toku_lt_create(toku_lock_tree** ptree, DB* db, BOOL duplicates,
                   int   (*compare_fun)(DB*,const DBT*,const DBT*),
                   int   (*dup_compare)(DB*,const DBT*,const DBT*),
                   void* (*user_malloc) (size_t),
                   void  (*user_free)   (void*),
                   void* (*user_realloc)(void*, size_t)) {
    if (!ptree || !db || !compare_fun || !dup_compare) return EINVAL;
    int r;

    toku_lock_tree* temp_tree =(toku_lock_tree*)user_malloc(sizeof(*temp_tree));
    if (0) {
        died1:
        user_free(temp_tree);
        return r;
    }
    if (!temp_tree) return errno;
    memset(temp_tree, 0, sizeof(*temp_tree));
    temp_tree->db          = db;
    temp_tree->duplicates  = duplicates;
    temp_tree->compare_fun = compare_fun;
    temp_tree->dup_compare = dup_compare;
    temp_tree->malloc      = user_malloc;
    temp_tree->free        = user_free;
    temp_tree->realloc     = user_realloc;
    r = toku_rt_create(&temp_tree->mainread,
                       __toku_lt_point_cmp, __toku_lt_txn_cmp, TRUE,
                       user_malloc, user_free, user_realloc);
    if (0) {
        died2:
        toku_rt_close(temp_tree->mainread);
        goto died1;
    }
    if (r!=0) goto died1;
    r = toku_rt_create(&temp_tree->borderwrite,
                       __toku_lt_point_cmp, __toku_lt_txn_cmp, FALSE,
                       user_malloc, user_free, user_realloc);
    if (0) {
        died3:
        toku_rt_close(temp_tree->borderwrite);
        goto died2;
    }
    if (r!=0) goto died2;
//TODO: Remove this, and use multiples per transaction
r = toku_rt_create(&temp_tree->selfwrite,
                   __toku_lt_point_cmp, __toku_lt_txn_cmp, FALSE,
                   user_malloc, user_free, user_realloc);
assert(temp_tree->selfwrite);
//TODO: Remove this, and use multiples per transaction
r = toku_rt_create(&temp_tree->selfread,
                   __toku_lt_point_cmp, __toku_lt_txn_cmp, TRUE,
                   user_malloc, user_free, user_realloc);
assert(temp_tree->selfread);
    temp_tree->buflen = __toku_default_buflen;
    temp_tree->buf    = (toku_range*)
                        user_malloc(temp_tree->buflen * sizeof(toku_range));
    if (!temp_tree->buf) {
        r = errno;
        goto died3;
    }
    //TODO: Create list of selfreads
    //TODO: Create list of selfwrites
    *ptree = temp_tree;
    return 0;
}

int toku_lt_close(toku_lock_tree* tree) {
    if (!tree)                                              return EINVAL;
    int r;
    r = toku_rt_close(tree->mainread);
    assert(r==0);
    r = toku_rt_close(tree->borderwrite);
    assert(r==0);
//TODO: Do this to ALLLLLLLL selfread and ALLLLLL selfwrite tables.
    /* Free all points referred to in a range tree (and close the tree). */
    __toku_lt_free_contents(tree,__toku_lt_ifexist_selfread (tree, (DB_TXN*)1));
    __toku_lt_free_contents(tree,__toku_lt_ifexist_selfwrite(tree, (DB_TXN*)1));
//TODO: After freeing the tree, need to remove it from both lists!!!
//      One list probably IN the transaction, and one additional one here.
    tree->free(tree->buf);
    tree->free(tree);
    return 0;
}

int toku_lt_acquire_read_lock(toku_lock_tree* tree, DB_TXN* txn,
                              const DBT* key, const DBT* data) {
    return toku_lt_acquire_range_read_lock(tree, txn, key, data, key, data);
}

int toku_lt_acquire_range_read_lock(toku_lock_tree* tree, DB_TXN* txn,
                                  const DBT* key_left,  const DBT* data_left,
                                  const DBT* key_right, const DBT* data_right) {
    if (!tree || !txn || !key_left || !key_right)           return EINVAL;
    if (!tree->duplicates && ( data_left ||  data_right))   return EINVAL;
    if (tree->duplicates  && (!data_left || !data_right))   return EINVAL;
    if (tree->duplicates  && key_left != data_left &&
        __toku_lt_is_infinite(key_left))                    return EINVAL;
    if (tree->duplicates  && key_right != data_right &&
        __toku_lt_is_infinite(key_right))                   return EINVAL;

    int r;
    toku_point left;
    toku_point right;
    toku_range query;
    BOOL dominated;
    
    __toku_init_point(&left,   tree,  key_left,  data_left);
    __toku_init_point(&right,  tree,  key_right, data_right);
    __toku_init_query(&query, &left, &right);
    
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
    r = __toku_lt_dominated(tree, &query, 
                            __toku_lt_ifexist_selfwrite(tree, txn), &dominated);
    if (r || dominated) return r;

    /* else if 'K' is dominated by selfread('txn') then return success. */
    r = __toku_lt_dominated(tree, &query, 
                            __toku_lt_ifexist_selfread(tree, txn), &dominated);
    if (r || dominated) return r; 

    /*
        else if 'K' meets borderwrite at 'peer' ('peer'!='txn') &&
                'K' meets selfwrite('peer') then return failure.
    */
    r = __toku_lt_check_borderwrite_conflict(tree, txn, &query);
    if (r!=0) return r;

    /* Now need to merge, copy the memory and insert. */

    toku_range  to_insert;
    __toku_init_insert(&to_insert, &left, &right, txn);

    /* Consolidate the new range and all the overlapping ranges */
    return __toku_consolidate(tree, &query, &to_insert, txn);
}

int toku_lt_acquire_write_lock(toku_lock_tree* tree, DB_TXN* txn,
                               const DBT* key, const DBT* data) {
    if (!tree || !txn || !key)                              return EINVAL;
    if (!tree->duplicates && data)                          return EINVAL;
    if (tree->duplicates && !data)                          return EINVAL;
    if (tree->duplicates && key != data &&
        __toku_lt_is_infinite(key))                         return EINVAL;
    int r;
    toku_point left;
    toku_point right;
    toku_range query;
    BOOL dominated;
    toku_range_tree* mainread;
    
    __toku_init_point(&left,   tree,  key, data);
    __toku_init_point(&right,  tree,  key, data);
    __toku_init_query(&query, &left, &right);

    /* if 'K' is dominated by selfwrite('txn') then return success. */
    r = __toku_lt_dominated(tree, &query, 
                            __toku_lt_ifexist_selfwrite(tree, txn), &dominated);
    if (r || dominated) return r;
    
    /* else if 'K' is dominated by selfread('txn') then return success. */
    mainread = tree->mainread; assert(mainread);
    r = __toku_lt_dominated(tree, &query, mainread, &dominated);
    if (r || dominated) return r; 

    r = __toku_lt_check_borderwrite_conflict(tree, txn, &query);
    if (r!=0) return r;

    /* Now need to copy the memory and insert. */
    assert(FALSE);  //Not implemented yet.

    /*
        No merging required in selfwrite.
        This is a point, and if merging was possible it would have been
        dominated by selfwrite.
        
        Must update borderwrite however.
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
    toku_range  to_insert;
    __toku_init_insert(&to_insert, &left, &right, txn);
    /*
        No merging required in selfwrite.
        This is a point, and if merging was possible it would have been
        dominated by selfwrite.
    */
    r = __toku_p_makecopy(tree, &to_insert.left);
    if (0) {
        died1:
        __toku_p_free(tree, to_insert.left);
        return __toku_lt_panic(tree, r);
    }
    to_insert.right = to_insert.left;
    toku_range_tree* selfwrite;
    r = __toku_lt_selfwrite(tree, txn, &selfwrite);
    if (r!=0) return __toku_lt_panic(tree, r);
    assert(selfwrite);
           
    r = toku_rt_insert(selfwrite, &to_insert);
    if (r!=0) goto died1;

    /* Need to update borderwrite. */
    toku_range_tree* borderwrite = tree->borderwrite;
    assert(borderwrite);
    unsigned numfound;
    r = toku_rt_find(borderwrite, &query, 1, &tree->buf, &tree->buflen,
                     &numfound);
    if (r!=0) __toku_lt_panic(tree, r);
    assert(numfound == 0 || numfound == 1);
    
    toku_range pred;
    toku_range succ;
    if (numfound == 0) {
        BOOL found_p;
        BOOL found_s;

        r = toku_rt_predecessor(borderwrite, to_insert.left,  &pred, &found_p);
        if (r!=0) return __toku_lt_panic(tree, r);
        r = toku_rt_successor  (borderwrite, to_insert.right, &succ, &found_s);
        if (r!=0) return __toku_lt_panic(tree, r);
        
        assert(!found_p || !found_s || pred.data != succ.data);
        if      (found_p && pred.data == txn) {
            r = toku_rt_delete(borderwrite, &pred);
            if (r!=0) return __toku_lt_panic(tree, r);
            to_insert.left = pred.left;
        }
        else if (found_s && succ.data == txn) {
            r = toku_rt_delete(borderwrite, &succ);
            if (r!=0) return __toku_lt_panic(tree, r);
            to_insert.right = succ.right;
        }
    }
    else if (tree->buf[0].data != txn) {
        toku_range_tree* peer_selfwrite =
            __toku_lt_ifexist_selfwrite(tree, tree->buf[0].data);
        assert(peer_selfwrite);
        BOOL found;

        r = toku_rt_predecessor(peer_selfwrite, to_insert.left,  &pred, &found);
        if (r!=0) return __toku_lt_panic(tree, r);
        assert(found);
        r = toku_rt_successor  (peer_selfwrite, to_insert.right, &succ, &found);
        if (r!=0) return __toku_lt_panic(tree, r);
        assert(found);
        r = toku_rt_delete(borderwrite, &tree->buf[0]);
        if (r!=0) return __toku_lt_panic(tree, r);
        pred.right = tree->buf[0].right;
        succ.left  = tree->buf[0].left;
        r = toku_rt_insert(borderwrite, &pred);
        if (r!=0) return __toku_lt_panic(tree, r);
        r = toku_rt_insert(borderwrite, &succ);
        if (r!=0) return __toku_lt_panic(tree, r);
    }
    if (numfound == 0 || tree->buf[0].data != txn) {
        r = toku_rt_insert(borderwrite, &to_insert);
        if (r!=0) return __toku_lt_panic(tree, r);
    }
    return 0;
}

int toku_lt_acquire_range_write_lock(toku_lock_tree* tree, DB_TXN* txn,
                                  const DBT* key_left,  const DBT* data_left,
                                  const DBT* key_right, const DBT* data_right) {
    if (!tree || !txn || !key_left || !key_right)           return EINVAL;
    if (!tree->duplicates && ( data_left ||  data_right))   return EINVAL;
    if (tree->duplicates  && (!data_left || !data_right))   return EINVAL;
    if (tree->duplicates  && key_left != data_left &&
        __toku_lt_is_infinite(key_left))                    return EINVAL;
    if (tree->duplicates  && key_right != data_right &&
        __toku_lt_is_infinite(key_right))                   return EINVAL;
    assert(FALSE);
    //We are not ready for this.
    //Not needed for Feb 1 release.
}

int toku_lt_unlock(toku_lock_tree* tree, DB_TXN* txn);
