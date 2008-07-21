#ifndef CACHETABLE_H
#define CACHETABLE_H

#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include <fcntl.h>
#include "brttypes.h"

/* Implement the cache table. */

typedef long long CACHEKEY;

/* Maintain a cache mapping from cachekeys to values (void*)
 * Some of the keys can be pinned.  Don't pin too many or for too long.
 * If the cachetable is too full, it will call the flush_callback() function with the key, the value, and the otherargs
   and then remove the key-value pair from the cache.
 * The callback won't be any of the currently pinned keys.
 * Also when flushing an object, the cachetable drops all references to it,
 * so you may need to free() it.
 * Note: The cachetable should use a common pool of memory, flushing things across cachetables.
 *  (The first implementation doesn't)
 * If you pin something twice, you must unpin it twice.
 * table_size is the initial size of the cache table hash table (in number of entries)
 * size limit is the upper bound of the sum of size of the entries in the cache table (total number of bytes)
 */
int toku_create_cachetable(CACHETABLE */*result*/, long size_limit, LSN initial_lsn, TOKULOGGER);

int toku_cachetable_openf (CACHEFILE *,CACHETABLE, const char */*fname*/, int flags, mode_t mode);
int toku_cachetable_openfd (CACHEFILE *,CACHETABLE, int /*fd*/, const char */*fname (used for logging)*/);

typedef void (cachetable_flush_func_t)(CACHEFILE, CACHEKEY key, void*value, long size, BOOL write_me, BOOL keep_me, LSN modified_lsn, BOOL rename_p);
typedef cachetable_flush_func_t *CACHETABLE_FLUSH_FUNC_T;

/* If we are asked to fetch something, get it by calling this back. */
typedef int (cachetable_fetch_func_t)(CACHEFILE, CACHEKEY key, u_int32_t fullhash, void **value, long *sizep, void *extraargs, LSN *written_lsn);
typedef cachetable_fetch_func_t *CACHETABLE_FETCH_FUNC_T;

/* Error if already present.  On success, pin the value. */
int toku_cachetable_put(CACHEFILE cf, CACHEKEY key, u_int32_t fullhash,
			void* value, long size,
			cachetable_flush_func_t flush_callback, cachetable_fetch_func_t fetch_callback, void *extraargs);

int toku_cachetable_get_and_pin(CACHEFILE, CACHEKEY, u_int32_t /*fullhash*/,
				void**/*value*/, long *sizep,
				cachetable_flush_func_t flush_callback, cachetable_fetch_func_t fetch_callback, void *extraargs);

/* If the the item is already in memory, then return 0 and store it in the void**.
 * If the item is not in memory, then return nonzero. */
int toku_cachetable_maybe_get_and_pin (CACHEFILE, CACHEKEY, u_int32_t /*fullhash*/, void**);

/* cachetable object state wrt external memory */
#define CACHETABLE_CLEAN 0
#define CACHETABLE_DIRTY 1

int toku_cachetable_unpin(CACHEFILE, CACHEKEY, u_int32_t fullhash, int dirty, long size); /* Note whether it is dirty when we unpin it. */
int toku_cachetable_remove (CACHEFILE, CACHEKEY, int /*write_me*/); /* Removing something already present is OK. */
int toku_cachetable_assert_all_unpinned (CACHETABLE);
int toku_cachefile_count_pinned (CACHEFILE, int /*printthem*/ );

/* Rename whatever is at oldkey to be newkey.  Requires that the object be pinned. */
int toku_cachetable_rename (CACHEFILE cachefile, CACHEKEY oldkey, CACHEKEY newkey);

//int cachetable_fsync_all (CACHETABLE); /* Flush everything to disk, but keep it in cache. */
int toku_cachetable_close (CACHETABLE*); /* Flushes everything to disk, and destroys the cachetable. */

int toku_cachefile_close (CACHEFILE*, TOKULOGGER);

int toku_cachefile_flush (CACHEFILE); 
// effect: flush everything owned by the cachefile.
// returns: 0 if success

void toku_cachefile_refup (CACHEFILE cfp); 
// Increment the reference count.  Use close to decrement it.

// Return on success (different from pread and pwrite)
//int cachefile_pwrite (CACHEFILE, const void *buf, size_t count, off_t offset);
//int cachefile_pread  (CACHEFILE, void *buf, size_t count, off_t offset);

int toku_cachefile_fd (CACHEFILE);
// get the file descriptor bound to this cachefile
// returns: the file descriptor

int toku_cachefile_set_fd (CACHEFILE cf, int fd, const char *fname);
// set the cachefile's fd and fname. 
// effect: bind the cachefile to a new fd and fname. the old fd is closed.
// returns: 0 if success

// Useful for debugging
void toku_cachetable_print_state (CACHETABLE ct);
void toku_cachetable_get_state(CACHETABLE ct, int *num_entries_ptr, int *hash_size_ptr, long *size_current_ptr, long *size_limit_ptr);
int toku_cachetable_get_key_state(CACHETABLE ct, CACHEKEY key, CACHEFILE cf, void **value_ptr,
				  int *dirty_ptr, long long *pin_ptr, long *size_ptr);

void toku_cachefile_verify (CACHEFILE cf);  // Verify the whole cachetable that the CF is in.  Slow.
void toku_cachetable_verify (CACHETABLE t); // Slow...

TOKULOGGER toku_cachefile_logger (CACHEFILE);
FILENUM toku_cachefile_filenum (CACHEFILE);

// What is the cachefile that goes with a particular filenum?
// During a transaction, we cannot reuse a filenum.
int toku_cachefile_of_filenum (CACHETABLE t, FILENUM filenum, CACHEFILE *cf);

int toku_cachetable_checkpoint (CACHETABLE ct);

u_int32_t toku_cachetable_hash (CACHEFILE cachefile, CACHEKEY key);
// Effect: Return a 32-bit hash key.  The hash key shall be suitable for using with bitmasking for a table of size power-of-two.

u_int32_t toku_cachefile_fullhash_of_header (CACHEFILE cachefile);

#endif
