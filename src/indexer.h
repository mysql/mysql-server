/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ifndef TOKU_INDEXER_H
#define TOKU_INDEXER_H

/* Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved.
 *
 * The technology is licensed by the Massachusetts Institute of Technology, 
 * Rutgers State University of New Jersey, and the Research Foundation of 
 * State University of New York at Stony Brook under United States of America 
 * Serial No. 11/760379 and to the patents and/or patent applications resulting from it.
 */



// locking and unlocking functions to synchronize cursor position with
// XXX_multiple APIs
void toku_indexer_lock(DB_INDEXER* indexer);

void toku_indexer_unlock(DB_INDEXER* indexer);

// The indexer populates multiple destination db's from the contents of one source db.
// While the indexes are being built by the indexer, the application may continue to 
// change the contents of the source db.  The changes will be reflected into the destination
// db's by the indexer.
//
// Each indexer references one source db.
// A source db may have multiple indexers referencing it.
// Each indexer references one or more destination db's.
// Each destination db references the one and only indexer that is building it.
//
// env must be set to the YDB environment
// txn must be set to the transaction under which the indexer will run
// *indexer is set to the address of the indexer object returned by the create function
// src_db is the source db
// N is the number of destination db's
// dest_dbs is an array of pointers to destination db's
// db_flags is currently unused
// indexer_flags is currently unused
//
// Returns 0 if the indexer has been created and sets *indexer to the indexer object.
// If an error occurred while creating the indexer object, a non-zero error number is returned.
//
// Clients must not operate on any of the dest_dbs concurrently with create_indexer();
int toku_indexer_create_indexer(DB_ENV *env,
                                DB_TXN *txn,
                                DB_INDEXER **indexer,
                                DB *src_db,
                                int N,
                                DB *dest_dbs[/*N*/],
                                uint32_t db_flags[/*N*/],
                                uint32_t indexer_flags) __attribute__((__visibility__("default")));

// Set the indexer poll function
int toku_indexer_set_poll_function(DB_INDEXER *indexer,
                                   int (*poll_function)(void *poll_extra,
                                                        float progress),
                                   void *poll_extra);

// Set the indexer error callback
int toku_indexer_set_error_callback(DB_INDEXER *indexer,
                                    void (*error_cb)(DB *db, int i, int err,
                                                     DBT *key, DBT *val,
                                                     void *error_extra),
                                    void *error_extra);

// Is the key right of the indexer's leaf entry cursor?
// Returns true  if right of le_cursor
// Returns false if left or equal to le_cursor
bool toku_indexer_is_key_right_of_le_cursor(DB_INDEXER *indexer, const DBT *key);

// Get the indexer's source db
DB *toku_indexer_get_src_db(DB_INDEXER *indexer);

// TEST set the indexer's test flags
extern "C" void toku_indexer_set_test_only_flags(DB_INDEXER *indexer, int flags) __attribute__((__visibility__("default")));

#define INDEXER_TEST_ONLY_ERROR_CALLBACK 1

typedef enum {
    INDEXER_CREATE = 0,     // number of indexers successfully created
    INDEXER_CREATE_FAIL,    // number of calls to toku_indexer_create_indexer() that failed
    INDEXER_BUILD,          // number of calls to indexer->build() succeeded
    INDEXER_BUILD_FAIL,     // number of calls to indexer->build() failed
    INDEXER_CLOSE,          // number of calls to indexer->close() that succeeded
    INDEXER_CLOSE_FAIL,     // number of calls to indexer->close() that failed
    INDEXER_ABORT,          // number of calls to indexer->abort()
    INDEXER_CURRENT,        // number of indexers currently in existence
    INDEXER_MAX,            // max number of indexers that ever existed simultaneously
    INDEXER_STATUS_NUM_ROWS
} indexer_status_entry;

typedef struct {
    bool initialized;
    TOKU_ENGINE_STATUS_ROW_S status[INDEXER_STATUS_NUM_ROWS];
} INDEXER_STATUS_S, *INDEXER_STATUS;

void toku_indexer_get_status(INDEXER_STATUS s);


#endif // TOKU_INDEXER_H
