/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef BRT_H
#define BRT_H
#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// This must be first to make the 64-bit file mode work right in Linux
#define _FILE_OFFSET_BITS 64
#include "brttypes.h"
#include "ybt.h"
#include <db.h>
#include "cachetable.h"
#include "log.h"
#include "brt-search.h"

// A callback function is invoked with the key, and the data.
// The pointers (to the bytevecs) must not be modified.  The data must be copied out before the callback function returns.
// Note: In the thread-safe version, the brt node remains locked while the callback function runs.  So return soon, and don't call the BRT code from the callback function.
// If the callback function returns a nonzero value (an error code), then that error code is returned from the get function itself.
// The cursor object will have been updated (so that if result==0 the current value is the value being passed)
//  (If r!=0 then the cursor won't have been updated.)
// If r!=0, it's up to the callback function to return that value of r.
//A 'key' bytevec of NULL means that element is not found (effectively infinity or
//-infinity depending on direction)
typedef int(*BRT_GET_CALLBACK_FUNCTION)(ITEMLEN, bytevec, ITEMLEN, bytevec, void*);
//Same as BRT_GET_CALLBACK_FUNCTION but returns both the answer to the query and
//the element on the other side of the border (as in heaviside function).
typedef int(*BRT_GET_STRADDLE_CALLBACK_FUNCTION)(ITEMLEN, bytevec, ITEMLEN, bytevec, ITEMLEN, bytevec, ITEMLEN, bytevec, void*);

int toku_open_brt (const char *fname, int is_create, BRT *, int nodesize, CACHETABLE, TOKUTXN, int(*)(DB*,const DBT*,const DBT*), DB*);

u_int32_t toku_serialize_descriptor_size(struct descriptor *desc);
int toku_brt_create(BRT *);
int toku_brt_set_flags(BRT, unsigned int flags);
int toku_brt_set_descriptor (BRT t, u_int32_t version, const DBT* descriptor, toku_dbt_upgradef dbt_userformat_upgrade);
int toku_brt_get_flags(BRT, unsigned int *flags);
int toku_brt_set_nodesize(BRT, unsigned int nodesize);
int toku_brt_get_nodesize(BRT, unsigned int *nodesize);

int toku_brt_set_bt_compare(BRT, brt_compare_func);
int toku_brt_set_dup_compare(BRT, brt_compare_func);

int toku_brt_set_filenum(BRT brt, FILENUM filenum);

int brt_set_cachetable(BRT, CACHETABLE);
int toku_brt_open(BRT, const char *fname, const char *fname_in_env, int is_create, int only_create, CACHETABLE ct, TOKUTXN txn, DB *db);
int toku_brt_open_recovery(BRT, const char *fname, const char *fname_in_env, int is_create, int only_create, CACHETABLE ct, TOKUTXN txn, DB *db, int recovery_force_fcreate);

int toku_brt_remove_subdb(BRT brt, const char *dbname, u_int32_t flags);

int toku_brt_broadcast_commit_all (BRT brt);
int toku_brt_lookup (BRT brt, DBT *k, DBT *v, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v);

// Effect: Insert a key and data pair into a brt
// Returns 0 if successful
int toku_brt_insert (BRT brt, DBT *k, DBT *v, TOKUTXN txn);

// Effect: Insert a key and data pair into a brt if the oplsn is newer than the brt lsn.  This function is called during recovery.
// Returns 0 if successful
int toku_brt_maybe_insert (BRT brt, DBT *k, DBT *v, TOKUTXN txn, BOOL oplsn_valid, LSN oplsn);

// Effect: Delete a key from a brt
// Returns 0 if successful
int toku_brt_delete (BRT brt, DBT *k, TOKUTXN txn);

// Effect: Delete a key from a brt if the oplsn is newer than the brt lsn.  This function is called during recovery.
// Returns 0 if successful
int toku_brt_maybe_delete (BRT brt, DBT *k, TOKUTXN txn, BOOL oplsn_valid, LSN oplsn);

// Effect: Delete a pair only if both k and v are equal according to the comparison function.
// Returns 0 if successful
int toku_brt_delete_both (BRT brt, DBT *k, DBT *v, TOKUTXN txn); 

// Effect: Delete a pair only if both k and v are equal according to the comparison function and the
// oplsn is newer than the brt lsn.  This function is called by recovery.
// Returns 0 if successful
int toku_brt_maybe_delete_both (BRT brt, DBT *k, DBT *v, TOKUTXN txn, BOOL oplsn_valid, LSN oplsn);

int toku_brt_db_delay_closed (BRT brt, DB* db, int (*close_db)(DB*, u_int32_t), u_int32_t close_flags);
int toku_close_brt (BRT, char **error_string);
int toku_close_brt_lsn (BRT brt, char **error_string, BOOL oplsn_valid, LSN oplsn);

int toku_brt_set_panic(BRT brt, int panic, char *panic_string);

int toku_dump_brt (FILE *,BRT brt);

void brt_fsync (BRT); /* fsync, but don't clear the caches. */
void brt_flush (BRT); /* fsync and clear the caches. */

int toku_brt_get_cursor_count (BRT brt);
// get the number of cursors in the tree
// returns: the number of cursors.
// asserts: the number of cursors >= 0.

int toku_brt_flush (BRT brt);
// effect: the tree's cachefile is flushed
// returns: 0 if success

int toku_brt_truncate (BRT brt);
// effect: remove everything from the tree
// returns: 0 if success

LSN toku_brt_checkpoint_lsn(BRT brt);

// create and initialize a cache table
// cachesize is the upper limit on the size of the size of the values in the table
// pass 0 if you want the default
int toku_brt_create_cachetable(CACHETABLE *t, long cachesize, LSN initial_lsn, TOKULOGGER);

extern int toku_brt_debug_mode;
int toku_verify_brt (BRT brt);

//int show_brt_blocknumbers(BRT);

typedef struct brt_cursor *BRT_CURSOR;
int toku_brt_cursor (BRT, BRT_CURSOR*, TOKULOGGER);

// get is deprecated in favor of the individual functions below
int toku_brt_cursor_get (BRT_CURSOR cursor, DBT *key, DBT *val, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v, int get_flags);

int toku_brt_flatten(BRT, TOKULOGGER logger);
int toku_brt_cursor_first(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v);
int toku_brt_cursor_last(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v);
int toku_brt_cursor_next(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v);
int toku_brt_cursor_next_nodup(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v);
int toku_brt_cursor_next_dup(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v);
int toku_brt_cursor_prev(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v);
int toku_brt_cursor_prev_nodup(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v);
int toku_brt_cursor_prev_dup(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v);
int toku_brt_cursor_current(BRT_CURSOR cursor, int op, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v);
int toku_brt_cursor_set(BRT_CURSOR cursor, DBT *key, DBT *val, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v);
int toku_brt_cursor_set_range(BRT_CURSOR cursor, DBT *key, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v);
int toku_brt_cursor_set_range_reverse(BRT_CURSOR cursor, DBT *key, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v);
int toku_brt_cursor_get_both_range(BRT_CURSOR cursor, DBT *key, DBT *val, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v);
int toku_brt_cursor_get_both_range_reverse(BRT_CURSOR cursor, DBT *key, DBT *val, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v);


typedef struct {
    YDB_HEAVISIDE_FUNCTION h;
    void *extra_h;
    int r_h;
    int direction;
} *HEAVI_WRAPPER, HEAVI_WRAPPER_S;
int toku_brt_cursor_heaviside(BRT_CURSOR cursor, BRT_GET_STRADDLE_CALLBACK_FUNCTION getf, void *getf_v, HEAVI_WRAPPER wrapper);
int toku_brt_cursor_delete(BRT_CURSOR cursor, int flags, TOKUTXN);
int toku_brt_cursor_close (BRT_CURSOR curs);
BOOL toku_brt_cursor_uninitialized(BRT_CURSOR c);

void toku_brt_cursor_peek(BRT_CURSOR cursor, const DBT **pkey, const DBT **pval);

typedef struct brtenv *BRTENV;
int brtenv_checkpoint (BRTENV env);

extern int toku_brt_do_push_cmd; // control whether push occurs eagerly.

// TODO: Get rid of this
int toku_brt_dbt_set(DBT* key, DBT* key_source);

int toku_brt_get_fd(BRT, int *);

int toku_brt_height_of_root(BRT, int *height); // for an open brt, return the current height.

enum brt_header_flags {
    TOKU_DB_DUP             = (1<<0),
    TOKU_DB_DUPSORT         = (1<<1),
    TOKU_DB_KEYCMP_BUILTIN  = (1<<2),
    TOKU_DB_VALCMP_BUILTIN  = (1<<3),
};

int toku_brt_keyrange (BRT brt, DBT *key, u_int64_t *less,  u_int64_t *equal,  u_int64_t *greater);
struct brtstat64_s {
    u_int64_t nkeys; /* estimate how many unique keys (even when flattened this may be an estimate)     */
    u_int64_t ndata; /* estimate the number of pairs (exact when flattened and committed)               */
    u_int64_t dsize; /* estimate the sum of the sizes of the pairs (exact when flattened and committed) */
    u_int64_t fsize;  /* the size of the underlying file                                                */
    u_int64_t ffree; /* Number of free bytes in the underlying file                                    */
};
int toku_brt_stat64 (BRT, TOKUTXN,
		     struct brtstat64_s *stat
		     );

int toku_brt_init(void (*ydb_lock_callback)(void), void (*ydb_unlock_callback)(void));
int toku_brt_destroy(void);
int toku_pwrite_lock_init(void);
int toku_pwrite_lock_destroy(void);

void toku_maybe_truncate_cachefile (CACHEFILE cf, u_int64_t size_used);
// Effect: truncate file if overallocated by at least 32MiB

int maybe_preallocate_in_file (int fd, u_int64_t size);
// Effect: If file size is less than SIZE, make it bigger by either doubling it or growing by 16MB whichever is less.

int toku_brt_note_table_lock (BRT brt, TOKUTXN txn);
// Effect: Record the fact that the BRT has a table lock (and thus no other txn will modify it until this txn completes.  As a result, we can limit the amount of information in the rollback data structure.

int toku_brt_zombie_needed (BRT brt);

//TODO: #1485 once we have multiple main threads, restore this code, analyze performance.
#ifndef TOKU_MULTIPLE_MAIN_THREADS
#define TOKU_MULTIPLE_MAIN_THREADS 0
#endif

#endif
