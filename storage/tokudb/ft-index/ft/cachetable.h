/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef CACHETABLE_H
#define CACHETABLE_H

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

#include <fcntl.h>
#include "fttypes.h"
#include "minicron.h"

// Maintain a cache mapping from cachekeys to values (void*)
// Some of the keys can be pinned.  Don't pin too many or for too long.
// If the cachetable is too full, it will call the flush_callback() function with the key, the value, and the otherargs
// and then remove the key-value pair from the cache.
// The callback won't be any of the currently pinned keys.
// Also when flushing an object, the cachetable drops all references to it,
// so you may need to free() it.
// Note: The cachetable should use a common pool of memory, flushing things across cachetables.
//  (The first implementation doesn't)
// If you pin something twice, you must unpin it twice.
// table_size is the initial size of the cache table hash table (in number of entries)
// size limit is the upper bound of the sum of size of the entries in the cache table (total number of bytes)

typedef BLOCKNUM CACHEKEY;

void toku_set_cleaner_period (CACHETABLE ct, uint32_t new_period);
uint32_t toku_get_cleaner_period_unlocked (CACHETABLE ct);
void toku_set_cleaner_iterations (CACHETABLE ct, uint32_t new_iterations);
uint32_t toku_get_cleaner_iterations (CACHETABLE ct);
uint32_t toku_get_cleaner_iterations_unlocked (CACHETABLE ct);

// cachetable operations

// create and initialize a cache table
// size_limit is the upper limit on the size of the size of the values in the table
// pass 0 if you want the default
int toku_cachetable_create(CACHETABLE *result, long size_limit, LSN initial_lsn, TOKULOGGER);

// Create a new cachetable.
// Effects: a new cachetable is created and initialized.
// The cachetable pointer is stored into result.
// The sum of the sizes of the memory objects is set to size_limit, in whatever
// units make sense to the user of the cachetable.
// Returns: If success, returns 0 and result points to the new cachetable. Otherwise,
// returns an error number.

// Returns a pointer to the checkpointer within the given cachetable.
CHECKPOINTER toku_cachetable_get_checkpointer(CACHETABLE ct);

// What is the cachefile that goes with a particular filenum?
// During a transaction, we cannot reuse a filenum.
int toku_cachefile_of_filenum (CACHETABLE t, FILENUM filenum, CACHEFILE *cf);

// What is the cachefile that goes with a particular iname (relative to env)?
// During a transaction, we cannot reuse an iname.
int toku_cachefile_of_iname_in_env (CACHETABLE ct, const char *iname_in_env, CACHEFILE *cf);

// Get the iname (within the cwd) associated with the cachefile
// Return the filename
char *toku_cachefile_fname_in_cwd (CACHEFILE cf);

void toku_cachetable_begin_checkpoint (CHECKPOINTER cp, TOKULOGGER);

void toku_cachetable_end_checkpoint(CHECKPOINTER cp, TOKULOGGER logger, 
                                   void (*testcallback_f)(void*),  void * testextra);

// Shuts down checkpoint thread
// Requires no locks be held that are taken by the checkpoint function
void toku_cachetable_minicron_shutdown(CACHETABLE ct);

// Close the cachetable.
// Effects: All of the memory objects are flushed to disk, and the cachetable is destroyed.
void toku_cachetable_close(CACHETABLE *ct); 

// Open a file and bind the file to a new cachefile object. (For use by test programs only.)
int toku_cachetable_openf(CACHEFILE *,CACHETABLE, const char *fname_in_env, int flags, mode_t mode);

// Bind a file to a new cachefile object.
int toku_cachetable_openfd(CACHEFILE *,CACHETABLE, int fd, 
                            const char *fname_relative_to_env);
int toku_cachetable_openfd_with_filenum (CACHEFILE *,CACHETABLE, int fd, 
                                         const char *fname_in_env,
                                         FILENUM filenum, bool* was_open);

// reserve a unique filenum
FILENUM toku_cachetable_reserve_filenum(CACHETABLE ct);

// Effect: Reserve a fraction of the cachetable memory.
// Returns the amount reserved.
// To return the memory to the cachetable, call toku_cachetable_release_reserved_memory
// Requires 0<fraction<1.
uint64_t toku_cachetable_reserve_memory(CACHETABLE, double fraction, uint64_t upper_bound);
void toku_cachetable_release_reserved_memory(CACHETABLE, uint64_t);

// cachefile operations

// Does an fsync of a cachefile.
void toku_cachefile_fsync(CACHEFILE cf);

enum partial_eviction_cost {
    PE_CHEAP=0, // running partial eviction is cheap, and can be done on the client thread
    PE_EXPENSIVE=1, // running partial eviction is expensive, and should not be done on the client thread
};

// cachetable pair clean or dirty WRT external memory
enum cachetable_dirty {
    CACHETABLE_CLEAN=0, // the cached object is clean WRT the cachefile
    CACHETABLE_DIRTY=1, // the cached object is dirty WRT the cachefile
};

// The flush callback is called when a key value pair is being written to storage and possibly removed from the cachetable.
// When write_me is true, the value should be written to storage.
// When keep_me is false, the value should be freed.
// When for_checkpoint is true, this was a 'pending' write
// Returns: 0 if success, otherwise an error number.
// Can access fd (fd is protected by a readlock during call)
typedef void (*CACHETABLE_FLUSH_CALLBACK)(CACHEFILE, int fd, CACHEKEY key, void *value, void **disk_data, void *write_extraargs, PAIR_ATTR size, PAIR_ATTR* new_size, bool write_me, bool keep_me, bool for_checkpoint, bool is_clone);

// The fetch callback is called when a thread is attempting to get and pin a memory
// object and it is not in the cachetable.
// Returns: 0 if success, otherwise an error number.  The address and size of the object
// associated with the key are returned.
// Can access fd (fd is protected by a readlock during call)
typedef int (*CACHETABLE_FETCH_CALLBACK)(CACHEFILE, PAIR p, int fd, CACHEKEY key, uint32_t fullhash, void **value_data, void **disk_data, PAIR_ATTR *sizep, int *dirtyp, void *read_extraargs);

// The cachetable calls the partial eviction estimate callback to determine if 
// partial eviction is a cheap operation that may be called by on the client thread
// or whether partial eviction is expensive and should be done on a background (writer) thread.
// The callback conveys this information by setting cost to either PE_CHEAP or PE_EXPENSIVE.
// If cost is PE_EXPENSIVE, then the callback also sets bytes_freed_estimate 
// to return an estimate of the number of bytes it will free
// so that the cachetable can estimate how much data is being evicted on background threads.
// If cost is PE_CHEAP, then the callback does not set bytes_freed_estimate.
typedef void (*CACHETABLE_PARTIAL_EVICTION_EST_CALLBACK)(void *ftnode_pv, void* disk_data, long* bytes_freed_estimate, enum partial_eviction_cost *cost, void *write_extraargs);

// The cachetable calls the partial eviction callback is to possibly try and partially evict pieces
// of the PAIR. The callback determines the strategy for what to evict. The callback may choose to free
// nothing, or may choose to free as much as possible. When the partial eviction callback is finished,
// it must call finalize with the new PAIR_ATTR and the given finalize_extra. After this point, the
// write lock will be released on the PAIR and it is no longer safe to operate on any of the passed arguments.
// This is useful for doing expensive cleanup work outside of the PAIR's write lock (such as destroying objects, etc)
//
// on entry, requires a write lock to be held on the PAIR in the cachetable while this function is called
// on exit, the finalize continuation is called
typedef int (*CACHETABLE_PARTIAL_EVICTION_CALLBACK)(void *ftnode_pv, PAIR_ATTR old_attr, void *write_extraargs,
                                                    void (*finalize)(PAIR_ATTR new_attr, void *extra), void *finalize_extra);

// The cachetable calls this function to determine if get_and_pin call requires a partial fetch. If this function returns true, 
// then the cachetable will subsequently call CACHETABLE_PARTIAL_FETCH_CALLBACK to perform
// a partial fetch. If this function returns false, then the PAIR's value is returned to the caller as is.
//
// An alternative to having this callback is to always call CACHETABLE_PARTIAL_FETCH_CALLBACK, and let
// CACHETABLE_PARTIAL_FETCH_CALLBACK decide whether to do any partial fetching or not.
// There is no particular reason why this alternative was not chosen.
// Requires: a read lock to be held on the PAIR
typedef bool (*CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK)(void *ftnode_pv, void *read_extraargs);

// The cachetable calls the partial fetch callback when a thread needs to read or decompress a subset of a PAIR into memory.
// An example is needing to read a basement node into memory. Another example is decompressing an internal node's
// message buffer. The cachetable determines if a partial fetch is necessary by first calling CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK.
// The new PAIR_ATTR of the PAIR is returned in sizep
// Can access fd (fd is protected by a readlock during call)
// Returns: 0 if success, otherwise an error number.  
typedef int (*CACHETABLE_PARTIAL_FETCH_CALLBACK)(void *value_data, void* disk_data, void *read_extraargs, int fd, PAIR_ATTR *sizep);

// The cachetable calls the put callback during a cachetable_put command to provide the opaque PAIR.
// The PAIR can then be used to later unpin the pair.
// Returns: 0 if success, otherwise an error number.  
typedef void (*CACHETABLE_PUT_CALLBACK)(CACHEKEY key, void *value_data, PAIR p);

// TODO(leif) XXX TODO XXX
typedef int (*CACHETABLE_CLEANER_CALLBACK)(void *ftnode_pv, BLOCKNUM blocknum, uint32_t fullhash, void *write_extraargs);

typedef void (*CACHETABLE_CLONE_CALLBACK)(void* value_data, void** cloned_value_data, long* clone_size, PAIR_ATTR* new_attr, bool for_checkpoint, void* write_extraargs);

typedef void (*CACHETABLE_CHECKPOINT_COMPLETE_CALLBACK)(void *value_data);

typedef struct {
    CACHETABLE_FLUSH_CALLBACK flush_callback;
    CACHETABLE_PARTIAL_EVICTION_EST_CALLBACK pe_est_callback;
    CACHETABLE_PARTIAL_EVICTION_CALLBACK pe_callback; 
    CACHETABLE_CLEANER_CALLBACK cleaner_callback;
    CACHETABLE_CLONE_CALLBACK clone_callback;
    CACHETABLE_CHECKPOINT_COMPLETE_CALLBACK checkpoint_complete_callback;
    void* write_extraargs; // parameter for flush_callback, pe_est_callback, pe_callback, and cleaner_callback
} CACHETABLE_WRITE_CALLBACK;

typedef void (*CACHETABLE_GET_KEY_AND_FULLHASH)(CACHEKEY* cachekey, uint32_t* fullhash, void* extra);

typedef void (*CACHETABLE_REMOVE_KEY)(CACHEKEY* cachekey, bool for_checkpoint, void* extra);

void toku_cachefile_set_userdata(CACHEFILE cf, void *userdata,
    void (*log_fassociate_during_checkpoint)(CACHEFILE, void*),
    void (*close_userdata)(CACHEFILE, int, void*, bool, LSN),
    void (*free_userdata)(CACHEFILE, void*),
    void (*checkpoint_userdata)(CACHEFILE, int, void*),
    void (*begin_checkpoint_userdata)(LSN, void*),
    void (*end_checkpoint_userdata)(CACHEFILE, int, void*),
    void (*note_pin_by_checkpoint)(CACHEFILE, void*),
    void (*note_unpin_by_checkpoint)(CACHEFILE, void*));
// Effect: Store some cachefile-specific user data.  When the last reference to a cachefile is closed, we call close_userdata().
// Before starting a checkpoint, we call checkpoint_prepare_userdata().
// When the cachefile needs to be checkpointed, we call checkpoint_userdata().
// If userdata is already non-NULL, then we simply overwrite it.

void *toku_cachefile_get_userdata(CACHEFILE);
// Effect: Get the user data.

CACHETABLE toku_cachefile_get_cachetable(CACHEFILE cf);
// Effect: Get the cachetable.

void toku_cachetable_swap_pair_values(PAIR old_pair, PAIR new_pair);
// Effect: Swaps the value_data of old_pair and new_pair. 
// Requires: both old_pair and new_pair to be pinned with write locks.

typedef enum {
    PL_READ = 0,
    PL_WRITE_CHEAP,
    PL_WRITE_EXPENSIVE
} pair_lock_type;

// put something into the cachetable and checkpoint dependent pairs
// if the checkpointing is necessary
void toku_cachetable_put_with_dep_pairs(
    CACHEFILE cachefile,
    CACHETABLE_GET_KEY_AND_FULLHASH get_key_and_fullhash,
    void *value,
    PAIR_ATTR attr,
    CACHETABLE_WRITE_CALLBACK write_callback,
    void *get_key_and_fullhash_extra,
    uint32_t num_dependent_pairs, // number of dependent pairs that we may need to checkpoint
    PAIR* dependent_pairs,
    enum cachetable_dirty* dependent_dirty, // array stating dirty/cleanness of dependent pairs
    CACHEKEY* key,
    uint32_t* fullhash,
    CACHETABLE_PUT_CALLBACK put_callback
    );

// Put a memory object into the cachetable.
// Effects: Lookup the key in the cachetable. If the key is not in the cachetable,
// then insert the pair and pin it. Otherwise return an error.  Some of the key
// value pairs may be evicted from the cachetable when the cachetable gets too big.
void toku_cachetable_put(CACHEFILE cf, CACHEKEY key, uint32_t fullhash,
			void *value, PAIR_ATTR size,
			CACHETABLE_WRITE_CALLBACK write_callback,
                        CACHETABLE_PUT_CALLBACK put_callback
                        );

// Get and pin the memory object of a PAIR, and write dependent pairs to disk
// if the dependent pairs are pending a checkpoint.
// Effects: If the memory object is in the cachetable, acquire a PAIR lock on it.
// Otherwise, fetch it from storage by calling the fetch callback.  If the fetch
// succeeded, add the memory object to the cachetable with a PAIR lock on it.
// Before returning to the user, if the PAIR object being retrieved, or any of the
// dependent pairs passed in as parameters must be written to disk for checkpoint,
// then the required PAIRs are written to disk for checkpoint.
// KEY PROPERTY OF DEPENDENT PAIRS: They are already locked by the client
// Returns: 0 if the memory object is in memory, otherwise an error number.
// Rationale:
//   begin_batched_pin and end_batched_pin take and release a read lock on the pair list.
//   Normally, that would be done within this get_and_pin, but we want to pin multiple nodes with a single acquisition of the read lock.
int toku_cachetable_get_and_pin_with_dep_pairs_batched (
    CACHEFILE cachefile,
    CACHEKEY key,
    uint32_t fullhash,
    void**value,
    long *sizep,
    CACHETABLE_WRITE_CALLBACK write_callback,
    CACHETABLE_FETCH_CALLBACK fetch_callback,
    CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK pf_req_callback,
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback,
    pair_lock_type lock_type,
    void* read_extraargs, // parameter for fetch_callback, pf_req_callback, and pf_callback
    uint32_t num_dependent_pairs, // number of dependent pairs that we may need to checkpoint
    PAIR* dependent_pairs,
    enum cachetable_dirty* dependent_dirty // array stating dirty/cleanness of dependent pairs
    );

// Effect: call toku_cachetable_get_and_pin_with_dep_pairs_batched once,
//  wrapped in begin_batched_pin and end_batched_pin calls.
int toku_cachetable_get_and_pin_with_dep_pairs (
    CACHEFILE cachefile,
    CACHEKEY key,
    uint32_t fullhash,
    void**value,
    long *sizep,
    CACHETABLE_WRITE_CALLBACK write_callback,
    CACHETABLE_FETCH_CALLBACK fetch_callback,
    CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK pf_req_callback,
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback,
    pair_lock_type lock_type,
    void* read_extraargs, // parameter for fetch_callback, pf_req_callback, and pf_callback
    uint32_t num_dependent_pairs, // number of dependent pairs that we may need to checkpoint
    PAIR* dependent_pairs,
    enum cachetable_dirty* dependent_dirty // array stating dirty/cleanness of dependent pairs
    );


// Get and pin a memory object.
// Effects: If the memory object is in the cachetable acquire the PAIR lock on it.
// Otherwise, fetch it from storage by calling the fetch callback.  If the fetch
// succeeded, add the memory object to the cachetable with a read lock on it.
// Returns: 0 if the memory object is in memory, otherwise an error number.
int toku_cachetable_get_and_pin (
    CACHEFILE cachefile, 
    CACHEKEY key, 
    uint32_t fullhash, 
    void**value, 
    long *sizep,
    CACHETABLE_WRITE_CALLBACK write_callback,
    CACHETABLE_FETCH_CALLBACK fetch_callback, 
    CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK pf_req_callback,
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback,
    bool may_modify_value,
    void* read_extraargs // parameter for fetch_callback, pf_req_callback, and pf_callback
    );

// does partial fetch on a pinned pair
void toku_cachetable_pf_pinned_pair(
    void* value,
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback,
    void* read_extraargs,
    CACHEFILE cf,
    CACHEKEY key,
    uint32_t fullhash
    ); 

struct unlockers {
    bool       locked;
    void (*f)(void* extra);
    void      *extra;
    UNLOCKERS  next;
};

// Effect:  If the block is in the cachetable, then return it.
//   Otherwise call the functions in unlockers, fetch the data (but don't pin it, since we'll just end up pinning it again later), and return TOKUDB_TRY_AGAIN.
// Rationale:
//   begin_batched_pin and end_batched_pin take and release a read lock on the pair list.
//   Normally, that would be done within this get_and_pin, but we want to pin multiple nodes with a single acquisition of the read lock.
int toku_cachetable_get_and_pin_nonblocking_batched (
    CACHEFILE cf,
    CACHEKEY key,
    uint32_t fullhash,
    void**value,
    long *sizep,
    CACHETABLE_WRITE_CALLBACK write_callback,
    CACHETABLE_FETCH_CALLBACK fetch_callback,
    CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK pf_req_callback,
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback,
    pair_lock_type lock_type,
    void *read_extraargs, // parameter for fetch_callback, pf_req_callback, and pf_callback
    UNLOCKERS unlockers
    );

// Effect: call toku_cachetable_get_and_pin_nonblocking_batched once,
//  wrapped in begin_batched_pin and end_batched_pin calls.
int toku_cachetable_get_and_pin_nonblocking (
    CACHEFILE cf,
    CACHEKEY key,
    uint32_t fullhash,
    void**value,
    long *sizep,
    CACHETABLE_WRITE_CALLBACK write_callback,
    CACHETABLE_FETCH_CALLBACK fetch_callback,
    CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK pf_req_callback  __attribute__((unused)),
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback  __attribute__((unused)),
    pair_lock_type lock_type,
    void *read_extraargs, // parameter for fetch_callback, pf_req_callback, and pf_callback
    UNLOCKERS unlockers
    );

int toku_cachetable_maybe_get_and_pin (CACHEFILE, CACHEKEY, uint32_t /*fullhash*/, pair_lock_type, void**);
// Effect: Maybe get and pin a memory object.
//  This function is similar to the get_and_pin function except that it
//  will not attempt to fetch a memory object that is not in the cachetable or requires any kind of blocking to get it.  
// Returns: If the the item is already in memory, then return 0 and store it in the
// void**.  If the item is not in memory, then return a nonzero error number.

int toku_cachetable_maybe_get_and_pin_clean (CACHEFILE, CACHEKEY, uint32_t /*fullhash*/, pair_lock_type, void**);
// Effect: Like maybe get and pin, but may pin a clean pair.

int toku_cachetable_unpin(CACHEFILE, PAIR, enum cachetable_dirty dirty, PAIR_ATTR size);
// Effect: Unpin a memory object
// Modifies: If the memory object is in the cachetable, then OR the dirty flag,
// update the size, and release the read lock on the memory object.
// Returns: 0 if success, otherwise returns an error number.
// Requires: The ct is locked.

int toku_cachetable_unpin_ct_prelocked_no_flush(CACHEFILE, PAIR, enum cachetable_dirty dirty, PAIR_ATTR size);
// Effect: The same as tokud_cachetable_unpin, except that the ct must not be locked.
// Requires: The ct is NOT locked.

int toku_cachetable_unpin_and_remove (CACHEFILE, PAIR, CACHETABLE_REMOVE_KEY, void*); /* Removing something already present is OK. */
// Effect: Remove an object from the cachetable.  Don't write it back.
// Requires: The object must be pinned exactly once.

// test-only wrapper that use CACHEKEY and fullhash
int toku_test_cachetable_unpin(CACHEFILE, CACHEKEY, uint32_t fullhash, enum cachetable_dirty dirty, PAIR_ATTR size);

// test-only wrapper that use CACHEKEY and fullhash
int toku_test_cachetable_unpin_ct_prelocked_no_flush(CACHEFILE, CACHEKEY, uint32_t fullhash, enum cachetable_dirty dirty, PAIR_ATTR size);

// test-only wrapper that use CACHEKEY
int toku_test_cachetable_unpin_and_remove (CACHEFILE, CACHEKEY, CACHETABLE_REMOVE_KEY, void*); /* Removing something already present is OK. */

int toku_cachefile_prefetch(CACHEFILE cf, CACHEKEY key, uint32_t fullhash,
                            CACHETABLE_WRITE_CALLBACK write_callback,
                            CACHETABLE_FETCH_CALLBACK fetch_callback,
                            CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK pf_req_callback,
                            CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback,
                            void *read_extraargs, // parameter for fetch_callback, pf_req_callback, and pf_callback 
                            bool *doing_prefetch);
// Effect: Prefetch a memory object for a given key into the cachetable
// Precondition: The cachetable mutex is NOT held.
// Postcondition: The cachetable mutex is NOT held.
// Returns: 0 if success
// Implement Note: 
//  1) The pair's rwlock is acquired (for write) (there is not a deadlock here because the rwlock is a pthread_cond_wait using the cachetable mutex).  
//  Case A:  Single-threaded.
//    A1)  Call cachetable_fetch_pair, which
//      a) Obtains a readlock on the cachefile's fd (to prevent multipler readers at once)
//      b) Unlocks the cachetable
//      c) Does the fetch off disk.
//      d) Locks the cachetable
//      e) Unlocks the fd lock.
//      f) Unlocks the pair rwlock.
//  Case B: Multithreaded
//      a) Enqueue a cachetable_reader into the workqueue.
//      b) Unlock the cache table.
//      c) The enqueue'd job later locks the cachetable, and calls cachetable_fetch_pair (doing the steps in A1 above).

int toku_cachetable_assert_all_unpinned (CACHETABLE);

int toku_cachefile_count_pinned (CACHEFILE, int /*printthem*/ );

// Close the cachefile.
// Effects: All of the cached object associated with the cachefile are evicted from
// the cachetable.  The flush callback is called for each of these objects.  The
// close function does not return until all of the objects are evicted.  The cachefile
// object is freed.
// If oplsn_valid is true then use oplsn as the LSN of the close instead of asking the logger.  oplsn_valid being true is only allowed during recovery, and requires that you are removing the last reference (otherwise the lsn wouldn't make it in.)
void toku_cachefile_close (CACHEFILE*, bool oplsn_valid, LSN oplsn);

// Return on success (different from pread and pwrite)
//int cachefile_pwrite (CACHEFILE, const void *buf, size_t count, toku_off_t offset);
//int cachefile_pread  (CACHEFILE, void *buf, size_t count, toku_off_t offset);

// Get the file descriptor associated with the cachefile
// Return the file descriptor
// Grabs a read lock protecting the fd
int toku_cachefile_get_fd (CACHEFILE);

// Get the iname (within the environment) associated with the cachefile
// Return the filename
char * toku_cachefile_fname_in_env (CACHEFILE cf);

// Make it so when the cachefile closes, the underlying file is unlinked
void toku_cachefile_unlink_on_close(CACHEFILE cf);

// is this cachefile marked as unlink on close?
bool toku_cachefile_is_unlink_on_close(CACHEFILE cf);

// Return the logger associated with the cachefile
TOKULOGGER toku_cachefile_logger (CACHEFILE);

// Return the filenum associated with the cachefile
FILENUM toku_cachefile_filenum (CACHEFILE);

// Effect: Return a 32-bit hash key.  The hash key shall be suitable for using with bitmasking for a table of size power-of-two.
uint32_t toku_cachetable_hash (CACHEFILE cachefile, CACHEKEY key);

uint32_t toku_cachefile_fullhash_of_header (CACHEFILE cachefile);

// debug functions

// Print the contents of the cachetable. This is mainly used from gdb
void toku_cachetable_print_state (CACHETABLE ct);

// Get the state of the cachetable. This is used to verify the cachetable
void toku_cachetable_get_state(CACHETABLE ct, int *num_entries_ptr, int *hash_size_ptr, long *size_current_ptr, long *size_limit_ptr);

// Get the state of a cachetable entry by key. This is used to verify the cachetable
int toku_cachetable_get_key_state(CACHETABLE ct, CACHEKEY key, CACHEFILE cf,
                                  void **value_ptr,
				  int *dirty_ptr,
                                  long long *pin_ptr,
                                  long *size_ptr);

// Verify the whole cachetable that the cachefile is in.  Slow.
void toku_cachefile_verify (CACHEFILE cf);

// Verify the cachetable. Slow.
void toku_cachetable_verify (CACHETABLE t);

// Not for use in production, but useful for testing.
void toku_cachetable_print_hash_histogram (void) __attribute__((__visibility__("default")));

void toku_cachetable_maybe_flush_some(CACHETABLE ct);

// for stat64
uint64_t toku_cachefile_size(CACHEFILE cf);

typedef enum {
    CT_MISS = 0,
    CT_MISSTIME,               // how many usec spent waiting for disk read because of cache miss
    CT_PREFETCHES,             // how many times has a block been prefetched into the cachetable?
    CT_SIZE_CURRENT,           // the sum of the sizes of the nodes represented in the cachetable
    CT_SIZE_LIMIT,             // the limit to the sum of the node sizes
    CT_SIZE_WRITING,           // the sum of the sizes of the nodes being written
    CT_SIZE_NONLEAF,           // number of bytes in cachetable belonging to nonleaf nodes
    CT_SIZE_LEAF,              // number of bytes in cachetable belonging to leaf nodes
    CT_SIZE_ROLLBACK,          // number of bytes in cachetable belonging to rollback nodes
    CT_SIZE_CACHEPRESSURE,     // number of bytes causing cache pressure (sum of buffers and workdone counters)
    CT_EVICTIONS,
    CT_CLEANER_EXECUTIONS,     // number of times the cleaner thread's loop has executed
    CT_CLEANER_PERIOD,
    CT_CLEANER_ITERATIONS,     // number of times the cleaner thread runs the cleaner per period
    CT_WAIT_PRESSURE_COUNT,
    CT_WAIT_PRESSURE_TIME,
    CT_LONG_WAIT_PRESSURE_COUNT,
    CT_LONG_WAIT_PRESSURE_TIME,
    CT_STATUS_NUM_ROWS
} ct_status_entry;

typedef struct {
    bool initialized;
    TOKU_ENGINE_STATUS_ROW_S status[CT_STATUS_NUM_ROWS];
} CACHETABLE_STATUS_S, *CACHETABLE_STATUS;

void toku_cachetable_get_status(CACHETABLE ct, CACHETABLE_STATUS s);

void toku_cachetable_set_env_dir(CACHETABLE ct, const char *env_dir);
char * toku_construct_full_name(int count, ...);
char * toku_cachetable_get_fname_in_cwd(CACHETABLE ct, const char * fname_in_env);

void cachefile_kibbutz_enq (CACHEFILE cf, void (*f)(void*), void *extra);
// Effect: Add a job to the cachetable's collection of work to do.  Note that function f must call remove_background_job_from_cf()

void remove_background_job_from_cf (CACHEFILE cf);
// Effect: When a kibbutz job or cleaner thread finishes in a cachefile,
// the cachetable must be notified.

// test-only function
int toku_cachetable_get_checkpointing_user_data_status(void);

// test-only function
int toku_cleaner_thread_for_test(CACHETABLE ct);
int toku_cleaner_thread(void *cleaner_v);

// test function. Exported in the ydb layer and used by tests that want to run DRD
// The default of 1M is too high for drd tests, so this is a mechanism to set a smaller number.
void toku_pair_list_set_lock_size(uint32_t num_locks);

// Used by ft-ops.cc to figure out if it has the write lock on a pair.
// Pretty hacky and not accurate enough, should be improved at the frwlock
// layer.
__attribute__((const,nonnull))
bool toku_ctpair_is_write_locked(PAIR pair);

#endif /* CACHETABLE_H */
