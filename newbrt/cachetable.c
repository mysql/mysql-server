/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."


#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <toku_portability.h>
#include "memory.h"
#include "workqueue.h"
#include "threadpool.h"
#include "cachetable.h"
#include "rwlock.h"
#include "toku_worker.h"
#include "log_header.h"
#include "checkpoint.h"
#include "minicron.h"
#include "log-internal.h"

#if !defined(TOKU_CACHETABLE_DO_EVICT_FROM_WRITER)
#error
#endif

// use worker threads 0->no 1->yes
#define DO_WORKER_THREAD 1
#if DO_WORKER_THREAD
static void cachetable_writer(WORKITEM);
static void cachetable_reader(WORKITEM);
#endif

// use cachetable locks 0->no 1->yes
#define DO_CACHETABLE_LOCK 1

// simulate long latency write operations with usleep. time in milliseconds.
#define DO_CALLBACK_USLEEP 0
#define DO_CALLBACK_BUSYWAIT 0

#define TRACE_CACHETABLE 0
#if TRACE_CACHETABLE
#define WHEN_TRACE_CT(x) x
#else
#define WHEN_TRACE_CT(x) ((void)0)
#endif

#define TOKU_DO_WAIT_TIME 0

static u_int64_t cachetable_hit;
static u_int64_t cachetable_miss;
static u_int64_t cachetable_wait_reading;
static u_int64_t cachetable_wait;
#if TOKU_DO_WAIT_TIME
static u_int64_t cachetable_miss_time;
static u_int64_t cachetable_wait_time;
#endif

enum ctpair_state {
    CTPAIR_INVALID = 0, // invalid
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
    enum typ_tag tag;
    CACHEFILE cachefile;
    CACHEKEY key;
    void    *value;
    long     size;
    enum ctpair_state state;
    enum cachetable_dirty dirty;

    char     verify_flag;        // Used in verify_cachetable()
    BOOL     write_me;           // write_pair 
    BOOL     remove_me;          // write_pair

    u_int32_t fullhash;

    CACHETABLE_FLUSH_CALLBACK flush_callback;
    CACHETABLE_FETCH_CALLBACK fetch_callback;
    void    *extraargs;

    PAIR     next,prev;          // In LRU list.
    PAIR     hash_chain;

    LSN      modified_lsn;       // What was the LSN when modified (undefined if not dirty)
    LSN      written_lsn;        // What was the LSN when written (we need to get this information when we fetch)

    BOOL     checkpoint_pending; // If this is on, then we have got to write the pair out to disk before modifying it.
    PAIR     pending_next;
    PAIR     pending_prev;

    struct rwlock rwlock; // multiple get's, single writer
    struct workqueue *cq;        // writers sometimes return ctpair's using this queue
    struct workitem asyncwork;   // work item for the worker threads
    u_int32_t refs;  //References that prevent descruction
    int already_removed;  //If a pair is removed from the cachetable, but cannot be freed because refs>0, this is set.
};

static void * const zero_value = 0;
static int const zero_size = 0;


static inline void
ctpair_add_ref(PAIR p) {
    assert(!p->already_removed);
    p->refs++;
}

static inline void ctpair_destroy(PAIR p) {
    assert(p->refs>0);
    p->refs--;
    if (p->refs==0) {
        rwlock_destroy(&p->rwlock);
        toku_free(p);
    }
}

// The cachetable is as close to an ENV as we get.
struct cachetable {
    enum typ_tag tag;
    u_int32_t n_in_table;         // number of pairs in the hash table
    u_int32_t table_size;         // number of buckets in the hash table
    PAIR *table;                  // hash table
    PAIR  head,tail;              // of LRU list. head is the most recently used. tail is least recently used.
    CACHEFILE cachefiles;         // list of cachefiles that use this cachetable
    CACHEFILE cachefiles_in_checkpoint; //list of cachefiles included in checkpoint in progress
    long size_current;            // the sum of the sizes of the pairs in the cachetable
    long size_limit;              // the limit to the sum of the pair sizes
    long size_writing;            // the sum of the sizes of the pairs being written
    TOKULOGGER logger;
    toku_pthread_mutex_t *mutex;  // coarse lock that protects the cachetable, the cachefiles, and the pairs
    struct workqueue wq;          // async work queue 
    THREADPOOL threadpool;        // pool of worker threads
    LSN lsn_of_checkpoint_in_progress;
    PAIR pending_head;           // list of pairs marked with checkpoint_pending
    struct rwlock pending_lock;  // multiple writer threads, single checkpoint thread
    struct minicron checkpointer; // the periodic checkpointing thread
    toku_pthread_mutex_t openfd_mutex;  // make toku_cachetable_openfd() single-threaded
};

// Lock the cachetable
static inline void cachetable_lock(CACHETABLE ct __attribute__((unused))) {
#if DO_CACHETABLE_LOCK
    int r = toku_pthread_mutex_lock(ct->mutex); assert(r == 0);
#endif
}

// Unlock the cachetable
static inline void cachetable_unlock(CACHETABLE ct __attribute__((unused))) {
#if DO_CACHETABLE_LOCK
    int r = toku_pthread_mutex_unlock(ct->mutex); assert(r == 0);
#endif
}

// Wait for cache table space to become available 
// size_current is number of bytes currently occupied by data (referred to by pairs)
// size_writing is number of bytes queued up to be written out (sum of sizes of pairs in CTPAIR_WRITING state)
static inline void cachetable_wait_write(CACHETABLE ct) {
    // if we're writing more than half the data in the cachetable
    while (2*ct->size_writing > ct->size_current) {
        workqueue_wait_write(&ct->wq, 0);
    }
}

struct cachefile {
    CACHEFILE next;
    CACHEFILE next_in_checkpoint;
    BOOL for_checkpoint; //True if part of the in-progress checkpoint
    u_int64_t refcount; /* CACHEFILEs are shared. Use a refcount to decide when to really close it.
			 * The reference count is one for every open DB.
			 * Plus one for every commit/rollback record.  (It would be harder to keep a count for every open transaction,
			 * because then we'd have to figure out if the transaction was already counted.  If we simply use a count for
			 * every record in the transaction, we'll be ok.  Hence we use a 64-bit counter to make sure we don't run out.
			 */
    BOOL is_closing;    /* TRUE if a cachefile is being close/has been closed. */
    int fd;       /* Bug: If a file is opened read-only, then it is stuck in read-only.  If it is opened read-write, then subsequent writers can write to it too. */
    CACHETABLE cachetable;
    struct fileid fileid;
    FILENUM filenum;
    char *fname_relative_to_env; /* Used for logging */

    void *userdata;
    int (*close_userdata)(CACHEFILE cf, void *userdata, char **error_string, LSN); // when closing the last reference to a cachefile, first call this function. 
    int (*begin_checkpoint_userdata)(CACHEFILE cf, LSN lsn_of_checkpoint, void *userdata); // before checkpointing cachefiles call this function.
    int (*checkpoint_userdata)(CACHEFILE cf, void *userdata); // when checkpointing a cachefile, call this function.
    int (*end_checkpoint_userdata)(CACHEFILE cf, void *userdata); // after checkpointing cachefiles call this function.
    toku_pthread_cond_t openfd_wait;    // openfd must wait until file is fully closed (purged from cachetable) if file is opened and closed simultaneously
};

static int
checkpoint_thread (void *cachetable_v)
// Effect:  If checkpoint_period>0 thn periodically run a checkpoint.
//  If someone changes the checkpoint_period (calling toku_set_checkpoint_period), then the checkpoint will run sooner or later.
//  If someone sets the checkpoint_shutdown boolean , then this thread exits. 
// This thread notices those changes by waiting on a condition variable.
{
    char *error_string;
    CACHETABLE ct = cachetable_v;
    int r = toku_checkpoint(ct, ct->logger, &error_string, NULL, NULL, NULL, NULL);
    if (r) {
	if (error_string) {
	    fprintf(stderr, "%s:%d Got error %d while doing: %s\n", __FILE__, __LINE__, r, error_string);
	} else {
	    fprintf(stderr, "%s:%d Got error %d while doing checkpoint\n", __FILE__, __LINE__, r);
	}
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

int toku_create_cachetable(CACHETABLE *result, long size_limit, LSN UU(initial_lsn), TOKULOGGER logger) {
    TAGMALLOC(CACHETABLE, ct);
    if (ct == 0) return ENOMEM;
    memset(ct, 0, sizeof(*ct));
    ct->table_size = 4;
    rwlock_init(&ct->pending_lock);
    XCALLOC_N(ct->table_size, ct->table);
    ct->size_limit = size_limit;
    ct->logger = logger;
    toku_init_workers(&ct->wq, &ct->threadpool);
    ct->mutex = workqueue_lock_ref(&ct->wq);
    int r = toku_pthread_mutex_init(&ct->openfd_mutex, NULL); assert(r == 0);
    toku_minicron_setup(&ct->checkpointer, 0, checkpoint_thread, ct); // default is no checkpointing
    *result = ct;
    return 0;
}

// What cachefile goes with particular fd?
int toku_cachefile_of_filenum (CACHETABLE ct, FILENUM filenum, CACHEFILE *cf) {
    CACHEFILE extant;
    for (extant = ct->cachefiles; extant; extant=extant->next) {
	if (extant->filenum.fileid==filenum.fileid) {
	    *cf = extant;
	    return 0;
	}
    }
    return ENOENT;
}

static FILENUM next_filenum_to_use={0};

static void cachefile_init_filenum(CACHEFILE cf, int fd, const char *fname_relative_to_env, struct fileid fileid) \
{
    cf->fd = fd;
    cf->fileid = fileid;
    cf->fname_relative_to_env = fname_relative_to_env ? toku_strdup(fname_relative_to_env) : 0;
}
//
// Increment the reference count
// MUST HOLD cachetable lock
static void
cachefile_refup (CACHEFILE cf) {
    cf->refcount++;
}


// If something goes wrong, close the fd.  After this, the caller shouldn't close the fd, but instead should close the cachefile.
int toku_cachetable_openfd (CACHEFILE *cfptr, CACHETABLE ct, int fd, const char *fname_relative_to_env) {
    int r;
    CACHEFILE extant;
    struct fileid fileid;
    
    r = toku_os_get_unique_file_id(fd, &fileid);
    if (r != 0) { 
        r=errno; close(fd); 
        return r;
    }
    r = toku_pthread_mutex_lock(&ct->openfd_mutex);   // purpose is to make this function single-threaded
    assert(r==0);
    cachetable_lock(ct);
    for (extant = ct->cachefiles; extant; extant=extant->next) {
	if (memcmp(&extant->fileid, &fileid, sizeof(fileid))==0) {
            //File is already open (and in cachetable as extant)
            cachefile_refup(extant);
	    if (extant->is_closing) {
		// if another thread is closing this file, wait until the close is fully complete
		r = toku_pthread_cond_wait(&extant->openfd_wait, ct->mutex);
		assert(r == 0);
		break;    // other thread has closed this file, go create a new cachefile
	    }	    
	    r = close(fd);
            assert(r == 0);
	    // re-use pre-existing cachefile 
	    *cfptr = extant;
	    r = 0;
	    goto exit;
	}
    }
    //File is not open.  Make a new cachefile.
 try_again:
    for (extant = ct->cachefiles; extant; extant=extant->next) {
	if (next_filenum_to_use.fileid==extant->filenum.fileid) {
	    next_filenum_to_use.fileid++;
	    goto try_again;
	}
    }
    {
	// create a new cachefile entry in the cachetable
        CACHEFILE XCALLOC(newcf);
        newcf->cachetable = ct;
        newcf->filenum.fileid = next_filenum_to_use.fileid++;
        cachefile_init_filenum(newcf, fd, fname_relative_to_env, fileid);
	newcf->refcount = 1;
	newcf->next = ct->cachefiles;
	ct->cachefiles = newcf;
	r = toku_pthread_cond_init(&newcf->openfd_wait, NULL); assert(r == 0);
	*cfptr = newcf;
	r = 0;
    }
 exit:
    {
	int rm = toku_pthread_mutex_unlock(&ct->openfd_mutex);
	assert (rm == 0);
    } 
    cachetable_unlock(ct);
    return r;
}

//TEST_ONLY_FUNCTION
int toku_cachetable_openf (CACHEFILE *cfptr, CACHETABLE ct, const char *fname, const char *fname_relative_to_env, int flags, mode_t mode) {
    int fd = open(fname, flags+O_BINARY, mode);
    if (fd<0) return errno;
    return toku_cachetable_openfd (cfptr, ct, fd, fname_relative_to_env);
}

WORKQUEUE toku_cachetable_get_workqueue(CACHETABLE ct) {
    return &ct->wq;
}

void toku_cachefile_get_workqueue_load (CACHEFILE cf, int *n_in_queue, int *n_threads) {
    CACHETABLE ct = cf->cachetable;
    *n_in_queue = workqueue_n_in_queue(&ct->wq, 1);
    *n_threads  = threadpool_get_current_threads(ct->threadpool);
}

int toku_cachefile_set_fd (CACHEFILE cf, int fd, const char *fname_relative_to_env) {
    int r;
    struct fileid fileid;
    r=toku_os_get_unique_file_id(fd, &fileid);
    if (r != 0) { 
        r=errno; close(fd); return r; 
    }
    if (cf->close_userdata && (r = cf->close_userdata(cf, cf->userdata, 0, ZERO_LSN))) {
        return r;
    }
    cf->close_userdata = NULL;
    cf->checkpoint_userdata = NULL;
    cf->begin_checkpoint_userdata = NULL;
    cf->end_checkpoint_userdata = NULL;
    cf->userdata = NULL;

    close(cf->fd);
    cf->fd = -1;
    if (cf->fname_relative_to_env) {
	toku_free(cf->fname_relative_to_env);
	cf->fname_relative_to_env = 0;
    }
    cachefile_init_filenum(cf, fd, fname_relative_to_env, fileid);
    return 0;
}

int toku_cachefile_fd (CACHEFILE cf) {
    return cf->fd;
}

BOOL
toku_cachefile_is_dev_null (CACHEFILE cf) {
    return (BOOL)(cf->fname_relative_to_env==NULL);
}

int
toku_cachefile_truncate (CACHEFILE cf, toku_off_t new_size) {
    int r;
    if (cf->fname_relative_to_env==NULL) r = 0; //Don't truncate /dev/null
    else {
        r = ftruncate(cf->fd, new_size);
        if (r != 0)
            r = errno;
    }
    return r;
}

static CACHEFILE remove_cf_from_list (CACHEFILE cf, CACHEFILE list) {
    if (list==0) return 0;
    else if (list==cf) {
	return list->next;
    } else {
	list->next = remove_cf_from_list(cf, list->next);
	return list;
    }
}

static int cachetable_flush_cachefile (CACHETABLE, CACHEFILE cf);

int toku_cachefile_close (CACHEFILE *cfp, TOKULOGGER logger, char **error_string, LSN lsn) {

    CACHEFILE cf = *cfp;
    CACHETABLE ct = cf->cachetable;
    cachetable_lock(ct);
    assert(cf->refcount>0);
    cf->refcount--;
    if (cf->refcount==0) {
        //Checkpoint holds a reference, close should be impossible if still in use by a checkpoint.
        assert(!cf->next_in_checkpoint);
        assert(!cf->for_checkpoint);
        assert(!cf->is_closing);
        cf->is_closing = TRUE; //Mark this cachefile so that no one will re-use it.
	int r;
	// cachetable_flush_cachefile() may release and retake cachetable_lock,
	// allowing another thread to get into toku_cachetable_openfd()
	if ((r = cachetable_flush_cachefile(ct, cf))) {
	error:
	    cf->cachetable->cachefiles = remove_cf_from_list(cf, cf->cachetable->cachefiles);
	    if (cf->refcount > 0) {
                int rs;
		assert(cf->refcount == 1);       // toku_cachetable_openfd() is single-threaded
                assert(!cf->next_in_checkpoint); //checkpoint cannot run on a closing file
                assert(!cf->for_checkpoint);     //checkpoint cannot run on a closing file
		rs = toku_pthread_cond_signal(&cf->openfd_wait); assert(rs == 0);
	    }
	    // we can destroy the condition variable because if there was another thread waiting, it was already signalled
            {
                int rd = toku_pthread_cond_destroy(&cf->openfd_wait);
                assert(rd == 0);
            }
	    if (cf->fname_relative_to_env) toku_free(cf->fname_relative_to_env);
	    int r2 = close(cf->fd);
	    if (r2!=0) fprintf(stderr, "%s:%d During error handling, could not close file r=%d errno=%d\n", __FILE__, __LINE__, r2, errno);
	    //assert(r == 0);
	    toku_free(cf);
	    *cfp = NULL;
	    cachetable_unlock(ct);
	    return r;
        }
	if (cf->close_userdata && (r = cf->close_userdata(cf, cf->userdata, error_string, lsn))) {
	    goto error;
	}
       	cf->close_userdata = NULL;
	cf->checkpoint_userdata = NULL;
	cf->begin_checkpoint_userdata = NULL;
	cf->end_checkpoint_userdata = NULL;
	cf->userdata = NULL;
        cf->cachetable->cachefiles = remove_cf_from_list(cf, cf->cachetable->cachefiles);
        // refcount could be non-zero if another thread is trying to open this cachefile,
	// but is blocked in toku_cachetable_openfd() waiting for us to finish closing it.
	if (cf->refcount > 0) {
            int rs;
	    assert(cf->refcount == 1);   // toku_cachetable_openfd() is single-threaded
	    rs = toku_pthread_cond_signal(&cf->openfd_wait); assert(rs == 0);
	}
	// we can destroy the condition variable because if there was another thread waiting, it was already signalled
        {
            int rd = toku_pthread_cond_destroy(&cf->openfd_wait);
            assert(rd == 0);
        }
        cachetable_unlock(ct);
	r = close(cf->fd);
	assert(r == 0);
        cf->fd = -1;
	if (logger) {
	    //assert(cf->fname);
	    //BYTESTRING bs = {.len=strlen(cf->fname), .data=cf->fname};
	    //r = toku_log_cfclose(logger, 0, 0, bs, cf->filenum);
	}

	if (cf->fname_relative_to_env) toku_free(cf->fname_relative_to_env);
	toku_free(cf);
	*cfp=NULL;
	return r;
    } else {
        cachetable_unlock(ct);
	*cfp=NULL;
	return 0;
    }
}

int toku_cachefile_flush (CACHEFILE cf) {
    CACHETABLE ct = cf->cachetable;
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

static void lru_remove (CACHETABLE ct, PAIR p) {
    if (p->next) {
	p->next->prev = p->prev;
    } else {
	assert(ct->tail==p);
	ct->tail = p->prev;
    }
    if (p->prev) {
	p->prev->next = p->next;
    } else {
	assert(ct->head==p);
	ct->head = p->next;
    }
    p->prev = p->next = 0;
}

static void lru_add_to_list (CACHETABLE ct, PAIR p) {
    // requires that touch_me is not currently in the table.
    assert(p->prev==0);
    p->prev = 0;
    p->next = ct->head;
    if (ct->head) {
	ct->head->prev = p;
    } else {
	assert(!ct->tail);
	ct->tail = p;
    }
    ct->head = p; 
}

static void lru_touch (CACHETABLE ct, PAIR p) {
    lru_remove(ct,p);
    lru_add_to_list(ct,p);
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

// Remove a pair from the cachetable
// Effects: the pair is removed from the LRU list and from the cachetable's hash table.
// The size of the objects in the cachetable is adjusted by the size of the pair being
// removed.

static void cachetable_remove_pair (CACHETABLE ct, PAIR p) {
    lru_remove(ct, p);
    pending_pairs_remove(ct, p);

    assert(ct->n_in_table>0);
    ct->n_in_table--;
    // Remove it from the hash chain.
    {
	unsigned int h = p->fullhash&(ct->table_size-1);
	ct->table[h] = remove_from_hash_chain (p, ct->table[h]);
    }
    ct->size_current -= p->size; assert(ct->size_current >= 0);
    p->already_removed = TRUE;
}

// Maybe remove a pair from the cachetable and free it, depending on whether
// or not there are any threads interested in the pair.  The flush callback
// is called with write_me and keep_me both false, and the pair is destroyed.

static void cachetable_maybe_remove_and_free_pair (CACHETABLE ct, PAIR p) {
    if (rwlock_users(&p->rwlock) == 0) {
        cachetable_remove_pair(ct, p);

        // helgrind
        CACHETABLE_FLUSH_CALLBACK flush_callback = p->flush_callback;
        CACHEFILE cachefile = p->cachefile;
        CACHEKEY key = p->key;
        void *value = p->value;
        void *extraargs = p->extraargs;
        long size = p->size;

        cachetable_unlock(ct);

        flush_callback(cachefile, key, value, extraargs, size, FALSE, FALSE, TRUE);

        cachetable_lock(ct);

        ctpair_destroy(p);
    }
}

static void abort_fetch_pair(PAIR p) {
    rwlock_write_unlock(&p->rwlock);
    if (rwlock_users(&p->rwlock) == 0)
        ctpair_destroy(p);
}

// Read a pair from a cachefile into memory using the pair's fetch callback
static int cachetable_fetch_pair(CACHETABLE ct, CACHEFILE cf, PAIR p) {
    // helgrind
    CACHETABLE_FETCH_CALLBACK fetch_callback = p->fetch_callback;
    CACHEKEY key = p->key;
    u_int32_t fullhash = p->fullhash;
    void *extraargs = p->extraargs;

    void *toku_value = 0;
    long size = 0;
    LSN written_lsn = ZERO_LSN;

    WHEN_TRACE_CT(printf("%s:%d CT: fetch_callback(%lld...)\n", __FILE__, __LINE__, key));    

    cachetable_unlock(ct);

    int r = fetch_callback(cf, key, fullhash, &toku_value, &size, extraargs, &written_lsn);

    cachetable_lock(ct);

    if (r) {
        cachetable_remove_pair(ct, p);
        p->state = CTPAIR_INVALID;
        if (p->cq) {
            workqueue_enq(p->cq, &p->asyncwork, 1);
            return r;
        }
        abort_fetch_pair(p);
        return r;
    } else {
        lru_touch(ct, p);
        p->value = toku_value;
        p->written_lsn = written_lsn;
        p->size = size;
        ct->size_current += size;
        if (p->cq) {
            workqueue_enq(p->cq, &p->asyncwork, 1);
            return 0;
        }
        p->state = CTPAIR_IDLE;
        rwlock_write_unlock(&p->rwlock);
        if (0) printf("%s:%d %"PRId64" complete\n", __FUNCTION__, __LINE__, key.b);
        return 0;
    }
}

static void cachetable_complete_write_pair (CACHETABLE ct, PAIR p, BOOL do_remove);

// Write a pair to storage
// Effects: an exclusive lock on the pair is obtained, the write callback is called,
// the pair dirty state is adjusted, and the write is completed.  The write_me boolean
// is true when the pair is dirty and the pair is requested to be written.  The keep_me
// boolean is true, so the pair is not yet evicted from the cachetable.
// Requires: This thread must hold the write lock for the pair.
static void cachetable_write_pair(CACHETABLE ct, PAIR p) {
    rwlock_read_lock(&ct->pending_lock, ct->mutex);

    // helgrind
    CACHETABLE_FLUSH_CALLBACK flush_callback = p->flush_callback;
    CACHEFILE cachefile = p->cachefile;
    CACHEKEY key = p->key;
    void *value = p->value;
    void *extraargs = p->extraargs;
    long size = p->size;
    BOOL dowrite = (BOOL)(p->dirty && p->write_me);
    BOOL for_checkpoint = p->checkpoint_pending;

    //Must set to FALSE before releasing cachetable lock
    p->checkpoint_pending = FALSE;   // This is the only place this flag is cleared.
    cachetable_unlock(ct);

    // write callback
    flush_callback(cachefile, key, value, extraargs, size, dowrite, TRUE, for_checkpoint);
#if DO_CALLBACK_USLEEP
    usleep(DO_CALLBACK_USLEEP);
#endif
#if DO_CALLBACK_BUSYWAIT
    struct timeval tstart;
    gettimeofday(&tstart, 0);
    long long ltstart = tstart.tv_sec * 1000000 + tstart.tv_usec;
    while (1) {
        struct timeval t;
        gettimeofday(&t, 0);
        long long lt = t.tv_sec * 1000000 + t.tv_usec;
        if (lt - ltstart > DO_CALLBACK_BUSYWAIT)
            break;
    }
#endif

    cachetable_lock(ct);

    // the pair is no longer dirty once written
    if (p->dirty && p->write_me)
        p->dirty = CACHETABLE_CLEAN;

    assert(!p->checkpoint_pending);
    rwlock_read_unlock(&ct->pending_lock);

    // stuff it into a completion queue for delayed completion if a completion queue exists
    // otherwise complete the write now
    if (p->cq)
        workqueue_enq(p->cq, &p->asyncwork, 1);
    else
        cachetable_complete_write_pair(ct, p, p->remove_me);
}

// complete the write of a pair by reseting the writing flag, adjusting the write
// pending size, and maybe removing the pair from the cachetable if there are no
// references to it

static void cachetable_complete_write_pair (CACHETABLE ct, PAIR p, BOOL do_remove) {
    p->cq = 0;
    p->state = CTPAIR_IDLE;

    // maybe wakeup any stalled writers when the pending writes fall below 
    // 1/8 of the size of the cachetable
    ct->size_writing -= p->size; 
    assert(ct->size_writing >= 0);
    if (8*ct->size_writing <= ct->size_current)
        workqueue_wakeup_write(&ct->wq, 0);

    rwlock_write_unlock(&p->rwlock);
    if (do_remove)
        cachetable_maybe_remove_and_free_pair(ct, p);
}

// flush and remove a pair from the cachetable.  the callbacks are run by a thread in
// a thread pool.

static void flush_and_maybe_remove (CACHETABLE ct, PAIR p, BOOL write_me) {
    rwlock_write_lock(&p->rwlock, ct->mutex);
    p->state = CTPAIR_WRITING;
    assert(ct->size_writing>=0);
    ct->size_writing += p->size;
    assert(ct->size_writing >= 0);
    p->write_me = write_me;
    p->remove_me = TRUE;
#if DO_WORKER_THREAD
    WORKITEM wi = &p->asyncwork;
    workitem_init(wi, cachetable_writer, p);
    // evictions without a write or unpinned pair's that are clean
    // can be run in the current thread
    if (!p->write_me || (!rwlock_readers(&p->rwlock) && !p->dirty)) {
        cachetable_write_pair(ct, p);
    } else {
#if !TOKU_CACHETABLE_DO_EVICT_FROM_WRITER
        p->remove_me = FALSE;           // run the remove on the main thread
#endif
        workqueue_enq(&ct->wq, wi, 0);
    }
#else
    cachetable_write_pair(ct, p);
#endif
}

static int maybe_flush_some (CACHETABLE ct, long size) {
    int r = 0;
again:
    if (size + ct->size_current > ct->size_limit + ct->size_writing) {
	{
	    //unsigned long rss __attribute__((__unused__)) = check_max_rss();
	    //printf("this-size=%.6fMB projected size = %.2fMB  limit=%2.fMB  rss=%2.fMB\n", size/(1024.0*1024.0), (size+t->size_current)/(1024.0*1024.0), t->size_limit/(1024.0*1024.0), rss/256.0);
	    //struct mallinfo m = mallinfo();
	    //printf(" arena=%d hblks=%d hblkhd=%d\n", m.arena, m.hblks, m.hblkhd);
	}
        /* Try to remove one. */
	PAIR remove_me;
	for (remove_me = ct->tail; remove_me; remove_me = remove_me->prev) {
	    if (remove_me->state == CTPAIR_IDLE && !rwlock_users(&remove_me->rwlock)) {
		flush_and_maybe_remove(ct, remove_me, TRUE);
		goto again;
	    }
	}
	/* All were pinned. */
	//printf("All are pinned\n");
	return 0; // Don't indicate an error code.  Instead let memory get overfull.
    }

    if ((4 * ct->n_in_table < ct->table_size) && ct->table_size > 4)
        cachetable_rehash(ct, ct->table_size/2);

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
                                 long size,
                                 CACHETABLE_FLUSH_CALLBACK flush_callback,
                                 CACHETABLE_FETCH_CALLBACK fetch_callback,
                                 void *extraargs, 
                                 enum cachetable_dirty dirty,
                                 LSN   written_lsn) {
    TAGMALLOC(PAIR, p);
    assert(p);
    memset(p, 0, sizeof *p);
    ctpair_add_ref(p);
    p->cachefile = cachefile;
    p->key = key;
    p->value = value;
    p->fullhash = fullhash;
    p->dirty = dirty;
    p->size = size;
    p->state = state;
    p->flush_callback = flush_callback;
    p->fetch_callback = fetch_callback;
    p->extraargs = extraargs;
    p->modified_lsn.lsn = 0;
    p->written_lsn  = written_lsn;
    p->fullhash = fullhash;
    p->next = p->prev = 0;
    rwlock_init(&p->rwlock);
    p->cq = 0;
    lru_add_to_list(ct, p);
    u_int32_t h = fullhash & (ct->table_size-1);
    p->hash_chain = ct->table[h];
    ct->table[h] = p;
    ct->n_in_table++;
    ct->size_current += size;
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
           cachetable_miss, cachetable_hit, cachetable_wait_reading, cachetable_wait);
}

static void
note_hash_count (int count) {
    if (count>=hash_histogram_max) count=hash_histogram_max-1;
    hash_histogram[count]++;
}

int toku_cachetable_put(CACHEFILE cachefile, CACHEKEY key, u_int32_t fullhash, void*value, long size,
			CACHETABLE_FLUSH_CALLBACK flush_callback, 
                        CACHETABLE_FETCH_CALLBACK fetch_callback, void *extraargs) {
    WHEN_TRACE_CT(printf("%s:%d CT cachetable_put(%lld)=%p\n", __FILE__, __LINE__, key, value));
    CACHETABLE ct = cachefile->cachetable;
    int count=0;
    cachetable_lock(ct);
    cachetable_wait_write(ct);
    {
	PAIR p;
	for (p=ct->table[fullhash&(cachefile->cachetable->table_size-1)]; p; p=p->hash_chain) {
	    count++;
	    if (p->key.b==key.b && p->cachefile==cachefile) {
		// Semantically, these two asserts are not strictly right.  After all, when are two functions eq?
		// In practice, the functions better be the same.
		assert(p->flush_callback==flush_callback);
		assert(p->fetch_callback==fetch_callback);
                rwlock_read_lock(&p->rwlock, ct->mutex);
		note_hash_count(count);
                cachetable_unlock(ct);
		return -1; /* Already present. */
	    }
	}
    }
    int r;
    if ((r=maybe_flush_some(ct, size))) {
        cachetable_unlock(ct);
        return r;
    }
    // flushing could change the table size, but wont' change the fullhash
    PAIR p = cachetable_insert_at(ct, cachefile, key, value, CTPAIR_IDLE, fullhash, size, flush_callback, fetch_callback, extraargs, CACHETABLE_DIRTY, ZERO_LSN);
    assert(p);
    rwlock_read_lock(&p->rwlock, ct->mutex);
    note_hash_count(count);
    cachetable_unlock(ct);
    return 0;
}

#if TOKU_DO_WAIT_TIME
static u_int64_t tdelta(struct timeval *tnew, struct timeval *told) {
    return (tnew->tv_sec * 1000000ULL + tnew->tv_usec) - (told->tv_sec * 1000000ULL + told->tv_usec);
}
#endif

// for debug 
static PAIR write_for_checkpoint_pair = NULL;

// On entry: hold the ct lock
// On exit:  the node is written out
// Method:   take write lock
//           if still pending write out the node
//           release the write lock.
static void
write_pair_for_checkpoint (CACHETABLE ct, PAIR p)
{
    write_for_checkpoint_pair = p;
    rwlock_write_lock(&p->rwlock, ct->mutex); // grab an exclusive lock on the pair
    assert(p->state!=CTPAIR_WRITING);         // if we have the write lock, no one else should be writing out the node
    if (p->checkpoint_pending) {
        // this is essentially a flush_and_maybe_remove except that
        // we already have p->rwlock and we just do the write in our own thread.
        assert(p->dirty); // it must be dirty if its pending.
        p->state = CTPAIR_WRITING; //most of this code should run only if NOT ALREADY CTPAIR_WRITING
        assert(ct->size_writing>=0);
        ct->size_writing += p->size;
        assert(ct->size_writing>=0);
        p->write_me = TRUE;
        p->remove_me = FALSE;
        cachetable_write_pair(ct, p);    // releases the write lock on the pair
    }
    else {
	rwlock_write_unlock(&p->rwlock); // didn't call cachetable_write_pair so we have to unlock it ourselves.
    }
    write_for_checkpoint_pair = NULL;
}

// for debugging
// valid only if this function is called only by a single thread
static u_int64_t get_and_pin_footprint = 0;

static CACHEFILE get_and_pin_cachefile = NULL;
static CACHEKEY  get_and_pin_key       = {0};
static u_int32_t get_and_pin_fullhash  = 0;            


int toku_cachetable_get_and_pin(CACHEFILE cachefile, CACHEKEY key, u_int32_t fullhash, void**value, long *sizep,
			        CACHETABLE_FLUSH_CALLBACK flush_callback, 
                                CACHETABLE_FETCH_CALLBACK fetch_callback, void *extraargs) {
    CACHETABLE ct = cachefile->cachetable;
    PAIR p;
    int count=0;


    get_and_pin_footprint = 1;

    cachetable_lock(ct);
    get_and_pin_cachefile = cachefile;
    get_and_pin_key       = key;
    get_and_pin_fullhash  = fullhash;            
    
    get_and_pin_footprint = 2;
    cachetable_wait_write(ct);
    get_and_pin_footprint = 3;
    for (p=ct->table[fullhash&(ct->table_size-1)]; p; p=p->hash_chain) {
	count++;
	if (p->key.b==key.b && p->cachefile==cachefile) {
#if TOKU_DO_WAIT_TIME
	    struct timeval t0;
	    int do_wait_time = 0;
#endif
            if (p->rwlock.writer || p->rwlock.want_write) {
                if (p->state == CTPAIR_READING)
                    cachetable_wait_reading++;
                else
                    cachetable_wait++;
#if TOKU_DO_WAIT_TIME
		do_wait_time = 1;
		gettimeofday(&t0, NULL);
#endif
            }
	    if (p->checkpoint_pending) {
		get_and_pin_footprint = 4;		
		write_pair_for_checkpoint(ct, p);
	    }
	    // still have the cachetable lock
	    // TODO: #1398  kill this hack before it multiplies further
	    // This logic here to prevent deadlock that results when a query pins a node,
	    // then the straddle callback creates a cursor that pins it again.  If 
	    // toku_cachetable_end_checkpoint() is called between those two calls to pin
	    // the node, then the checkpoint function waits for the first pin to be released
	    // while the callback waits for the checkpoint function to release the write
	    // lock.  The work-around is to have an unfair rwlock mechanism that favors the 
	    // reader.
#ifdef  BRT_LEVEL_STRADDLE_CALLBACK_LOGIC_NOT_READY
	    if (STRADDLE_HACK_INSIDE_CALLBACK) {
		get_and_pin_footprint = 6;
		rwlock_prefer_read_lock(&p->rwlock, ct->mutex);
	    }
	    else
#endif
		{
		    get_and_pin_footprint = 7;
		    rwlock_read_lock(&p->rwlock, ct->mutex);
		}
#if TOKU_DO_WAIT_TIME
	    if (do_wait_time) {
		struct timeval tnow;
		gettimeofday(&tnow, NULL);
		cachetable_wait_time += tdelta(&tnow, &t0);
	    }
#endif
	    get_and_pin_footprint = 8;
            if (p->state == CTPAIR_INVALID) {
		get_and_pin_footprint = 9;
                rwlock_read_unlock(&p->rwlock);
                if (rwlock_users(&p->rwlock) == 0)
                    ctpair_destroy(p);
                cachetable_unlock(ct);
		get_and_pin_footprint = 0;
                return ENODEV;
            }
	    lru_touch(ct,p);
	    *value = p->value;
            if (sizep) *sizep = p->size;
            cachetable_hit++;
	    note_hash_count(count);
            cachetable_unlock(ct);
	    WHEN_TRACE_CT(printf("%s:%d cachtable_get_and_pin(%lld)--> %p\n", __FILE__, __LINE__, key, *value));
	    get_and_pin_footprint = 0;
	    return 0;
	}
    }
    get_and_pin_footprint = 9;
    note_hash_count(count);
    int r;
    // Note.  hashit(t,key) may have changed as a result of flushing.  But fullhash won't have changed.
    {
	p = cachetable_insert_at(ct, cachefile, key, zero_value, CTPAIR_READING, fullhash, zero_size, flush_callback, fetch_callback, extraargs, CACHETABLE_CLEAN, ZERO_LSN);
        assert(p);
	get_and_pin_footprint = 10;
        rwlock_write_lock(&p->rwlock, ct->mutex);
#if TOKU_DO_WAIT_TIME
	struct timeval t0;
	gettimeofday(&t0, NULL);
#endif
        r = cachetable_fetch_pair(ct, cachefile, p);
        if (r) {
            cachetable_unlock(ct);
	    get_and_pin_footprint = 0;
            return r;
        }
        cachetable_miss++;
#if TOKU_DO_WAIT_TIME
	struct timeval tnow;
	gettimeofday(&tnow, NULL);
	cachetable_miss_time += tdelta(&tnow, &t0);
#endif
	get_and_pin_footprint = 11;
        rwlock_read_lock(&p->rwlock, ct->mutex);
        assert(p->state == CTPAIR_IDLE);

	*value = p->value;
        if (sizep) *sizep = p->size;
    }
    get_and_pin_footprint = 12;
    r = maybe_flush_some(ct, 0);
    cachetable_unlock(ct);
    WHEN_TRACE_CT(printf("%s:%d did fetch: cachtable_get_and_pin(%lld)--> %p\n", __FILE__, __LINE__, key, *value));
    get_and_pin_footprint = 0;
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
    for (p=ct->table[fullhash&(ct->table_size-1)]; p; p=p->hash_chain) {
	count++;
	if (p->key.b==key.b && p->cachefile==cachefile) {
            if (p->state == CTPAIR_IDLE && //If not idle, will require a stall and/or will be clean once it is idle, or might be GONE once not idle
                !p->checkpoint_pending &&  //If checkpoint pending, we would need to first write it, which would make it clean
                p->dirty &&
                rwlock_try_prefer_read_lock(&p->rwlock, ct->mutex) == 0 //Grab read lock.  If any stall would be necessary that means it would be clean AFTER the stall, so don't even try to stall
            ) {
                *value = p->value;
                lru_touch(ct,p);
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

//Used by shortcut query path.
//Same as toku_cachetable_maybe_get_and_pin except that we don't care if the node is clean or dirty (return the node regardless).
//All other conditions remain the same.
int toku_cachetable_maybe_get_and_pin_clean (CACHEFILE cachefile, CACHEKEY key, u_int32_t fullhash, void**value) {
    CACHETABLE ct = cachefile->cachetable;
    PAIR p;
    int count = 0;
    int r = -1;
    cachetable_lock(ct);
    for (p=ct->table[fullhash&(ct->table_size-1)]; p; p=p->hash_chain) {
	count++;
	if (p->key.b==key.b && p->cachefile==cachefile) {
            if (p->state == CTPAIR_IDLE && //If not idle, will require a stall and/or might be GONE once not idle
                !p->checkpoint_pending &&  //If checkpoint pending, we would need to first write it, which would make it clean (if the pin would be used for writes.  If would be used for read-only we could return it, but that would increase complexity)
                rwlock_try_prefer_read_lock(&p->rwlock, ct->mutex) == 0 //Grab read lock only if no stall required
            ) {
                *value = p->value;
                lru_touch(ct,p);
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


int toku_cachetable_unpin(CACHEFILE cachefile, CACHEKEY key, u_int32_t fullhash, enum cachetable_dirty dirty, long size)
// size==0 means that the size didn't change.
{
    CACHETABLE ct = cachefile->cachetable;
    PAIR p;
    WHEN_TRACE_CT(printf("%s:%d unpin(%lld)", __FILE__, __LINE__, key));
    //printf("%s:%d is dirty now=%d\n", __FILE__, __LINE__, dirty);
    int count = 0;
    int r = -1;
    //assert(fullhash == toku_cachetable_hash(cachefile, key));
    cachetable_lock(ct);
    for (p=ct->table[fullhash&(ct->table_size-1)]; p; p=p->hash_chain) {
	count++;
	if (p->key.b==key.b && p->cachefile==cachefile) {
	    assert(rwlock_readers(&p->rwlock)>0);
            rwlock_read_unlock(&p->rwlock);
	    if (dirty) p->dirty = CACHETABLE_DIRTY;
            if (size != 0) {
                ct->size_current -= p->size; if (p->state == CTPAIR_WRITING) ct->size_writing -= p->size;
                p->size = size;
                ct->size_current += p->size; if (p->state == CTPAIR_WRITING) ct->size_writing += p->size;
            }
	    WHEN_TRACE_CT(printf("[count=%lld]\n", p->pinned));
	    {
		if ((r=maybe_flush_some(ct, 0))) {
                    cachetable_unlock(ct);
                    return r;
                }
	    }
            r = 0; // we found one
            break;
	}
    }
    note_hash_count(count);
    cachetable_unlock(ct);
    return r;
}

int toku_cachefile_prefetch(CACHEFILE cf, CACHEKEY key, u_int32_t fullhash,
                            CACHETABLE_FLUSH_CALLBACK flush_callback, 
                            CACHETABLE_FETCH_CALLBACK fetch_callback, 
                            void *extraargs) {
    if (0) printf("%s:%d %"PRId64"\n", __FUNCTION__, __LINE__, key.b);
    CACHETABLE ct = cf->cachetable;
    cachetable_lock(ct);
    // lookup
    PAIR p;
    for (p = ct->table[fullhash&(ct->table_size-1)]; p; p = p->hash_chain)
	if (p->key.b==key.b && p->cachefile==cf)
            break;

    // if not found then create a pair in the READING state and fetch it
    if (p == 0) {
	p = cachetable_insert_at(ct, cf, key, zero_value, CTPAIR_READING, fullhash, zero_size, flush_callback, fetch_callback, extraargs, CACHETABLE_CLEAN, ZERO_LSN);
        assert(p);
        rwlock_write_lock(&p->rwlock, ct->mutex);
#if DO_WORKER_THREAD
        workitem_init(&p->asyncwork, cachetable_reader, p);
        workqueue_enq(&ct->wq, &p->asyncwork, 0);
#else
        cachetable_fetch_pair(ct, cf, p);
#endif
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
    // Now go through the LRU chain, make sure everything in the LRU chain is hashed, and set the verify flag.
    {
	PAIR p;
	for (p=ct->head; p; p=p->next) {
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
	    fprintf(stderr, "Something in the LRU chain is not hashed\n");
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
	for (p=ct->head; p; p=p->next) {
	    assert(p->cachefile!=cf);
	}
    }
}

// Flush (write to disk) all of the pairs that belong to a cachefile (or all pairs if 
// the cachefile is NULL.
// Must be holding cachetable lock on entry.
static int cachetable_flush_cachefile(CACHETABLE ct, CACHEFILE cf) {
    unsigned nfound = 0;
    struct workqueue cq;
    workqueue_init(&cq);

    // find all of the pairs owned by a cachefile and redirect their completion
    // to a completion queue.  flush and remove pairs in the IDLE state if they
    // are dirty.  pairs in the READING or WRITING states are already in the
    // work queue.
    unsigned i;

    unsigned num_pairs = 0;
    unsigned list_size = 16;
    PAIR *list = NULL;
    XMALLOC_N(list_size, list);
    //It is not safe to loop through the table (and hash chains) if you can
    //release the cachetable lock at any point within.

    //Make a list of pairs that belong to this cachefile.
    //Add a reference to them.
    for (i=0; i < ct->table_size; i++) {
	PAIR p;
	for (p = ct->table[i]; p; p = p->hash_chain) {
 	    if (cf == 0 || p->cachefile==cf) {
                ctpair_add_ref(p);
                list[num_pairs] = p;
                num_pairs++;
                if (num_pairs == list_size) {
                    list_size *= 2;
                    XREALLOC_N(list_size, list);
                }
            }
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
            if (p->state == CTPAIR_IDLE)
                flush_and_maybe_remove(ct, p, TRUE); //TODO: 1485 If this is being removed, why is it counted in nfound?
                                                     //TODO: 1485 How are things being added to the queue?
        }
        ctpair_destroy(p);     //Release our reference
    }
    toku_free(list);

    // wait for all of the pairs in the work queue to complete
    for (i=0; i<nfound; i++) {
        cachetable_unlock(ct);
        WORKITEM wi = 0;
        //This workqueue's mutex is NOT the cachetable lock.
        //You must not be holding the cachetable lock during the dequeue.
        int r = workqueue_deq(&cq, &wi, 1); assert(r == 0);
        cachetable_lock(ct);
        PAIR p = workitem_arg(wi);
        p->cq = 0;
        if (p->state == CTPAIR_READING) { //TODO: 1485 Doesn't this mean someone ELSE is holding a lock?
            rwlock_write_unlock(&p->rwlock);  //TODO: 1485 When did we grab a write lock? (the other person who grabbed the read (write) lock?
                                              //Does this mean no one has a pin?  since it never finished...
                                              //  POSSIBLE CAUSE
            cachetable_maybe_remove_and_free_pair(ct, p); //TODO: 1485 MUST be removed.  Can't be 'maybe_remove'
        } else if (p->state == CTPAIR_WRITING) { //TODO: 1485 This could mean WE or SOMEONE ELSE is holding a lock, right?  Can't be both.
                                                 //Someone else could have a PIN!  This could be the cause.
            cachetable_complete_write_pair(ct, p, TRUE);  //TODO: 1485 MUST be removed.  Can't be 'maybe_remove'
        } else if (p->state == CTPAIR_INVALID) {
            abort_fetch_pair(p);
        } else
            assert(0);
    }
    workqueue_destroy(&cq);
    assert_cachefile_is_flushed_and_removed(ct, cf);

    if ((4 * ct->n_in_table < ct->table_size) && (ct->table_size>4))
        cachetable_rehash(ct, ct->table_size/2);

    return 0;
}

/* Requires that no locks be held that are used by the checkpoint logic (ydb, etc.) */
void
toku_cachetable_minicron_shutdown(CACHETABLE ct) {
    int  r = toku_minicron_shutdown(&ct->checkpointer);
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
    int r;
    cachetable_lock(ct);
    if ((r=cachetable_flush_cachefile(ct, 0))) {
        cachetable_unlock(ct);
        return r;
    }
    u_int32_t i;
    for (i=0; i<ct->table_size; i++) {
	if (ct->table[i]) return -1;
    }
    assert(ct->size_writing == 0);
    rwlock_destroy(&ct->pending_lock);
    r = toku_pthread_mutex_destroy(&ct->openfd_mutex); assert(r == 0);
    cachetable_unlock(ct);
    toku_destroy_workers(&ct->wq, &ct->threadpool);
    toku_free(ct->table);
    toku_free(ct);
    *ctp = 0;
    return 0;
}

int toku_cachetable_unpin_and_remove (CACHEFILE cachefile, CACHEKEY key) {
    int r = ENOENT;
    // Removing something already present is OK.
    CACHETABLE ct = cachefile->cachetable;
    PAIR p;
    int count = 0;
    cachetable_lock(ct);
    u_int32_t fullhash = toku_cachetable_hash(cachefile, key);
    for (p=ct->table[fullhash&(ct->table_size-1)]; p; p=p->hash_chain) {
        count++;
	if (p->key.b==key.b && p->cachefile==cachefile) {
	    p->dirty = CACHETABLE_CLEAN; // clear the dirty bit.  We're just supposed to remove it.
	    assert(rwlock_readers(&p->rwlock)==1);
            rwlock_read_unlock(&p->rwlock);
            struct workqueue cq;
            workqueue_init(&cq);
            p->cq = &cq;
            if (p->state == CTPAIR_IDLE)
                flush_and_maybe_remove(ct, p, FALSE);
            cachetable_unlock(ct);
            WORKITEM wi = 0;
            r = workqueue_deq(&cq, &wi, 1);
            cachetable_lock(ct);
            PAIR pp = workitem_arg(wi);
            assert(r == 0 && pp == p);
            cachetable_complete_write_pair(ct, p, TRUE);
            workqueue_destroy(&cq);
            r = 0;
	    goto done;
	}
    }
 done:
    note_hash_count(count);
    cachetable_unlock(ct);
    return r;
}

static int
log_open_txn (OMTVALUE txnv, u_int32_t UU(index), void *loggerv) {
    TOKUTXN    txn    = txnv;
    TOKULOGGER logger = loggerv;
    if (toku_logger_txn_parent(txn)==NULL) { // only have to log the open root transactions
	int r = toku_log_xstillopen(logger, NULL, 0,
				    toku_txn_get_txnid(txn),
				    toku_txn_get_txnid(toku_logger_txn_parent(txn)));
	assert(r==0);
    }
    return 0;
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
	cachetable_lock(ct);
        //Make list of cachefiles to be included in checkpoint.
        //If refcount is 0, the cachefile is closing (performing a local checkpoint)
        {
            CACHEFILE cf;
            assert(ct->cachefiles_in_checkpoint==NULL);
            for (cf = ct->cachefiles; cf; cf=cf->next) {
                assert(!cf->is_closing); //Closing requires ydb lock (or in checkpoint).  Cannot happen.
                assert(cf->refcount>0);  //Must have a reference if not closing.
                //Incremement reference count of cachefile because we're using it for the checkpoint.
                //This will prevent closing during the checkpoint.
                cachefile_refup(cf);
                cf->next_in_checkpoint       = ct->cachefiles_in_checkpoint;
                ct->cachefiles_in_checkpoint = cf;
                cf->for_checkpoint           = TRUE;
            }
        }

	if (logger) {
	    // The checkpoint must be performed after the lock is acquired.
	    {
		LSN begin_lsn; // we'll need to store the lsn of the checkpoint begin in all the trees that are checkpointed.
		int r = toku_log_begin_checkpoint(logger, &begin_lsn, 0, 0);
		ct->lsn_of_checkpoint_in_progress = begin_lsn;
		assert(r==0);
	    }
	    // Log all the open transactions
	    {
                int r = toku_omt_iterate(logger->live_txns, log_open_txn, logger);
		assert(r==0);
	    }
	    // Log all the open files
	    {
                //Must loop through ALL open files (even if not included in checkpoint).
		CACHEFILE cf;
		for (cf = ct->cachefiles; cf; cf=cf->next) {
		    BYTESTRING bs = { strlen(cf->fname_relative_to_env), // don't include the NUL
				      cf->fname_relative_to_env };
		    int r = toku_log_fassociate(logger, NULL, 0, cf->filenum, bs);
		    assert(r==0);
		}
	    }
	}

        rwlock_write_lock(&ct->pending_lock, ct->mutex);
        for (i=0; i < ct->table_size; i++) {
            PAIR p;
            for (p = ct->table[i]; p; p=p->hash_chain) {
		assert(!p->checkpoint_pending);
                //Only include pairs belonging to cachefiles in the checkpoint
                if (!p->cachefile->for_checkpoint) continue;
                if (p->state == CTPAIR_READING)
                    continue;   // skip pairs being read as they will be clean
                else if (p->state == CTPAIR_IDLE || p->state == CTPAIR_WRITING) {
		    if (p->dirty) {
			p->checkpoint_pending = TRUE;
                        if (ct->pending_head)
                            ct->pending_head->pending_prev = p;
                        p->pending_next                = ct->pending_head;
                        p->pending_prev                = NULL;
                        ct->pending_head               = p;
		    }
                } else
                    assert(0);
            }
        }
        rwlock_write_unlock(&ct->pending_lock);

        //begin_checkpoint_userdata must be called AFTER all the pairs are marked as pending.
        //Once marked as pending, we own write locks on the pairs, which means the writer threads can't conflict.
	{
	    CACHEFILE cf;
	    for (cf = ct->cachefiles_in_checkpoint; cf; cf=cf->next_in_checkpoint) {
		if (cf->begin_checkpoint_userdata) {
		    int r = cf->begin_checkpoint_userdata(cf, ct->lsn_of_checkpoint_in_progress, cf->userdata);
		    assert(r==0);
		}
	    }
	}

	cachetable_unlock(ct);
    }
    return 0;
}


int
toku_cachetable_end_checkpoint(CACHETABLE ct, TOKULOGGER logger, char **error_string, void (*testcallback_f)(void*),  void * testextra) {
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

    cachetable_unlock(ct);

    {   // have just written data blocks, so next write the translation and header for each open dictionary
	CACHEFILE cf;
        //cachefiles_in_checkpoint is protected by the checkpoint_safe_lock
	for (cf = ct->cachefiles_in_checkpoint; cf; cf=cf->next_in_checkpoint) {
	    if (cf->checkpoint_userdata) {
		int r = cf->checkpoint_userdata(cf, cf->userdata);
		assert(r==0);
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
		int r = cf->end_checkpoint_userdata(cf, cf->userdata);
		assert(r==0);
	    }
	}
    }

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
            int r = toku_cachefile_close(&cf, logger, error_string, ct->lsn_of_checkpoint_in_progress);
            if (r!=0) {
                retval = r;
                goto panic;
            }
        }
    }

    // For testing purposes only.  Dictionary has been fsync-ed to disk but log has not yet been written.
    if (testcallback_f) 
	testcallback_f(testextra);      

    if (logger) {
	int r = toku_log_end_checkpoint(logger, NULL,
					1, // want the end_checkpoint to be fsync'd
					ct->lsn_of_checkpoint_in_progress.lsn, 0);
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

#if DO_WORKER_THREAD

// Worker thread function to write a pair from memory to its cachefile
static void cachetable_writer(WORKITEM wi) {
    PAIR p = workitem_arg(wi);
    CACHETABLE ct = p->cachefile->cachetable;
    cachetable_lock(ct);
    cachetable_write_pair(ct, p);
    cachetable_unlock(ct);
}

// Worker thread function to read a pair from a cachefile to memory
static void cachetable_reader(WORKITEM wi) {
    PAIR p = workitem_arg(wi);
    CACHETABLE ct = p->cachefile->cachetable;
    cachetable_lock(ct);
    int r = cachetable_fetch_pair(ct, p->cachefile, p);
#define DO_FLUSH_FROM_READER 0
    if (r == 0) {
#if DO_FLUSH_FROM_READER
        maybe_flush_some(ct, 0);
#else
        r = r;
#endif
    }
    cachetable_unlock(ct);
}

#endif

// debug functions

int toku_cachetable_assert_all_unpinned (CACHETABLE ct) {
    u_int32_t i;
    int some_pinned=0;
    cachetable_lock(ct);
    for (i=0; i<ct->table_size; i++) {
	PAIR p;
	for (p=ct->table[i]; p; p=p->hash_chain) {
	    assert(rwlock_readers(&p->rwlock)>=0);
	    if (rwlock_readers(&p->rwlock)) {
		//printf("%s:%d pinned: %"PRId64" (%p)\n", __FILE__, __LINE__, p->key.b, p->value);
		some_pinned=1;
	    }
	}
    }
    cachetable_unlock(ct);
    return some_pinned;
}

int toku_cachefile_count_pinned (CACHEFILE cf, int print_them) {
    u_int32_t i;
    int n_pinned=0;
    CACHETABLE ct = cf->cachetable;
    cachetable_lock(ct);
    for (i=0; i<ct->table_size; i++) {
	PAIR p;
	for (p=ct->table[i]; p; p=p->hash_chain) {
	    assert(rwlock_readers(&p->rwlock)>=0);
	    if (rwlock_readers(&p->rwlock) && (cf==0 || p->cachefile==cf)) {
		if (print_them) printf("%s:%d pinned: %"PRId64" (%p)\n", __FILE__, __LINE__, p->key.b, p->value);
		n_pinned++;
	    }
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
                printf(" {%"PRId64", %p, dirty=%d, pin=%d, size=%ld}", p->key.b, p->cachefile, (int) p->dirty, rwlock_readers(&p->rwlock), p->size);
            }
            printf("\n");
        }
    }
    cachetable_unlock(ct);
}

void toku_cachetable_get_state (CACHETABLE ct, int *num_entries_ptr, int *hash_size_ptr, long *size_current_ptr, long *size_limit_ptr) {
    cachetable_lock(ct);
    if (num_entries_ptr) 
        *num_entries_ptr = ct->n_in_table;
    if (hash_size_ptr)
        *hash_size_ptr = ct->table_size;
    if (size_current_ptr)
        *size_current_ptr = ct->size_current;
    if (size_limit_ptr)
        *size_limit_ptr = ct->size_limit;
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
                *pin_ptr = rwlock_readers(&p->rwlock);
            if (size_ptr)
                *size_ptr = p->size;
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
			     int (*close_userdata)(CACHEFILE, void*, char**, LSN),
			     int (*checkpoint_userdata)(CACHEFILE, void*),
			     int (*begin_checkpoint_userdata)(CACHEFILE, LSN, void*),
			     int (*end_checkpoint_userdata)(CACHEFILE, void*)) {
    cf->userdata = userdata;
    cf->close_userdata = close_userdata;
    cf->checkpoint_userdata = checkpoint_userdata;
    cf->begin_checkpoint_userdata = begin_checkpoint_userdata;
    cf->end_checkpoint_userdata = end_checkpoint_userdata;
}

void *toku_cachefile_get_userdata(CACHEFILE cf) {
    return cf->userdata;
}

int
toku_cachefile_fsync(CACHEFILE cf) {
    int r;

    if (cf->fname_relative_to_env==NULL) r = 0; //Don't fsync /dev/null
    else r = fsync(cf->fd);
    return r;
}

int toku_cachefile_redirect_nullfd (CACHEFILE cf) {
    int null_fd;
    struct fileid fileid;

    null_fd = open(DEV_NULL_FILE, O_WRONLY+O_BINARY);           
    assert(null_fd>=0);
    toku_os_get_unique_file_id(null_fd, &fileid);
    close(cf->fd);
    cf->fd = null_fd;
    if (cf->fname_relative_to_env) {
	toku_free(cf->fname_relative_to_env);
	cf->fname_relative_to_env = 0;
    }
    cachefile_init_filenum(cf, null_fd, NULL, fileid);
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
		result += p->size;
	    }
	}
    }
    return result;
}

