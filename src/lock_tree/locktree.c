/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/**
   \file  locktree.c
   \brief Lock trees: implementation
*/
  
#include <toku_portability.h>
#include <locktree.h>
#include <ydb-internal.h>
#include <brt-internal.h>
#include <toku_stdint.h>

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

#if DEBUG
static int toku_lt_debug = 0;
#endif

static inline int lt_panic(toku_lock_tree *tree, int r) {
    return tree->panic(tree->db, r);
}
                
const uint32_t __toku_default_buflen = 2;

static const DBT __toku_lt_infinity;
static const DBT __toku_lt_neg_infinity;

const DBT* const toku_lt_infinity     = &__toku_lt_infinity;
const DBT* const toku_lt_neg_infinity = &__toku_lt_neg_infinity;

static toku_pthread_mutex_t *
toku_ltm_get_mutex(toku_ltm *ltm) {
    toku_pthread_mutex_t *lock = ltm->use_lock;
    if (lock == NULL)
        lock = &ltm->lock;
    return lock;
}

void 
toku_ltm_set_mutex(toku_ltm *ltm, toku_pthread_mutex_t *use_lock) {
    ltm->use_lock = use_lock;
}

static void 
toku_ltm_init_mutex(toku_ltm *ltm) {
    int r = toku_pthread_mutex_init(&ltm->lock, NULL); assert_zero(r);
    ltm->use_lock = NULL;
}

static void 
toku_ltm_destroy_mutex(toku_ltm *ltm) {
    int r = toku_pthread_mutex_destroy(&ltm->lock); assert_zero(r);
}

void 
toku_ltm_lock_mutex(toku_ltm *ltm) {
    int r = toku_pthread_mutex_lock(toku_ltm_get_mutex(ltm)); assert_zero(r);
}

void 
toku_ltm_unlock_mutex(toku_ltm *ltm) {
    int r = toku_pthread_mutex_unlock(toku_ltm_get_mutex(ltm)); assert_zero(r);
}

char* toku_lt_strerror(TOKU_LT_ERROR r) {
    if (r >= 0) 
        return strerror(r);
    if (r == TOKU_LT_INCONSISTENT) {
        return "Locking data structures have become inconsistent.\n";
    }
    return "Unknown error in locking data structures.\n";
}
/* Compare two payloads assuming that at least one of them is infinite */ 
static inline int infinite_compare(const DBT* a, const DBT* b) {
    if (a == b)
        return  0;
    if (a == toku_lt_infinity)          
        return  1;
    if (b == toku_lt_infinity)          
        return -1;
    if (a == toku_lt_neg_infinity)      
        return -1;
    assert(b == toku_lt_neg_infinity);     
    return  1;
}

static inline BOOL lt_is_infinite(const DBT* p) {
    BOOL r;
    if (p == toku_lt_infinity || p == toku_lt_neg_infinity) {
        DBT* dbt = (DBT*)p;
        assert(!dbt->data && !dbt->size);
        r = TRUE;
    } else
        r = FALSE;
    return r;
}

/* Verifies that NULL data and size are consistent.
   i.e. The size is 0 if and only if the data is NULL. */
static inline int lt_verify_null_key(const DBT* key) {
    if (key && key->size && !key->data) 
        return EINVAL;
    return 0;
}

static inline DBT* recreate_DBT(DBT* dbt, void* payload, uint32_t length) {
    memset(dbt, 0, sizeof(DBT));
    dbt->data = payload;
    dbt->size = length;
    return dbt;
}

static inline int lt_txn_cmp(const TXNID a, const TXNID b) {
    return a < b ? -1 : (a != b);
}

static inline void toku_ltm_remove_lt(toku_ltm* mgr, toku_lock_tree* lt) {
    assert(mgr && lt);
    toku_lth_delete(mgr->lth, lt);
}

static inline int toku_ltm_add_lt(toku_ltm* mgr, toku_lock_tree* lt) {
    assert(mgr && lt);
    return toku_lth_insert(mgr->lth, lt);
}

int toku_lt_point_cmp(const toku_point* x, const toku_point* y) {
    DBT point_1;
    DBT point_2;

    assert(x && y);
    assert(x->lt);
    assert(x->lt == y->lt);

    if (lt_is_infinite(x->key_payload) ||
        lt_is_infinite(y->key_payload)) {
        /* If either payload is infinite, then:
           - if duplicates are allowed, the data must be the same 
             infinite value. 
           - if duplicates are not allowed, the data is irrelevant
             In either case, we do not have to compare data: the key will
             be the sole determinant of the comparison */
        return infinite_compare(x->key_payload, y->key_payload);
    }
    return x->lt->compare_fun(x->lt->db,
			      recreate_DBT(&point_1, x->key_payload, x->key_len),
			      recreate_DBT(&point_2, y->key_payload, y->key_len));
}

/* Lock tree manager functions begin here */
int toku_ltm_create(toku_ltm** pmgr,
                    uint32_t max_locks,
                    uint64_t max_lock_memory,
                    int   (*panic)(DB*, int), 
                    toku_dbt_cmp (*get_compare_fun_from_db)(DB*),
                    void* (*user_malloc) (size_t),
                    void  (*user_free)   (void*),
                    void* (*user_realloc)(void*, size_t)) {
    int r = ENOSYS;
    toku_ltm* tmp_mgr = NULL;

    if (!pmgr || !max_locks || !user_malloc || !user_free || !user_realloc) {
        r = EINVAL; goto cleanup;
    }
    assert(panic && get_compare_fun_from_db);

    tmp_mgr          = (toku_ltm*)user_malloc(sizeof(*tmp_mgr));
    if (!tmp_mgr) { 
        r = ENOMEM; goto cleanup; 
    }
    memset(tmp_mgr, 0, sizeof(toku_ltm));

    r = toku_ltm_set_max_locks(tmp_mgr, max_locks);
    if (r != 0)
        goto cleanup;
    r = toku_ltm_set_max_lock_memory(tmp_mgr, max_lock_memory);
    if (r != 0) 
        goto cleanup;
    tmp_mgr->panic            = panic;
    tmp_mgr->malloc           = user_malloc;
    tmp_mgr->free             = user_free;
    tmp_mgr->realloc          = user_realloc;
    tmp_mgr->get_compare_fun_from_db = get_compare_fun_from_db;

    r = toku_lth_create(&tmp_mgr->lth, user_malloc, user_free, user_realloc);
    if (r != 0) 
        goto cleanup;
    if (!tmp_mgr->lth) {
        r = ENOMEM; goto cleanup; 
    }

    r = toku_idlth_create(&tmp_mgr->idlth, user_malloc, user_free, user_realloc);
    if (r != 0) 
        goto cleanup;
    if (!tmp_mgr->idlth) { 
        r = ENOMEM; goto cleanup; 
    }
    toku_ltm_init_mutex(tmp_mgr);
    r = 0;
    *pmgr = tmp_mgr;
cleanup:
    if (r != 0) {
        if (tmp_mgr) {
            if (tmp_mgr->lth)
                toku_lth_close(tmp_mgr->lth);
            if (tmp_mgr->idlth)
                toku_idlth_close(tmp_mgr->idlth);
            user_free(tmp_mgr);
        }
    }
    return r;
}

int toku_ltm_close(toku_ltm* mgr) {
    int r           = ENOSYS;
    int first_error = 0;

    if (!mgr) { 
        r = EINVAL; goto cleanup; 
    }

    toku_lth_start_scan(mgr->lth);
    toku_lock_tree* lt;
    while ((lt = toku_lth_next(mgr->lth)) != NULL) {
        r = toku_lt_close(lt);
        if (r != 0 && first_error == 0) 
            first_error = r;
    }
    toku_lth_close(mgr->lth);
    toku_idlth_close(mgr->idlth);
    toku_ltm_destroy_mutex(mgr);
    mgr->free(mgr);

    r = first_error;
cleanup:
    return r;
}

void toku_ltm_get_status(toku_ltm* mgr, uint32_t * max_locks, uint32_t * curr_locks, 
                         uint64_t *max_lock_memory, uint64_t *curr_lock_memory,
                         LTM_STATUS s) {
    *max_locks = mgr->max_locks;
    *curr_locks = mgr->curr_locks;
    *max_lock_memory = mgr->max_lock_memory;
    *curr_lock_memory = mgr->curr_lock_memory;
    *s = mgr->status;
}

int toku_ltm_get_max_locks(toku_ltm* mgr, uint32_t* max_locks) {
    if (!mgr || !max_locks)
        return EINVAL;
    *max_locks = mgr->max_locks;
    return 0;
}

int toku_ltm_set_max_locks(toku_ltm* mgr, uint32_t max_locks) {
    if (!mgr || !max_locks)
        return EINVAL;
    if (max_locks < mgr->curr_locks) 
        return EDOM;
    mgr->max_locks = max_locks;
    return 0;
}

int toku_ltm_get_max_lock_memory(toku_ltm* mgr, uint64_t* max_lock_memory) {
    if (!mgr || !max_lock_memory)
        return EINVAL;
    *max_lock_memory = mgr->max_lock_memory;
    return 0;
}

int toku_ltm_set_max_lock_memory(toku_ltm* mgr, uint64_t max_lock_memory) {
    if (!mgr || !max_lock_memory)
        return EINVAL;
    if (max_lock_memory < mgr->curr_locks)
        return EDOM;
    mgr->max_lock_memory = max_lock_memory;
    return 0;
}

/* Functions to update the range count and compare it with the
   maximum number of ranges */
static inline BOOL ltm_lock_test_incr(toku_ltm* tree_mgr, uint32_t replace_locks) {
    assert(tree_mgr);
    assert(replace_locks <= tree_mgr->curr_locks);
    return (BOOL)(tree_mgr->curr_locks - replace_locks < tree_mgr->max_locks);
}

static inline void ltm_lock_incr(toku_ltm* tree_mgr, uint32_t replace_locks) {
    assert(ltm_lock_test_incr(tree_mgr, replace_locks));
    tree_mgr->curr_locks -= replace_locks;
    tree_mgr->curr_locks += 1;
}

static inline void ltm_lock_decr(toku_ltm* tree_mgr, uint32_t locks) {
    assert(tree_mgr);
    assert(tree_mgr->curr_locks >= locks);
    tree_mgr->curr_locks -= locks;
}

static inline void ltm_note_free_memory(toku_ltm *mgr, size_t mem) {
    assert(mgr->curr_lock_memory >= mem);
    mgr->curr_lock_memory -= mem;
}

static inline int ltm_note_allocate_memory(toku_ltm *mgr, size_t mem) {
    int r = TOKUDB_OUT_OF_LOCKS;
    if (mgr->curr_lock_memory + mem <= mgr->max_lock_memory) {
        mgr->curr_lock_memory += mem;
        r = 0;
    }
    return r;
}

static inline void p_free(toku_lock_tree* tree, toku_point* point) {
    assert(point);
    size_t freeing = sizeof(*point);
    if (!lt_is_infinite(point->key_payload)) {
        freeing += point->key_len;
        tree->free(point->key_payload);
    }
    tree->free(point);

    ltm_note_free_memory(tree->mgr, freeing);
}

/*
   Allocate and copy the payload.
*/
static inline int payload_copy(toku_lock_tree* tree,
                               void** payload_out, uint32_t* len_out,
                               void*  payload_in,  uint32_t  len_in) {
    int r = 0;
    assert(payload_out && len_out);
    if (!len_in) {
        assert(!payload_in || lt_is_infinite(payload_in));
        *payload_out = payload_in;
        *len_out     = len_in;
    }
    else {
        r = ltm_note_allocate_memory(tree->mgr, len_in);
        if (r == 0) {
            assert(payload_in);
            *payload_out = tree->malloc((size_t)len_in); //2808
            resource_assert(*payload_out);
            *len_out     = len_in;
            memcpy(*payload_out, payload_in, (size_t)len_in);
        }
    }
    return r;
}

static inline int p_makecopy(toku_lock_tree* tree, toku_point** ppoint) {
    assert(ppoint);
    toku_point*     point      = *ppoint;
    toku_point*     temp_point = NULL;
    int r;

    r = ltm_note_allocate_memory(tree->mgr, sizeof(toku_point));
    if (r != 0) 
        goto done;
    temp_point = (toku_point*)tree->malloc(sizeof(toku_point)); //2808
    resource_assert(temp_point);
    if (0) {
died1:
        tree->free(temp_point);
        ltm_note_free_memory(tree->mgr, sizeof(toku_point));
        goto done;
    }

    *temp_point = *point;

    r = payload_copy(tree,
                     &temp_point->key_payload, &temp_point->key_len,
                     point->key_payload,       point->key_len);
    if (r != 0) 
        goto died1;
    *ppoint = temp_point;
done:
    return r;
}

/* Provides access to a selfread tree for a particular transaction.
   Returns NULL if it does not exist yet. */
toku_range_tree* toku_lt_ifexist_selfread(toku_lock_tree* tree, TXNID txn) {
    assert(tree);
    rt_forest* forest = toku_rth_find(tree->rth, txn);
    return forest ? forest->self_read : NULL;
}

/* Provides access to a selfwrite tree for a particular transaction.
   Returns NULL if it does not exist yet. */
toku_range_tree* toku_lt_ifexist_selfwrite(toku_lock_tree* tree, TXNID txn) {
    assert(tree);
    rt_forest* forest = toku_rth_find(tree->rth, txn);
    return forest ? forest->self_write : NULL;
}

static inline int lt_add_locked_txn(toku_lock_tree* tree, TXNID txn) {
    int r = ENOSYS;
    BOOL half_done = FALSE;

    /* Neither selfread nor selfwrite exist. */
    r = toku_rth_insert(tree->rth, txn);
    if (r != 0)
        goto cleanup;
    r = toku_rth_insert(tree->txns_still_locked, txn);
    if (r != 0) { 
        half_done = TRUE; goto cleanup; 
    }
    r = 0;
cleanup:
    if (half_done)
        toku_rth_delete(tree->rth, txn); 
    return r;
}

/* Provides access to a selfread tree for a particular transaction.
   Creates it if it does not exist. */
static inline int lt_selfread(toku_lock_tree* tree, TXNID txn,
                              toku_range_tree** pselfread) {
    int r = ENOSYS;
    assert(tree && pselfread);

    rt_forest* forest = toku_rth_find(tree->rth, txn);
    if (!forest) {
        /* Neither selfread nor selfwrite exist. */
        r = lt_add_locked_txn(tree, txn);
        if (r != 0)
            goto cleanup;
        forest = toku_rth_find(tree->rth, txn);
    }
    assert(forest);
    if (!forest->self_read) {
        r = toku_rt_create(&forest->self_read,
                           toku_lt_point_cmp, lt_txn_cmp,
                           FALSE,
                           tree->malloc, tree->free, tree->realloc);
        if (r != 0)
            goto cleanup;
        assert(forest->self_read);
    }
    *pselfread = forest->self_read;
    r = 0;
cleanup:
    return r;
}

/* Provides access to a selfwrite tree for a particular transaction.
   Creates it if it does not exist. */
static inline int lt_selfwrite(toku_lock_tree* tree, TXNID txn,
                               toku_range_tree** pselfwrite) {
    int r = ENOSYS;
    assert(tree && pselfwrite);

    rt_forest* forest = toku_rth_find(tree->rth, txn);
    if (!forest) {
        /* Neither selfread nor selfwrite exist. */
        r = lt_add_locked_txn(tree, txn);
        if (r != 0) 
            goto cleanup;
        forest = toku_rth_find(tree->rth, txn);
    }
    assert(forest);
    if (!forest->self_write) {
        r = toku_rt_create(&forest->self_write,
                           toku_lt_point_cmp, lt_txn_cmp,
                           FALSE,
                           tree->malloc, tree->free, tree->realloc);
        if (r != 0) 
            goto cleanup;
        assert(forest->self_write);
    }
    *pselfwrite = forest->self_write;
    r = 0;
cleanup:
    return r;
}

static inline BOOL interval_dominated(toku_interval* query, toku_interval* by) {
    assert(query && by);
    return (BOOL)(toku_lt_point_cmp(query->left,  by->left) >= 0 &&
                  toku_lt_point_cmp(query->right, by->right) <= 0);
}

/*
    This function only supports non-overlapping trees.
    Uses the standard definition of dominated from the design document.
    Determines whether 'query' is dominated by 'rt'.
*/
static inline int lt_rt_dominates(toku_lock_tree* tree, toku_interval* query,
                                  toku_range_tree* rt, BOOL* dominated) {
    assert(tree && query && dominated);
    if (!rt) {
        *dominated = FALSE;
        return 0;
    }
    
    BOOL            allow_overlaps;
    const uint32_t  query_size = 1;
    toku_range      buffer[query_size];
    uint32_t        buflen     = query_size;
    toku_range*     buf        = &buffer[0];
    uint32_t        numfound;
    int             r;

    /* Sanity check. (Function only supports non-overlap range trees.) */
    r = toku_rt_get_allow_overlaps(rt, &allow_overlaps);
    if (r != 0) 
        return r;
    assert(!allow_overlaps);

    r = toku_rt_find(rt, query, query_size, &buf, &buflen, &numfound);
    if (r != 0) 
        return r;
    if (numfound == 0) {
        *dominated = FALSE;
        return 0;
    }
    assert(numfound == 1);
    *dominated = interval_dominated(query, &buf[0].ends);
    return 0;
}

typedef enum {TOKU_NO_CONFLICT, TOKU_MAYBE_CONFLICT, TOKU_YES_CONFLICT} toku_conflict;

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
static inline int lt_borderwrite_conflict(toku_lock_tree* tree, TXNID self,
                                          toku_interval* query,
                                          toku_conflict* conflict, TXNID* peer) {
    assert(tree && query && conflict && peer);
    toku_range_tree* rt = tree->borderwrite;
    assert(rt);

    const uint32_t query_size = 2;
    toku_range   buffer[query_size];
    uint32_t     buflen     = query_size;
    toku_range*  buf        = &buffer[0];
    uint32_t     numfound;
    int          r;

    r = toku_rt_find(rt, query, query_size, &buf, &buflen, &numfound);
    if (r != 0) 
        return r;
    assert(numfound <= query_size);
    if (numfound == 2) 
        *conflict = TOKU_YES_CONFLICT;
    else if (numfound == 0 || !lt_txn_cmp(buf[0].data, self)) 
        *conflict = TOKU_NO_CONFLICT;
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
static inline int lt_meets(toku_lock_tree* tree, toku_interval* query, toku_range_tree* rt, BOOL* met) {
    assert(tree && query && rt && met);
    const uint32_t query_size = 1;
    toku_range   buffer[query_size];
    uint32_t     buflen     = query_size;
    toku_range*  buf        = &buffer[0];
    uint32_t     numfound;
    int          r;
    BOOL         allow_overlaps;

    /* Sanity check. (Function only supports non-overlap range trees.) */
    r = toku_rt_get_allow_overlaps(rt, &allow_overlaps);
    if (r != 0) 
        return r;
    assert(!allow_overlaps);

    r = toku_rt_find(rt, query, query_size, &buf, &buflen, &numfound);
    if (r != 0) 
        return r;
    assert(numfound <= query_size);
    *met = (BOOL)(numfound != 0);
    return 0;
}

/* 
    Determines whether 'query' meets 'rt' at txn2 not equal to txn.
    This function supports all range trees, but queries must either be a single point,
    or the range tree is homogenous.
    Uses the standard definition of 'query' meets 'tree' at 'data' from the
    design document.
*/
static inline int lt_meets_peer(toku_lock_tree* tree, toku_interval* query, 
                                toku_range_tree* rt, BOOL is_homogenous,
                                TXNID self, BOOL* met) {
    assert(tree && query && rt && met);
    assert(query->left == query->right || is_homogenous);

    const uint32_t query_size = is_homogenous ? 1 : 2;
    toku_range   buffer[2];
    uint32_t     buflen     = query_size;
    toku_range*  buf        = &buffer[0];
    uint32_t     numfound;
    int          r;

    r = toku_rt_find(rt, query, query_size, &buf, &buflen, &numfound);
    if (r != 0) 
        return r;
    assert(numfound <= query_size);
    *met = (BOOL) (numfound == 2 || (numfound == 1 && lt_txn_cmp(buf[0].data, self)));
    return 0;
}

/* Checks for if a write range conflicts with reads.
   Supports ranges. */
static inline int lt_write_range_conflicts_reads(toku_lock_tree* tree, TXNID txn, toku_interval* query) {
    int r    = 0;
    BOOL met = FALSE;
    toku_rth_start_scan(tree->rth);
    rt_forest* forest;
    
    while ((forest = toku_rth_next(tree->rth)) != NULL) {
        if (forest->self_read != NULL && lt_txn_cmp(forest->hash_key, txn)) {
            r = lt_meets_peer(tree, query, forest->self_read, TRUE, txn, &met);
            if (r != 0)
                goto cleanup;
            if (met)  { 
                r = DB_LOCK_NOTGRANTED; goto cleanup; 
            }
        }
    }
    r = 0;
cleanup:
    return r;
}

#if !TOKU_LT_USE_BORDERWRITE

static inline int lt_write_range_conflicts_writes(toku_lock_tree* tree, TXNID txn, toku_interval* query) {
    int r    = 0;
    BOOL met = FALSE;
    toku_rth_start_scan(tree->rth);
    rt_forest* forest;
    
    while ((forest = toku_rth_next(tree->rth)) != NULL) {
        if (forest->self_write != NULL && lt_txn_cmp(forest->hash_key, txn)) {
            r = lt_meets_peer(tree, query, forest->self_write, TRUE, txn, &met);
            if (r != 0) 
                goto cleanup;
            if (met)  { 
                r = DB_LOCK_NOTGRANTED; goto cleanup; 
            }
        }
    }
    r = 0;
cleanup:
    return r;
}

#endif

/*
    Utility function to implement: (from design document)
    if K meets E at v'!=t and K meets W_v' then return failure.
*/
static inline int lt_check_borderwrite_conflict(toku_lock_tree* tree, TXNID txn, toku_interval* query) {
#if TOKU_LT_USE_BORDERWRITE
    assert(tree && query);
    toku_conflict conflict;
    TXNID peer;
    toku_range_tree* peer_selfwrite;
    int r;
    
    r = lt_borderwrite_conflict(tree, txn, query, &conflict, &peer);
    if (r != 0) 
        return r;
    if (conflict == TOKU_MAYBE_CONFLICT) {
        peer_selfwrite = toku_lt_ifexist_selfwrite(tree, peer);
        if (!peer_selfwrite) 
            return lt_panic(tree, TOKU_LT_INCONSISTENT);

        BOOL met;
        r = lt_meets(tree, query, peer_selfwrite, &met);
        if (r != 0)   
            return r;
        conflict = met ? TOKU_YES_CONFLICT : TOKU_NO_CONFLICT;
    }
    if (conflict == TOKU_YES_CONFLICT) 
        return DB_LOCK_NOTGRANTED;
    assert(conflict == TOKU_NO_CONFLICT);
    return 0;
#else
    int r = lt_write_range_conflicts_writes(tree, txn, query);
    return r;
#endif
}

static inline void payload_from_dbt(void** payload, uint32_t* len, const DBT* dbt) {
    assert(payload && len && dbt);
    if (lt_is_infinite(dbt)) *payload = (void*)dbt;
    else if (!dbt->size) {
        *payload = NULL;
        *len     = 0;
    } else {
        assert(dbt->data);
        *payload = dbt->data;
        *len     = dbt->size;
    }
}

static inline void init_point(toku_point* point, toku_lock_tree* tree, const DBT* key) {
    assert(point && tree && key);
    memset(point, 0, sizeof(toku_point));
    point->lt = tree;

    payload_from_dbt(&point->key_payload, &point->key_len, key);
}

static inline void init_query(toku_interval* query, toku_point* left, toku_point* right) {
    query->left  = left;
    query->right = right;
}

/*
    Memory ownership: 
     - to_insert we own (it's static)
     - to_insert.ends.left, .ends.right are toku_point's, and we own them.
       If we have consolidated, we own them because we had allocated
       them earlier, but
       if we have not consolidated we need to gain ownership now: 
       we will gain ownership by copying all payloads and 
       allocating the points. 
     - to_insert.{ends.left,ends.right}.{key_payload, data_payload} are owned by lt,
       we made copies from the DB at consolidation time 
*/

static inline void init_insert(toku_range* to_insert,
                               toku_point* left, toku_point* right,
                               TXNID txn) {
    to_insert->ends.left  = left;
    to_insert->ends.right = right;
    to_insert->data  = txn;
}

/* Returns whether the point already exists
   as an endpoint of the given range. */
static inline BOOL lt_p_independent(toku_point* point, toku_interval* range) {
    assert(point && range);
    return (BOOL)(point != range->left && point != range->right);
}

static inline int lt_determine_extreme(toku_lock_tree* tree,
                                             toku_range* to_insert,
                                             BOOL* alloc_left, BOOL* alloc_right,
                                             uint32_t numfound,
                                             uint32_t start_at) {
    assert(to_insert && tree && alloc_left && alloc_right);
    uint32_t i;
    assert(numfound <= tree->buflen);
    for (i = start_at; i < numfound; i++) {
        int c;
        /* Find the extreme left end-point among overlapping ranges */
        if ((c = toku_lt_point_cmp(tree->buf[i].ends.left, to_insert->ends.left))
            <= 0) {
            if ((!*alloc_left && c == 0) ||
                !lt_p_independent(tree->buf[i].ends.left, &to_insert->ends)) {
                return lt_panic(tree, TOKU_LT_INCONSISTENT); }
            *alloc_left      = FALSE;
            to_insert->ends.left  = tree->buf[i].ends.left;
        }
        /* Find the extreme right end-point */
        if ((c = toku_lt_point_cmp(tree->buf[i].ends.right, to_insert->ends.right))
            >= 0) {
            if ((!*alloc_right && c == 0) ||
                (tree->buf[i].ends.right == to_insert->ends.left &&
                 tree->buf[i].ends.left  != to_insert->ends.left) ||
                 tree->buf[i].ends.right == to_insert->ends.right) {
                return lt_panic(tree, TOKU_LT_INCONSISTENT); }
            *alloc_right     = FALSE;
            to_insert->ends.right = tree->buf[i].ends.right;
        }
    }
    return 0;
}

/* Find extreme given a starting point. */
static inline int lt_extend_extreme(toku_lock_tree* tree,toku_range* to_insert,
                                          BOOL* alloc_left, BOOL* alloc_right,
                                          uint32_t numfound) {
    return lt_determine_extreme(tree, to_insert, alloc_left, alloc_right,
                                      numfound, 0);
}

/* Has no starting point. */
static inline int lt_find_extreme(toku_lock_tree* tree,
                                  toku_range* to_insert,
                                  uint32_t numfound) {
    assert(numfound > 0);
    *to_insert = tree->buf[0];
    BOOL ignore_left = TRUE;
    BOOL ignore_right = TRUE;
    return lt_determine_extreme(tree, to_insert, &ignore_left, &ignore_right, numfound, 1);
}

static inline int lt_alloc_extreme(toku_lock_tree* tree, toku_range* to_insert,
                                   BOOL alloc_left, BOOL* alloc_right) {
    assert(to_insert && alloc_right);
    BOOL copy_left = FALSE;
    int r;
    
    /* The pointer comparison may speed up the evaluation in some cases, 
       but it is not strictly needed */
    if (alloc_left && alloc_right &&
        (to_insert->ends.left == to_insert->ends.right ||
         toku_lt_point_cmp(to_insert->ends.left, to_insert->ends.right) == 0)) {
        *alloc_right = FALSE;
        copy_left    = TRUE;
    }

    if (alloc_left) {
        r = p_makecopy(tree, &to_insert->ends.left);
        if (0) { 
        died1:
            if (alloc_left) 
                p_free(tree, to_insert->ends.left); 
            return r; 
        }
        if (r != 0) 
            return r;
    }
    if (*alloc_right) {
        assert(!copy_left);
        r = p_makecopy(tree, &to_insert->ends.right);
        if (r != 0) 
            goto died1;
    }
    else if (copy_left) 
        to_insert->ends.right = to_insert->ends.left;
    return 0;
}

static inline int lt_delete_overlapping_ranges(toku_lock_tree* tree,
                                               toku_range_tree* rt,
                                               uint32_t numfound) {
    assert(tree && rt);
    int r;
    uint32_t i;
    assert(numfound <= tree->buflen);
    for (i = 0; i < numfound; i++) {
        r = toku_rt_delete(rt, &tree->buf[i]);
        if (r != 0) 
            return r;
    }
    return 0;
}

static inline int lt_free_points(toku_lock_tree* tree, toku_interval* to_insert, uint32_t numfound) {
    assert(tree && to_insert);
    assert(numfound <= tree->buflen);

    for (uint32_t i = 0; i < numfound; i++) {
        /*
           We will maintain the invariant: (separately for read and write environments)
           (toku_lt_point_cmp(a, b) == 0 && a.txn == b.txn) => a == b
        */
        /* Do not double-free. */
        if (tree->buf[i].ends.right != tree->buf[i].ends.left &&
            lt_p_independent(tree->buf[i].ends.right, to_insert)) {
            p_free(tree, tree->buf[i].ends.right);
        }
        if (lt_p_independent(tree->buf[i].ends.left,  to_insert)) {
            p_free(tree, tree->buf[i].ends.left);
        }
    }
    return 0;
}

static inline int lt_borderwrite_insert(toku_lock_tree* tree, toku_interval* query, toku_range* to_insert);

/* Consolidate the new range and all the overlapping ranges
   If found_only is TRUE, we're only consolidating existing ranges in the interval
   specified inside of to_insert.
*/
static inline int consolidate_range_tree(toku_lock_tree* tree, BOOL found_only, toku_range* to_insert, toku_range_tree *rt, 
                                         BOOL do_borderwrite_insert) {
    assert(tree && to_insert);

    int r;
    BOOL             alloc_left    = TRUE;
    BOOL             alloc_right   = TRUE;
    toku_interval* query = &to_insert->ends;

    /* Find all overlapping ranges in the range tree */
    uint32_t numfound;
    r = toku_rt_find(rt, query, 0, &tree->buf, &tree->buflen, &numfound);
    if (r != 0) 
        return r;
    assert(numfound <= tree->buflen); // RFP?
    if (found_only) {
        /* If there is 0 or 1 found, it is already consolidated. */
        if (numfound < 2) 
            return 0;
        /* Copy the first one, so we only consolidate existing entries. */
        r = lt_find_extreme(tree, to_insert, numfound);
        if (r != 0) 
            return r;
        alloc_left = FALSE;
        alloc_right = FALSE;
    } else {
        /* Find the extreme left and right point of the consolidated interval */
        r = lt_extend_extreme(tree, to_insert, &alloc_left, &alloc_right, numfound);
        if (r != 0) 
            return r;
        if (!ltm_lock_test_incr(tree->mgr, numfound))
            return TOKUDB_OUT_OF_LOCKS;
    }
    /* Allocate the consolidated range */
    r = lt_alloc_extreme(tree, to_insert, alloc_left, &alloc_right);
    if (0) { died1:
        if (alloc_left)  p_free(tree, to_insert->ends.left);
        if (alloc_right) p_free(tree, to_insert->ends.right); 
        return r; 
    }
    if (r != 0)
        return r;
    /* From this point on we have to panic if we cannot finish. */
    /* Delete overlapping ranges from range tree ... */
    r = lt_delete_overlapping_ranges(tree, rt, numfound);
    if (r != 0) 
        return lt_panic(tree, r);

    if (do_borderwrite_insert) {
#if TOKU_LT_USE_BORDERWRITE
        toku_range borderwrite_insert = *to_insert;
        r = lt_borderwrite_insert(tree, query, &borderwrite_insert);
        if (r != 0) 
            return lt_panic(tree, r);
#endif
    }

    /* Free all the points from ranges in tree->buf[0]..tree->buf[numfound-1] */
    lt_free_points(tree, &to_insert->ends, numfound);

    /* We don't necessarily need to panic after here unless numfound > 0
       Which indicates we deleted something. */
    /* Insert extreme range into range tree */
    /* VL */
    r = toku_rt_insert(rt, to_insert);
    if (r != 0) {
        /* If we deleted/merged anything, this is a panic situation. */
        if (numfound) 
            return lt_panic(tree, TOKU_LT_INCONSISTENT);
        goto died1; 
    }

    ltm_lock_incr(tree->mgr, numfound);
    return 0;
}

static inline int consolidate_reads(toku_lock_tree* tree, BOOL found_only, toku_range* to_insert, TXNID txn) {
    assert(tree && to_insert);
    toku_range_tree* selfread;
    int r = lt_selfread(tree, txn, &selfread);
    if (r != 0) 
        return r;
    assert(selfread);
    return consolidate_range_tree(tree, found_only, to_insert, selfread, FALSE);
}

static inline int consolidate_writes(toku_lock_tree* tree, toku_range* to_insert, TXNID txn) {
    assert(tree && to_insert);
    toku_range_tree* selfwrite;
    int r = lt_selfwrite(tree, txn, &selfwrite);
    if (r != 0) 
        return r;
    assert(selfwrite);
    return consolidate_range_tree(tree, FALSE, to_insert, selfwrite, TRUE);
}

static inline void lt_init_full_query(toku_lock_tree* tree, toku_interval* query,
                                      toku_point* left, toku_point* right) {
    init_point(left,  tree, (DBT*)toku_lt_neg_infinity);
    init_point(right, tree, (DBT*)toku_lt_infinity);
    init_query(query, left, right);
}

typedef struct {
    toku_lock_tree*  lt;
    toku_interval*   query;
    toku_range*      store_value;
} free_contents_info;

static int free_contents_helper(toku_range* value, void* extra) {
    free_contents_info* info = extra;
    int r               = ENOSYS;

    *info->store_value = *value;
    if ((r=lt_free_points(info->lt, info->query, 1))) {
        return lt_panic(info->lt, r);
    }
    return 0;
}

/*
    TODO: Refactor.
    lt_free_points should be replaced (or supplanted) with a 
    lt_free_point (singular)
*/
static inline int lt_free_contents(toku_lock_tree* tree, toku_range_tree* rt, BOOL doclose) {
    assert(tree);
    if (!rt) 
        return 0;
    
    int r;

    toku_interval query;
    toku_point left;
    toku_point right;
    lt_init_full_query(tree, &query, &left, &right);
    free_contents_info info;
    info.lt          = tree;
    info.query       = &query;
    info.store_value = &tree->buf[0];

    if ((r = toku_rt_iterate(rt, free_contents_helper, &info))) 
        return r;
    if (doclose) 
        r = toku_rt_close(rt);
    else {
        r = 0;
        toku_rt_clear(rt);
    }
    assert_zero(r);
    return r;
}

static inline BOOL r_backwards(toku_interval* range) {
    assert(range && range->left && range->right);
    toku_point* left  = (toku_point*)range->left;
    toku_point* right = (toku_point*)range->right;

    /* Optimization: if all the pointers are equal, clearly left == right. */
    return (BOOL) ((left->key_payload  != right->key_payload) &&
                   (toku_lt_point_cmp(left, right) > 0));
}

static inline int lt_unlock_deferred_txns(toku_lock_tree* tree);

static inline void lt_set_comparison_functions(toku_lock_tree* tree,
                                                     DB* db) {
    assert(!tree->db && !tree->compare_fun);
    tree->db = db;
    tree->compare_fun = tree->get_compare_fun_from_db(tree->db);
    assert(tree->compare_fun);
}

static inline void lt_clear_comparison_functions(toku_lock_tree* tree) {
    assert(tree);
    tree->db          = NULL;
    tree->compare_fun = NULL; 
}

/* Preprocess step for acquire functions. */
static inline int lt_preprocess(toku_lock_tree* tree, DB* db,
                                  __attribute__((unused)) TXNID txn,
                                  const DBT* key_left,
                                  const DBT* key_right,
                                  toku_point* left, toku_point* right,
                                  toku_interval* query) {
    int r = ENOSYS;

    if (!tree || !db || !key_left || !key_right) {
        r = EINVAL; goto cleanup; 
    }

    /* Verify that NULL keys have payload and size that are mutually 
       consistent*/
    if ((r = lt_verify_null_key(key_left))   != 0)
        goto cleanup;
    if ((r = lt_verify_null_key(key_right))  != 0) 
        goto cleanup;

    init_point(left,  tree, key_left);
    init_point(right, tree, key_right);
    init_query(query, left, right);

    lt_set_comparison_functions(tree, db);

    /* Verify left <= right, otherwise return EDOM. */
    if (r_backwards(query)) { 
        r = EDOM; goto cleanup; 
    }
    r = 0;
cleanup:
    if (r == 0) {
        assert(tree->db && tree->compare_fun);
        /* Cleanup all existing deleted transactions */
        if (!toku_rth_is_empty(tree->txns_to_unlock)) {
            r = lt_unlock_deferred_txns(tree);
        }
    }
    return r;
}

/* Postprocess step for acquire functions. */
static inline void lt_postprocess(toku_lock_tree* tree) {
    lt_clear_comparison_functions(tree);
}

static inline int lt_get_border_in_selfwrite(toku_lock_tree* tree,
                                             toku_range* pred, toku_range* succ,
                                             BOOL* found_p,    BOOL* found_s,
                                             toku_range* to_insert) {
    assert(tree && pred && succ && found_p && found_s);                                    
    int r;
    toku_range_tree* rt = toku_lt_ifexist_selfwrite(tree, tree->bw_buf[0].data);
    if (!rt)  
        return lt_panic(tree, TOKU_LT_INCONSISTENT);
    r = toku_rt_predecessor(rt, to_insert->ends.left,  pred, found_p);
    if (r != 0) 
        return r;
    r = toku_rt_successor  (rt, to_insert->ends.right, succ, found_s);
    if (r != 0) 
        return r;
    return 0;
}

static inline int lt_get_border_in_borderwrite(toku_lock_tree* tree,
                                               toku_range* pred, toku_range* succ,
                                               BOOL* found_p,    BOOL* found_s,
                                               toku_range* to_insert) {
    assert(tree && pred && succ && found_p && found_s);                                    
    int r;
    toku_range_tree* rt = tree->borderwrite;
    if (!rt)  
        return lt_panic(tree, TOKU_LT_INCONSISTENT);
    r = toku_rt_predecessor(rt, to_insert->ends.left,  pred, found_p);
    if (r != 0) 
        return r;
    r = toku_rt_successor  (rt, to_insert->ends.right, succ, found_s);
    if (r != 0) 
        return r;
    return 0;
}

static inline int lt_expand_border(toku_lock_tree* tree, toku_range* to_insert,
                                   toku_range* pred, toku_range* succ,
                                   BOOL  found_p,    BOOL  found_s) {
    assert(tree && to_insert && pred && succ);
    int r;
    if (found_p && !lt_txn_cmp(pred->data, to_insert->data)) {
        r = toku_rt_delete(tree->borderwrite, pred);
        if (r != 0)
            return r;
        to_insert->ends.left = pred->ends.left;
    }
    else if (found_s && !lt_txn_cmp(succ->data, to_insert->data)) {
        r = toku_rt_delete(tree->borderwrite, succ);
        if (r != 0) 
            return r;
        to_insert->ends.right = succ->ends.right;
    }
    return 0;
}

static inline int lt_split_border(toku_lock_tree* tree, toku_range* to_insert,
                                  toku_range* pred, toku_range* succ,
                                  BOOL  found_p,    BOOL  found_s) {
    assert(tree && to_insert && pred && succ);
    int r;
    assert(lt_txn_cmp(tree->bw_buf[0].data, to_insert->data));
    if (!found_s || !found_p) 
        return lt_panic(tree, TOKU_LT_INCONSISTENT);

    r = toku_rt_delete(tree->borderwrite, &tree->bw_buf[0]);
    if (r != 0) 
        return lt_panic(tree, r);

    pred->ends.left  = tree->bw_buf[0].ends.left;
    succ->ends.right = tree->bw_buf[0].ends.right;
    if (r_backwards(&pred->ends) || r_backwards(&succ->ends)) {
        return lt_panic(tree, TOKU_LT_INCONSISTENT);}

    r = toku_rt_insert(tree->borderwrite, pred);
    if (r != 0) 
        return lt_panic(tree, r);
    r = toku_rt_insert(tree->borderwrite, succ);
    if (r != 0) 
        return lt_panic(tree, r);
    return 0;
}

/*
    NO MEMORY GETS FREED!!!!!!!!!!, it all is tied to selfwrites.
*/
static inline int lt_borderwrite_insert(toku_lock_tree* tree,
                                        toku_interval* query,
                                        toku_range* to_insert) {
    assert(tree && query && to_insert);
    int r;
    toku_range_tree* borderwrite = tree->borderwrite;   
    assert(borderwrite);

    // find all overlapping ranges.  there can be 0 or 1.
    const uint32_t query_size = 1;
    uint32_t numfound;
    r = toku_rt_find(borderwrite, query, query_size, &tree->bw_buf, &tree->bw_buflen, &numfound);
    if (r != 0) 
        return lt_panic(tree, r);
    assert(numfound <= query_size);

    if (numfound == 0) { 
        // Find the adjacent ranges in the borderwrite tree and expand them if they are owned by me

        // Find the predecessor and successor of the range to be inserted
        toku_range pred; BOOL found_p = FALSE;
        toku_range succ; BOOL found_s = FALSE;
        r = lt_get_border_in_borderwrite(tree, &pred, &succ, &found_p, &found_s, to_insert);
        if (r != 0) 
            return lt_panic(tree, r);
        if (found_p && found_s && !lt_txn_cmp(pred.data, succ.data)) {
            return lt_panic(tree, TOKU_LT_INCONSISTENT); }
        r = lt_expand_border(tree, to_insert, &pred, &succ, found_p, found_s);
        if (r != 0) 
            return lt_panic(tree, r);
        r = toku_rt_insert(borderwrite, to_insert);
        if (r != 0) 
            return lt_panic(tree, r);
    } else {
        assert(numfound == 1);
        if (!lt_txn_cmp(tree->bw_buf[0].data, to_insert->data)) { // the range overlaps a borderrange owned by me
            if (interval_dominated(&to_insert->ends, &tree->bw_buf[0].ends)) { // the range is dominated by the borderwrite range
                r = 0;
            } else {
                // expand the existing borderwrite range to include the range to be inserted
                if (toku_lt_point_cmp(to_insert->ends.left, tree->bw_buf[0].ends.left) > 0)
                    to_insert->ends.left = tree->buf[0].ends.left;
                if (toku_lt_point_cmp(to_insert->ends.right, tree->bw_buf[0].ends.right) < 0)
                    to_insert->ends.right = tree->buf[0].ends.right;
                r = toku_rt_delete(borderwrite, &tree->bw_buf[0]);
                if (r != 0)
                    return lt_panic(tree, r);
                r = toku_rt_insert(borderwrite, to_insert);
                if (r != 0)
                    return lt_panic(tree, r);
            }
        } else {
            // The range to be inserted overlapped with a borderwrite range owned by some other transaction.
            // Split the borderwrite range to remove the overlap with the range being inserted.

            // Find predecessor and successor of the range to be inserted
            toku_range pred; BOOL found_p = FALSE;
            toku_range succ; BOOL found_s = FALSE;
            r = lt_get_border_in_selfwrite(tree, &pred, &succ, &found_p, &found_s, to_insert);
            if (r != 0) 
                return lt_panic(tree, r);
            r = lt_split_border(tree, to_insert, &pred, &succ, found_p, found_s);
            if (r != 0) 
                return lt_panic(tree, r);
            r = toku_rt_insert(borderwrite, to_insert);
            if (r != 0) 
                return lt_panic(tree, r);
        }
    }

    return r;
}

/* TODO: Investigate better way of passing comparison functions. */
int toku_lt_create(toku_lock_tree** ptree,
                   int   (*panic)(DB*, int), 
                   toku_ltm* mgr,
                   toku_dbt_cmp (*get_compare_fun_from_db)(DB*),
                   void* (*user_malloc) (size_t),
                   void  (*user_free)   (void*),
                   void* (*user_realloc)(void*, size_t)) {
    int r = ENOSYS;
    toku_lock_tree* tmp_tree = NULL;
    if (!ptree || !mgr ||
        !get_compare_fun_from_db || !panic ||
        !user_malloc || !user_free || !user_realloc) {
        r = EINVAL; goto cleanup;
    }

    tmp_tree = (toku_lock_tree*)user_malloc(sizeof(*tmp_tree));
    if (!tmp_tree) { r = ENOMEM; goto cleanup; }
    memset(tmp_tree, 0, sizeof(toku_lock_tree));
    tmp_tree->panic            = panic;
    tmp_tree->mgr              = mgr;
    tmp_tree->malloc           = user_malloc;
    tmp_tree->free             = user_free;
    tmp_tree->realloc          = user_realloc;
    tmp_tree->get_compare_fun_from_db = get_compare_fun_from_db;
    tmp_tree->lock_escalation_allowed = TRUE;
    r = toku_rt_create(&tmp_tree->borderwrite,
                       toku_lt_point_cmp, lt_txn_cmp, FALSE,
                       user_malloc, user_free, user_realloc);
    if (r != 0)
        goto cleanup;
    r = toku_rth_create(&tmp_tree->rth, user_malloc, user_free, user_realloc);
    if (r != 0)
        goto cleanup;
    r = toku_rth_create(&tmp_tree->txns_to_unlock, user_malloc, user_free, user_realloc);
    if (r != 0) 
        goto cleanup; 
    r = toku_rth_create(&tmp_tree->txns_still_locked, user_malloc, user_free, user_realloc);
    if (r != 0)
        goto cleanup;
    tmp_tree->buflen = __toku_default_buflen;
    tmp_tree->buf    = (toku_range*)
                        user_malloc(tmp_tree->buflen * sizeof(toku_range));
    if (!tmp_tree->buf) { r = ENOMEM; goto cleanup; }
    tmp_tree->bw_buflen = __toku_default_buflen;
    tmp_tree->bw_buf    = (toku_range*)
                        user_malloc(tmp_tree->bw_buflen * sizeof(toku_range));
    if (!tmp_tree->bw_buf) { r = ENOMEM; goto cleanup; }
    r = toku_omt_create(&tmp_tree->dbs);
    if (r != 0) 
        goto cleanup;
    toku_lock_request_tree_init(tmp_tree);
    
    tmp_tree->ref_count = 1;
    *ptree = tmp_tree;
    r = 0;
cleanup:
    if (r != 0) {
        if (tmp_tree) {
            assert(user_free);
            if (tmp_tree->borderwrite)
                toku_rt_close(tmp_tree->borderwrite);
            if (tmp_tree->rth)
                toku_rth_close(tmp_tree->rth);
            if (tmp_tree->txns_to_unlock)
                toku_rth_close(tmp_tree->txns_to_unlock);
            if (tmp_tree->buf)
                user_free(tmp_tree->buf);
            if (tmp_tree->bw_buf)
                user_free(tmp_tree->bw_buf);
            if (tmp_tree->dbs)
                toku_omt_destroy(&tmp_tree->dbs);
            user_free(tmp_tree);
        }
    }
    return r;
}

void toku_ltm_invalidate_lt(toku_ltm* mgr, DICTIONARY_ID dict_id) {
    assert(mgr && dict_id.dictid != DICTIONARY_ID_NONE.dictid);
    toku_lt_map* map = NULL;
    map = toku_idlth_find(mgr->idlth, dict_id);
    if (map) { 
	toku_idlth_delete(mgr->idlth, dict_id);
    }
}


static inline void toku_lt_set_dict_id(toku_lock_tree* lt, DICTIONARY_ID dict_id) {
    assert(lt && dict_id.dictid != DICTIONARY_ID_NONE.dictid);
    lt->dict_id = dict_id;
}

static void lt_add_db(toku_lock_tree* tree, DB *db);
static void lt_remove_db(toku_lock_tree* tree, DB *db);

int toku_ltm_get_lt(toku_ltm* mgr, toku_lock_tree** ptree, 
                    DICTIONARY_ID dict_id, DB *db) {
    /* first look in hash table to see if lock tree exists for that db,
       if so return it */
    int r = ENOSYS;
    toku_lt_map* map     = NULL;
    toku_lock_tree* tree = NULL;
    BOOL added_to_ltm    = FALSE;
    BOOL added_to_idlth  = FALSE;
    BOOL added_extant_db  = FALSE;
    
    map = toku_idlth_find(mgr->idlth, dict_id);
    if (map != NULL) {
        /* Load already existing lock tree. */
        tree = map->tree;
        assert (tree != NULL);
        toku_lt_add_ref(tree);
        lt_add_db(tree, db);
        *ptree = tree;
        r = 0;
        goto cleanup;
    }
    /* Must create new lock tree for this dict_id*/
    r = toku_lt_create(&tree, mgr->panic, mgr,
                       mgr->get_compare_fun_from_db,
                       mgr->malloc, mgr->free, mgr->realloc);
    if (r != 0)
        goto cleanup;
    toku_lt_set_dict_id(tree, dict_id);
    /* add tree to ltm */
    r = toku_ltm_add_lt(mgr, tree);
    if (r != 0)
        goto cleanup;
    added_to_ltm = TRUE;

    /* add mapping to idlth*/
    r = toku_idlth_insert(mgr->idlth, dict_id);
    if (r != 0) 
        goto cleanup;
    added_to_idlth = TRUE;

    lt_add_db(tree, db);
    added_extant_db = TRUE;
    
    map = toku_idlth_find(mgr->idlth, dict_id);
    assert(map);
    map->tree = tree;

    /* No add ref needed because ref count was set to 1 in toku_lt_create */        
    *ptree = tree;
    r = 0;
cleanup:
    if (r != 0) {
        if (tree != NULL) {
            if (added_to_ltm)
                toku_ltm_remove_lt(mgr, tree);
            if (added_to_idlth)
                toku_idlth_delete(mgr->idlth, dict_id);
            if (added_extant_db)
                lt_remove_db(tree, db);
            toku_lt_close(tree); 
        }
    }
    return r;
}

int toku_lt_close(toku_lock_tree* tree) {
    int r = ENOSYS;
    int first_error = 0;
    if (!tree) { 
        r = EINVAL; goto cleanup; 
    }
    toku_lock_request_tree_destroy(tree);
    r = toku_rt_close(tree->borderwrite);
    if (!first_error && r != 0)
        first_error = r;

    toku_rth_start_scan(tree->rth);
    rt_forest* forest;
    
    while ((forest = toku_rth_next(tree->rth)) != NULL) {
        r = lt_free_contents(tree, forest->self_read, TRUE);
        if (!first_error && r != 0)
            first_error = r;
        r = lt_free_contents(tree, forest->self_write, TRUE);
        if (!first_error && r != 0) 
            first_error = r;
    }
    toku_rth_close(tree->rth);
    toku_rth_close(tree->txns_to_unlock);
    toku_rth_close(tree->txns_still_locked);
    toku_omt_destroy(&tree->dbs);

    tree->free(tree->buf);
    tree->free(tree->bw_buf);
    tree->free(tree);
    r = first_error;
cleanup:
    return r;
}

// toku_lt_acquire_read_lock() used only by test programs
int toku_lt_acquire_read_lock(toku_lock_tree* tree,
                              DB* db, TXNID txn,
                              const DBT* key) {
    return toku_lt_acquire_range_read_lock(tree, db, txn, key, key);
}


static int lt_try_acquire_range_read_lock(toku_lock_tree* tree,
                                          DB* db, TXNID txn,
                                          const DBT* key_left,
                                          const DBT* key_right) {
    int r;
    toku_point left;
    toku_point right;
    toku_interval query;
    BOOL dominated;
    
    r = lt_preprocess(tree, db, txn, 
                            key_left,
                            key_right,
                            &left, &right,
                            &query);
    if (r != 0)
        goto cleanup;

    /*
        For transaction 'txn' to acquire a read-lock on range 'K'=['ends.left','ends.right']:
            if 'K' is dominated by selfwrite('txn') then return success.
            else if 'K' is dominated by selfread('txn') then return success.
            else if 'K' meets borderwrite at 'peer' ('peer'!='txn') &&
                    'K' meets selfwrite('peer') then return failure.
            else
                add 'K' to selfread('txn') and selfwrite('txn').
                This requires merging.. see below.
    */

    /* if 'K' is dominated by selfwrite('txn') then return success. */
    r = lt_rt_dominates(tree, &query, 
                            toku_lt_ifexist_selfwrite(tree, txn), &dominated);
    if (r || dominated) 
        goto cleanup;

    /* else if 'K' is dominated by selfread('txn') then return success. */
    r = lt_rt_dominates(tree, &query, 
                            toku_lt_ifexist_selfread(tree, txn), &dominated);
    if (r || dominated) 
        goto cleanup;
    /*
        else if 'K' meets borderwrite at 'peer' ('peer'!='txn') &&
                'K' meets selfwrite('peer') then return failure.
    */
    r = lt_check_borderwrite_conflict(tree, txn, &query);
    if (r != 0) 
        goto cleanup;
    /* Now need to merge, copy the memory and insert. */
    toku_range  to_insert;
    init_insert(&to_insert, &left, &right, txn);
    /* Consolidate the new range and all the overlapping ranges */
    r = consolidate_reads(tree, FALSE, &to_insert, txn);
    if (r != 0) 
        goto cleanup;
    
    r = 0;
cleanup:
    if (tree)
        lt_postprocess(tree);
    return r;
}

/*
    Tests whether a range from BorderWrite is trivially escalatable.
    i.e. No read locks from other transactions overlap the range.
*/
static inline int border_escalation_trivial(toku_lock_tree* tree, 
                                            toku_range* border_range, 
                                            BOOL* trivial) {
    assert(tree && border_range && trivial);
    int r = ENOSYS;

    toku_interval query = border_range->ends;

    r = lt_write_range_conflicts_reads(tree, border_range->data, &query);
    if (r == DB_LOCK_NOTGRANTED || r == DB_LOCK_DEADLOCK) { 
        *trivial = FALSE; 
    } else if (r != 0) { 
        goto cleanup; 
    } else { 
        *trivial = TRUE; 
    }
    r = 0;
cleanup:
    return r;
}

/*  */
static inline int escalate_writes_from_border_range(toku_lock_tree* tree, 
                                                    toku_range* border_range) {
    int r = ENOSYS;
    if (!tree || !border_range) { 
        r = EINVAL; goto cleanup; 
    }
    
    TXNID txn = border_range->data;
    toku_range_tree* self_write = toku_lt_ifexist_selfwrite(tree, txn);
    assert(self_write);
    toku_interval query = border_range->ends;
    uint32_t numfound = 0;

    /*
     * Delete all overlapping ranges
     */
    r = toku_rt_find(self_write, &query, 0, &tree->buf, &tree->buflen, &numfound);
    if (r != 0)
        goto cleanup;
    /* Need at least two entries for this to actually help. */
    if (numfound < 2)
        goto cleanup;
    
    /*
     * Insert border_range into self_write table
     */
    for (uint32_t i = 0; i < numfound; i++) {
        r = toku_rt_delete(self_write, &tree->buf[i]);
        if (r != 0) { 
            r = lt_panic(tree, r); goto cleanup; 
        }
        /*
         * Clean up memory that is not referenced by border_range.
         */
        if (tree->buf[i].ends.left != tree->buf[i].ends.right &&
            lt_p_independent(tree->buf[i].ends.left, &border_range->ends)) {
            /* Do not double free if left and right are same point. */
            p_free(tree, tree->buf[i].ends.left);
        }
        if (lt_p_independent(tree->buf[i].ends.right, &border_range->ends)) {
            p_free(tree, tree->buf[i].ends.right);
        }
    }
    
    //Insert escalated range.
    r = toku_rt_insert(self_write, border_range);
    if (r != 0) { 
        r = lt_panic(tree, r); goto cleanup; 
    }
    ltm_lock_incr(tree->mgr, numfound);

    r = 0;
cleanup:
    return r;
}

static int lt_escalate_read_locks_in_interval(toku_lock_tree* tree,
                                              toku_interval* query,
                                              TXNID txn) {
    int r = ENOSYS;
    toku_range to_insert;

    init_insert(&to_insert, query->left, query->right, txn);
    r = consolidate_reads(tree, TRUE, &to_insert, txn);
    return r;
}

typedef struct {
    toku_lock_tree*  lt;
    toku_range_tree* border;
    toku_interval*   escalate_interval;
    TXNID            txn;
} escalate_info;

static int escalate_read_locks_helper(toku_range* border_range, void* extra) {
    escalate_info* info = extra;
    int r               = ENOSYS;

    if (!lt_txn_cmp(border_range->data, info->txn)) { 
        r = 0; goto cleanup; 
    }
    info->escalate_interval->right = border_range->ends.left;
    r = lt_escalate_read_locks_in_interval(info->lt,
                                       info->escalate_interval, info->txn);
    if (r != 0) 
        goto cleanup;
    info->escalate_interval->left = border_range->ends.right;
    r = 0;
cleanup:
    return r;
}

 //TODO: Whenever comparing TXNIDs use the comparison function INSTEAD of just '!= or =='
static int lt_escalate_read_locks(toku_lock_tree* tree, TXNID txn) {
    int r = ENOSYS;
    assert(tree);
    assert(tree->lock_escalation_allowed);

    toku_point neg_infinite;
    toku_point infinite;
    toku_interval query;
    lt_init_full_query(tree, &query, &neg_infinite, &infinite);

    toku_range_tree* border = tree->borderwrite;
    assert(border);
    escalate_info info;
    info.lt     = tree;
    info.border = border;
    info.escalate_interval = &query;
    info.txn    = txn;
    if ((r = toku_rt_iterate(border, escalate_read_locks_helper, &info))) 
        goto cleanup;
    /* Special case for zero entries in border?  Just do the 'after'? */
    query.right = &infinite;
    r = lt_escalate_read_locks_in_interval(tree, &query, txn);
cleanup:
    return r;
}

static int escalate_write_locks_helper(toku_range* border_range, void* extra) {
    toku_lock_tree* tree = extra;
    int r                = ENOSYS;
    BOOL trivial;
    if ((r = border_escalation_trivial(tree, border_range, &trivial))) 
        goto cleanup;
    if (!trivial) { 
        r = 0; goto cleanup; 
    }
    /*
     * At this point, we determine that escalation is simple,
     * Attempt escalation
     */
    r = escalate_writes_from_border_range(tree, border_range);
    if (r != 0) { 
        r = lt_panic(tree, r); goto cleanup; 
    }
    r = 0;
cleanup:
    return r;
}

/*
 * For each range in BorderWrite:
 *     Check to see if range conflicts any read lock held by other transactions
 *     Replaces all writes that overlap with range
 *     Deletes all reads dominated by range
 */
static int lt_escalate_write_locks(toku_lock_tree* tree) {
    int r = ENOSYS;
    assert(tree);
    assert(tree->borderwrite);

    if ((r = toku_rt_iterate(tree->borderwrite, escalate_write_locks_helper, tree))) 
        goto cleanup;
    r = 0;
cleanup:
    return r;
}

// run escalation algorithm on a given locktree
static int lt_do_escalation(toku_lock_tree* lt) {
    assert(lt);
    int r = ENOSYS;
    DB* db;  // extract db from lt
    OMTVALUE dbv;

    assert(toku_omt_size(lt->dbs) > 0);  // there is at least one db associated with this locktree
    r = toku_omt_fetch(lt->dbs, 0, &dbv);
    assert_zero(r);
    db = dbv;
    lt_set_comparison_functions(lt, db);
    
    if (!lt->lock_escalation_allowed) { 
        r = 0; goto cleanup; 
    }
    r = lt_escalate_write_locks(lt);
    if (r != 0)
        goto cleanup;

    rt_forest* forest;    
    toku_rth_start_scan(lt->rth);
    while ((forest = toku_rth_next(lt->rth)) != NULL) {
        if (forest->self_read) {
            r = lt_escalate_read_locks(lt, forest->hash_key);
            if (r != 0) 
                goto cleanup;
        }
    }
    r = 0;

cleanup:
    lt_clear_comparison_functions(lt);
    return r;
}

// run escalation algorithm on all locktrees
static int ltm_do_escalation(toku_ltm* mgr) {
    assert(mgr);
    int r = ENOSYS;
    toku_lock_tree* lt = NULL;

    toku_lth_start_scan(mgr->lth);  // initialize iterator in mgr
    while ((lt = toku_lth_next(mgr->lth)) != NULL) {
        r = lt_do_escalation(lt);
        if (r != 0) 
            goto cleanup;
    }

    r = 0;
cleanup:
    return r;
}

int toku_lt_acquire_range_read_lock(toku_lock_tree* tree, DB* db, TXNID txn,
				    const DBT* key_left,
				    const DBT* key_right) {
    int r = ENOSYS;

    r = lt_try_acquire_range_read_lock(tree, db, txn, 
                                       key_left, key_right);
    if (r==TOKUDB_OUT_OF_LOCKS) {
        r = ltm_do_escalation(tree->mgr);
        if (r == 0) { 
	    r = lt_try_acquire_range_read_lock(tree, db, txn, 
					       key_left, key_right);
	    if (r == 0) {
		tree->mgr->status.lock_escalation_successes++;
	    }
	    else if (r==TOKUDB_OUT_OF_LOCKS) {
		tree->mgr->status.lock_escalation_failures++;	    
	    }
	}
    }

    if (tree) {
	LTM_STATUS s = &(tree->mgr->status);
	if (r == 0) {
	    s->read_lock++;
	}
	else {
	    s->read_lock_fail++;
	    if (r == TOKUDB_OUT_OF_LOCKS) 
		s->out_of_read_locks++;
	}
    }
    return r;
}


static int lt_try_acquire_range_write_lock(toku_lock_tree* tree,
                                           DB* db, TXNID txn,
                                           const DBT* key_left,
                                           const DBT* key_right) {
    int r;
    toku_point left;
    toku_point right;
    toku_interval query;

    r = lt_preprocess(tree, db,   txn, 
                      key_left, key_right,
                      &left,    &right,
                      &query);
    if (r != 0)
        goto cleanup;

    // if query is dominated by selfwrite('txn') then return success
    BOOL dominated;
    r = lt_rt_dominates(tree, &query, toku_lt_ifexist_selfwrite(tree, txn), &dominated);
    if (r || dominated)
        goto cleanup;
    // if query meets any other read set then fail
    r = lt_write_range_conflicts_reads(tree, txn, &query);
    if (r != 0)
        goto cleanup;
    // query meets any other write set then fail
    r = lt_check_borderwrite_conflict(tree, txn, &query);
    if (r != 0)
        goto cleanup;
    // insert and consolidate into the local write set
    toku_range to_insert;
    init_insert(&to_insert, &left, &right, txn);
    r = consolidate_writes(tree, &to_insert, txn);
    if (r != 0)
        goto cleanup;
cleanup:
    if (tree)
        lt_postprocess(tree);
    return r;
}

int toku_lt_acquire_range_write_lock(toku_lock_tree* tree, DB* db, TXNID txn,
				     const DBT* key_left,
				     const DBT* key_right) {
    int r = ENOSYS;

    r = lt_try_acquire_range_write_lock(tree,   db, txn,
					key_left, key_right);
    if (r == TOKUDB_OUT_OF_LOCKS) {
        r = ltm_do_escalation(tree->mgr);
        if (r == 0) { 
	    r = lt_try_acquire_range_write_lock(tree,   db, txn,
						key_left, key_right);
	    if (r == 0) {
		tree->mgr->status.lock_escalation_successes++;
	    }
	    else if (r==TOKUDB_OUT_OF_LOCKS) {
		tree->mgr->status.lock_escalation_failures++;	    
	    }
	}
    }

    if (tree) {
	LTM_STATUS s = &(tree->mgr->status);
	if (r == 0) {
	    s->write_lock++;
	}
	else {
	    s->write_lock_fail++;
	    if (r == TOKUDB_OUT_OF_LOCKS) 
		s->out_of_write_locks++;
	}
    }
    return r;
}

// toku_lt_acquire_write_lock() used only by test programs
int toku_lt_acquire_write_lock(toku_lock_tree* tree, DB* db, TXNID txn, const DBT* key) {
    return toku_lt_acquire_range_write_lock(tree, db, txn, key, key);
}   

static inline int sweep_border(toku_lock_tree* tree, toku_range* range) {
    assert(tree && range);
    toku_range_tree* borderwrite = tree->borderwrite;
    assert(borderwrite);

    /* Find overlapping range in borderwrite */
    int r;
    const uint32_t query_size = 1;
    toku_range      buffer[query_size];
    uint32_t       buflen     = query_size;
    toku_range*     buf        = &buffer[0];
    uint32_t       numfound;

    toku_interval query = range->ends;
    r = toku_rt_find(borderwrite, &query, query_size, &buf, &buflen, &numfound);
    if (r != 0) 
        return r;
    assert(numfound <= query_size);
    
    /*  If none exists or data is not ours (we have already deleted the real
        overlapping range), continue to the end of the loop (i.e., return) */
    if (!numfound || lt_txn_cmp(buf[0].data, range->data)) 
        return 0;
    assert(numfound == 1);

    /* Delete s from borderwrite */
    r = toku_rt_delete(borderwrite, &buf[0]);
    if (r != 0)
        return r;

    /* Find pred(s.ends.left), and succ(s.ends.right) */
    toku_range pred;
    toku_range succ;
    BOOL found_p = FALSE;
    BOOL found_s = FALSE;

    r = lt_get_border_in_borderwrite(tree, &pred, &succ, &found_p, &found_s, &buf[0]);
    if (r != 0) 
        return r;
    if (found_p && found_s && !lt_txn_cmp(pred.data, succ.data) &&
        !lt_txn_cmp(pred.data, buf[0].data)) { 
        return lt_panic(tree, TOKU_LT_INCONSISTENT); }

    /* If both found and pred.data=succ.data, merge pred and succ (expand?)
       free_points */
    if (!found_p || !found_s || lt_txn_cmp(pred.data, succ.data)) 
        return 0;

    r = toku_rt_delete(borderwrite, &pred);
    if (r != 0) 
        return r;
    r = toku_rt_delete(borderwrite, &succ);
    if (r != 0) 
        return r;

    pred.ends.right = succ.ends.right;
    r = toku_rt_insert(borderwrite, &pred);
    if (r != 0) 
        return r;

    return 0;
}

/*
  Algorithm:
    For each range r in selfwrite
      Find overlapping range s in borderwrite 
      If none exists or data is not ours (we have already deleted the real
        overlapping range), continue
      Delete s from borderwrite
      Find pred(s.ends.left), and succ(s.ends.right)
      If both found and pred.data=succ.data, merge pred and succ (expand?)
    free_points
*/
static inline int lt_border_delete(toku_lock_tree* tree, toku_range_tree* rt) {
    int r;
    assert(tree);
    if (!rt) 
        return 0;

    /* Find the ranges in rt */
    toku_interval query;
    toku_point left;
    toku_point right;
    lt_init_full_query(tree, &query, &left, &right);

    uint32_t numfound;
    r = toku_rt_find(rt, &query, 0, &tree->buf, &tree->buflen, &numfound);
    if (r != 0) 
        return r;
    assert(numfound <= tree->buflen);
    
    uint32_t i;
    for (i = 0; i < numfound; i++) {
        r = sweep_border(tree, &tree->buf[i]);
        if (r != 0) 
            return r;
    }

    return 0;
}

static inline int lt_defer_unlocking_txn(toku_lock_tree* tree, TXNID txnid) {
    int r = ENOSYS;

    rt_forest* forest = toku_rth_find(tree->txns_to_unlock, txnid);
    /* Should not be unlocking a transaction twice. */
    assert(!forest);
    r = toku_rth_insert(tree->txns_to_unlock, txnid);
    if (r != 0) 
        goto cleanup;
    if (toku_rth_find(tree->txns_still_locked, txnid) != NULL) {
        toku_rth_delete(tree->txns_still_locked, txnid);
    }
    r = 0;
cleanup:
    return r;
}

static inline int lt_unlock_txn(toku_lock_tree* tree, TXNID txn) {
    if (!tree) 
        return EINVAL;
    int r;
    toku_range_tree *selfwrite = toku_lt_ifexist_selfwrite(tree, txn);
    toku_range_tree *selfread  = toku_lt_ifexist_selfread (tree, txn);

    uint32_t ranges = 0;

    if (selfread) {
        uint32_t size;
        r = toku_rt_get_size(selfread, &size);
        assert_zero(r);
        ranges += size;
        r = lt_free_contents(tree, selfread, TRUE);
        if (r != 0) 
            return lt_panic(tree, r);
    }

    if (selfwrite) {
        uint32_t size;
        r = toku_rt_get_size(selfwrite, &size);
        assert_zero(r);
        ranges += size;
        r = lt_border_delete(tree, selfwrite);
        if (r != 0) 
            return lt_panic(tree, r);
        r = lt_free_contents(tree, selfwrite, TRUE);
        if (r != 0) 
            return lt_panic(tree, r);
    }

    if (selfread || selfwrite) 
        toku_rth_delete(tree->rth, txn);
    
    ltm_lock_decr(tree->mgr, ranges);

    return 0;
}

static inline int lt_unlock_deferred_txns(toku_lock_tree* tree) {
    int r = ENOSYS;
    toku_rth_start_scan(tree->txns_to_unlock);
    rt_forest* forest = NULL;
    while ((forest = toku_rth_next(tree->txns_to_unlock)) != NULL) {
        /* This can only fail with a panic so it is fine to quit immediately. */
        r = lt_unlock_txn(tree, forest->hash_key);
        if (r != 0) 
            goto cleanup;
    }
    toku_rth_clear(tree->txns_to_unlock);
    r = 0;
cleanup:
    return r;
}

static inline void lt_clear(toku_lock_tree* tree) {
    int r;
    assert(tree);
    toku_rt_clear(tree->borderwrite);

    toku_rth_start_scan(tree->rth);
    rt_forest* forest;
    uint32_t ranges = 0;
    while ((forest = toku_rth_next(tree->rth)) != NULL) {
        uint32_t size;
        if (forest->self_read) {
            r = toku_rt_get_size(forest->self_read, &size);
            assert_zero(r);
            ranges += size;
            r = lt_free_contents(tree, forest->self_read, TRUE);
            assert_zero(r);
        }
        if (forest->self_write) {
            r = toku_rt_get_size(forest->self_write, &size);
            assert_zero(r);
            ranges += size;
            r = lt_free_contents(tree, forest->self_write, TRUE);
            assert_zero(r);
        }
        
    }
    toku_rth_clear(tree->rth);
    toku_rth_clear(tree->txns_to_unlock);
    /* tree->txns_still_locked is already empty, so we do not clear it. */
    ltm_lock_decr(tree->mgr, ranges);
}

int toku_lt_unlock(toku_lock_tree* tree, TXNID txn) {
    int r = ENOSYS;
    if (!tree) { 
        r = EINVAL; goto cleanup;
    }
#if DEBUG
    if (toku_lt_debug) 
        printf("%s:%u %lu\n", __FUNCTION__, __LINE__, txn);
#endif
    r = lt_defer_unlocking_txn(tree, txn);
    if (r != 0) 
        goto cleanup;
    if (toku_rth_is_empty(tree->txns_still_locked))
        lt_clear(tree);
    toku_lt_retry_lock_requests_locked(tree);
    r = 0;
cleanup:
    return r;
}

void toku_lt_add_ref(toku_lock_tree* tree) {
    assert(tree);
    assert(tree->ref_count > 0);
    tree->ref_count++;
}

static void toku_ltm_stop_managing_lt(toku_ltm* mgr, toku_lock_tree* tree) {
    toku_ltm_remove_lt(mgr, tree);
    toku_lt_map* map = toku_idlth_find(mgr->idlth, tree->dict_id);
    if (map && map->tree == tree) {
        toku_idlth_delete(mgr->idlth, tree->dict_id);
    }
}

int toku_lt_remove_ref(toku_lock_tree* tree) {
    int r = ENOSYS;
    assert(tree);
    assert(tree->ref_count > 0);
    tree->ref_count--;
    if (tree->ref_count > 0) { 
        r = 0; goto cleanup; 
    }
    assert(tree->dict_id.dictid != DICTIONARY_ID_NONE.dictid);
    toku_ltm_stop_managing_lt(tree->mgr, tree);
    r = toku_lt_close(tree);
    if (r != 0) 
        goto cleanup;

    r = 0;    
cleanup:
    return r;
}

//Heaviside function to find a DB by DB (used to find the index) (just sort by pointer addr)
static int find_db (OMTVALUE v, void *dbv) {
    DB *db = v;
    DB *dbfind = dbv;
    if (db < dbfind) 
        return -1;
    if (db > dbfind) 
        return +1;
    return 0;
}

static void lt_add_db(toku_lock_tree* tree, DB *db) {
    if (db != NULL) {
        int r;
        OMTVALUE get_dbv = NULL;
        uint32_t index;
        r = toku_omt_find_zero(tree->dbs, find_db, db, &get_dbv, &index);
        assert(r == DB_NOTFOUND);
        r = toku_omt_insert_at(tree->dbs, db, index);
        assert_zero(r);
    }
}

static void lt_remove_db(toku_lock_tree* tree, DB *db) {
    if (db != NULL) {
        int r;
        OMTVALUE get_dbv = NULL;
        uint32_t index;
        r = toku_omt_find_zero(tree->dbs, find_db, db, &get_dbv, &index);
        assert_zero(r);
        assert(db == get_dbv);
        r = toku_omt_delete_at(tree->dbs, index);
        assert_zero(r);
    }
}

void toku_lt_remove_db_ref(toku_lock_tree* tree, DB *db) {
    int r;
    lt_remove_db(tree, db);
    r = toku_lt_remove_ref(tree);
    assert_zero(r);
}

static void 
lt_verify(toku_lock_tree *lt) {    
    // verify the borderwrite tree
    toku_rt_verify(lt->borderwrite);

    // verify all of the selfread and selfwrite trees
    toku_rth_start_scan(lt->rth);
    rt_forest *forest;
    while ((forest = toku_rth_next(lt->rth)) != NULL) {
        if (forest->self_read)
            toku_rt_verify(forest->self_read);
        if (forest->self_write)
            toku_rt_verify(forest->self_write);
    }
}

void 
toku_lt_verify(toku_lock_tree *lt, DB *db) {
    lt_set_comparison_functions(lt, db);
    lt_verify(lt);
    lt_clear_comparison_functions(lt);
}

static void
toku_lock_request_init_wait(toku_lock_request *lock_request) {
    if (!lock_request->wait_initialized) {
        int r = toku_pthread_cond_init(&lock_request->wait, NULL); assert_zero(r);
        lock_request->wait_initialized = true;
    }
}

static void
toku_lock_request_destroy_wait(toku_lock_request *lock_request) {
    if (lock_request->wait_initialized) {
        int r = toku_pthread_cond_destroy(&lock_request->wait); assert_zero(r);
        lock_request->wait_initialized = false;
    }
}

void 
toku_lock_request_default_init(toku_lock_request *lock_request) {
    lock_request->db = NULL;
    lock_request->txnid = 0;
    lock_request->key_left = lock_request->key_right = NULL;
    lock_request->key_left_copy = (DBT) { .data = NULL, .size = 0, .flags = DB_DBT_REALLOC };
    lock_request->key_right_copy = (DBT) { .data = NULL, .size = 0, .flags = DB_DBT_REALLOC };
    lock_request->state = LOCK_REQUEST_INIT;
    lock_request->complete_r = 0;
    lock_request->type = LOCK_REQUEST_UNKNOWN;
    lock_request->tree = NULL;
    lock_request->wait_initialized = false;
}

void 
toku_lock_request_set(toku_lock_request *lock_request, DB *db, TXNID txnid, const DBT *key_left, const DBT *key_right, toku_lock_type lock_type) {
    assert(lock_request->state != LOCK_REQUEST_PENDING);
    lock_request->db = db;
    lock_request->txnid = txnid;
    lock_request->key_left = key_left;
    lock_request->key_right = key_right;
    lock_request->type = lock_type;
    lock_request->state = LOCK_REQUEST_INIT;
}

void 
toku_lock_request_init(toku_lock_request *lock_request, DB *db, TXNID txnid, const DBT *key_left, const DBT *key_right, toku_lock_type lock_type) {
    toku_lock_request_default_init(lock_request);
    toku_lock_request_set(lock_request, db, txnid, key_left, key_right, lock_type);
}

void 
toku_lock_request_destroy(toku_lock_request *lock_request) {
    if (lock_request->state == LOCK_REQUEST_PENDING)
        toku_lock_request_tree_delete(lock_request->tree, lock_request); 
    toku_lock_request_destroy_wait(lock_request);
    toku_free(lock_request->key_left_copy.data);
    toku_free(lock_request->key_right_copy.data);
}

static void
toku_lock_request_complete(toku_lock_request *lock_request, int complete_r) {
    lock_request->state = LOCK_REQUEST_COMPLETE;
    lock_request->complete_r = complete_r;
}

static const struct timeval max_timeval = { ~0, 0 };

int
toku_lock_request_wait(toku_lock_request *lock_request, toku_lock_tree *tree, struct timeval *wait_time) {
#if DEBUG
    if (toku_lt_debug)
        printf("%s:%u %lu\n", __FUNCTION__, __LINE__, lock_request->txnid);
#endif
    int r = 0;
    if (wait_time && wait_time->tv_sec != max_timeval.tv_sec) {
        struct timeval now;
        r = gettimeofday(&now, NULL); assert_zero(r);
        long int sec = now.tv_sec + wait_time->tv_sec;
        long int usec = now.tv_usec + wait_time->tv_usec;
        long int d_sec = usec / 1000000;
        long int d_usec = usec % 1000000;
        struct timespec ts = { sec + d_sec, d_usec * 1000 };
        while (lock_request->state == LOCK_REQUEST_PENDING) {
            toku_lock_request_init_wait(lock_request);
            r = pthread_cond_timedwait(&lock_request->wait, toku_ltm_get_mutex(tree->mgr), &ts);
            assert(r == 0 || r == ETIMEDOUT);
            if (r == ETIMEDOUT && lock_request->state == LOCK_REQUEST_PENDING) {
                toku_lock_request_tree_delete(tree, lock_request);
                toku_lock_request_complete(lock_request, DB_LOCK_NOTGRANTED);
            }
        }
    } else {
        while (lock_request->state == LOCK_REQUEST_PENDING) {
            toku_lock_request_init_wait(lock_request);
            r = toku_pthread_cond_wait(&lock_request->wait, toku_ltm_get_mutex(tree->mgr)); assert_zero(r);
        }
    }
    assert(lock_request->state == LOCK_REQUEST_COMPLETE);
    return lock_request->complete_r;
}

int 
toku_lock_request_wait_with_default_timeout(toku_lock_request *lock_request, toku_lock_tree *tree) {
    return toku_lock_request_wait(lock_request, tree, &tree->mgr->lock_wait_time);
}

void 
toku_lock_request_wakeup(toku_lock_request *lock_request, toku_lock_tree *tree UU()) {
    if (lock_request->wait_initialized) {
        int r = toku_pthread_cond_broadcast(&lock_request->wait); assert_zero(r);
    }
}

void 
toku_lock_request_tree_init(toku_lock_tree *tree) {
    int r = toku_omt_create(&tree->lock_requests); assert_zero(r);
}

void 
toku_lock_request_tree_destroy(toku_lock_tree *tree) {
    assert(toku_omt_size(tree->lock_requests) == 0);
    toku_omt_destroy(&tree->lock_requests);
}

static int 
compare_lock_request(OMTVALUE a, void *b) {
    toku_lock_request *a_lock_request = (toku_lock_request *) a;
    TXNID b_id = * (TXNID *) b;
    if (a_lock_request->txnid < b_id) 
        return -1;
    if (a_lock_request->txnid > b_id)
        return +1;
    return 0;
}

void 
toku_lock_request_tree_insert(toku_lock_tree *tree, toku_lock_request *lock_request) {
    lock_request->tree = tree;
    int r;
    OMTVALUE v;
    u_int32_t idx;
    r = toku_omt_find_zero(tree->lock_requests, compare_lock_request, &lock_request->txnid, &v, &idx); assert(r == DB_NOTFOUND);
    r = toku_omt_insert_at(tree->lock_requests, lock_request, idx); assert_zero(r);
}

void 
toku_lock_request_tree_delete(toku_lock_tree *tree, toku_lock_request *lock_request) {
    int r;
    OMTVALUE v;
    u_int32_t idx;
    r = toku_omt_find_zero(tree->lock_requests, compare_lock_request, &lock_request->txnid, &v, &idx);
    if (r == 0) {
        r = toku_omt_delete_at(tree->lock_requests, idx); assert_zero(r);
    }
}

toku_lock_request *
toku_lock_request_tree_find(toku_lock_tree *tree, TXNID id) {
    int r;
    OMTVALUE v;
    u_int32_t idx;
    r = toku_omt_find_zero(tree->lock_requests, compare_lock_request, &id, &v, &idx);
    toku_lock_request *lock_request = NULL;
    if (r == 0) 
        lock_request = (toku_lock_request *) v;
    return lock_request;
}

static void
copy_dbt(DBT *dest, const DBT *src) {
    dest->size = src->size;
    if (dest->size > 0) {
        dest->data = toku_xrealloc(dest->data, dest->size);
        memcpy(dest->data, src->data, dest->size);
    }
}

#if DEBUG
#include <ctype.h>
static void print_key(const char *sp, const DBT *k) {
    printf("%s", sp);
    if (k == toku_lt_neg_infinity)
        printf("-inf");
    else if (k == toku_lt_infinity)
        printf("inf");
    else {
        char *data = (char *) k->data;
        for (unsigned i = 0; i < k->size; i++)
            printf("%2.2x", data[i]);
        printf(" ");
        for (unsigned i = 0; i < k->size; i++) {
            int c = data[i];
            printf("%c", isprint(c) ? c : '.');
        }
    }
    printf("\n");
}
#endif

int 
toku_lock_request_start_locked(toku_lock_request *lock_request, toku_lock_tree *tree, bool copy_keys_if_not_granted) { 
    int r;
    assert(lock_request->state == LOCK_REQUEST_INIT);
    if (lock_request->type == LOCK_REQUEST_READ) {
        r = toku_lt_acquire_range_read_lock(tree, lock_request->db, lock_request->txnid, lock_request->key_left, lock_request->key_right);
    } else if (lock_request->type == LOCK_REQUEST_WRITE) {
        r = toku_lt_acquire_range_write_lock(tree, lock_request->db, lock_request->txnid, lock_request->key_left, lock_request->key_right);
    } else
        assert(0);
#if DEBUG
    if (toku_lt_debug) {
        printf("%s:%u %lu db=%p %s %u\n", __FUNCTION__, __LINE__, lock_request->txnid, lock_request->db, lock_request->type == LOCK_REQUEST_READ ? "r" : "w", lock_request->state);
        print_key("left=", lock_request->key_left);
        print_key("right=", lock_request->key_right);
    }
#endif
    if (r == DB_LOCK_NOTGRANTED) {
        lock_request->state = LOCK_REQUEST_PENDING;
        if (copy_keys_if_not_granted) {
            copy_dbt(&lock_request->key_left_copy, lock_request->key_left);
            if (!lt_is_infinite(lock_request->key_left))
                lock_request->key_left = &lock_request->key_left_copy;
            copy_dbt(&lock_request->key_right_copy, lock_request->key_right);
            if (!lt_is_infinite(lock_request->key_right))
                lock_request->key_right = &lock_request->key_right_copy;
        }
        toku_lock_request_tree_insert(tree, lock_request);

        // check for deadlock
        toku_lt_check_deadlock(tree, lock_request);
        if (lock_request->state == LOCK_REQUEST_COMPLETE)
            r = lock_request->complete_r;
    } else 
        toku_lock_request_complete(lock_request, r);

    return r;
}

int 
toku_lock_request_start(toku_lock_request *lock_request, toku_lock_tree *tree, bool copy_keys_if_not_granted) {
    toku_ltm_lock_mutex(tree->mgr);
    int r = toku_lock_request_start_locked(lock_request, tree, copy_keys_if_not_granted);
    toku_ltm_unlock_mutex(tree->mgr);
    return r;
}

int 
toku_lt_acquire_lock_request_with_timeout_locked(toku_lock_tree *tree, toku_lock_request *lock_request, struct timeval *wait_time) {
    int r = toku_lock_request_start_locked(lock_request, tree, false);
    if (r == DB_LOCK_NOTGRANTED)
        r = toku_lock_request_wait(lock_request, tree, wait_time);
    return r;
}

int 
toku_lt_acquire_lock_request_with_timeout(toku_lock_tree *tree, toku_lock_request *lock_request, struct timeval *wait_time) {
    toku_ltm_lock_mutex(tree->mgr);
    int r = toku_lt_acquire_lock_request_with_timeout_locked(tree, lock_request, wait_time);
    toku_ltm_unlock_mutex(tree->mgr);
    return r;
}

int
toku_lt_acquire_lock_request_with_default_timeout_locked(toku_lock_tree *tree, toku_lock_request *lock_request) {
    return toku_lt_acquire_lock_request_with_timeout_locked(tree, lock_request, &tree->mgr->lock_wait_time);
}

int
toku_lt_acquire_lock_request_with_default_timeout(toku_lock_tree *tree, toku_lock_request *lock_request) {
    toku_ltm_lock_mutex(tree->mgr);
    int r = toku_lt_acquire_lock_request_with_timeout_locked(tree, lock_request, &tree->mgr->lock_wait_time);
    toku_ltm_unlock_mutex(tree->mgr);
    return r;
}

void 
toku_lt_retry_lock_requests_locked(toku_lock_tree *tree) {
    int r;
    for (uint32_t i = 0; i < toku_omt_size(tree->lock_requests); ) {
        OMTVALUE v;
        r = toku_omt_fetch(tree->lock_requests, i, &v); assert_zero(r);
        toku_lock_request *lock_request = (toku_lock_request *) v;
        assert(lock_request->state == LOCK_REQUEST_PENDING);
        lock_request->state = LOCK_REQUEST_INIT;
        toku_omt_delete_at(tree->lock_requests, i);
        r = toku_lock_request_start_locked(lock_request, tree, false);
        if (lock_request->state == LOCK_REQUEST_COMPLETE) {
            toku_lock_request_wakeup(lock_request, tree);
        } else {
            assert(lock_request->state == LOCK_REQUEST_PENDING);
            i++;
        }
    }
}

#include <stdbool.h>
#include "wfg.h"

// build the WFG for a given lock request
//
// for each transaction B that blocks A's lock request
//     if B is blocked then add (A,T) to the WFG and if B is new, fill in the WFG from B
static void 
build_wfg_for_a_lock_request(toku_lock_tree *tree, struct wfg *wfg, toku_lock_request *a_lock_request) {
    txnid_set conflicts; txnid_set_init(&conflicts);
    int r = toku_lt_get_lock_request_conflicts(tree, a_lock_request, &conflicts); assert_zero(r);
    size_t n_conflicts = txnid_set_size(&conflicts);
    for (size_t i = 0; i < n_conflicts; i++) {
        TXNID b = txnid_set_get(&conflicts, i);
        toku_lock_request *b_lock_request = toku_lock_request_tree_find(tree, b);
        if (b_lock_request) {
            bool b_exists = wfg_node_exists(wfg, b);
            wfg_add_edge(wfg, a_lock_request->txnid, b);
            if (!b_exists)
                build_wfg_for_a_lock_request(tree, wfg, b_lock_request);
        }
    }
    txnid_set_destroy(&conflicts);
}

// check if a given lock request could deadlock with any granted locks.
void 
toku_lt_check_deadlock(toku_lock_tree *tree, toku_lock_request *a_lock_request) {
    // init the wfg
    struct wfg wfg_static; 
    struct wfg *wfg = &wfg_static; wfg_init(wfg);

    // build the wfg rooted with a lock request
    build_wfg_for_a_lock_request(tree, wfg, a_lock_request);

    // find cycles in the wfg rooted with A
    // if cycles exists then
    //     pick all necessary transactions T from the cycle needed to break the cycle
    //     hand T the deadlock error
    //         remove T's lock request
    //         set the lock request state to deadlocked
    //         wakeup T's lock request
    if (wfg_exist_cycle_from_txnid(wfg, a_lock_request->txnid)) {
        assert(a_lock_request->state == LOCK_REQUEST_PENDING);
        toku_lock_request_complete(a_lock_request, DB_LOCK_DEADLOCK);
        toku_lock_request_tree_delete(tree, a_lock_request);
        toku_lock_request_wakeup(a_lock_request, tree);
    }

    // destroy the wfg
    wfg_destroy(wfg);
}

static void 
add_conflicts(txnid_set *conflicts, toku_range *ranges, uint32_t nranges, TXNID id) {
    for (uint32_t i = 0; i < nranges; i++) 
        if (ranges[i].data != id)
            txnid_set_add(conflicts, ranges[i].data);
}

static void 
find_read_conflicts(toku_lock_tree *tree, toku_interval *query, TXNID id, txnid_set *conflicts, toku_range **range_ptr, uint32_t *n_expected_ranges_ptr) {
    uint32_t numfound;
    toku_rth_start_scan(tree->rth);
    rt_forest *forest;
    while ((forest = toku_rth_next(tree->rth)) != NULL) {
        if (forest->self_read != NULL && lt_txn_cmp(forest->hash_key, id)) {
            numfound = 0;
            int r = toku_rt_find(forest->self_read, query, 0, range_ptr, n_expected_ranges_ptr, &numfound);
            if (r == 0)
                add_conflicts(conflicts, *range_ptr, numfound, id);
        }
    }
}

// find transactions that conflict with a given lock request
// for read lock requests
//     conflicts = all transactions in the BWT that conflict with the lock request
// for write lock requests
//     conflicts = all transactions in the GRT that conflict with the lock request UNION
//                 all transactions in the BWT that conflict with the lock request
int 
toku_lt_get_lock_request_conflicts(toku_lock_tree *tree, toku_lock_request *lock_request, txnid_set *conflicts) {
    int r;

    // build a query from the lock request
    toku_point left; init_point(&left,  tree, lock_request->key_left);
    toku_point right; init_point(&right, tree, lock_request->key_right);
    toku_interval query; init_query(&query, &left, &right);
    lt_set_comparison_functions(tree, lock_request->db);

    uint32_t n_expected_ranges = 0;
    toku_range *ranges = NULL;

    if (lock_request->type == LOCK_REQUEST_WRITE) {
        // check conflicts with read locks
        find_read_conflicts(tree, &query, lock_request->txnid, conflicts, &ranges, &n_expected_ranges);
    }
 
    // check conflicts with write locks
    uint32_t numfound = 0;
    r = toku_rt_find(tree->borderwrite, &query, 0, &ranges, &n_expected_ranges, &numfound);
    if (r == 0) {
        for (uint32_t i = 0; i < numfound; i++) 
            if (ranges[i].data != lock_request->txnid)
                txnid_set_add(conflicts, ranges[i].data);
    }

    if (ranges) 
        tree->free(ranges);

    lt_clear_comparison_functions(tree);
    return r;
}

void 
toku_ltm_set_lock_wait_time(toku_ltm *mgr, uint64_t lock_wait_time_usec) {
    if (lock_wait_time_usec == UINT64_MAX)
        mgr->lock_wait_time = max_timeval;
    else
        mgr->lock_wait_time = (struct timeval) { lock_wait_time_usec / 1000000, lock_wait_time_usec % 1000000 };
}

void 
toku_ltm_get_lock_wait_time(toku_ltm *mgr, uint64_t *lock_wait_time_usec) {
    if (mgr->lock_wait_time.tv_sec == max_timeval.tv_sec && mgr->lock_wait_time.tv_usec == max_timeval.tv_usec)
        *lock_wait_time_usec = UINT64_MAX;
    else
        *lock_wait_time_usec = mgr->lock_wait_time.tv_sec * 1000000 + mgr->lock_wait_time.tv_usec;
}
