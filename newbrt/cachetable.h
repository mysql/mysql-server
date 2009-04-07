#ifndef CACHETABLE_H
#define CACHETABLE_H

#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include <fcntl.h>
#include "brttypes.h"
#include "workqueue.h"

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

// TODO: #1510  Add comments on how these behave
int toku_cachetable_begin_checkpoint (CACHETABLE ct, TOKULOGGER);
int toku_cachetable_end_checkpoint(CACHETABLE ct, TOKULOGGER logger, char **error_string);

// Does an fsync of a cachefile.
// Handles the case where cf points to /dev/null
int toku_cachefile_fsync(CACHEFILE cf);

// Close the cachetable.
// Effects: All of the memory objects are flushed to disk, and the cachetable is
// destroyed.
int toku_cachetable_close (CACHETABLE*); /* Flushes everything to disk, and destroys the cachetable. */

// Open a file and bind the file to a new cachefile object.
int toku_cachetable_openf (CACHEFILE *,CACHETABLE, const char */*fname*/, int flags, mode_t mode);

// Bind a file to a new cachefile object.
int toku_cachetable_openfd (CACHEFILE *,CACHETABLE, int /*fd*/, const char */*fname (used for logging)*/);

// Get access to the asynchronous work queue
// Returns: a pointer to the work queue
WORKQUEUE toku_cachetable_get_workqueue (CACHETABLE);

void toku_cachefile_get_workqueue_load (CACHEFILE, int *n_in_queue, int *n_threads);

// The flush callback is called when a key value pair is being written to storage and possibly removed from the cachetable.
// When write_me is true, the value should be written to storage.
// When keep_me is false, the value should be freed.
// When for_checkpoint is true, this was a 'pending' write
// Returns: 0 if success, otherwise an error number.
typedef void (*CACHETABLE_FLUSH_CALLBACK)(CACHEFILE, CACHEKEY key, void *value, void *extraargs, long size, BOOL write_me, BOOL keep_me, BOOL for_checkpoint);

// The fetch callback is called when a thread is attempting to get and pin a memory
// object and it is not in the cachetable.
// Returns: 0 if success, otherwise an error number.  The address and size of the object
// associated with the key are returned.
typedef int (*CACHETABLE_FETCH_CALLBACK)(CACHEFILE, CACHEKEY key, u_int32_t fullhash, void **value, long *sizep, void *extraargs, LSN *written_lsn);

void toku_cachefile_set_userdata(CACHEFILE cf, void *userdata, int (*close_userdata)(CACHEFILE, void*, char **/*error_string*/), int (*checkpoint_userdata)(CACHEFILE, void*), int (*begin_checkpoint_userdata)(CACHEFILE, LSN, void*), int (*end_checkpoint_userdata)(CACHEFILE, void*));
// Effect: Store some cachefile-specific user data.  When the last reference to a cachefile is closed, we call close_userdata().
// Before starting a checkpoint, we call checkpoint_prepare_userdata().
// When the cachefile needs to be checkpointed, we call checkpoint_userdata().
// If userdata is already non-NULL, then we simply overwrite it.

void *toku_cachefile_get_userdata(CACHEFILE);
// Effect: Get the user data.

// Put a memory object into the cachetable.
// Effects: Lookup the key in the cachetable. If the key is not in the cachetable,
// then insert the pair and pin it. Otherwise return an error.  Some of the key
// value pairs may be evicted from the cachetable when the cachetable gets too big.
// Returns: 0 if the memory object is placed into the cachetable, otherwise an
// error number.
int toku_cachetable_put(CACHEFILE cf, CACHEKEY key, u_int32_t fullhash,
			void *value, long size,
			CACHETABLE_FLUSH_CALLBACK flush_callback,
                        CACHETABLE_FETCH_CALLBACK fetch_callback, void *extraargs);

// Get and pin a memory object.
// Effects: If the memory object is in the cachetable, acquire a read lock on it.
// Otherwise, fetch it from storage by calling the fetch callback.  If the fetch
// succeeded, add the memory object to the cachetable with a read lock on it.
// Returns: 0 if the memory object is in memory, otherwise an error number.
int toku_cachetable_get_and_pin(CACHEFILE, CACHEKEY, u_int32_t /*fullhash*/,
				void **/*value*/, long *sizep,
				CACHETABLE_FLUSH_CALLBACK flush_callback,
                                CACHETABLE_FETCH_CALLBACK fetch_callback, void *extraargs);

// Maybe get and pin a memory object.
// Effects:  This function is identical to the get_and_pin function except that it
// will not attempt to fetch a memory object that is not in the cachetable.
// Returns: If the the item is already in memory, then return 0 and store it in the
// void**.  If the item is not in memory, then return a nonzero error number.
int toku_cachetable_maybe_get_and_pin (CACHEFILE, CACHEKEY, u_int32_t /*fullhash*/, void**);

// cachetable pair clean or dirty WRT external memory
enum cachetable_dirty {
    CACHETABLE_CLEAN=0, // the cached object is clean WRT the cachefile
    CACHETABLE_DIRTY=1, // the cached object is dirty WRT the cachefile
};

// Unpin a memory object
// Effects: If the memory object is in the cachetable, then OR the dirty flag,
// update the size, and release the read lock on the memory object.
// Returns: 0 if success, otherwise returns an error number.
int toku_cachetable_unpin(CACHEFILE, CACHEKEY, u_int32_t fullhash, enum cachetable_dirty dirty, long size);

int toku_cachetable_unpin_and_remove (CACHEFILE, CACHEKEY); /* Removing something already present is OK. */
// Effect: Remove an object from the cachetable.  Don't write it back.
// Requires: The object must be pinned exactly once.

// Prefetch a memory object for a given key into the cachetable
// Returns: 0 if success
int toku_cachefile_prefetch(CACHEFILE cf, CACHEKEY key, u_int32_t fullhash,
                            CACHETABLE_FLUSH_CALLBACK flush_callback, 
                            CACHETABLE_FETCH_CALLBACK fetch_callback, 
                            void *extraargs);

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
// Returns: 0 if success, otherwise returns an error number.
int toku_cachefile_close (CACHEFILE*, TOKULOGGER, char **error_string);

// Flush the cachefile.
// Effect: Flush everything owned by the cachefile from the cachetable. All dirty
// blocks are written.  All unpinned blocks are evicted from the cachetable.
// Returns: 0 if success, otherwise returns an error number.
int toku_cachefile_flush (CACHEFILE);

// Increment the reference count.  Use close to decrement it.
void toku_cachefile_refup (CACHEFILE cfp);

// Return on success (different from pread and pwrite)
//int cachefile_pwrite (CACHEFILE, const void *buf, size_t count, toku_off_t offset);
//int cachefile_pread  (CACHEFILE, void *buf, size_t count, toku_off_t offset);

// Get the file descriptor associated with the cachefile
// Return the file descriptor
int toku_cachefile_fd (CACHEFILE);

// Set the cachefile's fd and fname.
// Effect: Bind the cachefile to a new fd and fname. The old fd is closed.
// Returns: 0 if success, otherwise an error number
int toku_cachefile_set_fd (CACHEFILE cf, int fd, const char *fname);

// Equivalent to toku_cachefile_set_fd to /dev/null but without
// closing the user data.
int toku_cachefile_redirect_nullfd (CACHEFILE cf);

// Truncate a cachefile
int toku_cachefile_truncate (CACHEFILE cf, toku_off_t new_size);

//has it been redirected to dev null?
BOOL toku_cachefile_is_dev_null (CACHEFILE cf);

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

#define TOKU_CACHETABLE_DO_EVICT_FROM_WRITER 0

void toku_cachetable_maybe_flush_some(CACHETABLE ct);

u_int64_t toku_cachefile_size_in_memory(CACHEFILE cf);

#endif
