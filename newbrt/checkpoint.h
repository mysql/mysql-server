/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "$Id$"

int toku_set_checkpoint_period(CACHETABLE ct, u_int32_t new_period);
u_int32_t toku_get_checkpoint_period(CACHETABLE ct);
//Effect: Change [end checkpoint (n) - begin checkpoint (n+1)] delay to
//        new_period seconds.  0 means disable.

/******
 *
 * NOTE: multi_operation_lock is highest level lock
 *       checkpoint_safe_lock is next level lock
 *       ydb_big_lock is next level lock
 *
 *       Locks must always be taken in this sequence (highest level first).
 *
 */


/****** 
 * Client code must hold the checkpoint_safe lock during the following operations:
 *       - delete a dictionary via DB->remove
 *       - delete a dictionary via DB_TXN->abort(txn) (where txn created a dictionary)
 *       - rename a dictionary //TODO: Handlerton rename needs to take this
 *                             //TODO: Handlerton rename needs to be recoded for transaction recovery
 *****/

void toku_checkpoint_safe_client_lock(void);

void toku_checkpoint_safe_client_unlock(void);



/****** 
 * These functions are called from the ydb level.
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
// Callback is called during checkpoint procedure while checkpoint_safe lock is still held.
// Callback is primarily intended for use in testing.
int toku_checkpoint(CACHETABLE ct, TOKULOGGER logger, char **error_string, void (*callback_f)(void*), void * extra);
