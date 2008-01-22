/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <locktree.h>
#include <ydb-internal.h>
#include <brt-internal.h>
#include <stdlib.h>

DBT __toku_lt_infinity;
DBT __toku_lt_neg_infinity;

DBT* toku_lt_infinity     = &__toku_lt_infinity;
DBT* toku_lt_neg_infinity = &__toku_lt_neg_infinity;

static int __toku_infinity_compare(void* a, void* b) {
    if      (a == toku_lt_infinity     && b != toku_lt_infinity)     return  1;
    else if (b == toku_lt_infinity     && a != toku_lt_infinity)     return -1;
    else if (a == toku_lt_neg_infinity && b != toku_lt_neg_infinity) return -1;
    else if (b == toku_lt_neg_infinity && a != toku_lt_neg_infinity) return  1;
    else                                                             return  0;
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

/* Provides access to a selfwrite tree for a particular transaction.
   Returns NULL if it does not exist yet. */
static toku_range_tree* __toku_lt_ifexist_selfwrite(toku_lock_tree* tree,
                                                    DB_TXN* txn) {
    assert(tree && txn);
    assert(FALSE); //Not Implemented.
}

/* Provides access to a selfread tree for a particular transaction.
   Returns NULL if it does not exist yet. */
static toku_range_tree* __toku_lt_ifexist_selfread(toku_lock_tree* tree,
                                                   DB_TXN* txn) {
    assert(tree && txn);
    assert(FALSE); //Not Implemented.
}

/* Provides access to a selfwrite tree for a particular transaction.
   Creates it if it does not exist. */
static int __toku_lt_selfwrite(toku_lock_tree* tree, DB_TXN* txn,
                               toku_range_tree** pselfwrite) {
    assert(tree && txn && pselfwrite);
    assert(FALSE); //Not Implemented.
}

/* Provides access to a selfread tree for a particular transaction.
   Creates it if it does not exist. */
static int __toku_lt_selfread(toku_lock_tree* tree, DB_TXN* txn,
                              toku_range_tree** pselfread) {
    assert(tree && txn && pselfread);
    assert(FALSE); //Not Implemented.
}

static int __toku_lt_dominated(toku_lock_tree* tree, toku_range* query,
                               toku_range_tree* rt, BOOL* dominated) {
    assert(tree && query && rt && dominated);
    toku_range  buffer[1];
    toku_range* buf = &buffer[0];
    unsigned    buflen = sizeof(buf) / sizeof(buf[0]);
    unsigned    numfound;
    int         r;

    r = toku_rt_find(rt, query, 1, &buf, &buflen, &numfound);
    if (r!=0) return r;
    if (numfound == 0) {
        *dominated = FALSE;
        return 0;
    }
    *dominated = (__toku_lt_point_cmp(query->left, buf[0].right) <= 0 &&
                  __toku_lt_point_cmp(buf[0].left, query->right) <= 0);
    return 0;
}

static int __toku_lt_met_at_peer(toku_lock_tree* tree, DB_TXN* self,
                                 toku_range* query,
                                 toku_range_tree* rt,  DB_TXN** peer) {
    assert(tree && query && rt && peer);
    toku_range  buffer[2];
    toku_range* buf = &buffer[0];
    unsigned    buflen = sizeof(buf) / sizeof(buf[0]);
    unsigned    numfound;
    unsigned    i;
    int         r;

    r = toku_rt_find(rt, query, 2, &buf, &buflen, &numfound);
    if (r!=0) return r;
    for (i = 0; i < numfound; i++) {
        if (buf[i].data == self) continue;
        *peer = buf[i].data;
        return 0;
    }
    *peer = NULL;
    return 0;
}

static void __toku_init_point(toku_point* point, toku_lock_tree* tree,
                              DBT* key, DBT* data) {
    point->lt          = tree;
    point->key_payload = key->data;
    point->key_len     = key->size;
    if (tree->duplicates) {
        assert(data);
        point->data_payload = data->data;
        point->data_len     = data->size;
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

    toku_lock_tree* temp_tree = (toku_lock_tree*)malloc(sizeof(*temp_tree));
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
                       __toku_lt_point_cmp, __toku_lt_txn_cmp, TRUE);
    if (0) {
        died2:
        toku_rt_close(temp_tree->mainread);
        goto died1;
    }
    if (r!=0) goto died1;
    r = toku_rt_create(&temp_tree->borderwrite,
                       __toku_lt_point_cmp, __toku_lt_txn_cmp, FALSE);
    if (0) {
        died3:
        toku_rt_close(temp_tree->borderwrite);
        goto died2;
    }
    if (r!=0) goto died2;
    temp_tree->buflen = __toku_default_buflen;
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
    if (!tree || !txn || !key)                              return EINVAL;
    if (!tree->duplicates && data)                          return EINVAL;
    if (tree->duplicates && !data)                          return EINVAL;
    if (tree->duplicates && key != data &&
                           (key == toku_lt_infinity ||
                            key == toku_lt_neg_infinity))   return EINVAL;
    return toku_lt_acquire_range_read_lock(tree, txn, key, data, key, data);
}

int toku_lt_acquire_range_read_lock(toku_lock_tree* tree, DB_TXN* txn,
                                    DBT* key_left,  DBT* data_left,
                                    DBT* key_right, DBT* data_right) {
    if (!tree || !txn || !key_left || !key_right)           return EINVAL;
    if (!tree->duplicates && ( data_left ||  data_right))   return EINVAL;
    if (tree->duplicates  && (!data_left || !data_right))   return EINVAL;
    if (tree->duplicates  && key_left != data_left &&
        (key_left == toku_lt_infinity ||
         key_left == toku_lt_neg_infinity))                 return EINVAL;
    if (tree->duplicates  && key_right != data_right &&
        (key_right == toku_lt_infinity ||
         key_right == toku_lt_neg_infinity))                return EINVAL;
    assert(FALSE);  //Not implemented yet.

    int r;
    toku_point left;
    toku_point right;
    toku_range query;
    BOOL dominated;
    toku_range_tree* selfwrite;
    toku_range_tree* selfread;
    toku_range_tree* borderwrite;
    toku_range_tree* peer_selfwrite;
    DB_TXN* peer;
    
    __toku_init_point(&left,   tree,  key_left,  data_left);
    __toku_init_point(&right,  tree,  key_right, data_right);
    __toku_init_query(&query, &left, &right);
    
    selfwrite = __toku_lt_ifexist_selfwrite(tree, txn);
    if (selfwrite) {
        r = __toku_lt_dominated(tree, &query, selfwrite, &dominated);
        if (r!=0) return r;
        if (dominated) return 0;
    }
    selfread = __toku_lt_ifexist_selfread(tree, txn);
    if (selfread) {
        r = __toku_lt_dominated(tree, &query, selfread, &dominated);
        if (r!=0) return r;
        if (dominated) return 0;
    }
    borderwrite = tree->borderwrite;
    if (borderwrite) {
        r = __toku_lt_met_at_peer(tree, txn, &query, borderwrite, &peer);
        if (r!=0) return r;
        if (peer != NULL) {
            peer_selfwrite = __toku_lt_ifexist_selfwrite(tree, peer);
            assert(peer_selfwrite);
            r = __toku_lt_met_at_peer(tree, txn, &query, peer_selfwrite, &peer);
            if (r!=0)         return r;
            if (peer != NULL) return DB_LOCK_NOTGRANTED;
        }
    }
    /* Now need to merge, copy the memory and insert. */

    BOOL alloc_left  = TRUE;
    BOOL alloc_right = TRUE;
    toku_range to_insert;
    __toku_init_insert(&to_insert, &left, &right, txn);
    if (selfread) {
        //TODO: Find all that overlap in here.
        //TODO: extend range to that, delete from selfread and mainread
        //TODO: If left (or right) is extended/equal, copy the pointer
        //      and unset alloc_left (or right).
    }
    if (alloc_left && alloc_right && __toku_lt_point_cmp(&left, &right) == 0) {
        alloc_right = FALSE;
    }
    if (alloc_left) {
        r = __toku_p_makecopy(&left);
        assert(r==0); //TODO: Error Handling instead of assert
    }
    if (alloc_right) {
        r = __toku_p_makecopy(&right);
        assert(r==0); //TODO: Error Handling instead of assert
    }
    if (!selfread) {
        r = __toku_lt_selfread(tree, txn, &selfread);
        assert(r==0); //TODO: Error Handling instead of assert
        assert(selfread);
    }
    
    r = toku_rt_insert(selfread, &to_insert);
    assert(r==0); //TODO: Error Handling instead of assert
    assert(tree->mainread);
    r = toku_rt_insert(tree->mainread, &to_insert);
    assert(r==0); //TODO: Error Handling instead of assert
    assert(FALSE);  //Not implemented yet.
}

int toku_lt_acquire_write_lock(toku_lock_tree* tree, DB_TXN* txn,
                               DBT* key, DBT* data) {
    if (!tree || !txn || !key)                              return EINVAL;
    if (!tree->duplicates && data)                          return EINVAL;
    if (tree->duplicates && !data)                          return EINVAL;
    if (tree->duplicates && key != data &&
                           (key == toku_lt_infinity ||
                            key == toku_lt_neg_infinity))   return EINVAL;
    int r;
    toku_point left;
    toku_range query;
    BOOL dominated;
    toku_range_tree* selfwrite;
    toku_range_tree* mainread;
    toku_range_tree* borderwrite;
    toku_range_tree* peer_selfwrite;
    DB_TXN*          peer;
    
    __toku_init_point(&left,   tree,  key, data);
    __toku_init_query(&query, &left, &left);
    
    selfwrite = __toku_lt_ifexist_selfwrite(tree, txn);
    if (selfwrite) {
        r = __toku_lt_dominated(tree, &query, selfwrite, &dominated);
        if (r!=0) return r;
        if (dominated) return 0;
    }
    mainread = tree->mainread;
    if (mainread) {
        r = __toku_lt_met_at_peer(tree, txn, &query, mainread, &peer);
        if (r!=0)       return r;
        if (peer!=NULL) return DB_LOCK_NOTGRANTED;
    }
    borderwrite = tree->borderwrite;
    if (borderwrite) {
        r = __toku_lt_met_at_peer(tree, txn, &query, borderwrite, &peer);
        if (r!=0) return r;
        if (peer != NULL) {
            peer_selfwrite = __toku_lt_ifexist_selfwrite(tree, peer);
            assert(peer_selfwrite);
            r = __toku_lt_met_at_peer(tree, txn, &query, peer_selfwrite, &peer);
            if (r!=0)         return r;
            if (peer != NULL) return DB_LOCK_NOTGRANTED;
        }
    }
    /* Now need to copy the memory and insert. */
    assert(FALSE);  //Not implemented yet.
}

int toku_lt_acquire_range_write_lock(toku_lock_tree* tree, DB_TXN* txn,
                                     DBT* key_left,  DBT* data_left,
                                     DBT* key_right, DBT* data_right) {
    if (!tree || !txn || !key_left || !key_right)           return EINVAL;
    if (!tree->duplicates && ( data_left ||  data_right))   return EINVAL;
    if (tree->duplicates  && (!data_left || !data_right))   return EINVAL;
    if (tree->duplicates  && key_left != data_left &&
        (key_left == toku_lt_infinity ||
         key_left == toku_lt_neg_infinity))                 return EINVAL;
    if (tree->duplicates  && key_right != data_right &&
        (key_right == toku_lt_infinity ||
         key_right == toku_lt_neg_infinity))                return EINVAL;
    assert(FALSE);
    //We are not ready for this.
    //Not needed for Feb 1 release.
}

int toku_lt_unlock(toku_lock_tree* tree, DB_TXN* txn);
