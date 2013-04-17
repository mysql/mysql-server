/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "toku_portability.h"
#include "memory.h"
#include "workqueue.h"
#include "threadpool.h"
#include "cachetable.h"
#include "cachetable-rwlock.h"
#include "toku_worker.h"

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

enum ctpair_state {
    CTPAIR_INVALID = 0, // invalid
    CTPAIR_IDLE = 1,    // in memory
    CTPAIR_READING = 2, // being read into memory
    CTPAIR_WRITING = 3, // being written from memory
};

typedef struct ctpair *PAIR;
struct ctpair {
    enum typ_tag tag;
    CACHEFILE cachefile;
    CACHEKEY key;
    void    *value;
    long     size;
    enum ctpair_state state;
    enum cachetable_dirty dirty;
    char     verify_flag;         // Used in verify_cachetable()
    BOOL     write_me;

    u_int32_t fullhash;

    CACHETABLE_FLUSH_CALLBACK flush_callback;
    CACHETABLE_FETCH_CALLBACK fetch_callback;
    void    *extraargs;

    PAIR     next,prev;           // In LRU list.
    PAIR     hash_chain;

    LSN      modified_lsn;       // What was the LSN when modified (undefined if not dirty)
    LSN      written_lsn;        // What was the LSN when written (we need to get this information when we fetch)

    struct ctpair_rwlock rwlock; // multiple get's, single writer
    struct workqueue *cq;        // writers sometimes return ctpair's using this queue
    struct workitem asyncwork;   // work item for the worker threads
};

static void * const zero_value = 0;
static int const zero_size = 0;

static inline void ctpair_destroy(PAIR p) {
    ctpair_rwlock_destroy(&p->rwlock);
    toku_free(p);
}

// The cachetable is as close to an ENV as we get.
struct cachetable {
    enum typ_tag tag;
    u_int32_t n_in_table;         // number of pairs in the hash table
    u_int32_t table_size;         // number of buckets in the hash table
    PAIR *table;                  // hash table
    PAIR  head,tail;              // of LRU list. head is the most recently used. tail is least recently used.
    CACHEFILE cachefiles;         // list of cachefiles that use this cachetable
    long size_current;            // the sum of the sizes of the pairs in the cachetable
    long size_limit;              // the limit to the sum of the pair sizes
    long size_writing;            // the sum of the sizes of the pairs being written
    LSN lsn_of_checkpoint;        // the most recent checkpoint in the log.
    TOKULOGGER logger;
    toku_pthread_mutex_t *mutex;  // coarse lock that protects the cachetable, the cachefiles, and the pair's
    struct workqueue wq;          // async work queue 
    THREADPOOL threadpool;        // pool of worker threads
    char checkpointing;           // checkpoint in progress
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

// Wait for writes to complete if the size in the write queue is 1/2 of 
// the cachetable
static inline void cachetable_wait_write(CACHETABLE ct) {
    while (2*ct->size_writing > ct->size_current) {
        workqueue_wait_write(&ct->wq, 0);
    }
}

struct cachefile {
    CACHEFILE next;
    u_int64_t refcount; /* CACHEFILEs are shared. Use a refcount to decide when to really close it.
			 * The reference count is one for every open DB.
			 * Plus one for every commit/rollback record.  (It would be harder to keep a count for every open transaction,
			 * because then we'd have to figure out if the transaction was already counted.  If we simply use a count for
			 * every record in the transaction, we'll be ok.  Hence we use a 64-bit counter to make sure we don't run out.
			 */
    int fd;       /* Bug: If a file is opened read-only, then it is stuck in read-only.  If it is opened read-write, then subsequent writers can write to it too. */
    BOOL is_dirty;      /* Has this been written to since it was closed? */
    CACHETABLE cachetable;
    struct fileid fileid;
    FILENUM filenum;
    char *fname;

    void *userdata;
    int (*close_userdata)(CACHEFILE cf, void *userdata, char **error_string); // when closing the last reference to a cachefile, first call this function.
    int (*checkpoint_userdata)(CACHEFILE cf, void *userdata); // when checkpointing a cachefile, call this function.
};

int toku_create_cachetable(CACHETABLE *result, long size_limit, LSN initial_lsn, TOKULOGGER logger) {
#if defined __linux__
    {
	static int did_mallopt = 0;
	if (!did_mallopt) {
	    mallopt(M_MMAP_THRESHOLD, 1024*64); // 64K and larger should be malloced with mmap().
	    did_mallopt = 1;
	}
    }
#endif
    TAGMALLOC(CACHETABLE, ct);
    if (ct == 0) return ENOMEM;
    ct->n_in_table = 0;
    ct->table_size = 4;
    MALLOC_N(ct->table_size, ct->table);
    assert(ct->table);
    ct->head = ct->tail = 0;
    u_int32_t i;
    for (i=0; i<ct->table_size; i++) {
	ct->table[i]=0;
    }
    ct->cachefiles = 0;
    ct->size_current = 0;
    ct->size_limit = size_limit;
    ct->size_writing = 0;
    ct->lsn_of_checkpoint = initial_lsn;
    ct->logger = logger;
    ct->checkpointing = 0;
    toku_init_workers(&ct->wq, &ct->threadpool);
    ct->mutex = workqueue_lock_ref(&ct->wq);
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

static void cachefile_init_filenum(CACHEFILE cf, int fd, const char *fname, struct fileid fileid) \
{
    cf->fd = fd;
    cf->fileid = fileid;
    cf->fname  = fname ? toku_strdup(fname) : 0;
}

// If something goes wrong, close the fd.  After this, the caller shouldn't close the fd, but instead should close the cachefile.
int toku_cachetable_openfd (CACHEFILE *cfptr, CACHETABLE ct, int fd, const char *fname) {
    int r;
    CACHEFILE extant;
    struct fileid fileid;
    r = toku_os_get_unique_file_id(fd, &fileid);
    if (r != 0) { 
        r=errno; close(fd); 
        return r;
    }
    for (extant = ct->cachefiles; extant; extant=extant->next) {
	if (memcmp(&extant->fileid, &fileid, sizeof(fileid))==0) {
	    r = close(fd);
            assert(r == 0);
	    extant->refcount++;
	    *cfptr = extant;
	    return 0;
	}
    }
 try_again:
    for (extant = ct->cachefiles; extant; extant=extant->next) {
	if (next_filenum_to_use.fileid==extant->filenum.fileid) {
	    next_filenum_to_use.fileid++;
	    goto try_again;
	}
    }
    {
        BOOL was_dirty = FALSE;
        r = toku_graceful_open(fname, &was_dirty);
        if (r!=0) {
            close(fd); 
            return r;
        }

        CACHEFILE MALLOC(newcf);
        newcf->cachetable = ct;
        newcf->filenum.fileid = next_filenum_to_use.fileid++;
        cachefile_init_filenum(newcf, fd, fname, fileid);
	newcf->refcount = 1;
	newcf->next = ct->cachefiles;
	ct->cachefiles = newcf;

	newcf->userdata = 0;
	newcf->close_userdata = 0;
	newcf->checkpoint_userdata = 0;
        newcf->is_dirty = was_dirty;

	*cfptr = newcf;
	return 0;
    }
}

int toku_cachetable_openf (CACHEFILE *cfptr, CACHETABLE ct, const char *fname, int flags, mode_t mode) {
    int fd = open(fname, flags+O_BINARY, mode);
    if (fd<0) return errno;
    return toku_cachetable_openfd (cfptr, ct, fd, fname);
}

WORKQUEUE toku_cachetable_get_workqueue(CACHETABLE ct) {
    return &ct->wq;
}

void toku_cachefile_get_workqueue_load (CACHEFILE cf, int *n_in_queue, int *n_threads) {
    CACHETABLE ct = cf->cachetable;
    *n_in_queue = workqueue_n_in_queue(&ct->wq, 1);
    *n_threads  = threadpool_get_current_threads(ct->threadpool);
}

int toku_cachefile_set_fd (CACHEFILE cf, int fd, const char *fname) {
    int r;
    struct fileid fileid;
    r=toku_os_get_unique_file_id(fd, &fileid);
    if (r != 0) { 
        r=errno; close(fd); return r; 
    }
    if (cf->close_userdata && (r = cf->close_userdata(cf, cf->userdata, 0))) {
        return r;
    }
    cf->close_userdata = NULL;
    cf->checkpoint_userdata = NULL;
    cf->userdata = NULL;

    close(cf->fd);
    cf->fd = -1;
    if (cf->fname) {
        toku_free(cf->fname);
        cf->fname = 0;
    }
    cachefile_init_filenum(cf, fd, fname, fileid);
    return 0;
}

int toku_cachefile_fd (CACHEFILE cf) {
    return cf->fd;
}

int toku_cachefile_truncate0 (CACHEFILE cf) {
    int r = toku_graceful_dirty(cachefile);
    if (r!=0) return r;
    int r = ftruncate(cf->fd, 0);
    if (r != 0)
        r = errno;
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

// Increment the reference count
void toku_cachefile_refup (CACHEFILE cf) {
    cf->refcount++;
}

int toku_cachefile_close (CACHEFILE *cfp, TOKULOGGER logger, char **error_string) {
    CACHEFILE cf = *cfp;
    CACHETABLE ct = cf->cachetable;
    cachetable_lock(ct);
    assert(cf->refcount>0);
    cf->refcount--;
    if (cf->refcount==0) {
	int r;
	if ((r = cachetable_flush_cachefile(ct, cf))) {
            //This is not a graceful shutdown; do not set file as clean.
            cachetable_unlock(ct);
            return r;
        }
	if (cf->close_userdata && (r = cf->close_userdata(cf, cf->userdata, error_string))) {
            //This is not a graceful shutdown; do not set file as clean.
	    cachetable_unlock(ct);
	    return r;
	}
        //Graceful shutdown.  'clean' the file.
	if ((r = toku_graceful_close(cf))) {
	    cachetable_unlock(ct);
            return r;
        }
	cf->close_userdata = NULL;
	cf->checkpoint_userdata = NULL;
	cf->userdata = NULL;
        cf->cachetable->cachefiles = remove_cf_from_list(cf, cf->cachetable->cachefiles);
        cachetable_unlock(ct);
	r = close(cf->fd);
	assert(r == 0);
        cf->fd = -1;
	if (logger) {
	    //assert(cf->fname);
	    //BYTESTRING bs = {.len=strlen(cf->fname), .data=cf->fname};
	    //r = toku_log_cfclose(logger, 0, 0, bs, cf->filenum);
	}
	if (cf->fname) toku_free(cf->fname);
	toku_free(cf);
	*cfp=0;
	return r;
    } else {
        cachetable_unlock(ct);
	*cfp=0;
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

// Predicate to determine if a node must be renamed.  Nodes are renamed on the time they are written
// after a checkpoint.
//   Thus we need to rename it if it is dirty,
//    if it has been modified within the current checkpoint regime (hence non-strict inequality)
//    and the last time it was written was in a previous checkpoint regime (strict inequality)

static BOOL need_to_rename_p (CACHETABLE ct, PAIR p) {
    return (BOOL)(p->dirty
		  && p->modified_lsn.lsn>=ct->lsn_of_checkpoint.lsn   // nonstrict
		  && p->written_lsn.lsn < ct->lsn_of_checkpoint.lsn); // strict
}

// Remove a pair from the cachetable
// Effects: the pair is removed from the LRU list and from the cachetable's hash table.
// The size of the objects in the cachetable is adjusted by the size of the pair being
// removed.

static void cachetable_remove_pair (CACHETABLE ct, PAIR p) {
    lru_remove(ct, p);

    assert(ct->n_in_table>0);
    ct->n_in_table--;
    // Remove it from the hash chain.
    {
	unsigned int h = p->fullhash&(ct->table_size-1);
	ct->table[h] = remove_from_hash_chain (p, ct->table[h]);
    }
    ct->size_current -= p->size; assert(ct->size_current >= 0);
}

// Maybe remove a pair from the cachetable and free it, depending on whether
// or not there are any threads interested in the pair.  The flush callback
// is called with write_me and keep_me both false, and the pair is destroyed.

static void cachetable_maybe_remove_and_free_pair (CACHETABLE ct, PAIR p) {
    if (ctpair_users(&p->rwlock) == 0) {
        cachetable_remove_pair(ct, p);

        // helgrind
        CACHETABLE_FLUSH_CALLBACK flush_callback = p->flush_callback;
        CACHEFILE cachefile = p->cachefile;
        CACHEKEY key = p->key;
        void *value = p->value;
        void *extraargs = p->extraargs;
        long size = p->size;
        LSN lsn_of_checkpoint = ct->lsn_of_checkpoint;
        BOOL need_to_rename = need_to_rename_p(ct, p);

        cachetable_unlock(ct);

        flush_callback(cachefile, key, value, extraargs, size, FALSE, FALSE, 
                       lsn_of_checkpoint, need_to_rename);

        cachetable_lock(ct);

        ctpair_destroy(p);
    }
}

static void cachetable_abort_fetch_pair(CACHETABLE ct, PAIR p) {
    cachetable_remove_pair(ct, p);
    p->state = CTPAIR_INVALID;
    ctpair_write_unlock(&p->rwlock);
    if (ctpair_users(&p->rwlock) == 0)
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
        if (p->cq) {
            workqueue_enq(p->cq, &p->asyncwork, 1);
            return r;
        }
        cachetable_abort_fetch_pair(ct, p);
        return r;
    }

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
    ctpair_write_unlock(&p->rwlock);
    return 0;
}

static void cachetable_complete_write_pair (CACHETABLE ct, PAIR p, BOOL do_remove);

// Write a pair to storage
// Effects: an exclusive lock on the pair is obtained, the write callback is called,
// the pair dirty state is adjusted, and the write is completed.  The write_me boolean
// is true when the pair is dirty and the pair is requested to be written.  The keep_me
// boolean is true, so the pair is not yet evicted from the cachetable.

static void cachetable_write_pair(CACHETABLE ct, PAIR p) {
    ctpair_write_lock(&p->rwlock, ct->mutex);

    // helgrind
    CACHETABLE_FLUSH_CALLBACK flush_callback = p->flush_callback;
    CACHEFILE cachefile = p->cachefile;
    CACHEKEY key = p->key;
    void *value = p->value;
    void *extraargs = p->extraargs;
    long size = p->size;
    BOOL dowrite = (BOOL)(p->dirty && p->write_me);
    LSN lsn_of_checkpoint = ct->lsn_of_checkpoint;
    BOOL need_to_rename = need_to_rename_p(ct, p);

    cachetable_unlock(ct);

    // write callback
    flush_callback(cachefile, key, value, extraargs, size, dowrite, TRUE, lsn_of_checkpoint, need_to_rename);
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

    // stuff it into a completion queue for delayed completion if a completion queue exists
    // otherwise complete the write now
    if (p->cq)
        workqueue_enq(p->cq, &p->asyncwork, 1);
    else
        cachetable_complete_write_pair(ct, p, TRUE);
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

    ctpair_write_unlock(&p->rwlock);
    if (do_remove)
        cachetable_maybe_remove_and_free_pair(ct, p);
}

// flush and remove a pair from the cachetable.  the callbacks are run by a thread in
// a thread pool.

static void flush_and_maybe_remove (CACHETABLE ct, PAIR p, BOOL write_me) {
    p->state = CTPAIR_WRITING;
    ct->size_writing += p->size; assert(ct->size_writing >= 0);
    p->write_me = write_me;
#if DO_WORKER_THREAD
    WORKITEM wi = &p->asyncwork;
    workitem_init(wi, cachetable_writer, p);
    // evictions without a write or unpinned paris that are clean
    // can be run in the current thread
    if (!p->write_me || (!ctpair_pinned(&p->rwlock) && !p->dirty)) {
        cachetable_write_pair(ct, p);
    } else {
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
	    if (remove_me->state == CTPAIR_IDLE && !ctpair_users(&remove_me->rwlock)) {
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
    ctpair_rwlock_init(&p->rwlock);
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
void print_hash_histogram (void) {
    int i;
    for (i=0; i<hash_histogram_max; i++)
	if (hash_histogram[i]) printf("%d:%llu ", i, hash_histogram[i]);
    printf("\n");
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
                ctpair_read_lock(&p->rwlock, ct->mutex);
                cachetable_unlock(ct);
		note_hash_count(count);
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
    ctpair_read_lock(&p->rwlock, ct->mutex);
    cachetable_unlock(ct);
    note_hash_count(count);
    return 0;
}

int toku_cachetable_get_and_pin(CACHEFILE cachefile, CACHEKEY key, u_int32_t fullhash, void**value, long *sizep,
			        CACHETABLE_FLUSH_CALLBACK flush_callback, 
                                CACHETABLE_FETCH_CALLBACK fetch_callback, void *extraargs) {
    CACHETABLE ct = cachefile->cachetable;
    PAIR p;
    int count=0;
    cachetable_lock(ct);
    cachetable_wait_write(ct);
    for (p=ct->table[fullhash&(ct->table_size-1)]; p; p=p->hash_chain) {
	count++;
	if (p->key.b==key.b && p->cachefile==cachefile) {
            ctpair_read_lock(&p->rwlock, ct->mutex);
            if (p->state == CTPAIR_INVALID) {
                ctpair_read_unlock(&p->rwlock);
                if (ctpair_users(&p->rwlock) == 0)
                    ctpair_destroy(p);
                cachetable_unlock(ct);
                return ENODEV;
            }
	    lru_touch(ct,p);
	    *value = p->value;
            if (sizep) *sizep = p->size;
            cachetable_unlock(ct);
	    note_hash_count(count);
	    WHEN_TRACE_CT(printf("%s:%d cachtable_get_and_pin(%lld)--> %p\n", __FILE__, __LINE__, key, *value));
	    return 0;
	}
    }
    note_hash_count(count);
    int r;
    // Note.  hashit(t,key) may have changed as a result of flushing.  But fullhash won't have changed.
    {
	p = cachetable_insert_at(ct, cachefile, key, zero_value, CTPAIR_READING, fullhash, zero_size, flush_callback, fetch_callback, extraargs, CACHETABLE_CLEAN, ZERO_LSN);
        assert(p);
        ctpair_write_lock(&p->rwlock, ct->mutex);
        r = cachetable_fetch_pair(ct, cachefile, p);
        if (r) {
            cachetable_unlock(ct);
            return r;
        }
        ctpair_read_lock(&p->rwlock, ct->mutex);
        assert(p->state == CTPAIR_IDLE);

	*value = p->value;
        if (sizep) *sizep = p->size;
    }
    r = maybe_flush_some(ct, 0);
    cachetable_unlock(ct);
    WHEN_TRACE_CT(printf("%s:%d did fetch: cachtable_get_and_pin(%lld)--> %p\n", __FILE__, __LINE__, key, *value));
    return r;
}

// Lookup a key in the cachetable.  If it is found and it is not being written, then
// acquire a read lock on the pair, update the LRU list, and return sucess.  However,
// if it is being written, then allow the writer to evict it.  This prevents writers
// being suspended on a block that was just selected for eviction.
int toku_cachetable_maybe_get_and_pin (CACHEFILE cachefile, CACHEKEY key, u_int32_t fullhash, void**value) {
    CACHETABLE ct = cachefile->cachetable;
    PAIR p;
    int count = 0;
    cachetable_lock(ct);
    for (p=ct->table[fullhash&(ct->table_size-1)]; p; p=p->hash_chain) {
	count++;
	if (p->key.b==key.b && p->cachefile==cachefile && p->state == CTPAIR_IDLE) {
	    *value = p->value;
	    ctpair_read_lock(&p->rwlock, ct->mutex);
	    lru_touch(ct,p);
            cachetable_unlock(ct);
	    note_hash_count(count);
	    //printf("%s:%d cachetable_maybe_get_and_pin(%lld)--> %p\n", __FILE__, __LINE__, key, *value);
	    return 0;
	}
    }
    cachetable_unlock(ct);
    note_hash_count(count);
    return -1;
}


int toku_cachetable_unpin(CACHEFILE cachefile, CACHEKEY key, u_int32_t fullhash, enum cachetable_dirty dirty, long size) {
    CACHETABLE ct = cachefile->cachetable;
    PAIR p;
    WHEN_TRACE_CT(printf("%s:%d unpin(%lld)", __FILE__, __LINE__, key));
    //printf("%s:%d is dirty now=%d\n", __FILE__, __LINE__, dirty);
    int count = 0;
    //assert(fullhash == toku_cachetable_hash(cachefile, key));
    cachetable_lock(ct);
    for (p=ct->table[fullhash&(ct->table_size-1)]; p; p=p->hash_chain) {
	count++;
	if (p->key.b==key.b && p->cachefile==cachefile) {
	    assert(p->rwlock.pinned>0);
            ctpair_read_unlock(&p->rwlock);
	    if (dirty) p->dirty = CACHETABLE_DIRTY;
            if (size != 0) {
                ct->size_current -= p->size; if (p->state == CTPAIR_WRITING) ct->size_writing -= p->size;
                p->size = size;
                ct->size_current += p->size; if (p->state == CTPAIR_WRITING) ct->size_writing += p->size;
            }
	    WHEN_TRACE_CT(printf("[count=%lld]\n", p->pinned));
	    {
		int r;
		if ((r=maybe_flush_some(ct, 0))) {
                    cachetable_unlock(ct);
                    return r;
                }
	    }
            cachetable_unlock(ct);
	    note_hash_count(count);
	    return 0;
	}
    }
    cachetable_unlock(ct);
    note_hash_count(count);
    return -1;
}

int toku_cachefile_prefetch(CACHEFILE cf, CACHEKEY key, u_int32_t fullhash,
                            CACHETABLE_FLUSH_CALLBACK flush_callback, 
                            CACHETABLE_FETCH_CALLBACK fetch_callback, 
                            void *extraargs) {
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
        ctpair_write_lock(&p->rwlock, ct->mutex);
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
    cachetable_unlock(ct);
    note_hash_count(count);
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

// Flush all of the pairs that belong to a cachefile (or all pairs if 
// the cachefile is NULL.
static int cachetable_flush_cachefile(CACHETABLE ct, CACHEFILE cf) {
    unsigned nfound = 0;
    struct workqueue cq;
    workqueue_init(&cq);

    // find all of the pairs owned by a cachefile and redirect their completion
    // to a completion queue.  flush and remove pairs in the IDLE state if they
    // are dirty.  pairs in the READING or WRITING states are already in the
    // work queue.
    unsigned i;

    for (i=0; i < ct->table_size; i++) {
	PAIR p;
	for (p = ct->table[i]; p; p = p->hash_chain) {
 	    if (cf == 0 || p->cachefile==cf) {
                nfound++;
                p->cq = &cq;
                if (p->state == CTPAIR_IDLE)
                    flush_and_maybe_remove(ct, p, TRUE);
	    }
	}
    }

    // wait for all of the pairs in the work queue to complete
    for (i=0; i<nfound; i++) {
        cachetable_unlock(ct);
        WORKITEM wi = 0;
        int r = workqueue_deq(&cq, &wi, 1); assert(r == 0);
        cachetable_lock(ct);
        PAIR p = workitem_arg(wi);
        p->cq = 0;
        if (p->state == CTPAIR_READING)
            cachetable_abort_fetch_pair(ct, p);
        else if (p->state == CTPAIR_WRITING)
            cachetable_complete_write_pair(ct, p, TRUE);
        else
            assert(0);
    }
    workqueue_destroy(&cq);
    assert_cachefile_is_flushed_and_removed(ct, cf);

    if ((4 * ct->n_in_table < ct->table_size) && (ct->table_size>4))
        cachetable_rehash(ct, ct->table_size/2);

    return 0;
}

/* Require that it all be flushed. */
int toku_cachetable_close (CACHETABLE *ctp) {
    CACHETABLE ct=*ctp;
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
	    assert(p->rwlock.pinned==1);
            ctpair_read_unlock(&p->rwlock);
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
    cachetable_unlock(ct);
    note_hash_count(count);
    return r;
}

int toku_cachetable_checkpoint (CACHETABLE ct) {
    // Requires: Everything is unpinned.  (In the multithreaded version we have to wait for things to get unpinned and then
    //  grab them (or else the unpinner has to do something.)
    // Algorithm:  Write a checkpoint record to the log, noting the LSN of that record.
    //  Note the LSN of the previous checkpoint (stored in lsn_of_checkpoint)
    //  For every (unpinnned) dirty node in which the LSN is newer than the prev checkpoint LSN:
    //      flush the node (giving it a new nodeid, and fixing up the downpointer in the parent)
    // Watch out since evicting the node modifies the hash table.

    //?? This is a skeleton.  It compiles, but doesn't do anything reasonable yet.
    //??    log_the_checkpoint();

    struct workqueue cq;
    workqueue_init(&cq);

    cachetable_lock(ct);
    
    // set the checkpoint in progress flag. if already set then just return.
    if (!ct->checkpointing) {
        ct->checkpointing = 1;
        
        unsigned nfound = 0;
        unsigned i;
        for (i=0; i < ct->table_size; i++) {
            PAIR p;
            for (p = ct->table[i]; p; p=p->hash_chain) {
                // p->dirty && p->modified_lsn.lsn>ct->lsn_of_checkpoint.lsn
                if (p->state == CTPAIR_READING)
                    continue;   // skip pairs being read as they will be clean
                else if (p->state == CTPAIR_IDLE || p->state == CTPAIR_WRITING) {
                    nfound++;
                    p->cq = &cq;
                    // TODO force all IDLE pairs through the worker threads as that will
                    // serialize with any readers
                    if (p->state == CTPAIR_IDLE)
                        flush_and_maybe_remove(ct, p, TRUE);
                } else
                    assert(0);
            }
        }
        for (i=0; i<nfound; i++) {
            WORKITEM wi = 0;
            cachetable_unlock(ct);
            int r = workqueue_deq(&cq, &wi, 1); assert(r == 0);
            cachetable_lock(ct);
            PAIR p = workitem_arg(wi);
            assert(p->state == CTPAIR_IDLE || p->state == CTPAIR_WRITING);
            cachetable_complete_write_pair(ct, p, FALSE);
        }

	{
	    CACHEFILE cf;
	    for (cf = ct->cachefiles; cf; cf=cf->next) {
		if (cf->checkpoint_userdata) {
		    int r = cf->checkpoint_userdata(cf, cf->userdata);
		    assert(r==0);
		}
	    }
	}

        ct->checkpointing = 0; // clear the checkpoint in progress flag
    }

    cachetable_unlock(ct);
    workqueue_destroy(&cq);

    return 0;
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
    if (r == 0)
        maybe_flush_some(ct, 0);
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
	    assert(ctpair_pinned(&p->rwlock)>=0);
	    if (ctpair_pinned(&p->rwlock)) {
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
	    assert(ctpair_pinned(&p->rwlock)>=0);
	    if (ctpair_pinned(&p->rwlock) && (cf==0 || p->cachefile==cf)) {
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
                printf(" {%"PRId64", %p, dirty=%d, pin=%d, size=%ld}", p->key.b, p->cachefile, (int) p->dirty, p->rwlock.pinned, p->size);
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
                *pin_ptr = p->rwlock.pinned;
            if (size_ptr)
                *size_ptr = p->size;
            r = 0;
            break;
        }
    }
    cachetable_unlock(ct);    note_hash_count(count);
    return r;
}

void
toku_cachefile_set_userdata (CACHEFILE cf,
			     void *userdata,
			     int (*close_userdata)(CACHEFILE, void*, char**),
			     int (*checkpoint_userdata)(CACHEFILE, void*))
{
    cf->userdata = userdata;
    cf->close_userdata = close_userdata;
    cf->checkpoint_userdata = checkpoint_userdata;
}

void *toku_cachefile_get_userdata(CACHEFILE cf) {
    return cf->userdata;
}

int toku_cachefile_redirect_nullfd (CACHEFILE cf) {
    int null_fd;
    struct fileid fileid;

    null_fd = open(DEV_NULL_FILE, O_WRONLY+O_BINARY);           
    assert(null_fd>=0);
    toku_os_get_unique_file_id(null_fd, &fileid);
    close(cf->fd);
    cf->fd = null_fd;
    if (cf->fname) {
        toku_free(cf->fname);
        cf->fname = 0;
    }
    cachefile_init_filenum(cf, null_fd, NULL, fileid);
    return 0;
}

static toku_pthread_mutex_t graceful_mutex = TOKU_PTHREAD_MUTEX_INITIALIZER;
static int graceful_is_locked=0;

void toku_graceful_lock_init(void) {
    int r = toku_pthread_mutex_init(&graceful_mutex, NULL); assert(r == 0);
}

void toku_graceful_lock_destroy(void) {
    int r = toku_pthread_mutex_destroy(&graceful_mutex); assert(r == 0);
}

static inline void
lock_for_graceful (void) {
    // Locks the graceful_mutex. 
    int r = toku_pthread_mutex_lock(&graceful_mutex);
    assert(r==0);
    graceful_is_locked = 1;
}

static inline void
unlock_for_graceful (void) {
    graceful_is_locked = 0;
    int r = toku_pthread_mutex_unlock(&graceful_mutex);
    assert(r==0);
}

static int
graceful_open_get_append_fd(const char *db_fname, BOOL *is_dirtyp, BOOL *create) {
    BOOL clean_exists;
    BOOL dirty_exists;
    char cleanbuf[strlen(db_fname) + sizeof(".clean")];
    char dirtybuf[strlen(db_fname) + sizeof(".dirty")];

    sprintf(cleanbuf, "%s.clean", db_fname);
    sprintf(dirtybuf, "%s.dirty", db_fname);

    struct stat tmpbuf;
    clean_exists = stat(cleanbuf, &tmpbuf) == 0;
    dirty_exists = stat(dirtybuf, &tmpbuf) == 0;
    mode_t mode = S_IRWXU|S_IRWXG|S_IRWXO;
    int r = 0;

    *create = FALSE;
    if (dirty_exists && clean_exists) {
        r = unlink(cleanbuf);
        clean_exists = FALSE;
    }
    if (r==0) {
        if (!dirty_exists && !clean_exists) {
            *create = TRUE;
            dirty_exists = TRUE;
        }
        if (dirty_exists) {
            *is_dirtyp = TRUE;
            r = open(dirtybuf, O_WRONLY | O_CREAT | O_BINARY | O_APPEND, mode);
        }
        else {
            assert(clean_exists);
            *is_dirtyp = FALSE;
            r = open(cleanbuf, O_WRONLY | O_CREAT | O_BINARY | O_APPEND, mode);
        }
    }
    return r;
}

static int
graceful_close_get_append_fd(const char *db_fname, BOOL *db_missing) {
    BOOL clean_exists;
    BOOL dirty_exists;
    BOOL db_exists;
    char cleanbuf[strlen(db_fname) + sizeof(".clean")];
    char dirtybuf[strlen(db_fname) + sizeof(".dirty")];

    sprintf(cleanbuf, "%s.clean", db_fname);
    sprintf(dirtybuf, "%s.dirty", db_fname);

    struct stat tmpbuf;
    clean_exists = stat(cleanbuf, &tmpbuf) == 0;
    dirty_exists = stat(dirtybuf, &tmpbuf) == 0;
    db_exists    = stat(db_fname, &tmpbuf) == 0;
    mode_t mode = S_IRWXU|S_IRWXG|S_IRWXO;
    int r = 0;

    if (dirty_exists) {
        if (clean_exists) r = unlink(dirtybuf);
        else              r = rename(dirtybuf, cleanbuf);
    }
    if (db_exists) r = open(cleanbuf, O_WRONLY | O_CREAT | O_BINARY | O_APPEND, mode);
    else if (clean_exists) r = unlink(cleanbuf);
    *db_missing = !db_exists;
    return r;
}

static int
graceful_dirty_get_append_fd(const char *db_fname) {
    BOOL clean_exists;
    BOOL dirty_exists;
    char cleanbuf[strlen(db_fname) + sizeof(".clean")];
    char dirtybuf[strlen(db_fname) + sizeof(".dirty")];

    sprintf(cleanbuf, "%s.clean", db_fname);
    sprintf(dirtybuf, "%s.dirty", db_fname);

    struct stat tmpbuf;
    clean_exists = stat(cleanbuf, &tmpbuf) == 0;
    dirty_exists = stat(dirtybuf, &tmpbuf) == 0;
    mode_t mode = S_IRWXU|S_IRWXG|S_IRWXO;
    int r = 0;

    if (clean_exists) {
        if (dirty_exists) r = unlink(cleanbuf);
        else              r = rename(cleanbuf, dirtybuf);
    }
    r = open(dirtybuf, O_WRONLY | O_CREAT | O_BINARY | O_APPEND, mode);
    return r;
}

static void
graceful_log(int fd, char *operation, BOOL was_dirty, BOOL is_dirty) {
    //Logging.  Ignore errors.
    static char buf[sizeof(":-> pid= tid=  ")
                    +7  //operation
                    +5  //was dirty
                    +5  //is  dirty
                    +5  //process id
                    +5  //thread id
                    +26 //ctime string (including \n)
                   ];
    assert(graceful_is_locked); //ctime uses static buffer.  Lock must be held.
    time_t temptime;
    time(&temptime);
    snprintf(buf, sizeof(buf), "%-7s:%-5s->%-5s pid=%-5d tid=%-5d  %s",
             operation,
             was_dirty ? "dirty" : "clean",
             is_dirty  ? "dirty" : "clean",
             toku_os_getpid(),
             toku_os_gettid(),
             ctime(&temptime));
    write(fd, buf, strlen(buf));
} 

int
toku_graceful_open(const char *db_fname, BOOL *is_dirtyp) {
    int r;
    int r2 = 0;
    BOOL is_dirty;
    BOOL created;
    int fd;

    lock_for_graceful();
    fd = graceful_open_get_append_fd(db_fname, &is_dirty, &created);
    if (fd == -1) r = errno;
    else {
        graceful_log(fd, created ? "Created" : "Opened", is_dirty, is_dirty);
        *is_dirtyp = is_dirty;
        if (created || !is_dirty) r = 0;
        else r = TOKUDB_DIRTY_DICTIONARY;
        r2 = close(fd);
    }
    unlock_for_graceful();
    return r ? r : r2;
}

int
toku_graceful_close(CACHEFILE cf) {
    int r  = 0;
    int r2 = 0;
    int fd;
    const char *db_fname = cf->fname;

    if (db_fname) {
        lock_for_graceful();
        BOOL db_missing = FALSE;
        BOOL was_dirty = cf->is_dirty;
        fd = graceful_close_get_append_fd(db_fname, &db_missing);
        if (fd == -1) {
            if (!db_missing) r = errno;
        }
        else {
            graceful_log(fd, "Closed", was_dirty, FALSE);
            r2 = close(fd);
            cf->is_dirty = FALSE;
        }
        unlock_for_graceful();
    }
    return r ? r : r2;

}

int
toku_graceful_dirty(CACHEFILE cf) {
    int r  = 0;
    int r2 = 0;
    int fd;
    const char *db_fname = cf->fname;

    if (!cf->is_dirty && db_fname) {
        lock_for_graceful();
        fd = graceful_dirty_get_append_fd(db_fname);
        if (fd == -1) r = errno;
        else {
            graceful_log(fd, "Dirtied", FALSE, TRUE);
            r2 = close(fd);
            cf->is_dirty = TRUE;
        }
        unlock_for_graceful();
    }
    return r ? r : r2;
}

int
toku_graceful_delete(const char *db_fname) {
    BOOL clean_exists;
    char cleanbuf[strlen(db_fname) + sizeof(".clean")];
    BOOL dirty_exists;
    char dirtybuf[strlen(db_fname) + sizeof(".dirty")];

    sprintf(cleanbuf, "%s.clean", db_fname);
    sprintf(dirtybuf, "%s.dirty", db_fname);

    struct stat tmpbuf;
    lock_for_graceful();
    clean_exists = stat(cleanbuf, &tmpbuf) == 0;
    dirty_exists = stat(dirtybuf, &tmpbuf) == 0;

    int r = 0;
    if (clean_exists) {
        r = unlink(cleanbuf);
    }
    if (r==0 && dirty_exists) {
        r = unlink(dirtybuf);
    }
    unlock_for_graceful();
    return r;
}

