/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ifndef FT_OPS_H
#define FT_OPS_H
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// This must be first to make the 64-bit file mode work right in Linux
#define _FILE_OFFSET_BITS 64
#include "fttypes.h"
#include "ybt.h"
#include <db.h>
#include "cachetable.h"
#include "log.h"
#include "ft-search.h"
#include "compress.h"

// A callback function is invoked with the key, and the data.
// The pointers (to the bytevecs) must not be modified.  The data must be copied out before the callback function returns.
// Note: In the thread-safe version, the brt node remains locked while the callback function runs.  So return soon, and don't call the BRT code from the callback function.
// If the callback function returns a nonzero value (an error code), then that error code is returned from the get function itself.
// The cursor object will have been updated (so that if result==0 the current value is the value being passed)
//  (If r!=0 then the cursor won't have been updated.)
// If r!=0, it's up to the callback function to return that value of r.
// A 'key' bytevec of NULL means that element is not found (effectively infinity or
// -infinity depending on direction)
// When lock_only is false, the callback does optional lock tree locking and then processes the key and val.
// When lock_only is true, the callback only does optional lock tree locking.
typedef int(*FT_GET_CALLBACK_FUNCTION)(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra, bool lock_only);

int toku_open_ft_handle (const char *fname, int is_create, FT_HANDLE *, int nodesize, int basementnodesize, enum toku_compression_method compression_method, CACHETABLE, TOKUTXN, int(*)(DB *,const DBT*,const DBT*)) __attribute__ ((warn_unused_result));
int toku_ft_change_descriptor(FT_HANDLE t, const DBT* old_descriptor, const DBT* new_descriptor, BOOL do_log, TOKUTXN txn, BOOL update_cmp_descriptor);

// See the ft-ops.c file for what this toku_redirect_ft does

u_int32_t toku_serialize_descriptor_size(const DESCRIPTOR desc);
int toku_ft_handle_create(FT_HANDLE *)  __attribute__ ((warn_unused_result));
int toku_ft_set_flags(FT_HANDLE, unsigned int flags)  __attribute__ ((warn_unused_result));
int toku_ft_get_flags(FT_HANDLE, unsigned int *flags)  __attribute__ ((warn_unused_result));
int toku_ft_set_nodesize(FT_HANDLE, unsigned int nodesize)  __attribute__ ((warn_unused_result));
int toku_ft_get_nodesize(FT_HANDLE, unsigned int *nodesize) __attribute__ ((warn_unused_result));
void toku_ft_get_maximum_advised_key_value_lengths(unsigned int *klimit, unsigned int *vlimit);
int toku_ft_set_basementnodesize(FT_HANDLE, unsigned int basementnodesize)  __attribute__ ((warn_unused_result));
int toku_ft_get_basementnodesize(FT_HANDLE, unsigned int *basementnodesize) __attribute__ ((warn_unused_result));
int toku_ft_set_compression_method(FT_HANDLE, enum toku_compression_method) __attribute__ ((warn_unused_result));
int toku_ft_get_compression_method(FT_HANDLE, enum toku_compression_method *) __attribute__((warn_unused_result));

int toku_ft_set_bt_compare(FT_HANDLE, ft_compare_func)  __attribute__ ((warn_unused_result));
ft_compare_func toku_ft_get_bt_compare (FT_HANDLE brt);

void toku_ft_set_redirect_callback(FT_HANDLE brt, on_redirect_callback redir_cb, void* extra);

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
int toku_ft_set_update(FT_HANDLE brt, ft_update_func update_fun) __attribute__ ((warn_unused_result));

int toku_ft_handle_open(FT_HANDLE, const char *fname_in_env,
		  int is_create, int only_create, CACHETABLE ct, TOKUTXN txn)  __attribute__ ((warn_unused_result));
int toku_ft_handle_open_recovery(FT_HANDLE, const char *fname_in_env, int is_create, int only_create, CACHETABLE ct, TOKUTXN txn, 
			   FILENUM use_filenum, LSN max_acceptable_lsn)  __attribute__ ((warn_unused_result));

// close an ft handle during normal operation. the underlying ft may or may not close,
// depending if there are still references. an lsn for this close will come from the logger.
void toku_ft_handle_close(FT_HANDLE ft_handle);
// close an ft handle during recovery. the underlying ft must close, and will use the given lsn.
void toku_ft_handle_close_recovery(FT_HANDLE ft_handle, LSN oplsn);

int
toku_ft_handle_open_with_dict_id(
    FT_HANDLE t, 
    const char *fname_in_env, 
    int is_create, 
    int only_create, 
    CACHETABLE cachetable, 
    TOKUTXN txn, 
    DICTIONARY_ID use_dictionary_id
    )  __attribute__ ((warn_unused_result));

int toku_ft_lookup (FT_HANDLE brt, DBT *k, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));

// Effect: Insert a key and data pair into a brt
// Returns 0 if successful
int toku_ft_insert (FT_HANDLE brt, DBT *k, DBT *v, TOKUTXN txn)  __attribute__ ((warn_unused_result));

int toku_ft_optimize (FT_HANDLE brt)  __attribute__ ((warn_unused_result));

// Effect: Insert a key and data pair into a brt if the oplsn is newer than the brt lsn.  This function is called during recovery.
// Returns 0 if successful
int toku_ft_maybe_insert (FT_HANDLE brt, DBT *k, DBT *v, TOKUTXN txn, BOOL oplsn_valid, LSN oplsn, BOOL do_logging, enum ft_msg_type type)  __attribute__ ((warn_unused_result));

// Effect: Send an update message into a brt.  This function is called
// during recovery.
// Returns 0 if successful
int toku_ft_maybe_update(FT_HANDLE brt, const DBT *key, const DBT *update_function_extra, TOKUTXN txn, BOOL oplsn_valid, LSN oplsn, BOOL do_logging) __attribute__ ((warn_unused_result));

// Effect: Send a broadcasting update message into a brt.  This function
// is called during recovery.
// Returns 0 if successful
int toku_ft_maybe_update_broadcast(FT_HANDLE brt, const DBT *update_function_extra, TOKUTXN txn, BOOL oplsn_valid, LSN oplsn, BOOL do_logging, BOOL is_resetting_op) __attribute__ ((warn_unused_result));

int toku_ft_load_recovery(TOKUTXN txn, FILENUM old_filenum, char const * new_iname, int do_fsync, int do_log, LSN *load_lsn)  __attribute__ ((warn_unused_result));
int toku_ft_load(FT_HANDLE brt, TOKUTXN txn, char const * new_iname, int do_fsync, LSN *get_lsn)  __attribute__ ((warn_unused_result));
// 2954
int toku_ft_hot_index_recovery(TOKUTXN txn, FILENUMS filenums, int do_fsync, int do_log, LSN *hot_index_lsn);
int toku_ft_hot_index(FT_HANDLE brt, TOKUTXN txn, FILENUMS filenums, int do_fsync, LSN *lsn) __attribute__ ((warn_unused_result));

int toku_ft_log_put_multiple (TOKUTXN txn, FT_HANDLE src_ft, FT_HANDLE *brts, int num_fts, const DBT *key, const DBT *val)  __attribute__ ((warn_unused_result));
int toku_ft_log_put (TOKUTXN txn, FT_HANDLE brt, const DBT *key, const DBT *val)  __attribute__ ((warn_unused_result));
int toku_ft_log_del_multiple (TOKUTXN txn, FT_HANDLE src_ft, FT_HANDLE *brts, int num_fts, const DBT *key, const DBT *val) __attribute__ ((warn_unused_result));
int toku_ft_log_del (TOKUTXN txn, FT_HANDLE brt, const DBT *key) __attribute__ ((warn_unused_result));

// Effect: Delete a key from a brt
// Returns 0 if successful
int toku_ft_delete (FT_HANDLE brt, DBT *k, TOKUTXN txn)  __attribute__ ((warn_unused_result));

// Effect: Delete a key from a brt if the oplsn is newer than the brt lsn.  This function is called during recovery.
// Returns 0 if successful
int toku_ft_maybe_delete (FT_HANDLE brt, DBT *k, TOKUTXN txn, BOOL oplsn_valid, LSN oplsn, BOOL do_logging)  __attribute__ ((warn_unused_result));

int toku_ft_send_insert(FT_HANDLE brt, DBT *key, DBT *val, XIDS xids, enum ft_msg_type type) __attribute__ ((warn_unused_result));
int toku_ft_send_delete(FT_HANDLE brt, DBT *key, XIDS xids) __attribute__ ((warn_unused_result));
int toku_ft_send_commit_any(FT_HANDLE brt, DBT *key, XIDS xids) __attribute__ ((warn_unused_result));

int toku_close_ft_handle_nolsn (FT_HANDLE, char **error_string)  __attribute__ ((warn_unused_result));

int toku_ft_handle_set_panic(FT_HANDLE brt, int panic, char *panic_string)  __attribute__ ((warn_unused_result));

int toku_dump_ft (FILE *,FT_HANDLE brt)  __attribute__ ((warn_unused_result));

extern int toku_ft_debug_mode;
int toku_verify_ft (FT_HANDLE brt)  __attribute__ ((warn_unused_result));
int toku_verify_ft_with_progress (FT_HANDLE brt, int (*progress_callback)(void *extra, float progress), void *extra, int verbose, int keep_going)  __attribute__ ((warn_unused_result));

typedef struct ft_cursor *FT_CURSOR;
int toku_ft_cursor (FT_HANDLE, FT_CURSOR*, TOKUTXN, BOOL, BOOL)  __attribute__ ((warn_unused_result));
void toku_ft_cursor_set_leaf_mode(FT_CURSOR);
// Sets a boolean on the brt cursor that prevents uncessary copying of
// the cursor duing a one query.
void toku_ft_cursor_set_temporary(FT_CURSOR);
int toku_ft_cursor_is_leaf_mode(FT_CURSOR);
void toku_ft_cursor_set_range_lock(FT_CURSOR, const DBT *, const DBT *, BOOL, BOOL);

// get is deprecated in favor of the individual functions below
int toku_ft_cursor_get (FT_CURSOR cursor, DBT *key, FT_GET_CALLBACK_FUNCTION getf, void *getf_v, int get_flags)  __attribute__ ((warn_unused_result));

int toku_ft_cursor_first(FT_CURSOR cursor, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_ft_cursor_last(FT_CURSOR cursor, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_ft_cursor_next(FT_CURSOR cursor, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_ft_cursor_next_nodup(FT_CURSOR cursor, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_ft_cursor_prev(FT_CURSOR cursor, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_ft_cursor_prev_nodup(FT_CURSOR cursor, FT_GET_CALLBACK_FUNCTION getf, void *getf_v) __attribute__ ((warn_unused_result));
int toku_ft_cursor_current(FT_CURSOR cursor, int op, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_ft_cursor_set(FT_CURSOR cursor, DBT *key, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_ft_cursor_set_range(FT_CURSOR cursor, DBT *key, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_ft_cursor_set_range_reverse(FT_CURSOR cursor, DBT *key, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_ft_cursor_get_both_range(FT_CURSOR cursor, DBT *key, DBT *val, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_ft_cursor_get_both_range_reverse(FT_CURSOR cursor, DBT *key, DBT *val, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));

int toku_ft_cursor_delete(FT_CURSOR cursor, int flags, TOKUTXN)  __attribute__ ((warn_unused_result));
int toku_ft_cursor_close (FT_CURSOR curs)  __attribute__ ((warn_unused_result));
BOOL toku_ft_cursor_uninitialized(FT_CURSOR c)  __attribute__ ((warn_unused_result));

void toku_ft_cursor_peek(FT_CURSOR cursor, const DBT **pkey, const DBT **pval);

DICTIONARY_ID toku_ft_get_dictionary_id(FT_HANDLE);

enum ft_flags {
    //TOKU_DB_DUP             = (1<<0),  //Obsolete #2862
    //TOKU_DB_DUPSORT         = (1<<1),  //Obsolete #2862
    TOKU_DB_KEYCMP_BUILTIN  = (1<<2),
    TOKU_DB_VALCMP_BUILTIN_13  = (1<<3),
};

int 
toku_ft_keyrange (FT_HANDLE brt, DBT *key, u_int64_t *less,  u_int64_t *equal,  u_int64_t *greater) __attribute__ ((warn_unused_result));

struct ftstat64_s {
    u_int64_t nkeys; /* estimate how many unique keys (even when flattened this may be an estimate)     */
    u_int64_t ndata; /* estimate the number of pairs (exact when flattened and committed)               */
    u_int64_t dsize; /* estimate the sum of the sizes of the pairs (exact when flattened and committed) */
    u_int64_t fsize;  /* the size of the underlying file                                                */
    u_int64_t ffree; /* Number of free bytes in the underlying file                                    */
    u_int64_t create_time_sec; /* creation time in seconds. */
    u_int64_t modify_time_sec; /* time of last serialization, in seconds. */ 
    u_int64_t verify_time_sec; /* time of last verification, in seconds */
};

int 
toku_ft_handle_stat64 (FT_HANDLE, TOKUTXN, struct ftstat64_s *stat) __attribute__ ((warn_unused_result));

int toku_ft_layer_init(void) __attribute__ ((warn_unused_result));
void toku_ft_open_close_lock(void);
void toku_ft_open_close_unlock(void);
void toku_ft_layer_destroy(void);
void toku_ft_serialize_layer_init(void);
void toku_ft_serialize_layer_destroy(void);

void toku_maybe_truncate_file (int fd, uint64_t size_used, uint64_t expected_size, uint64_t *new_size);
// Effect: truncate file if overallocated by at least 32MiB

void toku_maybe_preallocate_in_file (int fd, int64_t size, int64_t expected_size, int64_t *new_size);
// Effect: make the file bigger by either doubling it or growing by 16MiB whichever is less, until it is at least size
// Return 0 on success, otherwise an error number.

void toku_ft_suppress_recovery_logs (FT_HANDLE brt, TOKUTXN txn);
// Effect: suppresses recovery logs
// Requires: this is a (target) redirected brt
//           implies: txnid_that_created_or_locked_when_empty matches txn 
//           implies: toku_txn_note_ft(brt, txn) has been called

int toku_ft_get_fragmentation(FT_HANDLE brt, TOKU_DB_FRAGMENTATION report) __attribute__ ((warn_unused_result));

BOOL toku_ft_is_empty_fast (FT_HANDLE brt) __attribute__ ((warn_unused_result));
// Effect: Return TRUE if there are no messages or leaf entries in the tree.  If so, it's empty.  If there are messages  or leaf entries, we say it's not empty
// even though if we were to optimize the tree it might turn out that they are empty.

int toku_ft_strerror_r(int error, char *buf, size_t buflen);
// Effect: LIke the XSI-compliant strerorr_r, extended to db_strerror().
// If error>=0 then the result is to do strerror_r(error, buf, buflen), that is fill buf with a descriptive error message.
// If error<0 then return a TokuDB-specific error code.  For unknown cases, we return -1 and set errno=EINVAL, even for cases that *should* be known.  (Not all DB errors are known by this function which is a bug.)

extern BOOL garbage_collection_debug;

#endif
