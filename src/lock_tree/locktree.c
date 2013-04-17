/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/**
   \file  locktree.c
   \brief Lock trees: implementation
*/
  
#include <toku_portability.h>
#include "memory.h"
#include <locktree.h>
#include <locktree-internal.h>
#include <ydb-internal.h>
#include <ft/ft-internal.h>
#include <toku_stdint.h>
#include <valgrind/drd.h>

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

#if !defined(TOKU_LT_DEBUG)
#define TOKU_LT_DEBUG 0
#endif
#if TOKU_LT_DEBUG
static int toku_lt_debug = 0;
#endif

///////////////////////////////////////////////////////////////////////////////////
// Engine status 
//
// Status is intended for display to humans to help understand system behavior.
// It does not need to be perfectly thread-safe.

#define STATUS_INIT(k,t,l) { \
	mgr->status.status[k].keyname = #k; \
	mgr->status.status[k].type    = t;  \
	mgr->status.status[k].legend  = "row locks: " l; \
    }

static void
status_init(toku_ltm* mgr) {
    // Note, this function initializes the keyname, type, and legend fields.
    // Value fields are initialized to zero by compiler.
    STATUS_INIT(LTM_LOCKS_LIMIT, UINT64, "number of locks allowed");
    STATUS_INIT(LTM_LOCKS_CURR, 	UINT64, "number of locks in existence"); 
    STATUS_INIT(LTM_LOCK_MEMORY_LIMIT, 	UINT64, "maximum amount of memory allowed for locks"); 
    STATUS_INIT(LTM_LOCK_MEMORY_CURR, 		UINT64, "memory in use for locks");
    STATUS_INIT(LTM_LOCK_ESCALATION_SUCCESSES, 	UINT64, "number of times lock escalation succeeded");
    STATUS_INIT(LTM_LOCK_ESCALATION_FAILURES, 	UINT64, "number of times lock escalation failed");
    STATUS_INIT(LTM_READ_LOCK, 			UINT64, "number of times read lock taken successfully");
    STATUS_INIT(LTM_READ_LOCK_FAIL, 		UINT64, "number of times read lock denied");
    STATUS_INIT(LTM_OUT_OF_READ_LOCKS, 		UINT64, "number of times read lock denied for out_of_locks");
    STATUS_INIT(LTM_WRITE_LOCK, 		UINT64, "number of times write lock taken successfully");
    STATUS_INIT(LTM_WRITE_LOCK_FAIL, 		UINT64, "number of times write lock denied");
    STATUS_INIT(LTM_OUT_OF_WRITE_LOCKS, 	UINT64, "number of times write lock denied for out_of_locks");
    STATUS_INIT(LTM_LT_CREATE, 			UINT64, "number of locktrees created");
    STATUS_INIT(LTM_LT_CREATE_FAIL, 		UINT64, "number of locktrees unable to be created");
    STATUS_INIT(LTM_LT_DESTROY, 		UINT64, "number of locktrees destroyed");
    STATUS_INIT(LTM_LT_NUM, 			UINT64, "number of locktrees (should be created - destroyed)");
    STATUS_INIT(LTM_LT_NUM_MAX, 		UINT64, "max number of locktrees that have existed simultaneously");
    mgr->status.initialized = true;
}
#undef STATUS_INIT

#define STATUS_VALUE(x) status.status[x].value.num

void 
toku_ltm_get_status(toku_ltm* mgr, LTM_STATUS statp) {
    if (!mgr->status.initialized) 
	status_init(mgr);
    mgr->STATUS_VALUE(LTM_LOCKS_LIMIT) = mgr->locks_limit;
    mgr->STATUS_VALUE(LTM_LOCKS_CURR) = mgr->curr_locks;
    mgr->STATUS_VALUE(LTM_LOCK_MEMORY_LIMIT) = mgr->lock_memory_limit;
    mgr->STATUS_VALUE(LTM_LOCK_MEMORY_CURR) = mgr->curr_lock_memory;
    *statp = mgr->status;
}

static inline int 
lt_panic(toku_lock_tree *tree, int r) {
    // TODO: (Zardosht) handle this panic properly
    return tree->mgr->panic(NULL, r);
}

// forward defs of lock request tree functions
static void lock_request_tree_init(toku_lock_tree *tree);
static void lock_request_tree_destroy(toku_lock_tree *tree);
static void lock_request_tree_insert(toku_lock_tree *tree, toku_lock_request *lock_request);
static void lock_request_tree_delete(toku_lock_tree *tree, toku_lock_request *lock_request);
static toku_lock_request *lock_request_tree_find(toku_lock_tree *tree, TXNID id);
                
const uint32_t __toku_default_buflen = 2;

static const DBT __toku_lt_infinity;
static const DBT __toku_lt_neg_infinity;

const DBT* const toku_lt_infinity     = &__toku_lt_infinity;
const DBT* const toku_lt_neg_infinity = &__toku_lt_neg_infinity;

static void 
ltm_mutex_lock(toku_ltm *mgr) {
    toku_mutex_lock(&mgr->mutex);
}

static void
ltm_mutex_unlock(toku_ltm *mgr) {
    toku_mutex_unlock(&mgr->mutex);
}

static void 
lt_mutex_lock(toku_lock_tree *tree) {
    toku_mutex_lock(&tree->mutex);
}

static void
lt_mutex_unlock(toku_lock_tree *tree) {
    toku_mutex_unlock(&tree->mutex);
}

char* 
toku_lt_strerror(TOKU_LT_ERROR r) {
    if (r >= 0) 
        return strerror(r);
    if (r == TOKU_LT_INCONSISTENT) {
        return "Locking data structures have become inconsistent.\n";
    }
    return "Unknown error in locking data structures.\n";
}

/* Compare two payloads assuming that at least one of them is infinite */ 
static inline int 
infinite_compare(const DBT* a, const DBT* b) {
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

static inline bool 
lt_is_infinite(const DBT* p) {
    bool r;
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
static inline int 
lt_verify_null_key(const DBT* key) {
    if (key && key->size && !key->data) 
        return EINVAL;
    return 0;
}

static inline DBT* 
recreate_DBT(DBT* dbt, void* payload, uint32_t length) {
    memset(dbt, 0, sizeof(DBT));
    dbt->data = payload;
    dbt->size = length;
    return dbt;
}

static inline int 
lt_txn_cmp(const TXNID a, const TXNID b) {
    return a < b ? -1 : (a != b);
}

static inline void 
ltm_remove_lt(toku_ltm* mgr, toku_lock_tree* lt) {
    assert(mgr && lt);
    toku_lth_delete(mgr->lth, lt);
}

static inline int 
ltm_add_lt(toku_ltm* mgr, toku_lock_tree* lt) {
    assert(mgr && lt);
    return toku_lth_insert(mgr->lth, lt);
}

int 
toku_lt_point_cmp(const toku_point* x, const toku_point* y) {
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
    return x->lt->compare_fun(&x->lt->fake_db,
                              recreate_DBT(&point_1, x->key_payload, x->key_len),
                              recreate_DBT(&point_2, y->key_payload, y->key_len));
}

/* Lock tree manager functions begin here */
int 
toku_ltm_create(toku_ltm** pmgr,
                uint32_t locks_limit,
                uint64_t lock_memory_limit,
                int   (*panic)(DB*, int)) {
    int r = ENOSYS;
    toku_ltm* mgr = NULL;

    if (!pmgr || !locks_limit || panic == NULL) {
        r = EINVAL; goto cleanup;
    }

    mgr          = (toku_ltm*) toku_xmalloc(sizeof(*mgr));
    memset(mgr, 0, sizeof(toku_ltm));

    r = toku_ltm_set_max_locks(mgr, locks_limit);
    if (r != 0)
        goto cleanup;
    r = toku_ltm_set_max_lock_memory(mgr, lock_memory_limit);
    if (r != 0) 
        goto cleanup;
    mgr->panic            = panic;

    r = toku_lth_create(&mgr->lth);
    assert(r == 0 && mgr->lth);

    r = toku_idlth_create(&mgr->idlth);
    assert(r == 0 && mgr->idlth);

    toku_mutex_init(&mgr->mutex, NULL);
    DRD_IGNORE_VAR(mgr->status);
    r = 0;
    *pmgr = mgr;
cleanup:
    if (r != 0) {
        if (mgr) {
            assert(mgr->lth == NULL);
            assert(mgr->idlth == NULL);
            toku_free(mgr);
        }
    }
    return r;
}

// For now, ltm_open does nothing.
int
toku_ltm_open(toku_ltm *mgr) {
    assert(mgr);
    return 0;
}

int 
toku_ltm_close(toku_ltm* mgr) {
    assert(mgr);

    int r           = ENOSYS;
    int first_error = 0;

    toku_lth_start_scan(mgr->lth);
    toku_lock_tree* lt;
    while ((lt = toku_lth_next(mgr->lth)) != NULL) {
        r = toku_lt_close(lt);
        if (r != 0 && first_error == 0) 
            first_error = r;
    }
    toku_lth_close(mgr->lth);
    toku_idlth_close(mgr->idlth);
    toku_mutex_destroy(&mgr->mutex);
    DRD_STOP_IGNORING_VAR(mgr->status);
    assert(mgr->curr_locks == 0 && mgr->curr_lock_memory == 0);
    toku_free(mgr);

    return first_error;
}

int 
toku_ltm_get_max_locks(toku_ltm* mgr, uint32_t* locks_limit) {
    if (!mgr || !locks_limit)
        return EINVAL;
    *locks_limit = mgr->locks_limit;
    return 0;
}

int 
toku_ltm_set_max_locks(toku_ltm* mgr, uint32_t locks_limit) {
    if (!mgr || !locks_limit)
        return EINVAL;
    if (locks_limit < mgr->curr_locks) 
        return EDOM;
    mgr->locks_limit = locks_limit;
    return 0;
}

int 
toku_ltm_get_max_lock_memory(toku_ltm* mgr, uint64_t* lock_memory_limit) {
    if (!mgr || !lock_memory_limit)
        return EINVAL;
    *lock_memory_limit = mgr->lock_memory_limit;
    return 0;
}

int 
toku_ltm_set_max_lock_memory(toku_ltm* mgr, uint64_t lock_memory_limit) {
    if (!mgr || !lock_memory_limit)
        return EINVAL;
    if (lock_memory_limit < mgr->curr_locks)
        return EDOM;
    mgr->lock_memory_limit = lock_memory_limit;
    return 0;
}

static inline void 
ltm_incr_locks(toku_ltm* tree_mgr, uint32_t replace_locks) {
    assert(replace_locks <= tree_mgr->curr_locks);
    (void) __sync_fetch_and_sub(&tree_mgr->curr_locks, replace_locks);
    (void) __sync_fetch_and_add(&tree_mgr->curr_locks, 1);
}

static inline void 
ltm_decr_locks(toku_ltm* tree_mgr, uint32_t locks) {
    assert(tree_mgr);
    assert(tree_mgr->curr_locks >= locks);
    (void) __sync_fetch_and_sub(&tree_mgr->curr_locks, locks);
}

static int 
ltm_out_of_locks(toku_ltm *mgr) {
    int r = 0;
    if (mgr->curr_locks >= mgr->locks_limit || mgr->curr_lock_memory >= mgr->lock_memory_limit)
        r = TOKUDB_OUT_OF_LOCKS;
    return r;
}

static void
ltm_incr_lock_memory(toku_ltm *mgr, size_t s) {
    (void) __sync_add_and_fetch(&mgr->curr_lock_memory, s);
}

static void 
ltm_incr_lock_memory_callback(void *extra, size_t s) {
    toku_ltm *mgr = (toku_ltm *) extra;
    ltm_incr_lock_memory(mgr, s);
}

static void 
ltm_decr_lock_memory(toku_ltm *mgr, size_t s) {
    assert(mgr->curr_lock_memory >= s);
    (void) __sync_sub_and_fetch(&mgr->curr_lock_memory, s);
}

static void 
ltm_decr_lock_memory_callback(void *extra, size_t s) {
    toku_ltm *mgr = (toku_ltm *) extra;
    ltm_decr_lock_memory(mgr, s);
}

static inline void 
p_free(toku_lock_tree* tree, toku_point* point) {
    assert(point);
    if (!lt_is_infinite(point->key_payload)) {
        ltm_decr_lock_memory(tree->mgr, toku_malloc_usable_size(point->key_payload));
        toku_free(point->key_payload);
    }
    ltm_decr_lock_memory(tree->mgr, toku_malloc_usable_size(point));
    toku_free(point);
}

/*
   Allocate and copy the payload.
*/
static inline int 
payload_copy(toku_lock_tree* tree,
             void** payload_out, uint32_t* len_out,
             void*  payload_in,  uint32_t  len_in) {
    int r = 0;
    assert(payload_out && len_out);
    if (!len_in) {
        assert(!payload_in || lt_is_infinite(payload_in));
        *payload_out = payload_in;
        *len_out     = len_in;
    } else {
        assert(payload_in);
        *payload_out = toku_xmalloc((size_t)len_in); //2808
        ltm_incr_lock_memory(tree->mgr, toku_malloc_usable_size(*payload_out));
        resource_assert(*payload_out);
        *len_out     = len_in;
        memcpy(*payload_out, payload_in, (size_t)len_in);
    }
    return r;
}

static inline void
p_makecopy(toku_lock_tree* tree, toku_point** ppoint) {
    assert(ppoint);
    int r;
    toku_point*     point      = *ppoint;
    toku_point*     temp_point = NULL;
    temp_point = (toku_point*)toku_xmalloc(sizeof(toku_point)); //2808
    ltm_incr_lock_memory(tree->mgr, toku_malloc_usable_size(temp_point));
    *temp_point = *point;
    r = payload_copy(tree,
                     &temp_point->key_payload, &temp_point->key_len,
                     point->key_payload,       point->key_len);
    assert_zero(r);
    *ppoint = temp_point;
}

/* Provides access to a selfread tree for a particular transaction.
   Returns NULL if it does not exist yet. */
toku_range_tree* 
toku_lt_ifexist_selfread(toku_lock_tree* tree, TXNID txn) {
    assert(tree);
    rt_forest* forest = toku_rth_find(tree->rth, txn);
    return forest ? forest->self_read : NULL;
}

/* Provides access to a selfwrite tree for a particular transaction.
   Returns NULL if it does not exist yet. */
toku_range_tree* 
toku_lt_ifexist_selfwrite(toku_lock_tree* tree, TXNID txn) {
    assert(tree);
    rt_forest* forest = toku_rth_find(tree->rth, txn);
    return forest ? forest->self_write : NULL;
}

static inline int 
lt_add_locked_txn(toku_lock_tree* tree, TXNID txn) {
    /* Neither selfread nor selfwrite exist. */
    int r = toku_rth_insert(tree->rth, txn);
    return r;
}

/* Provides access to a selfread tree for a particular transaction.
   Creates it if it does not exist. */
static inline int 
lt_selfread(toku_lock_tree* tree, TXNID txn, toku_range_tree** pselfread) {
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
        r = toku_rt_create(&forest->self_read, toku_lt_point_cmp, lt_txn_cmp, FALSE,
                           ltm_incr_lock_memory_callback, ltm_decr_lock_memory_callback, tree->mgr);
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
static inline int 
lt_selfwrite(toku_lock_tree* tree, TXNID txn, toku_range_tree** pselfwrite) {
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
        r = toku_rt_create(&forest->self_write, toku_lt_point_cmp, lt_txn_cmp, FALSE,
                           ltm_incr_lock_memory_callback, ltm_decr_lock_memory_callback, tree->mgr);
        if (r != 0) 
            goto cleanup;
        assert(forest->self_write);
    }
    *pselfwrite = forest->self_write;
    r = 0;
cleanup:
    return r;
}

static inline bool 
interval_dominated(toku_interval* query, toku_interval* by) {
    assert(query && by);
    return (bool)(toku_lt_point_cmp(query->left,  by->left) >= 0 &&
                  toku_lt_point_cmp(query->right, by->right) <= 0);
}

/*
    This function only supports non-overlapping trees.
    Uses the standard definition of dominated from the design document.
    Determines whether 'query' is dominated by 'rt'.
*/
static inline int 
lt_rt_dominates(toku_lock_tree* tree, toku_interval* query, toku_range_tree* rt, bool* dominated) {
    assert(tree && query && dominated);
    if (!rt) {
        *dominated = FALSE;
        return 0;
    }
    
    bool            allow_overlaps;
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

#if TOKU_LT_USE_BORDERWRITE

static inline bool 
interval_strictly_internal(toku_interval* query, toku_interval* to) {
    assert(query && to);
    return (bool)(toku_lt_point_cmp(query->left,  to->left) > 0 &&
                  toku_lt_point_cmp(query->right, to->right) < 0);
}

typedef enum {TOKU_NO_CONFLICT, TOKU_MAYBE_CONFLICT, TOKU_YES_CONFLICT} toku_conflict;

/*
    This function checks for conflicts in the borderwrite tree.
    If no range overlaps, there is no conflict.
    If >= 2 ranges overlap the query then, by definition of borderwrite,
    at least one overlapping regions must not be 'self'. Design document
    explains why this MUST cause a conflict.
    If exactly one border_range overlaps and its data == self, there is no conflict.
    If exactly one border_range overlaps and its data != self:
     - If the query range overlaps one of the endpoints of border_range,
       there must be a conflict
     - Otherwise (query range is strictly internal to border_range),
       we need to check the 'peer'write table to determine if there is a conflict or not.
*/
static inline int 
lt_borderwrite_conflict(toku_lock_tree* tree, TXNID self,
                        toku_interval* query,
                        toku_conflict* conflict, TXNID* peer) {
    assert(tree && query && conflict && peer);
    assert(tree->borderwrite);

    const uint32_t query_size = 2;
    toku_range   buffer[query_size];
    uint32_t     buflen     = query_size;
    toku_range*  buf        = &buffer[0];
    uint32_t     numfound;
    int          r;

    r = toku_rt_find(tree->borderwrite, query, query_size, &buf, &buflen, &numfound);
    if (r != 0) 
        return r;
    assert(numfound <= query_size);
    if (numfound == 0)
        *conflict = TOKU_NO_CONFLICT;
    else if (numfound == 1) {
        toku_interval* border_range = &buf[0].ends;
        TXNID border_txn = buf[0].data;
        if (!lt_txn_cmp(border_txn, self))
            *conflict = TOKU_NO_CONFLICT;
        else if (interval_strictly_internal(query, border_range)) {
            // Only the end-points of border_range are known to be locked.
            // We need to look at the self_write tree to determine
            // if there is a conflict or not.
            *conflict = TOKU_MAYBE_CONFLICT;
            *peer = border_txn;
        }
        else
            *conflict = TOKU_YES_CONFLICT;
    }
    else {
        // query overlaps >= 2 border ranges and therefore overlaps end points
        // of >= 2 border_ranges with different transactions (at least one must
        // conflict).
        *conflict = TOKU_YES_CONFLICT;
    }
    return 0;
}
#endif

/*
    Determines whether 'query' meets 'rt'.
    This function supports only non-overlapping trees with homogeneous 
    transactions, i.e., a selfwrite or selfread table only.
    Uses the standard definition of 'query' meets 'tree' at 'data' from the
    design document.
*/
static inline int 
lt_meets(toku_lock_tree* tree, toku_interval* query, toku_range_tree* rt, bool* met) {
    assert(tree && query && rt && met);
    const uint32_t query_size = 1;
    toku_range   buffer[query_size];
    uint32_t     buflen     = query_size;
    toku_range*  buf        = &buffer[0];
    uint32_t     numfound;
    int          r;
    bool         allow_overlaps;

    /* Sanity check. (Function only supports non-overlap range trees.) */
    r = toku_rt_get_allow_overlaps(rt, &allow_overlaps);
    if (r != 0) 
        return r;
    assert(!allow_overlaps);

    r = toku_rt_find(rt, query, query_size, &buf, &buflen, &numfound);
    if (r != 0) 
        return r;
    assert(numfound <= query_size);
    *met = (bool)(numfound != 0);
    return 0;
}

/* Checks for if a write range conflicts with reads.
   Supports ranges. */
static inline int 
lt_write_range_conflicts_reads(toku_lock_tree* tree, TXNID txn, toku_interval* query) {
    int r    = 0;
    bool met = FALSE;
    toku_rth_start_scan(tree->rth);
    rt_forest* forest;
    
    while ((forest = toku_rth_next(tree->rth)) != NULL) {
        if (forest->self_read != NULL && lt_txn_cmp(forest->hash_key, txn)) {
            r = lt_meets(tree, query, forest->self_read, &met);
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

static inline int 
lt_write_range_conflicts_writes(toku_lock_tree* tree, TXNID txn, toku_interval* query) {
    int r    = 0;
    bool met = FALSE;
    toku_rth_start_scan(tree->rth);
    rt_forest* forest;
    
    while ((forest = toku_rth_next(tree->rth)) != NULL) {
        if (forest->self_write != NULL && lt_txn_cmp(forest->hash_key, txn)) {
            r = lt_meets(tree, query, forest->self_write, &met);
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
static inline int 
lt_check_borderwrite_conflict(toku_lock_tree* tree, TXNID txn, toku_interval* query) {
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

        bool met;
        r = lt_meets(tree, query, peer_selfwrite, &met);
        if (r != 0)   
            return r;
        conflict = met ? TOKU_YES_CONFLICT : TOKU_NO_CONFLICT;
    }
    if (conflict == TOKU_NO_CONFLICT) 
        return 0;
    assert(conflict == TOKU_YES_CONFLICT);
    return DB_LOCK_NOTGRANTED;
#else
    int r = lt_write_range_conflicts_writes(tree, txn, query);
    return r;
#endif
}

static inline void 
payload_from_dbt(void** payload, uint32_t* len, const DBT* dbt) {
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

static inline void 
init_point(toku_point* point, toku_lock_tree* tree, const DBT* key) {
    assert(point && tree && key);
    memset(point, 0, sizeof(toku_point));
    point->lt = tree;
    payload_from_dbt(&point->key_payload, &point->key_len, key);
}

static inline void 
init_query(toku_interval* query, toku_point* left, toku_point* right) {
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

static inline void 
init_insert(toku_range* to_insert, toku_point* left, toku_point* right, TXNID txn) {
    to_insert->ends.left  = left;
    to_insert->ends.right = right;
    to_insert->data  = txn;
}

/* Returns whether the point already exists
   as an endpoint of the given range. */
static inline bool 
lt_p_independent(toku_point* point, toku_interval* range) {
    assert(point && range);
    return (bool)(point != range->left && point != range->right);
}

static inline int 
lt_determine_extreme(toku_lock_tree* tree,
                     toku_range* to_insert,
                     bool* alloc_left, BOOL* alloc_right,
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
static inline int 
lt_extend_extreme(toku_lock_tree* tree,toku_range* to_insert, bool* alloc_left, BOOL* alloc_right, uint32_t numfound) {
    return lt_determine_extreme(tree, to_insert, alloc_left, alloc_right, numfound, 0);
}

/* Has no starting point. */
static inline int 
lt_find_extreme(toku_lock_tree* tree, toku_range* to_insert, uint32_t numfound) {
    assert(numfound > 0);
    *to_insert = tree->buf[0];
    bool ignore_left = TRUE;
    bool ignore_right = TRUE;
    return lt_determine_extreme(tree, to_insert, &ignore_left, &ignore_right, numfound, 1);
}

static inline void 
lt_alloc_extreme(toku_lock_tree* tree, toku_range* to_insert, bool alloc_left, BOOL* alloc_right) {
    assert(to_insert && alloc_right);
    bool copy_left = FALSE;
    
    /* The pointer comparison may speed up the evaluation in some cases, 
       but it is not strictly needed */
    if (alloc_left && alloc_right &&
        (to_insert->ends.left == to_insert->ends.right ||
         toku_lt_point_cmp(to_insert->ends.left, to_insert->ends.right) == 0)) {
        *alloc_right = FALSE;
        copy_left    = TRUE;
    }

    if (alloc_left) {
        p_makecopy(tree, &to_insert->ends.left);
    }
    if (*alloc_right) {
        assert(!copy_left);
        p_makecopy(tree, &to_insert->ends.right);
    }
    else if (copy_left) 
        to_insert->ends.right = to_insert->ends.left;
}

static inline int 
lt_delete_overlapping_ranges(toku_lock_tree* tree, toku_range_tree* rt, uint32_t numfound) {
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

static inline int 
lt_free_points(toku_lock_tree* tree, toku_interval* to_insert, uint32_t numfound) {
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
static inline int 
consolidate_range_tree(toku_lock_tree* tree, bool found_only, toku_range* to_insert, toku_range_tree *rt, bool do_borderwrite_insert) {
    assert(tree && to_insert);

    int r;
    bool             alloc_left    = TRUE;
    bool             alloc_right   = TRUE;
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
    }
    /* Allocate the consolidated range */
    lt_alloc_extreme(tree, to_insert, alloc_left, &alloc_right);

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
        if (alloc_left)  p_free(tree, to_insert->ends.left);
        if (alloc_right) p_free(tree, to_insert->ends.right); 
        return lt_panic(tree, TOKU_LT_INCONSISTENT);
    }
    
    ltm_incr_locks(tree->mgr, numfound);
    return 0;
}

static inline int 
consolidate_reads(toku_lock_tree* tree, bool found_only, toku_range* to_insert, TXNID txn) {
    assert(tree && to_insert);
    toku_range_tree* selfread;
    int r = lt_selfread(tree, txn, &selfread);
    if (r != 0) 
        return r;
    assert(selfread);
    return consolidate_range_tree(tree, found_only, to_insert, selfread, FALSE);
}

static inline int 
consolidate_writes(toku_lock_tree* tree, toku_range* to_insert, TXNID txn) {
    assert(tree && to_insert);
    toku_range_tree* selfwrite;
    int r = lt_selfwrite(tree, txn, &selfwrite);
    if (r != 0) 
        return r;
    assert(selfwrite);
    return consolidate_range_tree(tree, FALSE, to_insert, selfwrite, TRUE);
}

static inline void 
lt_init_full_query(toku_lock_tree* tree, toku_interval* query, toku_point* left, toku_point* right) {
    init_point(left,  tree, (DBT*)toku_lt_neg_infinity);
    init_point(right, tree, (DBT*)toku_lt_infinity);
    init_query(query, left, right);
}

typedef struct {
    toku_lock_tree*  lt;
    toku_interval*   query;
    toku_range*      store_value;
} free_contents_info;

static int 
free_contents_helper(toku_range* value, void* extra) {
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
static inline int 
lt_free_contents(toku_lock_tree* tree, toku_range_tree* rt) {
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
    r = toku_rt_close(rt);
    assert_zero(r);
    return r;
}

static inline bool 
r_backwards(toku_interval* range) {
    assert(range && range->left && range->right);
    toku_point* left  = (toku_point*)range->left;
    toku_point* right = (toku_point*)range->right;

    /* Optimization: if all the pointers are equal, clearly left == right. */
    return (bool) ((left->key_payload  != right->key_payload) &&
                   (toku_lt_point_cmp(left, right) > 0));
}

/* Preprocess step for acquire functions. */
static inline int 
lt_preprocess(toku_lock_tree* tree,
              __attribute__((unused)) TXNID txn,
              const DBT* key_left,
              const DBT* key_right,
              toku_point* left, toku_point* right,
              toku_interval* query) {
    int r = ENOSYS;

    /* Verify that NULL keys have payload and size that are mutually 
       consistent*/
    if ((r = lt_verify_null_key(key_left))   != 0)
        goto cleanup;
    if ((r = lt_verify_null_key(key_right))  != 0) 
        goto cleanup;

    init_point(left,  tree, key_left);
    init_point(right, tree, key_right);
    init_query(query, left, right);

    /* Verify left <= right, otherwise return EDOM. */
    if (r_backwards(query)) { 
        r = EDOM; goto cleanup; 
    }
    r = 0;
cleanup:
    return r;
}

static inline int 
lt_get_border_in_selfwrite(toku_lock_tree* tree,
                           toku_range* pred, toku_range* succ,
                           bool* found_p,    BOOL* found_s,
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

static inline int 
lt_get_border_in_borderwrite(toku_lock_tree* tree,
                             toku_range* pred, toku_range* succ,
                             bool* found_p,    BOOL* found_s,
                             toku_range* to_insert) {
    assert(tree && pred && succ && found_p && found_s);                                    
    int r;
    if (!tree->borderwrite)  
        return lt_panic(tree, TOKU_LT_INCONSISTENT);
    r = toku_rt_predecessor(tree->borderwrite, to_insert->ends.left,  pred, found_p);
    if (r != 0) 
        return r;
    r = toku_rt_successor  (tree->borderwrite, to_insert->ends.right, succ, found_s);
    if (r != 0) 
        return r;
    return 0;
}

static inline int 
lt_expand_border(toku_lock_tree* tree, toku_range* to_insert,
                 toku_range* pred, toku_range* succ,
                 bool  found_p,    BOOL  found_s) {
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

static inline int 
lt_split_border(toku_lock_tree* tree, toku_range* to_insert,
                toku_range* pred, toku_range* succ,
                bool  found_p,    BOOL  found_s) {
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
static inline int 
lt_borderwrite_insert(toku_lock_tree* tree, toku_interval* query, toku_range* to_insert) {
    assert(tree && query && to_insert);
    assert(tree->borderwrite);

    int r;

    // find all overlapping ranges.  there can be 0 or 1.
    const uint32_t query_size = 1;
    uint32_t numfound;
    r = toku_rt_find(tree->borderwrite, query, query_size, &tree->bw_buf, &tree->bw_buflen, &numfound);
    if (r != 0) 
        return lt_panic(tree, r);
    assert(numfound <= query_size);

    if (numfound == 0) { 
        // Find the adjacent ranges in the borderwrite tree and expand them if they are owned by me

        // Find the predecessor and successor of the range to be inserted
        toku_range pred; bool found_p = FALSE;
        toku_range succ; bool found_s = FALSE;
        r = lt_get_border_in_borderwrite(tree, &pred, &succ, &found_p, &found_s, to_insert);
        if (r != 0) 
            return lt_panic(tree, r);
        if (found_p && found_s && !lt_txn_cmp(pred.data, succ.data)) {
            return lt_panic(tree, TOKU_LT_INCONSISTENT); }
        r = lt_expand_border(tree, to_insert, &pred, &succ, found_p, found_s);
        if (r != 0) 
            return lt_panic(tree, r);
        r = toku_rt_insert(tree->borderwrite, to_insert);
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
                    to_insert->ends.left = tree->bw_buf[0].ends.left;
                if (toku_lt_point_cmp(to_insert->ends.right, tree->bw_buf[0].ends.right) < 0)
                    to_insert->ends.right = tree->bw_buf[0].ends.right;
                r = toku_rt_delete(tree->borderwrite, &tree->bw_buf[0]);
                if (r != 0)
                    return lt_panic(tree, r);
                r = toku_rt_insert(tree->borderwrite, to_insert);
                if (r != 0)
                    return lt_panic(tree, r);
            }
        } else {
            // The range to be inserted overlapped with a borderwrite range owned by some other transaction.
            // Split the borderwrite range to remove the overlap with the range being inserted.

            // Find predecessor and successor of the range to be inserted
            toku_range pred; bool found_p = FALSE;
            toku_range succ; bool found_s = FALSE;
            r = lt_get_border_in_selfwrite(tree, &pred, &succ, &found_p, &found_s, to_insert);
            if (r != 0) 
                return lt_panic(tree, r);
            r = lt_split_border(tree, to_insert, &pred, &succ, found_p, found_s);
            if (r != 0) 
                return lt_panic(tree, r);
            r = toku_rt_insert(tree->borderwrite, to_insert);
            if (r != 0) 
                return lt_panic(tree, r);
        }
    }

    return r;
}

/* TODO: Investigate better way of passing comparison functions. */
int 
toku_lt_create(toku_lock_tree** ptree,
               toku_ltm* mgr,
               toku_dbt_cmp compare_fun) {
    int r = ENOSYS;
    toku_lock_tree* tmp_tree = NULL;
    if (!ptree || !mgr || !compare_fun) {
        r = EINVAL; goto cleanup;
    }

    // allocate a tree, initialized to zeroes
    tmp_tree = toku_xmalloc(sizeof(*tmp_tree));
    memset(tmp_tree, 0, sizeof(*tmp_tree));
    tmp_tree->mgr              = mgr;
    tmp_tree->compare_fun = compare_fun;
    tmp_tree->lock_escalation_allowed = TRUE;
    r = toku_rt_create(&tmp_tree->borderwrite, toku_lt_point_cmp, lt_txn_cmp, FALSE,
                       ltm_incr_lock_memory_callback, ltm_decr_lock_memory_callback, mgr);
    assert(r == 0);
    r = toku_rth_create(&tmp_tree->rth);
    assert(r == 0);
    tmp_tree->buflen = __toku_default_buflen;
    tmp_tree->buf    = (toku_range*) toku_xmalloc(tmp_tree->buflen * sizeof(toku_range));
    tmp_tree->bw_buflen = __toku_default_buflen;
    tmp_tree->bw_buf    = (toku_range*) toku_xmalloc(tmp_tree->bw_buflen * sizeof(toku_range));
    lock_request_tree_init(tmp_tree);
    toku_mutex_init(&tmp_tree->mutex, NULL);
    tmp_tree->ref_count = 1;
    *ptree = tmp_tree;
    r = 0;
cleanup:
    if (r != 0) {
        if (tmp_tree) {
            toku_free(tmp_tree);
        }
    }
    return r;
}

void 
toku_ltm_invalidate_lt(toku_ltm* mgr, DICTIONARY_ID dict_id) {
    assert(mgr && dict_id.dictid != DICTIONARY_ID_NONE.dictid);
    toku_lt_map* map = NULL;
    ltm_mutex_lock(mgr);
    map = toku_idlth_find(mgr->idlth, dict_id);
    if (map) { 
	toku_idlth_delete(mgr->idlth, dict_id);
    }
    ltm_mutex_unlock(mgr);
}

static inline void 
lt_set_dict_id(toku_lock_tree* lt, DICTIONARY_ID dict_id) {
    assert(lt && dict_id.dictid != DICTIONARY_ID_NONE.dictid);
    lt->dict_id = dict_id;
}

void
toku_lt_update_descriptor(toku_lock_tree* tree, DESCRIPTOR desc) {
    if (tree->desc_s.dbt.data) {
        toku_free(tree->desc_s.dbt.data);
        tree->desc_s.dbt.data = NULL;
    }
    if (desc) {
        tree->desc_s.dbt.size = desc->dbt.size;
        tree->desc_s.dbt.data = toku_memdup(desc->dbt.data, desc->dbt.size);
    }
}

int 
toku_ltm_get_lt(toku_ltm* mgr, toku_lock_tree** ptree, DICTIONARY_ID dict_id, DESCRIPTOR desc, 
        toku_dbt_cmp compare_fun, toku_lt_on_create_cb on_create_callback, void *on_create_extra,
        toku_lt_on_close_cb on_close_callback) {
    /* first look in hash table to see if lock tree exists for that db,
       if so return it */
    int r = ENOSYS;
    toku_lt_map* map     = NULL;
    toku_lock_tree* tree = NULL;
    bool added_to_ltm    = FALSE;
    bool added_to_idlth  = FALSE;

    // verify that the on_create and on_close callbacks are either 
    // mutually null or mutually non-null.
    if ((on_close_callback == NULL) != (on_close_callback == NULL)) {
        r = EINVAL;
        goto out;
    }
    
    ltm_mutex_lock(mgr);
    map = toku_idlth_find(mgr->idlth, dict_id);
    if (map != NULL) {
        /* Load already existing lock tree. */
        tree = map->tree;
        assert (tree != NULL);
        toku_lt_add_ref(tree);
        *ptree = tree;
        r = 0;
        goto cleanup;
    }
    /* Must create new lock tree for this dict_id*/
    r = toku_lt_create(&tree, mgr, compare_fun);
    if (r != 0) {
        goto cleanup;
    }

    // we just created the locktree, so call the callback if necessary,
    // and set the on_close callback. we checked above that these callbacks
    // are either mutually null or non-null, so this is correct.
    if (on_create_callback) {
        on_create_callback(tree, on_create_extra);
    } else {
        assert(on_close_callback == NULL);
    }
    tree->on_close_callback = on_close_callback;
    
    lt_set_dict_id(tree, dict_id);
    /* add tree to ltm */
    r = ltm_add_lt(mgr, tree);
    if (r != 0) {
        goto cleanup;
    }
    added_to_ltm = TRUE;

    toku_lt_update_descriptor(tree, desc);
    tree->fake_db.cmp_descriptor = &tree->desc_s;

    /* add mapping to idlth*/
    r = toku_idlth_insert(mgr->idlth, dict_id);
    if (r != 0) {
        goto cleanup;
    }
    added_to_idlth = TRUE;
    
    map = toku_idlth_find(mgr->idlth, dict_id);
    assert(map);
    map->tree = tree;

    /* No add ref needed because ref count was set to 1 in toku_lt_create */        
    *ptree = tree;
    r = 0;
cleanup:
    if (r == 0) {
        mgr->STATUS_VALUE(LTM_LT_CREATE)++;
        mgr->STATUS_VALUE(LTM_LT_NUM)++;
        if (mgr->STATUS_VALUE(LTM_LT_NUM) > mgr->STATUS_VALUE(LTM_LT_NUM_MAX)) {
            mgr->STATUS_VALUE(LTM_LT_NUM_MAX) = mgr->STATUS_VALUE(LTM_LT_NUM);
        }
    }
    else {
        if (tree != NULL) {
            if (added_to_ltm) {
                ltm_remove_lt(mgr, tree);
            }
            if (added_to_idlth) {
                toku_idlth_delete(mgr->idlth, dict_id);
            }
            // if the on_create callback was called, then we will have set
            // the on_close callback to something non-null, and it will
            // be called in toku_lt_close(), as required.
            toku_lt_close(tree); 
        }
        mgr->STATUS_VALUE(LTM_LT_CREATE_FAIL)++;
    }
    ltm_mutex_unlock(mgr);
    return r;
}

int 
toku_lt_close(toku_lock_tree* tree) {
    int r = ENOSYS;
    int first_error = 0;
    if (!tree) { 
        r = EINVAL; goto cleanup; 
    }
    // call the on close callback if necessary
    if (tree->on_close_callback) {
        tree->on_close_callback(tree);
    }
    tree->mgr->STATUS_VALUE(LTM_LT_DESTROY)++;
    tree->mgr->STATUS_VALUE(LTM_LT_NUM)--;
    lock_request_tree_destroy(tree);
    r = toku_rt_close(tree->borderwrite);
    if (!first_error && r != 0)
        first_error = r;

    uint32_t ranges = 0;
    toku_rth_start_scan(tree->rth);
    rt_forest* forest;
    while ((forest = toku_rth_next(tree->rth)) != NULL) {
        if (forest->self_read)
            ranges += toku_rt_get_size(forest->self_read);
        r = lt_free_contents(tree, forest->self_read);
        if (!first_error && r != 0)
            first_error = r;
        if (forest->self_write)
            ranges += toku_rt_get_size(forest->self_write);
        r = lt_free_contents(tree, forest->self_write);
        if (!first_error && r != 0) 
            first_error = r;
    }
    if (tree->desc_s.dbt.data) {
        toku_free(tree->desc_s.dbt.data);
        tree->desc_s.dbt.data = NULL;
    }
    ltm_decr_locks(tree->mgr, ranges);
    toku_rth_close(tree->rth);
    toku_mutex_destroy(&tree->mutex);
    toku_free(tree->buf);
    toku_free(tree->bw_buf);
    toku_free(tree->verify_buf);
    toku_free(tree);
    r = first_error;
cleanup:
    return r;
}

static int 
lt_try_acquire_range_read_lock(toku_lock_tree* tree, TXNID txn, const DBT* key_left, const DBT* key_right) {
    assert(tree);
    toku_mutex_assert_locked(&tree->mutex); // locked by this thread

    int r;
    toku_point left;
    toku_point right;
    toku_interval query;
    bool dominated;
    
    r = lt_preprocess(tree, txn, key_left, key_right, &left, &right, &query);
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

    // Check lock resource contraints
    r = ltm_out_of_locks(tree->mgr);
    if (r != 0)
        goto cleanup;

    // Now need to merge, copy the memory and insert
    toku_range  to_insert;
    init_insert(&to_insert, &left, &right, txn);
    // Consolidate the new range and all the overlapping ranges
    r = consolidate_reads(tree, FALSE, &to_insert, txn);
    if (r != 0) 
        goto cleanup;
    
    r = 0;
cleanup:
    return r;
}

/*
    Tests whether a range from BorderWrite is trivially escalatable.
    i.e. No read locks from other transactions overlap the range.
*/
static inline int 
border_escalation_trivial(toku_lock_tree* tree, toku_range* border_range, bool* trivial) {
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

static inline int 
escalate_writes_from_border_range(toku_lock_tree* tree, toku_range* border_range) {
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
    ltm_incr_locks(tree->mgr, numfound);

    r = 0;
cleanup:
    return r;
}

static int 
lt_escalate_read_locks_in_interval(toku_lock_tree* tree, toku_interval* query, TXNID txn) {
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

static int 
escalate_read_locks_helper(toku_range* border_range, void* extra) {
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
static int 
lt_escalate_read_locks(toku_lock_tree* tree, TXNID txn) {
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

static int 
escalate_write_locks_helper(toku_range* border_range, void* extra) {
    toku_lock_tree* tree = extra;
    int r                = ENOSYS;
    bool trivial;
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
static int 
lt_escalate_write_locks(toku_lock_tree* tree) {
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
static int 
lt_do_escalation(toku_lock_tree* lt) {
    assert(lt);
    toku_mutex_assert_locked(&lt->mutex);

    int r = ENOSYS;

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
    return r;
}

// run escalation algorithm on all locktrees
static int 
ltm_do_escalation(toku_ltm* mgr) {
    assert(mgr);
    int r = 0;
    ltm_mutex_lock(mgr);
    toku_lth_start_scan(mgr->lth);  // initialize iterator in mgr
    toku_lock_tree* lt;
    while ((lt = toku_lth_next(mgr->lth)) != NULL) {
        lt_mutex_lock(lt);
        r = lt_do_escalation(lt);
        lt_mutex_unlock(lt);
        if (r != 0) 
            break;
    }
    ltm_mutex_unlock(mgr);
    return r;
}

static int 
lt_acquire_range_read_lock(toku_lock_tree* tree, TXNID txn, const DBT* key_left, const DBT* key_right, bool do_escalation) {
    int r = ENOSYS;

    r = lt_try_acquire_range_read_lock(tree, txn, key_left, key_right);
    if (r==TOKUDB_OUT_OF_LOCKS && do_escalation) {
        lt_mutex_unlock(tree);
        r = ltm_do_escalation(tree->mgr);
        lt_mutex_lock(tree);
        if (r == 0) { 
	    r = lt_try_acquire_range_read_lock(tree, txn, key_left, key_right);
	    if (r == 0) {
		tree->mgr->STATUS_VALUE(LTM_LOCK_ESCALATION_SUCCESSES)++;
	    }
	    else if (r==TOKUDB_OUT_OF_LOCKS) {
		tree->mgr->STATUS_VALUE(LTM_LOCK_ESCALATION_FAILURES)++;	    
	    }
	}
    }

    if (tree) {
	if (r == 0) {
	    tree->mgr->STATUS_VALUE(LTM_READ_LOCK)++;
	}
	else {
	    tree->mgr->STATUS_VALUE(LTM_READ_LOCK_FAIL)++;
	    if (r == TOKUDB_OUT_OF_LOCKS) 
		tree->mgr->STATUS_VALUE(LTM_OUT_OF_READ_LOCKS)++;
	}
    }
    return r;
}

int 
toku_lt_acquire_range_read_lock(toku_lock_tree* tree, TXNID txn, const DBT* key_left, const DBT *key_right) {
    int r = 0;
    if (!tree || !key_left || !key_right)
        r = EINVAL;
    if (r == 0) {
        lt_mutex_lock(tree);
        r = lt_acquire_range_read_lock(tree, txn, key_left, key_right, true);
        lt_mutex_unlock(tree);
    }
    return r;
}

int 
toku_lt_acquire_read_lock(toku_lock_tree* tree, TXNID txn, const DBT* key) {
    return toku_lt_acquire_range_read_lock(tree, txn, key, key);
}

static int 
lt_try_acquire_range_write_lock(toku_lock_tree* tree, TXNID txn, const DBT* key_left, const DBT* key_right) {
    toku_mutex_assert_locked(&tree->mutex);

    int r;
    toku_point left;
    toku_point right;
    toku_interval query;

    r = lt_preprocess(tree, txn, key_left, key_right, &left, &right, &query);
    if (r != 0)
        goto cleanup;

    // if query is dominated by selfwrite('txn') then return success
    bool dominated;
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
    if (key_left == key_right) {
        // Point write lock.
        // Need to copy the memory and insert. No merging required in selfwrite.
        // This is a point, and if merging was possible it would have been dominated by selfwrite.

        r = ltm_out_of_locks(tree->mgr);
        if (r != 0)
            goto cleanup;

        // Insert into selfwrite
        toku_range to_insert;
        init_insert(&to_insert, &left, &right, txn);

        bool dummy = TRUE;
        lt_alloc_extreme(tree, &to_insert, TRUE, &dummy);
        bool free_left = FALSE;
        toku_range_tree* selfwrite;
        r = lt_selfwrite(tree, txn, &selfwrite);
        if (r != 0) { 
            free_left = TRUE; 
            goto cleanup_left;
        }
        assert(selfwrite);
        r = toku_rt_insert(selfwrite, &to_insert);
        if (r != 0) { 
            free_left = TRUE; 
            goto cleanup_left; 
        }
        // Update borderwrite
        r = lt_borderwrite_insert(tree, &query, &to_insert);
        if (r != 0) { 
            r = lt_panic(tree, r); 
            goto cleanup_left; 
        }
        ltm_incr_locks(tree->mgr, 0);
        r = 0;

    cleanup_left:
        if (r != 0)
            if (free_left) 
                p_free(tree, to_insert.ends.left);
    } else {
        // Check lock resource contraints
        r = ltm_out_of_locks(tree->mgr);
        if (r != 0)
            goto cleanup;

        // insert and consolidate into the local write set
        toku_range to_insert;
        init_insert(&to_insert, &left, &right, txn);
        r = consolidate_writes(tree, &to_insert, txn);
        if (r != 0)
            goto cleanup;
    }
cleanup:
    return r;
}

static int 
lt_acquire_range_write_lock(toku_lock_tree* tree, TXNID txn, const DBT* key_left, const DBT* key_right, bool do_escalation) {
    int r = ENOSYS;

    r = lt_try_acquire_range_write_lock(tree, txn, key_left, key_right);
    if (r == TOKUDB_OUT_OF_LOCKS && do_escalation) {
        lt_mutex_unlock(tree);
        r = ltm_do_escalation(tree->mgr);
        lt_mutex_lock(tree);
        if (r == 0) {
            r = lt_try_acquire_range_write_lock(tree, txn, key_left, key_right);
	    if (r == 0) {
		tree->mgr->STATUS_VALUE(LTM_LOCK_ESCALATION_SUCCESSES)++;
	    }
	    else if (r==TOKUDB_OUT_OF_LOCKS) {
		tree->mgr->STATUS_VALUE(LTM_LOCK_ESCALATION_FAILURES)++;	    
	    }
	}
    }

    if (tree) {
	if (r == 0) {
	    tree->mgr->STATUS_VALUE(LTM_WRITE_LOCK)++;
	}
	else {
	    tree->mgr->STATUS_VALUE(LTM_WRITE_LOCK_FAIL)++;
	    if (r == TOKUDB_OUT_OF_LOCKS) 
		tree->mgr->STATUS_VALUE(LTM_OUT_OF_WRITE_LOCKS)++;
	}
    }
    return r;
}

int
toku_lt_acquire_range_write_lock(toku_lock_tree* tree, TXNID txn, const DBT* key_left, const DBT* key_right) {
    int r = 0;
    if (!tree || !key_left || !key_right)
        r = EINVAL;
    if (r == 0) {
        lt_mutex_lock(tree);
        r = lt_acquire_range_write_lock(tree, txn, key_left, key_right, true);
        lt_mutex_unlock(tree);
    }
    return r;
}

int 
toku_lt_acquire_write_lock(toku_lock_tree* tree, TXNID txn, const DBT* key) {
    return toku_lt_acquire_range_write_lock(tree, txn, key, key);
}   

#if TOKU_LT_USE_BORDERWRITE
static inline int 
sweep_border(toku_lock_tree* tree, toku_range* range) {
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
    bool found_p = FALSE;
    bool found_s = FALSE;

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
static inline int 
lt_border_delete(toku_lock_tree* tree, toku_range_tree* rt) {
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
#endif

static inline int 
lt_unlock_txn(toku_lock_tree* tree, TXNID txn) {
    assert(tree);
    toku_mutex_assert_locked(&tree->mutex);

    int r;
    toku_range_tree *selfwrite = toku_lt_ifexist_selfwrite(tree, txn);
    toku_range_tree *selfread  = toku_lt_ifexist_selfread (tree, txn);

    uint32_t ranges = 0;

    if (selfread) {
        ranges += toku_rt_get_size(selfread);
        r = lt_free_contents(tree, selfread);
        if (r != 0) 
            return lt_panic(tree, r);
    }

    if (selfwrite) {
        ranges += toku_rt_get_size(selfwrite);

        r = lt_border_delete(tree, selfwrite);
        if (r != 0) 
            return lt_panic(tree, r);

        r = lt_free_contents(tree, selfwrite);
        if (r != 0) 
            return lt_panic(tree, r);
    }

    if (selfread || selfwrite) 
        toku_rth_delete(tree->rth, txn);
    
    ltm_decr_locks(tree->mgr, ranges);

    return 0;
}

static void lt_retry_lock_requests(toku_lock_tree *tree);

int 
toku_lt_unlock_txn(toku_lock_tree* tree, TXNID txn) {
#if TOKU_LT_DEBUG
    if (toku_lt_debug) 
        printf("%s:%u %lu\n", __FUNCTION__, __LINE__, txn);
#endif
    int r = 0;
    if (!tree) { 
        r = EINVAL; goto cleanup;
    }
    lt_mutex_lock(tree);
    lt_unlock_txn(tree, txn);
    lt_retry_lock_requests(tree);
    lt_mutex_unlock(tree);
cleanup:
    return r;
}

void 
toku_lt_add_ref(toku_lock_tree* tree) {
    assert(tree);
    lt_mutex_lock(tree);
    assert(tree->ref_count > 0);
    tree->ref_count++;
    lt_mutex_unlock(tree);
}

static void 
ltm_stop_managing_lt_unlocked(toku_ltm* mgr, toku_lock_tree* tree) {
    ltm_remove_lt(mgr, tree);
    toku_lt_map* map = toku_idlth_find(mgr->idlth, tree->dict_id);
    if (map && map->tree == tree) {
        toku_idlth_delete(mgr->idlth, tree->dict_id);
    }
}

int 
toku_lt_remove_ref(toku_lock_tree *tree) {
    int r = 0;
    bool do_close = false;
    
    // the following check is necessary to remove the lt, but not
    // sufficient. this will only tell us if we can bail out quickly
    // or if we have to keep going to do the actual expensive check
    // which includes taking the lt mgr lock (bigger lock)

    lt_mutex_lock(tree);
    assert(tree->ref_count > 0);
    if (tree->ref_count > 1) {
        // this reference cannot possibly be the last, so we just 
        // do the decrement and get out.
        tree->ref_count--;
        lt_mutex_unlock(tree);
        goto cleanup;
    }
    lt_mutex_unlock(tree);

    // now, get the manager lock and the tree lock, in that order.
    ltm_mutex_lock(tree->mgr);
    lt_mutex_lock(tree);
    // if the ref count is still 1, then we definitely have the last 
    // reference and can remove the lock tree. we know we have the last
    // because toku_ltm_get_lt holds the mgr mutex and add/remove ref
    // holds the tree mutex.
    assert(tree->ref_count > 0);
    if (--tree->ref_count == 0) {
        assert(tree->dict_id.dictid != DICTIONARY_ID_NONE.dictid);
        ltm_stop_managing_lt_unlocked(tree->mgr, tree);
        do_close = true;
    }
    lt_mutex_unlock(tree);
    ltm_mutex_unlock(tree->mgr);

    // for speed, do the actual close without holding any locks.
    // the tree is not in the manager, no one else can get to it.
    // so this is safe.
    if (do_close) {
        r = toku_lt_close(tree);
    } else {
        r = 0;
    }
cleanup:
    return r;
}

void 
toku_lt_remove_db_ref(toku_lock_tree* tree) {
    int r = toku_lt_remove_ref(tree);
    assert_zero(r);
}

void 
toku_lt_set_userdata(toku_lock_tree *tree, void *userdata) {
    tree->userdata = userdata;
}

void *
toku_lt_get_userdata(toku_lock_tree *tree) {
    return tree->userdata;
}

static void
lock_request_init_wait(toku_lock_request *lock_request) {
    if (!lock_request->wait_initialized) {
        toku_cond_init(&lock_request->wait, NULL);
        lock_request->wait_initialized = true;
    }
}

static void
lock_request_destroy_wait(toku_lock_request *lock_request) {
    if (lock_request->wait_initialized) {
        toku_cond_destroy(&lock_request->wait);
        lock_request->wait_initialized = false;
    }
}

void 
toku_lock_request_default_init(toku_lock_request *lock_request) {
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
toku_lock_request_set(toku_lock_request *lock_request, TXNID txnid, const DBT *key_left, const DBT *key_right, toku_lock_type lock_type) {
    assert(lock_request->state != LOCK_REQUEST_PENDING);
    lock_request->txnid = txnid;
    lock_request->key_left = key_left;
    lock_request->key_right = key_right;
    lock_request->type = lock_type;
    lock_request->state = LOCK_REQUEST_INIT;
}

void 
toku_lock_request_init(toku_lock_request *lock_request, TXNID txnid, const DBT *key_left, const DBT *key_right, toku_lock_type lock_type) {
    toku_lock_request_default_init(lock_request);
    toku_lock_request_set(lock_request, txnid, key_left, key_right, lock_type);
}

void 
toku_lock_request_destroy(toku_lock_request *lock_request) {
    if (lock_request->state == LOCK_REQUEST_PENDING) {
        toku_lock_tree *tree = lock_request->tree;
        lt_mutex_lock(tree);
        lock_request_tree_delete(lock_request->tree, lock_request); 
        lt_mutex_unlock(tree);
    }
    lock_request_destroy_wait(lock_request);
    toku_free(lock_request->key_left_copy.data);
    toku_free(lock_request->key_right_copy.data);
}

static void
lock_request_complete(toku_lock_request *lock_request, int complete_r) {
    lock_request->state = LOCK_REQUEST_COMPLETE;
    lock_request->complete_r = complete_r;
}

static const struct timeval max_timeval = { ~0, 0 };

static int
lock_request_wait(toku_lock_request *lock_request, toku_lock_tree *tree, struct timeval *wait_time, bool tree_locked) {
#if TOKU_LT_DEBUG
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
        if (!tree_locked) lt_mutex_lock(tree);
        while (lock_request->state == LOCK_REQUEST_PENDING) {
            lock_request_init_wait(lock_request);
            r = toku_cond_timedwait(&lock_request->wait, &tree->mutex, &ts);
            assert(r == 0 || r == ETIMEDOUT);
            if (r == ETIMEDOUT && lock_request->state == LOCK_REQUEST_PENDING) {
                lock_request_tree_delete(tree, lock_request);
                lock_request_complete(lock_request, DB_LOCK_NOTGRANTED);
            }
        }
        if (!tree_locked) lt_mutex_unlock(tree);
    } else {
        if (!tree_locked) lt_mutex_lock(tree);
        while (lock_request->state == LOCK_REQUEST_PENDING) {
            lock_request_init_wait(lock_request);
            toku_cond_wait(&lock_request->wait, &tree->mutex);
        }
        if (!tree_locked) lt_mutex_unlock(tree);
    }
    assert(lock_request->state == LOCK_REQUEST_COMPLETE);
    return lock_request->complete_r;
}

int
toku_lock_request_wait(toku_lock_request *lock_request, toku_lock_tree *tree, struct timeval *wait_time) {
    return lock_request_wait(lock_request, tree, wait_time, false);
}

int
toku_lock_request_wait_with_default_timeout(toku_lock_request *lock_request, toku_lock_tree *tree) {
    return toku_lock_request_wait(lock_request, tree, &tree->mgr->lock_wait_time);
}

static void
lock_request_wakeup(toku_lock_request *lock_request, toku_lock_tree *tree UU()) {
    if (lock_request->wait_initialized) {
        toku_cond_broadcast(&lock_request->wait);
    }
}

// a lock request tree contains pending lock requests. 
// initialize a lock request tree.
static void 
lock_request_tree_init(toku_lock_tree *tree) {
    int r = toku_omt_create(&tree->lock_requests); assert_zero(r);
}

// destroy a lock request tree.
// the tree must be empty when destroyed.
static void 
lock_request_tree_destroy(toku_lock_tree *tree) {
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

// insert a lock request into the tree.
static void 
lock_request_tree_insert(toku_lock_tree *tree, toku_lock_request *lock_request) {
    lock_request->tree = tree;
    int r;
    OMTVALUE v;
    u_int32_t idx;
    r = toku_omt_find_zero(tree->lock_requests, compare_lock_request, &lock_request->txnid, &v, &idx); assert(r == DB_NOTFOUND);
    r = toku_omt_insert_at(tree->lock_requests, lock_request, idx); assert_zero(r);
}

// delete a lock request from the tree.
static void 
lock_request_tree_delete(toku_lock_tree *tree, toku_lock_request *lock_request) {
    int r;
    OMTVALUE v;
    u_int32_t idx;
    r = toku_omt_find_zero(tree->lock_requests, compare_lock_request, &lock_request->txnid, &v, &idx);
    if (r == 0) {
        r = toku_omt_delete_at(tree->lock_requests, idx); assert_zero(r);
    }
}

// find a lock request for a given transaction id.
static toku_lock_request *
lock_request_tree_find(toku_lock_tree *tree, TXNID id) {
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

#if TOKU_LT_DEBUG
#include <ctype.h>
static void 
print_key(const char *sp, const DBT *k) {
    printf("%s", sp);
    if (k == toku_lt_neg_infinity)
        printf("-inf");
    else if (k == toku_lt_infinity)
        printf("inf");
    else {
        unsigned char *data = (unsigned char *) k->data;
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

static void lt_check_deadlock(toku_lock_tree *tree, toku_lock_request *a_lock_request);

static int 
lock_request_start(toku_lock_request *lock_request, toku_lock_tree *tree, bool copy_keys_if_not_granted, bool do_escalation) { 
    assert(lock_request->state == LOCK_REQUEST_INIT);
    assert(tree);
    toku_mutex_assert_locked(&tree->mutex);
    int r = 0;
    switch (lock_request->type) {
    case LOCK_REQUEST_READ:
        r = lt_acquire_range_read_lock(tree, lock_request->txnid, lock_request->key_left, lock_request->key_right, do_escalation);
        break;
    case LOCK_REQUEST_WRITE:
        r = lt_acquire_range_write_lock(tree, lock_request->txnid, lock_request->key_left, lock_request->key_right, do_escalation);
        break;
    case LOCK_REQUEST_UNKNOWN:
        assert(0);
    }
#if TOKU_LT_DEBUG
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
        lock_request_tree_insert(tree, lock_request);

        // check for deadlock
        lt_check_deadlock(tree, lock_request);
        if (lock_request->state == LOCK_REQUEST_COMPLETE)
            r = lock_request->complete_r;
    } else 
        lock_request_complete(lock_request, r);

    return r;
}

int 
toku_lock_request_start(toku_lock_request *lock_request, toku_lock_tree *tree, bool copy_keys_if_not_granted) {
    lt_mutex_lock(tree);
    int r = lock_request_start(lock_request, tree, copy_keys_if_not_granted, true);
    lt_mutex_unlock(tree);
    return r;
}

static int 
lt_acquire_lock_request_with_timeout_locked(toku_lock_tree *tree, toku_lock_request *lock_request, struct timeval *wait_time) {
    int r = lock_request_start(lock_request, tree, false, true);
    if (r == DB_LOCK_NOTGRANTED)
        r = lock_request_wait(lock_request, tree, wait_time, true);
    return r;
}

int 
toku_lt_acquire_lock_request_with_timeout(toku_lock_tree *tree, toku_lock_request *lock_request, struct timeval *wait_time) {
    lt_mutex_lock(tree);
    int r = lt_acquire_lock_request_with_timeout_locked(tree, lock_request, wait_time);
    lt_mutex_unlock(tree);
    return r;
}

int 
toku_lt_acquire_lock_request_with_default_timeout(toku_lock_tree *tree, toku_lock_request *lock_request) {
    int r = toku_lt_acquire_lock_request_with_timeout(tree, lock_request, &tree->mgr->lock_wait_time);
    return r;
}

static void 
lt_retry_lock_requests(toku_lock_tree *tree) {
    assert(tree);
    toku_mutex_assert_locked(&tree->mutex);

    for (uint32_t i = 0; i < toku_omt_size(tree->lock_requests); ) {
        int r;
        OMTVALUE v;
        r = toku_omt_fetch(tree->lock_requests, i, &v); assert_zero(r);
        toku_lock_request *lock_request = (toku_lock_request *) v;
        assert(lock_request->state == LOCK_REQUEST_PENDING);
        lock_request->state = LOCK_REQUEST_INIT;
        toku_omt_delete_at(tree->lock_requests, i);
        r = lock_request_start(lock_request, tree, false, false);
        if (lock_request->state == LOCK_REQUEST_COMPLETE) {
            lock_request_wakeup(lock_request, tree);
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
        toku_lock_request *b_lock_request = lock_request_tree_find(tree, b);
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
static void 
lt_check_deadlock(toku_lock_tree *tree, toku_lock_request *a_lock_request) {
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
        lock_request_complete(a_lock_request, DB_LOCK_DEADLOCK);
        lock_request_tree_delete(tree, a_lock_request);
        lock_request_wakeup(a_lock_request, tree);
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
            // All ranges in a self_read tree have the same txn
            int r = toku_rt_find(forest->self_read, query, 1, range_ptr, n_expected_ranges_ptr, &numfound);
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
        bool false_positive = false;
        if (numfound == 1 && interval_strictly_internal(&query, &ranges[0].ends)) {
            toku_range_tree* peer_selfwrite = toku_lt_ifexist_selfwrite(tree, ranges[0].data);
            if (!peer_selfwrite) {
                r = lt_panic(tree, TOKU_LT_INCONSISTENT);
                goto cleanup;
            }

            bool met;
            r = lt_meets(tree, &query, peer_selfwrite, &met);
            if (r != 0)   
                goto cleanup;
            false_positive = !met;
        }
        if (!false_positive) {
            for (uint32_t i = 0; i < numfound; i++) 
                if (ranges[i].data != lock_request->txnid)
                    txnid_set_add(conflicts, ranges[i].data);
        }
    }

cleanup:
    if (ranges) 
        toku_free(ranges);

    return r;
}

void 
toku_ltm_set_lock_wait_time(toku_ltm *mgr, uint64_t lock_wait_time_msec) {
    if (lock_wait_time_msec == UINT64_MAX)
        mgr->lock_wait_time = max_timeval;
    else
        mgr->lock_wait_time = (struct timeval) { lock_wait_time_msec / 1000, (lock_wait_time_msec % 1000) * 1000 };
}

void 
toku_ltm_get_lock_wait_time(toku_ltm *mgr, uint64_t *lock_wait_time_msec) {
    if (mgr->lock_wait_time.tv_sec == max_timeval.tv_sec && mgr->lock_wait_time.tv_usec == max_timeval.tv_usec)
        *lock_wait_time_msec = UINT64_MAX;
    else
        *lock_wait_time_msec = mgr->lock_wait_time.tv_sec * 1000 + mgr->lock_wait_time.tv_usec / 1000;
}

static void 
verify_range_in_borderwrite(toku_lock_tree *tree, toku_interval *query, TXNID txnid) {
    int r;

    uint32_t numfound;
    r = toku_rt_find(tree->borderwrite, query, 0, &tree->verify_buf, &tree->verify_buflen, &numfound);
    assert(r == 0);
    assert(numfound == 1);

    toku_range *range = &tree->verify_buf[0];
    assert(range->data == txnid);
    assert(interval_dominated(query, &range->ends));
}

struct verify_extra {
    toku_lock_tree *lt;
    TXNID id;
};

static int
verify_range_in_borderwrite_cb(toku_range *range, void *extra) {
    struct verify_extra *vextra = (struct verify_extra *) extra;
    verify_range_in_borderwrite(vextra->lt, &range->ends, vextra->id);
    return 0;
}

static void
verify_all_ranges_in_borderwrite(toku_lock_tree *lt, toku_range_tree *rt, TXNID id) {
    struct verify_extra vextra = { .lt = lt, .id = id };
    toku_rt_iterate(rt, verify_range_in_borderwrite_cb, &vextra);
}

static void
verify_all_selfwrite_ranges_in_borderwrite(toku_lock_tree *lt) {
    toku_rth_start_scan(lt->rth);
    rt_forest *forest;
    while ((forest = toku_rth_next(lt->rth))) {
	if (forest->self_write)
	    verify_all_ranges_in_borderwrite(lt, forest->self_write, forest->hash_key);
    }
}

static void 
lt_verify(toku_lock_tree *lt) {    
    // verify all of the selfread and selfwrite trees
    toku_rth_start_scan(lt->rth);
    rt_forest *forest;
    while ((forest = toku_rth_next(lt->rth)) != NULL) {
        if (forest->self_read)
            toku_rt_verify(forest->self_read);
        if (forest->self_write)
            toku_rt_verify(forest->self_write);
    }

    // verify the borderwrite tree
    toku_rt_verify(lt->borderwrite);

    // verify that the ranges in the selfwrite trees are in the borderwrite tree
    verify_all_selfwrite_ranges_in_borderwrite(lt);
}

void 
toku_lt_verify(toku_lock_tree *lt) {
    lt_mutex_lock(lt);
    lt_verify(lt);
    lt_mutex_unlock(lt);
}

#undef STATUS_VALUE
