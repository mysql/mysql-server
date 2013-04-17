#ifndef TOKU_INDEXER_H
#define TOKU_INDEXER_H

#if defined(__cplusplus)
extern "C" {
#endif


/* Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved.
 *
 * The technology is licensed by the Massachusetts Institute of Technology, 
 * Rutgers State University of New Jersey, and the Research Foundation of 
 * State University of New York at Stony Brook under United States of America 
 * Serial No. 11/760379 and to the patents and/or patent applications resulting from it.
 */

int toku_indexer_create_indexer(DB_ENV *env,
                                DB_TXN *txn,
                                DB_INDEXER **indexer,
                                DB *src_db,
                                int N,
                                DB *dest_dbs[N],
                                uint32_t db_flags[N],
                                uint32_t indexer_flags) __attribute__((__visibility__("default")));

int toku_indexer_set_poll_function(DB_INDEXER *indexer,
                                   int (*poll_function)(void *poll_extra,
                                                        float progress),
                                   void *poll_extra);

int toku_indexer_set_error_callback(DB_INDEXER *indexer,
                                    void (*error_cb)(DB *db, int i, int err,
                                                     DBT *key, DBT *val,
                                                     void *error_extra),
                                    void *error_extra);

int toku_indexer_is_key_right_of_le_cursor(DB_INDEXER *indexer, DB *db, const DBT *key);

DB *toku_indexer_get_src_db(DB_INDEXER *indexer);

void toku_indexer_set_test_only_flags(DB_INDEXER *indexer, int flags) __attribute__((__visibility__("default")));

#define INDEXER_TEST_ONLY_ERROR_CALLBACK 1

typedef struct indexer_status {
    uint64_t create;        // number of indexers successfully created
    uint64_t create_fail;   // number of calls to toku_indexer_create_indexer() that failed
    uint64_t build;         // number of calls to indexer->build()
    uint64_t close;         // number of calls to indexer->close()
    uint64_t close_fail;    // number of calls to indexer->close() that failed
    uint64_t abort;         // number of calls to indexer->abort()
    uint32_t current;       // number of indexers currently in existence
    uint32_t max;           // max number of indexers that ever existed simulataneously
} INDEXER_STATUS_S, *INDEXER_STATUS;

void toku_indexer_get_status(INDEXER_STATUS s);

#if defined(__cplusplus)
}
#endif

#endif // TOKU_INDEXER_H
