/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "ft.h"
#include "ft-internal.h"
#include "le-cursor.h"

// A LE_CURSOR is a special purpose FT_CURSOR that:
//  - enables prefetching
//  - does not perform snapshot reads. it reads everything, including uncommitted.
//
// A LE_CURSOR is good for scanning a FT from beginning to end. Useful for hot indexing.

struct le_cursor {
    // TODO: remove DBs from the ft layer comparison function 
    // so this is never necessary
    // use a fake db for comparisons. 
    struct __toku_db fake_db;
    FT_CURSOR ft_cursor;
    bool neg_infinity; // true when the le cursor is positioned at -infinity (initial setting)
    bool pos_infinity; // true when the le cursor is positioned at +infinity (when _next returns DB_NOTFOUND)
};

int 
toku_le_cursor_create(LE_CURSOR *le_cursor_result, FT_HANDLE ft_handle, TOKUTXN txn) {
    int result = 0;
    LE_CURSOR MALLOC(le_cursor);
    if (le_cursor == NULL) {
        result = get_error_errno();
    }
    else {
        result = toku_ft_cursor(ft_handle, &le_cursor->ft_cursor, txn, false, false);
        if (result == 0) {
            // TODO move the leaf mode to the ft cursor constructor
            toku_ft_cursor_set_leaf_mode(le_cursor->ft_cursor);
            le_cursor->neg_infinity = true;
            le_cursor->pos_infinity = false;
            // zero out the fake DB. this is a rare operation so it's not too slow.
            memset(&le_cursor->fake_db, 0, sizeof(le_cursor->fake_db));
        }
    }

    if (result == 0) {
        *le_cursor_result = le_cursor;
    } else {
        toku_free(le_cursor);
    }

    return result;
}

void toku_le_cursor_close(LE_CURSOR le_cursor) {
    toku_ft_cursor_close(le_cursor->ft_cursor);
    toku_free(le_cursor);
}

// Move to the next leaf entry under the LE_CURSOR
// Success: returns zero, calls the getf callback with the getf_v parameter
// Failure: returns a non-zero error number
int 
toku_le_cursor_next(LE_CURSOR le_cursor, FT_GET_CALLBACK_FUNCTION getf, void *getf_v) {
    int result;
    if (le_cursor->pos_infinity) {
        result = DB_NOTFOUND;
    } else {
        le_cursor->neg_infinity = false;
        // TODO replace this with a non deprecated function. Which?
        result = toku_ft_cursor_get(le_cursor->ft_cursor, NULL, getf, getf_v, DB_NEXT);
        if (result == DB_NOTFOUND) {
            le_cursor->pos_infinity = true;
        }
    }
    return result;
}

bool
toku_le_cursor_is_key_greater(LE_CURSOR le_cursor, const DBT *key) {
    bool result;
    if (le_cursor->neg_infinity) {
        result = true;      // all keys are greater than -infinity
    } else if (le_cursor->pos_infinity) {
        result = false;     // all keys are less than +infinity
    } else {
        // get the comparison function and descriptor from the cursor's ft
        FT_HANDLE ft_handle = le_cursor->ft_cursor->ft_handle;
        ft_compare_func keycompare = toku_ft_get_bt_compare(ft_handle);
        le_cursor->fake_db.cmp_descriptor = toku_ft_get_cmp_descriptor(ft_handle);
        // get the current position from the cursor and compare it to the given key.
        DBT *cursor_key = &le_cursor->ft_cursor->key;
        int r = keycompare(&le_cursor->fake_db, cursor_key, key);
        if (r < 0) {
            result = true;  // key is right of the cursor key
        } else {
            result = false; // key is at or left of the cursor key
        }
    }
    return result;
}
