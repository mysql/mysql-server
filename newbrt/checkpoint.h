/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2009 Tokutek Inc.  All rights reserved."
 
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#ident "$Id$"


// TODO: $1510  Need callback mechanism so newbrt layer can get/release ydb lock.


/******
 *
 * NOTE: atomic operation lock is highest level lock
 *       checkpoint_safe lock is next level lock
 *       ydb_big_lock is next level lock
 *
 *       Locks must always be taken in this sequence (highest level first).
 *
 */


/****** TODO: ydb layer should be taking this lock
 * Client code must hold the checkpoint_safe lock during the following operations:
 *       - delete a dictionary
 *       - truncate a dictionary
 *       - rename a dictionary
 *       - abort a transaction that created a dictionary (abort causes dictionary delete)
 *       - abort a transaction that had a table lock on an empty table (abort causes dictionary truncate)
 *****/

void toku_checkpoint_safe_client_lock(void);

void toku_checkpoint_safe_client_unlock(void);



/****** TODO: rename these functions as something like begin_atomic_operation and end_atomic_operation
 *            these may need ydb wrappers
 * These functions are called from the handlerton level.
 * Client code must hold the multi_operation lock during the following operations:
 *       - insertion into multiple indexes
 *       - replace into (simultaneous delete/insert on a single key)
 *****/

void toku_multi_operation_client_lock(void);

void toku_multi_operation_client_unlock(void);


// Initialize the checkpoint mechanism, must be called before any client operations.
// Must pass in function pointers to take/release ydb lock.
void toku_checkpoint_init(void (*ydb_lock_callback)(void), void (*ydb_unlock_callback)(void));

void toku_checkpoint_destroy(void);

// Take a checkpoint of all currently open dictionaries
int toku_checkpoint(CACHETABLE ct, TOKULOGGER logger, char **error_string);
