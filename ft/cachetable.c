/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <valgrind/helgrind.h>

#include "memory.h"
#include "workqueue.h"
#include "threadpool.h"
#include "cachetable.h"
#include "rwlock.h"
#include "nonblocking_mutex.h"
#include <ft/log_header.h>
#include "checkpoint.h"
#include "minicron.h"
#include "log-internal.h"
#include "kibbutz.h"
#include "background_job_manager.h"

///////////////////////////////////////////////////////////////////////////////////
// Engine status
//
// Status is intended for display to humans to help understand system behavior.
// It does not need to be perfectly thread-safe.

// These should be in the cachetable object, but we make them file-wide so that gdb can get them easily.
// They were left here after engine status cleanup (#2949, rather than moved into the status struct)
// so they are still easily available to the debugger and to save lots of typing.
static u_int64_t cachetable_miss;
static u_int64_t cachetable_misstime;     // time spent waiting for disk read
static u_int64_t cachetable_puts;          // how many times has a newly created node been put into the cachetable?
static u_int64_t cachetable_prefetches;    // how many times has a block been prefetched into the cachetable?
static u_int64_t cachetable_evictions;
static u_int64_t cleaner_executions; // number of times the cleaner thread's loop has executed

static CACHETABLE_STATUS_S ct_status;

// Note, toku_cachetable_get_status() is below, after declaration of cachetable.

#define STATUS_INIT(k,t,l) { \
    ct_status.status[k].keyname = #k; \
    ct_status.status[k].type    = t;  \
    ct_status.status[k].legend  = "cachetable: " l; \
    }

static void
status_init(void) {
    // Note, this function initializes the keyname, type, and legend fields.
    // Value fields are initialized to zero by compiler.

    STATUS_INIT(CT_MISS,                   UINT64, "miss");
    STATUS_INIT(CT_MISSTIME,               UINT64, "miss time");
    STATUS_INIT(CT_PUTS,                   UINT64, "puts (new nodes created)");
    STATUS_INIT(CT_PREFETCHES,             UINT64, "prefetches");
    STATUS_INIT(CT_SIZE_CURRENT,           UINT64, "size current");
    STATUS_INIT(CT_SIZE_LIMIT,             UINT64, "size limit");
    STATUS_INIT(CT_SIZE_MAX,               UINT64, "size max");
    STATUS_INIT(CT_SIZE_WRITING,           UINT64, "size writing");
    STATUS_INIT(CT_SIZE_NONLEAF,           UINT64, "size nonleaf");
    STATUS_INIT(CT_SIZE_LEAF,              UINT64, "size leaf");
    STATUS_INIT(CT_SIZE_ROLLBACK,          UINT64, "size rollback");
    STATUS_INIT(CT_SIZE_CACHEPRESSURE,     UINT64, "size cachepressure");
    STATUS_INIT(CT_EVICTIONS,              UINT64, "evictions");
    STATUS_INIT(CT_CLEANER_EXECUTIONS,     UINT64, "cleaner executions");
    STATUS_INIT(CT_CLEANER_PERIOD,         UINT64, "cleaner period");
    STATUS_INIT(CT_CLEANER_ITERATIONS,     UINT64, "cleaner iterations");
    ct_status.initialized = true;
}
#undef STATUS_INIT

#define STATUS_VALUE(x) ct_status.status[x].value.num

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
    void* value_data; // data used by client threads, FTNODEs and ROLLBACK_LOG_NODEs
    void* cloned_value_data; // cloned copy of value_data used for checkpointing
    long cloned_value_size; // size of cloned_value_data, used for accounting of ct->size_current
    void* disk_data; // data used to fetch/flush value_data to and from disk.
    PAIR_ATTR attr;

    enum cachetable_dirty dirty;

    u_int32_t fullhash;

    CACHETABLE_FLUSH_CALLBACK flush_callback;
    CACHETABLE_PARTIAL_EVICTION_EST_CALLBACK pe_est_callback;
    CACHETABLE_PARTIAL_EVICTION_CALLBACK pe_callback;
    CACHETABLE_CLEANER_CALLBACK cleaner_callback;
    CACHETABLE_CLONE_CALLBACK clone_callback;
    long size_evicting_estimate;
    void    *write_extraargs;

    PAIR     clock_next,clock_prev; // In clock.
    PAIR     hash_chain;
    u_int32_t count;        // clock count

    BOOL     checkpoint_pending; // If this is on, then we have got to write the pair out to disk before modifying it.
    PAIR     pending_next;
    PAIR     pending_prev;

    struct nb_mutex value_nb_mutex;    // single writer, protects value_data
    struct nb_mutex disk_nb_mutex;    // single writer, protects disk_data, is used for writing cloned nodes for checkpoint
    struct workitem asyncwork;   // work item for the worker threads
    struct workitem checkpoint_asyncwork;   // work item for the worker threads
    struct toku_list next_for_cachefile; // link in the cachefile list
};

static void * const zero_value = 0;
static PAIR_ATTR const zero_attr = {
    .size = 0, 
    .nonleaf_size = 0, 
    .leaf_size = 0, 
    .rollback_size = 0, 
    .cache_pressure_size = 0,
    .is_valid = TRUE
};

static void maybe_flush_some (CACHETABLE ct, long size);

static inline void ctpair_destroy(PAIR p) {
    nb_mutex_destroy(&p->value_nb_mutex);
    nb_mutex_destroy(&p->disk_nb_mutex);
    toku_free(p);
}

// The cachetable is as close to an ENV as we get.
//      cachetable_mutex
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
    toku_mutex_t *mutex;  // coarse lock that protects the cachetable, the cachefiles, and the pairs
    struct workqueue wq;          // async work queue 
    THREADPOOL threadpool;        // pool of worker threads
    struct workqueue checkpoint_wq;          
    THREADPOOL checkpoint_threadpool;        

    KIBBUTZ kibbutz;              // another pool of worker threads and jobs to do asynchronously.  

    LSN lsn_of_checkpoint_in_progress;
    // Variables used to detect threadsafety bugs are declared volatile to prevent compiler from using thread-local cache.
    volatile BOOL checkpoint_is_beginning;    // TRUE during begin_checkpoint(), used for detecting threadsafety bugs
    volatile uint64_t checkpoint_prohibited;  // nonzero when checkpoints are prohibited,  used for detecting threadsafety bugs
    u_int32_t checkpoint_num_files;  // how many cachefiles are in the checkpoint
    u_int32_t checkpoint_num_txns;   // how many transactions are in the checkpoint
    PAIR pending_head;           // list of pairs marked with checkpoint_pending
    struct rwlock pending_lock;  // multiple writer threads, single checkpoint thread,
                                 // see comments in toku_cachetable_begin_checkpoint to understand
                                 // purpose of the pending_lock
    struct minicron checkpointer; // the periodic checkpointing thread
    struct minicron cleaner; // the periodic cleaner thread
    u_int32_t cleaner_iterations; // how many times to run the cleaner per
                                  // cleaner period (minicron has a
                                  // minimum period of 1s so if you want
                                  // more frequent cleaner runs you must
                                  // use this)
    char *env_dir;

    // variables for engine status
    int64_t size_nonleaf;
    int64_t size_leaf;
    int64_t size_rollback;
    int64_t size_cachepressure;

    // variable used by the checkpoint thread to know
    // when all work induced by cloning on client threads is done
    BACKGROUND_JOB_MANAGER checkpoint_clones_bjm;

    // temporary, for handling flow control
    toku_cond_t flow_control_cond;
};


void
toku_cachetable_get_status(CACHETABLE ct, CACHETABLE_STATUS statp) {
    if (!ct_status.initialized) {
        status_init();
    }
    STATUS_VALUE(CT_MISS)                   = cachetable_miss;
    STATUS_VALUE(CT_MISSTIME)               = cachetable_misstime;
    STATUS_VALUE(CT_PUTS)                   = cachetable_puts;
    STATUS_VALUE(CT_PREFETCHES)             = cachetable_prefetches;
    STATUS_VALUE(CT_SIZE_CURRENT)           = ct->size_current;
    STATUS_VALUE(CT_SIZE_LIMIT)             = ct->size_limit;
    STATUS_VALUE(CT_SIZE_MAX)               = ct->size_max;
    STATUS_VALUE(CT_SIZE_WRITING)           = ct->size_evicting;
    STATUS_VALUE(CT_SIZE_NONLEAF)           = ct->size_nonleaf;
    STATUS_VALUE(CT_SIZE_LEAF)              = ct->size_leaf;
    STATUS_VALUE(CT_SIZE_ROLLBACK)          = ct->size_rollback;
    STATUS_VALUE(CT_SIZE_CACHEPRESSURE)     = ct->size_cachepressure;
    STATUS_VALUE(CT_EVICTIONS)              = cachetable_evictions;
    STATUS_VALUE(CT_CLEANER_EXECUTIONS)     = cleaner_executions;
    STATUS_VALUE(CT_CLEANER_PERIOD)         = toku_get_cleaner_period_unlocked(ct);
    STATUS_VALUE(CT_CLEANER_ITERATIONS)     = toku_get_cleaner_iterations_unlocked(ct);
    *statp = ct_status;
}


// Code bracketed with {BEGIN_CRITICAL_REGION; ... END_CRITICAL_REGION;} macros
// are critical regions in which a checkpoint is not permitted to begin.
// Must increment checkpoint_prohibited before testing checkpoint_is_beginning
// on entry to critical region.
#define BEGIN_CRITICAL_REGION {__sync_fetch_and_add(&ct->checkpoint_prohibited, 1); invariant(!ct->checkpoint_is_beginning);}

// Testing checkpoint_prohibited at end of critical region is just belt-and-suspenders redundancy,
// verifying that we just incremented it with the matching BEGIN macro.
#define END_CRITICAL_REGION {invariant(ct->checkpoint_prohibited > 0); __sync_fetch_and_sub(&ct->checkpoint_prohibited, 1);}

// Lock the cachetable. Used for a variety of purposes. TODO: like what?
static inline void cachetable_lock(CACHETABLE ct __attribute__((unused))) {
    toku_mutex_lock(ct->mutex);
}

// Unlock the cachetable
static inline void cachetable_unlock(CACHETABLE ct __attribute__((unused))) {
    toku_mutex_unlock(ct->mutex);
}

// Wait for cache table space to become available 
// size_current is number of bytes currently occupied by data (referred to by pairs)
// size_evicting is number of bytes queued up to be evicted
static inline void cachetable_wait_write(CACHETABLE ct) {
    // if we're writing more than half the data in the cachetable
    while (2*ct->size_evicting > ct->size_current) {
        toku_cond_wait(&ct->flow_control_cond, ct->mutex);
    }
}

static inline void cachetable_wakeup_write(CACHETABLE ct) {
    if (8*ct->size_evicting  <= ct->size_current) {
        toku_cond_broadcast(&ct->flow_control_cond);
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

    // If set and the cachefile closes, the file will be removed.
    // Clients must not operate on the cachefile after setting this,
    // nor attempt to open any cachefile with the same fname (dname)
    // until this cachefile has been fully closed and unlinked.
    bool unlink_on_close;
    int fd;       /* Bug: If a file is opened read-only, then it is stuck in read-only.  If it is opened read-write, then subsequent writers can write to it too. */
    CACHETABLE cachetable;
    struct fileid fileid;
    FILENUM filenum;
    char *fname_in_env; /* Used for logging */

    void *userdata;
    int (*log_fassociate_during_checkpoint)(CACHEFILE cf, void *userdata); // When starting a checkpoint we must log all open files.
    int (*log_suppress_rollback_during_checkpoint)(CACHEFILE cf, void *userdata); // When starting a checkpoint we must log which files need rollbacks suppressed
    int (*close_userdata)(CACHEFILE cf, int fd, void *userdata, char **error_string, BOOL lsnvalid, LSN); // when closing the last reference to a cachefile, first call this function. 
    int (*begin_checkpoint_userdata)(LSN lsn_of_checkpoint, void *userdata); // before checkpointing cachefiles call this function.
    int (*checkpoint_userdata)(CACHEFILE cf, int fd, void *userdata); // when checkpointing a cachefile, call this function.
    int (*end_checkpoint_userdata)(CACHEFILE cf, int fd, void *userdata); // after checkpointing cachefiles call this function.
    int (*note_pin_by_checkpoint)(CACHEFILE cf, void *userdata); // add a reference to the userdata to prevent it from being removed from memory
    int (*note_unpin_by_checkpoint)(CACHEFILE cf, void *userdata); // add a reference to the userdata to prevent it from being removed from memory
    LSN most_recent_global_checkpoint_that_finished_early;
    LSN for_local_checkpoint;
    enum cachefile_checkpoint_state checkpoint_state;
    BACKGROUND_JOB_MANAGER bjm;
};

// FIXME global with no toku prefix
void remove_background_job_from_cf(CACHEFILE cf)
{
    bjm_remove_background_job(cf->bjm);
}

// FIXME global with no toku prefix
void cachefile_kibbutz_enq (CACHEFILE cf, void (*f)(void*), void *extra)
// The function f must call remove_background_job_from_cf when it completes
{
    int r = bjm_add_background_job(cf->bjm);
    // if client should is adding a background job, then it must be done
    // at a time when the manager is accepting background jobs, otherwise
    // the client is screwing up
    assert_zero(r); 
    toku_kibbutz_enq(cf->cachetable->kibbutz, f, extra);
}

static int
checkpoint_thread (void *cachetable_v)
// Effect:  If checkpoint_period>0 thn periodically run a checkpoint.
//  If someone changes the checkpoint_period (calling toku_set_checkpoint_period), then the checkpoint will run sooner or later.
//  If someone sets the checkpoint_shutdown boolean , then this thread exits. 
// This thread notices those changes by waiting on a condition variable.
{
    CACHETABLE ct = (CACHETABLE) cachetable_v;
    int r = toku_checkpoint(ct, ct->logger, NULL, NULL, NULL, NULL, SCHEDULED_CHECKPOINT);
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

int toku_create_cachetable(CACHETABLE *result, long size_limit, LSN UU(initial_lsn), TOKULOGGER logger) {
    if (size_limit == 0) {
        size_limit = 128*1024*1024;
    }
    CACHETABLE MALLOC(ct);
    if (ct == 0) return ENOMEM;
    memset(ct, 0, sizeof(*ct));
    HELGRIND_VALGRIND_HG_DISABLE_CHECKING(&ct->size_nonleaf, sizeof ct->size_nonleaf); // modified only when the cachetable lock is held, but read by engine status
    HELGRIND_VALGRIND_HG_DISABLE_CHECKING(&ct->size_current, sizeof ct->size_current);
    HELGRIND_VALGRIND_HG_DISABLE_CHECKING(&ct->size_evicting, sizeof ct->size_evicting);
    HELGRIND_VALGRIND_HG_DISABLE_CHECKING(&ct->size_leaf, sizeof ct->size_leaf);
    HELGRIND_VALGRIND_HG_DISABLE_CHECKING(&ct->size_rollback, sizeof ct->size_rollback);
    HELGRIND_VALGRIND_HG_DISABLE_CHECKING(&ct->size_cachepressure, sizeof ct->size_cachepressure);
    ct->table_size = 4;
    rwlock_init(&ct->pending_lock);
    XCALLOC_N(ct->table_size, ct->table);
    ct->size_limit = size_limit;
    ct->size_reserved = unreservable_memory(size_limit);
    ct->logger = logger;
    toku_init_workers(&ct->wq, &ct->threadpool, 1);
    toku_init_workers(&ct->checkpoint_wq, &ct->checkpoint_threadpool, 8);
    ct->mutex = workqueue_lock_ref(&ct->wq);

    ct->kibbutz = toku_kibbutz_create(toku_os_get_number_active_processors());

    toku_minicron_setup(&ct->checkpointer, 0, checkpoint_thread, ct); // default is no checkpointing
    toku_minicron_setup(&ct->cleaner, 0, toku_cleaner_thread, ct); // default is no cleaner, for now
    ct->cleaner_iterations = 1; // default is one iteration
    ct->env_dir = toku_xstrdup(".");
    bjm_init(&ct->checkpoint_clones_bjm);
    toku_cond_init(&ct->flow_control_cond, NULL);
    *result = ct;
    return 0;
}

u_int64_t toku_cachetable_reserve_memory(CACHETABLE ct, double fraction) {
    cachetable_lock(ct);
    cachetable_wait_write(ct);
    uint64_t reserved_memory = fraction*(ct->size_limit-ct->size_reserved);
    ct->size_reserved += reserved_memory;
    maybe_flush_some(ct, reserved_memory);    
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
    toku_free(ct->env_dir);
    ct->env_dir = toku_xstrdup(env_dir);
}

// What cachefile goes with particular iname (iname relative to env)?
// The transaction that is adding the reference might not have a reference
// to the brt, therefore the cachefile might be closing.
// If closing, we want to return that it is not there, but must wait till after
// the close has finished.
// Once the close has finished, there must not be a cachefile with that name
// in the cachetable.
int toku_cachefile_of_iname_in_env (CACHETABLE ct, const char *iname_in_env, CACHEFILE *cf) {
    cachetable_lock(ct);
    CACHEFILE extant;
    int r;
    r = ENOENT;
    for (extant = ct->cachefiles; extant; extant=extant->next) {
        if (extant->fname_in_env &&
            !strcmp(extant->fname_in_env, iname_in_env)) {
            *cf = extant;
            r = 0;
            break;
        }
    }
    cachetable_unlock(ct);
    return r;
}

// What cachefile goes with particular fd?
// This function can only be called if the brt is still open, so file must 
// still be open
int toku_cachefile_of_filenum (CACHETABLE ct, FILENUM filenum, CACHEFILE *cf) {
    cachetable_lock(ct);
    CACHEFILE extant;
    int r = ENOENT;
    *cf = NULL;
    for (extant = ct->cachefiles; extant; extant=extant->next) {
        if (extant->filenum.fileid==filenum.fileid) {
            *cf = extant;
            r = 0;
            break;
        }
    }
    cachetable_unlock(ct);
    return r;
}

static FILENUM next_filenum_to_use={0};

static void cachefile_init_filenum(CACHEFILE cf, int fd, const char *fname_in_env, struct fileid fileid) {
    cf->fd = fd;
    cf->fileid = fileid;
    cf->fname_in_env = toku_xstrdup(fname_in_env);
}

// TEST-ONLY function
// If something goes wrong, close the fd.  After this, the caller shouldn't close the fd, but instead should close the cachefile.
int toku_cachetable_openfd (CACHEFILE *cfptr, CACHETABLE ct, int fd, const char *fname_in_env) {
    FILENUM filenum = toku_cachetable_reserve_filenum(ct);
    return toku_cachetable_openfd_with_filenum(cfptr, ct, fd, fname_in_env, filenum);
}

// Get a unique filenum from the cachetable
FILENUM
toku_cachetable_reserve_filenum(CACHETABLE ct) {
    CACHEFILE extant;
    FILENUM filenum;
    invariant(ct);
    cachetable_lock(ct);
try_again:
    for (extant = ct->cachefiles; extant; extant=extant->next) {
        if (next_filenum_to_use.fileid==extant->filenum.fileid) {
            next_filenum_to_use.fileid++;
            goto try_again;
        }
    }
    filenum = next_filenum_to_use;
    next_filenum_to_use.fileid++;
    cachetable_unlock(ct);
    return filenum;
}

int toku_cachetable_openfd_with_filenum (CACHEFILE *cfptr, CACHETABLE ct, int fd, 
                                         const char *fname_in_env,
                                         FILENUM filenum) {
    int r;
    CACHEFILE extant;
    struct fileid fileid;
    
    assert(filenum.fileid != FILENUM_NONE.fileid);
    r = toku_os_get_unique_file_id(fd, &fileid);
    if (r != 0) { 
        r=get_error_errno(); close(fd); // no change for t:2444
        return r;
    }
    cachetable_lock(ct);
    for (extant = ct->cachefiles; extant; extant=extant->next) {
        if (memcmp(&extant->fileid, &fileid, sizeof(fileid))==0) {
            // Clients must serialize cachefile open, close, and unlink
            // So, during open, we should never see a closing cachefile 
            // or one that has been marked as unlink on close.
            assert(!extant->unlink_on_close);

            // Reuse an existing cachefile and close the caller's fd, whose
            // responsibility has been passed to us.
            r = close(fd);
            assert(r == 0);
            *cfptr = extant;
            r = 0;
            goto exit;
        }
    }

    // assert that the filenum is not in use
    for (extant = ct->cachefiles; extant; extant=extant->next) {
        invariant(extant->filenum.fileid != filenum.fileid);
    }

    //File is not open.  Make a new cachefile.
    {
        // create a new cachefile entry in the cachetable
        CACHEFILE XCALLOC(newcf);
        newcf->cachetable = ct;
        newcf->filenum = filenum;
        cachefile_init_filenum(newcf, fd, fname_in_env, fileid);
        newcf->next = ct->cachefiles;
        ct->cachefiles = newcf;

        newcf->most_recent_global_checkpoint_that_finished_early = ZERO_LSN;
        newcf->for_local_checkpoint = ZERO_LSN;
        newcf->checkpoint_state = CS_NOT_IN_PROGRESS;

        bjm_init(&newcf->bjm);
        toku_list_init(&newcf->pairs_for_cachefile);
        *cfptr = newcf;
        r = 0;
    }
 exit:
    cachetable_unlock(ct);
    return r;
}

static void cachetable_flush_cachefile (CACHETABLE, CACHEFILE cf);

//TEST_ONLY_FUNCTION
int toku_cachetable_openf (CACHEFILE *cfptr, CACHETABLE ct, const char *fname_in_env, int flags, mode_t mode) {
    char *fname_in_cwd = toku_construct_full_name(2, ct->env_dir, fname_in_env);
    int fd = open(fname_in_cwd, flags+O_BINARY, mode);
    int r;
    if (fd<0) r = get_error_errno();
    else      r = toku_cachetable_openfd (cfptr, ct, fd, fname_in_env);
    toku_free(fname_in_cwd);
    return r;
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
    r=toku_os_get_unique_file_id(fd, &fileid);
    if (r != 0) { 
        r=get_error_errno(); close(fd); goto cleanup; // no change for t:2444
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
    //It is safe to have the name repeated since this is a ft-only test function.
    //There isn't an environment directory so its both env/cwd.
    cachefile_init_filenum(cf, fd, fname_in_env, fileid);
    r = 0;
cleanup:
    return r;
}

char *
toku_cachefile_fname_in_env (CACHEFILE cf) {
    return cf->fname_in_env;
}

int 
toku_cachefile_get_fd (CACHEFILE cf) {
    return cf->fd;
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
    ct->cachefiles = remove_cf_from_list_locked(cf, ct->cachefiles);
}

int 
toku_cachefile_close(CACHEFILE *cfp, char **error_string, BOOL oplsn_valid, LSN oplsn) {
    int r, close_error = 0;
    CACHEFILE cf = *cfp;
    CACHETABLE ct = cf->cachetable;

    bjm_wait_for_jobs_to_finish(cf->bjm);
    
    // Hold the cachetable lock while we check some invariants and
    // flush the cachefile.
    cachetable_lock(ct);

    // Clients should never attempt to close a cachefile that is being
    // checkpointed. We notify clients this is happening in the
    // note_pin_by_checkpoint callback.
    assert(!cf->next_in_checkpoint);
    assert(!cf->for_checkpoint);

    // Flush the cachefile and remove all of its pairs from the cachetable
    cachetable_flush_cachefile(ct, cf);
    assert(toku_list_empty(&cf->pairs_for_cachefile));

    // Call the close userdata callback to notify the client this cachefile
    // and its underlying file are going to be closed
    if (cf->close_userdata) {
        close_error = cf->close_userdata(cf, cf->fd, cf->userdata, error_string, oplsn_valid, oplsn);
    }

    remove_cf_from_cachefiles_list(cf);
    bjm_destroy(cf->bjm);
    cf->bjm = NULL;

    // Don't hold the cachetable lock during fsync/close/unlink, etc
    cachetable_unlock(ct);

    // fsync and close the fd. 
    r = toku_file_fsync_without_accounting(cf->fd);
    assert(r == 0);   
    r = close(cf->fd);
    assert(r == 0);

    // Unlink the file if the bit was set
    if (cf->unlink_on_close) {
        char *fname_in_cwd = toku_cachetable_get_fname_in_cwd(cf->cachetable, cf->fname_in_env);
        r = unlink(fname_in_cwd);
        assert_zero(r);
        toku_free(fname_in_cwd);
    }
    toku_free(cf->fname_in_env);
    toku_free(cf);

    // If close userdata returned nonzero, pass that error code to the caller
    if (close_error != 0) {
        r = close_error;
    }
    return r;
}

//
// This client calls this function to flush all PAIRs belonging to
// a cachefile from the cachetable. The client must ensure that
// while this function is called, no other thread does work on the 
// cachefile.
//
int toku_cachefile_flush (CACHEFILE cf) {
    bjm_wait_for_jobs_to_finish(cf->bjm);
    CACHETABLE ct = cf->cachetable;
    cachetable_lock(ct);
    cachetable_flush_cachefile(ct, cf);
    cachetable_unlock(ct);
    return 0;
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

// has ct locked on entry
// This function MUST NOT release and reacquire the cachetable lock
// Its callers (toku_cachetable_put_with_dep_pairs) depend on this behavior.
static void cachetable_rehash (CACHETABLE ct, u_int32_t newtable_size) {
    // printf("rehash %p %d %d %d\n", t, primeindexdelta, ct->n_in_table, ct->table_size);

    assert(newtable_size>=4 && ((newtable_size & (newtable_size-1))==0));
    PAIR *XCALLOC_N(newtable_size, newtable);
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
    assert(attr.is_valid);
    ct->size_current -= attr.size;
    ct->size_nonleaf -= attr.nonleaf_size;
    ct->size_leaf -= attr.leaf_size;
    ct->size_rollback -= attr.rollback_size;
    ct->size_cachepressure -= attr.cache_pressure_size;
    assert(ct->size_current >= 0);
}

static void
cachetable_add_pair_attr(CACHETABLE ct, PAIR_ATTR attr) {
    assert(attr.is_valid);
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
}

static void cachetable_free_pair(CACHETABLE ct, PAIR p) {
    // helgrind
    CACHETABLE_FLUSH_CALLBACK flush_callback = p->flush_callback;
    CACHEKEY key = p->key;
    void *value = p->value_data;
    void* disk_data = p->disk_data;
    void *write_extraargs = p->write_extraargs;
    PAIR_ATTR old_attr = p->attr;
    
    cachetable_evictions++;
    cachetable_unlock(ct);
    PAIR_ATTR new_attr = p->attr;
    // Note that flush_callback is called with write_me FALSE, so the only purpose of this 
    // call is to tell the brt layer to evict the node (keep_me is FALSE).
    // Also, because we have already removed the PAIR from the cachetable in 
    // cachetable_remove_pair, we cannot pass in p->cachefile and p->cachefile->fd
    // for the first two parameters, as these may be invalid (#5171), so, we
    // pass in NULL and -1, dummy values
    flush_callback(NULL, -1, key, value, &disk_data, write_extraargs, old_attr, &new_attr, FALSE, FALSE, TRUE, FALSE);
    
    cachetable_lock(ct);
    
    ctpair_destroy(p);
}

// Maybe remove a pair from the cachetable and free it, depending on whether
// or not there are any threads interested in the pair.  The flush callback
// is called with write_me and keep_me both false, and the pair is destroyed.
// The sole purpose of this function is to remove the node, so the write_me 
// argument to the flush callback is false, and the flush callback won't do
// anything except destroy the node.
static void cachetable_maybe_remove_and_free_pair (CACHETABLE ct, PAIR p) {
    if (nb_mutex_users(&p->value_nb_mutex) == 0) {
        // assumption is that if we are about to remove the pair
        // that no one has grabbed the disk_nb_mutex,
        // and that there is no cloned_value_data, because
        // no one is writing a cloned value out.
        assert(nb_mutex_users(&p->disk_nb_mutex) == 0);
        assert(p->cloned_value_data == NULL);
        cachetable_remove_pair(ct, p);
        cachetable_free_pair(ct, p);
    }
}

// assumes value_nb_mutex and disk_nb_mutex held on entry
// responsibility of this function is to only write a locked PAIR to disk
// and NOTHING else. We do not manipulate the state of the PAIR
// of the cachetable here (with the exception of ct->size_current for clones)
static void cachetable_only_write_locked_data(
    CACHETABLE ct, 
    PAIR p, 
    BOOL for_checkpoint,
    PAIR_ATTR* new_attr,
    BOOL is_clone
    ) 
{    
    CACHETABLE_FLUSH_CALLBACK flush_callback = p->flush_callback;
    CACHEFILE cachefile = p->cachefile;
    CACHEKEY key = p->key;
    void *value = is_clone ? p->cloned_value_data : p->value_data;
    void *disk_data = p->disk_data;
    void *write_extraargs = p->write_extraargs;
    PAIR_ATTR old_attr = p->attr;
    BOOL dowrite = TRUE;
    
    cachetable_unlock(ct);
    
    // write callback
    flush_callback(
        cachefile, 
        cachefile->fd, 
        key, 
        value, 
        &disk_data, 
        write_extraargs, 
        old_attr, 
        new_attr, 
        dowrite, 
        is_clone ? FALSE : TRUE, // keep_me (only keep if this is not cloned pointer)
        for_checkpoint, 
        is_clone //is_clone
        );
    p->disk_data = disk_data;
    cachetable_lock(ct);
    if (is_clone) {
        p->cloned_value_data = NULL;
        ct->size_current -= p->cloned_value_size;
        p->cloned_value_size = 0;
    }    
}


//
// This function writes a PAIR's value out to disk. Currently, it is called
// by get_and_pin functions that write a PAIR out for checkpoint, by 
// evictor threads that evict dirty PAIRS, and by the checkpoint thread
// that needs to write out a dirty node for checkpoint.
//
static void cachetable_write_locked_pair(CACHETABLE ct, PAIR p) {
    PAIR_ATTR old_attr = p->attr;
    PAIR_ATTR new_attr = p->attr;
    rwlock_read_lock(&ct->pending_lock, ct->mutex);
    BOOL for_checkpoint = p->checkpoint_pending;
    p->checkpoint_pending = FALSE;
    // grabbing the disk_nb_mutex here ensures that
    // after this point, no one is writing out a cloned value
    // if we grab the disk_nb_mutex inside the if clause,
    // then we may try to evict a PAIR that is in the process
    // of having its clone be written out
    nb_mutex_lock(&p->disk_nb_mutex, ct->mutex);
    // make sure that assumption about cloned_value_data is true
    // if we have grabbed the disk_nb_mutex, then that means that
    // there should be no cloned value data
    assert(p->cloned_value_data == NULL);
    if (p->dirty) {
        cachetable_only_write_locked_data(ct, p, for_checkpoint, &new_attr, FALSE);
        //
        // now let's update variables
        //
        if (new_attr.is_valid) {
            p->attr = new_attr;
            cachetable_change_pair_attr(ct, old_attr, new_attr);
        }
    }
    nb_mutex_unlock(&p->disk_nb_mutex);
    // the pair is no longer dirty once written
    p->dirty = CACHETABLE_CLEAN;
    
    assert(!p->checkpoint_pending);
    rwlock_read_unlock(&ct->pending_lock);
}

// complete the write of a pair by reseting the writing flag, and 
// maybe removing the pair from the cachetable if there are no
// references to it

static void cachetable_complete_write_pair (CACHETABLE ct, PAIR p) {
    nb_mutex_unlock(&p->value_nb_mutex);
    cachetable_maybe_remove_and_free_pair(ct, p);
}

// Write a pair to storage
// Effects: an exclusive lock on the pair is obtained, the write callback is called,
// the pair dirty state is adjusted, and the write is completed.  The keep_me
// boolean is true, so the pair is not yet evicted from the cachetable.
// Requires: This thread must hold the write lock for the pair.
static void cachetable_evict_pair(CACHETABLE ct, PAIR p) {
    long old_size = p->attr.size;
    // this function may change p->attr.size, so we saved
    // the estimate we must have put into ct->evicting_size above
    cachetable_write_locked_pair(ct, p);
    
    // maybe wakeup any stalled writers when the pending writes fall below 
    // 1/8 of the size of the cachetable
    ct->size_evicting -= old_size; 
    assert(ct->size_evicting  >= 0);
    cachetable_wakeup_write(ct);
    cachetable_complete_write_pair(ct, p);
}

// Worker thread function to writes and evicts  a pair from memory to its cachefile
static void cachetable_evicter(WORKITEM wi) {
    PAIR p = (PAIR) workitem_arg(wi);
    CACHEFILE cf = p->cachefile;
    CACHETABLE ct = cf->cachetable;
    cachetable_lock(ct);
    cachetable_evict_pair(ct, p);
    cachetable_unlock(ct);
    bjm_remove_background_job(cf->bjm);
}

// CT lock held on entry
// background job has been added for p->cachefile on entry
// responsibility of this function to make sure that background job is removed
static void try_evict_pair(CACHETABLE ct, PAIR p) {
    CACHEFILE cf = p->cachefile;
    // evictions without a write or unpinned pair's that are clean
    // can be run in the current thread

    // must check for before we grab the write lock because we may
    // be trying to evict something this thread is trying to read
    if (!nb_mutex_users(&p->value_nb_mutex)) {
        nb_mutex_lock(&p->value_nb_mutex, ct->mutex);

        assert(ct->size_evicting >= 0);
        ct->size_evicting += p->attr.size;
        assert(ct->size_evicting >= 0);

        // if the PAIR is dirty, the running eviction requires writing the 
        // PAIR out. if the disk_nb_mutex is grabbed, then running 
        // eviction requires waiting for the disk_nb_mutex to become available,
        // which may be expensive. Hence, if either is true, we 
        // do the eviction on a writer thread
        if (!p->dirty && (nb_mutex_writers(&p->disk_nb_mutex) == 0)) {
            cachetable_evict_pair(ct, p);
            bjm_remove_background_job(cf->bjm);
        }
        else {
            WORKITEM wi = &p->asyncwork;
            //responsibility of cachetable_evicter to remove background job
            workitem_init(wi, cachetable_evicter, p);
            workqueue_enq(&ct->wq, wi, 0);
        }
    }
}

static void do_partial_eviction(CACHETABLE ct, PAIR p) {
    PAIR_ATTR new_attr;
    PAIR_ATTR old_attr = p->attr;
    
    cachetable_unlock(ct);
    p->pe_callback(p->value_data, old_attr, &new_attr, p->write_extraargs);
    cachetable_lock(ct);

    cachetable_change_pair_attr(ct, old_attr, new_attr);
    p->attr = new_attr;

    assert(ct->size_evicting >= p->size_evicting_estimate);
    ct->size_evicting -= p->size_evicting_estimate;
    cachetable_wakeup_write(ct);
    nb_mutex_unlock(&p->value_nb_mutex);
}

static void cachetable_partial_eviction(WORKITEM wi) {
    PAIR p = (PAIR) workitem_arg(wi);
    CACHEFILE cf = p->cachefile;
    CACHETABLE ct = cf->cachetable;
    cachetable_lock(ct);
    do_partial_eviction(ct,p);
    cachetable_unlock(ct);
    bjm_remove_background_job(cf->bjm);
}

// cachetable lock held on entry
// run eviction on PAIR, may be partial eviction or full eviction
static bool run_eviction_on_pair(PAIR curr_in_clock, CACHETABLE ct) {
    bool ret_val = false;
    // function meant to be called on PAIR that is not being accessed right now
    assert(nb_mutex_users(&curr_in_clock->value_nb_mutex) == 0);
    assert(nb_mutex_users(&curr_in_clock->disk_nb_mutex) == 0);
    CACHEFILE cf = curr_in_clock->cachefile;
    int r = bjm_add_background_job(cf->bjm);
    if (r) {
        goto exit;
    }
    ret_val = true;
    if (curr_in_clock->count > 0) {
        curr_in_clock->count--;
        // call the partial eviction callback
        nb_mutex_lock(&curr_in_clock->value_nb_mutex, ct->mutex);
    
        void *value = curr_in_clock->value_data;
        void* disk_data = curr_in_clock->disk_data;
        void *write_extraargs = curr_in_clock->write_extraargs;
        enum partial_eviction_cost cost;
        long bytes_freed_estimate = 0;
        curr_in_clock->pe_est_callback(
            value, 
            disk_data,
            &bytes_freed_estimate, 
            &cost, 
            write_extraargs
            );
        if (cost == PE_CHEAP) {
            curr_in_clock->size_evicting_estimate = 0;
            do_partial_eviction(ct, curr_in_clock);
            bjm_remove_background_job(cf->bjm);
        }
        else if (cost == PE_EXPENSIVE) {
            // only bother running an expensive partial eviction
            // if it is expected to free space
            if (bytes_freed_estimate > 0) {
                curr_in_clock->size_evicting_estimate = bytes_freed_estimate;
                ct->size_evicting += bytes_freed_estimate;
                WORKITEM wi = &curr_in_clock->asyncwork;
                // responsibility of cachetable_partial_eviction to remove background job
                workitem_init(wi, cachetable_partial_eviction, curr_in_clock);
                workqueue_enq(&ct->wq, wi, 0);
            }
            else {
                nb_mutex_unlock(&curr_in_clock->value_nb_mutex);
                bjm_remove_background_job(cf->bjm);
            }
        }
        else {
            assert(FALSE);
        }        
    }
    else {
        // responsibility of try_evict_pair to eventually remove background job
        try_evict_pair(ct, curr_in_clock);
    }
exit:
    return ret_val;
}

static void maybe_flush_some (CACHETABLE ct, long size) {
    if (size + ct->size_current <= ct->size_limit + ct->size_evicting) return;

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
        if (set_val && 
            curr_in_clock->key.b == curr_cachekey.b &&
            curr_in_clock->cachefile->filenum.fileid == curr_filenum.fileid)
        {
            // we have identified a cycle where everything in the clock is in use
            // do not return an error
            // just let memory be overfull
            goto exit;
        }
        if (nb_mutex_users(&curr_in_clock->value_nb_mutex) || nb_mutex_users(&curr_in_clock->disk_nb_mutex)) {
            if (!set_val) {
                set_val = TRUE;
                curr_cachekey = ct->clock_head->key;
                curr_filenum = ct->clock_head->cachefile->filenum;
            }
        }
        else {
            bool eviction_run = run_eviction_on_pair(curr_in_clock, ct);
            if (eviction_run) {
                set_val = FALSE;
            }
            else if (!set_val) {
                set_val = TRUE;
                curr_cachekey = ct->clock_head->key;
                curr_filenum = ct->clock_head->cachefile->filenum;
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
    return;
}

void toku_cachetable_maybe_flush_some(CACHETABLE ct) {
    cachetable_lock(ct);
    maybe_flush_some(ct, 0);
    cachetable_unlock(ct);
}

// has ct locked on entry
// This function MUST NOT release and reacquire the cachetable lock
// Its callers (toku_cachetable_put_with_dep_pairs) depend on this behavior.
static PAIR cachetable_insert_at(CACHETABLE ct, 
                                 CACHEFILE cachefile, CACHEKEY key, void *value, 
                                 u_int32_t fullhash, 
                                 PAIR_ATTR attr,
                                 CACHETABLE_WRITE_CALLBACK write_callback,
                                 enum cachetable_dirty dirty) {
    PAIR MALLOC(p);
    assert(p);
    memset(p, 0, sizeof *p);
    p->cachefile = cachefile;
    p->key = key;
    p->value_data = value;
    p->cloned_value_data = NULL;
    p->cloned_value_size = 0;
    p->disk_data = NULL;
    p->fullhash = fullhash;
    p->dirty = dirty;
    p->attr = attr;
    p->flush_callback = write_callback.flush_callback;
    p->pe_callback = write_callback.pe_callback;
    p->pe_est_callback = write_callback.pe_est_callback;
    p->cleaner_callback = write_callback.cleaner_callback;
    p->clone_callback = write_callback.clone_callback;
    p->write_extraargs = write_callback.write_extraargs;
    p->fullhash = fullhash;
    p->clock_next = p->clock_prev = 0;
    nb_mutex_init(&p->value_nb_mutex);
    nb_mutex_init(&p->disk_nb_mutex);
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

// has ct locked on entry
// This function MUST NOT release and reacquire the cachetable lock
// Its callers (toku_cachetable_put_with_dep_pairs) depend on this behavior.
static int cachetable_put_internal(
    CACHEFILE cachefile, 
    CACHEKEY key, 
    u_int32_t fullhash, 
    void*value, 
    PAIR_ATTR attr,
    CACHETABLE_WRITE_CALLBACK write_callback
    )
{
    CACHETABLE ct = cachefile->cachetable;
    {
        PAIR p;
        for (p=ct->table[fullhash&(cachefile->cachetable->table_size-1)]; p; p=p->hash_chain) {
            if (p->key.b==key.b && p->cachefile==cachefile) {
                // Ideally, we would like to just assert(FALSE) here
                // and not return an error, but as of Dr. Noga,
                // cachetable-test2 depends on this behavior.
                // To replace the following with an assert(FALSE)
                // we need to change the behavior of cachetable-test2
                //
                // Semantically, these two asserts are not strictly right.  After all, when are two functions eq?
                // In practice, the functions better be the same.
                assert(p->flush_callback == write_callback.flush_callback);
                assert(p->pe_callback == write_callback.pe_callback);
                assert(p->cleaner_callback == write_callback.cleaner_callback);
                return -1; /* Already present, don't grab lock. */
            }
        }
    }
    // flushing could change the table size, but wont' change the fullhash
    cachetable_puts++;
    PAIR p = cachetable_insert_at(
        ct, 
        cachefile, 
        key, 
        value,
        fullhash, 
        attr, 
        write_callback,
        CACHETABLE_DIRTY
        );
    assert(p);
    nb_mutex_lock(&p->value_nb_mutex, ct->mutex);
    //note_hash_count(count);
    return 0;
}

// ct is locked on entry
// gets pair if exists, and that is all.
static int cachetable_get_pair (CACHEFILE cachefile, CACHEKEY key, u_int32_t fullhash, PAIR* pv) {
    CACHETABLE ct = cachefile->cachetable;
    PAIR p;
    int r = -1;
    for (p=ct->table[fullhash&(ct->table_size-1)]; p; p=p->hash_chain) {
        if (p->key.b==key.b && p->cachefile==cachefile) {
            *pv = p;
            r = 0;
            break;
        }
    }
    return r;
}

// ct locked on entry
static void
clone_pair(CACHETABLE ct, PAIR p) {
    PAIR_ATTR old_attr = p->attr;
    PAIR_ATTR new_attr;

    // act of cloning should be fast,
    // not sure if we have to release
    // and regrab the cachetable lock,
    // but doing it for now
    cachetable_unlock(ct);
    p->clone_callback(
        p->value_data,
        &p->cloned_value_data,
        &new_attr,
        TRUE,
        p->write_extraargs
        );
    cachetable_lock(ct);
    
    // now we need to do the same actions we would do
    // if the PAIR had been written to disk
    //
    // because we hold the value_nb_mutex,
    // it doesn't matter whether we clear 
    // the pending bit before the clone
    // or after the clone
    p->checkpoint_pending = FALSE;
    p->dirty = CACHETABLE_CLEAN;
    if (new_attr.is_valid) {
        p->attr = new_attr;
        cachetable_change_pair_attr(ct, old_attr, new_attr);
    }
    p->cloned_value_size = p->attr.size;
    ct->size_current += p->cloned_value_size;
}

static void checkpoint_cloned_pair(WORKITEM wi) {
    PAIR p = (PAIR) workitem_arg(wi);
    CACHETABLE ct = p->cachefile->cachetable;
    cachetable_lock(ct);
    PAIR_ATTR new_attr;
    // note that pending lock is not needed here because
    // we KNOW we are in the middle of a checkpoint
    // and that a begin_checkpoint cannot happen
    cachetable_only_write_locked_data(
        ct,
        p,
        TRUE, //for_checkpoint
        &new_attr,
        TRUE //is_clone
        );
    nb_mutex_unlock(&p->disk_nb_mutex);
    bjm_remove_background_job(ct->checkpoint_clones_bjm);
    cachetable_unlock(ct);
}

static void
checkpoint_cloned_pair_on_writer_thread(CACHETABLE ct, PAIR p) {
    WORKITEM wi = &p->checkpoint_asyncwork;
    workitem_init(wi, checkpoint_cloned_pair, p);
    workqueue_enq(&ct->checkpoint_wq, wi, 1);
}


//
// Given a PAIR p with the value_nb_mutex altready held, do the following:
//  - If the PAIR needs to be written out to disk for checkpoint:
//   - If the PAIR is cloneable, clone the PAIR and place the work
//      of writing the PAIR on a background thread.
//   - If the PAIR is not cloneable, write the PAIR to disk for checkpoint
//      on the current thread
//
static void
write_locked_pair_for_checkpoint(CACHETABLE ct, PAIR p)
{
    if (p->dirty && p->checkpoint_pending) {
        if (p->clone_callback) {
            nb_mutex_lock(&p->disk_nb_mutex, ct->mutex);
            assert(!p->cloned_value_data);
            clone_pair(ct, p);
            assert(p->cloned_value_data);
            // place it on the background thread and continue
            // responsibility of writer thread to release disk_nb_mutex
            int r = bjm_add_background_job(ct->checkpoint_clones_bjm);
            assert_zero(r);
            checkpoint_cloned_pair_on_writer_thread(ct, p);
            // possibly run eviction because act of cloning adds
            // to ct->size_current, we don't do it in 
            // write_pair_for_checkpoint_thread, because that clones at most
            // one node at any time, where as this may be called from many 
            // threads simultaneously
            maybe_flush_some(ct, 0);
        }
        else {
            // The pair is not cloneable, just write the pair to disk
            
            // we already have p->value_nb_mutex and we just do the write in our own thread.
            cachetable_write_locked_pair(ct, p); // keeps the PAIR's write lock
        }
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

// On entry: hold the ct lock
// On exit:  the node is written out
// Method:   take write lock
//           maybe write out the node
//           Else release write lock
static void
write_pair_for_checkpoint_thread (CACHETABLE ct, PAIR p)
{
    nb_mutex_lock(&p->value_nb_mutex, ct->mutex); // grab an exclusive lock on the pair    
    if (p->dirty && p->checkpoint_pending) {
        if (p->clone_callback) {
            nb_mutex_lock(&p->disk_nb_mutex, ct->mutex);
            assert(!p->cloned_value_data);
            clone_pair(ct, p);
            assert(p->cloned_value_data);
        }
        else {
            // The pair is not cloneable, just write the pair to disk            
            // we already have p->value_nb_mutex and we just do the write in our own thread.
            // this will grab and release disk_nb_mutex
            cachetable_write_locked_pair(ct, p); // keeps the PAIR's write lock
        }
        
        // now release value_nb_mutex, before we write the PAIR out
        // so that the PAIR is available to client threads
        nb_mutex_unlock(&p->value_nb_mutex); // didn't call cachetable_evict_pair so we have to unlock it ourselves.
        if (p->clone_callback) {
            // note that pending lock is not needed here because
            // we KNOW we are in the middle of a checkpoint
            // and that a begin_checkpoint cannot happen
            PAIR_ATTR attr;
            cachetable_only_write_locked_data(
                ct,
                p,
                TRUE, //for_checkpoint
                &attr,
                TRUE //is_clone
                );
            nb_mutex_unlock(&p->disk_nb_mutex);
        }
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
        nb_mutex_unlock(&p->value_nb_mutex);
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
         assert(nb_mutex_writers(&curr_dep_pair->value_nb_mutex));
         // we need to update the dirtyness of the dependent pair,
         // because the client may have dirtied it while holding its lock,
         // and if the pair is pending a checkpoint, it needs to be written out
         if (dependent_dirty[i]) curr_dep_pair->dirty = CACHETABLE_DIRTY;
         if (curr_dep_pair->checkpoint_pending) {
             write_locked_pair_for_checkpoint(ct, curr_dep_pair);
         }
     }
}

int toku_cachetable_put_with_dep_pairs(
    CACHEFILE cachefile, 
    CACHETABLE_GET_KEY_AND_FULLHASH get_key_and_fullhash,
    void*value, 
    PAIR_ATTR attr,
    CACHETABLE_WRITE_CALLBACK write_callback,
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
    //
    // we call maybe_flush_some outside of cachetable_put_internal
    // because maybe_flush_some may release the cachetable lock
    // and we require what comes below to not do so.
    // we require get_key_and_fullhash and cachetable_put_internal
    // to not release the cachetable lock, and we require the critical
    // region described below to not begin a checkpoint. The cachetable lock
    // is used to ensure that a checkpoint is not begun during 
    // cachetable_put_internal
    // 
    maybe_flush_some(ct, attr.size);
    int rval;
    {
        BEGIN_CRITICAL_REGION;   // checkpoint may not begin inside critical region, detect and crash if one begins

        get_key_and_fullhash(key, fullhash, get_key_and_fullhash_extra);
        rval = cachetable_put_internal(
                                       cachefile,
                                       *key,
                                       *fullhash,
                                       value,
                                       attr,
                                       write_callback
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

        END_CRITICAL_REGION;    // checkpoint after this point would no longer cause a threadsafety bug
    }
    cachetable_unlock(ct);
    return rval;
}


int toku_cachetable_put(CACHEFILE cachefile, CACHEKEY key, u_int32_t fullhash, void*value, PAIR_ATTR attr,
                        CACHETABLE_WRITE_CALLBACK write_callback
                        ) {
    CACHETABLE ct = cachefile->cachetable;
    cachetable_lock(ct);
    cachetable_wait_write(ct);
    maybe_flush_some(ct, attr.size);
    int r = cachetable_put_internal(
        cachefile,
        key,
        fullhash,
        value,
        attr,
        write_callback
        );
    cachetable_unlock(ct);
    return r;
}

static uint64_t get_tnow(void) {
    struct timeval tv;
    int r = gettimeofday(&tv, NULL); assert(r == 0);
    return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

//
// cachetable lock and PAIR lock are held on entry
// On exit, cachetable lock is still held, but PAIR lock
// is either released.
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

    nb_mutex_lock(&p->disk_nb_mutex, ct->mutex);
    cachetable_unlock(ct);
    int r = pf_callback(p->value_data, p->disk_data, read_extraargs, cachefile->fd, &new_attr);
    lazy_assert_zero(r);
    cachetable_lock(ct);
    p->attr = new_attr;
    cachetable_change_pair_attr(ct, old_attr, new_attr);
    nb_mutex_unlock(&p->disk_nb_mutex);
    if (!keep_pair_locked) {
        nb_mutex_unlock(&p->value_nb_mutex);
    }
}

void toku_cachetable_pf_pinned_pair(
    void* value,
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback,
    void* read_extraargs,
    CACHEFILE cf,
    CACHEKEY key,
    u_int32_t fullhash
    ) 
{
    PAIR_ATTR attr;
    PAIR p = NULL;
    cachetable_lock(cf->cachetable);
    int r =  cachetable_get_pair(cf, key, fullhash, &p);
    assert_zero(r);
    assert(p->value_data == value);
    assert(nb_mutex_writers(&p->value_nb_mutex));
    nb_mutex_lock(&p->disk_nb_mutex, cf->cachetable->mutex);    
    int fd = cf->fd;
    cachetable_unlock(cf->cachetable);
    pf_callback(value, p->disk_data, read_extraargs, fd, &attr);
    cachetable_lock(cf->cachetable);
    nb_mutex_unlock(&p->disk_nb_mutex);    
    cachetable_unlock(cf->cachetable);
}


int toku_cachetable_get_and_pin (
    CACHEFILE cachefile, 
    CACHEKEY key, 
    u_int32_t fullhash, 
    void**value, 
    long *sizep,
    CACHETABLE_WRITE_CALLBACK write_callback,
    CACHETABLE_FETCH_CALLBACK fetch_callback, 
    CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK pf_req_callback,
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback,
    BOOL may_modify_value,
    void* read_extraargs // parameter for fetch_callback, pf_req_callback, and pf_callback
    ) 
{
    // We have separate parameters of read_extraargs and write_extraargs because
    // the lifetime of the two parameters are different. write_extraargs may be used
    // long after this function call (e.g. after a flush to disk), whereas read_extraargs
    // will not be used after this function returns. As a result, the caller may allocate
    // read_extraargs on the stack, whereas write_extraargs must be allocated
    // on the heap.
    return toku_cachetable_get_and_pin_with_dep_pairs (
        cachefile, 
        key, 
        fullhash, 
        value, 
        sizep,
        write_callback,
        fetch_callback, 
        pf_req_callback,
        pf_callback,
        may_modify_value,
        read_extraargs,
        0, // number of dependent pairs that we may need to checkpoint
        NULL, // array of cachefiles of dependent pairs
        NULL, // array of cachekeys of dependent pairs
        NULL, //array of fullhashes of dependent pairs
        NULL // array stating dirty/cleanness of dependent pairs
        );
}

// Read a pair from a cachefile into memory using the pair's fetch callback
static void cachetable_fetch_pair(
    CACHETABLE ct, 
    CACHEFILE cf, 
    PAIR p, 
    CACHETABLE_FETCH_CALLBACK fetch_callback, 
    void* read_extraargs,
    BOOL keep_pair_locked
    ) 
{
    // helgrind
    CACHEKEY key = p->key;
    u_int32_t fullhash = p->fullhash;

    void *toku_value = NULL;
    void *disk_data = NULL;
    PAIR_ATTR attr;
    
    // FIXME this should be enum cachetable_dirty, right?
    int dirty = 0;

    nb_mutex_lock(&p->disk_nb_mutex, ct->mutex);
    cachetable_unlock(ct);

    int r;
    r = fetch_callback(cf, cf->fd, key, fullhash, &toku_value, &disk_data, &attr, &dirty, read_extraargs);
    if (dirty)
        p->dirty = CACHETABLE_DIRTY;

    cachetable_lock(ct);
    // ft-ops.c asserts that get_and_pin succeeds,
    // so we might as well just assert it here as opposed
    // to trying to support an INVALID state
    assert(r == 0);

    p->value_data = toku_value;
    p->disk_data = disk_data;
    p->attr = attr;
    cachetable_add_pair_attr(ct, attr);
    nb_mutex_unlock(&p->disk_nb_mutex);
    if (!keep_pair_locked) {
        nb_mutex_unlock(&p->value_nb_mutex);
    }
    if (0) printf("%s:%d %" PRId64 " complete\n", __FUNCTION__, __LINE__, key.b);
}

static BOOL resolve_checkpointing_fast(PAIR p) {
    return !(p->checkpoint_pending && (p->dirty == CACHETABLE_DIRTY) && !p->clone_callback);
}
static void checkpoint_pair_and_dependent_pairs(
    CACHETABLE ct,
    PAIR p,
    u_int32_t num_dependent_pairs, // number of dependent pairs that we may need to checkpoint
    CACHEFILE* dependent_cfs, // array of cachefiles of dependent pairs
    CACHEKEY* dependent_keys, // array of cachekeys of dependent pairs
    u_int32_t* dependent_fullhash, //array of fullhashes of dependent pairs
    enum cachetable_dirty* dependent_dirty // array stating dirty/cleanness of dependent pairs
    )
{
    //BEGIN_CRITICAL_REGION;   // checkpoint may not begin inside critical region, detect and crash if one begins
    
    //
    // A checkpoint must not begin while we are checking dependent pairs or pending bits. 
    // Here is why.
    //
    // Now that we have all of the locks on the pairs we 
    // care about, we can take care of the necessary checkpointing.
    // For each pair, we simply need to write the pair if it is 
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
    
    //END_CRITICAL_REGION;    // checkpoint after this point would no longer cause a threadsafety bug
}

int toku_cachetable_get_and_pin_with_dep_pairs (
    CACHEFILE cachefile, 
    CACHEKEY key, 
    u_int32_t fullhash, 
    void**value, 
    long *sizep,
    CACHETABLE_WRITE_CALLBACK write_callback,
    CACHETABLE_FETCH_CALLBACK fetch_callback, 
    CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK pf_req_callback,
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback,
    BOOL may_modify_value,
    void* read_extraargs, // parameter for fetch_callback, pf_req_callback, and pf_callback
    u_int32_t num_dependent_pairs, // number of dependent pairs that we may need to checkpoint
    CACHEFILE* dependent_cfs, // array of cachefiles of dependent pairs
    CACHEKEY* dependent_keys, // array of cachekeys of dependent pairs
    u_int32_t* dependent_fullhash, //array of fullhashes of dependent pairs
    enum cachetable_dirty* dependent_dirty // array stating dirty/cleanness of dependent pairs
    ) 
{
    CACHETABLE ct = cachefile->cachetable;
    PAIR p;
    cachetable_lock(ct);
    
    cachetable_wait_write(ct);
    for (p=ct->table[fullhash&(ct->table_size-1)]; p; p=p->hash_chain) {
        if (p->key.b==key.b && p->cachefile==cachefile) {
            // still have the cachetable lock
            nb_mutex_lock(&p->value_nb_mutex, ct->mutex);
            pair_touch(p);
            if (may_modify_value) {
                checkpoint_pair_and_dependent_pairs(
                    ct,
                    p,
                    num_dependent_pairs,
                    dependent_cfs,
                    dependent_keys,
                    dependent_fullhash,
                    dependent_dirty
                    );
            }
            cachetable_unlock(ct);
            
            BOOL partial_fetch_required = pf_req_callback(p->value_data,read_extraargs);
            // shortcutting a path to getting the user the data
            // helps scalability for in-memory workloads
            if (!partial_fetch_required) {
                *value = p->value_data;
                if (sizep) *sizep = p->attr.size;
                return 0;
            }
            cachetable_lock(ct);
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

                do_partial_fetch(ct, cachefile, p, pf_callback, read_extraargs, TRUE);
            }
            goto got_value;
        }
    }
    // Note.  hashit(t,key) may have changed as a result of flushing.  But fullhash won't have changed.
    // The pair was not found, we must retrieve it from disk 
    {
        // insert a PAIR into the cachetable
        p = cachetable_insert_at(
            ct, 
            cachefile, 
            key, 
            zero_value, 
            fullhash, 
            zero_attr, 
            write_callback,
            CACHETABLE_CLEAN
            );
        assert(p);
        nb_mutex_lock(&p->value_nb_mutex, ct->mutex);
        if (may_modify_value) {
            checkpoint_pair_and_dependent_pairs(
                ct,
                p,
                num_dependent_pairs,
                dependent_cfs,
                dependent_keys,
                dependent_fullhash,
                dependent_dirty
                );
        }
        uint64_t t0 = get_tnow();

        // Retrieve the value of the PAIR from disk.
        // The pair being fetched will be marked as pending if a checkpoint happens during the
        // fetch because begin_checkpoint will mark as pending any pair that is locked even if it is clean.        
        cachetable_fetch_pair(ct, cachefile, p, fetch_callback, read_extraargs, TRUE);
        cachetable_miss++;
        cachetable_misstime += get_tnow() - t0;
        goto got_value;
    }
got_value:
    *value = p->value_data;
    if (sizep) *sizep = p->attr.size;
    maybe_flush_some(ct, 0);
    cachetable_unlock(ct);
    return 0;
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
    int r = -1;
    cachetable_lock(ct);
    for (p=ct->table[fullhash&(ct->table_size-1)]; p; p=p->hash_chain) {
        if (p->key.b==key.b && p->cachefile==cachefile) {
            if (!p->checkpoint_pending &&  //If checkpoint pending, we would need to first write it, which would make it clean
                p->dirty &&
                nb_mutex_users(&p->value_nb_mutex) == 0
            ) {
                // because nb_mutex_users is 0, this is fast
                nb_mutex_lock(&p->value_nb_mutex, ct->mutex);                
                *value = p->value_data;
                pair_touch(p);
                r = 0;
            }
            break;
        }
    }
    cachetable_unlock(ct);
    return r;
}

//Used by flusher threads to possibly pin child on client thread if pinning is cheap
//Same as toku_cachetable_maybe_get_and_pin except that we don't care if the node is clean or dirty (return the node regardless).
//All other conditions remain the same.
int toku_cachetable_maybe_get_and_pin_clean (CACHEFILE cachefile, CACHEKEY key, u_int32_t fullhash, void**value) {
    CACHETABLE ct = cachefile->cachetable;
    PAIR p;
    int r = -1;
    cachetable_lock(ct);
    for (p=ct->table[fullhash&(ct->table_size-1)]; p; p=p->hash_chain) {
        if (p->key.b==key.b && p->cachefile==cachefile) {
            if (!p->checkpoint_pending &&  //If checkpoint pending, we would need to first write it, which would make it clean (if the pin would be used for writes.  If would be used for read-only we could return it, but that would increase complexity)
                nb_mutex_users(&p->value_nb_mutex) == 0
            ) {
                // because nb_mutex_users is 0, this is fast
                nb_mutex_lock(&p->value_nb_mutex, ct->mutex);                
                *value = p->value_data;
                r = 0;
            }
            break;
        }
    }
    cachetable_unlock(ct);
    return r;
}

static int
cachetable_unpin_internal(CACHEFILE cachefile, CACHEKEY key, u_int32_t fullhash, enum cachetable_dirty dirty, PAIR_ATTR attr, BOOL have_ct_lock, BOOL flush)
// size==0 means that the size didn't change.
{
    CACHETABLE ct = cachefile->cachetable;
    //printf("%s:%d is dirty now=%d\n", __FILE__, __LINE__, dirty);
    int r = -1;
    //assert(fullhash == toku_cachetable_hash(cachefile, key));
    if (!have_ct_lock) cachetable_lock(ct);
    for (PAIR p=ct->table[fullhash&(ct->table_size-1)]; p; p=p->hash_chain) {
        if (p->key.b==key.b && p->cachefile==cachefile) {
            assert(nb_mutex_writers(&p->value_nb_mutex)>0);
            nb_mutex_unlock(&p->value_nb_mutex);
            if (dirty) p->dirty = CACHETABLE_DIRTY;
            if (attr.is_valid) {
                PAIR_ATTR old_attr = p->attr;
                PAIR_ATTR new_attr = attr;
                cachetable_change_pair_attr(ct, old_attr, new_attr);
                p->attr = attr;
            }
            if (flush) {
                maybe_flush_some(ct, 0);
            }
            r = 0; // we found one
            break;
        }
    }
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
    long* UU(sizep),
    CACHETABLE_WRITE_CALLBACK write_callback,
    CACHETABLE_FETCH_CALLBACK fetch_callback, 
    CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK pf_req_callback,
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback,
    BOOL may_modify_value,
    void *read_extraargs,
    UNLOCKERS unlockers
    )
// Effect:  If the block is in the cachetable, then pin it and return it. 
//   Otherwise fetch the data (but don't pin it, since we'll just end up pinning it again later), and return TOKUDB_TRY_AGAIN.
{
    CACHETABLE ct = cf->cachetable;
    cachetable_lock(ct);
    // 
    // Even though there is a risk that cachetable_wait_write may wait on a bunch
    // of I/O to complete, we call this because if we 
    // are in this situation where a lot of data is being evicted on writer threads
    // then we are in a screw case anyway.
    //
    cachetable_wait_write(ct);
    PAIR p;
    for (p = ct->table[fullhash&(ct->table_size-1)]; p; p = p->hash_chain) {
        if (p->key.b==key.b && p->cachefile==cf) {
            //
            // In Doofenshmirtz, we keep the root to leaf path pinned
            // as we perform a query on a dictionary at any given time.
            // This implies that only ONE query client can ever be
            // in get_and_pin_nonblocking for this dictionary. 
            // So, if there is a write lock grabbed
            // on the PAIR that we want to lock, then some expensive operation 
            // MUST be happening (read from disk, write to disk, flush, etc...), 
            // and we should run the unlockers.
            // Otherwise, if there is no write lock grabbed, we know there will 
            // be no stall, so we grab the lock and return to the user
            //
            if (!nb_mutex_writers(&p->value_nb_mutex) && 
                (!may_modify_value || resolve_checkpointing_fast(p))) 
            {
                nb_mutex_lock(&p->value_nb_mutex, ct->mutex);
                if (may_modify_value && p->checkpoint_pending) {
                    write_locked_pair_for_checkpoint(ct, p);
                }
                pair_touch(p);
                // release the cachetable lock before calling pf_req_callback
                // helps scalability for in-memory workloads by not holding the cachetable lock
                // when calling pf_req_callback, and if possible, returns the PAIR to the user without
                // reacquiring the cachetable lock
                cachetable_unlock(ct);
                BOOL partial_fetch_required = pf_req_callback(p->value_data,read_extraargs);
                //
                // Just because the PAIR exists does necessarily mean the all the data the caller requires
                // is in memory. A partial fetch may be required, which is evaluated above
                // if the variable is true, a partial fetch is required so we must grab the PAIR's write lock
                // and then call a callback to retrieve what we need
                //
                if (partial_fetch_required) {
                    cachetable_lock(ct);
                    run_unlockers(unlockers); // The contract says the unlockers are run with the ct lock being held.
                    // Now wait for the I/O to occur.    
                    do_partial_fetch(ct, cf, p, pf_callback, read_extraargs, FALSE);
                    cachetable_unlock(ct);
                    return TOKUDB_TRY_AGAIN;
                }
                else {
                    *value = p->value_data;
                    return 0;
                }
            }
            else {
                run_unlockers(unlockers); // The contract says the unlockers are run with the ct lock being held.
                // Now wait for the I/O to occur.
                nb_mutex_lock(&p->value_nb_mutex, ct->mutex);
                if (may_modify_value && p->checkpoint_pending) {
                    write_locked_pair_for_checkpoint(ct, p);
                }
                nb_mutex_unlock(&p->value_nb_mutex);
                cachetable_unlock(ct);
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
        fullhash, 
        zero_attr, 
        write_callback,
        CACHETABLE_CLEAN
        );
    assert(p);
    nb_mutex_lock(&p->value_nb_mutex, ct->mutex);
    run_unlockers(unlockers); // we hold the ct mutex.
    u_int64_t t0 = get_tnow();
    cachetable_fetch_pair(ct, cf, p, fetch_callback, read_extraargs, FALSE);
    cachetable_miss++;
    cachetable_misstime += get_tnow() - t0;
    cachetable_unlock(ct);
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

// Worker thread function to read a pair from a cachefile to memory
static void cachetable_reader(WORKITEM wi) {
    struct cachefile_prefetch_args* cpargs = (struct cachefile_prefetch_args *) workitem_arg(wi);
    CACHEFILE cf = cpargs->p->cachefile;
    CACHETABLE ct = cf->cachetable;
    cachetable_lock(ct);
    cachetable_fetch_pair(
        ct,
        cpargs->p->cachefile,
        cpargs->p,
        cpargs->fetch_callback,
        cpargs->read_extraargs,
        FALSE
        );
    cachetable_unlock(ct);
    bjm_remove_background_job(cf->bjm);
    toku_free(cpargs);
}

static void cachetable_partial_reader(WORKITEM wi) {
    struct cachefile_partial_prefetch_args *cpargs = (struct cachefile_partial_prefetch_args *) workitem_arg(wi);
    CACHEFILE cf = cpargs->p->cachefile;
    CACHETABLE ct = cf->cachetable;
    cachetable_lock(ct);
    do_partial_fetch(ct, cpargs->p->cachefile, cpargs->p, cpargs->pf_callback, cpargs->read_extraargs, FALSE);
    cachetable_unlock(ct);
    bjm_remove_background_job(cf->bjm);
    toku_free(cpargs);
}

int toku_cachefile_prefetch(CACHEFILE cf, CACHEKEY key, u_int32_t fullhash,
                            CACHETABLE_WRITE_CALLBACK write_callback,
                            CACHETABLE_FETCH_CALLBACK fetch_callback,
                            CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK pf_req_callback,
                            CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback,
                            void *read_extraargs,
                            BOOL *doing_prefetch)
// Effect: See the documentation for this function in cachetable.h
{
    int r = 0;
    if (doing_prefetch) {
        *doing_prefetch = FALSE;
    }
    CACHETABLE ct = cf->cachetable;
    cachetable_lock(ct);
    // lookup
    PAIR p;
    for (p = ct->table[fullhash&(ct->table_size-1)]; p; p = p->hash_chain) {
        if (p->key.b==key.b && p->cachefile==cf) {
            pair_touch(p);
            break;
        }
    }

    // if not found then create a pair in the READING state and fetch it
    if (p == 0) {
        cachetable_prefetches++;
        r = bjm_add_background_job(cf->bjm);
        assert_zero(r);
        p = cachetable_insert_at(
            ct, 
            cf, 
            key, 
            zero_value, 
            fullhash, 
            zero_attr, 
            write_callback,
            CACHETABLE_CLEAN
            );
        assert(p);
        nb_mutex_lock(&p->value_nb_mutex, ct->mutex);
        struct cachefile_prefetch_args *MALLOC(cpargs);
        cpargs->p = p;
        cpargs->fetch_callback = fetch_callback;
        cpargs->read_extraargs = read_extraargs;
        workitem_init(&p->asyncwork, cachetable_reader, cpargs);
        workqueue_enq(&ct->wq, &p->asyncwork, 0);
        if (doing_prefetch) {
            *doing_prefetch = TRUE;
        }
    }
    else if (nb_mutex_users(&p->value_nb_mutex)==0) {        
        // nobody else is using the node, so we should go ahead and prefetch
        nb_mutex_lock(&p->value_nb_mutex, ct->mutex);
        BOOL partial_fetch_required = pf_req_callback(p->value_data, read_extraargs);

        if (partial_fetch_required) {
            r = bjm_add_background_job(cf->bjm);
            assert_zero(r);
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
            nb_mutex_unlock(&p->value_nb_mutex);
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
    u_int32_t fullhash = toku_cachetable_hash(cachefile, oldkey);
    cachetable_lock(ct);
    for (ptr_to_p = &ct->table[fullhash&(ct->table_size-1)],  p = *ptr_to_p;
         p;
         ptr_to_p = &p->hash_chain,                p = *ptr_to_p) {
        if (p->key.b==oldkey.b && p->cachefile==cachefile) {
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
    cachetable_unlock(ct);
    return -1;
}

void toku_cachefile_verify (CACHEFILE cf) {
    toku_cachetable_verify(cf->cachetable);
}

void toku_cachetable_verify (CACHETABLE ct) {
    cachetable_lock(ct);
    u_int32_t num_found = 0;

    // First clear all the verify flags by going through the hash chains
    {
        u_int32_t i;
        for (i=0; i<ct->table_size; i++) {
            PAIR p;
            for (p=ct->table[i]; p; p=p->hash_chain) {
                num_found++;
            }
        }
    }
    assert(num_found == ct->n_in_table);
    num_found = 0;
    // Now go through the clock chain, make sure everything in the LRU chain is hashed.
    {
        PAIR p;
        BOOL is_first = TRUE;
        for (p=ct->clock_head; ct->clock_head!=NULL && (p!=ct->clock_head || is_first); p=p->clock_next) {
            is_first=FALSE;
            PAIR p2;
            u_int32_t fullhash = p->fullhash;
            //assert(fullhash==toku_cachetable_hash(p->cachefile, p->key));
            for (p2=ct->table[fullhash&(ct->table_size-1)]; p2; p2=p2->hash_chain) {
                if (p2==p) {
                    /* found it */
                    num_found++;
                    goto next;
                }
            }
            fprintf(stderr, "Something in the clock chain is not hashed\n");
            assert(0);
        next:;
        }
        assert (num_found == ct->n_in_table);
    }
    cachetable_unlock(ct);
}

struct pair_flush_for_close{
    PAIR p;
    BACKGROUND_JOB_MANAGER bjm;
};

static void cachetable_flush_pair_for_close(WORKITEM wi) {
    struct pair_flush_for_close *args = cast_to_typeof(args) workitem_arg(wi);
    PAIR p = args->p;
    CACHEFILE cf = p->cachefile;
    CACHETABLE ct = cf->cachetable;
    cachetable_lock(ct);
    PAIR_ATTR attr;
    cachetable_only_write_locked_data(
        ct,
        p,
        FALSE, // not for a checkpoint, as we assert above
        &attr,
        FALSE // not a clone
        );            
    p->dirty = CACHETABLE_CLEAN;
    cachetable_unlock(ct);
    bjm_remove_background_job(args->bjm);
    toku_free(args);
}

// Flush (write to disk) all of the pairs that belong to a cachefile (or all pairs if 
// the cachefile is NULL.
// Must be holding cachetable lock on entry.
// 
// This function assumes that no client thread is accessing or 
// trying to access the cachefile while this function is executing.
// This implies no client thread will be trying to lock any nodes
// belonging to the cachefile.
//
// This function also assumes that the cachefile is not in the process
// of being used by a checkpoint. If a checkpoint is currently happening,
// it does NOT include this cachefile.
//
static void cachetable_flush_cachefile(CACHETABLE ct, CACHEFILE cf) {
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

    unsigned i;
    unsigned num_pairs = 0;
    unsigned list_size = 256;
    PAIR *list = NULL;
    XMALLOC_N(list_size, list);

    //Make a list of pairs that belong to this cachefile.
    if (cf == NULL) {
        for (i=0; i < ct->table_size; i++) {
            PAIR p;
            for (p = ct->table[i]; p; p = p->hash_chain) {
                if (cf == 0 || p->cachefile==cf) {
                    if (num_pairs == list_size) {
                        list_size *= 2;
                        XREALLOC_N(list_size, list);
                    }
                    list[num_pairs++] = p;
                }
            }
        }
    } 
    else {
        for (struct toku_list *next_pair = cf->pairs_for_cachefile.next; next_pair != &cf->pairs_for_cachefile; next_pair = next_pair->next) {
            PAIR p = toku_list_struct(next_pair, struct ctpair, next_for_cachefile);
            if (num_pairs == list_size) {
                list_size *= 2;
                XREALLOC_N(list_size, list);
            }
            list[num_pairs++] = p;
        }
    }
    
    // first write out dirty PAIRs
    BACKGROUND_JOB_MANAGER bjm = NULL;
    bjm_init(&bjm);
    for (i=0; i < num_pairs; i++) {
        PAIR p = list[i];
        assert(nb_mutex_users(&p->value_nb_mutex) == 0);
        assert(nb_mutex_users(&p->disk_nb_mutex) == 0);
        assert(!p->cloned_value_data);
        if (p->dirty == CACHETABLE_DIRTY) {
            int r = bjm_add_background_job(bjm);
            assert_zero(r);
            struct pair_flush_for_close *XMALLOC(args);
            args->p = p;
            args->bjm = bjm;
            workitem_init(&p->asyncwork, cachetable_flush_pair_for_close, args);
            workqueue_enq(&ct->wq, &p->asyncwork, 0);

        }
    }
    cachetable_unlock(ct);
    bjm_wait_for_jobs_to_finish(bjm);
    bjm_destroy(bjm);
    cachetable_lock(ct);
    
    // now get rid of everything
    for (i=0; i < num_pairs; i++) {
        PAIR p = list[i];
        assert(nb_mutex_users(&p->value_nb_mutex) == 0);
        assert(nb_mutex_users(&p->disk_nb_mutex) == 0);
        assert(!p->cloned_value_data);
        assert(p->dirty == CACHETABLE_CLEAN);
        cachetable_maybe_remove_and_free_pair(ct, p);
    } 

    if (cf) {
        assert(toku_list_empty(&cf->pairs_for_cachefile));
        bjm_reset(cf->bjm);
    }

    if ((4 * ct->n_in_table < ct->table_size) && (ct->table_size>4)) {
        cachetable_rehash(ct, ct->table_size/2);
    }    
    toku_free(list);
}

/* Requires that no locks be held that are used by the checkpoint logic */
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
    cachetable_lock(ct);
    cachetable_flush_cachefile(ct, NULL);
    u_int32_t i;
    for (i=0; i<ct->table_size; i++) {
        if (ct->table[i]) return -1;
    }
    assert(ct->size_evicting == 0);
    rwlock_destroy(&ct->pending_lock);
    cachetable_unlock(ct);
    toku_destroy_workers(&ct->wq, &ct->threadpool);
    toku_destroy_workers(&ct->checkpoint_wq, &ct->checkpoint_threadpool);
    toku_kibbutz_destroy(ct->kibbutz);
    bjm_destroy(ct->checkpoint_clones_bjm);
    toku_cond_destroy(&ct->flow_control_cond);
    toku_free(ct->table);
    toku_free(ct->env_dir);
    toku_free(ct);
    *ctp = 0;
    return 0;
}

int toku_cachetable_unpin_and_remove (
    CACHEFILE cachefile, 
    CACHEKEY key,
    CACHETABLE_REMOVE_KEY remove_key,
    void* remove_key_extra
    ) 
{
    int r = ENOENT;
    // Removing something already present is OK.
    CACHETABLE ct = cachefile->cachetable;
    PAIR p;
    cachetable_lock(ct);
    u_int32_t fullhash = toku_cachetable_hash(cachefile, key);
    for (p=ct->table[fullhash&(ct->table_size-1)]; p; p=p->hash_chain) {
        if (p->key.b==key.b && p->cachefile==cachefile) {
            p->dirty = CACHETABLE_CLEAN; // clear the dirty bit.  We're just supposed to remove it.
            assert(nb_mutex_writers(&p->value_nb_mutex));
            // grab disk_nb_mutex to ensure any background thread writing
            // out a cloned value completes
            nb_mutex_lock(&p->disk_nb_mutex, ct->mutex);
            assert(p->cloned_value_data == NULL);
            
            //
            // take care of key removal
            //
            BOOL for_checkpoint = p->checkpoint_pending;
            // now let's wipe out the pending bit, because we are
            // removing the PAIR
            p->checkpoint_pending = FALSE;
            //
            // Here is a tricky thing.
            // Later on in this function, we may release the
            // cachetable lock if other threads are blocked
            // on this pair, trying to acquire the PAIR lock. 
            // While the cachetable lock is released,
            // we may theoretically begin another checkpoint, or start
            // a cleaner thread.
            // So, just to be sure this PAIR won't be marked
            // for the impending checkpoint, we mark the
            // PAIR as clean. For the PAIR to not be picked by the
            // cleaner thread, we mark the cachepressure_size to be 0
            // This should not be an issue because we call
            // cachetable_remove_pair before
            // releasing the cachetable lock.
            //
            p->dirty = CACHETABLE_CLEAN;
            CACHEKEY key_to_remove = key;
            p->attr.cache_pressure_size = 0;
            //
            // callback for removing the key
            // for FTNODEs, this leads to calling
            // toku_free_blocknum
            //
            if (remove_key) {
                remove_key(
                    &key_to_remove, 
                    for_checkpoint, 
                    remove_key_extra
                    );
            }
            nb_mutex_unlock(&p->value_nb_mutex);
            nb_mutex_unlock(&p->disk_nb_mutex);
            //
            // As of Dr. Noga, only these threads may be
            // blocked waiting to lock this PAIR:
            //  - the checkpoint thread (because a checkpoint is in progress
            //     and the PAIR was in the list of pending pairs)
            //  - a client thread running get_and_pin_nonblocking, who
            //     ran unlockers, then waited on the PAIR lock.
            //     While waiting on a PAIR lock, another thread comes in,
            //     locks the PAIR, and ends up calling unpin_and_remove,
            //     all while get_and_pin_nonblocking is waiting on the PAIR lock.
            //     We did not realize this at first, which caused bug #4357
            // The following threads CANNOT be blocked waiting on 
            // the PAIR lock:
            //  - a thread trying to run eviction via maybe_flush_some. 
            //     That cannot happen because maybe_flush_some only
            //     attempts to lock PAIRS that are not locked, and this PAIR
            //     is locked.
            //  - cleaner thread, for the same reason as a thread running 
            //     eviction
            //  - client thread doing a normal get_and_pin. The client is smart
            //     enough to not try to lock a PAIR that another client thread
            //     is trying to unpin and remove. Note that this includes work
            //     done on kibbutzes.
            //  - writer thread. Writer threads do not grab PAIR locks. They
            //     get PAIR locks transferred to them by client threads.
            //

            // first thing we do is remove the PAIR from the various
            // cachetable data structures, so no other thread can possibly
            // access it. We do not want to risk some other thread
            // trying to lock this PAIR if we release the cachetable lock
            // below. If some thread is already waiting on the lock,
            // then we let that thread grab the lock and finish, but
            // we don't want any NEW threads to try to grab the PAIR
            // lock.
            //
            // Because we call cachetable_remove_pair and wait,
            // the threads that may be waiting
            // on this PAIR lock must be careful to do NOTHING with the PAIR 
            // As per our analysis above, we only need
            // to make sure the checkpoint thread and get_and_pin_nonblocking do
            // nothing, and looking at those functions, it is clear they do nothing.
            // 
            cachetable_remove_pair(ct, p);
            if (nb_mutex_blocked_writers(&p->value_nb_mutex)>0) {
                toku_cond_t cond;
                toku_cond_init(&cond, NULL);
                nb_mutex_wait_for_users(
                    &p->value_nb_mutex,
                    ct->mutex,
                    &cond
                    );
                toku_cond_destroy(&cond);
                assert(!p->checkpoint_pending);
                assert(p->attr.cache_pressure_size == 0);
            }
            // just a sanity check
            assert(nb_mutex_users(&p->disk_nb_mutex) == 0);
            assert(p->cloned_value_data == NULL);
            //Remove pair.
            cachetable_free_pair(ct, p);
            r = 0;
            goto done;
        }
    }
 done:
    cachetable_unlock(ct);
    return r;
}

static int
set_filenum_in_array(OMTVALUE hv, u_int32_t index, void*arrayv) {
    FILENUM *array = (FILENUM *) arrayv;
    FT h = (FT) hv;
    array[index] = toku_cachefile_filenum(h->cf);
    return 0;
}

static int
log_open_txn (OMTVALUE txnv, u_int32_t UU(index), void *extra) {
    int r;
    TOKUTXN    txn    = (TOKUTXN) txnv;
    TOKULOGGER logger = txn->logger;
    FILENUMS open_filenums;
    uint32_t num_filenums = toku_omt_size(txn->open_fts);
    FILENUM array[num_filenums];
    if (toku_txn_is_read_only(txn)) {
        goto cleanup;
    }
    else {
        CACHETABLE ct = (CACHETABLE) extra;
        ct->checkpoint_num_txns++;
    }

    open_filenums.num      = num_filenums;
    open_filenums.filenums = array;
    //Fill in open_filenums
    r = toku_omt_iterate(txn->open_fts, set_filenum_in_array, array);
    invariant(r==0);
    switch (toku_txn_get_state(txn)) {
    case TOKUTXN_LIVE:{
        r = toku_log_xstillopen(logger, NULL, 0, txn,
                                toku_txn_get_txnid(txn),
                                toku_txn_get_txnid(toku_logger_txn_parent(txn)),
                                txn->roll_info.rollentry_raw_count,
                                open_filenums,
                                txn->force_fsync_on_commit,
                                txn->roll_info.num_rollback_nodes,
                                txn->roll_info.num_rollentries,
                                txn->roll_info.spilled_rollback_head,
                                txn->roll_info.spilled_rollback_tail,
                                txn->roll_info.current_rollback);
        lazy_assert_zero(r);
        goto cleanup;
    }
    case TOKUTXN_PREPARING: {
        TOKU_XA_XID xa_xid;
        toku_txn_get_prepared_xa_xid(txn, &xa_xid);
        r = toku_log_xstillopenprepared(logger, NULL, 0, txn,
                                        toku_txn_get_txnid(txn),
                                        &xa_xid,
                                        txn->roll_info.rollentry_raw_count,
                                        open_filenums,
                                        txn->force_fsync_on_commit,
                                        txn->roll_info.num_rollback_nodes,
                                        txn->roll_info.num_rollentries,
                                        txn->roll_info.spilled_rollback_head,
                                        txn->roll_info.spilled_rollback_tail,
                                        txn->roll_info.current_rollback);
        lazy_assert_zero(r);
        goto cleanup;
    }
    case TOKUTXN_RETIRED:
    case TOKUTXN_COMMITTING:
    case TOKUTXN_ABORTING: {
        assert(0);
    }
    }
    // default is an error
    assert(0);
cleanup:
    return 0;
}

int
toku_cachetable_begin_checkpoint (CACHETABLE ct, TOKULOGGER logger) {
    // Requires:   All three checkpoint-relevant locks must be held (see checkpoint.c).
    // Algorithm:  Write a checkpoint record to the log, noting the LSN of that record.
    //             Use the begin_checkpoint callback to take necessary snapshots (header, btt)
    //             Mark every dirty node as "pending."  ("Pending" means that the node must be
    //                                                    written to disk before it can be modified.)
    {
        unsigned i;
        cachetable_lock(ct);
        //Initialize accountability counters
        ct->checkpoint_num_files = 0;
        ct->checkpoint_num_txns  = 0;

        //Make list of cachefiles to be included in checkpoint.
        {
            CACHEFILE cf;
            assert(ct->cachefiles_in_checkpoint==NULL);
            for (cf = ct->cachefiles; cf; cf=cf->next) {
                // The caller must serialize open, close, and begin checkpoint.
                // So we should never see a closing cachefile here.

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
        }

        if (logger) {
            // The checkpoint must be performed after the lock is acquired.
            {
                LSN begin_lsn={ .lsn = (uint64_t) -1 }; // we'll need to store the lsn of the checkpoint begin in all the trees that are checkpointed.
                TXN_MANAGER mgr = toku_logger_get_txn_manager(logger);
                TXNID last_xid = toku_txn_manager_get_last_xid(mgr);
                int r = toku_log_begin_checkpoint(logger, &begin_lsn, 0, 0, last_xid);
                assert(r==0);
                ct->lsn_of_checkpoint_in_progress = begin_lsn;
            }
            // Log all the open files
            {
                //Must loop through ALL open files (even if not included in checkpoint).
                CACHEFILE cf;
                for (cf = ct->cachefiles; cf; cf=cf->next) {
                    if (cf->log_fassociate_during_checkpoint) {
                        int r = cf->log_fassociate_during_checkpoint(cf, cf->userdata);
                        ct->checkpoint_num_files++;
                        assert(r==0);
                    }
                }
            }
            // Log all the open transactions MUST BE AFTER OPEN FILES
            {
                int r = toku_txn_manager_iter_over_live_txns(
                    logger->txn_manager,
                    log_open_txn,
                    ct
                    );
                assert(r==0);
            }
            // Log rollback suppression for all the open files MUST BE AFTER TXNS
            {
                //Must loop through ALL open files (even if not included in checkpoint).
                CACHEFILE cf;
                for (cf = ct->cachefiles; cf; cf=cf->next) {
                    if (cf->log_suppress_rollback_during_checkpoint) {
                        int r = cf->log_suppress_rollback_during_checkpoint(cf, cf->userdata);
                        assert(r==0);
                    }
                }
            }
        }

        unsigned int npending = 0;
        //
        // Here is why we have the pending lock, and why we take its write lock
        // at this point. 
        //
        // First, here is how the pending lock is used:
        //  - begin checkpoint grabs the write lock
        //  - threads that write a node to disk grab the read lock
        //
        // As a result, when we grab the write lock here, we know that
        // no writer threads are in the middle of writing a node out to disk.
        //
        // We are protecting against a race condition between writer
        // threads that write nodes to disk, and the beginning of this checkpoint.
        // When a writer thread writes a node to disk, the cachetable lock is released,
        // allowing a begin_checkpoint to occur. If writer threads and begin checkpoint
        // run concurrently, our checkpoint may be incorrect. Here is how.
        //
        // Here is the specific race condition. Suppose this pending lock does not exist,
        // and writer threads may be writing nodes to disk. Take the following scenario:
        // Writer thread:
        //  -grabs cachetable lock, has a dirty PAIR without the checkpoint pending bit set
        //  -releases the cachetable lock with the intent of writing this node to disk
        // Before the writer thread goes any further, the checkpoint thread comes along:
        //  - marks the dirty PAIR that is about to be written out as pending.
        //  - copies the current translation table of that contains the PAIR to the inprogress one (see struct block_table)
        // At this point, for the checkpoint to be correct, the dirty PAIR 
        // that is in the process of being written out should be included in the inprogress translation table. This PAIR 
        // belongs in the checkpoint.
        // Now let's go back to the writer thread:
        //  - because the checkpoint pending bit was not set for the PAIR, the for_checkpoint parameter
        //     passed into the flush callback is FALSE.
        //  - as a result, the PAIR is written to disk, the current translation table is updated, but the
        //     inprogress translation table is NOT updated.
        //  - the PAIR is marked as clean because it was just written to disk
        // Now, when the checkpoint thread gets around to this PAIR, it notices
        // that the checkpoint_pending bit is set, but the PAIR is clean. So no I/O is done.
        // The checkpoint_pending bit is cleared, without the inprogress translation table ever being 
        // updated. This is not correct. The result is that the checkpoint does not include the proper
        // state of this PAIR.
        //
        rwlock_write_lock(&ct->pending_lock, ct->mutex);
        ct->checkpoint_is_beginning = TRUE;         // detect threadsafety bugs, must set checkpoint_is_beginning ...
        invariant(ct->checkpoint_prohibited == 0);  // ... before testing checkpoint_prohibited
        bjm_reset(ct->checkpoint_clones_bjm);
        for (i=0; i < ct->table_size; i++) {
            PAIR p;
            for (p = ct->table[i]; p; p=p->hash_chain) {
                assert(!p->checkpoint_pending);
                //Only include pairs belonging to cachefiles in the checkpoint
                if (!p->cachefile->for_checkpoint) continue;
                // mark anything that is dirty OR currently in use
                // as pending a checkpoint
                //
                //The rule for the checkpoint_pending bit is as follows:
                //  - begin_checkpoint may set checkpoint_pending to true
                //    even though the pair lock on the node is not held. Only the
                //    cachetable lock is necessary
                //  - any thread that wants to clear the pending bit must own
                //     BOTH the cachetable lock and the PAIR lock. Otherwise,
                //     we may end up clearing the pending bit before the
                //     current lock is ever released.
                if (p->dirty || nb_mutex_writers(&p->value_nb_mutex)) {
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

        //begin_checkpoint_userdata must be called AFTER all the pairs are marked as pending.
        //Once marked as pending, we own write locks on the pairs, which means the writer threads can't conflict.
        {
            CACHEFILE cf;
            for (cf = ct->cachefiles_in_checkpoint; cf; cf=cf->next_in_checkpoint) {
                if (cf->begin_checkpoint_userdata) {
                    assert(cf->checkpoint_state == CS_NOT_IN_PROGRESS);
                    int r = cf->begin_checkpoint_userdata(ct->lsn_of_checkpoint_in_progress, cf->userdata);
                    assert(r==0);
                    cf->checkpoint_state = CS_CALLED_BEGIN_CHECKPOINT;
                }
            }
        }
        ct->checkpoint_is_beginning = FALSE;  // clear before releasing cachetable lock
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
                               void (*testcallback_f)(void*),  void * testextra) {
    // Requires:   The big checkpoint lock must be held (see checkpoint.c).
    // Algorithm:  Write all pending nodes to disk
    //             Use checkpoint callback to write snapshot information to disk (header, btt)
    //             Use end_checkpoint callback to fsync dictionary and log, and to free unused blocks
    // Note:       If testcallback is null (for testing purposes only), call it after writing dictionary but before writing log

    int retval = 0;
    cachetable_lock(ct);
    {
        PAIR p;
        while ((p = ct->pending_head)!=0) {
            ct->pending_head = ct->pending_head->pending_next;
            pending_pairs_remove(ct, p);
            write_pair_for_checkpoint_thread(ct, p); // if still pending, clear the pending bit and write out the node
            // Don't need to unlock and lock cachetable, because the cachetable was unlocked and locked while the flush callback ran.
        }
    }
    assert(!ct->pending_head);
    cachetable_unlock(ct);
    bjm_wait_for_jobs_to_finish(ct->checkpoint_clones_bjm);
    cachetable_lock(ct);


    {   // have just written data blocks, so next write the translation and header for each open dictionary
        CACHEFILE cf;
        for (cf = ct->cachefiles_in_checkpoint; cf; cf=cf->next_in_checkpoint) {
            if (cf->checkpoint_userdata) {
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
            }
        }
    }
    
    cachetable_unlock(ct);
    // For testing purposes only.  Dictionary has been fsync-ed to disk but log has not yet been written.
    if (testcallback_f) {
        testcallback_f(testextra);      
    }
    if (logger) {
        int r = toku_log_end_checkpoint(logger, NULL,
                                        1, // want the end_checkpoint to be fsync'd
                                        ct->lsn_of_checkpoint_in_progress, 
                                        0,
                                        ct->checkpoint_num_files,
                                        ct->checkpoint_num_txns);
        assert(r==0);
        toku_logger_note_checkpoint(logger, ct->lsn_of_checkpoint_in_progress);
    }
    cachetable_lock(ct);

    {   
        // everything has been written to file and fsynced
        // ... call checkpoint-end function in block translator
        //     to free obsolete blocks on disk used by previous checkpoint
        CACHEFILE cf;
        //cachefiles_in_checkpoint is protected by the checkpoint_safe_lock
        for (cf = ct->cachefiles_in_checkpoint; cf; cf=cf->next_in_checkpoint) {
            if (cf->end_checkpoint_userdata) {
                if (!logger || ct->lsn_of_checkpoint_in_progress.lsn != cf->most_recent_global_checkpoint_that_finished_early.lsn) {
                    assert(ct->lsn_of_checkpoint_in_progress.lsn >= cf->most_recent_global_checkpoint_that_finished_early.lsn);
                    cachetable_unlock(ct);
                    assert(cf->checkpoint_state == CS_CALLED_CHECKPOINT);
                    int r = cf->end_checkpoint_userdata(cf, cf->fd, cf->userdata);
                    assert(r==0);
                    cf->checkpoint_state = CS_NOT_IN_PROGRESS;
                    cachetable_lock(ct);
                }
                assert(cf->checkpoint_state == CS_NOT_IN_PROGRESS);
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
                int r = cf->note_unpin_by_checkpoint(cf, cf->userdata);
                if (r!=0) {
                    retval = r;
                    goto panic;
                }
            }
        }
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

// debug functions

int toku_cachetable_assert_all_unpinned (CACHETABLE ct) {
    u_int32_t i;
    int some_pinned=0;
    cachetable_lock(ct);
    for (i=0; i<ct->table_size; i++) {
        PAIR p;
        for (p=ct->table[i]; p; p=p->hash_chain) {
            assert(nb_mutex_writers(&p->value_nb_mutex)>=0);
            if (nb_mutex_writers(&p->value_nb_mutex)) {
                //printf("%s:%d pinned: %" PRId64 " (%p)\n", __FILE__, __LINE__, p->key.b, p->value_data);
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
        assert(nb_mutex_writers(&p->value_nb_mutex) >= 0);
        if (nb_mutex_writers(&p->value_nb_mutex)) {
            if (print_them) printf("%s:%d pinned: %" PRId64 " (%p)\n", __FILE__, __LINE__, p->key.b, p->value_data);
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
                printf(" {%" PRId64 ", %p, dirty=%d, pin=%d, size=%ld}", p->key.b, p->cachefile, (int) p->dirty, nb_mutex_writers(&p->value_nb_mutex), p->attr.size);
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
    int r = -1;
    u_int32_t fullhash = toku_cachetable_hash(cf, key);
    cachetable_lock(ct);
    for (p = ct->table[fullhash&(ct->table_size-1)]; p; p = p->hash_chain) {
        if (p->key.b == key.b && p->cachefile == cf) {
            if (value_ptr)
                *value_ptr = p->value_data;
            if (dirty_ptr)
                *dirty_ptr = p->dirty;
            if (pin_ptr)
                *pin_ptr = nb_mutex_writers(&p->value_nb_mutex);
            if (size_ptr)
                *size_ptr = p->attr.size;
            r = 0;
            break;
        }
    }
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
                             int (*begin_checkpoint_userdata)(LSN, void*),
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

//Only called by ft_end_checkpoint
//Must have access to cf->fd (must be protected)
int
toku_cachefile_fsync(CACHEFILE cf) {
    int r;
    r = toku_file_fsync(cf->fd);
    return r;
}

// Make it so when the cachefile closes, the underlying file is unlinked
void 
toku_cachefile_unlink_on_close(CACHEFILE cf) {
    assert(!cf->unlink_on_close);
    cf->unlink_on_close = true;
}

// is this cachefile marked as unlink on close?
bool 
toku_cachefile_is_unlink_on_close(CACHEFILE cf) {
    return cf->unlink_on_close;
}

u_int64_t toku_cachefile_size(CACHEFILE cf) {
    int64_t file_size;
    int fd = toku_cachefile_get_fd(cf);
    int r = toku_os_get_file_size(fd, &file_size);
    assert_zero(r);
    return file_size;
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
            char *XMALLOC_N(n, newname);
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

int
toku_cleaner_thread (void *cachetable_v)
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
    CACHETABLE ct = (CACHETABLE) cachetable_v;
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
        // here we select a PAIR for cleaning
        // look at some number of PAIRS, and 
        // pick what we think is the best one for cleaning
        //***** IMPORTANT ******
        // we MUST not pick a PAIR whose rating is 0. We have
        // numerous assumptions in other parts of the code that
        // this is the case:
        //  - this is how rollback nodes and leaf nodes are not selected for cleaning
        //  - this is how a thread that is calling unpin_and_remove will prevent
        //     the cleaner thread from picking its PAIR (see comments in that function)
        do {
            if (nb_mutex_users(&ct->cleaner_head->value_nb_mutex) > 0) {
                goto next_pair;
            }
            n_seen++;
            {
                long score = cleaner_thread_rate_pair(ct->cleaner_head);
                if (score > best_score) {
                    best_score = score;
                    best_pair = ct->cleaner_head;
                }
            }
        next_pair:
            ct->cleaner_head = ct->cleaner_head->clock_next;
        } while (ct->cleaner_head != first_pair && n_seen < CLEANER_N_TO_CHECK);
        //
        // at this point, if we have found a PAIR for cleaning, 
        // that is, best_pair != NULL, we do the clean
        //
        if (best_pair) {
            CACHEFILE cf = best_pair->cachefile;
            // try to add a background job to the manager
            // if we can't, that means the cachefile is flushing, so
            // we simply continue the for loop and this iteration
            // becomes a no-op
            int abj_ret = bjm_add_background_job(cf->bjm);
            if (abj_ret) {
                cachetable_unlock(ct);
                continue;
            }
            nb_mutex_lock(&best_pair->value_nb_mutex, ct->mutex);
            // verify a key assumption.
            assert(cleaner_thread_rate_pair(best_pair) > 0);
            if (best_pair->checkpoint_pending) {
                write_locked_pair_for_checkpoint(ct, best_pair);
            }

            BOOL cleaner_callback_called = FALSE;
            
            // it's theoretically possible that after writing a PAIR for checkpoint, the
            // PAIR's heuristic tells us nothing needs to be done. It is not possible
            // in Dr. Noga, but unit tests verify this behavior works properly.
            if (cleaner_thread_rate_pair(best_pair) > 0) 
            {
                cachetable_unlock(ct);
                int r = best_pair->cleaner_callback(best_pair->value_data,
                                                    best_pair->key,
                                                    best_pair->fullhash,
                                                    best_pair->write_extraargs);
                assert_zero(r);
                cleaner_callback_called = TRUE;
                cachetable_lock(ct);
            }

            // The cleaner callback must have unlocked the pair, so we
            // don't need to unlock it if the cleaner callback is called.
            if (!cleaner_callback_called) {
                nb_mutex_unlock(&best_pair->value_nb_mutex);
            }
            // We need to make sure the cachefile sticks around so a close
            // can't come destroy it.  That's the purpose of this
            // "add/remove_background_job" business, which means the
            // cachefile is still valid here, even though the cleaner
            // callback unlocks the pair. 
            bjm_remove_background_job(cf->bjm);
            cachetable_unlock(ct);
        }
        else {
            cachetable_unlock(ct);
            // If we didn't find anything this time around the cachetable,
            // we probably won't find anything if we run around again, so
            // just break out from the for-loop now and 
            // we'll try again when the cleaner thread runs again.
            break;
        }
    }
    return 0;
}

void __attribute__((__constructor__)) toku_cachetable_helgrind_ignore(void);
void
toku_cachetable_helgrind_ignore(void) {
    HELGRIND_VALGRIND_HG_DISABLE_CHECKING(&cachetable_miss, sizeof cachetable_miss);
    HELGRIND_VALGRIND_HG_DISABLE_CHECKING(&cachetable_misstime, sizeof cachetable_misstime);
    HELGRIND_VALGRIND_HG_DISABLE_CHECKING(&cachetable_puts, sizeof cachetable_puts);
    HELGRIND_VALGRIND_HG_DISABLE_CHECKING(&cachetable_prefetches, sizeof cachetable_prefetches);
    HELGRIND_VALGRIND_HG_DISABLE_CHECKING(&cachetable_evictions, sizeof cachetable_evictions);
    HELGRIND_VALGRIND_HG_DISABLE_CHECKING(&cleaner_executions, sizeof cleaner_executions);
    HELGRIND_VALGRIND_HG_DISABLE_CHECKING(&ct_status, sizeof ct_status);
}

#undef STATUS_VALUE
