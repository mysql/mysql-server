/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef CACHETABLE_H
#define CACHETABLE_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <fcntl.h>
#include "brttypes.h"
#include "workqueue.h"

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

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


int toku_set_cleaner_period (CACHETABLE ct, u_int32_t new_period);
u_int32_t toku_get_cleaner_period (CACHETABLE ct);
u_int32_t toku_get_cleaner_period_unlocked (CACHETABLE ct);
int toku_set_cleaner_iterations (CACHETABLE ct, u_int32_t new_iterations);
u_int32_t toku_get_cleaner_iterations (CACHETABLE ct);
u_int32_t toku_get_cleaner_iterations_unlocked (CACHETABLE ct);

// cachetable operations

int toku_create_cachetable(CACHETABLE */*result*/, long size_limit, LSN initial_lsn, TOKULOGGER);
// Create a new cachetable.
// Effects: a new cachetable is created and initialized.
// The cachetable pointer is stored into result.
// The sum of the sizes of the memory objects is set to size_limit, in whatever
// units make sense to the user of the cachetable.
// Returns: If success, returns 0 and result points to the new cachetable. Otherwise,
// returns an error number.

// What is the cachefile that goes with a particular filenum?
// During a transaction, we cannot reuse a filenum.
int toku_cachefile_of_filenum (CACHETABLE t, FILENUM filenum, CACHEFILE *cf);

// What is the cachefile that goes with a particular iname (relative to env)?
// During a transaction, we cannot reuse an iname.
int toku_cachefile_of_iname_in_env (CACHETABLE ct, const char *iname_in_env, CACHEFILE *cf);

// Get the iname (within the cwd) associated with the cachefile
// Return the filename
char * toku_cachefile_fname_in_cwd (CACHEFILE cf);

// return value of the cachefile is_closing flag
BOOL toku_cachefile_is_closing(CACHEFILE cf);

// TODO: #1510  Add comments on how these behave
int toku_cachetable_begin_checkpoint (CACHETABLE ct, TOKULOGGER);
int toku_cachetable_end_checkpoint(CACHETABLE ct, TOKULOGGER logger, 
                                   void (*ydb_lock)(void), void (*ydb_unlock)(void),
                                   void (*testcallback_f)(void*),  void * testextra);

// Shuts down checkpoint thread
// Requires no locks be held that are taken by the checkpoint function
void toku_cachetable_minicron_shutdown(CACHETABLE ct);

// Wait for the cachefile's background work to finish.
void toku_cachefile_wait_for_background_work_to_quiesce(CACHEFILE cf);

// Close the cachetable.
// Effects: All of the memory objects are flushed to disk, and the cachetable is destroyed.
int toku_cachetable_close (CACHETABLE*); /* Flushes everything to disk, and destroys the cachetable. */

// Open a file and bind the file to a new cachefile object. (For use by test programs only.)
int toku_cachetable_openf (CACHEFILE *,CACHETABLE, const char */*fname_in_env*/, int flags, mode_t mode);

// Returns the limit on the cachetable size
uint64_t toku_cachetable_get_size_limit(CACHETABLE ct);

// Bind a file to a new cachefile object.
int toku_cachetable_openfd (CACHEFILE *,CACHETABLE, int /*fd*/, 
			    const char *fname_relative_to_env); /*(used for logging)*/
int toku_cachetable_openfd_with_filenum (CACHEFILE *,CACHETABLE, int /*fd*/, 
					 const char *fname_in_env,
					 BOOL with_filenum, FILENUM filenum, BOOL reserved);

int toku_cachetable_reserve_filenum (CACHETABLE ct, FILENUM *reserved_filenum, BOOL with_filenum, FILENUM filenum);

void toku_cachetable_unreserve_filenum (CACHETABLE ct, FILENUM reserved_filenum);

// Effect: Reserve a fraction of the cachetable memory.
// Returns the amount reserved.
// To return the memory to the cachetable, call toku_cachetable_release_reserved_memory
// Requires 0<fraction<1.
uint64_t toku_cachetable_reserve_memory(CACHETABLE, double fraction);
void toku_cachetable_release_reserved_memory(CACHETABLE, uint64_t);

// Get access to the asynchronous work queue
// Returns: a pointer to the work queue
WORKQUEUE toku_cachetable_get_workqueue (CACHETABLE);

// cachefile operations

void toku_cachefile_get_workqueue_load (CACHEFILE, int *n_in_queue, int *n_threads);

// Does an fsync of a cachefile.
// Handles the case where cf points to /dev/null
int toku_cachefile_fsync(CACHEFILE cf);

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
typedef void (*CACHETABLE_FLUSH_CALLBACK)(CACHEFILE, int fd, CACHEKEY key, void *value, void *write_extraargs, PAIR_ATTR size, PAIR_ATTR* new_size, BOOL write_me, BOOL keep_me, BOOL for_checkpoint);

// The fetch callback is called when a thread is attempting to get and pin a memory
// object and it is not in the cachetable.
// Returns: 0 if success, otherwise an error number.  The address and size of the object
// associated with the key are returned.
// Can access fd (fd is protected by a readlock during call)
typedef int (*CACHETABLE_FETCH_CALLBACK)(CACHEFILE, int fd, CACHEKEY key, u_int32_t fullhash, void **value, PAIR_ATTR *sizep, int *dirtyp, void *read_extraargs);

// The cachetable calls the partial eviction estimate callback to determine if 
// partial eviction is a cheap operation that may be called by on the client thread
// or whether partial eviction is expensive and should be done on a background (writer) thread.
// The callback conveys this information by setting cost to either PE_CHEAP or PE_EXPENSIVE.
// If cost is PE_EXPENSIVE, then the callback also sets bytes_freed_estimate 
// to return an estimate of the number of bytes it will free
// so that the cachetable can estimate how much data is being evicted on background threads.
// If cost is PE_CHEAP, then the callback does not set bytes_freed_estimate.
typedef void (*CACHETABLE_PARTIAL_EVICTION_EST_CALLBACK)(void *brtnode_pv, long* bytes_freed_estimate, enum partial_eviction_cost *cost, void *write_extraargs);

// The cachetable calls the partial eviction callback is to possibly try and partially evict pieces
// of the PAIR. The callback determines the strategy for what to evict. The callback may choose to free
// nothing, or may choose to free as much as possible.
// old_attr is the PAIR_ATTR of the PAIR when the callback is called. 
// new_attr is set to the new PAIR_ATTR after the callback executes partial eviction
// Requires a write lock to be held on the PAIR in the cachetable while this function is called
typedef int (*CACHETABLE_PARTIAL_EVICTION_CALLBACK)(void *brtnode_pv, PAIR_ATTR old_attr, PAIR_ATTR* new_attr, void *write_extraargs);

// The cachetable calls this function to determine if get_and_pin call requires a partial fetch. If this function returns TRUE, 
// then the cachetable will subsequently call CACHETABLE_PARTIAL_FETCH_CALLBACK to perform
// a partial fetch. If this function returns FALSE, then the PAIR's value is returned to the caller as is.
//
// An alternative to having this callback is to always call CACHETABLE_PARTIAL_FETCH_CALLBACK, and let
// CACHETABLE_PARTIAL_FETCH_CALLBACK decide whether to possibly release the ydb lock and perform I/O.
// There is no particular reason why this alternative was not chosen.
// Requires: a read lock to be held on the PAIR
typedef BOOL (*CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK)(void *brtnode_pv, void *read_extraargs);

// The cachetable calls the partial fetch callback when a thread needs to read or decompress a subset of a PAIR into memory.
// An example is needing to read a basement node into memory. Another example is decompressing an internal node's
// message buffer. The cachetable determines if a partial fetch is necessary by first calling CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK.
// The new PAIR_ATTR of the PAIR is returned in sizep
// Can access fd (fd is protected by a readlock during call)
// Returns: 0 if success, otherwise an error number.  
typedef int (*CACHETABLE_PARTIAL_FETCH_CALLBACK)(void *brtnode_pv, void *read_extraargs, int fd, PAIR_ATTR *sizep);

// TODO(leif) XXX TODO XXX
typedef int (*CACHETABLE_CLEANER_CALLBACK)(void *brtnode_pv, BLOCKNUM blocknum, u_int32_t fullhash, void *write_extraargs);


typedef void (*CACHETABLE_GET_KEY_AND_FULLHASH)(CACHEKEY* cachekey, u_int32_t* fullhash, void* extra);

void toku_cachefile_set_userdata(CACHEFILE cf, void *userdata,
    int (*log_fassociate_during_checkpoint)(CACHEFILE, void*),
    int (*log_suppress_rollback_during_checkpoint)(CACHEFILE, void*),
    int (*close_userdata)(CACHEFILE, int, void*, char **/*error_string*/, BOOL, LSN),
    int (*checkpoint_userdata)(CACHEFILE, int, void*),
    int (*begin_checkpoint_userdata)(CACHEFILE, int, LSN, void*),
    int (*end_checkpoint_userdata)(CACHEFILE, int, void*),
    int (*note_pin_by_checkpoint)(CACHEFILE, void*),
    int (*note_unpin_by_checkpoint)(CACHEFILE, void*));
// Effect: Store some cachefile-specific user data.  When the last reference to a cachefile is closed, we call close_userdata().
// Before starting a checkpoint, we call checkpoint_prepare_userdata().
// When the cachefile needs to be checkpointed, we call checkpoint_userdata().
// If userdata is already non-NULL, then we simply overwrite it.

void *toku_cachefile_get_userdata(CACHEFILE);
// Effect: Get the user data.

CACHETABLE toku_cachefile_get_cachetable(CACHEFILE cf);
// Effect: Get the cachetable.

void toku_checkpoint_pairs(
    CACHEFILE cf,
    u_int32_t num_dependent_pairs, // number of dependent pairs that we may need to checkpoint
    CACHEFILE* dependent_cfs, // array of cachefiles of dependent pairs
    CACHEKEY* dependent_keys, // array of cachekeys of dependent pairs
    u_int32_t* dependent_fullhash, //array of fullhashes of dependent pairs
    enum cachetable_dirty* dependent_dirty // array stating dirty/cleanness of dependent pairs
    );


// put something into the cachetable and checkpoint dependent pairs
// if the checkpointing is necessary
int toku_cachetable_put_with_dep_pairs(
    CACHEFILE cachefile, 
    CACHETABLE_GET_KEY_AND_FULLHASH get_key_and_fullhash,
    void*value, 
    PAIR_ATTR attr,
    CACHETABLE_FLUSH_CALLBACK flush_callback,
    CACHETABLE_PARTIAL_EVICTION_EST_CALLBACK pe_est_callback,
    CACHETABLE_PARTIAL_EVICTION_CALLBACK pe_callback, 
    CACHETABLE_CLEANER_CALLBACK cleaner_callback,
    void *write_extraargs, // parameter for flush_callback, pe_est_callback, pe_callback, and cleaner_callback
    void *get_key_and_fullhash_extra,
    u_int32_t num_dependent_pairs, // number of dependent pairs that we may need to checkpoint
    CACHEFILE* dependent_cfs, // array of cachefiles of dependent pairs
    CACHEKEY* dependent_keys, // array of cachekeys of dependent pairs
    u_int32_t* dependent_fullhash, //array of fullhashes of dependent pairs
    enum cachetable_dirty* dependent_dirty, // array stating dirty/cleanness of dependent pairs
    CACHEKEY* key,
    u_int32_t* fullhash
    );


// Put a memory object into the cachetable.
// Effects: Lookup the key in the cachetable. If the key is not in the cachetable,
// then insert the pair and pin it. Otherwise return an error.  Some of the key
// value pairs may be evicted from the cachetable when the cachetable gets too big.
// Returns: 0 if the memory object is placed into the cachetable, otherwise an
// error number.
int toku_cachetable_put(CACHEFILE cf, CACHEKEY key, u_int32_t fullhash,
			void *value, PAIR_ATTR size,
			CACHETABLE_FLUSH_CALLBACK flush_callback,
			CACHETABLE_PARTIAL_EVICTION_EST_CALLBACK pe_est_callback,
                        CACHETABLE_PARTIAL_EVICTION_CALLBACK pe_callback,
                        CACHETABLE_CLEANER_CALLBACK cleaner_callback,
                        void *write_extraargs // parameter for flush_callback, pe_est_callback, pe_callback, and cleaner_callback
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
int toku_cachetable_get_and_pin_with_dep_pairs (
    CACHEFILE cachefile, 
    CACHEKEY key, 
    u_int32_t fullhash, 
    void**value, 
    long *sizep,
    CACHETABLE_FLUSH_CALLBACK flush_callback, 
    CACHETABLE_FETCH_CALLBACK fetch_callback, 
    CACHETABLE_PARTIAL_EVICTION_EST_CALLBACK pe_est_callback,
    CACHETABLE_PARTIAL_EVICTION_CALLBACK pe_callback, 
    CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK pf_req_callback,
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback,
    CACHETABLE_CLEANER_CALLBACK cleaner_callback,
    void* read_extraargs, // parameter for fetch_callback, pf_req_callback, and pf_callback
    void* write_extraargs, // parameter for flush_callback, pe_est_callback, pe_callback, and cleaner_callback
    u_int32_t num_dependent_pairs, // number of dependent pairs that we may need to checkpoint
    CACHEFILE* dependent_cfs, // array of cachefiles of dependent pairs
    CACHEKEY* dependent_keys, // array of cachekeys of dependent pairs
    u_int32_t* dependent_fullhash, //array of fullhashes of dependent pairs
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
    u_int32_t fullhash, 
    void**value, 
    long *sizep,
    CACHETABLE_FLUSH_CALLBACK flush_callback, 
    CACHETABLE_FETCH_CALLBACK fetch_callback, 
    CACHETABLE_PARTIAL_EVICTION_EST_CALLBACK pe_est_callback,
    CACHETABLE_PARTIAL_EVICTION_CALLBACK pe_callback, 
    CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK pf_req_callback  __attribute__((unused)),
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback  __attribute__((unused)),
    CACHETABLE_CLEANER_CALLBACK cleaner_callback,
    void* read_extraargs, // parameter for fetch_callback, pf_req_callback, and pf_callback
    void* write_extraargs // parameter for flush_callback, pe_est_callback, pe_callback, and cleaner_callback
    );

struct unlockers {
    BOOL       locked;
    void (*f)(void*extra);
    void      *extra;
    UNLOCKERS  next;
};

// Effect:  If the block is in the cachetable, then return it. 
//   Otherwise call the release_lock_callback, call the functions in unlockers, fetch the data (but don't pin it, since we'll just end up pinning it again later),
//   and return TOKU_DB_TRYAGAIN.
int toku_cachetable_get_and_pin_nonblocking (
    CACHEFILE cf, 
    CACHEKEY key, 
    u_int32_t fullhash, 
    void**value, 
    long *sizep,
    CACHETABLE_FLUSH_CALLBACK flush_callback, 
    CACHETABLE_FETCH_CALLBACK fetch_callback, 
    CACHETABLE_PARTIAL_EVICTION_EST_CALLBACK pe_est_callback,
    CACHETABLE_PARTIAL_EVICTION_CALLBACK pe_callback, 
    CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK pf_req_callback  __attribute__((unused)),
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback  __attribute__((unused)),
    CACHETABLE_CLEANER_CALLBACK cleaner_callback,
    void *read_extraargs, // parameter for fetch_callback, pf_req_callback, and pf_callback
    void* write_extraargs, // parameter for flush_callback, pe_est_callback, pe_callback, and cleaner_callback
    UNLOCKERS unlockers
    );

#define CAN_RELEASE_LOCK_DURING_IO

int toku_cachetable_maybe_get_and_pin (CACHEFILE, CACHEKEY, u_int32_t /*fullhash*/, void**);
// Effect: Maybe get and pin a memory object.
//  This function is similar to the get_and_pin function except that it
//  will not attempt to fetch a memory object that is not in the cachetable or requires any kind of blocking to get it.  
// Returns: If the the item is already in memory, then return 0 and store it in the
// void**.  If the item is not in memory, then return a nonzero error number.

int toku_cachetable_maybe_get_and_pin_clean (CACHEFILE, CACHEKEY, u_int32_t /*fullhash*/, void**);
// Effect: Like maybe get and pin, but may pin a clean pair.

int toku_cachetable_unpin(CACHEFILE, CACHEKEY, u_int32_t fullhash, enum cachetable_dirty dirty, PAIR_ATTR size);
// Effect: Unpin a memory object
// Modifies: If the memory object is in the cachetable, then OR the dirty flag,
// update the size, and release the read lock on the memory object.
// Returns: 0 if success, otherwise returns an error number.
// Requires: The ct is locked.

int toku_cachetable_unpin_ct_prelocked_no_flush(CACHEFILE, CACHEKEY, u_int32_t fullhash, enum cachetable_dirty dirty, PAIR_ATTR size);
// Effect: The same as tokud_cachetable_unpin, except that the ct must not be locked.
// Requires: The ct is NOT locked.

void toku_cachetable_prelock(CACHEFILE cf);
// Effect: locks cachetable

void toku_cachetable_unlock(CACHEFILE cf);
// Effect: unlocks cachetable


int toku_cachetable_unpin_and_remove (CACHEFILE, CACHEKEY, BOOL); /* Removing something already present is OK. */
// Effect: Remove an object from the cachetable.  Don't write it back.
// Requires: The object must be pinned exactly once.

int toku_cachefile_prefetch(CACHEFILE cf, CACHEKEY key, u_int32_t fullhash,
                            CACHETABLE_FLUSH_CALLBACK flush_callback, 
                            CACHETABLE_FETCH_CALLBACK fetch_callback,
                            CACHETABLE_PARTIAL_EVICTION_EST_CALLBACK pe_est_callback,
                            CACHETABLE_PARTIAL_EVICTION_CALLBACK pe_callback,
                            CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK pf_req_callback,
                            CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback,
                            CACHETABLE_CLEANER_CALLBACK cleaner_callback,
                            void *read_extraargs, // parameter for fetch_callback, pf_req_callback, and pf_callback 
                            void *write_extraargs, // parameter for flush_callback, pe_est_callback, pe_callback, and cleaner_callback
                            BOOL *doing_prefetch);
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

// Rename whatever is at oldkey to be newkey.  Requires that the object be pinned.
int toku_cachetable_rename (CACHEFILE cachefile, CACHEKEY oldkey, CACHEKEY newkey);

//int cachetable_fsync_all (CACHETABLE); /* Flush everything to disk, but keep it in cache. */

// Close the cachefile.
// Effects: All of the cached object associated with the cachefile are evicted from
// the cachetable.  The flush callback is called for each of these objects.  The
// close function does not return until all of the objects are evicted.  The cachefile
// object is freed.
// If oplsn_valid is TRUE then use oplsn as the LSN of the close instead of asking the logger.  oplsn_valid being TRUE is only allowed during recovery, and requires that you are removing the last reference (otherwise the lsn wouldn't make it in.)
// Returns: 0 if success, otherwise returns an error number.
int toku_cachefile_close (CACHEFILE*, char **error_string, BOOL oplsn_valid, LSN oplsn);

// Flush the cachefile.
// Effect: Flush everything owned by the cachefile from the cachetable. All dirty
// blocks are written.  All unpinned blocks are evicted from the cachetable.
// Returns: 0 if success, otherwise returns an error number.
int toku_cachefile_flush (CACHEFILE);

// Return on success (different from pread and pwrite)
//int cachefile_pwrite (CACHEFILE, const void *buf, size_t count, toku_off_t offset);
//int cachefile_pread  (CACHEFILE, void *buf, size_t count, toku_off_t offset);

// Get the file descriptor associated with the cachefile
// Return the file descriptor
// Grabs a read lock protecting the fd
int toku_cachefile_get_and_pin_fd (CACHEFILE);

// Get the iname (within the environment) associated with the cachefile
// Return the filename
char * toku_cachefile_fname_in_env (CACHEFILE cf);

// Releases the read lock (taken by toku_cachefile_get_and_pin_fd) protecting the fd
void toku_cachefile_unpin_fd (CACHEFILE);

// For test programs only.
// Set the cachefile's fd and fname.
// Effect: Bind the cachefile to a new fd and fname. The old fd is closed.
// Returns: 0 if success, otherwise an error number
int toku_cachefile_set_fd (CACHEFILE cf, int fd, const char *fname_relative_to_env);

// Equivalent to toku_cachefile_set_fd to /dev/null but without
// closing the user data.
int toku_cachefile_redirect_nullfd (CACHEFILE cf);

// Truncate a cachefile
int toku_cachefile_truncate (CACHEFILE cf, toku_off_t new_size);

//has it been redirected to dev null?
//Must have called toku_cachefile_get_and_pin_fd to hold a lock around this function
BOOL toku_cachefile_is_dev_null_unlocked (CACHEFILE cf);

// Return the logger associated with the cachefile
TOKULOGGER toku_cachefile_logger (CACHEFILE);

// Return the filenum associated with the cachefile
FILENUM toku_cachefile_filenum (CACHEFILE);

// Effect: Return a 32-bit hash key.  The hash key shall be suitable for using with bitmasking for a table of size power-of-two.
u_int32_t toku_cachetable_hash (CACHEFILE cachefile, CACHEKEY key);

u_int32_t toku_cachefile_fullhash_of_header (CACHEFILE cachefile);

// debug functions

// Print the contents of the cachetable. This is mainly used from gdb
void toku_cachetable_print_state (CACHETABLE ct);

// Get the state of the cachetable. This is used to verify the cachetable
void toku_cachetable_get_state(CACHETABLE ct, int *num_entries_ptr, int *hash_size_ptr, long *size_current_ptr, long *size_limit_ptr, int64_t *size_max_ptr);

// Get the state of a cachetable entry by key. This is used to verify the cachetable
int toku_cachetable_get_key_state(CACHETABLE ct, CACHEKEY key, CACHEFILE cf,
                                  void **value_ptr,
				  int *dirty_ptr,
                                  long long *pin_ptr,
                                  long *size_ptr);

// Verify the whole cachetable that the cachefile is in.  Slow.
void toku_cachefile_verify (CACHEFILE cf);

int64_t toku_cachetable_size_slowslow (CACHETABLE t);
int64_t toku_cachetable_size_discrepancy (CACHETABLE t);
int64_t toku_cachetable_size_discrepancy_pinned (CACHETABLE t);
int64_t toku_cachetable_size_slow (CACHETABLE t);

// Verify the cachetable. Slow.
void toku_cachetable_verify (CACHETABLE t);

// Not for use in production, but useful for testing.
void toku_cachetable_print_hash_histogram (void) __attribute__((__visibility__("default")));

#define TOKU_CACHETABLE_DO_EVICT_FROM_WRITER 0

void toku_cachetable_maybe_flush_some(CACHETABLE ct);

u_int64_t toku_cachefile_size_in_memory(CACHEFILE cf);


typedef struct cachetable_status {
    u_int64_t lock_taken;
    u_int64_t lock_released;
    u_int64_t hit;
    u_int64_t miss;
    u_int64_t misstime;     /* how many usec spent waiting for disk read because of cache miss */ 
    u_int64_t waittime;     /* how many usec spent waiting for another thread to release cache line */ 
    u_int64_t wait_reading;
    u_int64_t wait_writing;
    u_int64_t wait_checkpoint; // number of times get_and_pin waits for a node to be written for a checkpoint
    u_int64_t puts;          // how many times has a newly created node been put into the cachetable?
    u_int64_t prefetches;    // how many times has a block been prefetched into the cachetable?
    u_int64_t maybe_get_and_pins;      // how many times has maybe_get_and_pin(_clean) been called?
    u_int64_t maybe_get_and_pin_hits;  // how many times has maybe_get_and_pin(_clean) returned with a node?
    int64_t   size_current;            // the sum of the sizes of the nodes represented in the cachetable
    int64_t   size_limit;              // the limit to the sum of the node sizes
    int64_t   size_max;                // high water mark of size_current (max value size_current ever had)
    int64_t   size_writing;            // the sum of the sizes of the nodes being written
    u_int64_t get_and_pin_footprint;   
    uint64_t  local_checkpoint;        // number of times a local checkpoint was taken for a commit (2440)
    uint64_t  local_checkpoint_files;  // number of files subject to local checkpoint taken for a commit (2440)
    uint64_t  local_checkpoint_during_checkpoint;  // number of times a local checkpoint happened during normal checkpoint (2440)
    u_int64_t evictions;
    u_int64_t cleaner_executions; // number of times the cleaner thread's loop has executed
    int64_t size_nonleaf; // number of bytes in cachetable belonging to nonleaf nodes
    int64_t size_leaf; // number of bytes in cachetable belonging to leaf nodes
    int64_t size_rollback; // number of bytes in cachetable belonging to rollback nodes
    int64_t size_cachepressure; // number of bytes that cachetable thinks is causing cache pressure (sum of buffers, basically) 
} CACHETABLE_STATUS_S, *CACHETABLE_STATUS;

void toku_cachetable_get_status(CACHETABLE ct, CACHETABLE_STATUS s);

void toku_cachetable_set_env_dir(CACHETABLE ct, const char *env_dir);
char * toku_construct_full_name(int count, ...);
char * toku_cachetable_get_fname_in_cwd(CACHETABLE ct, const char * fname_in_env);

void toku_cachetable_set_lock_unlock_for_io (CACHETABLE ct, void (*ydb_lock_callback)(void), void (*ydb_unlock_callback)(void));
// Effect: When we do I/O we may need to release locks (e.g., the ydb lock).  These functions release the lock acquire the lock.

    
#if 0
int toku_cachetable_local_checkpoint_for_commit(CACHETABLE ct, TOKUTXN txn, uint32_t n, CACHEFILE cachefiles[]);
#endif

void cachefile_kibbutz_enq (CACHEFILE cf, void (*f)(void*), void *extra);
// Effect: Add a job to the cachetable's collection of work to do.  Note that function f must call remove_background_job()

void add_background_job (CACHEFILE cf, bool already_locked);
// Effect: When a kibbutz job or cleaner thread starts working, the
// cachefile must be notified (so during a close it can wait);
void remove_background_job (CACHEFILE cf, bool already_locked);
// Effect: When a kibbutz job or cleaner thread finishes in a cachefile,
// the cachetable must be notified.

// test-only function
extern int toku_cachetable_get_checkpointing_user_data_status(void);

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif
