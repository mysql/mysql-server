/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#ifndef LE_CURSOR_H
#define LE_CURSOR_H

// A leaf entry cursor (LE_CURSOR) is a special type of BRT_CURSOR that visits all of the leaf entries in a tree
// and returns the leaf entry to the caller.  It maintains a copy of the key that it was last positioned over to
// speed up key comparisions with a given key.  For example, the hot indexing could use the _key_right_of_cursor
// function to determine where a given key sits relative to the LE_CURSOR position.

// When _next and _key_right_of_cursor functions are run on multiple threads, they must be protected by a lock.  This
// lock is assumed to exist outside of the LE_CURSOR.

typedef struct le_cursor *LE_CURSOR;

// Create a leaf cursor for a tree (brt) within a transaction (txn)
// Success: returns 0, stores the LE_CURSOR in the le_cursor_result
// Failure: returns a non-zero error number
int le_cursor_create(LE_CURSOR *le_cursor_result, BRT brt, TOKUTXN txn);

// Close and free the LE_CURSOR
// Success: returns 0
// Failure: returns a non-zero error number
int le_cursor_close(LE_CURSOR le_cursor);

// Retrieve the next leaf entry under the LE_CURSOR
// Success: returns zero, stores the leaf entry key into the key dbt, and the leaf entry into the val dbt
// Failure: returns a non-zero error number
int le_cursor_next(LE_CURSOR le_cursor, DBT *key, DBT *val);

// Return TRUE if the key is to the right of the LE_CURSOR position
// Otherwise returns FALSE
// The LE_CURSOR position is intialized to -infinity. Any key comparision with -infinity returns TRUE.
// When the cursor runs off the right edge of the tree, the LE_CURSOR position is set to +infinity.  Any key comparision with +infinity
// returns FALSE.
int is_key_right_of_le_cursor(LE_CURSOR le_cursor, const DBT *key, int (*keycompare)(DB *extra, const DBT *, const DBT *), DB *extra);

#endif
