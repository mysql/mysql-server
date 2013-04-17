/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."


#include <toku_portability.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#include "memory.h"
#include "workqueue.h"
#include "threadpool.h"
#include "cachetable.h"
#include "rwlock.h"
#include "nonblocking_mutex.h"
#include "log_header.h"
#include "checkpoint.h"
#include "minicron.h"
#include "log-internal.h"
#include "kibbutz.h"

#include "brt-internal.h"


static void cachetable_writer(WORKITEM);
static void cachetable_reader(WORKITEM);
static void cachetable_partial_reader(WORKITEM);

#define TRACE_CACHETABLE 0
#if TRACE_CACHETABLE
#define WHEN_TRACE_CT(x) x
#else
#define WHEN_TRACE_CT(x) ((void)0)
#endif

// these should be in the cachetable object, but we make them file-wide so that gdb can get them easily
static u_int64_t cachetable_hit;
static u_int64_t cachetable_miss;
static u_int64_t cachetable_wait_reading;  // how many times does get_and_pin() wait for a node to be read?
static u_int64_t cachetable_wait_writing;  // how many times does get_and_pin() wait for a node to be written?
static u_int64_t cachetable_puts;          // how many times has a newly created node been put into the cachetable?
static u_int64_t cachetable_prefetches;    // how many times has a block been prefetched into the cachetable?
static u_int64_t cachetable_maybe_get_and_pins;      // how many times has maybe_get_and_pin(_clean) been called?
static u_int64_t cachetable_maybe_get_and_pin_hits;  // how many times has get_and_pin(_clean) returned with a node?
static u_int64_t cachetable_wait_checkpoint;         // number of times get_and_pin waits for a node to be written for a checkpoint
static u_int64_t cachetable_misstime;     // time spent waiting for disk read
static u_int64_t cachetable_waittime;     // time spent waiting for another thread to release lock (e.g. prefetch, writing)
static u_int64_t cachetable_lock_taken = 0;
static u_int64_t cachetable_lock_released = 0;
static u_int64_t local_checkpoint;        // number of times a local checkpoint was taken for a commit (2440)
static u_int64_t local_checkpoint_files;  // number of files subject to local checkpoint taken for a commit (2440)
static u_int64_t local_checkpoint_during_checkpoint;  // number of times a local checkpoint happened during normal checkpoint (2440)
static u_int64_t cachetable_evictions;
static u_int64_t cleaner_executions; // number of times the cleaner thread's loop has executed


enum ctpair_state {
    CTPAIR_IDLE = 1,    // in memory
    CTPAIR_READING = 2, // being read into memory
    CTPAIR_WRITING = 3, // being written from memory
};


/* The workqueue pointer cq is set in:
 *   cachetable_complete_write_pair()      cq is cleared, called from many paths, cachetable lock is held during this function
 *   cachetable_flush_cachefile()          called during close and truncate, cachetable lock is held during this function
 *   toku_cachetable_unpin_and_remove()    called during node merge, cachetable lock is held during this function
 *
 */
typedef struct ctpair *PAIR;
struct ctpair {
    CACHEFILE cachefile;
    CACHEKEY key;
    void    *value;
    PAIR_ATTR attr;

    //
    // This variable used to be used by various functions
    // to select a course of action. This is NO LONGER the case.
    // From now, this variable is for informational purposes
    // only, and should not be relied upon for any action
    // It is a helpful variable to peek at in the debugger.
    //
    enum ctpair_state state;
    enum cachetable_dirty dirty;

    char     verify_flag;        // Used in verify_cachetable()
    BOOL     remove_me;          // write_pair

    u_int32_t fullhash;

    CACHETABLE_FLUSH_CALLBACK flush_callback;
    CACHETABLE_PARTIAL_EVICTION_EST_CALLBACK pe_est_callback;
    CACHETABLE_PARTIAL_EVICTION_CALLBACK pe_callback;
    CACHETABLE_CLEANER_CALLBACK cleaner_callback;
    long size_evicting_estimate;
    void    *write_extraargs;

    PAIR     clock_next,clock_prev; // In clock.
    PAIR     hash_chain;
    u_int32_t count;        // clock count


    BOOL     checkpoint_pending; // If this is on, then we have got to write the pair out to disk before modifying it.
    PAIR     pending_next;
    PAIR     pending_prev;

    struct nb_mutex nb_mutex;    // single writer
    struct workqueue *cq;        // writers sometimes return ctpair's using this queue
    struct workitem asyncwork;   // work item for the worker threads
    u_int32_t refs;              // References that prevent destruction
    int already_removed;         // If a pair is removed from the cachetable, but cannot be freed because refs>0, this is set.
    struct toku_list next_for_cachefile; // link in the cachefile list
};

static void * const zero_value = 0;
static PAIR_ATTR const zero_attr = {
    .size = 0, 
    .nonleaf_size = 0, 
    .leaf_size = 0, 
    .rollback_size = 0, 
    .cache_pressure_size = 0
};

static int maybe_flush_some (CACHETABLE ct, long size);

static inline void
ctpair_add_ref(PAIR p) {
    assert(!p->already_removed);
    p->refs++;
}

static inline void ctpair_destroy(PAIR p) {
    assert(p->refs>0);
    p->refs--;
    if (p->refs==0) {
        nb_mutex_destroy(&p->nb_mutex);
        toku_free(p);
    }
}

// The cachetable is as close to an ENV as we get.
// There are 3 locks, must be taken in this order
//      openfd_mutex
//      cachetable_mutex
//      cachefiles_mutex
struct cachetable {
    u_int32_t n_in_table;         // number of pairs in the hash table
    u_int32_t table_size;         // number of buckets in the hash table
    PAIR *table;                  // hash table
    PAIR  clock_head;              // of clock . head is the next thing to be up for decrement. 
    PAIR  cleaner_head;              // for cleaner thread. head is the next thing to look at for possible cleaning. 
    CACHEFILE cachefiles;         // list of cachefiles that use this cachetable
    CACHEFILE cachefiles_in_checkpoint; //list of cachefiles included in checkpoint in progress
    int64_t size_reserved;           // How much memory is reserved (e.g., by the loader)
    int64_t size_current;            // the sum of the sizes of the pairs in the cachetable
    int64_t size_limit;              // the limit to the sum of the pair sizes
    int64_t size_evicting;            // the sum of the sizes of the pairs being written
    int64_t size_max;                // high water mark of size_current (max value size_current ever had)
    TOKULOGGER logger;
    toku_pthread_mutex_t *mutex;  // coarse lock that protects the cachetable, the cachefiles, and the pairs
    toku_pthread_mutex_t cachefiles_mutex;  // lock that protects the cachefiles list
    struct workqueue wq;          // async work queue 
    THREADPOOL threadpool;        // pool of worker threads

    KIBBUTZ kibbutz;              // another pool of worker threads and jobs to do asynchronously.  

    LSN lsn_of_checkpoint_in_progress;
    u_int32_t checkpoint_num_files;  // how many cachefiles are in the checkpoint
    u_int32_t checkpoint_num_txns;   // how many transactions are in the checkpoint
    PAIR pending_head;           // list of pairs marked with checkpoint_pending
    struct rwlock pending_lock;  // multiple writer threads, single checkpoint thread
    struct minicron checkpointer; // the periodic checkpointing thread
    struct minicron cleaner; // the periodic cleaner thread
    u_int32_t cleaner_iterations; // how many times to run the cleaner per
                                  // cleaner period (minicron has a
                                  // minimum period of 1s so if you want
                                  // more frequent cleaner runs you must
                                  // use this)
    toku_pthread_mutex_t openfd_mutex;  // make toku_cachetable_openfd() single-threaded
    OMT reserved_filenums;
    char *env_dir;
    BOOL set_env_dir; //Can only set env_dir once

    // For releasing locks during I/O.  These are named "ydb_lock_callback" but it could be viewed more generally as being used to release and reacquire locks while I/O is takign place.
    void (*ydb_lock_callback)(void);
    void (*ydb_unlock_callback)(void);

    // variables for engine status
    int64_t size_nonleaf;
    int64_t size_leaf;
    int64_t size_rollback;
    int64_t size_cachepressure;
};

// Lock the cachetable
static inline void cachefiles_lock(CACHETABLE ct) {
    int r = toku_pthread_mutex_lock(&ct->cachefiles_mutex); resource_assert_zero(r);
}

// Unlock the cachetable
static inline void cachefiles_unlock(CACHETABLE ct) {
    int r = toku_pthread_mutex_unlock(&ct->cachefiles_mutex); resource_assert_zero(r);
}

// Lock the cachetable
static inline void cachetable_lock(CACHETABLE ct __attribute__((unused))) {
    int r = toku_pthread_mutex_lock(ct->mutex); resource_assert_zero(r);;
    cachetable_lock_taken++;
}

// Unlock the cachetable
static inline void cachetable_unlock(CACHETABLE ct __attribute__((unused))) {
    cachetable_lock_released++;
    int r = toku_pthread_mutex_unlock(ct->mutex); resource_assert_zero(r);
}

// Wait for cache table space to become available 
// size_current is number of bytes currently occupied by data (referred to by pairs)
// size_evicting is number of bytes queued up to be written out (sum of sizes of pairs in CTPAIR_WRITING state)
static inline void cachetable_wait_write(CACHETABLE ct) {
    // if we're writing more than half the data in the cachetable
    while (2*ct->size_evicting > ct->size_current) {
        workqueue_wait_write(&ct->wq, 0);
    }
}

enum cachefile_checkpoint_state {
    CS_INVALID = 0,
    CS_NOT_IN_PROGRESS,
    CS_CALLED_BEGIN_CHECKPOINT,
    CS_CALLED_CHECKPOINT
};

struct cachefile {
    CACHEFILE next;
    CACHEFILE next_in_checkpoint;
    struct toku_list pairs_for_cachefile; // list of pairs for this cachefile
    BOOL for_checkpoint; //True if part of the in-progress checkpoint
    u_int64_t refcount; /* CACHEFILEs are shared. Use a refcount to decide when to really close it.
			 * The reference count is one for every open DB.
			 * Plus one for every commit/rollback record.  (It would be harder to keep a count for every open transaction,
			 * because then we'd have to figure out if the transaction was already counted.  If we simply use a count for
			 * every record in the transaction, we'll be ok.  Hence we use a 64-bit counter to make sure we don't run out.
			 */
    BOOL is_closing;    /* TRUE if a cachefile is being close/has been closed. */
    bool is_flushing;  // during cachetable_flush_cachefile, this must be
                       // true, to prevent the cleaner thread from messing
                       // with nodes in that cachefile
    struct rwlock fdlock; // Protect changing the fd and is_dev_null
                          // Only write-locked by toku_cachefile_redirect_nullfd()
    BOOL is_dev_null;    //True if was deleted and redirected to /dev/null (starts out FALSE, can be set to TRUE, can never be set back to FALSE)
    int fd;       /* Bug: If a file is opened read-only, then it is stuck in read-only.  If it is opened read-write, then subsequent writers can write to it too. */
    CACHETABLE cachetable;
    struct fileid fileid;
    FILENUM filenum;
    char *fname_in_env; /* Used for logging */

    void *userdata;
    int (*log_fassociate_during_checkpoint)(CACHEFILE cf, void *userdata); // When starting a checkpoint we must log all open files.
    int (*log_suppress_rollback_during_checkpoint)(CACHEFILE cf, void *userdata); // When starting a checkpoint we must log which files need rollbacks suppressed
    int (*close_userdata)(CACHEFILE cf, int fd, void *userdata, char **error_string, BOOL lsnvalid, LSN); // when closing the last reference to a cachefile, first call this function. 
    int (*begin_checkpoint_userdata)(CACHEFILE cf, int fd, LSN lsn_of_checkpoint, void *userdata); // before checkpointing cachefiles call this function.
    int (*checkpoint_userdata)(CACHEFILE cf, int fd, void *userdata); // when checkpointing a cachefile, call this function.
    int (*end_checkpoint_userdata)(CACHEFILE cf, int fd, void *userdata); // after checkpointing cachefiles call this function.
    int (*note_pin_by_checkpoint)(CACHEFILE cf, void *userdata); // add a reference to the userdata to prevent it from being removed from memory
    int (*note_unpin_by_checkpoint)(CACHEFILE cf, void *userdata); // add a reference to the userdata to prevent it from being removed from memory
    toku_pthread_cond_t openfd_wait;    // openfd must wait until file is fully closed (purged from cachetable) if file is opened and closed simultaneously
    toku_pthread_cond_t closefd_wait;   // toku_cachefile_of_iname_and_add_reference() must wait until file is fully closed (purged from cachetable) if run while file is being closed.
    u_int32_t closefd_waiting;          // Number of threads waiting on closefd_wait (0 or 1, error otherwise).
    struct rwlock checkpoint_lock; //protects checkpoint callback functions
                                   //acts as fast mutex by only using 'write-lock'
    LSN most_recent_global_checkpoint_that_finished_early;
    LSN for_local_checkpoint;
    enum cachefile_checkpoint_state checkpoint_state; //Protected by checkpoint_lock

    int n_background_jobs; // how many jobs in the cachetable's kibbutz or
                           // on the cleaner thread (anything
                           // cachetable_flush_cachefile should wait on)
                           // are working on this cachefile. Each job should, at the
                           // end, obtain the cachetable mutex, decrement
                           // this variable, and broadcast the
                           // kibbutz_wait condition variable to let
                           // anyone else know it's happened.
    toku_pthread_cond_t background_wait;  // Any job that finishes a
                                          // background job should
                                          // broadcast on this cond
                                          // variable (holding the
                                          // cachetable mutex).  That way
                                          // when closing the cachefile,
                                          // we can get a notification
                                          // when things finish.
};

void add_background_job(CACHEFILE cf, bool already_locked)
{
    if (!already_locked) {
        cachetable_lock(cf->cachetable);
    }
    cf->n_background_jobs++;
    if (!already_locked) {
        cachetable_unlock(cf->cachetable);
    }
}

void remove_background_job(CACHEFILE cf, bool already_locked)
{
    if (!already_locked) {
        cachetable_lock(cf->cachetable);
    }
    assert(cf->n_background_jobs>0);
    cf->n_background_jobs--;
    { int r = toku_pthread_cond_broadcast(&cf->background_wait); assert(r==0); }
    if (!already_locked) {
        cachetable_unlock(cf->cachetable);
    }
}

void cachefile_kibbutz_enq (CACHEFILE cf, void (*f)(void*), void *extra)
// The function f must call remove_background_job when it completes
{
    add_background_job(cf, false);
    toku_kibbutz_enq(cf->cachetable->kibbutz, f, extra);
}
static void wait_on_background_jobs_to_finish (CACHEFILE cf) {
    cachetable_lock(cf->cachetable);
    while (cf->n_background_jobs>0) {
        int r = toku_pthread_cond_wait(&cf->background_wait, cf->cachetable->mutex);
        assert(r==0);
    }
    cachetable_unlock(cf->cachetable);
}

static int
checkpoint_thread (void *cachetable_v)
// Effect:  If checkpoint_period>0 thn periodically run a checkpoint.
//  If someone changes the checkpoint_period (calling toku_set_checkpoint_period), then the checkpoint will run sooner or later.
//  If someone sets the checkpoint_shutdown boolean , then this thread exits. 
// This thread notices those changes by waiting on a condition variable.
{
    CACHETABLE ct = cachetable_v;
    int r = toku_checkpoint(ct, ct->logger, NULL, NULL, NULL, NULL);
    if (r) {
        fprintf(stderr, "%s:%d Got error %d while doing checkpoint\n", __FILE__, __LINE__, r);
	abort(); // Don't quite know what to do with these errors.
    }
    return r;
}

int toku_set_checkpoint_period (CACHETABLE ct, u_int32_t new_period) {
    return toku_minicron_change_period(&ct->checkpointer, new_period);
}

u_int32_t toku_get_checkpoint_period (CACHETABLE ct) {
    return toku_minicron_get_period(&ct->checkpointer);
}

u_int32_t toku_get_checkpoint_period_unlocked (CACHETABLE ct) {
    return toku_minicron_get_period_unlocked(&ct->checkpointer);
}

int toku_set_cleaner_period (CACHETABLE ct, u_int32_t new_period) {
    return toku_minicron_change_period(&ct->cleaner, new_period);
}

u_int32_t toku_get_cleaner_period (CACHETABLE ct) {
    return toku_minicron_get_period(&ct->cleaner);
}

u_int32_t toku_get_cleaner_period_unlocked (CACHETABLE ct) {
    return toku_minicron_get_period_unlocked(&ct->cleaner);
}

int toku_set_cleaner_iterations (CACHETABLE ct, u_int32_t new_iterations) {
    cachetable_lock(ct);
    ct->cleaner_iterations = new_iterations;
    cachetable_unlock(ct);
    return 0;
}

u_int32_t toku_get_cleaner_iterations (CACHETABLE ct) {
    cachetable_lock(ct);
    u_int32_t retval =  toku_get_cleaner_iterations_unlocked(ct);
    cachetable_unlock(ct);
    return retval;
}

u_int32_t toku_get_cleaner_iterations_unlocked (CACHETABLE ct) {
    return ct->cleaner_iterations;
}

// reserve 25% as "unreservable".  The loader cannot have it.
#define unreservable_memory(size) ((size)/4)

static int cleaner_thread (void *cachetable_v);

int toku_create_cachetable(CACHETABLE *result, long size_limit, LSN UU(initial_lsn), TOKULOGGER logger) {
    CACHETABLE MALLOC(ct);
    if (ct == 0) return ENOMEM;
    memset(ct, 0, sizeof(*ct));
    ct->table_size = 4;
    rwlock_init(&ct->pending_lock);
    XCALLOC_N(ct->table_size, ct->table);
    ct->size_limit = size_limit;
    ct->size_reserved = unreservable_memory(size_limit);
    ct->logger = logger;
    toku_init_workers(&ct->wq, &ct->threadpool);
    ct->mutex = workqueue_lock_ref(&ct->wq);
    int r = toku_pthread_mutex_init(&ct->openfd_mutex, NULL); resource_assert_zero(r);
    r = toku_pthread_mutex_init(&ct->cachefiles_mutex, 0); resource_assert_zero(r);

    ct->kibbutz = toku_kibbutz_create(toku_os_get_number_active_processors());

    toku_minicron_setup(&ct->checkpointer, 0, checkpoint_thread, ct); // default is no checkpointing
    toku_minicron_setup(&ct->cleaner, 0, cleaner_thread, ct); // default is no cleaner, for now
    ct->cleaner_iterations = 1; // default is one iteration
    r = toku_omt_create(&ct->reserved_filenums);  assert(r==0);
    ct->env_dir = toku_xstrdup(".");
    *result = ct;
    return 0;
}

u_int64_t toku_cachetable_reserve_memory(CACHETABLE ct, double fraction) {
    cachetable_lock(ct);
    cachetable_wait_write(ct);
    uint64_t reserved_memory = fraction*(ct->size_limit-ct->size_reserved);
    ct->size_reserved += reserved_memory;
    {
	int r = maybe_flush_some(ct, reserved_memory);
	if (r) {
	    cachetable_unlock(ct);
	    return r;
	}
    }
    ct->size_current += reserved_memory;
    cachetable_unlock(ct);
    return reserved_memory;
}

void toku_cachetable_release_reserved_memory(CACHETABLE ct, uint64_t reserved_memory) {
    cachetable_lock(ct);
    ct->size_current -= reserved_memory;
    ct->size_reserved -= reserved_memory;
    cachetable_unlock(ct);
}

void
toku_cachetable_set_env_dir(CACHETABLE ct, const char *env_dir) {
    assert(!ct->set_env_dir);
    toku_free(ct->env_dir);
    ct->env_dir = toku_xstrdup(env_dir);
    ct->set_env_dir = TRUE;
}

void
toku_cachetable_set_lock_unlock_for_io (CACHETABLE ct, void (*ydb_lock_callback)(void), void (*ydb_unlock_callback)(void))
// Effect: When we do I/O we may need to release locks (e.g., the ydb lock).  These functions release the lock acquire the lock.
{
    ct->ydb_lock_callback = ydb_lock_callback;
    ct->ydb_unlock_callback = ydb_unlock_callback;
}

//
// Increment the reference count
// MUST HOLD cachetable lock
static void
cachefile_refup (CACHEFILE cf) {
    cf->refcount++;
}

int toku_cachefile_fd(CACHEFILE cf) {
    return cf->fd;
}

// What cachefile goes with particular iname (iname relative to env)?
// The transaction that is adding the reference might not have a reference
// to the brt, therefore the cachefile might be closing.
// If closing, we want to return that it is not there, but must wait till after
// the close has finished.
// Once the close has finished, there must not be a cachefile with that name
// in the cachetable.
int toku_cachefile_of_iname_in_env (CACHETABLE ct, const char *iname_in_env, CACHEFILE *cf) {
    BOOL restarted = FALSE;
    cachefiles_lock(ct);
    CACHEFILE extant;
    int r;
restart:
    r = ENOENT;
    for (extant = ct->cachefiles; extant; extant=extant->next) {
        if (extant->fname_in_env &&
            !strcmp(extant->fname_in_env, iname_in_env)) {
            assert(!restarted); //If restarted and found again, this is an error.
            if (extant->is_closing) {
                //Cachefile is closing, wait till finished.
                assert(extant->closefd_waiting==0); //Single client thread (any more and this needs to be re-analyzed).
                extant->closefd_waiting++;
		int rwait = toku_pthread_cond_wait(&extant->closefd_wait, ct->mutex); resource_assert_zero(rwait);
                restarted = TRUE;
                goto restart; //Restart and verify that it is not found in the second loop.
            }
	    *cf = extant;
	    r = 0;
            break;
	}
    }
    cachefiles_unlock(ct);
    return r;
}

// What cachefile goes with particular fd?
// This function can only be called if the brt is still open, so file must 
// still be open and cannot be in the is_closing state.
int toku_cachefile_of_filenum (CACHETABLE ct, FILENUM filenum, CACHEFILE *cf) {
    cachefiles_lock(ct);
    CACHEFILE extant;
    int r = ENOENT;
    *cf = NULL;
    for (extant = ct->cachefiles; extant; extant=extant->next) {
	if (extant->filenum.fileid==filenum.fileid) {
            assert(!extant->is_closing);
	    *cf = extant;
            r = 0;
            break;
	}
    }
    cachefiles_unlock(ct);
    return r;
}

static FILENUM next_filenum_to_use={0};

static void cachefile_init_filenum(CACHEFILE cf, int fd, const char *fname_in_env, struct fileid fileid) {
    cf->fd = fd;
    cf->fileid = fileid;
    cf->fname_in_env = toku_xstrdup(fname_in_env);
}

// If something goes wrong, close the fd.  After this, the caller shouldn't close the fd, but instead should close the cachefile.
int toku_cachetable_openfd (CACHEFILE *cfptr, CACHETABLE ct, int fd, const char *fname_in_env) {
    return toku_cachetable_openfd_with_filenum(cfptr, ct, fd, fname_in_env, FALSE, next_filenum_to_use, FALSE);
}

static int
find_by_filenum (OMTVALUE v, void *filenumv) {
    FILENUM fnum     = *(FILENUM*)v;
    FILENUM fnumfind = *(FILENUM*)filenumv;
    if (fnum.fileid<fnumfind.fileid) return -1;
    if (fnum.fileid>fnumfind.fileid) return +1;
    return 0;
}

static BOOL
is_filenum_reserved(CACHETABLE ct, FILENUM filenum) {
    OMTVALUE v;
    int r;
    BOOL rval;

    r = toku_omt_find_zero(ct->reserved_filenums, find_by_filenum, &filenum, &v, NULL);
    if (r==0) {
        FILENUM* found = v;
        assert(found->fileid == filenum.fileid);
        rval = TRUE;
    }
    else {
        assert(r==DB_NOTFOUND);
        rval = FALSE;
    }
    return rval;
}

static void
reserve_filenum(CACHETABLE ct, FILENUM filenum) {
    int r;
    assert(filenum.fileid != FILENUM_NONE.fileid);

    uint32_t index;
    r = toku_omt_find_zero(ct->reserved_filenums, find_by_filenum, &filenum, NULL, &index);
    assert(r==DB_NOTFOUND);
    FILENUM *XMALLOC(entry);
    *entry = filenum;
    r = toku_omt_insert_at(ct->reserved_filenums, entry, index);
    assert(r==0);
}

static void
unreserve_filenum(CACHETABLE ct, FILENUM filenum) {
    OMTVALUE v;
    int r;

    uint32_t index;
    r = toku_omt_find_zero(ct->reserved_filenums, find_by_filenum, &filenum, &v, &index);
    assert(r==0);
    FILENUM* found = v;
    assert(found->fileid == filenum.fileid);
    toku_free(found);
    r = toku_omt_delete_at(ct->reserved_filenums, index);
    assert(r==0);
}

    
int
toku_cachetable_reserve_filenum (CACHETABLE ct, FILENUM *reserved_filenum, BOOL with_filenum, FILENUM filenum) {
    int r;
    CACHEFILE extant;
    
    cachetable_lock(ct);
    cachefiles_lock(ct);

    if (with_filenum) {
        // verify that filenum is not in use
        for (extant = ct->cachefiles; extant; extant=extant->next) {
            if (filenum.fileid == extant->filenum.fileid) {
                r = EEXIST;
                goto exit;
            }
        }
        if (is_filenum_reserved(ct, filenum)) {
            r = EEXIST;
            goto exit;
        }
    } else {
        // find an unused fileid and use it
    try_again:
        for (extant = ct->cachefiles; extant; extant=extant->next) {
            if (next_filenum_to_use.fileid==extant->filenum.fileid) {
                next_filenum_to_use.fileid++;
                goto try_again;
            }
        }
        if (is_filenum_reserved(ct, next_filenum_to_use)) {
            next_filenum_to_use.fileid++;
            goto try_again;
        }
    }
    {
        //Reserve a filenum.
        FILENUM reserved;
        if (with_filenum)
            reserved.fileid = filenum.fileid;
        else
            reserved.fileid = next_filenum_to_use.fileid++;
        reserve_filenum(ct, reserved);
        *reserved_filenum = reserved;
        r = 0;
    }
 exit:
    cachefiles_unlock(ct);
    cachetable_unlock(ct);
    return r;
}

void
toku_cachetable_unreserve_filenum (CACHETABLE ct, FILENUM reserved_filenum) {
    cachetable_lock(ct);
    cachefiles_lock(ct);
    unreserve_filenum(ct, reserved_filenum);
    cachefiles_unlock(ct);
    cachetable_unlock(ct);
}

int toku_cachetable_openfd_with_filenum (CACHEFILE *cfptr, CACHETABLE ct, int fd, 
					 const char *fname_in_env,
					 BOOL with_filenum, FILENUM filenum, BOOL reserved) {
    int r;
    CACHEFILE extant;
    struct fileid fileid;
    
    if (with_filenum) assert(filenum.fileid != FILENUM_NONE.fileid);
    if (reserved) assert(with_filenum);
    r = toku_os_get_unique_file_id(fd, &fileid);
    if (r != 0) { 
        r=errno; close(fd); // no change for t:2444
        return r;
    }
    r = toku_pthread_mutex_lock(&ct->openfd_mutex);   // purpose is to make this function single-threaded
    resource_assert_zero(r);
    cachetable_lock(ct);
    cachefiles_lock(ct);
    for (extant = ct->cachefiles; extant; extant=extant->next) {
	if (memcmp(&extant->fileid, &fileid, sizeof(fileid))==0) {
            //File is already open (and in cachetable as extant)
            cachefile_refup(extant);
	    if (extant->is_closing) {
		// if another thread is closing this file, wait until the close is fully complete
                cachefiles_unlock(ct); //Cannot hold cachefiles lock over the cond_wait
		r = toku_pthread_cond_wait(&extant->openfd_wait, ct->mutex); 
                resource_assert_zero(r);
                cachefiles_lock(ct);
		goto try_again;    // other thread has closed this file, go create a new cachefile
	    }	    
            assert(!is_filenum_reserved(ct, extant->filenum));
	    r = close(fd);  // no change for t:2444
            assert(r == 0);
	    // re-use pre-existing cachefile 
	    *cfptr = extant;
	    r = 0;
	    goto exit;
	}
    }

    //File is not open.  Make a new cachefile.

    if (with_filenum) {
        // verify that filenum is not in use
        for (extant = ct->cachefiles; extant; extant=extant->next) {
            if (filenum.fileid == extant->filenum.fileid) {
                r = EEXIST;
                goto exit;
            }
        }
        if (is_filenum_reserved(ct, filenum)) {
            if (reserved)
                unreserve_filenum(ct, filenum);
            else {
                r = EEXIST;
                goto exit;
            }
        }
    } else {
        // find an unused fileid and use it
    try_again:
        assert(next_filenum_to_use.fileid != FILENUM_NONE.fileid);
        for (extant = ct->cachefiles; extant; extant=extant->next) {
            if (next_filenum_to_use.fileid==extant->filenum.fileid) {
                next_filenum_to_use.fileid++;
                goto try_again;
            }
        }
        if (is_filenum_reserved(ct, next_filenum_to_use)) {
            next_filenum_to_use.fileid++;
            goto try_again;
        }
    }
    {
	// create a new cachefile entry in the cachetable
        CACHEFILE XCALLOC(newcf);
        newcf->cachetable = ct;
        newcf->filenum.fileid = with_filenum ? filenum.fileid : next_filenum_to_use.fileid++;
        cachefile_init_filenum(newcf, fd, fname_in_env, fileid);
	newcf->refcount = 1;
	newcf->next = ct->cachefiles;
	ct->cachefiles = newcf;

        rwlock_init(&newcf->fdlock);
        rwlock_init(&newcf->checkpoint_lock);
        newcf->most_recent_global_checkpoint_that_finished_early = ZERO_LSN;
        newcf->for_local_checkpoint = ZERO_LSN;
        newcf->checkpoint_state = CS_NOT_IN_PROGRESS;

	r = toku_pthread_cond_init(&newcf->openfd_wait, NULL); resource_assert_zero(r);
	r = toku_pthread_cond_init(&newcf->closefd_wait, NULL); resource_assert_zero(r);
        r = toku_pthread_cond_init(&newcf->background_wait, NULL); resource_assert_zero(r);
        toku_list_init(&newcf->pairs_for_cachefile);
	*cfptr = newcf;
	r = 0;
    }
 exit:
    cachefiles_unlock(ct);
    {
	int rm = toku_pthread_mutex_unlock(&ct->openfd_mutex);
	resource_assert_zero(rm);
    } 
    cachetable_unlock(ct);
    return r;
}

static int cachetable_flush_cachefile (CACHETABLE, CACHEFILE cf);
static void assert_cachefile_is_flushed_and_removed (CACHETABLE ct, CACHEFILE cf);

//TEST_ONLY_FUNCTION
int toku_cachetable_openf (CACHEFILE *cfptr, CACHETABLE ct, const char *fname_in_env, int flags, mode_t mode) {
    char *fname_in_cwd = toku_construct_full_name(2, ct->env_dir, fname_in_env);
    int fd = open(fname_in_cwd, flags+O_BINARY, mode);
    int r;
    if (fd<0) r = errno;
    else      r = toku_cachetable_openfd (cfptr, ct, fd, fname_in_env);
    toku_free(fname_in_cwd);
    return r;
}

WORKQUEUE toku_cachetable_get_workqueue(CACHETABLE ct) {
    return &ct->wq;
}

void toku_cachefile_get_workqueue_load (CACHEFILE cf, int *n_in_queue, int *n_threads) {
    CACHETABLE ct = cf->cachetable;
    *n_in_queue = workqueue_n_in_queue(&ct->wq, 1);
    *n_threads  = toku_thread_pool_get_current_threads(ct->threadpool);
}

//Test-only function
int toku_cachefile_set_fd (CACHEFILE cf, int fd, const char *fname_in_env) {
    int r;
    struct fileid fileid;
    (void)toku_cachefile_get_and_pin_fd(cf);
    r=toku_os_get_unique_file_id(fd, &fileid);
    if (r != 0) { 
        r=errno; close(fd); goto cleanup; // no change for t:2444
    }
    if (cf->close_userdata && (r = cf->close_userdata(cf, cf->fd, cf->userdata, 0, FALSE, ZERO_LSN))) {
        goto cleanup;
    }
    cf->close_userdata = NULL;
    cf->checkpoint_userdata = NULL;
    cf->begin_checkpoint_userdata = NULL;
    cf->end_checkpoint_userdata = NULL;
    cf->userdata = NULL;

    close(cf->fd); // no change for t:2444
    cf->fd = -1;
    if (cf->fname_in_env) {
	toku_free(cf->fname_in_env);
	cf->fname_in_env = NULL;
    }
    //It is safe to have the name repeated since this is a newbrt-only test function.
    //There isn't an environment directory so its both env/cwd.
    cachefile_init_filenum(cf, fd, fname_in_env, fileid);
    r = 0;
cleanup:
    toku_cachefile_unpin_fd(cf);
    return r;
}

char *
toku_cachefile_fname_in_env (CACHEFILE cf) {
    return cf->fname_in_env;
}

int toku_cachefile_get_and_pin_fd (CACHEFILE cf) {
    CACHETABLE ct = cf->cachetable;
    cachetable_lock(ct);
    rwlock_prefer_read_lock(&cf->fdlock, cf->cachetable->mutex);
    cachetable_unlock(ct);
    return cf->fd;
}

void toku_cachefile_unpin_fd (CACHEFILE cf) {
    CACHETABLE ct = cf->cachetable;
    cachetable_lock(ct);
    rwlock_read_unlock(&cf->fdlock);
    cachetable_unlock(ct);
}

//Must be holding a read or write lock on cf->fdlock
BOOL
toku_cachefile_is_dev_null_unlocked (CACHEFILE cf) {
    return cf->is_dev_null;
}

//Must already be holding fdlock (read or write)
int
toku_cachefile_truncate (CACHEFILE cf, toku_off_t new_size) {
    int r;
    if (toku_cachefile_is_dev_null_unlocked(cf)) r = 0; //Don't truncate /dev/null
    else {
        r = ftruncate(cf->fd, new_size);
        if (r != 0)
            r = errno;
    }
    return r;
}

static CACHEFILE remove_cf_from_list_locked (CACHEFILE cf, CACHEFILE list) {
    if (list==0) return 0;
    else if (list==cf) {
	return list->next;
    } else {
	list->next = remove_cf_from_list_locked(cf, list->next);
	return list;
    }
}

static void remove_cf_from_cachefiles_list (CACHEFILE cf) {
    CACHETABLE ct = cf->cachetable;
    cachefiles_lock(ct);
    ct->cachefiles = remove_cf_from_list_locked(cf, ct->cachefiles);
    cachefiles_unlock(ct);
}

void toku_cachefile_wait_for_background_work_to_quiesce(CACHEFILE cf) {
    wait_on_background_jobs_to_finish(cf);
}

int toku_cachefile_close (CACHEFILE *cfp, char **error_string, BOOL oplsn_valid, LSN oplsn) {

    CACHEFILE cf = *cfp;
    CACHETABLE ct = cf->cachetable;
    cachetable_lock(ct);
    cf->is_flushing = true;
    cachetable_unlock(ct);
    wait_on_background_jobs_to_finish(cf);
    cachetable_lock(ct);
    assert(cf->refcount>0);
    if (oplsn_valid)
        assert(cf->refcount==1); //Recovery is trying to force an lsn.  Must get passed through.
    cf->refcount--;
    if (cf->refcount==0) {
        //Checkpoint holds a reference, close should be impossible if still in use by a checkpoint.
        assert(!cf->next_in_checkpoint);
        assert(!cf->for_checkpoint);
        assert(!cf->is_closing);
        cf->is_closing = TRUE; //Mark this cachefile so that no one will re-use it.
	int r;
	// cachetable_flush_cachefile() may release and retake cachetable_lock,
	// allowing another thread to get into either/both of
        //  - toku_cachetable_openfd()
        //  - toku_cachefile_of_iname_and_add_reference()
	if ((r = cachetable_flush_cachefile(ct, cf))) {
	error:
	    remove_cf_from_cachefiles_list(cf);
	    if (cf->refcount > 0) {
                int rs;
		assert(cf->refcount == 1);       // toku_cachetable_openfd() is single-threaded
                assert(!cf->next_in_checkpoint); //checkpoint cannot run on a closing file
                assert(!cf->for_checkpoint);     //checkpoint cannot run on a closing file
		rs = toku_pthread_cond_signal(&cf->openfd_wait); resource_assert_zero(rs);
	    }
            if (cf->closefd_waiting > 0) {
                int rs;
                assert(cf->closefd_waiting == 1);
		rs = toku_pthread_cond_signal(&cf->closefd_wait); assert(rs == 0);
            }
	    // we can destroy the condition variables because if there was another thread waiting, it was already signalled
            {
                int rd;
                rd = toku_pthread_cond_destroy(&cf->openfd_wait);
                resource_assert_zero(rd);
                rd = toku_pthread_cond_destroy(&cf->closefd_wait);
                resource_assert_zero(rd);
            }
	    if (cf->fname_in_env) toku_free(cf->fname_in_env);

            rwlock_write_lock(&cf->fdlock, ct->mutex);
            if ( !toku_cachefile_is_dev_null_unlocked(cf) ) {
                int r3 = toku_file_fsync_without_accounting(cf->fd); //t:2444
                if (r3!=0) fprintf(stderr, "%s:%d During error handling, could not fsync file r=%d errno=%d\n", __FILE__, __LINE__, r3, errno);
            }
	    int r2 = close(cf->fd);
	    if (r2!=0) fprintf(stderr, "%s:%d During error handling, could not close file r=%d errno=%d\n", __FILE__, __LINE__, r2, errno);
	    //assert(r == 0);
            rwlock_write_unlock(&cf->fdlock);
            rwlock_destroy(&cf->fdlock);
            rwlock_write_lock(&cf->checkpoint_lock, ct->mutex); //Just to make sure we can get it
            rwlock_write_unlock(&cf->checkpoint_lock);
            rwlock_destroy(&cf->checkpoint_lock);
            assert(toku_list_empty(&cf->pairs_for_cachefile));
	    toku_free(cf);
	    *cfp = NULL;
	    cachetable_unlock(ct);
	    return r;
        }
	if (cf->close_userdata) {
            rwlock_prefer_read_lock(&cf->fdlock, ct->mutex);
            r = cf->close_userdata(cf, cf->fd, cf->userdata, error_string, oplsn_valid, oplsn);
            rwlock_read_unlock(&cf->fdlock);
            if (r!=0) goto error;
	}
       	cf->close_userdata = NULL;
	cf->checkpoint_userdata = NULL;
	cf->begin_checkpoint_userdata = NULL;
	cf->end_checkpoint_userdata = NULL;
	cf->userdata = NULL;
        remove_cf_from_cachefiles_list(cf);
        // refcount could be non-zero if another thread is trying to open this cachefile,
	// but is blocked in toku_cachetable_openfd() waiting for us to finish closing it.
	if (cf->refcount > 0) {
            int rs;
	    assert(cf->refcount == 1);   // toku_cachetable_openfd() is single-threaded
	    rs = toku_pthread_cond_signal(&cf->openfd_wait); resource_assert_zero(rs);
	}
        if (cf->closefd_waiting > 0) {
            int rs;
            assert(cf->closefd_waiting == 1);
            rs = toku_pthread_cond_signal(&cf->closefd_wait); resource_assert_zero(rs);
        }
        // we can destroy the condition variables because if there was another thread waiting, it was already signalled
        {
            int rd;
            rd = toku_pthread_cond_destroy(&cf->openfd_wait);
            resource_assert_zero(rd);
            rd = toku_pthread_cond_destroy(&cf->closefd_wait);
            resource_assert_zero(rd);
            rd = toku_pthread_cond_destroy(&cf->background_wait);
            resource_assert_zero(rd);
        }
        rwlock_write_lock(&cf->fdlock, ct->mutex); //Just make sure we can get it.
        cachetable_unlock(ct);

        if ( !toku_cachefile_is_dev_null_unlocked(cf) ) {
            r = toku_file_fsync_without_accounting(cf->fd); //t:2444
            assert(r == 0);   
        }

        cachetable_lock(ct);
        rwlock_write_unlock(&cf->fdlock);
        rwlock_destroy(&cf->fdlock);
        rwlock_write_lock(&cf->checkpoint_lock, ct->mutex); //Just to make sure we can get it
        rwlock_write_unlock(&cf->checkpoint_lock);
        rwlock_destroy(&cf->checkpoint_lock);
        assert(toku_list_empty(&cf->pairs_for_cachefile));
        cachetable_unlock(ct);

	r = close(cf->fd);
	assert(r == 0);
        cf->fd = -1;

	if (cf->fname_in_env) toku_free(cf->fname_in_env);
	toku_free(cf);
	*cfp=NULL;
	return r;
    } else {
        cachetable_unlock(ct);
	*cfp=NULL;
	return 0;
    }
}

//
// This client calls this function to flush all PAIRs belonging to
// a cachefile from the cachetable. The client must ensure that
// while this function is called, no other thread does work on the 
// cachefile.
//
int toku_cachefile_flush (CACHEFILE cf) {
    CACHETABLE ct = cf->cachetable;
    cachetable_lock(ct);
    cf->is_flushing = true;
    cachetable_unlock(ct);
    wait_on_background_jobs_to_finish(cf);
    cachetable_lock(ct);
    int r = cachetable_flush_cachefile(ct, cf);
    cachetable_unlock(ct);
    return r;
}

// This hash function comes from Jenkins:  http://burtleburtle.net/bob/c/lookup3.c
// The idea here is to mix the bits thoroughly so that we don't have to do modulo by a prime number.
// Instead we can use a bitmask on a table of size power of two.
// This hash function does yield improved performance on ./db-benchmark-test-tokudb and ./scanscan
static inline u_int32_t rot(u_int32_t x, u_int32_t k) {
    return (x<<k) | (x>>(32-k));
}
static inline u_int32_t final (u_int32_t a, u_int32_t b, u_int32_t c) {
    c ^= b; c -= rot(b,14);
    a ^= c; a -= rot(c,11);
    b ^= a; b -= rot(a,25);
    c ^= b; c -= rot(b,16);
    a ^= c; a -= rot(c,4); 
    b ^= a; b -= rot(a,14);
    c ^= b; c -= rot(b,24);
    return c;
}

u_int32_t toku_cachetable_hash (CACHEFILE cachefile, BLOCKNUM key)
// Effect: Return a 32-bit hash key.  The hash key shall be suitable for using with bitmasking for a table of size power-of-two.
{
    return final(cachefile->filenum.fileid, (u_int32_t)(key.b>>32), (u_int32_t)key.b);
}

#if 0
static unsigned int hashit (CACHETABLE ct, CACHEKEY key, CACHEFILE cachefile) {
    assert(0==(ct->table_size & (ct->table_size -1))); // make sure table is power of two
    return (toku_cachetable_hash(key,cachefile))&(ct->table_size-1);
}
#endif

static void cachetable_rehash (CACHETABLE ct, u_int32_t newtable_size) {
    // printf("rehash %p %d %d %d\n", t, primeindexdelta, ct->n_in_table, ct->table_size);

    assert(newtable_size>=4 && ((newtable_size & (newtable_size-1))==0));
    PAIR *newtable = toku_calloc(newtable_size, sizeof(*ct->table));
    u_int32_t i;
    //printf("%s:%d newtable_size=%d\n", __FILE__, __LINE__, newtable_size);
    assert(newtable!=0);
    u_int32_t oldtable_size = ct->table_size;
    ct->table_size=newtable_size;
    for (i=0; i<newtable_size; i++) newtable[i]=0;
    for (i=0; i<oldtable_size; i++) {
	PAIR p;
	while ((p=ct->table[i])!=0) {
	    unsigned int h = p->fullhash&(newtable_size-1);
	    ct->table[i] = p->hash_chain;
	    p->hash_chain = newtable[h];
	    newtable[h] = p;
	}
    }
    toku_free(ct->table);
    // printf("Freed\n");
    ct->table=newtable;
    //printf("Done growing or shrinking\n");
}

#define CLOCK_SATURATION 15
#define CLOCK_INITIAL_COUNT 3
#define CLOCK_WRITE_OUT 1

static void pair_remove (CACHETABLE ct, PAIR p) {
    if (p->clock_prev == p) {
        assert(ct->clock_head == p);
        assert(p->clock_next == p);
        assert(ct->cleaner_head == p);
        ct->clock_head = NULL;
        ct->cleaner_head = NULL;
    }
    else {
        if (p == ct->clock_head) {
            ct->clock_head = ct->clock_head->clock_next;
        }
        if (p == ct->cleaner_head) {
            ct->cleaner_head = ct->cleaner_head->clock_next;
        }
        p->clock_prev->clock_next = p->clock_next;
        p->clock_next->clock_prev = p->clock_prev;
        
    }
}

static void pair_add_to_clock (CACHETABLE ct, PAIR p) {
    // requires that p is not currently in the table.
    // inserts p into the clock list at the tail.

    p->count = CLOCK_INITIAL_COUNT;
    //assert either both head and tail are set or they are both NULL
    // tail and head exist
    if (ct->clock_head) {
        assert(ct->cleaner_head);
        // insert right before the head
        p->clock_next = ct->clock_head;
        p->clock_prev = ct->clock_head->clock_prev;

        p->clock_prev->clock_next = p;
        p->clock_next->clock_prev = p;

    }
    // this is the first element in the list
    else {
        ct->clock_head = p;
        p->clock_next = p->clock_prev = ct->clock_head;
        ct->cleaner_head = p;
    }
}

static void pair_touch (PAIR p) {
    p->count = (p->count < CLOCK_SATURATION) ? p->count+1 : CLOCK_SATURATION;
}

static PAIR remove_from_hash_chain (PAIR remove_me, PAIR list) {
    if (remove_me==list) return list->hash_chain;
    list->hash_chain = remove_from_hash_chain(remove_me, list->hash_chain);
    return list;
}

//Remove a pair from the list of pairs that were marked with the
//pending bit for the in-progress checkpoint.
//Requires: cachetable lock is held during duration.
static void
pending_pairs_remove (CACHETABLE ct, PAIR p) {
    if (p->pending_next) {
	p->pending_next->pending_prev = p->pending_prev;
    }
    if (p->pending_prev) {
	p->pending_prev->pending_next = p->pending_next;
    }
    else if (ct->pending_head==p) {
	ct->pending_head = p->pending_next;
    }
    p->pending_prev = p->pending_next = NULL;
}

static void
cachetable_remove_pair_attr (CACHETABLE ct, PAIR_ATTR attr) {
    ct->size_current -= attr.size;
    ct->size_nonleaf -= attr.nonleaf_size;
    ct->size_leaf -= attr.leaf_size;
    ct->size_rollback -= attr.rollback_size;
    ct->size_cachepressure -= attr.cache_pressure_size;
    assert(ct->size_current >= 0);
}

static void
cachetable_add_pair_attr(CACHETABLE ct, PAIR_ATTR attr) {
    ct->size_current += attr.size;
    if (ct->size_current > ct->size_max) {
        ct->size_max = ct->size_current;
    }
    ct->size_nonleaf += attr.nonleaf_size;
    ct->size_leaf += attr.leaf_size;
    ct->size_rollback += attr.rollback_size;
    ct->size_cachepressure += attr.cache_pressure_size;
}

static void
cachetable_change_pair_attr(CACHETABLE ct, PAIR_ATTR old_attr, PAIR_ATTR new_attr) {
    cachetable_add_pair_attr(ct, new_attr);
    cachetable_remove_pair_attr(ct, old_attr);
}

// Remove a pair from the cachetable
// Effects: the pair is removed from the LRU list and from the cachetable's hash table.
// The size of the objects in the cachetable is adjusted by the size of the pair being
// removed.

static void cachetable_remove_pair (CACHETABLE ct, PAIR p) {
    pair_remove(ct, p);
    pending_pairs_remove(ct, p);
    toku_list_remove(&p->next_for_cachefile);

    assert(ct->n_in_table>0);
    ct->n_in_table--;
    // Remove it from the hash chain.
    {
	unsigned int h = p->fullhash&(ct->table_size-1);
	ct->table[h] = remove_from_hash_chain (p, ct->table[h]);
    }
    cachetable_remove_pair_attr(ct, p->attr);
    p->already_removed = TRUE;
}

// Maybe remove a pair from the cachetable and free it, depending on whether
// or not there are any threads interested in the pair.  The flush callback
// is called with write_me and keep_me both false, and the pair is destroyed.

static void cachetable_maybe_remove_and_free_pair (CACHETABLE ct, PAIR p, BOOL* destroyed) {
    *destroyed = FALSE;
    if (nb_mutex_users(&p->nb_mutex) == 0) {
        cachetable_remove_pair(ct, p);

        // helgrind
        CACHETABLE_FLUSH_CALLBACK flush_callback = p->flush_callback;
        CACHEFILE cachefile = p->cachefile;
        CACHEKEY key = p->key;
        void *value = p->value;
        void *write_extraargs = p->write_extraargs;
        PAIR_ATTR old_attr = p->attr;

        rwlock_prefer_read_lock(&cachefile->fdlock, ct->mutex);
        cachetable_evictions++;
        cachetable_unlock(ct);
        PAIR_ATTR new_attr = p->attr;
        flush_callback(cachefile, cachefile->fd, key, value, write_extraargs, old_attr, &new_attr, FALSE, FALSE, TRUE);

        cachetable_lock(ct);
        rwlock_read_unlock(&cachefile->fdlock);

        ctpair_destroy(p);
        *destroyed = TRUE;
    }
}

// Read a pair from a cachefile into memory using the pair's fetch callback
static void cachetable_fetch_pair(
    CACHETABLE ct, 
    CACHEFILE cf, 
    PAIR p, 
    CACHETABLE_FETCH_CALLBACK fetch_callback, 
    void* read_extraargs
    ) 
{
    // helgrind
    CACHEKEY key = p->key;
    u_int32_t fullhash = p->fullhash;

    void *toku_value = 0;
    PAIR_ATTR attr;
    
    int dirty = 0;

    WHEN_TRACE_CT(printf("%s:%d CT: fetch_callback(%lld...)\n", __FILE__, __LINE__, key));    

    rwlock_prefer_read_lock(&cf->fdlock, ct->mutex);
    cachetable_unlock(ct);

    int r;
    assert(!toku_cachefile_is_dev_null_unlocked(cf));
    r = fetch_callback(cf, cf->fd, key, fullhash, &toku_value, &attr, &dirty, read_extraargs);
    if (dirty)
	p->dirty = CACHETABLE_DIRTY;

    cachetable_lock(ct);
    rwlock_read_unlock(&cf->fdlock);
    // brt.c asserts that get_and_pin succeeds,
    // so we might as well just assert it here as opposed
    // to trying to support an INVALID state
    assert(r == 0);

    p->value = toku_value;
    p->attr = attr;
    cachetable_add_pair_attr(ct, attr);
    if (p->cq) {
        workitem_init(&p->asyncwork, NULL, p);
        workqueue_enq(p->cq, &p->asyncwork, 1);
        return;
    }
    p->state = CTPAIR_IDLE;
    
    nb_mutex_write_unlock(&p->nb_mutex);
    if (0) printf("%s:%d %"PRId64" complete\n", __FUNCTION__, __LINE__, key.b);
}

static void cachetable_complete_write_pair (CACHETABLE ct, PAIR p, BOOL do_remove, BOOL* destroyed);


static void cachetable_write_locked_pair(CACHETABLE ct, PAIR p) {
    rwlock_read_lock(&ct->pending_lock, ct->mutex);
    
    // helgrind
    CACHETABLE_FLUSH_CALLBACK flush_callback = p->flush_callback;
    CACHEFILE cachefile = p->cachefile;
    CACHEKEY key = p->key;
    void *value = p->value;
    void *write_extraargs = p->write_extraargs;
    PAIR_ATTR old_attr = p->attr;
    PAIR_ATTR new_attr = p->attr;
    BOOL dowrite = (BOOL)(p->dirty);
    BOOL for_checkpoint = p->checkpoint_pending;
    
    //Must set to FALSE before releasing cachetable lock
    p->checkpoint_pending = FALSE;
    rwlock_prefer_read_lock(&cachefile->fdlock, ct->mutex);
    cachetable_unlock(ct);
    
    // write callback
    if (toku_cachefile_is_dev_null_unlocked(cachefile)) dowrite = FALSE;
    flush_callback(cachefile, cachefile->fd, key, value, write_extraargs, old_attr, &new_attr, dowrite, TRUE, for_checkpoint);
    
    cachetable_lock(ct);
    //
    // now let's update variables
    //
    p->attr = new_attr;
    cachetable_change_pair_attr(ct, old_attr, new_attr);
    rwlock_read_unlock(&cachefile->fdlock);
    
    // the pair is no longer dirty once written
    p->dirty = CACHETABLE_CLEAN;
    
    assert(!p->checkpoint_pending);
    rwlock_read_unlock(&ct->pending_lock);
}

// Write a pair to storage
// Effects: an exclusive lock on the pair is obtained, the write callback is called,
// the pair dirty state is adjusted, and the write is completed.  The keep_me
// boolean is true, so the pair is not yet evicted from the cachetable.
// Requires: This thread must hold the write lock for the pair.
static void cachetable_write_pair(CACHETABLE ct, PAIR p, BOOL remove_me) {
    long old_size = p->attr.size;
    // this function may change p->attr.size, so we saved
    // the estimate we must have put into ct->evicting_size above
    cachetable_write_locked_pair(ct, p);
    
    // maybe wakeup any stalled writers when the pending writes fall below 
    // 1/8 of the size of the cachetable
    if (remove_me) {
        ct->size_evicting -= old_size; 
        assert(ct->size_evicting  >= 0);
        if (8*ct->size_evicting  <= ct->size_current) {
            workqueue_wakeup_write(&ct->wq, 0);
        }
    }

    // stuff it into a completion queue for delayed completion if a completion queue exists
    // otherwise complete the write now
    if (p->cq)
        workqueue_enq(p->cq, &p->asyncwork, 1);
    else {
        BOOL destroyed;
        cachetable_complete_write_pair(ct, p, remove_me, &destroyed);
    }
}

// complete the write of a pair by reseting the writing flag, and 
// maybe removing the pair from the cachetable if there are no
// references to it

static void cachetable_complete_write_pair (CACHETABLE ct, PAIR p, BOOL do_remove, BOOL* destroyed) {
    p->cq = 0;
    p->state = CTPAIR_IDLE;
    
    nb_mutex_write_unlock(&p->nb_mutex);
    if (do_remove) {
        cachetable_maybe_remove_and_free_pair(ct, p, destroyed);
    }
}

static void try_evict_pair(CACHETABLE ct, PAIR p) {
    // evictions without a write or unpinned pair's that are clean
    // can be run in the current thread

    // must check for before we grab the write lock because we may
    // be trying to evict something this thread is trying to read
    if (!nb_mutex_users(&p->nb_mutex)) {
        nb_mutex_write_lock(&p->nb_mutex, ct->mutex);
        p->state = CTPAIR_WRITING;

        assert(ct->size_evicting >= 0);
        ct->size_evicting += p->attr.size;
        assert(ct->size_evicting >= 0);
        
        if (!p->dirty) {
            cachetable_write_pair(ct, p, TRUE);
        }
        else {
            p->remove_me = TRUE;
            WORKITEM wi = &p->asyncwork;
            workitem_init(wi, cachetable_writer, p);
            workqueue_enq(&ct->wq, wi, 0);
        }
    }
}

// flush and remove a pair from the cachetable.  the callbacks are run by a thread in
// a thread pool.
// flush and remove a pair from the cachetable.  the callbacks are run by a thread in
// a thread pool.
static void flush_and_maybe_remove (CACHETABLE ct, PAIR p) {
    nb_mutex_write_lock(&p->nb_mutex, ct->mutex);
    p->state = CTPAIR_WRITING;
    // this needs to be done here regardless of whether the eviction occurs on the main thread or on
    // a writer thread, because there may be a completion queue that needs access to this information
    WORKITEM wi = &p->asyncwork;
    // we are not going to remove if we are posting a dirty node
    // to the writer thread.
    // In that case, we will let the caller decide if they want to remove it.
    // We may be able to just let the writer thread evict the node,
    // but I (Zardosht) do not understand the caller well enough
    // so I am hesitant to change it.
    p->remove_me = FALSE;
    workitem_init(wi, cachetable_writer, p);
    // evictions without a write or unpinned pair's that are clean
    // can be run in the current thread
    if (!nb_mutex_writers(&p->nb_mutex) && !p->dirty) {
        assert(ct->size_evicting >= 0);
        ct->size_evicting += p->attr.size;
        assert(ct->size_evicting >= 0);
        cachetable_write_pair(ct, p, TRUE);
    } 
    else {
        workqueue_enq(&ct->wq, wi, 0);
    }
}

static void do_partial_eviction(CACHETABLE ct, PAIR p) {
    // This really should be something else, but need to set it to something
    // other than CTPAIR_IDLE so that other threads know to not hold
    // ydb lock while waiting on this node
    p->state = CTPAIR_WRITING;
    PAIR_ATTR new_attr;
    PAIR_ATTR old_attr = p->attr;
    
    cachetable_unlock(ct);
    p->pe_callback(p->value, old_attr, &new_attr, p->write_extraargs);
    cachetable_lock(ct);

    cachetable_change_pair_attr(ct, old_attr, new_attr);
    p->attr = new_attr;

    assert(ct->size_evicting >= p->size_evicting_estimate);
    ct->size_evicting -= p->size_evicting_estimate;
    if (8*ct->size_evicting  <= ct->size_current) {
        workqueue_wakeup_write(&ct->wq, 0);
    }

    p->state = CTPAIR_IDLE;
    if (p->cq) {
        workitem_init(&p->asyncwork, NULL, p);
        workqueue_enq(p->cq, &p->asyncwork, 1);
    }
    else {
        nb_mutex_write_unlock(&p->nb_mutex);
    }
}

static void cachetable_partial_eviction(WORKITEM wi) {
    PAIR p = workitem_arg(wi);
    CACHETABLE ct = p->cachefile->cachetable;
    cachetable_lock(ct);
    do_partial_eviction(ct,p);
    cachetable_unlock(ct);
}

static int maybe_flush_some (CACHETABLE ct, long size) {
    int r = 0;

    //
    // These variables will help us detect if everything in the clock is currently being accessed.
    // We must detect this case otherwise we will end up in an infinite loop below.
    //
    CACHEKEY curr_cachekey;
    curr_cachekey.b = INT64_MAX; // create initial value so compiler does not complain
    FILENUM curr_filenum;
    curr_filenum.fileid = UINT32_MAX; // create initial value so compiler does not complain
    BOOL set_val = FALSE;
    
    while ((ct->clock_head) && (size + ct->size_current > ct->size_limit + ct->size_evicting)) {
        PAIR curr_in_clock = ct->clock_head;
        if (nb_mutex_users(&curr_in_clock->nb_mutex)) {
            if (set_val && 
                curr_in_clock->key.b == curr_cachekey.b &&
                curr_in_clock->cachefile->filenum.fileid == curr_filenum.fileid)
            {
                // we have identified a cycle where everything in the clock is in use
                // do not return an error
                // just let memory be overfull
                r = 0;
                goto exit;
            }
            else {
                if (!set_val) {
                    set_val = TRUE;
                    curr_cachekey = ct->clock_head->key;
                    curr_filenum = ct->clock_head->cachefile->filenum;
                }
            }
        }
        else {
            set_val = FALSE;
            if (curr_in_clock->count > 0) {
                curr_in_clock->count--;
                // call the partial eviction callback
                nb_mutex_write_lock(&curr_in_clock->nb_mutex, ct->mutex);

                void *value = curr_in_clock->value;
                void *write_extraargs = curr_in_clock->write_extraargs;
                enum partial_eviction_cost cost;
                long bytes_freed_estimate = 0;
                curr_in_clock->pe_est_callback(
                    value, 
                    &bytes_freed_estimate, 
                    &cost, 
                    write_extraargs
                    );
                if (cost == PE_CHEAP) {
                    curr_in_clock->size_evicting_estimate = 0;
                    do_partial_eviction(ct, curr_in_clock);
                }
                else if (cost == PE_EXPENSIVE) {
                    // only bother running an expensive partial eviction
                    // if it is expected to free space
                    if (bytes_freed_estimate > 0) {
                        curr_in_clock->size_evicting_estimate = bytes_freed_estimate;
                        ct->size_evicting += bytes_freed_estimate;
                        // set the state before we post on writer thread (and give up cachetable lock)
                        // if we do not set the state here, any thread that tries
                        // to access this node in between when this function exits and the
                        // workitem gets placed on a writer thread will block (because it will think
                        // that the PAIR's lock will be released soon), and keep blocking
                        // until the workitem is done. This would be bad, as that thread may
                        // be holding the ydb lock.
                        curr_in_clock->state = CTPAIR_WRITING;
                        WORKITEM wi = &curr_in_clock->asyncwork;
                        workitem_init(wi, cachetable_partial_eviction, curr_in_clock);
                        workqueue_enq(&ct->wq, wi, 0);
                    }
                    else {
                        nb_mutex_write_unlock(&curr_in_clock->nb_mutex);
                    }
                }
                else {
                    assert(FALSE);
                }
                
            }
            else {
                try_evict_pair(ct, curr_in_clock);
            }
        }
        // at this point, either curr_in_clock is still in the list because it has not been fully evicted,
        // and we need to move ct->clock_head over. Otherwise, curr_in_clock has been fully evicted
        // and we do NOT need to move ct->clock_head, as the removal of curr_in_clock
        // modified ct->clock_head
        if (ct->clock_head && (ct->clock_head == curr_in_clock)) {
            ct->clock_head = ct->clock_head->clock_next;
        }
    }

    if ((4 * ct->n_in_table < ct->table_size) && ct->table_size > 4) {
        cachetable_rehash(ct, ct->table_size/2);
    }
exit:
    return r;
}

void toku_cachetable_maybe_flush_some(CACHETABLE ct) {
    cachetable_lock(ct);
    maybe_flush_some(ct, 0);
    cachetable_unlock(ct);
}

static PAIR cachetable_insert_at(CACHETABLE ct, 
                                 CACHEFILE cachefile, CACHEKEY key, void *value, 
                                 enum ctpair_state state,
                                 u_int32_t fullhash, 
                                 PAIR_ATTR attr,
                                 CACHETABLE_FLUSH_CALLBACK flush_callback,
                                 CACHETABLE_PARTIAL_EVICTION_EST_CALLBACK pe_est_callback,
                                 CACHETABLE_PARTIAL_EVICTION_CALLBACK pe_callback, 
                                 CACHETABLE_CLEANER_CALLBACK cleaner_callback,
                                 void *write_extraargs, 
                                 enum cachetable_dirty dirty) {
    PAIR MALLOC(p);
    assert(p);
    memset(p, 0, sizeof *p);
    ctpair_add_ref(p);
    p->cachefile = cachefile;
    p->key = key;
    p->value = value;
    p->fullhash = fullhash;
    p->dirty = dirty;
    p->attr = attr;
    p->state = state;
    p->flush_callback = flush_callback;
    p->pe_callback = pe_callback;
    p->pe_est_callback = pe_est_callback;
    p->cleaner_callback = cleaner_callback;
    p->write_extraargs = write_extraargs;
    p->fullhash = fullhash;
    p->clock_next = p->clock_prev = 0;
    p->remove_me = FALSE;
    nb_mutex_init(&p->nb_mutex);
    p->cq = 0;
    pair_add_to_clock(ct, p);
    toku_list_push(&cachefile->pairs_for_cachefile, &p->next_for_cachefile);
    u_int32_t h = fullhash & (ct->table_size-1);
    p->hash_chain = ct->table[h];
    ct->table[h] = p;
    ct->n_in_table++;
    cachetable_add_pair_attr(ct, attr);
    if (ct->n_in_table > ct->table_size) {
        cachetable_rehash(ct, ct->table_size*2);
    }
    return p;
}

enum { hash_histogram_max = 100 };
static unsigned long long hash_histogram[hash_histogram_max];
void toku_cachetable_print_hash_histogram (void) {
    int i;
    for (i=0; i<hash_histogram_max; i++)
	if (hash_histogram[i]) printf("%d:%llu ", i, hash_histogram[i]);
    printf("\n");
    printf("miss=%"PRIu64" hit=%"PRIu64" wait_reading=%"PRIu64" wait=%"PRIu64"\n", 
           cachetable_miss, cachetable_hit, cachetable_wait_reading, cachetable_wait_writing);
}

static void
note_hash_count (int count) {
    if (count>=hash_histogram_max) count=hash_histogram_max-1;
    hash_histogram[count]++;
}

// has ct locked on entry
// This function MUST NOT release and reacquire the cachetable lock
// Its callers (toku_cachetable_put_with_dep_pairs) depend on this behavior.
static int cachetable_put_internal(
    CACHEFILE cachefile, 
    CACHEKEY key, 
    u_int32_t fullhash, 
    void*value, 
    PAIR_ATTR attr,
    CACHETABLE_FLUSH_CALLBACK flush_callback,
    CACHETABLE_PARTIAL_EVICTION_EST_CALLBACK pe_est_callback,
    CACHETABLE_PARTIAL_EVICTION_CALLBACK pe_callback, 
    CACHETABLE_CLEANER_CALLBACK cleaner_callback,
    void *write_extraargs
    )
{
    CACHETABLE ct = cachefile->cachetable;
    int count=0;
    {
        PAIR p;
        for (p=ct->table[fullhash&(cachefile->cachetable->table_size-1)]; p; p=p->hash_chain) {
            count++;
            if (p->key.b==key.b && p->cachefile==cachefile) {
                // Semantically, these two asserts are not strictly right.  After all, when are two functions eq?
                // In practice, the functions better be the same.
                assert(p->flush_callback==flush_callback);
                assert(p->pe_callback==pe_callback);
                assert(p->cleaner_callback==cleaner_callback);
                return -1; /* Already present, don't grab lock. */
            }
        }
    }
    int r;
    if ((r=maybe_flush_some(ct, attr.size))) {
        return r;
    }
    // flushing could change the table size, but wont' change the fullhash
    cachetable_puts++;
    PAIR p = cachetable_insert_at(
        ct, 
        cachefile, 
        key, 
        value, 
        CTPAIR_IDLE, 
        fullhash, 
        attr, 
        flush_callback,
        pe_est_callback,
        pe_callback, 
        cleaner_callback,
        write_extraargs, 
        CACHETABLE_DIRTY
        );
    assert(p);
    nb_mutex_write_lock(&p->nb_mutex, ct->mutex);
    note_hash_count(count);
    return 0;
}

// ct is locked on entry
// gets pair if exists, and that is all.
static int cachetable_get_pair (CACHEFILE cachefile, CACHEKEY key, u_int32_t fullhash, PAIR* pv) {
    CACHETABLE ct = cachefile->cachetable;
    PAIR p;
    int count = 0;
    int r = -1;
    for (p=ct->table[fullhash&(ct->table_size-1)]; p; p=p->hash_chain) {
        count++;
        if (p->key.b==key.b && p->cachefile==cachefile) {
            *pv = p;
            r = 0;
            break;
        }
    }
    note_hash_count(count);
    return r;
}

static void
write_locked_pair_for_checkpoint(CACHETABLE ct, PAIR p)
{
    //
    // this function is called by toku_cachetable_get_and_pin to write locked nodes
    // out for checkpoint. get_and_pin assumes that there is no
    // completion queue, so we assert it here.
    //
    assert(!p->cq);
    if (p->dirty && p->checkpoint_pending) {
        // this is essentially a flush_and_maybe_remove except that
        // we already have p->nb_mutex and we just do the write in our own thread.
        p->state = CTPAIR_WRITING;
        cachetable_write_locked_pair(ct, p); // keeps the PAIR's write lock
    }
    else {
        //
        // we may clear the pending bit here because we have
        // both the cachetable lock and the PAIR lock.
        // The rule, as mentioned in  toku_cachetable_begin_checkpoint, 
        // is that to clear the bit, we must have both the PAIR lock
        // and the pending lock
        //
        p->checkpoint_pending = FALSE;
    }
}

//
// For each PAIR associated with these CACHEFILEs and CACHEKEYs
// if the checkpoint_pending bit is set and the PAIR is dirty, write the PAIR
// to disk.
// We assume the PAIRs passed in have been locked by the client that made calls
// into the cachetable that eventually make it here.
//
static void checkpoint_dependent_pairs(
    CACHETABLE ct,
    u_int32_t num_dependent_pairs, // number of dependent pairs that we may need to checkpoint
    CACHEFILE* dependent_cfs, // array of cachefiles of dependent pairs
    CACHEKEY* dependent_keys, // array of cachekeys of dependent pairs
    u_int32_t* dependent_fullhash, //array of fullhashes of dependent pairs
    enum cachetable_dirty* dependent_dirty // array stating dirty/cleanness of dependent pairs
    )
{
     for (u_int32_t i =0; i < num_dependent_pairs; i++) {
         PAIR curr_dep_pair = NULL;
         int r = cachetable_get_pair(
             dependent_cfs[i],
             dependent_keys[i],
             dependent_fullhash[i],
             &curr_dep_pair
             );
         // if we are passing in a dependent pair that we
         // claim is locked, then it better be here
         assert(r == 0);
         assert(curr_dep_pair != NULL);
         // pair had better be locked, as we are assuming
         // to own the write lock
         assert(nb_mutex_writers(&curr_dep_pair->nb_mutex));
         // we need to update the dirtyness of the dependent pair,
         // because the client may have dirtied it while holding its lock,
         // and if the pair is pending a checkpoint, it needs to be written out
         if (dependent_dirty[i]) curr_dep_pair->dirty = CACHETABLE_DIRTY;
         if (curr_dep_pair->checkpoint_pending) {
             write_locked_pair_for_checkpoint(ct, curr_dep_pair);
         }
     }
}

void toku_checkpoint_pairs(
    CACHEFILE cf,
    u_int32_t num_dependent_pairs, // number of dependent pairs that we may need to checkpoint
    CACHEFILE* dependent_cfs, // array of cachefiles of dependent pairs
    CACHEKEY* dependent_keys, // array of cachekeys of dependent pairs
    u_int32_t* dependent_fullhash, //array of fullhashes of dependent pairs
    enum cachetable_dirty* dependent_dirty // array stating dirty/cleanness of dependent pairs
    )
{
    checkpoint_dependent_pairs(
        cf->cachetable,
        num_dependent_pairs,
        dependent_cfs,
        dependent_keys,
        dependent_fullhash,
        dependent_dirty
        );
}

int toku_cachetable_put_with_dep_pairs(
    CACHEFILE cachefile, 
    CACHETABLE_GET_KEY_AND_FULLHASH get_key_and_fullhash,
    void*value, 
    PAIR_ATTR attr,
    CACHETABLE_FLUSH_CALLBACK flush_callback,
    CACHETABLE_PARTIAL_EVICTION_EST_CALLBACK pe_est_callback,
    CACHETABLE_PARTIAL_EVICTION_CALLBACK pe_callback, 
    CACHETABLE_CLEANER_CALLBACK cleaner_callback,
    void *write_extraargs,
    void *get_key_and_fullhash_extra,
    u_int32_t num_dependent_pairs, // number of dependent pairs that we may need to checkpoint
    CACHEFILE* dependent_cfs, // array of cachefiles of dependent pairs
    CACHEKEY* dependent_keys, // array of cachekeys of dependent pairs
    u_int32_t* dependent_fullhash, //array of fullhashes of dependent pairs
    enum cachetable_dirty* dependent_dirty, // array stating dirty/cleanness of dependent pairs
    CACHEKEY* key,
    u_int32_t* fullhash
    )
{
    //
    // need to get the key and filehash
    //
    CACHETABLE ct = cachefile->cachetable;
    cachetable_lock(ct);
    //
    // The reason we call cachetable_wait_write outside 
    // of cachetable_put_internal is that we want the operations
    // get_key_and_fullhash and cachetable_put_internal
    // to be atomic and NOT release the cachetable lock.
    // If the cachetable lock is released within cachetable_put_internal,
    // we may end up with a checkpoint beginning that has 
    // called get_key_and_fullhash (which causes a blocknum
    // to be allocated) but without the PAIR being in the cachetable
    // and checkpointed. The checkpoint would have a leaked blocknum.
    // So, we call cachetable_wait_write outside, and ensure that 
    // cachetable_put_internal does not release the cachetable lock
    //
    cachetable_wait_write(ct);
    get_key_and_fullhash(key, fullhash, get_key_and_fullhash_extra);
    int r = cachetable_put_internal(
        cachefile,
        *key,
        *fullhash,
        value,
        attr,
        flush_callback,
        pe_est_callback,
        pe_callback,
        cleaner_callback,
        write_extraargs
        );

    //
    // now that we have inserted the row, let's checkpoint the 
    // dependent nodes, if they need checkpointing
    //
    checkpoint_dependent_pairs(
        ct,
        num_dependent_pairs,
        dependent_cfs,
        dependent_keys,
        dependent_fullhash,
        dependent_dirty
        );

    cachetable_unlock(ct);
    return r;
}


int toku_cachetable_put(CACHEFILE cachefile, CACHEKEY key, u_int32_t fullhash, void*value, PAIR_ATTR attr,
			CACHETABLE_FLUSH_CALLBACK flush_callback,
			CACHETABLE_PARTIAL_EVICTION_EST_CALLBACK pe_est_callback,
                        CACHETABLE_PARTIAL_EVICTION_CALLBACK pe_callback,
                        CACHETABLE_CLEANER_CALLBACK cleaner_callback,
                        void *write_extraargs) {
    WHEN_TRACE_CT(printf("%s:%d CT cachetable_put(%lld)=%p\n", __FILE__, __LINE__, key, value));
    CACHETABLE ct = cachefile->cachetable;
    cachetable_lock(ct);
    cachetable_wait_write(ct);
    int r = cachetable_put_internal(
        cachefile,
        key,
        fullhash,
        value,
        attr,
        flush_callback,
        pe_est_callback,
        pe_callback,
        cleaner_callback,
        write_extraargs
        );
    cachetable_unlock(ct);
    return r;
}

static uint64_t get_tnow(void) {
    struct timeval tv;
    int r = gettimeofday(&tv, NULL); assert(r == 0);
    return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

// for debug 
static PAIR write_for_checkpoint_pair = NULL;


// On entry: hold the ct lock
// On exit:  the node is written out
// Method:   take write lock
//           maybe write out the node
//           if p->cq, put on completion queue.  Else release write lock
static void
write_pair_for_checkpoint (CACHETABLE ct, PAIR p)
{
    write_for_checkpoint_pair = p;
    nb_mutex_write_lock(&p->nb_mutex, ct->mutex); // grab an exclusive lock on the pair
    if (p->dirty && p->checkpoint_pending) {
        // this is essentially a flush_and_maybe_remove except that
        // we already have p->nb_mutex and we just do the write in our own thread.
        p->state = CTPAIR_WRITING;
        workitem_init(&p->asyncwork, NULL, p);
        cachetable_write_pair(ct, p, FALSE);    // releases the write lock on the pair
    }
    else {
        //
        // we may clear the pending bit here because we have
        // both the cachetable lock and the PAIR lock.
        // The rule, as mentioned in  toku_cachetable_begin_checkpoint, 
        // is that to clear the bit, we must have both the PAIR lock
        // and the pending lock
        //
        p->checkpoint_pending = FALSE;
        if (p->cq) {
            workitem_init(&p->asyncwork, NULL, p);
            workqueue_enq(p->cq, &p->asyncwork, 1);
        }
        else {
            nb_mutex_write_unlock(&p->nb_mutex); // didn't call cachetable_write_pair so we have to unlock it ourselves.
        }
    }
    write_for_checkpoint_pair = NULL;
}

//
// cachetable lock and PAIR lock are held on entry
// On exit, cachetable lock is still held, but PAIR lock
// is either released or ownership of PAIR lock is transferred
// via the completion queue.
//
static void
do_partial_fetch(
    CACHETABLE ct, 
    CACHEFILE cachefile, 
    PAIR p, 
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback, 
    void *read_extraargs,
    BOOL keep_pair_locked
    )
{
    PAIR_ATTR old_attr = p->attr;
    PAIR_ATTR new_attr = zero_attr;
    // As of Dr. No, only clean PAIRs may have pieces missing,
    // so we do a sanity check here.
    assert(!p->dirty);
    p->state = CTPAIR_READING;

    rwlock_prefer_read_lock(&cachefile->fdlock, ct->mutex);
    cachetable_unlock(ct);
    int r = pf_callback(p->value, read_extraargs, cachefile->fd, &new_attr);
    lazy_assert_zero(r);
    cachetable_lock(ct);
    rwlock_read_unlock(&cachefile->fdlock);
    p->attr = new_attr;
    cachetable_change_pair_attr(ct, old_attr, new_attr);
    p->state = CTPAIR_IDLE;
    if (keep_pair_locked) {
        // if the caller wants the pair to remain locked
        // that means the caller requests continued
        // ownership of the PAIR, so there better not
        // be a cq asking to transfer ownership
        assert(!p->cq);
    }
    else {
        if (p->cq) {
            workitem_init(&p->asyncwork, NULL, p);
            workqueue_enq(p->cq, &p->asyncwork, 1);
        }
        else {
            nb_mutex_write_unlock(&p->nb_mutex);
        }
    }
}


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
    CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK pf_req_callback,
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback,
    CACHETABLE_CLEANER_CALLBACK cleaner_callback,
    void* read_extraargs,
    void* write_extraargs
    ) 
{
    return toku_cachetable_get_and_pin_with_dep_pairs (
        cachefile, 
        key, 
        fullhash, 
        value, 
        sizep,
        flush_callback, 
        fetch_callback, 
        pe_est_callback,
        pe_callback, 
        pf_req_callback,
        pf_callback,
        cleaner_callback,
        read_extraargs,
        write_extraargs,
        0, // number of dependent pairs that we may need to checkpoint
        NULL, // array of cachefiles of dependent pairs
        NULL, // array of cachekeys of dependent pairs
        NULL, //array of fullhashes of dependent pairs
        NULL // array stating dirty/cleanness of dependent pairs
        );
}


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
    void* read_extraargs,
    void* write_extraargs,
    u_int32_t num_dependent_pairs, // number of dependent pairs that we may need to checkpoint
    CACHEFILE* dependent_cfs, // array of cachefiles of dependent pairs
    CACHEKEY* dependent_keys, // array of cachekeys of dependent pairs
    u_int32_t* dependent_fullhash, //array of fullhashes of dependent pairs
    enum cachetable_dirty* dependent_dirty // array stating dirty/cleanness of dependent pairs
    ) 
{
    CACHETABLE ct = cachefile->cachetable;
    PAIR p;
    int count=0;

    cachetable_lock(ct);
    
    cachetable_wait_write(ct);
    for (p=ct->table[fullhash&(ct->table_size-1)]; p; p=p->hash_chain) {
        count++;
        if (p->key.b==key.b && p->cachefile==cachefile) {
            // still have the cachetable lock
            //
            // at this point, we know the node is at least partially in memory,
            // but we do not know if the user requires a partial fetch (because
            // some basement node is missing or some message buffer needs
            // to be decompressed. So, we check to see if a partial fetch is required
            //
            nb_mutex_write_lock(&p->nb_mutex, ct->mutex);
            BOOL partial_fetch_required = pf_req_callback(p->value,read_extraargs);
            //
            // Just because the PAIR exists does necessarily mean the all the data the caller requires
            // is in memory. A partial fetch may be required, which is evaluated above
            // if the variable is true, a partial fetch is required so we must grab the PAIR's write lock
            // and then call a callback to retrieve what we need
            //
            if (partial_fetch_required) {
                // As of Dr. No, only clean PAIRs may have pieces missing,
                // so we do a sanity check here.
                assert(!p->dirty);
                p->state = CTPAIR_READING;

                do_partial_fetch(ct, cachefile, p, pf_callback, read_extraargs, TRUE);
            }

            pair_touch(p);
            cachetable_hit++;
            note_hash_count(count);
            WHEN_TRACE_CT(printf("%s:%d cachtable_get_and_pin(%lld)--> %p\n", __FILE__, __LINE__, key, *value));
            goto got_value;
	}
    }
    note_hash_count(count);
    int r;
    // Note.  hashit(t,key) may have changed as a result of flushing.  But fullhash won't have changed.
    {
	p = cachetable_insert_at(
            ct, 
            cachefile, 
            key, 
            zero_value, 
            CTPAIR_READING, 
            fullhash, 
            zero_attr, 
            flush_callback, 
            pe_est_callback,
            pe_callback, 
            cleaner_callback,
            write_extraargs, 
            CACHETABLE_CLEAN
            );
        assert(p);
        nb_mutex_write_lock(&p->nb_mutex, ct->mutex);
	uint64_t t0 = get_tnow();

        cachetable_fetch_pair(ct, cachefile, p, fetch_callback, read_extraargs);
        cachetable_miss++;
        cachetable_misstime += get_tnow() - t0;
        nb_mutex_write_lock(&p->nb_mutex, ct->mutex);
        goto got_value;
    }
got_value:
    *value = p->value;
    if (sizep) *sizep = p->attr.size;

    //
    // Now that we have all of the locks on the pairs we 
    // care about, we can take care of the necessary checkpointing.
    // For each pair, we simply need to do is write the pair if it is 
    // pending a checkpoint. If no pair is pending a checkpoint,
    // then all of this work will be done with the cachetable lock held,
    // so we don't need to worry about a checkpoint beginning 
    // in the middle of any operation below. If some pair
    // is pending a checkpoint, then the checkpoint thread
    // will not complete its current checkpoint until it can
    // successfully grab a lock on the pending pair and 
    // remove it from its list of pairs pending a checkpoint.
    // This cannot be done until we release the lock
    // that we have, which is not done in this function.
    // So, the point is, it is impossible for a checkpoint
    // to begin while we write any of these locked pairs
    // for checkpoint, even though writing a pair releases
    // the cachetable lock.
    //
    if (p->checkpoint_pending) {
        write_locked_pair_for_checkpoint(ct, p);
    }
    
    checkpoint_dependent_pairs(
        ct,
        num_dependent_pairs,
        dependent_cfs,
        dependent_keys,
        dependent_fullhash,
        dependent_dirty
        );

    r = maybe_flush_some(ct, 0);
    cachetable_unlock(ct);
    WHEN_TRACE_CT(printf("%s:%d did fetch: cachtable_get_and_pin(%lld)--> %p\n", __FILE__, __LINE__, key, *value));
    return r;
}

// Lookup a key in the cachetable.  If it is found and it is not being written, then
// acquire a read lock on the pair, update the LRU list, and return sucess.
//
// However, if the page is clean or has checkpoint pending, don't return success.
// This will minimize the number of dirty nodes.
// Rationale:  maybe_get_and_pin is used when the system has an alternative to modifying a node.
//  In the context of checkpointing, we don't want to gratuituously dirty a page, because it causes an I/O.
//  For example, imagine that we can modify a bit in a dirty parent, or modify a bit in a clean child, then we should modify
//  the dirty parent (which will have to do I/O eventually anyway) rather than incur a full block write to modify one bit.
//  Similarly, if the checkpoint is actually pending, we don't want to block on it.
int toku_cachetable_maybe_get_and_pin (CACHEFILE cachefile, CACHEKEY key, u_int32_t fullhash, void**value) {
    CACHETABLE ct = cachefile->cachetable;
    PAIR p;
    int count = 0;
    int r = -1;
    cachetable_lock(ct);
    cachetable_maybe_get_and_pins++;
    for (p=ct->table[fullhash&(ct->table_size-1)]; p; p=p->hash_chain) {
	count++;
	if (p->key.b==key.b && p->cachefile==cachefile) {
            if (!p->checkpoint_pending &&  //If checkpoint pending, we would need to first write it, which would make it clean
                p->dirty &&
                nb_mutex_users(&p->nb_mutex) == 0
            ) {
                cachetable_maybe_get_and_pin_hits++;
                // because nb_mutex_users is 0, this is fast
                nb_mutex_write_lock(&p->nb_mutex, ct->mutex);                
                *value = p->value;
                pair_touch(p);
                r = 0;
                //printf("%s:%d cachetable_maybe_get_and_pin(%lld)--> %p\n", __FILE__, __LINE__, key, *value);
            }
            break;
	}
    }
    note_hash_count(count);
    cachetable_unlock(ct);
    return r;
}

//Used by flusher threads to possibly pin child on client thread if pinning is cheap
//Same as toku_cachetable_maybe_get_and_pin except that we don't care if the node is clean or dirty (return the node regardless).
//All other conditions remain the same.
int toku_cachetable_maybe_get_and_pin_clean (CACHEFILE cachefile, CACHEKEY key, u_int32_t fullhash, void**value) {
    CACHETABLE ct = cachefile->cachetable;
    PAIR p;
    int count = 0;
    int r = -1;
    cachetable_lock(ct);
    cachetable_maybe_get_and_pins++;
    for (p=ct->table[fullhash&(ct->table_size-1)]; p; p=p->hash_chain) {
	count++;
	if (p->key.b==key.b && p->cachefile==cachefile) {
            if (!p->checkpoint_pending &&  //If checkpoint pending, we would need to first write it, which would make it clean (if the pin would be used for writes.  If would be used for read-only we could return it, but that would increase complexity)
                nb_mutex_users(&p->nb_mutex) == 0
            ) {
                cachetable_maybe_get_and_pin_hits++;
                // because nb_mutex_users is 0, this is fast
                nb_mutex_write_lock(&p->nb_mutex, ct->mutex);                
                *value = p->value;
                r = 0;
                //printf("%s:%d cachetable_maybe_get_and_pin_clean(%lld)--> %p\n", __FILE__, __LINE__, key, *value);
            }
            break;
	}
    }
    note_hash_count(count);
    cachetable_unlock(ct);
    return r;
}

static int
cachetable_unpin_internal(CACHEFILE cachefile, CACHEKEY key, u_int32_t fullhash, enum cachetable_dirty dirty, PAIR_ATTR attr, BOOL have_ct_lock, BOOL flush)
// size==0 means that the size didn't change.
{
    CACHETABLE ct = cachefile->cachetable;
    WHEN_TRACE_CT(printf("%s:%d unpin(%lld)", __FILE__, __LINE__, key));
    //printf("%s:%d is dirty now=%d\n", __FILE__, __LINE__, dirty);
    int count = 0;
    int r = -1;
    //assert(fullhash == toku_cachetable_hash(cachefile, key));
    if (!have_ct_lock) cachetable_lock(ct);
    for (PAIR p=ct->table[fullhash&(ct->table_size-1)]; p; p=p->hash_chain) {
	count++;
	if (p->key.b==key.b && p->cachefile==cachefile) {
	    assert(nb_mutex_writers(&p->nb_mutex)>0);
            nb_mutex_write_unlock(&p->nb_mutex);
	    if (dirty) p->dirty = CACHETABLE_DIRTY;
            PAIR_ATTR old_attr = p->attr;
            PAIR_ATTR new_attr = attr;
            cachetable_change_pair_attr(ct, old_attr, new_attr);
            p->attr = attr;
	    WHEN_TRACE_CT(printf("[count=%lld]\n", p->pinned));
	    {
                if (flush) {
                    if ((r=maybe_flush_some(ct, 0))) {
                        cachetable_unlock(ct);
                        return r;
                    }
                }
	    }
            r = 0; // we found one
            break;
	}
    }
    note_hash_count(count);
    if (!have_ct_lock) cachetable_unlock(ct);
    return r;
}

int toku_cachetable_unpin(CACHEFILE cachefile, CACHEKEY key, u_int32_t fullhash, enum cachetable_dirty dirty, PAIR_ATTR attr) {
    // By default we don't have the lock
    return cachetable_unpin_internal(cachefile, key, fullhash, dirty, attr, FALSE, TRUE);
}
int toku_cachetable_unpin_ct_prelocked_no_flush(CACHEFILE cachefile, CACHEKEY key, u_int32_t fullhash, enum cachetable_dirty dirty, PAIR_ATTR attr) {
    // We hold the cachetable mutex.
    return cachetable_unpin_internal(cachefile, key, fullhash, dirty, attr, TRUE, FALSE);
}

void toku_cachetable_prelock(CACHEFILE cf) {
    cachetable_lock(cf->cachetable);
}

void toku_cachetable_unlock(CACHEFILE cf) {
    cachetable_unlock(cf->cachetable);
}
static void
run_unlockers (UNLOCKERS unlockers) {
    while (unlockers) {
	assert(unlockers->locked);
	unlockers->locked = FALSE;
	unlockers->f(unlockers->extra);
	unlockers=unlockers->next;
    }
}

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
    CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK pf_req_callback,
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback,
    CACHETABLE_CLEANER_CALLBACK cleaner_callback,
    void *read_extraargs,
    void* write_extraargs,
    UNLOCKERS unlockers
    )
// Effect:  If the block is in the cachetable, then pin it and return it. 
//   Otherwise call the lock_unlock_callback (to unlock), fetch the data (but don't pin it, since we'll just end up pinning it again later),  and the call (to lock)
//   and return TOKUDB_TRY_AGAIN.
{
    CACHETABLE ct = cf->cachetable;
    cachetable_lock(ct);
    int count = 0;
    PAIR p;
    for (p = ct->table[fullhash&(ct->table_size-1)]; p; p = p->hash_chain) {
	count++;
	if (p->key.b==key.b && p->cachefile==cf) {
	    note_hash_count(count);

            //
            // In Dr. No, the ydb lock ensures that only one client may be successfully
            // doing a query on a dictionary table at any given time. This function
            // is called with the ydb lock held. So, if there is a write lock grabbed
            // on the PAIR that we want to lock, then some expensive operation 
            // MUST be happening (read from disk, write to disk, flush, etc...), 
            // and we should run the unlockers and release the ydb lock
            // Otherwise, if there is no write lock grabbed, we know there will 
            // be no stall, so we grab the lock and return to the user
            //
            if (!nb_mutex_writers(&p->nb_mutex) && !p->checkpoint_pending) {
                cachetable_hit++;
                nb_mutex_write_lock(&p->nb_mutex, ct->mutex);
                BOOL partial_fetch_required = pf_req_callback(p->value,read_extraargs);
                //
                // Just because the PAIR exists does necessarily mean the all the data the caller requires
                // is in memory. A partial fetch may be required, which is evaluated above
                // if the variable is true, a partial fetch is required so we must grab the PAIR's write lock
                // and then call a callback to retrieve what we need
                //
                if (partial_fetch_required) {
                    //
                    // The reason we have this assert is a sanity check
                    // to make sure that it is ok to set the 
                    // state of the pair to CTPAIR_READING.
                    // 
                    // As of this writing, the checkpoint code assumes
                    // that every pair that is in the CTPAIR_READING state
                    // is not dirty. Because we require dirty nodes to be
                    // fully in memory, we should never have a dirty node 
                    // require a partial fetch. So, just to be sure that 
                    // we can set the pair to CTPAIR_READING, we assert
                    // that the pair is not dirty
                    //
                    p->state = CTPAIR_READING;
                    run_unlockers(unlockers); // The contract says the unlockers are run with the ct lock being held.
                    if (ct->ydb_unlock_callback) ct->ydb_unlock_callback();
                    // Now wait for the I/O to occur.
    
                    do_partial_fetch(ct, cf, p, pf_callback, read_extraargs, FALSE);
    
                    cachetable_unlock(ct);
                    if (ct->ydb_lock_callback) ct->ydb_lock_callback();
                    return TOKUDB_TRY_AGAIN;
                }
                pair_touch(p);
                *value = p->value;
                if (sizep) *sizep = p->attr.size;
                // for ticket #3755
                assert(!p->checkpoint_pending);
                cachetable_unlock(ct);
                return 0;
            }
            else {
                run_unlockers(unlockers); // The contract says the unlockers are run with the ct lock being held.
                if (ct->ydb_unlock_callback) ct->ydb_unlock_callback();
                // Now wait for the I/O to occur.
                // We need to obtain the lock (waiting for the write to finish), but then we only waited so we could wake up again
                if (p->checkpoint_pending) {
                    // an optimization we can later do is if 
                    // we can grab the write lock on the pair and 
                    // it is clean, then dont run the unlockers, simply 
                    // clear the pending bit and return the PAIR to the user
                    // but this is simpler.
                    write_pair_for_checkpoint(ct, p);
                }
                else {
                    nb_mutex_write_lock(&p->nb_mutex, ct->mutex);
                    nb_mutex_write_unlock(&p->nb_mutex);
                }
                cachetable_unlock(ct);
                if (ct->ydb_lock_callback) ct->ydb_lock_callback();
                return TOKUDB_TRY_AGAIN;
            }
        }
    }
    assert(p==0);

    // Not found
    p = cachetable_insert_at(
        ct, 
        cf, 
        key, 
        zero_value, 
        CTPAIR_READING, 
        fullhash, 
        zero_attr, 
        flush_callback, 
        pe_est_callback, 
        pe_callback, 
        cleaner_callback, 
        write_extraargs, 
        CACHETABLE_CLEAN
        );
    assert(p);
    nb_mutex_write_lock(&p->nb_mutex, ct->mutex);
    run_unlockers(unlockers); // we hold the ct mutex.
    if (ct->ydb_unlock_callback) ct->ydb_unlock_callback();
    u_int64_t t0 = get_tnow();
    cachetable_fetch_pair(ct, cf, p, fetch_callback, read_extraargs);
    cachetable_miss++;
    cachetable_misstime += get_tnow() - t0;
    cachetable_unlock(ct);
    if (ct->ydb_lock_callback) ct->ydb_lock_callback();
    return TOKUDB_TRY_AGAIN;
}

struct cachefile_prefetch_args {
    PAIR p;
    CACHETABLE_FETCH_CALLBACK fetch_callback;
    void* read_extraargs;
};

struct cachefile_partial_prefetch_args {
    PAIR p;
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback;
    void *read_extraargs;
};

int toku_cachefile_prefetch(CACHEFILE cf, CACHEKEY key, u_int32_t fullhash,
                            CACHETABLE_FLUSH_CALLBACK flush_callback,
                            CACHETABLE_FETCH_CALLBACK fetch_callback,
                            CACHETABLE_PARTIAL_EVICTION_EST_CALLBACK pe_est_callback,
                            CACHETABLE_PARTIAL_EVICTION_CALLBACK pe_callback,
                            CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK pf_req_callback,
                            CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback,
                            CACHETABLE_CLEANER_CALLBACK cleaner_callback,
                            void *read_extraargs,
                            void *write_extraargs,
                            BOOL *doing_prefetch)
// Effect: See the documentation for this function in cachetable.h
{
    // TODO: Fix prefetching, as part of ticket 3635
    // Here is the cachetable's reason why we are not doing prefetching in Maxwell.
    // The fetch_callback requires data that is only valid in the caller's thread,
    // namely, a struct that the caller allocates that contains information
    // on what pieces of the node will be needed. This data is not necessarily 
    // valid when the prefetch thread gets around to trying to prefetch the node
    // If we pass this data to another thread, we need a mechanism for freeing it.
    // It may be another callback. That is way too many callbacks that are being used
    // Fixing this in a clean, simple way requires some thought.
    if (0) printf("%s:%d %"PRId64"\n", __FUNCTION__, __LINE__, key.b);
    if (doing_prefetch) {
        *doing_prefetch = FALSE;
    }
    CACHETABLE ct = cf->cachetable;
    cachetable_lock(ct);
    // lookup
    PAIR p;
    for (p = ct->table[fullhash&(ct->table_size-1)]; p; p = p->hash_chain) {
        if (p->key.b==key.b && p->cachefile==cf) {
            //Maybe check for pending and do write_pair_for_checkpoint()?
            pair_touch(p);
            break;
        }
    }

    // if not found then create a pair in the READING state and fetch it
    if (p == 0) {
        cachetable_prefetches++;
        p = cachetable_insert_at(
            ct, 
            cf, 
            key, 
            zero_value, 
            CTPAIR_READING, 
            fullhash, 
            zero_attr, 
            flush_callback, 
            pe_est_callback, 
            pe_callback, 
            cleaner_callback, 
            write_extraargs, 
            CACHETABLE_CLEAN
            );
        assert(p);
        nb_mutex_write_lock(&p->nb_mutex, ct->mutex);
        struct cachefile_prefetch_args *MALLOC(cpargs);
        cpargs->p = p;
        cpargs->fetch_callback = fetch_callback;
        cpargs->read_extraargs = read_extraargs;
        workitem_init(&p->asyncwork, cachetable_reader, cpargs);
        workqueue_enq(&ct->wq, &p->asyncwork, 0);
        if (doing_prefetch) {
            *doing_prefetch = TRUE;
        }
    } else if (nb_mutex_users(&p->nb_mutex)==0) {
        // nobody else is using the node, so we should go ahead and prefetch
        nb_mutex_write_lock(&p->nb_mutex, ct->mutex);
        BOOL partial_fetch_required = pf_req_callback(p->value, read_extraargs);

        if (partial_fetch_required) {
            p->state = CTPAIR_READING;
            struct cachefile_partial_prefetch_args *MALLOC(cpargs);
            cpargs->p = p;
            cpargs->pf_callback = pf_callback;
            cpargs->read_extraargs = read_extraargs;
            workitem_init(&p->asyncwork, cachetable_partial_reader, cpargs);
            workqueue_enq(&ct->wq, &p->asyncwork, 0);
            if (doing_prefetch) {
                *doing_prefetch = TRUE;
            }
        }
	else {
            nb_mutex_write_unlock(&p->nb_mutex);
	}
    }
    cachetable_unlock(ct);
    return 0;
}

// effect:   Move an object from one key to another key.
// requires: The object is pinned in the table
int toku_cachetable_rename (CACHEFILE cachefile, CACHEKEY oldkey, CACHEKEY newkey) {
    CACHETABLE ct = cachefile->cachetable;
    PAIR *ptr_to_p,p;
    int count = 0;
    u_int32_t fullhash = toku_cachetable_hash(cachefile, oldkey);
    cachetable_lock(ct);
    for (ptr_to_p = &ct->table[fullhash&(ct->table_size-1)],  p = *ptr_to_p;
         p;
         ptr_to_p = &p->hash_chain,                p = *ptr_to_p) {
        count++;
        if (p->key.b==oldkey.b && p->cachefile==cachefile) {
            note_hash_count(count);
            *ptr_to_p = p->hash_chain;
            p->key = newkey;
            u_int32_t new_fullhash = toku_cachetable_hash(cachefile, newkey);
            u_int32_t nh = new_fullhash&(ct->table_size-1);
            p->fullhash = new_fullhash;
            p->hash_chain = ct->table[nh];
            ct->table[nh] = p;
            cachetable_unlock(ct);
            return 0;
        }
    }
    note_hash_count(count);
    cachetable_unlock(ct);
    return -1;
}

void toku_cachefile_verify (CACHEFILE cf) {
    toku_cachetable_verify(cf->cachetable);
}

// for gdb
int64_t UU() toku_cachetable_size_slowslow (CACHETABLE ct) {
    // DANGER DANGER DANGER
    // This only works if every entry in the cachetable is actually a
    // BRTNODE.  Don't say you weren't warned.
    PAIR p;
    BOOL is_first = TRUE;
    int64_t ret = 0;
    for (p=ct->clock_head; ct->clock_head!=NULL && (p!=ct->clock_head || is_first); p=p->clock_next) {
        is_first=FALSE;
        ret += brtnode_memory_size((BRTNODE) p->value);
    }
    return ret;
}

int64_t UU() toku_cachetable_size_discrepancy (CACHETABLE ct) {
    // DANGER DANGER DANGER
    // This only works if every entry in the cachetable is actually a
    // BRTNODE.  Don't say you weren't warned.
    PAIR p;
    BOOL is_first = TRUE;
    int64_t ret = 0;
    for (p=ct->clock_head; ct->clock_head!=NULL && (p!=ct->clock_head || is_first); p=p->clock_next) {
        is_first=FALSE;
        ret += brtnode_memory_size((BRTNODE) p->value) - p->attr.size;
    }
    return ret;
}

int64_t UU() toku_cachetable_size_discrepancy_pinned (CACHETABLE ct) {
    // DANGER DANGER DANGER
    // This only works if every entry in the cachetable is actually a
    // BRTNODE.  Don't say you weren't warned.
    PAIR p;
    BOOL is_first = TRUE;
    int64_t ret = 0;
    for (p=ct->clock_head; ct->clock_head!=NULL && (p!=ct->clock_head || is_first); p=p->clock_next) {
        is_first=FALSE;
        if (nb_mutex_writers(&p->nb_mutex)) {
            ret += brtnode_memory_size((BRTNODE) p->value) - p->attr.size;
        }
    }
    return ret;
}

int64_t UU() toku_cachetable_size_slow (CACHETABLE ct) {
    PAIR p;
    BOOL is_first = TRUE;
    int64_t ret = 0;
    for (p=ct->clock_head; ct->clock_head!=NULL && (p!=ct->clock_head || is_first); p=p->clock_next) {
        is_first=FALSE;
        ret += p->attr.size;
    }
    return ret;
}

void toku_cachetable_verify (CACHETABLE ct) {
    cachetable_lock(ct);

    // First clear all the verify flags by going through the hash chains
    {
	u_int32_t i;
	for (i=0; i<ct->table_size; i++) {
	    PAIR p;
	    for (p=ct->table[i]; p; p=p->hash_chain) {
		p->verify_flag=0;
	    }
	}
    }
    // Now go through the clock chain, make sure everything in the LRU chain is hashed, and set the verify flag.
    {
	PAIR p;
        BOOL is_first = TRUE;
	for (p=ct->clock_head; ct->clock_head!=NULL && (p!=ct->clock_head || is_first); p=p->clock_next) {
            is_first=FALSE;
	    assert(p->verify_flag==0);
	    PAIR p2;
	    u_int32_t fullhash = p->fullhash;
	    //assert(fullhash==toku_cachetable_hash(p->cachefile, p->key));
	    for (p2=ct->table[fullhash&(ct->table_size-1)]; p2; p2=p2->hash_chain) {
		if (p2==p) {
		    /* found it */
		    goto next;
		}
	    }
	    fprintf(stderr, "Something in the clock chain is not hashed\n");
	    assert(0);
	next:
	    p->verify_flag = 1;
	}
    }
    // Now make sure everything in the hash chains has the verify_flag set to 1.
    {
	u_int32_t i;
	for (i=0; i<ct->table_size; i++) {
	    PAIR p;
	    for (p=ct->table[i]; p; p=p->hash_chain) {
		assert(p->verify_flag);
	    }
	}
    }

    cachetable_unlock(ct);
}

static void assert_cachefile_is_flushed_and_removed (CACHETABLE ct, CACHEFILE cf) {
    u_int32_t i;
    // Check it two ways
    // First way: Look through all the hash chains
    for (i=0; i<ct->table_size; i++) {
        PAIR p;
        for (p=ct->table[i]; p; p=p->hash_chain) {
            assert(p->cachefile!=cf);
        }
    }
    // Second way: Look through the LRU list.
    {
        PAIR p;
        for (p=ct->clock_head; p; p=p->clock_next) {
            assert(p->cachefile!=cf);
        }
    }
}

// Flush (write to disk) all of the pairs that belong to a cachefile (or all pairs if 
// the cachefile is NULL.
// Must be holding cachetable lock on entry.
// 
// This function assumes that no client thread is accessing or 
// trying to access the cachefile while this function is executing.
// This implies no client thread will be trying to lock any nodes
// belonging to the cachefile.
static int cachetable_flush_cachefile(CACHETABLE ct, CACHEFILE cf) {
    unsigned nfound = 0;
    //
    // Because work on a kibbutz is always done by the client thread,
    // and this function assumes that no client thread is doing any work
    // on the cachefile, we assume that no client thread will be adding jobs
    // to this cachefile's kibbutz.
    //
    // The caller of this function must ensure that there are 
    // no jobs added to the kibbutz. This implies that the only work other 
    // threads may be doing is work by the writer threads.
    //
    // Additionally, the cachetable lock is held on entry to this
    // function, so the cleaner thread cannot start any new work either.
    //
    // No other threads (other than kibbutzim and the cleaner thread) do
    // background work we care about as the system is today.
    //
    if (cf) {
        assert(cf->n_background_jobs == 0);
    }
    struct workqueue cq;
    workqueue_init(&cq);

    // find all of the pairs owned by a cachefile and redirect their completion
    // to a completion queue.  If an unlocked PAIR is dirty, flush and remove 
    // the PAIR. Locked PAIRs are on either a reader/writer thread (or maybe a
    // checkpoint thread?) and therefore will be placed on the completion queue.
    // The assumptions above lead to this reasoning. All pairs belonging to
    // this cachefile are either:
    //  - unlocked
    //  - locked and on a writer thread (or possibly on a checkpoint thread?).
    // We find all the pairs owned by the cachefile and do the following:
    //  - if the PAIR is clean and unlocked, then remove the PAIR
    //  - if the PAIR is dirty and unlocked, write the PAIR to disk on a writer thread
    //  - then, wait on all pairs that are on writer threads (includes pairs we just
    //    placed on the writer thread along with pairs that were on the writer thread
    //    when the function started).
    //  - Once the writer thread is done with a PAIR, remove it
    //
    // A question from Zardosht: Is it possible for a checkpoint thread
    // to be running, and also trying to get access to a PAIR while 
    // the PAIR is on the writer thread? Will this cause problems?
    // This question is encapsulated in #3941
    //
    unsigned i;

    unsigned num_pairs = 0;
    unsigned list_size = 256;
    PAIR *list = NULL;
    XMALLOC_N(list_size, list);
    //It is not safe to loop through the table (and hash chains) if you can
    //release the cachetable lock at any point within.

    //Make a list of pairs that belong to this cachefile.
    //Add a reference to them.
    if (cf == NULL) {
        for (i=0; i < ct->table_size; i++) {
            PAIR p;
            for (p = ct->table[i]; p; p = p->hash_chain) {
                if (cf == 0 || p->cachefile==cf) {
                    ctpair_add_ref(p);
                    if (num_pairs == list_size) {
                        list_size *= 2;
                        XREALLOC_N(list_size, list);
                    }
                    list[num_pairs++] = p;
                }
            }
        }
    } else {
        for (struct toku_list *next_pair = cf->pairs_for_cachefile.next; next_pair != &cf->pairs_for_cachefile; next_pair = next_pair->next) {
            PAIR p = toku_list_struct(next_pair, struct ctpair, next_for_cachefile);
            ctpair_add_ref(p);
            if (num_pairs == list_size) {
                list_size *= 2;
                XREALLOC_N(list_size, list);
            }
            list[num_pairs++] = p;
        }
    }
    //Loop through the list.
    //It is safe to access the memory (will not have been freed).
    //If 'already_removed' is set, then we should release our reference
    //and go to the next entry.
    for (i=0; i < num_pairs; i++) {
	PAIR p = list[i];
        if (!p->already_removed) {
            assert(cf == 0 || p->cachefile==cf);
            nfound++;
            p->cq = &cq;
            //
            // Once again, the assumption is that any PAIR
            // is either unlocked or on a writer thread work queue
            //
            if (!nb_mutex_writers(&p->nb_mutex)) {
                flush_and_maybe_remove(ct, p);
            }
        }
        ctpair_destroy(p);     //Release our reference
    }
    toku_free(list);

    // wait for all of the pairs in the work queue to complete
    //
    // An important assumption here is that none of the PAIRs that we
    // pop off  the work queue need to be written out to disk. So, it is
    // safe to simply call cachetable_maybe_remove_and_free_pair on 
    // the PAIRs we find. The reason we can make this assumption
    // is based on the assumption that upon entry of this function,
    // all PAIRs belonging to this cachefile are either idle,
    // being processed by a writer thread, or being processed by a kibbutz. 
    // At this point in the code, kibbutz work is finished and we 
    // assume the client will not add any more kibbutz work for this cachefile.
    // 
    // If it were possible
    // for some thread to change the state of the node before passing
    // it off here, and a write to disk were necessary, then the code
    // below would be wrong.
    //
    for (i=0; i<nfound; i++) {
        cachetable_unlock(ct);
        WORKITEM wi = 0;
        //This workqueue's mutex is NOT the cachetable lock.
        //You must not be holding the cachetable lock during the dequeue.
        int r = workqueue_deq(&cq, &wi, 1); assert(r == 0);
        cachetable_lock(ct);
        PAIR p = workitem_arg(wi);
        p->cq = 0;
        //Some other thread owned the lock, but transferred ownership to the thread executing this function
        nb_mutex_write_unlock(&p->nb_mutex);  //Release the lock, no one has a pin, per our assumptions above.
        BOOL destroyed;
        cachetable_maybe_remove_and_free_pair(ct, p, &destroyed);
    }
    workqueue_destroy(&cq);
    if (cf) {
        assert(toku_list_empty(&cf->pairs_for_cachefile));
        cf->is_flushing = false;
    } else {
        assert_cachefile_is_flushed_and_removed(ct, cf);
    }

    if ((4 * ct->n_in_table < ct->table_size) && (ct->table_size>4)) {
        cachetable_rehash(ct, ct->table_size/2);
    }

    return 0;
}

/* Requires that no locks be held that are used by the checkpoint logic (ydb, etc.) */
void
toku_cachetable_minicron_shutdown(CACHETABLE ct) {
    int  r = toku_minicron_shutdown(&ct->checkpointer);
    assert(r==0);
    r = toku_minicron_shutdown(&ct->cleaner);
    assert(r==0);
}

/* Require that it all be flushed. */
int 
toku_cachetable_close (CACHETABLE *ctp) {
    CACHETABLE ct=*ctp;
    if (!toku_minicron_has_been_shutdown(&ct->checkpointer)) {
        // for test code only, production code uses toku_cachetable_minicron_shutdown()
        int  r = toku_minicron_shutdown(&ct->checkpointer);
        assert(r==0);
    }
    if (!toku_minicron_has_been_shutdown(&ct->cleaner)) {
        // for test code only, production code uses toku_cachetable_minicron_shutdown()
        int  r = toku_minicron_shutdown(&ct->cleaner);
        assert(r==0);
    }
    int r;
    cachetable_lock(ct);
    if ((r=cachetable_flush_cachefile(ct, NULL))) {
        cachetable_unlock(ct);
        return r;
    }
    u_int32_t i;
    for (i=0; i<ct->table_size; i++) {
	if (ct->table[i]) return -1;
    }
    assert(ct->size_evicting == 0);
    rwlock_destroy(&ct->pending_lock);
    r = toku_pthread_mutex_destroy(&ct->openfd_mutex); resource_assert_zero(r);
    cachetable_unlock(ct);
    toku_destroy_workers(&ct->wq, &ct->threadpool);
    toku_kibbutz_destroy(ct->kibbutz);
    toku_omt_destroy(&ct->reserved_filenums);
    r = toku_pthread_mutex_destroy(&ct->cachefiles_mutex); resource_assert_zero(r);
    toku_free(ct->table);
    toku_free(ct->env_dir);
    toku_free(ct);
    *ctp = 0;
    return 0;
}

int toku_cachetable_unpin_and_remove (CACHEFILE cachefile, CACHEKEY key, BOOL ct_prelocked) {
    int r = ENOENT;
    // Removing something already present is OK.
    CACHETABLE ct = cachefile->cachetable;
    PAIR p;
    int count = 0;
    if (!ct_prelocked) cachetable_lock(ct);
    u_int32_t fullhash = toku_cachetable_hash(cachefile, key);
    for (p=ct->table[fullhash&(ct->table_size-1)]; p; p=p->hash_chain) {
        count++;
	if (p->key.b==key.b && p->cachefile==cachefile) {
	    p->dirty = CACHETABLE_CLEAN; // clear the dirty bit.  We're just supposed to remove it.
	    assert(nb_mutex_writers(&p->nb_mutex));
            nb_mutex_write_unlock(&p->nb_mutex);
            //
            // need to find a way to assert that 
            // ONLY the checkpoint thread may be blocked here
            //
            // The assumption here is that only the checkpoint thread may 
            // be blocked here. No writer thread may be a blocked writer, 
            // because the writer thread has only locked PAIRs. 
            // The writer thread does not try to acquire a lock. It cannot be a 
            // client thread either, because no client thread should be trying 
            // to lock a node that another thread is trying to remove 
            // from the cachetable. It cannot be a kibbutz thread either
            // because the client controls what work is done on the kibbutz, 
            // and should be smart enough to make sure that no other thread 
            // tries to lock a PAIR while trying to unpin_and_remove it. So, 
            // the only thread that is left that can possibly be a blocked 
            // writer is the checkpoint thread.
            //
            if (nb_mutex_blocked_writers(&p->nb_mutex)>0) {
                struct workqueue cq;
                workqueue_init(&cq);
                while (nb_mutex_blocked_writers(&p->nb_mutex)>0) {
                    //Someone (one or more checkpoint threads) is waiting for a write lock
                    //on this pair.
                    //They are still blocked because we have not released the
                    //cachetable lock.
                    //If we freed the memory for the pair we would have dangling
                    //pointers.  We need to let the checkpoint thread finish up with
                    //this pair.

                    p->cq = &cq;

                    //  If anyone is waiting on write lock, let them finish.
                    cachetable_unlock(ct);

                    WORKITEM wi = NULL;
                    r = workqueue_deq(&cq, &wi, 1);
                    //Writer is now done.
                    assert(r == 0);
                    PAIR pp = workitem_arg(wi);
                    assert(pp == p);

                    //We are holding the write lock on the pair
                    cachetable_lock(ct);
                    assert(nb_mutex_writers(&p->nb_mutex) == 1);
                    BOOL destroyed = FALSE;
                    // Because we assume it is just the checkpoint thread
                    // that may have been blocked (as argued above),
                    // it is safe to simply remove the PAIR from the 
                    // cachetable. We don't need to write anything out.
                    cachetable_complete_write_pair(ct, p, TRUE, &destroyed);
                    if (destroyed) {
                        break;
                    }
                }
                workqueue_destroy(&cq);
            }
            else {
                //Remove pair.
                BOOL destroyed = FALSE;;
                cachetable_maybe_remove_and_free_pair(ct, p, &destroyed);
                assert(destroyed);
            }
            r = 0;
	    goto done;
	}
    }
 done:
    note_hash_count(count);
    if (!ct_prelocked) cachetable_unlock(ct);
    return r;
}

static int
set_filenum_in_array(OMTVALUE brtv, u_int32_t index, void*arrayv) {
    FILENUM *array = arrayv;
    BRT brt = brtv;
    array[index] = toku_cachefile_filenum(brt->cf);
    return 0;
}

static int
log_open_txn (OMTVALUE txnv, u_int32_t UU(index), void *UU(extra)) {
    TOKUTXN    txn    = txnv;
    TOKULOGGER logger = txn->logger;
    FILENUMS open_filenums;
    uint32_t num_filenums = toku_omt_size(txn->open_brts);
    FILENUM array[num_filenums];
    {
        open_filenums.num      = num_filenums;
        open_filenums.filenums = array;
        //Fill in open_filenums
        int r = toku_omt_iterate(txn->open_brts, set_filenum_in_array, array);
        assert(r==0);
    }
    int r = toku_log_xstillopen(logger, NULL, 0,
                                toku_txn_get_txnid(txn),
                                toku_txn_get_txnid(toku_logger_txn_parent(txn)),
                                txn->rollentry_raw_count,
                                open_filenums,
                                txn->force_fsync_on_commit,
                                txn->num_rollback_nodes,
                                txn->num_rollentries,
                                txn->spilled_rollback_head,
                                txn->spilled_rollback_tail,
                                txn->current_rollback);
    assert(r==0);
    return 0;
}

static int
unpin_rollback_log_for_checkpoint (OMTVALUE txnv, u_int32_t UU(index), void *UU(extra)) {
    int r = 0;
    TOKUTXN    txn    = txnv;
    if (txn->pinned_inprogress_rollback_log) {
        r = toku_rollback_log_unpin(txn, txn->pinned_inprogress_rollback_log);
        assert(r==0);
    }
    return r;
}

// TODO: #1510 locking of cachetable is suspect
//             verify correct algorithm overall

int 
toku_cachetable_begin_checkpoint (CACHETABLE ct, TOKULOGGER logger) {
    // Requires:   All three checkpoint-relevant locks must be held (see checkpoint.c).
    // Algorithm:  Write a checkpoint record to the log, noting the LSN of that record.
    //             Use the begin_checkpoint callback to take necessary snapshots (header, btt)
    //             Mark every dirty node as "pending."  ("Pending" means that the node must be
    //                                                    written to disk before it can be modified.)

    {
        unsigned i;
	if (logger) { // Unpin all 'inprogress rollback log nodes' pinned by transactions
            int r = toku_omt_iterate(logger->live_txns,
                                     unpin_rollback_log_for_checkpoint,
                                     NULL);
            assert(r==0);
        }
	cachetable_lock(ct);
	//Initialize accountability counters
	ct->checkpoint_num_files = 0;
	ct->checkpoint_num_txns  = 0;

        //Make list of cachefiles to be included in checkpoint.
        //If refcount is 0, the cachefile is closing (performing a local checkpoint)
        {
            CACHEFILE cf;
            assert(ct->cachefiles_in_checkpoint==NULL);
            cachefiles_lock(ct);
            for (cf = ct->cachefiles; cf; cf=cf->next) {
                assert(!cf->is_closing); //Closing requires ydb lock (or in checkpoint).  Cannot happen.
                assert(cf->refcount>0);  //Must have a reference if not closing.
                //Incremement reference count of cachefile because we're using it for the checkpoint.
                //This will prevent closing during the checkpoint.

                // putting this check so that this function may be called
                // by cachetable tests
                if (cf->note_pin_by_checkpoint) {
                    int r = cf->note_pin_by_checkpoint(cf, cf->userdata);
                    assert(r==0);
                }
                cf->next_in_checkpoint       = ct->cachefiles_in_checkpoint;
                ct->cachefiles_in_checkpoint = cf;
                cf->for_checkpoint           = TRUE;
            }
            cachefiles_unlock(ct);
        }

	if (logger) {
	    // The checkpoint must be performed after the lock is acquired.
	    {
		LSN begin_lsn={.lsn=-1}; // we'll need to store the lsn of the checkpoint begin in all the trees that are checkpointed.
		int r = toku_log_begin_checkpoint(logger, &begin_lsn, 0, 0);
		assert(r==0);
		ct->lsn_of_checkpoint_in_progress = begin_lsn;
	    }
	    // Log all the open files
	    {
                //Must loop through ALL open files (even if not included in checkpoint).
		CACHEFILE cf;
                cachefiles_lock(ct);
		for (cf = ct->cachefiles; cf; cf=cf->next) {
                    if (cf->log_fassociate_during_checkpoint) {
                        int r = cf->log_fassociate_during_checkpoint(cf, cf->userdata);
			ct->checkpoint_num_files++;
                        assert(r==0);
                    }
		}
                cachefiles_unlock(ct);
	    }
	    // Log all the open transactions MUST BE AFTER OPEN FILES
	    {
                ct->checkpoint_num_txns = toku_omt_size(logger->live_txns);
                int r = toku_omt_iterate(logger->live_txns, log_open_txn, NULL);
		assert(r==0);
	    }
	    // Log rollback suppression for all the open files MUST BE AFTER TXNS
	    {
                //Must loop through ALL open files (even if not included in checkpoint).
		CACHEFILE cf;
                cachefiles_lock(ct);
		for (cf = ct->cachefiles; cf; cf=cf->next) {
                    if (cf->log_suppress_rollback_during_checkpoint) {
                        int r = cf->log_suppress_rollback_during_checkpoint(cf, cf->userdata);
                        assert(r==0);
                    }
		}
                cachefiles_unlock(ct);
	    }
	}

        unsigned int npending = 0;
        rwlock_write_lock(&ct->pending_lock, ct->mutex);
        for (i=0; i < ct->table_size; i++) {
            PAIR p;
            for (p = ct->table[i]; p; p=p->hash_chain) {
                assert(!p->checkpoint_pending);
                //Only include pairs belonging to cachefiles in the checkpoint
                if (!p->cachefile->for_checkpoint) continue;
                // mark anything that is dirty OR currently in use
                // as pending a checkpoint
                //
                //The rule for the checkpoint_pending big is as follows:
                //  - begin_checkpoint may set checkpoint_pending to true
                //    even though the pair lock on the node is not held. Only the
                //    cachetable lock is necessary
                //  - any thread that wants to clear the pending bit must own
                //     BOTH the cachetable lock and the PAIR lock. Otherwise,
                //     we may end up clearing the pending bit before giving the
                //     current lock ever released.
                if (p->dirty || nb_mutex_writers(&p->nb_mutex)) {
                    p->checkpoint_pending = TRUE;
                    if (ct->pending_head) {
                        ct->pending_head->pending_prev = p;
                    }
                    p->pending_next                = ct->pending_head;
                    p->pending_prev                = NULL;
                    ct->pending_head               = p;
                    npending++;
                }
            }
        }
        rwlock_write_unlock(&ct->pending_lock);
        if (0 && (npending > 0 || ct->checkpoint_num_files > 0 || ct->checkpoint_num_txns > 0)) {
            fprintf(stderr, "%s:%d pending=%u %u files=%u txns=%u\n", __FUNCTION__, __LINE__, npending, ct->n_in_table, ct->checkpoint_num_files, ct->checkpoint_num_txns);
        }

        //begin_checkpoint_userdata must be called AFTER all the pairs are marked as pending.
        //Once marked as pending, we own write locks on the pairs, which means the writer threads can't conflict.
	{
	    CACHEFILE cf;
            cachefiles_lock(ct);
	    for (cf = ct->cachefiles_in_checkpoint; cf; cf=cf->next_in_checkpoint) {
		if (cf->begin_checkpoint_userdata) {
                    rwlock_prefer_read_lock(&cf->fdlock, ct->mutex);
                    rwlock_write_lock(&cf->checkpoint_lock, ct->mutex);
                    assert(cf->checkpoint_state == CS_NOT_IN_PROGRESS);
		    int r = cf->begin_checkpoint_userdata(cf, cf->fd, ct->lsn_of_checkpoint_in_progress, cf->userdata);
		    assert(r==0);
                    cf->checkpoint_state = CS_CALLED_BEGIN_CHECKPOINT;
                    rwlock_write_unlock(&cf->checkpoint_lock);
                    rwlock_read_unlock(&cf->fdlock);
		}
	    }
            cachefiles_unlock(ct);
	}

	cachetable_unlock(ct);
    }
    return 0;
}


// This is used by the cachetable_race test.  
static volatile int toku_checkpointing_user_data_status = 0;
static void toku_cachetable_set_checkpointing_user_data_status (int v) {
    toku_checkpointing_user_data_status = v;
}
int toku_cachetable_get_checkpointing_user_data_status (void) {
    return toku_checkpointing_user_data_status;
}

int
toku_cachetable_end_checkpoint(CACHETABLE ct, TOKULOGGER logger,
                               void (*ydb_lock)(void), void (*ydb_unlock)(void),
                               void (*testcallback_f)(void*),  void * testextra) {
    // Requires:   The big checkpoint lock must be held (see checkpoint.c).
    // Algorithm:  Write all pending nodes to disk
    //             Use checkpoint callback to write snapshot information to disk (header, btt)
    //             Use end_checkpoint callback to fsync dictionary and log, and to free unused blocks
    // Note:       If testcallback is null (for testing purposes only), call it after writing dictionary but before writing log

    int retval = 0;
    cachetable_lock(ct);
    {
        // 
        // #TODO: #1424 Long-lived get and pin (held by cursor) will cause a deadlock here.
        //        Need some solution (possibly modify requirement for write lock or something else).
	PAIR p;
	while ((p = ct->pending_head)!=0) {
            ct->pending_head = ct->pending_head->pending_next;
            pending_pairs_remove(ct, p);
	    write_pair_for_checkpoint(ct, p); // if still pending, clear the pending bit and write out the node
	    // Don't need to unlock and lock cachetable, because the cachetable was unlocked and locked while the flush callback ran.
	}
    }
    assert(!ct->pending_head);


    {   // have just written data blocks, so next write the translation and header for each open dictionary
	CACHEFILE cf;
        //cachefiles_in_checkpoint is protected by the checkpoint_safe_lock
	for (cf = ct->cachefiles_in_checkpoint; cf; cf=cf->next_in_checkpoint) {
	    if (cf->checkpoint_userdata) {
                rwlock_prefer_read_lock(&cf->fdlock, ct->mutex);
                rwlock_write_lock(&cf->checkpoint_lock, ct->mutex);
                if (!logger || ct->lsn_of_checkpoint_in_progress.lsn != cf->most_recent_global_checkpoint_that_finished_early.lsn) {
                    assert(ct->lsn_of_checkpoint_in_progress.lsn >= cf->most_recent_global_checkpoint_that_finished_early.lsn);
                    cachetable_unlock(ct);
                    assert(cf->checkpoint_state == CS_CALLED_BEGIN_CHECKPOINT);
		    toku_cachetable_set_checkpointing_user_data_status(1);
                    int r = cf->checkpoint_userdata(cf, cf->fd, cf->userdata);
		    toku_cachetable_set_checkpointing_user_data_status(0);
                    assert(r==0);
                    cf->checkpoint_state = CS_CALLED_CHECKPOINT;
                    cachetable_lock(ct);
                }
                else {
                    assert(cf->checkpoint_state == CS_NOT_IN_PROGRESS);
                }
                rwlock_write_unlock(&cf->checkpoint_lock);
                rwlock_read_unlock(&cf->fdlock);
	    }
	}
    }

    {   // everything has been written to file (or at least OS internal buffer)...
	// ... so fsync and call checkpoint-end function in block translator
	//     to free obsolete blocks on disk used by previous checkpoint
	CACHEFILE cf;
        //cachefiles_in_checkpoint is protected by the checkpoint_safe_lock
	for (cf = ct->cachefiles_in_checkpoint; cf; cf=cf->next_in_checkpoint) {
	    if (cf->end_checkpoint_userdata) {
                rwlock_prefer_read_lock(&cf->fdlock, ct->mutex);
                rwlock_write_lock(&cf->checkpoint_lock, ct->mutex);
                if (!logger || ct->lsn_of_checkpoint_in_progress.lsn != cf->most_recent_global_checkpoint_that_finished_early.lsn) {
                    assert(ct->lsn_of_checkpoint_in_progress.lsn >= cf->most_recent_global_checkpoint_that_finished_early.lsn);
                    cachetable_unlock(ct);
                    //end_checkpoint fsyncs the fd, which needs the fdlock
                    assert(cf->checkpoint_state == CS_CALLED_CHECKPOINT);
                    int r = cf->end_checkpoint_userdata(cf, cf->fd, cf->userdata);
                    assert(r==0);
                    cf->checkpoint_state = CS_NOT_IN_PROGRESS;
                    cachetable_lock(ct);
                }
                assert(cf->checkpoint_state == CS_NOT_IN_PROGRESS);
                rwlock_write_unlock(&cf->checkpoint_lock);
                rwlock_read_unlock(&cf->fdlock);
	    }
	}
    }
    cachetable_unlock(ct);

    {
        //Delete list of cachefiles in the checkpoint,
        //remove reference
        //clear bit saying they're in checkpoint
	CACHEFILE cf;
        //cachefiles_in_checkpoint is protected by the checkpoint_safe_lock
        while ((cf = ct->cachefiles_in_checkpoint)) {
            ct->cachefiles_in_checkpoint = cf->next_in_checkpoint; 
            cf->next_in_checkpoint       = NULL;
            cf->for_checkpoint           = FALSE;
            // checking for function existing so that this function
            // can be called from cachetable tests
            if (cf->note_unpin_by_checkpoint) {
                ydb_lock();
                int r = cf->note_unpin_by_checkpoint(cf, cf->userdata);
                ydb_unlock();
                if (r!=0) {
                    retval = r;
                    goto panic;
                }
            }
        }
    }

    // For testing purposes only.  Dictionary has been fsync-ed to disk but log has not yet been written.
    if (testcallback_f) 
	testcallback_f(testextra);      

    if (logger) {
	int r = toku_log_end_checkpoint(logger, NULL,
					1, // want the end_checkpoint to be fsync'd
					ct->lsn_of_checkpoint_in_progress.lsn, 
					0,
					ct->checkpoint_num_files,
					ct->checkpoint_num_txns);
	assert(r==0);
	toku_logger_note_checkpoint(logger, ct->lsn_of_checkpoint_in_progress);
    }
    
panic:
    return retval;
}

TOKULOGGER toku_cachefile_logger (CACHEFILE cf) {
    return cf->cachetable->logger;
}

FILENUM toku_cachefile_filenum (CACHEFILE cf) {
    return cf->filenum;
}


// Worker thread function to write a pair from memory to its cachefile
// As of now, the writer thread NEVER evicts, hence passing FALSE
// for the third parameter to cachetable_write_pair
static void cachetable_writer(WORKITEM wi) {
    PAIR p = workitem_arg(wi);
    CACHETABLE ct = p->cachefile->cachetable;
    cachetable_lock(ct);
    cachetable_write_pair(ct, p, p->remove_me);
    cachetable_unlock(ct);
}

// Worker thread function to read a pair from a cachefile to memory
static void cachetable_reader(WORKITEM wi) {
    struct cachefile_prefetch_args* cpargs = workitem_arg(wi);
    CACHETABLE ct = cpargs->p->cachefile->cachetable;
    cachetable_lock(ct);
    // TODO: find a way to properly pass some information for read_extraargs
    // This is only called in toku_cachefile_prefetch, by putting it on a workqueue
    // The problem is described in comments in toku_cachefile_prefetch
    cachetable_fetch_pair(
        ct,
        cpargs->p->cachefile,
        cpargs->p,
        cpargs->fetch_callback,
        cpargs->read_extraargs
        );
    cachetable_unlock(ct);
    toku_free(cpargs);
}

static void cachetable_partial_reader(WORKITEM wi) {
    struct cachefile_partial_prefetch_args *cpargs = workitem_arg(wi);
    CACHETABLE ct = cpargs->p->cachefile->cachetable;
    cachetable_lock(ct);
    do_partial_fetch(ct, cpargs->p->cachefile, cpargs->p, cpargs->pf_callback, cpargs->read_extraargs, FALSE);
    cachetable_unlock(ct);
    toku_free(cpargs);
}


// debug functions

int toku_cachetable_assert_all_unpinned (CACHETABLE ct) {
    u_int32_t i;
    int some_pinned=0;
    cachetable_lock(ct);
    for (i=0; i<ct->table_size; i++) {
	PAIR p;
	for (p=ct->table[i]; p; p=p->hash_chain) {
	    assert(nb_mutex_writers(&p->nb_mutex)>=0);
	    if (nb_mutex_writers(&p->nb_mutex)) {
		//printf("%s:%d pinned: %"PRId64" (%p)\n", __FILE__, __LINE__, p->key.b, p->value);
		some_pinned=1;
	    }
	}
    }
    cachetable_unlock(ct);
    return some_pinned;
}

int toku_cachefile_count_pinned (CACHEFILE cf, int print_them) {
    assert(cf != NULL);
    int n_pinned=0;
    CACHETABLE ct = cf->cachetable;
    cachetable_lock(ct);
    for (struct toku_list *next_pair = cf->pairs_for_cachefile.next; next_pair != &cf->pairs_for_cachefile; next_pair = next_pair->next) {
        PAIR p = toku_list_struct(next_pair, struct ctpair, next_for_cachefile);
        assert(nb_mutex_writers(&p->nb_mutex) >= 0);
        if (nb_mutex_writers(&p->nb_mutex)) {
            if (print_them) printf("%s:%d pinned: %"PRId64" (%p)\n", __FILE__, __LINE__, p->key.b, p->value);
            n_pinned++;
        }
    }
    cachetable_unlock(ct);
    return n_pinned;
}

void toku_cachetable_print_state (CACHETABLE ct) {
    u_int32_t i;
    cachetable_lock(ct);
    for (i=0; i<ct->table_size; i++) {
        PAIR p = ct->table[i];
        if (p != 0) {
            printf("t[%u]=", i);
            for (p=ct->table[i]; p; p=p->hash_chain) {
                printf(" {%"PRId64", %p, dirty=%d, pin=%d, size=%ld}", p->key.b, p->cachefile, (int) p->dirty, nb_mutex_writers(&p->nb_mutex), p->attr.size);
            }
            printf("\n");
        }
    }
    cachetable_unlock(ct);
}

void toku_cachetable_get_state (CACHETABLE ct, int *num_entries_ptr, int *hash_size_ptr, long *size_current_ptr, long *size_limit_ptr, int64_t *size_max_ptr) {
    cachetable_lock(ct);
    if (num_entries_ptr) 
        *num_entries_ptr = ct->n_in_table;
    if (hash_size_ptr)
        *hash_size_ptr = ct->table_size;
    if (size_current_ptr)
        *size_current_ptr = ct->size_current;
    if (size_limit_ptr)
        *size_limit_ptr = ct->size_limit;
    if (size_max_ptr)
        *size_max_ptr = ct->size_max;
    cachetable_unlock(ct);
}

int toku_cachetable_get_key_state (CACHETABLE ct, CACHEKEY key, CACHEFILE cf, void **value_ptr,
				   int *dirty_ptr, long long *pin_ptr, long *size_ptr) {
    PAIR p;
    int count = 0;
    int r = -1;
    u_int32_t fullhash = toku_cachetable_hash(cf, key);
    cachetable_lock(ct);
    for (p = ct->table[fullhash&(ct->table_size-1)]; p; p = p->hash_chain) {
	count++;
        if (p->key.b == key.b && p->cachefile == cf) {
	    note_hash_count(count);
            if (value_ptr)
                *value_ptr = p->value;
            if (dirty_ptr)
                *dirty_ptr = p->dirty;
            if (pin_ptr)
                *pin_ptr = nb_mutex_writers(&p->nb_mutex);
            if (size_ptr)
                *size_ptr = p->attr.size;
            r = 0;
            break;
        }
    }
    note_hash_count(count);
    cachetable_unlock(ct);
    return r;
}

void
toku_cachefile_set_userdata (CACHEFILE cf,
			     void *userdata,
                             int (*log_fassociate_during_checkpoint)(CACHEFILE, void*),
                             int (*log_suppress_rollback_during_checkpoint)(CACHEFILE, void*),
			     int (*close_userdata)(CACHEFILE, int, void*, char**, BOOL, LSN),
			     int (*checkpoint_userdata)(CACHEFILE, int, void*),
			     int (*begin_checkpoint_userdata)(CACHEFILE, int, LSN, void*),
                             int (*end_checkpoint_userdata)(CACHEFILE, int, void*),
                             int (*note_pin_by_checkpoint)(CACHEFILE, void*),
                             int (*note_unpin_by_checkpoint)(CACHEFILE, void*)) {
    cf->userdata = userdata;
    cf->log_fassociate_during_checkpoint = log_fassociate_during_checkpoint;
    cf->log_suppress_rollback_during_checkpoint = log_suppress_rollback_during_checkpoint;
    cf->close_userdata = close_userdata;
    cf->checkpoint_userdata = checkpoint_userdata;
    cf->begin_checkpoint_userdata = begin_checkpoint_userdata;
    cf->end_checkpoint_userdata = end_checkpoint_userdata;
    cf->note_pin_by_checkpoint = note_pin_by_checkpoint;
    cf->note_unpin_by_checkpoint = note_unpin_by_checkpoint;
}

void *toku_cachefile_get_userdata(CACHEFILE cf) {
    return cf->userdata;
}

CACHETABLE
toku_cachefile_get_cachetable(CACHEFILE cf) {
    return cf->cachetable;
}

//Only called by toku_brtheader_end_checkpoint
//Must have access to cf->fd (must be protected)
int
toku_cachefile_fsync(CACHEFILE cf) {
    int r;
    if (toku_cachefile_is_dev_null_unlocked(cf)) 
        r = 0; //Don't fsync /dev/null
    else 
        r = toku_file_fsync(cf->fd);
    return r;
}

int toku_cachefile_redirect_nullfd (CACHEFILE cf) {
    int null_fd;
    struct fileid fileid;

    CACHETABLE ct = cf->cachetable;
    cachetable_lock(ct);
    rwlock_write_lock(&cf->fdlock, ct->mutex);
    null_fd = open(DEV_NULL_FILE, O_WRONLY+O_BINARY);           
    assert(null_fd>=0);
    int r = toku_os_get_unique_file_id(null_fd, &fileid);
    assert(r==0);
    close(cf->fd);  // no change for t:2444
    cf->fd = null_fd;
    char *saved_fname_in_env = cf->fname_in_env;
    cf->fname_in_env = NULL;
    cachefile_init_filenum(cf, null_fd, saved_fname_in_env, fileid);
    if (saved_fname_in_env) toku_free(saved_fname_in_env);
    cf->is_dev_null = TRUE;
    rwlock_write_unlock(&cf->fdlock);
    cachetable_unlock(ct);
    return 0;
}

u_int64_t
toku_cachefile_size_in_memory(CACHEFILE cf)
{
    u_int64_t result=0;
    CACHETABLE ct=cf->cachetable;
    unsigned long i;
    for (i=0; i<ct->table_size; i++) {
	PAIR p;
	for (p=ct->table[i]; p; p=p->hash_chain) {
	    if (p->cachefile==cf) {
		result += p->attr.size;
	    }
	}
    }
    return result;
}

void toku_cachetable_get_status(CACHETABLE ct, CACHETABLE_STATUS s) {
    s->lock_taken    = cachetable_lock_taken;
    s->lock_released = cachetable_lock_released;
    s->hit          = cachetable_hit;
    s->miss         = cachetable_miss;
    s->misstime     = cachetable_misstime;
    s->waittime     = cachetable_waittime;
    s->wait_reading = cachetable_wait_reading;
    s->wait_writing = cachetable_wait_writing;
    s->wait_checkpoint = cachetable_wait_checkpoint;
    s->puts         = cachetable_puts;
    s->prefetches   = cachetable_prefetches;
    s->maybe_get_and_pins      = cachetable_maybe_get_and_pins;
    s->maybe_get_and_pin_hits  = cachetable_maybe_get_and_pin_hits;
    s->size_current = ct->size_current;          
    s->size_limit   = ct->size_limit;            
    s->size_max     = ct->size_max;
    s->size_writing = ct->size_evicting;          
    s->get_and_pin_footprint = 0;
    s->local_checkpoint      = local_checkpoint;
    s->local_checkpoint_files = local_checkpoint_files;
    s->local_checkpoint_during_checkpoint = local_checkpoint_during_checkpoint;
    s->evictions = cachetable_evictions;
    s->cleaner_executions = cleaner_executions;
    s->size_nonleaf = ct->size_nonleaf;
    s->size_leaf = ct->size_leaf;
    s->size_rollback = ct->size_rollback;
    s->size_cachepressure = ct->size_cachepressure;
}

char *
toku_construct_full_name(int count, ...) {
    va_list ap;
    char *name = NULL;
    size_t n = 0;
    int i;
    va_start(ap, count);
    for (i=0; i<count; i++) {
        char *arg = va_arg(ap, char *);
        if (arg) {
            n += 1 + strlen(arg) + 1;
            char *newname = toku_xmalloc(n);
            if (name && !toku_os_is_absolute_name(arg))
                snprintf(newname, n, "%s/%s", name, arg);
            else
                snprintf(newname, n, "%s", arg);
            toku_free(name);
            name = newname;
        }
    }
    va_end(ap);

    return name;
}

char *
toku_cachetable_get_fname_in_cwd(CACHETABLE ct, const char * fname_in_env) {
    return toku_construct_full_name(2, ct->env_dir, fname_in_env);
}


// Returns the limit on the size of the cache table
uint64_t toku_cachetable_get_size_limit(CACHETABLE ct) {
    return ct->size_limit;
}


static long
cleaner_thread_rate_pair(PAIR p)
{
    return p->attr.cache_pressure_size;
}

static int const CLEANER_N_TO_CHECK = 8;

static int
cleaner_thread (void *cachetable_v)
// Effect:  runs a cleaner.
//
// We look through some number of nodes, the first N that we see which are
// unlocked and are not involved in a cachefile flush, pick one, and call
// the cleaner callback.  While we're picking a node, we have the
// cachetable lock the whole time, so we don't need any extra
// synchronization.  Once we have one we want, we lock it and notify the
// cachefile that we're doing some background work (so a flush won't
// start).  At this point, we can safely unlock the cachetable, do the
// work (callback), and unlock/release our claim to the cachefile.
{
    CACHETABLE ct = cachetable_v;
    assert(ct);
    u_int32_t num_iterations = toku_get_cleaner_iterations(ct);
    for (u_int32_t i = 0; i < num_iterations; ++i) {
        cleaner_executions++;
        cachetable_lock(ct);
        PAIR best_pair = NULL;
        int n_seen = 0;
        long best_score = 0;
        const PAIR first_pair = ct->cleaner_head;
        if (first_pair == NULL) {
            // nothing in the cachetable, just get out now
            cachetable_unlock(ct);
            break;
        }
        do {
            if (nb_mutex_users(&ct->cleaner_head->nb_mutex) > 0 || ct->cleaner_head->cachefile->is_flushing) {
                goto next_pair;
            }
            n_seen++;
            long score = cleaner_thread_rate_pair(ct->cleaner_head);
            if (score > best_score) {
                best_score = score;
                best_pair = ct->cleaner_head;
            }
        next_pair:
            ct->cleaner_head = ct->cleaner_head->clock_next;
        } while (ct->cleaner_head != first_pair && n_seen < CLEANER_N_TO_CHECK);
        if (best_pair) {
            nb_mutex_write_lock(&best_pair->nb_mutex, ct->mutex);
            add_background_job(best_pair->cachefile, true);
        }
        cachetable_unlock(ct);
        if (best_pair) {
            CACHEFILE cf = best_pair->cachefile;
            int r = best_pair->cleaner_callback(best_pair->value,
                                                best_pair->key,
                                                best_pair->fullhash,
                                                best_pair->write_extraargs);
            assert_zero(r);
            // We need to make sure the cachefile sticks around so a close
            // can't come destroy it.  That's the purpose of this
            // "add/remove_background_job" business, which means the
            // cachefile is still valid here, even though the cleaner
            // callback unlocks the pair.
            remove_background_job(cf, false);
            // The cleaner callback must have unlocked the pair, so we
            // don't need to unlock it here.
        } else {
            // If we didn't find anything this time around the cachetable,
            // we probably won't find anything if we run around again, so
            // just break out now and we'll try again when the cleaner
            // thread runs again.
            break;
        }
    }
    return 0;
}


#if 0
int 
toku_cachetable_local_checkpoint_for_commit (CACHETABLE ct, TOKUTXN txn, uint32_t n, CACHEFILE cachefiles[n]) {
    cachetable_lock(ct);
    local_checkpoint++;
    local_checkpoint_files += n;

    LSN begin_checkpoint_lsn = ZERO_LSN;
    uint32_t i;
    TOKULOGGER logger = txn->logger; 
    CACHEFILE cf;
    assert(logger); //Need transaction, so there must be a logger
    {
        int r = toku_log_local_txn_checkpoint(logger, &begin_checkpoint_lsn, 0, txn->txnid64);
        assert(r==0);
    }
    for (i = 0; i < n; i++) {
        cf = cachefiles[i];
        assert(cf->for_local_checkpoint.lsn == ZERO_LSN.lsn);
        cf->for_local_checkpoint = begin_checkpoint_lsn;
    }

    //Write out all dirty pairs.
    {
        uint32_t num_pairs = 0;
        uint32_t list_size = 256;
        PAIR *list = NULL;
        XMALLOC_N(list_size, list);
        PAIR p;

        //TODO: Determine if we can get rid of this use of pending_lock
        rwlock_write_lock(&ct->pending_lock, ct->mutex);
        for (i=0; i < ct->table_size; i++) {
            for (p = ct->table[i]; p; p=p->hash_chain) {
                //Only include pairs belonging to cachefiles in the checkpoint
                if (p->cachefile->for_local_checkpoint.lsn != begin_checkpoint_lsn.lsn) continue;
                if (p->state == CTPAIR_READING)
                    continue;   // skip pairs being read as they will be clean
                else if (p->state == CTPAIR_IDLE || p->state == CTPAIR_WRITING) {
                    if (p->dirty) {
                        ctpair_add_ref(p);
                        list[num_pairs] = p;
                        num_pairs++;
                        if (num_pairs == list_size) {
                            list_size *= 2;
                            XREALLOC_N(list_size, list);
                        }
                    }
                } else
                    assert(0);
            }
        }
        rwlock_write_unlock(&ct->pending_lock);

        for (i = 0; i < num_pairs; i++) {
            p = list[i];
            if (!p->already_removed) {
                write_pair_for_checkpoint(ct, p, TRUE);
            }
            ctpair_destroy(p);     //Release our reference
            // Don't need to unlock and lock cachetable,
            // because the cachetable was unlocked and locked while the flush callback ran.
        }
        toku_free(list);
    }

    for (i = 0; i < n; i++) {
        int r;
        cf = cachefiles[i];
        rwlock_prefer_read_lock(&cf->fdlock, ct->mutex);
        rwlock_write_lock(&cf->checkpoint_lock, ct->mutex);
        BOOL own_cachetable_lock = TRUE;
        switch (cf->checkpoint_state) {
        case CS_NOT_IN_PROGRESS:
            break;
        case CS_CALLED_BEGIN_CHECKPOINT:
            cachetable_unlock(ct);
            own_cachetable_lock = FALSE;
            assert(cf->checkpoint_state == CS_CALLED_BEGIN_CHECKPOINT);
            r = cf->checkpoint_userdata(cf, cf->fd, cf->userdata);
            assert(r==0);
            cf->checkpoint_state = CS_CALLED_CHECKPOINT;
            //FALL THROUGH ON PURPOSE.
        case CS_CALLED_CHECKPOINT:
            if (own_cachetable_lock)
                cachetable_unlock(ct);
            //end_checkpoint fsyncs the fd, which needs the fdlock
            assert(cf->checkpoint_state == CS_CALLED_CHECKPOINT);
            r = cf->end_checkpoint_userdata(cf, cf->fd, cf->userdata);
            assert(r==0);
            cf->checkpoint_state = CS_NOT_IN_PROGRESS;
            cachetable_lock(ct);
            assert(cf->most_recent_global_checkpoint_that_finished_early.lsn < ct->lsn_of_checkpoint_in_progress.lsn);
            cf->most_recent_global_checkpoint_that_finished_early = ct->lsn_of_checkpoint_in_progress;
	    local_checkpoint_during_checkpoint++;
            break;
        default:
            assert(FALSE);
        }
        { //Begin
            assert(cf->checkpoint_state == CS_NOT_IN_PROGRESS);
            r = cf->begin_checkpoint_userdata(cf, cf->fd, begin_checkpoint_lsn, cf->userdata);
            assert(r==0);
            cf->checkpoint_state = CS_CALLED_BEGIN_CHECKPOINT;
        }
        { //Middle
            assert(cf->checkpoint_state == CS_CALLED_BEGIN_CHECKPOINT);
            r = cf->checkpoint_userdata(cf, cf->fd, cf->userdata);
            assert(r==0);
            cf->checkpoint_state = CS_CALLED_CHECKPOINT;
        }
        { //End
            assert(cf->checkpoint_state == CS_CALLED_CHECKPOINT);
            r = cf->end_checkpoint_userdata(cf, cf->fd, cf->userdata);
            assert(r==0);
            cf->checkpoint_state = CS_NOT_IN_PROGRESS;
        }
        assert(cf->for_local_checkpoint.lsn == begin_checkpoint_lsn.lsn);
        cf->for_local_checkpoint = ZERO_LSN;

        rwlock_write_unlock(&cf->checkpoint_lock);
        rwlock_read_unlock(&cf->fdlock);
    }

    cachetable_unlock(ct);

    return 0;
}
#endif
