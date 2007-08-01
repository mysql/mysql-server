#ifndef CACHETABLE_H
#define CACHETABLE_H
#include <fcntl.h>

/* Implement the cache table. */

typedef long long CACHEKEY;
typedef struct cachetable *CACHETABLE;
typedef struct cachefile *CACHEFILE;

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
 */
int create_cachetable (CACHETABLE */*result*/, int /*n_entries*/);

int cachetable_openf (CACHEFILE *,CACHETABLE, const char */*fname*/, int flags, mode_t mode);

/* Error if already present.  On success, pin the value. */
int cachetable_put (CACHEFILE, CACHEKEY, void*/*value*/,
		    void(*flush_callback)(CACHEFILE, CACHEKEY key, void*value, int write_me, int keep_me),
		    int(*fetch_callback)(CACHEFILE, CACHEKEY key, void**value,void*extraargs), /* If we are asked to fetch something, get it by calling this back. */
		    void*extraargs
		    );

int cachetable_get_and_pin (CACHEFILE, CACHEKEY, void**/*value*/,
			    void(*flush_callback)(CACHEFILE,CACHEKEY,void*,int write_me, int keep_me),
			    int(*fetch_callback)(CACHEFILE, CACHEKEY key, void**value,void*extraargs), /* If we are asked to fetch something, get it by calling this back. */
			    void*extraargs
			    );

/* If the the item is already in memory, then return 0 and store it in the void**.
 * If the item is not in memory, then return nonzero. */
int cachetable_maybe_get_and_pin (CACHEFILE, CACHEKEY, void**);
int cachetable_unpin (CACHEFILE, CACHEKEY, int dirty); /* Note whether it is dirty when we unpin it. */
int cachetable_remove (CACHEFILE, CACHEKEY, int /*write_me*/); /* Removing something already present is OK. */
int cachetable_assert_all_unpinned (CACHETABLE);
int cachefile_count_pinned (CACHEFILE, int /*printthem*/ );

//int cachetable_fsync_all (CACHETABLE); /* Flush everything to disk, but keep it in cache. */
int cachetable_close (CACHETABLE*); /* Flushes everything to disk, and destroys the cachetable. */

int cachefile_close (CACHEFILE*);
//int cachefile_flush (CACHEFILE); /* Flush everything related to the VOID* to disk and free all memory.  Don't destroy the cachetable. */

// Return on success (different from pread and pwrite)
//int cachefile_pwrite (CACHEFILE, const void *buf, size_t count, off_t offset);
//int cachefile_pread  (CACHEFILE, void *buf, size_t count, off_t offset);

int cachefile_fd (CACHEFILE);

#endif
