/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"
#include "le-cursor.h"

// A LE_CURSOR is a special type of BRT_CURSOR that retrieves all of the leaf entries in a BRT.
// It caches the key that is was last positioned over to speed up key comparisions.

struct le_cursor {
    BRT_CURSOR brt_cursor;
    DBT key;           // the key that the le cursor is positioned at
                       // TODO a better implementation would fetch the key from the brt cursor 
    BOOL neg_infinity; // TRUE when the le cursor is positioned at -infinity (initial setting)
    BOOL pos_infinity; // TRUE when the le cursor is positioned at +infinity (when _next returns DB_NOTFOUND)
};

int 
le_cursor_create(LE_CURSOR *le_cursor_result, BRT brt, TOKUTXN txn) {
    int result = 0;
    LE_CURSOR le_cursor = (LE_CURSOR) toku_malloc(sizeof (struct le_cursor));
    if (le_cursor == NULL)
        result = errno;
    else {
        result = toku_brt_cursor(brt, &le_cursor->brt_cursor, txn, FALSE, FALSE);
        if (result == 0) {
            // TODO move the leaf mode to the brt cursor constructor
            toku_brt_cursor_set_leaf_mode(le_cursor->brt_cursor);
            toku_init_dbt(&le_cursor->key); 
            le_cursor->key.flags = DB_DBT_REALLOC;
            le_cursor->neg_infinity = TRUE;
            le_cursor->pos_infinity = FALSE;
        }
    }

    if (result == 0)
        *le_cursor_result = le_cursor;
    else
        toku_free(le_cursor);

    return result;
}

int 
le_cursor_close(LE_CURSOR le_cursor) {
    int result = toku_brt_cursor_close(le_cursor->brt_cursor);
    toku_destroy_dbt(&le_cursor->key);
    toku_free(le_cursor);
    return result;
}

// this implementation copies the key and leafentry into the supplied DBTs.
// this may be too slow.  an alternative implementation could avoid copying the
// key by fetching the key from the brt cursor, and could avoid copying the leaf entry
// by processing the leaf entry in the brt cursor callback.

struct le_cursor_callback_arg {
    DBT *key, *val;
};

// copy the key and the leaf entry to the given DBTs
static int
le_cursor_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *v) {
    struct le_cursor_callback_arg *arg = (struct le_cursor_callback_arg *) v;
    toku_dbt_set(keylen, key, arg->key, NULL);
    toku_dbt_set(vallen, val, arg->val, NULL);
    return 0;
}

int 
le_cursor_next(LE_CURSOR le_cursor, DBT *le) {
    int result;
    if (le_cursor->pos_infinity)
        result = DB_NOTFOUND;
    else {
        le_cursor->neg_infinity = FALSE;
        struct le_cursor_callback_arg arg = { &le_cursor->key, le };
        // TODO replace this with a non deprecated function
        result = toku_brt_cursor_get(le_cursor->brt_cursor, NULL, le_cursor_callback, &arg, DB_NEXT);
        if (result == DB_NOTFOUND)
            le_cursor->pos_infinity = TRUE;
    }
    return result;
}

BOOL
is_key_right_of_le_cursor(LE_CURSOR le_cursor, const DBT *key, DB *keycompare_db) {
    BOOL result;
    if (le_cursor->neg_infinity)
        result = TRUE;      // all keys are right of -infinity
    else if (le_cursor->pos_infinity)
        result = FALSE;     // all keys are left of +infinity
    else {
        brt_compare_func keycompare = toku_brt_get_bt_compare(le_cursor->brt_cursor->brt);
        int r = keycompare(keycompare_db, &le_cursor->key, key);
        if (r < 0)
            result = TRUE;  // key is right of the cursor key
        else
            result = FALSE; // key is at or left of the cursor key
    }
    return result;
}
