/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2014 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#include <config.h>

#include "ft/ft-internal.h"

#include "ft/cursor.h"
#include "ft/leafentry.h"
#include "ft/txn/txn.h"
#include "util/dbt.h"

int toku_ft_cursor_create(FT_HANDLE ft_handle, FT_CURSOR cursor, TOKUTXN ttxn,
                          bool is_snapshot_read,
                          bool disable_prefetching,
                          bool is_temporary) {
    if (is_snapshot_read) {
        invariant(ttxn != NULL);
        int accepted = toku_txn_reads_txnid(ft_handle->ft->h->root_xid_that_created, ttxn);
        if (accepted != TOKUDB_ACCEPT) {
            invariant(accepted == 0);
            return TOKUDB_MVCC_DICTIONARY_TOO_NEW;
        }
    }

    memset(cursor, 0, sizeof(*cursor));
    cursor->ft_handle = ft_handle;
    cursor->ttxn = ttxn;
    cursor->is_snapshot_read = is_snapshot_read;
    cursor->disable_prefetching = disable_prefetching;
    cursor->is_temporary = is_temporary;
    return 0;
}

void toku_ft_cursor_destroy(FT_CURSOR cursor) {
    toku_destroy_dbt(&cursor->key);
    toku_destroy_dbt(&cursor->val);
    toku_destroy_dbt(&cursor->range_lock_left_key);
    toku_destroy_dbt(&cursor->range_lock_right_key);
}

// deprecated, should only be used by tests
int toku_ft_cursor(FT_HANDLE ft_handle, FT_CURSOR *cursorptr, TOKUTXN ttxn,
                   bool is_snapshot_read, bool disable_prefetching) {
    FT_CURSOR XCALLOC(cursor);
    int r = toku_ft_cursor_create(ft_handle, cursor, ttxn, is_snapshot_read, disable_prefetching, false);
    if (r == 0) {
        *cursorptr = cursor;
    } else {
        toku_free(cursor);
    }
    return r;
}

// deprecated, should only be used by tests
void toku_ft_cursor_close(FT_CURSOR cursor) {
    toku_ft_cursor_destroy(cursor);
    toku_free(cursor);
}

void toku_ft_cursor_remove_restriction(FT_CURSOR cursor) {
    cursor->out_of_range_error = 0;
    cursor->direction = 0;
}

void toku_ft_cursor_set_check_interrupt_cb(FT_CURSOR cursor, FT_CHECK_INTERRUPT_CALLBACK cb, void *extra) {
    cursor->interrupt_cb = cb;
    cursor->interrupt_cb_extra = extra;
}

void toku_ft_cursor_set_leaf_mode(FT_CURSOR cursor) {
    cursor->is_leaf_mode = true;
}

int toku_ft_cursor_is_leaf_mode(FT_CURSOR cursor) {
    return cursor->is_leaf_mode;
}

// TODO: Rename / cleanup - this has nothing to do with locking
void toku_ft_cursor_set_range_lock(FT_CURSOR cursor,
                                   const DBT *left, const DBT *right,
                                   bool left_is_neg_infty, bool right_is_pos_infty,
                                   int out_of_range_error) {
    // Destroy any existing keys and then clone the given left, right keys
    toku_destroy_dbt(&cursor->range_lock_left_key);
    if (left_is_neg_infty) {
        cursor->left_is_neg_infty = true;
    } else {
        toku_clone_dbt(&cursor->range_lock_left_key, *left);
    }

    toku_destroy_dbt(&cursor->range_lock_right_key);
    if (right_is_pos_infty) {
        cursor->right_is_pos_infty = true;
    } else {
        toku_clone_dbt(&cursor->range_lock_right_key, *right);
    }

    // TOKUDB_FOUND_BUT_REJECTED is a DB_NOTFOUND with instructions to stop looking. (Faster)
    cursor->out_of_range_error = out_of_range_error == DB_NOTFOUND ? TOKUDB_FOUND_BUT_REJECTED : out_of_range_error;
    cursor->direction = 0;
}

void toku_ft_cursor_set_prefetching(FT_CURSOR cursor) {
    cursor->prefetching = true;
}

bool toku_ft_cursor_prefetching(FT_CURSOR cursor) {
    return cursor->prefetching;
}

//Return true if cursor is uninitialized.  false otherwise.
bool toku_ft_cursor_not_set(FT_CURSOR cursor) {
    assert((cursor->key.data==NULL) == (cursor->val.data==NULL));
    return (bool)(cursor->key.data == NULL);
}

struct ft_cursor_search_struct {
    FT_GET_CALLBACK_FUNCTION getf;
    void *getf_v;
    FT_CURSOR cursor;
    ft_search *search;
};

/* search for the first kv pair that matches the search object */
static int ft_cursor_search(FT_CURSOR cursor, ft_search *search,
                            FT_GET_CALLBACK_FUNCTION getf, void *getf_v, bool can_bulk_fetch) {
    int r = toku_ft_search(cursor->ft_handle, search, getf, getf_v, cursor, can_bulk_fetch);
    return r;
}

static inline int compare_k_x(FT_HANDLE ft_handle, const DBT *k, const DBT *x) {
    return ft_handle->ft->cmp(k, x);
}

int toku_ft_cursor_compare_one(const ft_search &UU(search), const DBT *UU(x)) {
    return 1;
}

static int ft_cursor_compare_set(const ft_search &search, const DBT *x) {
    FT_HANDLE CAST_FROM_VOIDP(ft_handle, search.context);
    return compare_k_x(ft_handle, search.k, x) <= 0; /* return min xy: kv <= xy */
}

static int
ft_cursor_current_getf(uint32_t keylen,                 const void *key,
                        uint32_t vallen,                 const void *val,
                        void *v, bool lock_only) {
    struct ft_cursor_search_struct *CAST_FROM_VOIDP(bcss, v);
    int r;
    if (key==NULL) {
        r = bcss->getf(0, NULL, 0, NULL, bcss->getf_v, lock_only);
    } else {
        FT_CURSOR cursor = bcss->cursor;
        DBT newkey;
        toku_fill_dbt(&newkey, key, keylen);
        if (compare_k_x(cursor->ft_handle, &cursor->key, &newkey) != 0) {
            r = bcss->getf(0, NULL, 0, NULL, bcss->getf_v, lock_only); // This was once DB_KEYEMPTY
            if (r==0) r = TOKUDB_FOUND_BUT_REJECTED;
        }
        else
            r = bcss->getf(keylen, key, vallen, val, bcss->getf_v, lock_only);
    }
    return r;
}

static int ft_cursor_compare_next(const ft_search &search, const DBT *x) {
    FT_HANDLE CAST_FROM_VOIDP(ft_handle, search.context);
    return compare_k_x(ft_handle, search.k, x) < 0; /* return min xy: kv < xy */
}

int toku_ft_cursor_current(FT_CURSOR cursor, int op, FT_GET_CALLBACK_FUNCTION getf, void *getf_v) {
    if (toku_ft_cursor_not_set(cursor)) {
        return EINVAL;
    }
    cursor->direction = 0;
    if (op == DB_CURRENT) {
        struct ft_cursor_search_struct bcss = {getf, getf_v, cursor, 0};
        ft_search search; 
        ft_search_init(&search, ft_cursor_compare_set, FT_SEARCH_LEFT, &cursor->key, nullptr, cursor->ft_handle);
        int r = toku_ft_search(cursor->ft_handle, &search, ft_cursor_current_getf, &bcss, cursor, false);
        ft_search_finish(&search);
        return r;
    }
    return getf(cursor->key.size, cursor->key.data, cursor->val.size, cursor->val.data, getf_v, false); // ft_cursor_copyout(cursor, outkey, outval);
}

int toku_ft_cursor_first(FT_CURSOR cursor, FT_GET_CALLBACK_FUNCTION getf, void *getf_v) {
    cursor->direction = 0;
    ft_search search; 
    ft_search_init(&search, toku_ft_cursor_compare_one, FT_SEARCH_LEFT, nullptr, nullptr, cursor->ft_handle);
    int r = ft_cursor_search(cursor, &search, getf, getf_v, false);
    ft_search_finish(&search);
    return r;
}

int toku_ft_cursor_last(FT_CURSOR cursor, FT_GET_CALLBACK_FUNCTION getf, void *getf_v) {
    cursor->direction = 0;
    ft_search search; 
    ft_search_init(&search, toku_ft_cursor_compare_one, FT_SEARCH_RIGHT, nullptr, nullptr, cursor->ft_handle);
    int r = ft_cursor_search(cursor, &search, getf, getf_v, false);
    ft_search_finish(&search);
    return r;
}

int toku_ft_cursor_check_restricted_range(FT_CURSOR c, const void *key, uint32_t keylen) {
    if (c->out_of_range_error) {
        FT ft = c->ft_handle->ft;
        DBT found_key;
        toku_fill_dbt(&found_key, key, keylen);
        if ((!c->left_is_neg_infty && c->direction <= 0 && ft->cmp(&found_key, &c->range_lock_left_key) < 0) ||
            (!c->right_is_pos_infty && c->direction >= 0 && ft->cmp(&found_key, &c->range_lock_right_key) > 0)) {
            invariant(c->out_of_range_error);
            return c->out_of_range_error;
        }
    }
    // Reset cursor direction to mitigate risk if some query type doesn't set the direction.
    // It is always correct to check both bounds (which happens when direction==0) but it can be slower.
    c->direction = 0;
    return 0;
}

int toku_ft_cursor_shortcut(FT_CURSOR cursor, int direction, uint32_t index, bn_data *bd,
                            FT_GET_CALLBACK_FUNCTION getf, void *getf_v,
                            uint32_t *keylen, void **key, uint32_t *vallen, void **val) {
    int r = 0;
    // if we are searching towards the end, limit is last element
    // if we are searching towards the beginning, limit is the first element
    uint32_t limit = (direction > 0) ? (bd->num_klpairs() - 1) : 0;

    //Starting with the prev, find the first real (non-provdel) leafentry.
    while (index != limit) {
        index += direction;
        LEAFENTRY le;
        void* foundkey = NULL;
        uint32_t foundkeylen = 0;
        
        r = bd->fetch_klpair(index, &le, &foundkeylen, &foundkey);
        invariant_zero(r);

        if (toku_ft_cursor_is_leaf_mode(cursor) || !le_val_is_del(le, cursor->is_snapshot_read, cursor->ttxn)) {
            le_extract_val(
                le,
                toku_ft_cursor_is_leaf_mode(cursor),
                cursor->is_snapshot_read,
                cursor->ttxn,
                vallen,
                val
                );
            *key = foundkey;
            *keylen = foundkeylen;

            cursor->direction = direction;
            r = toku_ft_cursor_check_restricted_range(cursor, *key, *keylen);
            if (r!=0) {
                paranoid_invariant(r == cursor->out_of_range_error);
                // We already got at least one entry from the bulk fetch.
                // Return 0 (instead of out of range error).
                r = 0;
                break;
            }
            r = getf(*keylen, *key, *vallen, *val, getf_v, false);
            if (r == TOKUDB_CURSOR_CONTINUE) {
                continue;
            }
            else {
                break;
            }
        }
    }

    return r;
}

int toku_ft_cursor_next(FT_CURSOR cursor, FT_GET_CALLBACK_FUNCTION getf, void *getf_v) {
    cursor->direction = +1;
    ft_search search; 
    ft_search_init(&search, ft_cursor_compare_next, FT_SEARCH_LEFT, &cursor->key, nullptr, cursor->ft_handle);
    int r = ft_cursor_search(cursor, &search, getf, getf_v, true);
    ft_search_finish(&search);
    if (r == 0) {
        toku_ft_cursor_set_prefetching(cursor);
    }
    return r;
}

static int ft_cursor_search_eq_k_x_getf(uint32_t keylen, const void *key,
                                        uint32_t vallen, const void *val,
                                        void *v, bool lock_only) {
    struct ft_cursor_search_struct *CAST_FROM_VOIDP(bcss, v);
    int r;
    if (key==NULL) {
        r = bcss->getf(0, NULL, 0, NULL, bcss->getf_v, false);
    } else {
        FT_CURSOR cursor = bcss->cursor;
        DBT newkey;
        toku_fill_dbt(&newkey, key, keylen);
        if (compare_k_x(cursor->ft_handle, bcss->search->k, &newkey) == 0) {
            r = bcss->getf(keylen, key, vallen, val, bcss->getf_v, lock_only);
        } else {
            r = bcss->getf(0, NULL, 0, NULL, bcss->getf_v, lock_only);
            if (r==0) r = TOKUDB_FOUND_BUT_REJECTED;
        }
    }
    return r;
}

/* search for the kv pair that matches the search object and is equal to k */
static int ft_cursor_search_eq_k_x(FT_CURSOR cursor, ft_search *search, FT_GET_CALLBACK_FUNCTION getf, void *getf_v) {
    struct ft_cursor_search_struct bcss = {getf, getf_v, cursor, search};
    int r = toku_ft_search(cursor->ft_handle, search, ft_cursor_search_eq_k_x_getf, &bcss, cursor, false);
    return r;
}

static int ft_cursor_compare_prev(const ft_search &search, const DBT *x) {
    FT_HANDLE CAST_FROM_VOIDP(ft_handle, search.context);
    return compare_k_x(ft_handle, search.k, x) > 0; /* return max xy: kv > xy */
}

int toku_ft_cursor_prev(FT_CURSOR cursor, FT_GET_CALLBACK_FUNCTION getf, void *getf_v) {
    cursor->direction = -1;
    ft_search search; 
    ft_search_init(&search, ft_cursor_compare_prev, FT_SEARCH_RIGHT, &cursor->key, nullptr, cursor->ft_handle);
    int r = ft_cursor_search(cursor, &search, getf, getf_v, true);
    ft_search_finish(&search);
    return r;
}

int toku_ft_cursor_compare_set_range(const ft_search &search, const DBT *x) {
    FT_HANDLE CAST_FROM_VOIDP(ft_handle, search.context);
    return compare_k_x(ft_handle, search.k, x) <= 0; /* return kv <= xy */
}

int toku_ft_cursor_set(FT_CURSOR cursor, DBT *key, FT_GET_CALLBACK_FUNCTION getf, void *getf_v) {
    cursor->direction = 0;
    ft_search search; 
    ft_search_init(&search, toku_ft_cursor_compare_set_range, FT_SEARCH_LEFT, key, nullptr, cursor->ft_handle);
    int r = ft_cursor_search_eq_k_x(cursor, &search, getf, getf_v);
    ft_search_finish(&search);
    return r;
}

int toku_ft_cursor_set_range(FT_CURSOR cursor, DBT *key, DBT *key_bound, FT_GET_CALLBACK_FUNCTION getf, void *getf_v) {
    cursor->direction = 0;
    ft_search search; 
    ft_search_init(&search, toku_ft_cursor_compare_set_range, FT_SEARCH_LEFT, key, key_bound, cursor->ft_handle);
    int r = ft_cursor_search(cursor, &search, getf, getf_v, false);
    ft_search_finish(&search);
    return r;
}

static int ft_cursor_compare_set_range_reverse(const ft_search &search, const DBT *x) {
    FT_HANDLE CAST_FROM_VOIDP(ft_handle, search.context);
    return compare_k_x(ft_handle, search.k, x) >= 0; /* return kv >= xy */
}

int toku_ft_cursor_set_range_reverse(FT_CURSOR cursor, DBT *key, FT_GET_CALLBACK_FUNCTION getf, void *getf_v) {
    cursor->direction = 0;
    ft_search search; 
    ft_search_init(&search, ft_cursor_compare_set_range_reverse, FT_SEARCH_RIGHT, key, nullptr, cursor->ft_handle);
    int r = ft_cursor_search(cursor, &search, getf, getf_v, false);
    ft_search_finish(&search);
    return r;
}

//TODO: When tests have been rewritten, get rid of this function.
//Only used by tests.
int toku_ft_cursor_get (FT_CURSOR cursor, DBT *key, FT_GET_CALLBACK_FUNCTION getf, void *getf_v, int get_flags) {
    int op = get_flags & DB_OPFLAGS_MASK;
    if (get_flags & ~DB_OPFLAGS_MASK)
        return EINVAL;

    switch (op) {
    case DB_CURRENT:
    case DB_CURRENT_BINDING:
        return toku_ft_cursor_current(cursor, op, getf, getf_v);
    case DB_FIRST:
        return toku_ft_cursor_first(cursor, getf, getf_v);
    case DB_LAST:
        return toku_ft_cursor_last(cursor, getf, getf_v);
    case DB_NEXT:
        if (toku_ft_cursor_not_set(cursor)) {
            return toku_ft_cursor_first(cursor, getf, getf_v);
        } else {
            return toku_ft_cursor_next(cursor, getf, getf_v);
        }
    case DB_PREV:
        if (toku_ft_cursor_not_set(cursor)) {
            return toku_ft_cursor_last(cursor, getf, getf_v);
        } else {
            return toku_ft_cursor_prev(cursor, getf, getf_v);
        }
    case DB_SET:
        return toku_ft_cursor_set(cursor, key, getf, getf_v);
    case DB_SET_RANGE:
        return toku_ft_cursor_set_range(cursor, key, nullptr, getf, getf_v);
    default: ;// Fall through
    }
    return EINVAL;
}

void toku_ft_cursor_peek(FT_CURSOR cursor, const DBT **pkey, const DBT **pval) {
    *pkey = &cursor->key;
    *pval = &cursor->val;
}

bool toku_ft_cursor_uninitialized(FT_CURSOR c) {
    return toku_ft_cursor_not_set(c);
}

int toku_ft_lookup(FT_HANDLE ft_handle, DBT *k, FT_GET_CALLBACK_FUNCTION getf, void *getf_v) {
    FT_CURSOR cursor;
    int r = toku_ft_cursor(ft_handle, &cursor, NULL, false, false);
    if (r != 0) {
        return r;
    }

    r = toku_ft_cursor_set(cursor, k, getf, getf_v);

    toku_ft_cursor_close(cursor);
    return r;
}
