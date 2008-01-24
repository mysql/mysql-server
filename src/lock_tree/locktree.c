/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <locktree.h>
#include <ydb-internal.h>
#include <brt-internal.h>
#include <stdlib.h>

static int __toku_lt_panic(toku_lock_tree *tree, int r) {
    tree->panicked = TRUE;
    return r;
}
                
DBT __toku_lt_infinity;
DBT __toku_lt_neg_infinity;

const DBT* toku_lt_infinity     = &__toku_lt_infinity;
const DBT* toku_lt_neg_infinity = &__toku_lt_neg_infinity;

static int __toku_infinity_compare(void* a, void* b) {
    if      (a == b)                                                 return  0;
    else if (a == toku_lt_infinity     && b != toku_lt_infinity)     return  1;
    else if (b == toku_lt_infinity     && a != toku_lt_infinity)     return -1;
    else if (a == toku_lt_neg_infinity && b != toku_lt_neg_infinity) return -1;
    else if (b == toku_lt_neg_infinity && a != toku_lt_neg_infinity) return  1;
    else                                                             return  0;
}

static BOOL __toku_lt_is_infinity(void* p) {
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

    partial_result = __toku_infinity_compare(x->key_payload, y->key_payload);
    if (partial_result) return partial_result;
        
    partial_result = x->lt->db->i->brt->compare_fun(x->lt->db,
                     __toku_recreate_DBT(&point_1, x->key_payload, x->key_len),
                     __toku_recreate_DBT(&point_2, y->key_payload, y->key_len));
    if (partial_result) return partial_result;
    
    if (!x->lt->duplicates) return 0;

    partial_result = __toku_infinity_compare(x->data_payload, y->data_payload);
    if (partial_result) return partial_result;
    
    return x->lt->db->i->brt->dup_compare(x->lt->db,
                   __toku_recreate_DBT(&point_1, x->data_payload, x->data_len),
                   __toku_recreate_DBT(&point_2, y->data_payload, y->data_len));
}

static int __toku_p_free(toku_point* point) {
    assert(point);
    toku_free(point);
    return 0;
}

/*
   Allocate and copy the payload.
*/
static void __toku_payload_copy(void** payload_out, u_int32_t* len_out,
                               void*  payload_in,  u_int32_t  len_in,
                               void*  free_memory) {
    assert(payload_out && len_out);
    if (__toku_lt_is_infinity(payload_in)) {
        assert(!len_in);
        *payload_out = payload_in;
        *len_out     = 0;
    }
    else if (!len_in) {
        *payload_out = NULL;
        *len_out     = 0;
    }
    else {
        *payload_out = free_memory;
        *len_out     = len_in;
        memcpy(payload_out, payload_in, len_in);
    }
}

//TODO: Do the void*'s need to be aligned at all?
//      If alignment is necessary, this code needs updating a bit.
static int __toku_p_makecopy(void** ppoint) {
    assert(ppoint);
    toku_point*     point      = *(toku_point**)ppoint;
    toku_point*     temp_point = NULL;
    size_t          to_alloc   = 0;
    
    to_alloc += sizeof(toku_point);
    to_alloc += point->key_len;
    to_alloc += point->data_len;
 
    temp_point = (toku_point*)toku_malloc(to_alloc);
    if (!temp_point) return errno;
    memcpy(temp_point, point, sizeof(toku_point));
    
    /* Using char* for pointer arithmetic. */
    char* next = (char*)temp_point;
    next += sizeof(toku_point);
    __toku_payload_copy(&temp_point->key_payload, &temp_point->key_len,
                              point->key_payload,       point->key_len,
                              next);
    next += point->key_len;
    __toku_payload_copy(&temp_point->data_payload, &temp_point->data_len,
                              point->data_payload,       point->data_len,
                              next);
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
    Determines whether 'query' meets 'rt' and if so says AT what data..
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

static void __toku_payload_from_dbt(void** payload, u_int32_t* len, DBT* dbt) {
    assert(payload && len && dbt);
    if (__toku_lt_is_infinity(dbt)) {
        *payload = dbt;
        *len     = 0;
    }
    else {
        *len     = dbt->size;
        *payload = (*len == 0) ? NULL : dbt->data;
    }
}

static void __toku_init_point(toku_point* point, toku_lock_tree* tree,
                              DBT* key, DBT* data) {
    point->lt          = tree;
    
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

static BOOL __toku_db_is_dupsort(DB* db) {
    unsigned int brtflags;
    toku_brt_get_flags(db->i->brt, &brtflags);
    return (brtflags & TOKU_DB_DUPSORT) != 0;
}

int toku_lt_create(toku_lock_tree** ptree, DB* db) {
    if (!ptree || !db)                                      return EINVAL;
    int r;

    toku_lock_tree* temp_tree = (toku_lock_tree*)toku_malloc(sizeof(*temp_tree));
    if (0) {
        died1:
        free(temp_tree);
        return r;
    }
    if (!temp_tree) return errno;
    memset(temp_tree, 0, sizeof(*temp_tree));
    temp_tree->db         = db;
    temp_tree->duplicates = __toku_db_is_dupsort(db);
    r = toku_rt_create(&temp_tree->mainread,
                       __toku_lt_point_cmp, __toku_lt_txn_cmp, TRUE,
                       toku_malloc, toku_free, toku_realloc);
    if (0) {
        died2:
        toku_rt_close(temp_tree->mainread);
        goto died1;
    }
    if (r!=0) goto died1;
    r = toku_rt_create(&temp_tree->borderwrite,
                       __toku_lt_point_cmp, __toku_lt_txn_cmp, FALSE,
                       toku_malloc, toku_free, toku_realloc);
    if (0) {
        died3:
        toku_rt_close(temp_tree->borderwrite);
        goto died2;
    }
    if (r!=0) goto died2;
//TODO: Remove this, and use multiples per transaction
r = toku_rt_create(&temp_tree->selfwrite,
                   __toku_lt_point_cmp, __toku_lt_txn_cmp, FALSE,
                   toku_malloc, toku_free, toku_realloc);
assert(temp_tree->selfwrite);
//TODO: Remove this, and use multiples per transaction
r = toku_rt_create(&temp_tree->selfread,
                   __toku_lt_point_cmp, __toku_lt_txn_cmp, TRUE,
                   toku_malloc, toku_free, toku_realloc);
assert(temp_tree->selfread);

    temp_tree->buflen = __toku_default_buflen;
    /* Using malloc here because range trees do not use toku_malloc/free. */
    temp_tree->buf    = (toku_range*)
                        malloc(temp_tree->buflen * sizeof(toku_range));
    if (!temp_tree->buf) {
        r = errno;
        goto died3;
    }
    //TODO: Create list of selfreads
    //TODO: Create list of selfwrites
    assert(FALSE);  //Not implemented yet.
}

int toku_lt_close(toku_lock_tree* tree) {
    if (!tree)                                              return EINVAL;
    //TODO: Free all memory held by things inside of trees!
    //TODO: Close mainread, borderwrite, all selfreads, all selfwrites,
    //TODO: remove selfreads and selfwrites from txns.
    //TODO: Free buf;
    assert(FALSE);  //Not implemented yet.
}

int toku_lt_acquire_read_lock(toku_lock_tree* tree, DB_TXN* txn,
                              DBT* key, DBT* data) {
    return toku_lt_acquire_range_read_lock(tree, txn, key, data, key, data);
}

int toku_lt_acquire_range_read_lock(toku_lock_tree* tree, DB_TXN* txn,
                                    DBT* key_left,  DBT* data_left,
                                    DBT* key_right, DBT* data_right) {
    if (!tree || !txn || !key_left || !key_right)           return EINVAL;
    if (!tree->duplicates && ( data_left ||  data_right))   return EINVAL;
    if (tree->duplicates  && (!data_left || !data_right))   return EINVAL;
    if (tree->duplicates  && key_left != data_left &&
        __toku_lt_is_infinity(key_left))                    return EINVAL;
    if (tree->duplicates  && key_right != data_right &&
        __toku_lt_is_infinity(key_right))                   return EINVAL;
    assert(FALSE);  //Not implemented yet.

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
    BOOL        alloc_left  = TRUE;
    BOOL        alloc_right = TRUE;
    BOOL        copy_left   = FALSE;
    toku_range  to_insert;
    __toku_init_insert(&to_insert, &left, &right, txn);

    toku_range_tree* mainread = tree->mainread;
    assert(mainread);
    toku_range_tree* selfread = __toku_lt_ifexist_selfread(tree, txn);
    if (selfread) {
        unsigned numfound;
        r = toku_rt_find(selfread, &query, 0, &tree->buf, &tree->buflen,
                         &numfound);
        if (r!=0) return r;
        
        /* Consolidate the new range and all the overlapping ranges */
        
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
        unsigned i;
        //TODO: If we panic, is it alright to have a memory leak here?
        //      What we should have, is some flags in 'lock_tree'
        //      to tell it that the buffer is the only way to access
        //      some things, and so it can delete it later?
        for (i = 0; i < numfound; i++) {
            /* Delete overlapping ranges from selfread ... */
            r = toku_rt_delete(selfread, &(tree->buf[i]));
            if (r!=0) return __toku_lt_panic(tree,r);
            /* ... and mainread.
               Growth direction: if we had no overlaps, the next two lines
               should be commented out */
            r = toku_rt_delete(mainread, &(tree->buf[i]));
            if (r!=0) return __toku_lt_panic(tree,r);
        }

        for (i = 0; i < numfound; i++) {
            /* Find the extreme left end-point among overlapping ranges */
            if (__toku_lt_point_cmp(tree->buf[i].left,to_insert.left) 
                <= 0) {
                assert(tree->buf[i].left != to_insert.left);
                assert(tree->buf[i].left != to_insert.right);
                alloc_left      = FALSE;
                to_insert.left  = tree->buf[i].left;
            }
            /* Find the extreme right end-point */
            if (__toku_lt_point_cmp(tree->buf[i].right,to_insert.right)
                >= 0) {
                assert(tree->buf[i].right != to_insert.left ||
                       (tree->buf[i].left == to_insert.left &&
                        tree->buf[i].left == tree->buf[i].right));
                assert(tree->buf[i].right != to_insert.right);
                alloc_right     = FALSE;
                to_insert.right = tree->buf[i].right;
            }
        }

        for (i = 0; i < numfound; i++) {
            /*
               We will maintain the invariant: (separately for read and write
               environments)
               (__toku_lt_point_cmp(a, b) == 0 && a.txn == b.txn) => a == b
            */
            /* Do not double-free. */
            if (tree->buf[i].right != tree->buf[i].left &&
                tree->buf[i].right != to_insert.left &&
                tree->buf[i].right != to_insert.right) {
                __toku_p_free(tree->buf[i].right);
            }
            if (tree->buf[i].left != to_insert.left &&
                tree->buf[i].left != to_insert.right) {
                __toku_p_free(tree->buf[i].left);
            }
        }
    }
    
    if (alloc_left && alloc_right && __toku_lt_point_cmp(&left, &right) == 0) {
        alloc_right = FALSE;
        copy_left   = TRUE;
    }
    if (alloc_left) {
        r = __toku_p_makecopy(&to_insert.left);
        if (0) {
            died1:
            if (alloc_left) __toku_p_free(to_insert.left);
            return __toku_lt_panic(tree, r);
        }
        if (r!=0) return __toku_lt_panic(tree,r);
    }
    if (alloc_right) {
        assert(!copy_left);
        r = __toku_p_makecopy(&to_insert.right);
        if (0) {
            died2:
            if (alloc_right) __toku_p_free(to_insert.right);
            return __toku_lt_panic(tree, r);
        }
        if (r!=0) goto died1;
    }
    else if (copy_left) to_insert.right = to_insert.left;
    if (!selfread) {
        r = __toku_lt_selfread(tree, txn, &selfread);
        if (r!=0) return __toku_lt_panic(tree, r);
        assert(selfread);
    }
    
    r = toku_rt_insert(selfread, &to_insert);
    if (r!=0) goto died2;
    assert(tree->mainread);
    r = toku_rt_insert(tree->mainread, &to_insert);
    if (r!=0) goto died2;
    return 0;
}

int toku_lt_acquire_write_lock(toku_lock_tree* tree, DB_TXN* txn,
                               DBT* key, DBT* data) {
    if (!tree || !txn || !key)                              return EINVAL;
    if (!tree->duplicates && data)                          return EINVAL;
    if (tree->duplicates && !data)                          return EINVAL;
    if (tree->duplicates && key != data &&
        __toku_lt_is_infinity(key))                         return EINVAL;
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
    r = __toku_p_makecopy(&to_insert.left);
    if (0) {
        died1:
        __toku_p_free(to_insert.left);
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
                                     DBT* key_left,  DBT* data_left,
                                     DBT* key_right, DBT* data_right) {
    if (!tree || !txn || !key_left || !key_right)           return EINVAL;
    if (!tree->duplicates && ( data_left ||  data_right))   return EINVAL;
    if (tree->duplicates  && (!data_left || !data_right))   return EINVAL;
    if (tree->duplicates  && key_left != data_left &&
        __toku_lt_is_infinity(key_left))                    return EINVAL;
    if (tree->duplicates  && key_right != data_right &&
        __toku_lt_is_infinity(key_right))                   return EINVAL;
    assert(FALSE);
    //We are not ready for this.
    //Not needed for Feb 1 release.
}

int toku_lt_unlock(toku_lock_tree* tree, DB_TXN* txn);
