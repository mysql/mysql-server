/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef FT_OPS_H
#define FT_OPS_H
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
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
// Note: In the thread-safe version, the ftnode remains locked while the callback function runs.  So return soon, and don't call the ft code from the callback function.
// If the callback function returns a nonzero value (an error code), then that error code is returned from the get function itself.
// The cursor object will have been updated (so that if result==0 the current value is the value being passed)
//  (If r!=0 then the cursor won't have been updated.)
// If r!=0, it's up to the callback function to return that value of r.
// A 'key' bytevec of NULL means that element is not found (effectively infinity or
// -infinity depending on direction)
// When lock_only is false, the callback does optional lock tree locking and then processes the key and val.
// When lock_only is true, the callback only does optional lock tree locking.
typedef int(*FT_GET_CALLBACK_FUNCTION)(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra, bool lock_only);

typedef bool(*FT_CHECK_INTERRUPT_CALLBACK)(void* extra);

int toku_open_ft_handle (const char *fname, int is_create, FT_HANDLE *, int nodesize, int basementnodesize, enum toku_compression_method compression_method, CACHETABLE, TOKUTXN, int(*)(DB *,const DBT*,const DBT*)) __attribute__ ((warn_unused_result));

// effect: changes the descriptor for the ft of the given handle.
// requires: 
// - cannot change descriptor for same ft in two threads in parallel. 
// - can only update cmp descriptor immidiately after opening the FIRST ft handle for this ft and before 
//   ANY operations. to update the cmp descriptor after any operations have already happened, all handles 
//   and transactions must close and reopen before the change, then you can update the cmp descriptor
void toku_ft_change_descriptor(FT_HANDLE t, const DBT* old_descriptor, const DBT* new_descriptor, bool do_log, TOKUTXN txn, bool update_cmp_descriptor);
uint32_t toku_serialize_descriptor_size(const DESCRIPTOR desc);

void toku_ft_handle_create(FT_HANDLE *ft);
void toku_ft_set_flags(FT_HANDLE, unsigned int flags);
void toku_ft_get_flags(FT_HANDLE, unsigned int *flags);
void toku_ft_handle_set_nodesize(FT_HANDLE, unsigned int nodesize);
void toku_ft_handle_get_nodesize(FT_HANDLE, unsigned int *nodesize);
void toku_ft_get_maximum_advised_key_value_lengths(unsigned int *klimit, unsigned int *vlimit);
void toku_ft_handle_set_basementnodesize(FT_HANDLE, unsigned int basementnodesize);
void toku_ft_handle_get_basementnodesize(FT_HANDLE, unsigned int *basementnodesize);
void toku_ft_handle_set_compression_method(FT_HANDLE, enum toku_compression_method);
void toku_ft_handle_get_compression_method(FT_HANDLE, enum toku_compression_method *);
void toku_ft_handle_set_fanout(FT_HANDLE, unsigned int fanout);
void toku_ft_handle_get_fanout(FT_HANDLE, unsigned int *fanout);

void toku_ft_set_bt_compare(FT_HANDLE, ft_compare_func);
ft_compare_func toku_ft_get_bt_compare (FT_HANDLE ft_h);

void toku_ft_set_redirect_callback(FT_HANDLE ft_h, on_redirect_callback redir_cb, void* extra);

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
void toku_ft_set_update(FT_HANDLE ft_h, ft_update_func update_fun);

int toku_ft_handle_open(FT_HANDLE, const char *fname_in_env,
		  int is_create, int only_create, CACHETABLE ct, TOKUTXN txn)  __attribute__ ((warn_unused_result));
int toku_ft_handle_open_recovery(FT_HANDLE, const char *fname_in_env, int is_create, int only_create, CACHETABLE ct, TOKUTXN txn, 
			   FILENUM use_filenum, LSN max_acceptable_lsn)  __attribute__ ((warn_unused_result));

// clone an ft handle. the cloned handle has a new dict_id but refers to the same fractal tree
int toku_ft_handle_clone(FT_HANDLE *cloned_ft_handle, FT_HANDLE ft_handle, TOKUTXN txn);

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

int toku_ft_lookup (FT_HANDLE ft_h, DBT *k, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));

// Effect: Insert a key and data pair into an ft
void toku_ft_insert (FT_HANDLE ft_h, DBT *k, DBT *v, TOKUTXN txn);

// Returns: 0 if the key was inserted, DB_KEYEXIST if the key already exists
int toku_ft_insert_unique(FT_HANDLE ft, DBT *k, DBT *v, TOKUTXN txn, bool do_logging);

// Effect: Optimize the ft
void toku_ft_optimize (FT_HANDLE ft_h);

// Effect: Insert a key and data pair into an ft if the oplsn is newer than the ft's lsn.  This function is called during recovery.
void toku_ft_maybe_insert (FT_HANDLE ft_h, DBT *k, DBT *v, TOKUTXN txn, bool oplsn_valid, LSN oplsn, bool do_logging, enum ft_msg_type type);

// Effect: Send an update message into an ft.  This function is called
// during recovery.
void toku_ft_maybe_update(FT_HANDLE ft_h, const DBT *key, const DBT *update_function_extra, TOKUTXN txn, bool oplsn_valid, LSN oplsn, bool do_logging);

// Effect: Send a broadcasting update message into an ft.  This function
// is called during recovery.
void toku_ft_maybe_update_broadcast(FT_HANDLE ft_h, const DBT *update_function_extra, TOKUTXN txn, bool oplsn_valid, LSN oplsn, bool do_logging, bool is_resetting_op);

void toku_ft_load_recovery(TOKUTXN txn, FILENUM old_filenum, char const * new_iname, int do_fsync, int do_log, LSN *load_lsn);
void toku_ft_load(FT_HANDLE ft_h, TOKUTXN txn, char const * new_iname, int do_fsync, LSN *get_lsn);
void toku_ft_hot_index_recovery(TOKUTXN txn, FILENUMS filenums, int do_fsync, int do_log, LSN *hot_index_lsn);
void toku_ft_hot_index(FT_HANDLE ft_h, TOKUTXN txn, FILENUMS filenums, int do_fsync, LSN *lsn);

void toku_ft_log_put_multiple (TOKUTXN txn, FT_HANDLE src_ft, FT_HANDLE *fts, uint32_t num_fts, const DBT *key, const DBT *val);
void toku_ft_log_put (TOKUTXN txn, FT_HANDLE ft_h, const DBT *key, const DBT *val);
void toku_ft_log_del_multiple (TOKUTXN txn, FT_HANDLE src_ft, FT_HANDLE *fts, uint32_t num_fts, const DBT *key, const DBT *val);
void toku_ft_log_del (TOKUTXN txn, FT_HANDLE ft_h, const DBT *key);

// Effect: Delete a key from an ft
void toku_ft_delete (FT_HANDLE ft_h, DBT *k, TOKUTXN txn);

// Effect: Delete a key from an ft if the oplsn is newer than the ft lsn.  This function is called during recovery.
void toku_ft_maybe_delete (FT_HANDLE ft_h, DBT *k, TOKUTXN txn, bool oplsn_valid, LSN oplsn, bool do_logging);

TXNID toku_ft_get_oldest_referenced_xid_estimate(FT_HANDLE ft_h);
TXN_MANAGER toku_ft_get_txn_manager(FT_HANDLE ft_h);

void toku_ft_send_insert(FT_HANDLE ft_h, DBT *key, DBT *val, XIDS xids, enum ft_msg_type type, txn_gc_info *gc_info);
void toku_ft_send_delete(FT_HANDLE ft_h, DBT *key, XIDS xids, txn_gc_info *gc_info);
void toku_ft_send_commit_any(FT_HANDLE ft_h, DBT *key, XIDS xids, txn_gc_info *gc_info);

int toku_close_ft_handle_nolsn (FT_HANDLE, char **error_string)  __attribute__ ((warn_unused_result));

int toku_dump_ft (FILE *,FT_HANDLE ft_h)  __attribute__ ((warn_unused_result));

extern int toku_ft_debug_mode;
int toku_verify_ft (FT_HANDLE ft_h)  __attribute__ ((warn_unused_result));
int toku_verify_ft_with_progress (FT_HANDLE ft_h, int (*progress_callback)(void *extra, float progress), void *extra, int verbose, int keep_going)  __attribute__ ((warn_unused_result));

typedef struct ft_cursor *FT_CURSOR;
int toku_ft_cursor (FT_HANDLE, FT_CURSOR*, TOKUTXN, bool, bool)  __attribute__ ((warn_unused_result));
void toku_ft_cursor_set_leaf_mode(FT_CURSOR);
// Sets a boolean on the ft cursor that prevents uncessary copying of
// the cursor duing a one query.
void toku_ft_cursor_set_temporary(FT_CURSOR);
void toku_ft_cursor_remove_restriction(FT_CURSOR);
void toku_ft_cursor_set_check_interrupt_cb(FT_CURSOR ftcursor, FT_CHECK_INTERRUPT_CALLBACK cb, void *extra);
int toku_ft_cursor_is_leaf_mode(FT_CURSOR);
void toku_ft_cursor_set_range_lock(FT_CURSOR, const DBT *, const DBT *, bool, bool, int);

// get is deprecated in favor of the individual functions below
int toku_ft_cursor_get (FT_CURSOR cursor, DBT *key, FT_GET_CALLBACK_FUNCTION getf, void *getf_v, int get_flags)  __attribute__ ((warn_unused_result));

int toku_ft_cursor_first(FT_CURSOR cursor, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_ft_cursor_last(FT_CURSOR cursor, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_ft_cursor_next(FT_CURSOR cursor, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_ft_cursor_prev(FT_CURSOR cursor, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_ft_cursor_current(FT_CURSOR cursor, int op, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_ft_cursor_set(FT_CURSOR cursor, DBT *key, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_ft_cursor_set_range(FT_CURSOR cursor, DBT *key, DBT *key_bound, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_ft_cursor_set_range_reverse(FT_CURSOR cursor, DBT *key, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_ft_cursor_get_both_range(FT_CURSOR cursor, DBT *key, DBT *val, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));
int toku_ft_cursor_get_both_range_reverse(FT_CURSOR cursor, DBT *key, DBT *val, FT_GET_CALLBACK_FUNCTION getf, void *getf_v)  __attribute__ ((warn_unused_result));

int toku_ft_cursor_delete(FT_CURSOR cursor, int flags, TOKUTXN)  __attribute__ ((warn_unused_result));
void toku_ft_cursor_close (FT_CURSOR curs);
bool toku_ft_cursor_uninitialized(FT_CURSOR c)  __attribute__ ((warn_unused_result));

void toku_ft_cursor_peek(FT_CURSOR cursor, const DBT **pkey, const DBT **pval);

DICTIONARY_ID toku_ft_get_dictionary_id(FT_HANDLE);

enum ft_flags {
    //TOKU_DB_DUP             = (1<<0),  //Obsolete #2862
    //TOKU_DB_DUPSORT         = (1<<1),  //Obsolete #2862
    TOKU_DB_KEYCMP_BUILTIN  = (1<<2),
    TOKU_DB_VALCMP_BUILTIN_13  = (1<<3),
};

void toku_ft_keyrange(FT_HANDLE ft_h, DBT *key, uint64_t *less,  uint64_t *equal,  uint64_t *greater);
void toku_ft_keysrange(FT_HANDLE ft_h, DBT* key_left, DBT* key_right, uint64_t *less_p, uint64_t* equal_left_p, uint64_t* middle_p, uint64_t* equal_right_p, uint64_t* greater_p, bool* middle_3_exact_p);

int toku_ft_get_key_after_bytes(FT_HANDLE ft_h, const DBT *start_key, uint64_t skip_len, void (*callback)(const DBT *end_key, uint64_t actually_skipped, void *extra), void *cb_extra);

struct ftstat64_s {
    uint64_t nkeys; /* estimate how many unique keys (even when flattened this may be an estimate)     */
    uint64_t ndata; /* estimate the number of pairs (exact when flattened and committed)               */
    uint64_t dsize; /* estimate the sum of the sizes of the pairs (exact when flattened and committed) */
    uint64_t fsize;  /* the size of the underlying file                                                */
    uint64_t ffree; /* Number of free bytes in the underlying file                                    */
    uint64_t create_time_sec; /* creation time in seconds. */
    uint64_t modify_time_sec; /* time of last serialization, in seconds. */ 
    uint64_t verify_time_sec; /* time of last verification, in seconds */
};

void toku_ft_handle_stat64 (FT_HANDLE, TOKUTXN, struct ftstat64_s *stat);

struct ftinfo64 {
    uint64_t num_blocks_allocated;  // number of blocks in the blocktable
    uint64_t num_blocks_in_use;     // number of blocks in use by most recent checkpoint
    uint64_t size_allocated;        // sum of sizes of blocks in blocktable
    uint64_t size_in_use;           // sum of sizes of blocks in use by most recent checkpoint
};

void toku_ft_handle_get_fractal_tree_info64(FT_HANDLE, struct ftinfo64 *);

int toku_ft_handle_iterate_fractal_tree_block_map(FT_HANDLE, int (*)(uint64_t,int64_t,int64_t,int64_t,int64_t,void*), void *);

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

int toku_ft_get_fragmentation(FT_HANDLE ft_h, TOKU_DB_FRAGMENTATION report) __attribute__ ((warn_unused_result));

bool toku_ft_is_empty_fast (FT_HANDLE ft_h) __attribute__ ((warn_unused_result));
// Effect: Return true if there are no messages or leaf entries in the tree.  If so, it's empty.  If there are messages  or leaf entries, we say it's not empty
// even though if we were to optimize the tree it might turn out that they are empty.

int toku_ft_strerror_r(int error, char *buf, size_t buflen);
// Effect: LIke the XSI-compliant strerorr_r, extended to db_strerror().
// If error>=0 then the result is to do strerror_r(error, buf, buflen), that is fill buf with a descriptive error message.
// If error<0 then return a TokuDB-specific error code.  For unknown cases, we return -1 and set errno=EINVAL, even for cases that *should* be known.  (Not all DB errors are known by this function which is a bug.)

extern bool garbage_collection_debug;

// This is a poor place to put global options like these.
void toku_ft_set_direct_io(bool direct_io_on);
void toku_ft_set_compress_buffers_before_eviction(bool compress_buffers);

void toku_note_deserialized_basement_node(bool fixed_key_size);

#endif
