/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef BRT_H
#define BRT_H
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// This must be first to make the 64-bit file mode work right in Linux
#define _FILE_OFFSET_BITS 64
#include "brttypes.h"
#include "ybt.h"
#include <db.h>
#include "cachetable.h"
#include "log.h"
#include "brt-search.h"
#include "c_dialects.h"

C_BEGIN

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

int toku_open_brt (const char *fname, int is_create, BRT *, int nodesize, int basementnodesize, CACHETABLE, TOKUTXN, int(*)(DB*,const DBT*,const DBT*), DB*) __attribute__ ((warn_unused_result));
int toku_brt_change_descriptor(BRT t, const DBT* old_descriptor, const DBT* new_descriptor, BOOL do_log, TOKUTXN txn);
int toku_update_descriptor(struct brt_header * h, DESCRIPTOR d, int fd);


int toku_dictionary_redirect (const char *dst_fname_in_env, BRT old_brt, TOKUTXN txn) __attribute__ ((warn_unused_result));
// See the brt.c file for what this toku_redirect_brt does

int toku_dictionary_redirect_abort(struct brt_header *old_h, struct brt_header *new_h, TOKUTXN txn) __attribute__ ((warn_unused_result));

u_int32_t toku_serialize_descriptor_size(const DESCRIPTOR desc);
int toku_brt_create(BRT *)  __attribute__ ((warn_unused_result));
int toku_brt_set_flags(BRT, unsigned int flags)  __attribute__ ((warn_unused_result));
int toku_brt_get_flags(BRT, unsigned int *flags)  __attribute__ ((warn_unused_result));
int toku_brt_set_nodesize(BRT, unsigned int nodesize)  __attribute__ ((warn_unused_result));
int toku_brt_get_nodesize(BRT, unsigned int *nodesize) __attribute__ ((warn_unused_result));
int toku_brt_set_basementnodesize(BRT, unsigned int basementnodesize)  __attribute__ ((warn_unused_result));
int toku_brt_get_basementnodesize(BRT, unsigned int *basementnodesize) __attribute__ ((warn_unused_result));

int toku_brt_set_bt_compare(BRT, brt_compare_func)  __attribute__ ((warn_unused_result));
brt_compare_func toku_brt_get_bt_compare (BRT brt);

// How updates (update/insert/deletes) work:
// There are two flavers of upsertdels:  Singleton and broadcast.
// When a singleton upsertdel message arrives it contains a key and an extra DBT.
//
// At the YDB layer, the function looks like
//
// int (*update_function)(DB*, DB_TXN*, const DBT *key, const DBT *old_val, const DBT *extra,
//                        void (*set_val)(const DBT *new_val, void *set_extra), void *set_extra);
//
// And there are two DB functions
//
// int DB->update(DB *, DB_TXN *, const DBT *key, const DBT *extra);
// Effect:
//    If there is a key-value pair visible to the txn with value old_val then the system calls
//      update_function(DB, key, old_val, extra, set_val, set_extra)
//    where set_val and set_extra are a function and a void* provided by the system.
//    The update_function can do one of two things:
//      a) call set_val(new_val, set_extra)
//         which has the effect of doing DB->put(db, txn, key, new_val, 0)
//         overwriting the old value.
//      b) Return DB_DELETE (a new return code)
//      c) Return 0 (success) without calling set_val, which leaves the old value unchanged.
//    If there is no such key-value pair visible to the txn, then the system calls
//       update_function(DB, key, NULL, extra, set_val, set_extra)
//    and the update_function can do one of the same three things.
// Implementation notes: Update acquires a write lock (just as DB->put
//    does).   This function works by sending a UPDATE message containing
//    the key and extra.
//
// int DB->update_broadcast(DB *, DB_TXN*, const DBT *extra);
// Effect: This has the same effect as building a cursor that walks
//  through the DB, calling DB->update() on every key that the cursor
//  finds.
// Implementation note: Acquires a write lock on the entire database.
//  This function works by sending an BROADCAST-UPDATE message containing
//   the key and the extra.

// Question: Why does the update_function need a DB_TXN?						    

int toku_brt_set_update(BRT brt, int (*update_fun)(DB *,
						   const DBT *key, const DBT *old_val, const DBT *extra,
						   void (*set_val)(const DBT *new_val, void *set_extra), void *set_extra))
     __attribute__ ((warn_unused_result));
int toku_brt_update(BRT brt, TOKUTXN txn, const DBT *key, const DBT *extra) __attribute__ ((warn_unused_result));
int toku_brt_broadcast_update(BRT brt, TOKUTXN txn, const DBT *extra) __attribute__ ((warn_unused_result));

int brt_set_cachetable(BRT, CACHETABLE);
int toku_brt_open(BRT, const char *fname_in_env,
		  int is_create, int only_create, CACHETABLE ct, TOKUTXN txn, DB *db)  __attribute__ ((warn_unused_result));
int toku_brt_open_recovery(BRT, const char *fname_in_env, int is_create, int only_create, CACHETABLE ct, TOKUTXN txn, 
			   DB *db, FILENUM use_filenum, LSN max_acceptable_lsn)  __attribute__ ((warn_unused_result));

int toku_brt_remove_subdb(BRT brt, const char *dbname, u_int32_t flags)  __attribute__ ((warn_unused_result));

int toku_brt_lookup (BRT brt, DBT *k, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));

// Effect: Insert a key and data pair into a brt
// Returns 0 if successful
int toku_brt_insert (BRT brt, DBT *k, DBT *v, TOKUTXN txn)  __attribute__ ((warn_unused_result));

int toku_brt_optimize (BRT brt)  __attribute__ ((warn_unused_result));

int toku_brt_optimize_for_upgrade (BRT brt)  __attribute__ ((warn_unused_result));

// Effect: Insert a key and data pair into a brt if the oplsn is newer than the brt lsn.  This function is called during recovery.
// Returns 0 if successful
int toku_brt_maybe_insert (BRT brt, DBT *k, DBT *v, TOKUTXN txn, BOOL oplsn_valid, LSN oplsn, BOOL do_logging, enum brt_msg_type type)  __attribute__ ((warn_unused_result));

// Effect: Send an update message into a brt.  This function is called
// during recovery.
// Returns 0 if successful
int toku_brt_maybe_update(BRT brt, const DBT *key, const DBT *update_function_extra, TOKUTXN txn, BOOL oplsn_valid, LSN oplsn, BOOL do_logging) __attribute__ ((warn_unused_result));

// Effect: Send a broadcasting update message into a brt.  This function
// is called during recovery.
// Returns 0 if successful
int toku_brt_maybe_update_broadcast(BRT brt, const DBT *update_function_extra, TOKUTXN txn, BOOL oplsn_valid, LSN oplsn, BOOL do_logging, BOOL is_resetting_op) __attribute__ ((warn_unused_result));

int toku_brt_load_recovery(TOKUTXN txn, char const * old_iname, char const * new_iname, int do_fsync, int do_log, LSN *load_lsn)  __attribute__ ((warn_unused_result));
int toku_brt_load(BRT brt, TOKUTXN txn, char const * new_iname, int do_fsync, LSN *get_lsn)  __attribute__ ((warn_unused_result));
// 2954
int toku_brt_hot_index_recovery(TOKUTXN txn, FILENUMS filenums, int do_fsync, int do_log, LSN *hot_index_lsn);
int toku_brt_hot_index(BRT brt, TOKUTXN txn, FILENUMS filenums, int do_fsync, LSN *lsn) __attribute__ ((warn_unused_result));

int toku_brt_log_put_multiple (TOKUTXN txn, BRT src_brt, BRT *brts, int num_brts, const DBT *key, const DBT *val)  __attribute__ ((warn_unused_result));
int toku_brt_log_put (TOKUTXN txn, BRT brt, const DBT *key, const DBT *val)  __attribute__ ((warn_unused_result));
int toku_brt_log_del_multiple (TOKUTXN txn, BRT src_brt, BRT *brts, int num_brts, const DBT *key, const DBT *val) __attribute__ ((warn_unused_result));
int toku_brt_log_del (TOKUTXN txn, BRT brt, const DBT *key) __attribute__ ((warn_unused_result));

// Effect: Delete a key from a brt
// Returns 0 if successful
int toku_brt_delete (BRT brt, DBT *k, TOKUTXN txn)  __attribute__ ((warn_unused_result));

// Effect: Delete a key from a brt if the oplsn is newer than the brt lsn.  This function is called during recovery.
// Returns 0 if successful
int toku_brt_maybe_delete (BRT brt, DBT *k, TOKUTXN txn, BOOL oplsn_valid, LSN oplsn, BOOL do_logging)  __attribute__ ((warn_unused_result));

int toku_brt_send_insert(BRT brt, DBT *key, DBT *val, XIDS xids, enum brt_msg_type type) __attribute__ ((warn_unused_result));
int toku_brt_send_delete(BRT brt, DBT *key, XIDS xids) __attribute__ ((warn_unused_result));
int toku_brt_send_commit_any(BRT brt, DBT *key, XIDS xids) __attribute__ ((warn_unused_result));

int toku_brt_db_delay_closed (BRT brt, DB* db, int (*close_db)(DB*, u_int32_t), u_int32_t close_flags)  __attribute__ ((warn_unused_result));
int toku_close_brt (BRT, char **error_string)  __attribute__ ((warn_unused_result));
int toku_close_brt_lsn (BRT brt, char **error_string, BOOL oplsn_valid, LSN oplsn)  __attribute__ ((warn_unused_result));

int toku_brt_set_panic(BRT brt, int panic, char *panic_string)  __attribute__ ((warn_unused_result));

int toku_dump_brt (FILE *,BRT brt)  __attribute__ ((warn_unused_result));

void brt_fsync (BRT); /* fsync, but don't clear the caches. */
void brt_flush (BRT); /* fsync and clear the caches. */

int toku_brt_get_cursor_count (BRT brt)  __attribute__ ((warn_unused_result));
// get the number of cursors in the tree
// returns: the number of cursors.
// asserts: the number of cursors >= 0.

int toku_brt_flush (BRT brt)  __attribute__ ((warn_unused_result));
// effect: the tree's cachefile is flushed
// returns: 0 if success

int toku_brt_truncate (BRT brt)  __attribute__ ((warn_unused_result));
// effect: remove everything from the tree
// returns: 0 if success

LSN toku_brt_checkpoint_lsn(BRT brt)  __attribute__ ((warn_unused_result));

// create and initialize a cache table
// cachesize is the upper limit on the size of the size of the values in the table
// pass 0 if you want the default
int toku_brt_create_cachetable(CACHETABLE *t, long cachesize, LSN initial_lsn, TOKULOGGER)  __attribute__ ((warn_unused_result));

extern int toku_brt_debug_mode;
int toku_verify_brt (BRT brt)  __attribute__ ((warn_unused_result));
int toku_verify_brt_with_progress (BRT brt, int (*progress_callback)(void *extra, float progress), void *extra, int verbose, int keep_going)  __attribute__ ((warn_unused_result));

//int show_brt_blocknumbers(BRT);

typedef struct brt_cursor *BRT_CURSOR;
int toku_brt_cursor (BRT, BRT_CURSOR*, TOKUTXN, BOOL)  __attribute__ ((warn_unused_result));
void toku_brt_cursor_set_leaf_mode(BRT_CURSOR);
int toku_brt_cursor_is_leaf_mode(BRT_CURSOR);
void toku_brt_cursor_set_range_lock(BRT_CURSOR, const DBT *, const DBT *, BOOL, BOOL);

// get is deprecated in favor of the individual functions below
int toku_brt_cursor_get (BRT_CURSOR cursor, DBT *key, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v, int get_flags)  __attribute__ ((warn_unused_result));

int toku_brt_flatten(BRT, TOKUTXN ttxn)  __attribute__ ((warn_unused_result));
int toku_brt_cursor_first(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_brt_cursor_last(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_brt_cursor_next(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_brt_cursor_next_nodup(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_brt_cursor_prev(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_brt_cursor_prev_nodup(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v) __attribute__ ((warn_unused_result));
int toku_brt_cursor_current(BRT_CURSOR cursor, int op, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_brt_cursor_set(BRT_CURSOR cursor, DBT *key, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_brt_cursor_set_range(BRT_CURSOR cursor, DBT *key, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_brt_cursor_set_range_reverse(BRT_CURSOR cursor, DBT *key, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_brt_cursor_get_both_range(BRT_CURSOR cursor, DBT *key, DBT *val, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_brt_cursor_get_both_range_reverse(BRT_CURSOR cursor, DBT *key, DBT *val, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));

int toku_brt_cursor_delete(BRT_CURSOR cursor, int flags, TOKUTXN)  __attribute__ ((warn_unused_result));
int toku_brt_cursor_close (BRT_CURSOR curs)  __attribute__ ((warn_unused_result));
BOOL toku_brt_cursor_uninitialized(BRT_CURSOR c)  __attribute__ ((warn_unused_result));

void toku_brt_cursor_peek(BRT_CURSOR cursor, const DBT **pkey, const DBT **pval);

typedef struct brtenv *BRTENV;
int brtenv_checkpoint (BRTENV env)  __attribute__ ((warn_unused_result));

extern int toku_brt_do_push_cmd; // control whether push occurs eagerly.

// TODO: Get rid of this
int toku_brt_dbt_set(DBT* key, DBT* key_source);

DICTIONARY_ID toku_brt_get_dictionary_id(BRT);

int toku_brt_height_of_root(BRT, int *height)  __attribute__ ((warn_unused_result)); // for an open brt, return the current height.

enum brt_header_flags {
    //TOKU_DB_DUP             = (1<<0),  //Obsolete #2862
    //TOKU_DB_DUPSORT         = (1<<1),  //Obsolete #2862
    TOKU_DB_KEYCMP_BUILTIN  = (1<<2),
    TOKU_DB_VALCMP_BUILTIN_13  = (1<<3),
};

int toku_brt_keyrange (BRT brt, DBT *key, u_int64_t *less,  u_int64_t *equal,  u_int64_t *greater)  __attribute__ ((warn_unused_result));
struct brtstat64_s {
    u_int64_t nkeys; /* estimate how many unique keys (even when flattened this may be an estimate)     */
    u_int64_t ndata; /* estimate the number of pairs (exact when flattened and committed)               */
    u_int64_t dsize; /* estimate the sum of the sizes of the pairs (exact when flattened and committed) */
    u_int64_t fsize;  /* the size of the underlying file                                                */
    u_int64_t ffree; /* Number of free bytes in the underlying file                                    */
};
int toku_brt_stat64 (BRT, TOKUTXN,
		     struct brtstat64_s *stat
		     )
    __attribute__ ((warn_unused_result));

int toku_brt_init(void (*ydb_lock_callback)(void),
                  void (*ydb_unlock_callback)(void),
                  void (*db_set_brt)(DB*,BRT))
     __attribute__ ((warn_unused_result));
int toku_brt_destroy(void)  __attribute__ ((warn_unused_result));
int toku_pwrite_lock_init(void) __attribute__ ((warn_unused_result));
int toku_pwrite_lock_destroy(void) __attribute__ ((warn_unused_result));
int toku_brt_serialize_init(void) __attribute__ ((warn_unused_result));
int toku_brt_serialize_destroy(void) __attribute__ ((warn_unused_result));

void toku_maybe_truncate_cachefile (CACHEFILE cf, int fd, u_int64_t size_used);
// Effect: truncate file if overallocated by at least 32MiB

int maybe_preallocate_in_file (int fd, u_int64_t size) __attribute__ ((warn_unused_result));
// Effect: If file size is less than SIZE, make it bigger by either doubling it or growing by 16MB whichever is less.


void toku_brt_require_local_checkpoint (BRT brt, TOKUTXN txn);
// Require that dictionary specified by brt is fully written to disk before
// transaction txn is committed.


void toku_brt_header_suppress_rollbacks(struct brt_header *h, TOKUTXN txn);
//Effect: suppresses rollback logs

void toku_brt_suppress_recovery_logs (BRT brt, TOKUTXN txn);
// Effect: suppresses recovery logs
// Requires: this is a (target) redirected brt
//           implies: txnid_that_created_or_locked_when_empty matches txn 
//           implies: toku_txn_note_brt(brt, txn) has been called

int toku_brt_zombie_needed (BRT brt) __attribute__ ((warn_unused_result));

int toku_brt_get_fragmentation(BRT brt, TOKU_DB_FRAGMENTATION report) __attribute__ ((warn_unused_result));
int toku_brt_header_set_panic(struct brt_header *h, int panic, char *panic_string) __attribute__ ((warn_unused_result));

BOOL toku_brt_is_empty_fast (BRT brt);
// Effect: Return TRUE if there are no messages or leaf entries in the tree.  If so, it's empty.  If there are messages  or leaf entries, we say it's not empty
// even though if we were to optimize the tree it might turn out that they are empty.

BOOL toku_brt_is_empty_fast (BRT brt) __attribute__ ((warn_unused_result));
// Effect: Return TRUE if there are no messages or leaf entries in the tree.  If so, it's empty.  If there are messages  or leaf entries, we say it's not empty
// even though if we were to optimize the tree it might turn out that they are empty.

BOOL toku_brt_is_recovery_logging_suppressed (BRT) __attribute__ ((warn_unused_result));

void toku_brt_bn_reset_stats(BRTNODE node, int childnum);
void toku_brt_leaf_reset_calc_leaf_stats(BRTNODE node);

int toku_brt_strerror_r(int error, char *buf, size_t buflen);
// Effect: LIke the XSI-compliant strerorr_r, extended to db_strerror().
// If error>=0 then the result is to do strerror_r(error, buf, buflen), that is fill buf with a descriptive error message.
// If error<0 then return a TokuDB-specific error code.  For unknown cases, we return -1 and set errno=EINVAL, even for cases that *should* be known.  (Not all DB errors are known by this function which is a bug.)

extern BOOL garbage_collection_debug;

C_END

#endif
