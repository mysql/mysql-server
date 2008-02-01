/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

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
static int __toku_lt_panic(toku_lock_tree *tree) {
    return tree->panic(tree->db);
}
                
const u_int32_t __toku_default_buflen = 2;

static const DBT __toku_lt_infinity;
static const DBT __toku_lt_neg_infinity;

const DBT* const toku_lt_infinity     = &__toku_lt_infinity;
const DBT* const toku_lt_neg_infinity = &__toku_lt_neg_infinity;

/* Compare two payloads assuming that at least one of them is infinite */ 
static int __toku_infinite_compare(void* a, void* b) {
    if    (a == b)                      return  0;
    if    (a == toku_lt_infinity)       return  1;
    if    (b == toku_lt_infinity)       return -1;
    if    (a == toku_lt_neg_infinity)   return -1;
    assert(b == toku_lt_neg_infinity);  return  1;
}

static BOOL __toku_lt_is_infinite(const void* p) {
    if (p == toku_lt_infinity || p == toku_lt_neg_infinity) {
        DBT* dbt = (DBT*)p;
        assert(!dbt->data && !dbt->size);
        return TRUE;
    }
    return FALSE;
}

/* Verifies that NULL data and size are consistent.
   i.e. The size is 0 if and only if the data is NULL. */
static void __toku_lt_verify_null_key(const DBT* key) {
    assert(!key || __toku_lt_is_infinite(key) ||
           (key->size > 0) == (key->data != NULL));
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

    assert(a && b);
    toku_point* x = (toku_point*)a;
    toku_point* y = (toku_point*)b;
    assert(x->lt);
    assert(x->lt == y->lt);

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

static void __toku_p_free(toku_lock_tree* tree, toku_point* point) {
    assert(point);
    tree->payload_used -= point->key_len;
    tree->payload_used -= point->data_len;
    tree->payload_used -= sizeof(toku_point);
    if (!__toku_lt_is_infinite(point->key_payload)) {
        tree->free(point->key_payload);
    }
    if (!__toku_lt_is_infinite(point->data_payload)) {
        tree->free(point->data_payload);
    }
    tree->free(point);
}

/*
   Allocate and copy the payload.
*/
static int __toku_payload_copy(toku_lock_tree* tree,
                               void** payload_out, u_int32_t* len_out,
                               void*  payload_in,  u_int32_t  len_in) {
    assert(payload_out && len_out);
    if (!len_in) {
        assert(!payload_in || __toku_lt_is_infinite(payload_in));
        *payload_out = payload_in;
        *len_out     = len_in;
    }
    else {
        assert(payload_in);
        if (tree->payload_used + len_in > tree->payload_capacity) return ENOMEM;
        *payload_out = tree->malloc((size_t)len_in);
        if (!*payload_out) return errno;
        tree->payload_used += len_in;
        *len_out     = len_in;
        memcpy(*payload_out, payload_in, (size_t)len_in);
    }
    return 0;
}

static int __toku_p_makecopy(toku_lock_tree* tree, void** ppoint) {
    assert(ppoint);
    toku_point*     point      = *(toku_point**)ppoint;
    toku_point*     temp_point = NULL;
    int r;

    if (tree->payload_used + sizeof(toku_point) > tree->payload_capacity) {
        return ENOMEM;}
    temp_point = (toku_point*)tree->malloc(sizeof(toku_point));
    if (0) {
        died1: tree->free(temp_point);
        tree->payload_used -= sizeof(toku_point); return r; }
    if (!temp_point) return errno;
    tree->payload_used += sizeof(toku_point);
    memcpy(temp_point, point, sizeof(toku_point));

    r = __toku_payload_copy(tree,
                            &temp_point->key_payload, &temp_point->key_len,
                                  point->key_payload,       point->key_len);
    if (0) {
        died2:
        if (!__toku_lt_is_infinite(temp_point->key_payload)) {
            tree->free(temp_point->key_payload); }
        tree->payload_used -= temp_point->key_len; goto died1; }
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
toku_range_tree* __toku_lt_ifexist_selfwrite(toku_lock_tree* tree,
                                             DB_TXN* txn) {
    assert(tree && txn);
//TODO: Implement real version.
return tree->selfwrite;
}

/* Provides access to a selfread tree for a particular transaction.
   Returns NULL if it does not exist yet. */
toku_range_tree* __toku_lt_ifexist_selfread(toku_lock_tree* tree, DB_TXN* txn) {
    assert(tree && txn);
//TODO: Implement.
return tree->selfread;
}

/* Provides access to a selfwrite tree for a particular transaction.
   Creates it if it does not exist. */
static int __toku_lt_selfwrite(toku_lock_tree* tree, DB_TXN* txn,
                               toku_range_tree** pselfwrite) {
    int r;
    assert(tree && txn && pselfwrite);

//TODO: Remove this, and use multiples per transaction
if (!tree->selfwrite)
{
r = toku_rt_create(&tree->selfwrite,
                   __toku_lt_point_cmp, __toku_lt_txn_cmp, FALSE,
                   tree->malloc, tree->free, tree->realloc);
if(r!=0) return r;
}
assert(tree->selfwrite);
*pselfwrite = tree->selfwrite;

    return 0;
}

/* Provides access to a selfread tree for a particular transaction.
   Creates it if it does not exist. */
static int __toku_lt_selfread(toku_lock_tree* tree, DB_TXN* txn,
                              toku_range_tree** pselfread) {
    int r;
    assert(tree && txn && pselfread);

//TODO: Remove this, and use multiples per transaction
if (!tree->selfread)
{
r = toku_rt_create(&tree->selfread,
                   __toku_lt_point_cmp, __toku_lt_txn_cmp, FALSE,
                   tree->malloc, tree->free, tree->realloc);
if(r!=0) return r;
}
assert(tree->selfread);
*pselfread = tree->selfread;

    return 0;
}

/*
    This function only supports non-overlapping trees.
    Uses the standard definition of dominated from the design document.
    Determines whether 'query' is dominated by 'rt'.
*/
static int __toku_lt_rt_dominates(toku_lock_tree* tree, toku_range* query,
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
    *dominated = (__toku_lt_point_cmp(query->left,  buf[0].left) >= 0 &&
                  __toku_lt_point_cmp(query->right, buf[0].right) <= 0);
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
static int __toku_lt_borderwrite_conflict(toku_lock_tree* tree, DB_TXN* self,
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
static int __toku_lt_meets(toku_lock_tree* tree, toku_range* query, 
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
    This function supports overlapping trees with heterogenous transactions,
    but queries must be a single point. 
    Uses the standard definition of 'query' meets 'tree' at 'data' from the
    design document.
*/
static int __toku_lt_meets_peer(toku_lock_tree* tree, toku_range* query, 
                                toku_range_tree* rt, DB_TXN* self, BOOL* met) {
    assert(tree && query && rt && self && met);
    assert(query->left == query->right);

    const u_int32_t query_size = 2;
    toku_range   buffer[query_size];
    u_int32_t     buflen     = query_size;
    toku_range*  buf        = &buffer[0];
    u_int32_t     numfound;
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
static int __toku_lt_check_borderwrite_conflict(toku_lock_tree* tree,
                                               DB_TXN* txn, toku_range* query) {
    assert(tree && txn && query);
    toku_conflict conflict;
    DB_TXN* peer;
    toku_range_tree* peer_selfwrite;
    int r;
    
    r = __toku_lt_borderwrite_conflict(tree, txn, query, &conflict, &peer);
    if (r!=0) return r;
    if (conflict == TOKU_MAYBE_CONFLICT) {
        assert(peer);
        peer_selfwrite = __toku_lt_ifexist_selfwrite(tree, peer);
        if (!peer_selfwrite) return __toku_lt_panic(tree);

        BOOL met;
        r = __toku_lt_meets(tree, query, peer_selfwrite, &met);
        if (r!=0)   return r;
        conflict = met ? TOKU_YES_CONFLICT : TOKU_NO_CONFLICT;
    }
    if    (conflict == TOKU_YES_CONFLICT) return DB_LOCK_NOTGRANTED;
    assert(conflict == TOKU_NO_CONFLICT);
    return 0;
}

static void __toku_payload_from_dbt(void** payload, u_int32_t* len,
                                    const DBT* dbt) {
    assert(payload && len && dbt);
    if (__toku_lt_is_infinite(dbt)) *payload = (void*)dbt;
    else {
        assert(!dbt->data == !dbt->size);
        *payload = dbt->data;
    }
    *len     = dbt->size;
}

static void __toku_init_point(toku_point* point, toku_lock_tree* tree,
                              const DBT* key, const DBT* data) {
    assert(point && tree && key);
    assert(!tree->duplicates == !data);
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

/* Returns whether the point already exists
   as an endpoint of the given range. */
static BOOL __toku_lt_p_independent(toku_point* point, toku_range* range) {
    assert(point && range);
    return point != range->left && point != range->right;
}

static int __toku_lt_extend_extreme(toku_lock_tree* tree,toku_range* to_insert,
                                    BOOL* alloc_left, BOOL* alloc_right,
                                    u_int32_t numfound) {
    assert(to_insert && tree && alloc_left && alloc_right);
    u_int32_t i;
    assert(numfound <= tree->buflen);
    for (i = 0; i < numfound; i++) {
        int c;
        /* Find the extreme left end-point among overlapping ranges */
        if ((c = __toku_lt_point_cmp(tree->buf[i].left, to_insert->left))
            <= 0) {
            if ((!*alloc_left && c == 0) ||
                !__toku_lt_p_independent(tree->buf[i].left, to_insert)) {
                return __toku_lt_panic(tree); }
            *alloc_left      = FALSE;
            to_insert->left  = tree->buf[i].left;
        }
        /* Find the extreme right end-point */
        if ((c = __toku_lt_point_cmp(tree->buf[i].right, to_insert->right))
            >= 0) {
            if ((!*alloc_right && c == 0) ||
                (tree->buf[i].right == to_insert->left &&
                 tree->buf[i].left  != to_insert->left) ||
                 tree->buf[i].right == to_insert->right) {
                return __toku_lt_panic(tree); }
            *alloc_right     = FALSE;
            to_insert->right = tree->buf[i].right;
        }
    }
    return 0;
}

static int __toku_lt_alloc_extreme(toku_lock_tree* tree, toku_range* to_insert,
                                   BOOL alloc_left, BOOL* alloc_right) {
    assert(to_insert && alloc_right);
    BOOL copy_left = FALSE;
    int r;
    
    /* The pointer comparison may speed up the evaluation in some cases, 
       but it is not strictly needed */
    if (alloc_left && alloc_right &&
        (to_insert->left == to_insert->right ||
         __toku_lt_point_cmp(to_insert->left, to_insert->right) == 0)) {
        *alloc_right = FALSE;
        copy_left    = TRUE;
    }

    if (alloc_left) {
        r = __toku_p_makecopy(tree, &to_insert->left);
        if (0) { died1:
            if (alloc_left) __toku_p_free(tree, to_insert->left); return r; }
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

static int __toku_lt_delete_overlapping_ranges(toku_lock_tree* tree,
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

static void __toku_lt_free_points(toku_lock_tree* tree, toku_range* to_insert,
                                  u_int32_t numfound) {
    assert(tree && to_insert);
    assert(numfound <= tree->buflen);

    u_int32_t i;
    for (i = 0; i < numfound; i++) {
        /*
           We will maintain the invariant: (separately for read and write
           environments)
           (__toku_lt_point_cmp(a, b) == 0 && a.txn == b.txn) => a == b
        */
        /* Do not double-free. */
        if (tree->buf[i].right != tree->buf[i].left &&
            __toku_lt_p_independent(tree->buf[i].right, to_insert)) {
            __toku_p_free(tree, tree->buf[i].right);
        }
        if (__toku_lt_p_independent(tree->buf[i].left,  to_insert)) {
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
    /* Find the self read tree */
    r = __toku_lt_selfread(tree, txn, &selfread);
    if (r!=0) return r;
    assert(selfread);
    /* Find all overlapping ranges in the self-read */
    u_int32_t numfound;
    r = toku_rt_find(selfread, query, 0, &tree->buf, &tree->buflen,
                     &numfound);
    if (r!=0) return r;
    assert(numfound <= tree->buflen);
    /* Find the extreme left and right point of the consolidated interval */
    r = __toku_lt_extend_extreme(tree, to_insert, &alloc_left, &alloc_right,
                                 numfound);
    if (r!=0) return r;
    /* Allocate the consolidated range */
    r = __toku_lt_alloc_extreme(tree, to_insert, alloc_left, &alloc_right);
    if (0) { died1:
        if (alloc_left)  __toku_p_free(tree, to_insert->left);
        if (alloc_right) __toku_p_free(tree, to_insert->right); return r; }
    if (r!=0) return r;
    /* From this point on we have to panic if we cannot finish. */
    /* Delete overlapping ranges from selfread ... */
    r = __toku_lt_delete_overlapping_ranges(tree, selfread, numfound);
    if (r!=0) return __toku_lt_panic(tree);
    /* ... and mainread.
       Growth direction: if we had no overlaps, the next line
       should be commented out */
    r = __toku_lt_delete_overlapping_ranges(tree, mainread, numfound);
    if (r!=0) return __toku_lt_panic(tree);
    /* Free all the points from ranges in tree->buf[0]..tree->buf[numfound-1] */
    __toku_lt_free_points(tree, to_insert, numfound);
    /* We don't necessarily need to panic after here unless numfound > 0
       Which indicates we deleted something. */
    /* Insert extreme range into selfread. */
    r = toku_rt_insert(selfread, to_insert);
    int r2;
    if (0) { died2: r2 = toku_rt_delete(selfread, to_insert);
        if (r2!=0) return __toku_lt_panic(tree); goto died1; }
    if (r!=0) {
        /* If we deleted/merged anything, this is a panic situation. */
        if (numfound) return __toku_lt_panic(tree); goto died1; }
    /* Insert extreme range into mainread. */
    assert(tree->mainread);
    r = toku_rt_insert(tree->mainread, to_insert);
    if (r!=0) {
        /* If we deleted/merged anything, this is a panic situation. */
        if (numfound) return __toku_lt_panic(tree); goto died2; }
    return 0;
}

static void __toku_lt_init_full_query(toku_lock_tree* tree, toku_range* query,
                                      toku_point* left, toku_point* right) {
    __toku_init_point(left,  tree,       (DBT*)toku_lt_neg_infinity,
                      tree->duplicates ? (DBT*)toku_lt_neg_infinity : NULL);
    __toku_init_point(right, tree,       (DBT*)toku_lt_infinity,
                      tree->duplicates ? (DBT*)toku_lt_infinity : NULL);
    __toku_init_query(query, left, right);
}

static int __toku_lt_free_contents_slow(toku_lock_tree* tree,
                                        toku_range_tree* rt) {
    int r;
    toku_range query;
    toku_point left;
    toku_point right;
    u_int32_t numfound;

    __toku_lt_init_full_query(tree, &query, &left, &right);
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
        __toku_lt_free_points(tree, &query, numfound);
    } while (TRUE);
    return r;
}

static int __toku_lt_free_contents(toku_lock_tree* tree, toku_range_tree* rt) {
    assert(tree);
    if (!rt) return 0;
    
    int r;
    int r2;

    toku_range query;
    toku_point left;
    toku_point right;
    __toku_lt_init_full_query(tree, &query, &left, &right);

    u_int32_t numfound;
    r = toku_rt_find(rt, &query, 0, &tree->buf, &tree->buflen, &numfound);
    if (r==0)               __toku_lt_free_points(tree, &query, numfound);
    else if (r==ENOMEM) r = __toku_lt_free_contents_slow(tree, rt);
    r2 = toku_rt_close(rt);
    return r ? r : r2;
}

static BOOL __toku_r_backwards(toku_range* range) {
    assert(range && range->left && range->right);
    toku_point* left  = (toku_point*)range->left;
    toku_point* right = (toku_point*)range->right;

    /* Optimization: if all the pointers are equal, clearly left == right. */
    return (left->key_payload  != right->key_payload ||
            left->data_payload != right->data_payload) &&
           __toku_lt_point_cmp(left, right) > 0;
}


static int __toku_lt_preprocess(toku_lock_tree* tree, DB_TXN* txn,
                                const DBT* key_left,  const DBT* data_left,
                                const DBT* key_right, const DBT* data_right,
                                toku_point* left, toku_point* right,
                                toku_range* query) {
    if (!tree || !txn || !key_left || !key_right)           return EINVAL;
    if (!tree->duplicates && ( data_left ||  data_right))   return EINVAL;
    if (tree->duplicates  && (!data_left || !data_right))   return EINVAL;
    if (tree->duplicates  && key_left != data_left &&
        __toku_lt_is_infinite(key_left))                    return EINVAL;
    if (tree->duplicates  && key_right != data_right &&
        __toku_lt_is_infinite(key_right))                   return EINVAL;

    /* Verify that NULL keys have payload and size that are mutually 
       consistent*/
    __toku_lt_verify_null_key(key_left);
    __toku_lt_verify_null_key(data_left);
    __toku_lt_verify_null_key(key_right);
    __toku_lt_verify_null_key(data_right);

    __toku_init_point(left,  tree, key_left,  data_left);
    __toku_init_point(right, tree, key_right, data_right);
    __toku_init_query(query, left, right);
    /* Verify left <= right, otherwise return EDOM. */
    if (__toku_r_backwards(query))                          return EDOM;
    return 0;
}

static int __toku_lt_get_border(toku_lock_tree* tree, u_int32_t numfound,
                                toku_range* pred, toku_range* succ,
                                BOOL* found_p,    BOOL* found_s,
                                toku_range* to_insert) {
    assert(tree && pred && succ && found_p && found_s);                                    
    int r;
    toku_range_tree* rt;
    rt = (numfound == 0) ? tree->borderwrite : 
                           __toku_lt_ifexist_selfwrite(tree, tree->buf[0].data);
    if (!rt)  return __toku_lt_panic(tree);
    r = toku_rt_predecessor(rt, to_insert->left,  pred, found_p);
    if (r!=0) return r;
    r = toku_rt_successor  (rt, to_insert->right, succ, found_s);
    if (r!=0) return r;
    if (*found_p && *found_s && pred->data == succ->data) {
        return __toku_lt_panic(tree); }
    return 0;
}

static int __toku_lt_expand_border(toku_lock_tree* tree, toku_range* to_insert,
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

static int __toku_lt_split_border(toku_lock_tree* tree, toku_range* to_insert,
                                   toku_range* pred, toku_range* succ,
                                   BOOL  found_p,    BOOL  found_s) {
    assert(tree && to_insert && pred && succ);
    int r;
    assert(tree->buf[0].data != to_insert->data);
    if (!found_s || !found_p) return __toku_lt_panic(tree);

    r = toku_rt_delete(tree->borderwrite, &tree->buf[0]);
    if (r!=0) return __toku_lt_panic(tree);

    pred->left  = tree->buf[0].left;
    succ->right = tree->buf[0].right;
    if (__toku_r_backwards(pred) || __toku_r_backwards(succ)) {
        return __toku_lt_panic(tree);}

    r = toku_rt_insert(tree->borderwrite, pred);
    if (r!=0) return __toku_lt_panic(tree);
    r = toku_rt_insert(tree->borderwrite, succ);
    if (r!=0) return __toku_lt_panic(tree);
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
static int __toku_lt_borderwrite_insert(toku_lock_tree* tree,
                                        toku_range* query,
                                        toku_range* to_insert) {
    assert(tree && query && to_insert);
    int r;
    toku_range_tree* borderwrite = tree->borderwrite;   assert(borderwrite);
    const u_int32_t query_size = 1;
    toku_range   buffer[query_size];
    u_int32_t     buflen     = query_size;
    toku_range*  buf        = &buffer[0];

    u_int32_t numfound;
    r = toku_rt_find(borderwrite, query, query_size, &buf, &buflen, &numfound);
    if (r!=0) return __toku_lt_panic(tree);
    assert(numfound <= query_size);

    /* No updated needed in borderwrite: we return right away. */
    if (numfound == 1 && tree->buf[0].data == to_insert->data) return 0;

    /* Find predecessor and successors */
    toku_range pred;
    toku_range succ;
    BOOL found_p;
    BOOL found_s;

    r = __toku_lt_get_border(tree, numfound, &pred, &succ, &found_p, &found_s,
                             to_insert);
    if (r!=0) return __toku_lt_panic(tree);
    
    if (numfound == 0) {
        r = __toku_lt_expand_border(tree, to_insert, &pred,   &succ,
                                                      found_p, found_s);
        if (r!=0) return __toku_lt_panic(tree);
    }
    else {  
        r = __toku_lt_split_border( tree, to_insert, &pred, &succ, 
                                                      found_p, found_s);
        if (r!=0) return __toku_lt_panic(tree);
    }
    r = toku_rt_insert(borderwrite, to_insert);
    if (r!=0) return __toku_lt_panic(tree);
    return 0;
}

int toku_lt_create(toku_lock_tree** ptree, DB* db, BOOL duplicates,
                   int   (*panic)(DB*), size_t payload_capacity,
                   int   (*compare_fun)(DB*,const DBT*,const DBT*),
                   int   (*dup_compare)(DB*,const DBT*,const DBT*),
                   void* (*user_malloc) (size_t),
                   void  (*user_free)   (void*),
                   void* (*user_realloc)(void*, size_t)) {
    if (!ptree || !db || !compare_fun || !dup_compare || !panic ||
        !payload_capacity || !user_malloc || !user_free || !user_realloc) {
        return EINVAL;
    }
    int r;

    toku_lock_tree* tmp_tree = (toku_lock_tree*)user_malloc(sizeof(*tmp_tree));
    if (0) { died1: user_free(tmp_tree); return r; }
    if (!tmp_tree) return errno;
    memset(tmp_tree, 0, sizeof(toku_lock_tree));
    tmp_tree->db               = db;
    tmp_tree->duplicates       = duplicates;
    tmp_tree->panic            = panic;
    tmp_tree->payload_capacity = payload_capacity;
    tmp_tree->compare_fun      = compare_fun;
    tmp_tree->dup_compare      = dup_compare;
    tmp_tree->malloc           = user_malloc;
    tmp_tree->free             = user_free;
    tmp_tree->realloc          = user_realloc;
    r = toku_rt_create(&tmp_tree->mainread,
                       __toku_lt_point_cmp, __toku_lt_txn_cmp, TRUE,
                       user_malloc, user_free, user_realloc);
    if (0) { died2: toku_rt_close(tmp_tree->mainread); goto died1; }
    if (r!=0) goto died1;
    r = toku_rt_create(&tmp_tree->borderwrite,
                       __toku_lt_point_cmp, __toku_lt_txn_cmp, FALSE,
                       user_malloc, user_free, user_realloc);
    if (0) { died3: toku_rt_close(tmp_tree->borderwrite); goto died2; }
    if (r!=0) goto died2;
    tmp_tree->buflen = __toku_default_buflen;
    tmp_tree->buf    = (toku_range*)
                        user_malloc(tmp_tree->buflen * sizeof(toku_range));
    if (!tmp_tree->buf) { r = errno; goto died3; }
//TODO: Create list of selfreads
//TODO: Create list of selfwrites
    *ptree = tmp_tree;
    return 0;
}

int toku_lt_close(toku_lock_tree* tree) {
    if (!tree)                                              return EINVAL;
    int r;
    int r2 = 0;
    r = toku_rt_close(tree->mainread);
    if        (r!=0) r2 = r;
    r = toku_rt_close(tree->borderwrite);
    if (!r2 && r!=0) r2 = r;
//TODO: Do this to ALLLLLLLL selfread and ALLLLLL selfwrite tables.
    /* Free all points referred to in a range tree (and close the tree). */
r = __toku_lt_free_contents(tree,
                            __toku_lt_ifexist_selfread (tree, (DB_TXN*)1));
if (!r2 && r!=0) r2 = r;
r= __toku_lt_free_contents( tree,
                            __toku_lt_ifexist_selfwrite(tree, (DB_TXN*)1));
if (!r2 && r!=0) r2 = r;
//TODO: After freeing the tree, need to remove it from both lists!!!
//      One list probably IN the transaction, and one additional one here.
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
    
    r = __toku_lt_preprocess(tree, txn, key_left,  data_left,
                                        key_right, data_right,
                                           &left,      &right,
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
    r = __toku_lt_rt_dominates(tree, &query, 
                            __toku_lt_ifexist_selfwrite(tree, txn), &dominated);
    if (r || dominated) return r;

    /* else if 'K' is dominated by selfread('txn') then return success. */
    r = __toku_lt_rt_dominates(tree, &query, 
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
    int r;
    toku_point endpoint;
    toku_range query;
    BOOL dominated;
    toku_range_tree* mainread;
    
    r = __toku_lt_preprocess(tree, txn, key,       data,
                                        key,       data,
                                       &endpoint, &endpoint,
                             &query);
    if (r!=0) return r;
    /* if 'K' is dominated by selfwrite('txn') then return success. */
    r = __toku_lt_rt_dominates(tree, &query, 
                            __toku_lt_ifexist_selfwrite(tree, txn), &dominated);
    if (r || dominated) return r;
    /* else if K meets mainread at 'txn2' then return failure */
    BOOL met;
    mainread = tree->mainread;                          assert(mainread);
    r = __toku_lt_meets_peer(tree, &query, mainread, txn, &met);
    if (r!=0) return r; 
    if (met)  return DB_LOCK_NOTGRANTED;
    /*
        else if 'K' meets borderwrite at 'peer' ('peer'!='txn') &&
                'K' meets selfwrite('peer') then return failure.
    */
    r = __toku_lt_check_borderwrite_conflict(tree, txn, &query);
    if (r!=0) return r;
    /*  Now need to copy the memory and insert.
        No merging required in selfwrite.
        This is a point, and if merging was possible it would have been
        dominated by selfwrite.
    */
    toku_range to_insert;
    __toku_init_insert(&to_insert, &endpoint, &endpoint, txn);
    BOOL dummy = TRUE;
    r = __toku_lt_alloc_extreme(tree, &to_insert, TRUE, &dummy);
    if (0) { died1:  __toku_p_free(tree, to_insert.left);   return r; }
    if (r!=0) return r;
    toku_range_tree* selfwrite;
    r = __toku_lt_selfwrite(tree, txn, &selfwrite);
    if (r!=0) goto died1;
    assert(selfwrite);
    r = toku_rt_insert(selfwrite, &to_insert);
    if (r!=0) goto died1;
    /* Need to update borderwrite. */
    r = __toku_lt_borderwrite_insert(tree, &query, &to_insert);
    if (r!=0) return __toku_lt_panic(tree);
    return 0;
}

int toku_lt_acquire_range_write_lock(toku_lock_tree* tree, DB_TXN* txn,
                                  const DBT* key_left,  const DBT* data_left,
                                  const DBT* key_right, const DBT* data_right) {
    int r;
    toku_point left;
    toku_point right;
    toku_range query;
    
    r = __toku_lt_preprocess(tree, txn, key_left,  data_left,
                                        key_right, data_right,
                                           &left,      &right,
                             &query);
    if (r!=0) return r;

    return ENOSYS;
    //We are not ready for this.
    //Not needed for Feb 1 release.
}

int toku_lt_unlock(toku_lock_tree* tree, DB_TXN* txn);
